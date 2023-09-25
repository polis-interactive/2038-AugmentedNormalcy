//
// Created by brucegoose on 4/12/23.
//

#ifndef INFRASTRUCTURE_DECODER_SW_DECODER_HPP
#define INFRASTRUCTURE_DECODER_SW_DECODER_HPP

#include <memory>
#include <queue>
#include <mutex>
#include <map>
#include <atomic>
#include <thread>
#include <iostream>
#include <condition_variable>


#include <jpeglib.h>
#if JPEG_LIB_VERSION_MAJOR > 9 || (JPEG_LIB_VERSION_MAJOR == 9 && JPEG_LIB_VERSION_MINOR >= 4)
typedef size_t jpeg_mem_len_t;
#else
typedef unsigned long jpeg_mem_len_t;
#endif

#include "decoder.hpp"

namespace infrastructure {

    class SwDecoder: public std::enable_shared_from_this<SwDecoder>, public Decoder {
    public:
        static std::shared_ptr<SwDecoder>Create(
            const DecoderConfig &config, DecoderBufferCallback output_callback
        ) {
            auto decoder = std::make_shared<SwDecoder>(config, std::move(output_callback));
            return std::move(decoder);
        }
        void PostJpegBuffer(std::shared_ptr<SizedBuffer> &&buffer) override;
        SwDecoder(const DecoderConfig &config, DecoderBufferCallback output_callback);
        ~SwDecoder();
    private:

        void StartDecoder() override;
        void StopDecoder() override;

        void setupDownstreamBuffers(unsigned int request_downstream_buffers);
        void run();
        void decodeBuffer(struct jpeg_decompress_struct &cinfo, std::shared_ptr<SizedBuffer> &&buffer);
        void queueDownstreamBuffer(DecoderBuffer *d);
        void teardownDownstreamBuffers();

        static const char _device_name[];
        int _decoder_fd = -1;

        const std::pair<int, int> _width_height;

        std::mutex _work_mutex;
        std::condition_variable _work_cv;
        std::queue<std::shared_ptr<SizedBuffer>> _work_queue;
        std::unique_ptr<std::thread> _work_thread;
        std::atomic<bool> _work_stop = { true };

        std::mutex _downstream_buffers_mutex;
        std::queue<DecoderBuffer *> _downstream_buffers;

    };

}



#endif //INFRASTRUCTURE_DECODER_SW_DECODER_HPP
