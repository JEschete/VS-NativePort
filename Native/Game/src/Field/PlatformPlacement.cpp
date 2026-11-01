// Hand-maintained (tools/port/owned.csv).
#include "Field/PlatformPlacement.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "Core/Log.hpp"

namespace VS::Field {
namespace {
constexpr int kFixedOne = 4096;

std::optional<std::size_t> PlatformIndex(const MovingPlatformRoom& room,
                                         int support_layer) {
  if (support_layer < kFirstPlatformLayer ||
      support_layer >= kFirstPlatformLayer + kMovingPlatformCapacity ||
      room.platforms.size() > kMovingPlatformCapacity) {
    VS_LOG_ERROR("field.platform",
                 "Invalid moving-platform layer {} or {} platforms",
                 support_layer, room.platforms.size());
    return std::nullopt;
  }
  const auto index =
      static_cast<std::size_t>(support_layer - kFirstPlatformLayer);
  if (index >= room.platforms.size()) return std::nullopt;
  return index;
}

std::optional<PlatformContact> ContactOf(const MovingPlatformRoom& room,
                                         std::size_t index) {
  const MovingPlatformPose& platform = room.platforms[index];
  const int frame = platform.bounce_frame;
  if (frame < 0 || std::cmp_greater(frame, room.bounce_offsets.size())) {
    VS_LOG_ERROR("field.platform", "Invalid bounce frame {} for platform {}",
                 frame, index);
    return std::nullopt;
  }
  // The original seven-entry table is followed by a zero sentinel in BATTLE.
  const int bounce = std::cmp_equal(frame, room.bounce_offsets.size())
                         ? 0
                         : room.bounce_offsets[static_cast<std::size_t>(frame)];
  const int x = platform.x / kFixedOne;
  const int height = bounce - platform.y / kFixedOne;
  const int z = platform.z / kFixedOne;
  if (!std::in_range<std::int16_t>(x) || !std::in_range<std::int16_t>(height) ||
      !std::in_range<std::int16_t>(z)) {
    VS_LOG_ERROR("field.platform",
                 "Platform {} placement exceeds actor coordinates ({},{},{})",
                 index, x, height, z);
    return std::nullopt;
  }
  return PlatformContact{.x = static_cast<std::int16_t>(x),
                         .height = static_cast<std::int16_t>(height),
                         .z = static_cast<std::int16_t>(z),
                         .platform_index = static_cast<std::int16_t>(index)};
}
}  // namespace

// F00901/3A1A0.c:274 and func_8008D2C0/146C.c:16763 share this transform.
// Placement may precede the scene's visibility command; collision stays hidden.
std::optional<PlatformContact> MovingPlatformPlacement(
    const MovingPlatformRoom& room, int support_layer) {
  const auto index = PlatformIndex(room, support_layer);
  if (!index) {
    VS_LOG_ERROR("field.platform",
                 "Placement requires authored platform layer {}",
                 support_layer);
    return std::nullopt;
  }
  return ContactOf(room, *index);
}

// decomp: BATTLE.PRG func_800A4A24 (F00920 src/BATTLE/BATTLE.PRG/3A1A0.c:1089,
// F00938 40564.c:120). The original returned its local contact array.
std::optional<PlatformContact> FindMovingPlatformById(
    const MovingPlatformRoom& room, int support_layer) {
  const auto index = PlatformIndex(room, support_layer);
  if (!index || room.platforms[*index].hidden) return std::nullopt;
  return ContactOf(room, *index);
}
}  // namespace VS::Field
