#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <format>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "ActorAssets.h"
#include "Actors/ActorRecord.h"
#include "Actors/WeaponModel.h"
#include "Audio/Playback/SoundPlayback.h"
#include "Core/Random.h"
#include "Core/Text/TextCode.h"
#include "DiscQueue.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Menu/Files.h"
#include "Menu/Score/Encyclopedia.h"
#include "Menu/Score/Records.h"
#include "ModelLoading.h"
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

TEST_CASE("Game Over fade links packets through native handles",
          "[render][menu][game-over]") {
  ClockFixture render;
  EmptySlotsFixture input;
  Menu_Files_UpdateGameOverChoices(1);
  for (int frame = 0; frame < 16; ++frame) {
    Input_buttonsPressed.allButtons = 0;
    CHECK(Menu_Files_UpdateGameOverChoices(0) == 0);
  }

  Input_buttonsPressed.allButtons = PADstart;
  CHECK(Menu_Files_UpdateGameOverChoices(0) == 0);
  Input_buttonsPressed.allButtons = 0;
  const auto* fade_packet = Render_frameScratch.primitiveCursor;
  CHECK(Menu_Files_UpdateGameOverChoices(0) == 0);
  CHECK(nextPrim(render.ordering.data() + 2) == fade_packet);
  CHECK(getaddr(render.ordering.data() + 2) == LegacyGpu_Handle(fade_packet));
  Menu_Files_UpdateGameOverChoices(1);
  (void)input;
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

TEST_CASE("Score list numbers retain every original digit and native link",
          "[render][menu][score][digits]") {
  const bool enemy = GENERATE(false, true);
  const bool dimmed = GENERATE(false, true);
  ClockFixture fixture;
  const int digits = enemy ? 3 : 2;
  const int rows = enemy ? 78 : 32;
  for (int number = 1; number <= rows; ++number) {
    fixture.ResetPackets();
    const auto* first =
        static_cast<const POLY_FT4*>(Render_frameScratch.primitiveCursor);
    const void* initial_next = nextPrim(fixture.ordering.data() + 2);
    if (enemy) {
      Menu_Score_DrawEnemyNumber(100, 50, number, dimmed);
    } else {
      Menu_Score_DrawTitleNumber(100, 50, number, dimmed);
    }
    CHECK(Render_frameScratch.primitiveCursor == first + digits);
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
  Menu_Framework_MenuRow row{};
  row.text[0] = static_cast<char>(Core_TextCode_PrintString);
  row.x = 60;
  row.y = 80;
  row.w = 100;
  row.backgroundWidth = 100;
  row.state = GENERATE(0, 1);
  row.isScrollable = GENERATE(1, 2);
  row.gradientState = GENERATE(0, 8);
  Menu_Score_RenderScrollableRow(&row);
  const auto begin = reinterpret_cast<uintptr_t>(fixture.storage.data() + 1);
  const auto end =
      reinterpret_cast<uintptr_t>(Render_frameScratch.primitiveCursor);
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
  CHECK(linked_packets == 24u + (row.isScrollable == 1) + (row.state == 1));
  CHECK(fixture.storage.front() == 0xDEADBEEF);
  CHECK(fixture.storage.back() == 0xDEADBEEF);
}

namespace {
struct ScoreViewerFixture : ClockFixture {
  decltype(Menu_Framework_menuItems) saved_items = Menu_Framework_menuItems;
  decltype(Menu_Score_monBinData) saved_enemies = Menu_Score_monBinData;
  decltype(Menu_Score_selectedEnemy) saved_selected = Menu_Score_selectedEnemy;
  decltype(Menu_Score_hideUi) saved_hidden = Menu_Score_hideUi;
  std::array<Menu_Framework_MenuRow, 40> items{};
  std::array<Menu_Score_EnemyEntry, 78> enemies{};

  ScoreViewerFixture() {
    Menu_Framework_menuItems = items.data();
    Menu_Score_monBinData = enemies.data();
    Menu_Score_hideUi = 0;
    for (auto& enemy : enemies) {
      enemy.name[2] = static_cast<char>(Core_TextCode_PrintString);
    }
  }

  ~ScoreViewerFixture() {
    Menu_Framework_menuItems = saved_items;
    Menu_Score_monBinData = saved_enemies;
    Menu_Score_selectedEnemy = saved_selected;
    Menu_Score_hideUi = saved_hidden;
  }
};
}  // namespace

TEST_CASE("Score encyclopedia viewer retains the complete original ID header",
          "[render][menu][score][viewer]") {
  const int selected = GENERATE(0, 77);
  ScoreViewerFixture fixture;
  Menu_Score_selectedEnemy = static_cast<int16_t>(selected);
  fixture.enemies[selected].classId = GENERATE(0, 1, 2, 3, 4, 5);
  Menu_Score_RenderEnemyViewer(8, 3);
  CHECK(fixture.Clock(160) == std::format("NO.   {:03}/078", selected + 1));
  CHECK(fixture.storage.front() == 0xDEADBEEF);
  CHECK(fixture.storage.back() == 0xDEADBEEF);
}

TEST_CASE("Score viewer banners use the native main ordering pointer",
          "[render][menu][score][banner]") {
  ClockFixture fixture;
  const auto* first =
      static_cast<const LINE_G2*>(Render_frameScratch.primitiveCursor);
  Menu_Score_DrawEnemyBanner(8, 18, 96, 12, 3);
  const size_t bytes =
      11 * sizeof(LINE_G2) + sizeof(POLY_F4) + 3 * sizeof(u_long);
  CHECK(Render_frameScratch.primitiveCursor ==
        reinterpret_cast<const u_char*>(first) + bytes);
  CHECK(nextPrim(fixture.ordering.data() + 25) ==
        reinterpret_cast<const u_char*>(first) + bytes - 3 * sizeof(u_long));
  CHECK(fixture.storage.front() == 0xDEADBEEF);
  CHECK(fixture.storage.back() == 0xDEADBEEF);
}

namespace {
struct ScoreAssetFixture {
  decltype(std::to_array(Content_legacyActorAssetRequests)) saved_requests =
      std::to_array(Content_legacyActorAssetRequests);
  decltype(Menu_Score_zudCdSlot) saved_disc = Menu_Score_zudCdSlot;
  decltype(Menu_Score_zudData) saved_data = Menu_Score_zudData;
  decltype(Menu_Score_zudBuffer) saved_buffer = Menu_Score_zudBuffer;
  int saved_variant = Menu_Score_modelVariant;
  int saved_palette = Menu_Score_paletteFlag;
  Content_DiscQueueSlot disc{};
  Menu_Score_ZudHeader header{};
  alignas(Actors_ActorRecord)
      std::array<u_char, 2 * sizeof(Actors_ActorRecord)> storage{};

  explicit ScoreAssetFixture(bool companion) {
    std::fill_n(Content_legacyActorAssetRequests, saved_requests.size(),
                Content_ActorAssetRequest{});
    disc.state = Content_DiscRequestState_Loaded;
    header.characterId = 13;
    header.weapon = 17;
    header.shield = 27;
    header.shpOffset = 48;
    header.weaponWepOffset = 128;
    header.shieldWepOffset = 256;
    header.battleSeqOffset = 384;
    header.commonSeqOffset = 512;
    header.battleSeqLen = header.commonSeqLen = 1;
    Menu_Score_zudCdSlot = &disc;
    Menu_Score_zudData = &header;
    Menu_Score_zudBuffer = storage.data();
    Menu_Score_modelVariant = companion ? 1 : 0;
    Menu_Score_paletteFlag = 1;
  }

  void CheckBodyRequest() {
    const auto& body = Content_legacyActorAssetRequests[0];
    CHECK(body.destination == storage.data());
    CHECK(std::bit_cast<u_int>(body.placement) == 0);
  }

  void CheckAttachmentRequests(bool companion) {
    const auto& attachment = Content_legacyActorAssetRequests[1];
    CHECK(attachment.destination ==
          storage.data() + sizeof(Actors_ActorRecord));
    CHECK(attachment.assetKind == (companion ? 1 : 4));
    CHECK(attachment.targetSlot == (companion ? 16 : 2));
    if (!companion) {
      const auto& shield = Content_legacyActorAssetRequests[2];
      CHECK(shield.destination == storage.data() + sizeof(Actors_ActorRecord) +
                                      sizeof(Actors_WeaponModel));
      CHECK(shield.targetSlot == 3);
    }
  }

  ~ScoreAssetFixture() {
    std::copy(saved_requests.begin(), saved_requests.end(),
              Content_legacyActorAssetRequests);
    Menu_Score_zudCdSlot = saved_disc;
    Menu_Score_zudData = saved_data;
    Menu_Score_zudBuffer = saved_buffer;
    Menu_Score_modelVariant = saved_variant;
    Menu_Score_paletteFlag = saved_palette;
  }
};
}  // namespace

TEST_CASE("Score model requests use native record strides and fresh placement",
          "[menu][score][assets]") {
  const bool companion = GENERATE(false, true);
  ScoreAssetFixture fixture(companion);
  REQUIRE(Menu_Score_LoadEnemyModelParts() == 1);
  REQUIRE(Content_FindFreeActorAssetRequest() == (companion ? 4 : 5));
  fixture.CheckBodyRequest();
  fixture.CheckAttachmentRequests(companion);
  CHECK(fixture.disc.state == Content_DiscRequestState_Free);
  CHECK(Menu_Score_zudCdSlot == nullptr);
}
