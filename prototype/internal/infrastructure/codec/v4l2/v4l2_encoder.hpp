//
// Created by brucegoose on 2/5/23.
//

#ifndef INFRASTRUCTURE_CODEC_V4L2_ENCODER_HPP
#define INFRASTRUCTURE_CODEC_V4L2_ENCODER_HPP


#include <thread>

#include "v4l2_codec.hpp"


namespace Codec {

    class V4l2Encoder : public Encoder {
    public:
        V4l2Encoder(
            const Config &config, std::shared_ptr<Context> &context, SendCallback &&payload_sender
        ):
            Encoder(config, context, std::move(payload_sender))
        {
            CreateEncoder(config, context);
        }
        ~V4l2Encoder();
    private:
        struct BufferDescription
        {
            void *mem;
            std::size_t size;
            std::size_t index;
            std::size_t bytes_used;
        };

        void CreateEncoder(const Config &config, std::shared_ptr<Context> &context) final;
        void StartEncoder() final;
        void DoResetEncoder() final {};
        void TryEncode(std::shared_ptr<void> &&buffer) final;
        void DoStopEncoder() final;

        void SetupEncoder(const Config &config);
        void SetupBuffers(int camera_buffer_count, int downstream_buffers_count);

        void HandleDownstream(std::stop_token st) noexcept;
        bool WaitForEncoder();
        std::shared_ptr<BufferDescription> &GetDownstreamBuffer();
        void SendDownstreamBuffer(std::shared_ptr<BufferDescription> &downstream_buffer);
        void QueueDownstreamBuffer(std::shared_ptr<BufferDescription> &downstream_buffer);

        int xioctl(int fd, unsigned long ctl, void *arg);

        int _encoder_fd = -1;

        std::mutex _input_buffers_available_mutex;
        std::queue<uint32_t> _input_buffers_available;
        std::mutex _input_buffers_processing_mutex;
        std::queue<std::shared_ptr<void>> _input_buffers_processing;

        std::vector<std::shared_ptr<BufferDescription>> _downstream_buffers = {};
        std::unique_ptr<std::jthread> _downstream_thread;

        std::shared_ptr<BufferDescription> _dummy_out = {};
    };

}

#endif //INFRASTRUCTURE_CODEC_V4L2_ENCODER_HPP
