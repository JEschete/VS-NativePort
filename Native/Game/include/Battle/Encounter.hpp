#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace VS::Game {

struct EncounterLimits {
  static constexpr int kEnemies = 8;
  static constexpr int kActorSlots = 16;
  static constexpr int kTiles = 32;
  static constexpr int kElevations = 4;
  static constexpr int kEnemyIdEnd = 127;
  static constexpr int kLastZone = 255;
  static constexpr int kLastExternalEvent = 9999;
  static constexpr int kStatusTimers = 8;
  static constexpr int kPositionAxes = 3;
  static constexpr int kMinPlayerStat = 1;
  static constexpr int kMaxPlayerStat = 999;
};

struct EncounterPosition {
  int x = 0;
  int z = 0;
  int elevation = 0;
  int direction = 0;
};

struct EncounterEnemy {
  int index = 0;
  std::string name;
  int hp = 0;
  int mp = 0;
  bool secondary_model = false;
  std::string unavailable_reason;
  std::string spawn_asset;
  int spawn_index = -1;
};

enum class EncounterFloor { kSupported, kBlocked, kPit, kNoFloor };

struct EncounterDoor {
  int index = 0;
  int destination_zone = 0;
  int destination_room = 0;
  int tile_x = 0;
  int tile_z = 0;
  int layer = 0;
  EncounterPosition destination;
  int required_flag = 0;
  int required_value = 0;
  int door_id = 0;
  bool entry_step = false;
};

struct EncounterRoom {
  int index = 0;
  std::string asset;
  int width = 0;
  int depth = 0;
  EncounterPosition entrance;
  std::string unavailable_reason;
  std::vector<EncounterFloor> floor;
  std::vector<EncounterDoor> doors;
};

struct EncounterZone {
  int id = 0;
  std::string asset;
  std::vector<EncounterRoom> rooms;
  std::vector<EncounterEnemy> enemies;
};

struct EncounterSpawn {
  int enemy = 0;
  EncounterPosition position;
};

enum class EncounterProfile {
  kPrologue,
  kPostPrologue,
  kPostMinotaur,
  kAllItemsA,
  kAllItemsB,
  kAllSpells,
  kAllAbilities,
  kAllBreakArts,
  kCount
};
const char* EncounterProfileId(EncounterProfile profile);
// Lab startup only, after normal New Game initialization and overlay loading.
void InitializeEncounterPlayerProfile(EncounterProfile profile);

enum class EncounterWeapon {
  kDefault,
  kDagger,
  kSword,
  kGreatsword,
  kAxeMace,
  kGreataxe,
  kStaff,
  kHeavyMace,
  kPolearm,
  kCrossbow,
  kBareHands,
  kCount
};
const char* EncounterWeaponId(EncounterWeapon weapon);
// Lab startup only, after New Game inventory initialization, before
// profiles/spawn.
void InitializeEncounterPlayerWeapon(EncounterWeapon weapon);

struct EncounterStatOverrides {
  std::optional<int> max_hp;
  std::optional<int> max_mp;
  std::optional<int> strength;
  std::optional<int> intelligence;
  std::optional<int> agility;
  bool Valid() const;
  bool operator==(const EncounterStatOverrides&) const = default;
};

// Lab startup only, after Ashley's normal actor/equipment initialization.
void InitializeEncounterPlayerStats(const EncounterStatOverrides& overrides);

enum class EncounterRoomPolicy { kIsolatedCombat, kOriginalRoom };

struct EncounterSetup {
  int zone = 0;
  int room = 0;
  std::uint32_t seed = 1;
  EncounterPosition player;
  std::vector<EncounterSpawn> enemies;
  EncounterProfile profile = EncounterProfile::kPrologue;
  EncounterStatOverrides stat_overrides;
  EncounterWeapon weapon = EncounterWeapon::kDefault;
  EncounterRoomPolicy room_policy = EncounterRoomPolicy::kIsolatedCombat;
  std::optional<int> external_event;
};

// Reads the same extracted ZND/MPD definitions consumed by the room loader.
// Does not initialize gameplay or modify content files.
std::expected<std::vector<EncounterZone>, std::string> ReadEncounterCatalog(
    bool strict = false);
std::vector<std::string> ValidateEncounter(const EncounterSetup& setup,
                                           const EncounterZone& zone);

enum class EncounterDoorRequestStatus { kAccepted, kBusy, kBlocked, kInvalid };
struct EncounterDoorRequest {
  EncounterDoorRequestStatus status;
  std::string reason;
};
// Queue a gated loader handoff on the game fiber, without changing progress
// flags. Diagnostic original-room sessions only, not physical traversal proof.
EncounterDoorRequest RequestEncounterDoorTransition(int index);

struct EncounterWeaponRangeState {
  std::array<int, EncounterLimits::kPositionAxes> components_world{};
  int shape = 0;
  int angle_steps = 0;
  int native_extent_world = 0;
};

struct EncounterActorState {
  int slot = 0;
  int hp = 0;
  int mp = 0;
  int max_hp = 0;
  int max_mp = 0;
  int risk = 0;
  int strength = 0;
  int intelligence = 0;
  int agility = 0;
  int total_strength = 0;
  int total_intelligence = 0;
  int total_agility = 0;
  int walk_speed = 0;
  int run_speed = 0;
  std::uint32_t statuses = 0;
  std::array<std::uint8_t, EncounterLimits::kStatusTimers> status_timers{};
  int command = 0;
  int command_state = 0;
  int weapon_drawn = 0;
  int weapon_category = 0;
  int weapon_blade = 0;
  int weapon_grip = 0;
  int weapon_model = 0;
  int shield_model = 0;
  std::uint32_t actor_flags = 0;
  int recovery = 0;
  int x = 0;
  int y = 0;
  int z = 0;
  int spawn_index = 0;
  bool defeated = false;
  bool ai_present = false;
  int ai_target = -1;
  int ai_candidates = 0;
  int ai_engagement_frames = 0;
  int ai_movement_mode = 0;
  int ai_destination_x = 0;
  int ai_destination_z = 0;
  int awareness_marker = 0;
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

struct EncounterItemState {
  int slot = 0;
  int id = 0;
  int count = 0;
};

struct EncounterLootState {
  int source_actor = 0;
  std::vector<EncounterItemState> items;
};

struct EncounterFloorTriggerState {
  int kind = 0;
  int tile_x = 0;
  int tile_z = 0;
  int activation = 0;
  int marker_fade = 0;
};

struct EncounterTargetState {
  int actor = 0;
  int limb = 0;
  std::array<int, EncounterLimits::kPositionAxes> position{};
};

struct EncounterTargetPageState {
  int total = 0;
  int page = 0;
  int pages = 0;
  int first = 0;
  int visible = 0;
  int selected = -1;
  int selection_step = 0;
};

struct EncounterRouletteState {
  int entry_phase = 0;
  int screen_state = 0;
  int timer = 0;
  unsigned animation = 0;
  int reel = 0;
  unsigned selection = 0;
  std::uint32_t bios_state = 0;
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
};

struct EncounterMenuRowState {
  int index = 0;
  std::string text;
  bool disabled = false;
  bool selected = false;
};

struct EncounterMenuState {
  bool available = false;
  bool loading = false;
  int selected_menu = -1;
  int submenu_phase = 0;
  int selected_row = -1;
  int list_phase = 0;
  int assignment_phase = 0;
  int assignment_type = 0;
  int assignment_selected = -1;
  std::vector<EncounterMenuRowState> rows;
};

struct EncounterState {
  bool ready = false;
  int zone = 0;
  int room = 0;
  std::uint32_t control_mode = 0;
  bool game_over = false;
  bool field_rendering_enabled = false;
  int menu_state = -1;
  int menu_highlighted_row = -1;
  std::optional<EncounterRouletteState> roulette;
  int frame_buffer = 0;
  std::uint32_t transition = 0;
  std::uint32_t native_buttons = 0;
  std::uint32_t native_pressed = 0;
  unsigned action_phase = 0;
  int action = 0;
  int source_actor = 0;
  int target_count = 0;
  std::array<int, EncounterLimits::kPositionAxes> area_center{};
  std::vector<EncounterTargetState> target_previews;
  EncounterTargetPageState target_page;
  bool rewards_available = false;
  std::vector<EncounterActorState> actors;
  std::vector<int> learned_actions;
  std::vector<EncounterItemState> items;
  std::vector<EncounterLootState> pending_loot;
  std::vector<EncounterFloorTriggerState> floor_triggers;
  EncounterReactionTimingState reaction_timing;
  std::vector<EncounterActionOption> action_options;
  EncounterMenuState menu;
  std::array<std::uint32_t, 32> status_masks{};
  std::vector<EncounterDoor> doors;
  std::vector<int> door_flag_values;
};

// Shared read-only field inspection. Empty while loading, outside gameplay,
// or after room/session teardown; does not require a Combat Lab setup.
EncounterState InspectGameplay();

enum class EncounterAdjustment {
  kLowerHp,
  kLowerMp,
  kRaiseRisk,
  kPoison,
  kParalysis,
  kNumbness,
  kCurse,
  kSilence,
  kStrengthDown,
  kIntelligenceDown,
  kAgilityDown,
  kEquipmentDown,
  kRestoreMp,
  kCount
};

struct EncounterAdjustmentResult {
  EncounterActorState before;
  EncounterActorState after;
};

const char* EncounterAdjustmentName(EncounterAdjustment adjustment);
// Lab-only preconditions, never item results. Refuses normal game sessions,
// loading, menus, active actions, and dead/missing Ashley.
std::expected<EncounterAdjustmentResult, std::string> AdjustEncounterPlayer(
    EncounterAdjustment adjustment);

// A single production session can be active: existing gameplay owns globals.
// RunEncounter must execute on a Platform::GameFiber, like Boot_RunGame.
std::expected<void, std::string> ConfigureEncounter(const EncounterSetup& setup,
                                                    const EncounterZone& zone);
void RunEncounter();
void RequestEncounterStop();
EncounterState InspectEncounter();

}  // namespace VS::Game
