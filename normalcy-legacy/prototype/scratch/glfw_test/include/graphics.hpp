
#ifndef GRAPHICS_CLASSES_H
#define GRAPHICS_CLASSES_H

#include <functional>
#include <stop_token>
#include <thread>
#include <memory>
#include <condition_variable>
#include <queue>
#include <mutex>
#include <map>

#include <chrono>
using namespace std::literals;


#include "request.hpp"

extern std::function<void(int)> shutdown_handler;
extern void signal_handler(int signal);


class GlesHandler {
public:
  GlesHandler();
  void Close();
  void Post(CompletedRequestPtr &&data);
private:
  void Run(std::stop_token st);
  std::unique_ptr<std::jthread> gles_thread = nullptr;
  GLFWwindow *window;
  std::queue<CompletedRequestPtr> _queue;
  std::mutex _mutex;
  std::condition_variable _cv;
  std::map<int, EglBuffer> buffers_;
};

class GLFWHandler {
public:
  GLFWHandler();
  void Close();
  void Post(CompletedRequestPtr &&data);
private:
  void Run(std::stop_token st);
  std::shared_ptr<GlesHandler> graphics_ = nullptr;
  std::unique_ptr<std::jthread> glfw_thread = nullptr;
};


#endif // GRAPHICS_CLASSES_H