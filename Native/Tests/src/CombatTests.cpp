#include <stdint.h>

#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionResultsLive.hpp"
#include "Battle/Actor.h"
#include "Battle/Damage.hpp"
#include "Battle/Feedback.hpp"
#include "Legacy/Psx.h"

int Tests_CombatTargetResults(void) {
  enum { kInitialLimbHp = 20 };
  Battle_ActionState actionState = {0};
  Battle_Actor target = {nullptr};
  Battle_ActorStats stats = {0};
  VS::Battle::FloatingLabelSlots labels{};
  Battle_ActionState* saved_action = VS::Battle::actionState;
  Battle_Actor* saved_target = VS::Battle::actors[2];
  Actors_ActorRecord* savedModel = Actors_actorRecords[2];
  auto* saved_labels = VS::Battle::floatingLabels;
  const int16_t saved_label_count = VS::Battle::floatingLabelCount;
  target.id = 2;
  target.roomSpawnIndex = 0x80;
  target.stats = &stats;
  stats.currentHp = stats.maxHp = 63;
  stats.currentMp = stats.maxMp = 40;
  for (size_t limb = 0; limb < sizeof stats.limbs / sizeof stats.limbs[0];
       ++limb) {
    stats.limbs[limb].hp = stats.limbs[limb].maxHp = kInitialLimbHp;
  }
  VS::Battle::actors[2] = &target;
  Actors_actorRecords[2] = nullptr;
  VS::Battle::floatingLabels = &labels;
  VS::Battle::floatingLabelCount = 0;
  VS::Battle::actionState = &actionState;
  actionState.queueIndex = 5;
  Battle_ActionRequest* action = &actionState.queue[1];
  action->actionId = 1;
  action->targetCount = 3;
  VS::Battle::HitResult* hit = &action->targets[0];
  hit->target.target_actor = 2;
  hit->target.target_limb = 0;
  hit->target.hp = 12;
  hit->hp_effect = 1;
  hit->limbs[0].effects.opaque0 = 4;
  action->targets[1] = *hit;
  action->targets[1].target.target_actor = 3;
  action->targets[2] = *hit;
  action->targets[2].targets_position = 1;
  VS::Battle::ApplyLiveActionTargetEffects(2, nullptr);
  int passed = stats.currentHp == 51 && stats.limbs[0].hp == 16 &&
               stats.limbs[1].hp == 20;
  hit->target.hp = 100;
  VS::Battle::ApplyLiveActionTargetEffects(2, nullptr);
  passed = passed && stats.currentHp == 0;
  hit->target.impact_kind = 2;
  hit->hp_effect = 2;
  hit->mp_effect = 1;
  hit->target.mp = 50;
  VS::Battle::ApplyLiveActionTargetEffects(2, nullptr);
  passed = passed && stats.currentHp == 63 && stats.currentMp == 0;
  hit->hp_effect = 0;
  hit->mp_effect = 2;
  VS::Battle::ApplyLiveActionTargetEffects(2, nullptr);
  passed = passed && stats.currentMp == 40;
  VS::Battle::actionState = saved_action;
  VS::Battle::actors[2] = saved_target;
  Actors_actorRecords[2] = savedModel;
  VS::Battle::floatingLabels = saved_labels;
  VS::Battle::floatingLabelCount = saved_label_count;
  return passed;
}
