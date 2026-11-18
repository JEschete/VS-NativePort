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
#include "Effects/Runtime.h"
#include "TestSupport/EffectProgramScope.hpp"

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

namespace {
using Heights = std::array<std::int32_t, 3>;
constexpr Heights Filled(std::int32_t height) {
  return {height, height, height};
}
constexpr Heights kLowered = Filled(fx::kLoweredEffectHeight);
constexpr Heights kRaised = Filled(fx::kRaisedEffectHeight);
constexpr Heights kKept{5, -6, 7};

Heights OffsetHeights(std::uint16_t effect_id, std::uint8_t register3 = 0) {
  Heights heights = kKept;
  fx::ScriptRegisters registers{};
  registers[fx::kActionVariantRegister] = register3;
  CHECK(fx::SetActionEffectVerticalOffset(effect_id, registers, heights) ==
        ScriptStep::kContinue);
  return heights;
}
}  // namespace

TEST_CASE("Effect script vertical offsets lower effects 6-21, 0x21 and 0x24",
          "[effect-script]") {
  CHECK(OffsetHeights(6) == kLowered);
  CHECK(OffsetHeights(21) == kLowered);
  CHECK(OffsetHeights(0x21) == kLowered);
  CHECK(OffsetHeights(0x24) == kLowered);
  CHECK(OffsetHeights(5) == kKept);
  CHECK(OffsetHeights(22) == kKept);
  CHECK(OffsetHeights(0xFFFF) == kKept);
}

TEST_CASE("Effect script vertical offsets raise effect 0x38 for variant 0x2B",
          "[effect-script]") {
  CHECK(OffsetHeights(0x38, 0x2B) == kRaised);
  CHECK(OffsetHeights(0x38, 0x2A) == kKept);
  CHECK(OffsetHeights(0x37, 0x2B) == kKept);
}

TEST_CASE("Effect script source signals name only actor-part sources",
          "[effect-script]") {
  CHECK(fx::SourceActorToSignal(4, 7) == std::optional<std::uint8_t>{7});
  CHECK(fx::SourceActorToSignal(4, 0xFF) == std::optional<std::uint8_t>{0xFF});
  CHECK_FALSE(fx::SourceActorToSignal(3, 7).has_value());
  CHECK_FALSE(fx::SourceActorToSignal(0x104, 7).has_value());
}

namespace {
using Curves = fx::ParticleColorCurves;
using Step = fx::OpcodeStep;
constexpr Step kDefinitionRefused{std::unexpect, Refusal::kDefinition};
constexpr Step kPresetRefused{std::unexpect, Refusal::kColorPreset};
// Definition 2 with red 45 (0b101101), green 51 (0b110011) and blue 38
// (0b100110), packed low bits first.
constexpr std::uint8_t kPackedFirst = 0x42;
constexpr std::uint8_t kPackedSecond = 0x3B;
constexpr std::uint8_t kPackedThird = 0x9B;
constexpr Curves kPackedCurves{.red = 45, .green = 51, .blue = 38};
constexpr Curves kOldCurves{.red = 1, .green = 2, .blue = 3};
}  // namespace

TEST_CASE("Effect script colour curves unpack three operand bytes",
          "[effect-script]") {
  const auto program = Program({kPackedFirst, kPackedSecond, kPackedThird});
  std::vector<Curves> curves(3, kOldCurves);
  ScriptCursor cursor{.program = program, .offset = 0};
  CHECK(fx::SetParticleColorCurves(cursor, curves) == kContinued);
  CHECK(cursor.offset == 3);
  CHECK(curves == std::vector<Curves>{kOldCurves, kOldCurves, kPackedCurves});

  const auto full = Program({0xFF, 0xFF, 0xFF});
  ScriptCursor top{.program = full, .offset = 0};
  std::vector<Curves> many(64);
  CHECK(fx::SetParticleColorCurves(top, many) == kContinued);
  CHECK(many.back() == Curves{.red = 63, .green = 63, .blue = 63});
}

TEST_CASE("Effect script colour curves refuse definitions past the package",
          "[effect-script]") {
  const auto program = Program({kPackedFirst, kPackedSecond, kPackedThird});
  std::vector<Curves> curves(2, kOldCurves);
  ScriptCursor cursor{.program = program, .offset = 0};
  CHECK(fx::SetParticleColorCurves(cursor, curves) == kDefinitionRefused);
  CHECK(cursor.offset == 3);
  CHECK(curves == std::vector<Curves>(2, kOldCurves));

  // A truncated operand refuses before any definition is chosen.
  ScriptCursor truncated{.program = std::span{program}.first(2), .offset = 0};
  std::vector<Curves> enough(3, kOldCurves);
  CHECK(fx::SetParticleColorCurves(truncated, enough) == kOperandRefused);
  CHECK(truncated.offset == 2);
  CHECK(enough == std::vector<Curves>(3, kOldCurves));
}

TEST_CASE("Effect script paired presets set definitions 0 and 1",
          "[effect-script]") {
  const std::array<fx::PairedColorPreset, 2> presets{{
      {Curves{1, 2, 3}, Curves{4, 3, 3}},
      {Curves{3, 1, 2}, Curves{3, 4, 3}},
  }};
  std::vector<Curves> curves(3, kOldCurves);
  CHECK(fx::ApplyPairedColorPreset(1, presets, curves) == kContinued);
  CHECK(curves ==
        std::vector<Curves>{Curves{3, 1, 2}, Curves{3, 4, 3}, kOldCurves});
  CHECK(fx::ApplyPairedColorPreset(2, presets, curves) == kPresetRefused);
  std::vector<Curves> single(1, kOldCurves);
  CHECK(fx::ApplyPairedColorPreset(0, presets, single) == kDefinitionRefused);
  CHECK(single == std::vector<Curves>(1, kOldCurves));
}

TEST_CASE("Effect script preset tables apply every entry in order",
          "[effect-script]") {
  const auto entry = [](std::uint8_t definition, std::uint8_t red) {
    return fx::ColorPresetEntry{.curves = Curves{red, 0, 0},
                                .definition = definition};
  };
  const std::array<fx::ColorPresetTable, 2> tables{{
      {entry(1, 10), entry(3, 11), entry(1, 12), entry(0, 13), entry(2, 14)},
      {entry(0, 20), entry(1, 21), entry(2, 22), entry(3, 23), entry(4, 24)},
  }};
  std::vector<Curves> curves(4, kOldCurves);
  CHECK(fx::ApplyColorPresetTable(0, tables, curves) == kContinued);
  CHECK(curves == std::vector<Curves>{Curves{13, 0, 0}, Curves{12, 0, 0},
                                      Curves{14, 0, 0}, Curves{11, 0, 0}});

  // An entry past the definitions refuses the whole table.
  std::vector<Curves> kept(4, kOldCurves);
  CHECK(fx::ApplyColorPresetTable(1, tables, kept) == kDefinitionRefused);
  CHECK(kept == std::vector<Curves>(4, kOldCurves));
  CHECK(fx::ApplyColorPresetTable(2, tables, kept) == kPresetRefused);
}

TEST_CASE("Effect script child completions keep the 5-bit count wrap",
          "[effect-script]") {
  using Count = std::optional<std::uint8_t>;
  CHECK(fx::CompleteChildScript(Count{3}) == Count{2});
  CHECK(fx::CompleteChildScript(Count{1}) == Count{0});
  CHECK(fx::CompleteChildScript(Count{0}) == Count{fx::kChildCountMask});
  CHECK_FALSE(fx::CompleteChildScript(std::nullopt).has_value());
}

TEST_CASE("Effect script invalid instructions report and yield",
          "[effect-script]") {
  const fx::ScriptDiagnosticStep report = fx::ReportInvalidInstruction();
  CHECK(report.diagnostic == fx::EffectDiagnostic::kInvalidInstruction);
  CHECK(report.step == ScriptStep::kYield);
  CHECK(fx::HandlerResult(report.step) == 0);
  CHECK_FALSE(fx::DescribeEffectDiagnostic(0x1E).empty());
  CHECK_FALSE(fx::DescribeEffectDiagnostic(0x14).empty());
  CHECK(fx::DescribeEffectDiagnostic(0).empty());
}

TEST_CASE("Live effect script child completions release the parent's child",
          "[effect-script]") {
  Effects_EffectInstance parent{};
  parent.childCount = 2;
  Effects_EffectInstance child{};
  child.parent = &parent;
  CHECK(fx::LiveCompleteChildEffectScript(&child) == 1);
  CHECK(parent.childCount == 1);
  parent.childCount = 0;
  parent.frame = 0x1234;
  CHECK(fx::LiveCompleteChildEffectScript(&child) == 1);
  CHECK(parent.childCount == fx::kChildCountMask);
  CHECK(parent.frame == 0x1234);
  child.parent = nullptr;
  CHECK(fx::LiveCompleteChildEffectScript(&child) == 1);
}

TEST_CASE("Live effect script signals skip sources that are not actor parts",
          "[effect-script]") {
  Effects_EffectInstance instance{};
  instance.context.source.type = 3;
  CHECK(fx::LiveSignalSourceActorFromScript(&instance) == 1);
  CHECK(fx::LiveReportInvalidEffectInstruction(nullptr) == 0);
  CHECK(fx::LiveReportInvalidEffectInstruction(&instance) == 0);
  fx::LiveReportEffectDiagnostic(0x55);
}

TEST_CASE(
    "Live effect script transform and colour opcodes refuse null "
    "instances",
    "[effect-script]") {
  CHECK(fx::LiveSetActionEffectVerticalOffset(nullptr) == 2);
  CHECK(fx::LiveSignalSourceActorFromScript(nullptr) == 2);
  CHECK(fx::LiveSetParticleColorCurvesFromScript(nullptr) == 2);
  CHECK(fx::LiveApplyPairedParticleColorPreset(nullptr) == 2);
  CHECK(fx::LiveCompleteChildEffectScript(nullptr) == 2);
  CHECK(fx::LiveApplyParticleColorPresetTable(nullptr) == 2);
}

namespace {
// Records the effect runtime calls the timeline, transform and environment
// opcodes make.
class RecordingHost final : public fx::ScriptEffectHost {
 public:
  void LoadTimeline(std::uint8_t index) override { loaded.push_back(index); }
  void TickTimeline(std::uint16_t child_offset) override {
    ticked.push_back(child_offset);
  }
  void FreeTimeline() override { ++freed; }
  void UpdateTransforms() override { ++updated; }
  void InitializeTransforms() override { ++initialized; }
  void UpdateParticleEnvironment() override { ++refreshed; }
  void SpawnScript(std::uint16_t script_offset) override {
    spawned.push_back(script_offset);
  }
  void AttachRenderer(std::uint8_t index, std::int32_t parameters) override {
    attached.emplace_back(index, parameters);
  }
  void UpdateRenderers() override { ++renderer_updates; }
  void EmitParticles(std::uint8_t event) override { emitted.push_back(event); }

  std::vector<std::uint8_t> loaded;
  std::vector<std::uint16_t> ticked;
  int freed = 0;
  int updated = 0;
  int initialized = 0;
  int refreshed = 0;
  std::vector<std::uint16_t> spawned;
  std::vector<std::pair<std::uint8_t, std::int32_t>> attached;
  int renderer_updates = 0;
  std::vector<std::uint8_t> emitted;
};

constexpr Step kTimelineRefused{std::unexpect, Refusal::kTimeline};

std::vector<std::uint8_t> LoadedFor(std::uint8_t value, bool has_parent,
                                    const fx::TimelineRemap& remap) {
  const auto program = Program({1});
  fx::ScriptRegisters registers{};
  registers[1] = value;
  ScriptCursor cursor{.program = program, .offset = 0};
  RecordingHost host;
  CHECK(fx::LoadRegisterTimeline(cursor, registers, has_parent, remap, host) ==
        kContinued);
  CHECK(cursor.offset == 1);
  return host.loaded;
}
}  // namespace

TEST_CASE("Effect script immediate timelines load and move to E000's loop",
          "[effect-script]") {
  const auto program = Program({0x30, 0x99});
  ScriptCursor cursor{.program = program, .offset = 0};
  RecordingHost host;
  CHECK(fx::LoadTimelineImmediate(cursor, host) == kContinued);
  CHECK(cursor.offset == fx::kImmediateTimelineLoop);
  CHECK(host.loaded == std::vector<std::uint8_t>{0x30});

  ScriptCursor truncated{.program = std::span{program}.first(0), .offset = 0};
  CHECK(fx::LoadTimelineImmediate(truncated, host) == kOperandRefused);
  CHECK(host.loaded.size() == 1);
}

TEST_CASE("Effect script register timelines map root values through the ids",
          "[effect-script]") {
  const fx::TimelineRemap remap{.ids = {1, 2, 5, 0}, .timeline_count = 5};
  using Loaded = std::vector<std::uint8_t>;
  CHECK(LoadedFor(0, false, remap) == Loaded{0});
  CHECK(LoadedFor(1, false, remap) == Loaded{1});
  // An id equal to the count is not remapped, as in the original.
  CHECK(LoadedFor(2, false, remap) == Loaded{2});
  CHECK(LoadedFor(3, false, remap) == Loaded{3});
  CHECK(LoadedFor(4, false, remap) == Loaded{4});
  CHECK(LoadedFor(0xFF, false, remap) == Loaded{0xFF});
}

TEST_CASE("Effect script register timelines load children's values unmapped",
          "[effect-script]") {
  using Loaded = std::vector<std::uint8_t>;
  CHECK(LoadedFor(1, true, {.ids = {1, 2, 5, 0}, .timeline_count = 5}) ==
        Loaded{1});
  // Without a timeline count, or below a negative one, values load as read.
  CHECK(LoadedFor(1, false, {.ids = {1, 2, 5, 0}, .timeline_count = {}}) ==
        Loaded{1});
  CHECK(LoadedFor(0, false, {.ids = {9}, .timeline_count = -1}) == Loaded{0});
  CHECK(LoadedFor(0, false, {.ids = {9}, .timeline_count = 10}) == Loaded{8});
}

TEST_CASE("Effect script register timelines refuse operands and registers",
          "[effect-script]") {
  const auto program = Program({4});
  ScriptCursor cursor{.program = program, .offset = 0};
  RecordingHost host;
  const fx::TimelineRemap remap{};
  CHECK(fx::LoadRegisterTimeline(cursor, {}, false, remap, host) ==
        kRegisterRefused);
  CHECK(cursor.offset == 1);
  CHECK(fx::LoadRegisterTimeline(cursor, {}, false, remap, host) ==
        kOperandRefused);
  CHECK(host.loaded.empty());
}

TEST_CASE("Effect script timeline ticks pass the child offset halfword",
          "[effect-script]") {
  const auto program = Program({0xD4, 0x01, 0x34});
  ScriptCursor cursor{.program = program, .offset = 0};
  RecordingHost host;
  CHECK(fx::TickTimelineFromScript(cursor, true, host) == kContinued);
  CHECK(cursor.offset == 2);
  CHECK(host.ticked == std::vector<std::uint16_t>{0x01D4});

  // The operand is read before the missing timeline refuses.
  cursor.offset = 0;
  CHECK(fx::TickTimelineFromScript(cursor, false, host) == kTimelineRefused);
  CHECK(cursor.offset == 2);
  CHECK(fx::TickTimelineFromScript(cursor, true, host) == kOperandRefused);
  CHECK(host.ticked.size() == 1);
}

TEST_CASE("Effect script timeline branches wait for every track to finish",
          "[effect-script]") {
  const auto program = Program({0xC5, 0x00});
  ScriptCursor cursor{.program = program, .offset = 0};
  CHECK(fx::BranchIfTimelineFinished(1U, cursor) == kContinued);
  CHECK(cursor.offset == 2);
  cursor.offset = 0;
  CHECK(fx::BranchIfTimelineFinished(0x80000000U, cursor) == kContinued);
  CHECK(cursor.offset == 2);
  cursor.offset = 0;
  CHECK(fx::BranchIfTimelineFinished(0U, cursor) == kContinued);
  CHECK(cursor.offset == 0xC5);
  cursor.offset = 0;
  CHECK(fx::BranchIfTimelineFinished(std::nullopt, cursor) == kTimelineRefused);
  CHECK(cursor.offset == 0);
}

TEST_CASE("Effect script timeline frees run only for a loaded timeline",
          "[effect-script]") {
  RecordingHost host;
  CHECK(fx::FreeScriptTimeline(false, host) == ScriptStep::kContinue);
  CHECK(host.freed == 0);
  CHECK(fx::FreeScriptTimeline(true, host) == ScriptStep::kContinue);
  CHECK(host.freed == 1);
}

TEST_CASE("Effect script runtime opcodes call the effect host and continue",
          "[effect-script]") {
  RecordingHost host;
  CHECK(fx::TickScriptTransforms(host) == ScriptStep::kContinue);
  CHECK(fx::InitializeScriptTransforms(host) == ScriptStep::kContinue);
  CHECK(fx::RefreshParticleEnvironment(host) == ScriptStep::kContinue);
  const std::array<int, 4> calls{host.updated, host.initialized, host.refreshed,
                                 host.freed};
  constexpr std::array<int, 4> kExpectedCalls{1, 1, 1, 0};
  CHECK(calls == kExpectedCalls);
  CHECK(host.loaded.empty());
  CHECK(host.ticked.empty());
}

TEST_CASE("Live effect script timeline branches read the instance timeline",
          "[effect-script]") {
  auto program = Program({0xC5, 0x00});
  const VS::test_support::EffectProgramScope recorded{program};
  auto instance = InstanceAt(program, 0);
  Effects_EffectTimeline timeline{};
  timeline.activeTracks = 0;
  instance.context.timeline = &timeline;
  CHECK(fx::LiveBranchIfEffectTimelineFinished(&instance) == 1);
  CHECK(instance.scriptOffset == 0xC5);
}

TEST_CASE("Live effect script timeline opcodes end without a timeline",
          "[effect-script]") {
  auto program = Program({0xC5, 0x00, 0xD4, 0x00, 4});
  const VS::test_support::EffectProgramScope recorded{program};
  auto instance = InstanceAt(program, 0);
  // The original read through the null timeline; the native ends.
  CHECK(fx::LiveBranchIfEffectTimelineFinished(&instance) == 2);
  CHECK(instance.scriptOffset == 0);
  instance.scriptOffset = 2;
  CHECK(fx::LiveTickEffectTimelineFromScript(&instance) == 2);
  CHECK(instance.scriptOffset == 4);
  CHECK(fx::LiveFreeEffectScriptTimeline(&instance) == 1);
  CHECK(fx::LiveLoadEffectTimelineFromRegister(&instance) == 2);
  CHECK(instance.scriptOffset == 5);
}

TEST_CASE(
    "Live effect script timeline and transform opcodes refuse null "
    "instances",
    "[effect-script]") {
  CHECK(fx::LiveLoadEffectTimelineImmediate(nullptr) == 2);
  CHECK(fx::LiveLoadEffectTimelineFromRegister(nullptr) == 2);
  CHECK(fx::LiveTickEffectTimelineFromScript(nullptr) == 2);
  CHECK(fx::LiveBranchIfEffectTimelineFinished(nullptr) == 2);
  CHECK(fx::LiveFreeEffectScriptTimeline(nullptr) == 2);
  CHECK(fx::LiveTickEffectTransforms(nullptr) == 2);
  CHECK(fx::LiveInitializeScriptEffectTransforms(nullptr) == 2);
  CHECK(fx::LiveRefreshScriptParticleEnvironment(nullptr) == 2);
}

namespace {
constexpr Step kRendererRefused{std::unexpect, Refusal::kRenderer};
}  // namespace

TEST_CASE("F01519 child script spawns read the script offset halfword",
          "[effect-script][F01519]") {
  const auto program = Program({0x34, 0x12});
  ScriptCursor cursor{.program = program, .offset = 0};
  RecordingHost host;
  CHECK(fx::SpawnChildScript(cursor, host) == kContinued);
  CHECK(cursor.offset == 2);
  CHECK(host.spawned == std::vector<std::uint16_t>{0x1234});

  ScriptCursor truncated{.program = std::span{program}.first(1), .offset = 0};
  CHECK(fx::SpawnChildScript(truncated, host) == kOperandRefused);
  CHECK(host.spawned.size() == 1);
}

TEST_CASE("F01522 renderer attaches join the parameter halfwords",
          "[effect-script][F01522]") {
  const auto program = Program({1, 0x34, 0x12, 0x78, 0xF6});
  using Attached = std::vector<std::pair<std::uint8_t, std::int32_t>>;
  RecordingHost host;
  ScriptCursor cursor{.program = program, .offset = 0};
  CHECK(fx::AttachScriptRenderer(cursor, 0, host) == kContinued);
  CHECK(cursor.offset == 5);
  CHECK(host.attached == Attached{{1, static_cast<std::int32_t>(0xF6781234U)}});

  // A nonzero instance parameter replaces the operands, once they are read.
  cursor.offset = 0;
  CHECK(fx::AttachScriptRenderer(cursor, -7, host) == kContinued);
  CHECK(cursor.offset == 5);
  CHECK(host.attached.back() == Attached::value_type{1, -7});
}

TEST_CASE("F01522 renderer attaches refuse operands and unknown renderers",
          "[effect-script][F01522]") {
  const auto program = Program({2, 0x34, 0x12, 0x78, 0xF6});
  RecordingHost host;
  ScriptCursor cursor{.program = program, .offset = 0};
  CHECK(fx::AttachScriptRenderer(cursor, 0, host) == kRendererRefused);
  CHECK(cursor.offset == 5);
  const auto length = GENERATE(0U, 1U, 2U, 3U, 4U);
  const auto valid = Program({0, 0x34, 0x12, 0x78, 0xF6});
  ScriptCursor truncated{.program = std::span{valid}.first(length),
                         .offset = 0};
  CHECK(fx::AttachScriptRenderer(truncated, 0, host) == kOperandRefused);
  CHECK(host.attached.empty());
}

TEST_CASE("F01526 renderer ticks update the instance's renderers",
          "[effect-script][F01526]") {
  RecordingHost host;
  CHECK(fx::TickScriptRenderers(host) == ScriptStep::kContinue);
  CHECK(host.renderer_updates == 1);
}

TEST_CASE("F01534 register particles emit the event the register holds",
          "[effect-script][F01534]") {
  const fx::ScriptRegisters registers{0x10, 0x80, 0xFF, 3};
  RecordingHost host;
  const auto index = GENERATE(0U, 1U, 2U, 3U);
  const auto program = Program({static_cast<std::uint8_t>(index)});
  ScriptCursor cursor{.program = program, .offset = 0};
  CHECK(fx::EmitRegisterParticles(cursor, registers, host) == kContinued);
  CHECK(cursor.offset == 1);
  CHECK(host.emitted == std::vector<std::uint8_t>{registers.at(index)});
}

TEST_CASE("F01534 register particles refuse operands and registers",
          "[effect-script][F01534]") {
  const auto program = Program({4});
  RecordingHost host;
  ScriptCursor cursor{.program = program, .offset = 0};
  CHECK(fx::EmitRegisterParticles(cursor, {}, host) == kRegisterRefused);
  CHECK(cursor.offset == 1);
  CHECK(fx::EmitRegisterParticles(cursor, {}, host) == kOperandRefused);
  CHECK(host.emitted.empty());
}

TEST_CASE("F01522 the Live attach ends on an unknown renderer",
          "[effect-script][F01522]") {
  auto program = Program({2, 0x34, 0x12, 0x78, 0xF6});
  const VS::test_support::EffectProgramScope recorded{program};
  auto instance = InstanceAt(program, 0);
  CHECK(fx::LiveAttachScriptRenderer(&instance) == 2);
  CHECK(instance.scriptOffset == 5);
  CHECK(instance.context.renderer == nullptr);
}

TEST_CASE(
    "Live effect script spawn, renderer and emit opcodes refuse null "
    "instances",
    "[effect-script][F01519][F01522][F01526][F01534]") {
  CHECK(fx::LiveSpawnEffectScript(nullptr) == 2);
  CHECK(fx::LiveAttachScriptRenderer(nullptr) == 2);
  CHECK(fx::LiveTickEffectRenderers(nullptr) == 2);
  CHECK(fx::LiveEmitParticlesFromScriptRegister(nullptr) == 2);
}
