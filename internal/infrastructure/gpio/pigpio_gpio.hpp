//
// Created by brucegoose on 5/26/23.
//

#ifndef INFRASTRUCTURE_GPIO_PIGPIO_GPIO_HPP
#define INFRASTRUCTURE_GPIO_PIGPIO_GPIO_HPP

#include "gpio.hpp"
#include "pigpio.h"

#include "utils/clock.hpp"

namespace infrastructure {

    class PiGpio: public Gpio, public std::enable_shared_from_this<PiGpio> {
    public:
        explicit PiGpio(const GpioConfig &config, std::function<void()> &&button_push_callback);
        void Start() override;
        void Stop() override;
        ~PiGpio();
    private:
        void handleButtonPush(int gpio, int level);
        const int _button_gpio_pin;
        const std::chrono::milliseconds _millis_debounce_timeout;
        std::chrono::time_point<Clock> _last_button_press;

    };

}

#endif //INFRASTRUCTURE_GPIO_PIGPIO_GPIO_HPP
