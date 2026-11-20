// Hand-maintained (tools/port/owned.csv).
#include "Effects/EffectScriptLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Effects/ActionEffectCallbacksLive.hpp"
#include "Effects/ActionRequest.hpp"
#include "Effects/EffectInstancesLive.hpp"
#include "Effects/EffectResources.hpp"
#include "Effects/EffectResourcesLive.hpp"
#include "Effects/EffectScript.hpp"
#include "Effects/Package.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/ParticleEmitterLive.hpp"
#include "Effects/Particles.hpp"
#include "Effects/Runtime.hpp"
#include "Effects/SpawnRequest.hpp"
#include "Effects/Timeline.hpp"
#include "Effects/TimelinePlaybackLive.hpp"
#include "Effects/Transforms.hpp"
#include "Effects/TransformsLive.hpp"
#include "Effects/TransformsLiveState.hpp"
#include "Legacy/Psx.hpp"

static_assert(std::extent_v<decltype(VS::Effects::actionVariantValues)> >=
              VS::Effects::kActionVariantValues);
static_assert(
    std::tuple_size_v<decltype(VS::Effects::LiveEffectInstance::transforms)> ==
    VS::Effects::kEffectTransformSlots);

namespace VS::Effects {
// decomp: D_800EC330 (src/BATTLE/BATTLE.PRG/5BF94.c:402); contents: module
// image (ModuleData.cpp).
LiveColorPresets<kPairedDefinitions> pairedParticleColorPresets{};
// decomp: D_800EC368 (src/BATTLE/BATTLE.PRG/5BF94.c:403); contents: module
// image (ModuleData.cpp).
LiveColorPresets<kColorPresetEntries> particleColorPresetTable{};

namespace {
std::uint64_t refusedCalls = 0;
std::uint64_t refusedReads = 0;
std::uint64_t refusedOpcodes = 0;
std::uint64_t refusedRegisters = 0;
std::uint64_t refusedDefinitions = 0;
std::uint64_t refusedPresets = 0;
std::uint64_t refusedTimelines = 0;
std::uint64_t refusedRenderers = 0;
std::uint64_t unmappedTimelines = 0;
std::uint64_t clampedTransforms = 0;
std::uint64_t wrappedChildren = 0;
std::uint64_t reportedDiagnostics = 0;

[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

void ReportRefusal(const char* entry) {
  if (ShouldReport(refusedCalls)) {
    VS::Core::LogWarn(
        "effects",
        "effect script: {} refused: no effect instance ({} refusals)", entry,
        refusedCalls);
  }
}

[[nodiscard]] ScriptCursor CursorOf(const LiveEffectInstance& instance) {
  return {.program = LiveEffectPrograms(instance.script),
          .offset = instance.script_offset};
}

// Stores the cursor's offset back, reporting where a read was refused.
void Settle(LiveEffectInstance& instance, const ScriptCursor& cursor,
            bool refused) {
  instance.script_offset = cursor.offset;
  if (refused && ShouldReport(refusedReads)) {
    VS::Core::LogWarn(
        "effects",
        "effect script: read at {:#x} refused: program has {} bytes "
        "({} refusals)",
        cursor.offset, cursor.program.size(), refusedReads);
  }
}

template <typename Value, typename Read>
[[nodiscard]] Value ReadOperand(LiveEffectInstance* instance, const char* entry,
                                Read read) {
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
[[nodiscard]] int RunStep(LiveEffectInstance* instance, const char* entry,
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

// Reports a refused operand opcode; Settle reports refused reads.
void ReportOpcodeRefusal(const char* entry, ScriptRefusal refusal,
                         std::uint16_t offset) {
  switch (refusal) {
    case ScriptRefusal::kOperand:
      return;
    case ScriptRefusal::kRegister:
      if (ShouldReport(refusedRegisters)) {
        VS::Core::LogWarn(
            "effects",
            "effect script: {} before {:#x} names a register past the "
            "{} registers ({} refusals)",
            entry, offset, kScriptRegisters, refusedRegisters);
      }
      return;
    case ScriptRefusal::kDefinition:
      if (ShouldReport(refusedDefinitions)) {
        VS::Core::LogWarn("effects",
                          "effect script: {} before {:#x} names a particle "
                          "definition past the package's {} ({} refusals)",
                          entry, offset, LiveParticleDefinitionCount(),
                          refusedDefinitions);
      }
      return;
    case ScriptRefusal::kColorPreset:
      if (ShouldReport(refusedPresets)) {
        VS::Core::LogWarn(
            "effects",
            "effect script: {} before {:#x} names a colour preset past "
            "the {} presets ({} refusals)",
            entry, offset, kParticleColorPresets, refusedPresets);
      }
      return;
    case ScriptRefusal::kTimeline:
      if (ShouldReport(refusedTimelines)) {
        VS::Core::LogWarn("effects",
                          "effect script: {} before {:#x} needs a timeline the "
                          "instance has not loaded ({} refusals)",
                          entry, offset, refusedTimelines);
      }
      return;
    case ScriptRefusal::kRenderer:
      if (ShouldReport(refusedRenderers)) {
        VS::Core::LogWarn(
            "effects",
            "effect script: {} before {:#x} names a renderer past the "
            "{} script renderers ({} refusals)",
            entry, offset, kScriptRenderers, refusedRenderers);
      }
      return;
  }
}

// Runs an operand opcode over the instance's cursor; a refused operand,
// register, definition or colour preset ends the instance.
template <typename Opcode>
[[nodiscard]] int RunOpcode(LiveEffectInstance* instance, const char* entry,
                            Opcode opcode) {
  if (instance == nullptr) {
    ReportRefusal(entry);
    return HandlerResult(ScriptStep::kEnd);
  }
  auto cursor = CursorOf(*instance);
  const OpcodeStep step = opcode(*instance, cursor);
  const auto refusal = step ? std::nullopt : std::optional{step.error()};
  Settle(*instance, cursor, refusal == ScriptRefusal::kOperand);
  if (refusal) ReportOpcodeRefusal(entry, *refusal, cursor.offset);
  return HandlerResult(step.value_or(ScriptStep::kEnd));
}

[[nodiscard]] ScriptRegisters RegistersOf(const LiveEffectInstance& instance) {
  ScriptRegisters registers{};
  std::ranges::transform(instance.registers, registers.begin(), [](char value) {
    return std::bit_cast<std::uint8_t>(value);
  });
  return registers;
}

void StoreRegisters(LiveEffectInstance& instance,
                    const ScriptRegisters& registers) {
  std::ranges::transform(
      registers, std::begin(instance.registers),
      [](std::uint8_t value) { return std::bit_cast<char>(value); });
}

[[nodiscard]] ActionVariantValues VariantValues() {
  ActionVariantValues values{};
  std::ranges::transform(
      std::span{actionVariantValues}.first<kActionVariantValues>(),
      values.begin(),
      [](char value) { return std::bit_cast<std::uint8_t>(value); });
  return values;
}

// The package's transforms, up to the instance's slots.
[[nodiscard]] std::size_t PackageTransformCount() {
  const std::optional<PackageParameters> parameters = ReadPackageParameters(
      LiveBoundPackageBlock(PackageBlockType::kParameters));
  if (!parameters) return 0;
  const std::size_t counted = parameters->transform_count;
  if (counted > kEffectTransformSlots && ShouldReport(clampedTransforms)) {
    VS::Core::LogWarn(
        "effects",
        "effect script: block 9 counts {} transforms; heights apply to "
        "the instance's {} ({} clamps)",
        counted, kEffectTransformSlots, clampedTransforms);
  }
  return std::min(counted, kEffectTransformSlots);
}

[[nodiscard]] std::span<LiveParticleDefinition> PackageDefinitions() {
  if (VS::Effects::package == nullptr ||
      VS::Effects::package->particle_definitions == nullptr) {
    return {};
  }
  return {VS::Effects::package->particle_definitions->Definitions(),
          LiveParticleDefinitionCount()};
}

// Runs a colour opcode over the package's definitions' curves, storing them
// back unless it refused.
template <typename Apply>
[[nodiscard]] OpcodeStep WithDefinitionCurves(Apply apply) {
  const std::span definitions = PackageDefinitions();
  std::vector<ParticleColorCurves> curves(definitions.size());
  std::ranges::transform(definitions, curves.begin(),
                         [](const LiveParticleDefinition& definition) {
                           return ParticleColorCurves{
                               .red = definition.r_curve,
                               .green = definition.g_curve,
                               .blue = definition.b_curve};
                         });
  const OpcodeStep step = apply(std::span{curves});
  if (!step) return step;
  for (auto&& [definition, curve] : std::views::zip(definitions, curves)) {
    definition.r_curve = curve.red;
    definition.g_curve = curve.green;
    definition.b_curve = curve.blue;
  }
  return step;
}

using PresetBytes = std::span<const std::uint8_t, kColorPresetEntryBytes>;

// A preset entry's red, green and blue curves; the fourth byte is padding in
// the paired presets and the definition index in the table.
[[nodiscard]] ParticleColorCurves CurvesOf(PresetBytes bytes) {
  return {.red = bytes[0], .green = bytes[1], .blue = bytes[2]};
}

// D_800EC330's seven paired presets.
[[nodiscard]] std::array<PairedColorPreset, kParticleColorPresets>
PairedPresets() {
  std::array<PairedColorPreset, kParticleColorPresets> presets{};
  std::ranges::transform(
      pairedParticleColorPresets, presets.begin(), [](const auto& preset) {
        PairedColorPreset paired{};
        std::ranges::transform(preset, paired.begin(), [](const auto& bytes) {
          return CurvesOf(PresetBytes{bytes});
        });
        return paired;
      });
  return presets;
}

// D_800EC368's seven preset tables.
[[nodiscard]] std::array<ColorPresetTable, kParticleColorPresets>
PresetTables() {
  std::array<ColorPresetTable, kParticleColorPresets> tables{};
  std::ranges::transform(
      particleColorPresetTable, tables.begin(), [](const auto& preset) {
        ColorPresetTable table{};
        std::ranges::transform(preset, table.begin(), [](const auto& bytes) {
          const PresetBytes entry{bytes};
          return ColorPresetEntry{.curves = CurvesOf(entry),
                                  .definition = entry.back()};
        });
        return table;
      });
  return tables;
}

// The timeline ids and timeline count a root instance's register timelines
// map through. A package without block 9 maps none.
[[nodiscard]] TimelineRemap PackageTimelineRemap() {
  TimelineRemap remap{};
  if (const auto parameters = ReadPackageParameters(
          LiveBoundPackageBlock(PackageBlockType::kParameters))) {
    remap.ids = parameters->timeline_ids;
  } else if (ShouldReport(unmappedTimelines)) {
    VS::Core::LogWarn("effects",
                      "effect script: the package binds no block 9; register "
                      "timelines load unmapped ({} times)",
                      unmappedTimelines);
  }
  const std::span timelines =
      LiveBoundPackageBlock(PackageBlockType::kTimelines);
  if (timelines.size() >= sizeof(LiveTimelineTable::count)) {
    remap.timeline_count =
        reinterpret_cast<const LiveTimelineTable*>(timelines.data())->count;
  }
  return remap;
}

// The instance's timeline, transforms, renderers, particles and children.
class InstanceEffectHost final : public ScriptEffectHost {
 public:
  explicit InstanceEffectHost(LiveEffectInstance& instance)
      : instance_(&instance) {}

  void LoadTimeline(std::uint8_t index) override {
    LiveLoadEffectTimeline(instance_, index, 0);
  }
  void TickTimeline(std::uint16_t child_offset) override {
    LiveTickEffectTimeline(instance_, child_offset);
  }
  void FreeTimeline() override {
    Core::Memory::FreeLiveFromHighEnd(instance_->context.timeline);
    instance_->context.timeline = nullptr;
  }
  void UpdateTransforms() override { LiveUpdateEffectTransforms(instance_); }
  void InitializeTransforms() override {
    LiveInitializeTransformStates(instance_);
  }
  void UpdateParticleEnvironment() override {
    LiveUpdateParticleGravityAndDrag();
  }
  // The original set only the source, script and offset, leaving the other
  // endpoints, priority, parent and token as whatever its stack held; the
  // child takes the instance's endpoints, priority and token, without a
  // parent.
  void SpawnScript(std::uint16_t script_offset) override {
    LiveEffectInstanceTargets& context = instance_->context;
    LiveEffectSpawnRequest spawn{};
    spawn.source = &context.source;
    spawn.emit_from = &context.emit_from;
    spawn.target = &context.target;
    spawn.emit_to = &context.emit_to;
    spawn.priority = static_cast<std::uint8_t>(instance_->priority);
    spawn.script = instance_->script;
    spawn.script_offset = script_offset;
    spawn.token = static_cast<std::uint8_t>(instance_->token);
    (void)LiveCreatePrimaryEffectInstance(&spawn);
  }
  void AttachRenderer(std::uint8_t index, std::int32_t parameters) override {
    (void)LiveAttachEffectRenderer(instance_, std::span(scriptRenderers)[index],
                                   parameters);
  }
  void UpdateRenderers() override { LiveUpdateEffectRenderers(instance_); }
  void EmitParticles(std::uint8_t event) override {
    EmitLiveParticleEvent(instance_, event, 0, 0, nullptr);
  }

 private:
  LiveEffectInstance* instance_;
};

// Runs an operand-free opcode over the instance's effect runtime.
template <typename Step>
[[nodiscard]] int RunHostStep(LiveEffectInstance* instance, const char* entry,
                              Step step) {
  if (instance == nullptr) {
    ReportRefusal(entry);
    return HandlerResult(ScriptStep::kEnd);
  }
  InstanceEffectHost host{*instance};
  return HandlerResult(step(*instance, host));
}

// The instance a frame's run reads and dispatches through the handler table.
class InstanceMachine final : public ScriptMachine {
 public:
  explicit InstanceMachine(LiveEffectInstance* instance)
      : instance_(instance) {}

  std::optional<std::uint8_t> NextOpcode() override {
    auto cursor = CursorOf(*instance_);
    const auto opcode = ReadScriptByte(cursor);
    Settle(*instance_, cursor, !opcode);
    return opcode;
  }

  std::optional<ScriptStep> Execute(std::uint8_t opcode) override {
    const std::span handlers{scriptOpcodeHandlers};
    if (opcode >= handlers.size() || handlers[opcode] == nullptr) {
      if (ShouldReport(refusedOpcodes)) {
        VS::Core::LogWarn(
            "effects",
            "effect script: opcode {:#x} at {:#x} has no handler ({} "
            "refusals)",
            opcode, static_cast<std::uint16_t>(instance_->script_offset - 1),
            refusedOpcodes);
      }
      return std::nullopt;
    }
    return StepFromHandlerResult(handlers[opcode](instance_));
  }

 private:
  LiveEffectInstance* instance_;
};
}  // namespace

// decomp: func_800D46DC (src/BATTLE/BATTLE.PRG/5BF94.c:7506)
void LiveBranchEffectScriptIf(int condition, LiveEffectInstance* instance) {
  if (instance == nullptr) {
    ReportRefusal("branch");
    return;
  }
  auto cursor = CursorOf(*instance);
  const bool moved = BranchScriptIf(condition != 0, cursor);
  Settle(*instance, cursor, !moved);
}

// decomp: func_800D4720 (src/BATTLE/BATTLE.PRG/5BF94.c:7515)
int LiveYieldEffectScriptAtOffset(LiveEffectInstance* instance) {
  return RunStep(instance, "yield",
                 [](ScriptCursor& cursor) { return YieldScriptAt(cursor); });
}

// decomp: func_800D474C (src/BATTLE/BATTLE.PRG/5BF94.c:7521)
int LiveJumpEffectScript(LiveEffectInstance* instance) {
  return RunStep(instance, "jump",
                 [](ScriptCursor& cursor) { return JumpScript(cursor); });
}

// decomp: func_800D47C0 (src/BATTLE/BATTLE.PRG/5BF94.c:7538)
int LiveEndEffectScript(LiveEffectInstance* /*instance*/) {
  return HandlerResult(EndScript());
}

// decomp: func_800D4DA8 (src/BATTLE/BATTLE.PRG/5BF94.c:7711)
int LiveContinueEffectScript(LiveEffectInstance* /*instance*/) {
  return HandlerResult(ContinueScript());
}

// decomp: func_800D5170 (src/BATTLE/BATTLE.PRG/5BF94.c:7804)
std::uint8_t LiveReadScriptByte(LiveEffectInstance* instance) {
  return ReadOperand<std::uint8_t>(
      instance, "read byte",
      [](ScriptCursor& cursor) { return ReadScriptByte(cursor); });
}

// decomp: func_800D5198 (src/BATTLE/BATTLE.PRG/5BF94.c:7806)
std::uint16_t LiveReadScriptHalfword(LiveEffectInstance* instance) {
  return ReadOperand<std::uint16_t>(
      instance, "read halfword",
      [](ScriptCursor& cursor) { return ReadScriptHalfword(cursor); });
}

// decomp: func_800D51D8 (src/BATTLE/BATTLE.PRG/5BF94.c:7813)
int LiveRunEffectScript(LiveEffectInstance* instance) {
  if (instance == nullptr) {
    ReportRefusal("run");
    return 0;
  }
  InstanceMachine machine{instance};
  return RunScript(machine) == ScriptRun::kEnded ? 0 : 1;
}

// decomp: func_800D47C8 (src/BATTLE/BATTLE.PRG/5BF94.c:7540)
int LiveSetEffectScriptSelector(LiveEffectInstance* instance) {
  return RunOpcode(instance, "set selector",
                   [](LiveEffectInstance& /*effect*/, ScriptCursor& cursor) {
                     return SetScriptSelector(cursor, scriptSelector);
                   });
}

// decomp: func_800D487C (src/BATTLE/BATTLE.PRG/5BF94.c:7563)
int LiveBranchIfEffectRenderersFinished(LiveEffectInstance* instance) {
  return RunOpcode(instance, "branch if renderers finished",
                   [](LiveEffectInstance& effect, ScriptCursor& cursor) {
                     return BranchIfRenderersFinished(
                         effect.context.renderer != nullptr, cursor);
                   });
}

// decomp: func_800D48E4 (src/BATTLE/BATTLE.PRG/5BF94.c:7576)
int LiveBranchIfActionVariantCleared(LiveEffectInstance* instance) {
  return RunOpcode(instance, "branch if action variant cleared",
                   [](LiveEffectInstance& /*effect*/, ScriptCursor& cursor) {
                     return BranchIfActionVariantCleared(actionVariant, cursor);
                   });
}

// decomp: func_800D4AB4 (src/BATTLE/BATTLE.PRG/5BF94.c:7625)
int LiveLoadActionVariantRegister(LiveEffectInstance* instance) {
  return RunOpcode(instance, "load action variant register",
                   [](LiveEffectInstance& effect, ScriptCursor& /*cursor*/) {
                     auto registers = RegistersOf(effect);
                     const OpcodeStep step = LoadActionVariantRegister(
                         actionVariant, VariantValues(), registers);
                     StoreRegisters(effect, registers);
                     return step;
                   });
}

// decomp: func_800D4AEC (src/BATTLE/BATTLE.PRG/5BF94.c:7633)
int LiveSetEffectScriptRegister(LiveEffectInstance* instance) {
  return RunOpcode(instance, "set register",
                   [](LiveEffectInstance& effect, ScriptCursor& cursor) {
                     auto registers = RegistersOf(effect);
                     const OpcodeStep step =
                         SetScriptRegister(cursor, registers);
                     StoreRegisters(effect, registers);
                     return step;
                   });
}

// decomp: func_800D4B34 (src/BATTLE/BATTLE.PRG/5BF94.c:7639)
int LiveBranchIfEffectRegisterEqual(LiveEffectInstance* instance) {
  return RunOpcode(instance, "branch if register equal",
                   [](LiveEffectInstance& effect, ScriptCursor& cursor) {
                     return BranchIfScriptRegisterEqual(cursor,
                                                        RegistersOf(effect));
                   });
}

// decomp: func_800D4B90 (src/BATTLE/BATTLE.PRG/5BF94.c:7645)
int LiveBranchIfParticleCountEqual(LiveEffectInstance* instance) {
  return RunOpcode(instance, "branch if particle count equal",
                   [](LiveEffectInstance& effect, ScriptCursor& cursor) {
                     return BranchIfParticleCountEqual(
                         std::bit_cast<std::uint8_t>(effect.particle_count),
                         cursor);
                   });
}

// decomp: func_800D4D98 (src/BATTLE/BATTLE.PRG/5BF94.c:7705)
int LiveClearScriptActionVariant(LiveEffectInstance* /*instance*/) {
  return HandlerResult(ClearActionVariant(actionVariant));
}

// decomp: func_800D49A4 (src/BATTLE/BATTLE.PRG/5BF94.c:7602)
int LiveSetActionEffectVerticalOffset(LiveEffectInstance* instance) {
  if (instance == nullptr) {
    ReportRefusal("vertical offset");
    return HandlerResult(ScriptStep::kEnd);
  }
  const std::span slots =
      std::span{instance->transforms}.first(PackageTransformCount());
  std::array<std::int32_t, kEffectTransformSlots> heights{};
  std::ranges::transform(
      slots, heights.begin(),
      [](const LiveTransformSlot& slot) { return slot.position.vy; });
  const ScriptStep step = SetActionEffectVerticalOffset(
      std::bit_cast<std::uint16_t>(instance->context.effect_id),
      RegistersOf(*instance), std::span{heights}.first(slots.size()));
  for (auto&& [slot, height] : std::views::zip(slots, heights)) {
    slot.position.vy = height;
  }
  return HandlerResult(step);
}

// decomp: func_800D4D64 (src/BATTLE/BATTLE.PRG/5BF94.c:7697)
int LiveSignalSourceActorFromScript(LiveEffectInstance* instance) {
  if (instance == nullptr) {
    ReportRefusal("signal source actor");
    return HandlerResult(ScriptStep::kEnd);
  }
  const LiveActionEffectTarget& source = instance->context.source;
  if (const auto actor = SourceActorToSignal(
          source.type, static_cast<std::uint8_t>(source.Actor().actor))) {
    VS::Effects::LiveSignalActorEffectAnimation(*actor);
  }
  return HandlerResult(ContinueScript());
}

// decomp: func_800D4F00 (src/BATTLE/BATTLE.PRG/5BF94.c:7749)
int LiveSetParticleColorCurvesFromScript(LiveEffectInstance* instance) {
  return RunOpcode(instance, "set particle colour curves",
                   [](LiveEffectInstance& /*effect*/, ScriptCursor& cursor) {
                     return WithDefinitionCurves(
                         [&cursor](std::span<ParticleColorCurves> curves) {
                           return SetParticleColorCurves(cursor, curves);
                         });
                   });
}

// decomp: func_800D4FB4 (src/BATTLE/BATTLE.PRG/5BF94.c:7762)
int LiveApplyPairedParticleColorPreset(LiveEffectInstance* instance) {
  return RunOpcode(
      instance, "apply paired colour preset",
      [](LiveEffectInstance& effect, ScriptCursor& /*cursor*/) {
        const auto presets = PairedPresets();
        return WithDefinitionCurves(
            [&effect, &presets](std::span<ParticleColorCurves> curves) {
              return ApplyPairedColorPreset(effect.context.target.color_preset,
                                            presets, curves);
            });
      });
}

// decomp: func_800D5048 (src/BATTLE/BATTLE.PRG/5BF94.c:7776)
int LiveCompleteChildEffectScript(LiveEffectInstance* instance) {
  if (instance == nullptr) {
    ReportRefusal("complete child");
    return HandlerResult(ScriptStep::kEnd);
  }
  LiveEffectInstance* parent = instance->parent;
  if (parent == nullptr) return HandlerResult(ContinueScript());
  const auto children = static_cast<std::uint8_t>(parent->child_count);
  if (children == 0 && ShouldReport(wrappedChildren)) {
    VS::Core::LogWarn(
        "effects",
        "effect script: a child completed with its parent counting no "
        "running children; the count wraps to {} ({} wraps)",
        kChildCountMask, wrappedChildren);
  }
  parent->child_count = CompleteChildScript(children).value_or(children);
  return HandlerResult(ContinueScript());
}

// decomp: func_800D5088 (src/BATTLE/BATTLE.PRG/5BF94.c:7785)
int LiveApplyParticleColorPresetTable(LiveEffectInstance* instance) {
  return RunOpcode(
      instance, "apply colour preset table",
      [](LiveEffectInstance& effect, ScriptCursor& /*cursor*/) {
        const auto tables = PresetTables();
        return WithDefinitionCurves(
            [&effect, &tables](std::span<ParticleColorCurves> curves) {
              return ApplyColorPresetTable(effect.context.target.color_preset,
                                           tables, curves);
            });
      });
}

// decomp: func_800D5150 (src/BATTLE/BATTLE.PRG/5BF94.c:7799)
int LiveReportInvalidEffectInstruction(LiveEffectInstance* /*instance*/) {
  const ScriptDiagnosticStep report = ReportInvalidInstruction();
  LiveReportEffectDiagnostic(static_cast<int>(report.diagnostic));
  return HandlerResult(report.step);
}

// decomp: func_800D48A4 (src/BATTLE/BATTLE.PRG/5BF94.c:7569)
int LiveLoadEffectTimelineImmediate(LiveEffectInstance* instance) {
  return RunOpcode(instance, "load timeline",
                   [](LiveEffectInstance& effect, ScriptCursor& cursor) {
                     InstanceEffectHost host{effect};
                     return LoadTimelineImmediate(cursor, host);
                   });
}

// decomp: func_800D4DB0 (src/BATTLE/BATTLE.PRG/5BF94.c:7713)
int LiveLoadEffectTimelineFromRegister(LiveEffectInstance* instance) {
  return RunOpcode(instance, "load register timeline",
                   [](LiveEffectInstance& effect, ScriptCursor& cursor) {
                     InstanceEffectHost host{effect};
                     const bool has_parent = effect.parent != nullptr;
                     return LoadRegisterTimeline(
                         cursor, RegistersOf(effect), has_parent,
                         has_parent ? TimelineRemap{} : PackageTimelineRemap(),
                         host);
                   });
}

// decomp: func_800D4E5C (src/BATTLE/BATTLE.PRG/5BF94.c:7728)
int LiveTickEffectTimelineFromScript(LiveEffectInstance* instance) {
  return RunOpcode(instance, "tick timeline",
                   [](LiveEffectInstance& effect, ScriptCursor& cursor) {
                     InstanceEffectHost host{effect};
                     return TickTimelineFromScript(
                         cursor, effect.context.timeline != nullptr, host);
                   });
}

// decomp: func_800D4E90 (src/BATTLE/BATTLE.PRG/5BF94.c:7734)
int LiveBranchIfEffectTimelineFinished(LiveEffectInstance* instance) {
  return RunOpcode(
      instance, "branch if timeline finished",
      [](LiveEffectInstance& effect, ScriptCursor& cursor) {
        const LiveEffectTimeline* timeline = effect.context.timeline;
        const auto active_tracks = timeline == nullptr
                                       ? std::nullopt
                                       : std::optional{timeline->active_tracks};
        return BranchIfTimelineFinished(active_tracks, cursor);
      });
}

// decomp: func_800D4EC0 (src/BATTLE/BATTLE.PRG/5BF94.c:7740)
int LiveFreeEffectScriptTimeline(LiveEffectInstance* instance) {
  return RunHostStep(instance, "free timeline",
                     [](LiveEffectInstance& effect, ScriptEffectHost& host) {
                       return FreeScriptTimeline(
                           effect.context.timeline != nullptr, host);
                     });
}

// decomp: func_800D4984 (src/BATTLE/BATTLE.PRG/5BF94.c:7596)
int LiveTickEffectTransforms(LiveEffectInstance* instance) {
  return RunHostStep(
      instance, "tick transforms",
      [](LiveEffectInstance& /*effect*/, ScriptEffectHost& host) {
        return TickScriptTransforms(host);
      });
}

// decomp: func_800D4A94 (src/BATTLE/BATTLE.PRG/5BF94.c:7619)
int LiveInitializeScriptEffectTransforms(LiveEffectInstance* instance) {
  return RunHostStep(
      instance, "initialize transforms",
      [](LiveEffectInstance& /*effect*/, ScriptEffectHost& host) {
        return InitializeScriptTransforms(host);
      });
}

// decomp: func_800D4D44 (src/BATTLE/BATTLE.PRG/5BF94.c:7691)
int LiveRefreshScriptParticleEnvironment(LiveEffectInstance* instance) {
  return RunHostStep(
      instance, "refresh particle environment",
      [](LiveEffectInstance& /*effect*/, ScriptEffectHost& host) {
        return RefreshParticleEnvironment(host);
      });
}

// decomp: func_800D4778 (src/BATTLE/BATTLE.PRG/5BF94.c:7527)
int LiveSpawnEffectScript(LiveEffectInstance* instance) {
  return RunOpcode(instance, "spawn script",
                   [](LiveEffectInstance& effect, ScriptCursor& cursor) {
                     InstanceEffectHost host{effect};
                     return SpawnChildScript(cursor, host);
                   });
}

// decomp: func_800D47F4 (src/BATTLE/BATTLE.PRG/5BF94.c:7546)
int LiveAttachScriptRenderer(LiveEffectInstance* instance) {
  return RunOpcode(instance, "attach renderer",
                   [](LiveEffectInstance& effect, ScriptCursor& cursor) {
                     InstanceEffectHost host{effect};
                     return AttachScriptRenderer(
                         cursor, effect.context.parameter, host);
                   });
}

// decomp: func_800D4910 (src/BATTLE/BATTLE.PRG/5BF94.c:7582)
int LiveTickEffectRenderers(LiveEffectInstance* instance) {
  return RunHostStep(
      instance, "tick renderers",
      [](LiveEffectInstance& /*effect*/, ScriptEffectHost& host) {
        return TickScriptRenderers(host);
      });
}

// decomp: func_800D4BD0 (src/BATTLE/BATTLE.PRG/5BF94.c:7651)
int LiveEmitParticlesFromScriptRegister(LiveEffectInstance* instance) {
  return RunOpcode(instance, "emit register particles",
                   [](LiveEffectInstance& effect, ScriptCursor& cursor) {
                     InstanceEffectHost host{effect};
                     return EmitRegisterParticles(cursor, RegistersOf(effect),
                                                  host);
                   });
}

// decomp: func_800CE644 (src/BATTLE/BATTLE.PRG/5BF94.c:5045)
void LiveReportEffectDiagnostic(int code) {
  if (!ShouldReport(reportedDiagnostics)) return;
  const std::string_view description = DescribeEffectDiagnostic(code);
  VS::Core::LogWarn("effects", "effect diagnostic {:#x}: {} ({} reports)", code,
                    description.empty() ? "unknown code" : description,
                    reportedDiagnostics);
}
}  // namespace VS::Effects
