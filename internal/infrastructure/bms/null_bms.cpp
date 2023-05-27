//
// Created by brucegoose on 5/26/23.
//

#include "null_bms.hpp"

namespace infrastructure {
    NullBms::NullBms(
        const infrastructure::BmsConfig &config, net::io_context &context, BmsMessageCallback &&post_callback
    ):
        Bms(config, context, std::move(post_callback))
    {}
    void NullBms::Start() {}
    void NullBms::Stop() {}
}
