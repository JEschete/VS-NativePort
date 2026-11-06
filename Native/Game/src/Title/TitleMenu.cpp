// Hand-maintained (tools/port/owned.csv).
#include "Title/TitleMenu.hpp"  // NOLINT(build/include): typed API

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Ending/DisplayHost.hpp"
#include "Menu/Sounds.hpp"
#include "Render/DisplayControl.hpp"
#include "Render/UiPackets.hpp"

namespace VS::Title {
namespace {
using Ending::BlendMode;
using Ending::PaletteBits;
using Ending::TextureDepth;
using Ending::TexturePageBits;
using Render::ScreenRect;

constexpr int kHalfwordShift = 16;
constexpr std::uint32_t kHalfwordMask = 0xFFFF;
constexpr int kByteShift = 8;
constexpr std::size_t kPaletteColors = 16;

// vs_getXY and vs_getWH: the low coordinate in the low half.
[[nodiscard]] constexpr std::int32_t Pack(int low, int high) noexcept {
  return static_cast<std::int32_t>(
      (static_cast<std::uint32_t>(high) << kHalfwordShift) |
      (static_cast<std::uint32_t>(low) & kHalfwordMask));
}

// vs_getUV0Clut.
[[nodiscard]] constexpr std::int32_t UvClut(int u, int v, int clut_x,
                                            int clut_y) noexcept {
  return static_cast<std::int32_t>(
      static_cast<std::uint32_t>(u) |
      (static_cast<std::uint32_t>(v) << kByteShift) |
      (static_cast<std::uint32_t>(PaletteBits(clut_x, clut_y))
       << kHalfwordShift));
}

// vs_getTpage: a draw-mode command without dithering.
constexpr std::uint32_t kDrawModeCommand = 0xE1000000;
[[nodiscard]] constexpr std::uint32_t DrawMode(TextureDepth depth,
                                               BlendMode blend, int x,
                                               int y) noexcept {
  return kDrawModeCommand | TexturePageBits(depth, blend, x, y);
}

[[nodiscard]] constexpr std::uint32_t Word(std::int32_t value) noexcept {
  return static_cast<std::uint32_t>(value);
}

[[nodiscard]] constexpr std::uint8_t Byte(int value) noexcept {
  return static_cast<std::uint8_t>(value);
}

// vs_getTag of a sprite (five words) and a tile (four) ending the chain.
constexpr std::uint32_t kSpriteTag = 0x05FFFFFF;
constexpr std::uint32_t kTileTag = 0x04FFFFFF;
// The sprite commands: semi-transparent with a grey level, and opaque at the
// neutral 0x80.
constexpr std::uint32_t kSemiTransparentSprite = 0x66000000;
constexpr std::uint32_t kNeutralSprite = 0x64808080;
constexpr std::uint32_t kGreyLevel = 0x010101;

// _menuItemTextClut and the _menuItemOutlineClut that follows it in the
// module (TITLE.PRG data, 22C.c:3763): _setMenuItemClut indexes past the
// text palettes into the outline palettes as rows 2 and 3.
// decomp: TITLE.PRG _menuItemTextClut (TITLE/TITLE.PRG data)
// decomp: TITLE.PRG _menuItemOutlineClut (TITLE/TITLE.PRG data)
[[nodiscard]] constexpr std::uint16_t Rgb5551(int red, int green, int blue,
                                              int alpha) noexcept {
  constexpr int kGreenShift = 5;
  constexpr int kBlueShift = 10;
  constexpr int kAlphaShift = 15;
  return static_cast<std::uint16_t>(red | (green << kGreenShift) |
                                    (blue << kBlueShift) |
                                    (alpha << kAlphaShift));
}
using Palette = std::array<std::uint16_t, kPaletteColors>;
constexpr std::size_t kPaletteRows = 4;
constexpr std::array<Palette, kPaletteRows> kMenuItemPalettes{{
    {Rgb5551(0, 0, 0, 0), Rgb5551(0, 0, 2, 1), Rgb5551(2, 0, 3, 1),
     Rgb5551(3, 0, 4, 1), Rgb5551(6, 0, 0, 1), Rgb5551(9, 1, 0, 1),
     Rgb5551(11, 2, 0, 1), Rgb5551(13, 3, 0, 1), Rgb5551(14, 2, 0, 1),
     Rgb5551(15, 3, 0, 1), Rgb5551(16, 4, 0, 1), Rgb5551(16, 3, 0, 1),
     Rgb5551(16, 4, 0, 1), Rgb5551(17, 5, 0, 1), Rgb5551(18, 7, 0, 1),
     Rgb5551(19, 8, 0, 1)},
    {Rgb5551(0, 0, 0, 0), Rgb5551(0, 0, 2, 1), Rgb5551(5, 3, 5, 1),
     Rgb5551(7, 5, 7, 1), Rgb5551(11, 8, 7, 1), Rgb5551(16, 12, 9, 1),
     Rgb5551(20, 15, 10, 1), Rgb5551(24, 18, 11, 1), Rgb5551(28, 19, 11, 1),
     Rgb5551(31, 21, 10, 1), Rgb5551(27, 22, 15, 1), Rgb5551(31, 22, 12, 1),
     Rgb5551(31, 23, 13, 1), Rgb5551(31, 25, 16, 1), Rgb5551(31, 27, 18, 1),
     Rgb5551(31, 28, 19, 1)},
    {Rgb5551(0, 0, 0, 0), Rgb5551(0, 0, 1, 1), Rgb5551(0, 0, 4, 1),
     Rgb5551(0, 0, 5, 1), Rgb5551(0, 0, 8, 1), Rgb5551(0, 0, 10, 1),
     Rgb5551(0, 0, 12, 1), Rgb5551(0, 0, 14, 1), Rgb5551(0, 0, 15, 1),
     Rgb5551(0, 0, 18, 1), Rgb5551(0, 0, 20, 1), Rgb5551(0, 0, 22, 1),
     Rgb5551(0, 0, 24, 1), Rgb5551(0, 0, 26, 1), Rgb5551(1, 1, 29, 1),
     Rgb5551(2, 3, 31, 1)},
    {Rgb5551(0, 0, 0, 0), Rgb5551(0, 0, 3, 1), Rgb5551(0, 0, 3, 1),
     Rgb5551(4, 2, 0, 1), Rgb5551(3, 0, 7, 1), Rgb5551(7, 3, 3, 1),
     Rgb5551(12, 7, 4, 1), Rgb5551(19, 12, 1, 1), Rgb5551(24, 17, 1, 1),
     Rgb5551(28, 20, 1, 1), Rgb5551(30, 20, 1, 1), Rgb5551(31, 22, 0, 1),
     Rgb5551(31, 22, 2, 1), Rgb5551(26, 20, 10, 1), Rgb5551(31, 25, 4, 1),
     Rgb5551(28, 23, 10, 1)},
}};
constexpr std::size_t kTextPalette = 0;
constexpr std::size_t kOutlinePalette = 2;
// The rows _setMenuItemClut blends between: an idle item's text, a selected
// item's text, and a settings submenu's heading (the second outline row).
constexpr int kIdleTextRow = 0;
constexpr int kSelectedTextRow = 1;
constexpr int kSettingsHeadingRow = 3;

// The item palettes in VRAM: text palettes from row 384, one per item, and
// the outline palette at row 394.
constexpr int kPaletteColumn = 480;
constexpr int kTextPaletteRow = 384;
constexpr int kOutlinePaletteRow = 394;
constexpr std::size_t kLoadedTextPalettes = 8;

[[nodiscard]] constexpr ScreenRect PaletteArea(int row) noexcept {
  return {.x = kPaletteColumn,
          .y = static_cast<std::int16_t>(row),
          .width = static_cast<std::int16_t>(kPaletteColors),
          .height = 1};
}

constexpr auto k4Bit = TextureDepth::k4Bit;
constexpr auto kAdd = BlendMode::kAdd;
// decomp: TITLE.PRG outlineTpages (TITLE/TITLE.PRG data)
constexpr std::array<std::uint32_t, kMenuItemCount> kOutlinePages{
    DrawMode(k4Bit, kAdd, 0, 256),   DrawMode(k4Bit, kAdd, 64, 256),
    DrawMode(k4Bit, kAdd, 128, 256), DrawMode(k4Bit, kAdd, 448, 256),
    DrawMode(k4Bit, kAdd, 0, 256),   DrawMode(k4Bit, kAdd, 64, 256),
    DrawMode(k4Bit, kAdd, 128, 256), DrawMode(k4Bit, kAdd, 448, 256),
    DrawMode(k4Bit, kAdd, 448, 0),   DrawMode(k4Bit, kAdd, 448, 0),
};
// decomp: TITLE.PRG outlineUvs (TITLE/TITLE.PRG data)
constexpr std::array<std::int32_t, kMenuItemCount> kOutlineUvClut{
    UvClut(0, 192, 480, 394),   UvClut(0, 192, 480, 394),
    UvClut(0, 192, 480, 394),   UvClut(128, 0, 480, 394),
    UvClut(144, 192, 480, 394), UvClut(160, 192, 480, 394),
    UvClut(160, 192, 480, 394), UvClut(128, 64, 480, 394),
    UvClut(128, 0, 480, 394),   UvClut(128, 64, 480, 394),
};
// decomp: TITLE.PRG textTpages (TITLE/TITLE.PRG data)
constexpr std::array<std::uint32_t, kMenuItemCount> kTextPages{
    DrawMode(k4Bit, kAdd, 512, 256), DrawMode(k4Bit, kAdd, 576, 256),
    DrawMode(k4Bit, kAdd, 576, 256), DrawMode(k4Bit, kAdd, 640, 256),
    DrawMode(k4Bit, kAdd, 768, 256), DrawMode(k4Bit, kAdd, 704, 256),
    DrawMode(k4Bit, kAdd, 640, 256), DrawMode(k4Bit, kAdd, 704, 256),
    DrawMode(k4Bit, kAdd, 768, 256), DrawMode(k4Bit, kAdd, 832, 256),
};
// decomp: TITLE.PRG textUvs (TITLE/TITLE.PRG data)
constexpr std::array<std::int32_t, kMenuItemCount> kTextUvClut{
    UvClut(128, 224, 480, 384), UvClut(0, 224, 480, 385),
    UvClut(128, 224, 480, 386), UvClut(0, 224, 480, 387),
    UvClut(0, 224, 480, 388),   UvClut(0, 224, 480, 389),
    UvClut(128, 224, 480, 390), UvClut(128, 224, 480, 391),
    UvClut(128, 224, 480, 392), UvClut(0, 224, 480, 393),
};
// decomp: TITLE.PRG outlineWhs (TITLE/TITLE.PRG data)
constexpr std::array<std::int32_t, kMenuItemCount> kOutlineSizes{
    Pack(144, 64), Pack(160, 64), Pack(160, 64), Pack(128, 64), Pack(112, 64),
    Pack(96, 64),  Pack(96, 64),  Pack(128, 64), Pack(128, 64), Pack(128, 64),
};
// decomp: TITLE.PRG outlineXys (TITLE/TITLE.PRG data)
constexpr std::array<std::int32_t, kMenuItemCount> kOutlinePositions{
    Pack(120, 224), Pack(112, 224), Pack(112, 224), Pack(128, 224),
    Pack(136, 224), Pack(144, 224), Pack(144, 224), Pack(128, 224),
    Pack(128, 224), Pack(128, 224),
};

// The menu strip's two halves, drawn under the items.
constexpr MenuSpritePacket kMenuStripLeft{
    kSpriteTag,
    DrawMode(TextureDepth::k16Bit, BlendMode::kAverage, 192, 256),
    kNeutralSprite,
    Word(Pack(176, 256)),
    Word(UvClut(0, 192, 0, 0)),
    Word(Pack(160, 64)),
};
constexpr MenuSpritePacket kMenuStripRight{
    kSpriteTag,
    DrawMode(TextureDepth::k16Bit, BlendMode::kAverage, 320, 256),
    kNeutralSprite,
    Word(Pack(176, 320)),
    Word(UvClut(32, 192, 0, 0)),
    Word(Pack(160, 64)),
};

// The pad bits the title reads (PADLup ... PADselect).
constexpr std::uint32_t kPadCircle = 1U << 5;
constexpr std::uint32_t kPadCross = 1U << 6;
constexpr std::uint32_t kPadSelect = 1U << 8;
constexpr std::uint32_t kPadStart = 1U << 11;
constexpr std::uint32_t kPadUp = 1U << 12;
constexpr std::uint32_t kPadRight = 1U << 13;
constexpr std::uint32_t kPadDown = 1U << 14;
constexpr std::uint32_t kPadLeft = 1U << 15;
constexpr std::uint32_t kAnyButton = 0xFFFF;
constexpr std::uint32_t kConfirmButtons = kPadStart | kPadCircle;
constexpr std::uint32_t kToggleButtons = kPadUp | kPadDown | kPadSelect;

// The file screens' VRAM (_gameLoadScreen, 22C.c:3135-3149).
struct ImageUpload {
  Image image;
  ScreenRect area;
};
constexpr std::array<ImageUpload, 6> kFileScreenAtlases{{
    {Image::kSaveUiPageZero, {768, 0, 64, 224}},
    {Image::kSaveUiPageOne, {448, 256, 64, 256}},
    {Image::kSaveFontPageZero, {832, 0, 64, 224}},
    {Image::kSaveFontPageOne, {896, 0, 64, 224}},
    {Image::kDebugFont, {960, 66, 64, 158}},
    {Image::kSaveBackgroundPalette, {768, 227, 256, 1}},
}};
constexpr std::array<ScreenRect, 2> kFileScreenClears{{
    {640, 256, 32, 240},
    {768, 256, 32, 240},
}};
constexpr ScreenRect kFileScreenBackground{672, 256, 96, 240};
// UpdateLivePlayTimeAndLoading(2) each frame of the file screens.
constexpr int kFileScreenVsyncMode = 2;
// _gameLoadScreen opens the load menu with its fade (vs_menu7_loadMenu(2)).
constexpr int kLoadMenuFadeIn = 2;
constexpr int kClearSaveFadeFrames = 30;

// _displayPublisherAndDeveloper's logos (22C.c:3579).
constexpr ScreenRect kLogoClear{0, 0, 320, 512};
constexpr ScreenRect kPublisherPaletteArea{320, 64, 16, 1};
constexpr ScreenRect kPublisherPixelsArea{320, 0, 64, 48};
constexpr ScreenRect kDeveloperArea{0, 240, 32, 14};
constexpr std::int16_t kLogoWidth = 320;
constexpr std::int16_t kLogoHeight = 240;
constexpr int kLogoBufferRow = 256;
constexpr ScreenRect kLogoScreen{0, 8, 0, 224};
constexpr int kLogoFrames = 364;
constexpr int kLogoFadeFrames = 32;
constexpr int kLogoFadeOutStart = 331;
constexpr int kLogoFadeStep = 4;
constexpr Render::ImmediateSpriteRequest kPublisherSprite{
    .position = Pack(32, 88),
    .uv_clut = UvClut(0, 0, 320, 64),
    .size = Pack(256, 48),
    .texture_page = 320 / 64,
};
constexpr Render::ImmediateSpriteRequest kDeveloperSprite{
    .position = Pack(96, 104),
    .uv_clut = UvClut(0, 240, 0, 253),
    .size = Pack(128, 13),
    .texture_page = 0,
};

// The halfwords (pixels) an area of VRAM holds.
[[nodiscard]] constexpr std::size_t Halfwords(ScreenRect area) noexcept {
  return static_cast<std::size_t>(area.width) *
         static_cast<std::size_t>(area.height);
}

// _copyTitleBgData's image (22C.c:3773): the 544x512 background then the
// 544x64 menu strip.
constexpr ScreenRect kBackgroundArea{480, 0, 544, 512};
constexpr ScreenRect kMenuStripArea{0, 448, 544, 64};
constexpr std::size_t kBackgroundHalfwords = Halfwords(kBackgroundArea);
constexpr std::size_t kRunControlBits = 16;
constexpr std::size_t kHalfwordsPerWord = 2;

// _initMenu's capture block (0x22380 bytes, 22C.c:3838): the finished menu
// strip (StoreImage at 688,256 by vs_title_exec), the strip as uploaded, the
// strip being blended, the copyright's background and the copyright being
// blended.
constexpr std::size_t kCaptureHalfwords = 0x22380 / 2;
constexpr ScreenRect kFinishedStripArea{688, 256, 160, 128};
constexpr ScreenRect kStripLeftArea{192, 448, 160, 64};
constexpr ScreenRect kStripRightArea{352, 448, 160, 64};
constexpr ScreenRect kCopyrightArea{678, 376, 180, 24};
constexpr std::size_t kStripHalfHalfwords = Halfwords(kStripLeftArea);
constexpr std::size_t kStripHalfwords = 2 * kStripHalfHalfwords;
constexpr std::size_t kCopyrightHalfwords = Halfwords(kCopyrightArea);
static_assert(Halfwords(kFinishedStripArea) == kStripHalfwords);
// Where each part starts in the capture block, in halfwords.
constexpr std::size_t kStripSource = kStripHalfwords;
constexpr std::size_t kStripBlend = 2 * kStripHalfwords;
constexpr std::size_t kCopyrightSource = 3 * kStripHalfwords;
constexpr std::size_t kCopyrightBlend = kCopyrightSource + kCopyrightHalfwords;
static_assert(kCopyrightBlend + kCopyrightHalfwords == kCaptureHalfwords);

// The colour fields of a 5:5:5 pixel and the fixed-point shifts of the
// blends: 32nds for the menu and copyright, 16ths for the palettes.
constexpr std::array<int, 3> kColorFields{0x1F, 0x3E0, 0x7C00};
constexpr int kImageBlendShift = 5;
constexpr int kPaletteBlendShift = 4;
constexpr std::uint16_t kOpaqueBit = 0x8000;
constexpr int kStripBlendVsyncs = 4;
constexpr int kCopyrightBlendVsyncs = 2;

// The menu animation (_setTitleMenuState, 22C.c:3997).
constexpr int kFullSaturation = 128;
constexpr int kSaturationStep = 8;
constexpr int kSelectedPosition = 64;
constexpr int kAbovePosition = 32;
constexpr int kBelowPosition = 96;
constexpr int kEnteringBelowPosition = 0x80;
constexpr int kMaxBlend = 16;
constexpr int kBlendStep = 4;
constexpr int kOutlineStep = 16;
constexpr int kScrollStep = 4;
constexpr int kOutlineScrollStep = 2;
constexpr int kMenuSlotSpacing = 32;
constexpr int kMainItems = 4;
constexpr int kMainItemMask = 3;
constexpr int kNavigationFrames = 10;
constexpr int kSettleFrames = 8;

// _drawTitleMenuItems' text lines (22C.c:4073): 32 one-pixel lines in four
// 32-line bands of the scrolling strip.
constexpr int kTextLines = 32;
constexpr int kTextBandShift = 5;
constexpr int kTextBandMask = 3;
constexpr int kSubmenuTextX = 192;
constexpr int kTextX = 128;
constexpr int kTextLeft = 176;
constexpr int kTextRight = 336;
constexpr int kTextWidth = 128;
constexpr int kTextClipOffset = 48;
constexpr int kTextTop = 240;
constexpr int kFadeInBandStart = 32;
constexpr int kFadeOutBandEnd = 127;
constexpr int kOutlineSubmenuShift = 64;
constexpr std::int32_t kOutlineMotion = 0x10001;

// vs_title_exec's idle limits in vertical blanks (22C.c:4491).
constexpr int kIdleFrames = 1000;
constexpr int kMovieIdleFrames = 100;
constexpr int kLeaveFrames = 64;
constexpr int kCopyrightBlendStep = 2;
constexpr int kStripBlendStep = 4;
constexpr int kBlendStart = 32;
constexpr int kTimeoutCutscenes = 4;

// The settings submenus (_menuVibrationSettings, _menuSoundSettings).
constexpr int kSettingsBlendStep = 2;
constexpr int kSettingsSteps = 8;
constexpr int kToggleSteps = 7;
constexpr int kOutlineFadeInSteps = 4;
constexpr int kOutlineFadeInStep = 64;
constexpr int kChoiceFadeStep = 24;
constexpr int kChoicePairSum = 11;
constexpr int kVibrationChoiceBase = 5;
constexpr int kStereoChoice = 7;
constexpr int kMonoChoiceStep = 3;
constexpr std::uint32_t kDiscMixVolume = 0x7F;
constexpr std::array<int, 3> kClosingOutlines{64, 128, 192};
constexpr int kClosingPosition = 128;
constexpr int kOpeningAbovePosition = 0;

// _initScreen's gameplay buffers (22C.c:4658) and _initGameData's request.
constexpr int kGameDisplayMargin = 16;
constexpr ScreenRect kGameScreen{0, 8, 256, 224};
constexpr ScreenRect kWholeVram{0, 0, 1024, 512};
constexpr GameDisplayRequest kNewGameDisplay{
    .width = 640,
    .height = 240,
    .projection_distance = 512,
    .background = {},
};
constexpr TimeTrialScore kUnsetTrialScore{
    .time = -8388608,
    .round = 0,
    .difficulty = 0,
};
constexpr std::size_t kAshleySnapshotBytes = 0x50;
constexpr int kNewGameTickSpeed = 2;
constexpr EntryDoor kNewGameDoor{
    .tile_x = 4,
    .tile_z = 2,
    .direction = 0,
    .elevation = 2,
};

// _playNewGameSfx's three effects.
constexpr std::array<int, 3> kNewGameSounds{1, 2, 3};

[[nodiscard]] int Sound(Menu::MenuSound sound) noexcept {
  return static_cast<int>(sound);
}

[[nodiscard]] MenuItem& Item(State& state, MenuItemId id) {
  return state.items[static_cast<std::size_t>(id)];
}

[[nodiscard]] bool ValidItem(int item) noexcept {
  return item >= 0 && item < static_cast<int>(kMenuItemCount);
}

[[nodiscard]] bool ValidCapture(std::span<const std::uint16_t> capture) {
  return capture.size() >= kCaptureHalfwords;
}

// Whether `capture` holds the menu's 0x22380 bytes. An empty capture is the
// one InitializeMenu already refused; a short one is refused here.
[[nodiscard]] bool CaptureHolds(Host& host,
                                std::span<const std::uint16_t> capture) {
  if (ValidCapture(capture)) {
    return true;
  }
  if (!capture.empty()) {
    host.Refuse("title menu capture smaller than its 0x22380 bytes");
  }
  return false;
}

// `factor` steps of 1 / (1 << shift) from `from` towards `to` on each 5:5:5
// field in the original's signed 32-bit arithmetic, each field masked after
// the shift so a factor past the range wraps as on the PS1.
[[nodiscard]] std::uint16_t BlendFields(std::uint16_t from, std::uint16_t to,
                                        int factor, int shift) noexcept {
  std::uint32_t blended = 0;
  for (const int field : kColorFields) {
    const int base = from & field;
    const int delta = ((to & field) - base) * factor;
    blended |= static_cast<std::uint32_t>((base << shift) + delta) &
               (static_cast<std::uint32_t>(field) << shift);
  }
  return static_cast<std::uint16_t>(blended >> shift);
}

void PrepareFileScreen(Host& host) {
  for (const ImageUpload& upload : kFileScreenAtlases) {
    host.UploadImage(upload.image, upload.area);
  }
  for (const ScreenRect& area : kFileScreenClears) {
    host.ClearImage(area);
  }
  host.UploadImage(Image::kSaveBackgroundPixels, kFileScreenBackground);
  host.InitializeFileDisplay();
}

void BeginFileScreenFrame(Host& host) {
  host.AdvanceFrameClock(kFileScreenVsyncMode);
  host.SwapFileBuffers();
  host.UpdateControllers();
}

void DrawFileScreen(Host& host) {
  host.DrawFileBackground();
  host.DrawFileFrame();
}

enum class ClearSavePhase : std::uint8_t {
  kInitialize,
  kLoading,
  kReady,
  kConfirmation,
  kExit
};

struct ClearSaveScreen {
  ClearSavePhase phase = ClearSavePhase::kInitialize;
  int fade_frames = 0;
};

void BeginClearSaveExit(Host& host, ClearSaveScreen& screen) {
  host.ReleaseCardResources();
  host.DrawExitFade();
  screen.fade_frames = kClearSaveFadeFrames;
  screen.phase = ClearSavePhase::kExit;
}

// One frame of _gameSaveScreen's phases; true once the exit fade is over.
bool StepClearSaveScreen(Host& host, ClearSaveScreen& screen) {
  switch (screen.phase) {
    case ClearSavePhase::kInitialize:
      host.StepCardResources(1);
      screen.phase = ClearSavePhase::kLoading;
      return false;
    case ClearSavePhase::kLoading:
      if (host.StepCardResources(0) != 0) {
        host.ResetFileElements();
        host.UpdateSaveMenu(1);
        screen.phase = ClearSavePhase::kReady;
      }
      return false;
    case ClearSavePhase::kReady: {
      const int result = host.UpdateSaveMenu(0);
      if (result > 0) {
        BeginClearSaveExit(host, screen);
        return false;
      }
      if (result < 0) {
        host.ShowSavePrompt();
        host.UpdateClearSaveReminder(1);
        screen.phase = ClearSavePhase::kConfirmation;
      }
      break;
    }
    case ClearSavePhase::kConfirmation: {
      const int result = host.UpdateClearSaveReminder(0);
      if (result != 0 && result != 1) {
        BeginClearSaveExit(host, screen);
        return false;
      }
      if (result == 1) {
        host.UpdateSaveMenu(1);
        screen.phase = ClearSavePhase::kReady;
      }
      break;
    }
    case ClearSavePhase::kExit:
      host.DrawExitFade();
      return --screen.fade_frames == 0;
  }
  DrawFileScreen(host);
  return false;
}

[[nodiscard]] DisplaySetup LogoDisplay(int display_y, int draw_y) noexcept {
  return {
      .display = {0, static_cast<std::int16_t>(display_y), kLogoWidth,
                  kLogoHeight},
      .screen = kLogoScreen,
      .interlaced = false,
      .draw = {0, static_cast<std::int16_t>(draw_y), kLogoWidth, kLogoHeight}};
}

// Frame `frame` shows one buffer while the other is drawn.
[[nodiscard]] DisplaySetup LogoFrameDisplay(int frame) noexcept {
  const int shown = frame & 1;
  return LogoDisplay(shown * kLogoBufferRow, (1 - shown) * kLogoBufferRow);
}

// The dimming of logo frame `frame`: fading in over the first 32 frames and
// out after frame 331.
[[nodiscard]] int LogoAttenuation(int frame) noexcept {
  if (frame < kLogoFadeFrames) {
    return (kLogoFadeFrames - 1 - frame) * kLogoFadeStep;
  }
  if (frame > kLogoFadeOutStart) {
    return (frame - kLogoFadeOutStart) * kLogoFadeStep;
  }
  return 0;
}

void DrawLogo(Host& host, Render::ImmediateSpriteRequest sprite, int frame) {
  sprite.texture_page |= LogoAttenuation(frame) << kHalfwordShift;
  Render::DrawImmediateSprite(host, sprite);
}

void ShowPublisher(Host& host) {
  for (int frame = 0; frame < kLogoFrames; ++frame) {
    DrawLogo(host, kPublisherSprite, frame);
    host.WaitForVerticalBlank();
    host.PutDisplay(LogoFrameDisplay(frame));
  }
}

// A button held while the developer logo is up skips to its fade-out.
void ShowDeveloper(Host& host) {
  for (int frame = 0; frame < kLogoFrames; ++frame) {
    if (frame >= kLogoFadeFrames && frame <= kLogoFadeOutStart &&
        (host.Input().held & kAnyButton) != 0) {
      frame = kLogoFadeOutStart;
    }
    DrawLogo(host, kDeveloperSprite, frame);
    host.WaitForVerticalBlank();
    host.UpdateControllers();
    host.PutDisplay(LogoFrameDisplay(frame));
  }
}

// A settings submenu: `parent` opens over the main items, `above` and
// `below` move aside, and the two choices appear at the selected and lower
// positions.
struct SettingsMenu {
  MenuItemId parent;
  MenuItemId above;
  MenuItemId below;
  MenuItemId upper_choice;
  MenuItemId lower_choice;
  void (*confirm)(Host& host, int choice);
};

void ConfirmVibration(Host& host, int choice) {
  PlayerSettings settings = host.Settings();
  settings.vibration_on = Byte(choice - kVibrationChoiceBase);
  host.SetSettings(settings);
  if (choice == static_cast<int>(MenuItemId::kVibrationOn)) {
    host.PlayVibrationPreset(0);
  }
}

void ConfirmSound(Host& host, int choice) {
  const int mono = (choice + 1) & 1;
  PlayerSettings settings = host.Settings();
  settings.mono_sound = Byte(mono);
  host.SetSettings(settings);
  host.SetMonoOutput(mono);
  host.SetDiscMixVolume(kDiscMixVolume);
}

constexpr SettingsMenu kVibrationMenu{
    .parent = MenuItemId::kVibration,
    .above = MenuItemId::kContinue,
    .below = MenuItemId::kSound,
    .upper_choice = MenuItemId::kVibrationOn,
    .lower_choice = MenuItemId::kVibrationOff,
    .confirm = ConfirmVibration,
};
constexpr SettingsMenu kSoundMenu{
    .parent = MenuItemId::kSound,
    .above = MenuItemId::kVibration,
    .below = MenuItemId::kNewGame,
    .upper_choice = MenuItemId::kStereo,
    .lower_choice = MenuItemId::kMono,
    .confirm = ConfirmSound,
};

// The choice and its pair (11 - choice) both name items.
[[nodiscard]] bool ValidChoice(int choice) noexcept {
  return ValidItem(choice) && ValidItem(kChoicePairSum - choice);
}

[[nodiscard]] std::uint8_t FadingOutline(int step, std::uint8_t current) {
  return step < kOutlineFadeInSteps ? Byte(step * kOutlineFadeInStep)
                                    : Byte(current - kOutlineStep);
}

void OpenSettingsMenu(Context& context, const SettingsMenu& menu, int choice) {
  State& state = context.state;
  Host& host = context.host;
  const int parent = static_cast<int>(menu.parent);
  Item(state, menu.above).motion = MenuItemMotion::kMovingUp;
  Item(state, menu.below).motion = MenuItemMotion::kMovingDown;
  Item(state, menu.below).target_position = Byte(kClosingPosition);
  Item(state, menu.above).target_position = Byte(kOpeningAbovePosition);
  Item(state, menu.parent).motion = MenuItemMotion::kSubmenu;
  InitializeMenuItem(state, static_cast<int>(menu.upper_choice),
                     Byte(kSelectedPosition));
  InitializeMenuItem(state, static_cast<int>(menu.lower_choice),
                     Byte(kBelowPosition));
  Item(state, menu.lower_choice).motion = MenuItemMotion::kSubmenu;
  Item(state, menu.upper_choice).motion = MenuItemMotion::kSubmenu;
  BlendMenuItemClut(host, static_cast<int>(menu.lower_choice), 0, kIdleTextRow,
                    kIdleTextRow);
  BlendMenuItemClut(host, static_cast<int>(menu.upper_choice), 0, kIdleTextRow,
                    kIdleTextRow);
  for (int step = 1; step <= kSettingsSteps; ++step) {
    BlendMenuItemClut(host, parent, step * kSettingsBlendStep, kSelectedTextRow,
                      kSettingsHeadingRow);
    MenuItem& item = Item(state, menu.parent);
    item.text_position = Byte(item.text_position - kScrollStep);
    DrawMenuAndPollInput(context);
  }
  for (int item = 0; item < kMainItems; ++item) {
    if (item != parent) {
      state.items[static_cast<std::size_t>(item)].motion =
          MenuItemMotion::kStationary;
    }
  }
  for (int step = 1; step <= kSettingsSteps; ++step) {
    DrawMenuAndPollInput(context);
  }
  MenuItem& chosen = state.items[static_cast<std::size_t>(choice)];
  for (int step = 0; step < kSettingsSteps; ++step) {
    BlendMenuItemClut(host, choice, step * kSettingsBlendStep, kIdleTextRow,
                      kSelectedTextRow);
    chosen.outline_saturation = FadingOutline(step, chosen.outline_saturation);
    DrawMenuAndPollInput(context);
  }
}

// Moves the highlight to the other choice; returns it.
int ToggleSettingsChoice(Context& context, int choice) {
  State& state = context.state;
  Host& host = context.host;
  const int next = kChoicePairSum - choice;
  PlayMenuChangeSound(host);
  MenuItem& chosen = state.items[static_cast<std::size_t>(next)];
  MenuItem& left = state.items[static_cast<std::size_t>(choice)];
  for (int step = 1; step <= kToggleSteps; ++step) {
    BlendMenuItemClut(host, next, step * kSettingsBlendStep, kIdleTextRow,
                      kSelectedTextRow);
    BlendMenuItemClut(host, choice, step * kSettingsBlendStep, kSelectedTextRow,
                      kIdleTextRow);
    chosen.outline_saturation = FadingOutline(step, chosen.outline_saturation);
    left.outline_saturation = Byte(left.outline_saturation - kOutlineStep);
    if (step == kToggleSteps) {
      break;
    }
    DrawMenuAndPollInput(context);
  }
  return next;
}

void RunSettingsChoice(Context& context, const SettingsMenu& menu, int choice) {
  Host& host = context.host;
  while (true) {
    DrawMenuAndPollInput(context);
    const Buttons buttons = host.Input();
    if ((buttons.held & kConfirmButtons) != 0) {
      PlayMenuSelectSound(host);
      menu.confirm(host, choice);
      return;
    }
    if ((buttons.held & kPadCross) != 0) {
      PlayMenuLeaveSound(host);
      return;
    }
    if ((buttons.pressed & kToggleButtons) != 0) {
      choice = ToggleSettingsChoice(context, choice);
    }
  }
}

void FadeChoice(MenuItem& item) {
  item.text_saturation = item.text_saturation < kChoiceFadeStep
                             ? 0
                             : Byte(item.text_saturation - kChoiceFadeStep);
  if (item.outline_saturation != 0) {
    item.outline_saturation = Byte(item.outline_saturation - kOutlineStep);
  }
}

void CloseSettingsMenu(Context& context, const SettingsMenu& menu) {
  State& state = context.state;
  InitializeMenuItem(state, static_cast<int>(menu.above),
                     Byte(kOpeningAbovePosition));
  InitializeMenuItem(state, static_cast<int>(menu.below),
                     Byte(kClosingPosition));
  Item(state, menu.above).motion = MenuItemMotion::kMovingDown;
  Item(state, menu.above).target_position = Byte(kAbovePosition);
  Item(state, menu.below).motion = MenuItemMotion::kMovingUp;
  Item(state, menu.below).target_position = Byte(kBelowPosition);
  MenuItem& parent = Item(state, menu.parent);
  for (int step = 1; step <= kSettingsSteps; ++step) {
    BlendMenuItemClut(context.host, static_cast<int>(menu.parent),
                      step * kSettingsBlendStep, kSettingsHeadingRow,
                      kSelectedTextRow);
    parent.text_position = Byte(parent.text_position + kScrollStep);
    FadeChoice(Item(state, menu.lower_choice));
    FadeChoice(Item(state, menu.upper_choice));
    DrawMenuAndPollInput(context);
  }
  Item(state, menu.lower_choice).is_enabled = 0;
  Item(state, menu.upper_choice).is_enabled = 0;
  parent.outline_position = Byte(kSelectedPosition);
  for (std::size_t step = 0; step < kClosingOutlines.size(); ++step) {
    parent.outline_saturation = Byte(kClosingOutlines.at(step));
    if (step + 1 < kClosingOutlines.size()) {
      DrawMenuAndPollInput(context);
    } else {
      DrawMenu(context);
    }
  }
  parent.text_blend_factor = Byte(kMaxBlend);
  parent.motion = MenuItemMotion::kStationary;
}

void ShowSettingsMenu(Context& context, const SettingsMenu& menu, int choice) {
  if (!ValidChoice(choice)) {
    context.host.Refuse("title setting outside the submenu's items");
    return;
  }
  OpenSettingsMenu(context, menu, choice);
  RunSettingsChoice(context, menu, choice);
  CloseSettingsMenu(context, menu);
}

void AnimateMenuItem(Host& host, MenuItem& item, int index) {
  const int saturation =
      std::min(item.text_saturation + kSaturationStep, kFullSaturation);
  item.text_saturation = Byte(saturation);
  switch (item.motion) {
    case MenuItemMotion::kStationary: {
      if (saturation != kFullSaturation ||
          item.text_position != kSelectedPosition) {
        break;
      }
      item.outline_position = item.text_position;
      const int blend = item.text_blend_factor;
      if (blend < kMaxBlend) {
        item.outline_saturation = Byte(blend * kOutlineStep);
        item.text_blend_factor = Byte(std::min(blend + kBlendStep, kMaxBlend));
      }
      break;
    }
    case MenuItemMotion::kMovingUp:
      item.text_position = Byte(item.text_position - kScrollStep);
      if (item.text_position == item.target_position) {
        item.motion = MenuItemMotion::kStationary;
      }
      break;
    case MenuItemMotion::kMovingDown:
      item.text_position = Byte(item.text_position + kScrollStep);
      if (item.text_position == item.target_position) {
        item.motion = MenuItemMotion::kStationary;
      }
      break;
    case MenuItemMotion::kSubmenu:
      break;
  }
  const int blend = item.text_blend_factor;
  if (static_cast<int>(item.motion) <
      static_cast<int>(MenuItemMotion::kSubmenu)) {
    BlendMenuItemClut(host, index, blend, kIdleTextRow, kSelectedTextRow);
  }
  if (blend != 0 && item.text_position != kSelectedPosition) {
    item.text_blend_factor = Byte(blend - 1);
  }
  if (item.outline_saturation == 0) {
    return;
  }
  if (item.outline_position != item.text_position) {
    item.outline_saturation = Byte(item.outline_saturation - kOutlineStep);
    item.outline_position =
        Byte(item.outline_position < item.text_position
                 ? item.outline_position + kOutlineScrollStep
                 : item.outline_position - kOutlineScrollStep);
  } else if (item.text_blend_factor == kMaxBlend &&
             item.outline_saturation > kFullSaturation) {
    item.outline_saturation = Byte(item.outline_saturation - kOutlineStep);
  }
}

// The colour of a fully shown text line: opaque at full saturation.
[[nodiscard]] std::uint32_t ShownTextColor(int saturation) noexcept {
  if (saturation == kFullSaturation) {
    return kNeutralSprite;
  }
  return kSemiTransparentSprite |
         Word(saturation * static_cast<int>(kGreyLevel));
}

[[nodiscard]] std::uint32_t FadedTextColor(int scaled) noexcept {
  return kSemiTransparentSprite |
         Word(static_cast<std::int32_t>(scaled * static_cast<int>(kGreyLevel)));
}

[[nodiscard]] int InitialMenuItem(Host& host) {
  const int item = host.InitialTitleSelection();
  if (item < 0 || item >= kMainItems) {
    host.Refuse("title selection outside the main menu");
    return static_cast<int>(MenuItemId::kNewGame);
  }
  return item;
}

void FadeInMenu(Context& context, std::span<std::uint16_t> capture) {
  Host& host = context.host;
  for (int factor = kBlendStart; factor >= 0; factor -= kCopyrightBlendStep) {
    BlendCopyright(host, capture, factor);
  }
  host.WaitForVerticalBlank();
  if (CaptureHolds(host, capture)) {
    host.StoreImage(kFinishedStripArea, capture.first(kStripHalfwords));
  }
  host.WaitForVerticalBlank();
  for (int factor = kBlendStart; factor >= 0; factor -= kStripBlendStep) {
    BlendMenuBackground(host, capture, factor);
    DrawMenu(context);
  }
  ReleaseMenuCapture(host, capture);
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    DrawMenuAndPollInput(context);
  }
}

// Confirming the highlighted item: New Game and Continue leave the menu,
// the settings open their submenu.
std::optional<int> ChooseMenuItem(Context& context, int menu_item) {
  switch (static_cast<MenuItemId>(menu_item)) {
    case MenuItemId::kNewGame:
      return NewGameChoice();
    case MenuItemId::kContinue:
      return static_cast<int>(MenuItemId::kContinue);
    case MenuItemId::kVibration:
      PlayMenuSelectSound(context.host);
      ShowVibrationSettings(context);
      break;
    case MenuItemId::kSound:
      PlayMenuSelectSound(context.host);
      ShowSoundSettings(context);
      break;
    default:
      break;
  }
  return std::nullopt;
}

// Scrolls the main items one place in `direction` (1 down the list, -1 up).
void MoveMenu(Context& context, int& menu_item, int direction) {
  State& state = context.state;
  menu_item = (menu_item + direction) & kMainItemMask;
  PlayMenuChangeSound(context.host);
  const bool forward = direction == 1;
  InitializeMenuItem(
      state, (menu_item + (forward ? 1 : -1)) & kMainItemMask,
      Byte(forward ? kEnteringBelowPosition : kOpeningAbovePosition));
  for (int index = 0; index < kMainItems; ++index) {
    MenuItem& item = state.items[static_cast<std::size_t>(index)];
    item.motion =
        forward ? MenuItemMotion::kMovingUp : MenuItemMotion::kMovingDown;
    item.target_position = Byte(
        item.text_position + (forward ? -kMenuSlotSpacing : kMenuSlotSpacing));
  }
  for (int frame = 0; frame < kNavigationFrames; ++frame) {
    DrawMenuAndPollInput(context);
  }
}

[[nodiscard]] int NavigationDirection(std::uint32_t held) noexcept {
  int direction = 0;
  if ((held & (kPadUp | kPadLeft)) != 0) {
    --direction;
  }
  if ((held & (kPadSelect | kPadRight | kPadDown)) != 0) {
    ++direction;
  }
  return direction;
}

// vs_title_exec's menu loop: the chosen item, or kTimeout after 1000 idle
// frames once the intro movie has been stopped for 100.
int WaitForSelection(Context& context, int& menu_item) {
  Host& host = context.host;
  host.InvalidateControllerConnections();
  int last_movie_frame = host.VerticalBlankCount();
  int last_button_frame = host.VerticalBlankCount();
  while (true) {
    host.WaitForVerticalBlank();
    host.UpdateControllers();
    if (host.IntroMoviePlaying()) {
      last_movie_frame = host.VerticalBlankCount();
    }
    if ((host.Input().held & kAnyButton) != 0) {
      last_button_frame = host.VerticalBlankCount();
    }
    if (host.VerticalBlankCount() - last_button_frame > kIdleFrames &&
        host.VerticalBlankCount() - last_movie_frame > kMovieIdleFrames) {
      return static_cast<int>(MenuItemId::kTimeout);
    }
    if ((host.Input().held & kConfirmButtons) != 0) {
      const std::optional<int> choice = ChooseMenuItem(context, menu_item);
      if (choice.has_value()) {
        return *choice;
      }
      last_button_frame = host.VerticalBlankCount();
    }
    const int direction = NavigationDirection(host.Input().held);
    if (direction != 0) {
      MoveMenu(context, menu_item, direction);
    }
    DrawMenu(context);
  }
}

void LeaveMenu(Host& host) {
  for (int frame = kLeaveFrames; frame > 0; --frame) {
    host.WaitForVerticalBlank();
    host.UpdateControllers();
    host.Submit(TransitionTile());
    host.UpdateVibrationPatterns();
  }
  host.WaitForVerticalBlank();
  host.SetDisplayEnabled(false);
}

// One showing of the title menu; the selection, negative to show it again.
int ShowMenu(Context& context, int& menu_item) {
  State& state = context.state;
  Host& host = context.host;
  for (int index = kMainItems * 2 - 1; index >= 0; --index) {
    state.items[static_cast<std::size_t>(index)].is_enabled = 0;
  }
  LoadBackgroundTextures(host);
  host.StartIntroMovie();
  host.PlayIntroMovie();
  InitializeTitleDisplay(host);
  host.SetDisplayEnabled(true);
  FadeInMenu(context, InitializeMenu(context, menu_item));
  int selection = WaitForSelection(context, menu_item);
  if (selection >= static_cast<int>(MenuItemId::kNewGame)) {
    if (host.IntroMoviePlaying()) {
      host.StopIntroMovie();
    }
    if (selection == static_cast<int>(MenuItemId::kContinue)) {
      PlayMenuSelectSound(host);
    } else {
      PlayNewGameSounds(host);
      PrepareExit(host, selection);
    }
  }
  if (selection == static_cast<int>(MenuItemId::kTimeout)) {
    selection = kTimeoutCutscenes - (host.VisitCount() & 1);
    PrepareExit(host, selection);
  }
  LeaveMenu(host);
  if (selection == static_cast<int>(MenuItemId::kContinue)) {
    selection = ShowContinueScreen(host);
    if (selection == static_cast<int>(MenuItemId::kContinue)) {
      PrepareExit(host, selection);
    }
  }
  return selection;
}
}  // namespace

// decomp: TITLE.PRG _playNewGameSfx (src/TITLE/TITLE.PRG/22C.c:18)
void PlayNewGameSounds(Host& host) {
  for (const int effect : kNewGameSounds) {
    host.PlaySoundEffect(effect);
  }
}

// decomp: TITLE.PRG _playMenuChangeSfx (src/TITLE/TITLE.PRG/22C.c:25)
void PlayMenuChangeSound(Host& host) {
  host.PlaySoundEffect(Sound(Menu::MenuSound::kChange));
}

// decomp: TITLE.PRG _playMenuSelectSfx (src/TITLE/TITLE.PRG/22C.c:27)
void PlayMenuSelectSound(Host& host) {
  host.PlaySoundEffect(Sound(Menu::MenuSound::kSelect));
}

// decomp: TITLE.PRG _playMenuLeaveSfx (src/TITLE/TITLE.PRG/22C.c:29)
void PlayMenuLeaveSound(Host& host) {
  host.PlaySoundEffect(Sound(Menu::MenuSound::kLeave));
}

// decomp: TITLE.PRG _gameLoadScreen (src/TITLE/TITLE.PRG/22C.c:3129)
int ShowContinueScreen(Host& host) {
  enum class Phase : std::uint8_t { kInitialize, kLoadCardResources, kRun };
  PrepareFileScreen(host);
  Phase phase = Phase::kInitialize;
  while (true) {
    BeginFileScreenFrame(host);
    switch (phase) {
      case Phase::kInitialize:
        host.StepCardResources(1);
        phase = Phase::kLoadCardResources;
        break;
      case Phase::kLoadCardResources:
        if (host.StepCardResources(0) != 0) {
          host.ResetFileElements();
          host.UpdateLoadMenu(kLoadMenuFadeIn);
          phase = Phase::kRun;
        }
        break;
      case Phase::kRun: {
        const int result = host.UpdateLoadMenu(0);
        if (result != 0) {
          host.ReleaseCardResources();
          host.SetDisplayEnabled(false);
          return result;
        }
        DrawFileScreen(host);
        break;
      }
    }
  }
}

// decomp: TITLE.PRG _gameSaveScreen (src/TITLE/TITLE.PRG/22C.c:3313)
void ShowClearSaveScreen(Host& host) {
  PrepareFileScreen(host);
  ClearSaveScreen screen;
  bool done = false;
  while (!done) {
    BeginFileScreenFrame(host);
    host.AdvanceCardCipher();
    done = StepClearSaveScreen(host, screen);
  }
  host.SetDisplayEnabled(false);
}

// decomp: TITLE.PRG _displayPublisherAndDeveloper
// (src/TITLE/TITLE.PRG/22C.c:3579)
void ShowPublisherAndDeveloper(Host& host) {
  host.ClearImage(kLogoClear);
  host.UploadImage(Image::kPublisherPalette, kPublisherPaletteArea);
  host.UploadImage(Image::kPublisherPixels, kPublisherPixelsArea);
  host.PutDisplay(LogoDisplay(kLogoBufferRow, 0));
  host.WaitForVerticalBlank();
  host.SetDisplayEnabled(true);
  ShowPublisher(host);
  host.UploadImage(Image::kDeveloperLogo, kDeveloperArea);
  ShowDeveloper(host);
  host.SetDisplayEnabled(false);
}

// decomp: TITLE.PRG _setMenuItemFadeIn (src/TITLE/TITLE.PRG/22C.c:3761)
void InitializeMenuItem(State& state, int item, std::uint8_t position) {
  if (!ValidItem(item)) {
    return;
  }
  state.items[static_cast<std::size_t>(item)] = {
      .is_enabled = 1,
      .motion = MenuItemMotion::kStationary,
      .text_saturation = 0,
      .outline_saturation = 0,
      .text_position = position,
      .target_position = position,
      .outline_position = position,
      .text_blend_factor = 0};
}

// decomp: TITLE.PRG _copyTitleBgData (src/TITLE/TITLE.PRG/22C.c:3773)
void LoadBackgroundTextures(Host& host) {
  constexpr std::size_t kImageHalfwords =
      kBackgroundHalfwords + Halfwords(kMenuStripArea);
  const std::span<std::uint16_t> image = host.AllocateLow(kImageHalfwords);
  if (image.size() < kImageHalfwords) {
    host.Refuse("no heap for the title background");
    if (!image.empty()) {
      host.FreeLow(image);
    }
    return;
  }
  if (!DecodeBackground(host.BackgroundWords(), image).has_value()) {
    host.Refuse("title background encoding overruns its image");
  }
  host.LoadImage(kBackgroundArea, image.first(kBackgroundHalfwords));
  host.LoadImage(kMenuStripArea, image.subspan(kBackgroundHalfwords));
  for (std::size_t item = 0; item < kLoadedTextPalettes; ++item) {
    host.LoadImage(PaletteArea(kTextPaletteRow + static_cast<int>(item)),
                   kMenuItemPalettes[kTextPalette]);
  }
  host.LoadImage(PaletteArea(kOutlinePaletteRow),
                 kMenuItemPalettes[kOutlinePalette]);
  host.FreeLow(image);
}

// decomp: TITLE.PRG _initTitleScreen (src/TITLE/TITLE.PRG/22C.c:3821)
void InitializeTitleDisplay(Host& host) {
  constexpr int kPutLine = 248;
  constexpr DisplaySetup kTitleDisplay{.display = {512, 16, 512, 480},
                                       .screen = {0, 8, 0, 224},
                                       .interlaced = true,
                                       .draw = {512, 0, 512, 480}};
  while (host.HorizontalBlankCount() < kPutLine) {
  }
  host.PutDisplay(kTitleDisplay);
  host.WaitForVerticalBlank();
}

// decomp: TITLE.PRG _initMenu (src/TITLE/TITLE.PRG/22C.c:3838)
std::span<std::uint16_t> InitializeMenu(Context& context, int item) {
  State& state = context.state;
  Host& host = context.host;
  for (MenuItem& entry : state.items) {
    entry.is_enabled = 0;
  }
  InitializeMenuItem(state, item, Byte(kSelectedPosition));
  InitializeMenuItem(state, (item + 1) & kMainItemMask, Byte(kBelowPosition));
  InitializeMenuItem(state, (item + kMainItemMask) & kMainItemMask,
                     Byte(kAbovePosition));
  const std::span<std::uint16_t> capture = host.AllocateLow(kCaptureHalfwords);
  if (!ValidCapture(capture)) {
    host.Refuse("no heap for the title menu capture");
    if (!capture.empty()) {
      host.FreeLow(capture);
    }
    return {};
  }
  host.StoreImage(kStripLeftArea,
                  capture.subspan(kStripSource, kStripHalfHalfwords));
  host.StoreImage(
      kStripRightArea,
      capture.subspan(kStripSource + kStripHalfHalfwords, kStripHalfHalfwords));
  host.StoreImage(kCopyrightArea,
                  capture.subspan(kCopyrightSource, kCopyrightHalfwords));
  return capture;
}

// decomp: TITLE.PRG _freeHeap (src/TITLE/TITLE.PRG/22C.c:3867)
void ReleaseMenuCapture(Host& host, std::span<std::uint16_t> capture) {
  if (!capture.empty()) {
    host.FreeLow(capture);
  }
}

// decomp: TITLE.PRG _fadeInMenu (src/TITLE/TITLE.PRG/22C.c:3869)
void BlendMenuBackground(Host& host, std::span<std::uint16_t> capture,
                         int factor) {
  if (CaptureHolds(host, capture)) {
    const auto target = capture.first(kStripHalfwords);
    const auto source = capture.subspan(kStripSource, kStripHalfwords);
    const auto blended = capture.subspan(kStripBlend, kStripHalfwords);
    std::ranges::transform(target, source, blended.begin(),
                           [factor](std::uint16_t to, std::uint16_t from) {
                             return BlendMenuBackgroundPixel(to, from, factor);
                           });
    host.LoadImage(kStripLeftArea, blended.first(kStripHalfHalfwords));
    host.LoadImage(kStripRightArea, blended.subspan(kStripHalfHalfwords));
  }
  host.WaitForVerticalBlanks(kStripBlendVsyncs);
  host.UpdateControllers();
}

// decomp: TITLE.PRG _fadeInMenuCopyright (src/TITLE/TITLE.PRG/22C.c:3914)
void BlendCopyright(Host& host, std::span<std::uint16_t> capture, int factor) {
  const std::span<const std::uint16_t> copyright = host.CopyrightPixels();
  const bool valid = CaptureHolds(host, capture);
  if (valid && copyright.size() < kCopyrightHalfwords) {
    host.Refuse("title copyright smaller than 180x24");
  }
  const bool blend = valid && copyright.size() >= kCopyrightHalfwords;
  const auto blended =
      blend ? capture.subspan(kCopyrightBlend, kCopyrightHalfwords)
            : std::span<std::uint16_t>{};
  if (blend) {
    std::ranges::transform(
        capture.subspan(kCopyrightSource, kCopyrightHalfwords),
        copyright.first(kCopyrightHalfwords), blended.begin(),
        [factor](std::uint16_t background, std::uint16_t text) {
          return BlendCopyrightPixel(background, text, factor);
        });
  }
  host.WaitForVerticalBlanks(kCopyrightBlendVsyncs);
  host.UpdateControllers();
  if (blend) {
    host.LoadImage(kCopyrightArea, blended);
  }
}

// decomp: TITLE.PRG _setMenuItemClut (src/TITLE/TITLE.PRG/22C.c:3957)
void BlendMenuItemClut(Host& host, int item, int factor, int from, int to) {
  if (!ValidItem(item) || from < 0 || to < 0 ||
      from >= static_cast<int>(kPaletteRows) ||
      to >= static_cast<int>(kPaletteRows)) {
    host.Refuse("title item palette outside its tables");
    return;
  }
  const Palette& start = kMenuItemPalettes[static_cast<std::size_t>(from)];
  const Palette& end = kMenuItemPalettes[static_cast<std::size_t>(to)];
  Palette palette{};
  for (std::size_t color = 1; color < kPaletteColors; ++color) {
    palette.at(color) =
        BlendPaletteEntry(start.at(color), end.at(color), factor);
  }
  host.LoadImage(PaletteArea(kTextPaletteRow + item), palette);
}

// decomp: TITLE.PRG _setTitleMenuState (src/TITLE/TITLE.PRG/22C.c:3997)
void UpdateMenuAnimation(Context& context) {
  for (std::size_t index = 0; index < kMenuItemCount; ++index) {
    MenuItem& item = context.state.items.at(index);
    if (item.is_enabled != 0) {
      AnimateMenuItem(context.host, item, static_cast<int>(index));
    }
  }
}

// decomp: TITLE.PRG _drawTitleMenuItems (src/TITLE/TITLE.PRG/22C.c:4073)
void DrawMenuItems(Context& context) {
  const State& state = context.state;
  for (std::size_t index = kMenuItemCount; index-- > 0;) {
    const MenuItem& item = state.items.at(index);
    if (item.is_enabled != 0 && item.outline_saturation != 0) {
      context.host.Submit(OutlinePacket(item, index));
    }
  }
  for (std::size_t index = kMenuItemCount; index-- > 0;) {
    const MenuItem& item = state.items.at(index);
    if (item.is_enabled == 0) {
      continue;
    }
    for (int line = 0; line < kTextLines; ++line) {
      const std::optional<MenuSpritePacket> packet =
          TextLinePacket(item, index, line);
      if (packet.has_value()) {
        context.host.Submit(*packet);
      }
    }
  }
}

// decomp: TITLE.PRG _drawTitleMenu (src/TITLE/TITLE.PRG/22C.c:4183): drawn
// once for each interlaced field.
void DrawMenu(Context& context) {
  Host& host = context.host;
  UpdateMenuAnimation(context);
  for (int field = 0; field < 2; ++field) {
    host.Submit(kMenuStripLeft);
    host.Submit(kMenuStripRight);
    DrawMenuItems(context);
    if (field == 0) {
      host.WaitForVerticalBlank();
      host.UpdateVibrationPatterns();
    } else {
      host.StopIntroMovieAfterAudioTail();
    }
  }
}

// decomp: TITLE.PRG _drawAndSyncTitleMenu (src/TITLE/TITLE.PRG/22C.c:4227)
void DrawMenuAndPollInput(Context& context) {
  DrawMenu(context);
  context.host.WaitForVerticalBlank();
  context.host.UpdateControllers();
}

// decomp: TITLE.PRG _menuVibrationSettings (src/TITLE/TITLE.PRG/22C.c:4246)
void ShowVibrationSettings(Context& context) {
  const int choice =
      context.host.Settings().vibration_on + kVibrationChoiceBase;
  ShowSettingsMenu(context, kVibrationMenu, choice);
}

// decomp: TITLE.PRG _menuSoundSettings (src/TITLE/TITLE.PRG/22C.c:4353)
void ShowSoundSettings(Context& context) {
  const int choice =
      kStereoChoice - context.host.Settings().mono_sound * kMonoChoiceStep;
  ShowSettingsMenu(context, kSoundMenu, choice);
}

// decomp: TITLE.PRG _nop (src/TITLE/TITLE.PRG/22C.c:4461)
int NewGameChoice() noexcept { return static_cast<int>(MenuItemId::kNewGame); }

// decomp: TITLE.PRG _initEnvironment (src/TITLE/TITLE.PRG/22C.c:4463)
void InitializeSettings(Context& context) {
  Host& host = context.host;
  PlayerSettings settings = host.Settings();
  host.ResetPlayerSettings();
  if (host.VisitCount() == 0) {
    ShowPublisherAndDeveloper(host);
    settings = {.mono_sound = 0, .vibration_on = 1};
  }
  settings = {.mono_sound = Byte(settings.mono_sound != 0 ? 1 : 0),
              .vibration_on = Byte(settings.vibration_on != 0 ? 1 : 0)};
  host.SetSettings(settings);
  host.SetMonoOutput(settings.mono_sound);
  host.SetDiscMixVolume(kDiscMixVolume);
  host.ClearInventoryIndices();
  host.SetGameModes({.difficulty = 1, .puzzle_mode = 1});
}

// decomp: TITLE.PRG vs_title_exec (src/TITLE/TITLE.PRG/22C.c:4491)
int Run(Context& context) {
  Host& host = context.host;
  if (host.ClearSavePending()) {
    host.StageClearGame();
    host.ClearPendingClearSave();
    host.SetVisitCount(0);
    ShowClearSaveScreen(host);
  }
  InitializeNewGameData(host);
  InitializeSettings(context);
  host.ClearIntroMoviePlaying();
  host.SetVisitCount(host.VisitCount() + 1);
  int menu_item = InitialMenuItem(host);
  int selection = static_cast<int>(MenuItemId::kTimeout);
  while (selection < static_cast<int>(MenuItemId::kNewGame)) {
    selection = ShowMenu(context, menu_item);
  }
  return selection;
}

// decomp: TITLE.PRG _initScreen (src/TITLE/TITLE.PRG/22C.c:4658)
void InitializeGameDisplay(Host& host, const GameDisplayRequest& request) {
  host.SetDisplayEnabled(false);
  host.SetGeometryOffset(request.width / 2, request.height / 2);
  host.SetProjectionDistance(request.projection_distance);
  host.ApplyGameDisplay(PlanGameDisplay(request));
  host.ClearImage(kWholeVram);
  host.ShowFrameBuffer(host.FrameBuffer());
}

// decomp: TITLE.PRG _renderScreen (src/TITLE/TITLE.PRG/22C.c:4684)
int PresentGameFrame(Host& host) {
  const int buffer = host.FrameBuffer() == 0 ? 1 : 0;
  host.SetFrameBuffer(buffer);
  const int vsync = host.AdvanceFrameClock(0);
  host.ShowFrameBuffer(buffer);
  host.DrawOrderingTable();
  return vsync;
}

// decomp: TITLE.PRG _initGameData (src/TITLE/TITLE.PRG/22C.c:4698). The
// native session reset comes first.
void InitializeNewGameData(Host& host) {
  host.ResetSession();
  InitializeGameDisplay(host, kNewGameDisplay);
  host.ClearStateFlags();
  host.RestoreInitialLearnedActions();
  host.ClearProgressRecords();
  host.SetBossTimeTrialScores(kUnsetTrialScore);
  host.ClearAshleySnapshot(kAshleySnapshotBytes);
  host.ClearSavedEntryDoor();
  host.RestoreNewGameInventory();
  host.SetTickSpeed(kNewGameTickSpeed);
  host.SetEntryDoor(kNewGameDoor);
}

// decomp: TITLE.PRG _setTitleExitFlags (src/TITLE/TITLE.PRG/22C.c:4741)
void PrepareExit(Host& host, int cause) { host.ApplyExit(PlanExit(cause)); }

ExitPlan PlanExit(int cause) noexcept {
  constexpr int kTimeoutFirstCutscene = 3;
  constexpr int kTimeoutSecondCutscene = 4;
  constexpr Destination kNewGameDestination{.zone = 1, .room = 0};
  constexpr Destination kFirstCutscene{.zone = 20, .room = 0};
  constexpr Destination kSecondCutscene{.zone = 2, .room = 1};
  switch (cause) {
    case static_cast<int>(MenuItemId::kNewGame):
      return {.state_flag_0c = 0,
              .state_flag_0d = 0,
              .state_flag_0e = std::nullopt,
              .destination = kNewGameDestination};
    case kTimeoutFirstCutscene:
      return {.state_flag_0c = 0,
              .state_flag_0d = 1,
              .state_flag_0e = 1,
              .destination = kFirstCutscene};
    case kTimeoutSecondCutscene:
      return {.state_flag_0c = 0,
              .state_flag_0d = 1,
              .state_flag_0e = 1,
              .destination = kSecondCutscene};
    default:
      return {.state_flag_0c = 0,
              .state_flag_0d = 0,
              .state_flag_0e = std::nullopt,
              .destination = std::nullopt};
  }
}

GameDisplayLayout PlanGameDisplay(const GameDisplayRequest& request) noexcept {
  const auto width = static_cast<std::int16_t>(request.width);
  const auto height =
      static_cast<std::int16_t>(request.height - kGameDisplayMargin);
  const ScreenRect left{.x = 0, .y = 0, .width = width, .height = height};
  const ScreenRect right{.x = width, .y = 0, .width = width, .height = height};
  return {.draw_areas = {left, right},
          .display_areas = {right, left},
          .screen = kGameScreen,
          .background = request.background};
}

std::optional<std::size_t> DecodeBackground(
    std::span<const std::uint32_t> encoded, std::span<std::uint16_t> out) {
  const std::size_t capacity = out.size() / kHalfwordsPerWord;
  std::size_t read = 0;
  std::size_t written = 0;
  while (read < encoded.size()) {
    const std::uint32_t control = encoded[read++];
    const std::size_t zeros = control & kHalfwordMask;
    const std::size_t literals = control >> kRunControlBits;
    if (zeros + literals > capacity - written ||
        literals > encoded.size() - read) {
      return std::nullopt;
    }
    std::fill_n(out.subspan(written * kHalfwordsPerWord).begin(),
                zeros * kHalfwordsPerWord, std::uint16_t{0});
    written += zeros;
    for (const std::uint32_t word : encoded.subspan(read, literals)) {
      out[written * kHalfwordsPerWord] =
          static_cast<std::uint16_t>(word & kHalfwordMask);
      out[written * kHalfwordsPerWord + 1] =
          static_cast<std::uint16_t>(word >> kRunControlBits);
      ++written;
    }
    read += literals;
  }
  return written * kHalfwordsPerWord;
}

std::uint16_t BlendMenuBackgroundPixel(std::uint16_t target,
                                       std::uint16_t source,
                                       int factor) noexcept {
  return BlendFields(source, target, factor, kImageBlendShift) | kOpaqueBit;
}

std::uint16_t BlendCopyrightPixel(std::uint16_t capture,
                                  std::uint16_t copyright,
                                  int factor) noexcept {
  return BlendFields(copyright, capture, factor, kImageBlendShift);
}

std::uint16_t BlendPaletteEntry(std::uint16_t from, std::uint16_t to,
                                int factor) noexcept {
  return BlendFields(from, to, factor, kPaletteBlendShift) | kOpaqueBit;
}

MenuSpritePacket OutlinePacket(const MenuItem& item,
                               std::size_t index) noexcept {
  if (index >= kMenuItemCount) {
    return {};
  }
  const std::int32_t base = kOutlinePositions[index];
  const std::int32_t position =
      item.motion == MenuItemMotion::kSubmenu
          ? base + (item.outline_position << kHalfwordShift) +
                kOutlineSubmenuShift
          : base + item.outline_position * kOutlineMotion;
  return {kSpriteTag,
          kOutlinePages[index],
          kSemiTransparentSprite | (item.outline_saturation * kGreyLevel),
          Word(position),
          Word(kOutlineUvClut[index]),
          Word(kOutlineSizes[index])};
}

std::optional<MenuSpritePacket> TextLinePacket(const MenuItem& item,
                                               std::size_t index,
                                               int line) noexcept {
  const int scanline = line + item.text_position;
  const int band = (scanline >> kTextBandShift) & kTextBandMask;
  if (index >= kMenuItemCount || band == 0) {
    return std::nullopt;
  }
  const int saturation = item.text_saturation;
  const bool submenu = item.motion == MenuItemMotion::kSubmenu;
  int x = submenu ? kSubmenuTextX : item.text_position + kTextX;
  std::int32_t uv_clut = kTextUvClut[index];
  int width = kTextWidth;
  if (x < kTextLeft) {
    uv_clut += kTextLeft - x;
    width = x - kTextClipOffset;
    x = kTextLeft;
  }
  if (x + width > kTextRight) {
    width = kTextRight - x;
  }
  std::uint32_t color = ShownTextColor(saturation);
  if (!submenu && band == 1) {
    color = FadedTextColor((saturation * (scanline - kFadeInBandStart)) >>
                           kTextBandShift);
  } else if (!submenu && band == kTextBandMask) {
    color = FadedTextColor((saturation * (kFadeOutBandEnd - scanline)) >>
                           kTextBandShift);
  }
  // The original ORs the halves unmasked: a line clipped past the right edge
  // keeps its negative width's sign bits.
  constexpr std::uint32_t kOneRow = 1U << kHalfwordShift;
  return MenuSpritePacket{
      kSpriteTag,
      kTextPages[index],
      color,
      Word(x) | (Word(scanline + kTextTop) << kHalfwordShift),
      Word(uv_clut + (line << kByteShift)),
      Word(width) | kOneRow};
}

TransitionTilePacket TransitionTile() noexcept {
  constexpr std::uint32_t kDarkenColor = 0x62080808;
  constexpr int kPageX = 320;
  constexpr int kScreenWidth = 640;
  constexpr int kScreenHeight = 480;
  return {kTileTag,
          DrawMode(TextureDepth::k16Bit, BlendMode::kSubtract, kPageX, 0),
          kDarkenColor, 0, Word(Pack(kScreenWidth, kScreenHeight))};
}
}  // namespace VS::Title
