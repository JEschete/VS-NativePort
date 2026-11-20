// The event script VM's module state (ScriptVmLiveState.hpp).
#include "Event/ScriptVmLiveState.hpp"

#include <cstdint>

#include "Event/ScriptActorOpcodes.hpp"
#include "Event/ScriptAudioOpcodes.hpp"
#include "Event/ScriptControlOpcodes.hpp"
#include "Event/ScriptDialogOpcodes.hpp"
#include "Event/ScriptEffectOpcodes.hpp"
#include "Event/WeaponModeOpcode.hpp"

namespace VS::Event {
// decomp: D_800E9C30 (D_800E9C30@BATTLE/BATTLE.PRG)
// Contents: the module image (ModuleData.cpp).
std::uint8_t opcodeSizes[kScriptOpcodeCount];

// decomp: _opcodeFunctionTable (_opcodeFunctionTable@BATTLE/BATTLE.PRG)
// Contents: the module image, relocated (ModulePointers.cpp).
ScriptOpcodeHandler opcodeHandlers[kScriptOpcodeCount];

// decomp: D_800EB588 (D_800EB588@BATTLE/BATTLE.PRG)
// Contents: the module image, relocated (ModulePointers.cpp).
std::uint8_t* textBoxFrameStyles[kTextBoxFrameStyleCount];

// decomp: D_800F4BB4 (D_800F4BB4@BATTLE/BATTLE.PRG)
std::int8_t cachedFlagValue;

// decomp: D_800F4BB6 (D_800F4BB6@BATTLE/BATTLE.PRG)
std::int16_t isFieldCameraRestoreScripted;

// decomp: D_800F4BB8 (D_800F4BB8@BATTLE/BATTLE.PRG)
std::uint8_t isStartConfirmed;

// decomp: D_800F4BE0 (D_800F4BE0@BATTLE/BATTLE.PRG)
std::uint16_t eventSource;

// decomp: D_800F4BE2 (D_800F4BE2@BATTLE/BATTLE.PRG)
std::int16_t roomChangeState;

// decomp: D_800F4BE4 (D_800F4BE4@BATTLE/BATTLE.PRG)
std::uint8_t isScanlineOverlayEnabled;

// decomp: D_800F4BF8 (D_800F4BF8@BATTLE/BATTLE.PRG)
std::uint8_t isMenuTransitionActive;

// decomp: vs_battle_textBoxStatuses
// (vs_battle_textBoxStatuses@BATTLE/BATTLE.PRG)
std::uint8_t isTextBoxAwaitingClose[kTextBoxCount];

// decomp: D_800F4C10 (D_800F4C10@BATTLE/BATTLE.PRG)
std::uint8_t loopCounters[kLoopCounterCount];

// decomp: D_800F4C20 (D_800F4C20@BATTLE/BATTLE.PRG)
std::uint8_t stopsMusicOnSkip;

// decomp: D_800F4C22 (D_800F4C22@BATTLE/BATTLE.PRG)
std::int16_t isMainThreadFinished;

// decomp: D_800F4C34 (D_800F4C34@BATTLE/BATTLE.PRG)
std::uint8_t playbackPhase;

// decomp: D_800F4C4E (D_800F4C4E@BATTLE/BATTLE.PRG)
std::uint8_t weaponTransitionPending;

// decomp: _evtFile (_evtFile@BATTLE/BATTLE.PRG)
std::int16_t eventId;

// decomp: D_800F4C54 (D_800F4C54@BATTLE/BATTLE.PRG)
std::int16_t isChainedEventLoaded;

// decomp: D_800F4C58 (D_800F4C58@BATTLE/BATTLE.PRG)
std::int16_t threadWaitFrames[kScriptThreadCount];

// decomp: _textBoxSelectionPending (_textBoxSelectionPending@BATTLE/BATTLE.PRG)
std::uint8_t isTextSelectionActive;

// decomp: D_800F4C68 (D_800F4C68@BATTLE/BATTLE.PRG)
std::uint8_t isSkipForced;

// decomp: D_800F4C6A (D_800F4C6A@BATTLE/BATTLE.PRG)
std::uint8_t isScriptBackdropEnabled;

// decomp: D_800FAF7C (D_800FAF7C@src/BATTLE/INITBTL.PRG/12AC.c)
ScriptOpcodeHandler skipOpcodeHandlers[kScriptOpcodeCount] = {
    // 0x00
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptWaitForEventFileOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptWaitForSplashFileOpcode,
    ScriptBeginSkippableCutsceneOpcode,
    ScriptEndCutsceneAndJumpOpcode,
    ScriptEndCutsceneOpcode,
    // 0x10
    ScriptNoOperationOpcode,
    ScriptNoOperationOpcode,
    ScriptDismissDialogBoxOpcode,
    ScriptNoOperationOpcode,
    ScriptYieldFrameOpcode,
    ScriptYieldFrameOpcode,
    ScriptLoadSplashImageOpcode,
    ScriptConfigureSplashLayerOpcode,
    ScriptTweenSplashLayerOpcode,
    ScriptFreeSplashImageOpcode,
    ScriptYieldFrameOpcode,
    ScriptSetDialogIntegerOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptSetDialogTransitionDurationOpcode,
    ScriptResetActorGroupStateOpcode,
    ScriptStoreActorTileOpcode,
    // 0x20
    ScriptSpawnActorOpcode,
    ScriptNavigateActorToTileOpcode,
    ScriptNoOperationOpcode,
    ScriptSetActorGroupFlagsOpcode,
    ScriptSetActorSoundMixOpcode,
    ScriptWalkActorToTileOpcode,
    ScriptWalkActorToWorldPosOpcode,
    ScriptSlideActorToTileOpcode,
    ScriptSlideActorToWorldPosOpcode,
    ScriptSetActorAttachmentOpcode,
    ScriptNavigateActorToWorldPosOpcode,
    ScriptTurnActorToFacingOpcode,
    ScriptFaceOtherActorOpcode,
    ScriptFaceTileOpcode,
    ScriptScaleActorOpcode,
    ScriptRemoveActorGroupOpcode,
    // 0x30
    ScriptYieldFrameOpcode,
    ScriptSetActorColorOverrideOpcode,
    ScriptNoOperationOpcode,
    ScriptRotateActorAxesOpcode,
    ScriptSetActorHeadAnglesOpcode,
    ScriptNoOperationOpcode,
    ScriptNoOperationOpcode,
    ScriptSetActorIdleAnimationOpcode,
    ScriptLookAtActorOpcode,
    ScriptLookAtWorldPosOpcode,
    ScriptLoadObjectModelOpcode,
    ScriptWaitForObjectLoadOpcode,
    ScriptReleaseObjectModelOpcode,
    ScriptSelectObjectTextureBankOpcode,
    ScriptSetActorOverlayPassOpcode,
    ScriptSetActorWeaponDrawnOpcode,
    // 0x40
    ScriptBranchOnFlagImmediateOpcode,
    ScriptBranchOnFlagPairOpcode,
    ScriptJumpForwardOpcode,
    ScriptNoOperationOpcode,
    ScriptNoOperationOpcode,
    ScriptJumpForwardOpcode,
    ScriptCacheFlagValueOpcode,
    ScriptBranchOnCachedFlagOpcode,
    ScriptNoOperationOpcode,
    ScriptSetLoopCounterOpcode,
    ScriptLoopBackwardOpcode,
    ScriptJumpForwardOpcode,
    ScriptJumpForwardOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    // 0x50
    ScriptApplyImmediateFlagOperationOpcode,
    ScriptApplyFlagPairOperationOpcode,
    ScriptStoreInventoryItemPresenceOpcode,
    ScriptClearRoomActorCacheOpcode,
    ScriptNoOperationOpcode,
    ScriptSetNpcUpdatesEnabledOpcode,
    ScriptStoreButtonStateOpcode,
    ScriptRestoreActorVitalsOpcode,
    ScriptSetHudVisibleOpcode,
    ScriptRunMenuTransitionOpcode,
    ScriptConfigureTimerOpcode,
    ScriptSetDialogSpeakerOpcode,
    ScriptSetActorEquipmentModelsOpcode,
    ScriptSetActorYawOscillationOpcode,
    ScriptYieldFrameOpcode,
    ScriptConfigureEasingCurveOpcode,
    // 0x60
    ScriptSetGimmickVisibilityOpcode,
    ScriptOverrideDepthCueDistanceOpcode,
    ScriptSetRoomLightingModeOpcode,
    ScriptClearAshleyStatusEffectsOpcode,
    ScriptShowGeometryGroupOpcode,
    ScriptHideGeometryGroupOpcode,
    ScriptSwapGeometryGroupFlagsOpcode,
    ScriptSetCollisionTileModeOpcode,
    ScriptChangeRoomOpcode,
    ScriptNoOperationOpcode,
    ScriptNoOperationOpcode,
    ScriptSetWipeAngleOpcode,
    ScriptConfigureScreenTransitionOpcode,
    ScriptNoOperationOpcode,
    ScriptYieldFrameOpcode,
    ScriptTogglePaletteHookOpcode,
    // 0x70
    ScriptTransformPalettesOpcode,
    ScriptSetActorRenderModeOpcode,
    ScriptSetAshleyBattleModeOpcode,
    ScriptSetRoomBackgroundVisibleOpcode,
    ScriptLoadRoomBackgroundOpcode,
    ScriptWaitForRoomBackgroundOpcode,
    ScriptFreeRoomBackgroundOpcode,
    ScriptSetEnvironmentParticlesOpcode,
    ScriptSetParticleVariationEnabledOpcode,
    ScriptControlRoomAnimationOpcode,
    ScriptMuteEnvironmentSoundOpcode,
    ScriptSetBackgroundOffsetOpcode,
    ScriptSetFirstPersonViewOpcode,
    ScriptAdjustGeometryDepthBiasOpcode,
    ScriptSetBackgroundParallaxOpcode,
    ScriptReportInvalidOpcodeOpcode,
    // 0x80
    ScriptNoOperationOpcode,
    ScriptNoOperationOpcode,
    ScriptNoOperationOpcode,
    CommandStopAllSfxOpcode,
    CommandStopSfxOpcode,
    CommandLoadSfxSlotOpcode,
    CommandWaitSfxSlotReleaseOpcode,
    CommandFreeSfxOpcode,
    CommandSelectSfxBankOpcode,
    CommandLoadEventMusicOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptNoOperationOpcode,
    // 0x90
    CommandLoadMusicSlotOpcode,
    CommandFreeMusicOpcode,
    ScriptNoOperationOpcode,
    ScriptNoOperationOpcode,
    ScriptNoOperationOpcode,
    ScriptNoOperationOpcode,
    ScriptNoOperationOpcode,
    ScriptNoOperationOpcode,
    ScriptNoOperationOpcode,
    CommandWaitMusicSlotLoadOpcode,
    ScriptYieldFrameOpcode,
    ScriptNoOperationOpcode,
    ScriptNoOperationOpcode,
    CommandLoadSoundFileOpcode,
    CommandWaitSoundQueueOpcode,
    ScriptNoOperationOpcode,
    // 0xA0
    CommandPlayActorEffectOpcode,
    CommandAdvanceEffectStageOpcode,
    CommandWaitEffectStateOpcode,
    CommandYieldEffectReleaseOpcode,
    CommandWaitEffectContinuationOpcode,
    CommandWaitEffectReleaseOpcode,
    CommandMarkEffectStageReadyOpcode,
    ScriptReportInvalidOpcodeOpcode,
    CommandSetMusicJumpThresholdOpcode,
    ScriptNoOperationOpcode,
    CommandSetActorTrailOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    // 0xB0
    ScriptReportInvalidOpcodeOpcode,
    CommandSetEnteredDoorOpcode,
    CommandStoreActorRangeBearingOpcode,
    CommandSetActorActivityOpcode,
    ScriptReportInvalidOpcodeOpcode,
    CommandConsumeMiscItemOpcode,
    CommandStoreActorDistanceOpcode,
    CommandSpecialOperationOpcode,
    CommandWaitScreenEffectLoadOpcode,
    CommandScreenTransformOpcode,
    CommandScreenTransformOpcode,
    ScriptNoOperationOpcode,
    CommandLoadScreenEffectsOpcode,
    CommandReservedScreenEffectOpcode,
    CommandFreeScreenEffectsOpcode,
    CommandScreenTransformOpcode,
    // 0xC0
    CommandCameraMotionOpcode,
    CommandCameraMotionOpcode,
    CommandCameraMotionOpcode,
    CommandCameraMotionOpcode,
    CommandCameraMotionOpcode,
    CommandCameraMotionOpcode,
    ScriptReportInvalidOpcodeOpcode,
    CommandCameraMotionOpcode,
    CommandCameraMotionOpcode,
    CommandCameraMotionOpcode,
    CommandCameraMotionOpcode,
    CommandCameraMotionOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    // 0xD0
    CommandCameraMotionOpcode,
    CommandCameraMotionOpcode,
    CommandCameraMotionOpcode,
    CommandCameraMotionOpcode,
    CommandCameraMotionOpcode,
    CommandCameraMotionOpcode,
    ScriptReportInvalidOpcodeOpcode,
    CommandCameraMotionOpcode,
    CommandCameraMotionOpcode,
    CommandCameraMotionOpcode,
    CommandCameraMotionOpcode,
    CommandCameraMotionOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    CommandStopCameraMotionOpcode,
    CommandSetCameraFollowingOpcode,
    // 0xE0
    ScriptYieldFrameOpcode,
    CommandSetScreenCaptureOpcode,
    CommandTweenCameraRollOpcode,
    CommandTweenCameraRollOpcode,
    CommandScreenTransformOpcode,
    CommandScreenTransformOpcode,
    CommandScreenTransformOpcode,
    CommandScreenTransformOpcode,
    CommandSetCameraOriginActorOpcode,
    CommandRestoreFieldCameraOpcode,
    CommandCameraLensOrShakeOpcode,
    CommandCameraLensOrShakeOpcode,
    CommandCameraLensOrShakeOpcode,
    CommandScreenTransformOpcode,
    CommandCameraLensOrShakeOpcode,
    CommandCameraLensOrShakeOpcode,
    // 0xF0
    ScriptYieldFrameOpcode,
    CommandSetTickSpeedOpcode,
    CommandPlayMenuSelectSfxOpcode,
    CommandSetScanlineOverlayOpcode,
    CommandWaitScriptContextOpcode,
    CommandStartScriptContextOpcode,
    CommandStopScriptContextOpcode,
    CommandLoadNextEventOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    ScriptReportInvalidOpcodeOpcode,
    CommandFinishEventOpcode,
    CommandFinishEventOpcode,
};
}  // namespace VS::Event
