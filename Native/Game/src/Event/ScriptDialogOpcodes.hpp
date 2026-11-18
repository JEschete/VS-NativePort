#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event VM opcode slots of the typed dialog commands
// (Event/ScriptDialogCommands.hpp). The opcode tables and the BATTLE.PRG
// module image hold these adapters; each hands its instruction, sized by the
// opcode's length, to the typed command with the live dialog state
// (ScriptDialogCommandsLive.cpp).

#include <cstdint>

namespace VS::Event {
std::intptr_t ScriptInitDialogBoxOpcode(std::uint8_t* command,
                                        std::int16_t thread_id);
std::intptr_t ScriptShowDialogTextOpcode(std::uint8_t* command,
                                         std::int16_t thread_id);
std::intptr_t ScriptDismissDialogBoxOpcode(std::uint8_t* command,
                                           std::int16_t thread_id);
std::intptr_t ScriptInitActorDialogBoxOpcode(std::uint8_t* command,
                                             std::int16_t thread_id);
std::intptr_t ScriptWaitForDialogDismissalOpcode(std::uint8_t* command,
                                                 std::int16_t thread_id);
std::intptr_t ScriptSetDialogIntegerOpcode(std::uint8_t* command,
                                           std::int16_t thread_id);
std::intptr_t ScriptSetDialogTransitionDurationOpcode(std::uint8_t* command,
                                                      std::int16_t thread_id);
std::intptr_t ScriptSelectDialogChoiceOpcode(std::uint8_t* command,
                                             std::int16_t thread_id);
std::intptr_t ScriptSetDialogSpeakerOpcode(std::uint8_t* command,
                                           std::int16_t thread_id);
}  // namespace VS::Event
