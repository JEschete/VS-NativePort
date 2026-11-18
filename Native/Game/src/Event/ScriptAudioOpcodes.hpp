#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event VM opcode slots of the typed sound commands
// (Event/ScriptAudioCommands.hpp). The opcode tables and the BATTLE.PRG
// module image hold these adapters; each hands its instruction, sized by the
// opcode's length, to the typed command with the live sound state
// (ScriptAudioCommandsLive.cpp).

#include <cstdint>

namespace VS::Event {
std::intptr_t ScriptPlaySoundCommandOpcode(std::uint8_t* command,
                                           std::int16_t thread_id);
std::intptr_t CommandPlayActorSfxOpcode(std::uint8_t* command,
                                        std::int16_t thread_id);
std::intptr_t CommandSlideSfxVolumeOpcode(std::uint8_t* command,
                                          std::int16_t thread_id);
std::intptr_t CommandStopAllSfxOpcode(std::uint8_t* command,
                                      std::int16_t thread_id);
std::intptr_t CommandStopSfxOpcode(std::uint8_t* command,
                                   std::int16_t thread_id);
std::intptr_t CommandLoadSfxSlotOpcode(std::uint8_t* command,
                                       std::int16_t thread_id);
std::intptr_t CommandWaitSfxSlotReleaseOpcode(std::uint8_t* command,
                                              std::int16_t thread_id);
std::intptr_t CommandFreeSfxOpcode(std::uint8_t* command,
                                   std::int16_t thread_id);
std::intptr_t CommandSelectSfxBankOpcode(std::uint8_t* command,
                                         std::int16_t thread_id);
std::intptr_t CommandLoadEventMusicOpcode(std::uint8_t* command,
                                          std::int16_t thread_id);
std::intptr_t CommandCrossfadeMusicOpcode(std::uint8_t* command,
                                          std::int16_t thread_id);
std::intptr_t CommandLoadMusicSlotOpcode(std::uint8_t* command,
                                         std::int16_t thread_id);
std::intptr_t CommandFreeMusicOpcode(std::uint8_t* command,
                                     std::int16_t thread_id);
std::intptr_t CommandPlayMusicOpcode(std::uint8_t* command,
                                     std::int16_t thread_id);
std::intptr_t CommandSlideMusicVolumeOpcode(std::uint8_t* command,
                                            std::int16_t thread_id);
std::intptr_t CommandSlideMusicTempoOpcode(std::uint8_t* command,
                                           std::int16_t thread_id);
std::intptr_t CommandSlideMusicPitchOpcode(std::uint8_t* command,
                                           std::int16_t thread_id);
std::intptr_t CommandStopMusicOpcode(std::uint8_t* command,
                                     std::int16_t thread_id);
std::intptr_t CommandWaitMusicSlotOpcode(std::uint8_t* command,
                                         std::int16_t thread_id);
std::intptr_t CommandPushMusicStateOpcode(std::uint8_t* command,
                                          std::int16_t thread_id);
std::intptr_t CommandWaitMusicSlotLoadOpcode(std::uint8_t* command,
                                             std::int16_t thread_id);
std::intptr_t CommandLoadSoundFileOpcode(std::uint8_t* command,
                                         std::int16_t thread_id);
std::intptr_t CommandWaitSoundQueueOpcode(std::uint8_t* command,
                                          std::int16_t thread_id);
std::intptr_t CommandWaitMusicTimerOpcode(std::uint8_t* command,
                                          std::int16_t thread_id);
std::intptr_t CommandSlideSfxPitchOpcode(std::uint8_t* command,
                                         std::int16_t thread_id);
std::intptr_t CommandSlideSfxPanOpcode(std::uint8_t* command,
                                       std::int16_t thread_id);
std::intptr_t CommandResumeMusicSlotOpcode(std::uint8_t* command,
                                           std::int16_t thread_id);
std::intptr_t CommandSetMusicJumpThresholdOpcode(std::uint8_t* command,
                                                 std::int16_t thread_id);
std::intptr_t CommandPlayMenuSelectSfxOpcode(std::uint8_t* command,
                                             std::int16_t thread_id);
}  // namespace VS::Event
