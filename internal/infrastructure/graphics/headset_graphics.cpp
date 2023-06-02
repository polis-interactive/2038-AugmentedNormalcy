//
// Created by brucegoose on 4/15/23.
//

#include <iostream>
#include <chrono>
using namespace std::literals;
typedef std::chrono::high_resolution_clock Clock;

#include <libdrm/drm_fourcc.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.hpp"

#include "headset_graphics.hpp"

#define BUFFER_OFFSET(idx) (static_cast<char*>(0) + (idx))


static GLint compile_shader(GLenum target, const char *source)
{
    GLuint s = glCreateShader(target);
    glShaderSource(s, 1, (const GLchar **)&source, NULL);
    glCompileShader(s);

    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);

    if (!ok)
    {
        GLchar *info;
        GLint size;

        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &size);
        info = (GLchar *)malloc(size);

        glGetShaderInfoLog(s, size, NULL, info);
        std::cout << "failed to compile shader: " << std::string(info) << "\nsource:\n" << std::string(source) << std::endl;
        throw std::runtime_error("failed to compile shader: " + std::string(info) + "\nsource:\n" +
                                 std::string(source));
    }

    return s;
}

static GLint link_program(GLint vs, GLint fs)
{
    GLint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        /* Some drivers return a size of 1 for an empty log.  This is the size
         * of a log that contains only a terminating NUL character.
         */
        GLint size;
        GLchar *info = NULL;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &size);
        if (size > 1)
        {
            info = (GLchar *)malloc(size);
            glGetProgramInfoLog(prog, size, NULL, info);
        }
        std::cout << "failed to link: " << std::string(info ? info : "<empty log>") << std::endl;
        throw std::runtime_error("failed to link: " + std::string(info ? info : "<empty log>"));
    }

    return prog;
}

static GLint displaySetup(int width, int height, int window_width, int window_height)
{
    char vs[512];
    snprintf(vs, sizeof(vs),
             "#version 310 es\n"
             "layout (location = 0) in vec3 v_pos;\n"
             "layout (location = 1) in vec3 v_color;\n"
             "layout (location = 2) in vec2 v_tex;\n"
             "out vec2 texcoord;\n"
             "out float c_pos;\n"
             "\n"
             "void main() {\n"
             "  gl_Position = vec4(v_pos, 1.0);\n"
             "  texcoord.x = v_tex.x;\n"
             "  texcoord.y = 1.0 - v_tex.y;\n"
             "  c_pos = v_color.x;\n"
             "}\n"
    );
    vs[sizeof(vs) - 1] = 0;
    GLint vs_s = compile_shader(GL_VERTEX_SHADER, vs);

    const char *fs =
            "#version 310 es\n"
            "#extension GL_OES_EGL_image_external : enable\n"
            "#define TWO_PI 6.28318530718\n"
            "precision mediump float;\n"
            "uniform samplerExternalOES s;\n"
            "in vec2 texcoord;\n"
            "in float c_pos;\n"
            "out vec4 fragColor;\n"
            "vec3 hsb2rgb( in vec3 c ){\n"
            "	vec3 rgb = clamp(abs(mod(c.x*6.0+vec3(0.0,4.0,2.0),6.0)-3.0)-1.0, 0.0, 1.0 );\n"
            "	rgb = rgb*rgb*(3.0-2.0*rgb);\n"
            "	return c.z * mix( vec3(1.0), rgb, c.y);\n"
            "}\n"
            "void main() {\n"
            "  fragColor = texture2D(s, texcoord);\n"
            "}\n";
    GLint fs_s = compile_shader(GL_FRAGMENT_SHADER, fs);
    GLint prog = link_program(vs_s, fs_s);

    return prog;
}

static GLint simpleShader() {
    char vs[512];
    snprintf(vs, sizeof(vs),
             "#version 310 es\n"
             "layout (location = 0) in vec3 v_pos;\n"
             "layout (location = 1) in vec3 v_color;\n"
             "layout (location = 2) in vec2 v_tex;\n"
             "out vec3 our_color;\n"
             "out vec2 tex_coord;\n"
             "\n"
             "void main() {\n"
             "  gl_Position = vec4(v_pos, 1.0);\n"
             "  our_color = v_color;\n"
             "  tex_coord = v_tex;\n"
             "}\n"
    );
    vs[sizeof(vs) - 1] = 0;
    GLint vs_s = compile_shader(GL_VERTEX_SHADER, vs);

    const char *fs =
            "#version 310 es\n"
            "precision mediump float;\n"
            "in vec3 our_color;\n"
            "in vec2 tex_coord;\n"
            "uniform sampler2D our_texture;\n"
            "out vec4 frag_color;\n"
            "void main() {\n"
            "  frag_color = texture(our_texture, tex_coord);\n"
            "}\n";
    GLint fs_s = compile_shader(GL_FRAGMENT_SHADER, fs);
    GLint prog = link_program(vs_s, fs_s);

    return prog;
}

namespace infrastructure {

    HeadsetGraphics::HeadsetGraphics(const GraphicsConfig &conf):
            Graphics(conf),
            _image_width(conf.get_image_width_height().first),
            _image_height(conf.get_image_width_height().second)
    {}
    HeadsetGraphics::~HeadsetGraphics() {
        StopGraphics();
    }

    void HeadsetGraphics::StartGraphics() {
        if (!_stop_running) {
            return;
        }
        _stop_running = false;
        auto self(shared_from_this());
        graphics_thread = std::make_unique<std::thread>([this, self]() {
            run();
        });
    }

    void HeadsetGraphics::StopGraphics() {
        if (_stop_running) {
            return;
        }
        _is_ready = false;
        _stop_running = true;
        if (graphics_thread != nullptr) {
            if (graphics_thread->joinable()) {
                graphics_thread->join();
            }
            graphics_thread = nullptr;
        }
        {
            std::unique_lock<std::mutex> lock(_image_mutex);
            while (!_image_queue.empty()) {
                _image_queue.pop();
            }
        }
    }
    void HeadsetGraphics::PostImage(std::shared_ptr<DecoderBuffer>&& buffer) {
        if (_is_ready && _is_display) {
            std::unique_lock<std::mutex> lock(_image_mutex);
            _image_queue.push(std::move(buffer));
        }
    }

    void HeadsetGraphics::PostGraphicsHeadsetState(const domain::HeadsetStates state) {
        std::unique_lock lk(_state_mutex);
        _state = state;
    }

    void HeadsetGraphics::run() {
        bool has_run = false;
        while (!_stop_running) {
            if (has_run) {
                std::this_thread::sleep_for(1s);
            }
            has_run = true;
            std::cout << "Starting graphics thread" << std::endl;
            if (!glfwInit()) {
                std::cout << "unable to start glfw context; trying again" << std::endl;
                continue;
            }

            glfwSetTime(0);
            setWindowHints();
            const GLFWvidmode * mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
            _width = mode->width;
            _height = mode->height;
            _window = glfwCreateWindow(_width, _height, "Headset", NULL, NULL);
            if (!_window)
            {
                std::cout << "Failed to create GLFW window background" << std::endl;
                continue;
            }

            glfwMakeContextCurrent(_window);
            int version = gladLoadGLES2Loader((GLADloadproc) glfwGetProcAddress);
            if (version == 0) {
                std::cout << "Failed to initialize Gles2 context" << std::endl;
                continue;
            }

            std::cout << "Vendor graphic card: " << glGetString(GL_VENDOR) << std::endl;
            std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
            std::cout << "Version GL: "<< glGetString(GL_VERSION) << std::endl;
            std::cout << "Version GLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

            /* load egl */
            int egl_version = gladLoadEGLLoader((GLADloadproc) glfwGetProcAddress);
            if (egl_version == 0) {
                std::cout << "failed to load egl" << std::endl;
            }
            auto egl_display = glfwGetEGLDisplay();

            auto egl_surface = glfwGetEGLSurface(_window);
            if (egl_surface == EGL_NO_SURFACE) {
                std::cout << "WHOOPS, no surface" << std::endl;
            }
            auto egl_context = glfwGetEGLContext(_window);
            if (egl_context == EGL_NO_CONTEXT ) {
                std::cout << "WHOOPS, no context" << std::endl;
            }

            if (!eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context)) {
                std::cout << "Failed to make egl current" << std::endl;
            }

            /* setup window */

            glViewport(0, 0, _width, _height);
            glfwSwapInterval(0);
            glfwSwapBuffers(_window);

            _image_shader = displaySetup(_image_width, _image_height, _width, _height);
            _screen_shader = simpleShader();
            glfwSetKeyCallback(_window,
                [](GLFWwindow * w, int key, int scancode, int action, int mods) {
                    if(key == GLFW_KEY_ESCAPE) {
                        glfwSetWindowShouldClose(w, GLFW_TRUE);
                    }
                }
            );

            glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            glfwShowWindow(_window);

            const float disp_width = _image_width / (float)_width;
            const float disp_height = _image_height / (float)_height;

            /* setup output */

            float vertices[] = {
                    // positions                // colors                 // texture coords
                    disp_width, disp_height,    0.0f, 0.0f, 0.0f, 0.0f,     1.0f, 1.0f, // top right
                    disp_width, -disp_height,   0.0f, 0.0f, 0.0f, 0.0f,     1.0f, 0.0f, // bottom right,
                    -disp_width, -disp_height,  0.0f, 0.0f, 0.0f, 0.0f,     0.0f, 0.0f, // bottom left,
                    -disp_width, disp_height,   0.0f, 0.0f, 0.0f, 0.0f,     0.0f, 1.0f, // top left,
            };

            unsigned int indices[] = {
                    0, 1, 3, // first triangle
                    1, 2, 3, // second triangle
            };

            glGenVertexArrays(1, &IMAGE_VAO);
            glGenBuffers(1, &IMAGE_VBO);
            glGenBuffers(1, &IMAGE_EBO);


            glBindVertexArray(IMAGE_VAO);

            glBindBuffer(GL_ARRAY_BUFFER, IMAGE_VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IMAGE_EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

            // position attribute
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);

            // color attribute
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);

            // texture coord attribute
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
            glEnableVertexAttribArray(2);

            // load textures
            std::filesystem::path assets_dir = ASSETS_DIR;
            _connecting_screen.LoadTexture(assets_dir / "connecting_screen.jpg");
            _ready_screen.LoadTexture(assets_dir / "ready_screen.jpg");
            _plugged_in_screen.LoadTexture(assets_dir / "plugged_in_screen.jpg");
            _dying_screen.LoadTexture(assets_dir / "dying_screen.jpg");

            std::cout << "At graphics loop" << std::endl;
            _is_ready = true;
            auto last_state = domain::HeadsetStates::CONNECTING;

            while (!glfwWindowShouldClose(_window) && !_stop_running) {
                glfwPollEvents();
                glClearColor(0, 0, 0, 1.0);
                glClear(GL_COLOR_BUFFER_BIT);

                // get the state
                auto state = domain::HeadsetStates::RUNNING;
                const bool is_transition = state != last_state;

                // transition if necessary
                if (state == domain::HeadsetStates::RUNNING && !is_transition) {
                    _is_display = true;
                } else if (last_state == domain::HeadsetStates::RUNNING){
                    _is_display = false;
                    std::unique_lock<std::mutex> lock(_image_mutex);
                    while (!_image_queue.empty()) {
                        _image_queue.pop();
                    }
                }

                switch (state) {
                    case domain::HeadsetStates::CONNECTING:
                        handleConnectingState(is_transition);
                        break;
                    case domain::HeadsetStates::READY:
                        handleReadyState(is_transition);
                        break;
                    case domain::HeadsetStates::RUNNING:
                        handleRunningState(is_transition);
                        break;
                    case domain::HeadsetStates::PLUGGED_IN:
                        handlePluggedInState(is_transition);
                        break;
                    case domain::HeadsetStates::DYING:
                        handleDyingState(is_transition);
                        break;
                    case domain::HeadsetStates::CLOSING:
                        // nothing to do
                        break;
                }
                EGLBoolean success [[maybe_unused]] = eglSwapBuffers(egl_display, egl_surface);
                last_state = state;
            }

            _is_ready = false;
            glDeleteVertexArrays(1, &IMAGE_VAO);
            glDeleteBuffers(1, &IMAGE_VBO);
            glDeleteBuffers(1, &IMAGE_EBO);

            // unload
            _connecting_screen.UnloadTexture();
            _ready_screen.UnloadTexture();
            _plugged_in_screen.UnloadTexture();
            _dying_screen.UnloadTexture();
        }
    }

    void HeadsetGraphics::handleConnectingState(const bool is_transition) {
        glUseProgram(_screen_shader);
        glBindTexture(GL_TEXTURE_2D, _connecting_screen.texture);
        glBindVertexArray(IMAGE_VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    void HeadsetGraphics::handleReadyState(const bool is_transition) {
        glUseProgram(_screen_shader);
        glBindTexture(GL_TEXTURE_2D, _ready_screen.texture);
        glBindVertexArray(IMAGE_VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    void HeadsetGraphics::handleRunningState(const bool is_transition) {
        static EglBuffer *egl_buffer = nullptr;
        std::shared_ptr<DecoderBuffer> data = nullptr;
        {
            std::unique_lock<std::mutex> lock(_image_mutex);
            while (!_image_queue.empty()) {
                data = _image_queue.front();
                _image_queue.pop();
            }
        }

        if (data) {
            EglBuffer &tmp_egl_buffer = _buffers[data->GetFd()];
            if (tmp_egl_buffer.fd == -1) {
                makeBuffer(data->GetFd(), data->GetSize(), tmp_egl_buffer);
            }
            egl_buffer = &tmp_egl_buffer;
        }

        if (egl_buffer) {
            glUseProgram(_image_shader);
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, egl_buffer->texture);
            glBindVertexArray(IMAGE_VAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
    }

    void HeadsetGraphics::handlePluggedInState(const bool is_transition) {
        glUseProgram(_screen_shader);
        glBindTexture(GL_TEXTURE_2D, _plugged_in_screen.texture);
        glBindVertexArray(IMAGE_VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    void HeadsetGraphics::handleDyingState(const bool is_transition) {
        glUseProgram(_screen_shader);
        glBindTexture(GL_TEXTURE_2D, _dying_screen.texture);
        glBindVertexArray(IMAGE_VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    void HeadsetGraphics::setWindowHints() {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
        glfwWindowHint(GLFW_OPENGL_COMPAT_PROFILE,GLFW_TRUE);

        glfwWindowHint(GLFW_SAMPLES, 0);
        glfwWindowHint(GLFW_RED_BITS, 8);
        glfwWindowHint(GLFW_GREEN_BITS, 8);
        glfwWindowHint(GLFW_BLUE_BITS, 8);
        glfwWindowHint(GLFW_ALPHA_BITS, 8);
        glfwWindowHint(GLFW_STENCIL_BITS, 8);

        glfwWindowHint(GLFW_DEPTH_BITS, 16);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    }

    void HeadsetGraphics::makeBuffer(int fd, size_t size, EglBuffer &buffer)
    {

        std::cout << "making buffer for: " << fd << std::endl;

        buffer.fd = fd;
        buffer.size = size;

        GLint encoding = EGL_ITU_REC601_EXT;
        GLint range = EGL_YUV_NARROW_RANGE_EXT;

        EGLint attribs[] = {
                EGL_WIDTH, static_cast<EGLint>(_image_width),
                EGL_HEIGHT, static_cast<EGLint>(_image_height),
                EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_YUV420,
                EGL_DMA_BUF_PLANE0_FD_EXT, fd,
                EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
                EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLint>(_image_width),
                EGL_DMA_BUF_PLANE1_FD_EXT, fd,
                EGL_DMA_BUF_PLANE1_OFFSET_EXT, static_cast<EGLint>(_image_width * _image_height),
                EGL_DMA_BUF_PLANE1_PITCH_EXT, static_cast<EGLint>(_image_width / 2),
                EGL_DMA_BUF_PLANE2_FD_EXT, fd,
                EGL_DMA_BUF_PLANE2_OFFSET_EXT, static_cast<EGLint>(_image_width * _image_height + (_image_width / 2) * (_image_height / 2)),
                EGL_DMA_BUF_PLANE2_PITCH_EXT, static_cast<EGLint>(_image_width / 2),
                EGL_YUV_COLOR_SPACE_HINT_EXT, encoding,
                EGL_SAMPLE_RANGE_HINT_EXT, range,
                EGL_NONE
        };

        auto display = glfwGetEGLDisplay();

        std::cout << "creating image" << std::endl;
        EGLImage image = eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer) NULL, attribs);
        if (!image) {
            std::cout << "failed to import fd " << fd << std::endl;
            std::cout << eglGetError() << std::endl;
            return;
        }

        glGenTextures(1, &buffer.texture);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, buffer.texture);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, image);

        eglDestroyImageKHR(display, image);
    }

    void HeadsetGraphics::Screen::LoadTexture(const std::string &image_path) {
        if (has_loaded) return;
        data = stbi_load(image_path.c_str(), &width, &height, &nrChannels, 0);

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        // set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        has_loaded = true;
    }

    void HeadsetGraphics::Screen::UnloadTexture() {
        if (has_loaded) {
            glDeleteTextures(1, &texture);
            stbi_image_free(data);
            has_loaded = false;
        }
    }

    HeadsetGraphics::Screen::~Screen() {
        UnloadTexture();
    }
}