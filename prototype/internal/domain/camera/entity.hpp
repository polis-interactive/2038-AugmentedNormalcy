//
// Created by brucegoose on 1/30/23.
//

#ifndef DOMAIN_CAMERA_ENTITY_HPP
#define DOMAIN_CAMERA_ENTITY_HPP

#include <sstream>
#include <memory>

#include <libcamera/controls.h>
#include <libcamera/geometry.h>
#include "third_party/nlohmann/json.hpp"

using namespace nlohmann;

namespace domain {

    // might be nice to have these configurable
    extern const int min_framerate;
    extern const int max_framerate;
    extern const std::vector<libcamera::Size> available_sizes;

/*
 * For now, we consider a command successful if it doesn't return an error message
 */

    class Camera {
    public:
        std::string SetBufferCount(int buffer_count) {
            if (buffer_count > 8) {
                return "buffer count must be less than 8";
            } else if (buffer_count < 2) {
                return "buffer count must be at least 2";
            }
            _buffer_count = buffer_count;
            return "";
        }
        std::string SetFramerate(int fps) {
            if (fps < min_framerate) {
                return "fps cannot be lower than " + std::to_string(min_framerate);
            } else if (fps > max_framerate) {
                return "fps cannot be higher than " + std::to_string(max_framerate);
            }
            _framerate = fps;
            return "";
        }
        std::string SetCaptureResolution(libcamera::Size resolution) {
            if (std::find(available_sizes.begin(), available_sizes.end(), resolution) == available_sizes.end()) {
                std::stringstream ss;
                ss << "Resolution " << resolution.toString() << " is unavailable; available resolutions include:";
                for (const auto &res : available_sizes) {
                    ss << " " << res.toString();
                }
                return ss.str();
            }
            _resolution = resolution;
            return "";
        }
        void SetBinOutput(bool bin_output) {
            _bin_output = bin_output;
        }

    public:
        // these are settable for a running instance
        int _buffer_count = 4;
        int _framerate = 30;
        libcamera::Size _resolution = libcamera::Size(1920, 1080);
        bool _bin_output = true;

        bool _is_base_camera = false;
        int _id = -1;
    };

}


#endif //DOMAIN_CAMERA_ENTITY_HPP
