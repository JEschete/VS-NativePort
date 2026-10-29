#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Ending/Credits/EmbeddedArt.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Ending/ClearResults.h"
#include "Ending/Credits.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace {
constexpr auto kOffsets = std::to_array<std::size_t>({
    0xE0,
    0x5F3C0,
    0x68000,
    0x697E0,
    0x68400,
    0x6B120,
    0x6B2F0,
    0x7120,
    0x18B40,
    0x2A560,
    0x3BF80,
    0x4D9A0,
});

template <typename Integer>
void WriteInteger(unsigned char* destination, Integer value) {
  std::memcpy(destination, &value, sizeof value);
}

struct EndingArtFixture {
  std::vector<unsigned char> original{std::begin(Ending_Credits_glyphWidths),
                                      std::end(Ending_Credits_glyphWidths)};
  Ending_Credits_TimHeader* original_workspace =
      Ending_Credits_illustrationTimWorkspace;

  EndingArtFixture() {
    std::fill_n(Ending_Credits_glyphWidths,
                std::size(Ending_Credits_glyphWidths), 0);
    for (std::size_t index = 0; index < kOffsets.size(); ++index) {
      SetTim(index, 16, 1, 2, 1);
    }
  }
  ~EndingArtFixture() {
    std::copy(original.begin(), original.end(), Ending_Credits_glyphWidths);
    Ending_Credits_illustrationTimWorkspace = original_workspace;
  }

  void SetTim(std::size_t index, std::uint16_t colors, std::uint16_t rows,
              std::uint16_t width, std::uint16_t height) {
    auto* tim = Ending_Credits_glyphWidths + kOffsets[index];
    WriteInteger<std::uint32_t>(tim, 0x10);
    WriteInteger<std::uint32_t>(tim + 4, colors == 256 ? 9 : 8);
    auto* block = tim + 8;
    const auto palette_bytes = static_cast<std::uint32_t>(
        12 + static_cast<std::size_t>(colors) * rows * 2);
    WriteInteger(block, palette_bytes);
    WriteInteger(block + 8, colors);
    WriteInteger(block + 10, rows);
    std::fill_n(block + 12, palette_bytes - 12, 0x55);
    block += palette_bytes;
    const auto pixel_bytes = static_cast<std::uint32_t>(
        12 + static_cast<std::size_t>(width) * height * 2);
    WriteInteger(block, pixel_bytes);
    WriteInteger(block + 8, width);
    WriteInteger(block + 10, height);
    std::fill_n(block + 12, pixel_bytes - 12,
                static_cast<unsigned char>(index + 1));
  }
};

struct EndingFrameFixture {
  Render_FrameScratch saved_frame = Render_frameScratch;
  std::vector<u_long> packets = std::vector<u_long>(
      5 * sizeof(POLY_FT4) / sizeof(u_long) + 2, 0xDEADBEEF);
  std::array<u_long, 0x800> ordering{};

  EndingFrameFixture() {
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    Render_frameScratch.primitiveCursor = packets.data() + 1;
    Render_frameScratch.orderingTable = ordering.data();
  }
  ~EndingFrameFixture() { Render_frameScratch = saved_frame; }
};

void CheckRankStrip(const POLY_FT4& strip, int index, int brightness,
                    void* previous) {
  CHECK(strip.x0 == index * 64);
  CHECK(strip.x1 == (index + 1) * 64);
  CHECK(strip.y0 == 8);
  CHECK(strip.y2 == 232);
  CHECK(strip.r0 == brightness);
  CHECK(nextPrim(&strip) == previous);
}
}  // namespace

TEST_CASE(
    "Credits own complete palette and pixel blocks for every embedded TIM",
    "[ending][render][ending-art]") {
  EndingArtFixture fixture;
  REQUIRE(Ending_Credits_PrepareEmbeddedArt() == 1);
  for (unsigned index = 0; index < kOffsets.size(); ++index) {
    const auto* tim = Ending_Credits_EmbeddedArt(index);
    const auto* bytes = reinterpret_cast<const unsigned char*>(tim);
    CHECK(tim->identifier == 0x10);
    CHECK(bytes != Ending_Credits_glyphWidths + kOffsets[index]);
    CHECK(bytes[20] == 0x55);
    CHECK(bytes[64] == index + 1);
  }
  CHECK(Ending_Credits_illustrationTimWorkspace ==
        Ending_Credits_EmbeddedArt(7));
}

TEST_CASE("Credits preserve the full original72224-byte writable TIM workspace",
          "[ending][render][ending-art]") {
  EndingArtFixture fixture;
  fixture.SetTim(7, 256, 1, 160, 224);
  REQUIRE(Ending_Credits_PrepareEmbeddedArt() == 1);
  auto* bytes =
      reinterpret_cast<unsigned char*>(Ending_Credits_illustrationTimWorkspace);
  CHECK(bytes[72223] == 8);
  bytes[72223] = 0xA5;
  CHECK(Ending_Credits_glyphWidths[kOffsets[7] + 72223] == 8);
  REQUIRE(Ending_Credits_PrepareEmbeddedArt() == 1);
  CHECK(bytes[72223] == 8);
}

TEST_CASE("Credits reject invalid or oversized TIM blocks without a workspace",
          "[ending][render][ending-art]") {
  EndingArtFixture fixture;
  SECTION("Missing original TIM") {
    Ending_Credits_glyphWidths[kOffsets[0]] = 0;
  }
  SECTION("Truncated block header") {
    WriteInteger<std::uint32_t>(Ending_Credits_glyphWidths + kOffsets[0] + 8,
                                8);
  }
  SECTION("Block exceeds source extent") {
    WriteInteger<std::uint32_t>(Ending_Credits_glyphWidths + kOffsets[0] + 8,
                                0xFFFFFFFF);
  }
  SECTION("Pixels disagree with the block length") {
    WriteInteger<std::uint16_t>(Ending_Credits_glyphWidths + kOffsets[0] + 16,
                                256);
  }
  CHECK(Ending_Credits_PrepareEmbeddedArt() == 0);
  CHECK(Ending_Credits_illustrationTimWorkspace == nullptr);
}

TEST_CASE("Clear results retain five strips in the native penultimate bucket",
          "[ending][render][ending-results]") {
  const int fade = GENERATE(1, 32, 64, 100);
  EndingFrameFixture fixture;
  const auto original_ordering = fixture.ordering;
  auto* strips = reinterpret_cast<POLY_FT4*>(fixture.packets.data() + 1);
  void* previous = nextPrim(&fixture.ordering[0x7FE]);
  Ending_DrawRankBackground(fade);
  CHECK(fixture.packets.front() == 0xDEADBEEF);
  CHECK(fixture.packets.back() == 0xDEADBEEF);
  CHECK(Render_frameScratch.primitiveCursor == strips + 5);
  CHECK(nextPrim(&fixture.ordering[0x7FE]) == strips + 4);
  for (int index = 0; index < 5; ++index) {
    CheckRankStrip(strips[index], index, std::clamp(fade, 0, 64) * 2,
                   index == 0 ? previous : &strips[index - 1]);
  }
  for (std::size_t index = 0; index < fixture.ordering.size(); ++index) {
    if (index != 0x7FE) {
      CHECK(fixture.ordering[index] == original_ordering[index]);
    }
  }
}

TEST_CASE(
    "Invisible clear-results backgrounds leave the native frame untouched",
    "[ending][render][ending-results]") {
  const int fade = GENERATE(-1, 0);
  EndingFrameFixture fixture;
  const auto original_ordering = fixture.ordering;
  const void* original_cursor = Render_frameScratch.primitiveCursor;
  Ending_DrawRankBackground(fade);
  CHECK(fixture.packets.front() == 0xDEADBEEF);
  CHECK(fixture.packets.back() == 0xDEADBEEF);
  CHECK(Render_frameScratch.primitiveCursor == original_cursor);
  CHECK(fixture.ordering == original_ordering);
}
