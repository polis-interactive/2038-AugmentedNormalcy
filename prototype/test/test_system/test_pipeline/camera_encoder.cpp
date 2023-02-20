//
// Created by brucegoose on 2/20/23.
//

#include <doctest.h>

#include <filesystem>
#include <fstream>

#include <chrono>
using namespace std::literals;

#include "infrastructure/camera/camera.hpp"
#include "infrastructure/codec/codec.hpp"

// this should only work on the rpi...
#if _AN_PLATFORM_ == PLATFORM_RPI

struct HardcodedRpiConfig : Camera::Config, Codec::Config {
    [[nodiscard]] Camera::Type get_camera_type() const final {
        return Camera::Type::LIBCAMERA;
    }
    [[nodiscard]] Codec::Type get_codec_type() const final {
        return Codec::Type::V4L2;
    }
    [[nodiscard]] std::pair<int, int> get_camera_width_height() const final {
        return {1536, 864};
    }
    [[nodiscard]] std::pair<int, int> get_codec_width_height() const final {
        return {1536, 864};
    }
    [[nodiscard]] int get_fps() const final {
        return 30;
    }
    [[nodiscard]] int get_camera_buffer_count() const final {
        return 4;
    }
    [[nodiscard]] int get_encoder_buffer_count() const final {
        return 4;
    }
};

class HardcodedRpiRecorder {
public:
    HardcodedRpiRecorder() {
       _context = Codec::Context::Create(_config);
       _encoder = Codec::Encoder::Create(
           _config, _context, std::bind_front(&HardcodedRpiRecorder::EncoderCallback, this)
       );
       _camera = Camera::Camera::Create(_config, std::bind_front(&HardcodedRpiRecorder::CameraCallback, this));
       setupOutput();
    }
    void Start() {
        _encoder->Start();
        _camera->Start();
    }
    void Stop() {
        _camera->Stop();
        _encoder->Stop();
    }
    std::size_t FileSize() {
        return _test_file_out.tellp();
    }
    ~HardcodedRpiRecorder() {
        _test_file_out.close();
    }
private:
    void setupOutput() {
        std::filesystem::path out_frame = TEST_DIR;
        out_frame /= "test_system";
        out_frame /= "test_pipeline";
        out_frame /= "out.h264";
        if(std::filesystem::remove(out_frame)) {
            std::cout << "Removed output file" << std::endl;
        } else {
            std::cout << "No output file to remove" << std::endl;
        }
        _test_file_out = std::ofstream(out_frame, std::ios::out | std::ios::binary);
        REQUIRE(_test_file_out.is_open());
    }
    void CameraCallback(std::shared_ptr<void> &&ptr) {
        _encoder->QueueEncode(std::move(ptr));
    }
    void EncoderCallback(std::shared_ptr<void> &&ptr) {
        auto encoded_buffer = std::static_pointer_cast<SizedPayloadBuffer>(ptr);
        _test_file_out.write(reinterpret_cast<char*>(encoded_buffer->GetMemory()), encoded_buffer->GetSize());
        _test_file_out.flush();
    }
    HardcodedRpiConfig _config;
    std::shared_ptr<Camera::Camera> _camera;
    std::shared_ptr<Codec::Context> _context;
    std::shared_ptr<Codec::Encoder> _encoder;
    std::ofstream _test_file_out;
};


TEST_CASE("SYSTEM_PIPELINE-Hardcoded_Rpi_Recorder") {
    HardcodedRpiRecorder recorder;
    recorder.Start();
    std::this_thread::sleep_for(1s);
    recorder.Stop();
    REQUIRE_GT(recorder.FileSize(), 1000);
}

#endif