//
// Created by bruce on 1/2/2023.
//

#include <memory>

#include "cuda_encoder.hpp"
#include "cuda_codec.hpp"

namespace Codec {

    void CudaEncoder::CreateEncoder(const Config &config, std::shared_ptr<Context> &context) {
        std::cout << "Creating encoder" << std::endl;
        const auto &[width, height] = config.get_codec_width_height();
        auto *cuda_context = static_cast<CudaContext *>(context.get());
        _encoder.reset(new NvEncoderCuda(
            *cuda_context->_context, width, height, NV_ENC_BUFFER_FORMAT_NV12
        ));
        NV_ENC_INITIALIZE_PARAMS initialize_params = {NV_ENC_INITIALIZE_PARAMS_VER };
        NV_ENC_CONFIG encode_config = {NV_ENC_CONFIG_VER };
        initialize_params.encodeConfig = &encode_config;
        // configs from here https://docs.nvidia.com/video-technologies/video-codec-sdk/nvenc-video-encoder-api-prog-guide/index.html#selecting-an-encoder-profile
        // encoder_preset.at(2) == p3
        // vTuningInfo.at(2) == ultralowlatency
        _encoder->CreateDefaultEncoderParams(
            &initialize_params, NV_ENC_CODEC_H264_GUID, encoder_preset.at(2),
            vTuningInfo.at(2)
        );
        // got from AppEncLowLatency example
        encode_config.gopLength = NVENC_INFINITE_GOPLENGTH;
        encode_config.frameIntervalP = 1;
        encode_config.encodeCodecConfig.h264Config.idrPeriod = NVENC_INFINITE_GOPLENGTH;
        // vRcMode.at(2) == cbr
        encode_config.rcParams.rateControlMode = vRcMode.at(2);
        // vMultiPass.at(1) == qres
        encode_config.rcParams.multiPass = vMultiPass.at(1);
        initialize_params.frameRateNum = config.get_fps();
        encode_config.rcParams.averageBitRate = (static_cast<unsigned int>(5.0f * initialize_params.encodeWidth * initialize_params.encodeHeight) / (1280 * 720)) * 100000;
        // this gives 93.75K at 60, > 50K I was using
        encode_config.rcParams.vbvBufferSize = (encode_config.rcParams.averageBitRate * initialize_params.frameRateDen / initialize_params.frameRateNum) * 5;
        encode_config.rcParams.maxBitRate = 2000000; // was encode_config.rcParams.averageBitRate, which was 1.125M instead of 2M
        encode_config.rcParams.vbvInitialDelay = encode_config.rcParams.vbvBufferSize;
        _encoder->CreateEncoder(&initialize_params);
    }
    void CudaEncoder::DoResetEncoder() {
        NV_ENC_INITIALIZE_PARAMS current_params = { NV_ENC_INITIALIZE_PARAMS_VER };
        NV_ENC_CONFIG encode_config = {NV_ENC_CONFIG_VER };
        current_params.encodeConfig = &encode_config;
        _encoder->GetInitializeParams(&current_params);
        auto context = (CUcontext)_encoder->GetDevice();
        _encoder.reset(new NvEncoderCuda(
            context, current_params.encodeWidth, current_params.encodeHeight, NV_ENC_BUFFER_FORMAT_NV12
        ));
        _encoder->CreateEncoder(&current_params);
    }

    void CudaEncoder::PrepareEncode(std::shared_ptr<void> &&gb) {
        const NvEncInputFrame* encoderInputFrame = _encoder->GetNextInputFrame();
        NvEncoderCuda::CopyToDeviceFrame(
            (CUcontext)_encoder->GetDevice(), gb.get(), 0, (CUdeviceptr)encoderInputFrame->inputPtr,
            encoderInputFrame->pitch, _encoder->GetEncodeWidth(), _encoder->GetEncodeHeight(), CU_MEMORYTYPE_DEVICE,
            encoderInputFrame->bufferFormat, encoderInputFrame->chromaOffsets, encoderInputFrame->numChromaPlanes
        );
    }

    std::size_t CudaEncoder::EncodeFrame(void *packet_ptr, const std::size_t header_size) {
        return _encoder->EncodeFixedFrame((uint8_t *)packet_ptr + header_size, MAX_FRAME_LENGTH - header_size);
    }

    void CudaEncoder::TryEncode(std::shared_ptr<void> &&in_buffer) {
        // this will release the buffer
        PrepareEncode(std::move(in_buffer));
        auto payload_buffer = _b_pool.New();
        auto frame_size = EncodeFrame(payload_buffer->_buffer.data(), BspPacket::HeaderSize());
        if (frame_size == 0) {
            // failed to encode packet
            return;
        }
        _sequence_number += 1;
        BspPacket packet{};
        packet.session_number = _session_number;
        packet.sequence_number = _sequence_number;
        packet.Pack(payload_buffer->_buffer.data(), frame_size);
        payload_buffer->_size = frame_size + BspPacket::HeaderSize();
        auto out_buffer = std::shared_ptr<void>(payload_buffer.get(), [this, &payload_buffer](void *b_ptr) {
            _b_pool.Free(std::move(payload_buffer));
        });
        _send_callback(std::move(out_buffer));
    }

}