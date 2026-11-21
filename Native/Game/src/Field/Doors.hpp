#pragma once
// The room doors' packed tile and flag words, the sigil lock tables and the
// door the player entered by or is heading to. Doors.cpp defines the globals.

#include <array>
#include <cstddef>
#include <cstdint>

namespace VS::Field {

struct MpdDoorRecord;

// decomp: D_800F18E8_t (src/BATTLE/BATTLE.PRG/mainInterface.h:6-9)
struct DoorFlagIdWord {
  // Copied to the saved door's required flag id (main.c:1254).
  std::uint16_t required_flag_id : 10;
  std::uint16_t reserved_bits10 : 6;
};
static_assert(sizeof(DoorFlagIdWord) == 0x2);

// decomp: _mpdRoomDoorSection2, upstream vs_TargetTile
// (src/SLUS_010.40/zoneState.h:31-36)
struct DoorTileLocation {
  std::uint16_t tile_x : 5;
  std::uint16_t tile_z : 5;
  std::uint16_t floor_layer : 3;
  // Upstream arrivalSurface: the collision surface Ashley arrives on.
  std::uint16_t entry_surface : 3;
};
static_assert(sizeof(DoorTileLocation) == 0x2);

inline constexpr std::size_t kLockedDoorRangeCount = 54;
inline constexpr std::size_t kLockedDoorPairCount = 23;
using LockedDoorRanges = std::array<std::int16_t, kLockedDoorRangeCount>;
inline constexpr std::size_t kLockedDoorPairBytes = 4;
using LockedDoorRoomPair = std::array<char, kLockedDoorPairBytes>;
using LockedDoorPairs = std::array<LockedDoorRoomPair, kLockedDoorPairCount>;

// decomp: D_800E849C (src/BATTLE/BATTLE.PRG/146C.c:307)
extern LockedDoorRanges lockedDoorIndexRanges;
// decomp: D_800E84AC (src/BATTLE/BATTLE.PRG/146C.c:308)
extern LockedDoorPairs lockedDoorRoomPairs;
// decomp: D_800F18E8 (src/BATTLE/BATTLE.PRG/mainInterface.h:16)
extern DoorFlagIdWord snapshotDoorFlagSource;
// decomp: D_800F1AB0, upstream vs_battle_spawnPoint
// (src/BATTLE/BATTLE.PRG/mpd.h:255)
extern MpdDoorRecord pendingDestinationDoor;
// decomp: vs_battle_doorEntered (src/BATTLE/BATTLE.PRG/mpd.h:256)
extern std::int16_t enteredDoorId;
// decomp: D_800F1CC8 (src/BATTLE/BATTLE.PRG/146C.h:52). The original also
// reads its tile location halfword as D_800F1CCA
// (src/BATTLE/BATTLE.PRG/146C.c:360, 16396).
extern MpdDoorRecord entranceDoorRecord;

}  // namespace VS::Field
