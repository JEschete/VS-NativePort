#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Memory/HeapLive.hpp"
#include "Core/Text/TextCode.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Manual.hpp"
#include "Menu/ManualPageLive.hpp"
#include "Menu/ManualTopicsLive.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"

namespace {
__declspec(noinline) void ReuseReturnedStack() {
  volatile unsigned char storage[4096];
  for (unsigned index = 0; index < sizeof(storage); ++index) {
    storage[index] = 0x55;
  }
}

struct ManualFrameFixture {
  VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  void* saved_buffer = VS::Render::legacyPrimitiveBuffers[0];
  void* saved_ordering = VS::Render::legacyOrderingTables[0];
  int saved_framebuffer = VS::Render::frameBuf;
  int saved_scroll = VS::Menu::manual::scrollPosition;
  int saved_arrow = VS::Menu::manual::pageArrowAnimState;
  std::vector<u_long> storage = std::vector<u_long>(0x8002, 0xDEADBEEF);
  std::array<u_long, 64> ordering{};

  ManualFrameFixture() {
    VS::Render::frameBuf = 0;
    VS::Render::legacyPrimitiveBuffers[0] = storage.data() + 1;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    VS::Render::legacyOrderingTables[0] = ordering.data();
    VS::Render::FrameScratchpad().primitive_cursor = storage.data() + 1;
    VS::Render::FrameScratchpad().ordering_table = ordering.data() + 16;
    VS::Menu::manual::scrollPosition = 0;
    VS::Menu::manual::pageArrowAnimState = 0;
  }

  ~ManualFrameFixture() {
    VS::Render::FrameScratchpad() = saved_frame;
    VS::Render::legacyPrimitiveBuffers[0] = saved_buffer;
    VS::Render::legacyOrderingTables[0] = saved_ordering;
    VS::Render::frameBuf = saved_framebuffer;
    VS::Menu::manual::scrollPosition = saved_scroll;
    VS::Menu::manual::pageArrowAnimState = saved_arrow;
  }

  void CheckPackets(std::size_t bytes, unsigned expected) const {
    const auto* begin = reinterpret_cast<const std::byte*>(storage.data() + 1);
    CHECK(VS::Render::FrameScratchpad().primitive_cursor == begin + bytes);
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
      VS::Menu::manual::LiveDrawControlsBackground(16, 18, 96, 12);
      bytes = 11 * sizeof(LINE_G2) + sizeof(POLY_F4);
      packets = 12;
      break;
    case 1:
      VS::Menu::manual::LiveDrawPageBackground(8, 52, 310, 136, 0);
      bytes = sizeof(POLY_G4) + 3 * sizeof(u_long);
      packets = 2;
      break;
    case 2:
      VS::Menu::manual::LiveDrawUpperScrollFade();
      bytes = 49 * sizeof(POLY_FT4) + 16 * sizeof(TILE) + 3 * sizeof(u_long);
      packets = 66;
      break;
    case 3:
      VS::Menu::manual::LiveDrawLowerScrollFade();
      bytes = 49 * sizeof(POLY_FT4) + 16 * sizeof(TILE) + 3 * sizeof(u_long);
      packets = 66;
      break;
    default:
      VS::Menu::manual::LiveDrawPageArrow(
          operation == 4 ? VS::Menu::manual::PageArrow::kUp
                         : VS::Menu::manual::PageArrow::kDown);
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
    line[2] = static_cast<char>(VS::Core::TextCode::kTerminator);
  }
  // The Live page text reads help text only from a game heap payload.
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, std::size_t{16} * 1024>
          arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  auto* payload = static_cast<int*>(
      VS::Core::Memory::AllocateLiveFromHighEnd(sizeof(help)));
  REQUIRE(payload != nullptr);
  std::ranges::copy(help, payload);
  const auto saved_box = *VS::Menu::LiveTextBox(0);
  auto* saved_help = VS::Menu::manual::helpText;
  const int saved_scroll = VS::Menu::manual::scrollPosition;
  VS::Menu::manual::helpText = payload;
  VS::Menu::manual::scrollPosition = scroll;
  VS::Menu::LiveTextBox(0)->flags.speaker_texture = 32;
  VS::Menu::manual::LiveCopyPageText();
  ReuseReturnedStack();
  std::string expected{static_cast<char>(VS::Core::TextCode::kChunkSize),
                       static_cast<char>(VS::Core::TextCode::kNoChunking),
                       static_cast<char>(VS::Core::TextCode::kFontTable),
                       static_cast<char>(VS::Core::TextCode::kFontTableZero)};
  for (int index = scroll; index < std::min(scroll + 10, 12); ++index) {
    expected.push_back(static_cast<char>(17 + index));
    expected.push_back(static_cast<char>(40 + index));
    expected.push_back(static_cast<char>(VS::Core::TextCode::kNewline));
  }
  expected.back() = static_cast<char>(VS::Core::TextCode::kTerminator);
  CHECK(std::string(VS::Menu::LiveTextBox(0)->text, expected.size()) ==
        expected);
  *VS::Menu::LiveTextBox(0) = saved_box;
  VS::Menu::manual::helpText = saved_help;
  VS::Menu::manual::scrollPosition = saved_scroll;
}
