#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

// The effect script loop of BATTLE.PRG (5BF94.c:7506-7826). An effect
// instance runs its package's block-8 program from a 16-bit offset, one
// opcode byte at a time, through a 49-entry handler table; branch operands
// are little-endian halfword offsets. The original read through the bare
// program pointer. The cursor bounds every read by the program block, and a
// refused read ends the instance.
namespace VS::Effects {
inline constexpr std::size_t kEffectScriptOpcodes = 49;

// What an opcode handler tells the loop: kYield (the original's 0, and any
// value other than 1 or 2) stops for the frame, kContinue (1) runs the next
// opcode and kEnd (2) ends the instance.
enum class ScriptStep : std::uint8_t { kYield, kContinue, kEnd };

// How a frame's run left the instance: the original returned 0 for kEnded
// (the caller frees it) and 1 for kYielded.
enum class ScriptRun : std::uint8_t { kEnded, kYielded };

// An instance's program bytes and its offset into them.
struct ScriptCursor {
  std::span<const std::byte> program;
  std::uint16_t offset = 0;
};

// func_800D5170 (5BF94.c:7804): the byte at the offset, which then advances
// with 16-bit wrap. Refused, leaving the offset, past the program's end.
[[nodiscard]] std::optional<std::uint8_t> ReadScriptByte(ScriptCursor& cursor);

// func_800D5198 (5BF94.c:7806): two byte reads, low byte first. Refused,
// leaving the offset, when either read is.
[[nodiscard]] std::optional<std::uint16_t> ReadScriptHalfword(
    ScriptCursor& cursor);

// func_800D46DC (5BF94.c:7506): moves to the halfword operand when
// `condition` holds and past it otherwise. False when the operand it needed
// was refused.
[[nodiscard]] bool BranchScriptIf(bool condition, ScriptCursor& cursor);

// func_800D4720 (5BF94.c:7515): moves to the operand and yields the frame.
[[nodiscard]] ScriptStep YieldScriptAt(ScriptCursor& cursor);

// func_800D474C (5BF94.c:7521): moves to the operand and continues.
[[nodiscard]] ScriptStep JumpScript(ScriptCursor& cursor);

// func_800D47C0 (5BF94.c:7538): the end opcode.
[[nodiscard]] ScriptStep EndScript();

// func_800D4DA8 (5BF94.c:7711): the no-operation opcode.
[[nodiscard]] ScriptStep ContinueScript();

// The step a handler's integer result means, and the integer a step returns
// through the original handler table.
[[nodiscard]] ScriptStep StepFromHandlerResult(std::int32_t result);
[[nodiscard]] std::int32_t HandlerResult(ScriptStep step);

// The instance a frame's run reads and dispatches for.
class ScriptMachine {
 public:
  ScriptMachine() = default;
  ScriptMachine(const ScriptMachine&) = delete;
  ScriptMachine& operator=(const ScriptMachine&) = delete;
  ScriptMachine(ScriptMachine&&) = delete;
  ScriptMachine& operator=(ScriptMachine&&) = delete;
  virtual ~ScriptMachine() = default;

  // The next opcode byte; none when the read is refused.
  [[nodiscard]] virtual std::optional<std::uint8_t> NextOpcode() = 0;
  // Runs the opcode's handler; none when the table has no handler for it.
  [[nodiscard]] virtual std::optional<ScriptStep> Execute(
      std::uint8_t opcode) = 0;
};

// func_800D51D8 (5BF94.c:7813): runs opcodes until one yields or ends the
// instance. A refused opcode read or handler ends it.
[[nodiscard]] ScriptRun RunScript(ScriptMachine& machine);

// An instance's four script registers (instance + 0x10), which opcodes set,
// test and read operands from.
inline constexpr std::size_t kScriptRegisters = 4;
using ScriptRegisters = std::array<std::uint8_t, kScriptRegisters>;

// The register the action-variant opcode loads, and the values it loads for
// the four action variants (D_800EC32C).
inline constexpr std::size_t kActionVariantRegister = 3;
inline constexpr std::size_t kActionVariantValues = 4;
using ActionVariantValues = std::array<std::uint8_t, kActionVariantValues>;

// Why an operand opcode refused: an operand read past the program, or a
// register operand past the four registers. The original wrote and read the
// instance fields after the registers; no shipped program names one (the
// effect packages only name registers 0, 1 and 3).
enum class ScriptRefusal : std::uint8_t { kOperand, kRegister };

using OpcodeStep = std::expected<ScriptStep, ScriptRefusal>;

// func_800D47C8 (5BF94.c:7540): stores the operand byte in the selector.
[[nodiscard]] OpcodeStep SetScriptSelector(ScriptCursor& cursor,
                                           std::int32_t& selector);

// func_800D487C (5BF94.c:7563): branches once no renderer is attached.
[[nodiscard]] OpcodeStep BranchIfRenderersFinished(bool renderers_attached,
                                                   ScriptCursor& cursor);

// func_800D48E4 (5BF94.c:7576): branches when the action variant is 0.
[[nodiscard]] OpcodeStep BranchIfActionVariantCleared(std::uint8_t variant,
                                                      ScriptCursor& cursor);

// func_800D4AB4 (5BF94.c:7625): loads the variant's value into register 3;
// a variant past the four keeps the register.
[[nodiscard]] ScriptStep LoadActionVariantRegister(
    std::uint8_t variant, const ActionVariantValues& values,
    ScriptRegisters& registers);

// func_800D4AEC (5BF94.c:7633): reads a register index, then a halfword
// whose low byte the register keeps.
[[nodiscard]] OpcodeStep SetScriptRegister(ScriptCursor& cursor,
                                           ScriptRegisters& registers);

// func_800D4B34 (5BF94.c:7639): reads a register index and a halfword, then
// branches when the register equals the whole halfword, so a value past 0xFF
// never branches.
[[nodiscard]] OpcodeStep BranchIfScriptRegisterEqual(
    ScriptCursor& cursor, const ScriptRegisters& registers);

// func_800D4B90 (5BF94.c:7645): branches when the operand byte equals the
// instance's particle count.
[[nodiscard]] OpcodeStep BranchIfParticleCountEqual(std::uint8_t particle_count,
                                                    ScriptCursor& cursor);

// func_800D4D98 (5BF94.c:7705): clears the action variant.
[[nodiscard]] ScriptStep ClearActionVariant(std::uint8_t& variant);
}  // namespace VS::Effects
