//
// Created by brucegoose on 1/30/23.
//

#ifndef DOMAIN_CAMERA_REPOSITORY_HPP
#define DOMAIN_CAMERA_REPOSITORY_HPP

#include "domain/camera/entity.hpp"

namespace domain {
    class CameraRepository {
    public:
        [[nodiscard]] virtual std::shared_ptr<domain::Camera> TryGetCamera(int id) = 0;
        [[nodiscard]] virtual std::shared_ptr<domain::Camera> TryGetBaseCamera() = 0;
        [[nodiscard]] virtual std::vector<std::shared_ptr<domain::Camera>> GetCameras() = 0;
        [[nodiscard]] virtual bool SetCamera(const std::shared_ptr<domain::Camera> &camera) = 0;
        [[nodiscard]] virtual bool SetCameras(const std::vector<std::shared_ptr<domain::Camera>> &cameras) = 0;
    };
}


#endif //DOMAIN_CAMERA_REPOSITORY_HPP
