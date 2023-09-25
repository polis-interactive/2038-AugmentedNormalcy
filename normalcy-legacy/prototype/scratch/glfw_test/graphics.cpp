
#include <iostream>
#include "graphics.hpp"

#include <libdrm/drm_fourcc.h>

#define BUFFER_OFFSET(idx) (static_cast<char*>(0) + (idx))

#define IS_VIVE 2


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

static void gl_setup(int width, int height, int window_width, int window_height)
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
			 "}\n",
			 2.0 * w_factor, 2.0 * h_factor);
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

	// static const float verts[] = { -w_factor, -h_factor, w_factor, -h_factor, w_factor, h_factor, -w_factor, h_factor };
	// glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
	//glEnableVertexAttribArray(0);
}

static void get_colour_space_info(std::optional<libcamera::ColorSpace> const &cs, GLint &encoding, GLint &range)
{
	encoding = EGL_ITU_REC601_EXT;
	range = EGL_YUV_NARROW_RANGE_EXT;

	if (cs == libcamera::ColorSpace::Sycc)
		range = EGL_YUV_FULL_RANGE_EXT;
	else if (cs == libcamera::ColorSpace::Smpte170m)
		/* all good */;
	else if (cs == libcamera::ColorSpace::Rec709)
		encoding = EGL_ITU_REC709_EXT;
	else
	 std::cout << "EglPreview: unexpected colour space " << libcamera::ColorSpace::toString(cs);
}

void makeBuffer(int fd, size_t size, StreamInfo const &info, EglBuffer &buffer)
{

	std::cout << "making buffer for: " << fd << std::endl;

	buffer.fd = fd;
	buffer.size = size;

	GLint encoding, range;
	get_colour_space_info(info.colour_space, encoding, range);

	EGLint attribs[] = {
		EGL_WIDTH, static_cast<EGLint>(info.width),
		EGL_HEIGHT, static_cast<EGLint>(info.height),
		EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_YUV420,
		EGL_DMA_BUF_PLANE0_FD_EXT, fd,
		EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
		EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLint>(info.stride),
		EGL_DMA_BUF_PLANE1_FD_EXT, fd,
		EGL_DMA_BUF_PLANE1_OFFSET_EXT, static_cast<EGLint>(info.stride * info.height),
		EGL_DMA_BUF_PLANE1_PITCH_EXT, static_cast<EGLint>(info.stride / 2),
		EGL_DMA_BUF_PLANE2_FD_EXT, fd,
		EGL_DMA_BUF_PLANE2_OFFSET_EXT, static_cast<EGLint>(info.stride * info.height + (info.stride / 2) * (info.height / 2)),
		EGL_DMA_BUF_PLANE2_PITCH_EXT, static_cast<EGLint>(info.stride / 2),
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


namespace GraphicsRunner {
  void WindowHints() {
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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  }
}

GlesHandler::GlesHandler() {
  gles_thread = std::make_unique<std::jthread>(std::bind_front(&GlesHandler::Run, this));
}

void GlesHandler::Close() {
  if (gles_thread != nullptr) {
    if (gles_thread->joinable() == true) {
      gles_thread->request_stop();
      gles_thread->join();
    }
    gles_thread = nullptr;
  }
}

void GlesHandler::Post(CompletedRequestPtr&&data) {
  std::unique_lock<std::mutex> lock(_mutex);
  _queue.push(std::move(data));
  _cv.notify_one();
}

void GlesHandler::Run(std::stop_token st) {

  std::cout << "Running graphics handler" << std::endl;
  
  int width, height;
  // width = 1920;
  // height = 1080;
#if IS_VIVE == 1
    width = 2160;
    height = 1200;
#elif IS_VIVE == 2
    width = 1920;
    height = 1080;
#else
    width = 1440;
    height = 2560;
#endif

  // width = 1280;
  // height = 720;

  GraphicsRunner::WindowHints();
  window = glfwCreateWindow(width, height, "OpenGL ES 2.0 Triangle (EGL)", NULL, NULL);
  if (!window)
  {
    std::cout << "Failed to create GLFW window background" << std::endl;
    return;
  }
  
  std::cout << "Made window?" << std::endl;
  
  glfwMakeContextCurrent(window);

    int version = gladLoadGLES2Loader((GLADloadproc) glfwGetProcAddress);
    if (version == 0) {
        std::cout << "Failed to initialize Gles2 context" << std::endl;
        return;
    }        
  
      std::cout << "Vendor graphic card: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "Version GL: "<< glGetString(GL_VERSION) << std::endl;
    std::cout << "Version GLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;   

  int egl_version = gladLoadEGLLoader((GLADloadproc) glfwGetProcAddress);
  if (egl_version == 0) {
  	std::cout << "failed to load egl" << std::endl;
  }

  auto egl_display = glfwGetEGLDisplay();

  std::cout << "EGL Client APIs: " << eglQueryString(egl_display, EGL_CLIENT_APIS) << std::endl;
  std::cout << "EGL Vendor: " << eglQueryString(egl_display, EGL_VENDOR) << std::endl;
  std::cout << "EGL Version: " <<  eglQueryString(egl_display, EGL_VERSION) << std::endl;
  std::cout << "EGL Extensions: " << eglQueryString(egl_display, EGL_EXTENSIONS) << std::endl;
  auto egl_surface = glfwGetEGLSurface(window);
  if (egl_surface == EGL_NO_SURFACE) {
  	std::cout << "WHOOPS, no surface" << std::endl;
  }
  auto egl_context = glfwGetEGLContext(window);
  if (egl_context == EGL_NO_CONTEXT ) {
  	std::cout << "WHOOPS, no context" << std::endl;
  }

  if (!eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context)) {
  	std::cout << "Failed to make egl current" << std::endl;
  }
  

  glViewport(0, 0, width, height);
  glfwSwapInterval(0);
  glfwSwapBuffers(window);
  
  gl_setup(width, height, width, height);
  
  glfwSetKeyCallback(window,
    [](GLFWwindow * w, int key, int scancode, int action, int mods) {
      if(key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(w, GLFW_TRUE);
        signal_handler(0);
      }
    }
  );

  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
  
  const float ratio = width / (float) height;
  
  glfwShowWindow(window);
  
  
  std::stop_callback cb(st, [this]() {
    _cv.notify_all(); //Wake thread on stop request
  });
#if IS_VIVE == 1
// vive
      float vertices[] = {
        // positions          // colors           // texture coords
         0.0f,  0.4f, 0.0f,   0.0f, 0.0f, 0.0f,   0.865f, 1.0f, // top right, left side
         0.0f,  -0.6f, 0.0f,   0.0f, 0.0f, 0.0f,   0.865f, 0.0f, // bottom right, left side
        -1.0f,  -0.6f, 0.0f,   0.0f, 0.0f, 0.0f,   0.0f, 0.0f, // bottom left, left side
        -1.0f,  0.4f, 0.0f,   0.0f, 0.0f, 0.0f,   0.0f, 1.0f, // top left, left side

         1.0f,  0.4f, 0.0f,   0.1f, 0.0f, 0.0f,   1.0f, 1.0f, // top right, right side
         1.0f,  -0.6f, 0.0f,   0.1f, 0.0f, 0.0f,   1.0f, 0.0f, // bottom right, right side
         0.0f,  -0.6f, 0.0f,   0.1f, 0.0f, 0.0f,   0.135f, 0.0f, // bottom left, right side
         0.0f,  0.4f, 0.0f,   0.1f, 0.0f, 0.0f,   0.135f, 1.0f,  // top left, right side
    };
#elif IS_VIVE == 2
   float vertices[] = {
        // positions          // colors           // texture coords
         0.2f,  0.8f, 0.0f,   0.0f, 0.0f, 0.0f,   0.5f, 1.0f, // top right, left side
         0.2f,  -0.8f, 0.0f,   0.0f, 0.0f, 0.0f,   0.5f, 0.0f, // bottom right, left side
        -0.8f,  -0.8f, 0.0f,   0.0f, 0.0f, 0.0f,   0.0f, 0.0f, // bottom left, left side
        -0.8f,  0.8f, 0.0f,   0.0f, 0.0f, 0.0f,   0.0f, 1.0f, // top left, left side

         0.8f,  0.8f, 0.0f,   0.1f, 0.0f, 0.0f,   1.0f, 1.0f, // top right, right side
         0.8f,  -0.8f, 0.0f,   0.1f, 0.0f, 0.0f,   1.0f, 0.0f, // bottom right, right side
         0.2f,  -0.8f, 0.0f,   0.1f, 0.0f, 0.0f,   0.5f, 0.0f, // bottom left, right side
         0.2f,  0.8f, 0.0f,   0.1f, 0.0f, 0.0f,   0.5f, 1.0f,  // top left, right side
    };
#else    
/*
// normal boy
      float vertices[] = {
        // positions          // colors           // texture coords
         0.0f,  1.0f, 0.0f,   0.0f, 0.0f, 0.0f,   0.5f, 1.0f, // top right, left side
         0.0f,  -1.0f, 0.0f,   0.0f, 0.0f, 0.0f,   0.5f, 0.0f, // bottom right, left side
        -1.0f,  -1.0f, 0.0f,   0.0f, 0.0f, 0.0f,   0.0f, 0.0f, // bottom left, left side
        -1.0f,  1.0f, 0.0f,   0.0f, 0.0f, 0.0f,   0.0f, 1.0f, // top left, left side

         1.0f,  1.0f, 0.0f,   0.1f, 0.0f, 0.0f,   1.0f, 1.0f, // top right, right side
         1.0f,  -1.0f, 0.0f,   0.1f, 0.0f, 0.0f,   1.0f, 0.0f, // bottom right, right side
         0.0f,  -1.0f, 0.0f,   0.1f, 0.0f, 0.0f,   0.5f, 0.0f, // bottom left, right side
         0.0f,  1.0f, 0.0f,   0.1f, 0.0f, 0.0f,   0.5f, 1.0f,  // top left, right side
    };
// 2k screen, full size
      float vertices[] = {
        // positions          // colors           // texture coords
         1.0f,  0.0f, 0.0f,   0.0f, 0.0f, 0.0f,   0.5f, 1.0f, // top right, left side
         -1.0f,  0.0f, 0.0f,   0.0f, 0.0f, 0.0f,   0.5f, 0.0f, // bottom right, left side
        -1.0f,  -1.0f, 0.0f,   0.0f, 0.0f, 0.0f,   0.0f, 0.0f, // bottom left, left side
        1.0f,  -1.0f, 0.0f,   0.0f, 0.0f, 0.0f,   0.0f, 1.0f, // top left, left side

         1.0f,  1.0f, 0.0f,   0.1f, 0.0f, 0.0f,   1.0f, 1.0f, // top right, right side
         -1.0f,  1.0f, 0.0f,   0.1f, 0.0f, 0.0f,   1.0f, 0.0f, // bottom right, right side
         -1.0f,  0.0f, 0.0f,   0.1f, 0.0f, 0.0f,   0.5f, 0.0f, // bottom left, right side
         1.0f,  0.0f, 0.0f,   0.1f, 0.0f, 0.0f,   0.5f, 1.0f,  // top left, right side
    };
    */
// 2k screen, 1080p output
      float vertices[] = {
        // positions          // colors           // texture coords
         0.75f,  0.0f, 0.0f,   0.0f, 0.0f, 0.0f,   0.5f, 1.0f, // top right, left side
         -0.75f,  0.0f, 0.0f,   0.0f, 0.0f, 0.0f,   0.5f, 0.0f, // bottom right, left side
        -0.75f,  -0.75f, 0.0f,   0.0f, 0.0f, 0.0f,   0.0f, 0.0f, // bottom left, left side
        0.75f,  -0.75f, 0.0f,   0.0f, 0.0f, 0.0f,   0.0f, 1.0f, // top left, left side

         0.75f,  0.75f, 0.0f,   0.1f, 0.0f, 0.0f,   1.0f, 1.0f, // top right, right side
         -0.75f,  0.75f, 0.0f,   0.1f, 0.0f, 0.0f,   1.0f, 0.0f, // bottom right, right side
         -0.75f,  0.0f, 0.0f,   0.1f, 0.0f, 0.0f,   0.5f, 0.0f, // bottom left, right side
         0.75f,  0.0f, 0.0f,   0.1f, 0.0f, 0.0f,   0.5f, 1.0f,  // top left, right side
    };
#endif
       /*
// 2k screen, double
      float vertices[] = {
        // positions          // colors           // texture coords
         1.0f,  0.0f, 0.0f,   0.0f, 0.0f, 0.0f,   0.5f, 1.0f, // top right, left side
         -1.0f,  0.0f, 0.0f,   0.0f, 0.0f, 0.0f,   0.5f, 0.0f, // bottom right, left side
        -1.0f,  -1.0f, 0.0f,   0.0f, 0.0f, 0.0f,   0.0f, 0.0f, // bottom left, left side
        1.0f,  -1.0f, 0.0f,   0.0f, 0.0f, 0.0f,   0.0f, 1.0f, // top left, left side

         1.0f,  1.0f, 0.0f,   0.1f, 0.0f, 0.0f,   0.5f, 1.0f, // top right, right side
         -1.0f,  1.0f, 0.0f,   0.1f, 0.0f, 0.0f,   0.5f, 0.0f, // bottom right, right side
         -1.0f,  0.0f, 0.0f,   0.1f, 0.0f, 0.0f,   0.0f, 0.0f, // bottom left, right side
         1.0f,  0.0f, 0.0f,   0.1f, 0.0f, 0.0f,   0.0f, 1.0f,  // top left, right side
    };
    
       */
    unsigned int indices[] = {  
        0, 1, 3, // first triangle, left side
        1, 2, 3, // second triangle, left side
        4, 5, 7,
        5, 6, 7,
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
  
  while (true) {
  	std::shared_ptr<CompletedRequest> data;
    {
      std::unique_lock<std::mutex> lock(_mutex);
      _cv.wait(lock, [st, this]() {
        return st.stop_requested() ||
          !_queue.empty() ||
          glfwWindowShouldClose(window);
      });
      if (st.stop_requested() || glfwWindowShouldClose(window)) {
        break;
      }
      glfwPollEvents();
      if (glfwWindowShouldClose(window)) {
        break;
      }
      if (_queue.empty()) {
        continue;
      }
      data = _queue.front();
      _queue.pop();
    }
    EglBuffer &egl_buffer = buffers_[data->_fd];
    if (egl_buffer.fd == -1) {
    	makeBuffer(data->_fd, data->_span.size(), data->_info, egl_buffer);
    }
		glClearColor(0, 0, 0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);

		glBindTexture(GL_TEXTURE_EXTERNAL_OES, egl_buffer.texture);
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_INT, 0);
		EGLBoolean success [[maybe_unused]] = eglSwapBuffers(egl_display, egl_surface);
  }
  
  std::cout << "Exiting graphics runner" << std::endl;

      glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}


GLFWHandler::GLFWHandler() {
  glfw_thread = std::make_unique<std::jthread>(std::bind_front(&GLFWHandler::Run, this));
}

void GLFWHandler::Close() {
  if (glfw_thread != nullptr) {
    if (glfw_thread->joinable() == true) {
      glfw_thread->request_stop();
      glfw_thread->join();
    }
    glfw_thread = nullptr;
  }
}

void GLFWHandler::Post(CompletedRequestPtr &&data) {
  if (graphics_ != nullptr) {
    graphics_->Post(std::move(data));
  }
}


void GLFWHandler::Run(std::stop_token st) {
  if (!glfwInit()) {
    std::cout << "unable to start glfw context; thread exiting early" << std::endl;
    return;
  }
  glfwSetTime(0);
  std::cout << "Starting graphics thread" << std::endl;
  graphics_ = std::shared_ptr<GlesHandler>(new GlesHandler());
  while (!st.stop_requested()) {
    std::cout << "Thread executing" << std::endl;
    std::this_thread::sleep_for(500ms);
  }
  std::cout << "Thread Exiting" << std::endl;
  
  graphics_->Close();
  glfwTerminate();
}