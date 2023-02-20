//
// Created by brucegoose on 2/18/23.
//

#ifndef INFRASTRUCTURE_CAMERA_HPP
#define INFRASTRUCTURE_CAMERA_HPP

#include <utility>

#include "../common.hpp"

namespace Camera {
    enum class Type {
        LIBCAMERA,
        FAKE,
    };

    struct Config {
        [[nodiscard]] virtual std::pair<int, int> get_camera_width_height() const = 0;
        // as opposed to 30
        [[nodiscard]] virtual int get_fps() const = 0;
        [[nodiscard]] virtual Type get_camera_type() const = 0;
        [[nodiscard]] virtual int get_camera_buffer_count() const = 0;
    };

    class Camera {
    public:
        [[nodiscard]] static std::shared_ptr<Camera> Create(
            const Config &config, SendCallback &&send_callback
        );
        Camera(const Config &config, SendCallback &&send_callback);
        void Start() {
            StartCamera();
        }
        void Stop() {
            StopCamera();
        }
    protected:
        SendCallback _send_callback;
    private:
        virtual void CreateCamera(const Config &config) = 0;
        virtual void StartCamera() = 0;
        virtual void StopCamera() = 0;
    };
}

#endif //INFRASTRUCTURE_CAMERA_HPP
