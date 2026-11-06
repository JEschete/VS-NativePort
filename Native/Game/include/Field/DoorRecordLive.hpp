#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Field/DoorRecord.hpp"

struct Field_MpdDoorRecord;

// The MPD door record's packed form (Field/Mpd.h Field_MpdDoorRecord) and the
// typed DoorRecord, both ways. Only the room file, the pending and entrance
// door globals and the generated callers that still pass records use it.
namespace VS::Field {
[[nodiscard]] DoorRecord DoorRecordOf(const Field_MpdDoorRecord& record);
// Fields wider than the record's bits are masked to them.
void StoreDoorRecord(const DoorRecord& door, Field_MpdDoorRecord& record);
}  // namespace VS::Field
