//
// Created by brucegoose on 4/22/23.
//

#include "config.hpp"
#include "runtime.hpp"

#include "service/camera_streamer.hpp"

#include <chrono>
using namespace std::literals;

static infrastructure::CameraType to_camera_type(const std::string& type) {
    if (type == "LIBCAMERA") return infrastructure::CameraType::LIBCAMERA;
    else if (type == "FAKE") return infrastructure::CameraType::FAKE;
    else if (type == "STRING") return infrastructure::CameraType::STRING;
    throw std::runtime_error("Unknown camera type: " + type);
}


static infrastructure::EncoderType to_encoder_type(const std::string& type) {
    if (type == "SW") return infrastructure::EncoderType::SW;
    else if (type == "NLL") return infrastructure::EncoderType::NLL;
    throw std::runtime_error("Unknown encoder type: " + type);
}

int main(int argc, char* argv[]) {

    application::RemoveSuccessFile();

    auto config = application::get_json_config(application::AppType::CAMERA, argc, argv);

    const service::CameraStreamerConfig camera_config(
        config.value("serverHost", "69.4.20.10"),
        config.value("serverPort", 6969),
        to_camera_type(config.value("cameraType", "LIBCAMERA")),
        {
            config.value("imageWidth", 1536),
            config.value("imageHeight", 864)
        },
        config.value("cameraLensPosition", 0.5f),
        to_encoder_type(config.value("encoderType", "SW")),
        config.value("encoderBuffersDownstream", 4)
    );
    auto service = service::CameraStreamer::Create(camera_config);
    service->Start();

    application::WaitForShutdown();

    service->Stop();
    std::this_thread::sleep_for(500ms);
    service->Unset();
    std::this_thread::sleep_for(500ms);

    application::CreateSuccessFile();
}