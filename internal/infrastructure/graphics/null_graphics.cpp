//
// Created by brucegoose on 5/16/23.
//

#include "null_graphics.hpp"

namespace infrastructure {
    NullGraphics::NullGraphics(const infrastructure::GraphicsConfig &conf):
            Graphics(conf)
    {}

    void NullGraphics::PostImage(std::shared_ptr<DecoderBuffer> &&buffer) {}
    void NullGraphics::StartGraphics() {}
    void NullGraphics::StopGraphics() {}
}