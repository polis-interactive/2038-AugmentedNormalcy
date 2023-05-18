//
// Created by brucegoose on 2/18/23.
//

#ifndef INFRASTRUCTURE_CAMERA_STRING_CAMERA_HPP
#define INFRASTRUCTURE_CAMERA_STRING_CAMERA_HPP

#include "camera.hpp"

#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <queue>
#include <vector>

namespace infrastructure {

    class StringCameraBuffer: public CameraBuffer {
    public:
        explicit StringCameraBuffer(const int &buffer_size);
        void SetBufferNumber(long &buffer_value);
    private:
        std::vector<uint8_t> _buffer_memory;
    };

    class StringCamera : public std::enable_shared_from_this<StringCamera>, public Camera {
    public:
        StringCamera(const CameraConfig &config, CameraBufferCallback &&send_callback);
        ~StringCamera();
    private:
        void CreateCamera(const CameraConfig &config) final;
        void StartCamera() final;
        void StopCamera() final;

        std::chrono::milliseconds _millis_frame_timeout;

        void queueCameraBuffer(StringCameraBuffer *buffer);
        std::mutex _camera_buffers_mutex;
        std::queue<StringCameraBuffer*> _camera_buffers;

        void run();
        std::unique_ptr<std::thread> _work_thread;
        std::atomic<bool> _work_stop = { true };
    };
}
#endif //INFRASTRUCTURE_CAMERA_STRING_CAMERA_HPP