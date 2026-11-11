// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/ActorCreation.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Battle/Ai/ActorCommands.hpp"
#include "Battle/Ai/ActorSettings.hpp"
#include "Battle/Ai/FrameUpdate.hpp"
#include "Core/Diagnostics.h"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle::Ai {
namespace {
constexpr unsigned kAttributeKinds = 0x15;
constexpr unsigned kShiftMask = 31;
constexpr std::uint32_t kObjectIdMask = 7;
constexpr std::uint32_t kFirstSpecialObject = 2;
constexpr std::uint32_t kSecondSpecialObject = 4;
constexpr int kStateAuxiliaryTag = 2;
constexpr std::uint16_t kAshleyRadius = 40;
constexpr std::uint16_t kMaxRadius = 62;
constexpr std::uint16_t kLargeBodyRadius = 128;
constexpr std::uint16_t kLargeBodyStepHeight = 32;
constexpr std::uint16_t kStepHeight = 128;
constexpr std::uint8_t kFixedStepClass = 8;
constexpr unsigned kMoveDistanceShift = 7;
constexpr std::uint32_t kReachMargin = 64;
constexpr std::int16_t kGroundClearance = -256;
constexpr std::int16_t kFlightHeight = -128;
constexpr std::uint32_t kNoTile = 0xFFFFFFFFu;
constexpr std::int32_t kNoPreviousTile = -1;
constexpr unsigned kFixedPointShift = 12;
constexpr std::int8_t kForwardPatrol = 1;
// Movement flags 0x30 select locomotion mode 1 (FrameUpdate.cpp); with one
// of 0xC6 also set, an actor spawned on a tile flagged 0x10 hovers there.
constexpr std::uint16_t kModeOneMovementFlags = 0x30;
constexpr std::uint16_t kHoverMovementFlags = 0xC6;
constexpr std::uint16_t kHoverTileFlag = 0x10;
constexpr std::uint8_t kStartedBehavior = 1;
// PackActorAiPersistenceFlags' idle bit.
constexpr std::int16_t kIdleFlag = 0x20;
constexpr unsigned kByteBits = 8;
constexpr unsigned kTileZShift = 16;
constexpr std::uint32_t kByteMask = 0xFF;
constexpr auto kGridSide = static_cast<std::uint32_t>(kTileFlagGridSide);

bool Refuse(const char* message) {
  Core_Debugf("ai", "%s", message);
  return false;
}

void MarkAttributeActor(ActorCreationHost& host, const CreationActor& actor) {
  if ((actor.kind & kAttributeKinds) == 0) return;
  host.StoreAttributeActorMask(
      host.AttributeActorMask() |
      (1u << (static_cast<unsigned>(actor.id) & kShiftMask)));
}

// An auxiliary actor whose model object ID is 2 or 4 takes the room's first
// free special actor slot; with every slot taken it takes none.
bool RegisterSpecialActor(ActorCreationHost& host, const CreationActor& actor) {
  if (actor.allocation_kind != kAuxiliaryAllocationKind) return true;
  const std::optional<std::uint32_t> object_word =
      host.ModelObjectWord(actor.handle);
  if (!object_word.has_value()) {
    return Refuse("AI special actors require the actor's model");
  }
  const std::uint32_t object_id = *object_word & kObjectIdMask;
  if (object_id != kFirstSpecialObject && object_id != kSecondSpecialObject) {
    return true;
  }
  const std::optional<RoomSpecialActors> slots = host.SpecialActors();
  if (!slots.has_value()) {
    return Refuse("AI special actors require the room's AI state");
  }
  const auto free_slot = std::ranges::find(*slots, kNoBattleActor);
  if (free_slot != slots->end()) {
    host.StoreSpecialActor(static_cast<std::size_t>(free_slot - slots->begin()),
                           actor.handle);
  }
  return true;
}

// Ashley's radius is fixed; other actors keep the model's below 63. A model
// wider than 128 keeps its own collision radius with a low step height.
void SizeBody(int actor_id, std::uint16_t model_radius, CreatedAiState& state) {
  state.radius =
      actor_id == 0 ? kAshleyRadius : std::min(model_radius, kMaxRadius);
  if (model_radius > kLargeBodyRadius) {
    state.step_height = kLargeBodyStepHeight;
    state.collision_radius = model_radius;
  } else {
    state.step_height = kStepHeight;
    state.collision_radius = state.radius;
  }
}

// The stats' movement and gait classes become the locomotion flags; a class
// with bit 3 set fixes the move distance to its step.
void ApplyStats(const CreationStats& stats, CreatedAiState& state) {
  state.locomotion_flags = static_cast<std::uint16_t>(
      stats.movement_class | (stats.gait_class << kByteBits));
  state.walk_step = stats.walk_speed;
  state.run_step = stats.run_speed;
  if ((stats.movement_class & kFixedStepClass) != 0) {
    state.move_distance = state.walk_step << kMoveDistanceShift;
  } else if ((stats.gait_class & kFixedStepClass) != 0) {
    state.move_distance = state.run_step << kMoveDistanceShift;
  }
  LocomotionState locomotion{.locomotion_flags = state.locomotion_flags};
  InitializeActorLocomotionMode(locomotion);
  state.movement_flags = locomotion.movement_flags;
  state.locomotion_mode = locomotion.mode;
  state.max_range = std::max(state.walk_step, state.run_step);
}

// Allocates the state and derives its size and locomotion from the model
// and stats.
std::optional<CreatedAiState> CreateBody(ActorCreationHost& host,
                                         const CreationActor& actor) {
  if (actor.id < 0 || actor.id >= kCleanupActorSlots) {
    Refuse("AI creation requires an actor ID in the AI state table");
    return std::nullopt;
  }
  const std::optional<CreationStats> stats = host.Stats(actor.handle);
  if (!host.Model(actor.handle).has_value() || !stats.has_value()) {
    Refuse("AI creation requires the actor's model and stats");
    return std::nullopt;
  }
  if (!host.AllocateAiState(actor.id, actor.handle)) {
    Refuse("AI creation found the heap exhausted");
    return std::nullopt;
  }
  const std::optional<std::int16_t> anchor_y = host.AnchorHeightY(actor.id);
  const std::optional<CreationModel> model = host.Model(actor.handle);
  if (!anchor_y.has_value() || !model.has_value()) {
    Refuse("AI creation requires the actor's anchor height");
    return std::nullopt;
  }
  CreatedAiState state;
  state.actor_id = static_cast<std::uint8_t>(actor.id);
  state.point_height = static_cast<std::int16_t>(*anchor_y - model->position_y);
  SizeBody(actor.id, model->radius, state);
  state.requested_movement_mode = 0;
  state.current_movement_mode = 0;
  state.patrol_direction = kForwardPatrol;
  state.has_auxiliary_tag_bone =
      host.HasAuxiliaryBoneTag(actor.id, kStateAuxiliaryTag);
  ApplyStats(*stats, state);
  return state;
}

// Decodes the settings and relations, then keeps the fixed move distance
// within the far perception range (less the reach margin).
void ApplyReach(ActorCreationHost& host, const CreationActor& actor,
                CreatedAiState& state) {
  std::uint32_t settings = actor.settings;
  if ((actor.saved_settings_flags & 1u) != 0) {
    settings = ApplySavedAiSettingsFlags(settings, actor.saved_settings_flags);
  }
  host.ApplySettings(state, settings);
  host.InitializeRelations(state);
  const auto reach = std::bit_cast<std::int32_t>(
      std::bit_cast<std::uint32_t>(state.move_distance) - kReachMargin);
  const auto reach_squared =
      static_cast<std::uint32_t>(static_cast<std::int64_t>(reach) * reach);
  if (state.far_distance_squared < reach_squared) {
    state.move_distance =
        Core::Math::SquareRootInteger(
            std::bit_cast<std::int32_t>(state.far_distance_squared)) -
        static_cast<std::int32_t>(kReachMargin);
  }
}

void Place(const CreationModel& model, CreatedAiState& state) {
  state.ground_clearance = kGroundClearance;
  state.flight_height = kFlightHeight;
  state.body_height = static_cast<std::int16_t>(model.body_height);
  state.tile = model.tile;
  state.flee_tile = model.tile;
  state.detour_tile = kNoTile;
  state.last_slide_tile = kNoTile;
  state.move_y = std::bit_cast<std::int32_t>(
      std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(model.position_y))
      << kFixedPointShift);
}

// Whether the model's current tile is flagged for hovering; nothing without
// a tile flag grid holding the tile.
std::optional<bool> IsHoverTile(const ActorCreationHost& host,
                                const CreationModel& model) {
  const std::span<const std::uint16_t> flags = host.TileFlags();
  const std::uint32_t x = model.tile & kByteMask;
  const std::uint32_t z = (model.tile >> kTileZShift) & kByteMask;
  const std::size_t cell = (static_cast<std::size_t>(z) * kGridSide) + x;
  if (x >= kGridSide || z >= kGridSide || cell >= flags.size()) {
    Refuse("AI hover start requires the tile flag grid to hold the tile");
    return std::nullopt;
  }
  return (flags[cell] & kHoverTileFlag) != 0;
}

// Ashley starts no command. Another actor hovers over a flagged tile, or,
// fresh or not remembered as idle, stops in its combat pose.
bool StartCommand(ActorCreationHost& host, const CreationActor& actor,
                  const CreationModel& model, std::int16_t saved_flags,
                  CreatedAiState& state) {
  if (actor.id == 0) return true;
  if ((state.movement_flags & kModeOneMovementFlags) != 0 &&
      (state.movement_flags & kHoverMovementFlags) != 0) {
    const std::optional<bool> hover_tile = IsHoverTile(host, model);
    if (!hover_tile.has_value()) return false;
    if (*hover_tile && !model.motion_locked) {
      state.next_tile = model.tile;
      host.IssueCommand(state, CommandIntent::kHover);
      return true;
    }
  }
  if ((saved_flags < 0 || (saved_flags & kIdleFlag) == 0) &&
      actor.combat_pose != 0) {
    host.IssueCommand(state, CommandIntent::kStop);
    state.behavior_mode = kStartedBehavior;
  }
  return true;
}

// The saved relation application always follows the last field creation
// writes, so the host stores the finished state before it.
void Finalize(ActorCreationHost& host, const CreationActor& actor,
              std::int16_t saved_flags, CreatedAiState& state) {
  host.ClearActorCommand(actor.handle);
  // Perception's last seen opcode is the one just cleared.
  state.observed_command_opcode = 0;
  state.previous_tile = kNoPreviousTile;
  state.saved_relation_options = saved_flags;
  host.ApplySavedRelations(state);
  if (state.actor_id != 0) {
    while (host.RebuildActionPlans(state)) {
    }
  }
}
}  // namespace

// decomp: func_800E6178 (src/BATTLE/BATTLE.PRG/7CD68.c:934).
std::optional<bool> CreateActorAiState(ActorCreationHost& host,
                                       const CreationActor& actor,
                                       std::int16_t saved_flags) {
  MarkAttributeActor(host, actor);
  if (!RegisterSpecialActor(host, actor)) return std::nullopt;
  if ((actor.kind & kAiStateActorKinds) == 0) return false;
  std::optional<CreatedAiState> state = CreateBody(host, actor);
  if (!state.has_value()) return std::nullopt;
  ApplyReach(host, actor, *state);
  const std::optional<CreationModel> model = host.Model(actor.handle);
  if (!model.has_value()) {
    Refuse("AI creation requires the actor's model");
    return std::nullopt;
  }
  Place(*model, *state);
  if (!StartCommand(host, actor, *model, saved_flags, *state)) {
    return std::nullopt;
  }
  Finalize(host, actor, saved_flags, *state);
  return saved_flags >= 0 && (saved_flags & kIdleFlag) != 0;
}
}  // namespace VS::Battle::Ai
