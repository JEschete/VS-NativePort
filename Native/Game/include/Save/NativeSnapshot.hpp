#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "Legacy/Psx.h"

extern "C" {
#include "Actors/RoomActorCache.h"
#include "Items/Inventory.h"
#include "State/Ashley.h"
#include "State/Progress.h"
#include "State/SceneTimer.h"
#include "State/Settings.h"
}

namespace VS::Game::Save {

struct Location {
  std::uint16_t zone = 0;
  std::uint16_t room = 0;
  std::uint16_t scene = 0;
  std::array<std::int32_t, 3> position{};
  std::int32_t facing = 0;
  std::int32_t camera_yaw = 0;
};

struct RoomCache {
  std::uint8_t recency = 0;
  std::uint8_t zone = 0;
  std::uint16_t map = 0;
  std::uint32_t next_slot = 0;
  std::array<Actors_RoomActorRecord, 8> actors{};
};

struct RandomState {
  std::uint32_t bios_state = 0;
  std::uint32_t interface_state = 0;
  std::array<std::int32_t, 97> shuffle{};
  std::uint32_t shuffle_index = 0;
  std::uint32_t movement_state = 0;
};

struct Snapshot {
  std::array<std::uint8_t, 0x440> script_flags{};
  std::array<bool, 256> learned_actions{};
  State_Settings settings{};
  State_AshleyStatsSnapshot ashley{};
  Items_Inventory inventory{};
  Items_InventoryIndices inventory_order{};
  Items_ContainerData storage{};
  State_RoomVisitFlags visits{};
  State_ArtsStatus arts{};
  State_ScoreData score{};
  std::array<RoomCache, 4> room_cache{};
  std::uint32_t room_cache_slot = 0;
  std::uint64_t play_ticks = 0;
  State_SceneTimer scene_timer{};
  RandomState random{};
  Location location{};
};

struct Document {
  std::uint64_t generation = 0;
  std::uint64_t saved_unix_ms = 0;
  Snapshot state{};
};

std::expected<std::string, std::string> Encode(const Document& document);
// Schema/integrity checks do not replace content resolution before application.
std::expected<Document, std::string> Decode(std::string_view data);
std::expected<void, std::string> Validate(std::string_view data);

}  // namespace VS::Game::Save
