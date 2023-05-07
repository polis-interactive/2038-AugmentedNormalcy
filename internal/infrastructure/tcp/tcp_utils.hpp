//
// Created by brucegoose on 4/22/23.
//

#ifndef AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_PACKET_HEADER_HPP
#define AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_PACKET_HEADER_HPP

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

namespace infrastructure {
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

    class TcpReadBuffer: public ResizableBuffer {
    public:
        TcpReadBuffer(std::size_t size, const bool is_leaky):
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
        virtual void SetSize(std::size_t used_size) {
            _size = used_size;
        };
        [[nodiscard]] bool IsLeakyBuffer() final {
            return _is_leaky;
        };
        ~TcpReadBuffer() {
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
                _buffers.push_back(new TcpReadBuffer(buffer_size, false));
            }
            _leaky_buffer = std::make_shared<TcpReadBuffer>(buffer_size, true);
        }
        [[nodiscard]] std::shared_ptr<TcpReadBuffer> GetReadBuffer() {
            TcpReadBuffer *buffer = nullptr;
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
            auto wrapped_buffer = std::shared_ptr<TcpReadBuffer>(
                    buffer, [this, s = std::move(self)](TcpReadBuffer * b) mutable {
                        std::unique_lock<std::mutex> lock(_buffer_mutex);
                        _buffers.push_back(b);
                    }
            );
            return wrapped_buffer;
        };
        ~TcpReadBufferPool() {
            TcpReadBuffer *buffer;
            std::unique_lock<std::mutex> lock(_buffer_mutex);
            while (!_buffers.empty()) {
                buffer = _buffers.front();
                _buffers.pop_front();
                delete buffer;
            }
        }
    private:
        std::shared_ptr<TcpReadBuffer> _leaky_buffer;
        std::deque<TcpReadBuffer *> _buffers;
        std::mutex _buffer_mutex;
    };
}

#endif //AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_PACKET_HEADER_HPP
