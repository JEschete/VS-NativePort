// Hand-maintained (tools/port/owned.csv).
// Live binding for the actor AI settings policies (ActorSettings.cpp).
#include "Battle/Ai/ActorSettingsLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <optional>

#include "Battle/Actor.h"
#include "Battle/Ai/ActorCleanup.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/BossOrbitLive.hpp"
#include "Battle/Ai/DecisionsLive.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/OrbitingBoss.hpp"
#include "Battle/Ai/PlanningPrimitivesLive.hpp"
#include "Battle/Ai/RelationsLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Field/Gimmicks.h"
#include "Field/Room.h"

namespace VS::Battle::Ai {
namespace {
// The actor-state creation view reads the saved pair from the actor's group
// word, so its second word is the room spawn's saved settings flags.
static_assert(offsetof(VS::Battle::Ai::AiActorView, saved_ai_settings) +
                  offsetof(VS::Battle::Ai::SavedSettingsPair, opaque4) ==
              offsetof(VS::Battle::Ai::AiActorView, saved_ai_settings_flags));
static_assert(offsetof(VS::Battle::Ai::AiActorView, saved_ai_settings_flags) ==
              offsetof(Battle_Actor, reserved34));
static_assert(offsetof(VS::Battle::Ai::AiActorView, ai_settings) ==
              offsetof(Battle_Actor, templateWord2C));
// The settings actor view reads the actor's sub-type, its spawn group word
// (the AI group bit-fields) and its spawn flags word.
static_assert(offsetof(VS::Battle::Ai::SettingsActorView, kind) ==
              offsetof(Battle_Actor, subType));
static_assert(offsetof(VS::Battle::Ai::SettingsActorView, opaque30) ==
              offsetof(Battle_Actor, lastTriggerTile) +
                  sizeof(Battle_Actor::lastTriggerTile));
static_assert(offsetof(VS::Battle::Ai::SettingsActorView, opaque34) ==
              offsetof(Battle_Actor, reserved34));

// The settings view aliases the AI state it initialises, so the fields it
// writes must be the ones the AI behaviours read.
using SettingsView = VS::Battle::Ai::SettingsView;
using AiState = VS::Battle::Ai::ActorAiState;
static_assert(offsetof(SettingsView, linked_actor) ==
              offsetof(AiState, linked_actor));
static_assert(offsetof(SettingsView, settings_constant19) ==
              offsetof(AiState, settings_constant19));
static_assert(offsetof(SettingsView, settings_bit26) ==
              offsetof(AiState, settings_bit26));
static_assert(offsetof(SettingsView, settings_byte3_bit0) ==
              offsetof(AiState, settings_byte3_bit0));
static_assert(offsetof(SettingsView, settings_constant23) ==
              offsetof(AiState, movement_enabled));
static_assert(offsetof(SettingsView, allow_fallback_action) ==
              offsetof(AiState, allow_fallback_action));
static_assert(offsetof(SettingsView, settings_bit27) ==
              offsetof(AiState, settings_bit27));
static_assert(offsetof(SettingsView, settings_bit14) ==
              offsetof(AiState, settings_bit14));
static_assert(offsetof(SettingsView, settings_mode_low) ==
              offsetof(AiState, settings_mode_low));
static_assert(offsetof(SettingsView, settings_bits21) ==
              offsetof(AiState, settings_bits21));
static_assert(offsetof(SettingsView, settings_bit25) ==
              offsetof(AiState, settings_bit25));
static_assert(offsetof(SettingsView, settings_bits30_value) ==
              offsetof(AiState, terrain_reject_mask));
static_assert(offsetof(SettingsView, actor) == offsetof(AiState, actor));
static_assert(offsetof(SettingsView, collision_radius) ==
              offsetof(AiState, collision_radius));
static_assert(offsetof(SettingsView, opaque_b0) ==
              offsetof(AiState, step_height_bytes));
static_assert(offsetof(SettingsView, facing_delay) ==
              offsetof(AiState, facing_delay));
static_assert(offsetof(SettingsView, opaque_c8) ==
              offsetof(AiState, wander_center_radius));
static_assert(offsetof(SettingsView, opaque_cc) ==
              offsetof(AiState, wander_center));
static_assert(offsetof(SettingsView, rest_period) ==
              offsetof(AiState, rest_period));
static_assert(offsetof(SettingsView, move_period) ==
              offsetof(AiState, move_period));
static_assert(offsetof(SettingsView, wander_rate) ==
              offsetof(AiState, wander_rate));
static_assert(offsetof(SettingsView, opaque101) ==
              offsetof(AiState, patrol_row));
static_assert(offsetof(SettingsView, opaque102) ==
              offsetof(AiState, patrol_rate));
static_assert(offsetof(SettingsView, movement_flags) ==
              offsetof(AiState, movement_flags));

// The waypoint halfwords follow the patrol routes in the room's auxiliary
// AI section.
using RoomWaypoints = std::array<std::uint16_t, kRoomWaypointCount>;
constexpr std::size_t kWaypointsOffset = 0x440;
constexpr std::size_t kWaypointsEnd = kWaypointsOffset + sizeof(RoomWaypoints);

// The module tables the decode indexes must cover each field's range.
static_assert(std::size(VS::Battle::Ai::kPerceptionRangesSquared) >=
              kPerceptionRangeCount);
static_assert(std::size(VS::Battle::Ai::viewAngles) >= kSettingsChoiceCount);
static_assert(std::size(VS::Battle::Ai::settingsBits8Values) >=
              kSettingsChoiceCount);
static_assert(std::size(VS::Battle::Ai::facingDelayScales) >=
              kSettingsChoiceCount);
static_assert(std::size(VS::Battle::Ai::kWanderTimingPresets) >=
              kWanderPresetCount);
static_assert(std::size(VS::Battle::Ai::kSettingsBits30Values) >=
              kSettingsChoiceCount);
static_assert(std::size(VS::Battle::Ai::kFlags34Bit14Values) >=
              kPatrolRateCount);

ActorAiSettingsTables LiveSettingsTables() {
  ActorAiSettingsTables tables;
  std::copy_n(std::begin(VS::Battle::Ai::kPerceptionRangesSquared),
              tables.perception_ranges_squared.size(),
              tables.perception_ranges_squared.begin());
  for (std::size_t index = 0; index < kSettingsChoiceCount; ++index) {
    tables.view_angles[index] = VS::Battle::Ai::viewAngles[index][0];
    tables.step_heights[index] = VS::Battle::Ai::settingsBits8Values[index][0];
    tables.facing_delay_scales[index] =
        VS::Battle::Ai::facingDelayScales[index];
    tables.terrain_reject_masks[index] =
        VS::Battle::Ai::kSettingsBits30Values[index];
  }
  for (std::size_t index = 0; index < kWanderPresetCount; ++index) {
    const VS::Battle::Ai::WanderTiming& preset =
        VS::Battle::Ai::kWanderTimingPresets[index];
    tables.wander_presets[index] =
        WanderPreset{preset.move_period, preset.rest_period, preset.rate};
  }
  std::copy_n(std::begin(VS::Battle::Ai::kFlags34Bit14Values),
              tables.patrol_rates.size(), tables.patrol_rates.begin());
  return tables;
}

std::optional<RoomWaypoints> LiveRoomWaypoints() {
  if (VS::Battle::Ai::roomHasWaypoints == nullptr) return std::nullopt;
  const auto* auxiliary =
      static_cast<const unsigned char*>(VS::Battle::Ai::roomHasWaypoints);
  Require(
      auxiliary == Field_roomData.section6 &&
          Field_roomData.header.section6Length >= kWaypointsEnd &&
          reinterpret_cast<const unsigned char*>(
              VS::Battle::Ai::roomWaypoints) == auxiliary + kWaypointsOffset,
      "AI settings: room waypoints do not belong to the active room "
      "auxiliary section");
  RoomWaypoints waypoints{};
  std::memcpy(waypoints.data(), auxiliary + kWaypointsOffset, sizeof waypoints);
  return waypoints;
}
}  // namespace

void ApplyLiveSavedAiSettingsFlags(
    int* settings_word,
    const VS::Battle::Ai::SavedSettingsPair* saved_settings) {
  Require(settings_word != nullptr && saved_settings != nullptr,
          "AI saved settings require a settings word and a saved pair");
  *settings_word = std::bit_cast<int>(ApplySavedAiSettingsFlags(
      std::bit_cast<std::uint32_t>(*settings_word), saved_settings->opaque4));
}

void ApplyLiveActorAiSettings(VS::Battle::Ai::SettingsView* state,
                              const VS::Battle::Ai::SettingsWord* settings) {
  Require(state != nullptr && settings != nullptr && state->actor != nullptr,
          "AI settings require a state, its actor and a settings word");
  const VS::Battle::Ai::SettingsActorView& actor = *state->actor;
  ActorAiSettingsSource source;
  source.settings = settings->word;
  source.spawn_group = actor.opaque30.word;
  source.spawn_flags = actor.opaque34.word;
  source.sub_type = actor.kind;
  source.collision_radius = state->collision_radius;
  source.movement_flags = state->movement_flags;
  source.room_waypoints = LiveRoomWaypoints();
  const ActorAiSettings decoded =
      ApplyActorAiSettings(source, LiveSettingsTables());

  state->far_distance_squared = decoded.far_distance_squared;
  state->near_distance_squared = decoded.near_distance_squared;
  state->angle = decoded.view_angle;
  state->view_cosine = decoded.view_cosine;
  state->settings_bit15 = decoded.settings_bit15 ? 1 : 0;
  state->settings_bit14 = decoded.area_lock ? 1 : 0;
  state->settings_bit13 = decoded.settings_bit13 ? 1 : 0;
  state->opaque_2d = decoded.action_movement_mask;
  state->opaque_b2 = decoded.tile_step_limit;
  state->opaque_b0 = decoded.step_height;
  state->facing_delay = decoded.facing_delay;
  state->settings_constant23 = 1;
  state->settings_byte3_bit0 = decoded.settings_bit24 ? 1 : 0;
  state->settings_bit23 = decoded.settings_bit23 ? 1 : 0;
  state->settings_bits21 = decoded.relation_category;
  state->settings_bits28 = decoded.settings_bits28;
  state->settings_bits30 = decoded.settings_bits30;
  state->settings_bit26 = decoded.settings_bit26 ? 1 : 0;
  state->move_period = decoded.wander.move_period;
  state->rest_period = decoded.wander.rest_period;
  state->wander_rate = decoded.wander.rate;
  state->settings_bit27 = decoded.settings_bit27 ? 1 : 0;
  state->settings_bit25 = decoded.settings_bit25 ? 1 : 0;
  if (decoded.terrain_reject_mask) {
    state->settings_bits30_value = *decoded.terrain_reject_mask;
  }
  state->settings_byte0_bit7 = decoded.settings_bit7 ? 1 : 0;
  state->allow_fallback_action = decoded.allow_fallback_action ? 1 : 0;
  state->settings_mode_low = decoded.special_mode ? 1 : 0;
  state->opaque102 = decoded.patrol_rate;
  state->opaque101 = decoded.patrol_row;
  state->opaque_c8 = decoded.wander_center_radius;
  state->opaque_cc = decoded.wander_center_tile_x;
  state->opaque_ce = decoded.wander_center_tile_z;
  if (decoded.orbiting_boss) {
    state->decision_callback = UpdateLiveOrbitingBossBehavior;
    state->settings_constant19 = 1;
    AllocateLiveOrbitingBossState(state);
  } else {
    state->decision_callback = SelectLiveDecisionBehavior(source.spawn_group);
  }
  state->linked_actor = static_cast<unsigned char>(kNoLinkedActor);
  ResolveLiveAiLinkedActors();
  // The settings view is the AI state it initialises (asserted above).
  BuildLiveAiActionMetadata(
      reinterpret_cast<VS::Battle::Ai::ActorAiState*>(state));
}
}  // namespace VS::Battle::Ai
