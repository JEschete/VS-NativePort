#pragma once
// The loaded MPD room record, its section lengths and the room tint. Room.cpp
// defines the room primitive range globals.

#include <cstddef>
#include <cstdint>

#include "Items/ItemRecords.hpp"

namespace VS::Field {

struct CameraBounds;
struct MapScene;
struct MpdCollisionHeader;
struct MpdDoorRecord;
struct MpdEnvironment;
struct MpdFloorTrigger;
struct MpdLockTrigger;
struct MpdMovingPlatform;
struct MpdSectionE;
struct MpdTileCell;
struct MpdViewTrigger;

// decomp: D_800F1BB0_t (src/BATTLE/BATTLE.PRG/2EA3C.h:4-11)
struct RoomTintRecord {
  std::int8_t red;
  std::int8_t green;
  std::int8_t blue;
  char reserved03;
  std::int16_t strength;
  std::int16_t is_active;
};
inline constexpr std::size_t kRoomTintRecordBytes = 0x8;
static_assert(sizeof(RoomTintRecord) == kRoomTintRecordBytes);

// decomp: MpdRoomSection::header (src/BATTLE/BATTLE.PRG/mpd.h:177-202). The
// byte length of each room section, in the order of RoomDataRecord's pointers.
struct MpdSectionLengths {
  std::uint32_t geometry_length;
  std::uint32_t collision_map_length;
  std::uint32_t tile_cells_length;
  std::uint32_t doors_length;
  std::uint32_t lighting_length;
  std::uint32_t ai_navigation_length;
  std::uint32_t section7_length;
  std::uint32_t section8_length;
  std::uint32_t floor_triggers_length;
  std::uint32_t lock_triggers_length;
  std::uint32_t environment_length;
  std::uint32_t texture_effects_length;
  std::uint32_t effect_scripts_length;
  std::uint32_t room_code_length;
  std::uint32_t minimap_scene_length;
  std::uint32_t tiled_background_length;
  std::uint32_t view_triggers_length;
  std::uint32_t moving_platforms_length;
  std::uint32_t containers_length;
  std::uint32_t sound_data_length;
  std::uint32_t section15_length;
  std::uint32_t section16_length;
  std::uint32_t section17_length;
  std::uint32_t zone_bounds_length;
};
inline constexpr std::size_t kMpdSectionLengthsBytes = 0x60;
static_assert(sizeof(MpdSectionLengths) == kMpdSectionLengthsBytes);

// decomp: _mpdRoomSection, upstream MpdRoomSection
// (src/BATTLE/BATTLE.PRG/mpd.h:173-227). The section pointers address native
// memory, so only the header keeps its PS1 layout.
struct RoomDataRecord {
  char is_loaded;
  char is_displayed;
  std::int16_t room_index;
  MpdSectionLengths header;
  std::int32_t* geometry;
  MpdCollisionHeader* collision_map;
  MpdTileCell* tile_cells;
  MpdDoorRecord* doors;
  void* lighting;
  void* ai_navigation;
  void* section7;
  void* section8;
  MpdFloorTrigger* floor_triggers;
  MpdLockTrigger* lock_triggers;
  MpdEnvironment* environment;
  void* texture_effects;
  void* effect_scripts;
  MpdSectionE* room_code;
  MapScene* minimap_scene;
  std::uint16_t* tiled_background;
  MpdViewTrigger* view_triggers;
  MpdMovingPlatform* moving_platforms;
  Items::TreasureChest* containers;
  std::uint32_t* sound_data;
  void* section15;
  void* section16;
  void* section17;
  CameraBounds* zone_bounds;
};

}  // namespace VS::Field
