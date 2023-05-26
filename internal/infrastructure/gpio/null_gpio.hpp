//
// Created by brucegoose on 5/26/23.
//

#ifndef INFRASTRUCTURE_GPIO_NULL_GPIO_HPP
#define INFRASTRUCTURE_GPIO_NULL_GPIO_HPP

#include "gpio.hpp"

namespace infrastructure {
    class NullGpio: public Gpio {
    public:
        NullGpio(const GpioConfig &config, std::function<void()> &&button_push_callback);
        void Start() override;
        void Stop() override;
    };
}

#endif //INFRASTRUCTURE_GPIO_NULL_GPIO_HPP
