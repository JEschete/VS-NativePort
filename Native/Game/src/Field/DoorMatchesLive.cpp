// Hand-maintained (tools/port/owned.csv).
#include "Field/DoorMatchesLive.hpp"

#include <cstddef>
#include <limits>
#include <span>
#include <vector>

#include "Core/BiosRand.h"
#include "Core/Log.hpp"
#include "Field/DoorMatches.hpp"
#include "Legacy/Psx.h"
extern "C" {
#include "Audio/Seq/Channel.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
}

namespace {
constexpr int kDoorQueryRandomByteIndex = 7;
std::vector<VS::Field::DoorTile> tiles;
std::vector<std::size_t> indices;
std::vector<Field_MpdDoorRecord*> matches;
static_assert(
    std::numeric_limits<decltype(Field_roomData.header.doorsLength)>::max() /
        sizeof(Field_MpdDoorRecord) <=
    std::numeric_limits<int>::max());
}  // namespace

// D_800F1D08, BATTLE/BATTLE.PRG: owns no PS1-sized inline pointer tail.
Field_DoorMatchList Field_matchingDoorRecords{};

namespace VS::Field {
// decomp: BATTLE.PRG func_8008BAC8 (src/BATTLE/BATTLE.PRG/146C.c:16185).
Field_DoorMatchList* TransitionLiveDoorsAtTile(int x, int z, int layer) {
  matches.clear();
  Field_matchingDoorRecords = {};
  if (Field_roomData.doors == nullptr) return nullptr;
  const auto bytes = Field_roomData.header.doorsLength;
  if (bytes % sizeof(Field_MpdDoorRecord) != 0) {
    VS_LOG_ERROR("field.doors", "invalid transition record bytes={}", bytes);
    return nullptr;
  }
  Audio_Seq_unusedStateFlags.opaqueAC[kDoorQueryRandomByteIndex] =
      static_cast<char>(Core_Random_Rand());
  const std::span records(Field_roomData.doors,
                          bytes / sizeof(Field_MpdDoorRecord));
  tiles.clear();
  tiles.reserve(records.size());
  for (const Field_MpdDoorRecord& door : records) {
    const auto& tile = door.tileLocation;
    tiles.push_back({.x = static_cast<std::uint8_t>(tile.tileX),
                     .z = static_cast<std::uint8_t>(tile.tileZ),
                     .layer = static_cast<std::uint8_t>(tile.floorLayer)});
  }
  TransitionDoorsAtTile(tiles, x, z, layer, indices);
  matches.reserve(indices.size());
  for (const std::size_t index : indices) matches.push_back(&records[index]);
  Field_matchingDoorRecords.count = static_cast<int>(matches.size());
  Field_matchingDoorRecords.records = matches.data();
  return &Field_matchingDoorRecords;
}
}  // namespace VS::Field
