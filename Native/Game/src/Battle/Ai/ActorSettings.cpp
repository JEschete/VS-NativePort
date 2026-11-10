// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/ActorSettings.hpp"

#include <algorithm>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle::Ai {
namespace {
// Saved-flag bits 1-11 land on settings bits 21-31.
constexpr unsigned kFlagsShift = 20;
constexpr std::uint32_t kFlagBits = kSavedAiSettingsMask >> kFlagsShift;

// The facing delay counts in steps of 128 for a small collision radius and
// 32 above it.
constexpr std::uint16_t kSmallStepRadius = 128;
constexpr std::uint32_t kSmallStep = 128;
constexpr std::uint32_t kLargeStep = 32;
constexpr unsigned kFacingDelayShift = 4;
// Movement flags that keep the state's existing terrain reject mask.
constexpr std::uint16_t kKeepTerrainMaskFlags = 0xC0;
// The view angle is in degrees; the cosine takes the half angle in 4096ths
// of a turn.
constexpr std::int32_t kFullTurn = 4096;
constexpr std::int32_t kDegreesPerTurn = 360;
constexpr std::uint16_t kActiveWaypointBit = 0x8000;

// A field of a packed settings or spawn word.
struct BitField {
  unsigned shift;
  std::uint32_t mask;

  [[nodiscard]] constexpr std::uint32_t In(std::uint32_t word) const {
    return (word >> shift) & mask;
  }
  [[nodiscard]] constexpr bool Set(std::uint32_t word) const {
    return In(word) != 0;
  }
};

// Settings word fields.
constexpr BitField kFacingDelayScaleField{2, 3};
constexpr BitField kWanderPresetField{4, 7};
constexpr BitField kSettingsBit7Field{7, 1};
constexpr BitField kStepHeightField{8, 3};
constexpr BitField kTileStepLimitField{10, 3};
constexpr BitField kActionMovementField{12, 1};
constexpr BitField kSettingsBit13Field{13, 1};
constexpr BitField kAreaLockField{14, 1};
constexpr BitField kSettingsBit15Field{15, 1};
constexpr BitField kViewAngleField{16, 3};
constexpr BitField kPerceptionRangeField{18, 7};
constexpr BitField kRelationCategoryField{21, 3};
constexpr BitField kSettingsBit23Field{23, 1};
constexpr BitField kSettingsBit24Field{24, 1};
constexpr BitField kSettingsBit25Field{25, 1};
constexpr BitField kSettingsBit26Field{26, 1};
constexpr BitField kSettingsBit27Field{27, 1};
constexpr BitField kSettingsBits28Field{28, 3};
constexpr BitField kSettingsBits30Field{30, 3};
// Spawn flags (actor word 0x34) fields.
constexpr BitField kSpecialModeField{12, 1};
constexpr BitField kAllowFallbackField{13, 1};
constexpr BitField kPatrolRateField{14, 1};
// Spawn group (actor word 0x30) fields.
constexpr BitField kPatrolRowField{14, 3};
constexpr BitField kWanderRadiusField{16, 0x3F};
constexpr BitField kWanderTileXField{22, 0x1F};
constexpr BitField kWanderTileZField{27, 0x1F};

bool HasActiveWaypoint(
    const std::optional<std::array<std::uint16_t, kRoomWaypointCount>>&
        waypoints) {
  return waypoints && std::any_of(waypoints->begin(), waypoints->end(),
                                  [](std::uint16_t waypoint) {
                                    return (waypoint & kActiveWaypointBit) != 0;
                                  });
}
}  // namespace

// decomp: BATTLE.PRG func_800E5A74 (src/BATTLE/BATTLE.PRG/7CD68.c:595).
std::uint32_t ApplySavedAiSettingsFlags(std::uint32_t settings,
                                        std::uint16_t saved_flags) {
  return ((saved_flags & kFlagBits) << kFlagsShift) |
         (settings & ~kSavedAiSettingsMask);
}

// decomp: BATTLE.PRG func_800E5A9C (src/BATTLE/BATTLE.PRG/7CD68.c:689).
ActorAiSettings ApplyActorAiSettings(const ActorAiSettingsSource& source,
                                     const ActorAiSettingsTables& tables) {
  const std::uint32_t settings = source.settings;
  ActorAiSettings result;

  result.far_distance_squared =
      tables.perception_ranges_squared[kPerceptionRangeField.In(settings)];
  result.near_distance_squared =
      std::min(result.far_distance_squared, kNearDistanceSquaredCap);
  result.view_angle = tables.view_angles[kViewAngleField.In(settings)];
  result.view_cosine = VS::Core::Math::Cosine(
      (static_cast<std::int32_t>(result.view_angle / 2) * kFullTurn) /
      kDegreesPerTurn);

  result.settings_bit15 = kSettingsBit15Field.Set(settings);
  result.area_lock = kAreaLockField.Set(settings);
  result.settings_bit13 = kSettingsBit13Field.Set(settings);
  result.action_movement_mask =
      HasActiveWaypoint(source.room_waypoints)
          ? static_cast<std::uint8_t>(kActionMovementField.In(settings))
          : 0;
  result.tile_step_limit =
      static_cast<std::uint8_t>(kTileStepLimitField.In(settings) + 1);
  result.step_height = tables.step_heights[kStepHeightField.In(settings)];

  const std::uint32_t step =
      source.collision_radius > kSmallStepRadius ? kLargeStep : kSmallStep;
  result.facing_delay = static_cast<std::uint16_t>(
      (step *
       tables.facing_delay_scales[kFacingDelayScaleField.In(settings)]) >>
      kFacingDelayShift);

  result.settings_bit24 = kSettingsBit24Field.Set(settings);
  result.settings_bit23 = kSettingsBit23Field.Set(settings);
  result.relation_category =
      static_cast<std::uint8_t>(kRelationCategoryField.In(settings));
  result.settings_bits28 =
      static_cast<std::uint8_t>(kSettingsBits28Field.In(settings));
  result.settings_bits30 =
      static_cast<std::uint8_t>(kSettingsBits30Field.In(settings));
  result.settings_bit26 = kSettingsBit26Field.Set(settings);
  result.wander = tables.wander_presets[kWanderPresetField.In(settings)];
  result.settings_bit27 = kSettingsBit27Field.Set(settings);
  result.settings_bit25 = kSettingsBit25Field.Set(settings);
  if ((source.movement_flags & kKeepTerrainMaskFlags) == 0) {
    result.terrain_reject_mask =
        tables.terrain_reject_masks[result.settings_bits30];
  }
  result.settings_bit7 = kSettingsBit7Field.Set(settings);

  const std::uint32_t flags = source.spawn_flags;
  const std::uint32_t group = source.spawn_group;
  result.allow_fallback_action = kAllowFallbackField.Set(flags);
  result.special_mode = kSpecialModeField.Set(flags);
  result.patrol_rate = tables.patrol_rates[kPatrolRateField.In(flags)];
  result.patrol_row = static_cast<std::uint8_t>(kPatrolRowField.In(group));
  result.wander_center_radius = kWanderRadiusField.In(group);
  result.wander_center_tile_x =
      static_cast<std::uint8_t>(kWanderTileXField.In(group));
  result.wander_center_tile_z =
      static_cast<std::uint8_t>(kWanderTileZField.In(group));
  result.orbiting_boss =
      static_cast<std::uint16_t>(source.sub_type - kFirstOrbitingBossSubType) <
      kOrbitingBossSubTypeCount;
  return result;
}
}  // namespace VS::Battle::Ai
