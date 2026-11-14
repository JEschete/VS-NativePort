// Hand-maintained (tools/port/owned.csv).
#include "Effects/EffectScript.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace VS::Effects {
namespace {
constexpr std::int32_t kYieldResult = 0;
constexpr std::int32_t kContinueResult = 1;
constexpr std::int32_t kEndResult = 2;
constexpr unsigned kHighByteShift = 8;
constexpr std::uint16_t kOperandBytes = 2;
}  // namespace

// decomp: func_800D5170 (src/BATTLE/BATTLE.PRG/5BF94.c:7804)
std::optional<std::uint8_t> ReadScriptByte(ScriptCursor& cursor) {
  const std::size_t at = cursor.offset;
  if (at >= cursor.program.size()) return std::nullopt;
  ++cursor.offset;
  return std::to_integer<std::uint8_t>(cursor.program[at]);
}

// decomp: func_800D5198 (src/BATTLE/BATTLE.PRG/5BF94.c:7806)
std::optional<std::uint16_t> ReadScriptHalfword(ScriptCursor& cursor) {
  ScriptCursor read = cursor;
  const auto low = ReadScriptByte(read);
  if (!low) return std::nullopt;
  const auto high = ReadScriptByte(read);
  if (!high) return std::nullopt;
  cursor.offset = read.offset;
  return static_cast<std::uint16_t>(*low | (*high << kHighByteShift));
}

// decomp: func_800D46DC (src/BATTLE/BATTLE.PRG/5BF94.c:7506)
bool BranchScriptIf(bool condition, ScriptCursor& cursor) {
  if (!condition) {
    cursor.offset = static_cast<std::uint16_t>(cursor.offset + kOperandBytes);
    return true;
  }
  const auto target = ReadScriptHalfword(cursor);
  if (!target) return false;
  cursor.offset = *target;
  return true;
}

// decomp: func_800D4720 (src/BATTLE/BATTLE.PRG/5BF94.c:7515)
ScriptStep YieldScriptAt(ScriptCursor& cursor) {
  const auto target = ReadScriptHalfword(cursor);
  if (!target) return ScriptStep::kEnd;
  cursor.offset = *target;
  return ScriptStep::kYield;
}

// decomp: func_800D474C (src/BATTLE/BATTLE.PRG/5BF94.c:7521)
ScriptStep JumpScript(ScriptCursor& cursor) {
  const auto target = ReadScriptHalfword(cursor);
  if (!target) return ScriptStep::kEnd;
  cursor.offset = *target;
  return ScriptStep::kContinue;
}

// decomp: func_800D47C0 (src/BATTLE/BATTLE.PRG/5BF94.c:7538)
ScriptStep EndScript() { return ScriptStep::kEnd; }

// decomp: func_800D4DA8 (src/BATTLE/BATTLE.PRG/5BF94.c:7711)
ScriptStep ContinueScript() { return ScriptStep::kContinue; }

ScriptStep StepFromHandlerResult(std::int32_t result) {
  switch (result) {
    case kContinueResult:
      return ScriptStep::kContinue;
    case kEndResult:
      return ScriptStep::kEnd;
    default:
      return ScriptStep::kYield;
  }
}

std::int32_t HandlerResult(ScriptStep step) {
  switch (step) {
    case ScriptStep::kContinue:
      return kContinueResult;
    case ScriptStep::kEnd:
      return kEndResult;
    case ScriptStep::kYield:
      break;
  }
  return kYieldResult;
}

// decomp: func_800D51D8 (src/BATTLE/BATTLE.PRG/5BF94.c:7813)
ScriptRun RunScript(ScriptMachine& machine) {
  for (;;) {
    const auto opcode = machine.NextOpcode();
    if (!opcode) return ScriptRun::kEnded;
    const auto step = machine.Execute(*opcode);
    if (!step || *step == ScriptStep::kEnd) return ScriptRun::kEnded;
    if (*step != ScriptStep::kContinue) return ScriptRun::kYielded;
  }
}
}  // namespace VS::Effects
