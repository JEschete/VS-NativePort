#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Event/ScriptVmLive.hpp"

namespace VS::test_support {

inline Event::ScriptLocation MountLiveEventScript(
    std::vector<std::uint8_t> bytes,
    std::weak_ptr<const std::vector<std::uint8_t>>* lifetime = nullptr) {
  const auto owner =
      std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes));
  if (lifetime != nullptr) {
    *lifetime = owner;
  }
  REQUIRE(
      Event::MountLiveScriptSource(Event::ScriptRegion::kEventScript, owner));
  const auto source =
      Event::LeaseLiveScriptSource(Event::ScriptRegion::kEventScript);
  REQUIRE(source);
  REQUIRE(source->At(0));
  return *source->At(0);
}

}  // namespace VS::test_support
