// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Compatibility.h"
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
#include "Input/Controller.h"
#include "Render/Display.h"
#include "Render/Images.h"
#include "State/Timers.h"

// decomp: D_800F4B20 (D_800F4B20@BATTLE/BATTLE.PRG)
u_short Event_scanlineOverlayFrame;

// decomp: D_800F4BB9 (D_800F4BB9@BATTLE/BATTLE.PRG)
u_char Event_isEffectWaitPending;

// decomp: D_800F4C48 (D_800F4C48@BATTLE/BATTLE.PRG)
void* Event_vibrationPatternBuffer;

// decomp: BATTLE.PRG func_800BA8BC (src/BATTLE/BATTLE.PRG/4A0A8.c:2454)
intptr_t Event_CommandPlayActorEffect(u_char* arg0, short arg1)
{
    Effects_StartScriptedActorEffect(
        Core_Memory_ReadUnalignedShort(arg0 + 1), Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 3)));
    return 0;
}

// decomp: BATTLE.PRG func_800BA90C (src/BATTLE/BATTLE.PRG/4A0A8.c:2461)
intptr_t Event_CommandAdvanceEffectStage(u_char* arg0, short arg1)
{
    Effects_ReleaseScriptedEffectTrigger(arg0[1]);
    if (Event_skipState == 2) {
        Effects_ClearPrimaryEffects();
    }
    return 0;
}

// decomp: BATTLE.PRG func_800BA94C (src/BATTLE/BATTLE.PRG/4A0A8.c:2470)
intptr_t Event_CommandWaitEffectState(u_char* arg0, short arg1)
{
    if ((Event_skipState == 2) && ((arg0[1] == 0) || (arg0[1] == 5))) {
        if (Event_isEffectWaitPending != 0) {
            if (Effects_ScriptedEffectPlaybackState() == arg0[1]) {
                Event_isEffectWaitPending = 0;
                return 0;
            }
        } else {
            Effects_ClearPrimaryEffects();
            Event_isEffectWaitPending = 1;
        }
        return 1;
    }
    return Effects_ScriptedEffectPlaybackState() != arg0[1];
}

// decomp: BATTLE.PRG func_800BAA04 (src/BATTLE/BATTLE.PRG/4A0A8.c:2487)
intptr_t Event_CommandYieldEffectRelease(u_char* arg0, short arg1)
{
    Core_NoOperation();
    return (Event_skipState == 2) * 4;
}

// decomp: BATTLE.PRG func_800BAA34 (src/BATTLE/BATTLE.PRG/4A0A8.c:2493)
intptr_t Event_CommandWaitEffectContinuation(u_char* arg0, short arg1) { return Effects_IsScriptedEffectTriggerReady() != 1; }

// decomp: BATTLE.PRG func_800BAA58 (src/BATTLE/BATTLE.PRG/4A0A8.c:2495)
intptr_t Event_CommandWaitEffectRelease(u_char* arg0, short arg1)
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
intptr_t Event_CommandMarkEffectStageReady(u_char* arg0, short arg1)
{
    Effects_RequeueEffectTrack(arg0[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800BAB14 (src/BATTLE/BATTLE.PRG/4A0A8.c:2519)
intptr_t Event_CommandPlayVibrationPattern(u_char* arg0, short arg1)
{
    short a0 = arg0[1];
    if (a0 == 0xFF) {
        Input_StopVibrationPatterns(0, 2);
    } else {
        if (a0 & 8) {
            Core_Memory_CopyAlignedWords(Event_vibrationPatternBuffer, Event_vibrationPatterns[a0 & 7], 0x80);
            Input_StartVibrationPattern(Event_vibrationPatternBuffer + 8, 0xFF, 2);
        } else {
            if (Event_vibrationPatterns[a0] != 0) {
                Input_StartVibrationPattern(Event_vibrationPatterns[a0] + 8, 0xFF, 2);
            }
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_800BABCC (src/BATTLE/BATTLE.PRG/4A0A8.c:2537)
intptr_t Event_CommandSetActorTrail(u_char* arg0, short arg1)
{
    short temp_v0 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 2));

    if (arg0[1] != 0) {
        Effects_StartWeaponTrail(temp_v0, arg0[4]);
    } else {
        Effects_FadeWeaponTrails(temp_v0, arg0[4]);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800BB288 (src/BATTLE/BATTLE.PRG/4A0A8.c:2733)
intptr_t Event_CommandWaitScreenEffectLoad(u_char* arg0, short arg1)
{
    if ((Effects_screenEffectOverlay != NULL) && (Effects_screenEffectOverlay->loading != 0)) {
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800BB2B8 (src/BATTLE/BATTLE.PRG/4A0A8.c:2741)
intptr_t Event_CommandSetScreenDistortion(u_char* arg0, short arg1)
{
    Effects_ScreenDistortion_SetFeedbackLayer(Core_Memory_ReadUnalignedShort(arg0 + 1));
    return 0;
}

// decomp: BATTLE.PRG func_800BB2E4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2747)
intptr_t Event_CommandLoadScreenEffects(u_char* arg0, short arg1)
{
    Effects_AcquireScreenEffectOverlay();
    return 0;
}

// decomp: BATTLE.PRG func_800BB304 (src/BATTLE/BATTLE.PRG/4A0A8.c:2753)
intptr_t Event_CommandReservedScreenEffect(u_char* arg0, short arg1) { return 0; }

// decomp: BATTLE.PRG func_800BB30C (src/BATTLE/BATTLE.PRG/4A0A8.c:2755)
intptr_t Event_CommandFreeScreenEffects(u_char* arg0, short arg1)
{
    Effects_ReleaseScreenEffectOverlay();
    return (Event_skipState == 2) * 4;
}

// decomp: BATTLE.PRG func_800BB4F0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2821)
intptr_t Event_CommandSetScanlineOverlay(u_char* arg0, short arg1)
{
    Event_isScanlineOverlayEnabled = arg0[1];
    return 0;
}

// decomp: BATTLE.PRG func_800BB874 (src/BATTLE/BATTLE.PRG/4A0A8.c:2937)
void Event_DrawEventScanlineOverlay(void)
{
    LINE_F2* var_s3;
    short i;
    u_int temp_v0_2;

    void** p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
    var_s3 = (LINE_F2*)p[0];

    ++Event_scanlineOverlayFrame;

    for (i = 0; i < Render_viewportHeight; ++i) {
        u_short v;
        temp_v0_2 = (((i + (Event_scanlineOverlayFrame >> 1)) & 1) << 5);
        v = temp_v0_2;
        if (temp_v0_2 != 0) {
            SetLineF2(var_s3);
            var_s3->r0 = var_s3->g0 = var_s3->b0 = v;
            setSemiTrans(var_s3, 1);
            var_s3->x0 = 0;
            var_s3->y0 = var_s3->y1 = i;
            var_s3->x1 = Render_viewportWidth;
            p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
            AddPrim(p[2], var_s3++);
        }
    }

    SetDrawTPage((DR_TPAGE*)var_s3, 1, 1, GetTPage(0, 2, 0, 0));

    p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
    AddPrim(p[2], var_s3);
    p[0] = (LINE_F2*)&var_s3->x0;
}

// decomp: BATTLE.PRG func_800BB9B0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2971)
void Event_UpdateEventImageTweens(void)
{
    short i;
    short temp_a0;
    Event_SplashTween* var_s0 = Event_splashTweens;
    Event_SplashLayerView* var_s1 = Render_gimLayers;

    for (i = 0; i < 3; ++i, ++var_s0, ++var_s1) {
        if (var_s0->durationFrames != 0) {
            var_s0->elapsedFrames += State_tickSpeed >> 1;
            temp_a0 = ((int (*)(int, int, int))Event_EvaluateInterpolationCurve)(var_s0->easingMode, var_s0->elapsedFrames, var_s0->durationFrames);
            if (Render_gimLayers != NULL) {
                var_s1->offsetX = (var_s0->startX + ((var_s0->deltaX * temp_a0) >> 0xC));
                var_s1->offsetY = (var_s0->startY + ((var_s0->deltaY * temp_a0) >> 0xC));
                var_s1->scaleFixed12 = (var_s0->startScaleFixed12 + ((var_s0->deltaScaleFixed12 * temp_a0) >> 0xC));
                var_s1->brightness = (var_s0->startBrightness + ((var_s0->deltaBrightness * temp_a0) >> 0xC));
            }
            if (var_s0->elapsedFrames >= var_s0->durationFrames) {
                var_s0->durationFrames = 0;
            }
        }
    }
}

// decomp: BATTLE.PRG func_800BBAFC (src/BATTLE/BATTLE.PRG/4A0A8.c:2995)
void Event_UpdateEventWeatherTween(void)
{
    if (Event_weatherTween.durationFrames != 0) {
        short temp_lo = ((++Event_weatherTween.elapsedFrames << 0xC) / Event_weatherTween.durationFrames);
        Event_weatherTween.windDirection =
            (Event_weatherTween.startWindDirection + ((Event_weatherTween.deltaWindDirection * temp_lo) >> 0xC)) & 0xFFF;
        Event_weatherTween.windStrength = Event_weatherTween.startWindStrength + ((Event_weatherTween.deltaWindStrength * temp_lo) >> 0xC);
        Event_weatherTween.intensity = (Event_weatherTween.startIntensity + ((Event_weatherTween.deltaIntensity * temp_lo) >> 0xC));
        Field_SetWeatherWindAngles(Event_weatherTween.windDirection, Event_weatherTween.windStrength);
        Field_SetWeatherIntensity(Event_weatherTween.intensity);
        if (Event_weatherTween.elapsedFrames == Event_weatherTween.durationFrames) {
            Event_weatherTween.durationFrames = 0;
        }
    }
}

