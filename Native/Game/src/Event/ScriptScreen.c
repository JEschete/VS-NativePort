// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorState.h"
#include "Core/Compatibility.h"
#include "Core/Memory/Copy.h"
#include "Event/Actors.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptScreen.h"
#include "Event/Splash.h"
#include "Field/RoomState.h"
#include "Field/Transitions.h"
#include "Render/Display.h"
#include "Render/Images.h"
#include "Render/Palette.h"

// decomp: D_800F4B30 (D_800F4B30@BATTLE/BATTLE.PRG)
Event_SplashTween Event_splashTweens[6];

// decomp: BATTLE.PRG func_800B6724 (src/BATTLE/BATTLE.PRG/4A0A8.c:609)
intptr_t Event_ScriptWaitForSplashFile(u_char* instruction, short thread_index) { return Render_gimLayers->isLoadPending; }

// decomp: BATTLE.PRG vs_battle_script_loadGim (src/BATTLE/BATTLE.PRG/4A0A8.c:851)
intptr_t Event_ScriptLoadSplashImage(u_char* instruction, short thread_index)
{
    Render_QueueGimImage(Core_Memory_ReadUnalignedShort(instruction + 1), instruction[3]);
    return 0;
}

// decomp: BATTLE.PRG func_800B7014 (src/BATTLE/BATTLE.PRG/4A0A8.c:857)
intptr_t Event_ScriptConfigureSplashLayer(u_char* instruction, short thread_index)
{
    Event_SplashTween* new_var;

    Event_SplashLayerView* temp_a1 = Render_gimLayers;

    if (temp_a1 == NULL) {
        return 0;
    }

    new_var = &Event_splashTweens[instruction[1] >> 6];
    new_var->durationFrames = 0;
    temp_a1 = temp_a1 + ((instruction[1] >> 6));
    temp_a1->renderMode = instruction[1] & 3;
    temp_a1->offsetX = (signed char)instruction[2] * 2;
    temp_a1->offsetY = (signed char)instruction[3] * 2;
    temp_a1->scaleFixed12 = instruction[4] << 7;
    temp_a1->brightness = instruction[5];
    Render_ActivateGimLayer(instruction[1] >> 6, temp_a1, instruction);
    return 0;
}

// decomp: BATTLE.PRG func_800B70E8 (src/BATTLE/BATTLE.PRG/4A0A8.c:879)
intptr_t Event_ScriptTweenSplashLayer(u_char* instruction, short thread_index)
{
    Event_SplashLayerView* temp_a3 = Render_gimLayers;
    int temp_a2 = instruction[5] >> 6;
    Event_SplashTween* temp_a1 = &Event_splashTweens[temp_a2];

    if (temp_a3 == NULL) {
        return 0;
    }

    temp_a3 += temp_a2;

    if (temp_a1->durationFrames != 0) {
        return 1;
    }

    temp_a1->deltaX = (((signed char)instruction[1] * 2) - (temp_a1->startX = temp_a3->offsetX));
    temp_a1->deltaY = (((signed char)instruction[2] * 2) - (temp_a1->startY = temp_a3->offsetY));
    temp_a1->deltaScaleFixed12 = ((instruction[3] << 7) - (temp_a1->startScaleFixed12 = temp_a3->scaleFixed12));
    temp_a1->deltaBrightness = instruction[4] - (temp_a1->startBrightness = ((char*)temp_a3)[3]);
    temp_a1->easingMode = instruction[5] & 7;
    temp_a1->durationFrames = instruction[6];
    temp_a1->elapsedFrames = 0;
    return 0;
}

// decomp: BATTLE.PRG func_800B71C4 (src/BATTLE/BATTLE.PRG/4A0A8.c:905)
intptr_t Event_ScriptFreeSplashImage(u_char* instruction, short thread_index)
{
    Render_ReleaseGimImages();
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
    int temp_v0 = Field_ScreenTransitionPhase();
    if (temp_v0 != 5) {
        if (temp_v0 == 0) {
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
        Render_DisableDisplay();
        break;
    case 1:
        Render_EnableDisplay();
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
        Core_NoOperation();
    } else {
        Core_NoOperation();
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B9C58 (src/BATTLE/BATTLE.PRG/4A0A8.c:2122)
intptr_t Event_ScriptTransformPalettes(u_char* instruction, short thread_index)
{
    Event_ActorSelection sp18;
    int temp_a2;

    switch (instruction[1] >> 4) {
    case 0:
        Event_BuildScriptActorSelection(Core_Memory_ReadUnalignedShort(instruction + 2), &sp18);
        while (1) {
            temp_a2 = Event_NextScriptSelectedActor(&sp18);
            if (temp_a2 == 1) {
                break;
            }
            Render_TransformActorColors(instruction[1] & 0xF, (1 << instruction[4]) & 0x1F, temp_a2,
                (signed char)instruction[5], (signed char)instruction[6], (signed char)instruction[7]);
        }
        break;
    case 1:
        Render_TransformSceneColors(instruction[1] & 0xF, (1 << instruction[4]) & 0x1F, (signed char)instruction[5],
            (signed char)instruction[6], (signed char)instruction[7]);
        break;
    case 2:
        Render_TransformBackgroundColors(instruction[1] & 0xF, (1 << instruction[4]) & 0x1F, (signed char)instruction[5],
            (signed char)instruction[6], (signed char)instruction[7]);
        break;
    }
    return 0;
}

