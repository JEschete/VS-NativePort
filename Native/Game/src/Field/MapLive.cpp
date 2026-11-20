// Hand-maintained (tools/port/owned.csv).
#include "Field/MapLive.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Core/Log.hpp"
#include "Field/Map.hpp"
#include "Field/MapProgress.hpp"
#include "Field/Minimap.hpp"
#include "Field/RoomStateLive.hpp"
#include "Legacy/Psx.hpp"
#include "State/Progress.hpp"

using VS::Core::Gte::MATRIX;

namespace {
using VS::Field::RoomVisitFlags;

static_assert(sizeof(VS::State::roomVisitProgress.room_flags) ==
              sizeof(RoomVisitFlags));

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
  return SetMinimapVisible(isMinimapVisible, visible);
}

void RestoreLiveSceneRoomVisits(void* scene, std::size_t size) {
  if (scene == nullptr) {
    VS::Core::LogError("field.map", "Missing map scene for room visits");
    return;
  }
  LiveRoomLocationIndex locations;
  const RoomVisitFlags flags =
      ToVisitFlags(VS::State::roomVisitProgress.room_flags);
  if (!RestoreSceneRoomVisits({static_cast<std::byte*>(scene), size}, flags,
                              locations)) {
    VS::Core::LogError("field.map",
                       "Map scene of {} bytes cannot hold its rooms", size);
  }
}

void BuildLiveMinimapRotation(MATRIX* rotation, int yaw_angle) {
  if (rotation == nullptr) {
    VS::Core::LogError("field.map", "Missing minimap rotation");
    return;
  }
  const MinimapRotation built = BuildMinimapRotation(yaw_angle);
  const std::span rows(rotation->m);
  for (std::size_t row = 0; row < kMinimapRotationOrder; ++row) {
    std::ranges::copy(built.at(row), std::span(rows[row]).begin());
  }
}

int LiveMapCompletionPercent() {
  RoomVisitFlags flags = ToVisitFlags(VS::State::roomVisitProgress.room_flags);
  const std::int32_t percent = MapCompletionPercent(flags, countableRoomMask);
  std::ranges::transform(
      flags, std::span(VS::State::roomVisitProgress.room_flags).begin(),
      [](std::uint32_t word) { return static_cast<int>(word); });
  return percent;
}
}  // namespace VS::Field
