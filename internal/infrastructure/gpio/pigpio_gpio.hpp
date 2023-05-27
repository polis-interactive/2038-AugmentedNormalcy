//
// Created by brucegoose on 5/26/23.
//

#ifndef INFRASTRUCTURE_GPIO_PIGPIO_GPIO_HPP
#define INFRASTRUCTURE_GPIO_PIGPIO_GPIO_HPP

#include <thread>
#include <atomic>

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
        void run();
        const int _button_gpio_pin;
        const std::chrono::milliseconds _millis_polling_timeout;
        const std::chrono::milliseconds _millis_debounce_timeout;
        bool _last_button_is_pushed = false;
        std::unique_ptr<std::thread> _work_thread;
        std::atomic<bool> _work_stop = { true };
    };

}

#endif //INFRASTRUCTURE_GPIO_PIGPIO_GPIO_HPP
