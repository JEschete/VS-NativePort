// Hand-maintained (tools/port/owned.csv).
#include "Field/Map.hpp"  // NOLINT(build/include): typed API

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Field {
namespace {
constexpr std::size_t kSceneCountBytes = 4;
constexpr std::size_t kSceneRoomBytes = 12;
constexpr std::size_t kRoomZoneOffset = 8;
constexpr std::size_t kRoomMapOffset = 10;
constexpr std::int32_t kVisitWordShift = 5;
constexpr std::uint32_t kVisitBitMask = 0x1F;
constexpr std::int32_t kMinimapTiltCosine = 2633;
constexpr std::int32_t kMinimapTiltSine = 3138;
constexpr std::int32_t kQ12One = 4096;
constexpr std::size_t kMarkedWord = 1;
constexpr std::uint32_t kMarkingVisit = 0x800000;
constexpr std::uint32_t kMarkedVisit = 0x400000;
constexpr std::int32_t kPercent = 100;
constexpr std::size_t kBitsPerWord = 32;

static_assert(kRoomVisitFlagWords * kBitsPerWord ==
              static_cast<std::size_t>(kRoomLocationCount));

template <typename T>
T ReadScalar(std::span<const std::byte> bytes, std::size_t offset) {
  T value{};
  std::memcpy(&value, bytes.subspan(offset, sizeof(T)).data(), sizeof(T));
  return value;
}

std::int16_t Q12(std::int32_t value) {
  return static_cast<std::int16_t>(value / kQ12One);
}
}  // namespace

// decomp: BATTLE.PRG func_8008B4C8 (src/BATTLE/BATTLE.PRG/146C.c:16012)
std::uint8_t SetMinimapVisible(std::uint8_t& visibility, std::int32_t visible) {
  visibility = static_cast<std::uint8_t>(visible);
  return visibility;
}

// decomp: BATTLE.PRG vs_battle_setRoomsUnk0
// (src/BATTLE/BATTLE.PRG/146C.c:16014)
bool RestoreSceneRoomVisits(std::span<std::byte> scene,
                            const RoomVisitFlags& room_flags,
                            RoomLocationIndex& locations) {
  if (scene.size() < kSceneCountBytes) return false;
  const auto room_count = ReadScalar<std::int32_t>(scene, 0);
  if (room_count < 0 ||
      static_cast<std::size_t>(room_count) >
          (scene.size() - kSceneCountBytes) / kSceneRoomBytes) {
    return false;
  }
  for (std::size_t room = 0; room < static_cast<std::size_t>(room_count);
       ++room) {
    const auto record = scene.subspan(
        kSceneCountBytes + (room * kSceneRoomBytes), kSceneRoomBytes);
    const std::int32_t zone_id =
        ReadScalar<std::int16_t>(record, kRoomZoneOffset);
    const std::int32_t map_id =
        ReadScalar<std::int16_t>(record, kRoomMapOffset);
    const std::int32_t location = locations.LocationId(zone_id, map_id - 1);
    std::uint32_t visited = 0;
    if (location > 0 && location < kRoomLocationCount) {
      const auto word = static_cast<std::size_t>(location >> kVisitWordShift);
      const auto bit = static_cast<std::uint32_t>(location) & kVisitBitMask;
      visited = room_flags.at(word) & (std::uint32_t{1} << bit);
    }
    std::memcpy(record.data(), &visited, sizeof(visited));
  }
  return true;
}

// decomp: BATTLE.PRG func_8008B590 (src/BATTLE/BATTLE.PRG/146C.c:16033)
MinimapRotation BuildMinimapRotation(std::int32_t yaw_angle) {
  const std::int32_t sine = Core::Math::Sine(yaw_angle);
  const std::int32_t cosine = Core::Math::Cosine(yaw_angle);
  const std::int32_t negative_sine = -sine;
  MinimapRotation rotation{};
  rotation[0] = {static_cast<std::int16_t>(cosine), 0,
                 static_cast<std::int16_t>(sine)};
  rotation[1] = {Q12(negative_sine * -kMinimapTiltSine),
                 static_cast<std::int16_t>(kMinimapTiltCosine),
                 Q12(-(cosine * kMinimapTiltSine))};
  rotation[2] = {Q12(negative_sine * kMinimapTiltCosine),
                 static_cast<std::int16_t>(kMinimapTiltSine),
                 Q12(cosine * kMinimapTiltCosine)};
  return rotation;
}

// decomp: BATTLE.PRG vs_battle_getMapCompletion
// (src/BATTLE/BATTLE.PRG/146C.c:17367)
std::int32_t MapCompletionPercent(RoomVisitFlags& room_flags,
                                  const RoomVisitFlags& countable_rooms) {
  if ((room_flags[kMarkedWord] & kMarkingVisit) != 0) {
    room_flags[kMarkedWord] |= kMarkedVisit;
  }
  std::int32_t visited = 0;
  for (std::size_t word = 0; word < kRoomVisitFlagWords; ++word) {
    visited += std::popcount(room_flags[word] & countable_rooms[word]);
  }
  return (visited * kPercent) / kCountableRoomTotal;
}
}  // namespace VS::Field
