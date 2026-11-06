#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>

// A room door as a value: the room it leads to, the tile it stands on and
// where the player enters the destination. It carries the twelve bytes of an
// MPD door record (main.h:446 _mpdRoomDoorSection_t) without their packing,
// so transitions, scripts and mods build doors without a PS1 layout.
namespace VS::Field {
// The widths of the packed fields: values are kept within them, as the
// record's bitfields would keep them.
inline constexpr std::uint8_t kDoorTileMask = 0x1F;
inline constexpr std::uint8_t kDoorFloorLayerMask = 0x07;
inline constexpr std::uint8_t kDoorEntryDirectionMask = 0x07;
inline constexpr std::uint8_t kDoorEntryElevationMask = 0x03;
inline constexpr std::uint16_t kDoorRequiredFlagIdMask = 0x3FF;
// The record's three trailing bytes, kept so a door survives a round trip.
inline constexpr std::size_t kDoorReservedBytes = 3;

struct DoorRecord {
  std::uint8_t destination_zone_id = 0;
  std::uint8_t destination_room_id = 0;
  // The door's own tile, floor layer and the facing it gives on entry.
  std::uint8_t tile_x = 0;
  std::uint8_t tile_z = 0;
  std::uint8_t floor_layer = 0;
  std::uint8_t entry_direction = 0;
  // The script flag and value that unlock the door.
  std::uint8_t required_flag_value = 0;
  std::uint16_t required_flag_id = 0;
  // The tile, height step and stairs entry in the destination room.
  std::uint8_t entry_tile_x = 0;
  std::uint8_t entry_tile_z = 0;
  std::uint8_t entry_elevation = 0;
  bool has_entry_step = false;
  // Bit 15 of the record's third halfword; scripts copy their step byte's
  // second bit into it.
  bool reserved_entry_bit = false;
  std::uint8_t door_id = 0;
  std::array<std::uint8_t, kDoorReservedBytes> reserved{};

  friend bool operator==(const DoorRecord&, const DoorRecord&) = default;
};
}  // namespace VS::Field
