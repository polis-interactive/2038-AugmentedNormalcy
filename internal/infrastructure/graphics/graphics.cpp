//
// Created by brucegoose on 5/16/23.
//

#include "graphics.hpp"

#ifdef _HEADSET_GRAPHICS_
#include "headset_graphics.hpp"
#endif

#ifdef _DISPLAY_GRAPHICS_
#include "display_graphics.hpp"
#endif

#include "null_graphics.hpp"

namespace infrastructure {
    std::shared_ptr<Graphics> Graphics::Create(const GraphicsConfig &config) {
        switch(config.get_graphics_type()) {
#ifdef _HEADSET_GRAPHICS_
            case GraphicsType::HEADSET:
                return std::make_shared<HeadsetGraphics>(config);
#endif
#ifdef _DISPLAY_GRAPHICS_
            case GraphicsType::DISPLAY:
                return std::make_shared<DisplayGraphics>(config);
#endif
            case GraphicsType::NONE:
                return std::make_shared<NullGraphics>(config);
            default:
                throw std::runtime_error("Selected graphics unavailable... ");
        }
    }

    Graphics::Graphics(const GraphicsConfig &config) {}
}