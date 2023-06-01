//
// Created by brucegoose on 5/16/23.
//

#ifndef INFRASTRUCTURE_GRAPHICS_NULL_GRAPHICS_HPP
#define INFRASTRUCTURE_GRAPHICS_NULL_GRAPHICS_HPP

#include "graphics.hpp"

namespace infrastructure {
    class NullGraphics: public Graphics {
    public:
        explicit NullGraphics(const GraphicsConfig &conf);
        void PostImage(std::shared_ptr<DecoderBuffer>&& buffer) override;
        void PostGraphicsHeadsetState(const domain::HeadsetStates state) override;
    private:
        void StartGraphics() override;
        void StopGraphics() override;
    };
}

#endif //INFRASTRUCTURE_GRAPHICS_NULL_GRAPHICS_HPP
