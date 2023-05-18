//
// Created by brucegoose on 4/12/23.
//

#include "sw_decoder.hpp"

#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/dma-heap.h>
#include <linux/videodev2.h>

#include <cstring>
#include <sstream>

#include <chrono>
using namespace std::literals;
typedef std::chrono::high_resolution_clock Clock;

namespace infrastructure {

    int xioctl(int fd, unsigned long ctl, void *arg) {
        int ret, num_tries = 10;
        do
        {
            ret = ioctl(fd, ctl, arg);
        } while (ret == -1 && errno == EINTR && num_tries-- > 0);
        return ret;
    }

    const char SwDecoder::_device_name[] = "/dev/video10";

    SwDecoder::SwDecoder(const DecoderConfig &config, DecoderBufferCallback send_callback):
        Decoder(config, std::move(send_callback)),
        _width_height(config.get_decoder_width_height())
    {
        auto downstream_count = config.get_decoder_downstream_buffer_count();

        _decoder_fd = open(_device_name, O_RDWR, 0);

        v4l2_format fmt = {0};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        fmt.fmt.pix_mp.width = _width_height.first;
        fmt.fmt.pix_mp.height = _width_height.second;
        fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420;
        fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
        fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_REC709;
        fmt.fmt.pix_mp.num_planes = 1;
        fmt.fmt.pix_mp.plane_fmt[0].bytesperline = _width_height.first;
        fmt.fmt.pix_mp.plane_fmt[0].sizeimage = _width_height.first * _width_height.second * 3 / 2;

        if (xioctl(_decoder_fd, VIDIOC_S_FMT, &fmt))
            throw std::runtime_error("failed to set capture caps");

        setupDownstreamBuffers(downstream_count);
    }

    void SwDecoder::StartDecoder() {

        if (!_work_stop) {
            return;
        }

        _work_stop = false;

        auto self(shared_from_this());
        _work_thread = std::make_unique<std::thread>([this, self]() mutable { run(); });

    }

    void SwDecoder::StopDecoder() {
        if (_work_stop) {
            return;
        }

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
        // just in case we skipped above
        _work_stop = true;

        while(!_work_queue.empty()) {
            _work_queue.pop();
        }
    }

    void SwDecoder::PostJpegBuffer(std::shared_ptr<SizedBuffer> &&buffer) {
        if (buffer == nullptr || _work_stop) {
            return;
        }
        std::unique_lock<std::mutex> lock(_work_mutex);
        _work_queue.push(std::move(buffer));
        _work_cv.notify_one();
    }

    void SwDecoder::run() {

        struct jpeg_decompress_struct cinfo;
        struct jpeg_error_mgr jerr;
        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_decompress(&cinfo);

        while(!_work_stop) {
            std::shared_ptr<SizedBuffer> buffer;
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
            decodeBuffer(cinfo, std::move(buffer));
        }

        jpeg_destroy_decompress(&cinfo);
    }

    void SwDecoder::decodeBuffer(struct jpeg_decompress_struct &cinfo, std::shared_ptr<SizedBuffer> &&sz_buffer) {
        DecoderBuffer *buffer = nullptr;
        {
            std::unique_lock<std::mutex> lock(_downstream_buffers_mutex);
            if (!_downstream_buffers.empty()) {
                buffer = _downstream_buffers.front();
                _downstream_buffers.pop();
            }
        }
        if (buffer == nullptr) {
            return;
        }


        jpeg_mem_src(&cinfo, (unsigned char*)sz_buffer->GetMemory(), sz_buffer->GetSize());
        jpeg_read_header(&cinfo, TRUE);
        cinfo.out_color_space = JCS_YCbCr;
        cinfo.raw_data_out = TRUE;
        jpeg_start_decompress(&cinfo);

        int stride2 = _width_height.first / 2;
        uint8_t *Y = (uint8_t *) buffer->GetMemory();
        uint8_t *U = Y + _width_height.first * _width_height.second;
        uint8_t *V = U + stride2 * (_width_height.second / 2);

        JSAMPROW y_rows[16];
        JSAMPROW u_rows[8];
        JSAMPROW v_rows[8];
        JSAMPARRAY data[] = { y_rows, u_rows, v_rows };

        while (cinfo.output_scanline < _width_height.second) {
            for (int i = 0; i < 16 && cinfo.output_scanline < _width_height.second; ++i, Y += _width_height.first) {
                y_rows[i] = Y;
            }
            for (int i = 0; i < 8 && cinfo.output_scanline < _width_height.second; ++i, U += stride2, V += stride2) {
                u_rows[i] = U;
                v_rows[i] = V;
            }
            jpeg_read_raw_data(&cinfo, data, 16);
        }
        jpeg_finish_decompress(&cinfo);

        auto self(shared_from_this());
        auto output_buffer = std::shared_ptr<DecoderBuffer>(
                buffer, [this, self](DecoderBuffer * e) mutable {
                    queueDownstreamBuffer(e);
                }
        );
        _send_callback(std::move(output_buffer));
    }

    void SwDecoder::queueDownstreamBuffer(DecoderBuffer *d) {
        std::unique_lock<std::mutex> lock(_downstream_buffers_mutex);
        _downstream_buffers.push(d);
    }

    void SwDecoder::setupDownstreamBuffers(unsigned int request_downstream_buffers) {

        v4l2_requestbuffers reqbufs = {};
        reqbufs.count = request_downstream_buffers;
        reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        reqbufs.memory = V4L2_MEMORY_MMAP;
        if (xioctl(_decoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
            throw std::runtime_error("request for output buffers failed");
        } else if (reqbufs.count != request_downstream_buffers) {
            std::stringstream out_str;
            out_str << "Unable to return " << request_downstream_buffers << " capture buffers; only got " << reqbufs.count;
            throw std::runtime_error(out_str.str());
        }

        v4l2_plane planes[VIDEO_MAX_PLANES];
        v4l2_buffer buffer = {};
        v4l2_exportbuffer expbuf = {};

        for (int i = 0; i < request_downstream_buffers; i++) {

            /*
             * Get buffer
             */

            buffer = {};
            memset(planes, 0, sizeof(planes));
            buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            buffer.memory = V4L2_MEMORY_MMAP;
            buffer.index = i;
            buffer.length = 1;
            buffer.m.planes = planes;

            if (xioctl(_decoder_fd, VIDIOC_QUERYBUF, &buffer) < 0)
                throw std::runtime_error("failed to query output buffer");

            /*
             * mmap
             */

            auto capture_size = buffer.m.planes[0].length;
            auto capture_offset = buffer.m.planes[0].m.mem_offset;
            auto capture_mem = mmap(
                    nullptr, capture_size, PROT_READ | PROT_WRITE, MAP_SHARED, _decoder_fd, capture_offset
            );
            if (capture_mem == MAP_FAILED)
                throw std::runtime_error("failed to mmap output buffer");

            /*
             * export to dmabuf
             */

            memset(&expbuf, 0, sizeof(expbuf));
            expbuf.type = buffer.type;
            expbuf.index = buffer.index;
            expbuf.flags = O_RDWR;

            if (xioctl(_decoder_fd, VIDIOC_EXPBUF, &expbuf) < 0)
                throw std::runtime_error("failed to export the capture buffer");

            /*
             * setup proxy
             */

            auto downstream_buffer = new DecoderBuffer(buffer.index, expbuf.fd, capture_mem, capture_size);
            _downstream_buffers.push(downstream_buffer);
        }
    }

    SwDecoder::~SwDecoder() {
        Stop();
        teardownDownstreamBuffers();
        close(_decoder_fd);
    }

    void SwDecoder::teardownDownstreamBuffers() {
        while(!_downstream_buffers.empty()) {
            auto buffer = _downstream_buffers.front();
            _downstream_buffers.pop();
            munmap(buffer->GetMemory(), buffer->GetSize());
            close(buffer->GetFd());
            delete buffer;
        }

        v4l2_requestbuffers reqbufs = {};
        reqbufs.count = 0;
        reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        reqbufs.memory = V4L2_MEMORY_MMAP;
        if (xioctl(_decoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
            std::cout << "Failed to free capture buffers" << std::endl;
        }
    }
}