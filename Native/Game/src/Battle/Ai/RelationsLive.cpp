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

#include "Battle/Action.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/DecisionsLive.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Battle/Ai/OrbitingBoss.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Battle/Damage.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/RandomLive.hpp"

using VS::Battle::ActionDefinition;
using VS::Battle::ActionRequestRecord;

namespace VS::Battle::Ai {
namespace {
static_assert(std::size(VS::Battle::Ai::actorAiStates) == kRelationActorSlots);
constexpr unsigned kPreservedRelationFlag = 8;

// The room actor IDs in battle actor list order.
class RoomActorList {
 public:
  RoomActorList() {
    for (const VS::Battle::Actor* actor = VS::Battle::actors[0];
         actor != nullptr; actor = actor->next) {
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

VS::Battle::Ai::ActorAiState& TableState(int actor) {
  VS::Battle::Ai::ActorAiState* state =
      VS::Battle::Ai::actorAiStates[static_cast<std::size_t>(actor)];
  Require(state != nullptr, "AI relations require every room actor's state");
  return *state;
}

class LiveRelationTable final : public ActorRelationTable {
 public:
  [[nodiscard]] std::span<const std::uint8_t> RoomActors() const override {
    return actors_.Ids();
  }
  [[nodiscard]] std::uint8_t Category(int actor) const override {
    return State(actor).relation_category;
  }
  [[nodiscard]] unsigned Classification(int source, int target) const override {
    return State(source).relations[target].classification;
  }
  void SetClassification(int source, int target, unsigned value) override {
    State(source).relations[target].classification = value;
  }

 private:
  [[nodiscard]] static VS::Battle::Ai::ActorAiState& State(int actor) {
    return TableState(actor);
  }

  RoomActorList actors_;
};

// The original reads the source through `state` and every target through the
// table; requiring the registration makes both the same state.
void InitializeRelations(VS::Battle::Ai::ActorAiState& state) {
  Require(state.actor_id < kRelationActorSlots &&
              VS::Battle::Ai::actorAiStates[state.actor_id] == &state,
          "AI relations require a registered actor state");
  LiveRelationTable table;
  Require(InitializeActorAiRelations(table, state.actor_id),
          "AI relations require a valid relation category");
}

class LiveAllianceActor final : public AshleyAllianceActor {
 public:
  explicit LiveAllianceActor(VS::Battle::Ai::ReactionView& reaction)
      : reaction_(reaction),
        settings_(*reinterpret_cast<VS::Battle::Ai::SettingsView*>(&reaction)),
        state_(*reinterpret_cast<VS::Battle::Ai::ActorAiState*>(&reaction)) {}

  [[nodiscard]] std::uint8_t ReactionCount() const override {
    return reaction_.alliance_reaction_count;
  }
  void SetReactionCount(std::uint8_t count) override {
    reaction_.alliance_reaction_count = count;
  }
  [[nodiscard]] bool AllianceActive() const override {
    return reaction_.temporary_ashley_alliance != 0;
  }
  void SetAllianceActive(bool active) override {
    reaction_.temporary_ashley_alliance = active ? 1 : 0;
  }
  void SetAwarenessIndicator(int marker) override {
    SetLiveActorAwarenessIndicator(reaction_.actor_id, marker);
  }
  void RecomputeRelations() override { InitializeRelations(state_); }
  void SaveDecisionAndApproach() override {
    settings_.saved_decision_callback = settings_.decision_callback;
    settings_.decision_callback = UpdateLiveEngagementApproachBehavior;
  }
  void RestoreSavedDecision() override {
    if (settings_.saved_decision_callback != nullptr) {
      settings_.decision_callback = settings_.saved_decision_callback;
    }
    settings_.saved_decision_callback = nullptr;
  }
  [[nodiscard]] bool RecoveryDisabled() const override {
    return reaction_.hostile_on_harm != 0;
  }
  void SetTemporaryAwareness(int marker, int frames) override {
    SetLiveTemporaryAwarenessIndicator(&state_, marker, frames);
  }
  void QueueReaction(int command, int range) override {
    SetLiveActorAiReactionWithDelay(
        reinterpret_cast<VS::Battle::Ai::PerceptionUpdateView*>(&reaction_),
        command, range);
  }
  [[nodiscard]] int Roll() override { return VS::Core::NextBiosRand(); }

 private:
  VS::Battle::Ai::ReactionView& reaction_;
  VS::Battle::Ai::SettingsView& settings_;
  VS::Battle::Ai::ActorAiState& state_;
};

VS::Battle::Ai::ReactionView& RequireAllianceState(
    VS::Battle::Ai::ReactionView* state) {
  Require(state != nullptr && state->actor_id < kRelationActorSlots,
          "Ashley alliance requires a valid actor state");
  return *state;
}

class LiveSavedRelationActors final : public SavedRelationActors {
 public:
  [[nodiscard]] std::span<const std::uint8_t> RoomActors() const override {
    return actors_.Ids();
  }
  [[nodiscard]] std::int16_t SavedOptions(int actor) const override {
    return TableState(actor).saved_relation_options;
  }
  void RestoreAshleyRelation(int actor,
                             const SavedAshleyRelation& saved) override {
    VS::Battle::Ai::RelationWord& relation = TableState(actor).relations[0];
    relation.active = 0;
    relation.perception_flags &= kPreservedRelationFlag;
    relation.previous_classification = saved.previous_classification;
    relation.classification = saved.classification;
    relation.classification_timer = saved.classification_timer;
  }
  void BeginForcedAlliance(int actor) override {
    LiveAllianceActor alliance(
        *reinterpret_cast<VS::Battle::Ai::ReactionView*>(&TableState(actor)));
    TryBeginAshleyAlliance(alliance, true);
  }

 private:
  RoomActorList actors_;
};

// The actors after Ashley with their AI group fields, in list order.
class LiveLinkedActorTable final : public LinkedActorTable {
 public:
  LiveLinkedActorTable() {
    const VS::Battle::Actor* head = VS::Battle::actors[0];
    Require(head != nullptr,
            "AI linked actors require Ashley at the battle actor list head");
    for (const VS::Battle::Actor* actor = head->next; actor != nullptr;
         actor = actor->next) {
      // The head takes one of the kRelationActorSlots list entries.
      Require(count_ + 1 < kRelationActorSlots && actor->id >= 0 &&
                  actor->id < kRelationActorSlots,
              "AI linked actors require a bounded battle actor list");
      actors_.at(count_++) = {
          .id = static_cast<std::uint8_t>(actor->id),
          .group = static_cast<std::uint8_t>(actor->ai_group),
          .linked_group = static_cast<std::uint8_t>(actor->ai_linked_group)};
    }
  }

  [[nodiscard]] std::span<const LinkGroupActor> FollowingActors()
      const override {
    return std::span(actors_).first(count_);
  }
  [[nodiscard]] std::optional<int> LinkedActor(int actor_id) const override {
    const VS::Battle::Ai::ActorAiState* state =
        VS::Battle::Ai::actorAiStates[static_cast<std::size_t>(actor_id)];
    if (state == nullptr) return std::nullopt;
    return state->linked_actor;
  }
  void SetLinkedActor(int actor_id, int linked) override {
    TableState(actor_id).linked_actor = static_cast<signed char>(linked);
  }

 private:
  std::array<LinkGroupActor, kRelationActorSlots> actors_{};
  std::size_t count_ = 0;
};

// The reaction view overlays the actor AI state.
static_assert(offsetof(VS::Battle::Ai::ReactionView, relations) ==
              offsetof(VS::Battle::Ai::ActorAiState, relations));
static_assert(sizeof(VS::Battle::Ai::RelationWordView) ==
              sizeof(VS::Battle::Ai::RelationWord));
using ActionTargets = decltype(ActionRequestRecord::targets);
constexpr std::size_t kMaxActionHits = std::tuple_size_v<ActionTargets>;

void RequireSlot(int actor) {
  Require(actor >= 0 && actor < kRelationActorSlots,
          "AI reactions require an actor inside the AI slots");
}

ReactionSettings ReadSettings(const VS::Battle::Ai::ReactionView& view) {
  return {.allied_with_ashley = view.temporary_ashley_alliance != 0,
          .settings_bit25 = view.hostile_on_harm != 0,
          .settings_bit26 = view.hostile_on_grievance != 0,
          .settings_bit27 = view.can_ally_with_ashley != 0,
          .shares_ally_hostility = view.shares_ally_hostility != 0,
          .category = view.relation_category};
}

VS::Battle::Ai::ReactionView* PeerView(int actor) {
  RequireSlot(actor);
  return reinterpret_cast<VS::Battle::Ai::ReactionView*>(
      VS::Battle::Ai::actorAiStates[static_cast<std::size_t>(actor)]);
}

class LiveActorReaction final : public ActorReactionHost {
 public:
  explicit LiveActorReaction(VS::Battle::Ai::ReactionView& state)
      : state_(state) {}

  [[nodiscard]] int ActorId() const override { return state_.actor_id; }
  [[nodiscard]] std::uint8_t BehaviorMode() const override {
    return state_.behavior_mode;
  }
  void SetBehaviorMode(std::uint8_t mode) override {
    state_.behavior_mode = mode;
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
    const VS::Battle::Ai::ReactionView* peer = PeerView(actor);
    if (peer == nullptr) return std::nullopt;
    return ReadSettings(*peer);
  }
  [[nodiscard]] std::optional<std::uint32_t> PeerRelation(
      int actor, int target) const override {
    VS::Battle::Ai::ReactionView* peer = PeerView(actor);
    if (peer == nullptr) return std::nullopt;
    return Word(*peer, target);
  }
  void SetPeerRelation(int actor, int target, std::uint32_t word) override {
    VS::Battle::Ai::ReactionView* peer = PeerView(actor);
    Require(peer != nullptr, "AI reactions require the peer's state");
    Word(*peer, target) = word;
  }
  [[nodiscard]] bool IsBeneficial(int action) const override {
    return IsLiveActionBeneficialToActor(action, state_.actor_id) != 0;
  }
  [[nodiscard]] bool IsHarmful(int action) const override {
    return IsLiveActionHarmfulToActor(action, state_.actor_id) != 0;
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
    if (VS::Battle::Ai::arenaOrbit != nullptr) {
      VS::Battle::Ai::arenaOrbit->attacks_after_retreat = 0;
    }
  }

 private:
  static std::uint32_t& Word(VS::Battle::Ai::ReactionView& view, int target) {
    RequireSlot(target);
    return view.relations[static_cast<std::size_t>(target)].word;
  }

  VS::Battle::Ai::ReactionView& state_;
};

ReactionHit ReadHit(const VS::Battle::HitResult& hit) {
  return {.actor = hit.target.target_actor,
          .hp_change = hit.target.hp,
          .hp_effect = hit.hp_effect,
          .skipped = hit.targets_position != 0};
}

// The completed action as the reaction policy reads it.
class LiveAppliedAction {
 public:
  explicit LiveAppliedAction(const ActionRequestRecord& request) {
    Require(request.target_count <= kMaxActionHits,
            "AI action reaction requires a bounded hit list");
    for (std::size_t i = 0; i < request.target_count; ++i) {
      hits_.at(i) = ReadHit(request.targets[i]);
    }
    action_.action = request.action_id;
    if (request.action_id < std::size(VS::Battle::actionDefinitions)) {
      const ActionDefinition& definition =
          VS::Battle::actionDefinitions[request.action_id];
      action_.hit_prerequisites = std::array<std::uint8_t, 2>{
          static_cast<std::uint8_t>(
              definition.hits[0].prerequisite_function_id),
          static_cast<std::uint8_t>(
              definition.hits[1].prerequisite_function_id)};
    }
    action_.acting = ReadHit(request.primary_target);
    action_.targets = std::span(hits_).first(request.target_count);
  }

  [[nodiscard]] const AppliedAction& Action() const { return action_; }

 private:
  std::array<ReactionHit, kMaxActionHits> hits_{};
  AppliedAction action_;
};

class LiveActionNotification final : public ActionNotificationHost {
 public:
  explicit LiveActionNotification(ActionRequestRecord& request)
      : request_(request) {}

  void ClearReengagementGrace(int actor) override {
    VS::Battle::Ai::ActorAiState* state =
        VS::Battle::Ai::actorAiStates[static_cast<std::size_t>(actor)];
    if (state != nullptr) state->reengagement_grace_frames = 0;
  }
  void ApplyRelationshipChanges() override {
    // The original passes the request as func_800DEEA4_t.
    ApplyLiveActionRelationshipChanges(
        reinterpret_cast<VS::Battle::Ai::ReactionRecord*>(&request_));
  }

 private:
  ActionRequestRecord& request_;
};
}  // namespace

void InitializeLiveActorAiRelations(VS::Battle::Ai::AiCreationView* state) {
  auto* initial = reinterpret_cast<VS::Battle::Ai::ActorAiState*>(state);
  Require(initial != nullptr, "AI relations require a valid actor state");
  InitializeRelations(*initial);
}

void ApplyLiveSavedActorAiRelations() {
  LiveSavedRelationActors actors;
  ApplySavedActorAiRelations(actors);
}

void EndLiveAshleyAlliance(VS::Battle::Ai::ReactionView* state) {
  LiveAllianceActor actor(RequireAllianceState(state));
  EndAshleyAlliance(actor);
}

void TryBeginLiveAshleyAlliance(VS::Battle::Ai::ReactionView* state,
                                int forced) {
  LiveAllianceActor actor(RequireAllianceState(state));
  TryBeginAshleyAlliance(actor, forced != 0);
}

void ResolveLiveAiLinkedActors() {
  LiveLinkedActorTable actors;
  Require(ResolveAiLinkedActors(actors),
          "AI linked actors require every listed actor's state");
}

void ApplyLiveActorActionReaction(VS::Battle::Ai::ReactionView* state,
                                  const ActionRequestRecord* request) {
  if (request == nullptr) return;
  Require(state != nullptr, "AI action reaction requires a valid actor state");
  const LiveAppliedAction applied(*request);
  LiveActorReaction host(*state);
  Require(ApplyActorActionReaction(host, applied.Action()),
          "AI action reaction requires a valid action and acting actor");
}

void PropagateLiveActorRelationships(VS::Battle::Ai::ReactionView* state) {
  Require(state != nullptr,
          "AI relationship propagation requires a valid actor state");
  LiveActorReaction host(*state);
  const ReactionSettings settings = host.Settings();
  if (!settings.allied_with_ashley && !settings.shares_ally_hostility) return;
  const RoomActorList actors;
  Require(PropagateActorRelationships(host, actors.Ids()),
          "AI relationship propagation requires every room actor's state");
}

void NotifyLiveActionRelationshipChanges(ActionRequestRecord* request) {
  if (request == nullptr) return;
  LiveActionNotification host(*request);
  Require(
      NotifyActionRelationshipChanges(host, ReadHit(request->primary_target)),
      "AI action notification actor is outside the AI slots");
}
}  // namespace VS::Battle::Ai
