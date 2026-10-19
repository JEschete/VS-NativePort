#pragma once

#include <cstdlib>

#include "Core/Log.hpp"

#define VS_ASSERT(cond)                                                   \
  do {                                                                    \
    if (!(cond)) {                                                        \
      ::VS::Core::LogWrite(::VS::Core::LogLevel::Error, "assert", #cond); \
      std::abort();                                                       \
    }                                                                     \
  } while (0)
