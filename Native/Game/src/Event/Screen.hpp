#pragma once
// The event camera's screen-effect tween record: the screen scale, tint,
// translation and radial warp tracks of Event::CameraState (Event/Camera.hpp).
// The rule is EventScreenTweens.hpp; EventScreenTweensLive.cpp loads and stores
// this record. The original toolchain's plain char was unsigned
// (-funsigned-char), so its bytes are std::uint8_t.
#include <cstddef>
#include <cstdint>

namespace VS::Event {

// decomp: func_800BDBB4_t (src/BATTLE/BATTLE.PRG/4DEA8.c:98)
// Up to three values eased together from their start values by their deltas.
struct TripleTween {
  std::uint8_t reserved00{};
  std::uint8_t easing_mode{};
  std::int16_t current_value0{};
  std::int16_t current_value1{};
  std::int16_t current_value2{};
  std::int16_t reserved08{};
  std::int16_t start_value0{};
  std::int16_t start_value1{};
  std::int16_t start_value2{};
  std::int16_t reserved10{};
  std::int16_t delta_value0{};
  std::int16_t delta_value1{};
  std::int16_t delta_value2{};
  std::int16_t reserved18{};
  std::int16_t reserved1a{};
  std::int16_t elapsed_frames{};
  std::int16_t duration_frames{};
};

namespace layout {
inline constexpr std::size_t kTripleTweenBytes = 0x20;
static_assert(sizeof(TripleTween) == kTripleTweenBytes);
}  // namespace layout

}  // namespace VS::Event
