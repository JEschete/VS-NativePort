#pragma once
// Hand-maintained (tools/port/owned.csv).
// The zone context and the room load's reset words as the copied battle
// flow, actors, events, menus and saves read and write them.
// RoomLoadingLive.cpp defines them and binds them to the typed zone and room
// loading rules (Field/RoomLoading.hpp).

#include "Field/Zone.h"
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

// decomp: _zoneContext (BATTLE/BATTLE.PRG 146C.h:719, 0x800F1880): the
// loaded zone's tables, music and file reads and the loaded room's id,
// extent, placements and scene.
extern Field_ZoneContext Field_zoneContext;
// decomp: D_800F1908 (BATTLE/BATTLE.PRG 146C.c:15435): a word the room's
// camera set-up clears; no reader is known.
extern int Field_roomResetWord;
// decomp: D_800F1BBC (BATTLE/BATTLE.PRG 146C.c:15743): a byte the room's
// section load clears; no reader is known.
extern char Field_reservedResetByte;

#ifdef __cplusplus
}
#endif
