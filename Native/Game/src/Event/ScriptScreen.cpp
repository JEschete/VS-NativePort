// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Core/Memory/CopyLive.hpp"
#include "Core/Runtime.hpp"
#include "Render/DisplayControlLive.hpp"
#include "Render/GimImagesLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorState.h"
#include "Event/Actors.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptScreen.h"
#include "Event/Splash.h"
#include "Field/RoomState.h"
#include "Field/Transitions.h"

// decomp: D_800F4B30 (D_800F4B30@BATTLE/BATTLE.PRG)
Event_SplashTween Event_splashTweens[6];

// decomp: BATTLE.PRG func_800B6724 (src/BATTLE/BATTLE.PRG/4A0A8.c:609)
intptr_t Event_ScriptWaitForSplashFile(u_char* instruction, short thread_index) { return VS::Render::LiveGimImageLoading(); }

// decomp: BATTLE.PRG vs_battle_script_loadGim (src/BATTLE/BATTLE.PRG/4A0A8.c:851)
intptr_t Event_ScriptLoadSplashImage(u_char* instruction, short thread_index)
{
    VS::Render::QueueLiveGimImage(VS::Core::Memory::ReadLiveUnalignedShort(instruction + 1), instruction[3]);
    return 0;
}

// decomp: BATTLE.PRG func_800B7014 (src/BATTLE/BATTLE.PRG/4A0A8.c:857)
intptr_t Event_ScriptConfigureSplashLayer(u_char* instruction, short thread_index)
{
    if (!VS::Render::LiveGimImagesPresent()) {
        return 0;
    }

    Event_splashTweens[instruction[1] >> 6].durationFrames = 0;
    VS::Render::ConfigureLiveGimLayer(instruction[1] >> 6, instruction[1] & 3, (signed char)instruction[2] * 2,
                                      (signed char)instruction[3] * 2, instruction[4] << 7, instruction[5]);
    return 0;
}

// decomp: BATTLE.PRG func_800B70E8 (src/BATTLE/BATTLE.PRG/4A0A8.c:879)
intptr_t Event_ScriptTweenSplashLayer(u_char* instruction, short thread_index)
{
    int layer_index = instruction[5] >> 6;
    Event_SplashTween* splash_tween = &Event_splashTweens[layer_index];
    const auto splash_layer = VS::Render::LiveGimLayerPlacement(layer_index);

    if (!splash_layer) {
        return 0;
    }

    if (splash_tween->durationFrames != 0) {
        return 1;
    }

    splash_tween->deltaX = (((signed char)instruction[1] * 2) - (splash_tween->startX = splash_layer->offset_x));
    splash_tween->deltaY = (((signed char)instruction[2] * 2) - (splash_tween->startY = splash_layer->offset_y));
    splash_tween->deltaScaleFixed12 = ((instruction[3] << 7) - (splash_tween->startScaleFixed12 = splash_layer->scale_and_order));
    splash_tween->deltaBrightness = instruction[4] - (splash_tween->startBrightness = splash_layer->brightness);
    splash_tween->easingMode = instruction[5] & 7;
    splash_tween->durationFrames = instruction[6];
    splash_tween->elapsedFrames = 0;
    return 0;
}

// decomp: BATTLE.PRG func_800B71C4 (src/BATTLE/BATTLE.PRG/4A0A8.c:905)
intptr_t Event_ScriptFreeSplashImage(u_char* instruction, short thread_index)
{
    VS::Render::ReleaseLiveGimImages();
    return (Event_skipState == 2) * 4;
}

// decomp: BATTLE.PRG func_800B71F4 (src/BATTLE/BATTLE.PRG/4A0A8.c:911)
intptr_t Event_ScriptWaitForSplashTween(u_char* instruction, short thread_index)
{
    return (Event_splashTweens[2].durationFrames | (Event_splashTweens[0].durationFrames | Event_splashTweens[1].durationFrames)) != 0;
}

// decomp: BATTLE.PRG func_800B9A40 (src/BATTLE/BATTLE.PRG/4A0A8.c:2049)
intptr_t Event_ScriptFadeOutAndWait(u_char* instruction, short thread_index)
{
    int transition_phase = Field_ScreenTransitionPhase();
    if (transition_phase != 5) {
        if (transition_phase == 0) {
            Field_BeginScreenFadeOut(instruction[1], instruction[2], instruction[3] * 0x10);
        }
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B9A94 (src/BATTLE/BATTLE.PRG/4A0A8.c:2061)
intptr_t Event_ScriptFadeIn(u_char* instruction, short thread_index)
{
    Field_BeginScreenFadeIn(instruction[1], instruction[2], instruction[3] * 0x10);
    return 0;
}

// decomp: BATTLE.PRG func_800B9AC4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2067)
intptr_t Event_ScriptSetWipeAngle(u_char* instruction, short thread_index)
{
    Field_SetTransitionWipeAngle(instruction[1] * 0x10);
    return 0;
}

// decomp: BATTLE.PRG func_800B9AE8 (src/BATTLE/BATTLE.PRG/4A0A8.c:2073)
intptr_t Event_ScriptConfigureScreenTransition(u_char* instruction, short thread_index)
{
    Field_ConfigureScreenTransition(instruction[1], instruction[2], instruction[3] * 0x10);
    return 0;
}

// decomp: BATTLE.PRG func_800B9B18 (src/BATTLE/BATTLE.PRG/4A0A8.c:2079)
intptr_t Event_ScriptSetRoomDisplayFlags(u_char* instruction, short thread_index)
{
    switch (instruction[1] & 3) {
    case 0:
        VS::Render::DisableLiveDisplay();
        break;
    case 1:
        VS::Render::EnableLiveDisplay();
        break;
    }

    switch ((instruction[1] >> 2) & 3) {
    case 0:
        Field_HidePrimaryRoomGeometry();
        break;
    case 1:
        Field_ShowPrimaryRoomGeometry();
        break;
    }

    switch ((instruction[1] >> 4) & 3) {
    case 0:
        Actors_DisableAllActorModels();
        break;
    case 1:
        Actors_EnableAllActorModels();
        break;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B9BF8 (src/BATTLE/BATTLE.PRG/4A0A8.c:2110)
intptr_t Event_ScriptWaitForScreenTransition(u_char* instruction, short thread_index) { return Field_ScreenTransitionPhase() != 0; }

// decomp: BATTLE.PRG func_800B9C18 (src/BATTLE/BATTLE.PRG/4A0A8.c:2112)
intptr_t Event_ScriptTogglePaletteHook(u_char* instruction, short thread_index)
{
    if (instruction[1] != 0) {
        VS::Core::NoOperation();
    } else {
        VS::Core::NoOperation();
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B9C58 (src/BATTLE/BATTLE.PRG/4A0A8.c:2122)
intptr_t Event_ScriptTransformPalettes(u_char* instruction, short thread_index)
{
    Event_ActorSelection actor_selection;
    int actor_id;

    switch (instruction[1] >> 4) {
    case 0:
        Event_BuildScriptActorSelection(VS::Core::Memory::ReadLiveUnalignedShort(instruction + 2), &actor_selection);
        while (1) {
            actor_id = Event_NextScriptSelectedActor(&actor_selection);
            if (actor_id == 1) {
                break;
            }
            VS::Render::TransformLiveActorColors(instruction[1] & 0xF, (1 << instruction[4]) & 0x1F, actor_id,
                (signed char)instruction[5], (signed char)instruction[6], (signed char)instruction[7]);
        }
        break;
    case 1:
        VS::Render::TransformLiveSceneColors(instruction[1] & 0xF, (1 << instruction[4]) & 0x1F, (signed char)instruction[5],
            (signed char)instruction[6], (signed char)instruction[7]);
        break;
    case 2:
        VS::Render::TransformLiveBackgroundColors(instruction[1] & 0xF, (1 << instruction[4]) & 0x1F, (signed char)instruction[5],
            (signed char)instruction[6], (signed char)instruction[7]);
        break;
    }
    return 0;
}

}  // extern "C"
