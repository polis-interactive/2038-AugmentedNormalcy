//
// Created by brucegoose on 5/27/23.
//

#ifndef DOMAIN_HEADSET_STATE_HPP
#define DOMAIN_HEADSET_STATE_HPP

#include <shared_mutex>

namespace domain {

    enum class HeadsetStates {
        CONNECTING,
        READY,
        RUNNING,
        PLUGGED_IN,
        DYING,
    };

    typedef std::pair<bool, HeadsetStates> StateTransition;

    class HeadsetState {
    public:
         [[nodiscard]] StateTransition PostBmsMessage(const BmsMessage &msg) {
            std::unique_lock lk(_state_mutex);
            const auto last_state = _state;
            if (msg.bms_is_plugged_in) {
                // bms is now plugged in
                _state = HeadsetStates::PLUGGED_IN;
            } else if (msg.bms_wants_shutdown) {
                // display a "plug me in" message
                _state = HeadsetStates::DYING;
            } else if (last_state == HeadsetStates::PLUGGED_IN || last_state == HeadsetStates::DYING) {
                // the display either is now unplugged or not dying. Not dying should never really occur without
                // going through a plugged in state, but might as well handle it. Might need to rethink this if
                // it starts flickering back and forth between not dying and dying
                _state = HeadsetStates::CONNECTING;
            }
            return { last_state != _state, _state };
        }
        [[nodiscard]] StateTransition PostTcpConnection(bool is_connected) {
            std::unique_lock lk(_state_mutex);
            const auto last_state = _state;
            if (!is_connected) {
                // we are no longer connected; check if we should do anything
                if (last_state == HeadsetStates::READY || last_state == HeadsetStates::RUNNING) {
                    // in this case, we expected to be connected; reset the state
                    _state = HeadsetStates::CONNECTING;
                }
            } else {
                if (last_state == HeadsetStates::CONNECTING) {
                    _state == HeadsetStates::READY;
                }
            }
            tcp_is_connected = is_connected;
            return { last_state != _state, _state };
        }
    private:
        mutable std::shared_mutex _state_mutex;
        bool tcp_is_connected = false;
        HeadsetStates _state = HeadsetStates::CONNECTING;
    };

}

#endif //DOMAIN_HEADSET_STATE_HPP