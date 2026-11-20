// Hand-maintained (tools/port/owned.csv).
// Live binding for the engagement roster and awareness markers
// (Engagement.cpp).
#include "Battle/Ai/EngagementLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <optional>

#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Battle/Ai/PlanningPrimitivesLive.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Feedback.hpp"
#include "Battle/HudOrchestrationLive.hpp"
#include "Core/Diagnostics.hpp"

namespace VS::Battle::Ai {
namespace {
static_assert(std::size(VS::Battle::Ai::actorAiStates) ==
              kEngagementActorSlots);
// The perception view aliases the actor AI state that holds the actor ID.
static_assert(offsetof(VS::Battle::Ai::PerceptionUpdateView,
                       awareness_frames_remaining) ==
              offsetof(VS::Battle::Ai::ActorAiState,
                       awareness_frames_remaining));
static_assert(offsetof(VS::Battle::Ai::PerceptionUpdateView,
                       awareness_marker) ==
              offsetof(VS::Battle::Ai::ActorAiState, awareness_marker));
// The AI state's engagement data is the battle actor; its pose bits are the
// command flags byte.
static_assert(offsetof(VS::Battle::Ai::EngagementData, reserved_a) + 1 ==
              offsetof(VS::Battle::Actor, command_flags));
constexpr unsigned kPoseShift = 2;
constexpr unsigned kPoseMask = 3u << kPoseShift;

class LiveAwarenessMarkers final : public AwarenessMarkers {
 public:
  void SetMarker(int actor_id, int marker) override {
    VS::Battle::SetLiveActorMarker(actor_id, marker);
  }
};

class LiveEngagementHost final : public EngagementBuildupHost {
 public:
  LiveEngagementHost(VS::Battle::Ai::ActorAiState& state,
                     VS::Battle::Ai::RoomAiState* room)
      : state_(state), room_(room) {}

  void SetMarker(int actor_id, int marker) override {
    VS::Battle::SetLiveActorMarker(actor_id, marker);
  }
  [[nodiscard]] bool CandidateEngagedThisUpdate() const override {
    return VS::Battle::Ai::candidateEngagedThisUpdate != 0;
  }
  void ClearEngagementRequest() override { state_.engagement_request = 0; }
  void LinkEngagedActor() override {
    Require(room_ != nullptr, "AI engagement requires room state");
    if (room_->engaged_head == nullptr) room_->engaged_tail = &state_;
    state_.next_engaged = room_->engaged_head;
    room_->engaged_head = &state_;
  }
  void UnlinkEngagedActor() override { UnlinkLiveEngagedActor(&state_); }
  [[nodiscard]] std::optional<bool> HasSingleOpponent() const override {
    const VS::Battle::Actor* first = VS::Battle::actors[0];
    if (first == nullptr) return std::nullopt;
    return first->next != nullptr && first->next->next == nullptr;
  }
  void RememberActionTarget() override { RememberLiveAiActionTarget(&state_); }

 private:
  VS::Battle::Ai::ActorAiState& state_;
  VS::Battle::Ai::RoomAiState* room_;
};

// Maps the room's engaged-list pointers to AI state table IDs. A pointer is
// matched by address alone, so a removed actor's stale successor is never
// dereferenced.
class LiveEngagedList final : public EngagedList {
 public:
  explicit LiveEngagedList(VS::Battle::Ai::RoomAiState& room) : room_(room) {}

  [[nodiscard]] std::optional<int> Head() const override {
    return IdOf(room_.engaged_head);
  }
  void SetHead(std::optional<int> actor_id) override {
    room_.engaged_head = StateOrNull(actor_id);
  }
  [[nodiscard]] std::optional<int> Tail() const override {
    return IdOf(room_.engaged_tail);
  }
  void SetTail(std::optional<int> actor_id) override {
    room_.engaged_tail = StateOrNull(actor_id);
  }
  [[nodiscard]] std::optional<int> Next(int actor_id) const override {
    return IdOf(At(actor_id).next_engaged);
  }
  void SetNext(int actor_id, std::optional<int> next) override {
    At(actor_id).next_engaged = StateOrNull(next);
  }
  [[nodiscard]] EngagementPhase Phase(int actor_id) const override {
    const VS::Battle::Ai::ActorAiState& state = At(actor_id);
    return {.phase = state.opaque_12f, .step = state.opaque130};
  }
  void SetPhase(int actor_id, EngagementPhase phase) override {
    VS::Battle::Ai::ActorAiState& state = At(actor_id);
    state.opaque_12f = phase.phase;
    state.opaque130 = phase.step;
  }
  void SetPose(int actor_id, unsigned pose) override {
    VS::Battle::Ai::ActorAiState& state = At(actor_id);
    Require(state.actor != nullptr, "AI engagement pose requires the actor");
    auto& actor = *reinterpret_cast<VS::Battle::Actor*>(state.actor);
    actor.command_flags =
        static_cast<u_char>((actor.command_flags & ~kPoseMask) |
                            ((pose << kPoseShift) & kPoseMask));
  }
  [[nodiscard]] unsigned AshleyClassification(int actor_id) const override {
    return At(actor_id).relations[0].classification;
  }

 private:
  static std::optional<int> IdOf(const VS::Battle::Ai::ActorAiState* state) {
    if (state == nullptr) return std::nullopt;
    for (int id = 0; id < kEngagementActorSlots; ++id) {
      if (VS::Battle::Ai::actorAiStates[static_cast<std::size_t>(id)] ==
          state) {
        return id;
      }
    }
    Require(false, "AI engaged list holds a state outside the AI state table");
    return std::nullopt;
  }
  static VS::Battle::Ai::ActorAiState* StateOrNull(
      std::optional<int> actor_id) {
    if (!actor_id.has_value()) return nullptr;
    return &At(*actor_id);
  }
  static VS::Battle::Ai::ActorAiState& At(int actor_id) {
    VS::Battle::Ai::ActorAiState* state =
        VS::Battle::Ai::actorAiStates[static_cast<std::size_t>(actor_id)];
    Require(state != nullptr, "AI engaged list requires the member's state");
    return *state;
  }

  VS::Battle::Ai::RoomAiState& room_;
};

class LiveEngagementTable final : public EngagementTable {
 public:
  [[nodiscard]] bool HasActorState(int actor_id) const override {
    return VS::Battle::Ai::actorAiStates[static_cast<std::size_t>(actor_id)] !=
           nullptr;
  }
  void DisengageActorState(int actor_id) override {
    DisengageLiveActor(
        VS::Battle::Ai::actorAiStates[static_cast<std::size_t>(actor_id)]);
  }
};

VS::Battle::Ai::ActorAiState& StateOf(VS::Battle::Ai::ActorAiState* state) {
  Require(state != nullptr, "AI engagement requires an actor state");
  return *state;
}

EngagementTimers TimersOf(const VS::Battle::Ai::ActorAiState& state) {
  return {.engaged_frames = state.engagement_frames,
          .cooldown_frames = state.engagement_cooldown_frames,
          .grace_frames = state.reengagement_grace_frames};
}

void StoreTimers(VS::Battle::Ai::ActorAiState& state,
                 const EngagementTimers& timers) {
  state.engagement_frames = timers.engaged_frames;
  state.engagement_cooldown_frames = timers.cooldown_frames;
  state.reengagement_grace_frames = timers.grace_frames;
}

EngagementRoster RosterOf(const VS::Battle::Ai::RoomAiState& room) {
  return {.engaged_mask = EngagedMask(room),
          .engaged_count = room.engaged_count,
          .limit = room.engagement_limit,
          .approach_moves = room.approach_moves};
}

void StoreRoster(VS::Battle::Ai::RoomAiState& room,
                 const EngagementRoster& roster) {
  StoreEngagedMask(room, roster.engaged_mask);
  room.engaged_count = roster.engaged_count;
}
}  // namespace

std::uint16_t EngagedMask(const VS::Battle::Ai::RoomAiState& room) {
  std::uint16_t mask = 0;
  std::memcpy(&mask, room.engaged_mask_bytes.data(), sizeof(mask));
  return mask;
}

void StoreEngagedMask(VS::Battle::Ai::RoomAiState& room, std::uint16_t mask) {
  std::memcpy(room.engaged_mask_bytes.data(), &mask, sizeof(mask));
}

void SetLiveActorAwarenessIndicator(int actor_id, int marker) {
  LiveAwarenessMarkers markers;
  Require(SetActorAwarenessIndicator(markers, actor_id, marker),
          "AI awareness marker requires an actor ID in the AI state table");
}

void ResetLiveActorAwarenessIndicator(
    VS::Battle::Ai::PlanEvaluationView* state) {
  Require(state != nullptr, "AI marker reset requires an actor state");
  LiveAwarenessMarkers markers;
  Require(ResetActorAwarenessIndicator(markers, state->actor_id),
          "AI marker reset requires an actor ID in the AI state table");
}

void SetLiveTemporaryAwarenessIndicator(VS::Battle::Ai::ActorAiState* pointer,
                                        int marker, int frames) {
  VS::Battle::Ai::ActorAiState& state = StateOf(pointer);
  TemporaryAwareness awareness{
      .frames_remaining = state.awareness_frames_remaining,
      .marker = state.awareness_marker};
  SetTemporaryAwarenessIndicator(awareness, marker, frames);
  state.awareness_marker = awareness.marker;
  state.awareness_frames_remaining = awareness.frames_remaining;
}

bool IsLiveEngagementReady(const VS::Battle::Ai::ActorAiState* state) {
  Require(state != nullptr, "AI engagement readiness requires an actor state");
  return IsEngagementReady(state->engagement_frames);
}

void UpdateLiveTemporaryAwarenessIndicator(
    VS::Battle::Ai::PerceptionUpdateView* view) {
  VS::Battle::Ai::ActorAiState& state =
      StateOf(reinterpret_cast<VS::Battle::Ai::ActorAiState*>(view));
  TemporaryAwareness awareness{
      .frames_remaining = state.awareness_frames_remaining,
      .marker = state.awareness_marker};
  LiveAwarenessMarkers markers;
  const bool updated =
      UpdateTemporaryAwarenessIndicator(markers, state.actor_id, awareness);
  state.awareness_frames_remaining = awareness.frames_remaining;
  Require(updated, "AI temporary marker requires an actor ID in the table");
}

void DisengageLiveActorById(int actor_id) {
  LiveEngagementTable table;
  if (!DisengageActorById(table, actor_id).has_value()) {
    VS::Core::Warnf("ai", "AI disengagement ignored actor ID %d", actor_id);
  }
}

void DisengageLiveActor(VS::Battle::Ai::ActorAiState* pointer) {
  VS::Battle::Ai::ActorAiState& state = StateOf(pointer);
  Require(VS::Battle::Ai::roomAiState != nullptr,
          "AI disengagement requires room state");
  VS::Battle::Ai::RoomAiState& room = *VS::Battle::Ai::roomAiState;
  EngagementTimers timers = TimersOf(state);
  EngagementRoster roster = RosterOf(room);
  LiveEngagementHost host(state, &room);
  const std::optional<bool> disengaged =
      DisengageActor(host, state.actor_id, timers, roster);
  Require(disengaged.has_value(),
          "AI disengagement requires a valid actor, roster and actor list");
  StoreTimers(state, timers);
  StoreRoster(room, roster);
}

bool TryEngageLiveActor(VS::Battle::Ai::ActorAiState* pointer) {
  VS::Battle::Ai::ActorAiState& state = StateOf(pointer);
  EngagementTimers timers = TimersOf(state);
  std::optional<EngagementRoster> roster;
  if (VS::Battle::Ai::roomAiState != nullptr) {
    roster = RosterOf(*VS::Battle::Ai::roomAiState);
  }
  LiveEngagementHost host(state, VS::Battle::Ai::roomAiState);
  const std::optional<bool> engaged = TryEngageActor(
      host, state.actor_id, timers, roster.has_value() ? &*roster : nullptr);
  Require(engaged.has_value(),
          "AI engagement requires a valid actor, roster and room state");
  StoreTimers(state, timers);
  if (roster.has_value()) StoreRoster(*VS::Battle::Ai::roomAiState, *roster);
  return *engaged;
}

void UnlinkLiveEngagedActor(VS::Battle::Ai::ActorAiState* pointer) {
  VS::Battle::Ai::ActorAiState& state = StateOf(pointer);
  Require(state.actor_id < kEngagementActorSlots &&
              VS::Battle::Ai::actorAiStates[state.actor_id] == &state,
          "AI engaged-list removal requires the AI state table's entry");
  Require(state.actor != nullptr && VS::Battle::Ai::roomAiState != nullptr,
          "AI engaged-list removal requires the actor and room state");
  LiveEngagedList list(*VS::Battle::Ai::roomAiState);
  Require(UnlinkEngagedActor(list, state.actor_id),
          "AI engaged-list removal requires a list without repeats");
}

void UpdateLiveEngagementBuildup(VS::Battle::Ai::ActorAiState* pointer) {
  VS::Battle::Ai::ActorAiState& state = StateOf(pointer);
  EngagementTimers timers = TimersOf(state);
  std::optional<EngagementRoster> roster;
  if (VS::Battle::Ai::roomAiState != nullptr) {
    roster = RosterOf(*VS::Battle::Ai::roomAiState);
  }
  LiveEngagementHost host(state, VS::Battle::Ai::roomAiState);
  const bool updated = UpdateEngagementBuildup(
      host, state.actor_id, timers, roster.has_value() ? &*roster : nullptr);
  Require(updated,
          "AI engagement buildup requires a valid actor, roster and list");
  StoreTimers(state, timers);
  if (roster.has_value()) StoreRoster(*VS::Battle::Ai::roomAiState, *roster);
}

void AnimateLiveEngagementPhase() {
  Require(VS::Battle::Ai::roomAiState != nullptr,
          "AI engagement animation requires room state");
  LiveEngagedList list(*VS::Battle::Ai::roomAiState);
  Require(AnimateEngagementPhase(list),
          "AI engagement animation requires a list without repeats");
}
}  // namespace VS::Battle::Ai
