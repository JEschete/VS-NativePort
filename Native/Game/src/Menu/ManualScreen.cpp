// Hand-maintained (tools/port/owned.csv).
// The manual screen's lifecycle and help pages (MENUE.PRG 494.c:132-448).
#include "Menu/ManualScreen.hpp"

#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <string_view>

#include "Core/Math/ScalarMath.hpp"
#include "Render/UiPackets.hpp"

namespace VS::Menu::manual {
namespace {
using Core::Math::WrappingAdd;

// vs_mainMenu_toggleBackgroundFade's argument.
constexpr std::int32_t kBackgroundFadeIn = 1;
constexpr std::int32_t kBackgroundFadeOut = 0;
constexpr std::int32_t kNoTopicMenuInit = 0;
constexpr std::int32_t kTopicMenuInit = 1;

constexpr std::string_view kTurnPageText = "TURN    PAGE";
constexpr std::string_view kMenuText = "MENU";
constexpr std::string_view kPageText = "PAGE";

struct IconPosition {
  std::int32_t x = 0;
  std::int32_t y = 0;
};
constexpr IconPosition kTurnPageIcon{.x = 8, .y = 16};
constexpr IconPosition kMenuIcon{.x = 8, .y = 34};
// vs_battle_renderTextRawColor's position word (vs_getXY).
constexpr std::int32_t kTurnPageTextXY = Render::PackScreenWord(0x1C, 0x12);
constexpr std::int32_t kMenuTextXY = Render::PackScreenWord(0x1C, 0x24);
constexpr std::int32_t kPageTextXY = Render::PackScreenWord(0xD8, 0xC4);
constexpr std::int32_t kPageNumberXY = Render::PackScreenWord(0x118, 0xC4);
constexpr std::int32_t kLastPageNumberXY = Render::PackScreenWord(0x130, 0xC4);

void FreeHeldBuffer(HelpPagesHost& host, HelpFile file) {
  if (host.HelpBufferHeld(file)) {
    host.FreeHelpBuffer(file);
  }
}

// _showMenu's getSelection step.
void SelectTopic(HelpPagesHost& host, HelpPagesState& state) {
  const std::int32_t row = host.UpdateTopicMenu(kNoTopicMenuInit);
  if (row == kNoTopicSelected) {
    return;
  }
  if (row < 0) {
    state.step =
        row == kTopicMenuExitToGame ? kHelpLeaveToGame : kHelpLeaveToMenu;
  } else if (row >= kTopicCount) {
    // The original would read a file pair past _helpFileCdFiles.
    host.Refuse("Manual topic row past the help files");
    state.step = kHelpLeaveToMenu;
  } else {
    state.step = kHelpLoadAssets;
    FreeHeldBuffer(host, HelpFile::kText);
    FreeHeldBuffer(host, HelpFile::kAssets);
    host.AllocateHelpBuffer(HelpFile::kText, row);
    host.AllocateHelpBuffer(HelpFile::kAssets, row);
    host.RequestHelpFile(HelpFile::kAssets, row);
    host.RequestHelpFile(HelpFile::kText, row);
  }
  const TopicCursor cursor = host.TopicMenuCursor();
  host.SetCurrentManual(static_cast<std::uint16_t>(cursor.row + cursor.scroll));
  host.DismissInformationBox();
}

// _showMenu's loadSubMenu and initSubMenu steps wait for the loaded state
// (494.c:308-311 and 322-325); a failed read never reaches it, so the
// original waits forever with no way out. Native cancels the topic's
// outstanding reads, frees their buffers and shows the topic list again, as
// leaving a page does (#408).
void AbandonTopic(HelpPagesHost& host, HelpPagesState& state,
                  std::initializer_list<HelpFile> pending) {
  host.Refuse("Manual help file failed to load");
  for (const HelpFile file : pending) {
    host.CancelHelpFile(file);
    host.ReleaseHelpFile(file);
    FreeHeldBuffer(host, file);
    host.ForgetHelpBuffer(file);
  }
  state.step = kHelpListTopics;
}

// _showMenu's loadSubMenu step.
void LoadAssets(HelpPagesHost& host, HelpPagesState& state) {
  const std::int16_t assets = host.HelpFileState(HelpFile::kAssets);
  if (assets == kHelpFileFailed) {
    AbandonTopic(host, state, {HelpFile::kAssets, HelpFile::kText});
    return;
  }
  if (assets != kHelpFileLoaded) {
    return;
  }
  host.ReleaseHelpFile(HelpFile::kAssets);
  if (const auto header = host.HelpAssets(); header.has_value()) {
    host.UploadSpriteTiles(kHelpAssetHeaderBytes, header->sprite_tiles);
    // The CLUTs follow the tiles; the offset wraps like the original's.
    host.UploadCluts(
        (header->sprite_tiles << kHelpSpriteTileShift) + kHelpAssetHeaderBytes,
        header->cluts);
  } else {
    // The original would read the header through a null pointer.
    host.Refuse("Manual help assets loaded without an asset buffer");
  }
  FreeHeldBuffer(host, HelpFile::kAssets);
  host.ForgetHelpBuffer(HelpFile::kAssets);
  state.step = kHelpLoadText;
}

// _showMenu's initSubMenu step.
void LoadText(HelpPagesHost& host, HelpPagesState& state) {
  const std::int16_t text = host.HelpFileState(HelpFile::kText);
  if (text == kHelpFileFailed) {
    AbandonTopic(host, state, {HelpFile::kText});
    return;
  }
  if (text != kHelpFileLoaded) {
    return;
  }
  host.ReleaseHelpFile(HelpFile::kText);
  host.InitTextBox(kPageTextPlacement);
  const std::uint16_t manual = host.CurrentManual();
  if (manual < kTopicCount) {
    state.scroll = host.ScrollMemory(manual);
  } else {
    // The original would read past the scroll memories.
    host.Refuse("Manual topic past the scroll memories");
    state.scroll = 0;
  }
  state.line_count = host.LastPageLineOffset();
  state.step = kHelpShowPage;
  state.load_counter = 0;
}

// Square held: Up on the first page jumps to the last, Down on the last page
// to the first, and repeated Up or Down turns one page. Returns the shown
// page.
std::int32_t TurnWholePage(HelpPagesHost& host, HelpPagesState& state,
                           std::uint32_t pressed, std::int32_t page,
                           std::int32_t last_page) {
  const std::uint32_t repeated = host.RepeatedButtons();
  if ((pressed & kButtonUp) != 0 && page == 0 && last_page > 0) {
    page = last_page;
  } else if ((pressed & kButtonDown) != 0 && page == last_page &&
             last_page > 0) {
    page = 0;
  } else if ((repeated & kButtonUp) != 0 && page > 0) {
    --page;
  } else if ((repeated & kButtonDown) != 0 && page < last_page) {
    ++page;
  } else {
    return page;
  }
  state.scroll = page * kPageLines;
  host.PlaySfx(kMenuSfxGroup, kSfxPageTurn);
  return page;
}

// Square up: Up at the top jumps to the last page, Down there back to the
// top, and repeated Up or Down scrolls one line. The drawn page number keeps
// the frame's first scroll position, as the original's does.
void ScrollLine(HelpPagesHost& host, HelpPagesState& state,
                std::uint32_t pressed) {
  const std::uint32_t repeated = host.RepeatedButtons();
  if ((pressed & kButtonUp) != 0 && state.scroll == 0 && state.line_count > 0) {
    state.scroll = state.line_count;
  } else if ((pressed & kButtonDown) != 0 && state.scroll == state.line_count &&
             state.scroll > 0) {
    state.scroll = 0;
  } else if ((repeated & kButtonUp) != 0 && state.scroll > 0) {
    --state.scroll;
  } else if ((repeated & kButtonDown) != 0 && state.scroll < state.line_count) {
    ++state.scroll;
  } else {
    return;
  }
  host.PlaySfx(kMenuSfxGroup, kSfxPageTurn);
}

void DrawControls(HelpPagesHost& host, std::int32_t page,
                  std::int32_t last_page) {
  host.DrawButtonIcon(Framework::ButtonIcon::kSquare, kTurnPageIcon.x,
                      kTurnPageIcon.y, kControlsLayer);
  host.DrawText(kTurnPageText, kTurnPageTextXY, kTextColor, kControlsLayer);
  host.DrawControlsBackground(kTurnPageControls);
  host.DrawButtonIcon(Framework::ButtonIcon::kCircle, kMenuIcon.x, kMenuIcon.y,
                      kControlsLayer);
  host.DrawText(kMenuText, kMenuTextXY, kTextColor, kControlsLayer);
  host.DrawControlsBackground(kMenuControls);
  host.DrawText(kPageText, kPageTextXY, kTextColor, kPageNumberLayer);
  // Pages count from the scroll position's tens, so neither sum overflows.
  host.DrawText(std::format("#{}/", page + 1), kPageNumberXY, kTextColor,
                kPageNumberLayer);
  host.DrawText(std::format("#{}", last_page + 1), kLastPageNumberXY,
                kTextColor, kPageNumberLayer);
}

// The page frame after the controls: the background, the scroll fades and,
// once the page has waited kPageContentDelay frames, its illustrations.
void DrawPage(HelpPagesHost& host, HelpPagesState& state) {
  host.DrawPageBackground(kPageBackground, kPageBackgroundColor);
  state.arrow_frame = WrappingAdd(state.arrow_frame, 1) & kArrowFrameMask;
  if (state.scroll > 0) {
    host.DrawUpperScrollFade();
  }
  if (state.scroll < state.line_count) {
    host.DrawLowerScrollFade();
  }
  if (state.load_counter < kPageContentDelay) {
    ++state.load_counter;
  } else {
    host.DrawPageContent();
  }
  host.CopyPageText();
}

// _showMenu's showSubMenu step.
void ShowPage(HelpPagesHost& host, HelpPagesState& state) {
  std::int32_t page = state.scroll / kPageLines;
  const std::int32_t last_page = state.line_count / kPageLines;
  const std::uint32_t pressed = host.PressedButtons();
  if ((pressed & (kButtonCircle | kButtonCross)) != 0) {
    state.step = kHelpListTopics;
  }
  if ((pressed & kButtonTriangle) != 0) {
    state.step = kHelpLeaveToGame;
  }
  if ((pressed & kButtonStart) != 0) {
    page = 0;
    state.scroll = 0;
  } else if ((host.PreviousButtons() & kButtonSquare) != 0) {
    page = TurnWholePage(host, state, pressed, page, last_page);
  } else {
    ScrollLine(host, state, pressed);
  }
  DrawControls(host, page, last_page);
  const std::uint16_t manual = host.CurrentManual();
  if (manual < kTopicCount) {
    // The memory is a halfword; the scroll position keeps its low 16 bits.
    host.SetScrollMemory(manual, static_cast<std::uint16_t>(state.scroll));
  } else {
    // The original would write past the scroll memories.
    host.Refuse("Manual topic past the scroll memories");
  }
  if (state.step != kHelpShowPage) {
    host.PlaySfx(kMenuSfxGroup,
                 (pressed & kButtonCircle) != 0 ? kSfxSelect : kSfxLeave);
    host.HideTextBox(kPageTextBox);
    FreeHeldBuffer(host, HelpFile::kText);
    host.ForgetHelpBuffer(HelpFile::kText);
    return;
  }
  DrawPage(host, state);
}

// _showMenu's exitToMenu and exitToBattle steps. The original frees both
// buffers without clearing their pointers; InitHelpState clears them before
// the next use.
std::int32_t LeaveHelp(HelpPagesHost& host, std::int32_t result) {
  FreeHeldBuffer(host, HelpFile::kText);
  FreeHeldBuffer(host, HelpFile::kAssets);
  host.RetireTexturePages(kHelpTexturePage, kHelpTexturePageCount,
                          kHelpTexturePageOwner);
  return result;
}
}  // namespace

// decomp: MENUE.PRG func_80102E9C (src/MENU/MENUE.PRG/494.c:132), 0x80102E9C
void ClearMenuExceptRow8(ScreenHost& host) {
  host.ClearMenuExcept(kUnusedKeptRow);
}

// decomp: MENUE.PRG _menuReady (src/MENU/MENUE.PRG/494.c:134), 0x80102EBC
void PollMenuReady(ScreenHost& host) { static_cast<void>(host.MenuRowsIdle()); }

// decomp: MENUE.PRG vs_menuE_exec (src/MENU/MENUE.PRG/494.c:142), 0x80102EDC
std::int32_t UpdateScreen(ScreenHost& host, std::uint8_t& step,
                          ScreenState& state) {
  switch (step) {
    case kScreenOpen:
      host.SetBackgroundFade(kBackgroundFadeIn);
      state.open_delay = kScreenOpenDelay;
      state.initialized = 0;
      step = kScreenInitialize;
      break;
    case kScreenInitialize:
      if (state.initialized == 0) {
        // isInit is a char: it keeps the result's low byte.
        state.initialized = static_cast<std::uint8_t>(host.InitializeHelp());
      }
      if (state.open_delay != 0) {
        --state.open_delay;
      } else if (state.initialized != 0) {
        host.SetFieldRenderingEnabled(0);
        host.SetHudHidden(1);
        host.SetHudSlideOffset(kHudSlideHidden);
        step = kScreenWaitReady;
      }
      break;
    case kScreenWaitReady:
      // The step is a byte: the answer is added and wraps, as the original's.
      step = static_cast<std::uint8_t>(
          step + static_cast<std::uint32_t>(host.MenuRowsIdle()));
      break;
    case kScreenShow:
      state.help_result = host.UpdateHelpPages();
      if (state.help_result == 0) {
        break;
      }
      host.SetBackgroundPriority(kClosingBackgroundLayer,
                                 kBackgroundBrightness);
      host.SetHudHidden(0);
      host.SetHudSlideOffset(0);
      host.SetFieldRenderingEnabled(1);
      host.SetMenuCommand(Framework::MenuCommand::kNone);
      host.ClearMenuExcept(kNoMenuItem);
      step = kScreenShutDown;
      break;
    case kScreenShutDown:
      host.SetBackgroundFade(kBackgroundFadeOut);
      step = kScreenExit;
      break;
    case kScreenExit:
      if (host.BackgroundFadeStep() != 0) {
        break;
      }
      if (state.help_result == kHelpExitToGame) {
        host.SetCurrentMenu(kGameMenu);
      }
      host.RevealMenu();
      step = kScreenClosed;
      return 1;
    default:
      break;
  }
  return 0;
}

// decomp: MENUE.PRG _initMenuState (src/MENU/MENUE.PRG/494.c:219), 0x801030A4
std::int32_t InitHelpState(HelpPagesHost& host, HelpPagesState& state) {
  if (host.CursorMemorySetting() == 0) {
    host.ClearDisplayState();
  }
  host.ReserveTexturePages(kHelpTexturePage, kHelpTexturePageCount,
                           kHelpTexturePageOwner);
  state.step = kHelpStart;
  state.scroll = 0;
  state.line_count = 0;
  host.ForgetHelpBuffer(HelpFile::kText);
  host.ForgetHelpBuffer(HelpFile::kAssets);
  return 1;
}

// decomp: MENUE.PRG _showMenu (src/MENU/MENUE.PRG/494.c:245), 0x80103110
std::int32_t UpdateHelpPages(HelpPagesHost& host, HelpPagesState& state) {
  switch (state.step) {
    case kHelpStart:
      host.SetBackgroundPriority(kHelpBackgroundLayer, kBackgroundBrightness);
      ++state.step;
      break;
    case kHelpListTopics:
      host.InitializeInformationBox();
      static_cast<void>(host.UpdateTopicMenu(kTopicMenuInit));
      ++state.step;
      break;
    case kHelpSelectTopic:
      SelectTopic(host, state);
      break;
    case kHelpLoadAssets:
      LoadAssets(host, state);
      break;
    case kHelpLoadText:
      LoadText(host, state);
      break;
    case kHelpShowPage:
      ShowPage(host, state);
      break;
    case kHelpLeaveToMenu:
      return LeaveHelp(host, kHelpExitToMenu);
    case kHelpLeaveToGame:
      return LeaveHelp(host, kHelpExitToGame);
    default:
      break;
  }
  return 0;
}
}  // namespace VS::Menu::manual
