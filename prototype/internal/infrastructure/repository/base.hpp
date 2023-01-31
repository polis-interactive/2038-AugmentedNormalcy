//
// Created by brucegoose on 1/31/23.
//

#ifndef INFRASTRUCTURE_REPOSITORY_BASE_HPP
#define INFRASTRUCTURE_REPOSITORY_BASE_HPP

#include "domain/camera/repository.hpp"

namespace infrastructure {
    class BaseRepository : public domain::CameraRepository {};
}

#endif //INFRASTRUCTURE_REPOSITORY_BASE_HPP
