// Hand-maintained (tools/port/owned.csv).
#include "Effects/EffectScript.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

#include "Effects/Transforms.hpp"

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

// The action effects func_800D49A4 lowers (6-21, 0x21 and 0x24) and the one
// it raises for a register-3 value.
constexpr std::uint16_t kFirstLoweredEffect = 6;
constexpr std::uint16_t kLoweredEffectRun = 16;
constexpr std::uint16_t kLoweredEffect = 0x21;
constexpr std::uint16_t kOtherLoweredEffect = 0x24;
constexpr std::uint16_t kRaisedEffect = 0x38;
constexpr std::uint8_t kRaisedVariant = 0x2B;

[[nodiscard]] bool IsLoweredEffect(std::uint16_t effect_id) {
  return static_cast<std::uint16_t>(effect_id - kFirstLoweredEffect) <
             kLoweredEffectRun ||
         effect_id == kLoweredEffect || effect_id == kOtherLoweredEffect;
}

// The colour opcode's three operand bytes, packed low bits first: a 6-bit
// definition index, then 6-bit red (2 + 4 bits), green (4 + 2 bits) and blue
// curves.
constexpr std::size_t kColorOperandBytes = 3;
using ColorOperands = std::array<std::uint8_t, kColorOperandBytes>;
constexpr std::uint8_t kDefinitionMask = 0x3F;
constexpr unsigned kDefinitionBits = 6;
constexpr unsigned kRedLowBits = 2;
constexpr unsigned kRedHighBits = 4;
constexpr unsigned kGreenLowBits = 4;
constexpr unsigned kGreenHighBits = 2;
constexpr std::uint8_t kRedHighMask = 0xF;
constexpr std::uint8_t kGreenHighMask = 0x3;

[[nodiscard]] ParticleColorCurves UnpackColorCurves(
    const ColorOperands& operands) {
  const auto [first, second, third] = operands;
  return {
      .red =
          static_cast<std::uint8_t>((first >> kDefinitionBits) |
                                    ((second & kRedHighMask) << kRedLowBits)),
      .green = static_cast<std::uint8_t>(
          (second >> kRedHighBits) |
          ((third & kGreenHighMask) << kGreenLowBits)),
      .blue = static_cast<std::uint8_t>(third >> kGreenHighBits),
  };
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

// decomp: func_800D49A4 (src/BATTLE/BATTLE.PRG/5BF94.c:7602)
ScriptStep SetActionEffectVerticalOffset(std::uint16_t effect_id,
                                         const ScriptRegisters& registers,
                                         std::span<std::int32_t> heights) {
  std::optional<std::int32_t> height;
  if (IsLoweredEffect(effect_id)) {
    height = kLoweredEffectHeight;
  } else if (effect_id == kRaisedEffect &&
             std::get<kActionVariantRegister>(registers) == kRaisedVariant) {
    height = kRaisedEffectHeight;
  }
  if (height) std::ranges::fill(heights, *height);
  return ScriptStep::kContinue;
}

// decomp: func_800D4D64 (src/BATTLE/BATTLE.PRG/5BF94.c:7697)
std::optional<std::uint8_t> SourceActorToSignal(std::uint16_t source_type,
                                                std::uint8_t actor) {
  if (source_type != kActorPartEndpoint) return std::nullopt;
  return actor;
}

// decomp: func_800D4F00 (src/BATTLE/BATTLE.PRG/5BF94.c:7749)
OpcodeStep SetParticleColorCurves(ScriptCursor& cursor,
                                  std::span<ParticleColorCurves> curves) {
  ColorOperands operands{};
  for (auto& operand : operands) {
    const auto value = ReadScriptByte(cursor);
    if (!value) return std::unexpected(ScriptRefusal::kOperand);
    operand = *value;
  }
  const std::size_t definition = operands[0] & kDefinitionMask;
  if (definition >= curves.size()) {
    return std::unexpected(ScriptRefusal::kDefinition);
  }
  curves[definition] = UnpackColorCurves(operands);
  return ScriptStep::kContinue;
}

// decomp: func_800D4FB4 (src/BATTLE/BATTLE.PRG/5BF94.c:7762)
OpcodeStep ApplyPairedColorPreset(std::uint16_t preset,
                                  std::span<const PairedColorPreset> presets,
                                  std::span<ParticleColorCurves> curves) {
  if (preset >= presets.size()) {
    return std::unexpected(ScriptRefusal::kColorPreset);
  }
  if (curves.size() < kPairedDefinitions) {
    return std::unexpected(ScriptRefusal::kDefinition);
  }
  std::ranges::copy(presets[preset], curves.begin());
  return ScriptStep::kContinue;
}

// decomp: func_800D5088 (src/BATTLE/BATTLE.PRG/5BF94.c:7785)
OpcodeStep ApplyColorPresetTable(std::uint16_t preset,
                                 std::span<const ColorPresetTable> tables,
                                 std::span<ParticleColorCurves> curves) {
  if (preset >= tables.size()) {
    return std::unexpected(ScriptRefusal::kColorPreset);
  }
  const ColorPresetTable& table = tables[preset];
  const auto names_definition = [&curves](const ColorPresetEntry& entry) {
    return entry.definition < curves.size();
  };
  if (!std::ranges::all_of(table, names_definition)) {
    return std::unexpected(ScriptRefusal::kDefinition);
  }
  for (const ColorPresetEntry& entry : table) {
    curves[entry.definition] = entry.curves;
  }
  return ScriptStep::kContinue;
}

// decomp: func_800D5048 (src/BATTLE/BATTLE.PRG/5BF94.c:7776)
std::optional<std::uint8_t> CompleteChildScript(
    std::optional<std::uint8_t> parent_children) {
  return parent_children.transform([](std::uint8_t children) {
    return static_cast<std::uint8_t>((children - 1) & kChildCountMask);
  });
}

// decomp: func_800CE644 (src/BATTLE/BATTLE.PRG/5BF94.c:5045)
std::string_view DescribeEffectDiagnostic(std::int32_t code) {
  switch (code) {
    case static_cast<std::int32_t>(EffectDiagnostic::kParticlePoolExhausted):
      return "particle pool exhausted";
    case static_cast<std::int32_t>(EffectDiagnostic::kInvalidInstruction):
      return "invalid effect script instruction";
    default:
      return {};
  }
}

// decomp: func_800D5150 (src/BATTLE/BATTLE.PRG/5BF94.c:7799)
ScriptDiagnosticStep ReportInvalidInstruction() {
  return {.diagnostic = EffectDiagnostic::kInvalidInstruction,
          .step = ScriptStep::kYield};
}

// decomp: func_800D48A4 (src/BATTLE/BATTLE.PRG/5BF94.c:7569)
OpcodeStep LoadTimelineImmediate(ScriptCursor& cursor, ScriptEffectHost& host) {
  const auto timeline = ReadScriptByte(cursor);
  if (!timeline) return std::unexpected(ScriptRefusal::kOperand);
  host.LoadTimeline(*timeline);
  cursor.offset = kImmediateTimelineLoop;
  return ScriptStep::kContinue;
}

// decomp: func_800D4DB0 (src/BATTLE/BATTLE.PRG/5BF94.c:7713)
OpcodeStep LoadRegisterTimeline(ScriptCursor& cursor,
                                const ScriptRegisters& registers,
                                bool has_parent, const TimelineRemap& remap,
                                ScriptEffectHost& host) {
  const auto index = ReadScriptByte(cursor);
  if (!index) return std::unexpected(ScriptRefusal::kOperand);
  if (*index >= registers.size()) {
    return std::unexpected(ScriptRefusal::kRegister);
  }
  std::uint8_t timeline = registers.at(*index);
  if (!has_parent && timeline < remap.ids.size()) {
    const std::uint8_t id = remap.ids.at(timeline);
    if (id != 0 && remap.timeline_count && id < *remap.timeline_count) {
      timeline = static_cast<std::uint8_t>(id - 1);
    }
  }
  host.LoadTimeline(timeline);
  return ScriptStep::kContinue;
}

// decomp: func_800D4E5C (src/BATTLE/BATTLE.PRG/5BF94.c:7728)
OpcodeStep TickTimelineFromScript(ScriptCursor& cursor, bool has_timeline,
                                  ScriptEffectHost& host) {
  const auto child_offset = ReadScriptHalfword(cursor);
  if (!child_offset) return std::unexpected(ScriptRefusal::kOperand);
  if (!has_timeline) return std::unexpected(ScriptRefusal::kTimeline);
  host.TickTimeline(*child_offset);
  return ScriptStep::kContinue;
}

// decomp: func_800D4E90 (src/BATTLE/BATTLE.PRG/5BF94.c:7734)
OpcodeStep BranchIfTimelineFinished(std::optional<std::uint32_t> active_tracks,
                                    ScriptCursor& cursor) {
  if (!active_tracks) return std::unexpected(ScriptRefusal::kTimeline);
  return Branch(*active_tracks == 0, cursor);
}

// decomp: func_800D4EC0 (src/BATTLE/BATTLE.PRG/5BF94.c:7740)
ScriptStep FreeScriptTimeline(bool has_timeline, ScriptEffectHost& host) {
  if (has_timeline) host.FreeTimeline();
  return ScriptStep::kContinue;
}

// decomp: func_800D4984 (src/BATTLE/BATTLE.PRG/5BF94.c:7596)
ScriptStep TickScriptTransforms(ScriptEffectHost& host) {
  host.UpdateTransforms();
  return ScriptStep::kContinue;
}

// decomp: func_800D4A94 (src/BATTLE/BATTLE.PRG/5BF94.c:7619)
ScriptStep InitializeScriptTransforms(ScriptEffectHost& host) {
  host.InitializeTransforms();
  return ScriptStep::kContinue;
}

// decomp: func_800D4D44 (src/BATTLE/BATTLE.PRG/5BF94.c:7691)
ScriptStep RefreshParticleEnvironment(ScriptEffectHost& host) {
  host.UpdateParticleEnvironment();
  return ScriptStep::kContinue;
}

// decomp: func_800D4778 (src/BATTLE/BATTLE.PRG/5BF94.c:7527)
OpcodeStep SpawnChildScript(ScriptCursor& cursor, ScriptEffectHost& host) {
  const auto script_offset = ReadScriptHalfword(cursor);
  if (!script_offset) return std::unexpected(ScriptRefusal::kOperand);
  host.SpawnScript(*script_offset);
  return ScriptStep::kContinue;
}

// decomp: func_800D47F4 (src/BATTLE/BATTLE.PRG/5BF94.c:7546)
OpcodeStep AttachScriptRenderer(ScriptCursor& cursor,
                                std::int32_t instance_parameter,
                                ScriptEffectHost& host) {
  const auto index = ReadScriptByte(cursor);
  if (!index) return std::unexpected(ScriptRefusal::kOperand);
  const auto low = ReadScriptHalfword(cursor);
  if (!low) return std::unexpected(ScriptRefusal::kOperand);
  const auto high = ReadScriptHalfword(cursor);
  if (!high) return std::unexpected(ScriptRefusal::kOperand);
  if (*index >= kScriptRenderers) {
    return std::unexpected(ScriptRefusal::kRenderer);
  }
  constexpr unsigned kHighHalfwordShift = 16;
  const std::int32_t parameters =
      instance_parameter != 0
          ? instance_parameter
          : static_cast<std::int32_t>(
                static_cast<std::uint32_t>(*low) |
                (static_cast<std::uint32_t>(*high) << kHighHalfwordShift));
  host.AttachRenderer(*index, parameters);
  return ScriptStep::kContinue;
}

// decomp: func_800D4910 (src/BATTLE/BATTLE.PRG/5BF94.c:7582)
ScriptStep TickScriptRenderers(ScriptEffectHost& host) {
  host.UpdateRenderers();
  return ScriptStep::kContinue;
}

// decomp: func_800D4BD0 (src/BATTLE/BATTLE.PRG/5BF94.c:7651)
OpcodeStep EmitRegisterParticles(ScriptCursor& cursor,
                                 const ScriptRegisters& registers,
                                 ScriptEffectHost& host) {
  const auto index = ReadScriptByte(cursor);
  if (!index) return std::unexpected(ScriptRefusal::kOperand);
  if (*index >= registers.size()) {
    return std::unexpected(ScriptRefusal::kRegister);
  }
  host.EmitParticles(registers.at(*index));
  return ScriptStep::kContinue;
}
}  // namespace VS::Effects
