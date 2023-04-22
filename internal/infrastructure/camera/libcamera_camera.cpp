//
// Created by brucegoose on 2/18/23.
//

#include "libcamera_camera.hpp"

#include <iostream>
#include <sys/mman.h>

#include <libcamera/controls.h>
#include <libcamera/control_ids.h>
#include <libcamera/formats.h>

using CameraConfiguration = libcamera::CameraConfiguration;
using CameraManager = libcamera::CameraManager;
namespace controls = libcamera::controls;
using FrameBuffer = libcamera::FrameBuffer;
using FrameBufferAllocator = libcamera::FrameBufferAllocator;
namespace formats = libcamera::formats;
using Request = libcamera::Request;
using Size = libcamera::Size;
using Stream = libcamera::Stream;
using StreamRole = libcamera::StreamRole;
using StreamRoles = libcamera::StreamRoles;

namespace infrastructure {
    void LibcameraCamera::CreateCamera(const CameraConfig &config) {
        openCamera();
        configureViewFinder(config);
        setupBuffers(config.get_camera_buffer_count());
        _streams["viewfinder"] = _configuration->at(0).stream();
        _frame_rate = config.get_fps();
    }

    void LibcameraCamera::openCamera() {
        _camera_manager = std::make_unique<CameraManager>();
        if (_camera_manager->start()) {
            throw std::runtime_error("couldn't start camera manager");
        }
        // for now assume cameras are unique, one per device
        _camera = _camera_manager->cameras().at(0);
        if (!_camera) {
            throw std::runtime_error("No cameras available");
        }
        if (_camera->acquire()) {
            throw std::runtime_error("Failed to acquire camera");
        }
        _camera_acquired = true;
    }

    void LibcameraCamera::configureViewFinder(const CameraConfig &config) {
        StreamRoles stream_roles = { StreamRole::Viewfinder };
        _configuration = _camera->generateConfiguration(stream_roles);
        if (!_configuration) {
            throw std::runtime_error("failed to generate viewfinder configuration");
        }
        const auto &[width, height] = config.get_camera_width_height();
        Size size(width, height);
        size.alignDownTo(2, 2); // YUV420 will want to be even
        std::cout << "Viewfinder size chosen is " << size.toString() << std::endl;

        _configuration->at(0).pixelFormat = formats::YUV420;
        _configuration->at(0).size = size;
        _configuration->at(0).bufferCount = config.get_camera_buffer_count();

        CameraConfiguration::Status validation = _configuration->validate();
        if (validation == CameraConfiguration::Invalid)
            throw std::runtime_error("failed to valid stream configurations");
        else if (validation == CameraConfiguration::Adjusted)
            std::cout << "Stream configuration adjusted" << std::endl;

        if (_camera->configure(_configuration.get()) < 0)
            throw std::runtime_error("failed to configure streams");

        std::cout << "Camera streams configured" << std::endl;
    }

    void LibcameraCamera::setupBuffers(const int &camera_buffer_count) {
        _allocator = std::make_unique<FrameBufferAllocator>(_camera);
        for (auto &config : *_configuration)
        {
            auto *stream = config.stream();

            if (_allocator->allocate(stream) < 0)
                throw std::runtime_error("failed to allocate capture buffers");

            for (const std::unique_ptr<FrameBuffer> &buffer : _allocator->buffers(stream))
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
                        _mapped_buffers[buffer.get()].push_back(
                                libcamera::Span<uint8_t>(static_cast<uint8_t *>(memory), buffer_size)
                        );
                        buffer_size = 0;
                    }
                }
                _frame_buffers[stream].push(buffer.get());
            }
        }
    }

    void LibcameraCamera::StartCamera() {
        makeRequests();
        setControls();
        // set done callback
        _camera->requestCompleted.connect(this, &LibcameraCamera::requestComplete);
        // queue requests
        for (auto &request : _requests)
        {
            if (_camera->queueRequest(request.get()) < 0)
                throw std::runtime_error("Failed to queue request");
        }
    }

    void LibcameraCamera::makeRequests() {
        auto free_buffers(_frame_buffers);
        while (true)
        {
            for (auto &config : *_configuration)
            {
                auto *stream = config.stream();
                if (stream == _configuration->at(0).stream())
                {
                    if (free_buffers[stream].empty())
                    {
                        std::cout << "Requests created: " << _requests.size() << std::endl;
                        return;
                    }
                    std::unique_ptr<Request> request = _camera->createRequest();
                    if (!request)
                        throw std::runtime_error("failed to make request");
                    _requests.push_back(std::move(request));
                }
                else if (free_buffers[stream].empty())
                    throw std::runtime_error("concurrent streams need matching numbers of buffers");

                FrameBuffer *buffer = free_buffers[stream].front();
                free_buffers[stream].pop();
                if (_requests.back()->addBuffer(stream, buffer) < 0)
                    throw std::runtime_error("failed to add buffer to request");
            }
        }
    }

    void LibcameraCamera::setControls() {
        /* setting controls */
        int64_t frame_time = 1000000 / _frame_rate;
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

        if (_camera->start(&_controls))
            throw std::runtime_error("failed to start camera");

        // started!
        _controls.clear();
        _camera_started = true;
    }

    void LibcameraCamera::requestComplete(Request *request) {
        if (request->status() == Request::RequestCancelled)
        {
            // request failed, probably closing
            return;
        }

        // I can't believe I need all this to get the memory location
        const Stream *stream = _configuration->at(0).stream();
        auto &buffers = request->buffers();
        auto *buffer = buffers.at(stream);
        auto span = _mapped_buffers.at(buffer)[0];
        void *mem = span.data();

        int fd = buffer->planes()[0].fd.get();request->reuse(Request::ReuseBuffers);

        auto ts = request->metadata().get(controls::SensorTimestamp);
        int64_t timestamp_ns = (ts ? *ts : buffer->metadata().timestamp) / 1000;

        auto *out_buffer = new CameraBuffer(
            static_cast<void *>(request), mem, fd, span.size(), timestamp_ns
        );
        {
            std::lock_guard<std::mutex> lock(_camera_buffers_mutex);
            _camera_buffers.insert(out_buffer);
        }
        auto self(shared_from_this());
        auto out_ptr = std::shared_ptr<SizedBufferPool>(out_buffer, [this, self, out_buffer](SizedBufferPool *p) {
            this->queueRequest(out_buffer);
        });
        _send_callback(std::move(out_ptr));
    }

    void LibcameraCamera::queueRequest(CameraBuffer *buffer) {
        std::lock_guard<std::mutex> stop_lock(_camera_stop_mutex);
        bool request_found;
        {
            std::lock_guard<std::mutex> lock(_camera_buffers_mutex);
            auto it = _camera_buffers.find(buffer);
            if (it != _camera_buffers.end())
            {
                request_found = true;
                _camera_buffers.erase(it);
            }
            else {
                request_found = false;
            }
        }

        auto *request = static_cast<Request *>(buffer->GetRequest());
        delete buffer;
        if (!_camera_started || !request_found) {
            return;
        }

        request->reuse(Request::ReuseBuffers);
        if (_camera->queueRequest(request) < 0)
            throw std::runtime_error("failed to queue request");
    }

    void LibcameraCamera::StopCamera() {
        {
            std::lock_guard<std::mutex> lock(_camera_stop_mutex);
            if (_camera_started) {
                if (_camera->stop()) {
                    throw std::runtime_error("failed to stop camera");
                }
                _camera_started = false;
            }
        }
        if (_camera) {
            _camera->requestCompleted.disconnect(this, &LibcameraCamera::requestComplete);
        }
        _camera_buffers.clear();
        _requests.clear();
        _controls.clear();
    }

    void LibcameraCamera::teardownCamera() {
        for (auto &iter : _mapped_buffers)
        {
            for (auto &span : iter.second) {
                munmap(span.data(), span.size());
            }
        }
        _mapped_buffers.clear();
        _allocator.reset();
        _configuration.reset();
        _frame_buffers.clear();
        _streams.clear();
    }
    void LibcameraCamera::closeCamera() {
        if (_camera_acquired) {
            _camera->release();
            _camera_acquired = false;
        }
        _camera.reset();
        _camera_manager.reset();
    }
}