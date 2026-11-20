#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Field/DoorRecord.hpp"

namespace VS::Field {
struct MpdDoorRecord;
}  // namespace VS::Field

// The MPD door record's packed form (Field/Mpd.hpp VS::Field::MpdDoorRecord)
// and the typed DoorRecord, both ways. Only the room file, the pending and
// entrance door globals and the generated callers that still pass records use
// it.
namespace VS::Field {
[[nodiscard]] DoorRecord DoorRecordOf(const VS::Field::MpdDoorRecord& record);
// Fields wider than the record's bits are masked to them.
void StoreDoorRecord(const DoorRecord& door, VS::Field::MpdDoorRecord& record);
}  // namespace VS::Field
