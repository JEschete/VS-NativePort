// Hand-maintained (tools/port/owned.csv).
#include "Field/PlatformPlacementLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Log.hpp"
#include "Field/PlatformPlacement.hpp"
#include "Legacy/Psx.h"
extern "C" {
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
}

namespace {
using VS::Field::kMovingPlatformCapacity;
using VS::Field::MovingPlatformPose;
using Poses = std::array<MovingPlatformPose, kMovingPlatformCapacity>;

// D_80068BEC holds seven offsets; frame 7 means no bounce.
constexpr std::size_t kBounceOffsetCount = 7;

std::span<const MovingPlatformPose> LivePlatforms(Poses& poses) {
  const auto bytes = Field_roomData.header.movingPlatformsLength;
  if (bytes % sizeof(Field_MpdMovingPlatform) != 0 ||
      bytes > poses.size() * sizeof(Field_MpdMovingPlatform)) {
    VS_LOG_ERROR("field.platform", "Invalid moving-platform span {}", bytes);
    return {};
  }
  if (Field_roomData.movingPlatforms == nullptr) return {};
  const std::span records(Field_roomData.movingPlatforms,
                          bytes / sizeof(Field_MpdMovingPlatform));
  for (std::size_t index = 0; index < records.size(); ++index) {
    const Field_MpdMovingPlatform& platform = records[index];
    poses[index] = {.x = platform.positionX,
                    .y = platform.positionY,
                    .z = platform.positionZ,
                    .bounce_frame = Field_platformBounceFrames[index],
                    .hidden = platform.isHidden != 0};
  }
  return std::span<const MovingPlatformPose>(poses).first(records.size());
}

template <typename Lookup>
std::optional<Field_PlatformContactPoint> LiveContact(int support_layer,
                                                      Lookup lookup) {
  Poses poses{};
  const VS::Field::MovingPlatformRoom room{
      .platforms = LivePlatforms(poses),
      .bounce_offsets = std::span<const std::int16_t>(
          Field_kPlatformBounceOffsets, kBounceOffsetCount)};
  const auto contact = lookup(room, support_layer);
  if (!contact) return std::nullopt;
  return Field_PlatformContactPoint{contact->x, contact->height, contact->z,
                                    contact->platform_index};
}
}  // namespace

namespace VS::Field {
std::optional<Field_PlatformContactPoint> LiveMovingPlatformPlacement(
    int support_layer) {
  return LiveContact(support_layer, MovingPlatformPlacement);
}

// decomp: BATTLE.PRG func_800A4A24 (src/BATTLE/BATTLE.PRG/3A1A0.c:1089).
std::optional<Field_PlatformContactPoint> FindLiveMovingPlatformById(
    int support_layer) {
  return LiveContact(support_layer, FindMovingPlatformById);
}
}  // namespace VS::Field
