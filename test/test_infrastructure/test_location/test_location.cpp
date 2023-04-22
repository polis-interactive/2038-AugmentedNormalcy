//
// Created by brucegoose on 4/19/23.
//

#include <doctest.h>
#include <iostream>

#include "infrastructure/location/location.hpp"

TEST_CASE("INFRASTRUCTURE_LOCATION-EnumerateSerialPorts") {
    auto serial_ports = infrastructure::EnumerateSerialPorts();
}