//
// Created by brucegoose on 6/1/23.
//

#ifndef DOMAIN_MESSAGE_HPP
#define DOMAIN_MESSAGE_HPP

#include <stdint.h>

#include <json.hpp>

namespace domain {

    struct BmsMessage {
        int battery_level;
        bool bms_is_plugged_in;
        bool bms_wants_shutdown;
    };

    typedef std::function<void(const BmsMessage)> BmsMessageCallback;

    class DomainMessage;
    typedef std::unique_ptr<DomainMessage> DomainMessagePtr;

    class DomainMessage {
    public:
        enum MessageType: int {
            HeadsetRotateCamera = 0,
            HeadsetResetCamera
        };

        [[nodiscard]] virtual MessageType GetMessageType() const = 0;
        [[nodiscard]] nlohmann::json GetMessage() const {
            return nlohmann::json{
                    {"message_type", GetMessageType()},
                    {"message_payload", GetMessagePayload()}
            };
        };
        static DomainMessagePtr TryParseMessage(const nlohmann::json&& json_data);
        virtual ~DomainMessage() {};
    protected:
        [[nodiscard]] virtual nlohmann::json GetMessagePayload() const = 0;
    };

}


#endif //DOMAIN_MESSAGE_HPP
