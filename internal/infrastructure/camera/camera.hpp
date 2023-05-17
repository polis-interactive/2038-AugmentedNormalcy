//
// Created by brucegoose on 5/16/23.
//

#ifndef INFRASTRUCTURE_CAMERA_HPP
#define INFRASTRUCTURE_CAMERA_HPP

#include "utils/buffers.hpp"

namespace infrastructure {

    enum class CameraType {
        LIBCAMERA,
        FAKE,
        STRING,
    };

    struct CameraConfig {
        [[nodiscard]] virtual CameraType get_camera_type() const = 0;
        [[nodiscard]] virtual std::pair<int, int> get_camera_width_height() const = 0;
        // as opposed to 30
        [[nodiscard]] virtual int get_fps() const = 0;
        [[nodiscard]] virtual int get_camera_buffer_count() const = 0;
        [[nodiscard]] virtual float get_lens_position() const = 0;
    };


    class Camera {
    public:
        [[nodiscard]] static std::shared_ptr<Camera> Create(
            const CameraConfig &config, CameraBufferCallback &&send_callback
        );
        Camera(const CameraConfig &config, CameraBufferCallback &&send_callback);
        void Start() {
            StartCamera();
        }
        void Stop() {
            StopCamera();
        }
    protected:
        CameraBufferCallback _send_callback;
    private:
        virtual void CreateCamera(const CameraConfig &config) = 0;
        virtual void StartCamera() = 0;
        virtual void StopCamera() = 0;
    };
}

#endif //INFRASTRUCTURE_CAMERA_HPP
