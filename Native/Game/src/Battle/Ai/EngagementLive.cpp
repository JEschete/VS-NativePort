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

#include "Battle/HudOrchestrationLive.hpp"

extern "C" {
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Core/Diagnostics.h"
}

#include "Battle/Ai/PlanningPrimitivesLive.hpp"

namespace VS::Battle::Ai {
namespace {
static_assert(std::size(Battle_Ai_actorAiStates) == kEngagementActorSlots);
// The perception view aliases the actor AI state that holds the actor ID.
static_assert(offsetof(Battle_Ai_PerceptionUpdateView,
                       awarenessFramesRemaining) ==
              offsetof(Battle_Ai_ActorAiState, awarenessFramesRemaining));
static_assert(offsetof(Battle_Ai_PerceptionUpdateView, awarenessMarker) ==
              offsetof(Battle_Ai_ActorAiState, awarenessMarker));
// The AI state's engagement data is the battle actor; its pose bits are the
// command flags byte.
static_assert(offsetof(Battle_Ai_EngagementData, reservedA) + 1 ==
              offsetof(Battle_Actor, commandFlags));
constexpr unsigned kPoseShift = 2;
constexpr unsigned kPoseMask = 3u << kPoseShift;

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

class LiveAwarenessMarkers final : public AwarenessMarkers {
 public:
  void SetMarker(int actor_id, int marker) override {
    VS::Battle::SetLiveActorMarker(actor_id, marker);
  }
};

class LiveEngagementHost final : public EngagementBuildupHost {
 public:
  LiveEngagementHost(Battle_Ai_ActorAiState& state, Battle_Ai_RoomAiState* room)
      : state_(state), room_(room) {}

  void SetMarker(int actor_id, int marker) override {
    VS::Battle::SetLiveActorMarker(actor_id, marker);
  }
  [[nodiscard]] bool CandidateEngagedThisUpdate() const override {
    return Battle_Ai_candidateEngagedThisUpdate != 0;
  }
  void ClearEngagementRequest() override { state_.engagementRequest = 0; }
  void LinkEngagedActor() override {
    Require(room_ != nullptr, "AI engagement requires room state");
    if (room_->engagedHead == nullptr) room_->engagedTail = &state_;
    state_.nextEngaged = room_->engagedHead;
    room_->engagedHead = &state_;
  }
  void UnlinkEngagedActor() override { UnlinkLiveEngagedActor(&state_); }
  [[nodiscard]] std::optional<bool> HasSingleOpponent() const override {
    const Battle_Actor* first = Battle_actors[0];
    if (first == nullptr) return std::nullopt;
    return first->next != nullptr && first->next->next == nullptr;
  }
  void RememberActionTarget() override { RememberLiveAiActionTarget(&state_); }

 private:
  Battle_Ai_ActorAiState& state_;
  Battle_Ai_RoomAiState* room_;
};

// Maps the room's engaged-list pointers to AI state table IDs. A pointer is
// matched by address alone, so a removed actor's stale successor is never
// dereferenced.
class LiveEngagedList final : public EngagedList {
 public:
  explicit LiveEngagedList(Battle_Ai_RoomAiState& room) : room_(room) {}

  [[nodiscard]] std::optional<int> Head() const override {
    return IdOf(room_.engagedHead);
  }
  void SetHead(std::optional<int> actor_id) override {
    room_.engagedHead = StateOrNull(actor_id);
  }
  [[nodiscard]] std::optional<int> Tail() const override {
    return IdOf(room_.engagedTail);
  }
  void SetTail(std::optional<int> actor_id) override {
    room_.engagedTail = StateOrNull(actor_id);
  }
  [[nodiscard]] std::optional<int> Next(int actor_id) const override {
    return IdOf(At(actor_id).nextEngaged);
  }
  void SetNext(int actor_id, std::optional<int> next) override {
    At(actor_id).nextEngaged = StateOrNull(next);
  }
  [[nodiscard]] EngagementPhase Phase(int actor_id) const override {
    const Battle_Ai_ActorAiState& state = At(actor_id);
    return {.phase = state.opaque12F, .step = state.opaque130};
  }
  void SetPhase(int actor_id, EngagementPhase phase) override {
    Battle_Ai_ActorAiState& state = At(actor_id);
    state.opaque12F = phase.phase;
    state.opaque130 = phase.step;
  }
  void SetPose(int actor_id, unsigned pose) override {
    Battle_Ai_ActorAiState& state = At(actor_id);
    Require(state.actor != nullptr, "AI engagement pose requires the actor");
    auto& actor = *reinterpret_cast<Battle_Actor*>(state.actor);
    actor.commandFlags = static_cast<u_char>(
        (actor.commandFlags & ~kPoseMask) | ((pose << kPoseShift) & kPoseMask));
  }
  [[nodiscard]] unsigned AshleyClassification(int actor_id) const override {
    return At(actor_id).relations[0].classification;
  }

 private:
  static std::optional<int> IdOf(const Battle_Ai_ActorAiState* state) {
    if (state == nullptr) return std::nullopt;
    for (int id = 0; id < kEngagementActorSlots; ++id) {
      if (Battle_Ai_actorAiStates[static_cast<std::size_t>(id)] == state) {
        return id;
      }
    }
    Require(false, "AI engaged list holds a state outside the AI state table");
    return std::nullopt;
  }
  static Battle_Ai_ActorAiState* StateOrNull(std::optional<int> actor_id) {
    if (!actor_id.has_value()) return nullptr;
    return &At(*actor_id);
  }
  static Battle_Ai_ActorAiState& At(int actor_id) {
    Battle_Ai_ActorAiState* state =
        Battle_Ai_actorAiStates[static_cast<std::size_t>(actor_id)];
    Require(state != nullptr, "AI engaged list requires the member's state");
    return *state;
  }

  Battle_Ai_RoomAiState& room_;
};

class LiveEngagementTable final : public EngagementTable {
 public:
  [[nodiscard]] bool HasActorState(int actor_id) const override {
    return Battle_Ai_actorAiStates[static_cast<std::size_t>(actor_id)] !=
           nullptr;
  }
  void DisengageActorState(int actor_id) override {
    DisengageLiveActor(
        Battle_Ai_actorAiStates[static_cast<std::size_t>(actor_id)]);
  }
};

Battle_Ai_ActorAiState& StateOf(Battle_Ai_ActorAiState* state) {
  Require(state != nullptr, "AI engagement requires an actor state");
  return *state;
}

EngagementTimers TimersOf(const Battle_Ai_ActorAiState& state) {
  return {.engaged_frames = state.engagementFrames,
          .cooldown_frames = state.engagementCooldownFrames,
          .grace_frames = state.reengagementGraceFrames};
}

void StoreTimers(Battle_Ai_ActorAiState& state,
                 const EngagementTimers& timers) {
  state.engagementFrames = timers.engaged_frames;
  state.engagementCooldownFrames = timers.cooldown_frames;
  state.reengagementGraceFrames = timers.grace_frames;
}

EngagementRoster RosterOf(const Battle_Ai_RoomAiState& room) {
  return {.engaged_mask = EngagedMask(room),
          .engaged_count = room.engagedCount,
          .limit = room.engagementLimit,
          .approach_moves = room.approachMoves};
}

void StoreRoster(Battle_Ai_RoomAiState& room, const EngagementRoster& roster) {
  StoreEngagedMask(room, roster.engaged_mask);
  room.engagedCount = roster.engaged_count;
}
}  // namespace

std::uint16_t EngagedMask(const Battle_Ai_RoomAiState& room) {
  std::uint16_t mask = 0;
  std::memcpy(&mask, room.engaged_mask_bytes, sizeof(mask));
  return mask;
}

void StoreEngagedMask(Battle_Ai_RoomAiState& room, std::uint16_t mask) {
  std::memcpy(room.engaged_mask_bytes, &mask, sizeof(mask));
}

void SetLiveActorAwarenessIndicator(int actor_id, int marker) {
  LiveAwarenessMarkers markers;
  Require(SetActorAwarenessIndicator(markers, actor_id, marker),
          "AI awareness marker requires an actor ID in the AI state table");
}

void ResetLiveActorAwarenessIndicator(Battle_Ai_PlanEvaluationView* state) {
  Require(state != nullptr, "AI marker reset requires an actor state");
  LiveAwarenessMarkers markers;
  Require(ResetActorAwarenessIndicator(markers, state->actorId),
          "AI marker reset requires an actor ID in the AI state table");
}

void SetLiveTemporaryAwarenessIndicator(Battle_Ai_ActorAiState* pointer,
                                        int marker, int frames) {
  Battle_Ai_ActorAiState& state = StateOf(pointer);
  TemporaryAwareness awareness{
      .frames_remaining = state.awarenessFramesRemaining,
      .marker = state.awarenessMarker};
  SetTemporaryAwarenessIndicator(awareness, marker, frames);
  state.awarenessMarker = awareness.marker;
  state.awarenessFramesRemaining = awareness.frames_remaining;
}

bool IsLiveEngagementReady(const Battle_Ai_ActorAiState* state) {
  Require(state != nullptr, "AI engagement readiness requires an actor state");
  return IsEngagementReady(state->engagementFrames);
}

void UpdateLiveTemporaryAwarenessIndicator(
    Battle_Ai_PerceptionUpdateView* view) {
  Battle_Ai_ActorAiState& state =
      StateOf(reinterpret_cast<Battle_Ai_ActorAiState*>(view));
  TemporaryAwareness awareness{
      .frames_remaining = state.awarenessFramesRemaining,
      .marker = state.awarenessMarker};
  LiveAwarenessMarkers markers;
  const bool updated =
      UpdateTemporaryAwarenessIndicator(markers, state.actorId, awareness);
  state.awarenessFramesRemaining = awareness.frames_remaining;
  Require(updated, "AI temporary marker requires an actor ID in the table");
}

void DisengageLiveActorById(int actor_id) {
  LiveEngagementTable table;
  if (!DisengageActorById(table, actor_id).has_value()) {
    Core_Warnf("ai", "AI disengagement ignored actor ID %d", actor_id);
  }
}

void DisengageLiveActor(Battle_Ai_ActorAiState* pointer) {
  Battle_Ai_ActorAiState& state = StateOf(pointer);
  Require(Battle_Ai_roomAiState != nullptr,
          "AI disengagement requires room state");
  Battle_Ai_RoomAiState& room = *Battle_Ai_roomAiState;
  EngagementTimers timers = TimersOf(state);
  EngagementRoster roster = RosterOf(room);
  LiveEngagementHost host(state, &room);
  const std::optional<bool> disengaged =
      DisengageActor(host, state.actorId, timers, roster);
  Require(disengaged.has_value(),
          "AI disengagement requires a valid actor, roster and actor list");
  StoreTimers(state, timers);
  StoreRoster(room, roster);
}

bool TryEngageLiveActor(Battle_Ai_ActorAiState* pointer) {
  Battle_Ai_ActorAiState& state = StateOf(pointer);
  EngagementTimers timers = TimersOf(state);
  std::optional<EngagementRoster> roster;
  if (Battle_Ai_roomAiState != nullptr) {
    roster = RosterOf(*Battle_Ai_roomAiState);
  }
  LiveEngagementHost host(state, Battle_Ai_roomAiState);
  const std::optional<bool> engaged = TryEngageActor(
      host, state.actorId, timers, roster.has_value() ? &*roster : nullptr);
  Require(engaged.has_value(),
          "AI engagement requires a valid actor, roster and room state");
  StoreTimers(state, timers);
  if (roster.has_value()) StoreRoster(*Battle_Ai_roomAiState, *roster);
  return *engaged;
}

void UnlinkLiveEngagedActor(Battle_Ai_ActorAiState* pointer) {
  Battle_Ai_ActorAiState& state = StateOf(pointer);
  Require(state.actorId < kEngagementActorSlots &&
              Battle_Ai_actorAiStates[state.actorId] == &state,
          "AI engaged-list removal requires the AI state table's entry");
  Require(state.actor != nullptr && Battle_Ai_roomAiState != nullptr,
          "AI engaged-list removal requires the actor and room state");
  LiveEngagedList list(*Battle_Ai_roomAiState);
  Require(UnlinkEngagedActor(list, state.actorId),
          "AI engaged-list removal requires a list without repeats");
}

void UpdateLiveEngagementBuildup(Battle_Ai_ActorAiState* pointer) {
  Battle_Ai_ActorAiState& state = StateOf(pointer);
  EngagementTimers timers = TimersOf(state);
  std::optional<EngagementRoster> roster;
  if (Battle_Ai_roomAiState != nullptr) {
    roster = RosterOf(*Battle_Ai_roomAiState);
  }
  LiveEngagementHost host(state, Battle_Ai_roomAiState);
  const bool updated = UpdateEngagementBuildup(
      host, state.actorId, timers, roster.has_value() ? &*roster : nullptr);
  Require(updated,
          "AI engagement buildup requires a valid actor, roster and list");
  StoreTimers(state, timers);
  if (roster.has_value()) StoreRoster(*Battle_Ai_roomAiState, *roster);
}

void AnimateLiveEngagementPhase() {
  Require(Battle_Ai_roomAiState != nullptr,
          "AI engagement animation requires room state");
  LiveEngagedList list(*Battle_Ai_roomAiState);
  Require(AnimateEngagementPhase(list),
          "AI engagement animation requires a list without repeats");
}
}  // namespace VS::Battle::Ai
