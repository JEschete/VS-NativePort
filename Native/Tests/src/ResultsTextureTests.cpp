#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "Ending/Results.hpp"
#include "Legacy/Psx.h"
#include "Menu/Results/CubePuzzleTextLive.hpp"
#include "Menu/Results/Results.h"
#include "Menu/Results/TimeAttack.hpp"
#include "Menu/Results/TimeAttackRatingLive.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/Render.hpp"
#include "Render/TextureRegionDrawLive.hpp"
#include "Render/UiPrimitives.hpp"

namespace {
struct ResultsTextureFixture {
  VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  void* saved_buffer = VS::Render::legacyPrimitiveBuffers[0];
  int saved_framebuffer = VS::Render::frameBuf;
  VS::Render::TextureRegion saved_ending_region =
      VS::Ending::resultAtlasRegions[12];
  int saved_time = VS::Menu::Results::timeTrialTime;
  std::vector<u_long> storage = std::vector<u_long>(
      32 * sizeof(POLY_GT4) / sizeof(u_long) + 2, 0xDEADBEEF);
  std::array<u_long, 32> ordering{};

  ResultsTextureFixture() {
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    VS::Render::frameBuf = 0;
    VS::Render::legacyPrimitiveBuffers[0] = storage.data() + 1;
    VS::Render::FrameScratchpad().primitive_cursor = storage.data() + 1;
    VS::Render::FrameScratchpad().ordering_table = ordering.data() + 16;
    VS::Ending::resultAtlasRegions[12] = {23, 45, 26, 19, 0x15, 0x345};
  }

  ~ResultsTextureFixture() {
    VS::Render::FrameScratchpad() = saved_frame;
    VS::Render::legacyPrimitiveBuffers[0] = saved_buffer;
    VS::Render::frameBuf = saved_framebuffer;
    VS::Ending::resultAtlasRegions[12] = saved_ending_region;
    VS::Menu::Results::timeTrialTime = saved_time;
  }

  POLY_GT4* Packets() {
    return reinterpret_cast<POLY_GT4*>(storage.data() + 1);
  }

  void CheckCount(std::size_t count) {
    CHECK(VS::Render::FrameScratchpad().primitive_cursor == Packets() + count);
    CHECK(storage.front() == 0xDEADBEEF);
    CHECK(storage.back() == 0xDEADBEEF);
    if (count != 0) {
      CHECK(nextPrim(&ordering[9]) == Packets() + count - 1);
      for (std::size_t index = 1; index < count; ++index) {
        CHECK(nextPrim(&Packets()[index]) == &Packets()[index - 1]);
      }
    }
  }
};

void CheckRegionPlacement(const POLY_GT4& packet,
                          const VS::Render::TextureRegion& region, int x,
                          int y) {
  // Copied C uses unsigned plain char, unlike the C++ test translation unit.
  const int width = static_cast<unsigned char>(region.width);
  const int height = static_cast<unsigned char>(region.height);
  CHECK(packet.x0 == x);
  CHECK(packet.y0 == y);
  CHECK(packet.x1 == x + width);
  CHECK(packet.y2 == y + height);
}

void CheckRegion(const POLY_GT4& packet,
                 const VS::Render::TextureRegion& region, int x, int y,
                 bool fading) {
  CheckRegionPlacement(packet, region, x, y);
  CHECK(packet.u0 == static_cast<unsigned char>(region.u));
  CHECK(packet.v0 == static_cast<unsigned char>(region.v));
  CHECK(packet.u1 == static_cast<unsigned char>(region.u + region.width));
  CHECK(packet.v2 == static_cast<unsigned char>(region.v + region.height));
  CHECK(packet.tpage == (region.texture_page | (fading ? 0x20 : 0)));
  CHECK(packet.clut == region.clut + (fading ? 1 : 0));
  CHECK(packet.code == 0x3E);
}
}  // namespace

TEST_CASE("Cube results draw both food-chain pieces and every rank from MENUF",
          "[menu][render][results-texture]") {
  const int fade = GENERATE(1, 7, 8, 9, 10, 11, 12, 13, 14, 64);
  const int rank = GENERATE(Catch::Generators::range(0, 16));
  ResultsTextureFixture fixture;
  VS::Menu::Results::LiveDrawFoodChain(160, 80, fade);
  VS::Menu::Results::LiveDrawCubePuzzleRank(160, 120, fade, rank);
  fixture.CheckCount(3);

  const auto* packets = fixture.Packets();
  const auto& left = Menu_Results_disMap[105];
  const auto& right = Menu_Results_disMap[106];
  const auto& label = Menu_Results_disMap[107 + rank];
  const int left_width = static_cast<unsigned char>(left.width);
  const int left_x = 160 - ((left_width + right.width) >> 1);
  CheckRegion(packets[0], left, left_x, 80, fade < 10);
  CheckRegion(packets[1], right, left_x + left_width, 80, fade < 10);
  CheckRegion(packets[2], label,
              160 - (static_cast<unsigned char>(label.width) >> 1), 120,
              fade < 10);
  CHECK(label.texture_page == getTPage(0, 0, 896, 256));
  CHECK(label.clut == getClut(896, 511));
  CHECK(left_width == 170);
  CHECK(right.width == 116);
}

TEST_CASE("Hidden cube results emit no textures",
          "[menu][render][results-texture]") {
  const int fade = GENERATE(-1, 0);
  ResultsTextureFixture fixture;
  const auto original_ordering = fixture.ordering;
  VS::Menu::Results::LiveDrawFoodChain(160, 80, fade);
  VS::Menu::Results::LiveDrawCubePuzzleRank(160, 120, fade, 0);
  fixture.CheckCount(0);
  CHECK(fixture.ordering == original_ordering);
}

namespace {
void CheckFadedPacket(const POLY_GT4& packet,
                      const std::array<CVECTOR, 2>& colors, int fade,
                      bool flash) {
  const auto expected_component = [fade, flash](int value) {
    if (fade < 8) return value * fade / 8;
    if (flash && fade == 8) return 192;
    if (flash && fade == 9) return 224;
    if (flash && fade < 14) {
      return (value * (fade - 10) + (14 - fade) * 192) / 4;
    }
    return value;
  };
  CHECK(packet.r0 == expected_component(colors[0].r));
  CHECK(packet.g0 == expected_component(colors[0].g));
  CHECK(packet.b0 == expected_component(colors[0].b));
  CHECK(packet.r1 == expected_component(colors[1].r));
  CHECK(packet.g1 == expected_component(colors[1].g));
  CHECK(packet.b1 == expected_component(colors[1].b));
}

void CheckGradientCorners(const POLY_GT4& packet) {
  CHECK(packet.r2 == packet.r0);
  CHECK(packet.g2 == packet.g0);
  CHECK(packet.b2 == packet.b0);
  CHECK(packet.r3 == packet.r1);
  CHECK(packet.g3 == packet.g1);
  CHECK(packet.b3 == packet.b1);
}
}  // namespace

TEST_CASE("Shared tint and flash fades retain each explicitly selected atlas",
          "[menu][ending][render][results-texture]") {
  const int fade = GENERATE(0, 1, 7, 8, 9, 10, 11, 12, 13, 14, 64);
  const bool flash = GENERATE(false, true);
  ResultsTextureFixture fixture;
  std::array<CVECTOR, 2> colors{{{128, 96, 64, 0}, {200, 180, 160, 0}}};
  colors[0].cd = static_cast<u_char>(fade);
  auto draw = flash ? VS::Render::DrawLiveTextureFlashFade
                    : VS::Render::DrawLiveTextureTintFade;
  draw(Menu_Results_disMap, 10, 20, 12, colors);
  draw(VS::Ending::resultAtlasRegions, 30, 40, 12, colors);
  draw(Menu_Results_disMap, 50, 60, 12, colors);
  fixture.CheckCount(fade == 0 ? 0 : 3);
  if (fade == 0) return;

  const bool fading = fade < (flash ? 10 : 8);
  CheckRegion(fixture.Packets()[0], Menu_Results_disMap[12], 10, 20, fading);
  CheckRegion(fixture.Packets()[1], VS::Ending::resultAtlasRegions[12], 30, 40,
              fading);
  CheckRegion(fixture.Packets()[2], Menu_Results_disMap[12], 50, 60, fading);
  for (int index = 0; index < 3; ++index) {
    CheckFadedPacket(fixture.Packets()[index], colors, fade, flash);
    CheckGradientCorners(fixture.Packets()[index]);
  }
}

namespace {
void CheckWipeStripGeometry(const POLY_GT4& packet,
                            const VS::Render::TextureRegion& region, int offset,
                            int width) {
  CHECK(packet.x0 == 10 + offset);
  CHECK(packet.x1 == 10 + offset + width);
  CHECK(packet.y0 == 20);
  CHECK(packet.y2 == 20 + region.height);
  CHECK(packet.u0 == static_cast<unsigned char>(region.u + offset));
  CHECK(packet.u1 == static_cast<unsigned char>(region.u + offset + width));
}

void CheckWipeStripFade(const POLY_GT4& packet,
                        const VS::Render::TextureRegion& region, int offset,
                        int width) {
  CHECK(packet.v0 == static_cast<unsigned char>(region.v));
  CHECK(packet.clut == region.clut + 1);
  CHECK(packet.tpage == (region.texture_page | 0x20));
  CHECK(packet.r0 == 128 * std::clamp(8 - offset, 0, 64) / 64);
  CHECK(packet.r1 == 128 * std::clamp(8 - offset - width, 0, 64) / 64);
}
}  // namespace

TEST_CASE("Time-attack rating wipes use MENUF for full and partial strips",
          "[menu][render][results-texture]") {
  const int rating = GENERATE(Catch::Generators::range(0, 5));
  const int fade = GENERATE(1, 64);
  constexpr std::array kTimes{0, 7680, 65536, 73216, 131072};
  ResultsTextureFixture fixture;
  VS::Menu::Results::timeTrialTime = kTimes[rating];
  VS::Menu::Results::LiveDrawTimeAttackRatingWipe(10, 20, fade);
  const auto& region = Menu_Results_disMap[78 + rating];
  const int count = fade == 64 ? 1 : (region.width + 11) / 12;
  fixture.CheckCount(count);
  if (fade == 64) {
    CheckRegion(fixture.Packets()[0], region, 10, 20, false);
    return;
  }

  for (int index = 0; index < count; ++index) {
    const auto& packet = fixture.Packets()[index];
    const int offset = index * 12;
    const int width = std::min(12, region.width - offset);
    CheckWipeStripGeometry(packet, region, offset, width);
    CheckWipeStripFade(packet, region, offset, width);
  }
}

TEST_CASE(
    "Wipes preserve ENDING selection after MENUF and skip unreached edges",
    "[menu][ending][render][results-texture]") {
  const int edge = GENERATE(10, 18, 600);
  ResultsTextureFixture fixture;
  std::array<CVECTOR, 2> colors{{{128, 96, 64, 0}, {200, 180, 160, 0}}};
  VS::Render::DrawLiveTextureWipe(Menu_Results_disMap, 10, 20, 12, colors,
                                  edge);
  const auto* ending_begin = static_cast<const POLY_GT4*>(
      VS::Render::FrameScratchpad().primitive_cursor);
  VS::Render::DrawLiveTextureWipe(VS::Ending::resultAtlasRegions, 10, 20, 12,
                                  colors, edge);
  if (edge == 10) {
    fixture.CheckCount(0);
  } else if (edge == 600) {
    fixture.CheckCount(2);
    CheckRegion(ending_begin[0], VS::Ending::resultAtlasRegions[12], 10, 20,
                false);
  } else {
    fixture.CheckCount(
        (static_cast<unsigned char>(Menu_Results_disMap[12].width) + 11) / 12 +
        3);
    CHECK(ending_begin[0].u0 == 23);
    CHECK(ending_begin[0].v0 == 45);
    CHECK(ending_begin[0].clut == 0x346);
    CHECK(ending_begin[0].tpage == 0x35);
    CHECK(ending_begin[2].x1 == 36);
    CHECK(ending_begin[2].u1 == 49);
  }
}
