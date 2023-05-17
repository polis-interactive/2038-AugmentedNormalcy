//
// Created by brucegoose on 4/22/23.
//

#include "config.hpp"
#include "runtime.hpp"

#include "service/server_streamer.hpp"

#include <chrono>
using namespace std::literals;


int main(int argc, char* argv[]) {

    application::RemoveSuccessFile();

    auto config = application::get_json_config(application::AppType::SERVER, argc, argv);

    const service::ServerStreamerConfig conf(
        config.value("tcpPoolSize", 6),
        config.value("serverPort", 6969),
        config.value("cameraBuffersCount", 4),
        config.value("cameraBufferSize", 1536 * 864 * 3 * 0.5)
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