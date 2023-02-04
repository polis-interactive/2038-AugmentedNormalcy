//
// Created by bruce on 12/31/2022.
//

#include "cuda_decoder.hpp"

#include <chrono>
using namespace std::literals;


namespace Codec {

    void CudaDecoder::CreateDecoder(const Config &config, std::shared_ptr<Context> &context) {
        std::cout << "Creating decoder" << std::endl;
        const auto &[width, height] = config.get_codec_width_height();
        auto *cuda_context = static_cast<CudaContext *>(context.get());
        _decoder = std::unique_ptr<NvDecoder>(new NvDecoder(
            *cuda_context->_context, true, cudaVideoCodec_H264, true, false, NULL, NULL,
            false, width, height, 1000, true
        ));
    }
    void CudaDecoder::ThreadStartup() {
        cuCtxSetCurrent(_decoder->GetContext());
    }
    void CudaDecoder::DecodeFrame(std::unique_ptr<BspPacket> &&frame) {
        int returned_frames = 0;
        while (returned_frames == 0) {
            returned_frames = _decoder->Decode(frame->data_start, frame->data_length);
        }
    }
    void CudaDecoder::SendDecodedFrame() {
        void *decoded_frame = _decoder->GetLockedFrame();
        // we might change this to autofree up the memory,  but it gets weird with the cuda context
        auto buffer = std::shared_ptr<void>(decoded_frame, [](void*){});
        _send_callback(std::move(buffer));
        _gpu_buffers.push_back(buffer);
    }
    void CudaDecoder::StopDecoder() {
        std::cout << "Stopping decoder" << std::endl;
        WaitFreeMemory();
    }
    void CudaDecoder::TryFreeMemory() {
        auto gpu_buffer_iter = _gpu_buffers.begin();
        while (gpu_buffer_iter != _gpu_buffers.end()) {
            if (gpu_buffer_iter->use_count() > 1) {
                return;
            }
            auto gpu_buffer = std::move(_gpu_buffers.front());
            gpu_buffer_iter = _gpu_buffers.erase(gpu_buffer_iter);
            auto *buffer = (uint8_t *)gpu_buffer.get();
            _decoder->UnlockFrame(&buffer);
        }
    }
    void CudaDecoder::WaitFreeMemory() {
        bool has_tried = false;
        do {
            if (has_tried) {
                // don't have to spam here as we don't have deadlines
                std::this_thread::sleep_for(10ms);
            }
            TryFreeMemory();
            has_tried = true;
        }
        while (!_gpu_buffers.empty());
    }

    void CudaDecoder::TryDecode(std::shared_ptr<void> &&in_buffer) {
        auto payload_buffer = static_pointer_cast<SizedPayloadBuffer>(in_buffer);
        auto packet = BspPacket::TryParseFrame(payload_buffer->GetMemory(), payload_buffer->GetSize());
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
        if (packet->session_number != _session_number) {
            // i think our decoders are "stateless"?
            // ResetDecoder();
            _session_number = packet->session_number;
        }
        _sequence_number = packet->sequence_number;
        _timestamp = packet->timestamp;
        DecodeFrame(std::move(packet));
        in_buffer.reset();
        SendDecodedFrame();
        TryFreeMemory();
    }

}
