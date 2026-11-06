// Hand-maintained (tools/port/owned.csv).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Render/DisplayControl.hpp"
#include "Render/UiPackets.hpp"

// TITLE.PRG's title screen (22C.c:18-4741): the publisher and developer
// logos, the four-item menu over the intro movie with its vibration and sound
// submenus, the continue and clear-save file screens, and the new-game state
// it hands to the field. The menu items animate in State; everything else
// (VRAM, display, pad, sound, movie, heap, file screens and game state) is
// reached through the Host (TitleMenuLive.cpp binds it to the game).
namespace VS::Title {
inline constexpr std::size_t kMenuItemCount = 10;
inline constexpr std::size_t kMenuSpriteWords = 6;
inline constexpr std::size_t kTransitionTileWords = 5;

// _menuItemState.motion (22C.c:3989).
enum class MenuItemMotion : std::uint8_t {
  kStationary = 0,
  kMovingUp = 1,
  kMovingDown = 2,
  kSubmenu = 3
};

// The menu items and vs_title_exec's result (22C.c:4491): a timeout plays
// an opening cutscene; New Game and Continue leave the title.
enum class MenuItemId : std::int8_t {
  kTimeout = -1,
  kNewGame = 0,
  kContinue = 1,
  kVibration = 2,
  kSound = 3,
  kMono = 4,
  kVibrationOff = 5,
  kVibrationOn = 6,
  kStereo = 7
};

// decomp: TITLE.PRG _menuItemState (src/TITLE/TITLE.PRG/22C.c:3989). The
// positions are scanlines of the menu strip; they wrap as bytes.
struct MenuItem {
  std::uint8_t is_enabled = 0;
  MenuItemMotion motion = MenuItemMotion::kStationary;
  std::uint8_t text_saturation = 0;
  std::uint8_t outline_saturation = 0;
  std::uint8_t text_position = 0;
  std::uint8_t target_position = 0;
  std::uint8_t outline_position = 0;
  std::uint8_t text_blend_factor = 0;

  friend bool operator==(const MenuItem&, const MenuItem&) = default;
};

struct State {
  std::array<MenuItem, kMenuItemCount> items{};
};

// A packed GPU sprite: tag, draw mode, colour, position, texture and size.
using MenuSpritePacket = std::array<std::uint32_t, kMenuSpriteWords>;
// A packed GPU tile: tag, draw mode, colour, position and size.
using TransitionTilePacket = std::array<std::uint32_t, kTransitionTileWords>;

// The images the title uploads from its module data.
enum class Image : std::uint8_t {
  kSaveUiPageZero,
  kSaveUiPageOne,
  kSaveFontPageZero,
  kSaveFontPageOne,
  kDebugFont,
  kSaveBackgroundPalette,
  kSaveBackgroundPixels,
  kPublisherPalette,
  kPublisherPixels,
  kDeveloperLogo
};

// A display area with its screen window (SetDefDispEnv) and a draw area
// (SetDefDrawEnv), put together.
struct DisplaySetup {
  Render::ScreenRect display{};
  Render::ScreenRect screen{};
  bool interlaced = false;
  Render::ScreenRect draw{};

  friend bool operator==(const DisplaySetup&, const DisplaySetup&) = default;
};

// _initScreen's request (22C.c:4658): the gameplay resolution, projection
// distance and clear colour.
struct GameDisplayRequest {
  int width = 0;
  int height = 0;
  int projection_distance = 0;
  Render::ClearColor background{};
};

// The two gameplay buffers _initScreen sets up: buffer b draws at
// draw_areas[b] and shows display_areas[b], clearing to `background` without
// dithering.
struct GameDisplayLayout {
  std::array<Render::ScreenRect, Render::kFrameBufferCount> draw_areas{};
  std::array<Render::ScreenRect, Render::kFrameBufferCount> display_areas{};
  Render::ScreenRect screen{};
  Render::ClearColor background{};

  friend bool operator==(const GameDisplayLayout&,
                         const GameDisplayLayout&) = default;
};

struct Buttons {
  std::uint32_t held = 0;
  std::uint32_t pressed = 0;
};

struct PlayerSettings {
  std::uint8_t mono_sound = 0;
  std::uint8_t vibration_on = 0;

  friend bool operator==(const PlayerSettings&,
                         const PlayerSettings&) = default;
};

// One boss time-trial record (State_TrialScore).
struct TimeTrialScore {
  std::int32_t time = 0;
  std::uint8_t round = 0;
  std::uint8_t difficulty = 0;

  friend bool operator==(const TimeTrialScore&,
                         const TimeTrialScore&) = default;
};

// The saved entry door's tile, facing and elevation a new game starts from.
struct EntryDoor {
  std::uint8_t tile_x = 0;
  std::uint8_t tile_z = 0;
  std::uint8_t direction = 0;
  std::uint8_t elevation = 0;

  friend bool operator==(const EntryDoor&, const EntryDoor&) = default;
};

struct Destination {
  std::uint8_t zone = 0;
  std::uint8_t room = 0;

  friend bool operator==(const Destination&, const Destination&) = default;
};

// _setTitleExitFlags (22C.c:4741): the unused state flags at 0x0C-0x0E and
// the entry door's destination; unset fields are left alone.
struct ExitPlan {
  std::uint8_t state_flag_0c = 0;
  std::uint8_t state_flag_0d = 0;
  std::optional<std::uint8_t> state_flag_0e;
  std::optional<Destination> destination;

  friend bool operator==(const ExitPlan&, const ExitPlan&) = default;
};

// The difficulty and puzzle mode _initEnvironment sets in the unused state
// flags (22C.c:4463).
struct GameModes {
  std::uint8_t difficulty = 0;
  std::uint8_t puzzle_mode = 0;

  friend bool operator==(const GameModes&, const GameModes&) = default;
};

class Host : public Render::ImmediatePacketTarget {
 public:
  // Reports a request the title refused (an exhausted heap, a capture too
  // small for the menu or a setting outside its items).
  virtual void Refuse(std::string_view what) = 0;
  // Sound effect `effect` of the menu group, centred.
  virtual void PlaySoundEffect(int effect) = 0;
  [[nodiscard]] virtual PlayerSettings Settings() = 0;
  // Writes only the mono and vibration settings.
  virtual void SetSettings(PlayerSettings settings) = 0;
  // Boot_InitializePlayerSettings.
  virtual void ResetPlayerSettings() = 0;
  virtual void SetMonoOutput(int mono) = 0;
  virtual void SetDiscMixVolume(std::uint32_t volume) = 0;

  virtual void UpdateControllers() = 0;
  [[nodiscard]] virtual Buttons Input() = 0;
  virtual void InvalidateControllerConnections() = 0;
  virtual void UpdateVibrationPatterns() = 0;
  virtual void PlayVibrationPreset(int preset) = 0;

  // VSync(0): the next vertical blank.
  virtual void WaitForVerticalBlank() = 0;
  // VSync(count): `count` vertical blanks.
  virtual void WaitForVerticalBlanks(int count) = 0;
  // VSync(1): lines drawn since the last vertical blank.
  [[nodiscard]] virtual int HorizontalBlankCount() = 0;
  // VSync(-1): vertical blanks since start-up.
  [[nodiscard]] virtual int VerticalBlankCount() = 0;
  // Advances play time and pending loads; returns the vsync result.
  virtual int AdvanceFrameClock(int vsync_mode) = 0;

  // Uploads the module image into the VRAM area.
  virtual void UploadImage(Image image, const Render::ScreenRect& area) = 0;
  virtual void LoadImage(const Render::ScreenRect& area,
                         std::span<const std::uint16_t> pixels) = 0;
  virtual void StoreImage(const Render::ScreenRect& area,
                          std::span<std::uint16_t> pixels) = 0;
  // Fills the VRAM area with black.
  virtual void ClearImage(const Render::ScreenRect& area) = 0;
  virtual void SetDisplayEnabled(bool enabled) = 0;
  virtual void PutDisplay(const DisplaySetup& setup) = 0;
  virtual void SetGeometryOffset(int x, int y) = 0;
  virtual void SetProjectionDistance(int distance) = 0;
  virtual void ApplyGameDisplay(const GameDisplayLayout& layout) = 0;
  [[nodiscard]] virtual int FrameBuffer() = 0;
  virtual void SetFrameBuffer(int buffer) = 0;
  // Puts the gameplay buffer's display and draw environments.
  virtual void ShowFrameBuffer(int buffer) = 0;
  // Draws the caller's ordering table.
  virtual void DrawOrderingTable() = 0;

  // A low-end heap block, or an empty span when the heap is exhausted.
  [[nodiscard]] virtual std::span<std::uint16_t> AllocateLow(
      std::size_t halfwords) = 0;
  virtual void FreeLow(std::span<std::uint16_t> block) = 0;
  // _titleScreenBg: run-length encoded words.
  [[nodiscard]] virtual std::span<const std::uint32_t> BackgroundWords() = 0;
  // _menuCopyright: 180x24 pixels.
  [[nodiscard]] virtual std::span<const std::uint16_t> CopyrightPixels() = 0;

  virtual void StartIntroMovie() = 0;
  virtual void PlayIntroMovie() = 0;
  virtual void StopIntroMovie() = 0;
  virtual void StopIntroMovieAfterAudioTail() = 0;
  [[nodiscard]] virtual bool IntroMoviePlaying() = 0;
  virtual void ClearIntroMoviePlaying() = 0;

  [[nodiscard]] virtual int VisitCount() = 0;
  virtual void SetVisitCount(int count) = 0;
  [[nodiscard]] virtual bool ClearSavePending() = 0;
  virtual void ClearPendingClearSave() = 0;
  // Stages the cleared game's save before the clear-save screen.
  virtual void StageClearGame() = 0;
  [[nodiscard]] virtual int InitialTitleSelection() = 0;

  // The file screens (Menu/FilesLive.hpp, Save/FileMenuLive.hpp).
  virtual void InitializeFileDisplay() = 0;
  virtual void SwapFileBuffers() = 0;
  virtual int StepCardResources(int init) = 0;
  virtual void ReleaseCardResources() = 0;
  virtual void AdvanceCardCipher() = 0;
  virtual void ResetFileElements() = 0;
  virtual int UpdateLoadMenu(int init) = 0;
  virtual int UpdateSaveMenu(int init) = 0;
  // Shows MCMAN.BIN's save prompt as the card message.
  virtual void ShowSavePrompt() = 0;
  virtual int UpdateClearSaveReminder(int init) = 0;
  virtual void DrawFileBackground() = 0;
  virtual void DrawFileFrame() = 0;
  virtual void DrawExitFade() = 0;

  // The game state a new game starts from.
  virtual void ResetSession() = 0;
  virtual void ClearStateFlags() = 0;
  virtual void RestoreInitialLearnedActions() = 0;
  // Room visits, play time, scores, the room slot, the recent room enemies
  // and the arts status.
  virtual void ClearProgressRecords() = 0;
  virtual void SetBossTimeTrialScores(const TimeTrialScore& score) = 0;
  virtual void ClearAshleySnapshot(std::size_t bytes) = 0;
  virtual void ClearSavedEntryDoor() = 0;
  virtual void RestoreNewGameInventory() = 0;
  virtual void SetTickSpeed(int speed) = 0;
  virtual void SetEntryDoor(const EntryDoor& door) = 0;
  virtual void ClearInventoryIndices() = 0;
  virtual void SetGameModes(const GameModes& modes) = 0;
  virtual void ApplyExit(const ExitPlan& plan) = 0;
};

struct Context {
  State& state;
  Host& host;
};

// decomp: TITLE.PRG _playNewGameSfx (src/TITLE/TITLE.PRG/22C.c:18)
void PlayNewGameSounds(Host& host);
// decomp: TITLE.PRG _playMenuChangeSfx (src/TITLE/TITLE.PRG/22C.c:25)
void PlayMenuChangeSound(Host& host);
// decomp: TITLE.PRG _playMenuSelectSfx (src/TITLE/TITLE.PRG/22C.c:27)
void PlayMenuSelectSound(Host& host);
// decomp: TITLE.PRG _playMenuLeaveSfx (src/TITLE/TITLE.PRG/22C.c:29)
void PlayMenuLeaveSound(Host& host);
// decomp: TITLE.PRG _gameLoadScreen (src/TITLE/TITLE.PRG/22C.c:3129)
int ShowContinueScreen(Host& host);
// decomp: TITLE.PRG _gameSaveScreen (src/TITLE/TITLE.PRG/22C.c:3313)
void ShowClearSaveScreen(Host& host);
// decomp: TITLE.PRG _displayPublisherAndDeveloper
// (src/TITLE/TITLE.PRG/22C.c:3579)
void ShowPublisherAndDeveloper(Host& host);
// decomp: TITLE.PRG _setMenuItemFadeIn (src/TITLE/TITLE.PRG/22C.c:3761)
void InitializeMenuItem(State& state, int item, std::uint8_t position);
// decomp: TITLE.PRG _copyTitleBgData (src/TITLE/TITLE.PRG/22C.c:3773)
void LoadBackgroundTextures(Host& host);
// decomp: TITLE.PRG _initTitleScreen (src/TITLE/TITLE.PRG/22C.c:3821)
void InitializeTitleDisplay(Host& host);
// decomp: TITLE.PRG _initMenu (src/TITLE/TITLE.PRG/22C.c:3838): the capture
// block, empty when the heap had no room.
std::span<std::uint16_t> InitializeMenu(Context& context, int item);
// decomp: TITLE.PRG _freeHeap (src/TITLE/TITLE.PRG/22C.c:3867)
void ReleaseMenuCapture(Host& host, std::span<std::uint16_t> capture);
// decomp: TITLE.PRG _fadeInMenu (src/TITLE/TITLE.PRG/22C.c:3869)
void BlendMenuBackground(Host& host, std::span<std::uint16_t> capture,
                         int factor);
// decomp: TITLE.PRG _fadeInMenuCopyright (src/TITLE/TITLE.PRG/22C.c:3914)
void BlendCopyright(Host& host, std::span<std::uint16_t> capture, int factor);
// decomp: TITLE.PRG _setMenuItemClut (src/TITLE/TITLE.PRG/22C.c:3957):
// the item's palette `factor` sixteenths of the way from palette row `from`
// to row `to`.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void BlendMenuItemClut(Host& host, int item, int factor, int from, int to);
// decomp: TITLE.PRG _setTitleMenuState (src/TITLE/TITLE.PRG/22C.c:3997)
void UpdateMenuAnimation(Context& context);
// decomp: TITLE.PRG _drawTitleMenuItems (src/TITLE/TITLE.PRG/22C.c:4073)
void DrawMenuItems(Context& context);
// decomp: TITLE.PRG _drawTitleMenu (src/TITLE/TITLE.PRG/22C.c:4183)
void DrawMenu(Context& context);
// decomp: TITLE.PRG _drawAndSyncTitleMenu (src/TITLE/TITLE.PRG/22C.c:4227)
void DrawMenuAndPollInput(Context& context);
// decomp: TITLE.PRG _menuVibrationSettings (src/TITLE/TITLE.PRG/22C.c:4246)
void ShowVibrationSettings(Context& context);
// decomp: TITLE.PRG _menuSoundSettings (src/TITLE/TITLE.PRG/22C.c:4353)
void ShowSoundSettings(Context& context);
// decomp: TITLE.PRG _nop (src/TITLE/TITLE.PRG/22C.c:4461): New Game.
[[nodiscard]] int NewGameChoice() noexcept;
// decomp: TITLE.PRG _initEnvironment (src/TITLE/TITLE.PRG/22C.c:4463)
void InitializeSettings(Context& context);
// decomp: TITLE.PRG vs_title_exec (src/TITLE/TITLE.PRG/22C.c:4491): the
// MenuItemId the title left with, never kTimeout.
int Run(Context& context);
// decomp: TITLE.PRG _initScreen (src/TITLE/TITLE.PRG/22C.c:4658)
void InitializeGameDisplay(Host& host, const GameDisplayRequest& request);
// decomp: TITLE.PRG _renderScreen (src/TITLE/TITLE.PRG/22C.c:4684)
int PresentGameFrame(Host& host);
// decomp: TITLE.PRG _initGameData (src/TITLE/TITLE.PRG/22C.c:4698)
void InitializeNewGameData(Host& host);
// decomp: TITLE.PRG _setTitleExitFlags (src/TITLE/TITLE.PRG/22C.c:4741)
void PrepareExit(Host& host, int cause);

// The policies the functions above apply, for the tests.
[[nodiscard]] ExitPlan PlanExit(int cause) noexcept;
[[nodiscard]] GameDisplayLayout PlanGameDisplay(
    const GameDisplayRequest& request) noexcept;
// _copyTitleBgData's run-length decoding: each control word holds a count of
// zero words (low half) and of literal words (high half), each word two
// halfwords. Returns the halfwords written, or nothing when the encoding
// overruns `out` or ends inside a literal run.
[[nodiscard]] std::optional<std::size_t> DecodeBackground(
    std::span<const std::uint32_t> encoded, std::span<std::uint16_t> out);
// _fadeInMenu's pixel: `factor` 32nds of the way from `source` to `target`.
[[nodiscard]] std::uint16_t BlendMenuBackgroundPixel(std::uint16_t target,
                                                     std::uint16_t source,
                                                     int factor) noexcept;
// _fadeInMenuCopyright's pixel: `factor` 32nds of the way from the copyright
// to the captured background.
[[nodiscard]] std::uint16_t BlendCopyrightPixel(std::uint16_t capture,
                                                std::uint16_t copyright,
                                                int factor) noexcept;
// _setMenuItemClut's entry: `factor` sixteenths of the way from `from` to
// `to`.
[[nodiscard]] std::uint16_t BlendPaletteEntry(std::uint16_t from,
                                              std::uint16_t to,
                                              int factor) noexcept;
// The outline sprite of item `index`.
[[nodiscard]] MenuSpritePacket OutlinePacket(const MenuItem& item,
                                             std::size_t index) noexcept;
// Scanline `line` (0-31) of item `index`'s text, or nothing for a line in
// the strip's hidden band.
[[nodiscard]] std::optional<MenuSpritePacket> TextLinePacket(
    const MenuItem& item, std::size_t index, int line) noexcept;
[[nodiscard]] TransitionTilePacket TransitionTile() noexcept;
}  // namespace VS::Title
