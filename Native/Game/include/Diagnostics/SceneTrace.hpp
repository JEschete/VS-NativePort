#pragma once

#include <cstdint>

namespace VS::Game {

// Diagnostic calls only: no VM, flag, skip or completion state is changed.
std::uint64_t RecordSceneCommand(std::int32_t event_id, std::int32_t source,
                                 std::int32_t thread, const void* instruction,
                                 const void* script_base, std::uint32_t size,
                                 std::int32_t skip_state);
void RecordSceneResult(std::uint64_t command, std::int64_t result);
void RecordScriptFlagWrite(std::int32_t flag, std::uint8_t before,
                           std::uint8_t after);

}  // namespace VS::Game
