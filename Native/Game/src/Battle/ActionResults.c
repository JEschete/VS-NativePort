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
    kWorldLabelIdTag = 0x80
  };
  if (actor_id >= kActorSlotCount) {
    if ((actor_id & kWorldLabelIdMask) == kWorldLabelIdTag) {
      Battle_QueueWorldLockedLabel((u_short*)position);
    }
    return;
  }
  if (actor_id < 0 || Battle_actionState == NULL) abort();
  Battle_ActionRequest* action =
      &Battle_actionState->queue[Battle_actionState->queueIndex & 3];
  if (action->targetCount >
          sizeof action->targets / sizeof action->targets[0] ||
      action->actionId >= sizeof Battle_actionDefinitions /
                              sizeof Battle_actionDefinitions[0]) {
    abort();
  }
  enum {
    ReactionHit,
    ReactionBlock = 4,
    ReactionRecover = 5,
    ReactionNone = 6
  } reaction = ReactionNone;
  for (unsigned index = 0; index < action->targetCount; ++index) {
    Battle_HitResult* hit = &action->targets[index];
    if (hit->targetsPosition != 0 || hit->target.targetActor != actor_id) {
      continue;
    }
    switch (hit->target.impactKind) {
      case 0:
      case 1:
        reaction = ReactionHit;
        break;
      case 5:
        reaction = ReactionRecover;
        break;
      case 3:
      case 4:
        if (reaction == ReactionNone &&
            !Battle_categoryIsAreaTarget
                [Battle_actionDefinitions[action->actionId].category]) {
          reaction = ReactionBlock;
        }
        break;
    }
    Battle_ApplyActorActionResult(hit);
  }
  switch (reaction) {
    case ReactionHit:
      Battle_PlayActorDamageReaction(actor_id,
                                     &Battle_actionState->rangeOrigin);
      break;
    case ReactionBlock:
      Battle_PlayDefenseAnimation(actor_id, &Battle_actionState->rangeOrigin);
      break;
    case ReactionRecover:
      if (actor_id == 0) Actors_PlayActorActionPose(0, NULL, 6);
      break;
    default:
      break;
  }
}
