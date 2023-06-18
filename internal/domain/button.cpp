//
// Created by brucegoose on 6/18/23.
//

#include <iostream>

#include "button.hpp"

namespace domain {
    Button::Button(const int debounce_ms, const int hold_ms, const int pressed_ms):
        _millis_debounce_timeout(debounce_ms),
        _millis_hold_timeout(hold_ms),
        _millis_pressed_timeout(pressed_ms)
    {}

    ButtonAction Button::UpdateButtonState(const bool button_is_pressed) {
        const auto now = Clock::now();
        const auto elapsed = now - _last_state_change;
        switch (_state) {
            case ButtonState::OPEN:
                if (button_is_pressed) {
                    transitionState(ButtonState::DEBOUNCED, now);
                    return ButtonAction::SINGLE_PUSH;
                }
                break;
            case ButtonState::DEBOUNCED:
                // check if we have fully debounced; if so, move to the correct state
                if (elapsed > _millis_debounce_timeout) {
                    const auto new_state = button_is_pressed ? ButtonState::PRESSED : ButtonState::OPEN;
                    transitionState(new_state, now);
                }
                break;
            case ButtonState::PRESSED:
                if (elapsed > _millis_hold_timeout) {
                    // button has been held long enough; emit a hold event based on number of presses
                    transitionState(ButtonState::HELD, now);
                    return ButtonAction::SINGLE_HOLD;
                }
                if (!button_is_pressed) {
                    // button has been released; back to open!
                    transitionState(ButtonState::OPEN, now);
                }
                break;
            case ButtonState::HELD:
                // wait for button to be released before tracking more presses
                if (!button_is_pressed) {
                    transitionState(ButtonState::OPEN, now);
                }
                break;
        }
        return ButtonAction::NULL_ACTION;
    }
    void Button::transitionState(const ButtonState &new_state, const ClockPoint &transition_time) {
        _state = new_state;
        _last_state_change = transition_time;
    }

    void Button::Reset() {
        transitionState(ButtonState::OPEN, Clock::now());
    }
}