// Hand-maintained (tools/port/owned.csv).
#include "Field/DoorRecordLive.hpp"

#include <cstddef>
#include <cstdint>

extern "C" {
#include "Field/Doors.h"
#include "Field/Mpd.h"
}

namespace {
// main.h:446 _mpdRoomDoorSection_t: twelve bytes, the door tile halfword at
// byte 2, the two flag and entry halfwords at bytes 4 and 6 and the door id
// at byte 8.
constexpr std::size_t kDoorRecordBytes = 0xC;
constexpr std::size_t kDoorTileOffset = 2;
constexpr std::size_t kDoorIdOffset = 8;
}  // namespace

static_assert(sizeof(Field_MpdDoorRecord) == kDoorRecordBytes);
static_assert(offsetof(Field_MpdDoorRecord, tileLocation) == kDoorTileOffset);
static_assert(offsetof(Field_MpdDoorRecord, doorId) == kDoorIdOffset);

namespace VS::Field {
DoorRecord DoorRecordOf(const Field_MpdDoorRecord& record) {
  DoorRecord door;
  door.destination_zone_id = record.destinationZoneId;
  door.destination_room_id = record.destinationRoomId;
  door.tile_x = static_cast<std::uint8_t>(record.tileLocation.tileX);
  door.tile_z = static_cast<std::uint8_t>(record.tileLocation.tileZ);
  door.floor_layer = static_cast<std::uint8_t>(record.tileLocation.floorLayer);
  door.entry_direction =
      static_cast<std::uint8_t>(record.tileLocation.entryDirection);
  door.required_flag_value =
      static_cast<std::uint8_t>(record.requiredFlagValue);
  door.entry_tile_x = static_cast<std::uint8_t>(record.entryTileX);
  door.entry_elevation = static_cast<std::uint8_t>(record.entryElevation);
  door.has_entry_step = record.hasEntryStep != 0;
  door.required_flag_id = static_cast<std::uint16_t>(record.requiredFlagId);
  door.entry_tile_z = static_cast<std::uint8_t>(record.entryTileZ);
  door.reserved_entry_bit = record.reserved06Bit15 != 0;
  door.door_id = record.doorId;
  door.reserved = {record.reserved09, record.reserved0A, record.reserved0B};
  return door;
}

void StoreDoorRecord(const DoorRecord& door, Field_MpdDoorRecord& record) {
  record.destinationZoneId = door.destination_zone_id;
  record.destinationRoomId = door.destination_room_id;
  record.tileLocation.tileX = door.tile_x & kDoorTileMask;
  record.tileLocation.tileZ = door.tile_z & kDoorTileMask;
  record.tileLocation.floorLayer = door.floor_layer & kDoorFloorLayerMask;
  record.tileLocation.entryDirection =
      door.entry_direction & kDoorEntryDirectionMask;
  record.requiredFlagValue = door.required_flag_value;
  record.entryTileX = door.entry_tile_x & kDoorTileMask;
  record.entryElevation = door.entry_elevation & kDoorEntryElevationMask;
  record.hasEntryStep = door.has_entry_step ? 1U : 0U;
  record.requiredFlagId = door.required_flag_id & kDoorRequiredFlagIdMask;
  record.entryTileZ = door.entry_tile_z & kDoorTileMask;
  record.reserved06Bit15 = door.reserved_entry_bit ? 1U : 0U;
  record.doorId = door.door_id;
  record.reserved09 = door.reserved[0];
  record.reserved0A = door.reserved[1];
  record.reserved0B = door.reserved[2];
}
}  // namespace VS::Field
