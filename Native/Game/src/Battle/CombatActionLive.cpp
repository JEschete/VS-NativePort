#include "Battle/CombatActionLive.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "Actors/ActorRecord.hpp"
#include "Actors/ActorSpawningLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionCameraSelectionLive.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionResolution.hpp"
#include "Battle/Actor.hpp"
#include "Battle/ActorVitalsLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiState.hpp"
#include "Battle/Ai/FrameUpdate.hpp"
#include "Battle/Damage.hpp"
#include "Battle/DamageCalculationsLive.hpp"
#include "Battle/FloatingLabelPlacementLive.hpp"
#include "Content/DataStore.hpp"
#include "Core/Log.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/SegmentCollisionLive.hpp"
#include "Items/EquipmentStats.hpp"
#include "Items/ItemRandomLive.hpp"
#include "State/Progress.hpp"
#include "State/ScriptFlagsLive.hpp"

namespace VS::Battle {
namespace {
constexpr unsigned kStoryProgressFlag = 0x82;
constexpr std::uint32_t kPositionKindLimit = 6;
constexpr unsigned kStatusFeedbackShift = 16;
constexpr std::uint32_t kEquipmentStatDecrease = 0x80000001U;
constexpr std::uint32_t kQueueMask = 3;

class InputRefusal final : public std::runtime_error {
 public:
  explicit InputRefusal(std::string_view reason)
      : std::runtime_error(std::string(reason)) {}
};

void Require(bool valid, std::string_view reason) {
  if (!valid) throw InputRefusal(reason);
}

struct LiveOwner {
  Actors::SpawnedActor identity;
  std::uint8_t slot;
  Actor& actor;
  Actors::ActorInstanceComponents components;
};

LiveOwner Owner(Actors::SpawnedActor identity) {
  const auto slot = Actors::LiveActorSlot(identity);
  auto* actor = Actors::ResolveLiveActorInstance(identity);
  const auto components = Actors::LiveActorComponents(identity);
  if (!slot || *slot >= kActionActorSlots || actor == nullptr || !components ||
      components->stats == nullptr) {
    throw InputRefusal(
        "Combat requires a published actor and its expected stats");
  }
  return {identity, static_cast<std::uint8_t>(*slot), *actor, *components};
}

Actors::SpawnedActor Identity(int slot) {
  Require(slot >= 0 && slot < kActionActorSlots,
          "Combat actor slot is outside the roster");
  const auto identity = Actors::LiveActorIdentityAt(slot);
  Require(identity != Actors::SpawnedActor::kNone,
          "Combat actor slot has no published identity");
  return identity;
}

CombatActorState Read(const LiveOwner& owner) {
  const auto& stats = *owner.components.stats;
  CombatActorState result{
      .identity = owner.identity,
      .slot = owner.slot,
      .weapon_drawn = std::bit_cast<std::uint32_t>(owner.actor.weapon_drawn),
      .spawn = owner.actor.room_spawn_index,
      .cancels_results = stats.cancels_hit_results != 0,
      .kill_counted = owner.actor.kill_counted != 0,
      .damage = ReadLiveDamageStats(stats),
      .hp = stats.current_hp,
      .max_hp = stats.max_hp,
      .mp = stats.current_mp,
      .max_mp = stats.max_mp,
      .agility = stats.agility,
      .statuses = std::bit_cast<std::uint32_t>(stats.statuses),
      .immunities = std::bit_cast<std::uint32_t>(stats.status_immunities),
      .mobility = {static_cast<std::uint8_t>(stats.mobility_limb0),
                   static_cast<std::uint8_t>(stats.mobility_limb1)},
      .default_limb = static_cast<std::uint8_t>(stats.default_target_limb),
      .recovery_timer = stats.action_recovery_timer,
      .recovery_bonus = stats.recovery_delay_bonus};
  for (std::size_t i = 0; i < result.limbs.size(); ++i) {
    const auto& limb = stats.limbs.at(i);
    const auto damage = ReadLiveDamageLimb(stats, static_cast<int>(i));
    if (!damage) throw InputRefusal("Combat limb is outside the actor");
    result.limbs.at(i) = {*damage,
                          {limb.hp, limb.max_hp},
                          limb.agility_defense_bonus,
                          limb.name_index};
    for (std::size_t j = 0; j < result.limbs.size(); ++j) {
      result.armor_splash.at(i).at(j) =
          std::bit_cast<std::uint8_t>(stats.dp_splash.at(i).at(j));
    }
  }
  return result;
}

std::vector<CombatActorState> ReadActors(const CombatActionIntent& intent) {
  std::vector<CombatActorState> result;
  const auto add = [&](Actors::SpawnedActor id) {
    if (std::ranges::find(result, id, &CombatActorState::identity) ==
        result.end()) {
      result.push_back(Read(Owner(id)));
    }
  };
  add(intent.source);
  for (const auto& target : intent.targets) {
    if (const auto* actor = std::get_if<CombatActorTarget>(&target)) {
      add(actor->actor);
    }
  }
  return result;
}

CombatKillProgress Progress(const CombatActorState& source,
                            const Content::DataStore& store) {
  if (source.slot != 0) return {};
  const auto category = Owner(source.identity).actor.equipped_weapon_category;
  const auto index = category % Items::kUnarmedWeaponCategory;
  const auto learned = State::artsStatus.arts_learned[index];
  const auto* learning = store.Learning();
  return {.story_progress = State::LiveScriptFlag(kStoryProgressFlag),
          .weapon_category = category,
          .learned_arts = learned,
          .weapon_kills = State::artsStatus.kills.weapon_category_kills[index],
          .required_kills = learning
                                ? learning->BreakArtRequirement(index, learned)
                                : std::nullopt,
          .weapon_attacks = State::scoreData.weapon_attacks[index]};
}

void CommitCalculation(const CombatActionResult& result) {
  std::vector<LiveOwner> owners;
  for (const auto& actor : result.actors) {
    auto owner = Owner(actor.identity);
    Require(owner.slot == actor.slot,
            "Combat actor moved before calculation commit");
    owners.push_back(owner);
  }
  for (std::size_t i = 0; i < result.actors.size(); ++i) {
    const auto& value = result.actors.at(i);
    const auto& owner = owners.at(i);
    auto& stats = *owner.components.stats;
    stats.current_hp = value.hp;
    stats.current_mp = value.mp;
    stats.risk = value.damage.risk;
    stats.weapon = value.damage.weapon;
    stats.shield = value.damage.shield;
    stats.action_recovery_timer = value.recovery_timer;
    owner.actor.kill_counted = value.kill_counted;
    for (std::size_t limb = 0; limb < value.limbs.size(); ++limb) {
      stats.limbs.at(limb).armor = value.limbs.at(limb).damage.armor;
    }
  }
  if (result.actors.front().slot == 0) {
    const auto index =
        result.progress.weapon_category % Items::kUnarmedWeaponCategory;
    State::artsStatus.kills.weapon_category_kills[index] =
        result.progress.weapon_kills;
    State::scoreData.weapon_attacks[index] = result.progress.weapon_attacks;
  }
}

struct Session {
  CombatActionSession actions;
  std::map<const ActionRequestRecord*, CombatActionId> requests;
};

Session& Pending() {
  static Session session;
  return session;
}

const Content::DataStore& Store() {
  const auto* store = Content::DataStore::Get();
  Require(store != nullptr && store->Actions() != nullptr &&
              store->StatusRules() != nullptr,
          "Combat catalogs are not loaded");
  return *store;
}

class Trajectory final : public ActionTrajectoryWorld {
 public:
  std::optional<ActionPoint> TargetPosition(int slot, int limb) override {
    ValidateModel(slot);
    if (limb < -1 || limb >= static_cast<int>(kActionLimbCount)) {
      return std::nullopt;
    }
    Core::Gte::SVECTOR position{};
    LiveActionTargetLimbPosition(slot, limb, &position);
    return std::bit_cast<ActionPoint>(position);
  }
  std::optional<std::uint8_t> DefaultLimb(int slot) override {
    return static_cast<std::uint8_t>(
        Owner(Identity(slot)).components.stats->default_target_limb);
  }
  std::optional<ActionPoint> Trace(ActionPoint origin, ActionPoint target,
                                   int source) override {
    const auto hit = Field::TraceLiveOwnedActorAndTerrainCollision(
        {origin.x, origin.y, origin.z}, {target.x, target.y, target.z}, source);
    if (!hit) return std::nullopt;
    return ActionPoint{
        hit->x, hit->y, hit->z,
        static_cast<std::int16_t>(hit->actor_slot ? *hit->actor_slot + 1 : 0)};
  }
  ActionPoint Origin(int slot) {
    ValidateModel(slot);
    Core::Gte::SVECTOR position{};
    Require(Actors::LiveActorBonePosition(slot, 0, &position, 0) >= 0,
            "Combat trajectory source has no position");
    return std::bit_cast<ActionPoint>(position);
  }

 private:
  static void ValidateModel(int slot) {
    const auto owner = Owner(Identity(slot));
    // The remaining skeleton query is a checked geometry producer, never a
    // calculator dependency. Do not let its borrowed views select another body.
    Require(owner.actor.stats == owner.components.stats &&
                Actors::actorRecords[slot] == owner.components.body &&
                Field::gimmickInstances[slot] == owner.components.gimmick,
            "Combat trajectory components no longer belong to their actor");
  }
};

CombatActionIntent Prepare(const ActionRequest& source, std::uint16_t action,
                           const Content::ActionRule& rule, bool execute) {
  Require(source.count <= source.targets.size(),
          "Combat target count exceeds request capacity");
  CombatActionIntent intent{
      .control = source.primary.actor == 0 ? CombatControl::kPlayer
                                           : CombatControl::kAi,
      .source = Identity(source.primary.actor),
      .source_limb = std::bit_cast<std::int8_t>(source.primary.limb),
      .action = action,
      .targets = {},
      .focus = source.focus};
  ActionRequest trajectory{};
  trajectory.primary.actor = source.primary.actor;
  Trajectory world;
  const bool redirects =
      execute && (rule.behavior & Content::kBehaviorHitsFirstObstruction) != 0;
  std::optional<ActionPoint> origin;
  if (redirects) origin = world.Origin(source.primary.actor);
  for (std::size_t i = 0; i < source.count; ++i) {
    const auto& input = source.targets.at(i);
    ActionHit hit{};
    hit.position_kind = input.position_kind;
    if (hit.position_kind == 0) {
      hit.actor = input.actor;
      hit.limb = input.limb;
    } else if (static_cast<std::uint32_t>(hit.position_kind) <
               kPositionKindLimit) {
      hit.SetPosition(input.Position());
    }
    if (redirects) {
      Require(RedirectActionTarget(trajectory, hit, origin, world).has_value(),
              "Combat target trajectory could not be resolved");
    }
    if (hit.position_kind == 0) {
      intent.targets.emplace_back(CombatActorTarget{
          Identity(hit.actor), std::bit_cast<std::int8_t>(hit.limb)});
    } else {
      intent.targets.emplace_back(CombatWorldTarget{
          static_cast<std::uint8_t>(hit.position_kind), hit.Position()});
    }
  }
  intent.redirected = trajectory.animation != 0;
  return intent;
}

ActionRequestRecord Project(const CombatActionResult& result) {
  ActionRequest request{};
  request.action = result.intent.action;
  request.affinity = result.affinity;
  request.damage_type = result.damage_type;
  request.primary = ProjectLiveCombatHit(result.source);
  request.count = static_cast<std::uint16_t>(result.hits.size());
  request.animation = result.intent.redirected;
  request.focus = result.intent.focus;
  for (std::size_t i = 0; i < result.hits.size(); ++i) {
    request.targets.at(i) = ProjectLiveCombatHit(result.hits.at(i));
  }
  return std::bit_cast<ActionRequestRecord>(request);
}
}  // namespace

ActionHit ProjectLiveCombatHit(const CombatHitResult& result) {
  ActionHit hit{};
  WriteActionHitResult(result.changes, hit);
  hit.threshold = result.threshold;
  hit.effects |= static_cast<std::uint32_t>(result.feedback)
                 << kStatusFeedbackShift;
  if (const auto* point = std::get_if<CombatWorldTarget>(&result.target)) {
    hit.position_kind = point->kind;
    hit.SetPosition(point->position);
  }
  return hit;
}

std::expected<CombatActorState, std::string> ReadLiveCombatActor(
    Actors::SpawnedActor actor) {
  try {
    return Read(Owner(actor));
  } catch (const InputRefusal& error) {
    return std::unexpected(error.what());
  }
}

std::expected<LiveCombatResolution, std::string> ResolveLiveCombatAction(
    const CombatActionIntent& intent, std::uint32_t chain_step, bool execute) {
  try {
    const auto& store = Store();
    const auto* definition = store.Actions()->Rule(intent.action);
    Require(definition != nullptr, "Combat action is outside the catalog");
    const auto actor_inputs = ReadActors(intent);
    Items::Live::LiveItemRandom random;
    CombatRuleInputs inputs{*definition,
                            actor_inputs,
                            random,
                            bladePpDamageBonuses,
                            store.StatusRules()->rules,
                            limbInjuryStatusIds,
                            Progress(actor_inputs.front(), store),
                            chain_step};
    auto result = ResolvePhysicalAction(intent, inputs, execute);
    if (!result) return std::unexpected(result.error());
    LiveCombatResolution resolved{.result = std::move(*result)};
    if (execute) {
      const auto id = Pending().actions.Submit(resolved.result);
      if (!id) return std::unexpected(id.error());
      try {
        CommitCalculation(resolved.result);
      } catch (const InputRefusal&) {
        Pending().actions.Forget(*id);
        throw;
      }
      resolved.id = *id;
    }
    return resolved;
  } catch (const InputRefusal& error) {
    return std::unexpected(error.what());
  }
}

std::expected<std::optional<std::uint32_t>, std::string>
TryResolveLiveCombatRequest(int action, ActionRequestRecord& result,
                            ActionRequestRecord& source, bool execute) {
  if (action != 1) return std::nullopt;
  try {
    const auto& store = Store();
    const auto* definition = store.Actions()->Rule(action);
    Require(definition != nullptr, "Normal combat action is missing");
    const auto input = &result == &source
                           ? ActionRequest{}
                           : std::bit_cast<ActionRequest>(source);
    if (input.primary.position_kind != 0 ||
        !IsPhysicalCombatRule(*definition)) {
      return std::nullopt;
    }
    Require(actionState != nullptr, "Combat request has no action state");
    const auto intent = Prepare(input, static_cast<std::uint16_t>(action),
                                *definition, execute);
    auto resolved =
        ResolveLiveCombatAction(intent, actionState->chain_step, execute);
    if (!resolved) return std::unexpected(resolved.error());
    ForgetLiveCombatRequest(result);
    result = Project(resolved->result);
    if (execute) {
      Pending().requests[&result] = resolved->id;
      Core::LogDebug("combat.action", "submitted id={} source={} action={}",
                     static_cast<std::uint64_t>(resolved->id),
                     resolved->result.source.changes.target, intent.action);
      if (actionState->queue_index == 0) source = result;
      for (const auto& notice : resolved->result.equipment_notices) {
        static_cast<void>(Owner(notice.actor));
        LiveQueueEquipmentStatChange(
            notice.label, notice.stat,
            notice.change == Items::StatChange::kIncrease
                ? 1
                : std::bit_cast<int>(kEquipmentStatDecrease));
      }
      for (const auto& warning : resolved->result.warnings) {
        Core::LogWarn("battle.learning", "weapon kill credit refused: {}",
                      warning);
      }
    }
    return resolved->result.availability;
  } catch (const InputRefusal& error) {
    return std::unexpected(error.what());
  }
}

CombatActionId LiveCombatActionForRequest(const ActionRequestRecord& request) {
  const auto& bindings = Pending().requests;
  const auto found = bindings.find(&request);
  return found == bindings.end() ? CombatActionId::kNone : found->second;
}

CombatActionId CurrentLiveCombatAction() {
  if (actionState == nullptr) return CombatActionId::kNone;
  return LiveCombatActionForRequest(
      actionState->queue[actionState->queue_index & kQueueMask]);
}

const CombatActionResult* FindLiveCombatAction(CombatActionId id) {
  return Pending().actions.Find(id);
}

ActionTargetHit LiveCombatTarget(const CombatHitResult& hit) {
  const auto* actor = std::get_if<CombatActorTarget>(&hit.target);
  const auto slot = actor != nullptr ? LiveCombatPresentationSlot(actor->actor)
                                     : std::nullopt;
  const bool retired =
      actor != nullptr && (!slot || *slot != hit.changes.target);
  return {.targets_position = actor == nullptr || retired,
          .target_actor = retired ? static_cast<std::uint8_t>(kActionActorSlots)
                                  : hit.changes.target,
          .impact_kind = hit.changes.impact};
}

std::optional<std::uint8_t> LiveCombatPresentationSlot(
    Actors::SpawnedActor identity) {
  const auto slot = Actors::LiveActorSlot(identity);
  const auto* actor = Actors::ResolveLiveActorInstance(identity);
  const auto components = Actors::LiveActorComponents(identity);
  if (!slot || *slot >= kActionActorSlots || actor == nullptr || !components ||
      components->stats == nullptr || actor->stats != components->stats ||
      Actors::actorRecords[*slot] != components->body ||
      Field::gimmickInstances[*slot] != components->gimmick ||
      Ai::actorAiStates[*slot] != Actors::LiveActorAiState(identity)) {
    Core::LogWarn("combat", "Presentation refused a retired or rebound actor");
    return std::nullopt;
  }
  return static_cast<std::uint8_t>(*slot);
}

bool LiveCombatPresentationAvailable(CombatActionId action) {
  if (action == CombatActionId::kNone) return true;
  const auto* result = FindLiveCombatAction(action);
  if (result == nullptr) {
    Core::LogWarn("combat", "Presentation belongs to an inactive action");
    return false;
  }
  return std::ranges::all_of(result->actors, [](const auto& actor) {
    return LiveCombatPresentationSlot(actor.identity).has_value();
  });
}

void ForgetLiveCombatRequest(const ActionRequestRecord& request) {
  auto& session = Pending();
  const auto found = session.requests.find(&request);
  if (found == session.requests.end()) return;
  ForgetLiveCombatAction(found->second);
}

void ForgetLiveCombatAction(CombatActionId action) {
  auto& session = Pending();
  std::erase_if(session.requests,
                [=](const auto& entry) { return entry.second == action; });
  session.actions.Forget(action);
}

void ResetLiveCombatActions() {
  Core::LogDebug("combat.action", "reset bound_requests={}",
                 Pending().requests.size());
  Pending().requests.clear();
  Pending().actions.Reset();
}

std::expected<CombatImpactResult, std::string> CommitLiveCombatImpact(
    CombatActionId action, std::optional<std::size_t> target) {
  const auto* result = FindLiveCombatAction(action);
  if (result == nullptr) {
    return std::unexpected("Combat impact belongs to an inactive action");
  }
  try {
    const auto source = Owner(result->intent.source);
    Require(!target || *target < result->hits.size(),
            "Combat impact index is outside the action");
    const auto& hit = target ? result->hits.at(*target) : result->source;
    const auto owner = Owner(Identity(hit.changes.target));
    auto owned_hit =
        target ? Pending().actions.TargetImpact(action, *target,
                                                source.identity, owner.identity)
               : Pending().actions.SourceImpact(action, source.identity);
    if (!owned_hit) return std::unexpected(owned_hit.error());
    LiveActorVitalsAccess access(*owner.components.stats);
    const auto state = access.Read();
    const auto& store = Store();
    Items::Live::LiveItemRandom random;
    auto applied =
        ApplyCombatImpact(*owned_hit, {owner.identity, owner.slot, state,
                                       store.StatusRules()->timings, random});
    if (!applied) return std::unexpected(applied.error());
    access.Write(applied->state);
    auto* ai = Actors::LiveActorAiState(owner.identity);
    for (const auto& event : applied->events) {
      const auto* movement = std::get_if<CombatMovementChange>(&event);
      if (ai == nullptr || movement == nullptr) continue;
      Ai::VerticalMovementSteps steps;
      Ai::SetActorVerticalMovementSteps(steps, movement->walk, movement->run);
      ai->walk_step = steps.walk_step;
      ai->run_step = steps.run_step;
      ai->max_range = steps.max_range;
    }
    return applied;
  } catch (const InputRefusal& error) {
    return std::unexpected(error.what());
  }
}
}  // namespace VS::Battle
