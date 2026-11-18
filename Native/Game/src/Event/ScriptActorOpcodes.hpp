#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event VM opcode slots of the typed actor commands
// (Event/ScriptActorCommands.hpp). The opcode tables and the BATTLE.PRG
// module image hold these adapters; each hands its instruction, sized by the
// opcode's length, to the typed command with the live actor, script, item and
// battle state (ScriptActorCommandsLive.cpp).

#include <cstdint>

namespace VS::Event {
std::intptr_t ScriptResetActorGroupStateOpcode(std::uint8_t* command,
                                               std::int16_t thread_id);
std::intptr_t ScriptStoreActorTileOpcode(std::uint8_t* command,
                                         std::int16_t thread_id);
std::intptr_t ScriptSpawnActorOpcode(std::uint8_t* command,
                                     std::int16_t thread_id);
std::intptr_t ScriptNavigateActorToTileOpcode(std::uint8_t* command,
                                              std::int16_t thread_id);
std::intptr_t ScriptPlayActorAnimationOpcode(std::uint8_t* command,
                                             std::int16_t thread_id);
std::intptr_t ScriptSetActorGroupFlagsOpcode(std::uint8_t* command,
                                             std::int16_t thread_id);
std::intptr_t ScriptSetActorSoundMixOpcode(std::uint8_t* command,
                                           std::int16_t thread_id);
std::intptr_t ScriptWalkActorToTileOpcode(std::uint8_t* command,
                                          std::int16_t thread_id);
std::intptr_t ScriptWalkActorToWorldPosOpcode(std::uint8_t* command,
                                              std::int16_t thread_id);
std::intptr_t ScriptSlideActorToTileOpcode(std::uint8_t* command,
                                           std::int16_t thread_id);
std::intptr_t ScriptSlideActorToWorldPosOpcode(std::uint8_t* command,
                                               std::int16_t thread_id);
std::intptr_t ScriptSetActorAttachmentOpcode(std::uint8_t* command,
                                             std::int16_t thread_id);
std::intptr_t ScriptNavigateActorToWorldPosOpcode(std::uint8_t* command,
                                                  std::int16_t thread_id);
std::intptr_t ScriptTurnActorToFacingOpcode(std::uint8_t* command,
                                            std::int16_t thread_id);
std::intptr_t ScriptFaceOtherActorOpcode(std::uint8_t* command,
                                         std::int16_t thread_id);
std::intptr_t ScriptFaceTileOpcode(std::uint8_t* command,
                                   std::int16_t thread_id);
std::intptr_t ScriptScaleActorOpcode(std::uint8_t* command,
                                     std::int16_t thread_id);
std::intptr_t ScriptRemoveActorGroupOpcode(std::uint8_t* command,
                                           std::int16_t thread_id);
std::intptr_t ScriptWaitForActorFlagsOpcode(std::uint8_t* command,
                                            std::int16_t thread_id);
std::intptr_t ScriptSetActorColorOverrideOpcode(std::uint8_t* command,
                                                std::int16_t thread_id);
std::intptr_t ScriptQueueActorAnimationOpcode(std::uint8_t* command,
                                              std::int16_t thread_id);
std::intptr_t ScriptRotateActorAxesOpcode(std::uint8_t* command,
                                          std::int16_t thread_id);
std::intptr_t ScriptSetActorHeadAnglesOpcode(std::uint8_t* command,
                                             std::int16_t thread_id);
std::intptr_t ScriptPlayActorAnimationAtFrameOpcode(std::uint8_t* command,
                                                    std::int16_t thread_id);
std::intptr_t ScriptQueueActorAnimationAtFrameOpcode(std::uint8_t* command,
                                                     std::int16_t thread_id);
std::intptr_t ScriptSetActorIdleAnimationOpcode(std::uint8_t* command,
                                                std::int16_t thread_id);
std::intptr_t ScriptLookAtActorOpcode(std::uint8_t* command,
                                      std::int16_t thread_id);
std::intptr_t ScriptLookAtWorldPosOpcode(std::uint8_t* command,
                                         std::int16_t thread_id);
std::intptr_t ScriptLoadObjectModelOpcode(std::uint8_t* command,
                                          std::int16_t thread_id);
std::intptr_t ScriptWaitForObjectLoadOpcode(std::uint8_t* command,
                                            std::int16_t thread_id);
std::intptr_t ScriptReleaseObjectModelOpcode(std::uint8_t* command,
                                             std::int16_t thread_id);
std::intptr_t ScriptSelectObjectTextureBankOpcode(std::uint8_t* command,
                                                  std::int16_t thread_id);
std::intptr_t ScriptSetActorOverlayPassOpcode(std::uint8_t* command,
                                              std::int16_t thread_id);
std::intptr_t ScriptSetActorWeaponDrawnOpcode(std::uint8_t* command,
                                              std::int16_t thread_id);
std::intptr_t ScriptPlayActorAnimationSoundOpcode(std::uint8_t* command,
                                                  std::int16_t thread_id);
std::intptr_t ScriptSetNpcUpdatesEnabledOpcode(std::uint8_t* command,
                                               std::int16_t thread_id);
std::intptr_t ScriptRestoreActorVitalsOpcode(std::uint8_t* command,
                                             std::int16_t thread_id);
std::intptr_t ScriptSetActorEquipmentModelsOpcode(std::uint8_t* command,
                                                  std::int16_t thread_id);
std::intptr_t ScriptSetActorYawOscillationOpcode(std::uint8_t* command,
                                                 std::int16_t thread_id);
std::intptr_t ScriptClearAshleyStatusEffectsOpcode(std::uint8_t* command,
                                                   std::int16_t thread_id);
std::intptr_t ScriptSetActorRenderModeOpcode(std::uint8_t* command,
                                             std::int16_t thread_id);
std::intptr_t CommandSetEnteredDoorOpcode(std::uint8_t* command,
                                          std::int16_t thread_id);
std::intptr_t CommandStoreActorRangeBearingOpcode(std::uint8_t* command,
                                                  std::int16_t thread_id);
std::intptr_t CommandSetActorActivityOpcode(std::uint8_t* command,
                                            std::int16_t thread_id);
std::intptr_t CommandAwardMiscLootOpcode(std::uint8_t* command,
                                         std::int16_t thread_id);
std::intptr_t CommandConsumeMiscItemOpcode(std::uint8_t* command,
                                           std::int16_t thread_id);
std::intptr_t CommandStoreActorDistanceOpcode(std::uint8_t* command,
                                              std::int16_t thread_id);
}  // namespace VS::Event
