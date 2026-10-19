// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/ConditionSnapshot.h"

#include <stdint.h>
#include <stdlib.h>

#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionSelection.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Behavior.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Ai/Perception.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/StatusEffects.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "Field/Gimmicks.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Legacy/Psx.h"
#include "Render/Palette.h"
#include "State/Flags.h"

// decomp: BATTLE.PRG func_800DC424 (src/BATTLE/BATTLE.PRG/73C24.s:8).
void Battle_Ai_CaptureActorConditionTable(Battle_Ai_ActionTiming* output) {
  if (output == NULL) abort();
  for (const Battle_Actor* actor = Battle_actors[0]; actor != NULL;
       actor = actor->next) {
    if ((unsigned)actor->id >= sizeof Battle_actors / sizeof Battle_actors[0] ||
        actor->stats == NULL) {
      Core_Debugf("ai", "Invalid condition snapshot actor=%d stats=%p",
                  actor->id, (void*)actor->stats);
      abort();
    }
    const Battle_ActorStats* stats = actor->stats;
    output[actor->id] = (Battle_Ai_ActionTiming){
        .opaque0Bit0 = (unsigned)actor->id,
        .opaque4 = stats->currentHp,
        .opaque6 = stats->maxHp,
        .opaque8 = stats->currentMp,
        .opaqueA = stats->maxMp,
        .opaqueC = (unsigned)stats->statuses,
        .opaque10 = (int16_t)stats->weapon.currentDp,
        .opaque12 = (int16_t)stats->weapon.maxDp,
        .opaque14 = (int16_t)stats->shield.currentDp,
        .opaque16 = (int16_t)stats->shield.maxDp,
    };
  }
}
