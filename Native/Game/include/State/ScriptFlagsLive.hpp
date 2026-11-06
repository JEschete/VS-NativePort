#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "State/ScriptFlags.hpp"

// Bindings from the live flag record (Audio_Seq_unusedStateFlags, the
// original vs_main_stateFlags) to ScriptFlags, in the original argument
// order for the generated callers (tools/port/copy_fixups.py
// OWNED_CALL_BINDINGS). The scene trace records each write; an id outside
// the record is refused with a "state" warning.
namespace VS::State {
// The live record, with the scene trace observing its writes.
[[nodiscard]] ScriptFlags LiveScriptFlags() noexcept;
// The id narrows to the original's signed halfword and the value to a byte.
[[nodiscard]] std::uint8_t LiveScriptFlag(int id);
void SetLiveScriptFlag(int id, int value);
void ResetLiveZoneScriptFlags(int preserve);
}  // namespace VS::State
