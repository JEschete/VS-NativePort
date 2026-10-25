#include <stdint.h>

#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResults.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Legacy/Psx.h"

int Tests_CombatTargetResults(void) {
  enum { kInitialLimbHp = 20 };
  Battle_ActionState actionState = {0};
  Battle_Actor target = {0};
  Battle_ActorStats stats = {0};
  Battle_FloatingLabel labels[32] = {{0}};
  Battle_ActionState* savedAction = Battle_actionState;
  Battle_Actor* savedTarget = Battle_actors[2];
  Actors_ActorRecord* savedModel = Actors_actorRecords[2];
  Battle_FloatingLabel(*savedLabels)[32] = Battle_floatingLabels;
  const int16_t saved_label_count = Battle_floatingLabelCount;
  target.id = 2;
  target.roomSpawnIndex = 0x80;
  target.stats = &stats;
  stats.currentHp = stats.maxHp = 63;
  stats.currentMp = stats.maxMp = 40;
  for (size_t limb = 0; limb < sizeof stats.limbs / sizeof stats.limbs[0];
       ++limb) {
    stats.limbs[limb].hp = stats.limbs[limb].maxHp = kInitialLimbHp;
  }
  Battle_actors[2] = &target;
  Actors_actorRecords[2] = NULL;
  Battle_floatingLabels = &labels;
  Battle_floatingLabelCount = 0;
  Battle_actionState = &actionState;
  actionState.queueIndex = 5;
  Battle_ActionRequest* action = &actionState.queue[1];
  action->actionId = 1;
  action->targetCount = 3;
  Battle_HitResult* hit = &action->targets[0];
  hit->target.targetActor = 2;
  hit->target.targetLimb = 0;
  hit->target.hp = 12;
  hit->hpEffect = 1;
  hit->limbs[0].effects.opaque0 = 4;
  action->targets[1] = *hit;
  action->targets[1].target.targetActor = 3;
  action->targets[2] = *hit;
  action->targets[2].targetsPosition = 1;
  Battle_ApplyActionTargetEffects(2, NULL);
  int passed = stats.currentHp == 51 && stats.limbs[0].hp == 16 &&
               stats.limbs[1].hp == 20;
  hit->target.hp = 100;
  Battle_ApplyActionTargetEffects(2, NULL);
  passed = passed && stats.currentHp == 0;
  hit->target.impactKind = 2;
  hit->hpEffect = 2;
  hit->mpEffect = 1;
  hit->target.mp = 50;
  Battle_ApplyActionTargetEffects(2, NULL);
  passed = passed && stats.currentHp == 63 && stats.currentMp == 0;
  hit->hpEffect = 0;
  hit->mpEffect = 2;
  Battle_ApplyActionTargetEffects(2, NULL);
  passed = passed && stats.currentMp == 40;
  Battle_actionState = savedAction;
  Battle_actors[2] = savedTarget;
  Actors_actorRecords[2] = savedModel;
  Battle_floatingLabels = savedLabels;
  Battle_floatingLabelCount = saved_label_count;
  return passed;
}
