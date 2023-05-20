//
// Created by brucegoose on 4/22/23.
//

#include "config.hpp"
#include "runtime.hpp"

#include "service/server/server_streamer.hpp"

#include <chrono>
using namespace std::literals;

static service::ClientAssignmentStrategy to_client_assignment_strategy(const std::string& type) {
    if (type == "CAMERA_THEN_HEADSET") return service::ClientAssignmentStrategy::CAMERA_THEN_HEADSET;
    if (type == "IP_BOUNDS") return service::ClientAssignmentStrategy::IP_BOUNDS;
    if (type == "ENDPOINT_PORT") return service::ClientAssignmentStrategy::ENDPOINT_PORT;
    throw std::runtime_error("Unknown ClientAssignmentStrategy: " + type);
}

static service::CameraSwitchingStrategy to_camera_switching_strategy(const std::string& type) {
    if (type == "MANUAL_ENTRY") return service::CameraSwitchingStrategy::MANUAL_ENTRY;
    if (type == "AUTOMATIC_TIMER") return service::CameraSwitchingStrategy::AUTOMATIC_TIMER;
    if (type == "HEADSET_CONTROLLED") return service::CameraSwitchingStrategy::HEADSET_CONTROLLED;
    if (type == "NONE") return service::CameraSwitchingStrategy::NONE;
    throw std::runtime_error("Unknown CameraSwitchingStrategy: " + type);
}

int main(int argc, char* argv[]) {

    application::RemoveSuccessFile();

    auto config = application::get_json_config(application::AppType::SERVER, argc, argv);

    const service::ServerStreamerConfig conf(
        config.value("tcpPoolSize", 6),
        config.value("serverPort", 6969),
        config.value("serverTimeoutOnRead", 3),
        config.value("cameraBuffersCount", 4),
        config.value("cameraBufferSize", 1536 * 864 * 3 * 0.5),
        to_client_assignment_strategy(config.value("serverClientAssignmentStrategy", "IP_BOUNDS")),
        to_camera_switching_strategy(config.value("serverCameraSwitchingStrategy", "AUTOMATIC_TIMER"))
    );
    auto service = service::ServerStreamer::Create(conf);
    service->Start();

    bool exit = false;

    application::WaitForShutdown();

    service->Stop();
    std::this_thread::sleep_for(500ms);
    service->Unset();
    std::this_thread::sleep_for(500ms);

    application::CreateSuccessFile();
}