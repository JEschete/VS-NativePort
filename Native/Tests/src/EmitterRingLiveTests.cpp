#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Core/Gte.h"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Core/Scratch.hpp"
#include "Effects/ActionRequest.hpp"
#include "Effects/Package.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/ParticleEmitterLive.hpp"
#include "Effects/ParticlePoolLive.hpp"
#include "Effects/Particles.hpp"
#include "Effects/PurgeTables.hpp"
#include "Effects/Runtime.h"
#include "Effects/Templates/RepeatingRingsLive.hpp"
#include "Effects/Templates/TemplateLiveState.hpp"
#include "Effects/TransformsLiveState.hpp"
#include "Legacy/Psx.h"

namespace {
namespace effects = VS::Effects;
namespace rings = VS::Effects::Templates;
namespace memory = VS::Core::Memory;
namespace math = VS::Core::Math;
using LiveRing = int (*)(VS::Effects::LiveRendererSlot*, unsigned int, int);
constexpr std::size_t kScratchBytes = 1024;
constexpr std::size_t kEmitterScratch = 0x1D0;

VS::Effects::LiveParticleEmitterScratch& Scratch() {
  const auto base = reinterpret_cast<std::uintptr_t>(VS::Core::ScratchBase());
  return *reinterpret_cast<VS::Effects::LiveParticleEmitterScratch*>(
      base + kEmitterScratch);
}

struct Definitions {
  std::uint32_t reserved{};
  std::array<VS::Effects::LiveParticleDefinition, 2> values{};
};

class LiveEmission {
 public:
  Definitions definitions;
  VS::Effects::LiveEffectPackage package{};
  std::unique_ptr<Effects_EffectInstance> instance =
      std::make_unique<Effects_EffectInstance>();
  std::array<VS::Effects::LiveParticle, 4> particles{};
  alignas(effects::LiveCurveTable) std::array<std::uint8_t, 8> curves{
      1, 0, 6, 0, 1, 0, 64, 0};

  LiveEmission() {
    std::ranges::copy(std::span(Effects_curveSamples), saved_samples_.begin());
    std::ranges::copy(
        std::span(static_cast<const std::byte*>(VS::Core::ScratchBase()),
                  kScratchBytes),
        saved_scratch_.begin());
    Effects_package = &package;
    Effects_currentInstance = instance.get();
    package.particle_definitions =
        reinterpret_cast<effects::LiveParticleDefinitionTable*>(&definitions);
    package.curve_block =
        reinterpret_cast<effects::LiveCurveTable*>(curves.data());
    package.curves[0] = curves.data() + 6;
    effects::RecordLiveParticleDefinitions(
        std::as_bytes(std::span(&definitions, 1)));
    effects::RecordLiveEffectCurves(std::as_bytes(std::span(curves)));
    std::ranges::fill(Effects_curveSamples, 0);
    Effects_curveSamples[1] = 64;
    VS::Effects::particlePool = particles.data();
    VS::Effects::freeParticles = particles.data();
    VS::Effects::particleCapacity = static_cast<int>(particles.size());
    VS::Effects::usedParticleCount = 0;
    VS::Effects::emissionCounter = 0;
    for (std::size_t index = 0; index < particles.size(); ++index) {
      particles.at(index).next =
          index + 1 < particles.size() ? &particles.at(index + 1) : nullptr;
      particles.at(index).previous =
          index > 0 ? &particles.at(index - 1) : nullptr;
    }
    for (auto& transform : instance->transforms) {
      math::StoreMatrix(transform.world, math::IdentityRotation());
      math::StoreMatrix(transform.view, math::IdentityRotation());
      transform.view.t[2] = 4096;
    }
    auto& definition = definitions.values[0];
    definition.flags = 0xA038;
    definition.count_range[0] = 1;
    definition.count_range[1] = 1;
    definition.interval_range[0] = 1;
    definition.interval_range[1] = 1;
    VS::Core::BiosRand().Seed(1);
    Core_Gte_Reset();
    Scratch() = {};
  }
  LiveEmission(const LiveEmission&) = delete;
  LiveEmission(LiveEmission&&) = delete;
  LiveEmission& operator=(const LiveEmission&) = delete;
  LiveEmission& operator=(LiveEmission&&) = delete;
  ~LiveEmission() {
    effects::ForgetLivePackageExtents();
    Effects_package = saved_package_;
    Effects_currentInstance = saved_instance_;
    VS::Effects::particlePool = saved_pool_;
    VS::Effects::freeParticles = saved_free_;
    VS::Effects::particleCapacity = saved_capacity_;
    VS::Effects::usedParticleCount = saved_used_;
    VS::Effects::emissionCounter = saved_counter_;
    std::ranges::copy(saved_samples_, Effects_curveSamples);
    std::ranges::copy(saved_scratch_,
                      static_cast<std::byte*>(VS::Core::ScratchBase()));
    VS::Core::BiosRand().Seed(saved_random_);
    Core_Gte_Reset();
  }
  void Emit() {
    effects::EmitLiveParticleEvent(instance.get(), 0, 0, 0, nullptr);
  }

 private:
  VS::Effects::LiveEffectPackage* saved_package_ = Effects_package;
  Effects_EffectInstance* saved_instance_ = Effects_currentInstance;
  VS::Effects::LiveParticle* saved_pool_ = VS::Effects::particlePool;
  VS::Effects::LiveParticle* saved_free_ = VS::Effects::freeParticles;
  int saved_capacity_ = VS::Effects::particleCapacity;
  int saved_used_ = VS::Effects::usedParticleCount;
  int saved_counter_ = VS::Effects::emissionCounter;
  std::uint32_t saved_random_ = VS::Core::BiosRand().State();
  std::array<int, 211> saved_samples_{};
  std::array<std::byte, kScratchBytes> saved_scratch_{};
  alignas(memory::kHeapUnitBytes)
      std::array<std::byte, memory::kHeapUnitBytes * 64> arena_{};
  memory::ScopedLiveGameHeap heap_{arena_};
};

class RingSlot {
 public:
  VS::Effects::LiveRendererSlot slot{};
  RingSlot() = default;
  RingSlot(const RingSlot&) = delete;
  RingSlot(RingSlot&&) = delete;
  RingSlot& operator=(const RingSlot&) = delete;
  RingSlot& operator=(RingSlot&&) = delete;
  ~RingSlot() { Release(); }
  void Release() {
    if (slot.renderer_state != nullptr) {
      memory::FreeLiveFromHighEnd(slot.renderer_state);
      slot.renderer_state = nullptr;
    }
  }
};

void CheckLiveBindings(LiveRing ring, LiveEmission& fixture, RingSlot& owner) {
  auto* state = static_cast<effects::LiveRingState*>(owner.slot.renderer_state);
  SECTION("missing current curve refuses without sampling a zero replacement") {
    fixture.definitions.values[0].position_curve = 1;
    fixture.package.curves[0] = nullptr;
    CHECK(ring(&owner.slot, 2, 0) == 0);
    CHECK(state->age == 0);
  }
  SECTION("unbound transform refuses before drawing or ageing") {
    fixture.definitions.values[0].transform_index = 16;
    CHECK(ring(&owner.slot, 2, 0) == 0);
    CHECK(state->lifetime == 2);
  }
}

void CheckLiveLifecycle(LiveRing ring, RingSlot& owner) {
  auto* state = static_cast<effects::LiveRingState*>(owner.slot.renderer_state);
  SECTION("repeated create does not overwrite or leak the existing state") {
    CHECK(ring(&owner.slot, 1, 3) == 0);
    CHECK(owner.slot.renderer_state == state);
  }
  SECTION("state expires without texture data and cleanup remains with owner") {
    CHECK(ring(&owner.slot, 2, 0) == 1);
    CHECK(ring(&owner.slot, 2, 0) == 0);
    CHECK(state->age == 2);
    CHECK(ring(&owner.slot, 4, 0) == 0);
    CHECK(owner.slot.renderer_state == state);
  }
}

void CheckLiveRing(LiveRing ring) {
  LiveEmission fixture;
  RingSlot owner;
  fixture.definitions.values[0].flags = rings::kRingEffectView;
  REQUIRE(ring(&owner.slot, 1, 2) == 1);
  REQUIRE(owner.slot.renderer_state != nullptr);
  SECTION("checked current bindings") {
    CheckLiveBindings(ring, fixture, owner);
  }
  SECTION("created-state lifetime") { CheckLiveLifecycle(ring, owner); }
  owner.Release();
  CHECK(memory::GameHeap().Empty());
}
}  // namespace

TEST_CASE("F01515 Live reads only current bound curves before allocating",
          "[F01515][emitter-rings-live][effects]") {
  LiveEmission fixture;
  fixture.definitions.values[0].position_curve = 1;
  Scratch().flags = 0x1234;
  SECTION("missing curve bytes") { fixture.package.curves[0] = nullptr; }
  SECTION("detached recorded curve block") {
    fixture.package.curve_block = nullptr;
  }
  SECTION("empty curve") { fixture.curves[4] = 0; }
  SECTION("unrecorded current block") { effects::ForgetLivePackageExtents(); }
  fixture.Emit();
  CHECK(VS::Effects::usedParticleCount == 0);
  CHECK(fixture.instance->particleCount == 0);
  CHECK(Scratch().flags == 0x1234);
  CHECK(VS::Core::BiosRand().State() == 1);
}

TEST_CASE("F01515 Live refuses inconsistent counts cycles and misaligned links",
          "[F01515][emitter-rings-live][effects]") {
  LiveEmission fixture;
  SECTION("used count exceeds the pool") { VS::Effects::usedParticleCount = 5; }
  SECTION("cycle in the free list") {
    fixture.particles[0].next = fixture.particles.data();
  }
  SECTION("misaligned free record") {
    const auto address =
        reinterpret_cast<std::uintptr_t>(fixture.particles.data()) + 1;
    VS::Effects::freeParticles =
        reinterpret_cast<VS::Effects::LiveParticle*>(address);
  }
  fixture.Emit();
  CHECK(fixture.instance->particles == nullptr);
  CHECK(fixture.instance->particleCount == 0);
  CHECK(VS::Core::BiosRand().State() == 1);
}

TEST_CASE(
    "F01515 Live resolves source and destination from the supplied instance",
    "[F01515][emitter-rings-live][effects]") {
  LiveEmission fixture;
  Effects_currentInstance = nullptr;
  auto& definition = fixture.definitions.values[0];
  std::ranges::fill(definition.pair_ranges[2], 10);
  fixture.instance->context.emit_from.position = {100, 0, 0, 0};
  fixture.instance->context.emit_to.position = {0, 0, 200, 0};
  SECTION("source") {
    definition.flags = 0x44038;
    fixture.Emit();
    CHECK(Scratch().aim_point.vx == 100);
    CHECK(fixture.particles[0].motion.velocity.vx > 0);
    CHECK(fixture.particles[0].motion.velocity.vz == 0);
  }
  SECTION("destination") {
    definition.flags = 0x46038;
    fixture.Emit();
    CHECK(Scratch().aim_point.vz == 200);
    CHECK(fixture.particles[0].motion.velocity.vx == 0);
    CHECK(fixture.particles[0].motion.velocity.vz > 0);
  }
  CHECK(fixture.instance->particleCount == 1);
}

TEST_CASE("F01515 Live binds native pool scratch and texture paths",
          "[F01515][emitter-rings-live][effects]") {
  LiveEmission fixture;
  auto& definition = fixture.definitions.values[0];
  definition.flags = 0xB008;
  definition.count_range[0] = 9;
  definition.count_range[1] = 9;
  definition.texture_index = 3;
  definition.tick_event = 2;
  definition.transparency_curve = 6;
  std::ranges::fill(definition.extent_range, 2);
  Scratch().transparency = 0x12345600;
  fixture.Emit();
  CHECK(fixture.instance->particles == &fixture.particles[3]);
  CHECK(VS::Effects::usedParticleCount == 4);
  CHECK(fixture.particles[0].age == 255);
  CHECK(fixture.particles[0].texture_animation.texture_index == 3);
  CHECK(fixture.particles[0].tick_event == 2);
  CHECK(Scratch().transparency == 0x12345606);
}

TEST_CASE("F01515 Live bulk release returns the emitted list to its owner",
          "[F01515][emitter-rings-live][effects]") {
  LiveEmission fixture;
  std::ranges::fill(fixture.definitions.values[0].count_range, 9);
  fixture.Emit();
  REQUIRE(fixture.instance->particleCount == 4);
  effects::LiveReleaseAllEffectParticles(fixture.instance.get());
  // The existing bulk-release contract retains the head until owner
  // destruction.
  CHECK(VS::Effects::freeParticles == fixture.instance->particles);
  CHECK(fixture.instance->particleCount == 0);
  CHECK(VS::Effects::usedParticleCount == 0);
}

TEST_CASE(
    "F01515 Live refuses an unavailable actor bone instead of using zero aim",
    "[F01515][emitter-rings-live][effects]") {
  LiveEmission fixture;
  fixture.definitions.values[0].flags = 0x44038;
  auto& source = fixture.instance->context.emit_from;
  source.type = 4;
  VS::Effects::LiveActorEndpoint actor = source.Actor();
  actor.actor = 255;
  actor.part = 9;
  source.SetActor(actor);
  fixture.Emit();
  CHECK(VS::Effects::usedParticleCount == 0);
  CHECK(fixture.instance->particleCount == 0);
  CHECK(VS::Core::BiosRand().State() == 1);
}

TEST_CASE(
    "F01515 Live rejects null instances definition bounds and transform bounds",
    "[F01515][emitter-rings-live][effects]") {
  LiveEmission fixture;
  effects::EmitLiveParticleEvent(nullptr, 0, 0, 0, nullptr);
  effects::EmitLiveParticleEvent(fixture.instance.get(), -1, 0, 0, nullptr);
  effects::EmitLiveParticleEvent(fixture.instance.get(), 2, 0, 0, nullptr);
  fixture.definitions.values[0].transform_index = 16;
  fixture.Emit();
  CHECK(fixture.instance->particleCount == 0);
  CHECK(VS::Effects::usedParticleCount == 0);
}

TEST_CASE("F01920 Live PLG052 checks current bindings and owner cleanup",
          "[F01920][emitter-rings-live][effects]") {
  CheckLiveRing(&rings::UpdateLivePlg052RepeatingRing);
}
TEST_CASE("F01998 Live PLG077 checks current bindings and owner cleanup",
          "[F01998][emitter-rings-live][effects]") {
  CheckLiveRing(&rings::UpdateLivePlg077RepeatingRing);
}
TEST_CASE("F02209 Live PLG162 checks current bindings and owner cleanup",
          "[F02209][emitter-rings-live][effects]") {
  CheckLiveRing(&rings::UpdateLivePlg162TransposedRing);
}
TEST_CASE("F02289 Live PLG236 checks current bindings and owner cleanup",
          "[F02289][emitter-rings-live][effects]") {
  CheckLiveRing(&rings::UpdateLivePlg236TransposedRing);
}
TEST_CASE("F02323 Live PLG263 checks current bindings and owner cleanup",
          "[F02323][emitter-rings-live][effects]") {
  CheckLiveRing(&rings::UpdateLivePlg263TransposedRing);
}
TEST_CASE("F02397 Live PLG300 checks current bindings and owner cleanup",
          "[F02397][emitter-rings-live][effects]") {
  CheckLiveRing(&rings::UpdateLivePlg300RepeatingRing);
}
TEST_CASE(
    "F02398 Live foreground PLG300 checks current bindings and owner cleanup",
    "[F02398][emitter-rings-live][effects]") {
  CheckLiveRing(&rings::UpdateLivePlg300ForegroundRepeatingRing);
}

TEST_CASE("EFFPURGE dispatches all repeating ring lifecycles to their overlays",
          "[emitter-rings-live][effects]") {
  struct Binding {
    std::size_t entry;
    LiveRing callback;
  };
  constexpr std::array kBindings{
      Binding{13, &rings::UpdateLivePlg052RepeatingRing},
      Binding{103, &rings::UpdateLivePlg077RepeatingRing},
      Binding{108, &rings::UpdateLivePlg077RepeatingRing},
      Binding{113, &rings::UpdateLivePlg077RepeatingRing},
      Binding{118, &rings::UpdateLivePlg077RepeatingRing},
      Binding{236, &rings::UpdateLivePlg162TransposedRing},
      Binding{276, &rings::UpdateLivePlg236TransposedRing},
      Binding{307, &rings::UpdateLivePlg263TransposedRing},
      Binding{338, &rings::UpdateLivePlg300RepeatingRing},
      Binding{339, &rings::UpdateLivePlg300ForegroundRepeatingRing}};
  for (const auto& binding : kBindings) {
    DYNAMIC_SECTION("entry " << binding.entry) {
      const auto callback = VS::Effects::effectEntryPoints[binding.entry];
      REQUIRE(callback == binding.callback);
      CheckLiveRing(callback);
    }
  }
}
