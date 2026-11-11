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

#include "Battle/Ai/ActorCleanup.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/BossOrbitLive.hpp"
#include "Battle/Ai/DecisionsLive.hpp"
#include "Battle/Ai/PlanningPrimitivesLive.hpp"
#include "Battle/Ai/RelationsLive.hpp"

extern "C" {
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Core/Diagnostics.h"
#include "Field/Gimmicks.h"
#include "Field/Room.h"
}

namespace VS::Battle::Ai {
namespace {
// The actor-state creation view reads the saved pair from the actor's group
// word, so its second word is the room spawn's saved settings flags.
static_assert(offsetof(Battle_Ai_AiActorView, savedAiSettings) +
                  offsetof(Battle_Ai_SavedSettingsPair, opaque4) ==
              offsetof(Battle_Ai_AiActorView, savedAiSettingsFlags));
static_assert(offsetof(Battle_Ai_AiActorView, savedAiSettingsFlags) ==
              offsetof(Battle_Actor, reserved34));
static_assert(offsetof(Battle_Ai_AiActorView, aiSettings) ==
              offsetof(Battle_Actor, templateWord2C));
// The settings actor view reads the actor's sub-type, its spawn group word
// (the AI group bit-fields) and its spawn flags word.
static_assert(offsetof(Battle_Ai_SettingsActorView, kind) ==
              offsetof(Battle_Actor, subType));
static_assert(offsetof(Battle_Ai_SettingsActorView, opaque30) ==
              offsetof(Battle_Actor, lastTriggerTile) +
                  sizeof(Battle_Actor::lastTriggerTile));
static_assert(offsetof(Battle_Ai_SettingsActorView, opaque34) ==
              offsetof(Battle_Actor, reserved34));

// The settings view aliases the AI state it initialises, so the fields it
// writes must be the ones the AI behaviours read.
using SettingsView = Battle_Ai_SettingsView;
using AiState = Battle_Ai_ActorAiState;
static_assert(offsetof(SettingsView, linkedActor) ==
              offsetof(AiState, linkedActor));
static_assert(offsetof(SettingsView, settingsConstant19) ==
              offsetof(AiState, settingsConstant19));
static_assert(offsetof(SettingsView, settingsBit26) ==
              offsetof(AiState, settingsBit26));
static_assert(offsetof(SettingsView, settingsByte3Bit0) ==
              offsetof(AiState, settingsByte3Bit0));
static_assert(offsetof(SettingsView, settingsConstant23) ==
              offsetof(AiState, movementEnabled));
static_assert(offsetof(SettingsView, allowFallbackAction) ==
              offsetof(AiState, allowFallbackAction));
static_assert(offsetof(SettingsView, settingsBit27) ==
              offsetof(AiState, settingsBit27));
static_assert(offsetof(SettingsView, settingsBit14) ==
              offsetof(AiState, settingsBit14));
static_assert(offsetof(SettingsView, settingsModeLow) ==
              offsetof(AiState, settingsModeLow));
static_assert(offsetof(SettingsView, settingsBits21) ==
              offsetof(AiState, settingsBits21));
static_assert(offsetof(SettingsView, settingsBit25) ==
              offsetof(AiState, settingsBit25));
static_assert(offsetof(SettingsView, settingsBits30Value) ==
              offsetof(AiState, terrain_reject_mask));
static_assert(offsetof(SettingsView, actor) == offsetof(AiState, actor));
static_assert(offsetof(SettingsView, collisionRadius) ==
              offsetof(AiState, collision_radius));
static_assert(offsetof(SettingsView, opaqueB0) ==
              offsetof(AiState, step_height_bytes));
static_assert(offsetof(SettingsView, facingDelay) ==
              offsetof(AiState, facingDelay));
static_assert(offsetof(SettingsView, opaqueC8) ==
              offsetof(AiState, wander_center_radius));
static_assert(offsetof(SettingsView, opaqueCC) ==
              offsetof(AiState, wander_center));
static_assert(offsetof(SettingsView, restPeriod) ==
              offsetof(AiState, restPeriod));
static_assert(offsetof(SettingsView, movePeriod) ==
              offsetof(AiState, movePeriod));
static_assert(offsetof(SettingsView, wanderRate) ==
              offsetof(AiState, wanderRate));
static_assert(offsetof(SettingsView, opaque101) ==
              offsetof(AiState, patrolRow));
static_assert(offsetof(SettingsView, opaque102) ==
              offsetof(AiState, patrolRate));
static_assert(offsetof(SettingsView, movementFlags) ==
              offsetof(AiState, movementFlags));

// The waypoint halfwords follow the patrol routes in the room's auxiliary
// AI section.
using RoomWaypoints = std::array<std::uint16_t, kRoomWaypointCount>;
constexpr std::size_t kWaypointsOffset = 0x440;
constexpr std::size_t kWaypointsEnd = kWaypointsOffset + sizeof(RoomWaypoints);

// The module tables the decode indexes must cover each field's range.
static_assert(std::size(Battle_Ai_kPerceptionRangesSquared) >=
              kPerceptionRangeCount);
static_assert(std::size(Battle_Ai_viewAngles) >= kSettingsChoiceCount);
static_assert(std::size(Battle_Ai_settingsBits8Values) >= kSettingsChoiceCount);
static_assert(std::size(Battle_Ai_facingDelayScales) >= kSettingsChoiceCount);
static_assert(std::size(Battle_Ai_kWanderTimingPresets) >= kWanderPresetCount);
static_assert(std::size(Battle_Ai_kSettingsBits30Values) >=
              kSettingsChoiceCount);
static_assert(std::size(Battle_Ai_kFlags34Bit14Values) >= kPatrolRateCount);

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

ActorAiSettingsTables LiveSettingsTables() {
  ActorAiSettingsTables tables;
  std::copy_n(std::begin(Battle_Ai_kPerceptionRangesSquared),
              tables.perception_ranges_squared.size(),
              tables.perception_ranges_squared.begin());
  for (std::size_t index = 0; index < kSettingsChoiceCount; ++index) {
    tables.view_angles[index] = Battle_Ai_viewAngles[index][0];
    tables.step_heights[index] = Battle_Ai_settingsBits8Values[index][0];
    tables.facing_delay_scales[index] = Battle_Ai_facingDelayScales[index];
    tables.terrain_reject_masks[index] = Battle_Ai_kSettingsBits30Values[index];
  }
  for (std::size_t index = 0; index < kWanderPresetCount; ++index) {
    const Battle_Ai_WanderTiming& preset =
        Battle_Ai_kWanderTimingPresets[index];
    tables.wander_presets[index] =
        WanderPreset{preset.movePeriod, preset.restPeriod, preset.rate};
  }
  std::copy_n(std::begin(Battle_Ai_kFlags34Bit14Values),
              tables.patrol_rates.size(), tables.patrol_rates.begin());
  return tables;
}

std::optional<RoomWaypoints> LiveRoomWaypoints() {
  if (Battle_Ai_roomHasWaypoints == nullptr) return std::nullopt;
  const auto* auxiliary =
      static_cast<const unsigned char*>(Battle_Ai_roomHasWaypoints);
  Require(auxiliary == Field_roomData.section6 &&
              Field_roomData.header.section6Length >= kWaypointsEnd &&
              reinterpret_cast<const unsigned char*>(Battle_Ai_roomWaypoints) ==
                  auxiliary + kWaypointsOffset,
          "AI settings: room waypoints do not belong to the active room "
          "auxiliary section");
  RoomWaypoints waypoints{};
  std::memcpy(waypoints.data(), auxiliary + kWaypointsOffset, sizeof waypoints);
  return waypoints;
}
}  // namespace

void ApplyLiveSavedAiSettingsFlags(
    int* settings_word, const Battle_Ai_SavedSettingsPair* saved_settings) {
  Require(settings_word != nullptr && saved_settings != nullptr,
          "AI saved settings require a settings word and a saved pair");
  *settings_word = std::bit_cast<int>(ApplySavedAiSettingsFlags(
      std::bit_cast<std::uint32_t>(*settings_word), saved_settings->opaque4));
}

void ApplyLiveActorAiSettings(Battle_Ai_SettingsView* state,
                              const Battle_Ai_SettingsWord* settings) {
  Require(state != nullptr && settings != nullptr && state->actor != nullptr,
          "AI settings require a state, its actor and a settings word");
  const Battle_Ai_SettingsActorView& actor = *state->actor;
  ActorAiSettingsSource source;
  source.settings = settings->word;
  source.spawn_group = actor.opaque30.word;
  source.spawn_flags = actor.opaque34.word;
  source.sub_type = actor.kind;
  source.collision_radius = state->collisionRadius;
  source.movement_flags = state->movementFlags;
  source.room_waypoints = LiveRoomWaypoints();
  const ActorAiSettings decoded =
      ApplyActorAiSettings(source, LiveSettingsTables());

  state->farDistanceSquared = decoded.far_distance_squared;
  state->nearDistanceSquared = decoded.near_distance_squared;
  state->angle = decoded.view_angle;
  state->viewCosine = decoded.view_cosine;
  state->settingsBit15 = decoded.settings_bit15 ? 1 : 0;
  state->settingsBit14 = decoded.area_lock ? 1 : 0;
  state->settingsBit13 = decoded.settings_bit13 ? 1 : 0;
  state->opaque2D = decoded.action_movement_mask;
  state->opaqueB2 = decoded.tile_step_limit;
  state->opaqueB0 = decoded.step_height;
  state->facingDelay = decoded.facing_delay;
  state->settingsConstant23 = 1;
  state->settingsByte3Bit0 = decoded.settings_bit24 ? 1 : 0;
  state->settingsBit23 = decoded.settings_bit23 ? 1 : 0;
  state->settingsBits21 = decoded.relation_category;
  state->settingsBits28 = decoded.settings_bits28;
  state->settingsBits30 = decoded.settings_bits30;
  state->settingsBit26 = decoded.settings_bit26 ? 1 : 0;
  state->movePeriod = decoded.wander.move_period;
  state->restPeriod = decoded.wander.rest_period;
  state->wanderRate = decoded.wander.rate;
  state->settingsBit27 = decoded.settings_bit27 ? 1 : 0;
  state->settingsBit25 = decoded.settings_bit25 ? 1 : 0;
  if (decoded.terrain_reject_mask) {
    state->settingsBits30Value = *decoded.terrain_reject_mask;
  }
  state->settingsByte0Bit7 = decoded.settings_bit7 ? 1 : 0;
  state->allowFallbackAction = decoded.allow_fallback_action ? 1 : 0;
  state->settingsModeLow = decoded.special_mode ? 1 : 0;
  state->opaque102 = decoded.patrol_rate;
  state->opaque101 = decoded.patrol_row;
  state->opaqueC8 = decoded.wander_center_radius;
  state->opaqueCC = decoded.wander_center_tile_x;
  state->opaqueCE = decoded.wander_center_tile_z;
  if (decoded.orbiting_boss) {
    state->decisionCallback = UpdateLiveOrbitingBossBehavior;
    state->settingsConstant19 = 1;
    AllocateLiveOrbitingBossState(state);
  } else {
    state->decisionCallback = SelectLiveDecisionBehavior(source.spawn_group);
  }
  state->linkedActor = static_cast<unsigned char>(kNoLinkedActor);
  ResolveLiveAiLinkedActors();
  // The settings view is the AI state it initialises (asserted above).
  BuildLiveAiActionMetadata(reinterpret_cast<Battle_Ai_ActorAiState*>(state));
}
}  // namespace VS::Battle::Ai
