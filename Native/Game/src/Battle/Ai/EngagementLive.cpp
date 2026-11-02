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

extern "C" {
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle::Ai {
namespace {
static_assert(std::size(Battle_Ai_actorAiStates) == kEngagementActorSlots);
// The perception view aliases the actor AI state that holds the actor ID.
static_assert(offsetof(Battle_Ai_PerceptionUpdateView,
                       awarenessFramesRemaining) ==
              offsetof(Battle_Ai_ActorAiState, awarenessFramesRemaining));
static_assert(offsetof(Battle_Ai_PerceptionUpdateView, awarenessMarker) ==
              offsetof(Battle_Ai_ActorAiState, awarenessMarker));

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

class LiveAwarenessMarkers final : public AwarenessMarkers {
 public:
  void SetMarker(int actor_id, int marker) override {
    Battle_SetActorMarker(actor_id, marker);
  }
};

class LiveEngagementHost final : public EngagementHost {
 public:
  LiveEngagementHost(Battle_Ai_ActorAiState& state, Battle_Ai_RoomAiState* room)
      : state_(state), room_(room) {}

  void SetMarker(int actor_id, int marker) override {
    Battle_SetActorMarker(actor_id, marker);
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
  void UnlinkEngagedActor() override {
    Require(state_.actor != nullptr && room_ != nullptr,
            "AI disengagement requires the actor and room state");
    Battle_Ai_UnlinkEngagedActor(&state_);
  }
  [[nodiscard]] std::optional<bool> HasSingleOpponent() const override {
    const Battle_Actor* first = Battle_actors[0];
    if (first == nullptr) return std::nullopt;
    return first->next != nullptr && first->next->next == nullptr;
  }

 private:
  Battle_Ai_ActorAiState& state_;
  Battle_Ai_RoomAiState* room_;
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
}  // namespace VS::Battle::Ai
