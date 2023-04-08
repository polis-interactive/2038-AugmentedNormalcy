//
// Created by brucegoose on 4/4/23.
//

#ifndef INFRASTRUCTURE_ENCODER_JETSON_ENCODER_HPP
#define INFRASTRUCTURE_ENCODER_JETSON_ENCODER_HPP

#include <memory>
#include <utility>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <cmath>

#include "NvJpegEncoder.h"
#include "NvBufSurface.h"

#include "utils/buffers.hpp"



/*
 * for the sake of time, not going to provide an alternative to use on linux; just get the code building locally
 *      then test on the jetson
 */

namespace infrastructure {

    struct EncoderConfig {
        [[nodiscard]] virtual unsigned int get_encoder_buffer_count() const = 0;
        [[nodiscard]] virtual std::pair<int, int> get_encoder_width_height() const = 0;
    };

    class JetsonBuffer: public SizedBuffer {
    public:
        JetsonBuffer(void *memory, std::size_t size): _memory(memory), _size(size) {}
        [[nodiscard]] void * GetMemory() final {
            return _memory;
        }
        [[nodiscard]] std::size_t GetSize() final {
            return _size;
        }
    private:
        void *_memory;
        const std::size_t _size;
    };

    class JetsonPlaneBuffer: public SizedBufferPool {
    public:
        explicit JetsonPlaneBuffer(const std::pair<int, int> &width_height_tuple);
        [[nodiscard]] int GetFd() const {
            return fd;
        }
        [[nodiscard]] std::shared_ptr<SizedBuffer> GetSizedBuffer() final {
            switch (++next_plane) {
                case 0:
                    return _y_plane;
                case 1:
                    return _u_plane;
                case 2:
                    return _v_plane;
                default:
                    return nullptr;
            }
        }
        void SyncCpu() {
            NvBufSurfaceSyncForCpu(_nvbuf_surf, 0, 0);
            NvBufSurfaceSyncForCpu(_nvbuf_surf, 0, 1);
            NvBufSurfaceSyncForCpu(_nvbuf_surf, 0, 2);
            next_plane = -1;
        }
        void SyncGpu() {
            NvBufSurfaceSyncForDevice(_nvbuf_surf, 0, 0);
            NvBufSurfaceSyncForDevice(_nvbuf_surf, 0, 1);
            NvBufSurfaceSyncForDevice(_nvbuf_surf, 0, 2);
        }
        ~JetsonPlaneBuffer();
    private:
        int fd = -1;
        int next_plane = -1;
        std::shared_ptr<JetsonBuffer> _y_plane = nullptr;
        std::shared_ptr<JetsonBuffer> _u_plane = nullptr;
        std::shared_ptr<JetsonBuffer> _v_plane = nullptr;
        NvBufSurface *_nvbuf_surf = nullptr;
    };

    class CharBuffer: public SizedBuffer {
    public:
        explicit CharBuffer(const std::size_t buffer_size):
            _max_buffer_size(buffer_size)
        {
            _buffer = new unsigned char[buffer_size];
        };
        [[nodiscard]] void *GetMemory() override {
            return _buffer;
        };
        [[nodiscard]] std::size_t GetSize() override {
            return _used_size;
        };
        [[nodiscard]] unsigned char ** GetMemoryForWrite() {
            return &_buffer;
        }
        [[nodiscard]] std::size_t &GetSizeForWrite() {
            _used_size = _max_buffer_size;
            return _used_size;
        }
        ~CharBuffer() {
            delete[] _buffer;
        }
    private:
        unsigned char *_buffer;
        std::size_t _used_size = 0;
        const std::size_t _max_buffer_size;
    };

    class Encoder: public std::enable_shared_from_this<Encoder>, public SizedPlaneBufferPool {
    public:
        [[nodiscard]] static std::shared_ptr<Encoder> Create(
            const EncoderConfig &config, SizedBufferCallback output_callback
        ) {
            auto encoder = std::make_shared<Encoder>(config, std::move(output_callback));
            encoder->Start();
            return std::move(encoder);
        }
        [[nodiscard]] std::shared_ptr<SizedBufferPool> GetSizedBufferPool() override;
        void PostSizedBufferPool(std::shared_ptr<SizedBufferPool> &&buffer) override;
        void Start();
        void Stop();
        Encoder(const EncoderConfig &config, SizedBufferCallback output_callback);
        ~Encoder();
    private:
        void run();
        void encodeBuffer(std::shared_ptr<JetsonPlaneBuffer> &&buffer);

        static int getMaxJpegSize(const std::pair<int, int> &width_height_tuple) {
            auto [width, height] = width_height_tuple;
            // given by nvidia
            return std::ceil(width * height * 3 / 2);
        }
        static std::string getUniqueJpegEncoderName() {
            auto this_encoder_number = ++_last_encoder_number;
            return "jetson_jpeg_encoder_" + std::to_string(this_encoder_number);
        }
        static std::atomic<int> _last_encoder_number;

        std::queue<JetsonPlaneBuffer *> _input_buffers;
        std::mutex _input_buffers_mutex;

        std::unique_ptr<std::thread> _work_thread;
        std::atomic<bool> _work_stop = { true };
        std::mutex _work_mutex;
        std::condition_variable _work_cv;
        std::queue<std::shared_ptr<JetsonPlaneBuffer>> _work_queue;

        std::shared_ptr<NvJPEGEncoder> _jpeg_encoder;

        SizedBufferCallback _output_callback;
        std::queue<CharBuffer *> _output_buffers;
        std::mutex _output_buffers_mutex;

        const std::pair<int, int> _width_height;
    };
}

#endif //INFRASTRUCTURE_ENCODER_JETSON_ENCODER_HPP
