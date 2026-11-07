#pragma once

#include <expected>
#include <memory>
#include <optional>
#include <vector>

#include "Event/ScriptSources.hpp"
#include "Event/ScriptVm.hpp"

namespace VS::Event {

// Retain this lease for as long as a consumer uses its byte view. It owns the
// bound generation, not whichever source is currently mounted.
struct ScriptDataLease {
  ScriptSource source;
  ScriptLocation location;

  [[nodiscard]] std::optional<ScriptCommand> Bytes() const noexcept {
    return source.View(location);
  }
};

// Native cursor and source state lives outside resettable module images.
[[nodiscard]] ScriptVmHost& LiveScriptVm();
[[nodiscard]] int ResolveLiveScriptActorReference(std::uint16_t reference);
using ScriptControlOpcode = ScriptFlow (*)(const ScriptInstruction&,
                                           ScriptVmHost&);
// C-table adapters return a status only; typed jumps remain in the active
// invocation. The instruction must be the one the native dispatcher lent.
std::intptr_t DispatchLiveScriptControlOpcode(std::uint8_t* instruction,
                                              std::int16_t thread,
                                              ScriptControlOpcode opcode);
[[nodiscard]] std::expected<ScriptSource, ScriptSourceError>
LeaseLiveScriptSource(ScriptRegion region);
[[nodiscard]] std::expected<ScriptDataLease, ScriptSourceError>
LeaseLiveDialogStrings();
[[nodiscard]] std::expected<ScriptDataLease, ScriptSourceError>
LeaseLiveTextBoxFrame(int style);
[[nodiscard]] std::expected<ScriptDataLease, ScriptSourceError>
LeaseLiveVibrationPattern(int pattern);
[[nodiscard]] std::expected<void, ScriptSourceError> MountLiveScriptSource(
    ScriptRegion region,
    std::shared_ptr<const std::vector<std::uint8_t>> source);
[[nodiscard]] std::expected<PreparedScriptSource, ScriptSourceError>
PrepareLiveRoomEntrySource(RoomEventLease room);
[[nodiscard]] std::expected<void, ScriptSourceError> PublishLiveScriptSource(
    PreparedScriptSource&& prepared);
[[nodiscard]] RoomEventLease LiveRoomScriptSections();
void PublishLiveRoomScriptSections(RoomEventLease room);

[[nodiscard]] bool InitializeLiveEventScriptData(ScriptLocation source);
[[nodiscard]] bool InitializeLiveScriptOpcodeTable();
[[nodiscard]] int UpdateLiveEventPlayback();
[[nodiscard]] bool ExecuteLiveScriptThread(int thread);
void ReleaseLiveScriptActorControl();

// Reset preserves active borrowed bytes. Abandon only immediately before the
// non-returning session transfer, after UI borrows have been invalidated.
void ResetLiveScriptRuntime();
void AbandonLiveScriptUses();

}  // namespace VS::Event
