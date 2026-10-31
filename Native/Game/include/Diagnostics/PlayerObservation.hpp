#pragma once

#include <string>
#include <vector>

#include "Battle/Encounter.hpp"
#include "nlohmann/json.hpp"

namespace VS::Game {

inline constexpr int kPlayerObservationSchema = 2;

struct EncounterWeaponRangeState {
  std::array<int, EncounterLimits::kPositionAxes> components_world{};
  int shape = 0;
  int angle_steps = 0;
  int native_extent_world = 0;
};

struct PlayerActorObservation : EncounterActorState {
  int model = -1;
  EncounterWeaponRangeState weapon_range;
  int relation_to_player = -1;
  bool relation_active = false;
  bool player_visible = false;
  bool hostile_to_player = false;
  std::uint32_t status_immunities = 0;
  std::vector<int> action_ids;
  std::vector<int> spell_action_ids;
  int base_attack_type = 0;
  int shield_pp = 0;
  int weapon_pp = 0;
};

struct EncounterReactionTimingState {
  static constexpr std::size_t kSlots = 3;
  bool active = false;
  std::string eligibility_status = "inactive";
  int source_actor = -1;
  int target_actor = -1;
  int action = -1;
  std::uint32_t queue_index = 0;
  std::uint32_t chain_step = 0;
  int timer = 0;
  int window = 0;
  int tick_speed = 0;
  bool story_timing = false;
  int previous_slot = 0;
  std::uint32_t previous_buttons = 0;
  bool attempted = false;
  int succeeded = 0;
  std::array<int, kSlots> chain_actions{};
  std::array<int, kSlots> defense_actions{};
  std::array<bool, kSlots> eligible{};
  int incoming_type = 0;
  int incoming_affinity = -1;
  int incoming_cost = 0;
  int incoming_hp_damage = 0;
  int incoming_mp_damage = 0;
  std::uint32_t incoming_statuses = 0;
};

struct EncounterActionOption {
  int id = 0;
  std::string name;
  std::string family;
  int menu_order = 0;
  int type = 0;
  int cost = 0;
  int restrictions = 0;
  std::uint32_t ai_flags = 0;
  // Warlock rank selector level (0 = LV1); rank rows reuse the base menu row.
  int rank = 0;
  // Rank rows the selector offers for this spell; 1 when it casts directly.
  int rank_count = 1;
};

struct EncounterMenuRowState {
  int index = 0;
  std::string text;
  bool disabled = false;
  bool selected = false;
  int item_id = 0;
  int item_count = 0;
};

struct EncounterMenuState {
  bool available = false;
  bool event_owned = false;
  bool loading = false;
  int selected_menu = -1;
  int submenu_phase = 0;
  int selected_row = -1;
  int list_phase = 0;
  int assignment_phase = 0;
  int assignment_type = 0;
  int assignment_selected = -1;
  int spell_level = -1;
  int spell_level_count = 0;
  int shortcut = 0;
  bool loot_input_ready = false;
  int loot_count = 0;
  std::vector<EncounterMenuRowState> rows;
};

struct EncounterItemOption {
  int id = 0;
  int action_id = 0;
  int count = 0;
  std::string name;
  int restrictions = 0;
  int effect_id = 0;
  int calculator_id = 0;
  int stat_factor = 0;
};

inline constexpr int kTileWorldUnits = 64;
inline constexpr std::size_t kTileSampleFields = 3;
inline constexpr std::size_t kStatusMaskCount = 32;

struct EncounterNavigationState {
  bool available = false;
  int origin_x = 0;
  int origin_z = 0;
  int width = 0;
  int depth = 0;
  int spacing = kTileWorldUnits;
  int collision_radius = 0;
  int collision_height = 0;
  int movement_mode = 0;
  int locomotion_state = 0;
  int route_phase = 0;
  bool jumping = false;
  std::array<int, kTileSampleFields> facing_tile{};
  // Packed heights; flags: point passable=1, pit=2, footprint clear=4.
  std::vector<std::array<int, kTileSampleFields>> samples;
};

// Original MPD section 13 container (Items_TreasureChest) plus its opened flag.
struct EncounterChestState {
  int index = 0;
  int tile_x = 0;
  int tile_z = 0;
  int layer = 0;
  int facing = 0;
  int lock_level = 0;
  int opened_flag = 0;
  bool opened = false;
  std::vector<EncounterItemState> misc;
};

struct PlayerObservationState : EncounterState {
  std::vector<PlayerActorObservation> player_actors;
  int target_selection_step = 0;
  EncounterReactionTimingState reaction_timing;
  std::vector<EncounterActionOption> action_options;
  EncounterMenuState menu;
  std::array<std::uint32_t, kStatusMaskCount> status_masks{};
  std::vector<EncounterDoor> doors;
  std::vector<int> door_flag_values;
  std::vector<EncounterChestState> chests;
  EncounterNavigationState navigation;
  std::vector<EncounterItemOption> item_options;
};

// Explicit diagnostic query only; ordinary gameplay inspection never calls it.
PlayerObservationState InspectPlayerObservation(const EncounterState& state);
void AppendPlayerObservations(nlohmann::json& data,
                              const EncounterState& state);

}  // namespace VS::Game
