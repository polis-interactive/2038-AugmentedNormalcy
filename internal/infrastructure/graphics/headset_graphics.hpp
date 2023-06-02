//
// Created by brucegoose on 4/15/23.
//

#ifndef INFRASTRUCTURE_GRAPHICS_HEADSET_GRAPHICS_HPP
#define INFRASTRUCTURE_GRAPHICS_HEADSET_GRAPHICS_HPP

#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <atomic>
#include <map>


#include "domain/headset_domain.hpp"

#include "graphics.hpp"


namespace infrastructure {

    class HeadsetGraphics : public std::enable_shared_from_this<HeadsetGraphics>, public Graphics {
    public:

        struct Screen {
            GLuint texture = 0;
            int width = 0;
            int height = 0;
            int nrChannels = 0;
            unsigned char *data = nullptr;
            bool has_loaded = false;

            void LoadTexture(const std::string &image_path);
            void UnloadTexture();
            ~Screen();

        };

        explicit HeadsetGraphics(const GraphicsConfig &conf);
        ~HeadsetGraphics();

        void PostImage(std::shared_ptr<DecoderBuffer>&& buffer) override;

        void PostGraphicsHeadsetState(const domain::HeadsetStates state) override;
    private:

        void StartGraphics() override;
        void StopGraphics() override;

        void run();

        static void setWindowHints();
        void makeBuffer(int fd, size_t size, EglBuffer &buffer);

        void handleConnectingState(const bool is_transition);
        void handleReadyState(const bool is_transition);
        void handleRunningState(const bool is_transition);
        void handlePluggedInState(const bool is_transition);
        void handleDyingState(const bool is_transition);

        std::atomic_bool _stop_running = true;
        std::atomic_bool _is_ready = false;
        std::atomic_bool _is_display = false;
        std::unique_ptr<std::thread> graphics_thread = nullptr;
        mutable std::shared_mutex _state_mutex;
        domain::HeadsetStates _state;
        std::mutex _image_mutex;
        std::queue<std::shared_ptr<DecoderBuffer>> _image_queue;

        Screen _connecting_screen;
        Screen _ready_screen;
        Screen _plugged_in_screen;
        Screen _dying_screen;

        const int _image_width;
        const int _image_height;
        int _width = 0;
        int _height = 0;
        GLFWwindow *_window = nullptr;
        std::map<int, EglBuffer> _buffers;

        unsigned int IMAGE_VBO, IMAGE_VAO, IMAGE_EBO;
    };
}

#endif //INFRASTRUCTURE_GRAPHICS_HEADSET_GRAPHICS_HPP
