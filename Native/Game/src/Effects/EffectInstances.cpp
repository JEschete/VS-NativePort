// Hand-maintained (tools/port/owned.csv).
#include "Effects/EffectInstances.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace VS::Effects {
namespace {
// Unlinks `record` from `chain`; false when it was not linked. The originals
// searched an empty chain from a null first record.
template <typename Record>
[[nodiscard]] bool Unlink(RecordChain<Record>& chain, Record& record) {
  Record* cursor = chain.First();
  if (cursor == &record) {
    chain.SetFirst(chain.Next(record));
    return true;
  }
  while (cursor != nullptr) {
    Record* next = chain.Next(*cursor);
    if (next == &record) {
      chain.SetNext(*cursor, chain.Next(record));
      return true;
    }
    cursor = next;
  }
  return false;
}

// func_800CE83C and func_800CF55C (5BF94.c:5114, 5593): the two chains'
// creation, which only the chain head differs in.
[[nodiscard]] Effects_EffectInstance* CreateEffectInstance(
    std::int32_t priority, EffectInstanceChain& chain) {
  Effects_EffectInstance* instance = chain.StartInstance();
  if (instance == nullptr) {
    return nullptr;
  }
  Effects_EffectInstance* previous = nullptr;
  Effects_EffectInstance* cursor = chain.First();
  while (cursor != nullptr && priority >= chain.PriorityOf(*cursor)) {
    previous = cursor;
    cursor = chain.Next(*cursor);
  }
  chain.SetNext(*instance, cursor);
  if (previous == nullptr) {
    chain.SetFirst(instance);
  } else {
    chain.SetNext(*previous, instance);
  }
  return instance;
}
}  // namespace

std::optional<SecondaryEffectPhase> ToSecondaryEffectPhase(std::int32_t value) {
  switch (value) {
    case std::to_underlying(SecondaryEffectPhase::kHeld):
      return SecondaryEffectPhase::kHeld;
    case std::to_underlying(SecondaryEffectPhase::kStarting):
      return SecondaryEffectPhase::kStarting;
    case std::to_underlying(SecondaryEffectPhase::kPlaying):
      return SecondaryEffectPhase::kPlaying;
    default:
      return std::nullopt;
  }
}

// decomp: BATTLE.PRG func_800CF484 (src/BATTLE/BATTLE.PRG/5BF94.c:5548)
void SetSecondaryEffectPhase(std::int16_t& context_phase,
                             SecondaryEffectPhase phase) {
  context_phase = std::to_underlying(phase);
}

// decomp: BATTLE.PRG func_800CE714 (src/BATTLE/BATTLE.PRG/5BF94.c:5074)
void InitEffectInstance(const EffectSpawn& spawn, EffectInstanceStart& record) {
  record.Begin(
      {.priority = static_cast<std::uint8_t>(spawn.priority),
       .script_offset = static_cast<std::uint16_t>(spawn.script_offset),
       .token = spawn.token});
  for (std::size_t transform = 0; transform < kEffectInstanceTransforms;
       ++transform) {
    record.ClearTransform(transform);
  }
  record.TakeEndpoints();
}

// decomp: BATTLE.PRG func_800CE83C (src/BATTLE/BATTLE.PRG/5BF94.c:5114)
Effects_EffectInstance* CreatePrimaryEffectInstance(
    std::int32_t priority, EffectInstanceChain& primaries) {
  return CreateEffectInstance(priority, primaries);
}

// decomp: BATTLE.PRG func_800CF55C (src/BATTLE/BATTLE.PRG/5BF94.c:5593)
Effects_EffectInstance* CreateSecondaryEffectInstance(
    std::int32_t priority, EffectInstanceChain& secondaries) {
  return CreateEffectInstance(priority, secondaries);
}

// decomp: BATTLE.PRG func_800CE8F4 (src/BATTLE/BATTLE.PRG/5BF94.c:5139)
void DestroyPrimaryEffectInstance(Effects_EffectInstance& instance,
                                  EffectInstanceChain& primaries) {
  static_cast<void>(Unlink(primaries, instance));
  primaries.ReleaseParticles(instance);
  RendererChain& renderers = primaries.Renderers(instance);
  Effects_RendererSlot* slot = renderers.First();
  while (slot != nullptr) {
    Effects_RendererSlot* next = renderers.Next(*slot);
    renderers.Run(*slot, RendererCommand::kStop, 0);
    static_cast<void>(DetachEffectRenderer(renderers, *slot));
    slot = next;
  }
  primaries.Free(instance);
}

// decomp: BATTLE.PRG func_800CF614 (src/BATTLE/BATTLE.PRG/5BF94.c:5617)
void DestroySecondaryEffectInstance(Effects_EffectInstance& instance,
                                    EffectInstanceChain& secondaries) {
  static_cast<void>(Unlink(secondaries, instance));
  secondaries.ReleaseParticles(instance);
  secondaries.ReleaseToken(instance);
  secondaries.Free(instance);
}

// decomp: BATTLE.PRG func_800CF694 (src/BATTLE/BATTLE.PRG/5BF94.c:5636)
Effects_RendererSlot* AttachEffectRenderer(RendererChain& renderers,
                                           EffectRenderer renderer,
                                           std::int32_t parameters) {
  Effects_RendererSlot* slot = renderers.NewSlot(renderer);
  if (slot == nullptr) {
    return nullptr;
  }
  renderers.SetNext(*slot, renderers.First());
  renderers.SetFirst(slot);
  renderers.Run(*slot, RendererCommand::kStart, parameters);
  return slot;
}

// decomp: BATTLE.PRG func_800CF70C (src/BATTLE/BATTLE.PRG/5BF94.c:5647)
bool DetachEffectRenderer(RendererChain& renderers,
                          Effects_RendererSlot& slot) {
  if (!Unlink(renderers, slot)) {
    return false;
  }
  renderers.Free(slot);
  return true;
}

// decomp: BATTLE.PRG func_800D4910 (src/BATTLE/BATTLE.PRG/5BF94.c:7582)
void UpdateEffectRenderers(Effects_EffectInstance& instance,
                           EffectRendererUpdates& renderers) {
  Effects_RendererSlot* slot = renderers.FirstRenderer(instance);
  while (slot != nullptr) {
    Effects_RendererSlot* next = renderers.NextRenderer(*slot);
    if (!renderers.UpdateRenderer(*slot)) {
      renderers.DetachRenderer(instance, *slot);
    }
    slot = next;
  }
}
}  // namespace VS::Effects
