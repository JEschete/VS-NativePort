// Hand-maintained (tools/port/owned.csv).
// Live binding for the actor AI state creation (ActorCreation.cpp).
#include "Battle/Ai/ActorCreationLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <optional>
#include <span>
#include <type_traits>

#include "Actors/Skeleton.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/ActorSettingsLive.hpp"
#include "Battle/Ai/PlanningWorkspaceLive.hpp"
#include "Battle/Ai/RelationsLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"

extern "C" {
#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/ModelInstance.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Navigation.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle::Ai {
namespace {
using AiState = Battle_Ai_ActorAiState;
using CreationView = Battle_Ai_AiCreationView;
using ActorView = Battle_Ai_AiActorView;
using ModelView = Battle_Ai_CreationModelView;
using StatsView = Battle_Ai_CreationStatsView;
using BoneTagKind = VS::Actors::BoneTagKind;

// The far perception range (AI state 0xAC) within reservedA0 (0xA0).
constexpr std::size_t kFarRangeOffset = 12;
// Model word 0x16C follows an auxiliary actor core's eight matrices.
constexpr std::size_t kCoreModel = offsetof(Actors_ActorCore, model);
constexpr std::size_t kCoreObject = offsetof(Actors_ActorCore, reserved16C);
constexpr std::size_t kModelObjectWordOffset = kCoreObject - kCoreModel;
// The command flags bits creation keeps (bits 0-3 are cleared).
constexpr std::uint8_t kKeptCommandFlags = 0xF0;
// Bytes 1 and 3 of the actor's state word hold its command opcode and flags.
constexpr std::size_t kCommandOpcodeByte = 1;
constexpr std::size_t kCommandFlagsByte = 3;
constexpr int kAuxiliaryTagKind = static_cast<int>(BoneTagKind::kAuxiliary);
constexpr auto kGridSide = static_cast<std::size_t>(kTileFlagGridSide);
constexpr std::size_t kTileFlagCells = kGridSide * kGridSide;
constexpr unsigned kTileLayerShift = 8;
constexpr unsigned kTileZShift = 16;
constexpr unsigned kTileStatusShift = 24;

static_assert(std::size(Battle_Ai_actorAiStates) == kCleanupActorSlots);
static_assert(std::extent_v<decltype(Battle_Ai_RoomAiState::specialActors)> ==
              kRoomSpecialActorSlots);
static_assert(
    std::extent_v<std::remove_pointer_t<decltype(Battle_Ai_tileFlagMap)>> ==
    kTileFlagGridSide);

// The original read the battle actor through the creation actor view.
static_assert(offsetof(ActorView, actorId) == offsetof(Battle_Actor, id));
static_assert(offsetof(ActorView, stateFlags) + kCommandOpcodeByte ==
              offsetof(Battle_Actor, commandOpcode));
static_assert(offsetof(ActorView, stateFlags) + kCommandFlagsByte ==
              offsetof(Battle_Actor, commandFlags));
static_assert(offsetof(ActorView, aiControlFlags) ==
              offsetof(Battle_Actor, kind));
static_assert(offsetof(ActorView, startCommandFlag) ==
              offsetof(Battle_Actor, spawnByte26));
static_assert(offsetof(ActorView, savedAiSettingsFlags) ==
              offsetof(Battle_Actor, reserved34));
static_assert(offsetof(ActorView, aiSettings) ==
              offsetof(Battle_Actor, templateWord2C));
static_assert(offsetof(ActorView, stats) == offsetof(Battle_Actor, stats));
static_assert(offsetof(ActorView, listExemptFlag) ==
              offsetof(Battle_Actor, allocationKind));
static_assert(offsetof(ActorView, model) == offsetof(Battle_Actor, model));

// ...its model through the creation model view...
static_assert(offsetof(Actors_ActorRecord, instance) == 0);
static_assert(offsetof(ModelView, position) ==
              offsetof(Actors_ModelInstance, position));
static_assert(offsetof(ModelView, tile) ==
              offsetof(Actors_ModelInstance, currentTileX));
static_assert(offsetof(ModelView, kind) == kModelObjectWordOffset);
static_assert(offsetof(ModelView, radius) ==
              offsetof(Actors_ActorRecord, collisionRadius));
static_assert(offsetof(ModelView, opaque63E) ==
              offsetof(Actors_ActorRecord, collisionHeight));

// ...its stats through the creation stats view...
static_assert(offsetof(StatsView, opaque30) ==
              offsetof(Battle_ActorStats, movementClass));
static_assert(offsetof(StatsView, opaque31) ==
              offsetof(Battle_ActorStats, walkSpeed));
static_assert(offsetof(StatsView, opaque32) ==
              offsetof(Battle_ActorStats, gaitClass));
static_assert(offsetof(StatsView, opaque33) ==
              offsetof(Battle_ActorStats, runSpeed));

// ...and wrote the AI state through the creation view.
static_assert(offsetof(CreationView, hasTaggedBone3) ==
              offsetof(AiState, hasTaggedBone3));
static_assert(offsetof(CreationView, tile) == offsetof(AiState, tile));
static_assert(offsetof(CreationView, previousTile) ==
              offsetof(AiState, previousTile));
static_assert(offsetof(CreationView, options) ==
              offsetof(AiState, savedRelationOptions));
static_assert(offsetof(CreationView, actor) == offsetof(AiState, actor));
static_assert(offsetof(CreationView, model) == offsetof(AiState, model));
static_assert(offsetof(CreationView, stats) == offsetof(AiState, stats));
static_assert(offsetof(CreationView, positionSource) ==
              offsetof(AiState, nativePointer60));
static_assert(offsetof(CreationView, tileSource) ==
              offsetof(AiState, nativePointer64));
static_assert(offsetof(CreationView, pointHeight) ==
              offsetof(AiState, pointHeight));
static_assert(offsetof(CreationView, actorId) == offsetof(AiState, actorId));
static_assert(offsetof(CreationView, opaque89) ==
              offsetof(AiState, requestedMovementMode));
static_assert(offsetof(CreationView, opaque8A) ==
              offsetof(AiState, currentMovementMode));
static_assert(offsetof(CreationView, radius) == offsetof(AiState, radius));
static_assert(offsetof(CreationView, collisionRadius) ==
              offsetof(AiState, collision_radius));
static_assert(offsetof(CreationView, opaque9C) ==
              offsetof(AiState, last_slide_tile_bytes));
static_assert(offsetof(CreationView, farDistanceSquared) ==
              offsetof(AiState, reservedA0) + kFarRangeOffset);
static_assert(offsetof(Battle_Ai_SettingsView, farDistanceSquared) ==
              offsetof(CreationView, farDistanceSquared));
static_assert(offsetof(CreationView, maxRange) == offsetof(AiState, maxRange));
static_assert(offsetof(CreationView, moveDistance) ==
              offsetof(AiState, moveDistance));
static_assert(offsetof(CreationView, bodyHeight) ==
              offsetof(AiState, bodyHeight));
static_assert(offsetof(CreationView, groundClearance) ==
              offsetof(AiState, reservedBC));
static_assert(offsetof(CreationView, flightHeight) ==
              offsetof(AiState, flightHeight));
static_assert(offsetof(CreationView, opaque103) ==
              offsetof(AiState, patrolDirection));
static_assert(offsetof(CreationView, opaque110) ==
              offsetof(AiState, flee_tile));
static_assert(offsetof(CreationView, opaque124) ==
              offsetof(AiState, reserved124));
static_assert(offsetof(CreationView, behaviorMode) ==
              offsetof(AiState, behaviorMode));
static_assert(offsetof(CreationView, stepHeight) ==
              offsetof(AiState, opaque13C));
static_assert(offsetof(CreationView, movementFlags) ==
              offsetof(AiState, movementFlags));
static_assert(offsetof(CreationView, nextTile) == offsetof(AiState, nextTile));
static_assert(offsetof(CreationView, moveY) == offsetof(AiState, moveY));
static_assert(offsetof(CreationView, opaque194) ==
              offsetof(AiState, detour_tile_bytes));
static_assert(offsetof(CreationView, opaque224) ==
              offsetof(AiState, locomotionFlags));
static_assert(offsetof(CreationView, opaque226) == offsetof(AiState, walkStep));
static_assert(offsetof(CreationView, opaque227) == offsetof(AiState, runStep));

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

BattleActorHandle HandleOf(const Battle_Actor* actor) {
  return static_cast<BattleActorHandle>(
      reinterpret_cast<std::uintptr_t>(actor));
}

// The core hands back only the handle CreateLiveActorAiState made from a
// live actor.
Battle_Actor& ActorOf(BattleActorHandle actor) {
  Require(actor != kNoBattleActor, "AI creation requires an actor");
  return *reinterpret_cast<Battle_Actor*>(static_cast<std::uintptr_t>(actor));
}

// Only actors with an equipped or secondary body allocation carry the actor
// record fields past the model instance; an auxiliary core ends after its
// matrices.
const Actors_ActorRecord* RecordOf(const Battle_Actor& actor) {
  if (actor.model == nullptr ||
      actor.allocationKind == kAuxiliaryAllocationKind) {
    return nullptr;
  }
  return reinterpret_cast<const Actors_ActorRecord*>(actor.model);
}

std::uint32_t TileWord(const Actors_ModelInstance& model) {
  return model.currentTileX |
         (static_cast<std::uint32_t>(model.currentTileLayer)
          << kTileLayerShift) |
         (static_cast<std::uint32_t>(model.currentTileZ) << kTileZShift) |
         (static_cast<std::uint32_t>(model.reserved5F) << kTileStatusShift);
}

template <typename Value, typename Bytes>
Value ReadBytes(const Bytes& bytes) {
  static_assert(sizeof(Bytes) >= sizeof(Value));
  Value value{};
  std::memcpy(&value, &bytes, sizeof value);
  return value;
}

template <typename Bytes, typename Value>
void WriteBytes(Bytes& bytes, Value value) {
  static_assert(sizeof(Bytes) >= sizeof(Value));
  std::memcpy(&bytes, &value, sizeof value);
}

void Store(const CreatedAiState& created, AiState& state) {
  state.hasTaggedBone3 = created.has_auxiliary_tag_bone ? 1 : 0;
  state.tile.word = created.tile;
  state.previousTile = created.previous_tile;
  state.savedRelationOptions = created.saved_relation_options;
  state.pointHeight = created.point_height;
  state.actorId = created.actor_id;
  state.requestedMovementMode = created.requested_movement_mode;
  state.currentMovementMode = created.current_movement_mode;
  state.radius = created.radius;
  state.collision_radius = created.collision_radius;
  WriteBytes(state.last_slide_tile_bytes, created.last_slide_tile);
  std::memcpy(&state.reservedA0[kFarRangeOffset], &created.far_distance_squared,
              sizeof created.far_distance_squared);
  state.maxRange = created.max_range;
  state.moveDistance = created.move_distance;
  state.bodyHeight = created.body_height;
  WriteBytes(state.reservedBC, created.ground_clearance);
  state.flightHeight = created.flight_height;
  state.patrolDirection = created.patrol_direction;
  state.flee_tile.word = created.flee_tile;
  state.reserved124 = created.observed_command_opcode;
  state.locomotionMode = created.locomotion_mode;
  state.behaviorMode = created.behavior_mode;
  state.opaque13C = created.step_height;
  state.movementFlags = created.movement_flags;
  state.nextTile.word = created.next_tile;
  state.moveY = created.move_y;
  WriteBytes(state.detour_tile_bytes, created.detour_tile);
  state.locomotionFlags = created.locomotion_flags;
  state.walkStep = created.walk_step;
  state.runStep = created.run_step;
}

CreatedAiState Load(const AiState& state) {
  CreatedAiState created;
  created.has_auxiliary_tag_bone = state.hasTaggedBone3 != 0;
  created.tile = state.tile.word;
  created.previous_tile = state.previousTile;
  created.saved_relation_options = state.savedRelationOptions;
  created.point_height = state.pointHeight;
  created.actor_id = state.actorId;
  created.requested_movement_mode = state.requestedMovementMode;
  created.current_movement_mode = state.currentMovementMode;
  created.radius = state.radius;
  created.collision_radius = state.collision_radius;
  created.last_slide_tile =
      ReadBytes<std::uint32_t>(state.last_slide_tile_bytes);
  std::memcpy(&created.far_distance_squared, &state.reservedA0[kFarRangeOffset],
              sizeof created.far_distance_squared);
  created.max_range = state.maxRange;
  created.move_distance = state.moveDistance;
  created.body_height = state.bodyHeight;
  created.ground_clearance = ReadBytes<std::int16_t>(state.reservedBC);
  created.flight_height = state.flightHeight;
  created.patrol_direction = state.patrolDirection;
  created.flee_tile = state.flee_tile.word;
  created.observed_command_opcode = state.reserved124;
  created.locomotion_mode = state.locomotionMode;
  created.behavior_mode = state.behaviorMode;
  created.step_height = state.opaque13C;
  created.movement_flags = state.movementFlags;
  created.next_tile = state.nextTile.word;
  created.move_y = state.moveY;
  created.detour_tile = ReadBytes<std::uint32_t>(state.detour_tile_bytes);
  created.locomotion_flags = state.locomotionFlags;
  created.walk_step = state.walkStep;
  created.run_step = state.runStep;
  return created;
}

AiState& StateOf(int actor_id) {
  Require(actor_id >= 0 && actor_id < kCleanupActorSlots,
          "AI creation requires an actor ID in the AI state table");
  AiState* state = Battle_Ai_actorAiStates[static_cast<std::size_t>(actor_id)];
  Require(state != nullptr, "AI creation lost the actor's AI state");
  return *state;
}

class LiveCreationHost final : public ActorCreationHost {
 public:
  [[nodiscard]] std::uint32_t AttributeActorMask() const override {
    return std::bit_cast<std::uint32_t>(Battle_Ai_attributeActorMask);
  }
  void StoreAttributeActorMask(std::uint32_t mask) override {
    Battle_Ai_attributeActorMask = std::bit_cast<int>(mask);
  }
  [[nodiscard]] std::optional<RoomSpecialActors> SpecialActors()
      const override {
    if (Battle_Ai_roomAiState == nullptr) return std::nullopt;
    RoomSpecialActors slots{};
    for (std::size_t slot = 0; slot < kRoomSpecialActorSlots; ++slot) {
      slots.at(slot) = HandleOf(Battle_Ai_roomAiState->specialActors[slot]);
    }
    return slots;
  }
  void StoreSpecialActor(std::size_t slot, BattleActorHandle actor) override {
    Require(Battle_Ai_roomAiState != nullptr && slot < kRoomSpecialActorSlots,
            "AI special actors require the room's AI state");
    Battle_Ai_roomAiState->specialActors[slot] = &ActorOf(actor);
  }
  [[nodiscard]] std::span<const std::uint16_t> TileFlags() const override {
    if (Battle_Ai_tileFlagMap == nullptr) return {};
    return {&Battle_Ai_tileFlagMap[0][0], kTileFlagCells};
  }
  [[nodiscard]] std::optional<std::uint32_t> ModelObjectWord(
      BattleActorHandle actor) const override {
    const Battle_Actor& live = ActorOf(actor);
    if (live.model == nullptr) return std::nullopt;
    const std::span model_bytes{reinterpret_cast<const std::byte*>(live.model),
                                kModelObjectWordOffset + sizeof(std::uint32_t)};
    std::uint32_t word = 0;
    std::memcpy(&word, model_bytes.subspan(kModelObjectWordOffset).data(),
                sizeof word);
    return word;
  }
  [[nodiscard]] std::optional<CreationModel> Model(
      BattleActorHandle actor) const override {
    const Actors_ActorRecord* record = RecordOf(ActorOf(actor));
    if (record == nullptr) return std::nullopt;
    const Actors_ModelInstance& model = record->instance;
    return CreationModel{.motion_locked = model.motionLocked != 0,
                         .position_y = model.position.vy,
                         .tile = TileWord(model),
                         .radius = record->collisionRadius,
                         .body_height = record->collisionHeight};
  }
  [[nodiscard]] std::optional<CreationStats> Stats(
      BattleActorHandle actor) const override {
    const Battle_ActorStats* stats = ActorOf(actor).stats;
    if (stats == nullptr) return std::nullopt;
    return CreationStats{
        .movement_class = static_cast<std::uint8_t>(stats->movementClass),
        .walk_speed = static_cast<std::uint8_t>(stats->walkSpeed),
        .gait_class = static_cast<std::uint8_t>(stats->gaitClass),
        .run_speed = static_cast<std::uint8_t>(stats->runSpeed)};
  }
  [[nodiscard]] bool AllocateAiState(int actor_id,
                                     BattleActorHandle actor) override {
    Require(actor_id >= 0 && actor_id < kCleanupActorSlots,
            "AI creation requires an actor ID in the AI state table");
    Battle_Actor& live = ActorOf(actor);
    Require(live.model != nullptr, "AI creation requires the actor's model");
    auto* state = static_cast<AiState*>(
        Core::Memory::AllocateLiveFromLowEnd(sizeof(AiState)));
    if (state == nullptr) return false;
    Battle_Ai_actorAiStates[static_cast<std::size_t>(actor_id)] = state;
    Core::Memory::ClearLiveAlignedWords(
        state, static_cast<std::int32_t>(sizeof(AiState)));
    state->actorId = static_cast<std::uint8_t>(actor_id);
    // The AI state's actor and model links are views of the battle actor
    // and its actor record (AiState.h).
    state->actor = reinterpret_cast<Battle_Ai_EngagementData*>(&live);
    state->model = reinterpret_cast<Battle_Ai_AiModelView*>(live.model);
    state->stats = live.stats;
    state->nativePointer60 = &live.model->position;
    state->nativePointer64 = &live.model->currentTileX;
    Core_Debugf("ai", "created state=%p id=%u model=%p stats=%p",
                static_cast<void*>(state),
                static_cast<unsigned int>(state->actorId),
                static_cast<void*>(live.model), static_cast<void*>(live.stats));
    return true;
  }
  [[nodiscard]] std::optional<std::int16_t> AnchorHeightY(
      int actor_id) const override {
    SVECTOR anchor{};
    if (VS::Actors::LiveActorTaggedBonePosition(
            actor_id, VS::Actors::kAnchorHeightSelector, &anchor, 0) < 0) {
      return std::nullopt;
    }
    return anchor.vy;
  }
  [[nodiscard]] bool HasAuxiliaryBoneTag(int actor_id, int tag) const override {
    return VS::Actors::FindLiveActorBoneTag(actor_id, tag, kAuxiliaryTagKind) >=
           0;
  }
  void ClearActorCommand(BattleActorHandle actor) override {
    Battle_Actor& live = ActorOf(actor);
    live.commandOpcode = 0;
    live.commandFlags &= kKeptCommandFlags;
  }

  void ApplySettings(CreatedAiState& created, std::uint32_t settings) override {
    AiState& state = StoreState(created);
    Battle_Ai_SettingsWord word{};
    word.word = settings;
    // The settings view aliases the AI state it decodes into
    // (ActorSettingsLive.cpp).
    ApplyLiveActorAiSettings(reinterpret_cast<Battle_Ai_SettingsView*>(&state),
                             &word);
    created = Load(state);
  }
  void InitializeRelations(CreatedAiState& created) override {
    AiState& state = StoreState(created);
    InitializeLiveActorAiRelations(reinterpret_cast<CreationView*>(&state));
    created = Load(state);
  }
  void IssueCommand(CreatedAiState& created, CommandIntent intent) override {
    AiState& state = StoreState(created);
    IssueLiveActorCommand(&state, static_cast<int>(intent));
    created = Load(state);
  }
  void ApplySavedRelations(CreatedAiState& created) override {
    AiState& state = StoreState(created);
    ApplyLiveSavedActorAiRelations();
    created = Load(state);
  }
  [[nodiscard]] bool RebuildActionPlans(CreatedAiState& created) override {
    AiState& state = StoreState(created);
    const bool again = RebuildLiveActorActionPlans(nullptr) != 0;
    created = Load(state);
    return again;
  }

 private:
  static AiState& StoreState(const CreatedAiState& created) {
    AiState& state = StateOf(created.actor_id);
    Store(created, state);
    return state;
  }
};
}  // namespace

// decomp: func_800E6178 (src/BATTLE/BATTLE.PRG/7CD68.c:934).
int CreateLiveActorAiState(Battle_Actor* actor, int options) {
  Require(actor != nullptr, "AI creation requires an actor");
  Core_Debugf(
      "ai", "create actor=%p id=%d options=0x%X model=%p stats=%p",
      static_cast<void*>(actor), actor->id, static_cast<unsigned int>(options),
      static_cast<void*>(actor->model), static_cast<void*>(actor->stats));
  const CreationActor creation{
      .handle = HandleOf(actor),
      .id = actor->id,
      .kind = actor->kind,
      .combat_pose = actor->spawnByte26,
      .saved_settings_flags = static_cast<std::uint16_t>(
          std::bit_cast<std::uint32_t>(actor->reserved34)),
      .settings = std::bit_cast<std::uint32_t>(actor->templateWord2C),
      .allocation_kind = actor->allocationKind};
  LiveCreationHost host;
  const std::optional<bool> remembered_idle =
      CreateActorAiState(host, creation, static_cast<std::int16_t>(options));
  Require(remembered_idle.has_value(), "AI creation refused the actor");
  if ((creation.kind & kAiStateActorKinds) != 0) {
    const AiState& state = StateOf(creation.id);
    Core_Debugf("ai",
                "initialized actor=%d state=%p bytes=%u radius=%u height=%u",
                creation.id, static_cast<const void*>(&state),
                static_cast<unsigned int>(sizeof(AiState)),
                static_cast<unsigned int>(state.radius),
                static_cast<unsigned int>(
                    static_cast<std::uint16_t>(state.pointHeight)));
  }
  return *remembered_idle ? 1 : 0;
}
}  // namespace VS::Battle::Ai
