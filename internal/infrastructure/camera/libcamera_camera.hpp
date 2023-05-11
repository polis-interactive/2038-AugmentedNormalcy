//
// Created by brucegoose on 2/18/23.
//

#ifndef INFRASTRUCTURE_CAMERA_LIBCAMERA_CAMERA_HPP
#define INFRASTRUCTURE_CAMERA_LIBCAMERA_CAMERA_HPP

#include "utils/buffers.hpp"

#include <map>
#include <queue>
#include <vector>
#include <mutex>

#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/framebuffer_allocator.h>

namespace infrastructure {

    struct LibcameraConfig {
        [[nodiscard]] virtual std::pair<int, int> get_camera_width_height() const = 0;
        // as opposed to 30
        [[nodiscard]] virtual int get_fps() const = 0;
        [[nodiscard]] virtual int get_camera_buffer_count() const = 0;
    };

    class LibcameraCamera : public std::enable_shared_from_this<LibcameraCamera> {
    public:
        static std::shared_ptr<LibcameraCamera>Create(
            const LibcameraConfig &config, CameraBufferCallback &&output_callback
        ) {
            auto camera = std::make_shared<LibcameraCamera>(config, std::move(output_callback));
            return std::move(camera);
        }
        LibcameraCamera(const LibcameraConfig &config, CameraBufferCallback &&send_callback);
        void Start();
        void Stop();
        ~LibcameraCamera() {
            Stop();
            teardownCamera();
            closeCamera();
        }
    private:
        void createCamera(const LibcameraConfig &config);

        void openCamera();
        void configureViewFinder(const LibcameraConfig &config);
        void setupBuffers(const int &camera_buffer_count);

        void makeRequests();
        void setControls();
        void requestComplete(libcamera::Request *request);
        void queueRequest(CameraBuffer *buffer);

        void closeCamera();
        void teardownCamera();

        CameraBufferCallback _send_callback;

        std::unique_ptr<libcamera::CameraManager> _camera_manager;
        std::shared_ptr<libcamera::Camera> _camera;
        std::unique_ptr<libcamera::CameraConfiguration> _configuration;

        std::mutex _camera_stop_mutex;
        bool _camera_acquired = false;
        bool _camera_started = false;

        float _frame_rate = 0.0;

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
