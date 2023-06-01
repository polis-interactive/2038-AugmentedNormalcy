//
// Created by brucegoose on 5/26/23.
//

#ifndef INFRASTRUCTURE_BMS_NULL_BMS_HPP
#define INFRASTRUCTURE_BMS_NULL_BMS_HPP

#include "bms.hpp"

namespace infrastructure {
    class NullBms: public Bms {
    public:
        NullBms(const BmsConfig &config, net::io_context &context, domain::BmsMessageCallback &&post_callback);
        void Start() override;
        void Stop() override;
    };
}

#endif //INFRASTRUCTURE_BMS_NULL_BMS_HPP
