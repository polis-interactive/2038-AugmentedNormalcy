//
// Created by brucegoose on 5/4/23.
//

#ifndef AUGMENTEDNORMALCY_TEST_ENCODER_FAKE_CAMERA_HPP
#define AUGMENTEDNORMALCY_TEST_ENCODER_FAKE_CAMERA_HPP

#include "utils/buffers.hpp"

class FakeCamera {
public:
    FakeCamera(const int buffer_count);
    std::shared_ptr<CameraBuffer> GetBuffer();
};

#endif //AUGMENTEDNORMALCY_TEST_ENCODER_FAKE_CAMERA_HPP
