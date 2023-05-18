//
// Created by brucegoose on 5/16/23.
//

#ifndef INFRASTRUCTURE_GRAPHICS_HPP
#define INFRASTRUCTURE_GRAPHICS_HPP

#include "utils/buffers.hpp"

namespace infrastructure {

    enum class GraphicsType {
        GLFW,
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
        void Start() {
            StartGraphics();
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
