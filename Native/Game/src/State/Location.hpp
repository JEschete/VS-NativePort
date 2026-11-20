#pragma once
// The door Ashley entered the current room through, kept for saves and for
// re-entering the room.
#include "Field/Mpd.hpp"

namespace VS::State {
// decomp: D_80061068 (D_80061068@SLUS_010.40)
extern VS::Field::MpdDoorRecord savedEntryDoor;
}  // namespace VS::State
