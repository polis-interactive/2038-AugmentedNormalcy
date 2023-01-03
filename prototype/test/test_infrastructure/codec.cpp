//
// Created by bruce on 12/31/2022.
//

#include <doctest.h>

#include <utility>

#include "infrastructure/codec/codec.hpp"
#include "infrastructure/codec/decoder.hpp"
#include "infrastructure/codec/encoder.hpp"

struct TestConfig : infrastructure::CodecConfig {
    [[nodiscard]] std::tuple<int, int> get_codec_width_height() const override {
        return std::tuple{1920, 1080};
    }
    [[nodiscard]] bool get_is_60_fps() const override {
        return true;
    }
};

using payload_send_function = std::function<void(std::shared_ptr<payload_buffer> &&buffer, std::size_t buffer_size)>;

struct RiggedSender: PayloadSend {
    explicit RiggedSender(payload_buffer_pool &b_pool, payload_send_function sender) :
        _b_pool(b_pool), _sender(std::move(sender))
    {}
    std::shared_ptr<payload_buffer> GetBuffer() override {
        return std::move(_b_pool.New());
    }
    void Send(std::shared_ptr<payload_buffer> &&buffer, std::size_t buffer_size) override {
        _sender(std::move(buffer), buffer_size);
    };
    payload_buffer_pool &_b_pool;
    payload_send_function _sender;
};

struct RiggedReceiver : QueuedPayloadReceive{
    RiggedReceiver(
        payload_buffer_pool &b_pool, std::shared_ptr<payload_buffer> &&buffer,
        std::size_t buffer_size
    ):
        _b_pool(b_pool),
        _buffer(buffer),
        _bytes_received(buffer_size)
    {}
    [[nodiscard]] payload_tuple GetPayload() override {
        return { _buffer, _bytes_received};
    }
    ~RiggedReceiver() {
        _b_pool.Free(std::move(_buffer));
    }
    std::shared_ptr<payload_buffer> _buffer;
    std::size_t _bytes_received;
    payload_buffer_pool &_b_pool;
};



TEST_CASE("Just messing with cuda") {
    auto conf = TestConfig();
    auto ctx = infrastructure::CodecContext();
    auto b_pool = payload_buffer_pool(
        1, [](){ return std::make_shared<payload_buffer>(); }
    );
    auto callback = [](std::shared_ptr<uint8_t *> ptr){
        // should write to file
        std::cout << ptr.get() << std::endl;
    };
    auto dec = infrastructure::Decoder(conf, ctx, callback);

    auto rigged_sender = std::make_shared<RiggedSender>(
        b_pool,
        [&b_pool, &dec](std::shared_ptr<payload_buffer> &&buffer, std::size_t buffer_size) {
            auto rigged_receiver = std::make_shared<RiggedReceiver>(b_pool, std::move(buffer), buffer_size);
            dec.QueueDecode(std::move(rigged_receiver));
        }
    );
    infrastructure::Encoder enc(conf, ctx, rigged_sender);

    // no we input a cuda mapped image from this file, and the pipeline should write out a modifed version of that file.
    // to check "success", the output should look something like the input file

}