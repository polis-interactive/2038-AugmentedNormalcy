//
// Created by brucegoose on 4/22/23.
//

#ifndef AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_PACKET_HEADER_HPP
#define AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_PACKET_HEADER_HPP

#include "utils/buffers.hpp"
#include <iostream>
#include <thread>
#include <boost/asio.hpp>
#include <memory>

#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif

const auto rolling_less_than = [](const uint16_t &a, const uint16_t &b) {
    // a == b, not less than
    // a < b if b - a is < 2^(16 - 1) (less than a big roll-over)
    // a > b if a - b is < 2^(16 - 1) (a is bigger but there is a huge roll-over)
    return a != b and (
            (a < b and (b - a) < std::pow(2, 16 - 1)) or
            (a > b and (a - b) > std::pow(2,16 - 1))
    );
};

typedef net::detail::socket_option::boolean<SOL_SOCKET, SO_REUSEPORT> reuse_port;

namespace infrastructure {

    class TcpReaderMessage
    {
    public:
        static constexpr std::size_t header_length = 16;
        static constexpr std::size_t max_body_length = 786432;

        TcpReaderMessage(): _body_length(0)
        {
        }

        [[nodiscard]] const char* Data() const
        {
            return _data;
        }

        char* Data()
        {
            return _data;
        }

        std::size_t BodyLength() const
        {
            return _body_length;
        }


        bool DecodeHeader()
        {
            char header[header_length + 1] = "";
            strncat(header, _data, header_length);
            _body_length = atoi(header);
            if (_body_length > max_body_length)
            {
                _body_length = 0;
                return false;
            }
            return true;
        }

        [[nodiscard]] std::size_t Length() const
        {
            return header_length;
        }


    private:
        char _data[header_length];
        std::size_t _body_length;
    };

    class TcpWriterMessage
    {
    public:

        static constexpr std::size_t header_length = 16;
        static constexpr std::size_t max_body_length = 786432;

        explicit TcpWriterMessage(std::shared_ptr<SizedBuffer> &&buffer):
                _buffer(std::move(buffer))
        {
            encodeHeader();
        }

        [[nodiscard]] std::shared_ptr<SizedBuffer> &GetBuffer() {
            return _buffer;
        }

        char* Data()
        {
            return _data;
        }

        [[nodiscard]] std::size_t Length() const
        {
            return header_length;
        }
    private:
        void encodeHeader()
        {
            char header[header_length + 1] = "";
            std::sprintf(header, "%16d", static_cast<int>(_buffer->GetSize()));
            std::memcpy(_data, header, header_length);
        }

        char _data[header_length]{};
        std::shared_ptr<SizedBuffer> _buffer;
    };

    struct PacketHeader {
    public:
        PacketHeader() {
            memset(_data, 0, sizeof(_data));
        }
        /* common */
        static constexpr uint64_t MaxSize = 65536;
        char *Data() {
            return _data;
        }
        [[nodiscard]] std::size_t Size() const {
            return sizeof _data;
        }
        [[nodiscard]] uint32_t DataLength() const {
            return _data_length;
        }
        [[nodiscard]] uint32_t BytesWritten() const {
            return _bytes_written;
        }
        void OffsetPacket(std::size_t bytes_written) {
            _data_length -= bytes_written;
            _bytes_written += bytes_written;
        }
        [[nodiscard]] bool IsFinished() {
            _bytes_written += _data_length;
            return _bytes_written >= _total_bytes;
        }
        /* write methods */
        void SetupHeader(uint64_t total_bytes) {
            std::memset(_data, 0, sizeof _data);
            _recall_packet_number += 1;
            _packet_number = _recall_packet_number;
            _total_bytes = total_bytes;
            _bytes_written = 0;
            _data_length = std::min(total_bytes, MaxSize);
        }
        void SetupNextHeader() {
            _sequence_number += 1;
            _data_length = std::min(_total_bytes - _bytes_written, MaxSize);
        }
        /* read methods */
        [[nodiscard]] bool Ok() const {
            if (_front != 0 || _back != 0) {
                return false;
            }
            return true;
        }
        void ResetHeader() {
            _bytes_written = 0;
        }


    private:
        /* common members */
        union {
            struct {
                uint32_t _front;
                uint16_t _packet_number;
                uint16_t _sequence_number;
                uint16_t _session_number;
                uint16_t _timestamp;
                uint32_t _data_length;
                uint32_t _total_bytes;
                uint32_t _back;
            };
            char _data[24] = {};
        };
        uint64_t _bytes_written = 0;

        /* write members */
        uint16_t _recall_packet_number = 0;

        /* read members */
        uint16_t _last_packet_number = 0;
        uint16_t _last_sequence_number = 0;

    };

    class TcpBuffer: public ResizableBuffer {
    public:
        TcpBuffer(std::size_t size, const bool is_leaky):
                _max_size(size), _is_leaky(is_leaky)
        {
            _buffer = new unsigned char[_max_size];
        }
        [[nodiscard]] void * GetMemory() final {
            return _buffer;
        }
        [[nodiscard]] std::size_t GetSize() final {
            return _size;
        }
        void SetSize(std::size_t used_size) final {
            _size = used_size;
        };
        [[nodiscard]] bool IsLeakyBuffer() final {
            return _is_leaky;
        };
        ~TcpBuffer() {
            delete[] _buffer;
        }
    private:
        const std::size_t _max_size;
        const bool _is_leaky;
        unsigned char *_buffer;
        std::size_t _size;
    };

    class TcpReadBufferPool: public std::enable_shared_from_this<TcpReadBufferPool> {
    public:
        static std::shared_ptr<TcpReadBufferPool> Create(const int buffer_count, const int buffer_size) {
            auto buffer_pool = std::make_shared<TcpReadBufferPool>(buffer_count, buffer_size);
            return buffer_pool;
        }
        TcpReadBufferPool(const int buffer_count, const int buffer_size) {
            for (int i = 0; i < buffer_count; i++) {
                _buffers.push_back(new TcpBuffer(buffer_size, false));
            }
            _leaky_buffer = std::make_shared<TcpBuffer>(buffer_size, true);
        }
        [[nodiscard]] std::shared_ptr<TcpBuffer> GetReadBuffer() {
            TcpBuffer *buffer = nullptr;
            {
                std::unique_lock<std::mutex> lock(_buffer_mutex);
                if (!_buffers.empty()) {
                    buffer = _buffers.front();
                    _buffers.pop_front();
                }
            }
            if (buffer == nullptr) {
                return _leaky_buffer;
            }
            auto self(shared_from_this());
            auto wrapped_buffer = std::shared_ptr<TcpBuffer>(
                    buffer, [this, self](TcpBuffer * b) mutable {
                        std::unique_lock<std::mutex> lock(_buffer_mutex);
                        _buffers.push_back(b);
                    }
            );
            return wrapped_buffer;
        };
        ~TcpReadBufferPool() {
            TcpBuffer *buffer;
            std::unique_lock<std::mutex> lock(_buffer_mutex);
            while (!_buffers.empty()) {
                buffer = _buffers.front();
                _buffers.pop_front();
                delete buffer;
            }
        }
    private:
        std::mutex _buffer_mutex;
        std::deque<TcpBuffer *> _buffers;
        std::shared_ptr<TcpBuffer> _leaky_buffer;
    };

    class TcpWriteBufferPool: public std::enable_shared_from_this<TcpWriteBufferPool> {
    public:
        static std::shared_ptr<TcpWriteBufferPool> Create(const int buffer_count, const int buffer_size) {
            auto buffer_pool = std::make_shared<TcpWriteBufferPool>(buffer_count, buffer_size);
            return buffer_pool;
        }
        TcpWriteBufferPool(const int buffer_count, const int buffer_size) {
            for (int i = 0; i < buffer_count; i++) {
                _buffers.push_back(new TcpBuffer(buffer_size, false));
            }
        }
        [[nodiscard]] std::shared_ptr<TcpBuffer> CopyToWriteBuffer(std::shared_ptr<SizedBuffer> copy_buffer) {
            TcpBuffer *buffer = nullptr;
            {
                std::unique_lock<std::mutex> lock(_buffer_mutex);
                if (!_buffers.empty()) {
                    buffer = _buffers.front();
                    _buffers.pop_front();
                }
            }
            if (buffer == nullptr) {
                // just being explicit
                return nullptr;
            }
            memcpy(buffer->GetMemory(), copy_buffer->GetMemory(), copy_buffer->GetSize());
            buffer->SetSize(copy_buffer->GetSize());
            auto self(shared_from_this());
            auto wrapped_buffer = std::shared_ptr<TcpBuffer>(
                buffer, [this, self](TcpBuffer * b) mutable {
                    std::unique_lock<std::mutex> lock(_buffer_mutex);
                    _buffers.push_back(b);
                }
            );
            return wrapped_buffer;
        };
        ~TcpWriteBufferPool() {
            TcpBuffer *buffer;
            std::unique_lock<std::mutex> lock(_buffer_mutex);
            while (!_buffers.empty()) {
                buffer = _buffers.front();
                _buffers.pop_front();
                delete buffer;
            }
        }
    private:
        std::mutex _buffer_mutex;
        std::deque<TcpBuffer *> _buffers;
    };
}

#endif //AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_PACKET_HEADER_HPP
