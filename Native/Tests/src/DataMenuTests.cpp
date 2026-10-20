#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Audio/Playback/SoundPlayback.h"
#include "Core/Random.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Menu/Files.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/Text/CompactFont.h"
#include "Render/UiPrimitives.h"
#include "Save/MemoryCard/GameFiles.h"
#include "State/PlayTime.h"
#include "Title/SaveFiles.h"
}

namespace {
struct EmptySlotsFixture {
  u_int* saved_sfx_offsets = Audio_Playback_soundEffectProgramOffsets;
  u_short* saved_sfx_selectors = Audio_Playback_soundEffectBankSelectors;
  decltype(Input_buttonsPressed) saved_pressed = Input_buttonsPressed;
  decltype(Input_buttonRepeat) saved_repeat = Input_buttonRepeat;
  Save_CardFileInfoRecord* saved_summaries = Title_saveFileSummaries;
  u_short* saved_table = Menu_Files_textTable;
  char* saved_message = Menu_Files_memoryCardMessage;
  int saved_cursor = Menu_Files_selectCursorXy;
  std::array<Menu_Files_FileMenuElement, 10> saved_elements =
      std::to_array(Menu_Files_fileMenuElements);
  std::array<Save_CardFileInfoRecord, 5> summaries{};
  std::array<u_short, 2048> text{};
  std::array<u_int, 0x180> sfx_offsets{};
  std::array<u_short, 0x180> sfx_selectors{};

  EmptySlotsFixture() {
    for (auto& summary : summaries) {
      summary.summary.base.slotState = Save_CardFileSlotStatus_Available;
    }
    Title_saveFileSummaries = summaries.data();
    Menu_Files_textTable = text.data();
    // Input feedback needs a bound bank, even when no sound programs exist.
    sfx_offsets.fill(UINT32_MAX);
    Audio_Playback_soundEffectProgramOffsets = sfx_offsets.data();
    Audio_Playback_soundEffectBankSelectors = sfx_selectors.data();
    Input_buttonsPressed.allButtons = 0;
    Input_buttonRepeat = 0;
  }

  ~EmptySlotsFixture() {
    std::copy(saved_elements.begin(), saved_elements.end(),
              Menu_Files_fileMenuElements);
    Title_saveFileSummaries = saved_summaries;
    Menu_Files_textTable = saved_table;
    Menu_Files_memoryCardMessage = saved_message;
    Menu_Files_selectCursorXy = saved_cursor;
    Audio_Playback_soundEffectProgramOffsets = saved_sfx_offsets;
    Audio_Playback_soundEffectBankSelectors = saved_sfx_selectors;
    Input_buttonsPressed = saved_pressed;
    Input_buttonRepeat = saved_repeat;
  }

  void CheckEmptySlotInput() const {
    for (size_t index = 0; index < summaries.size(); ++index) {
      Input_buttonsPressed.allButtons = PADRright;
      CHECK(Menu_Files_UpdateLoadFiles(0) == 0);
      CHECK(Menu_Files_selectCursorXy != 0);
      Input_buttonsPressed.allButtons = 0;
      Input_buttonRepeat = PADLdown;
      CHECK(Menu_Files_UpdateLoadFiles(0) == 0);
      Input_buttonRepeat = 0;
    }
  }
};

struct ClockFixture {
  Render_FrameScratch saved_frame = Render_frameScratch;
  void* saved_buffer = Render_legacyPrimitiveBuffers[0];
  int saved_framebuffer = Render_frameBuf;
  State_PlayTime saved_time = State_playTime;
  u_int saved_random = Core_interfaceRandomState;
  std::array<u_char, 264> saved_trim{};
  std::array<u_char, 92> saved_widths{};
  std::array<u_char, 60> saved_indices{};
  std::vector<u_long> storage =
      std::vector<u_long>(0x20000 / sizeof(u_long) + 2, 0xDEADBEEF);
  std::array<u_long, 32> ordering{};

  ClockFixture() {
    std::copy_n(Render_compactGlyphLeftTrim, saved_trim.size(),
                saved_trim.begin());
    std::copy_n(Render_compactGlyphWidths, saved_widths.size(),
                saved_widths.begin());
    std::copy_n(Render_compactGlyphIndices, saved_indices.size(),
                saved_indices.begin());
    std::fill_n(Render_compactGlyphLeftTrim, saved_trim.size(), 0);
    std::fill_n(Render_compactGlyphWidths, saved_widths.size(), 0x66);
    for (size_t index = 0; index < saved_indices.size(); ++index) {
      Render_compactGlyphIndices[index] = static_cast<u_char>(index);
    }
    Render_frameBuf = 0;
    Render_legacyPrimitiveBuffers[0] = storage.data() + 1;
    Render_frameScratch.orderingTable = ordering.data() + 18;
    Render_frameScratch.overlayOrderingTable = ordering.data() + 2;
    Core_interfaceRandomState = 0;
    ResetPackets();
  }

  ~ClockFixture() {
    std::copy(saved_trim.begin(), saved_trim.end(),
              Render_compactGlyphLeftTrim);
    std::copy(saved_widths.begin(), saved_widths.end(),
              Render_compactGlyphWidths);
    std::copy(saved_indices.begin(), saved_indices.end(),
              Render_compactGlyphIndices);
    Render_frameScratch = saved_frame;
    Render_legacyPrimitiveBuffers[0] = saved_buffer;
    Render_frameBuf = saved_framebuffer;
    State_playTime = saved_time;
    Core_interfaceRandomState = saved_random;
  }

  void ResetPackets() {
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    Render_frameScratch.primitiveCursor = storage.data() + 1;
  }

  std::string Clock() const {
    std::vector<std::pair<int16_t, char>> glyphs;
    const void* packet = &ordering.back();
    for (size_t count = 0; packet != nullptr && count < 4096; ++count) {
      const auto* tag = static_cast<const P_TAG*>(packet);
      if (tag->len == 4) {
        const auto* words = static_cast<const u_long*>(packet);
        if ((words[2] >> 16) == 200) {
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
  State_playTime.all = (hours << 24) | (34u << 16) | (56u << 8);
  const auto saved = State_playTime.all;
  const auto expected =
      hours >= 100 ? "100:00:00:00" : std::format("{:02}:34:56:00", hours);
  Menu_Files_DrawPlayTime();
  CHECK(fixture.Clock() == expected);
  CHECK(State_playTime.all == saved);
  CHECK(fixture.storage.front() == 0xDEADBEEF);
  CHECK(fixture.storage.back() == 0xDEADBEEF);
  Menu_Files_ResetMenu7Statics();
  fixture.ResetPackets();
  Menu_Files_DrawPlayTime();
  CHECK(fixture.Clock() == expected);
  CHECK(State_playTime.all == saved);
}

TEST_CASE("Empty Load summaries remain unavailable and can be cancelled",
          "[menu][data][empty]") {
  EmptySlotsFixture fixture;
  const int port = GENERATE(1, 2);
  CHECK(Menu_Files_UpdateLoadFiles(port) == 0);
  CHECK(Menu_Files_UpdateLoadFiles(0) == 0);
  for (size_t index = 0; index < fixture.summaries.size(); ++index) {
    CHECK(Menu_Files_fileMenuElements[index + 5].slotUnavailable);
  }
  fixture.CheckEmptySlotInput();
  Input_buttonsPressed.allButtons = PADRdown;
  CHECK(Menu_Files_UpdateLoadFiles(0) == -1);
  CHECK(Menu_Files_selectCursorXy == 0);
}
