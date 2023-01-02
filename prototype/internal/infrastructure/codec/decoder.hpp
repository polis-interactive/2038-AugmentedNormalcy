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


namespace infrastructure {

    class Decoder {
    public:
        explicit Decoder(const CodecConfig &config, CodecContext &context) :
            _wt(utility::WorkerThread<QueuedPayload>::CreateWorkerThread(
                std::bind_front(&Decoder::TryDecode, this)
            ))
        {
            CreateDecoder(config, context);
        }
        void Start() {
            StartDecoder();
            _wt->Start();
        }
        void Stop() {
            _wt->Stop();
            StopDecoder();
        }
    private:
        // private impl based on platform
        void CreateDecoder(const CodecConfig &config, CodecContext &context);
        void StartDecoder();
        void ResetDecoder();
        void CopyToDecoder(std::unique_ptr<BspPacket> &&frame);
        void DecodeFrame();
        void TryFreeMemory();
        void StopDecoder();

        void TryDecodeAndFreeMemory(std::shared_ptr<QueuedPayload> &&qp) {
            TryDecode(std::move(qp));
            TryFreeMemory();
        }

        void TryDecode(std::shared_ptr<QueuedPayload> &&qp) {
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
                ResetDecoder();
                _session_number = packet->session_number;
            }
            _sequence_number = packet->sequence_number;
            // on debug here, we can trace how long it's been since the last _timestamp; make sure the encode / transfer
            // is reliable / track jitter
            _timestamp = packet->timestamp;
            // move implicitly resets the packet
            CopyToDecoder(std::move(packet));
            qp.reset();
            DecodeFrame();
        }

        uint16_t _session_number = 0;
        uint16_t _sequence_number = 0;
        uint16_t _timestamp = 0;
        std::shared_ptr<utility::WorkerThread<QueuedPayload>> _wt;
        std::unique_ptr<NvDecoder> _decoder = {nullptr};
    };

}

#endif //INFRASTRUCTURE_CODEC_DECODER_HPP
