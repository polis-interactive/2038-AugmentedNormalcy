//
// Created by brucegoose on 5/26/23.
//

#ifndef INFRASTRUCTURE_GPIO_HPP
#define INFRASTRUCTURE_GPIO_HPP


#include <memory>

namespace infrastructure {

    enum class GpioType {
        PIGPIO,
        NLL
    };

    struct GpioConfig {
        [[nodiscard]] virtual int get_button_pin() const = 0;
        [[nodiscard]] virtual int get_button_debounce_ms() const = 0;
    };

    class Gpio {
    public:
        [[nodiscard]] static std::shared_ptr<Gpio> Create(
            const GpioConfig &config
        );
        Gpio(const GpioConfig &config);
        virtual void Start() = 0;
        virtual void Stop() = 0;
    };

}

#endif //INFRASTRUCTURE_GPIO_HPP
