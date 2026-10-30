#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include "Legacy/Psx.h"

extern "C" {
#include "Actors/RoomActorCache.h"
#include "Field/Mpd.h"
#include "Items/Inventory.h"
#include "State/Ashley.h"
#include "State/Progress.h"
#include "State/SceneTimer.h"
#include "State/Settings.h"
}

namespace VS::Game::Save {

inline constexpr std::size_t kPositionAxes = 3;
inline constexpr std::size_t kCachedRoomActors = 8;
inline constexpr std::size_t kRandomShuffleEntries = 97;
inline constexpr std::size_t kScriptFlagBytes = 0x440;
inline constexpr std::size_t kLearnedActionCount = 256;
inline constexpr std::size_t kCachedRooms = 4;
// F03601 TITLE 80069cbc clears 0x100 bytes, not all container consumables.
inline constexpr std::size_t kClearContainerResetStacks = 64;

struct TileAddress {
  std::uint8_t x = 0;
  std::uint8_t z = 0;
  std::uint8_t layer = 0;
};

struct GroundedPlacement {
  TileAddress tile{};
  std::uint8_t facing = 0;
};

struct Location {
  std::uint16_t zone = 0;
  std::uint16_t room = 0;
  std::uint16_t scene = 0;
  std::array<std::int32_t, kPositionAxes> position{};
  std::int32_t facing = 0;
  std::int32_t camera_yaw = 0;
  std::uint8_t floor_layer = 0;
  TileAddress previous_tile{};
  std::int16_t previous_facing = 0;
  GroundedPlacement saved_placement{};
};

struct OrbitCamera {
  // Current yaw is Location::camera_yaw; preferred yaw/initial distance retain
  // their original Ashley cameraPitch/cameraDistance fields.
  std::int32_t pitch = 0;
  std::int32_t distance = 0;
  std::int32_t mode = 0;
  std::int32_t rotation_step = 0;
  std::int32_t zoom_mode = 0;
  std::int32_t zoom_velocity = 0;
  std::int32_t tracking_speed = 0;
  std::array<std::int32_t, kPositionAxes> look_at{};
  std::array<std::int32_t, kPositionAxes> view_angles{};
};

struct RoomCache {
  std::uint8_t recency = 0;
  std::uint8_t zone = 0;
  std::uint16_t map = 0;
  std::uint32_t next_slot = 0;
  std::array<Actors_RoomActorRecord, kCachedRoomActors> actors{};
};

struct RandomState {
  std::uint32_t bios_state = 0;
  std::uint32_t interface_state = 0;
  std::array<std::int32_t, kRandomShuffleEntries> shuffle{};
  std::uint32_t shuffle_index = 0;
  std::uint32_t movement_state = 0;
};

struct Snapshot {
  std::array<std::uint8_t, kScriptFlagBytes> script_flags{};
  std::array<bool, kLearnedActionCount> learned_actions{};
  State_Settings settings{};
  State_AshleyStatsSnapshot ashley{};
  Items_Inventory inventory{};
  Items_InventoryIndices inventory_order{};
  Items_ContainerData storage{};
  State_RoomVisitFlags visits{};
  State_ArtsStatus arts{};
  State_ScoreData score{};
  std::array<RoomCache, kCachedRooms> room_cache{};
  std::uint32_t room_cache_slot = 0;
  std::uint64_t play_ticks = 0;
  State_SceneTimer scene_timer{};
  RandomState random{};
  Location location{};
  OrbitCamera camera{};
};

struct Document {
  std::uint64_t generation = 0;
  std::uint64_t saved_unix_ms = 0;
  Snapshot state{};
  // TITLE clear saves restart through the original door/spawn contract, not
  // a captured field position. Schema3 checkpoints have no such entry.
  std::optional<Field_MpdDoorRecord> new_game_entry{};
};

std::expected<std::string, std::string> Encode(const Document& document);
// Schema/integrity checks do not replace content resolution before application.
std::expected<Document, std::string> Decode(std::string_view data);
std::expected<void, std::string> Validate(std::string_view data);
// Compare wire-visible fields of validated documents, not native padding.
// This does not replace schema/integrity or content validation before loading.
std::expected<bool, std::string> SameDocument(const Document& before,
                                              const Document& current);

}  // namespace VS::Game::Save
