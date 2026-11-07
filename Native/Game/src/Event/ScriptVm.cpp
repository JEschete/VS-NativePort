// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Actors/ActorSpawningLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Battle/AbilitiesLive.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Core/Memory/CopyLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Event/ScriptActorCommandsLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Field/RoomStateLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "State/SceneTimerLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/Rewards.h"
#include "Content/DiscLayout.h"
#include "DiscQueue.h"
#include "Effects/ScreenDistortion/ScreenDistortion.h"
#include "Event/Playback.h"
#include "Event/Script.h"
#include "Event/ScriptActorOpcodes.h"
#include "Event/ScriptAudio.h"
#include "Event/ScriptAudioOpcodes.h"
#include "Event/ScriptCamera.h"
#include "Event/ScriptControlOpcodes.h"
#include "Event/ScriptDialogOpcodes.h"
#include "Event/ScriptEffectOpcodes.h"
#include "Event/ScriptEffects.h"
#include "Event/ScriptField.h"
#include "Event/ScriptScreen.h"
#include "Event/ScriptVm.h"
#include "Event/WeaponModeOpcode.h"
#include "Field/Gimmicks.h"
#include "Menu/TextBox.h"

// decomp: D_800E9BE0 (D_800E9BE0@BATTLE/BATTLE.PRG)
char Event_skipPaletteResetCommand[336]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9BE8 (D_800E9BE8@BATTLE/BATTLE.PRG)
char Event_kSkipSoundRestoreMarker[328]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9BF4 (D_800E9BF4@BATTLE/BATTLE.PRG)
short Event_kSceneTimerModes[158]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9BF8 (D_800E9BF8@BATTLE/BATTLE.PRG)
short Event_kSceneTimerResetScopes[156]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9C00 (D_800E9C00@BATTLE/BATTLE.PRG)
short Event_kSpecialOperationTenArguments[152]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9C30 (D_800E9C30@BATTLE/BATTLE.PRG)
u_char Event_kOpcodeSizes[256]; /* contents: module image (ModuleData.cpp) */

// decomp: _opcodeFunctionTable (_opcodeFunctionTable@BATTLE/BATTLE.PRG)
intptr_t (*Event_kOpcodeHandlers[256])(u_char*, short); /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_800EB588 (D_800EB588@BATTLE/BATTLE.PRG)
u_char* Event_textBoxFrameStyles[16]; /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_800F4B28 (D_800F4B28@BATTLE/BATTLE.PRG)
u_short Event_customEasingCurves[66];

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
char Event_weaponTransitionPending;

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
intptr_t (*Event_kSkipOpcodeHandlers[256])(u_char*, short) = { Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode,
    Event_ScriptWaitForEventFile, Event_ScriptReportInvalidOpcode, Event_ScriptWaitForSplashFile, Event_ScriptBeginSkippableCutsceneOpcode, Event_ScriptEndCutsceneAndJumpOpcode,
    Event_ScriptEndCutsceneOpcode, Event_ScriptNoOperationOpcode, Event_ScriptNoOperationOpcode, Event_ScriptDismissDialogBoxOpcode,
    Event_ScriptNoOperationOpcode, Event_ScriptYieldFrameOpcode, Event_ScriptYieldFrameOpcode, Event_ScriptLoadSplashImage, Event_ScriptConfigureSplashLayer,
    Event_ScriptTweenSplashLayer, Event_ScriptFreeSplashImage, Event_ScriptYieldFrameOpcode, Event_ScriptSetDialogIntegerOpcode,
    Event_ScriptReportInvalidOpcode, Event_ScriptSetDialogTransitionDurationOpcode, Event_ScriptResetActorGroupStateOpcode, Event_ScriptStoreActorTileOpcode, Event_ScriptSpawnActorOpcode,
    Event_ScriptNavigateActorToTileOpcode, Event_ScriptNoOperationOpcode, Event_ScriptSetActorGroupFlagsOpcode, Event_ScriptSetActorSoundMixOpcode, Event_ScriptWalkActorToTileOpcode,
    Event_ScriptWalkActorToWorldPosOpcode, Event_ScriptSlideActorToTileOpcode, Event_ScriptSlideActorToWorldPosOpcode, Event_ScriptSetActorAttachmentOpcode, Event_ScriptNavigateActorToWorldPosOpcode,
    Event_ScriptTurnActorToFacingOpcode, Event_ScriptFaceOtherActorOpcode, Event_ScriptFaceTileOpcode, Event_ScriptScaleActorOpcode, Event_ScriptRemoveActorGroupOpcode,
    Event_ScriptYieldFrameOpcode, Event_ScriptSetActorColorOverrideOpcode, Event_ScriptNoOperationOpcode, Event_ScriptRotateActorAxesOpcode, Event_ScriptSetActorHeadAnglesOpcode,
    Event_ScriptNoOperationOpcode, Event_ScriptNoOperationOpcode, Event_ScriptSetActorIdleAnimationOpcode, Event_ScriptLookAtActorOpcode, Event_ScriptLookAtWorldPosOpcode,
    Event_ScriptLoadObjectModelOpcode, Event_ScriptWaitForObjectLoadOpcode, Event_ScriptReleaseObjectModelOpcode, Event_ScriptSelectObjectTextureBankOpcode, Event_ScriptSetActorOverlayPassOpcode,
    Event_ScriptSetActorWeaponDrawnOpcode, Event_ScriptBranchOnFlagImmediateOpcode, Event_ScriptBranchOnFlagPairOpcode,
    Event_ScriptJumpForwardOpcode, Event_ScriptNoOperationOpcode, Event_ScriptNoOperationOpcode, Event_ScriptJumpForwardOpcode,
    Event_ScriptCacheFlagValueOpcode, Event_ScriptBranchOnCachedFlagOpcode,
    Event_ScriptNoOperationOpcode, Event_ScriptSetLoopCounterOpcode, Event_ScriptLoopBackwardOpcode, Event_ScriptJumpForwardOpcode,
    Event_ScriptJumpForwardOpcode, Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_ScriptApplyImmediateFlagOperationOpcode, Event_ScriptApplyFlagPairOperationOpcode,
    Event_ScriptStoreInventoryItemPresenceOpcode, Event_ScriptClearRoomActorCache, Event_ScriptNoOperationOpcode,
    Event_ScriptSetNpcUpdatesEnabledOpcode, Event_ScriptStoreButtonStateOpcode, Event_ScriptRestoreActorVitalsOpcode, Event_ScriptSetHudVisible,
    Event_ScriptRunMenuTransition, Event_ScriptConfigureTimerOpcode, Event_ScriptSetDialogSpeakerOpcode, Event_ScriptSetActorEquipmentModelsOpcode, Event_ScriptSetActorYawOscillationOpcode,
    Event_ScriptYieldFrameOpcode, Event_ScriptConfigureEasingCurveOpcode, Event_ScriptSetGimmickVisibility, Event_ScriptOverrideDepthCueDistance, Event_ScriptSetRoomLightingMode,
    Event_ScriptClearAshleyStatusEffectsOpcode, Event_ScriptShowGeometryGroup, Event_ScriptHideGeometryGroup, Event_ScriptSwapGeometryGroupFlags, Event_ScriptSetCollisionTileMode,
    Event_ScriptChangeRoom, Event_ScriptNoOperationOpcode, Event_ScriptNoOperationOpcode, Event_ScriptSetWipeAngle, Event_ScriptConfigureScreenTransition,
    Event_ScriptNoOperationOpcode, Event_ScriptYieldFrameOpcode, Event_ScriptTogglePaletteHook, Event_ScriptTransformPalettes, Event_ScriptSetActorRenderModeOpcode,
    Event_ScriptSetAshleyBattleModeOpcode, Event_ScriptSetRoomBackgroundVisible, Event_ScriptLoadRoomBackground, Event_ScriptWaitForRoomBackground, Event_ScriptFreeRoomBackground,
    Event_ScriptSetEnvironmentParticles, Event_ScriptSetParticleVariationEnabled, Event_ScriptControlRoomAnimation, Event_ScriptMuteEnvironmentSound, Event_ScriptSetBackgroundOffset,
    Event_ScriptSetFirstPersonView, Event_ScriptAdjustGeometryDepthBias, Event_ScriptSetBackgroundParallax, Event_ScriptReportInvalidOpcode, Event_ScriptNoOperationOpcode,
    Event_ScriptNoOperationOpcode, Event_ScriptNoOperationOpcode, Event_CommandStopAllSfxOpcode, Event_CommandStopSfxOpcode,
    Event_CommandLoadSfxSlotOpcode, Event_CommandWaitSfxSlotReleaseOpcode, Event_CommandFreeSfxOpcode,
    Event_CommandSelectSfxBankOpcode, Event_CommandLoadEventMusicOpcode,
    Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode, Event_ScriptNoOperationOpcode,
    Event_CommandLoadMusicSlotOpcode, Event_CommandFreeMusicOpcode, Event_ScriptNoOperationOpcode,
    Event_ScriptNoOperationOpcode, Event_ScriptNoOperationOpcode, Event_ScriptNoOperationOpcode, Event_ScriptNoOperationOpcode, Event_ScriptNoOperationOpcode,
    Event_ScriptNoOperationOpcode, Event_CommandWaitMusicSlotLoadOpcode, Event_ScriptYieldFrameOpcode, Event_ScriptNoOperationOpcode,
    Event_ScriptNoOperationOpcode, Event_CommandLoadSoundFileOpcode, Event_CommandWaitSoundQueueOpcode,
    Event_ScriptNoOperationOpcode, Event_CommandPlayActorEffectOpcode, Event_CommandAdvanceEffectStageOpcode, Event_CommandWaitEffectStateOpcode, Event_CommandYieldEffectReleaseOpcode,
    Event_CommandWaitEffectContinuationOpcode, Event_CommandWaitEffectReleaseOpcode, Event_CommandMarkEffectStageReadyOpcode, Event_ScriptReportInvalidOpcode, Event_CommandSetMusicJumpThresholdOpcode,
    Event_ScriptNoOperationOpcode, Event_CommandSetActorTrailOpcode, Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_CommandSetEnteredDoorOpcode, Event_CommandStoreActorRangeBearingOpcode, Event_CommandSetActorActivityOpcode,
    Event_ScriptReportInvalidOpcode, Event_CommandConsumeMiscItemOpcode, Event_CommandStoreActorDistanceOpcode, Event_CommandSpecialOperation,
    Event_CommandWaitScreenEffectLoadOpcode, Event_CommandScreenTransform, Event_CommandScreenTransform, Event_ScriptNoOperationOpcode, Event_CommandLoadScreenEffectsOpcode,
    Event_CommandReservedScreenEffectOpcode, Event_CommandFreeScreenEffectsOpcode, Event_CommandScreenTransform, Event_CommandCameraMotion, Event_CommandCameraMotion,
    Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_ScriptReportInvalidOpcode,
    Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion,
    Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion,
    Event_CommandCameraMotion, Event_CommandCameraMotion, Event_ScriptReportInvalidOpcode, Event_CommandCameraMotion, Event_CommandCameraMotion,
    Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_CommandStopCameraMotion, Event_CommandSetCameraFollowing, Event_ScriptYieldFrameOpcode, Event_CommandSetScreenCapture,
    Event_CommandTweenCameraRoll, Event_CommandTweenCameraRoll, Event_CommandScreenTransform, Event_CommandScreenTransform, Event_CommandScreenTransform,
    Event_CommandScreenTransform, Event_CommandSetCameraOriginActor, Event_CommandRestoreFieldCamera, Event_CommandCameraLensOrShake, Event_CommandCameraLensOrShake,
    Event_CommandCameraLensOrShake, Event_CommandScreenTransform, Event_CommandCameraLensOrShake, Event_CommandCameraLensOrShake, Event_ScriptYieldFrameOpcode,
    Event_CommandSetTickSpeedOpcode, Event_CommandPlayMenuSelectSfxOpcode, Event_CommandSetScanlineOverlayOpcode, Event_CommandWaitScriptContextOpcode,
    Event_CommandStartScriptContextOpcode, Event_CommandStopScriptContextOpcode, Event_CommandLoadNextEvent, Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode,
    Event_ScriptReportInvalidOpcode, Event_ScriptReportInvalidOpcode, Event_CommandFinishEvent, Event_CommandFinishEvent };

// decomp: BATTLE.PRG func_800AACDC (src/BATTLE/BATTLE.PRG/40564.c:1280)
void Event_RestoreScriptTextureRectangle(void)
{
    void* new_var;
    RECT source_rectangle;
    DR_MOVE* move_primitive;

    source_rectangle.x = 470;
    source_rectangle.y = 384;
    source_rectangle.w = 18;
    source_rectangle.h = 36;

    new_var = (void*)((uintptr_t)Core_ScratchBase() + 0x000);
    move_primitive = *((DR_MOVE**)((uintptr_t)Core_ScratchBase() + 0x000));
    new_var = (void*)0x10;

    SetDrawMove(move_primitive, &source_rectangle, 16, 400);
    VS::Render::AppendLiveOverlayPrimitive(move_primitive);

    *((void**)((uintptr_t)Core_ScratchBase() + 0x000)) = (DR_MOVE*)(move_primitive + 1);
}

// decomp: BATTLE.PRG vs_battle_invalidOpcode (src/BATTLE/BATTLE.PRG/4A0A8.c:588)
intptr_t Event_ScriptReportInvalidOpcode(u_char* instruction, short thread_index)
{
    char buf[64];

    sprintf(buf, "unknown code detected:%d,$%08x,$%02x", thread_index, (u_int)instruction, *instruction);
    // Debug printing presumably ifdef'd out
    return 0;
}

// decomp: BATTLE.PRG func_800B66F4 (src/BATTLE/BATTLE.PRG/4A0A8.c:601)
intptr_t Event_ScriptWaitForEventFile(u_char* instruction, short thread_index)
{
    if ((Event_eventFileDiscRequest != 0) && (*Event_eventFileDiscRequest != 4)) {
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG vs_battle_script_specialOp (src/BATTLE/BATTLE.PRG/4A0A8.c:2651)
intptr_t Event_CommandSpecialOperation(u_char* instruction, short thread_index)
{
    char diagnostic_message[32];

    switch (instruction[1]) {
    case 0:
        VS::Actors::QueueLiveAshleyModelLoad(instruction[2]);
        break;
    case 1:
        if (Event_isMenuTransitionActive != 0) {
            if (Battle_UpdateRewardSequence() != 0) {
                return 1;
            }
            VS::State::SetLiveScriptFlag(0xB6, 0);
            Event_isMenuTransitionActive = 0;
            VS::Boot::RunEndingAndReturnToTitleLive();
            break;
        }
        VS::State::SetLiveScriptFlag(0xB6, 3);
        Battle_BeginEventRewardResults();
        Event_isMenuTransitionActive = 1;
        return 1;
    case 2:
        Event_isSkipForced = 1;
        break;
    case 3:
        if (VS::Battle::LiveIsActionUnlocked(instruction[2]) != 0) {
            VS::State::SetLiveScriptFlag(0xB1, 0);
        } else {
            VS::Battle::LiveUnlockAction(instruction[2]);
            VS::State::SetLiveScriptFlag(0xB1, 1);
        }
        break;
    case 4:
        VS::State::SetLiveScriptFlag(0xB1, VS::Actors::CountLiveActorsByDisposition(4));
        break;
    case 5:
        VS::Event::SetLiveEventActorActivity(instruction[2]);
        break;
    case 6:
        VS::Core::Memory::FreeLiveFromHighEnd(Event_chainedEventBuffer);
        break;
    case 7:
        Event_SetEventFieldCameraOverride(instruction[2]);
        break;
    case 8:
        VS::State::SetLiveSceneTimerPaused(instruction[2]);
        break;
    case 9:
        Event_StartEventCameraOrbitReturn(instruction[2]);
        break;
    case 10:
        VS::Field::LiveSetCameraPitchPreset(Event_kSpecialOperationTenArguments[instruction[2]]);
        break;
    case 11:
        Event_ScaleEventCameraDuration(instruction[2]);
        break;
    case 12:
        Field_FreezeMovingPlatforms(instruction[2]);
        break;
    case 13:
        VS::Field::LiveClearGeometryOcclusion();
        break;
    case 14:
        Field_SetMovingPlatformsPaused(instruction[2]);
        break;
    case 15:
        Effects_ScreenDistortion_SetVignetteEnabled(instruction[2]);
        break;
    case 16:
        Event_subroutineReturnCursor = (intptr_t)(instruction + 6);
        return (intptr_t)&Event_specialSubroutineScript;
    case 17:
        VS::Field::LiveCancelPendingRoomVisit();
        break;
    default:
        sprintf(diagnostic_message, "<SPECIAL=%d> IS UNDEFINED!", instruction[1]);
        break;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800BB5AC (src/BATTLE/BATTLE.PRG/4A0A8.c:2845)
intptr_t Event_CommandLoadNextEvent(u_char* instruction, short thread_index)
{
    Event_eventId = VS::Core::Memory::ReadLiveUnalignedShort(instruction + 1);
    Event_chainedEventBuffer = VS::Core::Memory::AllocateLiveFromHighEnd(0x1800);
    Event_QueueEventScriptLoad(Event_eventId, Event_chainedEventBuffer);
    Event_isChainedEventLoaded = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800BB604 (src/BATTLE/BATTLE.PRG/4A0A8.c:2854)
intptr_t Event_CommandFinishEvent(u_char* instruction, short thread_index)
{
    int i;
    int pending_text_box_count;

    pending_text_box_count = 0;

    for (i = 0; i < 8; ++i) {
        if (Menu_DismissTextBox(i) != 0) {
            ++pending_text_box_count;
        }
    }

    if (pending_text_box_count == 0) {
        VS::State::SetLiveTickSpeed(2);
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

// decomp: BATTLE.PRG func_800BFE34 (src/BATTLE/BATTLE.PRG/573B8.c:154)
int Event_OpcodeSize(u_char* instruction) { return Event_kOpcodeSizes[instruction[0]]; }

// decomp: BATTLE.PRG func_800BFE50 (src/BATTLE/BATTLE.PRG/573B8.c:156)
int Event_ResolveScriptActorReference(u_short actor_reference)
{
    switch (actor_reference >> 12) {
    case 0:
        return VS::Actors::FindLiveActorByModelId(actor_reference & 0xFFF);
    case 1:
        return actor_reference & 0x1F;
    case 2:
        return actor_reference;
    }
    return actor_reference;
}

// decomp: BATTLE.PRG func_800C00E8 (src/BATTLE/BATTLE.PRG/573B8.c:223)
void Event_QueueEventScriptLoad(int arg0, void* destination_buffer)
{
    Event_eventFileDiscLocation.size = VS_0000_EVT_SIZE;
    Event_eventFileDiscLocation.lba = (Event_eventId * 3) + VS_0000_EVT_LBA; // .EVT files must be sequential
    (*(Content_DiscQueueSlot* (*))&Event_eventFileDiscRequest) = VS::Content::AllocateLiveDiscRequest(&Event_eventFileDiscLocation);
    if ((*(Content_DiscQueueSlot* (*))&Event_eventFileDiscRequest) != NULL) {
        VS::Content::EnqueueLiveDiscRequest((*(Content_DiscQueueSlot* (*))&Event_eventFileDiscRequest), destination_buffer);
    } else {
        (*(Content_DiscQueueSlot* (*))&Event_eventFileDiscRequest) = VS::Content::CompleteLiveRefusedDiscRequest(&Event_eventFileDiscLocation, destination_buffer);
    }
}

// decomp: BATTLE.PRG func_800C0150 (src/BATTLE/BATTLE.PRG/573B8.c:231)
void Event_ResetCutsceneActorsToIdle(void)
{
    short i;

    for (i = 0; i < 17; ++i) {
        if (VS::Actors::LiveActorAtSlot(i) != NULL) {
            VS::Actors::PlayLiveActorAnimation(i, 1, 0, 0);
        }
    }
}

// decomp: INITBTL.PRG _initScriptFunctionTable (src/BATTLE/INITBTL.PRG/12AC.c:69)
void Event_InitializeScriptOpcodeTable(void)
{
    Event_isAmbientSoundSuspended = 0;
    Event_playbackPhase = 0;
    Event_isStartConfirmed = 0;
    Event_isMainThreadFinished = 0;
    VS::Core::Memory::ZeroLiveAlignedWords(Event_threadCursors, sizeof Event_threadCursors);
    VS::Core::Memory::ZeroLiveAlignedWords(Event_threadWaitFrames, sizeof Event_threadWaitFrames);
    Event_roomChangeState = 0;
    Event_scriptBuffer = VS::Core::Memory::AllocateLiveFromLowEnd(0x1800);
    Event_vibrationPatternBuffer = VS::Core::Memory::AllocateLiveFromLowEnd(0x80);
    Event_legacySkipHandlerCopy = (intptr_t (**)(u_char *, short))(VS::Core::Memory::AllocateLiveFromLowEnd(sizeof(Event_kSkipOpcodeHandlers)));
    VS::Core::Memory::CopyLiveAlignedWords(Event_legacySkipHandlerCopy, Event_kSkipOpcodeHandlers, sizeof(Event_kSkipOpcodeHandlers));
}

}  // extern "C"
