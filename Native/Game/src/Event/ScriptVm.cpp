// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Event/ScriptActorOpcodes.h"
#include "Event/ScriptAudioOpcodes.h"
#include "Event/ScriptCamera.h"
#include "Event/ScriptControlOpcodes.h"
#include "Event/ScriptDialogOpcodes.h"
#include "Event/ScriptEffectOpcodes.h"
#include "Event/ScriptField.h"
#include "Event/ScriptScreen.h"
#include "Event/ScriptVm.h"
#include "Event/WeaponModeOpcode.h"

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
intptr_t (*Event_kSkipOpcodeHandlers[256])(u_char*, short) = { Event_ScriptReportInvalidOpcodeOpcode,
    Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptReportInvalidOpcodeOpcode,
    Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptReportInvalidOpcodeOpcode,
    Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptReportInvalidOpcodeOpcode,
    Event_ScriptWaitForEventFileOpcode, Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptWaitForSplashFile, Event_ScriptBeginSkippableCutsceneOpcode, Event_ScriptEndCutsceneAndJumpOpcode,
    Event_ScriptEndCutsceneOpcode, Event_ScriptNoOperationOpcode, Event_ScriptNoOperationOpcode, Event_ScriptDismissDialogBoxOpcode,
    Event_ScriptNoOperationOpcode, Event_ScriptYieldFrameOpcode, Event_ScriptYieldFrameOpcode, Event_ScriptLoadSplashImage, Event_ScriptConfigureSplashLayer,
    Event_ScriptTweenSplashLayer, Event_ScriptFreeSplashImage, Event_ScriptYieldFrameOpcode, Event_ScriptSetDialogIntegerOpcode,
    Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptSetDialogTransitionDurationOpcode, Event_ScriptResetActorGroupStateOpcode, Event_ScriptStoreActorTileOpcode, Event_ScriptSpawnActorOpcode,
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
    Event_ScriptJumpForwardOpcode, Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptReportInvalidOpcodeOpcode,
    Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptApplyImmediateFlagOperationOpcode, Event_ScriptApplyFlagPairOperationOpcode,
    Event_ScriptStoreInventoryItemPresenceOpcode, Event_ScriptClearRoomActorCache, Event_ScriptNoOperationOpcode,
    Event_ScriptSetNpcUpdatesEnabledOpcode, Event_ScriptStoreButtonStateOpcode, Event_ScriptRestoreActorVitalsOpcode, Event_ScriptSetHudVisible,
    Event_ScriptRunMenuTransition, Event_ScriptConfigureTimerOpcode, Event_ScriptSetDialogSpeakerOpcode, Event_ScriptSetActorEquipmentModelsOpcode, Event_ScriptSetActorYawOscillationOpcode,
    Event_ScriptYieldFrameOpcode, Event_ScriptConfigureEasingCurveOpcode, Event_ScriptSetGimmickVisibility, Event_ScriptOverrideDepthCueDistance, Event_ScriptSetRoomLightingMode,
    Event_ScriptClearAshleyStatusEffectsOpcode, Event_ScriptShowGeometryGroup, Event_ScriptHideGeometryGroup, Event_ScriptSwapGeometryGroupFlags, Event_ScriptSetCollisionTileMode,
    Event_ScriptChangeRoom, Event_ScriptNoOperationOpcode, Event_ScriptNoOperationOpcode, Event_ScriptSetWipeAngle, Event_ScriptConfigureScreenTransition,
    Event_ScriptNoOperationOpcode, Event_ScriptYieldFrameOpcode, Event_ScriptTogglePaletteHook, Event_ScriptTransformPalettes, Event_ScriptSetActorRenderModeOpcode,
    Event_ScriptSetAshleyBattleModeOpcode, Event_ScriptSetRoomBackgroundVisible, Event_ScriptLoadRoomBackground, Event_ScriptWaitForRoomBackground, Event_ScriptFreeRoomBackground,
    Event_ScriptSetEnvironmentParticles, Event_ScriptSetParticleVariationEnabled, Event_ScriptControlRoomAnimation, Event_ScriptMuteEnvironmentSound, Event_ScriptSetBackgroundOffset,
    Event_ScriptSetFirstPersonView, Event_ScriptAdjustGeometryDepthBias, Event_ScriptSetBackgroundParallax, Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptNoOperationOpcode,
    Event_ScriptNoOperationOpcode, Event_ScriptNoOperationOpcode, Event_CommandStopAllSfxOpcode, Event_CommandStopSfxOpcode,
    Event_CommandLoadSfxSlotOpcode, Event_CommandWaitSfxSlotReleaseOpcode, Event_CommandFreeSfxOpcode,
    Event_CommandSelectSfxBankOpcode, Event_CommandLoadEventMusicOpcode,
    Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptReportInvalidOpcodeOpcode,
    Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptNoOperationOpcode,
    Event_CommandLoadMusicSlotOpcode, Event_CommandFreeMusicOpcode, Event_ScriptNoOperationOpcode,
    Event_ScriptNoOperationOpcode, Event_ScriptNoOperationOpcode, Event_ScriptNoOperationOpcode, Event_ScriptNoOperationOpcode, Event_ScriptNoOperationOpcode,
    Event_ScriptNoOperationOpcode, Event_CommandWaitMusicSlotLoadOpcode, Event_ScriptYieldFrameOpcode, Event_ScriptNoOperationOpcode,
    Event_ScriptNoOperationOpcode, Event_CommandLoadSoundFileOpcode, Event_CommandWaitSoundQueueOpcode,
    Event_ScriptNoOperationOpcode, Event_CommandPlayActorEffectOpcode, Event_CommandAdvanceEffectStageOpcode, Event_CommandWaitEffectStateOpcode, Event_CommandYieldEffectReleaseOpcode,
    Event_CommandWaitEffectContinuationOpcode, Event_CommandWaitEffectReleaseOpcode, Event_CommandMarkEffectStageReadyOpcode, Event_ScriptReportInvalidOpcodeOpcode, Event_CommandSetMusicJumpThresholdOpcode,
    Event_ScriptNoOperationOpcode, Event_CommandSetActorTrailOpcode, Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptReportInvalidOpcodeOpcode,
    Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptReportInvalidOpcodeOpcode,
    Event_ScriptReportInvalidOpcodeOpcode, Event_CommandSetEnteredDoorOpcode, Event_CommandStoreActorRangeBearingOpcode, Event_CommandSetActorActivityOpcode,
    Event_ScriptReportInvalidOpcodeOpcode, Event_CommandConsumeMiscItemOpcode, Event_CommandStoreActorDistanceOpcode, Event_CommandSpecialOperationOpcode,
    Event_CommandWaitScreenEffectLoadOpcode, Event_CommandScreenTransform, Event_CommandScreenTransform, Event_ScriptNoOperationOpcode, Event_CommandLoadScreenEffectsOpcode,
    Event_CommandReservedScreenEffectOpcode, Event_CommandFreeScreenEffectsOpcode, Event_CommandScreenTransform, Event_CommandCameraMotion, Event_CommandCameraMotion,
    Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_ScriptReportInvalidOpcodeOpcode,
    Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion,
    Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptReportInvalidOpcodeOpcode,
    Event_ScriptReportInvalidOpcodeOpcode, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion,
    Event_CommandCameraMotion, Event_CommandCameraMotion, Event_ScriptReportInvalidOpcodeOpcode, Event_CommandCameraMotion, Event_CommandCameraMotion,
    Event_CommandCameraMotion, Event_CommandCameraMotion, Event_CommandCameraMotion, Event_ScriptReportInvalidOpcodeOpcode,
    Event_ScriptReportInvalidOpcodeOpcode, Event_CommandStopCameraMotion, Event_CommandSetCameraFollowing, Event_ScriptYieldFrameOpcode, Event_CommandSetScreenCapture,
    Event_CommandTweenCameraRoll, Event_CommandTweenCameraRoll, Event_CommandScreenTransform, Event_CommandScreenTransform, Event_CommandScreenTransform,
    Event_CommandScreenTransform, Event_CommandSetCameraOriginActor, Event_CommandRestoreFieldCamera, Event_CommandCameraLensOrShake, Event_CommandCameraLensOrShake,
    Event_CommandCameraLensOrShake, Event_CommandScreenTransform, Event_CommandCameraLensOrShake, Event_CommandCameraLensOrShake, Event_ScriptYieldFrameOpcode,
    Event_CommandSetTickSpeedOpcode, Event_CommandPlayMenuSelectSfxOpcode, Event_CommandSetScanlineOverlayOpcode, Event_CommandWaitScriptContextOpcode,
    Event_CommandStartScriptContextOpcode, Event_CommandStopScriptContextOpcode, Event_CommandLoadNextEventOpcode, Event_ScriptReportInvalidOpcodeOpcode,
    Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptReportInvalidOpcodeOpcode,
    Event_ScriptReportInvalidOpcodeOpcode, Event_ScriptReportInvalidOpcodeOpcode, Event_CommandFinishEventOpcode, Event_CommandFinishEventOpcode };

}  // extern "C"
