// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Actor.h"
#include "Core/Compatibility.h"
#include "Core/Diagnostics.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/WordCopy.h"
#include "DiscQueue.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
#include "Effects/ScreenDistortion/ScreenDistortion.h"
#include "Effects/ScreenEffects.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptEffects.h"
#include "Event/ScriptField.h"
#include "Event/ScriptScreen.h"
#include "Event/ScriptVm.h"
#include "Event/Splash.h"
#include "Event/Weather.h"
#include "Field/Environment.h"
#include "Field/Gimmicks.h"
#include "Input/Controller.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Display.h"
#include "Render/Images.h"
#include "Render/Palette.h"
#include "State/Timers.h"

// decomp: D_800F4B20 (D_800F4B20@BATTLE/BATTLE.PRG)
u_short Event_scanlineOverlayFrame;

// decomp: D_800F4BB9 (D_800F4BB9@BATTLE/BATTLE.PRG)
u_char Event_isEffectWaitPending;

// decomp: D_800F4C48 (D_800F4C48@BATTLE/BATTLE.PRG)
void* Event_vibrationPatternBuffer;

// decomp: BATTLE.PRG func_800BA8BC (src/BATTLE/BATTLE.PRG/4A0A8.c:2454)
intptr_t Event_CommandPlayActorEffect(u_char* instruction, short thread_index)
{
    const int effect = Core_Memory_ReadUnalignedShort(instruction + 1);
    const u_short selector = Core_Memory_ReadUnalignedShort(instruction + 3);
    const unsigned referenceType = selector >> 12;
    const int actor = referenceType <= 1 ? Event_ResolveScriptActorReference(selector) : -1;
    const Battle_Actor* target = (unsigned)actor < 16 ? Actors_ActorAtSlot(actor) : NULL;
    const int empty_slot = (unsigned)actor < 16 && target == NULL &&
        Actors_actorRecords[actor] == NULL && Field_gimmickInstances[actor] == NULL;
    const int unresolved_model = empty_slot && (referenceType == 1 || actor == 1);
    if (!unresolved_model && ((unsigned)actor >= 16 ||
        (referenceType == 0 && (target == NULL || target->modelId != selector)) ||
        (Actors_actorRecords[actor] == NULL && Field_gimmickInstances[actor] == NULL))) {
        Core_Errorf("event", "effect=%d awaits unavailable actor selector=0x%04X slot=%d",
            effect, (unsigned)selector, actor);
        return 1;
    }
    if (unresolved_model) {
        // Actors_FindActorByModelId (146C.c) returns slot 1 for an absent model, Event_ResolveScriptActorReference
        // returns a direct slot reference unchecked, and Effects_StartScriptedActorEffect starts the
        // effect at once. Empty-slot geometry reads (Actors_ActorBoneEndpointPosition, Actors_ActorPlacement,
        // Actors_ActorStateFlags) return without writing, so target-dependent tracks keep defaults.
        Core_Debugf("event", "effect=%d absent actor selector=0x%04X targets empty slot %d", effect, (unsigned)selector, actor);
    }
    Effects_StartScriptedActorEffect(effect, actor);
    return 0;
}

// decomp: BATTLE.PRG func_800BA90C (src/BATTLE/BATTLE.PRG/4A0A8.c:2461)
intptr_t Event_CommandAdvanceEffectStage(u_char* instruction, short thread_index)
{
    Effects_ReleaseScriptedEffectTrigger(instruction[1]);
    if (Event_skipState == 2) {
        Effects_ClearPrimaryEffects();
    }
    return 0;
}

// decomp: BATTLE.PRG func_800BA94C (src/BATTLE/BATTLE.PRG/4A0A8.c:2470)
intptr_t Event_CommandWaitEffectState(u_char* instruction, short thread_index)
{
    if ((Event_skipState == 2) && ((instruction[1] == 0) || (instruction[1] == 5))) {
        if (Event_isEffectWaitPending != 0) {
            if (Effects_ScriptedEffectPlaybackState() == instruction[1]) {
                Event_isEffectWaitPending = 0;
                return 0;
            }
        } else {
            Effects_ClearPrimaryEffects();
            Event_isEffectWaitPending = 1;
        }
        return 1;
    }
    return Effects_ScriptedEffectPlaybackState() != instruction[1];
}

// decomp: BATTLE.PRG func_800BAA04 (src/BATTLE/BATTLE.PRG/4A0A8.c:2487)
intptr_t Event_CommandYieldEffectRelease(u_char* instruction, short thread_index)
{
    Core_NoOperation();
    return (Event_skipState == 2) * 4;
}

// decomp: BATTLE.PRG func_800BAA34 (src/BATTLE/BATTLE.PRG/4A0A8.c:2493)
intptr_t Event_CommandWaitEffectContinuation(u_char* instruction, short thread_index) { return Effects_IsScriptedEffectTriggerReady() != 1; }

// decomp: BATTLE.PRG func_800BAA58 (src/BATTLE/BATTLE.PRG/4A0A8.c:2495)
intptr_t Event_CommandWaitEffectRelease(u_char* instruction, short thread_index)
{
    if (Event_isEffectWaitPending == 0) {
        Effects_ClearPrimaryEffects();
        Event_isEffectWaitPending = 1;
    } else if ((Effects_ScriptedEffectPlaybackState() == 0) || (Effects_ScriptedEffectPlaybackState() == 5)) {
        Event_isEffectWaitPending = 0;
        return 0;
    }
    return 1;
}

// decomp: BATTLE.PRG func_800BAACC (src/BATTLE/BATTLE.PRG/4A0A8.c:2507)
intptr_t Event_CommandMarkEffectStageReady(u_char* instruction, short thread_index)
{
    Effects_RequeueEffectTrack(instruction[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800BAB14 (src/BATTLE/BATTLE.PRG/4A0A8.c:2519)
intptr_t Event_CommandPlayVibrationPattern(u_char* instruction, short thread_index)
{
    short pattern_selector = instruction[1];
    if (pattern_selector == 0xFF) {
        Input_StopVibrationPatterns(0, 2);
    } else {
        if (pattern_selector & 8) {
            Core_Memory_CopyAlignedWords(Event_vibrationPatternBuffer, Event_vibrationPatterns[pattern_selector & 7], 0x80);
            Input_StartVibrationPattern(Event_vibrationPatternBuffer + 8, 0xFF, 2);
        } else {
            if (Event_vibrationPatterns[pattern_selector] != 0) {
                Input_StartVibrationPattern(Event_vibrationPatterns[pattern_selector] + 8, 0xFF, 2);
            }
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_800BABCC (src/BATTLE/BATTLE.PRG/4A0A8.c:2537)
intptr_t Event_CommandSetActorTrail(u_char* instruction, short thread_index)
{
    short actor_id = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 2));

    if (instruction[1] != 0) {
        Effects_StartWeaponTrail(actor_id, instruction[4]);
    } else {
        Effects_FadeWeaponTrails(actor_id, instruction[4]);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800BB288 (src/BATTLE/BATTLE.PRG/4A0A8.c:2733)
intptr_t Event_CommandWaitScreenEffectLoad(u_char* instruction, short thread_index)
{
    if ((Effects_screenEffectOverlay != NULL) && (Effects_screenEffectOverlay->loading != 0)) {
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800BB2B8 (src/BATTLE/BATTLE.PRG/4A0A8.c:2741)
intptr_t Event_CommandSetScreenDistortion(u_char* instruction, short thread_index)
{
    Effects_ScreenDistortion_SetFeedbackLayer(Core_Memory_ReadUnalignedShort(instruction + 1));
    return 0;
}

// decomp: BATTLE.PRG func_800BB2E4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2747)
intptr_t Event_CommandLoadScreenEffects(u_char* instruction, short thread_index)
{
    Effects_AcquireScreenEffectOverlay();
    return 0;
}

// decomp: BATTLE.PRG func_800BB304 (src/BATTLE/BATTLE.PRG/4A0A8.c:2753)
intptr_t Event_CommandReservedScreenEffect(u_char* instruction, short thread_index) { return 0; }

// decomp: BATTLE.PRG func_800BB30C (src/BATTLE/BATTLE.PRG/4A0A8.c:2755)
intptr_t Event_CommandFreeScreenEffects(u_char* instruction, short thread_index)
{
    Effects_ReleaseScreenEffectOverlay();
    return (Event_skipState == 2) * 4;
}

// decomp: BATTLE.PRG func_800BB4F0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2821)
intptr_t Event_CommandSetScanlineOverlay(u_char* instruction, short thread_index)
{
    Event_isScanlineOverlayEnabled = instruction[1];
    return 0;
}

// decomp: BATTLE.PRG func_800BB874 (src/BATTLE/BATTLE.PRG/4A0A8.c:2937)
void Event_DrawEventScanlineOverlay(void)
{
    LINE_F2* primitive_cursor;
    short i;
    u_int scanline_intensity;

    void** p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
    primitive_cursor = (LINE_F2*)p[0];

    ++Event_scanlineOverlayFrame;

    for (i = 0; i < Render_viewportHeight; ++i) {
        u_short v;
        scanline_intensity = (((i + (Event_scanlineOverlayFrame >> 1)) & 1) << 5);
        v = scanline_intensity;
        if (scanline_intensity != 0) {
            SetLineF2(primitive_cursor);
            primitive_cursor->r0 = primitive_cursor->g0 = primitive_cursor->b0 = v;
            setSemiTrans(primitive_cursor, 1);
            primitive_cursor->x0 = 0;
            primitive_cursor->y0 = primitive_cursor->y1 = i;
            primitive_cursor->x1 = Render_viewportWidth;
            p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
            AddPrim(p[2], primitive_cursor++);
        }
    }

    SetDrawTPage((DR_TPAGE*)primitive_cursor, 1, 1, GetTPage(0, 2, 0, 0));

    p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
    AddPrim(p[2], primitive_cursor);
    p[0] = (LINE_F2*)&primitive_cursor->x0;
}

// decomp: BATTLE.PRG func_800BB9B0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2971)
void Event_UpdateEventImageTweens(void)
{
    short i;
    short eased_fraction;
    Event_SplashTween* splash_tween = Event_splashTweens;
    Event_SplashLayerView* splash_layer = Render_gimLayers;

    for (i = 0; i < 3; ++i, ++splash_tween, ++splash_layer) {
        if (splash_tween->durationFrames != 0) {
            splash_tween->elapsedFrames += State_tickSpeed >> 1;
            eased_fraction = ((int (*)(int, int, int))Event_EvaluateInterpolationCurve)(splash_tween->easingMode, splash_tween->elapsedFrames, splash_tween->durationFrames);
            if (Render_gimLayers != NULL) {
                splash_layer->offsetX = (splash_tween->startX + ((splash_tween->deltaX * eased_fraction) >> 0xC));
                splash_layer->offsetY = (splash_tween->startY + ((splash_tween->deltaY * eased_fraction) >> 0xC));
                splash_layer->scaleFixed12 = (splash_tween->startScaleFixed12 + ((splash_tween->deltaScaleFixed12 * eased_fraction) >> 0xC));
                splash_layer->brightness = (splash_tween->startBrightness + ((splash_tween->deltaBrightness * eased_fraction) >> 0xC));
            }
            if (splash_tween->elapsedFrames >= splash_tween->durationFrames) {
                splash_tween->durationFrames = 0;
            }
        }
    }
}

// decomp: BATTLE.PRG func_800BBAFC (src/BATTLE/BATTLE.PRG/4A0A8.c:2995)
void Event_UpdateEventWeatherTween(void)
{
    if (Event_weatherTween.durationFrames != 0) {
        short elapsed_fraction = ((++Event_weatherTween.elapsedFrames << 0xC) / Event_weatherTween.durationFrames);
        Event_weatherTween.windDirection =
            (Event_weatherTween.startWindDirection + ((Event_weatherTween.deltaWindDirection * elapsed_fraction) >> 0xC)) & 0xFFF;
        Event_weatherTween.windStrength = Event_weatherTween.startWindStrength + ((Event_weatherTween.deltaWindStrength * elapsed_fraction) >> 0xC);
        Event_weatherTween.intensity = (Event_weatherTween.startIntensity + ((Event_weatherTween.deltaIntensity * elapsed_fraction) >> 0xC));
        Field_SetWeatherWindAngles(Event_weatherTween.windDirection, Event_weatherTween.windStrength);
        Field_SetWeatherIntensity(Event_weatherTween.intensity);
        if (Event_weatherTween.elapsedFrames == Event_weatherTween.durationFrames) {
            Event_weatherTween.durationFrames = 0;
        }
    }
}

