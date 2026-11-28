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

#include "Actors/ActorInstances.hpp"
#include "Actors/ActorRecord.hpp"
#include "Actors/ActorSpawningLive.hpp"
#include "Actors/ModelInstance.hpp"
#include "Actors/Skeleton.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/ActorSettingsLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/PlanningWorkspaceLive.hpp"
#include "Battle/Ai/RelationsLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Field/Gimmicks.hpp"

using VS::Core::Gte::SVECTOR;

namespace VS::Battle::Ai {
namespace {
using AiState = VS::Battle::Ai::ActorAiState;
using CreationView = VS::Battle::Ai::AiCreationView;
using ActorView = VS::Battle::Ai::AiActorView;
using ModelView = VS::Battle::Ai::CreationModelView;
using StatsView = VS::Battle::Ai::CreationStatsView;
using BoneTagKind = VS::Actors::BoneTagKind;

// The far perception range (AI state 0xAC) within reservedA0 (0xA0).
constexpr std::size_t kFarRangeOffset = 12;
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

static_assert(std::size(VS::Battle::Ai::actorAiStates) == kCleanupActorSlots);
static_assert(
    std::tuple_size_v<decltype(VS::Battle::Ai::RoomAiState::special_actors)> ==
    kRoomSpecialActorSlots);
static_assert(
    std::extent_v<
        std::remove_pointer_t<decltype(VS::Battle::Ai::tileFlagMap)>> ==
    kTileFlagGridSide);

// The original read the battle actor through the creation actor view.
static_assert(offsetof(ActorView, actor_id) == offsetof(VS::Battle::Actor, id));
static_assert(offsetof(ActorView, state_flags) + kCommandOpcodeByte ==
              offsetof(VS::Battle::Actor, command_opcode));
static_assert(offsetof(ActorView, state_flags) + kCommandFlagsByte ==
              offsetof(VS::Battle::Actor, command_flags));
static_assert(offsetof(ActorView, ai_control_flags) ==
              offsetof(VS::Battle::Actor, kind));
static_assert(offsetof(ActorView, start_command_flag) ==
              offsetof(VS::Battle::Actor, combat_pose));
static_assert(offsetof(ActorView, saved_ai_settings_flags) ==
              offsetof(VS::Battle::Actor, spawn_ai_settings));
static_assert(offsetof(ActorView, ai_settings) ==
              offsetof(VS::Battle::Actor, ai_settings));
static_assert(offsetof(ActorView, stats) == offsetof(VS::Battle::Actor, stats));
static_assert(offsetof(ActorView, list_exempt_flag) ==
              offsetof(VS::Battle::Actor, allocation_kind));
static_assert(offsetof(ActorView, model) == offsetof(VS::Battle::Actor, model));

// ...its model through the creation model view...
static_assert(offsetof(VS::Actors::ActorRecord, instance) == 0);
static_assert(offsetof(ModelView, position) ==
              offsetof(VS::Actors::ModelInstance, position));
static_assert(offsetof(ModelView, tile) ==
              offsetof(VS::Actors::ModelInstance, current_tile_x));
static_assert(offsetof(ModelView, kind) ==
              offsetof(Field::GimmickInstance, object_info));
static_assert(offsetof(ModelView, radius) ==
              offsetof(VS::Actors::ActorRecord, collision_radius));
static_assert(offsetof(ModelView, opaque_63e) ==
              offsetof(VS::Actors::ActorRecord, collision_height));

// ...its stats through the creation stats view...
static_assert(offsetof(StatsView, opaque30) ==
              offsetof(VS::Battle::ActorStats, movement_class));
static_assert(offsetof(StatsView, opaque31) ==
              offsetof(VS::Battle::ActorStats, walk_speed));
static_assert(offsetof(StatsView, opaque32) ==
              offsetof(VS::Battle::ActorStats, gait_class));
static_assert(offsetof(StatsView, opaque33) ==
              offsetof(VS::Battle::ActorStats, run_speed));

// ...and wrote the AI state through the creation view.
static_assert(offsetof(CreationView, has_tagged_bone3) ==
              offsetof(AiState, has_tagged_bone3));
static_assert(offsetof(CreationView, tile) == offsetof(AiState, tile));
static_assert(offsetof(CreationView, previous_tile) ==
              offsetof(AiState, previous_tile));
static_assert(offsetof(CreationView, options) ==
              offsetof(AiState, saved_relation_options));
static_assert(offsetof(CreationView, actor) == offsetof(AiState, actor));
static_assert(offsetof(CreationView, model) == offsetof(AiState, model));
static_assert(offsetof(CreationView, stats) == offsetof(AiState, stats));
static_assert(offsetof(CreationView, position_source) ==
              offsetof(AiState, native_pointer60));
static_assert(offsetof(CreationView, tile_source) ==
              offsetof(AiState, native_pointer64));
static_assert(offsetof(CreationView, point_height) ==
              offsetof(AiState, point_height));
static_assert(offsetof(CreationView, actor_id) == offsetof(AiState, actor_id));
static_assert(offsetof(CreationView, opaque89) ==
              offsetof(AiState, requested_movement_mode));
static_assert(offsetof(CreationView, opaque_8a) ==
              offsetof(AiState, current_movement_mode));
static_assert(offsetof(CreationView, radius) == offsetof(AiState, radius));
static_assert(offsetof(CreationView, collision_radius) ==
              offsetof(AiState, collision_radius));
static_assert(offsetof(CreationView, opaque_9c) ==
              offsetof(AiState, last_slide_tile_bytes));
static_assert(offsetof(CreationView, far_distance_squared) ==
              offsetof(AiState, reserved_a0) + kFarRangeOffset);
static_assert(offsetof(VS::Battle::Ai::SettingsView, far_distance_squared) ==
              offsetof(CreationView, far_distance_squared));
static_assert(offsetof(CreationView, max_range) ==
              offsetof(AiState, max_range));
static_assert(offsetof(CreationView, move_distance) ==
              offsetof(AiState, move_distance));
static_assert(offsetof(CreationView, body_height) ==
              offsetof(AiState, body_height));
static_assert(offsetof(CreationView, ground_clearance) ==
              offsetof(AiState, ground_clearance));
static_assert(offsetof(CreationView, flight_height) ==
              offsetof(AiState, flight_height));
static_assert(offsetof(CreationView, opaque103) ==
              offsetof(AiState, patrol_direction));
static_assert(offsetof(CreationView, opaque110) ==
              offsetof(AiState, flee_tile));
static_assert(offsetof(CreationView, opaque124) ==
              offsetof(AiState, reserved124));
static_assert(offsetof(CreationView, behavior_mode) ==
              offsetof(AiState, behavior_mode));
static_assert(offsetof(CreationView, step_height) ==
              offsetof(AiState, default_turn_step));
static_assert(offsetof(CreationView, movement_flags) ==
              offsetof(AiState, movement_flags));
static_assert(offsetof(CreationView, next_tile) ==
              offsetof(AiState, next_tile));
static_assert(offsetof(CreationView, move_y) == offsetof(AiState, move_y));
static_assert(offsetof(CreationView, opaque194) ==
              offsetof(AiState, detour_tile_bytes));
static_assert(offsetof(CreationView, opaque224) ==
              offsetof(AiState, locomotion_flags));
static_assert(offsetof(CreationView, opaque226) ==
              offsetof(AiState, walk_step));
static_assert(offsetof(CreationView, opaque227) == offsetof(AiState, run_step));

VS::Battle::Actor& ActorOf(BattleActorHandle actor) {
  auto* live = Actors::ResolveLiveActorInstance(actor);
  Require(live != nullptr, "AI creation requires a current actor identity");
  return *live;
}

Actors::ActorRecord* RecordOf(BattleActorHandle actor) {
  const auto components = Actors::LiveActorComponents(actor);
  return components ? components->body : nullptr;
}

std::uint32_t TileWord(const VS::Actors::ModelInstance& model) {
  return model.current_tile_x |
         (static_cast<std::uint32_t>(model.current_tile_layer)
          << kTileLayerShift) |
         (static_cast<std::uint32_t>(model.current_tile_z) << kTileZShift) |
         (static_cast<std::uint32_t>(model.current_tile_spare)
          << kTileStatusShift);
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
  state.has_tagged_bone3 = created.has_auxiliary_tag_bone ? 1 : 0;
  state.tile.word = created.tile;
  state.previous_tile = created.previous_tile;
  state.saved_relation_options = created.saved_relation_options;
  state.point_height = created.point_height;
  state.actor_id = created.actor_id;
  state.requested_movement_mode = created.requested_movement_mode;
  state.current_movement_mode = created.current_movement_mode;
  state.radius = created.radius;
  state.collision_radius = created.collision_radius;
  WriteBytes(state.last_slide_tile_bytes, created.last_slide_tile);
  std::memcpy(&state.reserved_a0[kFarRangeOffset],
              &created.far_distance_squared,
              sizeof created.far_distance_squared);
  state.max_range = created.max_range;
  state.move_distance = created.move_distance;
  state.body_height = created.body_height;
  state.ground_clearance = created.ground_clearance;
  state.flight_height = created.flight_height;
  state.patrol_direction = created.patrol_direction;
  state.flee_tile.word = created.flee_tile;
  state.reserved124 = created.observed_command_opcode;
  state.locomotion_mode = created.locomotion_mode;
  state.behavior_mode = created.behavior_mode;
  state.default_turn_step = created.step_height;
  state.movement_flags = created.movement_flags;
  state.next_tile.word = created.next_tile;
  state.move_y = created.move_y;
  WriteBytes(state.detour_tile_bytes, created.detour_tile);
  state.locomotion_flags = created.locomotion_flags;
  state.walk_step = created.walk_step;
  state.run_step = created.run_step;
}

CreatedAiState Load(const AiState& state) {
  CreatedAiState created;
  created.has_auxiliary_tag_bone = state.has_tagged_bone3 != 0;
  created.tile = state.tile.word;
  created.previous_tile = state.previous_tile;
  created.saved_relation_options = state.saved_relation_options;
  created.point_height = state.point_height;
  created.actor_id = state.actor_id;
  created.requested_movement_mode = state.requested_movement_mode;
  created.current_movement_mode = state.current_movement_mode;
  created.radius = state.radius;
  created.collision_radius = state.collision_radius;
  created.last_slide_tile =
      ReadBytes<std::uint32_t>(state.last_slide_tile_bytes);
  std::memcpy(&created.far_distance_squared,
              &state.reserved_a0[kFarRangeOffset],
              sizeof created.far_distance_squared);
  created.max_range = state.max_range;
  created.move_distance = state.move_distance;
  created.body_height = state.body_height;
  created.ground_clearance = state.ground_clearance;
  created.flight_height = state.flight_height;
  created.patrol_direction = state.patrol_direction;
  created.flee_tile = state.flee_tile.word;
  created.observed_command_opcode = state.reserved124;
  created.locomotion_mode = state.locomotion_mode;
  created.behavior_mode = state.behavior_mode;
  created.step_height = state.default_turn_step;
  created.movement_flags = state.movement_flags;
  created.next_tile = state.next_tile.word;
  created.move_y = state.move_y;
  created.detour_tile = ReadBytes<std::uint32_t>(state.detour_tile_bytes);
  created.locomotion_flags = state.locomotion_flags;
  created.walk_step = state.walk_step;
  created.run_step = state.run_step;
  return created;
}

AiState& StateOf(Actors::SpawnedActor actor) {
  AiState* state = Actors::LiveActorAiState(actor);
  Require(state != nullptr, "AI creation lost the actor's AI state");
  return *state;
}

class LiveCreationHost final : public ActorCreationHost {
 public:
  explicit LiveCreationHost(Actors::SpawnedActor owner) : owner_(owner) {}

  [[nodiscard]] std::uint32_t AttributeActorMask() const override {
    return std::bit_cast<std::uint32_t>(VS::Battle::Ai::attributeActorMask);
  }
  void StoreAttributeActorMask(std::uint32_t mask) override {
    VS::Battle::Ai::attributeActorMask = std::bit_cast<int>(mask);
  }
  [[nodiscard]] std::optional<RoomSpecialActors> SpecialActors()
      const override {
    if (VS::Battle::Ai::roomAiState == nullptr) return std::nullopt;
    RoomSpecialActors slots = VS::Battle::Ai::roomAiState->special_actors;
    for (auto& actor : slots) {
      if (Actors::ResolveLiveActorInstance(actor) == nullptr) {
        actor = kNoBattleActor;
      }
    }
    return slots;
  }
  void StoreSpecialActor(std::size_t slot, BattleActorHandle actor) override {
    Require(
        VS::Battle::Ai::roomAiState != nullptr && slot < kRoomSpecialActorSlots,
        "AI special actors require the room's AI state");
    VS::Battle::Ai::roomAiState->special_actors[slot] = actor;
  }
  [[nodiscard]] std::span<const std::uint16_t> TileFlags() const override {
    if (VS::Battle::Ai::tileFlagMap == nullptr) return {};
    return {&VS::Battle::Ai::tileFlagMap[0][0], kTileFlagCells};
  }
  [[nodiscard]] std::optional<std::uint32_t> ModelObjectWord(
      BattleActorHandle actor) const override {
    const auto components = Actors::LiveActorComponents(actor);
    if (!components || components->gimmick == nullptr) {
      return std::nullopt;
    }
    std::uint32_t word = 0;
    std::memcpy(&word, &components->gimmick->object_info, sizeof word);
    return word;
  }
  [[nodiscard]] std::optional<CreationModel> Model(
      BattleActorHandle actor) const override {
    const VS::Actors::ActorRecord* record = RecordOf(actor);
    if (record == nullptr) return std::nullopt;
    const VS::Actors::ModelInstance& model = record->instance;
    return CreationModel{.motion_locked = model.motion_locked != 0,
                         .position_y = model.position.vy,
                         .tile = TileWord(model),
                         .radius = record->collision_radius,
                         .body_height = record->collision_height};
  }
  [[nodiscard]] std::optional<CreationStats> Stats(
      BattleActorHandle actor) const override {
    const auto components = Actors::LiveActorComponents(actor);
    if (!components || components->stats == nullptr) return std::nullopt;
    const auto* stats = components->stats;
    return CreationStats{
        .movement_class = static_cast<std::uint8_t>(stats->movement_class),
        .walk_speed = static_cast<std::uint8_t>(stats->walk_speed),
        .gait_class = static_cast<std::uint8_t>(stats->gait_class),
        .run_speed = static_cast<std::uint8_t>(stats->run_speed)};
  }
  [[nodiscard]] bool AllocateAiState(int actor_id,
                                     BattleActorHandle actor) override {
    Require(actor_id >= 0 && actor_id < kCleanupActorSlots,
            "AI creation requires an actor ID in the AI state table");
    VS::Battle::Actor& live = ActorOf(actor);
    auto* record = RecordOf(actor);
    Require(record != nullptr, "AI creation requires the actor's owned model");
    const auto components = Actors::LiveActorComponents(actor);
    Require(components.has_value(), "AI creation requires actor components");
    if (!components) return false;
    if (VS::Battle::Ai::actorAiStates[static_cast<std::size_t>(actor_id)] !=
            nullptr ||
        Actors::LiveActorAiState(actor) != nullptr) {
      Core::Warnf("ai", "AI creation refused an occupied state slot %d",
                  actor_id);
      return false;
    }
    auto* state = static_cast<AiState*>(
        Core::Memory::AllocateLiveFromLowEnd(sizeof(AiState)));
    if (state == nullptr) return false;
    if (!Actors::BindLiveActorAiState(actor, state)) {
      Core::Memory::FreeLiveFromLowEnd(state);
      return false;
    }
    VS::Battle::Ai::actorAiStates[static_cast<std::size_t>(actor_id)] = state;
    Core::Memory::ClearLiveAlignedWords(
        state, static_cast<std::int32_t>(sizeof(AiState)));
    state->actor_id = static_cast<std::uint8_t>(actor_id);
    // The AI state's actor and model links are views of the battle actor
    // and its actor record (AiState.hpp).
    state->actor = reinterpret_cast<VS::Battle::Ai::EngagementData*>(&live);
    state->model =
        reinterpret_cast<VS::Battle::Ai::ModelStatePrefix*>(&record->instance);
    state->stats = components->stats;
    state->native_pointer60 = &record->instance.position;
    state->native_pointer64 = &record->instance.current_tile_x;
    VS::Core::Debugf("ai", "created state=%p id=%u model=%p stats=%p",
                     static_cast<void*>(state),
                     static_cast<unsigned int>(state->actor_id),
                     static_cast<void*>(&record->instance),
                     static_cast<void*>(components->stats));
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
    VS::Battle::Actor& live = ActorOf(actor);
    live.command_opcode = 0;
    live.command_flags &= kKeptCommandFlags;
  }

  void ApplySettings(CreatedAiState& created, std::uint32_t settings) override {
    AiState& state = StoreState(created);
    VS::Battle::Ai::SettingsWord word{};
    word.word = settings;
    // The settings view aliases the AI state it decodes into
    // (ActorSettingsLive.cpp).
    ApplyLiveActorAiSettings(
        reinterpret_cast<VS::Battle::Ai::SettingsView*>(&state), &word);
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
  AiState& StoreState(const CreatedAiState& created) {
    AiState& state = StateOf(owner_);
    Store(created, state);
    return state;
  }
  Actors::SpawnedActor owner_;
};
}  // namespace

// decomp: func_800E6178 (src/BATTLE/BATTLE.PRG/7CD68.c:342).
int CreateLiveActorAiState(VS::Battle::Actor* actor, int options) {
  Require(actor != nullptr, "AI creation requires an actor");
  return CreateLiveActorInstanceAi(Actors::LiveActorIdentity(actor), options);
}

int CreateLiveActorInstanceAi(Actors::SpawnedActor identity, int options) {
  auto* actor = Actors::ResolveLiveActorInstance(identity);
  const auto slot = Actors::LiveActorSlot(identity);
  Require(actor != nullptr && slot.has_value(),
          "AI creation requires a registered actor");
  if (!slot) return 0;
  VS::Core::Debugf(
      "ai", "create actor=%p id=%d options=0x%X model=%p stats=%p",
      static_cast<void*>(actor), actor->id, static_cast<unsigned int>(options),
      static_cast<void*>(actor->model), static_cast<void*>(actor->stats));
  const CreationActor creation{
      .handle = identity,
      .id = static_cast<int>(*slot),
      .kind = actor->kind,
      .combat_pose = actor->combat_pose,
      .saved_settings_flags = static_cast<std::uint16_t>(
          std::bit_cast<std::uint32_t>(actor->spawn_ai_settings)),
      .settings = std::bit_cast<std::uint32_t>(actor->ai_settings),
      .shape = VS::Actors::LiveActorShape(actor)};
  LiveCreationHost host(identity);
  const std::optional<bool> remembered_idle =
      CreateActorAiState(host, creation, static_cast<std::int16_t>(options));
  Require(remembered_idle.has_value(), "AI creation refused the actor");
  if ((creation.kind & kAiStateActorKinds) != 0) {
    const AiState& state = StateOf(identity);
    VS::Core::Debugf(
        "ai", "initialized actor=%d state=%p bytes=%u radius=%u height=%u",
        creation.id, static_cast<const void*>(&state),
        static_cast<unsigned int>(sizeof(AiState)),
        static_cast<unsigned int>(state.radius),
        static_cast<unsigned int>(
            static_cast<std::uint16_t>(state.point_height)));
  }
  return *remembered_idle ? 1 : 0;
}
}  // namespace VS::Battle::Ai
