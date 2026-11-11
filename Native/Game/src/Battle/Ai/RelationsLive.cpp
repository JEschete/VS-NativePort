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
#include <type_traits>

#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/DecisionsLive.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"

extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/OrbitingBoss.h"
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
    SetLiveTemporaryAwarenessIndicator(&state_, marker, frames);
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

// The reaction view overlays the actor AI state.
static_assert(offsetof(Battle_Ai_ReactionView, relations) ==
              offsetof(Battle_Ai_ActorAiState, relations));
static_assert(sizeof(Battle_Ai_RelationWordView) ==
              sizeof(Battle_Ai_RelationWord));
using ActionTargets = decltype(Battle_ActionRequest::targets);
constexpr std::size_t kMaxActionHits = std::extent_v<ActionTargets>;

void RequireSlot(int actor) {
  Require(actor >= 0 && actor < kRelationActorSlots,
          "AI reactions require an actor inside the AI slots");
}

ReactionSettings ReadSettings(const Battle_Ai_ReactionView& view) {
  return {.allied_with_ashley = view.temporaryAshleyAlliance != 0,
          .settings_bit25 = view.settingsBit25 != 0,
          .settings_bit26 = view.settingsBit26 != 0,
          .settings_bit27 = view.settingsBit27 != 0,
          .shares_ally_hostility = view.settingsByte3Bit0 != 0,
          .category = view.settingsBits21};
}

Battle_Ai_ReactionView* PeerView(int actor) {
  RequireSlot(actor);
  return reinterpret_cast<Battle_Ai_ReactionView*>(
      Battle_Ai_actorAiStates[static_cast<std::size_t>(actor)]);
}

class LiveActorReaction final : public ActorReactionHost {
 public:
  explicit LiveActorReaction(Battle_Ai_ReactionView& state) : state_(state) {}

  [[nodiscard]] int ActorId() const override { return state_.actorId; }
  [[nodiscard]] std::uint8_t BehaviorMode() const override {
    return state_.behaviorMode;
  }
  void SetBehaviorMode(std::uint8_t mode) override {
    state_.behaviorMode = mode;
  }
  [[nodiscard]] ReactionSettings Settings() const override {
    return ReadSettings(state_);
  }
  [[nodiscard]] std::uint32_t Relation(int target) const override {
    return Word(state_, target);
  }
  void SetRelation(int target, std::uint32_t word) override {
    Word(state_, target) = word;
  }
  [[nodiscard]] std::optional<ReactionSettings> PeerSettings(
      int actor) const override {
    const Battle_Ai_ReactionView* peer = PeerView(actor);
    if (peer == nullptr) return std::nullopt;
    return ReadSettings(*peer);
  }
  [[nodiscard]] std::optional<std::uint32_t> PeerRelation(
      int actor, int target) const override {
    Battle_Ai_ReactionView* peer = PeerView(actor);
    if (peer == nullptr) return std::nullopt;
    return Word(*peer, target);
  }
  void SetPeerRelation(int actor, int target, std::uint32_t word) override {
    Battle_Ai_ReactionView* peer = PeerView(actor);
    Require(peer != nullptr, "AI reactions require the peer's state");
    Word(*peer, target) = word;
  }
  [[nodiscard]] bool IsBeneficial(int action) const override {
    return IsLiveActionBeneficialToActor(action, state_.actorId) != 0;
  }
  [[nodiscard]] bool IsHarmful(int action) const override {
    return IsLiveActionHarmfulToActor(action, state_.actorId) != 0;
  }
  void TryBeginAlliance() override {
    LiveAllianceActor actor(state_);
    TryBeginAshleyAlliance(actor, false);
  }
  void EndAlliance() override {
    LiveAllianceActor actor(state_);
    EndAshleyAlliance(actor);
  }
  void MakeHostile(int target) override {
    MakeLiveTargetHostile(&state_, target);
  }
  void StopArenaOrbit() override {
    if (Battle_Ai_arenaOrbit != nullptr) Battle_Ai_arenaOrbit->isOrbiting = 0;
  }

 private:
  static std::uint32_t& Word(Battle_Ai_ReactionView& view, int target) {
    RequireSlot(target);
    return view.relations[static_cast<std::size_t>(target)].word;
  }

  Battle_Ai_ReactionView& state_;
};

ReactionHit ReadHit(const Battle_HitResult& hit) {
  return {.actor = hit.target.targetActor,
          .hp_change = hit.target.hp,
          .hp_effect = hit.hpEffect,
          .skipped = hit.targetsPosition != 0};
}

// The completed action as the reaction policy reads it.
class LiveAppliedAction {
 public:
  explicit LiveAppliedAction(const Battle_ActionRequest& request) {
    Require(request.targetCount <= kMaxActionHits,
            "AI action reaction requires a bounded hit list");
    for (std::size_t i = 0; i < request.targetCount; ++i) {
      hits_.at(i) = ReadHit(request.targets[i]);
    }
    action_.action = request.actionId;
    if (request.actionId < std::size(Battle_actionDefinitions)) {
      const Battle_ActionDefinition& definition =
          Battle_actionDefinitions[request.actionId];
      action_.hit_prerequisites = std::array<std::uint8_t, 2>{
          static_cast<std::uint8_t>(definition.hits[0].prerequisiteFunctionId),
          static_cast<std::uint8_t>(definition.hits[1].prerequisiteFunctionId)};
    }
    action_.acting = ReadHit(request.primaryTarget);
    action_.targets = std::span(hits_).first(request.targetCount);
  }

  [[nodiscard]] const AppliedAction& Action() const { return action_; }

 private:
  std::array<ReactionHit, kMaxActionHits> hits_{};
  AppliedAction action_;
};

class LiveActionNotification final : public ActionNotificationHost {
 public:
  explicit LiveActionNotification(Battle_ActionRequest& request)
      : request_(request) {}

  void ClearReengagementGrace(int actor) override {
    Battle_Ai_ActorAiState* state =
        Battle_Ai_actorAiStates[static_cast<std::size_t>(actor)];
    if (state != nullptr) state->reengagementGraceFrames = 0;
  }
  void ApplyRelationshipChanges() override {
    // The original passes the request as func_800DEEA4_t.
    ApplyLiveActionRelationshipChanges(
        reinterpret_cast<Battle_Ai_ReactionRecord*>(&request_));
  }

 private:
  Battle_ActionRequest& request_;
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

void ApplyLiveActorActionReaction(Battle_Ai_ReactionView* state,
                                  const Battle_ActionRequest* request) {
  if (request == nullptr) return;
  Require(state != nullptr, "AI action reaction requires a valid actor state");
  const LiveAppliedAction applied(*request);
  LiveActorReaction host(*state);
  Require(ApplyActorActionReaction(host, applied.Action()),
          "AI action reaction requires a valid action and acting actor");
}

void PropagateLiveActorRelationships(Battle_Ai_ReactionView* state) {
  Require(state != nullptr,
          "AI relationship propagation requires a valid actor state");
  LiveActorReaction host(*state);
  const ReactionSettings settings = host.Settings();
  if (!settings.allied_with_ashley && !settings.shares_ally_hostility) return;
  const RoomActorList actors;
  Require(PropagateActorRelationships(host, actors.Ids()),
          "AI relationship propagation requires every room actor's state");
}

void NotifyLiveActionRelationshipChanges(Battle_ActionRequest* request) {
  if (request == nullptr) return;
  LiveActionNotification host(*request);
  Require(
      NotifyActionRelationshipChanges(host, ReadHit(request->primaryTarget)),
      "AI action notification actor is outside the AI slots");
}
}  // namespace VS::Battle::Ai
