//
// Created by brucegoose on 5/26/23.
//

#include <stdexcept>
#include "pigpio_gpio.hpp"


std::function<void(int, int)> gpioAlertHandler = nullptr;

void globalGpioAlertHandler(int gpio, int level, uint32_t tick) {
    if (gpioAlertHandler != nullptr) { // make sure it's assigned
        gpioAlertHandler(gpio, level);
    }
}

namespace infrastructure {
    PiGpio::PiGpio(const infrastructure::GpioConfig &config, std::function<void()> &&button_push_callback):
        Gpio(config, std::move(button_push_callback)),
        _button_gpio_pin(config.get_button_pin()),
        _millis_debounce_timeout(config.get_button_debounce_ms()),
        _last_button_press(Clock::now())
    {
        if (gpioInitialise() < 0) {
            throw std::runtime_error("Failed to startup gpio service");
        }
        if (gpioSetMode(_button_gpio_pin, PI_INPUT) < 0) {
            throw std::runtime_error("Failed to set pin to type input");
        }
        if (gpioSetPullUpDown(_button_gpio_pin, PI_PUD_UP) < 0) {
            throw std::runtime_error("Failed to set pin to pullup");

        }
    }

    void PiGpio::Start() {
        auto self(shared_from_this());
        gpioAlertHandler = [this, self](int gpio, int level) {
            handleButtonPush(gpio, level);
        };
        gpioSetAlertFunc(_button_gpio_pin, globalGpioAlertHandler);
    }

    void PiGpio::handleButtonPush(int gpio, int level) {
        if (gpio != _button_gpio_pin) {
            return;
        }
        const auto now = Clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - _last_button_press);
        if (duration < _millis_debounce_timeout) {
            // button debounced; ignore
            return;
        }
        // button push on low level
        if (level == 0) {
            _post_button_push_callback();
        }
        _last_button_press = now;
    }

    void PiGpio::Stop() {
        gpioSetAlertFunc(_button_gpio_pin, nullptr);
        gpioAlertHandler = nullptr;
    }

    PiGpio::~PiGpio() {
        gpioSetAlertFunc(_button_gpio_pin, nullptr);
        gpioAlertHandler = nullptr;
        gpioTerminate();
    }
}