//
// Created by brucegoose on 2/3/23.
//

#include "codec.hpp"

#include <memory>

#include "cuda/cuda_codec.hpp"
#include "cuda/cuda_decoder.hpp"
#include "cuda/cuda_encoder.hpp"

#include "v4l2/v4l2_encoder.hpp"

namespace Codec {

    std::shared_ptr<Context> Context::Create(const Config &config) {
        switch(config.get_codec_type()) {
#ifdef _CUDA_CODEC_
            case Type::CUDA:
                return std::make_shared<CudaContext>();
#endif
            default:
                throw std::runtime_error("Selected codec unavailable... ");
        }
    }

    std::shared_ptr<Encoder> Encoder::Create(
        const Codec::Config &config, std::shared_ptr<Context> &context,
        std::shared_ptr<PayloadSend> &&payload_sender
    ) {
        switch(config.get_codec_type()) {
#ifdef _CUDA_CODEC_
            case Type::CUDA:
                return std::make_shared<CudaEncoder>(config, context, std::move(payload_sender));
#endif
#ifdef _V4L2_CODEC_
            case Type::V4L2:
                return std::make_shared<V4l2Encoder>(config, context, std::move(payload_sender));
#endif
            default:
                throw std::runtime_error("Selected codec unavailable... ");
        }
    }

    Encoder::Encoder(
       const Codec::Config &config, std::shared_ptr<Context> &context,
       std::shared_ptr<PayloadSend> &&payload_sender
    ) :
        _wt(utility::WorkerThread<PayloadReceive>::CreateWorkerThread(
            std::bind_front(&Encoder::TryEncode, this)
        )),
        _payload_sender(std::move(payload_sender))
    {}

    void Encoder::TryEncode(std::shared_ptr<void> &&in_buffer) {
        // this will release the buffer
        PrepareEncode(std::move(in_buffer));
        auto buffer = _payload_sender->GetBuffer();
        auto frame_size = EncodeFrame(buffer.get(), BspPacket::HeaderSize());
        if (frame_size == 0) {
            // failed to encode packet
            return;
        }
        _sequence_number += 1;
        BspPacket packet{};
        packet.session_number = _session_number;
        packet.sequence_number = _sequence_number;
        // for now, fk the timestamp;
        packet.Pack(buffer, frame_size);
        _payload_sender->Send(std::move(buffer), packet.PacketSize());
    }

    std::shared_ptr<Decoder> Decoder::Create(
        const Config &config, std::shared_ptr<Context> &context,
        std::function<void(std::shared_ptr<void>)> send_callback
    ) {
        switch(config.get_codec_type()) {
#ifdef _CUDA_CODEC_
            case Type::CUDA:
                return std::make_shared<CudaDecoder>(config, context, std::move(send_callback));
#endif
            default:
                throw std::runtime_error("Selected codec unavailable... ");
        }
    }

    Decoder::Decoder(
        const Codec::Config &config, std::shared_ptr<Context> &context,
        std::function<void(std::shared_ptr<void>)> send_callback
    ):
        _wt(utility::WorkerThread<PayloadReceive>::CreateWorkerThread(
            std::bind_front(&Decoder::TryDecode, this),
            std::bind_front(&Decoder::ThreadStartup, this)
        )),
        _send_callback(std::move(send_callback))
    {}

    void Decoder::TryDecode(std::shared_ptr<PayloadReceive> &&qp) {
        auto payload = qp->GetPayload();
        auto packet = BspPacket::TryParseFrame(payload->GetMemory(), payload->GetSize());
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
        _timestamp = packet->timestamp;
        DecodeFrame(std::move(packet));
        qp.reset();
        SendDecodedFrame();
        TryFreeMemory();
    }

}
