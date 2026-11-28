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

#include "Actors/ActorRecord.hpp"
#include "Actors/ActorSpawningLive.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/OrbitingBoss.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Battle/Damage.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"

namespace VS::Battle::Ai {
namespace {
static_assert(std::size(VS::Battle::Ai::actorAiStates) == kCleanupActorSlots);
static_assert(std::size(VS::Battle::actors) >= kCleanupActorSlots);
static_assert(std::size(VS::Actors::actorRecords) >= kCleanupActorSlots);
static_assert(std::extent_v<decltype(FrameContext::actors)> ==
              kCleanupActorSlots);
static_assert(
    std::extent_v<
        std::remove_pointer_t<decltype(VS::Battle::Ai::tileFlagMap)>> ==
    kTileFlagGridSide);
static_assert(
    std::tuple_size_v<decltype(VS::Battle::Ai::RoomAiState::special_actors)> ==
    kRoomSpecialActorSlots);
static_assert(
    std::tuple_size_v<decltype(VS::Battle::Ai::ActorAiState::relations)> ==
    kCleanupActorSlots);
static_assert(sizeof(VS::Battle::Ai::RelationWord) == sizeof(std::uint32_t));
static_assert(sizeof(VS::Battle::Ai::TilePosition) == sizeof(std::uint32_t));
// Room bytes 0-3 hold the reservable tile range; byte 0x0E the room flags.
constexpr std::size_t kRoomMinTileZOffset = 0x03;
constexpr std::size_t kRoomFlagsOffset = 0x0E;
constexpr std::size_t kRoomEngagedCountOffset = 0x12;
static_assert(offsetof(VS::Battle::Ai::RoomAiState, max_tile_x) == 0);
static_assert(offsetof(VS::Battle::Ai::RoomAiState, max_tile_z) == 1);
static_assert(offsetof(VS::Battle::Ai::RoomAiState, min_tile_x) == 2);
static_assert(offsetof(VS::Battle::Ai::RoomAiState, min_tile_z) ==
              kRoomMinTileZOffset);
static_assert(offsetof(VS::Battle::Ai::RoomAiState, reserved_e_prefix) ==
              kRoomFlagsOffset);
static_assert(offsetof(VS::Battle::Ai::RoomAiState, engaged_count) ==
              kRoomEngagedCountOffset);
// The views callers hold alias the actor AI state.
static_assert(offsetof(VS::Battle::Ai::PerceptionUpdateView, alerted) ==
              offsetof(VS::Battle::Ai::ActorAiState, opaque18));
static_assert(offsetof(VS::Battle::Ai::ReactionView, relations) ==
              offsetof(VS::Battle::Ai::ActorAiState, relations));
// The original allocates a 0x20-byte orbit state.
constexpr std::size_t kOrbitStateBytes = 0x20;
static_assert(sizeof(VS::Battle::Ai::ArenaOrbitState) == kOrbitStateBytes);

constexpr auto kGridSide = static_cast<std::size_t>(kTileFlagGridSide);
constexpr std::size_t kTileFlagCells = kGridSide * kGridSide;
constexpr unsigned kLayerBits = 4;
constexpr unsigned kLocomotionShift = 3;

std::span<std::uint16_t> LiveTileFlags() {
  if (VS::Battle::Ai::tileFlagMap == nullptr) return {};
  return {&VS::Battle::Ai::tileFlagMap[0][0], kTileFlagCells};
}

VS::Battle::Ai::ActorAiState* StateAt(int actor_id) {
  return VS::Battle::Ai::actorAiStates[static_cast<std::size_t>(actor_id)];
}

VS::Battle::Actor* ActorOf(BattleActorHandle actor) {
  auto* live = Actors::ResolveLiveActorInstance(actor);
  Require(live != nullptr,
          "AI teardown command flags require a current actor identity");
  return live;
}

ReservationState LoadReservations(const VS::Battle::Ai::ActorAiState& state) {
  return {.navigation_reset = state.on_moving_platform,
          .next_tile = state.next_tile.word,
          .holds_route_gate = state.route_gate != 0};
}

CleanupRoom LoadRoom(const VS::Battle::Ai::RoomAiState& room) {
  CleanupRoom loaded{.max_x = room.max_tile_x,
                     .max_z = room.max_tile_z,
                     .min_x = room.min_tile_x,
                     .min_z = room.min_tile_z,
                     .flags = room.reserved_e_prefix[0],
                     .engaged_mask = EngagedMask(room),
                     .engaged_count = room.engaged_count,
                     .special_actors = room.special_actors};
  return loaded;
}

void StoreRoomFields(const CleanupRoom& room,
                     VS::Battle::Ai::RoomAiState& target) {
  target.reserved_e_prefix[0] = room.flags;
  StoreEngagedMask(target, room.engaged_mask);
  target.engaged_count = room.engaged_count;
  // Cleanup only ever empties a special actor slot.
  for (std::size_t slot = 0; slot < kRoomSpecialActorSlots; ++slot) {
    if (room.special_actors.at(slot) == kNoBattleActor) {
      target.special_actors[slot] = kNoBattleActor;
    }
  }
}

class LiveCleanupHost final : public ActorCleanupHost {
 public:
  [[nodiscard]] bool HasTileFlagMap() const override {
    return VS::Battle::Ai::tileFlagMap != nullptr;
  }
  [[nodiscard]] std::span<std::uint16_t> TileFlags() override {
    return LiveTileFlags();
  }
  [[nodiscard]] bool HasAiState(int actor_id) const override {
    return StateAt(actor_id) != nullptr;
  }
  [[nodiscard]] std::optional<ReservationState> Reservations(
      int actor_id) const override {
    const VS::Battle::Ai::ActorAiState* state = StateAt(actor_id);
    if (state == nullptr) return std::nullopt;
    return LoadReservations(*state);
  }
  void StoreReservations(int actor_id, const ReservationState& state) override {
    StateAt(actor_id)->on_moving_platform = state.navigation_reset;
  }
  [[nodiscard]] std::optional<CleanupRoom> Room() const override {
    if (VS::Battle::Ai::roomAiState == nullptr) return std::nullopt;
    return LoadRoom(*VS::Battle::Ai::roomAiState);
  }
  void StoreRoom(const CleanupRoom& room) override {
    StoreRoomFields(room, *VS::Battle::Ai::roomAiState);
  }
  [[nodiscard]] std::optional<int> LinkedActor(int actor_id) const override {
    const VS::Battle::Ai::ActorAiState* state = StateAt(actor_id);
    if (state == nullptr) return std::nullopt;
    return state->linked_actor;
  }
  void SetLinkedActor(int actor_id, int linked) override {
    StateAt(actor_id)->linked_actor = static_cast<signed char>(linked);
  }
  [[nodiscard]] std::optional<std::uint8_t> CommandOpcode(
      int actor_id) const override {
    const VS::Battle::Actor* actor = VS::Battle::actors[actor_id];
    if (actor == nullptr) return std::nullopt;
    return actor->command_opcode;
  }
  [[nodiscard]] std::optional<ModelMotion> RecordMotion(
      int actor_id) const override {
    const VS::Actors::ActorRecord* record =
        VS::Actors::actorRecords[static_cast<std::size_t>(actor_id)];
    if (record == nullptr) return std::nullopt;
    // Model byte 0x0A packs the movement mode (bits 0-2) and locomotion
    // state (bits 3-4); byte 0x0B starts with the reaction state.
    const VS::Actors::ModelInstance& model = record->instance;
    return ModelMotion{
        .movement = static_cast<std::uint8_t>(
            model.movement_mode | (model.locomotion_state << kLocomotionShift)),
        .reaction = model.reaction_state};
  }
  [[nodiscard]] std::optional<std::uint8_t> StateModelSupport(
      int actor_id) const override {
    const VS::Battle::Ai::ActorAiState* state = StateAt(actor_id);
    if (state == nullptr || state->model == nullptr) return std::nullopt;
    const VS::Actors::ModelInstance& model =
        reinterpret_cast<const VS::Actors::ActorRecord*>(state->model)
            ->instance;
    return static_cast<std::uint8_t>(model.support_layer |
                                     (model.placement_layer << kLayerBits));
  }
  [[nodiscard]] std::optional<PersistenceFields> Persistence(
      int actor_id) const override {
    const VS::Battle::Ai::ActorAiState* state = StateAt(actor_id);
    if (state == nullptr) return std::nullopt;
    return PersistenceFields{
        .uses_room_action_delay = state->temporary_ashley_alliance,
        .relation_to_ashley = std::bit_cast<std::uint32_t>(state->relations[0]),
        .behavior_mode = state->behavior_mode};
  }
  [[nodiscard]] std::uint32_t AttributeActorMask() const override {
    return static_cast<std::uint32_t>(VS::Battle::Ai::attributeActorMask);
  }
  void StoreAttributeActorMask(std::uint32_t mask) override {
    VS::Battle::Ai::attributeActorMask = static_cast<int>(mask);
  }
  [[nodiscard]] int BattleActorSlots() const override {
    return static_cast<int>(std::size(VS::Battle::actors));
  }
  [[nodiscard]] bool HasNavigationOverride(int actor_id) const override {
    return StateAt(actor_id)->native_pointer_18c != nullptr;
  }
  void StoreCommandFlags(BattleActorHandle actor, std::uint8_t flags) override {
    ActorOf(actor)->command_flags = flags;
  }
  void FreeNavigationOverride(int actor_id) override {
    VS::Battle::Ai::ActorAiState& state = *StateAt(actor_id);
    void* route = state.native_pointer_18c;
    state.native_pointer_18c = nullptr;
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
        reinterpret_cast<VS::Battle::Ai::PlanEvaluationView*>(
            StateAt(actor_id)));
  }
  void UnlinkEngagedActor(int actor_id) override {
    UnlinkLiveEngagedActor(StateAt(actor_id));
  }
  void RetireAiState(int actor_id) override {
    const auto slot = static_cast<std::size_t>(actor_id);
    VS::Battle::Ai::ActorAiState* state = VS::Battle::Ai::actorAiStates[slot];
    VS::Battle::Ai::actorAiStates[slot] = nullptr;
    FrameContext* frame = &LiveFrameContext();
    Require(frame != nullptr, "AI teardown requires the frame state");
    frame->actors[slot] = nullptr;
    if (state != nullptr) VS::Core::Memory::FreeLiveFromLowEnd(state);
  }
  void FreeOrbitingBossState() override {
    VS::Battle::Ai::ArenaOrbitState* orbit = VS::Battle::Ai::arenaOrbit;
    VS::Battle::Ai::arenaOrbit = nullptr;
    if (orbit != nullptr) VS::Core::Memory::FreeLiveFromLowEnd(orbit);
  }
  bool AllocateOrbitingBossState(std::int16_t cooldown) override {
    void* storage = VS::Core::Memory::AllocateLiveFromLowEnd(
        sizeof(VS::Battle::Ai::ArenaOrbitState));
    if (storage == nullptr) return false;
    VS::Battle::Ai::ArenaOrbitState* orbit = std::construct_at(
        static_cast<VS::Battle::Ai::ArenaOrbitState*>(storage));
    orbit->cooldown = cooldown;
    VS::Battle::Ai::arenaOrbit = orbit;
    return true;
  }
};

class LiveReactionDelay final : public ReactionDelayHost {
 public:
  explicit LiveReactionDelay(const VS::Battle::Ai::ActorAiState& state)
      : state_(state) {}

  int NextRandomMasked(int range) override {
    return NextLiveActorRandomMasked(range);
  }
  // The original writes through the AI state's actor pointer (0x54), which
  // always names the battle actor in the state's slot.
  void SetCommandFlags(std::uint8_t flags) override {
    Require(state_.actor_id < kCleanupActorSlots,
            "AI reaction request actor ID is outside the AI slots");
    VS::Battle::Actor* actor = VS::Battle::actors[state_.actor_id];
    Require(actor != nullptr, "AI reaction request requires an actor");
    actor->command_flags = flags;
  }

 private:
  const VS::Battle::Ai::ActorAiState& state_;
};

HostilityState LoadHostility(const VS::Battle::Ai::ActorAiState& source) {
  HostilityState state{.alerted = source.opaque18};
  for (std::size_t id = 0; id < state.relations.size(); ++id) {
    state.relations.at(id) = std::bit_cast<std::uint32_t>(source.relations[id]);
  }
  return state;
}

void StoreHostility(const HostilityState& state,
                    VS::Battle::Ai::ActorAiState& target) {
  target.opaque18 = state.alerted;
  for (std::size_t id = 0; id < state.relations.size(); ++id) {
    target.relations[id] =
        std::bit_cast<VS::Battle::Ai::RelationWord>(state.relations.at(id));
  }
}

class LiveHostility final : public HostilityHost {
 public:
  explicit LiveHostility(VS::Battle::Ai::ActorAiState& state) : state_(state) {}

  void DrawBiosRandom() override { (void)VS::Core::NextBiosRand(); }
  void ShowAwarenessIndicator(HostilityState& state, int indicator,
                              int frames) override {
    StoreHostility(state, state_);
    SetLiveTemporaryAwarenessIndicator(&state_, indicator, frames);
    state = LoadHostility(state_);
  }
  void RequestReaction(HostilityState& state, int value, int range) override {
    StoreHostility(state, state_);
    SetLiveActorAiReactionWithDelay(
        reinterpret_cast<VS::Battle::Ai::PerceptionUpdateView*>(&state_), value,
        range);
    state = LoadHostility(state_);
  }

 private:
  VS::Battle::Ai::ActorAiState& state_;
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

void ReleaseLiveActorAiReservations(VS::Battle::Ai::ActorAiState* state) {
  Require(state != nullptr && VS::Battle::Ai::roomAiState != nullptr,
          "AI reservation release requires actor and room state");
  ReservationState reservations = LoadReservations(*state);
  CleanupRoom room = LoadRoom(*VS::Battle::Ai::roomAiState);
  Require(ReleaseActorAiReservations(reservations, room, LiveTileFlags()),
          "AI reservation release requires a room range inside the tile "
          "grid");
  state->on_moving_platform = reservations.navigation_reset;
  StoreRoomFields(room, *VS::Battle::Ai::roomAiState);
}

void FreeLiveActorAiState(VS::Battle::Actor* actor) {
  Require(actor != nullptr, "AI teardown requires an actor");
  TeardownActor teardown{.handle = Actors::LiveActorIdentity(actor),
                         .id = actor->id,
                         .command_flags = actor->command_flags,
                         .sub_type = actor->sub_type,
                         .kind = static_cast<std::uint8_t>(actor->kind)};
  LiveCleanupHost host;
  Require(FreeActorAiState(host, teardown),
          "AI teardown requires an actor in the AI slots, the room state and "
          "consistent engagement");
}

void AllocateLiveOrbitingBossState(VS::Battle::Ai::SettingsView* /*state*/) {
  LiveCleanupHost host;
  Require(AllocateOrbitingBossState(host), "AI orbit-state allocation failed");
}

void SetLiveActorAiReactionWithDelay(
    VS::Battle::Ai::PerceptionUpdateView* state, int value, int range) {
  Require(state != nullptr, "AI reaction request requires state");
  auto& live = *reinterpret_cast<VS::Battle::Ai::ActorAiState*>(state);
  ReactionDelayState delay{
      .has_navigation_override = live.native_pointer_18c != nullptr,
      .decision_delay = live.decision_delay};
  LiveReactionDelay host(live);
  if (SetActorAiReactionWithDelay(delay, host, value, range)) {
    live.decision_delay = delay.decision_delay;
  }
}

void MakeLiveTargetHostile(VS::Battle::Ai::ReactionView* state, int target) {
  Require(state != nullptr, "AI hostility request requires state");
  auto& live = *reinterpret_cast<VS::Battle::Ai::ActorAiState*>(state);
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
