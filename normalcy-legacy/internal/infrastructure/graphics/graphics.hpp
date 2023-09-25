//
// Created by brucegoose on 5/16/23.
//

#ifndef INFRASTRUCTURE_GRAPHICS_HPP
#define INFRASTRUCTURE_GRAPHICS_HPP

#include "utils/buffers.hpp"

#include "domain/headset_domain.hpp"

#if _HEADSET_GRAPHICS_ || _DISPLAY_GRAPHICS_

#define GLAD_GL_IMPLEMENTATION
#include "glad/glad_egl.h"
#include "glad/glad.h"

#define GLFW_INCLUDE_NONE 1
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_EGL 1
#define GLFW_NATIVE_INCLUDE_NONE 1
#include <GLFW/glfw3native.h>

struct EglBuffer
{
    EglBuffer() : fd(-1) {}
    int fd;
    size_t size;
    GLuint texture;
};

#endif

namespace infrastructure {

    enum class GraphicsType {
        HEADSET,
        DISPLAY,
        NONE,
    };

    struct GraphicsConfig {
        [[nodiscard]] virtual GraphicsType get_graphics_type() const = 0;
        [[nodiscard]] virtual std::pair<int, int> get_image_width_height() const = 0;
    };

    class Graphics {
    public:
        [[nodiscard]] static std::shared_ptr<Graphics> Create(const GraphicsConfig &config);
        Graphics(const GraphicsConfig &config);
        virtual void PostImage(std::shared_ptr<DecoderBuffer>&& buffer) = 0;

        virtual void PostGraphicsHeadsetState(const domain::HeadsetStates state) = 0;
        void Start() {
            StartGraphics();
        void PostGraphicsState(const domain::HeadsetStates state);
        }
        void Stop() {
            StopGraphics();
        }
    private:
        virtual void StartGraphics() = 0;
        virtual void StopGraphics() = 0;
    };

}

#endif //INFRASTRUCTURE_GRAPHICS_HPP
