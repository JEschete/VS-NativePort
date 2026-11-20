#pragma once
// Hand-maintained (tools/port/owned.csv).
// The room rendering switches, palettes and tint as the copied menus, battle
// flow, gimmicks and weather read and write them. RoomRenderingLive.cpp
// defines them and mirrors them into the typed RoomRenderState
// (Field/RoomRendering.hpp) around each call.

#include "Field/Room.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Field {

// decomp: D_800F190C (BATTLE/BATTLE.PRG 146C.c:15521): 1 while the field
// (room, actors and effects) draws behind the menus.
extern int isFieldRenderingEnabled;
// decomp: D_800F1BA4 (BATTLE/BATTLE.PRG 146C.c:16010): nonzero while the
// room's animations, effects and weather hold still.
extern std::uint16_t areRoomUpdatesPaused;
// decomp: D_800F1BA8 (BATTLE/BATTLE.PRG 146C.c:15953): the room's 1024
// palette colours (low heap, 18.c:595), or null before a session starts.
extern std::uint16_t* roomPalettes;
// decomp: D_800F1BB0 (BATTLE/BATTLE.PRG 146C.c:15995): the room tint's
// channel offsets, strength and on switch.
extern VS::Field::RoomTintRecord roomTint;

}  // namespace VS::Field
