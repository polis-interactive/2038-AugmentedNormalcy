//
// Created by brucegoose on 2/5/23.
//

#ifndef INFRASTRUCTURE_CODEC_V4L2_ENCODER_HPP
#define INFRASTRUCTURE_CODEC_V4L2_ENCODER_HPP

#include "v4l2_codec.hpp"


namespace Codec {

    class V4l2Encoder : public Encoder {
    public:
        V4l2Encoder(
            const Config &config, std::shared_ptr<Context> &context, std::shared_ptr<PayloadSend> &&payload_sender
        ):
            Encoder(config, context, std::move(payload_sender))
        {
            CreateEncoder(config, context);
        }
    private:
        static constexpr int NUM_CAPTURE_BUFFERS = 8;

        void CreateEncoder(const Config &config, std::shared_ptr<Context> &context) final;
        void DoResetEncoder() final;
        void PrepareEncode(std::shared_ptr<void> &&buffer) final;
        std::size_t EncodeFrame(void *packet_ptr, std::size_t header_size) final;

        int xioctl(int fd, unsigned long ctl, void *arg);
        void SetupEncoder(const Config &config);
        void SetupBuffers();
        bool WaitForEncoder();

        int _encoder_fd;

        struct BufferDescription
        {
            void *mem;
            size_t size;
        };
        BufferDescription _upstream_buffers[NUM_CAPTURE_BUFFERS];
        int _upstream_buffers_count;
        std::deque<std::shared_ptr<void>> _upstream_buffers_queue;
        std::mutex _upstream_buffers_mutex;

    };

}

#endif //INFRASTRUCTURE_CODEC_V4L2_ENCODER_HPP
