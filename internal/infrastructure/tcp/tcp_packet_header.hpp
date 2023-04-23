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
}

#endif //AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_PACKET_HEADER_HPP
