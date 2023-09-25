//
// Created by brucegoose on 5/26/23.
//

#include "null_gpio.hpp"

namespace infrastructure {
    NullGpio::NullGpio(const infrastructure::GpioConfig &config, domain::ButtonActionCallback &&button_push_callback):
        Gpio(config, std::move(button_push_callback))
    {}
    void NullGpio::Start() {}
    void NullGpio::Stop() {}
}