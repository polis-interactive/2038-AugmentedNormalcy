//
// Created by bruce on 1/2/2023.
//

#ifndef INFRASTRUCTURE_CODEC_CUDA_ENCODER_HPP
#define INFRASTRUCTURE_CODEC_CUDA_ENCODER_HPP

#include "NvInterface/nvEncodeAPI.h"
#include "NvEncoder/NvEncoderCuda.h"

#include "cuda_codec.hpp"

namespace Codec {

    using NvEncCudaPtr = std::unique_ptr<NvEncoderCuda, std::function<void(NvEncoderCuda*)>>;
    const auto EncodeDeleteFunc = [](NvEncoderCuda *pEnc)
    {
        if (pEnc)
        {
            pEnc->DestroyEncoder();
            delete pEnc;
        }
    };

    class CudaEncoder : public Encoder {
    public:
        CudaEncoder(
            const Config &config, std::shared_ptr<Context> &context, SendCallback &&payload_sender
        ):
            Encoder(config, context, std::move(payload_sender)),
            _encoder(nullptr, EncodeDeleteFunc)
        {
            CreateEncoder(config, context);
        }
    private:
        void CreateEncoder(const Config &config, std::shared_ptr<Context> &context) final;
        void DoResetEncoder() final;
        void TryEncode(std::shared_ptr<void> &&buffer) final;

        void PrepareEncode(std::shared_ptr<void> &&buffer);
        std::size_t EncodeFrame(void *packet_ptr, std::size_t header_size);

        NvEncCudaPtr _encoder;

    public:
        /* encoder tuning info, nvidia */

        // -preset
        const std::vector<GUID> encoder_preset = std::vector<GUID>{
            NV_ENC_PRESET_P1_GUID,
            NV_ENC_PRESET_P2_GUID,
            NV_ENC_PRESET_P3_GUID,
            NV_ENC_PRESET_P4_GUID,
            NV_ENC_PRESET_P5_GUID,
            NV_ENC_PRESET_P6_GUID,
            NV_ENC_PRESET_P7_GUID,
        };
        // -tuninginfo
        const std::vector<NV_ENC_TUNING_INFO> vTuningInfo = std::vector<NV_ENC_TUNING_INFO>{
            NV_ENC_TUNING_INFO_HIGH_QUALITY,
            NV_ENC_TUNING_INFO_LOW_LATENCY,
            NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY,
            NV_ENC_TUNING_INFO_LOSSLESS
        };
        // -multipass
        const std::vector<NV_ENC_MULTI_PASS> vMultiPass = std::vector<NV_ENC_MULTI_PASS>{
            NV_ENC_MULTI_PASS_DISABLED,
            NV_ENC_TWO_PASS_QUARTER_RESOLUTION,
            NV_ENC_TWO_PASS_FULL_RESOLUTION,
        };
        // -profile
        const std::vector<GUID> vProfile = std::vector<GUID>{
            GUID{},
            NV_ENC_CODEC_PROFILE_AUTOSELECT_GUID,
            NV_ENC_H264_PROFILE_BASELINE_GUID,
            NV_ENC_H264_PROFILE_MAIN_GUID,
            NV_ENC_H264_PROFILE_HIGH_GUID,
            NV_ENC_H264_PROFILE_HIGH_444_GUID,
            NV_ENC_H264_PROFILE_STEREO_GUID,
            NV_ENC_H264_PROFILE_PROGRESSIVE_HIGH_GUID,
            NV_ENC_H264_PROFILE_CONSTRAINED_HIGH_GUID,
            NV_ENC_HEVC_PROFILE_MAIN_GUID,
            NV_ENC_HEVC_PROFILE_MAIN10_GUID,
            NV_ENC_HEVC_PROFILE_FREXT_GUID,
            NV_ENC_AV1_PROFILE_MAIN_GUID,
        };
        // -rc
        const std::vector<NV_ENC_PARAMS_RC_MODE> vRcMode = std::vector<NV_ENC_PARAMS_RC_MODE>{
            NV_ENC_PARAMS_RC_CONSTQP,
            NV_ENC_PARAMS_RC_VBR,
            NV_ENC_PARAMS_RC_CBR,
        };

    };
}

#endif //INFRASTRUCTURE_CODEC_CUDA_ENCODER_HPP
