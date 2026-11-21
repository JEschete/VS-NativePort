#pragma once
// The loaded zone's context: its file tables, file reads and enemy tables, the
// zone music, and the loaded room's id, tile extent, camera bounds, placements
// and scene. RoomLoadingLive.cpp defines it.

#include <cstdint>
#include <type_traits>

#include "Field/RoomLoading.hpp"

namespace VS::Content {
struct DiscFile;
struct DiscQueueSlot;
}  // namespace VS::Content
namespace VS::Field {
struct MpdEnemySpawn;
}

namespace VS::Field {

// decomp: ZoneContext (src/BATTLE/BATTLE.PRG/zone.h:45-73, formerly
// _zoneContext_t); bounds is upstream's ZoneContextBounds
// (src/BATTLE/BATTLE.PRG/mpd.h:166-171).
struct ZoneContext {
  std::int32_t zone_id = 0;
  // Each room's MPD file extent, indexed by room id.
  VS::Content::DiscFile* room_files = nullptr;
  std::int32_t zone_enemy_count = 0;
  ZoneEnemyTemplate* zone_enemies = nullptr;
  // Each zone enemy's ZUD file extent, indexed like zone_enemies.
  VS::Content::DiscFile* zud_files = nullptr;
  // The cached ZUD's index plus one; 0 when none is cached.
  std::int32_t loaded_zud_index_plus_one = 0;
  VS::Content::DiscQueueSlot* zud_disc_request = nullptr;
  void* zud_data = nullptr;
  VS::Content::DiscQueueSlot* zone_disc_request = nullptr;
  void* zone_data = nullptr;
  std::int32_t music_id = 0;
  std::int32_t music_slot = 0;
  std::int32_t music_fade_enabled = 0;
  std::int32_t room_id = 0;
  std::int32_t reserved38 = 0;
  std::int16_t tile_columns = 0;
  std::int16_t tile_rows = 0;
  ZoneBounds bounds;
  // The words the original's D_800F18C8 view clears when a room loads.
  std::int32_t reserved48 = 0;
  std::int32_t reserved4c = 0;
  std::int32_t reserved50 = 0;
  std::int32_t reserved54 = 0;
  VS::Content::DiscQueueSlot* room_disc_request = nullptr;
  void* room_data = nullptr;
  std::uint32_t room_enemy_count = 0;
  VS::Field::MpdEnemySpawn* room_enemies = nullptr;
  std::int32_t scene_id = 0;
};
static_assert(std::is_trivially_copyable_v<ZoneContext>);

}  // namespace VS::Field
