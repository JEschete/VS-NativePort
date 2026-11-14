#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Effects/EffectScript.hpp"
#include "Effects/EffectScriptLive.hpp"
#include "TestSupport/EffectProgramScope.hpp"
extern "C" {
#include "Effects/Runtime.h"
}

namespace {
namespace fx = VS::Effects;
using fx::ScriptCursor;
using fx::ScriptRun;
using fx::ScriptStep;

std::vector<std::byte> Program(std::initializer_list<std::uint8_t> bytes) {
  std::vector<std::byte> program;
  program.reserve(bytes.size());
  for (const std::uint8_t value : bytes) {
    program.push_back(static_cast<std::byte>(value));
  }
  return program;
}

// A full 16-bit program whose byte at each offset is its low byte, so a read
// that wraps shows where it read from.
std::vector<std::byte> WrappingProgram() {
  std::vector<std::byte> program(std::size_t{0x10000});
  for (std::size_t at = 0; at < program.size(); ++at) {
    program[at] = static_cast<std::byte>(at);
  }
  program[0xFFFF] = std::byte{0xAB};
  program[0] = std::byte{0xCD};
  return program;
}

// Opcodes are their read order; each executes the next scripted step, and
// the program holds `readable` opcodes.
class ScriptedMachine final : public fx::ScriptMachine {
 public:
  ScriptedMachine(std::vector<std::optional<ScriptStep>> steps,
                  std::size_t readable)
      : steps_(std::move(steps)), readable_(readable) {}

  std::optional<std::uint8_t> NextOpcode() override {
    if (reads_ >= readable_) return std::nullopt;
    return static_cast<std::uint8_t>(reads_++);
  }

  std::optional<ScriptStep> Execute(std::uint8_t opcode) override {
    executed.push_back(opcode);
    return steps_.at(opcode);
  }

  std::vector<std::uint8_t> executed;

 private:
  std::vector<std::optional<ScriptStep>> steps_;
  std::size_t readable_;
  std::size_t reads_ = 0;
};

Effects_EffectInstance* AsInstance(void* instance) {
  return static_cast<Effects_EffectInstance*>(instance);
}

int YieldHandler(void* /*instance*/) { return 0; }
int OddHandler(void* /*instance*/) { return 7; }
int ContinueHandler(void* instance) {
  return fx::LiveContinueEffectScript(AsInstance(instance));
}
int EndHandler(void* instance) {
  return fx::LiveEndEffectScript(AsInstance(instance));
}
int JumpHandler(void* instance) {
  return fx::LiveJumpEffectScript(AsInstance(instance));
}
int YieldAtHandler(void* instance) {
  return fx::LiveYieldEffectScriptAtOffset(AsInstance(instance));
}

constexpr std::uint8_t kOpYield = 0;
constexpr std::uint8_t kOpContinue = 1;
constexpr std::uint8_t kOpEnd = 2;
constexpr std::uint8_t kOpJump = 3;
constexpr std::uint8_t kOpYieldAt = 4;
constexpr std::uint8_t kOpOdd = 5;
constexpr std::uint8_t kOpMissing = 6;
constexpr std::uint8_t kOpPastTable = 49;

// Installs test handlers in the production handler table while it lives.
class HandlerTableScope {
 public:
  HandlerTableScope() : saved_(std::to_array(Effects_scriptOpcodeHandlers)) {
    std::ranges::fill(Effects_scriptOpcodeHandlers, nullptr);
    Effects_scriptOpcodeHandlers[kOpYield] = YieldHandler;
    Effects_scriptOpcodeHandlers[kOpContinue] = ContinueHandler;
    Effects_scriptOpcodeHandlers[kOpEnd] = EndHandler;
    Effects_scriptOpcodeHandlers[kOpJump] = JumpHandler;
    Effects_scriptOpcodeHandlers[kOpYieldAt] = YieldAtHandler;
    Effects_scriptOpcodeHandlers[kOpOdd] = OddHandler;
  }
  HandlerTableScope(const HandlerTableScope&) = delete;
  HandlerTableScope& operator=(const HandlerTableScope&) = delete;
  HandlerTableScope(HandlerTableScope&&) = delete;
  HandlerTableScope& operator=(HandlerTableScope&&) = delete;
  ~HandlerTableScope() {
    std::ranges::copy(saved_, std::begin(Effects_scriptOpcodeHandlers));
  }

 private:
  decltype(std::to_array(Effects_scriptOpcodeHandlers)) saved_;
};

Effects_EffectInstance InstanceAt(std::vector<std::byte>& program,
                                  std::uint16_t offset) {
  Effects_EffectInstance instance{};
  instance.script = reinterpret_cast<char*>(program.data());
  instance.scriptOffset = offset;
  return instance;
}
}  // namespace

TEST_CASE("Effect script byte reads advance one byte and stop at the program",
          "[effect-script]") {
  const auto program = Program({0x12, 0xFE});
  ScriptCursor cursor{.program = program, .offset = 0};
  CHECK(fx::ReadScriptByte(cursor) == std::optional<std::uint8_t>{0x12});
  CHECK(cursor.offset == 1);
  CHECK(fx::ReadScriptByte(cursor) == std::optional<std::uint8_t>{0xFE});
  CHECK(cursor.offset == 2);
  CHECK_FALSE(fx::ReadScriptByte(cursor));
  CHECK(cursor.offset == 2);

  ScriptCursor empty{};
  CHECK_FALSE(fx::ReadScriptByte(empty));
  CHECK(empty.offset == 0);
}

TEST_CASE("Effect script reads wrap the 16-bit offset as the original did",
          "[effect-script]") {
  const auto program = WrappingProgram();
  ScriptCursor cursor{.program = program, .offset = 0xFFFF};
  CHECK(fx::ReadScriptByte(cursor) == std::optional<std::uint8_t>{0xAB});
  CHECK(cursor.offset == 0);

  cursor.offset = 0xFFFF;
  CHECK(fx::ReadScriptHalfword(cursor) == std::optional<std::uint16_t>{0xCDAB});
  CHECK(cursor.offset == 1);
}

TEST_CASE("Effect script halfwords read the low byte first and refuse whole",
          "[effect-script]") {
  const auto program = Program({0x34, 0x12, 0x78});
  ScriptCursor cursor{.program = program, .offset = 0};
  CHECK(fx::ReadScriptHalfword(cursor) == std::optional<std::uint16_t>{0x1234});
  CHECK(cursor.offset == 2);
  CHECK_FALSE(fx::ReadScriptHalfword(cursor));
  CHECK(cursor.offset == 2);
}

TEST_CASE("Effect script branches move to the operand only when taken",
          "[effect-script]") {
  const auto program = Program({0x00, 0x34, 0x12});
  ScriptCursor cursor{.program = program, .offset = 1};
  CHECK(fx::BranchScriptIf(true, cursor));
  CHECK(cursor.offset == 0x1234);

  cursor.offset = 1;
  CHECK(fx::BranchScriptIf(false, cursor));
  CHECK(cursor.offset == 3);

  // An untaken branch skips its operand without reading it, wrapping.
  cursor.offset = 0xFFFF;
  CHECK(fx::BranchScriptIf(false, cursor));
  CHECK(cursor.offset == 1);

  cursor.offset = 2;
  CHECK_FALSE(fx::BranchScriptIf(true, cursor));
  CHECK(cursor.offset == 2);
}

TEST_CASE("Effect script jumps continue and yields stop at the operand",
          "[effect-script]") {
  const auto program = Program({0x10, 0x00, 0x20});
  ScriptCursor cursor{.program = program, .offset = 0};
  CHECK(fx::JumpScript(cursor) == ScriptStep::kContinue);
  CHECK(cursor.offset == 0x10);

  cursor.offset = 0;
  CHECK(fx::YieldScriptAt(cursor) == ScriptStep::kYield);
  CHECK(cursor.offset == 0x10);

  CHECK(fx::EndScript() == ScriptStep::kEnd);
  CHECK(fx::ContinueScript() == ScriptStep::kContinue);
}

TEST_CASE("Effect script jumps and yields end on a refused operand",
          "[effect-script]") {
  const auto program = Program({0x10, 0x00, 0x20});
  ScriptCursor cursor{.program = program, .offset = 2};
  CHECK(fx::JumpScript(cursor) == ScriptStep::kEnd);
  CHECK(cursor.offset == 2);
  CHECK(fx::YieldScriptAt(cursor) == ScriptStep::kEnd);
  CHECK(cursor.offset == 2);
}

TEST_CASE("Effect script handler results map to steps as the loop reads them",
          "[effect-script]") {
  CHECK(fx::StepFromHandlerResult(0) == ScriptStep::kYield);
  CHECK(fx::StepFromHandlerResult(1) == ScriptStep::kContinue);
  CHECK(fx::StepFromHandlerResult(2) == ScriptStep::kEnd);
  CHECK(fx::StepFromHandlerResult(3) == ScriptStep::kYield);
  CHECK(fx::StepFromHandlerResult(-1) == ScriptStep::kYield);
  for (const auto step :
       {ScriptStep::kYield, ScriptStep::kContinue, ScriptStep::kEnd}) {
    CHECK(fx::StepFromHandlerResult(fx::HandlerResult(step)) == step);
  }
}

TEST_CASE("Effect script runs continue until a handler yields or ends",
          "[effect-script]") {
  const auto [last, run] =
      GENERATE(std::pair{ScriptStep::kYield, ScriptRun::kYielded},
               std::pair{ScriptStep::kEnd, ScriptRun::kEnded});
  ScriptedMachine machine{
      {ScriptStep::kContinue, ScriptStep::kContinue, last, ScriptStep::kEnd},
      4};
  CHECK(fx::RunScript(machine) == run);
  CHECK(machine.executed == std::vector<std::uint8_t>{0, 1, 2});
}

TEST_CASE("Effect script runs end on a refused opcode or a missing handler",
          "[effect-script]") {
  ScriptedMachine past_end{{ScriptStep::kContinue, ScriptStep::kContinue}, 2};
  CHECK(fx::RunScript(past_end) == ScriptRun::kEnded);
  CHECK(past_end.executed == std::vector<std::uint8_t>{0, 1});

  ScriptedMachine missing{{ScriptStep::kContinue, std::nullopt}, 4};
  CHECK(fx::RunScript(missing) == ScriptRun::kEnded);
  CHECK(missing.executed == std::vector<std::uint8_t>{0, 1});
}

TEST_CASE("Live effect script runs dispatch the production handler table",
          "[effect-script]") {
  const HandlerTableScope handlers;
  auto program = Program({kOpContinue, kOpJump, 0x06, 0x00, kOpEnd, kOpEnd,
                          kOpYieldAt, 0x09, 0x00, kOpOdd, kOpYield, kOpEnd});
  const VS::test_support::EffectProgramScope recorded{program};
  auto instance = InstanceAt(program, 0);

  CHECK(fx::LiveRunEffectScript(&instance) == 1);
  CHECK(instance.scriptOffset == 9);
  CHECK(fx::LiveRunEffectScript(&instance) == 1);
  CHECK(instance.scriptOffset == 10);
  CHECK(fx::LiveRunEffectScript(&instance) == 1);
  CHECK(instance.scriptOffset == 11);
  CHECK(fx::LiveRunEffectScript(&instance) == 0);
  CHECK(instance.scriptOffset == 12);
}

TEST_CASE("Live effect script runs end on opcodes the table cannot run",
          "[effect-script]") {
  const HandlerTableScope handlers;
  const auto opcode = GENERATE(kOpMissing, kOpPastTable, std::uint8_t{0xFF});
  auto program = Program({kOpContinue, opcode, kOpYield});
  const VS::test_support::EffectProgramScope recorded{program};
  auto instance = InstanceAt(program, 0);
  CHECK(fx::LiveRunEffectScript(&instance) == 0);
  CHECK(instance.scriptOffset == 2);
}

TEST_CASE("Live effect script runs end on an unrecorded program",
          "[effect-script]") {
  const HandlerTableScope handlers;
  auto program = Program({kOpContinue, kOpJump, 0x01});
  auto instance = InstanceAt(program, 0);
  CHECK(fx::LiveRunEffectScript(&instance) == 0);
  CHECK(instance.scriptOffset == 0);
  CHECK(fx::LiveReadScriptByte(&instance) == 0);
  CHECK(instance.scriptOffset == 0);
}

TEST_CASE("Live effect script operands end where a truncated read was refused",
          "[effect-script]") {
  const HandlerTableScope handlers;
  auto program = Program({kOpContinue, kOpJump, 0x01});
  const VS::test_support::EffectProgramScope recorded{program};
  auto instance = InstanceAt(program, 0);
  CHECK(fx::LiveRunEffectScript(&instance) == 0);
  CHECK(instance.scriptOffset == 2);
  instance.scriptOffset = 2;
  CHECK(fx::LiveReadScriptHalfword(&instance) == 0);
  CHECK(instance.scriptOffset == 2);
}

TEST_CASE("Live effect script reads refuse a forgotten program",
          "[effect-script]") {
  auto program = Program({kOpContinue, kOpJump, 0x01});
  auto instance = InstanceAt(program, 0);
  {
    const VS::test_support::EffectProgramScope recorded{program};
    CHECK(fx::LiveReadScriptByte(&instance) == kOpContinue);
  }
  CHECK(fx::LiveReadScriptByte(&instance) == 0);
  CHECK(instance.scriptOffset == 1);
}

TEST_CASE("Live effect script operands store the moved offset",
          "[effect-script]") {
  auto program = Program({0x34, 0x12, 0x78, 0x56});
  const VS::test_support::EffectProgramScope recorded{program};
  auto instance = InstanceAt(program, 0);

  CHECK(fx::LiveReadScriptHalfword(&instance) == 0x1234);
  CHECK(instance.scriptOffset == 2);
  CHECK(fx::LiveReadScriptByte(&instance) == 0x78);
  CHECK(instance.scriptOffset == 3);

  instance.scriptOffset = 2;
  fx::LiveBranchEffectScriptIf(1, &instance);
  CHECK(instance.scriptOffset == 0x5678);
  instance.scriptOffset = 2;
  fx::LiveBranchEffectScriptIf(0, &instance);
  CHECK(instance.scriptOffset == 4);
}

TEST_CASE("Live effect script entries refuse a null instance",
          "[effect-script]") {
  CHECK(fx::LiveRunEffectScript(nullptr) == 0);
  CHECK(fx::LiveReadScriptByte(nullptr) == 0);
  CHECK(fx::LiveReadScriptHalfword(nullptr) == 0);
  CHECK(fx::LiveJumpEffectScript(nullptr) == 2);
  CHECK(fx::LiveYieldEffectScriptAtOffset(nullptr) == 2);
  fx::LiveBranchEffectScriptIf(1, nullptr);
  CHECK(fx::LiveEndEffectScript(nullptr) == 2);
  CHECK(fx::LiveContinueEffectScript(nullptr) == 1);
}
