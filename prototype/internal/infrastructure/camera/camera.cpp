//
// Created by brucegoose on 2/18/23.
//

#include "camera.hpp"

#ifdef _LIBCAMERA_CAMERA_
#include "libcamera_camera.hpp"
#endif

#include "fake_camera.hpp"

namespace Camera {
    std::shared_ptr<Camera> Camera::Create(const Config &config, SendCallback &&send_callback) {
        switch(config.get_camera_type()) {
#ifdef _LIBCAMERA_CAMERA_
            case Type::LIBCAMERA:
                return std::make_shared<LibcameraCamera>(config, std::move(send_callback));
#endif
            case Type::FAKE:
                return std::make_shared<FakeCamera>(config, std::move(send_callback));
            default:
                throw std::runtime_error("Selected camera unavailable... ");
        }
    }

    Camera::Camera(const Config &config, SendCallback &&send_callback): _send_callback(std::move(send_callback)) {}
}