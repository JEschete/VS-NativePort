// Hand-maintained (tools/port/owned.csv).
#include "Render/DepthCue.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace VS::Render {
namespace {
constexpr std::int32_t kChannelScale = 16;
constexpr int kCueShift = 12;
constexpr int kChannelShift = 4;
constexpr std::int32_t kChannelMax = 0xFF;

std::uint8_t BlendChannel(std::uint8_t channel, std::int16_t cue,
                          std::int32_t far_channel) {
  constexpr std::int64_t kIrMin = std::numeric_limits<std::int16_t>::min();
  constexpr std::int64_t kIrMax = std::numeric_limits<std::int16_t>::max();
  const std::int64_t scaled = std::int64_t{channel} * kChannelScale;
  const std::int64_t difference =
      std::clamp(std::int64_t{far_channel} - scaled, kIrMin, kIrMax);
  const std::int64_t blended = scaled + ((difference * cue) >> kCueShift);
  return static_cast<std::uint8_t>(
      std::clamp<std::int64_t>(blended >> kChannelShift, 0, kChannelMax));
}
}  // namespace

DepthCue FogNearDepthCue(std::int32_t near_distance,
                         std::int32_t projection_distance) noexcept {
  if (near_distance <= 0) {
    return {};
  }
  const std::int64_t divisor = std::max(projection_distance, 1);
  const std::int64_t coefficient =
      -(std::int64_t{near_distance} * kFogNearScale) / divisor;
  return {.coefficient = static_cast<std::int16_t>(std::clamp<std::int64_t>(
              coefficient, std::numeric_limits<std::int16_t>::min(),
              std::numeric_limits<std::int16_t>::max())),
          .offset = kFogCueOffset};
}

void SetFogNearDistance(DepthCueRegisters& registers,
                        std::int32_t near_distance,
                        std::int32_t projection_distance) {
  registers.SetDepthCue(FogNearDepthCue(near_distance, projection_distance));
}

ColorCode ApplyDepthCueColor(const ColorCode& color, std::int32_t cue,
                             const FarColor& far_color) noexcept {
  const auto halfword_cue = static_cast<std::int16_t>(cue);
  return {.red = BlendChannel(color.red, halfword_cue, far_color[0]),
          .green = BlendChannel(color.green, halfword_cue, far_color[1]),
          .blue = BlendChannel(color.blue, halfword_cue, far_color[2]),
          .code = color.code};
}
}  // namespace VS::Render
