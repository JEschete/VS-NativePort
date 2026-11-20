// Hand-maintained (tools/port/owned.csv).
#include "Field/DoorRecordLive.hpp"

#include <cstddef>
#include <cstdint>

#include "Field/Mpd.hpp"

namespace {
// main.h:446 _mpdRoomDoorSection_t: twelve bytes, the door tile halfword at
// byte 2, the two flag and entry halfwords at bytes 4 and 6 and the door id
// at byte 8.
constexpr std::size_t kDoorRecordBytes = 0xC;
constexpr std::size_t kDoorTileOffset = 2;
constexpr std::size_t kDoorIdOffset = 8;
}  // namespace

static_assert(sizeof(VS::Field::MpdDoorRecord) == kDoorRecordBytes);
static_assert(offsetof(VS::Field::MpdDoorRecord, tile_location) ==
              kDoorTileOffset);
static_assert(offsetof(VS::Field::MpdDoorRecord, door_id) == kDoorIdOffset);

namespace VS::Field {
DoorRecord DoorRecordOf(const VS::Field::MpdDoorRecord& record) {
  DoorRecord door;
  door.destination_zone_id = record.destination_zone_id;
  door.destination_room_id = record.destination_room_id;
  door.tile_x = static_cast<std::uint8_t>(record.tile_location.tile_x);
  door.tile_z = static_cast<std::uint8_t>(record.tile_location.tile_z);
  door.floor_layer =
      static_cast<std::uint8_t>(record.tile_location.floor_layer);
  door.entry_surface =
      static_cast<std::uint8_t>(record.tile_location.entry_surface);
  door.required_flag_value =
      static_cast<std::uint8_t>(record.required_flag_value);
  door.entry_tile_x = static_cast<std::uint8_t>(record.entry_tile_x);
  door.entry_facing = static_cast<std::uint8_t>(record.entry_facing);
  door.has_entry_step = record.has_entry_step != 0;
  door.required_flag_id = static_cast<std::uint16_t>(record.required_flag_id);
  door.entry_tile_z = static_cast<std::uint8_t>(record.entry_tile_z);
  door.walks_in = record.walks_in != 0;
  door.door_id = record.door_id;
  door.reserved = {record.reserved09, record.reserved0a, record.reserved0b};
  return door;
}

void StoreDoorRecord(const DoorRecord& door, VS::Field::MpdDoorRecord& record) {
  record.destination_zone_id = door.destination_zone_id;
  record.destination_room_id = door.destination_room_id;
  record.tile_location.tile_x = door.tile_x & kDoorTileMask;
  record.tile_location.tile_z = door.tile_z & kDoorTileMask;
  record.tile_location.floor_layer = door.floor_layer & kDoorFloorLayerMask;
  record.tile_location.entry_surface =
      door.entry_surface & kDoorEntrySurfaceMask;
  record.required_flag_value = door.required_flag_value;
  record.entry_tile_x = door.entry_tile_x & kDoorTileMask;
  record.entry_facing = door.entry_facing & kDoorEntryFacingMask;
  record.has_entry_step = door.has_entry_step ? 1U : 0U;
  record.required_flag_id = door.required_flag_id & kDoorRequiredFlagIdMask;
  record.entry_tile_z = door.entry_tile_z & kDoorTileMask;
  record.walks_in = door.walks_in ? 1U : 0U;
  record.door_id = door.door_id;
  record.reserved09 = door.reserved[0];
  record.reserved0a = door.reserved[1];
  record.reserved0b = door.reserved[2];
}
}  // namespace VS::Field
