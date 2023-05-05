//
// Created by brucegoose on 5/4/23.
//

#ifndef AUGMENTEDNORMALCY_TEST_ENCODER_FAKE_CAMERA_HPP
#define AUGMENTEDNORMALCY_TEST_ENCODER_FAKE_CAMERA_HPP

#include "utils/buffers.hpp"

#include <mutex>
#include <deque>


class FakeCamera: public std::enable_shared_from_this<FakeCamera> {
public:
    static std::shared_ptr<FakeCamera> Create(const int buffer_count) {
        auto camera = std::make_shared<FakeCamera>(buffer_count);
        return camera;
    }
    FakeCamera(const int buffer_count);
    std::shared_ptr<CameraBuffer> GetBuffer();
    ~FakeCamera();
private:
    int _camera_fd = -1;
    std::mutex _camera_mutex;
    std::deque<CameraBuffer *> _camera_buffers;
};

#endif //AUGMENTEDNORMALCY_TEST_ENCODER_FAKE_CAMERA_HPP
