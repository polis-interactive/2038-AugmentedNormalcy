//
// Created by brucegoose on 2/18/23.
//

#ifndef INFRASTRUCTURE_CAMERA_HPP
#define INFRASTRUCTURE_CAMERA_HPP

#include <utility>

#include "utils/buffers.hpp"

namespace infrastructure {

    enum class CameraType {
        LIBCAMERA,
        FAKE,
    };

    struct CameraConfig {
        [[nodiscard]] virtual std::pair<int, int> get_camera_width_height() const = 0;
        // as opposed to 30
        [[nodiscard]] virtual int get_fps() const = 0;
        [[nodiscard]] virtual CameraType get_camera_type() const = 0;
        [[nodiscard]] virtual int get_camera_buffer_count() const = 0;
    };

    class Camera {
    public:
        [[nodiscard]] static std::shared_ptr<Camera> Create(
            const CameraConfig &config, SizedBufferPoolCallback &&send_callback
        );
        Camera(const CameraConfig &config, SizedBufferPoolCallback &&send_callback);
        void Start() {
            StartCamera();
        }
        void Stop() {
            StopCamera();
        }
    protected:
        SizedBufferPoolCallback _send_callback;
    private:
        virtual void CreateCamera(const CameraConfig &config) = 0;
        virtual void StartCamera() = 0;
        virtual void StopCamera() = 0;
    };
}

#endif //INFRASTRUCTURE_CAMERA_HPP
