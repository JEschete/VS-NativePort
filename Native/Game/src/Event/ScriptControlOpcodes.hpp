#pragma once

#include <cstdint>

namespace VS::Event {
std::intptr_t ScriptNoOperationOpcode(std::uint8_t* instruction,
                                      std::int16_t thread_index);
std::intptr_t ScriptYieldFrameOpcode(std::uint8_t* instruction,
                                     std::int16_t thread_index);
std::intptr_t CommandWaitFramesOpcode(std::uint8_t* instruction,
                                      std::int16_t thread_index);
std::intptr_t CommandSetTickSpeedOpcode(std::uint8_t* instruction,
                                        std::int16_t thread_index);
std::intptr_t CommandWaitScriptContextOpcode(std::uint8_t* instruction,
                                             std::int16_t thread_index);
std::intptr_t CommandStartScriptContextOpcode(std::uint8_t* instruction,
                                              std::int16_t thread_index);
std::intptr_t CommandStopScriptContextOpcode(std::uint8_t* instruction,
                                             std::int16_t thread_index);
std::intptr_t ScriptCacheFlagValueOpcode(std::uint8_t* instruction,
                                         std::int16_t thread_index);
std::intptr_t ScriptBranchOnCachedFlagOpcode(std::uint8_t* instruction,
                                             std::int16_t thread_index);
std::intptr_t ScriptSetLoopCounterOpcode(std::uint8_t* instruction,
                                         std::int16_t thread_index);
std::intptr_t ScriptLoopBackwardOpcode(std::uint8_t* instruction,
                                       std::int16_t thread_index);
std::intptr_t ScriptApplyImmediateFlagOperationOpcode(
    std::uint8_t* instruction, std::int16_t thread_index);
std::intptr_t ScriptApplyFlagPairOperationOpcode(std::uint8_t* instruction,
                                                 std::int16_t thread_index);
std::intptr_t ScriptBranchOnFlagImmediateOpcode(std::uint8_t* instruction,
                                                std::int16_t thread_index);
std::intptr_t ScriptBranchOnFlagPairOpcode(std::uint8_t* instruction,
                                           std::int16_t thread_index);
std::intptr_t ScriptJumpForwardOpcode(std::uint8_t* instruction,
                                      std::int16_t thread_index);
std::intptr_t ScriptReturnToSavedAddressOpcode(std::uint8_t* instruction,
                                               std::int16_t thread_index);
std::intptr_t ScriptStoreInventoryItemPresenceOpcode(std::uint8_t* instruction,
                                                     std::int16_t thread_index);
std::intptr_t ScriptStoreButtonStateOpcode(std::uint8_t* instruction,
                                           std::int16_t thread_index);
std::intptr_t ScriptBeginSkippableCutsceneOpcode(std::uint8_t* instruction,
                                                 std::int16_t thread_index);
std::intptr_t ScriptEndCutsceneAndJumpOpcode(std::uint8_t* instruction,
                                             std::int16_t thread_index);
std::intptr_t ScriptEndCutsceneOpcode(std::uint8_t* instruction,
                                      std::int16_t thread_index);
std::intptr_t ScriptWaitForButtonAndYieldOpcode(std::uint8_t* instruction,
                                                std::int16_t thread_index);
std::intptr_t ScriptWaitForButtonOrTimeoutOpcode(std::uint8_t* instruction,
                                                 std::int16_t thread_index);
std::intptr_t ScriptConfigureTimerOpcode(std::uint8_t* instruction,
                                         std::int16_t thread_index);
std::intptr_t ScriptConfigureEasingCurveOpcode(std::uint8_t* instruction,
                                               std::int16_t thread_index);
std::intptr_t ScriptReportInvalidOpcodeOpcode(std::uint8_t* instruction,
                                              std::int16_t thread_index);
std::intptr_t ScriptWaitForEventFileOpcode(std::uint8_t* instruction,
                                           std::int16_t thread_index);
std::intptr_t CommandLoadNextEventOpcode(std::uint8_t* instruction,
                                         std::int16_t thread_index);
std::intptr_t CommandFinishEventOpcode(std::uint8_t* instruction,
                                       std::int16_t thread_index);
std::intptr_t CommandSpecialOperationOpcode(std::uint8_t* instruction,
                                            std::int16_t thread_index);
std::intptr_t ScriptSetGimmickVisibilityOpcode(std::uint8_t* instruction,
                                               std::int16_t thread_index);
std::intptr_t ScriptOverrideDepthCueDistanceOpcode(std::uint8_t* instruction,
                                                   std::int16_t thread_index);
std::intptr_t ScriptSetRoomLightingModeOpcode(std::uint8_t* instruction,
                                              std::int16_t thread_index);
std::intptr_t ScriptShowGeometryGroupOpcode(std::uint8_t* instruction,
                                            std::int16_t thread_index);
std::intptr_t ScriptHideGeometryGroupOpcode(std::uint8_t* instruction,
                                            std::int16_t thread_index);
std::intptr_t ScriptSwapGeometryGroupFlagsOpcode(std::uint8_t* instruction,
                                                 std::int16_t thread_index);
std::intptr_t ScriptSetCollisionTileModeOpcode(std::uint8_t* instruction,
                                               std::int16_t thread_index);
std::intptr_t ScriptAdjustGeometryDepthBiasOpcode(std::uint8_t* instruction,
                                                  std::int16_t thread_index);
std::intptr_t ScriptSetRoomBackgroundVisibleOpcode(std::uint8_t* instruction,
                                                   std::int16_t thread_index);
std::intptr_t ScriptLoadRoomBackgroundOpcode(std::uint8_t* instruction,
                                             std::int16_t thread_index);
std::intptr_t ScriptWaitForRoomBackgroundOpcode(std::uint8_t* instruction,
                                                std::int16_t thread_index);
std::intptr_t ScriptFreeRoomBackgroundOpcode(std::uint8_t* instruction,
                                             std::int16_t thread_index);
std::intptr_t ScriptSetBackgroundOffsetOpcode(std::uint8_t* instruction,
                                              std::int16_t thread_index);
std::intptr_t ScriptSetFirstPersonViewOpcode(std::uint8_t* instruction,
                                             std::int16_t thread_index);
std::intptr_t ScriptSetBackgroundParallaxOpcode(std::uint8_t* instruction,
                                                std::int16_t thread_index);
std::intptr_t CommandSetCameraOriginActorOpcode(std::uint8_t* instruction,
                                                std::int16_t thread_index);
std::intptr_t CommandCameraLensOrShakeOpcode(std::uint8_t* instruction,
                                             std::int16_t thread_index);
std::intptr_t CommandSetScreenCaptureOpcode(std::uint8_t* instruction,
                                            std::int16_t thread_index);
std::intptr_t CommandTweenCameraRollOpcode(std::uint8_t* instruction,
                                           std::int16_t thread_index);
std::intptr_t CommandScreenTransformOpcode(std::uint8_t* instruction,
                                           std::int16_t thread_index);
std::intptr_t CommandWaitCameraTracksOpcode(std::uint8_t* instruction,
                                            std::int16_t thread_index);
std::intptr_t ScriptFadeOutAndWaitOpcode(std::uint8_t* instruction,
                                         std::int16_t thread_index);
std::intptr_t ScriptFadeInOpcode(std::uint8_t* instruction,
                                 std::int16_t thread_index);
std::intptr_t ScriptSetWipeAngleOpcode(std::uint8_t* instruction,
                                       std::int16_t thread_index);
std::intptr_t ScriptConfigureScreenTransitionOpcode(std::uint8_t* instruction,
                                                    std::int16_t thread_index);
std::intptr_t ScriptSetRoomDisplayFlagsOpcode(std::uint8_t* instruction,
                                              std::int16_t thread_index);
std::intptr_t ScriptWaitForScreenTransitionOpcode(std::uint8_t* instruction,
                                                  std::int16_t thread_index);
std::intptr_t ScriptTogglePaletteHookOpcode(std::uint8_t* instruction,
                                            std::int16_t thread_index);
std::intptr_t ScriptTransformPalettesOpcode(std::uint8_t* instruction,
                                            std::int16_t thread_index);
std::intptr_t ScriptWaitForSplashFileOpcode(std::uint8_t* instruction,
                                            std::int16_t thread_index);
std::intptr_t ScriptLoadSplashImageOpcode(std::uint8_t* instruction,
                                          std::int16_t thread_index);
std::intptr_t ScriptConfigureSplashLayerOpcode(std::uint8_t* instruction,
                                               std::int16_t thread_index);
std::intptr_t ScriptTweenSplashLayerOpcode(std::uint8_t* instruction,
                                           std::int16_t thread_index);
std::intptr_t ScriptFreeSplashImageOpcode(std::uint8_t* instruction,
                                          std::int16_t thread_index);
std::intptr_t ScriptWaitForSplashTweenOpcode(std::uint8_t* instruction,
                                             std::int16_t thread_index);
std::intptr_t ScriptSetEnvironmentParticlesOpcode(std::uint8_t* instruction,
                                                  std::int16_t thread_index);
std::intptr_t ScriptSetParticleVariationEnabledOpcode(
    std::uint8_t* instruction, std::int16_t thread_index);
std::intptr_t ScriptMuteEnvironmentSoundOpcode(std::uint8_t* instruction,
                                               std::int16_t thread_index);
std::intptr_t ScriptControlRoomAnimationOpcode(std::uint8_t* instruction,
                                               std::int16_t thread_index);
std::intptr_t CommandCameraMotionOpcode(std::uint8_t* instruction,
                                        std::int16_t thread_index);
std::intptr_t CommandStopCameraMotionOpcode(std::uint8_t* instruction,
                                            std::int16_t thread_index);
std::intptr_t CommandSetCameraFollowingOpcode(std::uint8_t* instruction,
                                              std::int16_t thread_index);
std::intptr_t CommandRestoreFieldCameraOpcode(std::uint8_t* instruction,
                                              std::int16_t thread_index);
std::intptr_t ScriptClearRoomActorCacheOpcode(std::uint8_t* instruction,
                                              std::int16_t thread_index);
std::intptr_t ScriptSetHudVisibleOpcode(std::uint8_t* instruction,
                                        std::int16_t thread_index);
std::intptr_t ScriptRunMenuTransitionOpcode(std::uint8_t* instruction,
                                            std::int16_t thread_index);
std::intptr_t ScriptChangeRoomOpcode(std::uint8_t* instruction,
                                     std::int16_t thread_index);
}  // namespace VS::Event
