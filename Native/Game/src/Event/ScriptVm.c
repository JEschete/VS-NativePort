// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ActorLoading.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Audio/Music.h"
#include "Audio/Sfx.h"
#include "Battle/Abilities.h"
#include "Battle/Actor.h"
#include "Battle/Rewards.h"
#include "Boot/Lifecycle.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "DiscQueue.h"
#include "Effects/ScreenDistortion/ScreenDistortion.h"
#include "Event/Interpolation.h"
#include "Event/Playback.h"
#include "Event/Script.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptAudio.h"
#include "Event/ScriptCamera.h"
#include "Event/ScriptDialog.h"
#include "Event/ScriptEffects.h"
#include "Event/ScriptField.h"
#include "Event/ScriptScreen.h"
#include "Event/ScriptVm.h"
#include "Event/Splash.h"
#include "Event/Triggers.h"
#include "Field/Camera.h"
#include "Field/Gimmicks.h"
#include "Field/RoomEffects.h"
#include "Field/RoomRendering.h"
#include "Field/RoomState.h"
#include "Field/Transitions.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Render/Palette.h"
#include "Render/UiPrimitives.h"
#include "State/Flags.h"
#include "State/Timers.h"

// decomp: D_800E9BE0 (D_800E9BE0@BATTLE/BATTLE.PRG)
char Event_skipPaletteResetCommand[336]; /* contents: module image (ModuleData.c) */

// decomp: D_800E9BE8 (D_800E9BE8@BATTLE/BATTLE.PRG)
char Event_kSkipSoundRestoreMarker[328]; /* contents: module image (ModuleData.c) */

// decomp: D_800E9BF4 (D_800E9BF4@BATTLE/BATTLE.PRG)
short Event_kSceneTimerModes[158]; /* contents: module image (ModuleData.c) */

// decomp: D_800E9BF8 (D_800E9BF8@BATTLE/BATTLE.PRG)
short Event_kSceneTimerResetScopes[156]; /* contents: module image (ModuleData.c) */

// decomp: D_800E9C00 (D_800E9C00@BATTLE/BATTLE.PRG)
short Event_kSpecialOperationTenArguments[152]; /* contents: module image (ModuleData.c) */

// decomp: D_800E9C30 (D_800E9C30@BATTLE/BATTLE.PRG)
u_char Event_kOpcodeSizes[256]; /* contents: module image (ModuleData.c) */

// decomp: _opcodeFunctionTable (_opcodeFunctionTable@BATTLE/BATTLE.PRG)
int (*Event_kOpcodeHandlers[572])(u_char*, short); /* data: contents loaded in Step 5 */

// decomp: D_800EB588 (D_800EB588@BATTLE/BATTLE.PRG)
u_char* Event_textBoxFrameStyles[16]; /* data: contents loaded in Step 5 */

// decomp: D_800F4B28 (D_800F4B28@BATTLE/BATTLE.PRG)
u_short Event_customEasingCurves[66];

// decomp: D_800F4B8B (D_800F4B8B@BATTLE/BATTLE.PRG)
char Event_weatherTweenDuration;

// decomp: D_800F4BB0 (D_800F4BB0@BATTLE/BATTLE.PRG)
u_short* Event_dialogStrings;

// decomp: D_800F4BB4 (D_800F4BB4@BATTLE/BATTLE.PRG)
char Event_cachedFlagValue;

// decomp: D_800F4BB6 (D_800F4BB6@BATTLE/BATTLE.PRG)
short Event_isFieldCameraRestoreScripted;

// decomp: D_800F4BB8 (D_800F4BB8@BATTLE/BATTLE.PRG)
char Event_isStartConfirmed;

// decomp: D_800F4BC0 (D_800F4BC0@BATTLE/BATTLE.PRG)
char* Event_vibrationPatterns[10];

// decomp: D_800F4BE0 (D_800F4BE0@BATTLE/BATTLE.PRG)
u_short Event_eventSource;

// decomp: D_800F4BE2 (D_800F4BE2@BATTLE/BATTLE.PRG)
short Event_roomChangeState;

// decomp: D_800F4BE4 (D_800F4BE4@BATTLE/BATTLE.PRG)
char Event_isScanlineOverlayEnabled;

// decomp: D_800F4BF8 (D_800F4BF8@BATTLE/BATTLE.PRG)
char Event_isMenuTransitionActive;

// decomp: vs_battle_textBoxStatuses (vs_battle_textBoxStatuses@BATTLE/BATTLE.PRG)
char Event_isTextBoxAwaitingClose[80];

// decomp: D_800F4C10 (D_800F4C10@BATTLE/BATTLE.PRG)
char Event_loopCounters[64];

// decomp: D_800F4C20 (D_800F4C20@BATTLE/BATTLE.PRG)
char Event_stopsMusicOnSkip;

// decomp: D_800F4C22 (D_800F4C22@BATTLE/BATTLE.PRG)
short Event_isMainThreadFinished;

// decomp: D_800F4C24 (D_800F4C24@BATTLE/BATTLE.PRG)
void* Event_chainedEventBuffer;

// decomp: D_800F4C30 (D_800F4C30@BATTLE/BATTLE.PRG)
void* Event_scriptBuffer;

// decomp: D_800F4C34 (D_800F4C34@BATTLE/BATTLE.PRG)
char Event_playbackPhase;

// decomp: D_800F4C38 (D_800F4C38@BATTLE/BATTLE.PRG)
u_char* Event_threadCursors[6];

// decomp: D_800F4C4E (D_800F4C4E@BATTLE/BATTLE.PRG)
char Event_reservedResetByte;

// decomp: _evtFile (_evtFile@BATTLE/BATTLE.PRG)
short Event_eventId;

// decomp: D_800F4C54 (D_800F4C54@BATTLE/BATTLE.PRG)
short Event_isChainedEventLoaded;

// decomp: D_800F4C58 (D_800F4C58@BATTLE/BATTLE.PRG)
short Event_threadWaitFrames[4];

// decomp: _textBoxSelectionPending (_textBoxSelectionPending@BATTLE/BATTLE.PRG)
char Event_isTextSelectionActive;

// decomp: D_800F4C64 (D_800F4C64@BATTLE/BATTLE.PRG)
uintptr_t Event_subroutineReturnCursor;

// decomp: D_800F4C68 (D_800F4C68@BATTLE/BATTLE.PRG)
char Event_isSkipForced;

// decomp: D_800F4C6A (D_800F4C6A@BATTLE/BATTLE.PRG)
char Event_isScriptBackdropEnabled;

// decomp: D_800FAF7C (D_800FAF7C@src/BATTLE/INITBTL.PRG/12AC.c)
int (*Event_kSkipOpcodeHandlers[])(u_char*, short) = { Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode,
    Event_ScriptWaitForEventFile, Event_ScriptReportInvalidOpcode, Event_ScriptWaitForSplashFile, Event_ScriptBeginSkippableCutscene, Event_ScriptEndCutsceneAndJump,
    Event_ScriptEndCutscene, Event_ScriptNoOperation, Event_ScriptNoOperation, Event_ScriptDismissDialogBox,
    Event_ScriptNoOperation, Event_ScriptYieldFrame, Event_ScriptYieldFrame, Event_ScriptLoadSplashImage, Event_ScriptConfigureSplashLayer,
    Event_ScriptTweenSplashLayer, Event_ScriptFreeSplashImage, Event_ScriptYieldFrame, Event_ScriptSetDialogInteger,
    Event_ScriptReportInvalidOpcode, Event_ScriptSetDialogTransitionDuration, Event_ScriptResetActorGroupState, Event_ScriptStoreActorTile, Event_ScriptSpawnActor,
    Event_ScriptNavigateActorToTile, Event_ScriptNoOperation, Event_ScriptSetActorGroupFlags, Event_ScriptSetActorSoundMix, Event_ScriptWalkActorToTile,
    Event_ScriptWalkActorToWorldPos, Event_ScriptSlideActorToTile, Event_ScriptSlideActorToWorldPos, Event_ScriptSetActorAttachment, Event_ScriptNavigateActorToWorldPos,
    Event_ScriptTurnActorToFacing, Event_ScriptFaceOtherActor, Event_ScriptFaceTile, Event_ScriptScaleActor, Event_ScriptRemoveActorGroup,
    Event_ScriptYieldFrame, Event_ScriptSetActorColorOverride, Event_ScriptNoOperation, Event_ScriptRotateActorAxes, Event_ScriptSetActorHeadAngles,
    Event_ScriptNoOperation, Event_ScriptNoOperation, Event_ScriptSetActorIdleAnimation, Event_ScriptLookAtActor, Event_ScriptLookAtWorldPos,
    Event_ScriptLoadObjectModel, Event_ScriptWaitForObjectLoad, Event_ScriptReleaseObjectModel, Event_ScriptSelectObjectTextureBank, Event_ScriptSetActorOverlayPass,
    Event_ScriptSetActorWeaponDrawn, Event_ScriptBranchOnFlagImmediate, Event_ScriptBranchOnFlagPair,
    Event_ScriptJumpForward, Event_ScriptNoOperation, Event_ScriptNoOperation, Event_ScriptJumpForward,
    Event_ScriptCacheFlagValue, Event_ScriptBranchOnCachedFlag,
    Event_ScriptNoOperation, Event_ScriptSetLoopCounter, Event_ScriptLoopBackward, Event_ScriptJumpForward,
    Event_ScriptJumpForward, Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_ScriptApplyImmediateFlagOperation, Event_ScriptApplyFlagPairOperation,
    Event_ScriptStoreInventoryItemPresence, Event_ScriptPreloadRoom, Event_ScriptNoOperation,
    Event_ScriptSetNpcUpdatesEnabled, Event_ScriptStoreButtonState, Event_ScriptRestoreActorVitals, Event_ScriptSetHudVisible,
    Event_ScriptRunMenuTransition, Event_ScriptConfigureTimer, Event_ScriptSetDialogSpeaker, Event_ScriptSetActorEquipmentModels, Event_ScriptSetActorYawOscillation,
    Event_ScriptYieldFrame, Event_ScriptConfigureEasingCurve, Event_ScriptSetGimmickVisibility, Event_ScriptOverrideDepthCueDistance, Event_ScriptSetRoomLightingMode,
    Event_ScriptClearAshleyStatusEffects, Event_ScriptShowGeometryGroup, Event_ScriptHideGeometryGroup, Event_ScriptSwapGeometryGroupFlags, Event_ScriptSetCollisionTileMode,
    Event_ScriptChangeRoom, Event_ScriptNoOperation, Event_ScriptNoOperation, Event_ScriptSetWipeAngle, Event_ScriptConfigureScreenTransition,
    Event_ScriptNoOperation, Event_ScriptYieldFrame, Event_ScriptTogglePaletteHook, Event_ScriptTransformPalettes, Event_ScriptSetActorRenderMode,
    Event_ScriptSetAshleyBattleMode, Event_ScriptSetRoomBackgroundVisible, Event_ScriptLoadRoomBackground, Event_ScriptWaitForRoomBackground, Event_ScriptFreeRoomBackground,
    Event_ScriptSetEnvironmentParticles, Event_ScriptSetParticleVariationEnabled, Event_ScriptControlRoomAnimation, Event_ScriptMuteEnvironmentSound, Event_ScriptSetBackgroundOffset,
    Event_ScriptSetFirstPersonView, Event_ScriptAdjustGeometryDepthBias, Event_ScriptSetBackgroundParallax, Event_ScriptReportInvalidOpcode, Event_ScriptNoOperation,
    Event_ScriptNoOperation, Event_ScriptNoOperation, Event_CommandStopAllSfx, Event_CommandStopSfx,
    Event_CommandLoadSfxSlot, Event_CommandWaitSfxSlotRelease, Event_CommandFreeSfx,
    Event_CommandSelectSfxBank, Event_CommandLoadEventMusic,
    Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode, Event_ScriptNoOperation,
    Event_CommandLoadMusicSlot, Event_CommandFreeMusic, Event_ScriptNoOperation,
    Event_ScriptNoOperation, Event_ScriptNoOperation, Event_ScriptNoOperation, Event_ScriptNoOperation, Event_ScriptNoOperation,
    Event_ScriptNoOperation, Event_CommandWaitMusicSlotLoad, Event_ScriptYieldFrame, Event_ScriptNoOperation,
    Event_ScriptNoOperation, Event_CommandLoadSoundFile, Event_CommandWaitSoundQueue,
    Event_ScriptNoOperation, Event_CommandPlayActorEffect, Event_CommandAdvanceEffectStage, Event_CommandWaitEffectState, Event_CommandYieldEffectRelease,
    Event_CommandWaitEffectContinuation, Event_CommandWaitEffectRelease, Event_CommandMarkEffectStageReady, Event_ScriptReportInvalidOpcode, Event_CommandSetMusicJumpThreshold,
    Event_ScriptNoOperation, Event_CommandSetActorTrail, Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_CommandSetEnteredDoor, Event_CommandStoreActorRangeBearing, Event_CommandSetActorActivity,
    Event_ScriptReportInvalidOpcode, Event_CommandConsumeMiscItem, Event_CommandStoreActorDistance, Event_CommandSpecialOperation,
    Event_CommandWaitScreenEffectLoad, Event_CommandScreenTransform, Event_CommandScreenTransform, Event_ScriptNoOperation, Event_CommandLoadScreenEffects,
    Event_CommandReservedScreenEffect, Event_CommandFreeScreenEffects, Event_CommandScreenTransform, Event_CommandCameraMotion, Event_CommandCameraMotion,
    Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_ScriptReportInvalidOpcode,
    Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion,
    Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion,
    Event_CommandCameraMotion, Event_CommandCameraMotion, Event_ScriptReportInvalidOpcode, Event_CommandCameraMotion, Event_CommandCameraMotion,
    Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_CommandStopCameraMotion, Event_CommandSetCameraFollowing, Event_ScriptYieldFrame, Event_CommandSetScreenCapture,
    Event_CommandTweenCameraRoll, Event_CommandTweenCameraRoll, Event_CommandScreenTransform, Event_CommandScreenTransform, Event_CommandScreenTransform,
    Event_CommandScreenTransform, Event_CommandSetCameraOriginActor, Event_CommandRestoreFieldCamera, Event_CommandCameraLensOrShake, Event_CommandCameraLensOrShake,
    Event_CommandCameraLensOrShake, Event_CommandScreenTransform, Event_CommandCameraLensOrShake, Event_CommandCameraLensOrShake, Event_ScriptYieldFrame,
    Event_CommandSetTickSpeed, Event_CommandPlayMenuSelectSfx, Event_CommandSetScanlineOverlay, Event_CommandWaitScriptContext,
    Event_CommandStartScriptContext, Event_CommandStopScriptContext, Event_CommandLoadNextEvent, Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode, Event_CommandFinishEvent, Event_CommandFinishEvent };

// decomp: BATTLE.PRG func_800AACDC (src/BATTLE/BATTLE.PRG/40564.c:1280)
void Event_RestoreScriptTextureRectangle(void)
{
    void* new_var;
    RECT sp10;
    DR_MOVE* temp_s0;

    sp10.x = 470;
    sp10.y = 384;
    sp10.w = 18;
    sp10.h = 36;

    new_var = (void*)((uintptr_t)Core_ScratchBase() + 0x000);
    temp_s0 = *((DR_MOVE**)((uintptr_t)Core_ScratchBase() + 0x000));
    new_var = (void*)0x10;

    SetDrawMove(temp_s0, &sp10, 16, 400);
    ((void (*)(DR_MOVE*))Render_AppendOverlayPrimitive)(temp_s0);

    *((void**)((uintptr_t)Core_ScratchBase() + 0x000)) = (DR_MOVE*)(temp_s0 + 1);
}

// decomp: BATTLE.PRG vs_battle_invalidOpcode (src/BATTLE/BATTLE.PRG/4A0A8.c:588)
int Event_ScriptReportInvalidOpcode(u_char* arg0, short arg1)
{
    char buf[64];

    sprintf(buf, "unknown code detected:%d,$%08x,$%02x", arg1, (u_int)arg0, *arg0);
    // Debug printing presumably ifdef'd out
    return 0;
}

// decomp: BATTLE.PRG func_800B66E4 (src/BATTLE/BATTLE.PRG/4A0A8.c:597)
int Event_ScriptNoOperation(u_char* arg0, short arg1) { return 0; }

// decomp: BATTLE.PRG func_800B66EC (src/BATTLE/BATTLE.PRG/4A0A8.c:599)
int Event_ScriptYieldFrame(u_char* arg0, short arg1) { return 4; }

// decomp: BATTLE.PRG func_800B66F4 (src/BATTLE/BATTLE.PRG/4A0A8.c:601)
int Event_ScriptWaitForEventFile(u_char* arg0, short arg1)
{
    if ((Event_eventFileDiscRequest != 0) && (*Event_eventFileDiscRequest != 4)) {
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B6744 (src/BATTLE/BATTLE.PRG/4A0A8.c:611)
int Event_ScriptBeginSkippableCutscene(u_char* arg0, short arg1)
{
    Event_skipState = 1;
    Event_stopsMusicOnSkip = !(arg0[1] & 1);
    return (Event_isSkipForced != 0) * 4;
}

// decomp: BATTLE.PRG func_800B6778 (src/BATTLE/BATTLE.PRG/4A0A8.c:618)
void Event_FinishSkippedCutscene(u_char* arg0)
{
    short i;

    for (i = 0; i < 3; ++i) {
        Event_skipPaletteResetCommand[1] = (i << 4) | 8;
        Event_ScriptTransformPalettes(Event_skipPaletteResetCommand, 0);
    }

    Event_RestoreScriptTextureRectangle();

    i = 0xF;
    while (*arg0 != i) {
        if (memcmp(arg0, Event_kSkipSoundRestoreMarker, 4) == 0) {
            State_SetScriptFlag(0xA8, 0);
            return;
        }
        arg0 = &arg0[Event_kOpcodeSizes[*arg0]];
    }

    Field_RestartRoomEffectSound();
    State_SetScriptFlag(0xA8, 0);
}

// decomp: BATTLE.PRG func_800B6868 (src/BATTLE/BATTLE.PRG/4A0A8.c:642)
int Event_ScriptEndCutsceneAndJump(u_char* arg0, short arg1)
{
    char temp_v1 = Event_skipState;
    Event_skipState = 0;
    if (temp_v1 == 2) {
        Event_FinishSkippedCutscene(arg0);
        return 4;
    }
    return (int)&arg0[Core_Memory_ReadUnalignedShort(arg0 + 1)];
}

// decomp: BATTLE.PRG func_800B68C4 (src/BATTLE/BATTLE.PRG/4A0A8.c:653)
int Event_ScriptEndCutscene(u_char* arg0, short arg1)
{
    char temp_v1 = Event_skipState;
    Event_skipState = 0;
    if (temp_v1 == 2) {
        Event_FinishSkippedCutscene(arg0);
        return 4;
    }
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_buttonPressed (src/BATTLE/BATTLE.PRG/4A0A8.c:843)
int Event_ScriptWaitForButtonAndYield(u_char* arg0, short arg1)
{
    if (Input_buttonsPressed.allButtons & arg0[1]) {
        return 4;
    }
    return 1;
}

// decomp: BATTLE.PRG vs_battle_script_jumpFwdIfFlag (src/BATTLE/BATTLE.PRG/4A0A8.c:1543)
int Event_ScriptBranchOnFlagImmediate(u_char* arg0, short arg1)
{
    char temp_a0 =
        State_ScriptFlag(((u_char)arg0[2] >> 2) | ((arg0[1] & 0xF) << 6));
    char temp_a1 = arg0[3];

    switch (arg0[1] & 0xF0) {
    case 0:
        if (temp_a1 >= temp_a0) {
            return (int)NULL;
        }
        break;
    case 16:
        if (temp_a0 >= temp_a1) {
            return (int)NULL;
        }
        break;
    case 32:
        if (temp_a0 == temp_a1) {
            return (int)NULL;
        }
        break;
    case 48:
        if (temp_a0 != temp_a1) {
            return (int)NULL;
        }
        break;
    case 64:
        if (temp_a0 < temp_a1) {
            return (int)NULL;
        }
        break;
    case 80:
        if (temp_a1 < temp_a0) {
            return (int)NULL;
        }
        break;
    case 96:
        if (temp_a0 & temp_a1) {
            return (int)NULL;
        }
        break;
    }
    return (long)&arg0[Core_Memory_ReadUnalignedShort(arg0 + 4)];
}

// decomp: BATTLE.PRG vs_battle_script_jumpFwdIfFlags (src/BATTLE/BATTLE.PRG/4A0A8.c:1589)
int Event_ScriptBranchOnFlagPair(u_char* arg0, short arg1)
{
    char temp_s0 =
        State_ScriptFlag(((u_char)arg0[2] >> 2) | ((arg0[1] & 0xF) << 6));
    char temp_a0 = State_ScriptFlag(arg0[3] | ((arg0[2] & 3) << 8));

    switch (arg0[1] & 0xF0) {
    case 0:
        if (temp_s0 <= temp_a0) {
            return (int)NULL;
        }
        break;
    case 16:
        if (temp_s0 >= temp_a0) {
            return (int)NULL;
        }
        break;
    case 32:
        if (temp_s0 == temp_a0) {
            return (int)NULL;
        }
        break;
    case 48:
        if (temp_s0 != temp_a0) {
            return (int)NULL;
        }
        break;
    case 64:
        if (temp_s0 < temp_a0) {
            return (int)NULL;
        }
        break;
    case 80:
        if (temp_s0 > temp_a0) {
            return (int)NULL;
        }
        break;
    }
    return (long)&arg0[Core_Memory_ReadUnalignedShort(arg0 + 4)];
}

// decomp: BATTLE.PRG vs_battle_script_jumpFwd (src/BATTLE/BATTLE.PRG/4A0A8.c:1630)
int Event_ScriptJumpForward(u_char* arg0, short arg1)
{
    return (long)&arg0[Core_Memory_ReadUnalignedShort(arg0 + 1)];
}

// decomp: BATTLE.PRG vs_battle_script_setFlagImmContext (src/BATTLE/BATTLE.PRG/4A0A8.c:1640)
int Event_ScriptCacheFlagValue(u_char* arg0, short arg1)
{
    Event_cachedFlagValue = State_ScriptFlag(Core_Memory_ReadUnalignedShort(arg0 + 1));
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_jumpFwdIfFlagContext (src/BATTLE/BATTLE.PRG/4A0A8.c:1646)
int Event_ScriptBranchOnCachedFlag(u_char* arg0, short arg1)
{
    if (Event_cachedFlagValue == arg0[1]) {
        return (long)&arg0[Core_Memory_ReadUnalignedShort(arg0 + 2)];
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B8C44 (src/BATTLE/BATTLE.PRG/4A0A8.c:1654)
int Event_ScriptReturnToSavedAddress(u_char* arg0, short arg1) { return Event_subroutineReturnCursor; }

// decomp: BATTLE.PRG func_800B8C54 (src/BATTLE/BATTLE.PRG/4A0A8.c:1656)
int Event_ScriptSetLoopCounter(u_char* arg0, short arg1)
{
    if (arg0[1] != 0xFF) {
        Event_loopCounters[arg0[1]] = arg0[2];
    }
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_jumpBackIf (src/BATTLE/BATTLE.PRG/4A0A8.c:1664)
int Event_ScriptLoopBackward(u_char* arg0, short arg1)
{
    if (arg0[1] == 0xFF) {
        return (long)&arg0[-Core_Memory_ReadUnalignedShort(arg0 + 2)];
    }
    if (--Event_loopCounters[arg0[1]] != 0) {
        return (long)&arg0[-Core_Memory_ReadUnalignedShort(arg0 + 2)];
    }
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_setFlagImm (src/BATTLE/BATTLE.PRG/4A0A8.c:1685)
int Event_ScriptApplyImmediateFlagOperation(u_char* arg0, short arg1)
{
    int temp_a1 = arg0[1];
    short temp_s1 = arg0[3];
    int temp_s0 = (arg0[2] >> 2) + ((temp_a1 & 0xF) << 6);

    switch (temp_a1 & 0xF0) {
    case 0x0:
        State_SetScriptFlag(temp_s0, State_ScriptFlag(temp_s0) + temp_s1);
        break;
    case 0x10:
        State_SetScriptFlag(temp_s0, State_ScriptFlag(temp_s0) - temp_s1);
        break;
    case 0x20:
        State_SetScriptFlag(temp_s0, State_ScriptFlag(temp_s0) * temp_s1);
        break;
    case 0x30:
        State_SetScriptFlag(temp_s0, State_ScriptFlag(temp_s0) / temp_s1);
        break;
    case 0x40:
        State_SetScriptFlag(temp_s0, State_ScriptFlag(temp_s0) % temp_s1);
        break;
    case 0x50:
        State_SetScriptFlag(temp_s0, State_ScriptFlag(temp_s0) & temp_s1);
        break;
    case 0x60:
        State_SetScriptFlag(temp_s0, State_ScriptFlag(temp_s0) | temp_s1);
        break;
    case 0x70:
        State_SetScriptFlag(temp_s0, temp_s1);
        break;
    }
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_setFlag (src/BATTLE/BATTLE.PRG/4A0A8.c:1720)
int Event_ScriptApplyFlagPairOperation(u_char* arg0, short arg1)
{
    int tmp;
    char prev;

    int flag0 = (arg0[2] >> 2) + ((arg0[1] & 0xF) << 6);
    int flag1 = arg0[3] + ((arg0[2] & 3) << 8);

    switch (arg0[1] & 0xF0) {
    case 0x0:
        State_SetScriptFlag(
            flag0, State_ScriptFlag(flag0) + State_ScriptFlag(flag1));
        break;
    case 0x10:
        State_SetScriptFlag(
            flag0, State_ScriptFlag(flag0) - State_ScriptFlag(flag1));
        break;
    case 0x20:
        State_SetScriptFlag(
            flag0, State_ScriptFlag(flag0) * State_ScriptFlag(flag1));
        break;
    case 0x30:
        State_SetScriptFlag(
            flag0, State_ScriptFlag(flag0) / State_ScriptFlag(flag1));
        break;
    case 0x40:
        State_SetScriptFlag(
            flag0, State_ScriptFlag(flag0) % State_ScriptFlag(flag1));
        break;
    case 0x50:
        State_SetScriptFlag(
            flag0, State_ScriptFlag(flag0) & State_ScriptFlag(flag1));
        break;
    case 0x60:
        State_SetScriptFlag(
            flag0, State_ScriptFlag(flag0) | State_ScriptFlag(flag1));
        break;
    case 0x70:
        State_SetScriptFlag(flag0, State_ScriptFlag(flag1));
        break;
    case 0x80:
        tmp = State_ScriptFlag(flag0) - State_ScriptFlag(flag1);
        State_SetScriptFlag(flag0, ABS(tmp));
        break;
    case 0x90:
        prev = State_ScriptFlag(flag0);
        State_SetScriptFlag(flag0, State_ScriptFlag(flag1));
        State_SetScriptFlag(flag1, prev);
        break;
    }
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_setFlagIfItemInInventory (src/BATTLE/BATTLE.PRG/4A0A8.c:1773)
int Event_ScriptStoreInventoryItemPresence(u_char* arg0, short arg1)
{
    State_SetScriptFlag(Core_Memory_ReadUnalignedShort(arg0 + 3),
        Items_InventoryItemCount(Core_Memory_ReadUnalignedShort(arg0 + 1)));
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_setFlagIfButton (src/BATTLE/BATTLE.PRG/4A0A8.c:1798)
int Event_ScriptStoreButtonState(u_char* arg0, short arg1)
{
    short temp_v0 = Core_Memory_ReadUnalignedShort(arg0 + 1);
    State_SetScriptFlag(temp_v0 & 0x3FF, temp_v0 & 0x8000
                                                ? Input_buttonsPressed.ports[0].lowByte
                                                : Input_previousButtons);
    return 0;
}

// decomp: BATTLE.PRG func_800B94D8 (src/BATTLE/BATTLE.PRG/4A0A8.c:1886)
int Event_ScriptConfigureTimer(u_char* arg0, short arg1)
{
    if (arg0[1] != 0) {
        State_StartSceneTimer(Event_kSceneTimerModes[arg0[3]], Event_kSceneTimerResetScopes[arg0[2]],
            (arg0[4] << 0x18) | (arg0[5] << 0x10) | (arg0[6] << 8));
    } else {
        State_ResetSceneTimer();
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B96B0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1934)
int Event_ScriptWaitForButtonOrTimeout(u_char* arg0, short arg1)
{
    if (Event_threadWaitFrames[arg1] == 0) {
        Event_threadWaitFrames[arg1] = arg0[2];
        return 1;
    }
    if ((Input_buttonsPressed.allButtons & arg0[1]) || (--Event_threadWaitFrames[arg1] <= 0)) {
        Event_threadWaitFrames[arg1] = 0;
        Input_buttonsPressed.allButtons = 0;
        return 0;
    }
    return 1;
}

// decomp: BATTLE.PRG func_800B9724 (src/BATTLE/BATTLE.PRG/4A0A8.c:1948)
int Event_ScriptConfigureEasingCurve(u_char* arg0, short arg1)
{
    int var_v1;
    u_char* temp_a1;
    u_short* new_var;

    new_var = Event_customEasingCurves;
    temp_a1 = (u_char*)((arg0[1] >> 6) + new_var);
    temp_a1[0] = ((arg0[1] >> 4) & 3);

    var_v1 = arg0[1] & 7;
    if (arg0[1] & 8) {
        var_v1 |= 0xF8;
    }
    temp_a1[1] = var_v1;
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_specialOp (src/BATTLE/BATTLE.PRG/4A0A8.c:2651)
int Event_CommandSpecialOperation(u_char* arg0, short arg1)
{
    char sp10[32];

    switch (arg0[1]) {
    case 0:
        Actors_QueueAshleyModelLoad(arg0[2]);
        break;
    case 1:
        if (Event_isMenuTransitionActive != 0) {
            if (Battle_UpdateRewardSequence() != 0) {
                return 1;
            }
            State_SetScriptFlag(0xB6, 0);
            Event_isMenuTransitionActive = 0;
            Boot_RunEndingAndReturnToTitle();
            break;
        }
        State_SetScriptFlag(0xB6, 3);
        Battle_BeginEventRewardResults();
        Event_isMenuTransitionActive = 1;
        return 1;
    case 2:
        Event_isSkipForced = 1;
        break;
    case 3:
        if (Battle_IsActionUnlocked(arg0[2]) != 0) {
            State_SetScriptFlag(0xB1, 0);
        } else {
            Battle_UnlockAction(arg0[2]);
            State_SetScriptFlag(0xB1, 1);
        }
        break;
    case 4:
        State_SetScriptFlag(0xB1, Actors_CountActorsByDisposition(4));
        break;
    case 5:
        Event_SetEventActorActivity(arg0[2]);
        break;
    case 6:
        Core_Memory_FreeFromHighEnd(Event_chainedEventBuffer);
        break;
    case 7:
        Event_SetEventFieldCameraOverride(arg0[2]);
        break;
    case 8:
        State_SetSceneTimerPaused(arg0[2]);
        break;
    case 9:
        Event_StartEventCameraOrbitReturn(arg0[2]);
        break;
    case 10:
        Field_SetCameraPitchPreset(Event_kSpecialOperationTenArguments[arg0[2]]);
        break;
    case 11:
        Event_ScaleEventCameraDuration(arg0[2]);
        break;
    case 12:
        Field_FreezeMovingPlatforms(arg0[2]);
        break;
    case 13:
        Field_ClearGeometryOcclusion();
        break;
    case 14:
        Field_SetMovingPlatformsPaused(arg0[2]);
        break;
    case 15:
        Effects_ScreenDistortion_SetVignetteEnabled(arg0[2]);
        break;
    case 16:
        Event_subroutineReturnCursor = (uintptr_t)(arg0 + 6);
        return (long)&Event_specialSubroutineScript;
    case 17:
        Field_CancelPendingRoomVisit();
        break;
    default:
        sprintf(sp10, "<SPECIAL=%d> IS UNDEFINED!", arg0[1]);
        break;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800BB474 (src/BATTLE/BATTLE.PRG/4A0A8.c:2799)
int Event_CommandWaitFrames(u_char* arg0, short arg1)
{
    if (Event_threadWaitFrames[arg1] == 0) {
        Event_threadWaitFrames[arg1] = arg0[1];
        return 1;
    }

    --Event_threadWaitFrames[arg1];

    if ((Event_threadWaitFrames[arg1] << 0x10) <= 0) {
        Event_threadWaitFrames[arg1] = 0;
        return 0;
    }
    return 1;
}

// decomp: BATTLE.PRG func_800BB4CC (src/BATTLE/BATTLE.PRG/4A0A8.c:2815)
int Event_CommandSetTickSpeed(u_char* arg0, short arg1)
{
    State_SetTickSpeed(arg0[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800BB504 (src/BATTLE/BATTLE.PRG/4A0A8.c:2827)
int Event_CommandWaitScriptContext(u_char* arg0, short arg1)
{
    u_short temp_a0 = arg0[1] & 3;
    if ((arg0[1] & 0x80)) {
        Event_threadCursors[temp_a0] = 0;
        return 0;
    }
    return Event_threadCursors[temp_a0] != 0;
}

// decomp: BATTLE.PRG func_800BB554 (src/BATTLE/BATTLE.PRG/4A0A8.c:2837)
int Event_CommandStartScriptContext(u_char* arg0, short arg1)
{
    Event_threadCursors[arg0[1]] = arg0 + 4;
    return (int)&arg0[Core_Memory_ReadUnalignedShort(arg0 + 2)];
}

// decomp: BATTLE.PRG func_800BB5A4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2843)
int Event_CommandStopScriptContext(u_char* arg0, short arg1) { return 3; }

// decomp: BATTLE.PRG func_800BB5AC (src/BATTLE/BATTLE.PRG/4A0A8.c:2845)
int Event_CommandLoadNextEvent(u_char* arg0, short arg1)
{
    Event_eventId = Core_Memory_ReadUnalignedShort(arg0 + 1);
    Event_chainedEventBuffer = Core_Memory_AllocateFromHighEnd(0x1800);
    Event_QueueEventScriptLoad(Event_eventId, Event_chainedEventBuffer);
    Event_isChainedEventLoaded = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800BB604 (src/BATTLE/BATTLE.PRG/4A0A8.c:2854)
int Event_CommandFinishEvent(u_char* arg0, short arg1)
{
    int i;
    int var_s1;

    var_s1 = 0;

    for (i = 0; i < 8; ++i) {
        if (Menu_DismissTextBox(i) != 0) {
            ++var_s1;
        }
    }

    if (var_s1 == 0) {
        State_SetTickSpeed(2);
        return 2;
    }
    return 1;
}

// decomp: BATTLE.PRG func_800BEB00 (src/BATTLE/BATTLE.PRG/4A0A8.c:4246)
int Event_EventPlaybackState(void) { return Event_playbackPhase; }

// decomp: BATTLE.PRG func_800BEB10 (src/BATTLE/BATTLE.PRG/4A0A8.c:4248)
short Event_PendingEventSelector(void) { return (Event_eventSource << 0xC) | Event_eventId; }

// decomp: BATTLE.PRG func_800BEC30 (src/BATTLE/BATTLE.PRG/4A0A8.c:4287)
int Event_RequestEventStart(void)
{
    if (Event_playbackPhase == 1) {
        Event_isStartConfirmed = Event_playbackPhase;
        return 1;
    }
    return 1;
}

// decomp: BATTLE.PRG func_800BF5F0 (src/BATTLE/BATTLE.PRG/4A0A8.c:4588)
void Event_InitializeEventScriptData(u_char* data)
{
    short i;
    u_char* table;
    Event_dialogStrings = (u_short*)(data + *(u_short*)(data + 2));
    table = data + *(u_short*)(data + 4);
    for (i = 9; i < 16; ++i) {
        Event_textBoxFrameStyles[i] = ((u_short*)table)[i - 8] ? table + ((u_short*)table)[i - 8] : 0;
    }
    table = data + *(u_short*)(data + 6);
    for (i = 0; i < 8; ++i) {
        Event_vibrationPatterns[i] = ((u_short*)table)[i] ? (char*)table + ((u_short*)table)[i] : 0;
    }
    Event_threadCursors[0] = data + 16;
    Event_threadCursors[1] = Event_threadCursors[2] = Event_threadCursors[3] = 0;
    Event_skipState = 0;
    Event_isSkipForced = 0;
    State_SetScriptFlag(168, 0);
    Event_threadWaitFrames[1] = Event_threadWaitFrames[2] = Event_threadWaitFrames[3] = 0;
    Event_isEffectWaitPending = 0;
    Event_reservedResetByte = 0;
    Event_isTextSelectionActive = 0;
    Event_isScanlineOverlayEnabled = 0;
    Event_isMenuTransitionActive = 0;
    Event_roomChangeState = 0;
    Event_isFieldCameraRestoreScripted = 0;
    Event_threadWaitFrames[0] = 0;
    Event_isMainThreadFinished = 0;
    for (i = 0; i < 8; ++i) {
        Event_isTextBoxAwaitingClose[i] = 0;
    }
    for (i = 0; i < 4; ++i) {
        ((Event_ScriptBytePair*)Event_customEasingCurves)[i].lowByte = ((Event_ScriptBytePair*)Event_customEasingCurves)[i].highByte = 0;
    }
    Event_weatherTweenDuration = 0;
    Event_splashTweens[0].durationFrames = Event_splashTweens[1].durationFrames = Event_splashTweens[2].durationFrames = 0;
    Event_musicSlot = Audio_CurrentMusicSlot();
    Event_eventFileDiscRequest = 0;
    for (i = 16; i < 32; ++i) {
        State_SetScriptFlag(i, 0);
    }
}

// decomp: BATTLE.PRG func_800BF850 (src/BATTLE/BATTLE.PRG/4A0A8.c:4633)
int Event_UpdateEventPlayback(void)
{
    short i;
    int _[2] __attribute__((unused));

    if (Event_playbackPhase == 0) {
        return 0;
    }
    if (Event_playbackPhase == 1) {
        if (Event_isStartConfirmed != 0) {
            Event_playbackPhase = 2;
            Event_isStartConfirmed = 0;
            Event_InitializeEventCamera();
            ((void (*)())Actors_ReturnAllActorsToIdle)();
            for (i = 0; i < 17; ++i) {
                Event_scriptControlledActors[i] = 0;
            }
            return 2;
        }
        return 0;
    }
    if (Event_playbackPhase == 2) {
        Event_isScriptBackdropEnabled = 0;
        Event_isChainedEventLoaded = 0;
        switch ((short)Event_eventSource) {
        case 1:
            Event_InitializeEventScriptData(Event_scriptBuffer);
            Event_playbackPhase = 4;
            break;
        case 2:
            Event_InitializeEventScriptData(
                (u_char*)Event_roomDoorEventData + ((short*)Event_roomDoorEventData)[Event_eventId]);
            Event_playbackPhase = 4;
            break;
        case 0:
            Event_QueueEventScriptLoad(Event_eventId, Event_scriptBuffer);
            Event_playbackPhase = 3;
            break;
        }
        return 2;
    }
    if (Event_playbackPhase == 3) {
        if (*Event_eventFileDiscRequest == 4) {
            Content_ReleaseDiscRequest((Content_DiscQueueSlot*)Event_eventFileDiscRequest);
            if (Event_isChainedEventLoaded != 0) {
                Core_Memory_CopyAlignedWords(Event_scriptBuffer, Event_chainedEventBuffer, 0x1800);
                Core_Memory_FreeFromHighEnd(Event_chainedEventBuffer);
                Event_isChainedEventLoaded = 0;
            }
            Event_InitializeEventScriptData(Event_scriptBuffer);
            Event_playbackPhase = 4;
        }
        return 2;
    }
    Event_UpdateEventCameraOrigin();
    if (Event_isMainThreadFinished == 0) {
        if (Event_ExecuteScriptThread(Event_threadCursors, 0) != 0) {
            if (Event_isFieldCameraRestoreScripted == 0 && Event_isChainedEventLoaded == 0 && Event_roomChangeState == 0) {
                Event_RestoreEventFieldCamera(-1, -1);
            }
            Event_isMainThreadFinished = 1;
        }
        for (i = 1; i < 4; ++i) {
            if (Event_threadCursors[i] != NULL) {
                Event_ExecuteScriptThread(&Event_threadCursors[i], i);
            }
        }
    }
    Event_UpdateEventCameraTracks();
    Event_UpdateEventScreenTransforms();
    Event_ApplyEventCameraState();
    Event_UpdateEventImageTweens();
    Event_UpdateEventWeatherTween();
    if (Event_isScanlineOverlayEnabled != 0) {
        Event_DrawEventScanlineOverlay();
    }
    if (Event_isMainThreadFinished != 0 && (u_char)Event_EventCameraCompletionMask() == 0xFF) {
        if (Event_isChainedEventLoaded != 0) {
            Event_playbackPhase = 3;
            Event_eventSource = 0;
        } else {
            ((void (*)())Event_ReleaseScriptActorControl)();
            Event_playbackPhase = 0;
        }
        Event_isMainThreadFinished = 0;
    }
    return 2;
}

// decomp: BATTLE.PRG func_800BFBB8 (src/BATTLE/BATTLE.PRG/573B8.c:67)
short Event_ExecuteScriptThread(u_char** arg0, short arg1)
{
    int ret;
    u_char* script = *arg0;

    if ((arg1 == 0) && (Event_skipState == 1)
        && ((Input_buttonsPressed.allButtons & PADstart) || (Event_isSkipForced != 0))) {
        Event_isSkipForced = 0;
        Event_threadWaitFrames[3] = 0;
        Event_threadWaitFrames[2] = 0;
        Event_threadWaitFrames[1] = 0;
        Event_threadWaitFrames[0] = 0;
        Event_isTextBoxAwaitingClose[3] = 0;
        Event_isTextBoxAwaitingClose[2] = 0;
        Event_isTextBoxAwaitingClose[1] = 0;
        Event_isTextBoxAwaitingClose[0] = 0;

        if (Event_stopsMusicOnSkip != 0) {
            Audio_StopMusicSlotPlayback(0);
        }

        Audio_StopAllSfxPlayback();
        Input_StopVibrationPatterns(0, 2);
        Event_ResetCutsceneActorsToIdle();
        Field_BeginScreenFadeOut(0, 0xFF, 0);
        Render_DisableDisplay();
        State_SetScriptFlag(0xA8, 1);
        Event_skipState = 2;
    }

    while (1) {
        if (Event_skipState != 2) {
            ret = Event_kOpcodeHandlers[*script](script, arg1);
        } else {
            ret = Event_legacySkipHandlerCopy[*script](script, arg1);
        }

        switch (ret) {
        case 0:
            script += Event_OpcodeSize(script);
            continue;

        case 1:
            break;

        case 3:
            *arg0 = NULL;
            return 0;

        case 2:
            return 1;

        case 4:
            script += Event_OpcodeSize(script);
            break;

        default:
            script = (u_char*)ret;
            continue;
        }

        *arg0 = script;
        return 0;
    }
}

// decomp: BATTLE.PRG func_800BFD9C (src/BATTLE/BATTLE.PRG/573B8.c:133)
void Event_ReleaseScriptActorControl(void)
{
    int i;

    for (i = 0; i < 17; i++) {
        if ((*(unsigned char (*)[17])&Event_scriptControlledActors)[i] != 0) {
            Actors_ResetActorMovement(i);
        }
    }

    Event_FreeEventCamera();
}

// decomp: BATTLE.PRG func_800BFE34 (src/BATTLE/BATTLE.PRG/573B8.c:154)
int Event_OpcodeSize(u_char* arg0) { return Event_kOpcodeSizes[arg0[0]]; }

// decomp: BATTLE.PRG func_800BFE50 (src/BATTLE/BATTLE.PRG/573B8.c:156)
int Event_ResolveScriptActorReference(u_short arg0)
{
    switch (arg0 >> 12) {
    case 0:
        return Actors_FindActorByModelId(arg0 & 0xFFF);
    case 1:
        return arg0 & 0x1F;
    case 2:
        return arg0;
    }
}

// decomp: BATTLE.PRG func_800BFEBC (src/BATTLE/BATTLE.PRG/573B8.c:168)
int Event_EvaluateInterpolationCurve(short mode, short frame, short duration)
{
    short position = frame;
    int value;
    int power;
    int i;

    if (frame > duration) {
        position = duration;
    }

    switch (mode) {
    case 0:
        return position * ONE / duration;
    case 1:
        return Core_Gte_Sin(position * (ONE / 4) / duration);
    case 2:
        return (Core_Gte_Cos(position * (ONE / 2) / duration + ONE / 2) + ONE) >> 1;
    case 3:
        return Core_Gte_Sin(position * (ONE / 4) / duration - ONE / 4) + ONE;
    default:
        mode -= 4;
        power = (*(Event_InterpolationCurve (*)[])&Event_customEasingCurves)[mode].power;
        value = position * ONE / duration;

        if (power != 0) {
            if (power > 0) {
                for (i = 0; i < power; ++i) {
                    value = (value * value) >> 12;
                }
            } else {
                value = ONE - value;

                for (i = 0; i < -power; ++i) {
                    value = (value * value) >> 12;
                }

                value = ONE - value;
            }
        }

        switch ((*(Event_InterpolationCurve (*)[])&Event_customEasingCurves)[mode].curveFamily) {
        case 1:
            return Core_Gte_Sin(value / 4);
        case 2:
            return (Core_Gte_Cos(value / 2 + ONE / 2) + ONE) >> 1;
        case 3:
            return Core_Gte_Sin(value / 4 - ONE / 4) + ONE;
        case 0:
        default:
            return value;
        }
    }
}

// decomp: BATTLE.PRG func_800C00E8 (src/BATTLE/BATTLE.PRG/573B8.c:223)
void Event_QueueEventScriptLoad(int arg0, void* arg1)
{
    Event_eventFileDiscLocation.size = VS_0000_EVT_SIZE;
    Event_eventFileDiscLocation.lba = (Event_eventId * 3) + VS_0000_EVT_LBA; // .EVT files must be sequential
    (*(Content_DiscQueueSlot* (*))&Event_eventFileDiscRequest) = Content_AllocateDiscRequest(&Event_eventFileDiscLocation);
    Content_EnqueueDiscRequest((*(Content_DiscQueueSlot* (*))&Event_eventFileDiscRequest), arg1);
}

// decomp: BATTLE.PRG func_800C0150 (src/BATTLE/BATTLE.PRG/573B8.c:231)
void Event_ResetCutsceneActorsToIdle(void)
{
    short i;

    for (i = 0; i < 17; ++i) {
        if (Actors_ActorAtSlot(i) != NULL) {
            ((void (*)(int, int, int, int))Actors_PlayActorAnimation)(i, 1, 0, 0);
        }
    }
}

// decomp: INITBTL.PRG _initScriptFunctionTable (src/BATTLE/INITBTL.PRG/12AC.c:69)
void Event_InitializeScriptOpcodeTable(void)
{
    Event_isAmbientSoundSuspended = 0;
    Event_roomChangeState = 0;
    Event_scriptBuffer = Core_Memory_AllocateFromLowEnd(0x1800);
    Event_vibrationPatternBuffer = Core_Memory_AllocateFromLowEnd(0x80);
    Event_legacySkipHandlerCopy = Core_Memory_AllocateFromLowEnd(0x400);
    Core_Memory_CopyAlignedWords(Event_legacySkipHandlerCopy, Event_kSkipOpcodeHandlers, 0x400);
}

