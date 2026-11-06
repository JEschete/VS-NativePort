#pragma once
// Hand-maintained (tools/port/owned.csv).
// One event script instruction and its little-endian operand reads. The
// typed opcode handlers check an instruction's length once, refuse a short
// one, and then read operands at fixed offsets.
#include <cstddef>
#include <cstdint>
#include <span>

namespace VS::Event {

// One decoded instruction: its opcode byte followed by its operands.
using ScriptCommand = std::span<const std::uint8_t>;

// The operand byte at `offset` of an instruction whose length was checked.
[[nodiscard]] constexpr std::uint8_t OperandByte(ScriptCommand command,
                                                 std::size_t offset) {
  return command[offset];
}

// The little-endian 16-bit operand at `offset` of an instruction whose
// length was checked.
[[nodiscard]] constexpr std::int16_t OperandHalf(ScriptCommand command,
                                                 std::size_t offset) {
  constexpr unsigned kByteBits = 8;
  return static_cast<std::int16_t>(
      static_cast<unsigned>(command[offset]) |
      (static_cast<unsigned>(command[offset + 1]) << kByteBits));
}

}  // namespace VS::Event
