//
// Created by brucegoose on 2/18/23.
//

#include "camera.hpp"

#ifdef _LIBCAMERA_CAMERA_
#include "libcamera_camera.hpp"
#endif

#include "fake_camera.hpp"

namespace infrastructure {
    std::shared_ptr<Camera> Camera::Create(const CameraConfig &config, SizedBufferCallback &&send_callback) {
        switch(config.get_camera_type()) {
#ifdef _LIBCAMERA_CAMERA_
            case CameraType::LIBCAMERA:
                return std::make_shared<LibcameraCamera>(config, std::move(send_callback));
#endif
            case CameraType::FAKE:
                return std::make_shared<FakeCamera>(config, std::move(send_callback));
            default:
                throw std::runtime_error("Selected camera unavailable... ");
        }
    }

    Camera::Camera(const CameraConfig &config, SizedBufferCallback &&send_callback):
        _send_callback(std::move(send_callback))
    {}
}