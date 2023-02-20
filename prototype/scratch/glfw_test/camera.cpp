
#include <iostream>
#include <libcamera/formats.h>
#include <libcamera/control_ids.h>
#include <libcamera/controls.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>

#include "camera.hpp"

using namespace libcamera;
namespace controls = libcamera::controls;

CameraTest::CameraTest(GLFWHandler &preview) : preview_(preview), _controls(controls::controls) {
  openCamera();
  if (!camera_acquired_) {
    return;
  }
  configureViewfinder();
  if (!viewfinder_configured_) {
    return;
  }
  startCamera();
  if (!camera_started_) {
    return;
  }
  std::cout << "Init successful!" << std::endl;
}

void CameraTest::openCamera() {

  /* Get Camera Manager */
  camera_manager_ = std::make_unique<CameraManager>();
  if (camera_manager_->start()) {
    std::cerr << "Failed to start camera manager" << std::endl;
    return;
  }

  /* Get Camera */
  camera_ = camera_manager_->cameras().at(0);
  if (!camera_) {
    std::cerr << "No Cameras Available" << std::endl;
    return;
  }
  if (camera_->acquire()) {
    std::cerr << "Failed to acquire the camera" << std::endl;
    return;
  }
  std::cout << "Acquired camera: " << camera_->id() << std::endl;
  camera_acquired_  = true;

}

void CameraTest::configureViewfinder() {

  /* configure role */
  StreamRoles stream_roles = { StreamRole::Viewfinder };
  configuration_ = camera_->generateConfiguration(stream_roles);
  if (!configuration_) {
    throw std::runtime_error("failed to generate viewfinder configuration");
  }

  /* bin viewfinder */
  Size size(1920, 1080);
  auto area = camera_->properties().get(properties::PixelArrayActiveAreas);
  size = (*area)[0].size() / 2;
  size = size.boundedToAspectRatio(Size(1920, 1080));
  size.alignDownTo(2, 2); // YUV420 will want to be even
  std::cout << "Viewfinder size chosen is " << size.toString() << std::endl;

  /* configure stream */
  configuration_->at(0).pixelFormat = libcamera::formats::YUV420;
  configuration_->at(0).size = size;
  configuration_->at(0).bufferCount = 4;
  _controls.set(controls::draft::NoiseReductionMode, 3);

  /* validate */
  CameraConfiguration::Status validation = configuration_->validate();
  if (validation == CameraConfiguration::Invalid)
    throw std::runtime_error("failed to valid stream configurations");
  else if (validation == CameraConfiguration::Adjusted)
    std::cout << "Stream configuration adjusted" << std::endl;

  if (camera_->configure(configuration_.get()) < 0)
    throw std::runtime_error("failed to configure streams");
  std::cout << "Camera streams configured" << std::endl;

  /* allocating buffers */
  allocator_ = new FrameBufferAllocator(camera_);
  for (StreamConfiguration &config : *configuration_)
  {
    Stream *stream = config.stream();

    if (allocator_->allocate(stream) < 0)
      throw std::runtime_error("failed to allocate capture buffers");

    for (const std::unique_ptr<FrameBuffer> &buffer : allocator_->buffers(stream))
    {
      // "Single plane" buffers appear as multi-plane here, but we can spot them because then
      // planes all share the same fd. We accumulate them so as to mmap the buffer only once.
      size_t buffer_size = 0;
      for (unsigned i = 0; i < buffer->planes().size(); i++)
      {
        const FrameBuffer::Plane &plane = buffer->planes()[i];
        buffer_size += plane.length;
        if (i == buffer->planes().size() - 1 || plane.fd.get() != buffer->planes()[i + 1].fd.get())
        {
          void *memory = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, plane.fd.get(), 0);
          mapped_buffers_[buffer.get()].push_back(
            libcamera::Span<uint8_t>(static_cast<uint8_t *>(memory), buffer_size));
          buffer_size = 0;
        }
      }
      frame_buffers_[stream].push(buffer.get());
    }
  }
  std::cout << "Buffers allocated and mapped" << std::endl;

  streams_["viewfinder"] = configuration_->at(0).stream();
  std::cout << "Viewfinder setup complete" << std::endl;
  viewfinder_configured_ = true;
}

void CameraTest::startCamera() {
  makeRequests();

  /* setting controls */
   int64_t frame_time = 1000000 / 30.0;
   _controls.set(controls::FrameDurationLimits, libcamera::Span<const int64_t, 2>({ frame_time, frame_time }));
   _controls.set(controls::Saturation, 1.0);
   _controls.set(controls::Contrast, 1.0);
   _controls.set(controls::Brightness, 0.0);
   _controls.set(controls::Sharpness, 1.0);
   _controls.set(controls::ExposureValue, 0.0);
   _controls.set(controls::AeExposureMode, 0);
   _controls.set(controls::AeMeteringMode, 0);
   _controls.set(controls::AwbMode, 0);
   _controls.set(controls::draft::NoiseReductionMode, 3);

  if (camera_->start(&_controls))
    throw std::runtime_error("failed to start camera");

  _controls.clear();
  camera_started_ = true;

  camera_->requestCompleted.connect(this, &CameraTest::requestComplete);
  for (std::unique_ptr<Request> &request : requests_)
  {
    if (camera_->queueRequest(request.get()) < 0)
      throw std::runtime_error("Failed to queue request");
  }

}

void CameraTest::makeRequests() {
  auto free_buffers(frame_buffers_);
  while (true)
  {
    for (StreamConfiguration &config : *configuration_)
    {
      Stream *stream = config.stream();
      if (stream == configuration_->at(0).stream())
      {
        if (free_buffers[stream].empty())
        {
          std::cout << "Requests created" << std::endl;
          return;
        }
        std::unique_ptr<Request> request = camera_->createRequest();
        if (!request)
          throw std::runtime_error("failed to make request");
        requests_.push_back(std::move(request));
      }
      else if (free_buffers[stream].empty())
        throw std::runtime_error("concurrent streams need matching numbers of buffers");

      FrameBuffer *buffer = free_buffers[stream].front();
      free_buffers[stream].pop();
      if (requests_.back()->addBuffer(stream, buffer) < 0)
        throw std::runtime_error("failed to add buffer to request");
    }
  }
}

void CameraTest::requestComplete(Request *request) {
  if (request->status() == Request::RequestCancelled)
  {
    std::cout << "WE GOT FKD D:" << std::endl; 
    return;
  }

  Stream *stream = configuration_->at(0).stream();
	StreamInfo info = GetStreamInfo(stream);
	auto &buffers = request->buffers();
	FrameBuffer* buffer = request->buffers().at(stream);
	libcamera::Span span = mapped_buffers_.at(buffer)[0];
	int fd = buffer->planes()[0].fd.get();

  CompletedRequest *r = new CompletedRequest(sequence_++, request, fd, std::move(span), std::move(info));
  CompletedRequestPtr payload(r, [this](CompletedRequest *cr) { this->queueRequest(cr); });
  preview_.Post(std::move(payload));
}

std::vector<libcamera::Span<uint8_t>> CameraTest::Mmap(FrameBuffer *buffer) const
{
	auto item = mapped_buffers_.find(buffer);
	if (item == mapped_buffers_.end())
		return {};
	return item->second;
}

StreamInfo CameraTest::GetStreamInfo(Stream const *stream) const
{
	StreamConfiguration const &cfg = stream->configuration();
	StreamInfo info;
	info.width = cfg.size.width;
	info.height = cfg.size.height;
	info.stride = cfg.stride;
	info.pixel_format = stream->configuration().pixelFormat;
	info.colour_space = stream->configuration().colorSpace;
	return info;
}


void CameraTest::queueRequest(CompletedRequest *completed_request) {
	BufferMap buffers(std::move(completed_request->buffers));
	std::lock_guard<std::mutex> stop_lock(camera_stop_mutex_);
	Request *request = completed_request->request;
	delete completed_request;
	assert(request);
	if (!camera_started_)
		return;
	for (auto const &p : buffers)
	{
		if (request->addBuffer(p.first, p.second) < 0)
			throw std::runtime_error("failed to add buffer to request in QueueRequest");
	}
	if (camera_->queueRequest(request) < 0)
		throw std::runtime_error("failed to queue request");
}

void CameraTest::stopCamera() {
	{
		std::lock_guard<std::mutex> lock(camera_stop_mutex_);
		if (camera_started_) {
    	    if (camera_->stop()) {
                throw std::runtime_error("failed to stop camera");
            }
            camera_started_ = false;
        }
	}

  if (camera_) {
      camera_->requestCompleted.disconnect(this, &CameraTest::requestComplete);
  }

  requests_.clear();
  _controls.clear();
}

void CameraTest::teardown() {

  for (auto &iter : mapped_buffers_)
  {
    // assert(iter.first->planes().size() == iter.second.size());
    // for (unsigned i = 0; i < iter.first->planes().size(); i++)
    for (auto &span : iter.second)
      munmap(span.data(), span.size());
  }
  mapped_buffers_.clear();

  delete allocator_;
  allocator_ = nullptr;

  configuration_.reset();

  frame_buffers_.clear();

  streams_.clear();
}

void CameraTest::closeCamera() {
  if (camera_acquired_) {
    camera_->release();
    camera_acquired_ = false;
  }
  camera_.reset();
  camera_manager_.reset();
}

CameraTest::~CameraTest() {
  stopCamera();
  teardown();
  closeCamera();
}
