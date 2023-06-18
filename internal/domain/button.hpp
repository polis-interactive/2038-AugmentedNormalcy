//
// Created by brucegoose on 6/18/23.
//

#ifndef DOMAIN_BUTTON_HPP
#define DOMAIN_BUTTON_HPP

#include <functional>
#include "utils/clock.hpp"

namespace domain {

    enum class ButtonAction {
        NULL_ACTION,
        SINGLE_PUSH,
        SINGLE_HOLD
    };

    typedef std::function<void(const domain::ButtonAction)> ButtonActionCallback;

    class Button {
    public:
        Button(const int debounce_ms, const int hold_ms, const int pressed_ms);
        void Reset();
        ButtonAction UpdateButtonState(const bool button_is_pressed);
        Button() = delete;
        Button (const Button&) = delete;
        Button& operator= (const Button&) = delete;
    private:
        const std::chrono::milliseconds _millis_debounce_timeout;
        const std::chrono::milliseconds _millis_pressed_timeout;
        const std::chrono::milliseconds _millis_hold_timeout;

        ButtonAction emitAction(const bool is_hold_action);

        // only pressed and waiting can emit events; otherwise, sends null action
        enum class ButtonState {
            OPEN,
            DEBOUNCED,
            PRESSED,
            HELD, // state after pressed if emitted a hold event
        };
        void transitionState(const ButtonState &new_state, const ClockPoint &transition_time);
        ButtonState _state = ButtonState::OPEN;
        ClockPoint _last_state_change;


    };

}

#endif