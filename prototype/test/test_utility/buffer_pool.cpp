//
// Created by bruce on 12/28/2022.
//

#include <doctest.h>
#include <array>

#include "utility/buffer_pool.hpp"


TEST_CASE("Check that this is even usable...") {
    auto pool = utility::BufferPool<std::array<uint8_t, 12>>(2, [](){ return std::make_shared<std::array<uint8_t, 12>>(); });
}

TEST_CASE("Check if it does anything...") {
    auto pool = utility::BufferPool<std::array<uint8_t, 12>>(
            2, [](){ return std::make_shared<std::array<uint8_t, 12>>(); }
    );

    auto buffer_1 = pool.New();

    // should just be buffer_1 and std::vector initializer that have references
    REQUIRE_EQ(buffer_1.use_count(), 2);
    // we took a buffer, should have one remaining
    REQUIRE_EQ(pool.AvailableBuffers(), 1);

    pool.Free(std::move(buffer_1));
    // giving back the buffer should free it as there are no references
    REQUIRE_EQ(pool.AvailableBuffers(), 2);
    // we should have dumped the buffer
    REQUIRE_EQ(buffer_1.get(), nullptr);

    // now take 2 references
    buffer_1 = pool.New();
    REQUIRE_NE(buffer_1.get(), nullptr);
    auto buffer_2 = buffer_1;
    // vector, buf 1, buf 2
    REQUIRE_EQ(buffer_1.use_count(), 3);

    pool.Free(std::move(buffer_1));
    // shouldn't free as buffer_2 has a reference
    REQUIRE_EQ(pool.AvailableBuffers(), 1);
    REQUIRE_EQ(buffer_1.get(), nullptr);
    REQUIRE_NE(buffer_2.get(), nullptr);
    // now the second free should work
    pool.Free(std::move(buffer_2));
    REQUIRE_EQ(pool.AvailableBuffers(), 2);
    REQUIRE_EQ(buffer_2.get(), nullptr);

    // lastly, lets make the pool panic
    buffer_1 = pool.New();
    buffer_2 = pool.New();
    REQUIRE_EQ(pool.AvailableBuffers(), 0);
    REQUIRE_THROWS(
        [](utility::BufferPool<std::array<uint8_t, 12>> &pool) {
            auto buffer_3 = pool.New();
        }(pool)
    );
}