#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <initializer_list>
#include <iterator>
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

using Refusal = fx::ScriptRefusal;
constexpr fx::OpcodeStep kContinued{ScriptStep::kContinue};
constexpr fx::OpcodeStep kOperandRefused{std::unexpect, Refusal::kOperand};
constexpr fx::OpcodeStep kRegisterRefused{std::unexpect, Refusal::kRegister};

// Restores the selector, the action variant and its four values.
class ScriptGlobalsScope {
 public:
  ScriptGlobalsScope()
      : selector_(Effects_scriptSelector),
        variant_(Effects_actionVariant),
        values_(VariantValues()) {}
  ScriptGlobalsScope(const ScriptGlobalsScope&) = delete;
  ScriptGlobalsScope& operator=(const ScriptGlobalsScope&) = delete;
  ScriptGlobalsScope(ScriptGlobalsScope&&) = delete;
  ScriptGlobalsScope& operator=(ScriptGlobalsScope&&) = delete;
  ~ScriptGlobalsScope() {
    Effects_scriptSelector = selector_;
    Effects_actionVariant = variant_;
    std::ranges::copy(values_, std::begin(Effects_actionVariantValues));
  }

  static void SetVariantValues(std::array<char, fx::kActionVariantValues> to) {
    std::ranges::copy(to, std::begin(Effects_actionVariantValues));
  }

 private:
  static std::array<char, fx::kActionVariantValues> VariantValues() {
    std::array<char, fx::kActionVariantValues> values{};
    std::ranges::copy_n(std::begin(Effects_actionVariantValues), values.size(),
                        values.begin());
    return values;
  }

  int selector_;
  std::uint8_t variant_;
  std::array<char, fx::kActionVariantValues> values_;
};
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

TEST_CASE("Effect script selectors store the zero-extended operand byte",
          "[effect-script]") {
  const auto program = Program({0xFF});
  ScriptCursor cursor{.program = program, .offset = 0};
  std::int32_t selector = -1;
  CHECK(fx::SetScriptSelector(cursor, selector) == kContinued);
  CHECK(selector == 0xFF);
  CHECK(cursor.offset == 1);

  selector = 7;
  CHECK(fx::SetScriptSelector(cursor, selector) == kOperandRefused);
  CHECK(selector == 7);
  CHECK(cursor.offset == 1);
}

TEST_CASE("Effect script renderer branches wait for the renderers to finish",
          "[effect-script]") {
  const auto program = Program({0x34, 0x12});
  ScriptCursor cursor{.program = program, .offset = 0};
  CHECK(fx::BranchIfRenderersFinished(false, cursor) == kContinued);
  CHECK(cursor.offset == 0x1234);
  cursor.offset = 0;
  CHECK(fx::BranchIfRenderersFinished(true, cursor) == kContinued);
  CHECK(cursor.offset == 2);
}

TEST_CASE("Effect script variant branches wait for a cleared action variant",
          "[effect-script]") {
  const auto program = Program({0x34, 0x12});
  ScriptCursor cursor{.program = program, .offset = 0};
  CHECK(fx::BranchIfActionVariantCleared(0, cursor) == kContinued);
  CHECK(cursor.offset == 0x1234);
  cursor.offset = 0;
  CHECK(fx::BranchIfActionVariantCleared(1, cursor) == kContinued);
  CHECK(cursor.offset == 2);

  cursor.offset = 1;
  CHECK(fx::BranchIfActionVariantCleared(0, cursor) == kOperandRefused);
  CHECK(cursor.offset == 1);
}

TEST_CASE("Effect script particle-count branches compare the operand byte",
          "[effect-script]") {
  const auto program = Program({0x90, 0x34, 0x12});
  ScriptCursor cursor{.program = program, .offset = 0};
  CHECK(fx::BranchIfParticleCountEqual(0x90, cursor) == kContinued);
  CHECK(cursor.offset == 0x1234);
  cursor.offset = 0;
  CHECK(fx::BranchIfParticleCountEqual(0x10, cursor) == kContinued);
  CHECK(cursor.offset == 3);

  cursor.offset = 1;
  CHECK(fx::BranchIfParticleCountEqual(0x34, cursor) == kOperandRefused);
  CHECK(cursor.offset == 2);
}

TEST_CASE("Effect script action variants below four load register 3",
          "[effect-script]") {
  constexpr fx::ActionVariantValues kValues{10, 20, 30, 40};
  for (const std::uint8_t variant : std::to_array<std::uint8_t>({0, 1, 2, 3})) {
    fx::ScriptRegisters registers{1, 2, 3, 4};
    CHECK(fx::LoadActionVariantRegister(variant, kValues, registers) ==
          ScriptStep::kContinue);
    CHECK(registers == fx::ScriptRegisters{1, 2, 3, kValues.at(variant)});
  }
  for (const std::uint8_t variant : std::to_array<std::uint8_t>({4, 5, 0xFF})) {
    fx::ScriptRegisters registers{1, 2, 3, 4};
    CHECK(fx::LoadActionVariantRegister(variant, kValues, registers) ==
          ScriptStep::kContinue);
    CHECK(registers == fx::ScriptRegisters{1, 2, 3, 4});
  }

  std::uint8_t variant = 3;
  CHECK(fx::ClearActionVariant(variant) == ScriptStep::kContinue);
  CHECK(variant == 0);
}

TEST_CASE("Effect script register writes keep the halfword's low byte",
          "[effect-script]") {
  const auto program = Program({1, 0x34, 0x12, 3, 0xFF, 0x00});
  ScriptCursor cursor{.program = program, .offset = 0};
  fx::ScriptRegisters registers{1, 2, 3, 4};
  CHECK(fx::SetScriptRegister(cursor, registers) == kContinued);
  CHECK(registers == fx::ScriptRegisters{1, 0x34, 3, 4});
  CHECK(cursor.offset == 3);
  CHECK(fx::SetScriptRegister(cursor, registers) == kContinued);
  CHECK(registers == fx::ScriptRegisters{1, 0x34, 3, 0xFF});
  CHECK(cursor.offset == 6);
}

TEST_CASE("Effect script register branches compare the whole halfword",
          "[effect-script]") {
  const auto program =
      Program({2, 0x80, 0x00, 0x78, 0x56, 2, 0x80, 0x01, 0x78, 0x56});
  const fx::ScriptRegisters registers{0, 0, 0x80, 0};
  ScriptCursor cursor{.program = program, .offset = 0};
  CHECK(fx::BranchIfScriptRegisterEqual(cursor, registers) == kContinued);
  CHECK(cursor.offset == 0x5678);

  // 0x180 keeps the register's low byte but is not equal to it.
  cursor.offset = 5;
  CHECK(fx::BranchIfScriptRegisterEqual(cursor, registers) == kContinued);
  CHECK(cursor.offset == 10);
}

TEST_CASE("Effect script register operands refuse registers past the four",
          "[effect-script]") {
  const auto program = Program({4, 0x34, 0x12, 0x78, 0x56, 0xFF, 0x00});
  fx::ScriptRegisters registers{1, 2, 3, 4};
  ScriptCursor cursor{.program = program, .offset = 0};
  CHECK(fx::SetScriptRegister(cursor, registers) == kRegisterRefused);
  CHECK(cursor.offset == 3);
  cursor.offset = 0;
  CHECK(fx::BranchIfScriptRegisterEqual(cursor, registers) == kRegisterRefused);
  CHECK(cursor.offset == 3);
  CHECK(registers == fx::ScriptRegisters{1, 2, 3, 4});

  // A truncated halfword refuses the operand, whatever the index.
  cursor.offset = 5;
  CHECK(fx::SetScriptRegister(cursor, registers) == kOperandRefused);
  CHECK(cursor.offset == 6);
  cursor.offset = 5;
  CHECK(fx::BranchIfScriptRegisterEqual(cursor, registers) == kOperandRefused);
  CHECK(registers == fx::ScriptRegisters{1, 2, 3, 4});
}

TEST_CASE("Live effect script register opcodes update the instance",
          "[effect-script]") {
  auto program = Program({1, 0x34, 0x12, 1, 0x34, 0x00, 0x20, 0x00});
  const VS::test_support::EffectProgramScope recorded{program};
  auto instance = InstanceAt(program, 0);
  CHECK(fx::LiveSetEffectScriptRegister(&instance) == 1);
  CHECK(std::bit_cast<std::uint8_t>(instance.registers[1]) == 0x34);
  CHECK(instance.scriptOffset == 3);
  CHECK(fx::LiveBranchIfEffectRegisterEqual(&instance) == 1);
  CHECK(instance.scriptOffset == 0x20);

  // The original compared the count as an unsigned byte.
  auto counted = Program({0x90, 0x40, 0x00});
  const VS::test_support::EffectProgramScope counted_scope{counted};
  auto particles = InstanceAt(counted, 0);
  particles.particleCount = std::bit_cast<char>(std::uint8_t{0x90});
  CHECK(fx::LiveBranchIfParticleCountEqual(&particles) == 1);
  CHECK(particles.scriptOffset == 0x40);

  particles.scriptOffset = 1;
  CHECK(fx::LiveBranchIfEffectRenderersFinished(&particles) == 1);
  CHECK(particles.scriptOffset == 0x40);
}

TEST_CASE("Live effect script opcodes follow the selector and action variant",
          "[effect-script]") {
  const ScriptGlobalsScope globals;
  ScriptGlobalsScope::SetVariantValues({10, 20, 30, 40});
  auto program = Program({0xC8, 0x34, 0x12});
  const VS::test_support::EffectProgramScope recorded{program};
  auto instance = InstanceAt(program, 0);

  CHECK(fx::LiveSetEffectScriptSelector(&instance) == 1);
  CHECK(Effects_scriptSelector == 0xC8);

  Effects_actionVariant = 2;
  CHECK(fx::LiveLoadActionVariantRegister(&instance) == 1);
  CHECK(instance.registers[3] == 30);
  CHECK(fx::LiveBranchIfActionVariantCleared(&instance) == 1);
  CHECK(instance.scriptOffset == 3);
}

TEST_CASE("Live effect script variant clears release the variant branch",
          "[effect-script]") {
  const ScriptGlobalsScope globals;
  auto program = Program({0x34, 0x12});
  const VS::test_support::EffectProgramScope recorded{program};
  auto instance = InstanceAt(program, 0);

  Effects_actionVariant = 2;
  CHECK(fx::LiveClearScriptActionVariant(nullptr) == 1);
  CHECK(Effects_actionVariant == 0);
  CHECK(fx::LiveBranchIfActionVariantCleared(&instance) == 1);
  CHECK(instance.scriptOffset == 0x1234);
}

TEST_CASE("Live effect script register opcodes end on refused operands",
          "[effect-script]") {
  auto program = Program({4, 0x34, 0x12, 1, 0x34});
  const VS::test_support::EffectProgramScope recorded{program};
  auto instance = InstanceAt(program, 0);
  instance.registers[1] = 9;
  CHECK(fx::LiveSetEffectScriptRegister(&instance) == 2);
  CHECK(instance.scriptOffset == 3);
  CHECK(fx::LiveSetEffectScriptRegister(&instance) == 2);
  CHECK(instance.scriptOffset == 4);
  CHECK(instance.registers[1] == 9);
  instance.scriptOffset = 0;
  CHECK(fx::LiveBranchIfEffectRegisterEqual(&instance) == 2);
}

TEST_CASE("Live effect script operand opcodes refuse null instances",
          "[effect-script]") {
  CHECK(fx::LiveSetEffectScriptSelector(nullptr) == 2);
  CHECK(fx::LiveBranchIfEffectRenderersFinished(nullptr) == 2);
  CHECK(fx::LiveBranchIfActionVariantCleared(nullptr) == 2);
  CHECK(fx::LiveLoadActionVariantRegister(nullptr) == 2);
  CHECK(fx::LiveSetEffectScriptRegister(nullptr) == 2);
  CHECK(fx::LiveBranchIfEffectRegisterEqual(nullptr) == 2);
  CHECK(fx::LiveBranchIfParticleCountEqual(nullptr) == 2);
}
