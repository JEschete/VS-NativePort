#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
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
}  // namespace VS::Effects
