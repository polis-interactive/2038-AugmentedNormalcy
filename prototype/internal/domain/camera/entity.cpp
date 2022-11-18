//
// Created by brucegoose on 1/30/23.
//

#include "domain/camera/entity.hpp"

namespace domain {

    const int min_framerate = 15;
    const int max_framerate = 60;

#if defined(WIN32) || defined(__arm__)
    const std::vector<std::pair<int, int>> available_sizes = {
    { 1920, 1080 },
    { 1280, 720 },
    { 720, 480 }
};
#else
    const std::vector<std::pair<int, int>> available_sizes = {
            { 1280, 720 },
            { 720, 480 }
    };
#endif

}

