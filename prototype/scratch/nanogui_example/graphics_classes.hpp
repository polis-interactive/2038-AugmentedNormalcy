
#ifndef GRAPHICS_CLASSES_H
#define GRAPHICS_CLASSES_H

#include  <functional>
#include <stop_token>
#include <thread>
#include <memory>

#include <chrono>
using namespace std::literals;

#define GLAD_GLES2_IMPLEMENTATION
#include <glad/gles2.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>


class GLFWHandler {
public:
  GLFWHandler();
  void Close();
private:
  void Run(std::stop_token st);
  std::unique_ptr<std::jthread> glfw_thread = nullptr;
};

class GlesHandler {
public:
  GlesHandler();
  void Close();
private:
  void Run(std::stop_token st);
  std::unique_ptr<std::jthread> gles_thread = nullptr;
  GLFWwindow *window;
};


#endif // GRAPHICS_CLASSES_H