#pragma once

#include "vs/core/Log.hpp"

#include <cstdlib>

#define VS_ASSERT(cond)                                                                           \
    do {                                                                                          \
        if (!(cond)) {                                                                            \
            ::VS::Core::LogWrite(::VS::Core::LogLevel::Error, "assert", #cond);                  \
            std::abort();                                                                         \
        }                                                                                         \
    } while (0)
