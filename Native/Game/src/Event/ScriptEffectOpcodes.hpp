#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event VM opcode slots of the typed effect commands
// (Event/ScriptEffectCommands.hpp). The opcode tables and the BATTLE.PRG
// module image hold these adapters; each hands its instruction, sized by the
// opcode's length, to the typed command with the live effect state
// (ScriptEffectCommandsLive.cpp).

#include <cstdint>

namespace VS::Event {
std::intptr_t CommandPlayActorEffectOpcode(std::uint8_t* command,
                                           std::int16_t thread_id);
std::intptr_t CommandAdvanceEffectStageOpcode(std::uint8_t* command,
                                              std::int16_t thread_id);
std::intptr_t CommandWaitEffectStateOpcode(std::uint8_t* command,
                                           std::int16_t thread_id);
std::intptr_t CommandYieldEffectReleaseOpcode(std::uint8_t* command,
                                              std::int16_t thread_id);
std::intptr_t CommandWaitEffectContinuationOpcode(std::uint8_t* command,
                                                  std::int16_t thread_id);
std::intptr_t CommandWaitEffectReleaseOpcode(std::uint8_t* command,
                                             std::int16_t thread_id);
std::intptr_t CommandMarkEffectStageReadyOpcode(std::uint8_t* command,
                                                std::int16_t thread_id);
std::intptr_t CommandPlayVibrationPatternOpcode(std::uint8_t* command,
                                                std::int16_t thread_id);
std::intptr_t CommandSetActorTrailOpcode(std::uint8_t* command,
                                         std::int16_t thread_id);
std::intptr_t CommandWaitScreenEffectLoadOpcode(std::uint8_t* command,
                                                std::int16_t thread_id);
std::intptr_t CommandSetScreenDistortionOpcode(std::uint8_t* command,
                                               std::int16_t thread_id);
std::intptr_t CommandLoadScreenEffectsOpcode(std::uint8_t* command,
                                             std::int16_t thread_id);
std::intptr_t CommandReservedScreenEffectOpcode(std::uint8_t* command,
                                                std::int16_t thread_id);
std::intptr_t CommandFreeScreenEffectsOpcode(std::uint8_t* command,
                                             std::int16_t thread_id);
std::intptr_t CommandSetScanlineOverlayOpcode(std::uint8_t* command,
                                              std::int16_t thread_id);
}  // namespace VS::Event
