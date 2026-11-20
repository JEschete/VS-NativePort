#pragma once
// The event script VM's module state: the opcode tables and the scalar state
// the live VM host (ScriptVmLive.cpp) shares with the opcode bindings, the
// field and the session. ScriptVmLiveState.cpp defines them. Script sources,
// thread cursors, the subroutine return, dialog strings, vibration patterns
// and easing curves are owned by the native VM host instead.

#include <cstddef>
#include <cstdint>

#include "Event/ScriptDialogCommands.hpp"
#include "Event/ScriptVm.hpp"

namespace VS::Event {
inline constexpr std::size_t kScriptOpcodeCount = 256;
// An opcode handler: its instruction and the running thread.
using ScriptOpcodeHandler = std::intptr_t (*)(std::uint8_t*, std::int16_t);

// Each opcode's instruction length in bytes.
// decomp: D_800E9C30 (D_800E9C30@BATTLE/BATTLE.PRG)
extern std::uint8_t opcodeSizes[kScriptOpcodeCount];
// The handler each opcode runs during playback.
// decomp: _opcodeFunctionTable (_opcodeFunctionTable@BATTLE/BATTLE.PRG)
extern ScriptOpcodeHandler opcodeHandlers[kScriptOpcodeCount];
// The handler each opcode runs while a cutscene is skipped.
// decomp: D_800FAF7C (D_800FAF7C@src/BATTLE/INITBTL.PRG/12AC.c)
extern ScriptOpcodeHandler skipOpcodeHandlers[kScriptOpcodeCount];
// The text box frame style images.
// decomp: D_800EB588 (D_800EB588@BATTLE/BATTLE.PRG)
extern std::uint8_t* textBoxFrameStyles[kTextBoxFrameStyleCount];
// The flag value cached for the next conditional branch.
// decomp: D_800F4BB4 (D_800F4BB4@BATTLE/BATTLE.PRG)
extern std::int8_t cachedFlagValue;
// Nonzero while the script, not the field, restores the field camera.
// decomp: D_800F4BB6 (D_800F4BB6@BATTLE/BATTLE.PRG)
extern std::int16_t isFieldCameraRestoreScripted;
// Nonzero once the event's start is confirmed.
// decomp: D_800F4BB8 (D_800F4BB8@BATTLE/BATTLE.PRG)
extern std::uint8_t isStartConfirmed;
// Where the running event came from (EventSource).
// decomp: D_800F4BE0 (D_800F4BE0@BATTLE/BATTLE.PRG)
extern std::uint16_t eventSource;
// The scripted room change's progress, shared with LoadRoomEventSections.
// decomp: D_800F4BE2 (D_800F4BE2@BATTLE/BATTLE.PRG)
extern std::int16_t roomChangeState;
// Nonzero while the scanline overlay draws.
// decomp: D_800F4BE4 (D_800F4BE4@BATTLE/BATTLE.PRG)
extern std::uint8_t isScanlineOverlayEnabled;
// Nonzero while a scripted menu transition runs.
// decomp: D_800F4BF8 (D_800F4BF8@BATTLE/BATTLE.PRG)
extern std::uint8_t isMenuTransitionActive;
// Per text box, nonzero while the script waits for the box to close.
// decomp: vs_battle_textBoxStatuses
// (vs_battle_textBoxStatuses@BATTLE/BATTLE.PRG)
extern std::uint8_t isTextBoxAwaitingClose[kTextBoxCount];
// The script loop counters.
// decomp: D_800F4C10 (D_800F4C10@BATTLE/BATTLE.PRG)
extern std::uint8_t loopCounters[kLoopCounterCount];
// Nonzero when skipping the cutscene stops its music.
// decomp: D_800F4C20 (D_800F4C20@BATTLE/BATTLE.PRG)
extern std::uint8_t stopsMusicOnSkip;
// Nonzero once the event's main thread has finished.
// decomp: D_800F4C22 (D_800F4C22@BATTLE/BATTLE.PRG)
extern std::int16_t isMainThreadFinished;
// The event playback phase (PlaybackPhase).
// decomp: D_800F4C34 (D_800F4C34@BATTLE/BATTLE.PRG)
extern std::uint8_t playbackPhase;
// Nonzero while a scripted weapon mode change is pending.
// decomp: D_800F4C4E (D_800F4C4E@BATTLE/BATTLE.PRG)
extern std::uint8_t weaponTransitionPending;
// The running event's id.
// decomp: _evtFile (_evtFile@BATTLE/BATTLE.PRG)
extern std::int16_t eventId;
// Nonzero once the chained event's file is loaded.
// decomp: D_800F4C54 (D_800F4C54@BATTLE/BATTLE.PRG)
extern std::int16_t isChainedEventLoaded;
// Per thread, the frames left before the thread resumes.
// decomp: D_800F4C58 (D_800F4C58@BATTLE/BATTLE.PRG)
extern std::int16_t threadWaitFrames[kScriptThreadCount];
// Nonzero while a text box selection is pending.
// decomp: _textBoxSelectionPending (_textBoxSelectionPending@BATTLE/BATTLE.PRG)
extern std::uint8_t isTextSelectionActive;
// Nonzero when the cutscene skip is forced.
// decomp: D_800F4C68 (D_800F4C68@BATTLE/BATTLE.PRG)
extern std::uint8_t isSkipForced;
// 0xFF while the script keeps the texture LOD depth (opcode 0x61), else 0.
// decomp: D_800F4C6A (D_800F4C6A@BATTLE/BATTLE.PRG)
extern std::uint8_t isScriptBackdropEnabled;
}  // namespace VS::Event
