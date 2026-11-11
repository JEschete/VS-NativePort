// Hand-maintained (tools/port/owned.csv).
// Live binding for the actor cleanup (ActorCleanup.cpp).
#include "Battle/Ai/ActorCleanupLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>

#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Core/BiosRand.h"
#include "Core/Memory/HeapLive.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle::Ai {
namespace {
static_assert(std::size(Battle_Ai_actorAiStates) == kCleanupActorSlots);
static_assert(std::size(Battle_actors) >= kCleanupActorSlots);
static_assert(std::size(Actors_actorRecords) >= kCleanupActorSlots);
static_assert(std::extent_v<decltype(Battle_Ai_FrameContext::actors)> ==
              kCleanupActorSlots);
static_assert(
    std::extent_v<std::remove_pointer_t<decltype(Battle_Ai_tileFlagMap)>> ==
    kTileFlagGridSide);
static_assert(std::extent_v<decltype(Battle_Ai_RoomAiState::specialActors)> ==
              kRoomSpecialActorSlots);
static_assert(std::extent_v<decltype(Battle_Ai_ActorAiState::relations)> ==
              kCleanupActorSlots);
static_assert(sizeof(Battle_Ai_RelationWord) == sizeof(std::uint32_t));
static_assert(sizeof(Battle_Ai_TilePosition) == sizeof(std::uint32_t));
// Room bytes 0-3 hold the reservable tile range; byte 0x0E the room flags.
constexpr std::size_t kRoomMinTileZOffset = 0x03;
constexpr std::size_t kRoomFlagsOffset = 0x0E;
constexpr std::size_t kRoomEngagedCountOffset = 0x12;
static_assert(offsetof(Battle_Ai_RoomAiState, maxTileX) == 0);
static_assert(offsetof(Battle_Ai_RoomAiState, maxTileZ) == 1);
static_assert(offsetof(Battle_Ai_RoomAiState, minTileX) == 2);
static_assert(offsetof(Battle_Ai_RoomAiState, minTileZ) == kRoomMinTileZOffset);
static_assert(offsetof(Battle_Ai_RoomAiState, reservedEPrefix) ==
              kRoomFlagsOffset);
static_assert(offsetof(Battle_Ai_RoomAiState, engagedCount) ==
              kRoomEngagedCountOffset);
// The views callers hold alias the actor AI state.
static_assert(offsetof(Battle_Ai_PerceptionUpdateView, opaque18) ==
              offsetof(Battle_Ai_ActorAiState, opaque18));
static_assert(offsetof(Battle_Ai_ReactionView, relations) ==
              offsetof(Battle_Ai_ActorAiState, relations));
// The original allocates a 0x20-byte orbit state.
constexpr std::size_t kOrbitStateBytes = 0x20;
static_assert(sizeof(Battle_Ai_ArenaOrbitState) == kOrbitStateBytes);

constexpr auto kGridSide = static_cast<std::size_t>(kTileFlagGridSide);
constexpr std::size_t kTileFlagCells = kGridSide * kGridSide;
constexpr unsigned kLayerBits = 4;
constexpr unsigned kLocomotionShift = 3;

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

std::span<std::uint16_t> LiveTileFlags() {
  if (Battle_Ai_tileFlagMap == nullptr) return {};
  return {&Battle_Ai_tileFlagMap[0][0], kTileFlagCells};
}

Battle_Ai_ActorAiState* StateAt(int actor_id) {
  return Battle_Ai_actorAiStates[static_cast<std::size_t>(actor_id)];
}

BattleActorHandle HandleOf(const Battle_Actor* actor) {
  return static_cast<BattleActorHandle>(
      reinterpret_cast<std::uintptr_t>(actor));
}

// Only FreeLiveActorAiState hands the core a non-empty handle to store
// through, and it made that handle from a live actor.
Battle_Actor* ActorOf(BattleActorHandle actor) {
  Require(actor != kNoBattleActor,
          "AI teardown command flags require an actor");
  return reinterpret_cast<Battle_Actor*>(static_cast<std::uintptr_t>(actor));
}

ReservationState LoadReservations(const Battle_Ai_ActorAiState& state) {
  return {.navigation_reset = state.opaque1A,
          .next_tile = state.nextTile.word,
          .holds_route_gate = state.route_gate != 0};
}

CleanupRoom LoadRoom(const Battle_Ai_RoomAiState& room) {
  CleanupRoom loaded{.max_x = room.maxTileX,
                     .max_z = room.maxTileZ,
                     .min_x = room.minTileX,
                     .min_z = room.minTileZ,
                     .flags = room.reservedEPrefix[0],
                     .engaged_mask = EngagedMask(room),
                     .engaged_count = room.engagedCount};
  for (std::size_t slot = 0; slot < kRoomSpecialActorSlots; ++slot) {
    loaded.special_actors.at(slot) = HandleOf(room.specialActors[slot]);
  }
  return loaded;
}

void StoreRoomFields(const CleanupRoom& room, Battle_Ai_RoomAiState& target) {
  target.reservedEPrefix[0] = room.flags;
  StoreEngagedMask(target, room.engaged_mask);
  target.engagedCount = room.engaged_count;
  // Cleanup only ever empties a special actor slot.
  for (std::size_t slot = 0; slot < kRoomSpecialActorSlots; ++slot) {
    if (room.special_actors.at(slot) == kNoBattleActor) {
      target.specialActors[slot] = nullptr;
    }
  }
}

class LiveCleanupHost final : public ActorCleanupHost {
 public:
  [[nodiscard]] bool HasTileFlagMap() const override {
    return Battle_Ai_tileFlagMap != nullptr;
  }
  [[nodiscard]] std::span<std::uint16_t> TileFlags() override {
    return LiveTileFlags();
  }
  [[nodiscard]] bool HasAiState(int actor_id) const override {
    return StateAt(actor_id) != nullptr;
  }
  [[nodiscard]] std::optional<ReservationState> Reservations(
      int actor_id) const override {
    const Battle_Ai_ActorAiState* state = StateAt(actor_id);
    if (state == nullptr) return std::nullopt;
    return LoadReservations(*state);
  }
  void StoreReservations(int actor_id, const ReservationState& state) override {
    StateAt(actor_id)->opaque1A = state.navigation_reset;
  }
  [[nodiscard]] std::optional<CleanupRoom> Room() const override {
    if (Battle_Ai_roomAiState == nullptr) return std::nullopt;
    return LoadRoom(*Battle_Ai_roomAiState);
  }
  void StoreRoom(const CleanupRoom& room) override {
    StoreRoomFields(room, *Battle_Ai_roomAiState);
  }
  [[nodiscard]] std::optional<int> LinkedActor(int actor_id) const override {
    const Battle_Ai_ActorAiState* state = StateAt(actor_id);
    if (state == nullptr) return std::nullopt;
    return state->linkedActor;
  }
  void SetLinkedActor(int actor_id, int linked) override {
    StateAt(actor_id)->linkedActor = static_cast<signed char>(linked);
  }
  [[nodiscard]] std::optional<std::uint8_t> CommandOpcode(
      int actor_id) const override {
    const Battle_Actor* actor = Battle_actors[actor_id];
    if (actor == nullptr) return std::nullopt;
    return actor->commandOpcode;
  }
  [[nodiscard]] std::optional<ModelMotion> RecordMotion(
      int actor_id) const override {
    const Actors_ActorRecord* record =
        Actors_actorRecords[static_cast<std::size_t>(actor_id)];
    if (record == nullptr) return std::nullopt;
    // Model byte 0x0A packs the movement mode (bits 0-2) and locomotion
    // state (bits 3-4); byte 0x0B starts with the reaction state.
    const Actors_ModelInstance& model = record->instance;
    return ModelMotion{
        .movement = static_cast<std::uint8_t>(
            model.movementMode | (model.locomotionState << kLocomotionShift)),
        .reaction = model.reactionState};
  }
  [[nodiscard]] std::optional<std::uint8_t> StateModelSupport(
      int actor_id) const override {
    const Battle_Ai_ActorAiState* state = StateAt(actor_id);
    if (state == nullptr || state->model == nullptr) return std::nullopt;
    const Actors_ModelInstance& model =
        reinterpret_cast<const Actors_ActorRecord*>(state->model)->instance;
    return static_cast<std::uint8_t>(model.supportLayer |
                                     (model.placementLayer << kLayerBits));
  }
  [[nodiscard]] std::optional<PersistenceFields> Persistence(
      int actor_id) const override {
    const Battle_Ai_ActorAiState* state = StateAt(actor_id);
    if (state == nullptr) return std::nullopt;
    return PersistenceFields{
        .uses_room_action_delay = state->usesRoomActionDelay,
        .relation_to_ashley = std::bit_cast<std::uint32_t>(state->relations[0]),
        .behavior_mode = state->behaviorMode};
  }
  [[nodiscard]] std::uint32_t AttributeActorMask() const override {
    return static_cast<std::uint32_t>(Battle_Ai_attributeActorMask);
  }
  void StoreAttributeActorMask(std::uint32_t mask) override {
    Battle_Ai_attributeActorMask = static_cast<int>(mask);
  }
  [[nodiscard]] int BattleActorSlots() const override {
    return static_cast<int>(std::size(Battle_actors));
  }
  [[nodiscard]] bool HasNavigationOverride(int actor_id) const override {
    return StateAt(actor_id)->nativePointer18C != nullptr;
  }
  void StoreCommandFlags(BattleActorHandle actor, std::uint8_t flags) override {
    ActorOf(actor)->commandFlags = flags;
  }
  void FreeNavigationOverride(int actor_id) override {
    Battle_Ai_ActorAiState& state = *StateAt(actor_id);
    void* route = state.nativePointer18C;
    state.nativePointer18C = nullptr;
    VS::Core::Memory::FreeLiveFromHighEnd(route);
  }
  void ClearContactOccupancy(int contact) override {
    ClearLiveAiContactOccupancy(contact);
  }
  void Disengage(int actor_id) override {
    DisengageLiveActor(StateAt(actor_id));
  }
  void ResetAwarenessIndicator(int actor_id) override {
    ResetLiveActorAwarenessIndicator(
        reinterpret_cast<Battle_Ai_PlanEvaluationView*>(StateAt(actor_id)));
  }
  void UnlinkEngagedActor(int actor_id) override {
    UnlinkLiveEngagedActor(StateAt(actor_id));
  }
  void RetireAiState(int actor_id) override {
    const auto slot = static_cast<std::size_t>(actor_id);
    Battle_Ai_ActorAiState* state = Battle_Ai_actorAiStates[slot];
    Battle_Ai_actorAiStates[slot] = nullptr;
    Battle_Ai_FrameContext* frame = Battle_Ai_FrameState();
    Require(frame != nullptr, "AI teardown requires the frame state");
    frame->actors[slot] = nullptr;
    if (state != nullptr) VS::Core::Memory::FreeLiveFromLowEnd(state);
  }
  void FreeOrbitingBossState() override {
    Battle_Ai_ArenaOrbitState* orbit = Battle_Ai_arenaOrbit;
    Battle_Ai_arenaOrbit = nullptr;
    if (orbit != nullptr) VS::Core::Memory::FreeLiveFromLowEnd(orbit);
  }
  bool AllocateOrbitingBossState(std::int16_t cooldown) override {
    void* storage = VS::Core::Memory::AllocateLiveFromLowEnd(
        sizeof(Battle_Ai_ArenaOrbitState));
    if (storage == nullptr) return false;
    Battle_Ai_ArenaOrbitState* orbit =
        std::construct_at(static_cast<Battle_Ai_ArenaOrbitState*>(storage));
    orbit->cooldown = cooldown;
    Battle_Ai_arenaOrbit = orbit;
    return true;
  }
};

class LiveReactionDelay final : public ReactionDelayHost {
 public:
  explicit LiveReactionDelay(const Battle_Ai_ActorAiState& state)
      : state_(state) {}

  int NextRandomMasked(int range) override {
    return NextLiveActorRandomMasked(range);
  }
  // The original writes through the AI state's actor pointer (0x54), which
  // always names the battle actor in the state's slot.
  void SetCommandFlags(std::uint8_t flags) override {
    Require(state_.actorId < kCleanupActorSlots,
            "AI reaction request actor ID is outside the AI slots");
    Battle_Actor* actor = Battle_actors[state_.actorId];
    Require(actor != nullptr, "AI reaction request requires an actor");
    actor->commandFlags = flags;
  }

 private:
  const Battle_Ai_ActorAiState& state_;
};

HostilityState LoadHostility(const Battle_Ai_ActorAiState& source) {
  HostilityState state{.alerted = source.opaque18};
  for (std::size_t id = 0; id < state.relations.size(); ++id) {
    state.relations.at(id) = std::bit_cast<std::uint32_t>(source.relations[id]);
  }
  return state;
}

void StoreHostility(const HostilityState& state,
                    Battle_Ai_ActorAiState& target) {
  target.opaque18 = state.alerted;
  for (std::size_t id = 0; id < state.relations.size(); ++id) {
    target.relations[id] =
        std::bit_cast<Battle_Ai_RelationWord>(state.relations.at(id));
  }
}

class LiveHostility final : public HostilityHost {
 public:
  explicit LiveHostility(Battle_Ai_ActorAiState& state) : state_(state) {}

  void DrawBiosRandom() override { (void)Core_Random_Rand(); }
  void ShowAwarenessIndicator(HostilityState& state, int indicator,
                              int frames) override {
    StoreHostility(state, state_);
    SetLiveTemporaryAwarenessIndicator(&state_, indicator, frames);
    state = LoadHostility(state_);
  }
  void RequestReaction(HostilityState& state, int value, int range) override {
    StoreHostility(state, state_);
    SetLiveActorAiReactionWithDelay(
        reinterpret_cast<Battle_Ai_PerceptionUpdateView*>(&state_), value,
        range);
    state = LoadHostility(state_);
  }

 private:
  Battle_Ai_ActorAiState& state_;
};
}  // namespace

void ReleaseLiveAiDestinationTile(int x, int z) {
  Require(ReleaseAiDestinationTile(LiveTileFlags(), x, z),
          "AI reservation release tile is outside the tile grid");
}

void FreeLiveActorNavigationOverride(int actor_id) {
  LiveCleanupHost host;
  Require(FreeActorNavigationOverride(host, actor_id),
          "AI navigation override actor ID is outside the AI slots");
}

void ForceReleaseLiveActorAiReservations(int actor_id) {
  LiveCleanupHost host;
  Require(ForceReleaseActorAiReservations(host, actor_id),
          "AI reservation release requires an AI slot, the room state and a "
          "room range inside the tile grid");
}

void ReleaseLiveActorAiReservations(Battle_Ai_ActorAiState* state) {
  Require(state != nullptr && Battle_Ai_roomAiState != nullptr,
          "AI reservation release requires actor and room state");
  ReservationState reservations = LoadReservations(*state);
  CleanupRoom room = LoadRoom(*Battle_Ai_roomAiState);
  Require(ReleaseActorAiReservations(reservations, room, LiveTileFlags()),
          "AI reservation release requires a room range inside the tile "
          "grid");
  state->opaque1A = reservations.navigation_reset;
  StoreRoomFields(room, *Battle_Ai_roomAiState);
}

void FreeLiveActorAiState(Battle_Actor* actor) {
  Require(actor != nullptr, "AI teardown requires an actor");
  TeardownActor teardown{.handle = HandleOf(actor),
                         .id = actor->id,
                         .command_flags = actor->commandFlags,
                         .sub_type = actor->subType,
                         .kind = static_cast<std::uint8_t>(actor->kind)};
  LiveCleanupHost host;
  Require(FreeActorAiState(host, teardown),
          "AI teardown requires an actor in the AI slots, the room state and "
          "consistent engagement");
}

void AllocateLiveOrbitingBossState(Battle_Ai_SettingsView* /*state*/) {
  LiveCleanupHost host;
  Require(AllocateOrbitingBossState(host), "AI orbit-state allocation failed");
}

void SetLiveActorAiReactionWithDelay(Battle_Ai_PerceptionUpdateView* state,
                                     int value, int range) {
  Require(state != nullptr, "AI reaction request requires state");
  auto& live = *reinterpret_cast<Battle_Ai_ActorAiState*>(state);
  ReactionDelayState delay{
      .has_navigation_override = live.nativePointer18C != nullptr,
      .decision_delay = live.decisionDelay};
  LiveReactionDelay host(live);
  if (SetActorAiReactionWithDelay(delay, host, value, range)) {
    live.decisionDelay = delay.decision_delay;
  }
}

void MakeLiveTargetHostile(Battle_Ai_ReactionView* state, int target) {
  Require(state != nullptr, "AI hostility request requires state");
  auto& live = *reinterpret_cast<Battle_Ai_ActorAiState*>(state);
  HostilityState hostility = LoadHostility(live);
  LiveHostility host(live);
  Require(MakeTargetHostile(hostility, host, target),
          "AI hostility target is outside the AI slots");
  StoreHostility(hostility, live);
}

void RefreshLiveActorAiReservations(int actor_id) {
  LiveCleanupHost host;
  Require(RefreshActorAiReservations(host, actor_id),
          "AI reservation refresh requires an AI slot, the actor's record "
          "and the room state");
}

void ReleaseLiveActorAiContactOccupancy(int actor_id) {
  LiveCleanupHost host;
  Require(ReleaseActorAiContactOccupancy(host, actor_id),
          "AI contact release requires an AI slot with both a battle actor "
          "and an AI state with a model");
}

void ClearLiveActorAiLinksAndPlan(int actor_id) {
  LiveCleanupHost host;
  Require(ClearActorAiLinksAndPlan(host, actor_id),
          "AI link clearing actor ID is outside the AI slots");
}

std::int16_t PackLiveActorAiPersistenceFlags(int actor_id) {
  const LiveCleanupHost host;
  const std::optional<std::int16_t> flags =
      PackActorAiPersistenceFlags(host, actor_id);
  Require(flags.has_value(), "AI persistence actor ID is outside the AI slots");
  return *flags;
}

void ClearLiveActorAiNavigationReset(int actor_id) {
  LiveCleanupHost host;
  Require(ClearActorAiNavigationReset(host, actor_id),
          "AI navigation reset actor ID is outside the AI slots");
}

void InitializeLiveActorAiExtensionHook() { InitializeActorAiExtensionHook(); }
}  // namespace VS::Battle::Ai
