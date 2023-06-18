//
// Created by brucegoose on 5/27/23.
//

#ifndef DOMAIN_HEADSET_DOMAIN_HPP
#define DOMAIN_HEADSET_DOMAIN_HPP

#include <mutex>
#include <shared_mutex>

#include "message.hpp"
#include "button.hpp"


namespace domain {

    enum class HeadsetStates {
        CONNECTING,
        READY,
        RUNNING,
        CLOSING,
    };

    typedef std::pair<bool, HeadsetStates> StateTransition;

    class HeadsetState {
    public:
        [[nodiscard]] HeadsetStates GetState() const {
            std::shared_lock lk(_state_mutex);
            return _state;

        }
        void PostState(HeadsetStates state) {
            std::unique_lock lk(_state_mutex);
            _state = state;
        }
        [[nodiscard]] StateTransition PostTcpConnection(bool is_connected) {
            std::unique_lock lk(_state_mutex);
            if (_state == HeadsetStates::CLOSING) {
                return { false, _state };
            }
            const auto last_state = _state;
            if (!is_connected) {
                // we are no longer connected; check if we should do anything
                if (last_state == HeadsetStates::READY || last_state == HeadsetStates::RUNNING) {
                    // in this case, we expected to be connected; reset the state
                    _state = HeadsetStates::CONNECTING;
                }
            } else {
                if (last_state == HeadsetStates::CONNECTING && websocket_is_connected) {
                    _state = HeadsetStates::READY;
                }
            }
            tcp_is_connected = is_connected;
            return { last_state != _state, _state };
        }

        [[nodiscard]] StateTransition PostWebsocketConnection(bool is_connected) {
            std::unique_lock lk(_state_mutex);
            if (_state == HeadsetStates::CLOSING) {
                return { false, _state };
            }
            const auto last_state = _state;
            if (!is_connected) {
                // we are no longer connected; check if we should do anything
                if (last_state == HeadsetStates::READY || last_state == HeadsetStates::RUNNING) {
                    // in this case, we expected to be connected; reset the state
                    _state = HeadsetStates::CONNECTING;
                }
            } else {
                if (last_state == HeadsetStates::CONNECTING && tcp_is_connected) {
                    _state = HeadsetStates::READY;
                }
            }
            websocket_is_connected = is_connected;
            return { last_state != _state, _state };

        }
    private:
        mutable std::shared_mutex _state_mutex;
        bool tcp_is_connected = false;
        bool websocket_is_connected = false;
        HeadsetStates _state = HeadsetStates::CONNECTING;
    };

    class RotateCameraMessage: public DomainMessage {
    public:
        [[nodiscard]] MessageType GetMessageType() const final {
            return DomainMessage::MessageType::RotateCamera;
        };
        [[nodiscard]] nlohmann::json GetMessagePayload() const final {
            return 1;
        };
    protected:
        friend class DomainMessage;
        static DomainMessagePtr TryCreate(const nlohmann::json& json_data) {
            return std::make_unique<RotateCameraMessage>();
        }
    };

}

#endif //DOMAIN_HEADSET_DOMAIN_HPP
