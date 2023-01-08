//
// Created by bruce on 12/31/2022.
//

#ifndef INFRASTRUCTURE_CODEC_DECODER_HPP
#define INFRASTRUCTURE_CODEC_DECODER_HPP

#include "NvDecoder/NvDecoder.h"

#include "utility/worker_thread.hpp"

#include "infrastructure/common.hpp"
#include "infrastructure/codec/bsp_packet.hpp"
#include "infrastructure/codec/codec.hpp"

#include <deque>
#include <functional>
#include <utility>


namespace infrastructure {

    class Decoder {
    public:
        Decoder(
            const CodecConfig &config, CodecContext &context,
            std::function<void(std::shared_ptr<GpuBuffer>)> send_callback
        ) :
            _wt(utility::WorkerThread<QueuedPayloadReceive>::CreateWorkerThread(
                std::bind_front(&Decoder::TryDecode, this),
                std::bind_front(&Decoder::ThreadStartup, this)
            )),
            _send_callback(std::move(send_callback))
        {
            CreateDecoder(config, context);
        }
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
    private:
        // private impl based on platform
        void CreateDecoder(const CodecConfig &config, CodecContext &context);

        void ThreadStartup();

        void DecodeFrame(std::unique_ptr<BspPacket> &&frame);
        void SendDecodedFrame();
        void TryFreeMemory();
        void WaitFreeMemory();
        void StopDecoder();

        void TryDecode(std::shared_ptr<QueuedPayloadReceive> &&qp) {
            auto [payload, size] = qp->GetPayload();
            auto packet = BspPacket::TryParseFrame(payload, size);
            // header wasn't parsable ||
            // session number is less than our current one ||
            // sequence number is less than or equal to our current one
            if (
                !packet ||
                rolling_less_than(packet->session_number, _session_number) ||
                packet->sequence_number == _sequence_number ||
                rolling_less_than(packet->sequence_number, _sequence_number)
            ) {
                // should send a failure message back;
                return;
            }
            // we know the sequence number was incremented from previous check; new stream
            if (packet->session_number != _session_number) {
                // i think our decoders are "stateless"?
                // ResetDecoder();
                _session_number = packet->session_number;
            }
            _sequence_number = packet->sequence_number;
            // on debug here, we can trace how long it's been since the last _timestamp; make sure the encode / transfer
            // is reliable / track jitter
            _timestamp = packet->timestamp;
            DecodeFrame(std::move(packet));
            // return the buffer asap
            qp.reset();
            SendDecodedFrame();
            TryFreeMemory();
        }

        uint16_t _session_number = 0;
        uint16_t _sequence_number = 0;
        uint16_t _timestamp = 0;
        std::shared_ptr<utility::WorkerThread<QueuedPayloadReceive>> _wt;
        std::unique_ptr<NvDecoder> _decoder = {nullptr};
        std::deque<std::shared_ptr<GpuBuffer>> _gpu_buffers;
        std::function<void(std::shared_ptr<GpuBuffer>)> _send_callback;
    };

}

#endif //INFRASTRUCTURE_CODEC_DECODER_HPP
