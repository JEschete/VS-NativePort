#pragma once
// Hand-maintained (tools/port/owned.csv).
// The zone context as the copied battle flow, actors, events, menus and saves
// read and write it. RoomLoadingLive.cpp defines it and binds it to the typed
// zone and room loading rules (Field/RoomLoading.hpp).

#include "Field/Zone.hpp"

namespace VS::Field {

// decomp: zoneContext (src/BATTLE/BATTLE.PRG/zone.h:75, 0x800F1880): the
// loaded zone's tables, music and file reads and the loaded room's id,
// extent, placements and scene.
extern ZoneContext zoneContext;

}  // namespace VS::Field
