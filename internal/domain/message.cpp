//
// Created by brucegoose on 6/1/23.
//

#include <iostream>

#include "message.hpp"
#include "headset_domain.hpp"

namespace domain {

    DomainMessagePtr DomainMessage::TryParseMessage(const nlohmann::json&& json_data) {
        DomainMessagePtr message_out = nullptr;
        try {
            MessageType message_type = json_data.at("message_type").get<MessageType>();
            switch (message_type) {
                case MessageType::HeadsetRotateCamera:
                    return HeadsetRotateCameraMessage::TryCreate(json_data.at("message_payload"));
                case MessageType::HeadsetResetCamera:
                    return HeadsetResetCameraMessage::TryCreate(json_data.at("message_payload"));
                default:
                    std::cout << "DomainMessage::TryParseMessage: received unhandled message type: "
                        << message_type << std::endl;
                    return message_out;
            }
        } catch (nlohmann::json::exception& e) {
            std::cout << "DomainMessage::TryParseMessage: received JSON error: " << e.what() << std::endl;
            return message_out;
        }
    }

}