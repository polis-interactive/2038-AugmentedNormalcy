//
// Created by brucegoose on 2/3/23.
//

#ifndef INFRASTRUCTURE_CODEC_HPP
#define INFRASTRUCTURE_CODEC_HPP

#include <utility>
#include <memory>

#include "utility/worker_thread.hpp"

#include "bsp_packet.hpp"
#include "../common.hpp"

namespace Codec {

    enum class Type {
        Cuda,
    };

    struct Config {
        [[nodiscard]] virtual std::pair<int, int> get_codec_width_height() const = 0;
        // as opposed to 30
        [[nodiscard]] virtual int get_fps() const = 0;
        [[nodiscard]] virtual Type get_codec_type() const = 0;
    };

    class Context {
    public:
        [[nodiscard]] static std::shared_ptr<Context> Create(const Config &config);
    };

    struct PayloadSend {
        [[nodiscard]] virtual std::shared_ptr<void> GetBuffer() = 0;
        virtual void Send(std::shared_ptr<void> &&buffer, std::size_t buffer_size) = 0;
    };

    class Encoder {
    public:
        [[nodiscard]] static std::shared_ptr<Encoder> Create(
            const Config &config, std::shared_ptr<Context> &context,
            std::shared_ptr<PayloadSend> &&payload_sender
        );

        Encoder(const Config &config, std::shared_ptr<Context> &context, std::shared_ptr<PayloadSend> &&payload_sender);
        void Start() {
            _wt->Start();
        }
        void QueueEncode(std::shared_ptr<void> &&buffer) {
            _wt->PostWork(std::move(buffer));
        }
        void Stop() {
            _wt->Stop();
            StopEncoder();
        }
    protected:

        void ResetEncoder() {
            _session_number += 1;
            _sequence_number = 0;
            DoResetEncoder();
        }
        uint16_t _session_number = 1;
        uint16_t _sequence_number = 0;
        uint16_t _timestamp = 0;
    private:
        virtual void CreateEncoder(const Config &config, std::shared_ptr<Context> &context) = 0;
        virtual void DoResetEncoder() = 0;
        virtual void PrepareEncode(std::shared_ptr<void> &&buffer) = 0;
        virtual std::size_t EncodeFrame(void *packet_ptr, std::size_t header_size) = 0;
        virtual void StopEncoder() {
            ResetEncoder();
        };

        void TryEncode(std::shared_ptr<void> &&buffer);

        std::shared_ptr<utility::WorkerThread<void>> _wt;
        std::shared_ptr<PayloadSend> _payload_sender;
    };

    using payload_tuple = std::pair<std::shared_ptr<void>, std::size_t>;
    struct QueuedPayloadReceive {
        [[nodiscard]]  virtual payload_tuple GetPayload() = 0;
    };

    class Decoder {
    public:
        [[nodiscard]] static std::shared_ptr<Decoder> Create(
            const Config &config, std::shared_ptr<Context> &context,
            std::function<void(std::shared_ptr<void>)> send_callback
        );

        Decoder(
            const Config &config, std::shared_ptr<Context> &context,
            std::function<void(std::shared_ptr<void>)> send_callback
        );
        void Start() {
            _wt->Start();
        }
        void QueueDecode(std::shared_ptr<QueuedPayloadReceive> &&qp) {
            _wt->PostWork(std::move(qp));
        }
        void Stop() {
            _wt->Stop();
            StopDecoder();
        }
    protected:
        uint16_t _session_number = 0;
        uint16_t _sequence_number = 0;
        uint16_t _timestamp = 0;
        std::function<void(std::shared_ptr<void>)> _send_callback;
    private:
        virtual void CreateDecoder(const Config &config, std::shared_ptr<Context> &context) = 0;
        virtual void ThreadStartup() = 0;
        virtual void DecodeFrame(std::unique_ptr<BspPacket> &&frame) = 0;
        virtual void SendDecodedFrame() = 0;
        virtual void TryFreeMemory() = 0;
        virtual void StopDecoder() = 0;

        void TryDecode(std::shared_ptr<QueuedPayloadReceive> &&qp);
        std::shared_ptr<utility::WorkerThread<QueuedPayloadReceive>> _wt;

    };
}

#endif //INFRASTRUCTURE_CODEC_HPP
