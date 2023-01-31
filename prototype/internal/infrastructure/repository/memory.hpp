//
// Created by brucegoose on 1/31/23.
//

#ifndef AUGMENTEDNORMALCY_PROTOTYPE_MEMORY_HPP
#define AUGMENTEDNORMALCY_PROTOTYPE_MEMORY_HPP

#include "domain/camera/serializer.hpp"
#include "infrastructure/repository/base.hpp"
#include "third_party/nlohmann/json.hpp"

using namespace nlohmann;

namespace infrastructure {
    class MemoryRepository : public BaseRepository {
    public:
        [[nodiscard]] std::shared_ptr<domain::Camera> TryGetCamera(int id) override {
            auto j_store = _camera_store.find(id);
            if (j_store == _camera_store.end()) {
                return {nullptr};
            }
            return domain::JsonToCamera(*j_store);
        };
        [[nodiscard]] std::shared_ptr<domain::Camera> TryGetBaseCamera() override {
            if (!_base_camera.is_object()) {
                return {nullptr};
            }
            return domain::JsonToCamera(_base_camera);
        };
        [[nodiscard]] std::vector<std::shared_ptr<domain::Camera>> GetCameras() override {
            std::vector<std::shared_ptr<domain::Camera>> c_store;
            for (const auto &[_, j] : _camera_store) {
                auto c = domain::JsonToCamera(j);
                if (c != nullptr) {
                    c_store.push_back(c);
                }
            }
            return c_store;
        };
        [[nodiscard]] bool SetCamera(const std::shared_ptr<domain::Camera> &camera) override {
            const domain::Camera &c = *camera;
            json j = domain::CameraToJson(c);
            auto j_store = _camera_store.find(c._id);
            if (j_store == _camera_store.end()) {
                _camera_store.insert({ c._id, j });
            } else {
                j_store->second = j;
            }
            return true;
        };
        [[nodiscard]] bool SetCameras(const std::vector<std::shared_ptr<domain::Camera>> &cameras) override {
            auto ret = true;
            for(const auto &c: cameras) {
                ret = SetCamera(c);
            }
            return ret;
        };
    private:
        std::map<int, json> _camera_store;
        json _base_camera;
    };
}

#endif //AUGMENTEDNORMALCY_PROTOTYPE_MEMORY_HPP
