// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionResultsLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>

#include "Actors/ActorRecord.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Battle/Action.h"
#include "Battle/ActionAnimation.hpp"
#include "Battle/ActionAnimationsLive.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionResults.hpp"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/ActorVitalsLive.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Feedback.hpp"
#include "Battle/FloatingLabelPlacementLive.hpp"
#include "Battle/FloatingLabelProducersLive.hpp"
#include "Battle/FloatingLabelQueueLive.hpp"
#include "Battle/Hit.hpp"
#include "Battle/StatusEffects.hpp"
#include "Battle/StatusHelpersLive.hpp"
#include "Battle/StatusLifecycleLive.hpp"
#include "Battle/TargetingState.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Log.hpp"
#include "Legacy/Psx.h"

namespace VS::Battle {
namespace {
constexpr int kRecoveryPose = 6;
constexpr std::size_t kWorldAxes = 3;

class LiveActionTargetEffects final : public ActionTargetEffects {
 public:
  // `action` is null for world target ids, which read no hits.
  LiveActionTargetEffects(int actor_id, Battle_ActionRequest* action,
                          SVECTOR* position)
      : actor_id_(actor_id), action_(action), position_(position) {}

  void QueueWorldLabel() override {
    if (position_ == nullptr) {
      VS::Core::LogError("combat", "World target {} has no label position",
                         actor_id_);
      std::abort();
    }
    std::array<u_short, kWorldAxes> world = {
        static_cast<u_short>(position_->vx),
        static_cast<u_short>(position_->vy),
        static_cast<u_short>(position_->vz)};
    QueueLiveWorldLockedLabel(world);
  }

  std::size_t HitCount() override {
    if (action_->targetCount > std::size(action_->targets)) {
      VS::Core::LogError("combat", "Invalid action target count {}",
                         action_->targetCount);
      std::abort();
    }
    return action_->targetCount;
  }

  ActionTargetHit Hit(std::size_t index) override {
    const VS::Battle::HitResult& hit = action_->targets[index];
    return {.targets_position = hit.targets_position != 0,
            .target_actor = hit.target.target_actor,
            .impact_kind = static_cast<std::uint8_t>(hit.target.impact_kind)};
  }

  bool IsAreaAction() override {
    return VS::Battle::categoryIsAreaTarget
               [Battle_actionDefinitions[action_->actionId].category] != 0;
  }

  void ApplyHit(std::size_t index) override {
    VS::Battle::HitResult* hit = &action_->targets[index];
    VS::Core::Debugf("combat.result",
                     "action=%u target=%d limb=%u impact=%u hp=%d mp=%d "
                     "hp_effect=%u mp_effect=%u",
                     action_->actionId, actor_id_, hit->target.target_limb,
                     hit->target.impact_kind, hit->target.hp, hit->target.mp,
                     hit->hp_effect, hit->mp_effect);
    ApplyLiveActorActionResult(hit);
  }

  void PlayDamageReaction(int actor_id,
                          std::size_t visited_hit_count) override {
    PlayLiveTargetDamageReaction(
        actor_id, &VS::Battle::actionState->rangeOrigin, visited_hit_count);
  }

  bool HasActorRecord(int actor_id) override {
    return VS::Actors::actorRecords[actor_id] != nullptr;
  }

  void PlayDefense(int actor_id) override {
    PlayLiveDefenseAnimation(actor_id, &VS::Battle::actionState->rangeOrigin);
  }

  void PlayRecoveryPose() override {
    VS::Actors::PlayLiveActorActionPose(0, nullptr, kRecoveryPose);
  }

 private:
  int actor_id_;
  Battle_ActionRequest* action_;
  SVECTOR* position_;
};

ActorActionResult ReadResult(const VS::Battle::HitResult& hit) {
  ActorActionResult result{
      .target = static_cast<std::uint8_t>(hit.target.target_actor),
      .limb = std::bit_cast<std::int8_t>(
          static_cast<std::uint8_t>(hit.target.target_limb)),
      .impact = static_cast<std::uint8_t>(hit.target.impact_kind),
      .changes = {{
          {hit.target.hp, static_cast<std::uint8_t>(hit.hp_effect)},
          {hit.target.mp, static_cast<std::uint8_t>(hit.mp_effect)},
          {hit.risk_change, static_cast<std::uint8_t>(hit.risk_effect)},
          {hit.strength_change, static_cast<std::uint8_t>(hit.strength_effect)},
          {hit.intelligence_change,
           static_cast<std::uint8_t>(hit.intelligence_effect)},
          {hit.agility_change, static_cast<std::uint8_t>(hit.agility_effect)},
          {hit.max_hp_change, static_cast<std::uint8_t>(hit.max_hp_effect)},
          {hit.max_mp_change, static_cast<std::uint8_t>(hit.max_mp_effect)},
          {hit.weapon_dp_change,
           static_cast<std::uint8_t>(hit.weapon_dp_effect)},
          {hit.weapon_pp_change,
           static_cast<std::uint8_t>(hit.weapon_pp_effect)},
          {hit.armor_dp_change, static_cast<std::uint8_t>(hit.armor_dp_effect)},
          {hit.shield_pp_change,
           static_cast<std::uint8_t>(hit.shield_pp_effect)},
      }},
      .inflict_statuses = std::bit_cast<std::uint32_t>(hit.inflict_statuses),
      .remove_statuses = std::bit_cast<std::uint32_t>(hit.remove_statuses)};
  for (std::size_t index = 0; index < result.limbs.size(); ++index) {
    result.limbs.at(index) = {hit.limbs[index].effects.opaque0,
                              hit.limbs[index].effects.opaque2};
  }
  return result;
}

class LiveActorActionResultWorld final : public ActorActionResultWorld {
 public:
  explicit LiveActorActionResultWorld(VS::Battle::HitResult& hit) : hit_(hit) {}
  void CommitEquipmentStats() override { LiveCommitEquipmentStatChanges(); }
  ActorActionResult ReadHit() override { return ReadResult(hit_); }
  ActorVitalsAccess* TargetStats(std::uint8_t slot) override {
    if (slot >= std::size(VS::Battle::actors) ||
        VS::Battle::actors[slot] == nullptr ||
        VS::Battle::actors[slot]->stats == nullptr) {
      return nullptr;
    }
    stats_.emplace(*VS::Battle::actors[slot]->stats);
    return &stats_.value();
  }
  void Number(int slot, int limb, std::uint32_t amount, int label) override {
    QueueLiveActorStatChangeNumber(slot, limb, std::bit_cast<int>(amount),
                                   label);
  }
  void StatusLabel(int slot, std::uint32_t mask, bool removing) override {
    LiveQueueActorStatusLabels(slot, 0, std::bit_cast<int>(mask), removing);
  }
  void RemoveStatuses(ActorVitalsAccess& stats, std::uint32_t mask) override {
    auto& owner = static_cast<LiveActorVitalsAccess&>(stats).Owner();
    RemoveLiveActorStatuses(std::bit_cast<int>(mask), &owner);
  }
  void InflictStatuses(ActorVitalsAccess& stats, std::uint32_t mask) override {
    auto& owner = static_cast<LiveActorVitalsAccess&>(stats).Owner();
    InflictLiveActorStatuses(std::bit_cast<int>(mask), &owner);
  }

 private:
  VS::Battle::HitResult& hit_;
  std::optional<LiveActorVitalsAccess> stats_;
};

}  // namespace

// decomp: BATTLE.PRG func_8006CB04 (146C/func_8006CB04.s).
void ApplyLiveActionTargetEffects(int actor_id, SVECTOR* position) {
  if (actor_id >= kActionActorSlots) {
    LiveActionTargetEffects effects(actor_id, nullptr, position);
    ApplyActionTargetEffects(actor_id, effects);
    return;
  }
  if (actor_id < 0 || VS::Battle::actionState == nullptr) {
    VS::Core::LogError(
        "combat", "Invalid action target {} or missing action state", actor_id);
    std::abort();
  }
  const std::size_t queue_mask = std::size(VS::Battle::actionState->queue) - 1;
  Battle_ActionRequest& action =
      VS::Battle::actionState
          ->queue[VS::Battle::actionState->queueIndex & queue_mask];
  if (action.targetCount > std::size(action.targets) ||
      action.actionId >= std::size(Battle_actionDefinitions)) {
    VS::Core::LogError("combat", "Invalid action {} with {} targets",
                       action.actionId, action.targetCount);
    std::abort();
  }
  LiveActionTargetEffects effects(actor_id, &action, position);
  ApplyActionTargetEffects(actor_id, effects);
}

// decomp: _applyBattleAbilityEffect (src/BATTLE/BATTLE.PRG/146C.c:14018).
void ApplyLiveActorActionResult(VS::Battle::HitResult* hit) {
  if (hit == nullptr) {
    VS::Core::LogError("combat", "Action result requires a hit");
    std::abort();
  }
  LiveActorActionResultWorld world(*hit);
  if (!ApplyActorActionResult(world)) {
    VS::Core::LogError("combat",
                       "Action result requires valid target stats/used limb");
    std::abort();
  }
}
}  // namespace VS::Battle
