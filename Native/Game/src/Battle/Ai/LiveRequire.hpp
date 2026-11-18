#pragma once
// Contract refusals shared by the Battle AI Live bindings: a broken
// precondition is reported on the "ai" error channel, then aborts.

#include <cstdlib>
#include <optional>

#include "Core/Diagnostics.hpp"

namespace VS::Battle::Ai {

inline void Require(bool valid, const char* message) {
  if (!valid) {
    Core::Errorf("ai", "%s", message);
    std::abort();
  }
}

template <typename T>
T Required(const std::optional<T>& value, const char* message) {
  Require(value.has_value(), message);
  return *value;
}

}  // namespace VS::Battle::Ai
