//
// Created by bruce on 12/31/2022.
//

#ifndef INFRASTRUCTURE_CODEC_DECODER_HPP
#define INFRASTRUCTURE_CODEC_DECODER_HPP

#include "infrastructure/common.hpp"

#include "utility/worker_thread.hpp"

#include "infrastructure/codec/packet.hpp"


namespace infrastructure {

    struct DecoderConfig {

    };

    class Decoder {
    public:
        explicit Decoder(const DecoderConfig &config) :
            _wt(utility::WorkerThread<QueuedPayload>::CreateWorkerThread(
                std::bind_front(&Decoder::TryDecode, this)
            ))
        {
            CreateDecoder();
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
        void CreateDecoder();
        void StartDecoder();
        void StopDecoder();
        void TryDecode(std::shared_ptr<QueuedPayload> &&qp) {
            auto [payload, size] = qp->GetPayload();
            auto header = PacketHeader::TryParseFrame(payload, size);
            // header wasn't parsable ||
            // session number is less than our current one ||
            // sequence number is less than or equal to our current one
            if (
                !header ||
                rolling_less_than(header->session_number, session_number) ||
                header->sequence_number == sequence_number ||
                rolling_less_than(header->sequence_number, sequence_number)
            ) {
                // should send a failure message back;
                return;
            }
            // we know the sequence number was incremented from previous check; new stream
            if (header->session_number != session_number) {
                // decoder reset
                session_number = header->session_number;
            }
            sequence_number = header->sequence_number;
            // copy to decoder
            // now we release the shared buffer / header
            qp.reset();
            header.reset();
            // now we do the blocking decode
        }
        uint16_t session_number = 0;
        uint16_t sequence_number = 0;
        std::shared_ptr<utility::WorkerThread<QueuedPayload>> _wt;
    };

}

#endif //INFRASTRUCTURE_CODEC_DECODER_HPP
