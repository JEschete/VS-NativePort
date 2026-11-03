#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Core/Gte.h"
#include "Legacy/Psx.h"
#include "Render/DepthCue.hpp"
#include "Render/DepthCueLive.hpp"

namespace {
namespace Render = VS::Render;

struct FogFixture {
  FogFixture() {
    Core_Gte_Reset();
    Core_Gte_SetGeomScreen(320);
  }

  ~FogFixture() { Core_Gte_Reset(); }
};

Core_GteWord ProjectCue(std::int16_t depth) {
  SVECTOR vertex{0, 0, depth, 0};
  Core_GteWord screen = 0;
  Core_GteWord cue = -1;
  Core_GteWord flags = 0;
  Core_Gte_RotTransPers(&vertex, &screen, &cue, &flags);
  return cue;
}

Core_GteWord ReferenceCue(Core_GteWord near_distance, Core_GteWord projection,
                          std::int16_t depth) {
  const auto coefficient =
      std::clamp<std::int64_t>(-std::int64_t{near_distance} * 320 /
                                   std::max<Core_GteWord>(projection, 1),
                               -32768, 32767);
  const auto quotient = std::min<std::int64_t>(
      ((std::int64_t{projection} * 131072 / depth) + 1) / 2, 131071);
  const auto accumulator = std::bit_cast<std::int32_t>(
      static_cast<std::uint32_t>(quotient * coefficient + 20971520));
  return std::clamp<Core_GteWord>(accumulator >> 12, 0, 4096);
}

void CheckCue(Core_GteWord near_distance, Core_GteWord projection,
              std::int16_t depth) {
  Core_Gte_SetGeomScreen(projection);
  Render::SetLiveFogNearDistance(near_distance, projection);
  CHECK(ProjectCue(depth) == ReferenceCue(near_distance, projection, depth));
}

void CheckColor(const CVECTOR& actual, const CVECTOR& expected) {
  CHECK(actual.r == expected.r);
  CHECK(actual.g == expected.g);
  CHECK(actual.b == expected.b);
  CHECK(actual.cd == expected.cd);
}
}  // namespace

TEST_CASE("Map fog begins at its configured near plane without dimming it",
          "[render][fog]") {
  FogFixture fixture;
  Render::SetLiveFogNearDistance(768, 320);
  Core_Gte_SetFarColor(0, 0, 0);
  SVECTOR vertex{0, 0, 768, 0};
  Core_GteWord screen = 0;
  Core_GteWord cue = -1;
  Core_GteWord flags = 0;
  CHECK(Core_Gte_RotTransPers(&vertex, &screen, &cue, &flags) == 192);
  CHECK(cue == 0);
  CVECTOR input{160, 80, 40, 0x3C};
  CVECTOR output{};
  Render::ApplyLiveDepthCueColor(&input, cue, &output);
  CHECK(output.r == input.r);
  CHECK(output.g == input.g);
  CHECK(output.b == input.b);
  CHECK(output.cd == input.cd);
}

TEST_CASE("Fog near distance computes the source cue registers",
          "[render][fog][render-display]") {
  CHECK(Render::FogNearDepthCue(768, 320).coefficient == -768);
  CHECK(Render::FogNearDepthCue(768, 320).offset == Render::kFogCueOffset);
  // The native policy (#195): no fog for a non-positive distance, a
  // non-positive projection distance counts as 1 and the coefficient
  // saturates to the DQA halfword.
  CHECK(Render::FogNearDepthCue(0, 320).coefficient == 0);
  CHECK(Render::FogNearDepthCue(0, 320).offset == 0);
  CHECK(Render::FogNearDepthCue(-5, 320).offset == 0);
  CHECK(Render::FogNearDepthCue(1, 0).coefficient == -320);
  CHECK(Render::FogNearDepthCue(1, -9).coefficient == -320);
  CHECK(Render::FogNearDepthCue(INT32_MAX, 1).coefficient ==
        std::numeric_limits<std::int16_t>::min());
}

TEST_CASE("Disabling fog writes cleared cue registers",
          "[render][fog][render-display]") {
  struct Registers final : Render::DepthCueRegisters {
    Render::DepthCue cue{.coefficient = 1, .offset = 1};
    int writes = 0;
    void SetDepthCue(const Render::DepthCue& value) override {
      cue = value;
      ++writes;
    }
  } registers;
  Render::SetFogNearDistance(registers, 0, 320);
  CHECK(registers.writes == 1);
  CHECK(registers.cue.coefficient == 0);
  CHECK(registers.cue.offset == 0);
}

TEST_CASE("Depth cue saturates like the GTE DPCS pipeline",
          "[render][fog][render-display]") {
  const Render::ColorCode color{
      .red = 160, .green = 80, .blue = 40, .code = 0x3C};
  // The far colour is held times 16; a difference past the IR halfword
  // saturates before the multiply.
  const Render::FarColor far{100000, -100000, 40 * 16};
  const auto full = Render::ApplyDepthCueColor(color, 4096, far);
  CHECK(full.red == 255);
  CHECK(full.green == 0);
  CHECK(full.blue == 40);
  CHECK(full.code == 0x3C);
}

TEST_CASE(
    "Depth cue keeps the IR0 halfword and clamps away from the far colour",
    "[render][fog][render-display]") {
  const Render::ColorCode color{
      .red = 160, .green = 80, .blue = 40, .code = 0x3C};
  // IR0 is a halfword: only the low 16 bits of the cue are used.
  const auto wrapped =
      Render::ApplyDepthCueColor(color, 0x10000 + 2048, {0, 0, 0});
  const auto half = Render::ApplyDepthCueColor(color, 2048, {0, 0, 0});
  CHECK(wrapped.red == half.red);
  CHECK(wrapped.green == half.green);
  CHECK(wrapped.blue == half.blue);
  CHECK(half.red == 80);
  // A negative cue moves away from the far colour and clamps to a byte.
  const auto away = Render::ApplyDepthCueColor(color, -4096, {0, 0, 0});
  CHECK(away.red == 255);
  CHECK(away.green == 160);
  CHECK(away.blue == 80);
}

TEST_CASE("Live depth cue ignores a null colour", "[render][fog]") {
  FogFixture fixture;
  CVECTOR output{1, 2, 3, 4};
  Render::ApplyLiveDepthCueColor(nullptr, 4096, &output);
  CheckColor(output, CVECTOR{1, 2, 3, 4});
  Render::ApplyLiveDepthCueColor(&output, 4096, nullptr);
  CheckColor(output, CVECTOR{1, 2, 3, 4});
}

TEST_CASE("Map fog reaches the source midpoint and far-color endpoints",
          "[render][fog]") {
  FogFixture fixture;
  Render::SetLiveFogNearDistance(768, 320);
  CHECK(ProjectCue(384) == 0);
  CHECK(ProjectCue(768) == 0);
  CHECK(ProjectCue(1536) == 2560);
  CHECK(ProjectCue(3840) == 4096);
  CHECK(ProjectCue(8192) == 4096);
}

TEST_CASE("Native fog covers source-scaled projection and depth tuples",
          "[render][fog]") {
  FogFixture fixture;
  constexpr std::array<Core_GteWord, 8> kNear{1,   64,  127,  256,
                                              320, 768, 1024, 8192};
  constexpr std::array<Core_GteWord, 6> kProjection{64,  128, 256,
                                                    320, 512, 1000};
  constexpr std::array<std::int16_t, 12> kDepth{
      1, 32, 64, 128, 320, 768, 1024, 1536, 3840, 8192, 16384, 32767};
  for (Core_GteWord near_distance : kNear) {
    for (Core_GteWord projection : kProjection) {
      for (std::int16_t depth : kDepth) {
        CheckCue(near_distance, projection, depth);
      }
    }
  }
}

TEST_CASE(
    "Fog preserves disabled state across positive and nonpositive reentry",
    "[render][fog]") {
  FogFixture fixture;
  constexpr std::array<Core_GteWord, 3> kDisabled{0, -1, INT32_MIN};
  for (Core_GteWord near_distance : kDisabled) {
    Render::SetLiveFogNearDistance(768, 320);
    REQUIRE(ProjectCue(1536) == 2560);
    Render::SetLiveFogNearDistance(near_distance, 0);
    CHECK(ProjectCue(1536) == 0);
  }
}

TEST_CASE(
    "Large native fog distances saturate without signed multiply overflow",
    "[render][fog]") {
  FogFixture fixture;
  constexpr std::array<Core_GteWord, 5> kNear{100000, 6710886, 7000000,
                                              10000000, INT32_MAX};
  for (Core_GteWord near_distance : kNear) CheckCue(near_distance, 512, 32767);
}

TEST_CASE("Typed depth cue preserves every in-range factor and color code",
          "[render][fog]") {
  FogFixture fixture;
  Core_Gte_SetFarColor(24, 192, 96);
  CVECTOR input{160, 80, 40, 0x3C};
  for (Core_GteWord cue = 0; cue <= 4096; ++cue) {
    CVECTOR expected{
        static_cast<unsigned char>((160 * (4096 - cue) + 24 * cue) / 4096),
        static_cast<unsigned char>((80 * (4096 - cue) + 192 * cue) / 4096),
        static_cast<unsigned char>((40 * (4096 - cue) + 96 * cue) / 4096),
        0x3C};
    CVECTOR output{};
    Render::ApplyLiveDepthCueColor(&input, cue, &output);
    CheckColor(output, expected);
  }
}

TEST_CASE("Typed depth cue supports an aliased color at the far endpoint",
          "[render][fog]") {
  FogFixture fixture;
  Core_Gte_SetFarColor(24, 192, 96);
  CVECTOR color{160, 80, 40, 0x3C};
  Render::ApplyLiveDepthCueColor(&color, 4096, &color);
  CheckColor(color, CVECTOR{24, 192, 96, 0x3C});
}

TEST_CASE("Retired fog entries leave no module address bindings",
          "[render][fog][render-display]") {
  // No module image stores SetFogNear or DpqColor (#276 slice A4), so the
  // typed cores are reached only through OWNED_CALL_BINDINGS.
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40",
                                          0x80041954u) == nullptr);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40",
                                          0x80041A00u) == nullptr);
}
