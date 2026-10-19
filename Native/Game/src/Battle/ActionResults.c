// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionResults.h"

#include <stdlib.h>

#include "Actors/Animation.h"
#include "Battle/Action.h"
#include "Battle/ActionAnimation.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Feedback.h"
#include "Battle/Hit.h"
#include "Battle/StatusEffects.h"
#include "Battle/Targeting.h"
#include "Legacy/Psx.h"

// decomp: BATTLE.PRG func_8006CB04 (146C/func_8006CB04.s); compared with
// rood-boi's PC 146C.c callback.
void Battle_ApplyActionTargetEffects(int actor_id, SVECTOR* position) {
  enum {
    kActorSlotCount = 16,
    kWorldLabelIdMask = 0x1F0,
    kWorldLabelIdTag = 0x80,
    kRecoveryPose = 6,
    kBlockImpact = 3,
    kAlternateBlockImpact = 4,
    kRecoveryImpact = 5,
    kActionQueueMask =
        sizeof Battle_actionState->queue / sizeof Battle_actionState->queue[0] -
        1
  };
  if (actor_id >= kActorSlotCount) {
    if ((actor_id & kWorldLabelIdMask) == kWorldLabelIdTag) {
      Battle_QueueWorldLockedLabel((u_short*)position);
    }
    return;
  }
  if (actor_id < 0 || Battle_actionState == NULL) abort();
  Battle_ActionRequest* action =
      &Battle_actionState
           ->queue[Battle_actionState->queueIndex & kActionQueueMask];
  if (action->targetCount >
          sizeof action->targets / sizeof action->targets[0] ||
      action->actionId >= sizeof Battle_actionDefinitions /
                              sizeof Battle_actionDefinitions[0]) {
    abort();
  }
  enum {
    kReactionHit,
    kReactionBlock = 4,
    kReactionRecover = 5,
    kReactionNone = 6
  } reaction = kReactionNone;
  for (unsigned index = 0; index < action->targetCount; ++index) {
    Battle_HitResult* hit = &action->targets[index];
    if (hit->targetsPosition != 0 || hit->target.targetActor != actor_id) {
      continue;
    }
    switch (hit->target.impactKind) {
      case 0:
      case 1:
        reaction = kReactionHit;
        break;
      case kRecoveryImpact:
        reaction = kReactionRecover;
        break;
      case kBlockImpact:
      case kAlternateBlockImpact:
        if (reaction == kReactionNone &&
            !Battle_categoryIsAreaTarget
                [Battle_actionDefinitions[action->actionId].category]) {
          reaction = kReactionBlock;
        }
        break;
      default:
        break;
    }
    Battle_ApplyActorActionResult(hit);
  }
  switch (reaction) {
    case kReactionHit:
      Battle_PlayActorDamageReaction(actor_id,
                                     &Battle_actionState->rangeOrigin);
      break;
    case kReactionBlock:
      Battle_PlayDefenseAnimation(actor_id, &Battle_actionState->rangeOrigin);
      break;
    case kReactionRecover:
      if (actor_id == 0) Actors_PlayActorActionPose(0, NULL, kRecoveryPose);
      break;
    default:
      break;
  }
}
