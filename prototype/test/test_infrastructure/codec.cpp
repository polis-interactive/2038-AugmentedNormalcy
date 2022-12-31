//
// Created by bruce on 12/31/2022.
//

#include <doctest.h>

#include "infrastructure/codec/decoder.hpp"
#include "Utils/Logger.h"


simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();

TEST_CASE("Just messing with cuda") {
    infrastructure::DecoderConfig conf;
    infrastructure::Decoder dec(conf);
    dec.Start();
    dec.Stop();
}