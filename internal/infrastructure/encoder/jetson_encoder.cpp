//
// Created by brucegoose on 4/4/23.
//

#include <sys/mman.h>

#include "jetson_encoder.hpp"

namespace infrastructure {

    JetsonBuffer::JetsonBuffer(const std::pair<int, int> &width_height_tuple) {
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

        _size = _nvbuf_surf->surfaceList->planeParams.height[0] * _nvbuf_surf->surfaceList->planeParams.pitch[0];
        _size_1 = _nvbuf_surf->surfaceList->planeParams.height[1] * _nvbuf_surf->surfaceList->planeParams.pitch[1];
        _size_2 = _nvbuf_surf->surfaceList->planeParams.height[2] * _nvbuf_surf->surfaceList->planeParams.pitch[2];

        /*
        _nvbuf_surf->surfaceList->dataSize = GetSize();
        _nvbuf_surf->surfaceList->planeParams.psize[0] = _size;
        _nvbuf_surf->surfaceList->planeParams.psize[1] = _size_1;
        _nvbuf_surf->surfaceList->planeParams.psize[2] = _size_2;
        */


        std::cout << _nvbuf_surf->memType << std::endl;

        NvBufSurfaceMap(_nvbuf_surf, 0, 0, NVBUF_MAP_READ_WRITE);
        NvBufSurfaceSyncForCpu(_nvbuf_surf, 0, 0);
        NvBufSurfaceMap(_nvbuf_surf, 0, 1, NVBUF_MAP_READ_WRITE);
        NvBufSurfaceSyncForCpu(_nvbuf_surf, 0, 1);
        NvBufSurfaceMap(_nvbuf_surf, 0, 2, NVBUF_MAP_READ_WRITE);
        NvBufSurfaceSyncForCpu(_nvbuf_surf, 0, 2);

        std::cout << _nvbuf_surf->memType << std::endl;


        // just going to mmap it myself
        _memory = mmap(
                NULL,
                _size,
                PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                _nvbuf_surf->surfaceList->planeParams.offset[0]
        );
        if (_memory == MAP_FAILED) {
            std::cout << "FAILED TO MMAP AT ADDRESS" << std::endl;
        }
        _memory_1 = mmap(
                (uint8_t *) _memory + _size,
                _size_1,
                PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd,
                _nvbuf_surf->surfaceList->planeParams.offset[1]
        );
        if (_memory_1 == MAP_FAILED) {
            std::cout << "FAILED TO MMAP AT ADDRESS" << std::endl;
        }
        _memory_2 = mmap(
                (uint8_t *) _memory_1 + _size_1,
                _size_2,
                PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd,
                _nvbuf_surf->surfaceList->planeParams.offset[2]
        );
        if (_memory_2 == MAP_FAILED) {
            std::cout << "FAILED TO MMAP AT ADDRESS" << std::endl;
        }

        _nvbuf_surf->surfaceList->mappedAddr.addr[0] = _memory;
        _nvbuf_surf->surfaceList->mappedAddr.addr[1] = _memory_1;
        _nvbuf_surf->surfaceList->mappedAddr.addr[2] = _memory_2;
    }

    JetsonBuffer::~JetsonBuffer() {
        if (_memory != nullptr) {
            munmap(_memory, _size);
        }
        if (_memory_1 != nullptr) {
            munmap(_memory, _size_1);

        }
        if (_memory_2 != nullptr) {
            munmap(_memory, _size_2);

        }
        if (fd != -1) {
            NvBufSurf::NvDestroy(fd);
            fd = -1;
        }
    }

    std::atomic<int> Encoder::_last_encoder_number = { -1 };

    Encoder::Encoder(const EncoderConfig &config, SizedBufferCallback output_callback):
        _output_callback(std::move(output_callback)),
        _width_height(config.get_encoder_width_height())
    {
        // create downstream buffers
        auto downstream_max_size = getMaxJpegSize(_width_height);
        for (int i = 0; i < config.get_encoder_buffer_count(); i++) {
            _output_buffers.push(new CharBuffer(downstream_max_size));
        }
    }

    std::shared_ptr<SizedBuffer> Encoder::GetSizedBuffer() {
        std::unique_lock<std::mutex> lock(_input_buffers_mutex);
        auto jetson_buffer = _input_buffers.front();
        _input_buffers.pop();
        auto self(shared_from_this());
        auto buffer = std::shared_ptr<SizedBuffer>(
                (SizedBuffer *) jetson_buffer, [this, s = std::move(self), jetson_buffer](SizedBuffer *) mutable {
                    std::cout << "Do I get called multiple times?" << std::endl;
                    std::unique_lock<std::mutex> lock(_input_buffers_mutex);
                    _input_buffers.push(jetson_buffer);
                }
        );
        return std::move(buffer);
    }

    void Encoder::PostSizedBuffer(std::shared_ptr<SizedBuffer> &&buffer) {
        if (_work_stop) {
            return;
        }
        auto jetson_buffer = std::static_pointer_cast<JetsonBuffer>(buffer);
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
        for (int i = 0; i < _output_buffers.size(); i++) {
            _input_buffers.push(new JetsonBuffer(_width_height));
        }
        auto encoder_name = getUniqueJpegEncoderName();
        _jpeg_encoder = std::shared_ptr<NvJPEGEncoder>(
                NvJPEGEncoder::createJPEGEncoder(encoder_name.c_str()),
                [](NvJPEGEncoder *encoder) {
                    delete encoder;
                }
        );
        while(!_work_stop) {
            std::shared_ptr<JetsonBuffer> buffer;
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

    void Encoder::encodeBuffer(std::shared_ptr<JetsonBuffer> &&buffer) {
        if (_work_stop) {
            return;
        }
        // get output buffer
        CharBuffer *char_buffer;
        {
            std::unique_lock<std::mutex> lock(_output_buffers_mutex);
            char_buffer = _output_buffers.front();
            _output_buffers.pop();
        }
        auto ptr = (unsigned char *) char_buffer->GetMemory();
        auto sz = char_buffer->GetMaxSize();

        // do the encode
        auto ret = _jpeg_encoder->encodeFromFd(
            buffer->GetFd(), JCS_YCbCr, char_buffer->GetMemoryForWrite(), sz, 75
        );
        char_buffer->SetCurrentSize(sz);
        std::cout << sz << ", " << char_buffer->GetMaxSize() << "," << char_buffer->GetSize() << "?" << std::endl;
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