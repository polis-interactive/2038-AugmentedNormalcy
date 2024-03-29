//
// Created by brucegoose on 5/26/23.
//

#include <stdexcept>
#include "pigpio_gpio.hpp"


namespace infrastructure {
    PiGpio::PiGpio(const infrastructure::GpioConfig &config, domain::ButtonActionCallback &&button_push_callback):
        Gpio(config, std::move(button_push_callback)),
        _button_gpio_pin(config.get_button_pin()),
        _millis_polling_timeout(config.get_button_polling_ms()),
        _button(
            config.get_button_debounce_ms(),
            config.get_button_hold_ms()
        )
    {
        int cfg = gpioCfgGetInternals();
        cfg |= PI_CFG_NOSIGHANDLER;  // (1<<10)
        gpioCfgSetInternals(cfg);
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
        if (!_work_stop) {
            return;
        }
        _work_stop = false;
        auto self(shared_from_this());
        _work_thread = std::make_unique<std::thread>([this, self]() mutable { run(); });
    }

    void PiGpio::run() {
        _button.Reset();
        while (!_work_stop) {
            const bool button_is_pushed = gpioRead(_button_gpio_pin) == 0;
            const auto button_action = _button.UpdateButtonState(button_is_pushed);
            if (button_action != domain::ButtonAction::NULL_ACTION) {
                _post_button_push_callback(button_action);
            }
            std::this_thread::sleep_for(_millis_polling_timeout);
        }
    }

    void PiGpio::Stop() {
        if (_work_stop) {
            return;
        }

        if (_work_thread) {
            if (_work_thread->joinable()) {
                _work_stop = true;
                _work_thread->join();
            }
            _work_thread.reset();
        }
        // just in case we skipped above
        _work_stop = true;
    }

    PiGpio::~PiGpio() {
        Stop();
        gpioTerminate();
    }
}