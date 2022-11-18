//
// Created by bruce on 12/31/2022.
//

#include <doctest.h>

#include "infrastructure/common.hpp"

TEST_CASE("Rollover less than") {
    uint16_t a = 0;
    uint16_t b = 1;

    // trivial case
    REQUIRE_FALSE(rolling_less_than(a, a));

    // simple case, a < b
    REQUIRE(rolling_less_than(a, b));
    REQUIRE_FALSE(rolling_less_than(b, a));

    // clear rollover, a > b
    b = 65050;
    REQUIRE_FALSE(rolling_less_than(a, b));
    REQUIRE(rolling_less_than(b, a));
}