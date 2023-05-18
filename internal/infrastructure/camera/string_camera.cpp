//
// Created by brucegoose on 2/18/23.
//

#include "string_camera.hpp"

typedef std::chrono::high_resolution_clock Clock;

namespace infrastructure {

    StringCameraBuffer::StringCameraBuffer(const int &buffer_size):
        _buffer_memory(buffer_size, 0),
        CameraBuffer(nullptr, nullptr, -1, buffer_size, 0)
    {
        _buffer = _buffer_memory.data();
    }

    void StringCameraBuffer::SetBufferNumber(long &buffer_value) {
        std::fill(_buffer_memory.begin(), _buffer_memory.end(), 0);
        ++buffer_value;
        for (int i = 0; i < sizeof(long); ++i) {
            _buffer_memory[63 - i] = (buffer_value >> (i * 8)) & 0xFF;
        }
    }

    StringCamera::StringCamera(const CameraConfig &config, CameraBufferCallback &&send_callback):
        Camera(config, std::move(send_callback))
    {
        CreateCamera(config);
    }
    void StringCamera::CreateCamera(const CameraConfig &config) {
        const auto width_height = config.get_camera_width_height();
        const auto camera_buffer_count = config.get_camera_buffer_count();
        const auto fps = config.get_fps();
        _millis_frame_timeout = std::chrono::milliseconds(
            static_cast<int>(1000.0f / fps)
        );
        const auto sz = width_height.first * width_height.second;
        for (int i = 0; i < camera_buffer_count; i++) {
            auto camera_buffer = new StringCameraBuffer(sz);
            _camera_buffers.push(camera_buffer);
        }
    }
    void StringCamera::StartCamera() {
        if (!_work_stop) {
            return;
        }
        _work_stop = false;
        auto self(shared_from_this());
        _work_thread = std::make_unique<std::thread>([this, self]() mutable { run(); });
    }
    void StringCamera::StopCamera() {
        if (_work_stop) {
            return;
        }
        if (_work_thread) {
            if (_work_thread->joinable()) {
                _work_stop = true;
                _work_thread->join();
            }
            _work_thread.reset();
        }
        // just in case we skipped above
        _work_stop = true;
    }
    void StringCamera::run() {
        long prog_cntr = 0;
        while(!_work_stop) {
            const auto start = Clock::now();
            StringCameraBuffer *buffer = nullptr;
            {
                std::unique_lock<std::mutex> lk(_camera_buffers_mutex);
                if (!_camera_buffers.empty()) {
                    buffer = _camera_buffers.front();
                    _camera_buffers.pop();
                }
            }
            if (buffer != nullptr) {
                buffer->SetBufferNumber(prog_cntr);
                auto self(shared_from_this());
                auto output_buffer = std::shared_ptr<StringCameraBuffer>(
                    buffer, [this, self](StringCameraBuffer *s) mutable {
                        queueCameraBuffer(s);
                    }
                );
                _send_callback(output_buffer);
            }
            const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
            std::this_thread::sleep_for(_millis_frame_timeout - duration);
        }
    }

    void StringCamera::queueCameraBuffer(StringCameraBuffer *s) {
        std::unique_lock<std::mutex> lock(_camera_buffers_mutex);
        _camera_buffers.push(s);
    }

    StringCamera::~StringCamera() {
        std::unique_lock<std::mutex> lock(_camera_buffers_mutex);
        while (!_camera_buffers.empty()) {
            _camera_buffers.pop();
        }
    }

}