// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionResultsLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>

#include "Actors/AnimationControlLive.hpp"
#include "Battle/ActionAnimationsLive.hpp"
#include "Battle/ActionResults.hpp"
#include "Battle/ActorVitalsLive.hpp"
#include "Battle/StatusHelpersLive.hpp"
#include "Battle/StatusLifecycleLive.hpp"
#include "Core/Log.hpp"
#include "Legacy/Psx.h"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Action.h"
#include "Battle/ActionAnimation.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/Hit.h"
#include "Battle/StatusEffects.h"
#include "Battle/TargetingState.h"
#include "Core/Unported.h"
}

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
      VS_LOG_ERROR("combat", "World target {} has no label position",
                   actor_id_);
      std::abort();
    }
    std::array<u_short, kWorldAxes> world = {
        static_cast<u_short>(position_->vx),
        static_cast<u_short>(position_->vy),
        static_cast<u_short>(position_->vz)};
    Battle_QueueWorldLockedLabel(world.data());
  }

  std::size_t HitCount() override {
    if (action_->targetCount > std::size(action_->targets)) {
      VS_LOG_ERROR("combat", "Invalid action target count {}",
                   action_->targetCount);
      std::abort();
    }
    return action_->targetCount;
  }

  ActionTargetHit Hit(std::size_t index) override {
    const Battle_HitResult& hit = action_->targets[index];
    return {.targets_position = hit.targetsPosition != 0,
            .target_actor = hit.target.targetActor,
            .impact_kind = static_cast<std::uint8_t>(hit.target.impactKind)};
  }

  bool IsAreaAction() override {
    return Battle_categoryIsAreaTarget
               [Battle_actionDefinitions[action_->actionId].category] != 0;
  }

  void ApplyHit(std::size_t index) override {
    Battle_HitResult* hit = &action_->targets[index];
    Core_Debugf("combat.result",
                "action=%u target=%d limb=%u impact=%u hp=%d mp=%d "
                "hp_effect=%u mp_effect=%u",
                action_->actionId, actor_id_, hit->target.targetLimb,
                hit->target.impactKind, hit->target.hp, hit->target.mp,
                hit->hpEffect, hit->mpEffect);
    ApplyLiveActorActionResult(hit);
  }

  void PlayDamageReaction(int actor_id) override {
    Battle_PlayActorDamageReaction(actor_id, &Battle_actionState->rangeOrigin);
  }

  bool HasActorRecord(int actor_id) override {
    return Actors_actorRecords[actor_id] != nullptr;
  }

  void PlayDefense(int actor_id) override {
    PlayLiveDefenseAnimation(actor_id, &Battle_actionState->rangeOrigin);
  }

  void PlayRecoveryPose() override {
    VS::Actors::PlayLiveActorActionPose(0, nullptr, kRecoveryPose);
  }

 private:
  int actor_id_;
  Battle_ActionRequest* action_;
  SVECTOR* position_;
};

ActorActionResult ReadResult(const Battle_HitResult& hit) {
  ActorActionResult result{
      .target = static_cast<std::uint8_t>(hit.target.targetActor),
      .limb = std::bit_cast<std::int8_t>(
          static_cast<std::uint8_t>(hit.target.targetLimb)),
      .impact = static_cast<std::uint8_t>(hit.target.impactKind),
      .changes = {{
          {hit.target.hp, static_cast<std::uint8_t>(hit.hpEffect)},
          {hit.target.mp, static_cast<std::uint8_t>(hit.mpEffect)},
          {hit.riskChange, static_cast<std::uint8_t>(hit.riskEffect)},
          {hit.strengthChange, static_cast<std::uint8_t>(hit.strengthEffect)},
          {hit.intelligenceChange,
           static_cast<std::uint8_t>(hit.intelligenceEffect)},
          {hit.agilityChange, static_cast<std::uint8_t>(hit.agilityEffect)},
          {hit.maxHpChange, static_cast<std::uint8_t>(hit.maxHpEffect)},
          {hit.maxMpChange, static_cast<std::uint8_t>(hit.maxMpEffect)},
          {hit.weaponDpChange, static_cast<std::uint8_t>(hit.weaponDpEffect)},
          {hit.weaponPpChange, static_cast<std::uint8_t>(hit.weaponPpEffect)},
          {hit.armorDpChange, static_cast<std::uint8_t>(hit.armorDpEffect)},
          {hit.shieldPpChange, static_cast<std::uint8_t>(hit.shieldPpEffect)},
      }},
      .inflict_statuses = std::bit_cast<std::uint32_t>(hit.inflictStatuses),
      .remove_statuses = std::bit_cast<std::uint32_t>(hit.removeStatuses)};
  for (std::size_t index = 0; index < result.limbs.size(); ++index) {
    result.limbs.at(index) = {hit.limbs[index].effects.opaque0,
                              hit.limbs[index].effects.opaque2};
  }
  return result;
}

class LiveActorActionResultWorld final : public ActorActionResultWorld {
 public:
  explicit LiveActorActionResultWorld(Battle_HitResult& hit) : hit_(hit) {}
  void CommitEquipmentStats() override { Battle_CommitEquipmentStatChanges(); }
  ActorActionResult ReadHit() override { return ReadResult(hit_); }
  ActorVitalsAccess* TargetStats(std::uint8_t slot) override {
    if (slot >= std::size(Battle_actors) || Battle_actors[slot] == nullptr ||
        Battle_actors[slot]->stats == nullptr) {
      return nullptr;
    }
    stats_.emplace(*Battle_actors[slot]->stats);
    return &stats_.value();
  }
  void Number(int slot, int limb, std::uint32_t amount, int label) override {
    QueueLiveActorStatChangeNumber(slot, limb, std::bit_cast<int>(amount),
                                   label);
  }
  void StatusLabel(int slot, std::uint32_t mask, bool removing) override {
    Battle_QueueActorStatusLabels(slot, 0, std::bit_cast<int>(mask), removing);
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
  Battle_HitResult& hit_;
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
  if (actor_id < 0 || Battle_actionState == nullptr) {
    VS_LOG_ERROR("combat", "Invalid action target {} or missing action state",
                 actor_id);
    std::abort();
  }
  const std::size_t queue_mask = std::size(Battle_actionState->queue) - 1;
  Battle_ActionRequest& action =
      Battle_actionState->queue[Battle_actionState->queueIndex & queue_mask];
  if (action.targetCount > std::size(action.targets) ||
      action.actionId >= std::size(Battle_actionDefinitions)) {
    VS_LOG_ERROR("combat", "Invalid action {} with {} targets", action.actionId,
                 action.targetCount);
    std::abort();
  }
  LiveActionTargetEffects effects(actor_id, &action, position);
  ApplyActionTargetEffects(actor_id, effects);
}

// decomp: _applyBattleAbilityEffect (src/BATTLE/BATTLE.PRG/146C.c:14018).
void ApplyLiveActorActionResult(Battle_HitResult* hit) {
  if (hit == nullptr) {
    VS_LOG_ERROR("combat", "Action result requires a hit");
    std::abort();
  }
  LiveActorActionResultWorld world(*hit);
  if (!ApplyActorActionResult(world)) {
    VS_LOG_ERROR("combat",
                 "Action result requires valid target stats/used limb");
    std::abort();
  }
}
}  // namespace VS::Battle
