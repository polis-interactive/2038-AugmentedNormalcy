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

#define GLAD_GL_IMPLEMENTATION
#include "glad/glad_egl.h"
#include "glad/glad.h"

#define GLFW_INCLUDE_NONE 1
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_EGL 1
#define GLFW_NATIVE_INCLUDE_NONE 1
#include <GLFW/glfw3native.h>

#include "graphics.hpp"

struct EglBuffer
{
    EglBuffer() : fd(-1) {}
    int fd;
    size_t size;
    GLuint texture;
};

namespace infrastructure {

    class GlfwGraphics : public std::enable_shared_from_this<GlfwGraphics>, public Graphics {
    public:
        explicit GlfwGraphics(const GraphicsConfig &conf);
        ~GlfwGraphics();

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
