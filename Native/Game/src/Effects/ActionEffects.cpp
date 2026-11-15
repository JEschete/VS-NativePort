// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Effects/EffectRuntimeLive.hpp"
#include "Effects/PrimaryEffectLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

// decomp: BATTLE.PRG func_8007BE20 (src/BATTLE/BATTLE.PRG/146C.c:8581)
void Effects_StartScriptedActorEffect(int effect_id, int target_actor_index)
{
    Effects_ActionEffectRequest request;
    int i;

    int count = 0;
    Battle_Actor** actors = Battle_actors;

    request.count = 1;
    request.source.type = 5;
    request.effectId = effect_id;
    request.otherCount = 0;
    request.source.value.position.vz = 0;
    request.source.value.position.vy = 0;
    request.source.value.position.vx = 0;
    request.targets[0].type = 4;
    request.targets[0].value.actor.actor = target_actor_index;
    request.targets[0].value.actor.part = 0;

    for (i = 0; i < 16; ++i) {
        if ((actors[i] != NULL) && (i != target_actor_index)) {
            request.others[count] = i;
            ++count;
        }
    }

    request.otherCount = count;
    VS::Effects::LiveBeginActionEffect(&request, 0, 0);
}

// decomp: BATTLE.PRG func_8007BEC4 (src/BATTLE/BATTLE.PRG/146C.c:8611)
void Effects_ReleaseScriptedEffectTrigger(int track_index) { VS::Effects::LiveStartEffectTrack(track_index); }

// decomp: BATTLE.PRG func_8007BEE4 (src/BATTLE/BATTLE.PRG/146C.c:8613)
int Effects_ScriptedEffectPlaybackState(void) { return VS::Effects::LivePrimaryEffectPhase(); }

// decomp: BATTLE.PRG func_8007BF0C (src/BATTLE/BATTLE.PRG/146C.c:8617)
int Effects_IsScriptedEffectTriggerReady(void) { return VS::Effects::LiveCanStartEffectTrack() != 0; }

}  // extern "C"
