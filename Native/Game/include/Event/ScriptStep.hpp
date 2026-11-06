#pragma once
// Hand-maintained (tools/port/owned.csv).
#include <cstdint>

namespace VS::Event {

// What the event VM does after an opcode: move to the next command, run the
// same command again next frame, or move on and end the thread's frame. These
// are the return codes of the dispatch loop, func_800BFBB8
// (src/BATTLE/BATTLE.PRG/573B8.c:67).
enum class ScriptStep : std::uint8_t {
  kAdvance = 0,
  kHold = 1,
  kAdvanceAndYield = 4,
};

}  // namespace VS::Event
