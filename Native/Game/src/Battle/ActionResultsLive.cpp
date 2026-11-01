// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionResultsLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>

#include "Battle/ActionResults.hpp"
#include "Core/Log.hpp"
#include "Legacy/Psx.h"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Battle/Action.h"
#include "Battle/ActionAnimation.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Feedback.h"
#include "Battle/Hit.h"
#include "Battle/StatusEffects.h"
#include "Battle/Targeting.h"
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
    Battle_ApplyActorActionResult(hit);
  }

  void PlayDamageReaction(int actor_id) override {
    Battle_PlayActorDamageReaction(actor_id, &Battle_actionState->rangeOrigin);
  }

  bool HasActorRecord(int actor_id) override {
    return Actors_actorRecords[actor_id] != nullptr;
  }

  void PlayDefense(int actor_id) override {
    Battle_PlayDefenseAnimation(actor_id, &Battle_actionState->rangeOrigin);
  }

  void PlayRecoveryPose() override {
    Actors_PlayActorActionPose(0, nullptr, kRecoveryPose);
  }

 private:
  int actor_id_;
  Battle_ActionRequest* action_;
  SVECTOR* position_;
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
}  // namespace VS::Battle
