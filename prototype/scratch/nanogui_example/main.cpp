
#include <iostream>
#include <signal.h>
#include <cstdio>

#include "linmath.h"

#include "graphics_classes.hpp"


std::function<void(int)> shutdown_handler;
void signal_handler(int signal) { shutdown_handler(signal); }

struct Vertex
{
    vec2 pos;
    vec3 col;
};

const Vertex vertices[3] =
{
    { { -0.6f, -0.4f }, { 1.f, 0.f, 0.f } },
    { {  0.6f, -0.4f }, { 0.f, 1.f, 0.f } },
    { {   0.f,  0.6f }, { 0.f, 0.f, 1.f } }
};

static const char* vertex_shader_text =
"#version 100\n"
"precision mediump float;\n"
"uniform mat4 MVP;\n"
"attribute vec3 vCol;\n"
"attribute vec2 vPos;\n"
"varying vec3 color;\n"
"void main()\n"
"{\n"
"    gl_Position = MVP * vec4(vPos, 0.0, 1.0);\n"
"    color = vCol;\n"
"}\n";

static const char* fragment_shader_text =
"#version 100\n"
"precision mediump float;\n"
"varying vec3 color;\n"
"void main()\n"
"{\n"
"    gl_FragColor = vec4(color, 1.0);\n"
"}\n";

namespace GraphicsRunner {
  void WindowHints() {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);

    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_RED_BITS, 8);
    glfwWindowHint(GLFW_GREEN_BITS, 8);
    glfwWindowHint(GLFW_BLUE_BITS, 8);
    glfwWindowHint(GLFW_ALPHA_BITS, 8);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);

    glfwWindowHint(GLFW_DEPTH_BITS, 16);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
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

void GlesHandler::Run(std::stop_token st) {

  std::cout << "Running graphics handler" << std::endl;
  
  int width, height;
  width = 1920;
  height = 1080;
  GraphicsRunner::WindowHints();
  window = glfwCreateWindow(width, height, "OpenGL ES 2.0 Triangle (EGL)", NULL, NULL);
  if (!window)
  {
    std::cout << "Failed to create GLFW window background" << std::endl;
    return;
  }
  
  std::cout << "Made window?" << std::endl;
  
  glfwMakeContextCurrent(window);
  gladLoadGLES2(glfwGetProcAddress);
  glfwSwapInterval(1);

  GLuint vertex_buffer;
  glGenBuffers(1, &vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  const GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &vertex_shader_text, NULL);
  glCompileShader(vertex_shader);

  const GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &fragment_shader_text, NULL);
  glCompileShader(fragment_shader);

  const GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);

  const GLint mvp_location = glGetUniformLocation(program, "MVP");
  const GLint vpos_location = glGetAttribLocation(program, "vPos");
  const GLint vcol_location = glGetAttribLocation(program, "vCol");

  glEnableVertexAttribArray(vpos_location);
  glEnableVertexAttribArray(vcol_location);
  glVertexAttribPointer(vpos_location, 2, GL_FLOAT, GL_FALSE,
      sizeof(Vertex), (void*) offsetof(Vertex, pos));
  glVertexAttribPointer(vcol_location, 3, GL_FLOAT, GL_FALSE,
      sizeof(Vertex), (void*) offsetof(Vertex, col));
                          
  glViewport(0, 0, width, height);
  glfwSwapInterval(0);
  glfwSwapBuffers(window);
  
  glfwSetKeyCallback(window,
    [](GLFWwindow * w, int key, int scancode, int action, int mods) {
      if(key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(w, GLFW_TRUE);
        signal_handler(0);
      }
    }
  );
  
  const float ratio = width / (float) height;
  
  glfwShowWindow(window);       
  glfwFocusWindow(window);
    

  while (!glfwWindowShouldClose(window) && !st.stop_requested()) {
    
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);
        
    mat4x4 m, p, mvp;
    mat4x4_identity(m);
    mat4x4_rotate_Z(m, m, (float) glfwGetTime());
    mat4x4_ortho(p, -ratio, ratio, -1.f, 1.f, 1.f, -1.f);
    mat4x4_mul(mvp, p, m);

    glUseProgram(program);
    glUniformMatrix4fv(mvp_location, 1, GL_FALSE, (const GLfloat*) &mvp);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glfwSwapBuffers(window);
    
    glfwPollEvents();
  }

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


void GLFWHandler::Run(std::stop_token st) {
  if (!glfwInit()) {
    std::cout << "unable to start glfw context; thread exiting early" << std::endl;
    return;
  }
  glfwSetTime(0);
  std::cout << "Starting graphics thread" << std::endl;
  GlesHandler graphics;
  while (!st.stop_requested()) {
    std::cout << "Thread executing" << std::endl;
    std::this_thread::sleep_for(500ms);
  }
  std::cout << "Thread Exiting" << std::endl;
  
  graphics.Close();
  glfwTerminate();
}
  

int main(int /* argc */, char ** /* argv */) {

  freopen( "output.txt", "w", stdout );
    
  std::cout << "Starting glfw thread" << std::endl;

  
  GLFWHandler handler;
  bool exit = false;

  shutdown_handler = [&](int signal) {
    std::cout << "Server shutdown...\n";
    exit = true;
  };

  signal(SIGINT, signal_handler);
    
  
   while(!exit){
      std::this_thread::sleep_for(1s);
   }
   
   handler.Close();
   
   std::cout << "Prog exit!" << std::endl;
   
   return 0;
}
