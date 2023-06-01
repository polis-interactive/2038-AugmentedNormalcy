//
// Created by brucegoose on 4/22/23.
//

#include "config.hpp"
#include "runtime.hpp"

#include "service/headset/headset_streamer.hpp"

#include <chrono>
using namespace std::literals;

static infrastructure::DecoderType to_decoder_type(const std::string& type) {
    if (type == "SW") return infrastructure::DecoderType::SW;
    else if (type == "NONE") return infrastructure::DecoderType::NONE;
    throw std::runtime_error("Unknown decoder type: " + type);
}

static infrastructure::GraphicsType to_graphics_type(const std::string& type) {
    if (type == "HEADSET") return infrastructure::GraphicsType::HEADSET;
    if (type == "DISPLAY") return infrastructure::GraphicsType::DISPLAY;
    else if (type == "NONE") return infrastructure::GraphicsType::NONE;
    throw std::runtime_error("Unknown graphics type: " + type);
}

static infrastructure::GpioType to_gpio_type(const std::string &type) {
    if (type == "PIGPIO") return infrastructure::GpioType::PIGPIO;
    else if (type == "NONE") return infrastructure::GpioType::NONE;
    throw std::runtime_error("Unknown gpio type: " + type);
}

static infrastructure::BmsType to_bms_type(const std::string &type) {
    if (type == "SERIAL") return infrastructure::BmsType::SERIAL;
    else if (type == "NONE") return infrastructure::BmsType::NONE;
    throw std::runtime_error("Unknown bms type: " + type);
}

int main(int argc, char* argv[]) {

    application::RemoveSuccessFile();

    auto config = application::get_json_config(application::AppType::HEADSET, argc, argv);

    const service::HeadsetStreamerConfig headset_config(
        config.value("serverHost", "69.4.20.10"),
        config.value("serverPort", 6969),
        config.value("clientTimeoutOnRead", 3),
        config.value("clientUseFixedPort", false),
        config.value("websocketServerHost", "69.4.20.10"),
        config.value("websocketServerPort", 8008),
        {
            config.value("imageWidth", 1536),
            config.value("imageHeight", 864)
        },
        config.value("tcpReadBuffers", 4),
        config.value("decoderBuffersDownstream", 4),
        to_decoder_type(config.value("decoderType", "SW")),
        to_graphics_type(config.value("graphicsType", "GLFW")),
        to_gpio_type(config.value("gpioType", "PIGPIO")),
        to_bms_type(config.value("bmsType", "SERIAL"))
     );
    auto service = service::HeadsetStreamer::Create(headset_config);
    service->Start();

    application::WaitForShutdown();

    service->Stop();
    std::this_thread::sleep_for(500ms);
    service->Unset();
    std::this_thread::sleep_for(500ms);

    application::CreateSuccessFile();

}