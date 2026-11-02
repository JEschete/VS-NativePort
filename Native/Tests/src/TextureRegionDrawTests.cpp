#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"
#include "Render/TextureRegionDraw.hpp"
#include "Render/TextureRegionDrawLive.hpp"

extern "C" {
#include "Ending/ClearResults.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace {
using VS::Render::QuadColor;
using VS::Render::RegionColor;
using VS::Render::RegionQuad;
using VS::Render::TextureRegion;

class RecordingTarget final : public VS::Render::RegionQuadTarget {
 public:
  void Draw(const RegionQuad& quad) override { quads.push_back(quad); }
  std::vector<RegionQuad> quads;
};

constexpr TextureRegion kRegion{250, 240, 30, 20, 0x15, 0x345};

std::array<RegionColor, 2> Colors(int step) {
  return {{{128, 96, 64, static_cast<std::uint8_t>(step)},
           {200, 180, 160, static_cast<std::uint8_t>(step)}}};
}
}  // namespace

TEST_CASE("Region tint fade scales colors and selects the fade palette",
          "[render][texture-region]") {
  RecordingTarget target;
  VS::Render::DrawTextureTintFade(kRegion, 10, 20, Colors(0), target);
  CHECK(target.quads.empty());

  VS::Render::DrawTextureTintFade(kRegion, 10, 20, Colors(4), target);
  REQUIRE(target.quads.size() == 1);
  const RegionQuad& faded = target.quads[0];
  CHECK(faded.left_color == QuadColor{64, 48, 32});
  CHECK(faded.right_color == QuadColor{100, 90, 80});
  CHECK(faded.clut == 0x346);
  CHECK(faded.texture_page == 0x35);
}

TEST_CASE("Region tint fade ends at the colors and the region palette",
          "[render][texture-region]") {
  RecordingTarget target;
  VS::Render::DrawTextureTintFade(kRegion, 10, 20, Colors(8), target);
  REQUIRE(target.quads.size() == 1);
  CHECK(target.quads[0].left_color == QuadColor{128, 96, 64});
  CHECK(target.quads[0].clut == 0x345);
  CHECK(target.quads[0].texture_page == 0x15);
}

TEST_CASE("Region quads wrap texel coordinates within the page",
          "[render][texture-region]") {
  RecordingTarget target;
  VS::Render::DrawTextureTintFade(kRegion, 300, 400, Colors(8), target);
  REQUIRE(target.quads.size() == 1);
  const RegionQuad& quad = target.quads[0];
  CHECK(quad.left == 300);
  CHECK(quad.right == 330);
  CHECK(quad.top == 400);
  CHECK(quad.bottom == 420);
  CHECK(quad.u_left == 250);
  CHECK(quad.u_right == 24);
  CHECK(quad.v_top == 240);
  CHECK(quad.v_bottom == 4);
}

TEST_CASE("Region flash steps through grey and blends back to the colors",
          "[render][texture-region]") {
  const int step = GENERATE(8, 9, 10, 12, 13, 14);
  RecordingTarget target;
  VS::Render::DrawTextureFlashFade(kRegion, 0, 0, Colors(step), target);
  REQUIRE(target.quads.size() == 1);
  const RegionQuad& quad = target.quads[0];
  const auto expected = [step](int component) {
    if (step == 8) return 192;
    if (step == 9) return 224;
    if (step < 14) return VS::Render::BlendFlashComponent(component, step - 10);
    return component;
  };
  CHECK(quad.left_color.r == expected(128));
  CHECK(quad.right_color.b == expected(160));
  CHECK(quad.clut == (step < 10 ? 0x346 : 0x345));
  CHECK(quad.texture_page == (step < 10 ? 0x35 : 0x15));
}

TEST_CASE("Flash blend weights mix toward the light grey",
          "[render][texture-region]") {
  CHECK(VS::Render::BlendFlashComponent(0, 0) == 192);
  CHECK(VS::Render::BlendFlashComponent(255, 4) == 255);
  CHECK(VS::Render::BlendFlashComponent(128, 2) == 160);
  CHECK(VS::Render::BlendFlashComponent(200, 3) == 198);
}

namespace {
std::vector<RegionQuad> StraddlingWipe() {
  RecordingTarget target;
  VS::Render::DrawTextureWipe(kRegion, 10, 20, Colors(0), 30, target);
  return target.quads;
}
}  // namespace

TEST_CASE("Region wipe splits at its edge into texel strips",
          "[render][texture-region]") {
  const std::vector<RegionQuad> quads = StraddlingWipe();
  REQUIRE(quads.size() == 3);
  CHECK(quads[0].left == 10);
  CHECK(quads[0].right == 22);
  CHECK(quads[2].left == 34);
  CHECK(quads[2].right == 40);
  CHECK(quads[0].u_left == 250);
  CHECK(quads[1].u_left == 6);
  CHECK(quads[2].u_right == 24);
}

TEST_CASE("Region wipe strips fade toward the edge with the fade palette",
          "[render][texture-region]") {
  const std::vector<RegionQuad> quads = StraddlingWipe();
  REQUIRE(quads.size() == 3);
  CHECK(quads[0].left_color.r == 128 * 20 / 64);
  CHECK(quads[0].right_color.r == 200 * 8 / 64);
  CHECK(quads[1].right_color.r == 0);
  CHECK(quads[2].clut == 0x346);
  CHECK(quads[2].texture_page == 0x35);
}

TEST_CASE(
    "Region wipe draws whole before its feather and nothing past its edge",
    "[render][texture-region]") {
  RecordingTarget target;
  VS::Render::DrawTextureWipe(kRegion, 10, 20, Colors(0), 30 + 75, target);
  REQUIRE(target.quads.size() == 1);
  CHECK(target.quads[0].clut == 0x345);
  target.quads.clear();
  VS::Render::DrawTextureWipe(kRegion, 30, 20, Colors(0), 30, target);
  CHECK(target.quads.empty());
}

namespace {
struct LiveRegionFixture {
  Render_FrameScratch saved_frame = Render_frameScratch;
  void* saved_buffer = Render_legacyPrimitiveBuffers[0];
  int saved_framebuffer = Render_frameBuf;
  std::vector<u_long> storage =
      std::vector<u_long>(4 * sizeof(POLY_GT4) / sizeof(u_long), 0);
  std::array<u_long, 16> ordering{};
  std::array<Render_TextureRegion, 2> atlas{
      {{1, 2, 3, 4, 0x15, 0x345}, {5, 6, 7, 8, 0x16, 0x346}}};

  LiveRegionFixture() {
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    Render_frameBuf = 0;
    Render_legacyPrimitiveBuffers[0] = storage.data();
    Render_frameScratch.primitiveCursor = storage.data();
    Render_frameScratch.orderingTable = ordering.data() + 8;
  }

  ~LiveRegionFixture() {
    Render_frameScratch = saved_frame;
    Render_legacyPrimitiveBuffers[0] = saved_buffer;
    Render_frameBuf = saved_framebuffer;
  }

  [[nodiscard]] bool Drew() const {
    return Render_frameScratch.primitiveCursor != storage.data();
  }
};
}  // namespace

TEST_CASE("Live region draws reject out-of-range regions and short colors",
          "[render][texture-region]") {
  LiveRegionFixture fixture;
  const std::array<CVECTOR, 2> colors{{{128, 96, 64, 8}, {200, 180, 160, 8}}};
  VS::Render::DrawLiveTextureTintFade(fixture.atlas, 0, 0, 2, colors);
  VS::Render::DrawLiveTextureFlashFade(fixture.atlas, 0, 0, -1, colors);
  VS::Render::DrawLiveTextureWipe(fixture.atlas, 0, 0, 2, colors, 600);
  VS::Render::DrawLiveTextureTintFade(fixture.atlas, 0, 0, 1,
                                      std::span(colors).first(1));
  CHECK_FALSE(fixture.Drew());

  VS::Render::DrawLiveTextureTintFade(fixture.atlas, 0, 0, 1, colors);
  REQUIRE(fixture.Drew());
  const auto* packet =
      reinterpret_cast<const POLY_GT4*>(fixture.storage.data());
  CHECK(packet->u0 == 5);
  CHECK(packet->clut == 0x346);
  CHECK(nextPrim(&fixture.ordering[1]) == packet);
}

TEST_CASE("Live region draws stop at a missing primitive buffer",
          "[render][texture-region]") {
  LiveRegionFixture fixture;
  const std::array<CVECTOR, 2> colors{{{128, 96, 64, 8}, {200, 180, 160, 8}}};
  Render_legacyPrimitiveBuffers[0] = nullptr;
  VS::Render::DrawLiveTextureTintFade(fixture.atlas, 0, 0, 0, colors);
  CHECK_FALSE(fixture.Drew());
}

TEST_CASE("ENDING congratulations tint holds the third color its draw reads",
          "[render][texture-region][ending]") {
  // D4.c:1970 draws glyph 0x17 from &D_800DBA7C[1]; issue #259.
  STATIC_REQUIRE(std::size(Ending_congratulationsTint) == 3);
  STATIC_REQUIRE(sizeof Ending_congratulationsTint == 12);
}
