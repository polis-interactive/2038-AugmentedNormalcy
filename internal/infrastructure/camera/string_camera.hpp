//
// Created by brucegoose on 2/18/23.
//

#ifndef INFRASTRUCTURE_CAMERA_FAKE_CAMERA_HPP
#define INFRASTRUCTURE_CAMERA_FAKE_CAMERA_HPP

#include "camera.hpp"

namespace infrastructure {
    class StringCamera : public Camera {
    public:
        StringCamera(const CameraConfig &config, CameraBufferCallback &&send_callback):
                Camera(config, std::move(send_callback))
        {
            CreateCamera(config);
        }
    private:
        void CreateCamera(const CameraConfig &config) final;
        void StartCamera() final;
        void StopCamera() final;
    };
}
#endif //INFRASTRUCTURE_CAMERA_FAKE_CAMERA_HPP