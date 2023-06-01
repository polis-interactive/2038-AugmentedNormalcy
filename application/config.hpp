
#ifndef AUGMENTEDNORMALCY_APPLICATION_CONFIG_HPP
#define AUGMENTEDNORMALCY_APPLICATION_CONFIG_HPP

#include <fstream>
#include <filesystem>
#include <json.hpp>

namespace application {

    enum class AppType {
        CAMERA,
        HEADSET,
        SERVER,
        DISPLAY
    };

    nlohmann::json get_json_config(const AppType &app_type, int argc, char * argv[]) {
        std::string config_name;
        switch (app_type) {
            case AppType::CAMERA:
                config_name = "camera.";
                break;
            case AppType::HEADSET:
                config_name = "headset.";
                break;
            case AppType::SERVER:
                config_name = "server.";
                break;
            case AppType::DISPLAY:
                config_name = "display.";
                break;
        }
        if (argc > 1) {
            config_name += argv[1];
        } else {
            config_name += "default";
        }
        config_name += ".json";
        std::filesystem::path app_dir = APPLICATION_DIR;
        auto normalcy_dir = app_dir.parent_path();
        auto conf_dir = normalcy_dir / "config";
        auto conf_file_path = conf_dir / config_name;
        if (!std::filesystem::exists(conf_file_path)) {
            throw std::runtime_error("Couldn't find config file at path: " + conf_file_path.string());
        }

        std::ifstream config_file(conf_file_path);
        nlohmann::json config;
        config_file >> config;
        return config;
    }

}

#endif //AUGMENTEDNORMALCY_APPLICATION_CONFIG_HPP