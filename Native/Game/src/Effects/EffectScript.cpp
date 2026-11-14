// Hand-maintained (tools/port/owned.csv).
#include "Effects/EffectScript.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>

namespace VS::Effects {
namespace {
constexpr std::int32_t kYieldResult = 0;
constexpr std::int32_t kContinueResult = 1;
constexpr std::int32_t kEndResult = 2;
constexpr unsigned kHighByteShift = 8;
constexpr std::uint16_t kOperandBytes = 2;

// The step a branch opcode takes: continue, or the operand refusal.
[[nodiscard]] OpcodeStep Branch(bool condition, ScriptCursor& cursor) {
  if (!BranchScriptIf(condition, cursor)) {
    return std::unexpected(ScriptRefusal::kOperand);
  }
  return ScriptStep::kContinue;
}

struct RegisterOperands {
  std::size_t index = 0;
  std::uint16_t value = 0;
};

// The register index byte and the halfword after it, both read before the
// index is checked, as the original read them.
[[nodiscard]] std::expected<RegisterOperands, ScriptRefusal>
ReadRegisterOperands(ScriptCursor& cursor) {
  const auto index = ReadScriptByte(cursor);
  if (!index) return std::unexpected(ScriptRefusal::kOperand);
  const auto value = ReadScriptHalfword(cursor);
  if (!value) return std::unexpected(ScriptRefusal::kOperand);
  if (*index >= kScriptRegisters) {
    return std::unexpected(ScriptRefusal::kRegister);
  }
  return RegisterOperands{.index = *index, .value = *value};
}
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

// decomp: func_800D47C8 (src/BATTLE/BATTLE.PRG/5BF94.c:7540)
OpcodeStep SetScriptSelector(ScriptCursor& cursor, std::int32_t& selector) {
  const auto value = ReadScriptByte(cursor);
  if (!value) return std::unexpected(ScriptRefusal::kOperand);
  selector = *value;
  return ScriptStep::kContinue;
}

// decomp: func_800D487C (src/BATTLE/BATTLE.PRG/5BF94.c:7563)
OpcodeStep BranchIfRenderersFinished(bool renderers_attached,
                                     ScriptCursor& cursor) {
  return Branch(!renderers_attached, cursor);
}

// decomp: func_800D48E4 (src/BATTLE/BATTLE.PRG/5BF94.c:7576)
OpcodeStep BranchIfActionVariantCleared(std::uint8_t variant,
                                        ScriptCursor& cursor) {
  return Branch(variant == 0, cursor);
}

// decomp: func_800D4AB4 (src/BATTLE/BATTLE.PRG/5BF94.c:7625)
ScriptStep LoadActionVariantRegister(std::uint8_t variant,
                                     const ActionVariantValues& values,
                                     ScriptRegisters& registers) {
  if (variant < values.size()) {
    std::get<kActionVariantRegister>(registers) = values.at(variant);
  }
  return ScriptStep::kContinue;
}

// decomp: func_800D4AEC (src/BATTLE/BATTLE.PRG/5BF94.c:7633)
OpcodeStep SetScriptRegister(ScriptCursor& cursor, ScriptRegisters& registers) {
  return ReadRegisterOperands(cursor).transform(
      [&registers](const RegisterOperands& operands) {
        registers.at(operands.index) =
            static_cast<std::uint8_t>(operands.value);
        return ScriptStep::kContinue;
      });
}

// decomp: func_800D4B34 (src/BATTLE/BATTLE.PRG/5BF94.c:7639)
OpcodeStep BranchIfScriptRegisterEqual(ScriptCursor& cursor,
                                       const ScriptRegisters& registers) {
  return ReadRegisterOperands(cursor).and_then(
      [&registers, &cursor](const RegisterOperands& operands) {
        return Branch(registers.at(operands.index) == operands.value, cursor);
      });
}

// decomp: func_800D4B90 (src/BATTLE/BATTLE.PRG/5BF94.c:7645)
OpcodeStep BranchIfParticleCountEqual(std::uint8_t particle_count,
                                      ScriptCursor& cursor) {
  const auto count = ReadScriptByte(cursor);
  if (!count) return std::unexpected(ScriptRefusal::kOperand);
  return Branch(*count == particle_count, cursor);
}

// decomp: func_800D4D98 (src/BATTLE/BATTLE.PRG/5BF94.c:7705)
ScriptStep ClearActionVariant(std::uint8_t& variant) {
  variant = 0;
  return ScriptStep::kContinue;
}
}  // namespace VS::Effects
