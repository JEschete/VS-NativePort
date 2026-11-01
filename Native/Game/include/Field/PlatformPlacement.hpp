#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "Field/PlatformLayers.hpp"

// Moving-platform contact lookup. Platforms are passed as their position,
// visibility and bounce frame, so the lookup reads no room global or PS1
// address and returns its contact by value.
namespace VS::Field {
inline constexpr int kMovingPlatformCapacity = 4;

struct MovingPlatformPose {
  // World position in 1/4096 units.
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t z = 0;
  // Index into the bounce offsets; the offsets' size means no bounce.
  int bounce_frame = 0;
  bool hidden = false;
};

// An actor-space platform contact: x/z and height (up is negative) in world
// units, and the platform's index in the room.
struct PlatformContact {
  std::int16_t x = 0;
  std::int16_t height = 0;
  std::int16_t z = 0;
  std::int16_t platform_index = 0;

  friend bool operator==(const PlatformContact&,
                         const PlatformContact&) = default;
};

struct MovingPlatformRoom {
  std::span<const MovingPlatformPose> platforms;
  std::span<const std::int16_t> bounce_offsets;
};

// The authored contact of the platform on `support_layer`, hidden or not, for
// scripted placement. Empty, and logged, for a layer with no room platform, a
// bounce frame beyond the table or a position beyond actor coordinates.
std::optional<PlatformContact> MovingPlatformPlacement(
    const MovingPlatformRoom& room, int support_layer);

// The contact of the visible platform on `support_layer` (func_800A4A24).
// Empty when it is hidden, absent or invalid.
std::optional<PlatformContact> FindMovingPlatformById(
    const MovingPlatformRoom& room, int support_layer);
}  // namespace VS::Field
