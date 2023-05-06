//
// Created by brucegoose on 5/4/23.
//

#include "sw_encoder.hpp"


namespace infrastructure {

    SwEncoder::SwEncoder(const EncoderConfig &config, SizedBufferCallback output_callback):
            _output_callback(std::move(output_callback)),
            _width_height(config.get_encoder_width_height())
    {
        auto downstream_count = config.get_encoder_downstream_buffer_count();
        setupDownstreamBuffers(downstream_count);
    }

    void SwEncoder::Start() {

        if (!_work_stop) {
            return;
        }

        _work_stop = false;

        auto self(shared_from_this());
        _work_thread = std::make_unique<std::thread>([this, s = std::move(self)]() mutable { run(); });

    }

    void SwEncoder::Stop() {
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

    void SwEncoder::PostCameraBuffer(std::shared_ptr<CameraBuffer> &&buffer) {
        if (buffer == nullptr || _work_stop) {
            return;
        }
        std::unique_lock<std::mutex> lock(_work_mutex);
        _work_queue.push(std::move(buffer));
        _work_cv.notify_one();
    }

    void SwEncoder::run() {

        struct jpeg_compress_struct cinfo;
        struct jpeg_error_mgr jerr;
        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_compress(&cinfo);

        cinfo.image_width = _width_height.first;
        cinfo.image_height = _width_height.second;
        cinfo.input_components = 3;
        cinfo.in_color_space = JCS_YCbCr;
        cinfo.restart_interval = 0;

        jpeg_set_defaults(&cinfo);
        cinfo.raw_data_in = TRUE;
        jpeg_set_quality(&cinfo, 192, TRUE);

        while(!_work_stop) {
            std::shared_ptr<CameraBuffer> buffer;
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
            encodeBuffer(cinfo, std::move(buffer));
        }
    }

    void SwEncoder::encodeBuffer(struct jpeg_compress_struct &cinfo, std::shared_ptr<CameraBuffer> &&cam_buffer) {
        EncoderBuffer *buffer = nullptr;
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
        buffer->ResetSize();
        jpeg_mem_dest(&cinfo, buffer->GetMemoryPointer(), buffer->GetSizePointer());
        jpeg_start_compress(&cinfo, TRUE);

        int stride2 = _width_height.first / 2;
        uint8_t *Y = (uint8_t *) cam_buffer->GetMemory();
        uint8_t *U = (uint8_t *)Y + _width_height.first * _width_height.second;
        uint8_t *V = (uint8_t *)U + stride2 * (_width_height.second / 2);
        uint8_t *Y_max = U - _width_height.first;
        uint8_t *U_max = V - stride2;
        uint8_t *V_max = U_max + stride2 * (_width_height.second / 2);

        JSAMPROW y_rows[16];
        JSAMPROW u_rows[8];
        JSAMPROW v_rows[8];

        for (uint8_t *Y_row = Y, *U_row = U, *V_row = V; cinfo.next_scanline < _width_height.second;)
        {
            for (int i = 0; i < 16; i++, Y_row += _width_height.first)
                y_rows[i] = std::min(Y_row, Y_max);
            for (int i = 0; i < 8; i++, U_row += stride2, V_row += stride2)
                u_rows[i] = std::min(U_row, U_max), v_rows[i] = std::min(V_row, V_max);

            JSAMPARRAY rows[] = { y_rows, u_rows, v_rows };
            jpeg_write_raw_data(&cinfo, rows, 16);
        }

        jpeg_finish_compress(&cinfo);
        auto self(shared_from_this());
        auto output_buffer = std::shared_ptr<EncoderBuffer>(
            buffer, [this, s = std::move(self)](EncoderBuffer * e) mutable {
                queueDownstreamBuffer(e);
            }
        );
        _output_callback(std::move(output_buffer));

    }


    void SwEncoder::queueDownstreamBuffer(EncoderBuffer *e) {
        std::unique_lock<std::mutex> lock(_downstream_buffers_mutex);
        _downstream_buffers.push(e);
    }

    void SwEncoder::setupDownstreamBuffers(unsigned int request_downstream_buffers) {
        const auto max_size = _width_height.first * _width_height.second * 3 / 2;
        for (int i = 0; i < request_downstream_buffers; i++) {
            auto downstream_buffer = new EncoderBuffer(max_size);
            _downstream_buffers.push(downstream_buffer);
        }
    }

    SwEncoder::~SwEncoder() {
        Stop();
        teardownDownstreamBuffers();
    }

    void SwEncoder::teardownDownstreamBuffers() {
        while(!_downstream_buffers.empty()) {
            auto buffer = _downstream_buffers.front();
            _downstream_buffers.pop();
            delete buffer;
        }
    }
}