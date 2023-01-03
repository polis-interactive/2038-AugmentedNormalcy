//
// Created by bruce on 1/2/2023.
//

#include "infrastructure/codec/encoder.hpp"

namespace infrastructure {

    void Encoder::CreateEncoder(const CodecConfig &config, CodecContext &context) {
        std::cout << "Creating encoder" << std::endl;
        const auto &[width, height] = config.get_codec_width_height();
        _encoder.reset(new NvEncoderCuda(
            *context._context, width, height, NV_ENC_BUFFER_FORMAT_NV12
        ));
        NV_ENC_INITIALIZE_PARAMS initialize_params = {NV_ENC_INITIALIZE_PARAMS_VER };
        NV_ENC_CONFIG encode_config = {NV_ENC_CONFIG_VER };
        initialize_params.encodeConfig = &encode_config;
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
        initialize_params.frameRateNum = config.get_is_60_fps() ? 60 : 30;
        encode_config.rcParams.averageBitRate = (static_cast<unsigned int>(5.0f * initialize_params.encodeWidth * initialize_params.encodeHeight) / (1280 * 720)) * 100000;
        // this gives 93.75K at 60, > 50K I was using
        encode_config.rcParams.vbvBufferSize = (encode_config.rcParams.averageBitRate * initialize_params.frameRateDen / initialize_params.frameRateNum) * 5;
        encode_config.rcParams.maxBitRate = 2000000; // was encode_config.rcParams.averageBitRate, which was 1.125M instead of 2M
        encode_config.rcParams.vbvInitialDelay = encode_config.rcParams.vbvBufferSize;
        _encoder->CreateEncoder(&initialize_params);
        const NvEncInputFrame* encoderInputFrame = _encoder->GetNextInputFrame();
    }
    void Encoder::StopEncoder() {
        std::cout << "Stopping Encoder" << std::endl;
        ResetEncoder();
    }
    void Encoder::ResetEncoder() {
        NV_ENC_INITIALIZE_PARAMS current_params = { NV_ENC_INITIALIZE_PARAMS_VER };
        NV_ENC_CONFIG encode_config = {NV_ENC_CONFIG_VER };
        current_params.encodeConfig = &encode_config;
        _encoder->GetInitializeParams(&current_params);
        auto context = (CUcontext)_encoder->GetDevice();
        _encoder.reset(new NvEncoderCuda(
            context, current_params.encodeWidth, current_params.encodeHeight, NV_ENC_BUFFER_FORMAT_NV12
        ));
        _encoder->CreateEncoder(&current_params);
        _session_number += 1;
        _sequence_number = 0;
    }

    void Encoder::PrepareEncode(std::shared_ptr<GpuBuffer> &&gb) {
        const NvEncInputFrame* encoderInputFrame = _encoder->GetNextInputFrame();
        NvEncoderCuda::CopyToDeviceFrame(
            (CUcontext)_encoder->GetDevice(), gb.get(), (int)encoderInputFrame->pitch, (CUdeviceptr)encoderInputFrame->inputPtr,
            encoderInputFrame->pitch, _encoder->GetEncodeWidth(), _encoder->GetEncodeHeight(), CU_MEMORYTYPE_DEVICE,
            encoderInputFrame->bufferFormat, encoderInputFrame->chromaOffsets, encoderInputFrame->numChromaPlanes
        );
    }

    std::size_t Encoder::EncodeFrame(uint8_t *packet_ptr, const std::size_t header_size) {
        return _encoder->EncodeFixedFrame(packet_ptr + header_size, MAX_FRAME_LENGTH - header_size);
    }

}