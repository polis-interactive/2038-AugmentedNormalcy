//
// Created by brucegoose on 2/18/23.
//

#ifndef INFRASTRUCTURE_CAMERA_LIBCAMERA_CAMERA_HPP
#define INFRASTRUCTURE_CAMERA_LIBCAMERA_CAMERA_HPP

#include "utils/buffers.hpp"

#include "camera.hpp"

#include <map>
#include <queue>
#include <vector>
#include <mutex>

#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/framebuffer_allocator.h>

namespace infrastructure {

    class LibcameraCamera : public std::enable_shared_from_this<LibcameraCamera>, public Camera {
    public:
        LibcameraCamera(const CameraConfig &config, CameraBufferCallback &&send_callback):
            Camera(config, std::move(send_callback))
        {
            CreateCamera(config);
        };
        ~LibcameraCamera() {
            Stop();
            teardownCamera();
            closeCamera();
        }
    private:
        void CreateCamera(const CameraConfig &config) final;
        void StartCamera() final;
        void StopCamera() final;

        void openCamera();
        void configureViewFinder(const CameraConfig &config);
        void setupBuffers(const int &camera_buffer_count);

        void makeRequests();
        void setControls();
        void requestComplete(libcamera::Request *request);
        void queueRequest(CameraBuffer *buffer);

        void closeCamera();
        void teardownCamera();

        std::unique_ptr<libcamera::CameraManager> _camera_manager;
        std::shared_ptr<libcamera::Camera> _camera;
        std::unique_ptr<libcamera::CameraConfiguration> _configuration;

        std::mutex _camera_stop_mutex;
        bool _camera_acquired = false;
        bool _camera_started = false;

        float _frame_rate = 0.0;
        float _lens_position = 0.0;

        std::unique_ptr<libcamera::FrameBufferAllocator> _allocator;
        std::map<std::string, libcamera::Stream *> _streams;
        std::map<libcamera::Stream *, std::queue<libcamera::FrameBuffer *>> _frame_buffers;
        std::map<libcamera::FrameBuffer *, std::vector<libcamera::Span<uint8_t>>> _mapped_buffers;
        std::vector<std::unique_ptr<libcamera::Request>> _requests;

        std::mutex _camera_buffers_mutex;
        std::set<CameraBuffer *> _camera_buffers;

        libcamera::ControlList _controls;
    };
}

#endif //INFRASTRUCTURE_CAMERA_LIBCAMERA_CAMERA_HPP
