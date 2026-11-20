// Hand-maintained (tools/port/owned.csv).
#include "Field/DoorMatchesLive.hpp"

#include <cstddef>
#include <limits>
#include <span>
#include <vector>

#include "Core/Log.hpp"
#include "Core/RandomLive.hpp"
#include "Field/DoorMatches.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Mpd.hpp"
#include "Field/Room.hpp"
#include "Legacy/Psx.hpp"
#include "State/StateFlags.hpp"

namespace {
constexpr int kDoorQueryRandomByteIndex = 7;
std::vector<VS::Field::DoorTile> tiles;
std::vector<std::size_t> indices;
std::vector<VS::Field::MpdDoorRecord*> matches;
static_assert(std::numeric_limits<
                  decltype(VS::Field::roomData.header.doors_length)>::max() /
                  sizeof(VS::Field::MpdDoorRecord) <=
              std::numeric_limits<int>::max());
}  // namespace

// D_800F1D08, BATTLE/BATTLE.PRG: owns no PS1-sized inline pointer tail.
VS::Field::DoorMatchList VS::Field::matchingDoorRecords{};

namespace VS::Field {
// decomp: BATTLE.PRG func_8008BAC8 (src/BATTLE/BATTLE.PRG/146C.c:16185).
DoorMatchList* TransitionLiveDoorsAtTile(int x, int z, int layer) {
  matches.clear();
  matchingDoorRecords = {};
  if (VS::Field::roomData.doors == nullptr) return nullptr;
  const auto bytes = VS::Field::roomData.header.doors_length;
  if (bytes % sizeof(VS::Field::MpdDoorRecord) != 0) {
    VS::Core::LogError("field.doors", "invalid transition record bytes={}",
                       bytes);
    return nullptr;
  }
  VS::State::stateFlags.opaque_ac[kDoorQueryRandomByteIndex] =
      static_cast<char>(VS::Core::NextBiosRand());
  const std::span records(VS::Field::roomData.doors,
                          bytes / sizeof(VS::Field::MpdDoorRecord));
  tiles.clear();
  tiles.reserve(records.size());
  for (const VS::Field::MpdDoorRecord& door : records) {
    const auto& tile = door.tile_location;
    tiles.push_back({.x = static_cast<std::uint8_t>(tile.tile_x),
                     .z = static_cast<std::uint8_t>(tile.tile_z),
                     .layer = static_cast<std::uint8_t>(tile.floor_layer)});
  }
  TransitionDoorsAtTile(tiles, x, z, layer, indices);
  matches.reserve(indices.size());
  for (const std::size_t index : indices) matches.push_back(&records[index]);
  matchingDoorRecords.count = static_cast<int>(matches.size());
  matchingDoorRecords.records = matches.data();
  return &matchingDoorRecords;
}
}  // namespace VS::Field
