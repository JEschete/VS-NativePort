// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Event/ScriptActorOpcodes.hpp"
#include "Event/ScriptAudioOpcodes.hpp"
#include "Event/ScriptControlOpcodes.hpp"
#include "Event/ScriptDialogOpcodes.hpp"
#include "Event/ScriptEffectOpcodes.hpp"
#include "Event/ScriptVm.h"
#include "Event/WeaponModeOpcode.hpp"

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
intptr_t (*Event_kSkipOpcodeHandlers[256])(u_char*, short) = { VS::Event::ScriptReportInvalidOpcodeOpcode,
    VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode,
    VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode,
    VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode,
    VS::Event::ScriptWaitForEventFileOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptWaitForSplashFileOpcode, VS::Event::ScriptBeginSkippableCutsceneOpcode, VS::Event::ScriptEndCutsceneAndJumpOpcode,
    VS::Event::ScriptEndCutsceneOpcode, VS::Event::ScriptNoOperationOpcode, VS::Event::ScriptNoOperationOpcode, VS::Event::ScriptDismissDialogBoxOpcode,
    VS::Event::ScriptNoOperationOpcode, VS::Event::ScriptYieldFrameOpcode, VS::Event::ScriptYieldFrameOpcode, VS::Event::ScriptLoadSplashImageOpcode, VS::Event::ScriptConfigureSplashLayerOpcode,
    VS::Event::ScriptTweenSplashLayerOpcode, VS::Event::ScriptFreeSplashImageOpcode, VS::Event::ScriptYieldFrameOpcode, VS::Event::ScriptSetDialogIntegerOpcode,
    VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptSetDialogTransitionDurationOpcode, VS::Event::ScriptResetActorGroupStateOpcode, VS::Event::ScriptStoreActorTileOpcode, VS::Event::ScriptSpawnActorOpcode,
    VS::Event::ScriptNavigateActorToTileOpcode, VS::Event::ScriptNoOperationOpcode, VS::Event::ScriptSetActorGroupFlagsOpcode, VS::Event::ScriptSetActorSoundMixOpcode, VS::Event::ScriptWalkActorToTileOpcode,
    VS::Event::ScriptWalkActorToWorldPosOpcode, VS::Event::ScriptSlideActorToTileOpcode, VS::Event::ScriptSlideActorToWorldPosOpcode, VS::Event::ScriptSetActorAttachmentOpcode, VS::Event::ScriptNavigateActorToWorldPosOpcode,
    VS::Event::ScriptTurnActorToFacingOpcode, VS::Event::ScriptFaceOtherActorOpcode, VS::Event::ScriptFaceTileOpcode, VS::Event::ScriptScaleActorOpcode, VS::Event::ScriptRemoveActorGroupOpcode,
    VS::Event::ScriptYieldFrameOpcode, VS::Event::ScriptSetActorColorOverrideOpcode, VS::Event::ScriptNoOperationOpcode, VS::Event::ScriptRotateActorAxesOpcode, VS::Event::ScriptSetActorHeadAnglesOpcode,
    VS::Event::ScriptNoOperationOpcode, VS::Event::ScriptNoOperationOpcode, VS::Event::ScriptSetActorIdleAnimationOpcode, VS::Event::ScriptLookAtActorOpcode, VS::Event::ScriptLookAtWorldPosOpcode,
    VS::Event::ScriptLoadObjectModelOpcode, VS::Event::ScriptWaitForObjectLoadOpcode, VS::Event::ScriptReleaseObjectModelOpcode, VS::Event::ScriptSelectObjectTextureBankOpcode, VS::Event::ScriptSetActorOverlayPassOpcode,
    VS::Event::ScriptSetActorWeaponDrawnOpcode, VS::Event::ScriptBranchOnFlagImmediateOpcode, VS::Event::ScriptBranchOnFlagPairOpcode,
    VS::Event::ScriptJumpForwardOpcode, VS::Event::ScriptNoOperationOpcode, VS::Event::ScriptNoOperationOpcode, VS::Event::ScriptJumpForwardOpcode,
    VS::Event::ScriptCacheFlagValueOpcode, VS::Event::ScriptBranchOnCachedFlagOpcode,
    VS::Event::ScriptNoOperationOpcode, VS::Event::ScriptSetLoopCounterOpcode, VS::Event::ScriptLoopBackwardOpcode, VS::Event::ScriptJumpForwardOpcode,
    VS::Event::ScriptJumpForwardOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode,
    VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptApplyImmediateFlagOperationOpcode, VS::Event::ScriptApplyFlagPairOperationOpcode,
    VS::Event::ScriptStoreInventoryItemPresenceOpcode, VS::Event::ScriptClearRoomActorCacheOpcode, VS::Event::ScriptNoOperationOpcode,
    VS::Event::ScriptSetNpcUpdatesEnabledOpcode, VS::Event::ScriptStoreButtonStateOpcode, VS::Event::ScriptRestoreActorVitalsOpcode, VS::Event::ScriptSetHudVisibleOpcode,
    VS::Event::ScriptRunMenuTransitionOpcode, VS::Event::ScriptConfigureTimerOpcode, VS::Event::ScriptSetDialogSpeakerOpcode, VS::Event::ScriptSetActorEquipmentModelsOpcode, VS::Event::ScriptSetActorYawOscillationOpcode,
    VS::Event::ScriptYieldFrameOpcode, VS::Event::ScriptConfigureEasingCurveOpcode, VS::Event::ScriptSetGimmickVisibilityOpcode, VS::Event::ScriptOverrideDepthCueDistanceOpcode, VS::Event::ScriptSetRoomLightingModeOpcode,
    VS::Event::ScriptClearAshleyStatusEffectsOpcode, VS::Event::ScriptShowGeometryGroupOpcode, VS::Event::ScriptHideGeometryGroupOpcode, VS::Event::ScriptSwapGeometryGroupFlagsOpcode, VS::Event::ScriptSetCollisionTileModeOpcode,
    VS::Event::ScriptChangeRoomOpcode, VS::Event::ScriptNoOperationOpcode, VS::Event::ScriptNoOperationOpcode, VS::Event::ScriptSetWipeAngleOpcode, VS::Event::ScriptConfigureScreenTransitionOpcode,
    VS::Event::ScriptNoOperationOpcode, VS::Event::ScriptYieldFrameOpcode, VS::Event::ScriptTogglePaletteHookOpcode, VS::Event::ScriptTransformPalettesOpcode, VS::Event::ScriptSetActorRenderModeOpcode,
    VS::Event::ScriptSetAshleyBattleModeOpcode, VS::Event::ScriptSetRoomBackgroundVisibleOpcode, VS::Event::ScriptLoadRoomBackgroundOpcode, VS::Event::ScriptWaitForRoomBackgroundOpcode, VS::Event::ScriptFreeRoomBackgroundOpcode,
    VS::Event::ScriptSetEnvironmentParticlesOpcode, VS::Event::ScriptSetParticleVariationEnabledOpcode, VS::Event::ScriptControlRoomAnimationOpcode, VS::Event::ScriptMuteEnvironmentSoundOpcode, VS::Event::ScriptSetBackgroundOffsetOpcode,
    VS::Event::ScriptSetFirstPersonViewOpcode, VS::Event::ScriptAdjustGeometryDepthBiasOpcode, VS::Event::ScriptSetBackgroundParallaxOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptNoOperationOpcode,
    VS::Event::ScriptNoOperationOpcode, VS::Event::ScriptNoOperationOpcode, VS::Event::CommandStopAllSfxOpcode, VS::Event::CommandStopSfxOpcode,
    VS::Event::CommandLoadSfxSlotOpcode, VS::Event::CommandWaitSfxSlotReleaseOpcode, VS::Event::CommandFreeSfxOpcode,
    VS::Event::CommandSelectSfxBankOpcode, VS::Event::CommandLoadEventMusicOpcode,
    VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode,
    VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptNoOperationOpcode,
    VS::Event::CommandLoadMusicSlotOpcode, VS::Event::CommandFreeMusicOpcode, VS::Event::ScriptNoOperationOpcode,
    VS::Event::ScriptNoOperationOpcode, VS::Event::ScriptNoOperationOpcode, VS::Event::ScriptNoOperationOpcode, VS::Event::ScriptNoOperationOpcode, VS::Event::ScriptNoOperationOpcode,
    VS::Event::ScriptNoOperationOpcode, VS::Event::CommandWaitMusicSlotLoadOpcode, VS::Event::ScriptYieldFrameOpcode, VS::Event::ScriptNoOperationOpcode,
    VS::Event::ScriptNoOperationOpcode, VS::Event::CommandLoadSoundFileOpcode, VS::Event::CommandWaitSoundQueueOpcode,
    VS::Event::ScriptNoOperationOpcode, VS::Event::CommandPlayActorEffectOpcode, VS::Event::CommandAdvanceEffectStageOpcode, VS::Event::CommandWaitEffectStateOpcode, VS::Event::CommandYieldEffectReleaseOpcode,
    VS::Event::CommandWaitEffectContinuationOpcode, VS::Event::CommandWaitEffectReleaseOpcode, VS::Event::CommandMarkEffectStageReadyOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::CommandSetMusicJumpThresholdOpcode,
    VS::Event::ScriptNoOperationOpcode, VS::Event::CommandSetActorTrailOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode,
    VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode,
    VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::CommandSetEnteredDoorOpcode, VS::Event::CommandStoreActorRangeBearingOpcode, VS::Event::CommandSetActorActivityOpcode,
    VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::CommandConsumeMiscItemOpcode, VS::Event::CommandStoreActorDistanceOpcode, VS::Event::CommandSpecialOperationOpcode,
    VS::Event::CommandWaitScreenEffectLoadOpcode, VS::Event::CommandScreenTransformOpcode, VS::Event::CommandScreenTransformOpcode, VS::Event::ScriptNoOperationOpcode, VS::Event::CommandLoadScreenEffectsOpcode,
    VS::Event::CommandReservedScreenEffectOpcode, VS::Event::CommandFreeScreenEffectsOpcode, VS::Event::CommandScreenTransformOpcode, VS::Event::CommandCameraMotionOpcode, VS::Event::CommandCameraMotionOpcode,
    VS::Event::CommandCameraMotionOpcode, VS::Event::CommandCameraMotionOpcode, VS::Event::CommandCameraMotionOpcode, VS::Event::CommandCameraMotionOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode,
    VS::Event::CommandCameraMotionOpcode, VS::Event::CommandCameraMotionOpcode, VS::Event::CommandCameraMotionOpcode, VS::Event::CommandCameraMotionOpcode, VS::Event::CommandCameraMotionOpcode,
    VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode,
    VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::CommandCameraMotionOpcode, VS::Event::CommandCameraMotionOpcode, VS::Event::CommandCameraMotionOpcode, VS::Event::CommandCameraMotionOpcode,
    VS::Event::CommandCameraMotionOpcode, VS::Event::CommandCameraMotionOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::CommandCameraMotionOpcode, VS::Event::CommandCameraMotionOpcode,
    VS::Event::CommandCameraMotionOpcode, VS::Event::CommandCameraMotionOpcode, VS::Event::CommandCameraMotionOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode,
    VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::CommandStopCameraMotionOpcode, VS::Event::CommandSetCameraFollowingOpcode, VS::Event::ScriptYieldFrameOpcode, VS::Event::CommandSetScreenCaptureOpcode,
    VS::Event::CommandTweenCameraRollOpcode, VS::Event::CommandTweenCameraRollOpcode, VS::Event::CommandScreenTransformOpcode, VS::Event::CommandScreenTransformOpcode, VS::Event::CommandScreenTransformOpcode,
    VS::Event::CommandScreenTransformOpcode, VS::Event::CommandSetCameraOriginActorOpcode, VS::Event::CommandRestoreFieldCameraOpcode, VS::Event::CommandCameraLensOrShakeOpcode, VS::Event::CommandCameraLensOrShakeOpcode,
    VS::Event::CommandCameraLensOrShakeOpcode, VS::Event::CommandScreenTransformOpcode, VS::Event::CommandCameraLensOrShakeOpcode, VS::Event::CommandCameraLensOrShakeOpcode, VS::Event::ScriptYieldFrameOpcode,
    VS::Event::CommandSetTickSpeedOpcode, VS::Event::CommandPlayMenuSelectSfxOpcode, VS::Event::CommandSetScanlineOverlayOpcode, VS::Event::CommandWaitScriptContextOpcode,
    VS::Event::CommandStartScriptContextOpcode, VS::Event::CommandStopScriptContextOpcode, VS::Event::CommandLoadNextEventOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode,
    VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode,
    VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::ScriptReportInvalidOpcodeOpcode, VS::Event::CommandFinishEventOpcode, VS::Event::CommandFinishEventOpcode };

