// Hand-maintained (tools/port/owned.csv).
#include "Effects/EffectInstancesLive.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Effects/EffectInstances.hpp"
#include "Effects/ParticlePoolLive.hpp"
#include "Effects/Particles.hpp"
#include "Effects/PrimaryEffectLive.hpp"
#include "Effects/Runtime.h"
#include "Effects/SpawnRequest.hpp"

namespace VS::Effects {
namespace {
using Instance = Effects_EffectInstance;
using Slot = Effects_RendererSlot;
using Spawn = LiveEffectSpawnRequest;
static_assert(std::extent_v<decltype(Instance::transforms)> ==
              kEffectInstanceTransforms);
static_assert(std::is_same_v<Effects_EffectRenderer, EffectRenderer>);

std::uint64_t refused_calls = 0;

void Refuse(std::string_view call, std::string_view reason) {
  if (std::has_single_bit(++refused_calls)) {
    VS::Core::LogWarn("effects",
                      "effect instance: {} refused: {} ({} refusals)", call,
                      reason, refused_calls);
  }
}

// Whether the spawn request and its four endpoints are present.
[[nodiscard]] bool Usable(const Spawn* spawn, std::string_view call) {
  if (spawn == nullptr || spawn->source == nullptr ||
      spawn->emit_from == nullptr || spawn->target == nullptr ||
      spawn->emit_to == nullptr) {
    Refuse(call, "no spawn request or endpoint");
    return false;
  }
  return true;
}

class LiveStart final : public EffectInstanceStart {
 public:
  LiveStart(const Spawn& spawn, Instance& instance)
      : spawn_(&spawn), instance_(&instance) {}

  void Begin(const EffectInstanceIdentity& identity) override {
    instance_->parent = spawn_->parent;
    instance_->particleCount = 0;
    instance_->priority = static_cast<char>(identity.priority);
    instance_->scriptOffset = identity.script_offset;
    instance_->script = spawn_->script;
    std::ranges::fill(instance_->registers, char{0});
    instance_->token = identity.token;
    instance_->childCount = 0;
    instance_->frame = 0;
    instance_->particles = nullptr;
  }
  void ClearTransform(std::size_t transform) override {
    // 5BF94.c:4655-4666 clears all but the position's x, y and z.
    LiveTransformSlot& slot = std::span(instance_->transforms)[transform];
    slot.rotation = {};
    slot.flags = 0;
    slot.reserved_8 = 0;
    slot.reserved_c = 0;
    slot.frame = 0;
    slot.reserved_10 = {};
    slot.position.pad = 0;
    slot.reserved_2c = {};
    slot.actor = 0;
    slot.bone = 0;
    slot.target_actor = 0;
    slot.target_bone = 0;
  }
  void TakeEndpoints() override {
    LiveEffectInstanceTargets& context = instance_->context;
    context.source = *spawn_->source;
    context.emit_from = *spawn_->emit_from;
    context.target = *spawn_->target;
    context.emit_to = *spawn_->emit_to;
    context.timeline = nullptr;
    context.renderer = nullptr;
    context.parameter = 0;
  }

 private:
  const Spawn* spawn_;
  Instance* instance_;
};

[[nodiscard]] EffectSpawn SpawnOf(const Spawn& spawn) {
  return {.priority = spawn.priority,
          .script_offset = spawn.script_offset,
          .token = spawn.token};
}

void Start(const Spawn& spawn, Instance& instance) {
  LiveStart start(spawn, instance);
  InitEffectInstance(SpawnOf(spawn), start);
}

// The renderers of the instance it targets.
class LiveRendererChain final : public RendererChain {
 public:
  LiveRendererChain() = default;
  explicit LiveRendererChain(Instance& instance) : instance_(&instance) {}

  void Target(Instance& instance) { instance_ = &instance; }

  Slot* First() override { return instance_->context.renderer; }
  void SetFirst(Slot* slot) override { instance_->context.renderer = slot; }
  Slot* Next(Slot& slot) override { return slot.next; }
  void SetNext(Slot& slot, Slot* next) override { slot.next = next; }
  Slot* NewSlot(EffectRenderer renderer) override {
    void* storage = Core::Memory::AllocateLiveFromHighEnd(sizeof(Slot));
    if (storage == nullptr) {
      return nullptr;
    }
    return std::construct_at(
        static_cast<Slot*>(storage),
        Slot{.next = nullptr, .renderer = renderer, .rendererState = nullptr});
  }
  void Run(Slot& slot, RendererCommand command,
           std::int32_t parameters) override {
    if (slot.renderer == nullptr) {
      Refuse("renderer command", "a slot without a renderer");
      return;
    }
    static_cast<void>(
        slot.renderer(&slot, std::to_underlying(command), parameters));
  }
  void Free(Slot& slot) override {
    if (slot.rendererState != nullptr) {
      Core::Memory::FreeLiveFromHighEnd(slot.rendererState);
    }
    Core::Memory::FreeLiveFromHighEnd(&slot);
  }

 private:
  Instance* instance_ = nullptr;
};

class LiveInstanceChain final : public EffectInstanceChain {
 public:
  explicit LiveInstanceChain(Instance*& first, const Spawn* spawn = nullptr)
      : first_(&first), spawn_(spawn) {}

  Instance* First() override { return *first_; }
  void SetFirst(Instance* instance) override { *first_ = instance; }
  Instance* Next(Instance& instance) override { return instance.next; }
  void SetNext(Instance& instance, Instance* next) override {
    instance.next = next;
  }
  std::uint8_t PriorityOf(Instance& instance) override {
    return static_cast<std::uint8_t>(instance.priority);
  }
  Instance* StartInstance() override {
    if (spawn_ == nullptr) {
      return nullptr;
    }
    void* storage = Core::Memory::AllocateLiveFromHighEnd(sizeof(Instance));
    if (storage == nullptr) {
      return nullptr;
    }
    Instance* instance = std::construct_at(static_cast<Instance*>(storage));
    Start(*spawn_, *instance);
    return instance;
  }
  void ReleaseParticles(Instance& instance) override {
    LiveReleaseAllEffectParticles(&instance);
  }
  RendererChain& Renderers(Instance& instance) override {
    renderers_.Target(instance);
    return renderers_;
  }
  void ReleaseToken(Instance& instance) override {
    LiveReleaseEffectToken(static_cast<int>(instance.token));
  }
  void Free(Instance& instance) override {
    Core::Memory::FreeLiveFromHighEnd(&instance);
  }

 private:
  Instance** first_;
  const Spawn* spawn_;
  LiveRendererChain renderers_;
};

using Create = Instance* (*)(std::int32_t, EffectInstanceChain&);

[[nodiscard]] Instance* CreateOn(Instance*& first, Spawn* spawn, Create create,
                                 std::string_view call) {
  if (!Usable(spawn, call)) {
    return nullptr;
  }
  LiveInstanceChain chain(first, spawn);
  return create(spawn->priority, chain);
}

// An instance's renderers, as a frame updates them.
class LiveRendererUpdates final : public EffectRendererUpdates {
 public:
  Slot* FirstRenderer(Instance& instance) override {
    return instance.context.renderer;
  }
  Slot* NextRenderer(Slot& slot) override { return slot.next; }
  bool UpdateRenderer(Slot& slot) override {
    return LiveUpdateEffectRenderer(slot);
  }
  void DetachRenderer(Instance& instance, Slot& slot) override {
    LiveDetachEffectRenderer(&instance, &slot);
  }
};
}  // namespace

void LiveInitEffectInstance(LiveEffectSpawnRequest* spawn_request,
                            Effects_EffectInstance* instance) {
  if (instance == nullptr) {
    Refuse("init", "no effect instance");
    return;
  }
  if (Usable(spawn_request, "init")) {
    Start(*spawn_request, *instance);
  }
}

Effects_EffectInstance* LiveCreatePrimaryEffectInstance(
    LiveEffectSpawnRequest* spawn_request) {
  return CreateOn(Effects_primaryInstances, spawn_request,
                  &CreatePrimaryEffectInstance, "create primary");
}

Effects_EffectInstance* LiveCreateSecondaryEffectInstance(
    LiveEffectSpawnRequest* spawn_request) {
  return CreateOn(Effects_secondaryInstances, spawn_request,
                  &CreateSecondaryEffectInstance, "create secondary");
}

void LiveDestroyPrimaryEffectInstance(Effects_EffectInstance* instance) {
  if (instance == nullptr) {
    Refuse("destroy primary", "no effect instance");
    return;
  }
  LiveInstanceChain chain(Effects_primaryInstances);
  DestroyPrimaryEffectInstance(*instance, chain);
}

void LiveDestroySecondaryEffectInstance(Effects_EffectInstance* instance) {
  if (instance == nullptr) {
    Refuse("destroy secondary", "no effect instance");
    return;
  }
  LiveInstanceChain chain(Effects_secondaryInstances);
  DestroySecondaryEffectInstance(*instance, chain);
}

void LiveSetSecondaryEffectPhase(int phase, Effects_EffectInstance* instance) {
  const std::optional<SecondaryEffectPhase> known =
      ToSecondaryEffectPhase(phase);
  if (instance == nullptr || !known) {
    Refuse("secondary phase", "no effect instance or an unknown phase");
    return;
  }
  SetSecondaryEffectPhase(instance->context.phase, *known);
}

Effects_RendererSlot* LiveAttachEffectRenderer(Effects_EffectInstance* instance,
                                               EffectRenderer renderer,
                                               int renderer_parameters) {
  if (instance == nullptr || renderer == nullptr) {
    Refuse("attach renderer", "no effect instance or renderer");
    return nullptr;
  }
  LiveRendererChain renderers(*instance);
  return AttachEffectRenderer(renderers, renderer, renderer_parameters);
}

void LiveDetachEffectRenderer(Effects_EffectInstance* instance,
                              Effects_RendererSlot* slot) {
  if (instance == nullptr || slot == nullptr) {
    Refuse("detach renderer", "no effect instance or renderer slot");
    return;
  }
  LiveRendererChain renderers(*instance);
  if (!DetachEffectRenderer(renderers, *slot)) {
    Refuse("detach renderer", "a slot the instance does not hold");
  }
}

bool LiveUpdateEffectRenderer(Effects_RendererSlot& slot) {
  if (slot.renderer == nullptr) {
    Refuse("renderer update", "the slot has no renderer; detached");
    return false;
  }
  return slot.renderer(&slot, std::to_underlying(RendererCommand::kUpdate),
                       0) != 0;
}

void LiveUpdateEffectRenderers(Effects_EffectInstance* instance) {
  if (instance == nullptr) {
    Refuse("update renderers", "no effect instance");
    return;
  }
  LiveRendererUpdates renderers;
  UpdateEffectRenderers(*instance, renderers);
}
}  // namespace VS::Effects
