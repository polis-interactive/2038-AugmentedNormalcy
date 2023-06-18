//
// Created by brucegoose on 6/18/23.
//

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
                    _press_count += 1;
                }
            case ButtonState::DEBOUNCED:
                // check if we have fully debounced; if so, move to the correct state
                if (elapsed > _millis_debounce_timeout) {
                    const auto new_state = button_is_pressed ? ButtonState::PRESSED : ButtonState::WAITING;
                    transitionState(new_state, now);
                    if (_press_count > 2) {
                        // reset the counter, emit a triple push
                        return emitAction(false);
                    }
                }
                break;
            case ButtonState::PRESSED:
                if (elapsed > _millis_hold_timeout) {
                    // button has been held long enough; emit a hold event based on number of presses
                    transitionState(ButtonState::HELD, now);
                    return emitAction(true);
                }
                if (!button_is_pressed) {
                    // button has been released; debounce and check for more presses
                    transitionState(ButtonState::DEBOUNCED, now);
                }
                break;
            case ButtonState::HELD:
                // wait for button to be released before tracking more presses
                if (!button_is_pressed) {
                    transitionState(ButtonState::OPEN, now);
                }
                break;
            case ButtonState::WAITING:
                if (elapsed > _millis_pressed_timeout) {
                    // button has been unpushed for long enough; emit a event based on number of presses
                    transitionState(ButtonState::OPEN, now);
                    return emitAction(false);
                }
                if (button_is_pressed) {
                    // button has been pressed; inc the counter, debounce and check for more presses
                    _press_count += 1;
                    transitionState(ButtonState::DEBOUNCED, now);
                }
                break;
        }
        return ButtonAction::NULL_ACTION;
    }
    void Button::transitionState(const ButtonState &new_state, const ClockPoint &transition_time) {
        _state = new_state;
        _last_state_change = transition_time;
    }

    ButtonAction Button::emitAction(const bool is_hold_action) {
        const auto send_press_count = _press_count;
        _press_count = 0;
        if (send_press_count > 2) {
            return is_hold_action ? ButtonAction::TRIPLE_HOLD : ButtonAction::TRIPLE_PUSH;
        } else if (send_press_count == 2) {
            return is_hold_action ? ButtonAction::DOUBLE_HOLD : ButtonAction::DOUBLE_PUSH;
        } else {
            return is_hold_action ? ButtonAction::SINGLE_HOLD : ButtonAction::SINGLE_PUSH;
        }
    }
}