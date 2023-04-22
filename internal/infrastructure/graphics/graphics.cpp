//
// Created by brucegoose on 4/15/23.
//

#include <iostream>
#include <chrono>
using namespace std::literals;
typedef std::chrono::high_resolution_clock Clock;

#include <libdrm/drm_fourcc.h>

#include "graphics.hpp"

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

static void displaySetup(int width, int height, int window_width, int window_height)
{
    float w_factor = width / (float)window_width;
    float h_factor = height / (float)window_height;
    float max_dimension = std::max(w_factor, h_factor);
    w_factor /= max_dimension;
    h_factor /= max_dimension;
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
             "  texcoord.x = 1.0 - v_tex.x;\n"
             "  texcoord.y = v_tex.y;\n"
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

    glUseProgram(prog);
}

namespace infrastructure {

    void Graphics::Start() {
        if (!_stop_running) {
            return;
        }
        _stop_running = false;
        auto self(shared_from_this());
        graphics_thread = std::make_unique<std::thread>([this, s = std::move(self)]() {
            run();
        });
    }

    void Graphics::Stop() {
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
    void Graphics::PostImage(std::shared_ptr<DecoderBuffer>&& buffer) {
        if (_is_ready) {
            std::unique_lock<std::mutex> lock(_image_mutex);
            _image_queue.push(std::move(buffer));
        }
    }

    void Graphics::run() {
        bool has_run = false;
        while (!_stop_running) {
            if (has_run) {
                std::this_thread::sleep_for(100ms);
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

            displaySetup(_image_width, _image_height, _width, _height);
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
            unsigned int VBO, VAO, EBO;
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glGenBuffers(1, &EBO);


            glBindVertexArray(VAO);

            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
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

            std::cout << "At graphics loop" << std::endl;
            _is_ready = true;

            EglBuffer *egl_buffer = nullptr;
            while (!glfwWindowShouldClose(_window) && !_stop_running) {
                glfwPollEvents();
                std::shared_ptr<DecoderBuffer> data = nullptr;
                {
                    std::unique_lock<std::mutex> lock(_image_mutex);
                    while (!_image_queue.empty()) {
                        data = _image_queue.front();
                        _image_queue.pop();
                    }
                }
                glClearColor(0, 0, 0, 1.0);
                glClear(GL_COLOR_BUFFER_BIT);
                if (data) {
                    EglBuffer &tmp_egl_buffer = _buffers[data->GetFd()];
                    if (tmp_egl_buffer.fd == -1) {
                        makeBuffer(data->GetFd(), data->GetSize(), tmp_egl_buffer);
                    }
                    egl_buffer = &tmp_egl_buffer;
                }
                if (egl_buffer) {
                    glBindTexture(GL_TEXTURE_EXTERNAL_OES, egl_buffer->texture);
                    glBindVertexArray(VAO);
                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
                }
                EGLBoolean success [[maybe_unused]] = eglSwapBuffers(egl_display, egl_surface);
            }

            _is_ready = false;
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
            glDeleteBuffers(1, &EBO);
        }
    }

    void Graphics::setWindowHints() {
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

    void Graphics::makeBuffer(int fd, size_t size, EglBuffer &buffer)
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
}