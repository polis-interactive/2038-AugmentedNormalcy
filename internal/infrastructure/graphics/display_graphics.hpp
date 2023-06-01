//
// Created by brucegoose on 4/15/23.
//

#ifndef INFRASTRUCTURE_GRAPHICS_GLFW_GRAPHICS_HPP
#define INFRASTRUCTURE_GRAPHICS_GLFW_GRAPHICS_HPP

#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <atomic>
#include <map>

#include "graphics.hpp"

struct EglBuffer
{
    EglBuffer() : fd(-1) {}
    int fd;
    size_t size;
    GLuint texture;
};

namespace infrastructure {

    class MyFuckingGraphics : public std::enable_shared_from_this<MyFuckingGraphics>, public Graphics {
    public:
        explicit MyFuckingGraphics(const GraphicsConfig &conf);
        ~MyFuckingGraphics();

        void PostImage(std::shared_ptr<DecoderBuffer>&& buffer) override;
    private:

        void StartGraphics() override;
        void StopGraphics() override;

        void run();

        static void setWindowHints();
        void makeBuffer(int fd, size_t size, EglBuffer &buffer);

        std::atomic_bool _stop_running = true;
        std::atomic_bool _is_ready = false;
        std::unique_ptr<std::thread> graphics_thread = nullptr;
        std::mutex _image_mutex;
        std::queue<std::shared_ptr<DecoderBuffer>> _image_queue;

        const int _image_width;
        const int _image_height;
        int _width = 0;
        int _height = 0;
        GLFWwindow *_window = nullptr;
        std::map<int, EglBuffer> _buffers;
    };
}

#endif //INFRASTRUCTURE_GRAPHICS_GLFW_GRAPHICS_HPP
