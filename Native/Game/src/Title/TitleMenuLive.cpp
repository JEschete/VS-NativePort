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
#include "Boot/GameLoop.hpp"
#include "Boot/PlayerSettings.hpp"
#include "Content/StringTables.hpp"
#include "Core/Gte.h"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Field/Doors.h"
#include "Field/Mpd.h"
#include "Input/ControllerLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/InventoryLive.hpp"
#include "Legacy/Psx.h"
#include "Menu/FilesLive.hpp"
#include "Menu/FilesLiveState.hpp"
#include "Menu/Sounds.hpp"
#include "Movie/Intro/Playback.hpp"
#include "Movie/IntroPlayerLive.hpp"
#include "Render/Display.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Save/FileMenuLive.hpp"
#include "Save/MemoryCard/GameFilesLive.hpp"
#include "Save/MenuAssetsLive.hpp"
#include "Save/NativeMenu.hpp"
#include "Save/NativeResume.hpp"
#include "State/Ashley.hpp"
#include "State/GameClockLive.hpp"
#include "State/Location.hpp"
#include "State/PlayTime.hpp"
#include "State/Progress.hpp"
#include "State/RoomMemory.hpp"
#include "State/StateFlags.hpp"
#include "State/Timers.hpp"
#include "Title/Credits.hpp"
#include "Title/NewGame.hpp"
#include "Title/SaveFiles.hpp"
#include "Title/TitleMenu.hpp"  // NOLINT(build/include): typed API
#include "Title/TitleMenuLiveState.hpp"
#include "Title/TitleScreen.hpp"

namespace mcman_text = VS::Content::text::mcman_bin;

// decomp: vs_main_titleScreenCount (vs_main_titleScreenCount@SLUS_010.40)
int VS::Title::titleScreenVisitCount = 0;

namespace VS::Title {
namespace {
constexpr const char* kChannel = "title";
constexpr std::size_t kPixelBytes = 2;
constexpr int kHalfwordShift = 16;
constexpr std::uint32_t kHalfwordMask = 0xFFFF;
// The 16-word primitive buffer DrawPrim reads the packets from.
constexpr std::size_t kPacketWords = 16;
constexpr std::uint32_t kTrialTimeMask = 0xFFFFFF;

static_assert(sizeof(VS::Title::initialLearnedActionBits) ==
              sizeof(VS::State::learnedActionFlags));

void Warn(std::string_view what) { VS::Core::LogWarn(kChannel, "{}", what); }

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
  VS::Core::LogWarn(
      kChannel, "Title VRAM transfer refused: {}x{} at ({}, {}) has {} pixels",
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
      return {VS::Title::saveMenuUiAtlas.page_zero_pixels,
              sizeof VS::Title::saveMenuUiAtlas.page_zero_pixels};
    case Image::kSaveUiPageOne:
      return {VS::Title::saveMenuUiAtlas.page_one_pixels,
              sizeof VS::Title::saveMenuUiAtlas.page_one_pixels};
    case Image::kSaveFontPageZero:
      return {&VS::Title::saveMenuFontAtlas[0],
              sizeof VS::Title::saveMenuFontAtlas[0]};
    case Image::kSaveFontPageOne:
      return {&VS::Title::saveMenuFontAtlas[1],
              sizeof VS::Title::saveMenuFontAtlas[1]};
    case Image::kDebugFont:
      return {VS::Title::debugFontAtlas, sizeof VS::Title::debugFontAtlas};
    case Image::kSaveBackgroundPalette:
      return {VS::Title::saveMenuBackground.palette,
              sizeof VS::Title::saveMenuBackground.palette};
    case Image::kSaveBackgroundPixels:
      return {VS::Title::saveMenuBackground.indexed_pixels,
              sizeof VS::Title::saveMenuBackground.indexed_pixels};
    case Image::kPublisherPalette:
      return {VS::Title::publisherLogo, kPaletteBytes};
    case Image::kPublisherPixels:
      return {VS::Title::publisherLogo + kPaletteBytes,
              sizeof VS::Title::publisherLogo - kPaletteBytes};
    case Image::kDeveloperLogo:
      return {VS::Title::developerLogo, sizeof VS::Title::developerLogo};
  }
  return {nullptr, 0};
}

[[nodiscard]] std::size_t BufferIndex(int buffer) {
  if (buffer != 0 && buffer != 1) {
    VS::Core::LogWarn(kChannel, "Title frame buffer {} is not 0 or 1", buffer);
  }
  return static_cast<std::size_t>(buffer & 1);
}

class LiveHost final : public Host {
 public:
  void Submit(std::span<const std::uint32_t> packet) override {
    std::array<u_long, kPacketWords> words{};
    if (packet.size() > words.size()) {
      VS::Core::LogWarn(kChannel, "Title packet of {} words exceeds {} words",
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
        .mono_sound = static_cast<std::uint8_t>(VS::Input::settings.mono_sound),
        .vibration_on =
            static_cast<std::uint8_t>(VS::Input::settings.vibration_on)};
  }
  void SetSettings(PlayerSettings settings) override {
    VS::Input::settings.mono_sound = static_cast<char>(settings.mono_sound);
    VS::Input::settings.vibration_on = static_cast<char>(settings.vibration_on);
  }
  void ResetPlayerSettings() override { VS::Boot::InitializePlayerSettings(); }
  void SetMonoOutput(int mono) override { Audio::LiveSetMonoOutput(mono); }
  void SetDiscMixVolume(std::uint32_t volume) override {
    if (!Audio::Control::SetLiveDiscMixVolume(volume)) {
      VS::Core::LogWarn(kChannel, "Title disc mix volume {} was refused",
                        volume);
    }
  }

  void UpdateControllers() override { ::VS::Input::UpdateLiveControllers(); }
  Buttons Input() override {
    return {.held = static_cast<std::uint32_t>(VS::Input::buttonsState),
            .pressed = VS::Input::buttonsPressed};
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
      VS::Core::LogWarn(kChannel, "Title image {} upload to {}x{} refused",
                        static_cast<int>(image), area.width, area.height);
      return;
    }
    std::size_t rows = static_cast<std::size_t>(area.height);
    if (rows * row_bytes > source.bytes) {
      rows = source.bytes / row_bytes;
      VS::Core::LogWarn(
          kChannel, "Title image {} holds {} bytes; uploading {} of {} rows",
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
      DRAWENV& draw = VS::Render::legacyDrawEnvironments[buffer];
      DISPENV& display = VS::Render::legacyDisplayEnvironments[buffer];
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
  int FrameBuffer() override { return VS::Render::frameBuf; }
  void SetFrameBuffer(int buffer) override { VS::Render::frameBuf = buffer; }
  void ShowFrameBuffer(int buffer) override {
    const std::size_t index = BufferIndex(buffer);
    PutDispEnv(&VS::Render::legacyDisplayEnvironments[index]);
    PutDrawEnv(&VS::Render::legacyDrawEnvironments[index]);
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
    return std::span<const std::uint32_t>(VS::Title::titleScreenBackground);
  }
  std::span<const std::uint16_t> CopyrightPixels() override {
    return std::span<const std::uint16_t>(VS::Title::copyrightImage);
  }

  void StartIntroMovie() override { Movie::Intro::StartLive(); }
  void PlayIntroMovie() override { Movie::Intro::PlayLive(); }
  void StopIntroMovie() override { Movie::Intro::StopLive(); }
  void StopIntroMovieAfterAudioTail() override {
    Movie::Intro::StopLiveAfterAudioTail();
  }
  bool IntroMoviePlaying() override { return VS::Movie::Intro::isPlaying != 0; }
  void ClearIntroMoviePlaying() override { VS::Movie::Intro::isPlaying = 0; }

  int VisitCount() override { return titleScreenVisitCount; }
  void SetVisitCount(int count) override { titleScreenVisitCount = count; }
  bool ClearSavePending() override { return VS::Boot::isClearSavePending != 0; }
  void ClearPendingClearSave() override { VS::Boot::isClearSavePending = 0; }
  void StageClearGame() override { VS::Game::Save::StageClearGame(); }
  int InitialTitleSelection() override {
    return VS::Game::Save::InitialTitleSelection();
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
    if (VS::Title::saveMenuText == nullptr) {
      Warn("title save prompt refused: MCMAN.BIN is not loaded");
      return;
    }
    VS::Title::memoryCardMessage = reinterpret_cast<char*>(
        VS::Title::saveMenuText + mcman_text::offset::kSavePrompt);
  }
  int UpdateClearSaveReminder(int init) override {
    return Menu::Files::UpdateLiveClearSaveReminder(init);
  }
  void DrawFileBackground() override { Menu::Files::DrawLiveBackground(); }
  void DrawFileFrame() override {
    Menu::Files::DrawLiveTitleFrame(VS::Menu::Files::frameBuf);
  }
  void DrawExitFade() override { Menu::Files::DrawLiveExitFade(); }

  void ResetSession() override { VS::Game::Save::ResetSession(); }
  void ClearStateFlags() override {
    std::memset(&VS::State::stateFlags, 0, sizeof VS::State::stateFlags);
  }
  void RestoreInitialLearnedActions() override {
    Core::Memory::CopyLiveAlignedWords(
        VS::State::learnedActionFlags, VS::Title::initialLearnedActionBits,
        sizeof VS::Title::initialLearnedActionBits);
  }
  void ClearProgressRecords() override {
    Core::Memory::ZeroLiveAlignedWords(&VS::State::roomVisitProgress,
                                       sizeof VS::State::roomVisitProgress);
    Core::Memory::ZeroLiveAlignedWords(&VS::State::playTime,
                                       sizeof VS::State::playTime);
    Core::Memory::ZeroLiveAlignedWords(&VS::State::scoreData,
                                       sizeof VS::State::scoreData);
    VS::State::currentRoomStateSlot = 0;
    Core::Memory::ZeroLiveAlignedWords(VS::State::recentRoomEnemyStates,
                                       sizeof VS::State::recentRoomEnemyStates);
    Core::Memory::ZeroLiveAlignedWords(&VS::State::artsStatus,
                                       sizeof VS::State::artsStatus);
  }
  void SetBossTimeTrialScores(const TimeTrialScore& score) override {
    for (auto& boss : VS::State::scoreData.boss_time_trial_scores) {
      for (VS::State::TrialScore& rank : boss) {
        rank.time = static_cast<std::uint32_t>(score.time) & kTrialTimeMask;
        rank.round = score.round;
        rank.difficulty = score.difficulty;
      }
    }
  }
  void ClearAshleySnapshot(std::size_t bytes) override {
    if (bytes > sizeof VS::State::ashleySnapshot) {
      VS::Core::LogWarn(kChannel,
                        "Title snapshot clear of {} bytes exceeds the {}-byte "
                        "snapshot",
                        bytes, sizeof VS::State::ashleySnapshot);
      return;
    }
    Core::Memory::ZeroLiveAlignedWords(&VS::State::ashleySnapshot,
                                       static_cast<std::int32_t>(bytes));
  }
  void ClearSavedEntryDoor() override {
    Core::Memory::ZeroLiveAlignedWords(&VS::State::savedEntryDoor,
                                       sizeof VS::State::savedEntryDoor);
  }
  void RestoreNewGameInventory() override {
    Core::Memory::CopyLiveAlignedWords(&VS::Items::inventory,
                                       VS::Title::newGameInventoryTemplate,
                                       sizeof VS::Items::inventory);
  }
  void SetTickSpeed(int speed) override { VS::State::tickSpeed = speed; }
  void SetEntryDoor(const EntryDoor& door) override {
    VS::State::savedEntryDoor.entryTileX = door.tile_x;
    VS::State::savedEntryDoor.entryTileZ = door.tile_z;
    VS::State::savedEntryDoor.tileLocation.entryDirection = door.direction;
    VS::State::savedEntryDoor.entryElevation = door.elevation;
  }
  void ClearInventoryIndices() override { VS::Items::inventoryIndices = {}; }
  void SetGameModes(const GameModes& modes) override {
    VS::State::stateFlags.difficulty = static_cast<char>(modes.difficulty);
    VS::State::stateFlags.puzzle_mode = static_cast<char>(modes.puzzle_mode);
  }
  void ApplyExit(const ExitPlan& plan) override {
    VS::State::stateFlags.opaque_c = static_cast<char>(plan.state_flag_0c);
    VS::State::stateFlags.opaque_d = static_cast<char>(plan.state_flag_0d);
    if (plan.state_flag_0e.has_value()) {
      VS::State::stateFlags.opaque_e = static_cast<char>(*plan.state_flag_0e);
    }
    if (plan.destination.has_value()) {
      VS::State::savedEntryDoor.destinationZoneId = plan.destination->zone;
      VS::State::savedEntryDoor.destinationRoomId = plan.destination->room;
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
