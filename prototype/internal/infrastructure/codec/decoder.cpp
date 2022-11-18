//
// Created by bruce on 12/31/2022.
//

#include "infrastructure/codec/decoder.hpp"

#include <chrono>
using namespace std::literals;



namespace infrastructure {
    void Decoder::CreateDecoder(const CodecConfig &config, CodecContext &context) {
        std::cout << "Creating decoder" << std::endl;
        const auto &[width, height] = config.get_codec_width_height();
        _decoder = std::unique_ptr<NvDecoder>(new NvDecoder(
            *context._context, true, cudaVideoCodec_H264, true, false, NULL, NULL,
            false, width, height, 1000, true
        ));
    }
    void Decoder::ThreadStartup() {
        cuCtxSetCurrent(_decoder->GetContext());
    }
    void Decoder::DecodeFrame(std::unique_ptr<BspPacket> &&frame) {
        int returned_frames = 0;
        while (returned_frames == 0) {
            returned_frames = _decoder->Decode(frame->data_start, frame->data_length);
        }
    }
    void Decoder::SendDecodedFrame() {
        uint8_t *decoded_frame = _decoder->GetLockedFrame();
        auto buffer = std::shared_ptr<GpuBuffer>(decoded_frame, [](GpuBuffer){});
        _send_callback(buffer);
        _gpu_buffers.push_back(buffer);
    }
    void Decoder::StopDecoder() {
        std::cout << "Stopping decoder" << std::endl;
        WaitFreeMemory();
    }
    void Decoder::TryFreeMemory() {
        auto gpu_buffer_iter = _gpu_buffers.begin();
        while (gpu_buffer_iter != _gpu_buffers.end()) {
            if (gpu_buffer_iter->use_count() > 1) {
                return;
            }
            auto gpu_buffer = std::move(_gpu_buffers.front());
            gpu_buffer_iter = _gpu_buffers.erase(gpu_buffer_iter);
            uint8_t *buffer = gpu_buffer.get();
            _decoder->UnlockFrame(&buffer);
        }
    }
    void Decoder::WaitFreeMemory() {
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

}
