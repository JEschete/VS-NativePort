#include "Field/DoorMatches.h"

#include <limits>
#include <span>
#include <vector>

#include "Core/BiosRand.h"
#include "Core/Log.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
}

namespace {
constexpr int kDoorQueryRandomByteIndex = 7;
std::vector<Field_MpdDoorRecord*> matches;
static_assert(
    std::numeric_limits<decltype(Field_roomData.header.doorsLength)>::max() /
        sizeof(Field_MpdDoorRecord) <=
    std::numeric_limits<int>::max());
}  // namespace

// D_800F1D08, BATTLE/BATTLE.PRG: owns no PS1-sized inline pointer tail.
Field_DoorMatchList Field_matchingDoorRecords{};

// func_8008BAC8, BATTLE/BATTLE.PRG/146C.c:16185.
Field_DoorMatchList* Field_TransitionDoorsAtTile(int x, int z, int layer) {
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
  const auto count = bytes / sizeof(Field_MpdDoorRecord);
  matches.reserve(count);
  for (auto& door : std::span(Field_roomData.doors, count)) {
    const auto& tile = door.tileLocation;
    if ((layer >= 2 && tile.floorLayer == layer) ||
        (tile.tileX == x && tile.tileZ == z && tile.floorLayer == layer)) {
      matches.push_back(&door);
    }
  }
  Field_matchingDoorRecords.count = static_cast<int>(matches.size());
  Field_matchingDoorRecords.records = matches.data();
  return &Field_matchingDoorRecords;
}
