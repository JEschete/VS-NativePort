#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Core/Gte.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Render/Lighting.h"
}

namespace {
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
  Render_SetFogNearDistance(near_distance, projection);
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
  Core_Gte_SetFogNear(768, 320);
  Core_Gte_SetFarColor(0, 0, 0);
  SVECTOR vertex{0, 0, 768, 0};
  Core_GteWord screen = 0;
  Core_GteWord cue = -1;
  Core_GteWord flags = 0;
  CHECK(Core_Gte_RotTransPers(&vertex, &screen, &cue, &flags) == 192);
  CHECK(cue == 0);
  CVECTOR input{160, 80, 40, 0x3C};
  CVECTOR output{};
  Core_Gte_DpqColor(&input, cue, &output);
  CHECK(output.r == input.r);
  CHECK(output.g == input.g);
  CHECK(output.b == input.b);
  CHECK(output.cd == input.cd);
}

TEST_CASE("Fog and depth-cue exports share the existing typed native APIs",
          "[render][fog]") {
  using NativeFog = void (*)(Core_GteWord, Core_GteWord);
  using NativeColor = void (*)(CVECTOR*, Core_GteWord, CVECTOR*);
  CHECK((std::is_same_v<decltype(&Render_SetFogNearDistance), NativeFog>));
  CHECK((std::is_same_v<decltype(&Render_ApplyDepthCueColor), NativeColor>));
  CHECK(&Render_SetFogNearDistance == &Core_Gte_SetFogNear);
  CHECK(&Render_ApplyDepthCueColor == &Core_Gte_DpqColor);
}

TEST_CASE("Map fog reaches the source midpoint and far-color endpoints",
          "[render][fog]") {
  FogFixture fixture;
  Render_SetFogNearDistance(768, 320);
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
    Core_Gte_SetFogNear(768, 320);
    REQUIRE(ProjectCue(1536) == 2560);
    Render_SetFogNearDistance(near_distance, 0);
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
    Render_ApplyDepthCueColor(&input, cue, &output);
    CheckColor(output, expected);
  }
}

TEST_CASE("Typed depth cue supports an aliased color at the far endpoint",
          "[render][fog]") {
  FogFixture fixture;
  Core_Gte_SetFarColor(24, 192, 96);
  CVECTOR color{160, 80, 40, 0x3C};
  Render_ApplyDepthCueColor(&color, 4096, &color);
  CheckColor(color, CVECTOR{24, 192, 96, 0x3C});
}

TEST_CASE("Fog typed resident export invokes the native projection path",
          "[render][fog][redirect-binding]") {
  FogFixture fixture;
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  auto* entry =
      VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40", 0x80041954u);
  REQUIRE(entry != nullptr);
  const auto configure =
      reinterpret_cast<void (*)(Core_GteWord, Core_GteWord)>(entry);
  CHECK(configure == Core_Gte_SetFogNear);
  configure(768, 320);
  CHECK(ProjectCue(768) == 0);
  CHECK(ProjectCue(1536) == 2560);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40",
                                          0x80041955u) == nullptr);
}

TEST_CASE("Depth-cue typed resident export invokes native color interpolation",
          "[render][fog][redirect-binding]") {
  FogFixture fixture;
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  auto* entry =
      VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40", 0x80041A00u);
  REQUIRE(entry != nullptr);
  const auto color =
      reinterpret_cast<void (*)(CVECTOR*, Core_GteWord, CVECTOR*)>(entry);
  CHECK(color == Core_Gte_DpqColor);
  Core_Gte_SetFarColor(24, 192, 96);
  CVECTOR input{160, 80, 40, 0x3C};
  color(&input, 4096, &input);
  CheckColor(input, CVECTOR{24, 192, 96, 0x3C});
  CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40",
                                          0x80041A01u) == nullptr);
}
