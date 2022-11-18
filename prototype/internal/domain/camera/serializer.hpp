//
// Created by brucegoose on 1/31/23.
//

#ifndef DOMAIN_CAMERA_SERIALIZER_HPP
#define DOMAIN_CAMERA_SERIALIZER_HPP

#include "domain/camera/entity.hpp"
#include "third_party/nlohmann/json.hpp"

using namespace nlohmann;

namespace domain {
    [[nodiscard]] json CameraToJson(const Camera &c) {
        return json {
            { "id", c._id },
            { "is_base_camera", c._is_base_camera },
            { "resolution", {
                { "width", c._resolution.width },
                { "height", c._resolution.height }
            }},
            { "framerate", c._framerate },
            { "buffer_count", c._buffer_count },
            { "bin_output", c._bin_output }
        };
    }
    [[nodiscard]] std::shared_ptr<Camera>JsonToCamera(const json &j) {
        if (!j.is_object()) {
            return {nullptr};
        }
        if (j.at("id").get<int>() == 0 && !j.at("is_base_camera").get<bool>()) {
            return {nullptr};
        }
        auto c = std::make_shared<Camera>();
        j.at("id").get_to(c->_id);
        j.at("is_base_camera").get_to(c->_is_base_camera);
        j.at("resolution").at("width").get_to(c->_resolution.width);
        j.at("resolution").at("height").get_to(c->_resolution.height);
        j.at("framerate").get_to(c->_framerate);
        j.at("buffer_count").get_to(c->_buffer_count);
        j.at("bin_output").get_to(c->_bin_output);
        return c;
    }
}

#endif //DOMAIN_CAMERA_SERIALIZER_HPP
