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
        CUDA,
        V4L2,
    };

    struct Config {
        [[nodiscard]] virtual std::pair<int, int> get_codec_width_height() const = 0;
        // as opposed to 30
        [[nodiscard]] virtual int get_fps() const = 0;
        [[nodiscard]] virtual Type get_codec_type() const = 0;
        // for some reason, we need to know this for the v4l2 encoder
        [[nodiscard]] virtual int get_camera_buffer_count() const = 0;
        [[nodiscard]] virtual int get_encoder_buffer_count() const = 0;
    };

    class Context {
    public:
        [[nodiscard]] static std::shared_ptr<Context> Create(const Config &config);
    };

    class Encoder {
    public:
        [[nodiscard]] static std::shared_ptr<Encoder> Create(
            const Config &config, std::shared_ptr<Context> &context,
            SendCallback &&send_callback
        );

        Encoder(const Config &config, std::shared_ptr<Context> &context, SendCallback &&send_callback);
        void Start() {
            _wt->Start();
            StartEncoder();
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
        SendCallback _send_callback;
        PayloadBufferPool _b_pool;
    private:
        virtual void StartEncoder() {};
        virtual void CreateEncoder(const Config &config, std::shared_ptr<Context> &context) = 0;
        virtual void DoResetEncoder() = 0;
        virtual void DoStopEncoder() {}
        virtual void StopEncoder() {
            ResetEncoder();
            DoStopEncoder();
        };
        // this might need to be made a functor
        virtual void TryEncode(std::shared_ptr<void> &&buffer) = 0;
        std::shared_ptr<utility::WorkerThread<void>> _wt;
    };

    class Decoder {
    public:
        [[nodiscard]] static std::shared_ptr<Decoder> Create(
            const Config &config, std::shared_ptr<Context> &context,
            SendCallback &&send_callback
        );
        Decoder(
            const Config &config, std::shared_ptr<Context> &context,
            SendCallback &&send_callback
        );
        void Start() {
            _wt->Start();
        }
        void QueueDecode(std::shared_ptr<void> &&qp) {
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
        SendCallback _send_callback;
    private:
        virtual void CreateDecoder(const Config &config, std::shared_ptr<Context> &context) = 0;
        virtual void ThreadStartup() = 0;
        // this might need to be made a functor
        virtual void TryDecode(std::shared_ptr<void> &&qp) = 0;
        virtual void StopDecoder() = 0;

        // can probably make this SizedPayloadBuffer, but for some reason the linter didn't like that...
        std::shared_ptr<utility::WorkerThread<void>> _wt;

    };
}

#endif //INFRASTRUCTURE_CODEC_HPP
