// Hand-maintained (tools/port/owned.csv).
#include "Field/MapLive.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Core/Log.hpp"
#include "Field/Map.hpp"
#include "Field/RoomStateLive.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Field/MapProgress.h"
#include "Field/Minimap.h"
#include "State/Progress.h"
}

namespace {
using VS::Field::RoomVisitFlags;

static_assert(sizeof(State_roomVisitProgress.roomFlags) ==
              sizeof(RoomVisitFlags));
static_assert(sizeof(Field_countableRoomMask) == sizeof(RoomVisitFlags));

RoomVisitFlags ToVisitFlags(std::span<const int> words) {
  RoomVisitFlags flags{};
  std::ranges::transform(words, flags.begin(), [](int word) {
    return static_cast<std::uint32_t>(word);
  });
  return flags;
}

// Cross-slice calls: retarget when those rows are promoted.
class LiveRoomLocationIndex final : public VS::Field::RoomLocationIndex {
 public:
  std::int32_t LocationId(std::int32_t zone_id,
                          std::int32_t map_index) override {
    return VS::Field::LiveFindLocationId(zone_id, map_index);
  }
};
// End of cross-slice calls.
}  // namespace

namespace VS::Field {
int SetLiveMinimapVisible(int visible) {
  std::uint8_t visibility = 0;
  const std::uint8_t stored = SetMinimapVisible(visibility, visible);
  Field_isMinimapVisible = static_cast<char>(visibility);
  return stored;
}

void RestoreLiveSceneRoomVisits(void* scene, std::size_t size) {
  if (scene == nullptr) {
    VS_LOG_ERROR("field.map", "Missing map scene for room visits");
    return;
  }
  LiveRoomLocationIndex locations;
  const RoomVisitFlags flags = ToVisitFlags(State_roomVisitProgress.roomFlags);
  if (!RestoreSceneRoomVisits({static_cast<std::byte*>(scene), size}, flags,
                              locations)) {
    VS_LOG_ERROR("field.map", "Map scene of {} bytes cannot hold its rooms",
                 size);
  }
}

void BuildLiveMinimapRotation(MATRIX* rotation, int yaw_angle) {
  if (rotation == nullptr) {
    VS_LOG_ERROR("field.map", "Missing minimap rotation");
    return;
  }
  const MinimapRotation built = BuildMinimapRotation(yaw_angle);
  const std::span rows(rotation->m);
  for (std::size_t row = 0; row < kMinimapRotationOrder; ++row) {
    std::ranges::copy(built.at(row), std::span(rows[row]).begin());
  }
}

int LiveMapCompletionPercent() {
  RoomVisitFlags flags = ToVisitFlags(State_roomVisitProgress.roomFlags);
  const std::int32_t percent =
      MapCompletionPercent(flags, ToVisitFlags(Field_countableRoomMask));
  std::ranges::transform(
      flags, std::span(State_roomVisitProgress.roomFlags).begin(),
      [](std::uint32_t word) { return static_cast<int>(word); });
  return percent;
}
}  // namespace VS::Field
