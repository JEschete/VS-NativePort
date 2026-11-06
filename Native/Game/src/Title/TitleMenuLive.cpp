// Hand-maintained (tools/port/owned.csv).
// Live binding for TITLE.PRG's title (Title/TitleMenu.cpp): the pad and
// player settings, the intro movie, VRAM and the display environments, the
// low end of the heap, the menu sounds, the module images, the file screens
// and the game state a new game starts from, in the legacy globals the rest
// of the game reads.
#include "Title/TitleMenuLive.hpp"  // NOLINT(build/include): typed API

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include "Audio/SfxLive.hpp"
#include "Audio/SoundBanksLive.hpp"
#include "Audio/XaMixLive.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Input/ControllerLive.hpp"
#include "Menu/FilesLive.hpp"
#include "Menu/Sounds.hpp"
#include "Movie/IntroPlayerLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Save/FileMenuLive.hpp"
#include "Save/MemoryCard/GameFilesLive.hpp"
#include "Save/MenuAssetsLive.hpp"
#include "State/GameClockLive.hpp"
#include "Title/TitleMenu.hpp"  // NOLINT(build/include): typed API

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Boot/GameLoop.h"
#include "Boot/PlayerSettings.h"
#include "Content/StringTables.h"
#include "Core/Gte.h"
#include "Field/Doors.h"
#include "Field/Mpd.h"
#include "Input/ControllerLiveState.h"
#include "Legacy/Psx.h"
#include "Menu/FilesLiveState.h"
#include "Movie/Intro/Playback.h"
#include "Render/Display.h"
#include "Save/NativeMenu.h"
#include "Save/NativeResume.h"
#include "State/Ashley.h"
#include "State/Location.h"
#include "State/PlayTime.h"
#include "State/Progress.h"
#include "State/RoomMemory.h"
#include "State/StateFlags.h"
#include "State/Timers.h"
#include "Title/Credits.h"
#include "Title/NewGame.h"
#include "Title/SaveFiles.h"
#include "Title/TitleMenuLiveState.h"
#include "Title/TitleScreen.h"

// decomp: vs_main_titleScreenCount (vs_main_titleScreenCount@SLUS_010.40)
int Title_titleScreenVisitCount = 0;
}

namespace VS::Title {
namespace {
constexpr const char* kChannel = "title";
constexpr std::size_t kPixelBytes = 2;
constexpr int kHalfwordShift = 16;
constexpr std::uint32_t kHalfwordMask = 0xFFFF;
// The 16-word primitive buffer DrawPrim reads the packets from.
constexpr std::size_t kPacketWords = 16;
// _initGameData clears the 0x440 bytes of unused state flags.
constexpr std::size_t kStateFlagsBytes = 0x440;
// The bosses' time-trial records: eight bosses, three ranks each.
constexpr std::size_t kTrialBosses = 8;
constexpr std::size_t kTrialRanks = 3;
constexpr std::uint32_t kTrialTimeMask = 0xFFFFFF;
// The sizes of the TITLE.PRG data the module globals stand in for.
constexpr std::size_t kSaveUiBytes = 0xF000;
constexpr std::size_t kSaveFontBytes = 0x7000;
constexpr std::size_t kSaveBackgroundBytes = 0xB600;
constexpr std::size_t kBackgroundEncodingBytes = 222932;
constexpr std::size_t kCopyrightBytes = std::size_t{180} * 24 * kPixelBytes;
constexpr std::size_t kPublisherLogoBytes = 6176;
constexpr std::size_t kDeveloperLogoBytes = 896;

static_assert(sizeof(State_StateFlags) == kStateFlagsBytes);
static_assert(sizeof(Title_SaveMenuUiRecord) == kSaveUiBytes);
static_assert(sizeof(Title_SaveMenuFontRecord) == kSaveFontBytes);
static_assert(sizeof(Title_SaveMenuBackgroundRecord) == kSaveBackgroundBytes);
static_assert(sizeof(Title_titleScreenBackground) == kBackgroundEncodingBytes);
static_assert(sizeof(Title_copyrightImage) == kCopyrightBytes);
static_assert(sizeof(Title_publisherLogo) == kPublisherLogoBytes);
static_assert(sizeof(Title_developerLogo) == kDeveloperLogoBytes);
static_assert(sizeof(Title_initialLearnedActionBits) ==
              sizeof(State_learnedActionFlags));
static_assert(std::size(Audio_Seq_unusedScoredata.bossTimeTrialScores) ==
              kTrialBosses);
static_assert(std::size(Audio_Seq_unusedScoredata.bossTimeTrialScores[0]) ==
              kTrialRanks);

void Warn(std::string_view what) { VS_LOG_WARN(kChannel, "{}", what); }

[[nodiscard]] int Pack(int low, int high) {
  return static_cast<int>((static_cast<std::uint32_t>(high) << kHalfwordShift) |
                          (static_cast<std::uint32_t>(low) & kHalfwordMask));
}

[[nodiscard]] RECT ToRect(const Render::ScreenRect& area) {
  return {area.x, area.y, area.width, area.height};
}

// The bytes a VRAM area covers; none for a negative extent.
[[nodiscard]] std::size_t AreaBytes(const Render::ScreenRect& area) {
  if (area.width < 0 || area.height < 0) {
    return 0;
  }
  return static_cast<std::size_t>(area.width) *
         static_cast<std::size_t>(area.height) * kPixelBytes;
}

[[nodiscard]] bool CoversArea(const Render::ScreenRect& area,
                              std::size_t pixels) {
  if (area.width >= 0 && area.height >= 0 &&
      pixels * kPixelBytes >= AreaBytes(area)) {
    return true;
  }
  VS_LOG_WARN(kChannel,
              "Title VRAM transfer refused: {}x{} at ({}, {}) has {} pixels",
              area.width, area.height, area.x, area.y, pixels);
  return false;
}

struct ImageSource {
  const void* pixels;
  std::size_t bytes;
};

// The module image `image` names (TITLE.PRG data).
[[nodiscard]] ImageSource Source(Image image) {
  constexpr std::size_t kPaletteBytes = 32;
  switch (image) {
    case Image::kSaveUiPageZero:
      return {Title_saveMenuUiAtlas.pageZeroPixels,
              sizeof Title_saveMenuUiAtlas.pageZeroPixels};
    case Image::kSaveUiPageOne:
      return {Title_saveMenuUiAtlas.pageOnePixels,
              sizeof Title_saveMenuUiAtlas.pageOnePixels};
    case Image::kSaveFontPageZero:
      return {&Title_saveMenuFontAtlas[0], sizeof Title_saveMenuFontAtlas[0]};
    case Image::kSaveFontPageOne:
      return {&Title_saveMenuFontAtlas[1], sizeof Title_saveMenuFontAtlas[1]};
    case Image::kDebugFont:
      return {Title_debugFontAtlas, sizeof Title_debugFontAtlas};
    case Image::kSaveBackgroundPalette:
      return {Title_saveMenuBackground.palette,
              sizeof Title_saveMenuBackground.palette};
    case Image::kSaveBackgroundPixels:
      return {Title_saveMenuBackground.indexedPixels,
              sizeof Title_saveMenuBackground.indexedPixels};
    case Image::kPublisherPalette:
      return {Title_publisherLogo, kPaletteBytes};
    case Image::kPublisherPixels:
      return {Title_publisherLogo + kPaletteBytes,
              sizeof Title_publisherLogo - kPaletteBytes};
    case Image::kDeveloperLogo:
      return {Title_developerLogo, sizeof Title_developerLogo};
  }
  return {nullptr, 0};
}

[[nodiscard]] std::size_t BufferIndex(int buffer) {
  if (buffer != 0 && buffer != 1) {
    VS_LOG_WARN(kChannel, "Title frame buffer {} is not 0 or 1", buffer);
  }
  return static_cast<std::size_t>(buffer & 1);
}

class LiveHost final : public Host {
 public:
  void Submit(std::span<const std::uint32_t> packet) override {
    std::array<u_long, kPacketWords> words{};
    if (packet.size() > words.size()) {
      VS_LOG_WARN(kChannel, "Title packet of {} words exceeds {} words",
                  packet.size(), words.size());
      return;
    }
    std::ranges::copy(packet, words.begin());
    DrawPrim(words.data());
  }
  void Refuse(std::string_view what) override { Warn(what); }

  void PlaySoundEffect(int effect) override {
    Audio::LivePlaySfxCentered(Menu::kMenuSoundChannelGroup, effect);
  }
  PlayerSettings Settings() override {
    return {
        .mono_sound = static_cast<std::uint8_t>(Input_settings.monoSound),
        .vibration_on = static_cast<std::uint8_t>(Input_settings.vibrationOn)};
  }
  void SetSettings(PlayerSettings settings) override {
    Input_settings.monoSound = static_cast<char>(settings.mono_sound);
    Input_settings.vibrationOn = static_cast<char>(settings.vibration_on);
  }
  void ResetPlayerSettings() override { Boot_InitializePlayerSettings(); }
  void SetMonoOutput(int mono) override { Audio::LiveSetMonoOutput(mono); }
  void SetDiscMixVolume(std::uint32_t volume) override {
    if (!Audio::Control::SetLiveDiscMixVolume(volume)) {
      VS_LOG_WARN(kChannel, "Title disc mix volume {} was refused", volume);
    }
  }

  void UpdateControllers() override { ::VS::Input::UpdateLiveControllers(); }
  Buttons Input() override {
    return {.held = static_cast<std::uint32_t>(Input_buttonsState),
            .pressed = Input_buttonsPressed.allButtons};
  }
  void InvalidateControllerConnections() override {
    ::VS::Input::InvalidateLiveControllerConnections();
  }
  void UpdateVibrationPatterns() override {
    ::VS::Input::UpdateLiveVibrationPatterns();
  }
  void PlayVibrationPreset(int preset) override {
    ::VS::Input::PlayLiveVibrationPreset(preset);
  }

  void WaitForVerticalBlank() override { VSync(0); }
  void WaitForVerticalBlanks(int count) override { VSync(count); }
  int HorizontalBlankCount() override { return VSync(1); }
  int VerticalBlankCount() override { return VSync(-1); }
  int AdvanceFrameClock(int vsync_mode) override {
    return ::VS::State::UpdateLivePlayTimeAndLoading(vsync_mode);
  }

  // An image larger than its module data uploads the whole rows the data
  // holds (the debug font's 158 rows overrun its 19712 bytes by 512).
  void UploadImage(Image image, const Render::ScreenRect& area) override {
    const ImageSource source = Source(image);
    const std::size_t row_bytes =
        area.width > 0 ? static_cast<std::size_t>(area.width) * kPixelBytes : 0;
    if (source.pixels == nullptr || row_bytes == 0 || area.height <= 0) {
      VS_LOG_WARN(kChannel, "Title image {} upload to {}x{} refused",
                  static_cast<int>(image), area.width, area.height);
      return;
    }
    std::size_t rows = static_cast<std::size_t>(area.height);
    if (rows * row_bytes > source.bytes) {
      rows = source.bytes / row_bytes;
      VS_LOG_WARN(kChannel,
                  "Title image {} holds {} bytes; uploading {} of {} rows",
                  static_cast<int>(image), source.bytes, rows, area.height);
    }
    Render::UploadLiveImageRectangle(Pack(area.x, area.y), source.pixels,
                                     Pack(area.width, static_cast<int>(rows)));
  }
  void LoadImage(const Render::ScreenRect& area,
                 std::span<const std::uint16_t> pixels) override {
    if (!CoversArea(area, pixels.size())) {
      return;
    }
    RECT rect = ToRect(area);
    // libgpu's LoadImage only reads the pixels.
    ::LoadImage(&rect, reinterpret_cast<u_long*>(
                           const_cast<std::uint16_t*>(pixels.data())));
  }
  void StoreImage(const Render::ScreenRect& area,
                  std::span<std::uint16_t> pixels) override {
    if (!CoversArea(area, pixels.size())) {
      return;
    }
    RECT rect = ToRect(area);
    ::StoreImage(&rect, reinterpret_cast<u_long*>(pixels.data()));
  }
  void ClearImage(const Render::ScreenRect& area) override {
    RECT rect = ToRect(area);
    ::ClearImage(&rect, 0, 0, 0);
  }
  void SetDisplayEnabled(bool enabled) override {
    SetDispMask(enabled ? 1 : 0);
  }
  void PutDisplay(const DisplaySetup& setup) override {
    DISPENV display{};
    DRAWENV draw{};
    SetDefDispEnv(&display, setup.display.x, setup.display.y,
                  setup.display.width, setup.display.height);
    display.screen = ToRect(setup.screen);
    display.isinter = setup.interlaced ? 1 : 0;
    SetDefDrawEnv(&draw, setup.draw.x, setup.draw.y, setup.draw.width,
                  setup.draw.height);
    PutDispEnv(&display);
    PutDrawEnv(&draw);
  }
  void SetGeometryOffset(int x, int y) override {
    Core_Gte_SetGeomOffset(x, y);
  }
  void SetProjectionDistance(int distance) override {
    Core_Gte_SetGeomScreen(distance);
  }
  void ApplyGameDisplay(const GameDisplayLayout& layout) override {
    for (std::size_t buffer = 0; buffer < Render::kFrameBufferCount; ++buffer) {
      DRAWENV& draw = Render_legacyDrawEnvironments[buffer];
      DISPENV& display = Render_legacyDisplayEnvironments[buffer];
      const Render::ScreenRect& draw_area = layout.draw_areas.at(buffer);
      const Render::ScreenRect& display_area = layout.display_areas.at(buffer);
      SetDefDrawEnv(&draw, draw_area.x, draw_area.y, draw_area.width,
                    draw_area.height);
      SetDefDispEnv(&display, display_area.x, display_area.y,
                    display_area.width, display_area.height);
      display.screen = ToRect(layout.screen);
      draw.dtd = 0;
      draw.isbg = 1;
      setRGB0(&draw, layout.background.red, layout.background.green,
              layout.background.blue);
    }
  }
  int FrameBuffer() override { return Render_frameBuf; }
  void SetFrameBuffer(int buffer) override { Render_frameBuf = buffer; }
  void ShowFrameBuffer(int buffer) override {
    const std::size_t index = BufferIndex(buffer);
    PutDispEnv(&Render_legacyDisplayEnvironments[index]);
    PutDrawEnv(&Render_legacyDrawEnvironments[index]);
  }
  // _renderScreen's caller passed its ordering table; the live title has
  // none (PresentGameFrame has no live caller).
  void DrawOrderingTable() override {
    Warn("title frame draw refused: there is no ordering table");
  }

  std::span<std::uint16_t> AllocateLow(std::size_t halfwords) override {
    void* block = Core::Memory::AllocateLiveFromLowEnd(halfwords * kPixelBytes);
    if (block == nullptr) {
      return {};
    }
    return {static_cast<std::uint16_t*>(block), halfwords};
  }
  void FreeLow(std::span<std::uint16_t> block) override {
    Core::Memory::FreeLiveFromLowEnd(block.data());
  }
  std::span<const std::uint32_t> BackgroundWords() override {
    return std::span<const std::uint32_t>(Title_titleScreenBackground);
  }
  std::span<const std::uint16_t> CopyrightPixels() override {
    return std::span<const std::uint16_t>(Title_copyrightImage);
  }

  void StartIntroMovie() override { Movie::Intro::StartLive(); }
  void PlayIntroMovie() override { Movie::Intro::PlayLive(); }
  void StopIntroMovie() override { Movie::Intro::StopLive(); }
  void StopIntroMovieAfterAudioTail() override {
    Movie::Intro::StopLiveAfterAudioTail();
  }
  bool IntroMoviePlaying() override { return Movie_Intro_isPlaying != 0; }
  void ClearIntroMoviePlaying() override { Movie_Intro_isPlaying = 0; }

  int VisitCount() override { return Title_titleScreenVisitCount; }
  void SetVisitCount(int count) override {
    Title_titleScreenVisitCount = count;
  }
  bool ClearSavePending() override { return Boot_isClearSavePending != 0; }
  void ClearPendingClearSave() override { Boot_isClearSavePending = 0; }
  void StageClearGame() override { Save_NativeStageClearGame(); }
  int InitialTitleSelection() override {
    return Save_NativeInitialTitleSelection();
  }

  void InitializeFileDisplay() override {
    Menu::Files::InitializeLiveDisplay();
  }
  void SwapFileBuffers() override { Menu::Files::SwapLiveBuffers(); }
  int StepCardResources(int init) override {
    return Save::MemoryCard::StepLiveTitleResources(init);
  }
  void ReleaseCardResources() override {
    Save::MemoryCard::ReleaseLiveTitleResources();
  }
  void AdvanceCardCipher() override {
    Save::MemoryCard::NextLiveTitleCipherBits(0);
  }
  void ResetFileElements() override { Menu::Files::ResetLiveTitleElements(); }
  int UpdateLoadMenu(int init) override {
    return Menu::UpdateLiveLoadMenu(init);
  }
  int UpdateSaveMenu(int init) override {
    return Menu::UpdateLiveSaveMenu(init);
  }
  void ShowSavePrompt() override {
    if (Title_saveMenuText == nullptr) {
      Warn("title save prompt refused: MCMAN.BIN is not loaded");
      return;
    }
    Title_memoryCardMessage = reinterpret_cast<char*>(
        Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_savePrompt);
  }
  int UpdateClearSaveReminder(int init) override {
    return Menu::Files::UpdateLiveClearSaveReminder(init);
  }
  void DrawFileBackground() override { Menu::Files::DrawLiveBackground(); }
  void DrawFileFrame() override {
    Menu::Files::DrawLiveTitleFrame(Menu_Files_frameBuf);
  }
  void DrawExitFade() override { Menu::Files::DrawLiveExitFade(); }

  void ResetSession() override { Save_NativeResetSession(); }
  void ClearStateFlags() override {
    std::memset(&Audio_Seq_unusedStateFlags, 0,
                sizeof Audio_Seq_unusedStateFlags);
  }
  void RestoreInitialLearnedActions() override {
    Core::Memory::CopyLiveAlignedWords(State_learnedActionFlags,
                                       Title_initialLearnedActionBits,
                                       sizeof Title_initialLearnedActionBits);
  }
  void ClearProgressRecords() override {
    Core::Memory::ZeroLiveAlignedWords(&State_roomVisitProgress,
                                       sizeof State_roomVisitProgress);
    Core::Memory::ZeroLiveAlignedWords(&State_playTime, sizeof State_playTime);
    Core::Memory::ZeroLiveAlignedWords(&Audio_Seq_unusedScoredata,
                                       sizeof Audio_Seq_unusedScoredata);
    State_currentRoomStateSlot = 0;
    Core::Memory::ZeroLiveAlignedWords(State_recentRoomEnemyStates,
                                       sizeof State_recentRoomEnemyStates);
    Core::Memory::ZeroLiveAlignedWords(&Audio_Seq_unusedArtsStatus,
                                       sizeof Audio_Seq_unusedArtsStatus);
  }
  void SetBossTimeTrialScores(const TimeTrialScore& score) override {
    for (auto& boss : Audio_Seq_unusedScoredata.bossTimeTrialScores) {
      for (State_TrialScore& rank : boss) {
        rank.time = static_cast<std::uint32_t>(score.time) & kTrialTimeMask;
        rank.round = score.round;
        rank.difficulty = score.difficulty;
      }
    }
  }
  void ClearAshleySnapshot(std::size_t bytes) override {
    if (bytes > sizeof State_ashleySnapshot) {
      VS_LOG_WARN(kChannel,
                  "Title snapshot clear of {} bytes exceeds the {}-byte "
                  "snapshot",
                  bytes, sizeof State_ashleySnapshot);
      return;
    }
    Core::Memory::ZeroLiveAlignedWords(&State_ashleySnapshot,
                                       static_cast<std::int32_t>(bytes));
  }
  void ClearSavedEntryDoor() override {
    Core::Memory::ZeroLiveAlignedWords(&State_savedEntryDoor,
                                       sizeof State_savedEntryDoor);
  }
  void RestoreNewGameInventory() override {
    Core::Memory::CopyLiveAlignedWords(&Audio_Seq_unusedInventory,
                                       Title_newGameInventoryTemplate,
                                       sizeof Audio_Seq_unusedInventory);
  }
  void SetTickSpeed(int speed) override { State_tickSpeed = speed; }
  void SetEntryDoor(const EntryDoor& door) override {
    State_savedEntryDoor.entryTileX = door.tile_x;
    State_savedEntryDoor.entryTileZ = door.tile_z;
    State_savedEntryDoor.tileLocation.entryDirection = door.direction;
    State_savedEntryDoor.entryElevation = door.elevation;
  }
  void ClearInventoryIndices() override {
    std::memset(&Audio_Seq_unusedInventoryIndices, 0,
                sizeof Audio_Seq_unusedInventoryIndices);
  }
  void SetGameModes(const GameModes& modes) override {
    Audio_Seq_unusedStateFlags.difficulty = static_cast<char>(modes.difficulty);
    Audio_Seq_unusedStateFlags.puzzleMode =
        static_cast<char>(modes.puzzle_mode);
  }
  void ApplyExit(const ExitPlan& plan) override {
    Audio_Seq_unusedStateFlags.opaqueC = static_cast<char>(plan.state_flag_0c);
    Audio_Seq_unusedStateFlags.opaqueD = static_cast<char>(plan.state_flag_0d);
    if (plan.state_flag_0e.has_value()) {
      Audio_Seq_unusedStateFlags.opaqueE =
          static_cast<char>(*plan.state_flag_0e);
    }
    if (plan.destination.has_value()) {
      State_savedEntryDoor.destinationZoneId = plan.destination->zone;
      State_savedEntryDoor.destinationRoomId = plan.destination->room;
    }
  }
};

State g_state{};
LiveHost g_host;
}  // namespace

int RunLive() {
  Context context{.state = g_state, .host = g_host};
  return Run(context);
}

void InitializeLiveNewGameData() { InitializeNewGameData(g_host); }

void PrepareLiveExit(int cause) { PrepareExit(g_host, cause); }

State& LiveTitleState() { return g_state; }

Host& LiveTitleHost() { return g_host; }
}  // namespace VS::Title
