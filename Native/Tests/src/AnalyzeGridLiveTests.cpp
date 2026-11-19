#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Gte.h"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Scratch.hpp"
#include "Effects/AnalyzeGrid.hpp"
#include "Effects/AnalyzeGridLive.hpp"
#include "Effects/Package.h"
#include "Effects/PackageExtents.hpp"
#include "Effects/Particles.h"
#include "Effects/PurgeTables.hpp"
#include "Effects/Runtime.h"
#include "Render/Camera.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"

namespace {
namespace effects = VS::Effects;
namespace memory = VS::Core::Memory;
constexpr std::size_t kPrimitiveWords = 0x20000 / sizeof(u_long);
constexpr std::size_t kOrderingWords = 0x2088 / sizeof(u_long);
constexpr std::size_t kSceneOffset = 32;
constexpr std::size_t kEmitterOffset = 0x1D0;
constexpr std::uint32_t kInitialLink = 0xC4000007;

Effects_ParticleEmitterScratch& Emitter() {
  return *reinterpret_cast<Effects_ParticleEmitterScratch*>(
      static_cast<std::byte*>(VS::Core::ScratchBase()) + kEmitterOffset);
}

struct Definitions {
  std::uint32_t reserved{};
  std::array<Effects_ParticleDefinition, 2> entries{};
};
static_assert(offsetof(Definitions, entries) == 4);

class LiveGrid {
 public:
  LiveGrid() {
    Effects_package = &package;
    Effects_currentInstance = &instance;
    effects::ForgetLivePackageExtents();
    package.block5Data = reinterpret_cast<Effects_PackageBlock5*>(&definitions);
    effects::RecordLiveParticleDefinitions(
        std::as_bytes(std::span{&definitions, 1}));
    definitions.entries[1].pairRanges[3][0] = 37;
    definitions.entries[1].pairRanges[3][2] = 19;
    VS::Render::frameBuf = 0;
    VS::Render::nearClip = 0;
    VS::Render::legacyPrimitiveBuffers[0] = primitives.data();
    VS::Render::legacyOrderingTables[0] = ordering.data();
    VS::Render::FrameScratchpad().primitive_cursor = primitives.data();
    VS::Render::FrameScratchpad().ordering_table =
        ordering.data() + kSceneOffset;
    VS::Render::FrameScratchpad().view_matrix = MATRIX{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
      VS::Render::FrameScratchpad().view_matrix.m[axis][axis] = 4096;
    }
    VS::Render::FrameScratchpad().view_matrix.t[2] = 400;
    ordering.fill(kInitialLink);
    Core_Gte_Reset();
    Core_Gte_SetGeomScreen(256);
    Core_Gte_SetGeomOffset(160, 120);
  }
  ~LiveGrid() {
    if (memory::LivePayloadCapacity(slot.rendererState).has_value()) {
      memory::FreeLiveFromHighEnd(slot.rendererState);
    }
    Effects_package = &package;
    effects::ForgetLivePackageExtents();
    Effects_package = saved_package_;
    Effects_currentInstance = saved_instance_;
    Effects_actionRequest = saved_request_;
    VS::Render::frameBuf = saved_frame_buf_;
    VS::Render::nearClip = saved_near_clip_;
    VS::Render::FrameScratchpad() = saved_frame_;
    Emitter() = saved_emitter_;
    VS::Render::legacyPrimitiveBuffers[0] = saved_primitives_;
    VS::Render::legacyOrderingTables[0] = saved_ordering_;
  }
  LiveGrid(const LiveGrid&) = delete;
  LiveGrid& operator=(const LiveGrid&) = delete;
  LiveGrid(LiveGrid&&) = delete;
  LiveGrid& operator=(LiveGrid&&) = delete;

  effects::AnalyzeGridState& State() {
    const auto capacity = memory::LivePayloadCapacity(slot.rendererState);
    if (!capacity.has_value() ||
        *capacity < sizeof(effects::AnalyzeGridState)) {
      throw std::logic_error("grid test has no complete state");
    }
    return *static_cast<effects::AnalyzeGridState*>(slot.rendererState);
  }

  std::array<std::byte, 4096> arena{};
  memory::ScopedLiveGameHeap heap{arena};
  Definitions definitions{};
  Effects_EffectPackage package{};
  Effects_EffectInstance instance{};
  Effects_RendererSlot slot{};
  std::array<u_long, kPrimitiveWords> primitives{};
  std::array<u_long, kOrderingWords> ordering{};

 private:
  Effects_EffectPackage* saved_package_ = Effects_package;
  Effects_EffectInstance* saved_instance_ = Effects_currentInstance;
  Effects_ActionEffectRequest saved_request_ = Effects_actionRequest;
  int saved_frame_buf_ = VS::Render::frameBuf;
  int saved_near_clip_ = VS::Render::nearClip;
  VS::Render::FrameScratch saved_frame_ = VS::Render::FrameScratchpad();
  Effects_ParticleEmitterScratch saved_emitter_ = Emitter();
  void* saved_primitives_ = VS::Render::legacyPrimitiveBuffers[0];
  void* saved_ordering_ = VS::Render::legacyOrderingTables[0];
};

void CheckLiveLifecycle(Effects_EffectRenderer entry) {
  LiveGrid live;
  REQUIRE(entry(&live.slot, 1, 0x0102) == 1);
  CHECK(live.State().u == 37);
  void* allocation = live.slot.rendererState;
  live.State().age = 99;
  REQUIRE(entry(&live.slot, 1, 0x0102) == 1);
  CHECK(live.slot.rendererState == allocation);
  CHECK(live.State().age == 0);
  REQUIRE(entry(&live.slot, 3, 0) == 1);
  CHECK(live.State().lifetime == 1);
  CHECK(entry(&live.slot, 2, 0) == 0);
  CHECK(live.State().lifetime == 0);
}

void CheckLiveCleanup(Effects_EffectRenderer entry) {
  LiveGrid live;
  REQUIRE(entry(&live.slot, 1, 0) == 1);
  void* allocation = live.slot.rendererState;
  CHECK(entry(&live.slot, 4, 0) == 0);
  CHECK(live.slot.rendererState == allocation);
  CHECK(memory::LivePayloadCapacity(allocation).has_value());
  // EffectInstancesLive::LiveRendererChain::Free owns both allocations.
  memory::FreeLiveFromHighEnd(allocation);
  live.slot.rendererState = nullptr;
  CHECK_FALSE(memory::LivePayloadCapacity(allocation).has_value());
  CHECK(entry(&live.slot, 2, 0) == 0);
  CHECK(entry(&live.slot, 3, 0) == 0);
}

void CheckLiveUnknownCommands(Effects_EffectRenderer entry) {
  const unsigned int command = GENERATE(0U, 5U, 0xFFFFFFFFU);
  LiveGrid live;
  CHECK(entry(&live.slot, command, 0) == 1);
  CHECK(live.slot.rendererState == nullptr);
  REQUIRE(entry(&live.slot, 1, 5) == 1);
  const auto before = live.State();
  CHECK(entry(&live.slot, command, 0) == 1);
  CHECK(live.State() == before);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        live.primitives.data());
}

void CheckLiveRefusals(Effects_EffectRenderer entry) {
  const int failure = GENERATE(0, 1, 2, 3, 4);
  LiveGrid live;
  CHECK(entry(nullptr, 1, 0) == 0);
  REQUIRE(entry(&live.slot, 1, 0x0105) == 1);
  const auto before = live.State();
  if (failure == 0) {
    Effects_package = nullptr;
  } else if (failure == 1) {
    Effects_currentInstance = nullptr;
  } else if (failure == 2) {
    live.definitions.entries[1].transformIndex = 255;
  } else if (failure == 3) {
    live.definitions.entries[1].positionCurve = 1;
  } else {
    effects::ForgetLivePackageExtents();
  }
  CHECK(entry(&live.slot, 2, 0) == 0);
  CHECK(live.State() == before);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        live.primitives.data());
}

void CheckLiveCreateRefusals(Effects_EffectRenderer entry) {
  LiveGrid live;
  CHECK(entry(&live.slot, 1, 0x0201) == 0);
  CHECK(live.slot.rendererState == nullptr);
  std::array<std::byte, 32> exhausted_arena{};
  memory::ScopedLiveGameHeap exhausted(exhausted_arena);
  CHECK(entry(&live.slot, 1, 1) == 0);
  CHECK(live.slot.rendererState == nullptr);
}

void CheckLiveStateExtent(Effects_EffectRenderer entry) {
  LiveGrid live;
  effects::AnalyzeGridState outside_heap{};
  live.slot.rendererState = &outside_heap;
  CHECK(entry(&live.slot, 2, 0) == 0);
  CHECK(entry(&live.slot, 3, 0) == 0);
  CHECK(entry(&live.slot, 1, 0) == 0);
  live.slot.rendererState = memory::AllocateLiveFromHighEnd(1);
  REQUIRE(live.slot.rendererState != nullptr);
  CHECK(entry(&live.slot, 2, 0) == 0);
  CHECK(entry(&live.slot, 1, 0) == 0);
}

void CheckLiveDrawCapacity(Effects_EffectRenderer entry) {
  const int capacity = GENERATE(0, 1, 2);
  LiveGrid live;
  REQUIRE(entry(&live.slot, 1, 0) == 1);
  if (capacity == 1) {
    VS::Render::FrameScratchpad().primitive_cursor =
        live.primitives.data() + live.primitives.size() - 1;
  } else if (capacity == 2) {
    VS::Render::legacyOrderingTables[0] = nullptr;
  }
  void* before = VS::Render::FrameScratchpad().primitive_cursor;
  REQUIRE(entry(&live.slot, 2, 0) == 1);
  CHECK(live.State().age == 1);
  if (capacity != 0) {
    CHECK(VS::Render::FrameScratchpad().primitive_cursor == before);
    CHECK(live.ordering[kSceneOffset + 100] == kInitialLink);
  } else {
    CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
          live.primitives.data() + 64 * effects::Templates::kGridPacketWords);
    CHECK((live.ordering[kSceneOffset + 100] & 0xFF000000U) ==
          (kInitialLink & 0xFF000000U));
    CHECK(live.primitives[2] == 0x2E808080U);
  }
}
}  // namespace

TEST_CASE("F01893 Live Analyze resets and expires its own slot",
          "[effects-analyze-grid][F01893][live]") {
  CheckLiveLifecycle(effects::UpdateLivePlg005AnalyzeGrid);
}
TEST_CASE("F01893 Live Analyze leaves deallocation to the renderer owner",
          "[effects-analyze-grid][F01893][live]") {
  CheckLiveCleanup(effects::UpdateLivePlg005AnalyzeGrid);
}
TEST_CASE("F01893 Live Analyze ignores unknown legacy commands",
          "[effects-analyze-grid][F01893][live]") {
  CheckLiveUnknownCommands(effects::UpdateLivePlg005AnalyzeGrid);
}
TEST_CASE("F01893 Live Analyze refuses missing or stale tick inputs",
          "[effects-analyze-grid][F01893][live]") {
  CheckLiveRefusals(effects::UpdateLivePlg005AnalyzeGrid);
}
TEST_CASE(
    "F01893 Live Analyze refuses invalid definitions and allocation failure",
    "[effects-analyze-grid][F01893][live]") {
  CheckLiveCreateRefusals(effects::UpdateLivePlg005AnalyzeGrid);
}
TEST_CASE("F01893 Live Analyze validates state extents",
          "[effects-analyze-grid][F01893][live]") {
  CheckLiveStateExtent(effects::UpdateLivePlg005AnalyzeGrid);
}
TEST_CASE("F01893 Live Analyze draws only within frame capacity",
          "[effects-analyze-grid][F01893][live]") {
  CheckLiveDrawCapacity(effects::UpdateLivePlg005AnalyzeGrid);
}

TEST_CASE("F01926 Live Analyze resets and expires its own slot",
          "[effects-analyze-grid][F01926][live]") {
  CheckLiveLifecycle(effects::UpdateLivePlg054AnalyzeGrid);
}
TEST_CASE("F01926 Live Analyze leaves deallocation to the renderer owner",
          "[effects-analyze-grid][F01926][live]") {
  CheckLiveCleanup(effects::UpdateLivePlg054AnalyzeGrid);
}
TEST_CASE("F01926 Live Analyze ignores unknown legacy commands",
          "[effects-analyze-grid][F01926][live]") {
  CheckLiveUnknownCommands(effects::UpdateLivePlg054AnalyzeGrid);
}
TEST_CASE("F01926 Live Analyze refuses missing or stale tick inputs",
          "[effects-analyze-grid][F01926][live]") {
  CheckLiveRefusals(effects::UpdateLivePlg054AnalyzeGrid);
}
TEST_CASE(
    "F01926 Live Analyze refuses invalid definitions and allocation failure",
    "[effects-analyze-grid][F01926][live]") {
  CheckLiveCreateRefusals(effects::UpdateLivePlg054AnalyzeGrid);
}
TEST_CASE("F01926 Live Analyze validates state extents",
          "[effects-analyze-grid][F01926][live]") {
  CheckLiveStateExtent(effects::UpdateLivePlg054AnalyzeGrid);
}
TEST_CASE("F01926 Live Analyze draws only within frame capacity",
          "[effects-analyze-grid][F01926][live]") {
  CheckLiveDrawCapacity(effects::UpdateLivePlg054AnalyzeGrid);
}

TEST_CASE("F01928 Live Analyze resets and expires its own slot",
          "[effects-analyze-grid][F01928][live]") {
  CheckLiveLifecycle(effects::UpdateLivePlg055AnalyzeGrid);
}
TEST_CASE("F01928 Live Analyze leaves deallocation to the renderer owner",
          "[effects-analyze-grid][F01928][live]") {
  CheckLiveCleanup(effects::UpdateLivePlg055AnalyzeGrid);
}
TEST_CASE("F01928 Live Analyze ignores unknown legacy commands",
          "[effects-analyze-grid][F01928][live]") {
  CheckLiveUnknownCommands(effects::UpdateLivePlg055AnalyzeGrid);
}
TEST_CASE("F01928 Live Analyze refuses missing or stale tick inputs",
          "[effects-analyze-grid][F01928][live]") {
  CheckLiveRefusals(effects::UpdateLivePlg055AnalyzeGrid);
}
TEST_CASE(
    "F01928 Live Analyze refuses invalid definitions and allocation failure",
    "[effects-analyze-grid][F01928][live]") {
  CheckLiveCreateRefusals(effects::UpdateLivePlg055AnalyzeGrid);
}
TEST_CASE("F01928 Live Analyze validates state extents",
          "[effects-analyze-grid][F01928][live]") {
  CheckLiveStateExtent(effects::UpdateLivePlg055AnalyzeGrid);
}
TEST_CASE("F01928 Live Analyze draws only within frame capacity",
          "[effects-analyze-grid][F01928][live]") {
  CheckLiveDrawCapacity(effects::UpdateLivePlg055AnalyzeGrid);
}

TEST_CASE("F01996 Live Analyze resets and expires its own slot",
          "[effects-analyze-grid][F01996][live]") {
  CheckLiveLifecycle(effects::UpdateLivePlg074AnalyzeGrid);
}
TEST_CASE("F01996 Live Analyze leaves deallocation to the renderer owner",
          "[effects-analyze-grid][F01996][live]") {
  CheckLiveCleanup(effects::UpdateLivePlg074AnalyzeGrid);
}
TEST_CASE("F01996 Live Analyze ignores unknown legacy commands",
          "[effects-analyze-grid][F01996][live]") {
  CheckLiveUnknownCommands(effects::UpdateLivePlg074AnalyzeGrid);
}
TEST_CASE("F01996 Live Analyze refuses missing or stale tick inputs",
          "[effects-analyze-grid][F01996][live]") {
  CheckLiveRefusals(effects::UpdateLivePlg074AnalyzeGrid);
}
TEST_CASE(
    "F01996 Live Analyze refuses invalid definitions and allocation failure",
    "[effects-analyze-grid][F01996][live]") {
  CheckLiveCreateRefusals(effects::UpdateLivePlg074AnalyzeGrid);
}
TEST_CASE("F01996 Live Analyze validates state extents",
          "[effects-analyze-grid][F01996][live]") {
  CheckLiveStateExtent(effects::UpdateLivePlg074AnalyzeGrid);
}
TEST_CASE("F01996 Live Analyze draws only within frame capacity",
          "[effects-analyze-grid][F01996][live]") {
  CheckLiveDrawCapacity(effects::UpdateLivePlg074AnalyzeGrid);
}

TEST_CASE("Analyze plugin dispatch retains overlay lifecycle bindings",
          "[effects-analyze-grid][integration]") {
  struct Binding {
    std::size_t index;
    Effects_EffectRenderer entry;
  };
  constexpr std::array<Binding, 4> kBindings{{
      {0, effects::UpdateLivePlg005AnalyzeGrid},
      {16, effects::UpdateLivePlg054AnalyzeGrid},
      {18, effects::UpdateLivePlg055AnalyzeGrid},
      {91, effects::UpdateLivePlg074AnalyzeGrid},
  }};
  for (const auto& binding : kBindings) {
    CAPTURE(binding.index);
    REQUIRE(VS::Effects::effectEntryPoints[binding.index] == binding.entry);
    CheckLiveLifecycle(VS::Effects::effectEntryPoints[binding.index]);
  }
}
