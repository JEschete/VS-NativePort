#pragma once

#include <cstdint>

#include "Event/ScriptVm.hpp"

namespace VS::Game {

// Diagnostic calls only: no VM, flag, skip or completion state is changed.
std::uint64_t RecordSceneCommand(std::int32_t event_id,
                                 Event::EventSource source,
                                 const Event::ScriptInstruction& instruction,
                                 std::uint32_t size,
                                 Event::CutsceneSkip skip_state);
void RecordSceneResult(std::uint64_t command, Event::ScriptFlow result);
void RecordScriptFlagWrite(std::int32_t flag, std::uint8_t before,
                           std::uint8_t after);

}  // namespace VS::Game
