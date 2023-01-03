//
// Created by bruce on 1/2/2023.
//

#ifndef AUGMENTEDNORMALCY_PROTOTYPE_ENCODER_HPP
#define AUGMENTEDNORMALCY_PROTOTYPE_ENCODER_HPP

#include "NvInterface/nvEncodeAPI.h"
#include "NvEncoder/NvEncoderCuda.h"

#include "utility/worker_thread.hpp"

#include "infrastructure/common.hpp"
#include "infrastructure/codec/bsp_packet.hpp"
#include "infrastructure/codec/codec.hpp"

namespace infrastructure {

    using NvEncCudaPtr = std::unique_ptr<NvEncoderCuda, std::function<void(NvEncoderCuda*)>>;
    const auto EncodeDeleteFunc = [](NvEncoderCuda *pEnc)
    {
        if (pEnc)
        {
            pEnc->DestroyEncoder();
            delete pEnc;
        }
    };

    class Encoder {
    public:
        Encoder(
            const CodecConfig &config, CodecContext &context, std::shared_ptr<PayloadSend> &&payload_sender
        ):
            _wt(utility::WorkerThread<GpuBuffer>::CreateWorkerThread(
                std::bind_front(&Encoder::TryEncode, this)
            )),
            _encoder(nullptr, EncodeDeleteFunc),
            _payload_sender(std::move(payload_sender))
        {
            CreateEncoder(config, context);
        }
        void Start() {
            _wt->Start();
        }
        void Stop() {
            _wt->Stop();
            StopEncoder();
        }
    private:
        void CreateEncoder(const CodecConfig &config, CodecContext &context);

        void ResetEncoder();
        void PrepareEncode(std::shared_ptr<GpuBuffer> &&gb);
        std::size_t EncodeFrame(uint8_t *packet_ptr, const std::size_t header_size);
        void StopEncoder();


        void TryEncode(std::shared_ptr<GpuBuffer> &&gb) {
            // this will release the gpu buffer
            PrepareEncode(std::move(gb));
            auto buffer = _payload_sender->GetBuffer();
            auto frame_size = EncodeFrame(buffer->data(), BspPacket::HeaderSize());
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
        uint16_t _session_number = 1;
        uint16_t _sequence_number = 0;
        uint16_t _timestamp = 0;
        std::shared_ptr<utility::WorkerThread<GpuBuffer>> _wt;
        NvEncCudaPtr _encoder;
        std::shared_ptr<PayloadSend> _payload_sender;
    };

    /* encoder tuning info, nvidia */

    // -preset
    const std::vector<GUID> encoder_preset = std::vector<GUID> {
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
    const std::vector<GUID> vProfile = std::vector<GUID> {
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
    const std::vector<NV_ENC_PARAMS_RC_MODE> vRcMode = std::vector<NV_ENC_PARAMS_RC_MODE> {
        NV_ENC_PARAMS_RC_CONSTQP,
        NV_ENC_PARAMS_RC_VBR,
        NV_ENC_PARAMS_RC_CBR,
    };

}

#endif //AUGMENTEDNORMALCY_PROTOTYPE_ENCODER_HPP
