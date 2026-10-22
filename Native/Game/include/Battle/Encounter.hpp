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

struct EncounterRoom {
  int index = 0;
  std::string asset;
  int width = 0;
  int depth = 0;
  EncounterPosition entrance;
  std::string unavailable_reason;
  std::vector<EncounterFloor> floor;
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

struct EncounterSetup {
  int zone = 0;
  int room = 0;
  std::uint32_t seed = 1;
  EncounterPosition player;
  std::vector<EncounterSpawn> enemies;
  EncounterProfile profile = EncounterProfile::kPrologue;
  EncounterStatOverrides stat_overrides;
  EncounterWeapon weapon = EncounterWeapon::kDefault;
};

// Reads the same extracted ZND/MPD definitions consumed by the room loader.
// Does not initialize gameplay or modify content files.
std::expected<std::vector<EncounterZone>, std::string> ReadEncounterCatalog();
std::vector<std::string> ValidateEncounter(const EncounterSetup& setup,
                                           const EncounterZone& zone);

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

struct EncounterState {
  bool ready = false;
  int zone = 0;
  int room = 0;
  std::uint32_t control_mode = 0;
  bool game_over = false;
  bool field_rendering_enabled = false;
  int menu_state = -1;
  int menu_highlighted_row = -1;
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
  bool rewards_available = false;
  std::vector<EncounterActorState> actors;
  std::vector<int> learned_actions;
  std::vector<EncounterItemState> items;
  std::vector<EncounterLootState> pending_loot;
  std::vector<EncounterFloorTriggerState> floor_triggers;
};

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
