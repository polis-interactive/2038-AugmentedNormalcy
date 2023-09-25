//
// Created by brucegoose on 5/26/23.
//

#include <stdexcept>

#include "gpio.hpp"

#ifdef _PIGIPO_
#include "pigpio_gpio.hpp"
#endif

#include "null_gpio.hpp"

namespace infrastructure {
    std::shared_ptr<Gpio> Gpio::Create(
            const infrastructure::GpioConfig &config, domain::ButtonActionCallback &&button_push_callback
    ) {
        switch(config.get_gpio_type()) {

#ifdef _PIGIPO_
            case GpioType::PIGPIO:
                return std::make_shared<PiGpio>(config, std::move(button_push_callback));
#endif
            case GpioType::NONE:
                return std::make_shared<NullGpio>(config, std::move(button_push_callback));
            default:
                throw std::runtime_error("Selected gpio type unavailable...");
        }
    }

    Gpio::Gpio(const infrastructure::GpioConfig &config, domain::ButtonActionCallback &&button_push_callback):
        _post_button_push_callback(std::move(button_push_callback))
    {}
}