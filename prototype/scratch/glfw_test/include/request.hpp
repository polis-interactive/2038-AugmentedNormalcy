
#pragma once

#include <memory>

#include <libcamera/controls.h>
#include <libcamera/request.h> 
#include <libcamera/color_space.h>
#include <libcamera/pixel_format.h>
#include <libcamera/stream.h>

#define GLAD_GL_IMPLEMENTATION
#include "glad/glad_egl.h"
#include "glad/glad.h"

#define GLFW_INCLUDE_NONE 1
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_EGL 1
#define GLFW_NATIVE_INCLUDE_NONE 1
#include <GLFW/glfw3native.h>

struct StreamInfo
{
	StreamInfo() : width(0), height(0), stride(0) {}
	unsigned int width;
	unsigned int height;
	unsigned int stride;
	libcamera::PixelFormat pixel_format;
	std::optional<libcamera::ColorSpace> colour_space;
};


struct CompletedRequest
{
	using BufferMap = libcamera::Request::BufferMap;
	using ControlList = libcamera::ControlList;
	using Request = libcamera::Request;
  using Stream = libcamera::Stream;
  using StreamConfiguration = libcamera::StreamConfiguration;

	CompletedRequest(unsigned int seq, Request *r, int fd, libcamera::Span<uint8_t> &&span, StreamInfo &&info)
		: sequence(seq), metadata(r->metadata()), buffers(r->buffers()), request(r), _fd(fd), _span(span), _info(info)
	{
		r->reuse();
	}
	unsigned int sequence;
	ControlList metadata;
	Request *request;

	BufferMap buffers;
  int _fd;
  libcamera::Span<uint8_t> _span;
  StreamInfo _info;
};

using CompletedRequestPtr = std::shared_ptr<CompletedRequest>;


struct EglBuffer
{
  EglBuffer() : fd(-1) {}
  int fd;
	size_t size;
	StreamInfo info;
	GLuint texture;
};
 


