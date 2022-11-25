/*
* Copyright 2017-2022 NVIDIA Corporation.  All rights reserved.
*
* Please refer to the NVIDIA end user license agreement (EULA) associated
* with this source code for terms and conditions that govern your use of
* this software. Any use, reproduction, disclosure, or distribution of
* this software and related documentation outside the terms of the EULA
* is strictly prohibited.
*
*/

/**
*  This sample application demonstrates transcoding of an input video stream.
*  If requested by the user, the bit-depth of the decoded content will be
*  converted to the target bit-depth before encoding. The only supported
*  conversions are from 8 bit to 10 bit (per component) and vice versa.
*/

#include <cuda.h>
#include <cuda_runtime.h>
#include <iostream>
#include <memory>
#include <functional>
#include "NvEncoder/NvEncoderCuda.h"
#include "NvDecoder/NvDecoder.h"
#include "../../Utils/NvCodecUtils.h"
#include "../../Utils/NvEncoderCLIOptions.h"
#include "../../Utils/FFmpegDemuxer.h"
#include "../../Utils/ColorSpace.h"

#include "FramePresenter.h"
#include "FramePresenterGLX.h"


simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();

void ShowHelpAndExit(const char *szBadOption = NULL)
{
    bool bThrowError = false;
    std::ostringstream oss;
    if (szBadOption) 
    {
        oss << "Error parsing \"" << szBadOption << "\"" << std::endl;
        bThrowError = true;
    }
    oss << "Options:" << std::endl
        << "-i           input_file" << std::endl
        << "-o           output_file" << std::endl
        << "-ob          Bit depth of the output: 8 10" << std::endl
        << "-gpu         Ordinal of GPU to use" << std::endl
        ;
    oss << NvEncoderInitParam().GetHelpMessage(false, false, true);
    if (bThrowError)
    {
        throw std::invalid_argument(oss.str());
    }
    else
    {
        std::cout << oss.str();
        exit(0);
    }
}

void ParseCommandLine(int argc, char *argv[], char *szInputFileName, char *szOutputFileName, int &nOutBitDepth, int &iGpu, NvEncoderInitParam &initParam) 
{
    std::ostringstream oss;
    int i;
    for (i = 1; i < argc; i++)
    {
        if (!_stricmp(argv[i], "-h"))
        {
            ShowHelpAndExit();
        }
        if (!_stricmp(argv[i], "-i"))
        {
            if (++i == argc)
            {
                ShowHelpAndExit("-i");
            }
            sprintf(szInputFileName, "%s", argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-o"))
        {
            if (++i == argc)
            {
                ShowHelpAndExit("-o");
            }
            sprintf(szOutputFileName, "%s", argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-ob"))
        {
            if (++i == argc)
            {
                ShowHelpAndExit("-ob");
            }
            nOutBitDepth = atoi(argv[i]);
            if (nOutBitDepth != 8 && nOutBitDepth != 10) 
            {
                ShowHelpAndExit("-ob");
            }
            continue;
        }
        if (!_stricmp(argv[i], "-gpu")) 
        {
            if (++i == argc)
            {
                ShowHelpAndExit("-gpu");
            }
            iGpu = atoi(argv[i]);
            continue;
        }
        // Regard as encoder parameter
        if (argv[i][0] != '-') 
        {
            ShowHelpAndExit(argv[i]);
        }
        oss << argv[i] << " ";
        while (i + 1 < argc && argv[i + 1][0] != '-') 
        {
            oss << argv[++i] << " ";
        }
    }
    initParam = NvEncoderInitParam(oss.str().c_str());
}

int main(int argc, char **argv) {
    char szInFilePath[260] = "";
    char szOutFilePath[260] = "";
    int nOutBitDepth = 0;
    int iGpu = 0;
    try
    {
        using NvEncCudaPtr = std::unique_ptr<NvEncoderCuda, std::function<void(NvEncoderCuda*)>>;
        auto EncodeDeleteFunc = [](NvEncoderCuda *pEnc)
        {
            if (pEnc)
            {
                pEnc->DestroyEncoder();
                delete pEnc;
            }
        };

        // Delay instantiating the encoder until we've decoded some frames.
        NvEncCudaPtr pEnc(nullptr, EncodeDeleteFunc);

        NvEncoderInitParam encodeCLIOptions;
        ParseCommandLine(argc, argv, szInFilePath, szOutFilePath, nOutBitDepth, iGpu, encodeCLIOptions);

        CheckInputFile(szInFilePath);

        if (!*szOutFilePath) {
            sprintf(szOutFilePath, encodeCLIOptions.IsCodecH264() ? "out.h264" : encodeCLIOptions.IsCodecHEVC() ? "out.hevc" : "out.av1");
        }

        std::ifstream fpIn(szInFilePath, std::ifstream::in | std::ifstream::binary);
        if (!fpIn)
        {
            std::ostringstream err;
            err << "Unable to open input file: " << szInFilePath << std::endl;
            throw std::invalid_argument(err.str());
        }


        ck(cuInit(0));
        int nGpu = 0;
        ck(cuDeviceGetCount(&nGpu));
        if (iGpu < 0 || iGpu >= nGpu) {
            std::cout << "GPU ordinal out of range. Should be within [" << 0 << ", " << nGpu - 1 << "]" << std::endl;
            return 1;
        }
        CUdevice cuDevice = 0;
        ck(cuDeviceGet(&cuDevice, iGpu));
        char szDeviceName[80];
        ck(cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice));
        std::cout << "GPU in use: " << szDeviceName << std::endl;
        CUcontext cuContext = NULL;
        ck(cuCtxCreate(&cuContext, 0, cuDevice));

        // Prepare the demuxer and decoder for operations on the input stream.
        FFmpegDemuxer demuxer(szInFilePath);
        if (demuxer.GetChromaFormat() == AV_PIX_FMT_YUV444P || demuxer.GetChromaFormat() == AV_PIX_FMT_YUV444P10LE || demuxer.GetChromaFormat() == AV_PIX_FMT_YUV444P12LE)
        {
            std::cout << "Error: Sample app doesn't support YUV444" << std::endl;
            return 1;
        }
        NvDecoder dec(cuContext, true, FFmpeg2NvCodecId(demuxer.GetVideoCodec()), false, true);
        
        NvDecoder dec_out(cuContext, true, FFmpeg2NvCodecId(demuxer.GetVideoCodec()));
        std::cout << "Video Codec: " << demuxer.GetVideoCodec() << std::endl;
        std::cout << "Should be: " << AV_CODEC_ID_H264 << std::endl;
        
        int nWidth = (demuxer.GetWidth() + 1) & ~1;
        int nPitch = nWidth * 4;

        FramePresenterGLX gInstance(nWidth, demuxer.GetHeight());

        // Check whether we have valid NVIDIA libraries installed
        if (!gInstance.isVendorNvidia()) {
            std::cout<<"\nFailed to find NVIDIA libraries\n";
            return 0;
        }

        int nVideoBytes = 0, nFrameReturned = 0, nFrame = 0, nFrameReturned_out = 0, nFrame_out = 0, iMatrix = 0;
        uint8_t *pVideo = NULL, *pFrame = NULL, *pFrame_out = NULL;
        size_t max_frame_seen = 0;
        bool bOut10 = false;
        CUdeviceptr dpFrame = 0;
        do {
            demuxer.Demux(&pVideo, &nVideoBytes);
            nFrameReturned = dec.Decode(pVideo, nVideoBytes);

            for (int i = 0; i < nFrameReturned; i++)
            {
                pFrame = dec.GetFrame();
                if (!pEnc)
                {
                    // We've successfully decoded some frames; create an encoder now.

                    pEnc.reset(new NvEncoderCuda(cuContext, dec.GetWidth(), dec.GetHeight(), NV_ENC_BUFFER_FORMAT_NV12));

                    NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
                    NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
                    initializeParams.encodeConfig = &encodeConfig;
                    pEnc->CreateDefaultEncoderParams(&initializeParams, encodeCLIOptions.GetEncodeGUID(), encodeCLIOptions.GetPresetGUID(), encodeCLIOptions.GetTuningInfo());

                    encodeCLIOptions.SetInitParams(&initializeParams, NV_ENC_BUFFER_FORMAT_NV12);

                    pEnc->CreateEncoder(&initializeParams);
                }

                std::vector<std::vector<uint8_t>> vPacket;
                const NvEncInputFrame* encoderInputFrame = pEnc->GetNextInputFrame();
                NvEncoderCuda::CopyToDeviceFrame(cuContext,
                    pFrame,
                    dec.GetDeviceFramePitch(),
                    (CUdeviceptr)encoderInputFrame->inputPtr,
                    encoderInputFrame->pitch,
                    pEnc->GetEncodeWidth(),
                    pEnc->GetEncodeHeight(),
                    CU_MEMORYTYPE_DEVICE,
                    encoderInputFrame->bufferFormat,
                    encoderInputFrame->chromaOffsets,
                    encoderInputFrame->numChromaPlanes);
                pEnc->EncodeFrame(vPacket);
                
                nFrame += (int)vPacket.size();
                for (std::vector<uint8_t> &packet : vPacket)
                {
                    if (packet.size() > max_frame_seen) {
                        max_frame_seen = packet.size();
                        std::cout << "Max packet seen: " << packet.size() << std::endl;
                    }
                    nFrameReturned_out = dec_out.Decode(&packet[0], packet.size());
                    if (!nFrame_out && nFrameReturned_out)
                        LOG(INFO) << dec_out.GetVideoInfo();
                    for (int j = 0; j < nFrameReturned_out; j++) {
                        pFrame_out = dec_out.GetFrame();
                        gInstance.GetDeviceFrameBuffer(&dpFrame, &nPitch);
                        iMatrix = dec_out.GetVideoFormatInfo().video_signal_description.matrix_coefficients;
                        Nv12ToColor32<BGRA32>(pFrame_out, dec_out.GetWidth(), (uint8_t *)dpFrame, nPitch, dec_out.GetWidth(), dec_out.GetHeight(), iMatrix);
                        gInstance.ReleaseDeviceFrameBuffer();
                    }
                    nFrame_out += nFrameReturned_out;
                }
            }
        } while (nVideoBytes);

        if (pEnc)
        {
            std::vector<std::vector<uint8_t>> vPacket;
            pEnc->EndEncode(vPacket);
            nFrame += (int)vPacket.size();
            for (std::vector<uint8_t> &packet : vPacket)
            {
                std::cout << packet.size() << std::endl;
            }
            std::cout << std::endl;
        }

        fpIn.close();
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cout << ex.what();
        exit(1);
    }
    return 0;
}
