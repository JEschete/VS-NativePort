#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Field/DoorMatchList.h"
#include "Field/DoorMatches.hpp"

// Binding from the current room's door records to the typed door lookup.
namespace VS::Field {
// Refreshes the door query random byte, then lists the current room's doors at
// the tile in Field_matchingDoorRecords, borrowed until the next query or room
// unload. Returns null with an empty list when the room has no doors or its
// door bytes are not whole records.
Field_DoorMatchList* TransitionLiveDoorsAtTile(int x, int z, int layer);
}  // namespace VS::Field
