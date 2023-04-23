//
// Created by brucegoose on 2/18/23.
//

#ifndef INFRASTRUCTURE_CAMERA_LIBCAMERA_CAMERA_HPP
#define INFRASTRUCTURE_CAMERA_LIBCAMERA_CAMERA_HPP

#include "camera.hpp"

#include <map>
#include <queue>
#include <vector>
#include <mutex>

#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/framebuffer_allocator.h>

namespace infrastructure {

    struct CameraPlaneBuffer: public SizedBuffer {
    public:
        CameraPlaneBuffer(void *buffer, std::size_t size):
            _buffer(buffer),
            _size(size)
        {}
        [[nodiscard]] void *GetMemory() final {
            return _buffer;
        }
        [[nodiscard]] std::size_t GetSize() final {
            return _size;
        };
    private:
        void *_buffer;
        std::size_t _size;
    };

    struct CameraBuffer: public SizedBufferPool {
        /*
         * TODO: these objects should live longer; currently, they are created on every request; can cache them
         *  in a map by FD
         */
        CameraBuffer(
                void *request, void *buffer, int fd, std::size_t size, int64_t timestamp_us
        ):
            _request(request), _buffer(buffer), _fd(fd), _size(size), _timestamp_us(timestamp_us)
        {}
        [[nodiscard]] std::shared_ptr<SizedBuffer> GetSizedBuffer() final {
            switch (++next_plane) {
#if _AN_PLATFORM_ == PLATFORM_RPI
                case 0:
                    return std::make_shared<CameraPlaneBuffer>(_buffer, _size * 2 / 3);
                case 1:
                    return std::make_shared<CameraPlaneBuffer>((uint8_t *)_buffer + _size * 2 / 3, _size / 6);
                case 2:
                    return std::make_shared<CameraPlaneBuffer>(
                        (uint8_t *)_buffer + _size * 2 / 3 + _size / 6, _size / 6);
#else
                case 0:
                    return std::make_shared<CameraPlaneBuffer>(_buffer, _size);
#endif
                default:
                    return nullptr;
            }
        };
        [[nodiscard]] void *GetRequest() const {
            return _request;
        }
        [[nodiscard]] int GetFd() const {
            return _fd;
        }

    private:
        void *_request;
        void * _buffer;
        int next_plane = -1;
        int _fd;
        std::size_t _size;
        int64_t _timestamp_us;
    };

    class LibcameraCamera : public Camera, public std::enable_shared_from_this<LibcameraCamera> {
    public:
        LibcameraCamera(const CameraConfig &config, SizedBufferPoolCallback &&send_callback):
            Camera(config, std::move(send_callback))
        {
            CreateCamera(config);
        };
        ~LibcameraCamera() {
            StopCamera();
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
