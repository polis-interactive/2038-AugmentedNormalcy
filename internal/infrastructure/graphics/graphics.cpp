//
// Created by brucegoose on 5/16/23.
//

#include "graphics.hpp"
#include "glfw_graphics.hpp"
#include "null_graphics.hpp"

namespace infrastructure {
    std::shared_ptr<Graphics> Graphics::Create(const GraphicsConfig &config) {
        switch(config.get_graphics_type()) {
            case GraphicsType::GLFW:
                return std::make_shared<GlfwGraphics>(config);
            case GraphicsType::NLL:
                return std::make_shared<NullGraphics>(config);
            default:
                throw std::runtime_error("Selected graphics unavailable... ");
        }
    }

    Graphics::Graphics(const GraphicsConfig &config) {}
}