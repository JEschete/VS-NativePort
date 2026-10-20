#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Core/Text/TextCode.h"
#include "Input/Controller.h"
#include "Menu/Manual.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace {
__declspec(noinline) void ReuseReturnedStack() {
  volatile unsigned char storage[4096];
  for (unsigned index = 0; index < sizeof(storage); ++index) {
    storage[index] = 0x55;
  }
}

struct ManualFrameFixture {
  Render_FrameScratch saved_frame = Render_frameScratch;
  void* saved_buffer = Render_legacyPrimitiveBuffers[0];
  int saved_framebuffer = Render_frameBuf;
  int saved_scroll = Menu_Manual_scrollPosition;
  int saved_arrow = Menu_Manual_pageArrowAnimState;
  std::vector<u_long> storage = std::vector<u_long>(0x8002, 0xDEADBEEF);
  std::array<u_long, 64> ordering{};

  ManualFrameFixture() {
    Render_frameBuf = 0;
    Render_legacyPrimitiveBuffers[0] = storage.data() + 1;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    Render_frameScratch.primitiveCursor = storage.data() + 1;
    Render_frameScratch.orderingTable = ordering.data() + 16;
    Menu_Manual_scrollPosition = 0;
    Menu_Manual_pageArrowAnimState = 0;
  }

  ~ManualFrameFixture() {
    Render_frameScratch = saved_frame;
    Render_legacyPrimitiveBuffers[0] = saved_buffer;
    Render_frameBuf = saved_framebuffer;
    Menu_Manual_scrollPosition = saved_scroll;
    Menu_Manual_pageArrowAnimState = saved_arrow;
  }

  void CheckPackets(std::size_t bytes, unsigned expected) const {
    const auto* begin = reinterpret_cast<const std::byte*>(storage.data() + 1);
    CHECK(Render_frameScratch.primitiveCursor == begin + bytes);
    CHECK(storage.front() == 0xDEADBEEF);
    CHECK(storage.back() == 0xDEADBEEF);
    const void* packet = &ordering.back();
    unsigned count = 0;
    unsigned traversed = 0;
    while (packet != nullptr && traversed < 512) {
      ++traversed;
      const auto* tag = static_cast<const P_TAG*>(packet);
      if (tag->len != 0) ++count;
      packet = isendprim(packet) ? nullptr : nextPrim(packet);
    }
    CHECK(packet == nullptr);
    CHECK(count == expected);
  }
};
}  // namespace

TEST_CASE("Manual panels use full native frame pointers and packet links",
          "[menu][manual][render]") {
  const int operation = GENERATE(0, 1, 2, 3, 4, 5);
  ManualFrameFixture fixture;
  REQUIRE(reinterpret_cast<std::uintptr_t>(fixture.storage.data()) >
          UINT32_MAX);
  std::size_t bytes = 0;
  unsigned packets = 0;
  switch (operation) {
    case 0:
      Menu_Manual_DrawControlsBackground(16, 18, 96, 12);
      bytes = 11 * sizeof(LINE_G2) + sizeof(POLY_F4);
      packets = 12;
      break;
    case 1:
      Menu_Manual_DrawPageBackground(8, 52, 310, 136, 0);
      bytes = sizeof(POLY_G4) + 3 * sizeof(u_long);
      packets = 2;
      break;
    case 2:
      Menu_Manual_DrawUpperScrollFade();
      bytes = 49 * sizeof(POLY_FT4) + 16 * sizeof(TILE) + 3 * sizeof(u_long);
      packets = 66;
      break;
    case 3:
      Menu_Manual_DrawLowerScrollFade();
      bytes = 49 * sizeof(POLY_FT4) + 16 * sizeof(TILE) + 3 * sizeof(u_long);
      packets = 66;
      break;
    default:
      Menu_Manual_DrawPageArrow(operation == 4 ? Menu_Manual_ArrowType_Up
                                               : Menu_Manual_ArrowType_Down);
      bytes = sizeof(POLY_FT4);
      packets = 1;
      break;
  }
  fixture.CheckPackets(bytes, packets);
}

TEST_CASE("Manual page text remains alive until the textbox renders it",
          "[menu][manual][text]") {
  const int scroll = GENERATE(0, 10);
  std::array<int, 32> help{};
  auto* offsets = reinterpret_cast<u_short*>(help.data() + 4);
  offsets[0] = 12;
  for (int index = 0; index < 12; ++index) {
    offsets[index + 1] = static_cast<u_short>(14 + index * 2);
    auto* line = reinterpret_cast<char*>(offsets + offsets[index + 1]);
    line[0] = static_cast<char>(17 + index);
    line[1] = static_cast<char>(40 + index);
    line[2] = static_cast<char>(Core_TextCode_Terminator);
  }
  const auto saved_box = *Menu_TextBox(0);
  auto* saved_help = Menu_Manual_helpText;
  const int saved_scroll = Menu_Manual_scrollPosition;
  Menu_Manual_helpText = help.data();
  Menu_Manual_scrollPosition = scroll;
  Menu_TextBox(0)->flags.speakerTexture = 32;
  Menu_Manual_CopyPageText();
  ReuseReturnedStack();
  std::string expected{static_cast<char>(Core_TextCode_ChunkSize),
                       static_cast<char>(Core_TextCode_NoChunking),
                       static_cast<char>(Core_TextCode_FontTable),
                       static_cast<char>(Core_TextCode_FontTableZero)};
  for (int index = scroll; index < std::min(scroll + 10, 12); ++index) {
    expected.push_back(static_cast<char>(17 + index));
    expected.push_back(static_cast<char>(40 + index));
    expected.push_back(static_cast<char>(Core_TextCode_Newline));
  }
  expected.back() = static_cast<char>(Core_TextCode_Terminator);
  CHECK(std::string(Menu_TextBox(0)->string, expected.size()) == expected);
  *Menu_TextBox(0) = saved_box;
  Menu_Manual_helpText = saved_help;
  Menu_Manual_scrollPosition = saved_scroll;
}
