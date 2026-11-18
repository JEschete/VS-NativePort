#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "Menu/ManualTopics.hpp"

// The manual screen's lifecycle and its help pages (MENUE.PRG
// 494.c:132-448): the screen's open, wait, show, fade-out and close steps
// around the main menu; the help state's reset; and the help page loop, which
// lists the topics, loads the chosen topic's illustration assets and text
// from disc, pages through the text and releases both on the way out. The
// cores reach the menu framework, the help buffers, disc requests, display
// state, buttons and drawing through hosts; the live binding maps them onto
// the module statics (ManualScreenLive.cpp).
namespace VS::Menu::manual {
// --- Screen (vs_menuE_exec) -------------------------------------------------

// The screen's step byte. Steps outside 3-8 are idle.
inline constexpr std::uint8_t kScreenClosed = 0;
inline constexpr std::uint8_t kScreenOpen = 3;
inline constexpr std::uint8_t kScreenInitialize = 4;
inline constexpr std::uint8_t kScreenWaitReady = 5;
inline constexpr std::uint8_t kScreenShow = 6;
inline constexpr std::uint8_t kScreenShutDown = 7;
inline constexpr std::uint8_t kScreenExit = 8;
// Frames the screen waits after starting the background fade.
inline constexpr std::uint8_t kScreenOpenDelay = 8;
// D_800EB9B0 while the manual is up: the HUD slid off screen.
inline constexpr std::int32_t kHudSlideHidden = 0x200000;
// vs_mainMenu_setBackgroundRenderPriority on the way out.
inline constexpr std::int32_t kClosingBackgroundLayer = -4;
inline constexpr std::int32_t kBackgroundBrightness = 128;
// vs_mainMenu_menuItemIds_none: clears every menu row.
inline constexpr std::int32_t kNoMenuItem = 40;
// menuActionNone.
inline constexpr std::int32_t kNoMenuCommand = 0;
// vs_battle_menuState.currentMenu after leaving for the game (the help pages'
// kHelpExitToGame).
inline constexpr std::int32_t kGameMenu = 10;
// The row func_80102E9C keeps.
inline constexpr std::int32_t kUnusedKeptRow = 8;

// The help page results UpdateHelpPages returns when it is done.
inline constexpr std::int32_t kHelpExitToMenu = 1;
inline constexpr std::int32_t kHelpExitToGame = 2;

class ScreenHost {
 public:
  virtual ~ScreenHost() = default;
  // vs_mainMenu_clearMenuExcept.
  virtual void ClearMenuExcept(std::int32_t row) = 0;
  // vs_mainmenu_ready.
  [[nodiscard]] virtual std::int32_t MenuRowsIdle() = 0;
  // vs_mainMenu_toggleBackgroundFade.
  virtual void SetBackgroundFade(std::int32_t fade) = 0;
  // _initMenuState (InitHelpState).
  [[nodiscard]] virtual std::int32_t InitializeHelp() = 0;
  // _showMenu (UpdateHelpPages).
  [[nodiscard]] virtual std::int32_t UpdateHelpPages() = 0;
  // func_8008A4DC.
  virtual void SetFieldRenderingEnabled(std::int32_t enabled) = 0;
  // func_800CB654.
  virtual void SetHudHidden(std::int32_t hidden) = 0;
  // D_800EB9B0.
  virtual void SetHudSlideOffset(std::int32_t offset) = 0;
  // vs_mainMenu_setBackgroundRenderPriority.
  virtual void SetBackgroundPriority(std::int32_t layer,
                                     std::int32_t brightness) = 0;
  // vs_mainMenu_setMenuCommand.
  virtual void SetMenuCommand(std::int32_t command) = 0;
  // vs_mainMenu_backgroundFadeStep.
  [[nodiscard]] virtual std::int32_t BackgroundFadeStep() const = 0;
  // vs_battle_menuState.currentMenu.
  virtual void SetCurrentMenu(std::int32_t menu) = 0;
  // vs_mainMenu_hideMenu = 0.
  virtual void RevealMenu() = 0;
};

// vs_menuE_exec's statics.
struct ScreenState {
  // D_8010522D: frames left before the screen may open.
  std::uint8_t open_delay = 0;
  // isInit: InitHelpState's result, kept as a byte.
  std::uint8_t initialized = 0;
  // menuResult: UpdateHelpPages' last result.
  std::int32_t help_result = 0;

  friend bool operator==(const ScreenState&, const ScreenState&) = default;
};

// func_80102E9C (unused): clears the menu but row kUnusedKeptRow.
void ClearMenuExceptRow8(ScreenHost& host);
// _menuReady (unused): polls whether the menu rows are idle and drops the
// answer.
void PollMenuReady(ScreenHost& host);
// vs_menuE_exec: advances the screen's `step` byte by one frame. Returns 1
// on the frame the screen closes (the step back at kScreenClosed), else 0.
[[nodiscard]] std::int32_t UpdateScreen(ScreenHost& host, std::uint8_t& step,
                                        ScreenState& state);

// --- Help pages (_initMenuState, _showMenu) ---------------------------------

// The help pages' steps (_showMenuState). Steps outside 0-7 are idle.
inline constexpr std::int32_t kHelpStart = 0;
inline constexpr std::int32_t kHelpListTopics = 1;
inline constexpr std::int32_t kHelpSelectTopic = 2;
inline constexpr std::int32_t kHelpLoadAssets = 3;
inline constexpr std::int32_t kHelpLoadText = 4;
inline constexpr std::int32_t kHelpShowPage = 5;
inline constexpr std::int32_t kHelpLeaveToMenu = 6;
inline constexpr std::int32_t kHelpLeaveToGame = 7;
inline constexpr std::int32_t kHelpStepCount = 8;

// _topMenu's results that leave the list: -3 leaves for the game, the other
// negative results except kNoTopicSelected return to the menu.
inline constexpr std::int32_t kTopicMenuExitToGame = -3;

// The help texture pages func_8007DFF0 lends and func_8007E0A8 retires.
inline constexpr std::uint32_t kHelpTexturePage = 0x1D;
inline constexpr std::uint32_t kHelpTexturePageCount = 3;
inline constexpr std::int32_t kHelpTexturePageOwner = 5;
// vs_mainMenu_setBackgroundRenderPriority while the help pages are up.
inline constexpr std::int32_t kHelpBackgroundLayer = 2046;
// vs_main_CdQueueStateLoaded.
inline constexpr std::int16_t kHelpFileLoaded = 4;
// The disc queue's error state: the read failed or was cancelled, and the
// request never reaches kHelpFileLoaded.
inline constexpr std::int16_t kHelpFileFailed = 6;
// The asset file: a sprite tile count and a CLUT count, then the tiles (128
// bytes each) and the CLUTs.
inline constexpr std::uint32_t kHelpAssetHeaderBytes = 8;
inline constexpr std::uint32_t kHelpSpriteTileShift = 7;
// The pages hold ten lines; their numbers are drawn from 1.
inline constexpr std::int32_t kPageLines = kLinesPerPage;
// Frames a freshly shown page waits before _drawContent draws it.
inline constexpr std::int32_t kPageContentDelay = 8;
// _pageArrowAnimState counts frames modulo 16.
inline constexpr std::int32_t kArrowFrameMask = 0xF;

// The pad bits _showMenu reads.
inline constexpr std::uint32_t kButtonTriangle = 0x10;
inline constexpr std::uint32_t kButtonCircle = 0x20;
inline constexpr std::uint32_t kButtonCross = 0x40;
inline constexpr std::uint32_t kButtonSquare = 0x80;
inline constexpr std::uint32_t kButtonStart = 0x800;
inline constexpr std::uint32_t kButtonUp = 0x1000;
inline constexpr std::uint32_t kButtonDown = 0x4000;

// vs_main_playSfxDefault(0x7E, ...) with VS_SFX_MENUCHANGE, VS_SFX_MENUSELECT
// and VS_SFX_MENULEAVE (src/SLUS_010.40/sfx.h:3-5).
inline constexpr std::int32_t kMenuSfxGroup = 0x7E;
inline constexpr std::int32_t kSfxPageTurn = 4;
inline constexpr std::int32_t kSfxSelect = 5;
inline constexpr std::int32_t kSfxLeave = 6;

// vs_mainmenu_renderButton's button ids.
inline constexpr std::int32_t kSquareButtonIcon = 1;
inline constexpr std::int32_t kCircleButtonIcon = 2;
// The ordering table entries the controls and page number are drawn before.
inline constexpr std::int32_t kControlsLayer = 6;
inline constexpr std::int32_t kPageNumberLayer = 7;
inline constexpr std::int32_t kTextColor = 0x808080;
// The text box that shows the page.
inline constexpr std::int32_t kPageTextBox = 0;

// vs_battle_initTextBox's arguments for the page text box.
struct TextBoxPlacement {
  std::int32_t id = 0;
  std::int32_t flags = 0;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t chars_per_line = 0;
  std::int32_t line_count = 0;
  std::int32_t center_x = 0;
  std::int32_t center_y = 0;

  friend bool operator==(const TextBoxPlacement&,
                         const TextBoxPlacement&) = default;
};
inline constexpr TextBoxPlacement kPageTextPlacement{
    .id = kPageTextBox,
    .flags = 0,
    .x = 8,
    .y = 0x34,
    .chars_per_line = 0x19,
    .line_count = 0xA,
    .center_x = 8,
    .center_y = 0x34,
};

// A rectangle in screen pixels.
struct ScreenRect {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t w = 0;
  std::int32_t h = 0;

  friend bool operator==(const ScreenRect&, const ScreenRect&) = default;
};
// _drawControlsBg behind the two control hints and _setPageBg behind the
// page.
inline constexpr ScreenRect kTurnPageControls{
    .x = 0x10,
    .y = 0x12,
    .w = 0x60,
    .h = 0xC,
};
inline constexpr ScreenRect kMenuControls{
    .x = 0x10,
    .y = 0x24,
    .w = 0x40,
    .h = 0xC,
};
inline constexpr ScreenRect kPageBackground{
    .x = 8,
    .y = 52,
    .w = 310,
    .h = 136,
};
inline constexpr std::int32_t kPageBackgroundColor = 0;

// The help files, as the halves of _helpFileCdFiles' pairs.
enum class HelpFile : std::uint8_t { kAssets = 0, kText = 1 };
inline constexpr std::size_t kHelpFilesPerTopic = 2;

// The asset file's header words.
struct HelpAssetHeader {
  std::uint32_t sprite_tiles = 0;
  std::uint32_t cluts = 0;

  friend bool operator==(const HelpAssetHeader&,
                         const HelpAssetHeader&) = default;
};

// _showMenu's statics besides the help buffers and disc requests, which the
// host keeps.
struct HelpPagesState {
  // _showMenuState.
  std::int32_t step = kHelpStart;
  // _scrollPosition: the first shown line.
  std::int32_t scroll = 0;
  // _lineCount: the first line of the last page.
  std::int32_t line_count = 0;
  // _pageArrowAnimState.
  std::int32_t arrow_frame = 0;
  // _helpPageLoadCounter.
  std::int32_t load_counter = 0;

  friend bool operator==(const HelpPagesState&,
                         const HelpPagesState&) = default;
};

// The calls the help pages make. The page drawing and text calls read the
// help state the core is running; a live host publishes it to the module
// statics before each one, as the original's stores precede its calls.
class HelpPagesHost {
 public:
  virtual ~HelpPagesHost() = default;
  // vs_main_settings.cursorMemory.
  [[nodiscard]] virtual std::uint8_t CursorMemorySetting() const = 0;
  // vs_main_bzero(&vs_battle_manualDisplayState, ...).
  virtual void ClearDisplayState() = 0;
  // func_8007DFF0 and func_8007E0A8.
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
  virtual void ReserveTexturePages(std::uint32_t first, std::uint32_t count,
                                   std::int32_t owner) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
  virtual void RetireTexturePages(std::uint32_t first, std::uint32_t count,
                                  std::int32_t owner) = 0;
  // vs_mainMenu_setBackgroundRenderPriority.
  virtual void SetBackgroundPriority(std::int32_t layer,
                                     std::int32_t brightness) = 0;
  // vs_mainMenu_initInformationBox and vs_mainMenu_dismissInformationBox.
  virtual void InitializeInformationBox() = 0;
  virtual void DismissInformationBox() = 0;
  // _topMenu.
  [[nodiscard]] virtual std::int32_t UpdateTopicMenu(std::int32_t init) = 0;
  // D_800F4EE8.cursorMemories[4] and [5].
  [[nodiscard]] virtual TopicCursor TopicMenuCursor() const = 0;
  // vs_battle_manualDisplayState: the shown topic and each topic's
  // remembered scroll position. The core checks topics against
  // kTopicCount.
  [[nodiscard]] virtual std::uint16_t CurrentManual() const = 0;
  virtual void SetCurrentManual(std::uint16_t manual) = 0;
  [[nodiscard]] virtual std::uint16_t ScrollMemory(std::size_t topic) const = 0;
  virtual void SetScrollMemory(std::size_t topic, std::uint16_t scroll) = 0;
  // _helpText and _helpAssets: whether the pointer is set, vs_main_freeHeapR
  // (which keeps the pointer), clearing the pointer, and vs_main_allocHeapR
  // of the topic's file size. The core checks topics against kTopicCount.
  [[nodiscard]] virtual bool HelpBufferHeld(HelpFile file) const = 0;
  virtual void FreeHelpBuffer(HelpFile file) = 0;
  virtual void ForgetHelpBuffer(HelpFile file) = 0;
  virtual void AllocateHelpBuffer(HelpFile file, std::int32_t topic) = 0;
  // vs_main_allocateCdQueueSlot and vs_main_cdEnqueue of the topic's file
  // into its buffer; the request's state; vs_main_freeCdQueueSlot.
  virtual void RequestHelpFile(HelpFile file, std::int32_t topic) = 0;
  [[nodiscard]] virtual std::int16_t HelpFileState(HelpFile file) const = 0;
  virtual void ReleaseHelpFile(HelpFile file) = 0;
  // Native: stops a pending read into the file's buffer so the buffer can be
  // freed. The original has no such step; it never abandons a help read.
  virtual void CancelHelpFile(HelpFile file) = 0;
  // The loaded asset file's header, or nothing when there is no asset
  // buffer to read it from.
  [[nodiscard]] virtual std::optional<HelpAssetHeader> HelpAssets() const = 0;
  // _copySprites and _copyCluts from `byte_offset` into the asset buffer.
  virtual void UploadSpriteTiles(std::uint32_t byte_offset,
                                 std::uint32_t count) = 0;
  virtual void UploadCluts(std::uint32_t byte_offset, std::uint32_t count) = 0;
  // vs_battle_initTextBox; vs_battle_getTextBox(id)->state = 0.
  virtual void InitTextBox(const TextBoxPlacement& placement) = 0;
  virtual void HideTextBox(std::int32_t id) = 0;
  // _getRoundedLineCount.
  [[nodiscard]] virtual std::int32_t LastPageLineOffset() = 0;
  // vs_main_buttonsPressed.all, vs_main_buttonsPreviousState and
  // vs_main_buttonRepeat.
  [[nodiscard]] virtual std::uint32_t PressedButtons() const = 0;
  [[nodiscard]] virtual std::uint32_t PreviousButtons() const = 0;
  [[nodiscard]] virtual std::uint32_t RepeatedButtons() const = 0;
  // vs_main_playSfxDefault.
  virtual void PlaySfx(std::int32_t group, std::int32_t effect) = 0;
  // vs_mainmenu_renderButton and vs_battle_renderTextRawColor before
  // ordering table entry `layer`.
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
  virtual void DrawButtonIcon(std::int32_t button, std::int32_t x,
                              std::int32_t y, std::int32_t layer) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
  virtual void DrawText(std::string_view text, std::int32_t xy,
                        std::int32_t color, std::int32_t layer) = 0;
  // _drawControlsBg and _setPageBg.
  virtual void DrawControlsBackground(const ScreenRect& rect) = 0;
  virtual void DrawPageBackground(const ScreenRect& rect,
                                  std::int32_t color) = 0;
  // _fadeMenuUpper, _fadeMenuLower, _drawContent and _copyNextPageText.
  virtual void DrawUpperScrollFade() = 0;
  virtual void DrawLowerScrollFade() = 0;
  virtual void DrawPageContent() = 0;
  virtual void CopyPageText() = 0;
  virtual void Refuse(std::string_view reason) = 0;
};

// _initMenuState: forgets the remembered topic and scroll positions unless
// cursor memory is on, lends the help texture pages and resets the help
// state. Returns 1. Leaves the arrow frame, load counter and disc requests
// as they were.
[[nodiscard]] std::int32_t InitHelpState(HelpPagesHost& host,
                                         HelpPagesState& state);

// _showMenu: advances the help pages by one frame. Returns kHelpExitToMenu or
// kHelpExitToGame on the frame they are left (after releasing the help
// buffers and texture pages), else 0. A topic past the help files is
// refused and leaves for the menu; a shown topic past the scroll memories
// is refused and pages from the top without remembering its position; a
// loaded asset file without a buffer is refused and uploads nothing. A help
// read that fails is refused: the topic's outstanding reads are cancelled,
// their buffers freed and the topic list shown again (the original waits for
// it forever, #408).
[[nodiscard]] std::int32_t UpdateHelpPages(HelpPagesHost& host,
                                           HelpPagesState& state);
}  // namespace VS::Menu::manual
