
#pragma once

#include <memory>
#include <map>
#include <queue>
#include <vector>

#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/framebuffer_allocator.h>

#include <libcamera/property_ids.h>

#include "request.hpp"
#include "graphics.hpp"

namespace properties = libcamera::properties;

class CameraTest
{
public:
  using CameraManager = libcamera::CameraManager;
  using Stream = libcamera::Stream;
  using StreamRole = libcamera::StreamRole;
  using StreamRoles = libcamera::StreamRoles;
  using FrameBuffer = libcamera::FrameBuffer;
  using FrameBufferAllocator = libcamera::FrameBufferAllocator;
  using Request = libcamera::Request;
  using CameraConfiguration = libcamera::CameraConfiguration;
  using ControlList = libcamera::ControlList;
  using Size = libcamera::Size;
  using BufferMap = Request::BufferMap;
  CameraTest(GLFWHandler &preview);
  ~CameraTest();

protected:
  std::vector<libcamera::Span<uint8_t>> Mmap(FrameBuffer *buffer) const;
  StreamInfo GetStreamInfo(Stream const *stream) const;
  void openCamera();
  void configureViewfinder();
  void startCamera();
  void makeRequests();
  void requestComplete(Request *request);
  void queueRequest(CompletedRequest *completed_request);
  void stopCamera();
  void teardown();
  void closeCamera();
  std::unique_ptr<CameraManager> camera_manager_;
  std::shared_ptr<libcamera::Camera> camera_;
  std::unique_ptr<CameraConfiguration> configuration_;
  FrameBufferAllocator *allocator_ = nullptr;
  std::map<Stream *, std::queue<FrameBuffer *>> frame_buffers_;
  std::map<FrameBuffer *, std::vector<libcamera::Span<uint8_t>>> mapped_buffers_;
  std::map<std::string, Stream *> streams_;
  std::vector<std::unique_ptr<Request>> requests_;
  ControlList _controls;
  std::mutex camera_stop_mutex_;
  uint64_t sequence_ = 0;

  int status_;
  bool camera_started_ = false;
  bool camera_acquired_  = false;
  bool viewfinder_configured_ = false;

  GLFWHandler &preview_;
};