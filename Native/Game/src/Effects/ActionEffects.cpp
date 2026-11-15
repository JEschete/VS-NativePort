// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Actors/AnimationControlLive.hpp"
#include "Battle/HudStateLive.hpp"
#include "Effects/EffectRuntimeLive.hpp"
#include "Effects/PrimaryEffectLive.hpp"
#include "Field/GameModeLive.hpp"
#include "Field/MapLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
#include "Field/Interactions.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"
#include "State/Timers.h"

// decomp: BATTLE.PRG func_8006CDB8 (src/BATTLE/BATTLE.PRG/146C.c:2149)
void Effects_SignalActorEffectAnimation(int actor_index) { VS::Actors::RequestLiveActorAnimationAdvance(actor_index); }

// decomp: BATTLE.PRG func_8006D028 (src/BATTLE/BATTLE.PRG/146C.c:2211)
void Effects_TintActionActorRed(int actor_index, int unused __attribute__((unused)))
{
    Effects_tintedActor = actor_index;
    VS::Render::TransformLiveActorColors(0xE, 8, actor_index, 0x10, 0, 0);
}

// decomp: BATTLE.PRG func_8006D064 (src/BATTLE/BATTLE.PRG/146C.c:2217)
void Effects_RestoreActionActorTint(void)
{
    VS::Render::TransformLiveActorColors(8, 4, Effects_tintedActor, 0, 0, 0);
    Effects_tintedActor = 0;
}

// decomp: BATTLE.PRG func_800733F8 (src/BATTLE/BATTLE.PRG/146C.c:4613)
void Effects_BeginAshleyStandaloneEffect(int effect_id)
{
    Effects_ActionEffectRequest request;

    request.effectId = effect_id;
    Field_controlMode = 4;
    request.count = 1;
    request.otherCount = 0;
    request.source.type = 4;
    request.source.value.actor.actor = 0;
    request.source.value.actor.eventActorSlot = 0;
    request.source.value.actor.part = 0;
    request.targets[0].type = 4;
    request.targets[0].value.actor.actor = 0;
    request.targets[0].value.actor.eventActorSlot = 0;
    request.targets[0].value.actor.part = 0;
    request.position.vz = 0;
    request.position.vy = 0;
    request.position.vx = 0;
    VS::Effects::LiveBeginActionEffect(&request, 0, 0);
    State_tickSpeed = 4;
    VS::Battle::SetLiveHudHidden(1);
    VS::Field::SetLiveMinimapVisible(0);
}

// decomp: BATTLE.PRG func_80073484 (src/BATTLE/BATTLE.PRG/146C.c:4638)
void Effects_EndAshleyStandaloneEffect(void)
{
    VS::Field::LiveEnterFieldExplorationMode();
    State_tickSpeed = 2;
    VS::Battle::SetLiveHudHidden(0);
    VS::Field::SetLiveMinimapVisible(1);
}

// decomp: BATTLE.PRG func_80074294 (src/BATTLE/BATTLE.PRG/146C.c:5064)
int Effects_ActionEffectBusyFlags(void) { return Effects_effectBusyFlags & 3; }

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
