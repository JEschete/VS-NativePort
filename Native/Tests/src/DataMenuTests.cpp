#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Audio/SoundPlaybackLive.hpp"
#include "Core/RandomLive.hpp"
#include "Core/Text/TextCode.hpp"
#include "Input/Buttons.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/FilesLive.hpp"
#include "Menu/FilesLiveState.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/GazetteTerminalLiveFixture.hpp"
#include "Menu/Score/Encyclopedia.hpp"
#include "Menu/Score/GazetteCameraLive.hpp"
#include "Menu/Score/GazetteRowsLive.hpp"
#include "Menu/Score/GazetteStatisticsLive.hpp"
#include "Menu/Score/GazetteTerminalLive.hpp"
#include "Menu/Score/Records.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/Render.hpp"
#include "Render/Text/CompactFont.hpp"
#include "Render/UiPrimitives.hpp"
#include "Save/MemoryCard/SaveImage.hpp"
#include "State/PlayTime.hpp"
#include "Title/SaveFiles.hpp"

using VS::Legacy::kPadLdown;
using VS::Legacy::kPadRdown;
using VS::Legacy::kPadRright;
using VS::Legacy::kPadStart;

namespace {
namespace files = VS::Menu::Files;

files::Context LiveFiles() {
  return {.state = files::LiveFilesState(), .host = files::LiveFilesHost()};
}

struct EmptySlotsFixture {
  decltype(VS::Input::buttonsPressed) saved_pressed = VS::Input::buttonsPressed;
  decltype(VS::Input::buttonRepeat) saved_repeat = VS::Input::buttonRepeat;
  VS::Save::CardFileInfoRecord* saved_summaries = VS::Title::saveFileSummaries;
  u_short* saved_table = VS::Menu::Files::textTable;
  char* saved_message = VS::Menu::Files::memoryCardMessage;
  int saved_cursor = VS::Menu::Files::selectCursorXy;
  std::array<files::Element, files::kElementCount> saved_elements =
      files::LiveFilesState().elements;
  std::array<VS::Save::CardFileInfoRecord, 5> summaries{};
  std::array<u_short, 2048> text{};
  std::array<u_int, 0x180> sfx_offsets{};
  std::array<u_short, 0x180> sfx_selectors{};
  VS::Audio::Playback::ScopedLiveEffectTables sfx_tables{
      {.program_offsets = sfx_offsets,
       .bank_selectors = sfx_selectors,
       .program_data = {}}};

  EmptySlotsFixture() {
    for (auto& summary : summaries) {
      summary.summary.base.slot_state =
          VS::Save::CardFileSlotStatus::kAvailable;
    }
    VS::Title::saveFileSummaries = summaries.data();
    VS::Menu::Files::textTable = text.data();
    // Input feedback needs a bound bank, even when no sound programs exist.
    sfx_offsets.fill(UINT32_MAX);
    VS::Input::buttonsPressed = 0;
    VS::Input::buttonRepeat = 0;
  }

  ~EmptySlotsFixture() {
    files::LiveFilesState().elements = saved_elements;
    VS::Title::saveFileSummaries = saved_summaries;
    VS::Menu::Files::textTable = saved_table;
    VS::Menu::Files::memoryCardMessage = saved_message;
    VS::Menu::Files::selectCursorXy = saved_cursor;
    VS::Input::buttonsPressed = saved_pressed;
    VS::Input::buttonRepeat = saved_repeat;
  }

  void CheckEmptySlotInput() const {
    files::Context context = LiveFiles();
    for (size_t index = 0; index < summaries.size(); ++index) {
      VS::Input::buttonsPressed = kPadRright;
      CHECK(files::UpdateLoadFiles(context, 0) == 0);
      CHECK(VS::Menu::Files::selectCursorXy != 0);
      VS::Input::buttonsPressed = 0;
      VS::Input::buttonRepeat = kPadLdown;
      CHECK(files::UpdateLoadFiles(context, 0) == 0);
      VS::Input::buttonRepeat = 0;
    }
  }
};

struct ClockFixture {
  VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  void* saved_buffer = VS::Render::legacyPrimitiveBuffers[0];
  void* saved_table = VS::Render::legacyOrderingTables[0];
  int saved_framebuffer = VS::Render::frameBuf;
  VS::State::PlayTime saved_time = VS::State::playTime;
  u_int saved_random = VS::Core::interfaceRandomState;
  std::array<u_char, 264> saved_trim{};
  std::array<u_char, 92> saved_widths{};
  std::array<u_char, 60> saved_indices{};
  std::vector<u_long> storage =
      std::vector<u_long>(0x20000 / sizeof(u_long) + 2, 0xDEADBEEF);
  std::array<u_long, 32> ordering{};

  ClockFixture() {
    std::copy_n(VS::Render::compactGlyphLeftTrim, saved_trim.size(),
                saved_trim.begin());
    std::copy_n(VS::Render::compactGlyphWidths, saved_widths.size(),
                saved_widths.begin());
    std::copy_n(VS::Render::compactGlyphIndices, saved_indices.size(),
                saved_indices.begin());
    std::fill_n(VS::Render::compactGlyphLeftTrim, saved_trim.size(), 0);
    std::fill_n(VS::Render::compactGlyphWidths, saved_widths.size(), 0x66);
    for (size_t index = 0; index < saved_indices.size(); ++index) {
      VS::Render::compactGlyphIndices[index] = static_cast<u_char>(index);
    }
    VS::Render::frameBuf = 0;
    VS::Render::legacyPrimitiveBuffers[0] = storage.data() + 1;
    // The live chain lookups only accept the frame's own ordering table.
    VS::Render::legacyOrderingTables[0] = ordering.data();
    VS::Render::FrameScratchpad().ordering_table = ordering.data() + 18;
    VS::Render::FrameScratchpad().overlay_ordering_table = ordering.data() + 2;
    VS::Core::interfaceRandomState = 0;
    ResetPackets();
  }

  ~ClockFixture() {
    std::copy(saved_trim.begin(), saved_trim.end(),
              VS::Render::compactGlyphLeftTrim);
    std::copy(saved_widths.begin(), saved_widths.end(),
              VS::Render::compactGlyphWidths);
    std::copy(saved_indices.begin(), saved_indices.end(),
              VS::Render::compactGlyphIndices);
    VS::Render::FrameScratchpad() = saved_frame;
    VS::Render::legacyPrimitiveBuffers[0] = saved_buffer;
    VS::Render::legacyOrderingTables[0] = saved_table;
    VS::Render::frameBuf = saved_framebuffer;
    VS::State::playTime = saved_time;
    VS::Core::interfaceRandomState = saved_random;
  }

  void ResetPackets() {
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    VS::Render::FrameScratchpad().primitive_cursor = storage.data() + 1;
  }

  std::string Clock(int y = 200) const {
    std::vector<std::pair<int16_t, char>> glyphs;
    const void* packet = &ordering.back();
    for (size_t count = 0; packet != nullptr && count < 4096; ++count) {
      const auto* tag = static_cast<const P_TAG*>(packet);
      if (tag->len == 4) {
        const auto* words = static_cast<const u_long*>(packet);
        if ((words[2] >> 16) == static_cast<unsigned>(y)) {
          const unsigned uv = words[3];
          const unsigned index = (uv & 0xFE) + (((uv >> 8) & 0xFF) == 86);
          glyphs.emplace_back(static_cast<int16_t>(words[2] & 0xFFFF),
                              static_cast<char>(index + ' '));
        }
      }
      packet = isendprim(packet) ? nullptr : nextPrim(packet);
    }
    REQUIRE(packet == nullptr);
    std::sort(glyphs.begin(), glyphs.end());
    std::string result;
    for (const auto& [position, character] : glyphs) {
      (void)position;
      result += character;
    }
    return result;
  }
};
}  // namespace

TEST_CASE("Data play time uses mutable digits and retains overflow formatting",
          "[render][menu][data][clock]") {
  ClockFixture fixture;
  const unsigned hours = GENERATE(0u, 12u, 99u, 100u);
  VS::State::playTime = {.frames = 0,
                         .seconds = 56,
                         .minutes = 34,
                         .hours = static_cast<std::int8_t>(hours)};
  const auto saved = VS::State::playTime;
  const auto expected =
      hours >= 100 ? "100:00:00:00" : std::format("{:02}:34:56:00", hours);
  files::Context context = LiveFiles();
  files::DrawPlayTime(context);
  CHECK(fixture.Clock() == expected);
  CHECK(VS::State::playTime == saved);
  CHECK(fixture.storage.front() == 0xDEADBEEF);
  CHECK(fixture.storage.back() == 0xDEADBEEF);
  files::ResetLiveFilesStatics();
  fixture.ResetPackets();
  files::DrawPlayTime(context);
  CHECK(fixture.Clock() == expected);
  CHECK(VS::State::playTime == saved);
}

TEST_CASE("Game Over fade links packets through native handles",
          "[render][menu][game-over]") {
  ClockFixture render;
  EmptySlotsFixture input;
  files::Context context = LiveFiles();
  files::UpdateGameOverChoices(context, true);
  for (int frame = 0; frame < 16; ++frame) {
    VS::Input::buttonsPressed = 0;
    CHECK(files::UpdateGameOverChoices(context, false) == 0);
  }

  VS::Input::buttonsPressed = kPadStart;
  CHECK(files::UpdateGameOverChoices(context, false) == 0);
  VS::Input::buttonsPressed = 0;
  const auto* fade_packet = VS::Render::FrameScratchpad().primitive_cursor;
  CHECK(files::UpdateGameOverChoices(context, false) == 0);
  CHECK(nextPrim(render.ordering.data() + 2) == fade_packet);
  CHECK(getaddr(render.ordering.data() + 2) == LegacyGpu_Handle(fade_packet));
  files::UpdateGameOverChoices(context, true);
  (void)input;
}

TEST_CASE("Empty Load summaries remain unavailable and can be cancelled",
          "[menu][data][empty]") {
  EmptySlotsFixture fixture;
  const int port = GENERATE(1, 2);
  files::Context context = LiveFiles();
  CHECK(files::UpdateLoadFiles(context, port) == 0);
  CHECK(files::UpdateLoadFiles(context, 0) == 0);
  for (size_t index = 0; index < fixture.summaries.size(); ++index) {
    CHECK(files::LiveFilesState().elements[index + 5].slot_unavailable != 0);
  }
  fixture.CheckEmptySlotInput();
  VS::Input::buttonsPressed = kPadRdown;
  CHECK(files::UpdateLoadFiles(context, 0) == -1);
  CHECK(VS::Menu::Files::selectCursorXy == 0);
}

TEST_CASE("Score list numbers retain every original digit and native link",
          "[render][menu][score][digits]") {
  const bool enemy = GENERATE(false, true);
  const bool dimmed = GENERATE(false, true);
  ClockFixture fixture;
  const int digits = enemy ? 3 : 2;
  const int rows = enemy ? 78 : 32;
  for (int number = 1; number <= rows; ++number) {
    fixture.ResetPackets();
    const auto* first = static_cast<const POLY_FT4*>(
        VS::Render::FrameScratchpad().primitive_cursor);
    const void* initial_next = nextPrim(fixture.ordering.data() + 2);
    if (enemy) {
      VS::Menu::Score::DrawLiveEnemyNumber(100, 50, number, dimmed);
    } else {
      VS::Menu::Score::DrawLiveTitleNumber(100, 50, number, dimmed);
    }
    CHECK(VS::Render::FrameScratchpad().primitive_cursor == first + digits);
    CHECK(nextPrim(fixture.ordering.data() + 2) == first + digits - 1);
    const auto text =
        enemy ? std::format("{:03}", number) : std::format("{:02}", number);
    for (int index = 0; index < digits; ++index) {
      const auto& packet = first[index];
      const int u = (text[digits - index - 1] - '0') * 6;
      const int x = 100 + (enemy ? 24 : 19) - index * 5;
      const int shade = dimmed ? 32 : 128;
      CHECK((std::array<int, 14>{packet.code, packet.r0, packet.g0, packet.b0,
                                 packet.x0, packet.y0, packet.x3, packet.y3,
                                 packet.u0, packet.v0, packet.u3, packet.v3,
                                 packet.clut, packet.tpage} ==
             std::array<int, 14>{0x2E, shade, shade, shade, x, 46, x + 6, 56, u,
                                 0, u + 6, 10, getClut(dimmed ? 880 : 832, 223),
                                 getTPage(0, dimmed ? 1 : 0, 768, 0)}));
      CHECK(nextPrim(&packet) ==
            (index == 0 ? initial_next : first + index - 1));
    }
  }
  CHECK(fixture.storage.front() == 0xDEADBEEF);
  CHECK(fixture.storage.back() == 0xDEADBEEF);
}

TEST_CASE("Score scroll fades keep every packet in its native ordering chain",
          "[render][menu][score][scroll]") {
  ClockFixture fixture;
  using VS::Menu::Framework::RowState;
  VS::Menu::Framework::MenuRow row{};
  row.text[0] = static_cast<std::uint8_t>(VS::Core::TextCode::kPrintString);
  row.x = 60;
  row.y = 80;
  row.w = 100;
  row.background_width = 100;
  row.state = GENERATE(RowState::kInactive, RowState::kIdle);
  row.is_scrollable = GENERATE(1, 2);
  row.gradient_state = GENERATE(0, 8);
  VS::Menu::Score::RenderLiveScrollableRow(&row);
  const auto begin = reinterpret_cast<uintptr_t>(fixture.storage.data() + 1);
  const auto end = reinterpret_cast<uintptr_t>(
      VS::Render::FrameScratchpad().primitive_cursor);
  const auto table = reinterpret_cast<uintptr_t>(fixture.ordering.data());
  const void* packet = &fixture.ordering.back();
  size_t visited = 0;
  size_t linked_packets = 0;
  while (packet != nullptr && visited++ < 256) {
    const auto address = reinterpret_cast<uintptr_t>(packet);
    if (address >= begin && address < end) ++linked_packets;
    REQUIRE(((address >= begin && address < end) ||
             (address >= table && address < table + sizeof fixture.ordering)));
    packet = isendprim(packet) ? nullptr : nextPrim(packet);
  }
  CHECK(packet == nullptr);
  CHECK(linked_packets ==
        24u + (row.is_scrollable == 1) + (row.state == RowState::kIdle));
  CHECK(fixture.storage.front() == 0xDEADBEEF);
  CHECK(fixture.storage.back() == 0xDEADBEEF);
}

namespace {
struct ScoreViewerFixture : gazette_terminal_live_tests::TerminalFixture {
  ClockFixture clock;

  ScoreViewerFixture() { BorrowMon(); }
};
}  // namespace

TEST_CASE("Score encyclopedia viewer retains the complete original ID header",
          "[render][menu][score][viewer]") {
  const int selected = GENERATE(0, 77);
  ScoreViewerFixture fixture;
  VS::Menu::Score::selectedEnemy = static_cast<int16_t>(selected);
  const auto enemy =
      fixture.MonBytes().subspan(static_cast<std::size_t>(selected) *
                                     gazette_terminal_live_tests::kEnemyStride,
                                 gazette_terminal_live_tests::kEnemyStride);
  enemy[2] = static_cast<std::byte>(GENERATE(0, 1, 2, 3, 4, 5));
  enemy[3] = std::byte{};
  VS::Menu::Score::RenderLiveEnemyViewer(8, 3);
  CHECK(fixture.clock.Clock(160) ==
        std::format("NO.   {:03}/078", selected + 1));
  CHECK(fixture.clock.storage.front() == 0xDEADBEEF);
  CHECK(fixture.clock.storage.back() == 0xDEADBEEF);
}

TEST_CASE("Score viewer banners use the native main ordering pointer",
          "[render][menu][score][banner]") {
  ClockFixture fixture;
  const auto* first = static_cast<const LINE_G2*>(
      VS::Render::FrameScratchpad().primitive_cursor);
  VS::Menu::Score::DrawLiveEnemyBanner(8, 18, 96, 12, 3);
  const size_t bytes =
      11 * sizeof(LINE_G2) + sizeof(POLY_F4) + 3 * sizeof(u_long);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        reinterpret_cast<const u_char*>(first) + bytes);
  CHECK(nextPrim(fixture.ordering.data() + 25) ==
        reinterpret_cast<const u_char*>(first) + bytes - 3 * sizeof(u_long));
  CHECK(fixture.storage.front() == 0xDEADBEEF);
  CHECK(fixture.storage.back() == 0xDEADBEEF);
}
