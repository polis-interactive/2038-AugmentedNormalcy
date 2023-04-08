//
// Created by brucegoose on 4/4/23.
//

#include <sys/mman.h>

#include "jetson_encoder.hpp"

using namespace std::literals;

namespace infrastructure {

    JetsonPlaneBuffer::JetsonPlaneBuffer(const std::pair<int, int> &width_height_tuple) {
        NvBufSurf::NvCommonAllocateParams params;
        /* Create PitchLinear output buffer for transform. */
        params.memType = NVBUF_MEM_SURFACE_ARRAY;
        params.width = width_height_tuple.first;
        params.height = width_height_tuple.second;
        params.layout = NVBUF_LAYOUT_PITCH;
        params.colorFormat = NVBUF_COLOR_FORMAT_YUV420;

        params.memtag = NvBufSurfaceTag_CAMERA;

        auto ret = NvBufSurf::NvAllocate(&params, 1, &fd);
        if (ret < 0) {
            std::cout << "Error allocating buffer: " << ret << std::endl;
        }


        ret = NvBufSurfaceFromFd(fd, (void**)(&_nvbuf_surf));
        if (ret != 0)
        {
            std::cout << "failed to get surface from fd" << std::endl;
        }

        NvBufSurfaceMap(_nvbuf_surf, 0, 0, NVBUF_MAP_READ_WRITE);
        NvBufSurfaceMap(_nvbuf_surf, 0, 1, NVBUF_MAP_READ_WRITE);
        NvBufSurfaceMap(_nvbuf_surf, 0, 2, NVBUF_MAP_READ_WRITE);

        _y_plane = std::make_shared<JetsonBuffer>(
            _nvbuf_surf->surfaceList->mappedAddr.addr[0],
            _nvbuf_surf->surfaceList->planeParams.height[0] * _nvbuf_surf->surfaceList->planeParams.pitch[0]
        );
        _u_plane = std::make_shared<JetsonBuffer>(
            _nvbuf_surf->surfaceList->mappedAddr.addr[1],
            _nvbuf_surf->surfaceList->planeParams.height[1] * _nvbuf_surf->surfaceList->planeParams.pitch[1]
        );
        _v_plane = std::make_shared<JetsonBuffer>(
            _nvbuf_surf->surfaceList->mappedAddr.addr[2],
            _nvbuf_surf->surfaceList->planeParams.height[2] * _nvbuf_surf->surfaceList->planeParams.pitch[2]
        );
    }

    JetsonPlaneBuffer::~JetsonPlaneBuffer() {
        if (_nvbuf_surf) {
            if (_nvbuf_surf->surfaceList->mappedAddr.addr[2]) {
                NvBufSurfaceUnMap(_nvbuf_surf, 0, 2);
            }
            if (_nvbuf_surf->surfaceList->mappedAddr.addr[1]) {
                NvBufSurfaceUnMap(_nvbuf_surf, 0, 1);
            }
            if (_nvbuf_surf->surfaceList->mappedAddr.addr[0]) {
                NvBufSurfaceUnMap(_nvbuf_surf, 0, 0);
            }
        }
        if (fd != -1) {
            NvBufSurf::NvDestroy(fd);
            fd = -1;
        }
    }

    std::shared_ptr<CharBuffer> LeakyPlaneBuffer::_buffer;

    std::atomic<int> Encoder::_last_encoder_number = { -1 };

    Encoder::Encoder(const EncoderConfig &config, SizedBufferCallback output_callback):
        _output_callback(std::move(output_callback)),
        _width_height(config.get_encoder_width_height())
    {
        // create plane buffers
        for (int i = 0; i < config.get_encoder_buffer_count(); i++) {
            _input_buffers.push(new JetsonPlaneBuffer(_width_height));
        }
        // create leaky buffer
        LeakyPlaneBuffer::initialize(_width_height);
        _leaky_upstream_buffer = std::make_shared<LeakyPlaneBuffer>();
        // create downstream buffers
        auto downstream_max_size = getMaxJpegSize(_width_height);
        for (int i = 0; i < config.get_encoder_buffer_count(); i++) {
            _output_buffers.push(new CharBuffer(downstream_max_size));
        }
    }

    std::shared_ptr<SizedBufferPool> Encoder::GetSizedBufferPool() {
        JetsonPlaneBuffer *jetson_plane_buffer;
        {
            std::unique_lock<std::mutex> lock(_input_buffers_mutex);
            jetson_plane_buffer = _input_buffers.front();
            if (jetson_plane_buffer) {
                _input_buffers.pop();
            }
        }
        if (!jetson_plane_buffer) {
            return _leaky_upstream_buffer;
        }
        jetson_plane_buffer->SyncCpu();
        auto self(shared_from_this());
        auto buffer = std::shared_ptr<SizedBufferPool>(
                (SizedBufferPool *) jetson_plane_buffer,
                [this, s = std::move(self), jetson_plane_buffer](SizedBufferPool *) mutable {
                    std::unique_lock<std::mutex> lock(_input_buffers_mutex);
                    _input_buffers.push(jetson_plane_buffer);
                }
        );
        return std::move(buffer);
    }

    void Encoder::PostSizedBufferPool(std::shared_ptr<SizedBufferPool> &&buffer) {
        if (_work_stop) {
            return;
        }
        auto plane_buffer = std::static_pointer_cast<PlaneBuffer>(buffer);
        if (plane_buffer->IsLeakyBuffer()) {
            return;
        }
        auto jetson_buffer = std::static_pointer_cast<JetsonPlaneBuffer>(plane_buffer);
        std::unique_lock<std::mutex> lock(_work_mutex);
        _work_queue.push(std::move(jetson_buffer));
        _work_cv.notify_one();
    }

    void Encoder::Start() {
        {
            std::unique_lock<std::mutex> lock(_work_mutex);
            _work_queue = {};
        }
        if (!_work_thread) {
            _work_stop = false;
            auto self(shared_from_this());
            _work_thread = std::make_unique<std::thread>([this, s = std::move(self)]() mutable {
                run();
            });
        }
    }

    void Encoder::run() {
        auto encoder_name = getUniqueJpegEncoderName();
        _jpeg_encoder = std::shared_ptr<NvJPEGEncoder>(
                NvJPEGEncoder::createJPEGEncoder(encoder_name.c_str()),
                [](NvJPEGEncoder *encoder) {
                    delete encoder;
                }
        );
        while(!_work_stop) {
            std::shared_ptr<JetsonPlaneBuffer> buffer;
            {
                std::unique_lock<std::mutex> lock(_work_mutex);
                _work_cv.wait(lock, [this]() {
                    return !_work_queue.empty() || _work_stop;
                });
                if (_work_stop) {
                    return;
                } else if (_work_queue.empty()) {
                    continue;
                }
                buffer = std::move(_work_queue.front());
                _work_queue.pop();
            }
            encodeBuffer(std::move(buffer));
        }
        _jpeg_encoder.reset();
    }

    void Encoder::encodeBuffer(std::shared_ptr<JetsonPlaneBuffer> &&buffer) {
        if (_work_stop) {
            return;
        }
        buffer->SyncGpu();
        // get output buffer
        CharBuffer *char_buffer;
        {
            std::unique_lock<std::mutex> lock(_output_buffers_mutex);
            char_buffer = _output_buffers.front();
            _output_buffers.pop();
        }

        // do the encode
        auto ret = _jpeg_encoder->encodeFromFd(
            buffer->GetFd(), JCS_YCbCr, char_buffer->GetMemoryForWrite(), char_buffer->GetSizeForWrite(), 75
        );
        // free the plane buffer asap
        buffer.reset();

        // if the encode was successful, push it downstream with a lambda to requeue it
        if (ret >= 0) {
            auto self(shared_from_this());
            auto output_buffer = std::shared_ptr<SizedBuffer>(
                    (SizedBuffer *) char_buffer, [this, s = std::move(self), char_buffer](SizedBuffer *) mutable {
                        std::unique_lock<std::mutex> lock(_output_buffers_mutex);
                        _output_buffers.push(char_buffer);
                    }
            );
            _output_callback(std::move(output_buffer));
        }
    }

    void Encoder::Stop() {
        if (_work_thread) {
            if (_work_thread->joinable()) {
                {
                    std::unique_lock<std::mutex> lock(_work_mutex);
                    _work_stop = true;
                    _work_cv.notify_one();
                }
                _work_thread->join();
            }
            _work_thread.reset();
        }
    }

    Encoder::~Encoder() {
        Stop();
        /*
         * empty work queue, to get here it should be empty though as those pointers
         * have a reference to shared_from_this...
         */
        {
            std::unique_lock<std::mutex> lock(_work_mutex);
            _work_queue = {};
        }
        /*
         * destroy jetson buffers; this will unmap them and clean up the dma file
         */
        {
            std::unique_lock<std::mutex> lock(_input_buffers_mutex);
            while (!_input_buffers.empty()) {
                auto jetson_buffer = _input_buffers.front();
                _input_buffers.pop();
                delete jetson_buffer;
            }
        }
        /*
         * destroy the char buffers
         */
        {
            std::unique_lock<std::mutex> lock(_output_buffers_mutex);
            while (!_output_buffers.empty()) {
                auto char_buffer = _output_buffers.front();
                _output_buffers.pop();
                delete char_buffer;
            }
        }
    }

}