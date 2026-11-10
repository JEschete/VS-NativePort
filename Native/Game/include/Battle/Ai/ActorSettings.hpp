#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

// How a battle actor's packed AI settings word is assembled and decoded into
// the fields of its new AI state. These policies only transform settings
// values and the module tables they index, so they reach no actor, AI state
// table or PS1 address.
namespace VS::Battle::Ai {
// Settings bits 21-31 (relation category and above) that the actor's saved
// settings flags can replace; bits 0-20 always keep the enemy template's value.
inline constexpr std::uint32_t kSavedAiSettingsMask = 0xFFE00000u;

// Replaces settings bits 21-31 with bits 1-11 of the actor's saved settings
// flags (func_800E5A74). Bit 0, which the caller tests to enable the override,
// and bits 12-15 are ignored.
[[nodiscard]] std::uint32_t ApplySavedAiSettingsFlags(
    std::uint32_t settings, std::uint16_t saved_flags);

// Entries a two-bit settings field selects from (view angle, step height,
// facing delay scale and terrain reject mask tables).
inline constexpr std::size_t kSettingsChoiceCount = 4;
inline constexpr std::size_t kPerceptionRangeCount = 8;
inline constexpr std::size_t kWanderPresetCount = 8;
inline constexpr std::size_t kPatrolRateCount = 2;
// Waypoint halfwords in a room's auxiliary AI section.
inline constexpr std::size_t kRoomWaypointCount = 8;
// The near perception range is capped at 0x600 units (squared).
inline constexpr std::uint32_t kNearDistanceSquaredCap = 0x240000u;
// Actor sub-types 0xAC and 0xAD run the orbiting boss behaviour.
inline constexpr std::uint16_t kFirstOrbitingBossSubType = 0xAC;
inline constexpr std::uint16_t kOrbitingBossSubTypeCount = 2;

// One wander timing preset (BATTLE.PRG D_80069C3C entry).
struct WanderPreset {
  std::uint16_t move_period = 0;
  std::uint16_t rest_period = 0;
  std::uint8_t rate = 0;
};

// The BATTLE.PRG tables the settings decode indexes; the live binding copies
// them from the loaded module image.
struct ActorAiSettingsTables {
  // D_80069C1C, settings bits 18-20.
  std::array<std::uint32_t, kPerceptionRangeCount> perception_ranges_squared{};
  // D_800F17B8 (first halfword of each pair), settings bits 16-17.
  std::array<std::uint16_t, kSettingsChoiceCount> view_angles{};
  // D_800F17C8 (first halfword of each pair), settings bits 8-9.
  std::array<std::uint16_t, kSettingsChoiceCount> step_heights{};
  // D_800F17D8, settings bits 2-3.
  std::array<std::uint8_t, kSettingsChoiceCount> facing_delay_scales{};
  // D_80069C3C, settings bits 4-6.
  std::array<WanderPreset, kWanderPresetCount> wander_presets{};
  // D_80069C18, settings bits 30-31.
  std::array<std::uint8_t, kSettingsChoiceCount> terrain_reject_masks{};
  // D_80069C08, spawn flags bit 14.
  std::array<std::uint8_t, kPatrolRateCount> patrol_rates{};
};

// What an actor's AI settings are decoded from.
struct ActorAiSettingsSource {
  // The template settings word, with any saved flags already applied.
  std::uint32_t settings = 0;
  // The actor's spawn group word (actor word 0x30).
  std::uint32_t spawn_group = 0;
  // The actor's spawn flags word (actor word 0x34).
  std::uint32_t spawn_flags = 0;
  // The actor's sub-type (actor halfword 0x1A).
  std::uint16_t sub_type = 0;
  // The new AI state's collision radius (0x94) and movement flags (0x15E).
  std::uint16_t collision_radius = 0;
  std::uint16_t movement_flags = 0;
  // The room's waypoint halfwords, absent when the room has no auxiliary
  // AI section.
  std::optional<std::array<std::uint16_t, kRoomWaypointCount>> room_waypoints;
};

// The AI state fields the settings decode writes, by PS1 AI state offset.
struct ActorAiSettings {
  std::uint32_t far_distance_squared = 0;   // 0xAC
  std::uint32_t near_distance_squared = 0;  // 0xA4
  std::uint16_t view_angle = 0;             // 0x128
  std::int32_t view_cosine = 0;             // 0xA8
  bool settings_bit15 = false;              // 0x25
  bool area_lock = false;                   // 0x29, settings bit 14
  bool settings_bit13 = false;              // 0x24
  // 0x2D: settings bit 12, kept only when a room waypoint is active.
  std::uint8_t action_movement_mask = 0;
  std::uint8_t tile_step_limit = 0;    // 0xB2
  std::uint16_t step_height = 0;       // 0xB0
  std::uint16_t facing_delay = 0;      // 0xBA
  bool settings_bit24 = false;         // 0x21
  bool settings_bit23 = false;         // 0x22
  std::uint8_t relation_category = 0;  // 0x2C, settings bits 21-22
  std::uint8_t settings_bits28 = 0;    // 0x2F
  std::uint8_t settings_bits30 = 0;    // 0x31
  bool settings_bit26 = false;         // 0x20
  WanderPreset wander{};               // 0xD6, 0xD4, 0xD8
  bool settings_bit27 = false;         // 0x27
  bool settings_bit25 = false;         // 0x30
  // 0x32: absent when movement flags 0xC0 keep the state's existing mask.
  std::optional<std::uint8_t> terrain_reject_mask;
  bool settings_bit7 = false;              // 0x33
  bool allow_fallback_action = false;      // 0x26, spawn flags bit 13
  bool special_mode = false;               // 0x2B, spawn flags bit 12
  std::uint8_t patrol_rate = 0;            // 0x102
  std::uint8_t patrol_row = 0;             // 0x101
  std::uint32_t wander_center_radius = 0;  // 0xC8
  std::uint8_t wander_center_tile_x = 0;   // 0xCC
  std::uint8_t wander_center_tile_z = 0;   // 0xCE
  // Selects the orbiting boss behaviour instead of the spawn group's
  // decision behaviour.
  bool orbiting_boss = false;
};

// Decodes the settings word, spawn words and AI state inputs into the new AI
// state's settings fields (func_800E5A9C).
[[nodiscard]] ActorAiSettings ApplyActorAiSettings(
    const ActorAiSettingsSource& source, const ActorAiSettingsTables& tables);
}  // namespace VS::Battle::Ai
