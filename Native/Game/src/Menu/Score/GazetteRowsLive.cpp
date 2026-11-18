// Hand-maintained (tools/port/owned.csv).
// Live binding for the gazette rows (GazetteRows.hpp): implements the hosts
// over the frame's packets, the menu framework's rows and cursor memory, the
// settings and the score screen's strings.
#include "Menu/Score/GazetteRowsLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <string_view>
#include <vector>

#include "Battle/StatusEffects.h"
#include "Core/Log.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Score/GazetteRows.hpp"
#include "Menu/Score/ScoreScreen.h"
#include "Render/Display.h"
#include "Render/Render.h"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.h"

namespace VS::Menu::Score {
namespace {
static_assert(sizeof(Framework::MenuRow) == sizeof(Menu_Framework_MenuRow));
static_assert(offsetof(Framework::MenuRow, x) ==
              offsetof(Menu_Framework_MenuRow, x));
static_assert(offsetof(Framework::MenuRow, text) ==
              offsetof(Menu_Framework_MenuRow, text));

// The rows link before the overlay ordering table's third entry and the
// arrow before its second (238.c:79, 107).
constexpr std::size_t kRowOrderingEntry = 2;
constexpr std::size_t kArrowOrderingEntry = 1;
constexpr std::size_t kLineWords = 7;
constexpr std::uint32_t kLineTag = 0x06000000U;
constexpr std::uint32_t kLinkMask = 0xFFFFFFU;
// vs_getRGB0(Tile, 0, 0, 0).
constexpr int kShadowTileCode = 0x60000000;
constexpr std::size_t kSpriteDrawModeWord = 1;
constexpr std::size_t kSpriteTextureWord = 4;
// _menuText's halfwords: the offsets, then the strings.
constexpr std::size_t kMenuTextHalfwords = std::size(Menu_Score_menuText);
// A top menu row's two text entries: the name and no description.
constexpr std::size_t kTextPerRow = 2;
// D_800F4EE8.cursorMemories[2] and [3].
constexpr std::size_t kTopMenuCursorRow = 2;
constexpr std::size_t kTopMenuFirstRow = 3;

ScrollableRowAnimation live_animation;
TopMenuState live_top_menu;
std::uint64_t refusals = 0;

void Refuse(std::string_view what) {
  if (std::has_single_bit(++refusals)) {
    VS::Core::LogWarn("menu.score", "gazette rows: {} ({} refusals)", what,
                      refusals);
  }
}

[[nodiscard]] u_long* RowOrderingEntry() {
  auto* table = static_cast<u_long*>(Render_frameScratch.overlayOrderingTable);
  return table == nullptr ? nullptr : table + kRowOrderingEntry;
}

class LiveScrollableRow final : public ScrollableRowHost {
 public:
  LiveScrollableRow(Menu_Framework_MenuRow* row, u_long* before)
      : row_(row), before_(before) {}

  int FrameBufferIndex() override { return Render_frameBuf; }
  void DrawScrollArrow(const ScrollArrowSprite& arrow) override {
    u_long* packet = Render::QueueLiveSpriteWithTexturePage(
        arrow.brightness, static_cast<int>(arrow.position),
        static_cast<int>(arrow.size),
        before_ - (kRowOrderingEntry - kArrowOrderingEntry));
    packet[kSpriteTextureWord] = arrow.texture;
  }
  int DrawGlyph(std::uint32_t character, int x) override {
    return Framework::DrawLiveFeatheredRowCharacter(character, x, row_,
                                                    before_);
  }
  std::uint32_t GradientStopColor(std::uint32_t gradient,
                                  std::uint32_t color_index,
                                  int brightness) override {
    return static_cast<std::uint32_t>(
        Framework::LiveGradientStopColor(gradient, color_index, brightness));
  }
  void AddGradientLine(const GradientLine& line) override {
    u_long* packet = Render::ReserveLivePrimitiveWords(kLineWords);
    if (packet == nullptr) {
      Refuse("no room for a row background line");
      return;
    }
    packet[0] = (*before_ & kLinkMask) | kLineTag;
    std::ranges::copy(line, packet + 1);
    *before_ = LegacyGpu_Handle(packet);
    Render_frameScratch.primitiveCursor = packet + kLineWords;
  }
  void AddShadowTile(std::uint32_t position, std::uint32_t size) override {
    Render::InsertLiveThreeWordPrimitive(before_, kShadowTileCode,
                                         static_cast<int>(position),
                                         static_cast<int>(size));
  }
  void AddReflection(const ReflectionSprite& reflection) override {
    u_long* packet = Render::QueueLiveSprite(
        reflection.brightness, static_cast<int>(reflection.position),
        static_cast<int>(reflection.size), before_);
    packet[kSpriteDrawModeWord] = reflection.draw_mode;
    packet[kSpriteTextureWord] = reflection.texture;
  }
  void Refuse(const char* reason) override { Score::Refuse(reason); }

 private:
  Menu_Framework_MenuRow* row_;
  u_long* before_;
};

// _menuText's string at offset entry `choice`, or null when the entry or
// its offset is outside the table.
[[nodiscard]] char* TopMenuChoiceText(std::size_t choice) {
  if (choice >= kMenuTextHalfwords) return nullptr;
  const std::size_t offset = Menu_Score_menuText[choice];
  if (offset >= kMenuTextHalfwords) return nullptr;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return reinterpret_cast<char*>(&Menu_Score_menuText[offset]);
}

class LiveGazetteMenu final : public GazetteMenuHost {
 public:
  explicit LiveGazetteMenu(char** text = nullptr) : text_(text) {}

  void ListTopMenuChoices() override {
    listed_.assign(kTopMenuChoiceCount * kTextPerRow, nullptr);
    for (std::size_t choice = 0; choice < kTopMenuChoiceCount; ++choice) {
      listed_[choice * kTextPerRow] = TopMenuChoiceText(choice);
      if (listed_[choice * kTextPerRow] == nullptr) {
        Score::Refuse("top menu text offset outside _menuText");
      }
    }
    text_ = listed_.data();
  }
  std::uint8_t GazetteRow() override {
    return Battle_unusedMenu9CursorMemory.gazetteRow;
  }
  void SetTopMenuCursor(TopMenuCursor cursor) override {
    VS::Menu::Framework::menuMemory.cursorMemories[kTopMenuCursorRow] =
        static_cast<char>(cursor.row);
    VS::Menu::Framework::menuMemory.cursorMemories[kTopMenuFirstRow] =
        static_cast<char>(cursor.first_row);
  }
  std::uint8_t TopMenuCursorRow() override {
    return static_cast<std::uint8_t>(
        VS::Menu::Framework::menuMemory.cursorMemories[kTopMenuCursorRow]);
  }
  std::uint8_t CursorMemorySetting() override {
    return static_cast<std::uint8_t>(VS::Input::settings.cursorMemory);
  }
  void SetCursorMemorySetting(std::uint8_t setting) override {
    VS::Input::settings.cursorMemory = static_cast<char>(setting);
  }
  void ShowRows(std::int32_t row_info,
                std::span<const std::int32_t> row_types) override {
    std::vector<int> types(row_types.begin(), row_types.end());
    int no_rows = 0;
    Framework::CreateLiveMenuRows(static_cast<int>(row_types.size()), row_info,
                                  text_,
                                  types.empty() ? &no_rows : types.data());
  }
  std::int32_t SelectedRow() override { return Framework::LiveSelectedRow(); }
  void ClearMenuExcept(std::int32_t row) override {
    Framework::ClearLiveMenuExcept(row);
  }
  void FlyOutAndHoist(std::int32_t row, std::int32_t offset) override {
    Framework::FlyOutLiveRowsAndHoistSelection(row, offset);
  }
  std::int32_t MenuRowsIdle() override {
    return Framework::AreLiveMenuRowsIdle();
  }
  void Refuse(const char* reason) override { Score::Refuse(reason); }

 private:
  char** text_;
  std::vector<char*> listed_;
};
}  // namespace

// decomp: MENU9.PRG _renderScrollableMenuItem
// (src/MENU/MENU9.PRG/238.c:54), 80102A7C
void RenderLiveScrollableRow(Menu_Framework_MenuRow* row) {
  u_long* before = RowOrderingEntry();
  if (row == nullptr || before == nullptr) {
    Refuse("scrollable row without a row or an ordering table");
    return;
  }
  LiveScrollableRow host(row, before);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  RenderScrollableRow(host, live_animation,
                      *reinterpret_cast<Framework::MenuRow*>(row));
}

// decomp: MENU9.PRG _setRows (src/MENU/MENU9.PRG/238.c:196), 80102E10
void RestoreLiveTopMenuCursor(int row_count, int absolute_row, char* text[]) {
  if (text == nullptr && row_count > 0) {
    Refuse("top menu rows without text");
    return;
  }
  LiveGazetteMenu host(text);
  RestoreTopMenuCursor(host, row_count, absolute_row);
}

// decomp: MENU9.PRG _getSelectedRow (src/MENU/MENU9.PRG/238.c:228), 80102F0C
int LiveSelectedTopMenuRow() {
  LiveGazetteMenu host;
  return GetSelectedTopMenuRow(host);
}

// decomp: MENU9.PRG _topMenu (src/MENU/MENU9.PRG/238.c:1430), 80104BD0
int UpdateLiveTopMenu(int initialize) {
  LiveGazetteMenu host;
  return UpdateTopMenu(host, live_top_menu, initialize);
}

// decomp: MENU9.PRG _clearMenuExceptRow8 (src/MENU/MENU9.PRG/238.c:258),
// 80102FB8
void ClearLiveMenuExceptRow8() {
  LiveGazetteMenu host;
  ClearMenuExceptRow8(host);
}

// decomp: MENU9.PRG _menuReady (src/MENU/MENU9.PRG/238.c:264), 80102FD8
void PollLiveMenuReady() {
  LiveGazetteMenu host;
  PollMenuReady(host);
}

void ResetGazetteRowsLive() {
  live_animation = {};
  live_top_menu = {};
}

ScrollableRowAnimation& LiveScrollableRowAnimation() noexcept {
  return live_animation;
}

TopMenuState& LiveTopMenuState() noexcept { return live_top_menu; }

std::uint64_t LiveGazetteRowsRefusals() noexcept { return refusals; }
}  // namespace VS::Menu::Score
