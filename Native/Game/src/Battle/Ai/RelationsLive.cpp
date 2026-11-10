// Hand-maintained (tools/port/owned.csv).
// Live binding for the actor relation and Ashley alliance policies
// (Relations.cpp).
#include "Battle/Ai/RelationsLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <span>

#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/DecisionsLive.hpp"
#include "Battle/Ai/EngagementLive.hpp"

extern "C" {
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Core/BiosRand.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle::Ai {
namespace {
static_assert(std::size(Battle_Ai_actorAiStates) == kRelationActorSlots);
constexpr unsigned kPreservedRelationFlag = 8;

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

// The room actor IDs in battle actor list order.
class RoomActorList {
 public:
  RoomActorList() {
    for (const Battle_Actor* actor = Battle_actors[0]; actor != nullptr;
         actor = actor->next) {
      Require(count_ < ids_.size() && actor->id >= 0 &&
                  actor->id < kRelationActorSlots,
              "AI relations require a bounded battle actor list");
      ids_.at(count_++) = static_cast<std::uint8_t>(actor->id);
    }
  }

  [[nodiscard]] std::span<const std::uint8_t> Ids() const {
    return std::span(ids_).first(count_);
  }

 private:
  std::array<std::uint8_t, kRelationActorSlots> ids_{};
  std::size_t count_ = 0;
};

Battle_Ai_ActorAiState& TableState(int actor) {
  Battle_Ai_ActorAiState* state =
      Battle_Ai_actorAiStates[static_cast<std::size_t>(actor)];
  Require(state != nullptr, "AI relations require every room actor's state");
  return *state;
}

class LiveRelationTable final : public ActorRelationTable {
 public:
  [[nodiscard]] std::span<const std::uint8_t> RoomActors() const override {
    return actors_.Ids();
  }
  [[nodiscard]] std::uint8_t Category(int actor) const override {
    return State(actor).settingsBits21;
  }
  [[nodiscard]] unsigned Classification(int source, int target) const override {
    return State(source).relations[target].classification;
  }
  void SetClassification(int source, int target, unsigned value) override {
    State(source).relations[target].classification = value;
  }

 private:
  [[nodiscard]] static Battle_Ai_ActorAiState& State(int actor) {
    return TableState(actor);
  }

  RoomActorList actors_;
};

// The original reads the source through `state` and every target through the
// table; requiring the registration makes both the same state.
void InitializeRelations(Battle_Ai_ActorAiState& state) {
  Require(state.actorId < kRelationActorSlots &&
              Battle_Ai_actorAiStates[state.actorId] == &state,
          "AI relations require a registered actor state");
  LiveRelationTable table;
  Require(InitializeActorAiRelations(table, state.actorId),
          "AI relations require a valid relation category");
}

class LiveAllianceActor final : public AshleyAllianceActor {
 public:
  explicit LiveAllianceActor(Battle_Ai_ReactionView& reaction)
      : reaction_(reaction),
        settings_(*reinterpret_cast<Battle_Ai_SettingsView*>(&reaction)),
        state_(*reinterpret_cast<Battle_Ai_ActorAiState*>(&reaction)) {}

  [[nodiscard]] std::uint8_t ReactionCount() const override {
    return reaction_.allianceReactionCount;
  }
  void SetReactionCount(std::uint8_t count) override {
    reaction_.allianceReactionCount = count;
  }
  [[nodiscard]] bool AllianceActive() const override {
    return reaction_.temporaryAshleyAlliance != 0;
  }
  void SetAllianceActive(bool active) override {
    reaction_.temporaryAshleyAlliance = active ? 1 : 0;
  }
  void SetAwarenessIndicator(int marker) override {
    SetLiveActorAwarenessIndicator(reaction_.actorId, marker);
  }
  void RecomputeRelations() override { InitializeRelations(state_); }
  void SaveDecisionAndApproach() override {
    settings_.savedDecisionCallback = settings_.decisionCallback;
    settings_.decisionCallback = UpdateLiveEngagementApproachBehavior;
  }
  void RestoreSavedDecision() override {
    if (settings_.savedDecisionCallback != nullptr) {
      settings_.decisionCallback = settings_.savedDecisionCallback;
    }
    settings_.savedDecisionCallback = nullptr;
  }
  [[nodiscard]] bool RecoveryDisabled() const override {
    return reaction_.settingsBit25 != 0;
  }
  void SetTemporaryAwareness(int marker, int frames) override {
    Battle_Ai_SetTemporaryAwarenessIndicator(&state_, marker, frames);
  }
  void QueueReaction(int command, int range) override {
    SetLiveActorAiReactionWithDelay(
        reinterpret_cast<Battle_Ai_PerceptionUpdateView*>(&reaction_), command,
        range);
  }
  [[nodiscard]] int Roll() override { return Core_Random_Rand(); }

 private:
  Battle_Ai_ReactionView& reaction_;
  Battle_Ai_SettingsView& settings_;
  Battle_Ai_ActorAiState& state_;
};

Battle_Ai_ReactionView& RequireAllianceState(Battle_Ai_ReactionView* state) {
  Require(state != nullptr && state->actorId < kRelationActorSlots,
          "Ashley alliance requires a valid actor state");
  return *state;
}

class LiveSavedRelationActors final : public SavedRelationActors {
 public:
  [[nodiscard]] std::span<const std::uint8_t> RoomActors() const override {
    return actors_.Ids();
  }
  [[nodiscard]] std::int16_t SavedOptions(int actor) const override {
    return TableState(actor).savedRelationOptions;
  }
  void RestoreAshleyRelation(int actor,
                             const SavedAshleyRelation& saved) override {
    Battle_Ai_RelationWord& relation = TableState(actor).relations[0];
    relation.active = 0;
    relation.reserved0Bit1 &= kPreservedRelationFlag;
    relation.previousClassification = saved.previous_classification;
    relation.classification = saved.classification;
    relation.classificationTimer = saved.classification_timer;
  }
  void BeginForcedAlliance(int actor) override {
    LiveAllianceActor alliance(
        *reinterpret_cast<Battle_Ai_ReactionView*>(&TableState(actor)));
    TryBeginAshleyAlliance(alliance, true);
  }

 private:
  RoomActorList actors_;
};

// The actors after Ashley with their AI group fields, in list order.
class LiveLinkedActorTable final : public LinkedActorTable {
 public:
  LiveLinkedActorTable() {
    const Battle_Actor* head = Battle_actors[0];
    Require(head != nullptr,
            "AI linked actors require Ashley at the battle actor list head");
    for (const Battle_Actor* actor = head->next; actor != nullptr;
         actor = actor->next) {
      // The head takes one of the kRelationActorSlots list entries.
      Require(count_ + 1 < kRelationActorSlots && actor->id >= 0 &&
                  actor->id < kRelationActorSlots,
              "AI linked actors require a bounded battle actor list");
      actors_.at(count_++) = {
          .id = static_cast<std::uint8_t>(actor->id),
          .group = static_cast<std::uint8_t>(actor->aiGroup),
          .linked_group = static_cast<std::uint8_t>(actor->aiLinkedGroup)};
    }
  }

  [[nodiscard]] std::span<const LinkGroupActor> FollowingActors()
      const override {
    return std::span(actors_).first(count_);
  }
  [[nodiscard]] std::optional<int> LinkedActor(int actor_id) const override {
    const Battle_Ai_ActorAiState* state =
        Battle_Ai_actorAiStates[static_cast<std::size_t>(actor_id)];
    if (state == nullptr) return std::nullopt;
    return state->linkedActor;
  }
  void SetLinkedActor(int actor_id, int linked) override {
    TableState(actor_id).linkedActor = static_cast<signed char>(linked);
  }

 private:
  std::array<LinkGroupActor, kRelationActorSlots> actors_{};
  std::size_t count_ = 0;
};
}  // namespace

void InitializeLiveActorAiRelations(Battle_Ai_AiCreationView* state) {
  auto* initial = reinterpret_cast<Battle_Ai_ActorAiState*>(state);
  Require(initial != nullptr, "AI relations require a valid actor state");
  InitializeRelations(*initial);
}

void ApplyLiveSavedActorAiRelations() {
  LiveSavedRelationActors actors;
  ApplySavedActorAiRelations(actors);
}

void EndLiveAshleyAlliance(Battle_Ai_ReactionView* state) {
  LiveAllianceActor actor(RequireAllianceState(state));
  EndAshleyAlliance(actor);
}

void TryBeginLiveAshleyAlliance(Battle_Ai_ReactionView* state, int forced) {
  LiveAllianceActor actor(RequireAllianceState(state));
  TryBeginAshleyAlliance(actor, forced != 0);
}

void ResolveLiveAiLinkedActors() {
  LiveLinkedActorTable actors;
  Require(ResolveAiLinkedActors(actors),
          "AI linked actors require every listed actor's state");
}
}  // namespace VS::Battle::Ai
