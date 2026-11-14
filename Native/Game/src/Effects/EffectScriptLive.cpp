// Hand-maintained (tools/port/owned.csv).
#include "Effects/EffectScriptLive.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <type_traits>

#include "Core/Log.hpp"
#include "Effects/EffectScript.hpp"
#include "Effects/PackageExtents.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Effects/Runtime.h"
}

static_assert(std::extent_v<decltype(Effects_scriptOpcodeHandlers)> ==
              VS::Effects::kEffectScriptOpcodes);
static_assert(std::extent_v<decltype(Effects_EffectInstance::registers)> ==
              VS::Effects::kScriptRegisters);
static_assert(std::extent_v<decltype(Effects_actionVariantValues)> >=
              VS::Effects::kActionVariantValues);

namespace VS::Effects {
namespace {
std::uint64_t refused_calls = 0;
std::uint64_t refused_reads = 0;
std::uint64_t refused_opcodes = 0;
std::uint64_t refused_registers = 0;

[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

void ReportRefusal(const char* entry) {
  if (ShouldReport(refused_calls)) {
    VS_LOG_WARN("effects",
                "effect script: {} refused: no effect instance ({} refusals)",
                entry, refused_calls);
  }
}

[[nodiscard]] ScriptCursor CursorOf(const Effects_EffectInstance& instance) {
  return {.program = LiveEffectPrograms(instance.script),
          .offset = instance.scriptOffset};
}

// Stores the cursor's offset back, reporting where a read was refused.
void Settle(Effects_EffectInstance& instance, const ScriptCursor& cursor,
            bool refused) {
  instance.scriptOffset = cursor.offset;
  if (refused && ShouldReport(refused_reads)) {
    VS_LOG_WARN("effects",
                "effect script: read at {:#x} refused: program has {} bytes "
                "({} refusals)",
                cursor.offset, cursor.program.size(), refused_reads);
  }
}

template <typename Value, typename Read>
[[nodiscard]] Value ReadOperand(Effects_EffectInstance* instance,
                                const char* entry, Read read) {
  if (instance == nullptr) {
    ReportRefusal(entry);
    return 0;
  }
  auto cursor = CursorOf(*instance);
  const std::optional<Value> value = read(cursor);
  Settle(*instance, cursor, !value);
  return value.value_or(0);
}

template <typename Step>
[[nodiscard]] int RunStep(Effects_EffectInstance* instance, const char* entry,
                          Step step) {
  if (instance == nullptr) {
    ReportRefusal(entry);
    return HandlerResult(ScriptStep::kEnd);
  }
  auto cursor = CursorOf(*instance);
  // The operand steps end the instance only when their operand is refused.
  const ScriptStep result = step(cursor);
  Settle(*instance, cursor, result == ScriptStep::kEnd);
  return HandlerResult(result);
}

// Runs an operand opcode over the instance's cursor; a refused operand or
// register ends the instance.
template <typename Opcode>
[[nodiscard]] int RunOpcode(Effects_EffectInstance* instance, const char* entry,
                            Opcode opcode) {
  if (instance == nullptr) {
    ReportRefusal(entry);
    return HandlerResult(ScriptStep::kEnd);
  }
  auto cursor = CursorOf(*instance);
  const OpcodeStep step = opcode(*instance, cursor);
  const auto refusal = step ? std::nullopt : std::optional{step.error()};
  Settle(*instance, cursor, refusal == ScriptRefusal::kOperand);
  if (refusal == ScriptRefusal::kRegister && ShouldReport(refused_registers)) {
    VS_LOG_WARN("effects",
                "effect script: {} before {:#x} names a register past the "
                "{} registers ({} refusals)",
                entry, cursor.offset, kScriptRegisters, refused_registers);
  }
  return HandlerResult(step.value_or(ScriptStep::kEnd));
}

[[nodiscard]] ScriptRegisters RegistersOf(
    const Effects_EffectInstance& instance) {
  ScriptRegisters registers{};
  std::ranges::transform(instance.registers, registers.begin(), [](char value) {
    return std::bit_cast<std::uint8_t>(value);
  });
  return registers;
}

void StoreRegisters(Effects_EffectInstance& instance,
                    const ScriptRegisters& registers) {
  std::ranges::transform(
      registers, std::begin(instance.registers),
      [](std::uint8_t value) { return std::bit_cast<char>(value); });
}

[[nodiscard]] ActionVariantValues VariantValues() {
  ActionVariantValues values{};
  std::ranges::transform(
      std::span{Effects_actionVariantValues}.first<kActionVariantValues>(),
      values.begin(),
      [](char value) { return std::bit_cast<std::uint8_t>(value); });
  return values;
}

// The instance a frame's run reads and dispatches through the handler table.
class InstanceMachine final : public ScriptMachine {
 public:
  explicit InstanceMachine(Effects_EffectInstance* instance)
      : instance_(instance) {}

  std::optional<std::uint8_t> NextOpcode() override {
    auto cursor = CursorOf(*instance_);
    const auto opcode = ReadScriptByte(cursor);
    Settle(*instance_, cursor, !opcode);
    return opcode;
  }

  std::optional<ScriptStep> Execute(std::uint8_t opcode) override {
    const std::span handlers{Effects_scriptOpcodeHandlers};
    if (opcode >= handlers.size() || handlers[opcode] == nullptr) {
      if (ShouldReport(refused_opcodes)) {
        VS_LOG_WARN("effects",
                    "effect script: opcode {:#x} at {:#x} has no handler ({} "
                    "refusals)",
                    opcode,
                    static_cast<std::uint16_t>(instance_->scriptOffset - 1),
                    refused_opcodes);
      }
      return std::nullopt;
    }
    return StepFromHandlerResult(handlers[opcode](instance_));
  }

 private:
  Effects_EffectInstance* instance_;
};
}  // namespace

// decomp: func_800D46DC (src/BATTLE/BATTLE.PRG/5BF94.c:7506)
void LiveBranchEffectScriptIf(int condition, Effects_EffectInstance* instance) {
  if (instance == nullptr) {
    ReportRefusal("branch");
    return;
  }
  auto cursor = CursorOf(*instance);
  const bool moved = BranchScriptIf(condition != 0, cursor);
  Settle(*instance, cursor, !moved);
}

// decomp: func_800D4720 (src/BATTLE/BATTLE.PRG/5BF94.c:7515)
int LiveYieldEffectScriptAtOffset(Effects_EffectInstance* instance) {
  return RunStep(instance, "yield",
                 [](ScriptCursor& cursor) { return YieldScriptAt(cursor); });
}

// decomp: func_800D474C (src/BATTLE/BATTLE.PRG/5BF94.c:7521)
int LiveJumpEffectScript(Effects_EffectInstance* instance) {
  return RunStep(instance, "jump",
                 [](ScriptCursor& cursor) { return JumpScript(cursor); });
}

// decomp: func_800D47C0 (src/BATTLE/BATTLE.PRG/5BF94.c:7538)
int LiveEndEffectScript(Effects_EffectInstance* /*instance*/) {
  return HandlerResult(EndScript());
}

// decomp: func_800D4DA8 (src/BATTLE/BATTLE.PRG/5BF94.c:7711)
int LiveContinueEffectScript(Effects_EffectInstance* /*instance*/) {
  return HandlerResult(ContinueScript());
}

// decomp: func_800D5170 (src/BATTLE/BATTLE.PRG/5BF94.c:7804)
std::uint8_t LiveReadScriptByte(Effects_EffectInstance* instance) {
  return ReadOperand<std::uint8_t>(
      instance, "read byte",
      [](ScriptCursor& cursor) { return ReadScriptByte(cursor); });
}

// decomp: func_800D5198 (src/BATTLE/BATTLE.PRG/5BF94.c:7806)
std::uint16_t LiveReadScriptHalfword(Effects_EffectInstance* instance) {
  return ReadOperand<std::uint16_t>(
      instance, "read halfword",
      [](ScriptCursor& cursor) { return ReadScriptHalfword(cursor); });
}

// decomp: func_800D51D8 (src/BATTLE/BATTLE.PRG/5BF94.c:7813)
int LiveRunEffectScript(Effects_EffectInstance* instance) {
  if (instance == nullptr) {
    ReportRefusal("run");
    return 0;
  }
  InstanceMachine machine{instance};
  return RunScript(machine) == ScriptRun::kEnded ? 0 : 1;
}

// decomp: func_800D47C8 (src/BATTLE/BATTLE.PRG/5BF94.c:7540)
int LiveSetEffectScriptSelector(Effects_EffectInstance* instance) {
  return RunOpcode(
      instance, "set selector",
      [](Effects_EffectInstance& /*effect*/, ScriptCursor& cursor) {
        return SetScriptSelector(cursor, Effects_scriptSelector);
      });
}

// decomp: func_800D487C (src/BATTLE/BATTLE.PRG/5BF94.c:7563)
int LiveBranchIfEffectRenderersFinished(Effects_EffectInstance* instance) {
  return RunOpcode(instance, "branch if renderers finished",
                   [](Effects_EffectInstance& effect, ScriptCursor& cursor) {
                     return BranchIfRenderersFinished(
                         effect.context.renderer != nullptr, cursor);
                   });
}

// decomp: func_800D48E4 (src/BATTLE/BATTLE.PRG/5BF94.c:7576)
int LiveBranchIfActionVariantCleared(Effects_EffectInstance* instance) {
  return RunOpcode(
      instance, "branch if action variant cleared",
      [](Effects_EffectInstance& /*effect*/, ScriptCursor& cursor) {
        return BranchIfActionVariantCleared(Effects_actionVariant, cursor);
      });
}

// decomp: func_800D4AB4 (src/BATTLE/BATTLE.PRG/5BF94.c:7625)
int LiveLoadActionVariantRegister(Effects_EffectInstance* instance) {
  return RunOpcode(
      instance, "load action variant register",
      [](Effects_EffectInstance& effect, ScriptCursor& /*cursor*/) {
        auto registers = RegistersOf(effect);
        const OpcodeStep step = LoadActionVariantRegister(
            Effects_actionVariant, VariantValues(), registers);
        StoreRegisters(effect, registers);
        return step;
      });
}

// decomp: func_800D4AEC (src/BATTLE/BATTLE.PRG/5BF94.c:7633)
int LiveSetEffectScriptRegister(Effects_EffectInstance* instance) {
  return RunOpcode(instance, "set register",
                   [](Effects_EffectInstance& effect, ScriptCursor& cursor) {
                     auto registers = RegistersOf(effect);
                     const OpcodeStep step =
                         SetScriptRegister(cursor, registers);
                     StoreRegisters(effect, registers);
                     return step;
                   });
}

// decomp: func_800D4B34 (src/BATTLE/BATTLE.PRG/5BF94.c:7639)
int LiveBranchIfEffectRegisterEqual(Effects_EffectInstance* instance) {
  return RunOpcode(instance, "branch if register equal",
                   [](Effects_EffectInstance& effect, ScriptCursor& cursor) {
                     return BranchIfScriptRegisterEqual(cursor,
                                                        RegistersOf(effect));
                   });
}

// decomp: func_800D4B90 (src/BATTLE/BATTLE.PRG/5BF94.c:7645)
int LiveBranchIfParticleCountEqual(Effects_EffectInstance* instance) {
  return RunOpcode(instance, "branch if particle count equal",
                   [](Effects_EffectInstance& effect, ScriptCursor& cursor) {
                     return BranchIfParticleCountEqual(
                         std::bit_cast<std::uint8_t>(effect.particleCount),
                         cursor);
                   });
}

// decomp: func_800D4D98 (src/BATTLE/BATTLE.PRG/5BF94.c:7705)
int LiveClearScriptActionVariant(Effects_EffectInstance* /*instance*/) {
  return HandlerResult(ClearActionVariant(Effects_actionVariant));
}
}  // namespace VS::Effects
