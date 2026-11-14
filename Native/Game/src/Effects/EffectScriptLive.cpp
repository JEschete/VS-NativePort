// Hand-maintained (tools/port/owned.csv).
#include "Effects/EffectScriptLive.hpp"

#include <bit>
#include <cstdint>
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

namespace VS::Effects {
namespace {
std::uint64_t refused_calls = 0;
std::uint64_t refused_reads = 0;
std::uint64_t refused_opcodes = 0;

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
}  // namespace VS::Effects
