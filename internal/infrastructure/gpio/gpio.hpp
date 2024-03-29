//
// Created by brucegoose on 5/26/23.
//

#ifndef INFRASTRUCTURE_GPIO_HPP
#define INFRASTRUCTURE_GPIO_HPP


#include <memory>
#include <functional>

#include "domain/button.hpp"

namespace infrastructure {

    enum class GpioType {
        PIGPIO,
        NONE
    };

    struct GpioConfig {
        [[nodiscard]] virtual GpioType get_gpio_type() const = 0;
        [[nodiscard]] virtual int get_button_pin() const = 0;
        [[nodiscard]] virtual int get_button_debounce_ms() const = 0;
        [[nodiscard]] virtual int get_button_hold_ms() const = 0;
        [[nodiscard]] virtual int get_button_polling_ms() const = 0;
    };

    class Gpio {
    public:
        [[nodiscard]] static std::shared_ptr<Gpio> Create(
            const GpioConfig &config, domain::ButtonActionCallback &&button_push_callback
        );
        Gpio(const GpioConfig &config, domain::ButtonActionCallback &&button_push_callback);
        virtual void Start() = 0;
        virtual void Stop() = 0;
    protected:
        domain::ButtonActionCallback _post_button_push_callback;
    };

}

#endif //INFRASTRUCTURE_GPIO_HPP
