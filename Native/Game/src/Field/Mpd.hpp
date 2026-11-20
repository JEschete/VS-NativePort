#pragma once
// The MPD room file's on-disc records: the file header, the room sections
// (collision, tiles, doors, triggers, environment, moving platforms), the
// polygon records and their colour views, and the enemy and object spawns.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Core/Gte.hpp"
#include "Field/Doors.hpp"

namespace VS::Field {

// decomp: _mpdEnemy, upstream MpdEnemy (src/BATTLE/BATTLE.PRG/zone.h:15-43)
inline constexpr std::size_t kMpdEnemySettingsBytes = 8;
struct MpdEnemySpawn {
  std::uint8_t kind;
  std::uint8_t index;
  std::uint8_t is_hidden;
  std::uint8_t reserved03;
  std::uint8_t enemy_id;
  std::uint8_t slot;
  std::uint8_t flag_test;
  std::uint8_t stat_test;
  std::uint16_t flag_value;
  std::uint16_t stat_value;
  std::uint8_t tile_x;
  std::uint8_t direction;
  std::uint8_t tile_z;
  std::uint8_t height;
  std::array<std::uint8_t, kMpdEnemySettingsBytes> settings;
  std::array<std::uint8_t, 2> reserved18;
  std::uint16_t item_id;
  std::uint16_t item2_id;
  std::uint16_t item3_id;
  std::uint8_t item_count;
  std::uint8_t item2_count;
  std::uint8_t item3_count;
  std::uint8_t facing;
  std::uint8_t actor_state_byte;
  std::uint8_t variant;
  std::uint8_t reserved26;
  std::uint8_t scene_test;
};
inline constexpr std::size_t kMpdEnemySpawnBytes = 0x28;
static_assert(sizeof(MpdEnemySpawn) == kMpdEnemySpawnBytes);

// decomp: _mpdHeader, upstream MpdHeader (src/BATTLE/BATTLE.PRG/146C.c:183-196)
struct MpdHeader {
  std::int32_t room_section_offset;
  std::uint16_t room_section_size;
  std::int32_t cleared_section_offset;
  std::uint16_t cleared_section_size;
  std::int32_t script_section_offset;
  std::uint16_t script_section_size;
  std::int32_t door_section_offset;
  std::uint16_t door_section_size;
  std::int32_t enemy_section_offset;
  std::int32_t enemy_section_size;
  std::int32_t loot_section_offset;
  std::int32_t loot_section_size;
};
inline constexpr std::size_t kMpdHeaderBytes = 0x30;
static_assert(sizeof(MpdHeader) == kMpdHeaderBytes);

// decomp: _mpdPolygt3 (BATTLE.PRG 146C.h:44 when copied; no longer declared
// upstream)
struct MpdTriangle {
  std::int16_t vertex1_x, vertex1_y, vertex1_z;
  char vertex2_offset_x, vertex2_offset_y, vertex2_offset_z;
  char vertex3_offset_x, vertex3_offset_y, vertex3_offset_z;
  char red1, green1, blue1, polygon_type;
  char red2, green2, blue2, u3;
  char red3, green3, blue3, v3;
  char u1, v1;
  std::int16_t texel_mask;
  char u2, v2;
  std::int16_t texture_id;
};
inline constexpr std::size_t kMpdTriangleBytes = 0x20;
static_assert(sizeof(MpdTriangle) == kMpdTriangleBytes);

// decomp: _mpdPolygt4 (BATTLE.PRG 146C.h:59 when copied; no longer declared
// upstream)
struct MpdQuad {
  std::int16_t vertex1_x, vertex1_y, vertex1_z;
  char vertex2_offset_x, vertex2_offset_y, vertex2_offset_z;
  char vertex3_offset_x, vertex3_offset_y, vertex3_offset_z;
  char red1, green1, blue1, polygon_type;
  char red2, green2, blue2, u3;
  char red3, green3, blue3, v3;
  char u1, v1;
  std::int16_t texel_mask;
  char u2, v2;
  std::int16_t texture_id;
  char vertex4_offset_x, vertex4_offset_y, vertex4_offset_z, u4;
  char red4, green4, blue4, v4;
};
inline constexpr std::size_t kMpdQuadBytes = 0x28;
static_assert(sizeof(MpdQuad) == kMpdQuadBytes);

// decomp: _mpdRoomCollisionMapSection, upstream MpdRoomCollisionMapSection
// (src/BATTLE/BATTLE.PRG/mpd.h:7-12)
struct MpdCollisionHeader {
  std::int16_t width;
  std::int16_t height;
  std::int16_t height_origin;
  std::int16_t cell_count;
};
inline constexpr std::size_t kMpdCollisionHeaderBytes = 0x8;
static_assert(sizeof(MpdCollisionHeader) == kMpdCollisionHeaderBytes);

// decomp: _mpdRoomDoorSection_t, upstream vs_Warp
// (src/SLUS_010.40/zoneState.h:38-51)
struct MpdDoorRecord {
  std::uint8_t destination_zone_id;
  std::uint8_t destination_room_id;
  DoorTileLocation tile_location;
  std::uint16_t required_flag_value : 8;
  std::uint16_t entry_tile_x : 5;
  std::uint16_t entry_elevation : 2;
  std::uint16_t has_entry_step : 1;
  std::uint16_t required_flag_id : 10;
  std::uint16_t entry_tile_z : 5;
  std::uint16_t reserved06_bit15 : 1;
  std::uint8_t door_id;
  std::uint8_t reserved09;
  std::uint8_t reserved0a;
  std::uint8_t reserved0b;
};
inline constexpr std::size_t kMpdDoorRecordBytes = 0xC;
static_assert(sizeof(MpdDoorRecord) == kMpdDoorRecordBytes);

// decomp: _mpdRoomSection11, upstream MpdRoomSection11
// (src/BATTLE/BATTLE.PRG/mpd.h:116-125). The effect id bytes follow the
// record.
struct MpdViewTrigger {
  std::uint16_t tile_x : 5;
  std::uint16_t tile_z : 5;
  std::uint16_t floor_layer : 1;
  std::uint16_t view_direction : 3;
  std::uint16_t mode : 1;
  std::uint16_t reserved00_bit15 : 1;
  std::uint16_t effect_count;
};
inline constexpr std::size_t kMpdViewTriggerBytes = 0x4;
static_assert(sizeof(MpdViewTrigger) == kMpdViewTriggerBytes);

// decomp: _mpdRoomSection3, upstream MpdTile
// (src/BATTLE/BATTLE.PRG/mpd.h:23-38). The "never read" bits were checked
// against every native port of the original's readers and all 92,049 cells on
// the disc (#347).
struct MpdTileCell {
  std::uint32_t surface_kind : 5;
  // Ramps and stairs: a rise between two slope cells is not a wall, and
  // stepping onto one slides instead of climbing (3A1A0.c:1731-1747).
  std::uint32_t is_slope : 1;
  std::uint32_t blocks_sphere_terrain : 1;
  // Zero in every disc cell and never read.
  std::uint32_t padding00_bits7 : 3;
  // Upstream MpdTileShape: 0 open, 1-4 one triangular half closed, 5 blocked.
  std::uint32_t passability : 3;
  std::uint32_t has_event_trigger : 1;
  std::uint32_t is_platform_docked : 1;
  // Set in 544 disc cells, but nothing in the original reads it.
  std::uint32_t unused_bit15 : 1;
  std::uint32_t has_room_trigger : 1;
  std::uint32_t is_pit : 1;
  // Bit 0 selects the camera occlusion; bit 1 keeps a blocked cell sampled as
  // floor by the target sphere (146C.c:5086, 58578.c:1014).
  std::uint32_t occlusion_and_floor_flags : 2;
  std::uint32_t forbids_gimmick_placement : 1;
  // Zero in every disc cell and never read.
  std::uint32_t padding00_bits21 : 2;
  // The AI navigation map (func_800E6F9C) copies bits 23-31 into its tile
  // flags: the area (flags & 15), no-walk (0x10), no-flight (0x40) and three
  // terrain classes that actor reject masks test (0x2000, 0x1000, 0x800).
  std::uint32_t ai_area : 4;
  std::uint32_t ai_no_walk : 1;
  std::uint32_t ai_no_flight : 1;
  std::uint32_t ai_terrain_classes : 3;
};
inline constexpr std::size_t kMpdTileCellBytes = 0x4;
static_assert(sizeof(MpdTileCell) == kMpdTileCellBytes);

// decomp: _mpdRoomSection12, upstream MpdCloudstone
// (src/BATTLE/BATTLE.PRG/mpd.h:127-150)
inline constexpr std::size_t kMpdPlatformWaypoints = 4;
inline constexpr std::size_t kMpdPlatformSegments = 6;
struct MpdMovingPlatform {
  std::uint32_t scene_object_id : 8;
  std::uint32_t is_looping : 1;
  std::uint32_t is_hidden : 1;
  std::uint32_t end_waypoint : 2;
  std::uint32_t swaps_colors_when_frozen : 2;
  std::uint32_t start_waypoint : 2;
  std::uint32_t reserved00_bits16 : 7;
  std::uint32_t reserved00_bits23 : 3;
  std::uint32_t is_moving_forward : 1;
  std::uint32_t current_waypoint : 2;
  std::uint32_t current_segment : 3;
  std::uint32_t is_waiting : 1;
  std::uint32_t control_state : 7;
  std::uint32_t terminal_waypoint : 2;
  std::uint32_t reserved04_bits10 : 6;
  std::uint32_t motion_ticks : 16;
  std::array<VS::Core::Gte::VECTOR, kMpdPlatformWaypoints> waypoints;
  std::array<VS::Core::Gte::VECTOR, kMpdPlatformSegments> segment_velocities;
  std::int32_t position_x;
  std::int32_t position_y;
  std::int32_t position_z;
  MpdTileCell tile_cell;
};
inline constexpr std::size_t kMpdMovingPlatformBytes = 0xB8;
static_assert(sizeof(MpdMovingPlatform) == kMpdMovingPlatformBytes);

// decomp: _mpdRoomSection9::unk8.lock (src/BATTLE/BATTLE.PRG/mpd.h:47-50)
struct MpdLockBytes {
  char state;
  char room_effect;
};

// decomp: _mpdRoomSection9::unk8 (src/BATTLE/BATTLE.PRG/mpd.h:45-51)
union MpdLockState {
  std::int16_t switch_value;
  MpdLockBytes lock;
};

// decomp: _mpdRoomSection9, upstream MpdRoomSection9
// (src/BATTLE/BATTLE.PRG/mpd.h:40-54)
struct MpdFloorTrigger {
  std::int16_t tile_x;
  std::int16_t tile_z;
  std::int16_t floor_layer;
  std::int16_t kind;
  MpdLockState lock_state;
  char activation;
  char marker_fade;
};
inline constexpr std::size_t kMpdFloorTriggerBytes = 0xC;
static_assert(sizeof(MpdFloorTrigger) == kMpdFloorTriggerBytes);

// decomp: _mpdRoomSectionA, upstream MpdRoomSectionA
// (src/BATTLE/BATTLE.PRG/mpd.h:56-63)
struct MpdLockTrigger {
  MpdFloorTrigger trigger;
  std::int16_t key_or_sigil;
  std::int16_t is_open;
  std::int16_t linked_lock_index;
  char is_triggered;
  char delay_frames;
};
inline constexpr std::size_t kMpdLockTriggerBytes = 0x14;
static_assert(sizeof(MpdLockTrigger) == kMpdLockTriggerBytes);

// decomp: _mpdRoomSectionB::unk48 (src/BATTLE/BATTLE.PRG/mpd.h:85-88)
union PackedShortPair {
  std::array<std::int16_t, 2> halves;
  std::int32_t word;
};

// decomp: _mpdRoomSectionB, upstream MpdRoomSectionB
// (src/BATTLE/BATTLE.PRG/mpd.h:65-93)
inline constexpr std::size_t kMpdBackgroundColors = 5;
struct MpdEnvironment {
  std::array<std::int32_t, kMpdBackgroundColors> background_colors;
  VS::Core::Gte::VECTOR background_rotation;
  std::int32_t reserved24;
  std::int32_t reserved28;
  std::int32_t freeze_effect_id;
  char is_workshop;
  char environment_flags;
  char trap_level;
  char reserved33;
  std::int16_t weather_kind;
  std::int16_t wind_direction;
  std::int16_t wind_direction_limit;
  std::int16_t wind_direction_interval;
  std::int16_t weather_intensity;
  std::int16_t weather_intensity_limit;
  std::int16_t weather_intensity_interval;
  std::int16_t wind_strength;
  std::int16_t wind_strength_limit;
  std::int16_t wind_strength_interval;
  std::array<PackedShortPair, 2> weather_area;
  std::int16_t scene_id;
  std::int16_t fall_destination_zone;
  std::int16_t fall_destination_room;
  std::int16_t opaque56;
};
inline constexpr std::size_t kMpdEnvironmentBytes = 0x58;
static_assert(sizeof(MpdEnvironment) == kMpdEnvironmentBytes);

// decomp: _mpdRoomSectionE, upstream MpdRoomSectionE
// (src/BATTLE/BATTLE.PRG/mpd.h:95-102). The two arrays' payload follows the
// record; first_array and second_array are offsets from its start.
struct MpdSectionE {
  std::int32_t reserved00;
  std::int32_t second_array_offset;
  std::int32_t has_second_array;
  std::uint32_t first_array;
  std::uint32_t second_array;
};
inline constexpr std::size_t kMpdSectionEBytes = 0x14;
static_assert(sizeof(MpdSectionE) == kMpdSectionEBytes);

// decomp: func_80091C6C_t2 (src/BATTLE/BATTLE.PRG/2842C.c:142-158)
inline constexpr std::size_t kMpdUvPositionAndColorBytes = 0xF;
inline constexpr std::size_t kMpdUvColorBytes = 3;
inline constexpr std::size_t kMpdUvTextureAndPositionBytes = 5;
struct MpdPolygonUvView {
  std::array<char, kMpdUvPositionAndColorBytes> position_and_first_color;
  std::uint8_t type;
  std::array<char, kMpdUvColorBytes> second_color;
  std::uint8_t u2;
  std::array<char, kMpdUvColorBytes> third_color;
  std::uint8_t v2;
  std::uint8_t u0;
  std::uint8_t v0;
  std::array<char, 2> texel_mask;
  std::uint8_t u1;
  std::uint8_t v1;
  std::array<char, kMpdUvTextureAndPositionBytes>
      texture_id_and_fourth_position;
  std::uint8_t u3;
  std::array<char, kMpdUvColorBytes> fourth_color;
  std::uint8_t v3;
};
inline constexpr std::size_t kMpdPolygonUvViewBytes = 0x28;
static_assert(sizeof(MpdPolygonUvView) == kMpdPolygonUvViewBytes);

inline constexpr std::size_t kMpdColorViewPositionBytes = 0xC;

// decomp: func_80093914_t (src/BATTLE/BATTLE.PRG/2842C.c:92-105)
inline constexpr std::size_t kMpdTriangleColorRemainderBytes = 0xA;
struct MpdTriangleColorView {
  std::array<std::uint8_t, kMpdColorViewPositionBytes> positions;
  std::uint8_t red1;
  std::uint8_t green1;
  std::uint8_t blue1;
  std::uint8_t type;
  std::uint8_t red2;
  std::uint8_t green2;
  std::uint8_t blue2;
  std::uint8_t u3;
  std::uint8_t red3;
  std::uint8_t green3;
  std::array<std::uint8_t, kMpdTriangleColorRemainderBytes> remainder;
};
static_assert(sizeof(MpdTriangleColorView) == kMpdTriangleBytes);

// decomp: func_80093914_t2 (src/BATTLE/BATTLE.PRG/2842C.c:107-124)
inline constexpr std::size_t kMpdQuadColorMiddleBytes = 0xE;
struct MpdQuadColorView {
  std::array<std::uint8_t, kMpdColorViewPositionBytes> positions;
  std::uint8_t red1;
  std::uint8_t green1;
  std::uint8_t blue1;
  std::uint8_t type;
  std::uint8_t red2;
  std::uint8_t green2;
  std::uint8_t blue2;
  std::uint8_t u3;
  std::uint8_t red3;
  std::uint8_t green3;
  std::array<std::uint8_t, kMpdQuadColorMiddleBytes> middle;
  std::uint8_t red4;
  std::uint8_t green4;
  std::uint8_t blue4;
  std::uint8_t v4;
};
static_assert(sizeof(MpdQuadColorView) == kMpdQuadBytes);

// decomp: mpdObjectSpawn, upstream MpdObjectSpawn
// (src/BATTLE/BATTLE.PRG/146C.c:198-214)
inline constexpr std::size_t kMpdObjectSpawnTailBytes = 0x18;
struct MpdObjectSpawn {
  std::uint8_t kind;
  std::uint8_t reserved01;
  std::uint8_t reserved02;
  std::uint8_t reserved03;
  std::uint16_t flag;
  std::uint8_t flag_min;
  std::uint8_t flag_max;
  std::uint8_t slot;
  std::uint8_t reserved09;
  std::uint8_t tile_x;
  std::uint8_t tile_z;
  std::uint8_t height;
  std::uint8_t direction;
  std::uint16_t model;
  std::array<std::uint8_t, kMpdObjectSpawnTailBytes> reserved10;
};
inline constexpr std::size_t kMpdObjectSpawnBytes = 0x28;
static_assert(sizeof(MpdObjectSpawn) == kMpdObjectSpawnBytes);

}  // namespace VS::Field
