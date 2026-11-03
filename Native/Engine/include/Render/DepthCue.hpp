#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>

// Depth cueing (fog) for the map and field geometry: the near-plane cue the
// projection writes per vertex, and the colour blend toward the far colour.
// The policies read no global state; the GTE-backed registers and far colour
// are bound in DepthCueLive.cpp.
namespace VS::Render {
// The projection's cue is (h / z) * coefficient + offset, Q12 clamped to
// [0, 4096] (GTE DQA/DQB).
struct DepthCue {
  std::int16_t coefficient = 0;
  std::int32_t offset = 0;
};

// The cue's distance scale (5 * 64) and offset (5 << 22, cue 5 * 1024 at an
// infinite distance before clamping).
inline constexpr std::int64_t kFogNearScale = 320;
inline constexpr std::int32_t kFogCueOffset = 0x1400000;

// SLUS_010.40 SetFogNear (libgte, 32154.s:9). The cue is zero up to
// `near_distance` and rises past it. The original wrapped -(320 * a) / h to a
// halfword and divided unchecked; the native policy (#195) turns fog off for a
// non-positive distance, treats a non-positive projection distance as 1 and
// saturates the coefficient.
[[nodiscard]] DepthCue FogNearDepthCue(
    std::int32_t near_distance, std::int32_t projection_distance) noexcept;

class DepthCueRegisters {
 public:
  virtual ~DepthCueRegisters() = default;
  virtual void SetDepthCue(const DepthCue& cue) = 0;
};

void SetFogNearDistance(DepthCueRegisters& registers,
                        std::int32_t near_distance,
                        std::int32_t projection_distance);

struct ColorCode {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
  std::uint8_t code = 0;

  friend bool operator==(const ColorCode&, const ColorCode&) = default;
};

inline constexpr std::size_t kColorChannels = 3;

// The far colour as the GTE holds it: each channel times 16.
using FarColor = std::array<std::int32_t, kColorChannels>;

// SLUS_010.40 DpqColor (libgte DPCS, 32154.s): each channel moves toward the
// far colour by cue / 4096. The cue is a halfword (IR0), the difference
// saturates to a halfword (IR1-3) and the result to a byte; the code byte is
// kept.
[[nodiscard]] ColorCode ApplyDepthCueColor(const ColorCode& color,
                                           std::int32_t cue,
                                           const FarColor& far_color) noexcept;
}  // namespace VS::Render
