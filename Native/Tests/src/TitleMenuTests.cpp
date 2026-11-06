#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Menu/FilesLive.hpp"
#include "Save/MemoryCard/GameFilesLive.hpp"
#include "Title/TitleMenu.hpp"
#include "Title/TitleMenuLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Field/Mpd.h"
#include "Input/ControllerLiveState.h"
#include "Render/Display.h"
#include "State/Location.h"
#include "State/StateFlags.h"
#include "Title/SaveFiles.h"
#include "Title/TitleMenuLiveState.h"
}

namespace {
namespace title = VS::Title;
using title::MenuItemMotion;
using VS::Render::ScreenRect;

constexpr std::uint32_t kCircle = 1U << 5;
constexpr std::uint32_t kCross = 1U << 6;
constexpr std::uint32_t kStart = 1U << 11;
constexpr std::uint32_t kUp = 1U << 12;
constexpr std::uint32_t kDown = 1U << 14;

// The menu capture: two 160x64 strips of the background, then the copyright
// area's background and the copyright itself, each 180x24.
constexpr std::size_t kCaptureHalfwords = 70080;
constexpr std::size_t kStripHalfwords = 20480;
constexpr std::size_t kCopyrightSource = 61440;
constexpr std::size_t kCopyrightHalfwords = 4320;
constexpr std::size_t kBackgroundImageHalfwords = 313344;

// Strip pixels that blend half and half into kStripBlended.
constexpr std::uint16_t kStripTarget = 0x001F;
constexpr std::uint16_t kStripSource = 0x7C00;
constexpr std::uint16_t kStripBlended = 0xBC0F;

constexpr std::uint32_t kClippedTextSize = (1U << 16U) | 112U;

constexpr int kChangeSound = 4;
constexpr int kSelectSound = 5;
constexpr int kLeaveSound = 6;

struct LoadRecord {
  ScreenRect area;
  std::size_t size = 0;
  std::vector<std::uint16_t> pixels;
};

struct StoreRecord {
  ScreenRect area;
  std::ptrdiff_t offset = 0;
  std::size_t size = 0;

  friend bool operator==(const StoreRecord&, const StoreRecord&) = default;
};

struct UploadRecord {
  title::Image image = title::Image::kSaveUiPageZero;
  ScreenRect area;

  friend bool operator==(const UploadRecord&, const UploadRecord&) = default;
};

int Next(std::deque<int>& results, int fallback) {
  if (results.empty()) {
    return fallback;
  }
  const int result = results.front();
  results.pop_front();
  return result;
}

title::Buttons Held(std::uint32_t bits) { return {.held = bits}; }

title::Buttons Pressed(std::uint32_t bits) {
  return {.held = bits, .pressed = bits};
}

// A scripted title host: it records what the title asked for and answers
// from its fields. Per-frame events are logged only while log_frames is set.
class FakeHost final : public title::Host {
 public:
  static constexpr int kPollLimit = 200000;
  static constexpr int kVerticalBlankLimit = 100000;
  static constexpr int kClockLimit = 100000;
  static constexpr std::uint16_t kCopyrightPixel = 0x4321;

  std::vector<std::string> events;
  bool log_frames = true;
  bool record_packets = true;
  bool record_pixels = true;

  std::vector<std::string> refusals;
  std::vector<int> sounds;
  title::PlayerSettings settings{};
  int mono = -1;
  std::uint32_t disc_volume = 0;
  std::vector<int> presets;

  std::deque<title::Buttons> script;
  title::Buttons idle{};
  title::Buttons buttons{};
  bool script_running = true;
  int polls = 0;
  int vibration_updates = 0;

  int vblanks = 0;
  int vblank_limit = kVerticalBlankLimit;
  int hblanks = 0;
  int clock_advances = 0;
  int vsync_result = 0;

  std::vector<std::vector<std::uint32_t>> packets;
  int packet_count = 0;
  int tiles = 0;
  std::vector<UploadRecord> uploads;
  std::vector<LoadRecord> loads;
  std::vector<StoreRecord> stores;
  std::vector<ScreenRect> clears;
  std::vector<title::DisplaySetup> displays;
  std::vector<title::GameDisplayLayout> layouts;
  int frame_buffer = 0;

  std::size_t heap_limit = std::numeric_limits<std::size_t>::max();
  std::deque<std::vector<std::uint16_t>> blocks;
  std::vector<std::size_t> freed;
  std::vector<std::uint32_t> background;
  std::vector<std::uint16_t> copyright =
      std::vector<std::uint16_t>(kCopyrightHalfwords, kCopyrightPixel);

  bool movie_plays = false;
  bool movie_playing = false;
  int audio_tails = 0;

  int visit_count = 1;
  bool clear_save_pending = false;
  int initial_selection = 0;

  std::deque<int> card_steps;
  std::deque<int> load_results;
  std::deque<int> save_results;
  std::deque<int> reminder_results;
  std::vector<int> load_menu_calls;
  std::vector<int> save_menu_calls;
  std::vector<int> reminder_calls;
  int file_swaps = 0;
  int cipher_steps = 0;
  int file_draws = 0;
  int file_frame_draws = 0;
  int exit_fades = 0;

  title::TimeTrialScore trial{};
  std::size_t ashley_bytes = 0;
  int tick_speed = 0;
  title::EntryDoor door{};
  title::GameModes modes{};
  std::vector<title::ExitPlan> exits;

  [[nodiscard]] std::ptrdiff_t Count(std::string_view name) const {
    return std::ranges::count(events, name);
  }

  // The first event named `name`, or -1.
  [[nodiscard]] std::ptrdiff_t Find(std::string_view name) const {
    const auto found = std::ranges::find(events, name);
    return found == events.end() ? -1 : found - events.begin();
  }

  // Whether `first` was logged, and before `second` was.
  [[nodiscard]] bool Precedes(std::string_view first,
                              std::string_view second) const {
    const std::ptrdiff_t first_index = Find(first);
    const std::ptrdiff_t second_index = Find(second);
    return first_index >= 0 && second_index > first_index;
  }

  void Submit(std::span<const std::uint32_t> packet) override {
    Frame("Submit");
    ++packet_count;
    if (packet.size() == title::kTransitionTileWords) {
      ++tiles;
    }
    if (record_packets) {
      packets.emplace_back(packet.begin(), packet.end());
    }
  }

  void Refuse(std::string_view what) override {
    Log("Refuse");
    refusals.emplace_back(what);
  }
  void PlaySoundEffect(int effect) override { sounds.push_back(effect); }
  title::PlayerSettings Settings() override {
    Log("Settings");
    return settings;
  }
  void SetSettings(title::PlayerSettings value) override {
    Log("SetSettings");
    settings = value;
  }
  void ResetPlayerSettings() override { Log("ResetPlayerSettings"); }
  void SetMonoOutput(int value) override {
    Log("SetMonoOutput");
    mono = value;
  }
  void SetDiscMixVolume(std::uint32_t volume) override {
    Log("SetDiscMixVolume");
    disc_volume = volume;
  }

  void UpdateControllers() override {
    Frame("UpdateControllers");
    if (++polls > kPollLimit) {
      throw std::runtime_error("the title never stopped polling");
    }
    if (!script_running) {
      buttons = {};
    } else if (script.empty()) {
      buttons = idle;
    } else {
      buttons = script.front();
      script.pop_front();
    }
  }
  title::Buttons Input() override { return buttons; }
  void InvalidateControllerConnections() override {
    Log("InvalidateControllerConnections");
    script_running = true;
  }
  void UpdateVibrationPatterns() override {
    Frame("UpdateVibrationPatterns");
    ++vibration_updates;
  }
  void PlayVibrationPreset(int preset) override { presets.push_back(preset); }

  void WaitForVerticalBlank() override {
    Frame("VSync");
    AddVerticalBlanks(1);
  }
  void WaitForVerticalBlanks(int count) override {
    Frame("VSync " + std::to_string(count));
    AddVerticalBlanks(count);
  }
  int HorizontalBlankCount() override { return hblanks++; }
  int VerticalBlankCount() override { return vblanks; }
  int AdvanceFrameClock(int vsync_mode) override {
    Log("AdvanceFrameClock " + std::to_string(vsync_mode));
    if (++clock_advances > kClockLimit) {
      throw std::runtime_error("the title never stopped its frame clock");
    }
    return vsync_result;
  }

  void UploadImage(title::Image image, const ScreenRect& area) override {
    Log("UploadImage");
    uploads.push_back({.image = image, .area = area});
  }
  void LoadImage(const ScreenRect& area,
                 std::span<const std::uint16_t> pixels) override {
    Frame("LoadImage");
    LoadRecord record{.area = area, .size = pixels.size(), .pixels = {}};
    if (record_pixels) {
      record.pixels.assign(pixels.begin(), pixels.end());
    }
    loads.push_back(std::move(record));
  }
  void StoreImage(const ScreenRect& area,
                  std::span<std::uint16_t> pixels) override {
    Log("StoreImage");
    std::ptrdiff_t offset = -1;
    if (!blocks.empty()) {
      offset = pixels.data() - blocks.back().data();
    }
    stores.push_back({.area = area, .offset = offset, .size = pixels.size()});
  }
  void ClearImage(const ScreenRect& area) override {
    Log("ClearImage");
    clears.push_back(area);
  }
  void SetDisplayEnabled(bool enabled) override {
    Log(enabled ? "DisplayOn" : "DisplayOff");
    if (!enabled) {
      script_running = false;
    }
  }
  void PutDisplay(const title::DisplaySetup& setup) override {
    Frame("PutDisplay");
    displays.push_back(setup);
  }
  void SetGeometryOffset(int x, int y) override {
    Log("GeometryOffset " + std::to_string(x) + "," + std::to_string(y));
  }
  void SetProjectionDistance(int distance) override {
    Log("Projection " + std::to_string(distance));
  }
  void ApplyGameDisplay(const title::GameDisplayLayout& layout) override {
    Log("ApplyGameDisplay");
    layouts.push_back(layout);
  }
  int FrameBuffer() override { return frame_buffer; }
  void SetFrameBuffer(int buffer) override {
    Log("SetFrameBuffer " + std::to_string(buffer));
    frame_buffer = buffer;
  }
  void ShowFrameBuffer(int buffer) override {
    Log("ShowFrameBuffer " + std::to_string(buffer));
  }
  void DrawOrderingTable() override { Log("DrawOrderingTable"); }

  std::span<std::uint16_t> AllocateLow(std::size_t halfwords) override {
    const std::size_t size = std::min(halfwords, heap_limit);
    if (size == 0) {
      return {};
    }
    return blocks.emplace_back(size);
  }
  void FreeLow(std::span<std::uint16_t> block) override {
    Log("FreeLow");
    freed.push_back(block.size());
  }
  std::span<const std::uint32_t> BackgroundWords() override {
    return background;
  }
  std::span<const std::uint16_t> CopyrightPixels() override {
    return copyright;
  }

  void StartIntroMovie() override {
    Log("StartIntroMovie");
    movie_playing = movie_plays;
  }
  void PlayIntroMovie() override { Log("PlayIntroMovie"); }
  void StopIntroMovie() override {
    Log("StopIntroMovie");
    movie_playing = false;
  }
  void StopIntroMovieAfterAudioTail() override {
    Frame("AudioTail");
    ++audio_tails;
  }
  bool IntroMoviePlaying() override { return movie_playing; }
  void ClearIntroMoviePlaying() override {
    Log("ClearIntroMoviePlaying");
    movie_playing = false;
  }

  int VisitCount() override { return visit_count; }
  void SetVisitCount(int count) override {
    Log("SetVisitCount " + std::to_string(count));
    visit_count = count;
  }
  bool ClearSavePending() override { return clear_save_pending; }
  void ClearPendingClearSave() override {
    Log("ClearPendingClearSave");
    clear_save_pending = false;
  }
  void StageClearGame() override { Log("StageClearGame"); }
  int InitialTitleSelection() override { return initial_selection; }

  void InitializeFileDisplay() override { Log("InitializeFileDisplay"); }
  void SwapFileBuffers() override { ++file_swaps; }
  int StepCardResources(int init) override {
    Log("StepCardResources " + std::to_string(init));
    return init != 0 ? 0 : Next(card_steps, 1);
  }
  void ReleaseCardResources() override { Log("ReleaseCardResources"); }
  void AdvanceCardCipher() override { ++cipher_steps; }
  void ResetFileElements() override { Log("ResetFileElements"); }
  int UpdateLoadMenu(int init) override {
    load_menu_calls.push_back(init);
    return init != 0 ? 0 : Next(load_results, 1);
  }
  int UpdateSaveMenu(int init) override {
    save_menu_calls.push_back(init);
    return init != 0 ? 0 : Next(save_results, 1);
  }
  void ShowSavePrompt() override { Log("ShowSavePrompt"); }
  int UpdateClearSaveReminder(int init) override {
    reminder_calls.push_back(init);
    return init != 0 ? 0 : Next(reminder_results, 2);
  }
  void DrawFileBackground() override { ++file_draws; }
  void DrawFileFrame() override { ++file_frame_draws; }
  void DrawExitFade() override { ++exit_fades; }

  void ResetSession() override { Log("ResetSession"); }
  void ClearStateFlags() override { Log("ClearStateFlags"); }
  void RestoreInitialLearnedActions() override {
    Log("RestoreInitialLearnedActions");
  }
  void ClearProgressRecords() override { Log("ClearProgressRecords"); }
  void SetBossTimeTrialScores(const title::TimeTrialScore& score) override {
    Log("SetBossTimeTrialScores");
    trial = score;
  }
  void ClearAshleySnapshot(std::size_t bytes) override {
    Log("ClearAshleySnapshot");
    ashley_bytes = bytes;
  }
  void ClearSavedEntryDoor() override { Log("ClearSavedEntryDoor"); }
  void RestoreNewGameInventory() override { Log("RestoreNewGameInventory"); }
  void SetTickSpeed(int speed) override {
    Log("SetTickSpeed");
    tick_speed = speed;
  }
  void SetEntryDoor(const title::EntryDoor& value) override {
    Log("SetEntryDoor");
    door = value;
  }
  void ClearInventoryIndices() override { Log("ClearInventoryIndices"); }
  void SetGameModes(const title::GameModes& value) override {
    Log("SetGameModes");
    modes = value;
  }
  void ApplyExit(const title::ExitPlan& plan) override {
    Log("ApplyExit");
    exits.push_back(plan);
  }

 private:
  void Log(std::string event) { events.push_back(std::move(event)); }
  void Frame(std::string event) {
    if (log_frames) {
      Log(std::move(event));
    }
  }
  void AddVerticalBlanks(int count) {
    vblanks += count;
    if (vblanks > vblank_limit) {
      throw std::runtime_error("the title never stopped waiting");
    }
  }
};

struct Fixture {
  title::State state;
  FakeHost host;
  title::Context context{.state = state, .host = host};
};

// Item `selected` in the middle of the main menu with its neighbours.
void PlaceMainItems(title::State& state, int selected) {
  constexpr std::uint8_t kMiddle = 64;
  constexpr std::uint8_t kBelow = 96;
  constexpr std::uint8_t kAbove = 32;
  title::InitializeMenuItem(state, selected, kMiddle);
  title::InitializeMenuItem(state, (selected + 1) & 3, kBelow);
  title::InitializeMenuItem(state, (selected + 3) & 3, kAbove);
}

// A whole title run: the pad is idle until the menu waits for a selection
// and per-frame records are off.
void PrepareRun(FakeHost& host) {
  host.script_running = false;
  host.log_frames = false;
  host.record_packets = false;
  host.record_pixels = false;
}

void Repeat(std::deque<title::Buttons>& script, int count,
            title::Buttons buttons = {}) {
  for (int frame = 0; frame < count; ++frame) {
    script.push_back(buttons);
  }
}

void Animate(Fixture& fixture, int frames) {
  for (int frame = 0; frame < frames; ++frame) {
    title::UpdateMenuAnimation(fixture.context);
  }
}

// The background as one zero halfword and two literal halfwords.
void LoadBackground(FakeHost& host) {
  host.background = {0x00010002, 0x12345678};
  title::LoadBackgroundTextures(host);
}

// A menu capture with its first strip in kStripTarget and its second in
// kStripSource.
std::vector<std::uint16_t> StripCapture() {
  std::vector<std::uint16_t> capture(kCaptureHalfwords);
  const std::span<std::uint16_t> whole(capture);
  std::ranges::fill(whole.first(kStripHalfwords), kStripTarget);
  std::ranges::fill(whole.subspan(kStripHalfwords, kStripHalfwords),
                    kStripSource);
  return capture;
}

// Items moving up, wrapping, moving down, in a submenu and fading.
void PlaceScrollingItems(title::State& state) {
  auto& items = state.items;
  items[1] = {.is_enabled = 1,
              .motion = MenuItemMotion::kMovingUp,
              .text_position = 96,
              .target_position = 64};
  items[2] = {.is_enabled = 1,
              .motion = MenuItemMotion::kMovingUp,
              .text_position = 2,
              .target_position = 250};
  items[3] = {.is_enabled = 1,
              .motion = MenuItemMotion::kMovingDown,
              .text_position = 32,
              .target_position = 40};
  items[4] = {.is_enabled = 1,
              .motion = MenuItemMotion::kSubmenu,
              .text_position = 64,
              .target_position = 64};
  items[5] = {.is_enabled = 1,
              .text_position = 96,
              .target_position = 96,
              .text_blend_factor = 3};
}

// Item 0 with its text shown and outline half shown, and item 2 above it.
void DrawOutlinedItems(Fixture& fixture) {
  title::State& state = fixture.state;
  title::InitializeMenuItem(state, 0, 64);
  state.items[0].text_saturation = 128;
  state.items[0].outline_saturation = 64;
  title::InitializeMenuItem(state, 2, 0);
  state.items[2].outline_saturation = 16;
  title::DrawMenuItems(fixture.context);
}

// The text line of a fully shown item at `position`.
std::optional<title::MenuSpritePacket> TextLineAt(std::uint8_t position) {
  const title::MenuItem item{.is_enabled = 1,
                             .text_saturation = 128,
                             .text_position = position,
                             .target_position = position};
  return title::TextLinePacket(item, 0, 0);
}

// One menu frame with item 0 selected in the middle of the strip.
void DrawFirstItem(Fixture& fixture) {
  title::InitializeMenuItem(fixture.state, 0, 64);
  title::DrawMenu(fixture.context);
}

// The vibration submenu over main item 2: Up once the items settle, then
// Start.
void ToggleVibration(Fixture& fixture) {
  FakeHost& host = fixture.host;
  host.log_frames = false;
  host.record_packets = false;
  PlaceMainItems(fixture.state, 2);
  Repeat(host.script, 24);
  host.script.push_back(Pressed(kUp));
  Repeat(host.script, 6);
  host.script.push_back(Held(kStart));
  host.idle = Held(kCross);
  title::ShowVibrationSettings(fixture.context);
}

// The sound submenu over main item 3: Down once the items settle, then
// Circle.
void SwitchToMono(Fixture& fixture) {
  FakeHost& host = fixture.host;
  host.log_frames = false;
  host.record_packets = false;
  PlaceMainItems(fixture.state, 3);
  Repeat(host.script, 24);
  host.script.push_back(Pressed(kDown));
  Repeat(host.script, 6);
  host.script.push_back(Held(kCircle));
  title::ShowSoundSettings(fixture.context);
}

// A run from an initial selection outside the main menu, with Start held.
int RunNewGame(Fixture& fixture) {
  FakeHost& host = fixture.host;
  PrepareRun(host);
  host.movie_plays = true;
  host.initial_selection = 9;
  host.script = {Held(kStart)};
  return title::Run(fixture.context);
}

// An idle run with a cleared game waiting to be saved.
void PrepareIdleRun(FakeHost& host) {
  PrepareRun(host);
  host.clear_save_pending = true;
  host.visit_count = 5;
}

// The clear-save screen: a refused save, then a save and the reminder.
void SaveThenRemind(FakeHost& host) {
  host.log_frames = false;
  host.save_results = {-1, 1};
  host.reminder_results = {0, 1};
  title::ShowClearSaveScreen(host);
}

int ContinueFromCard(FakeHost& host) {
  host.log_frames = false;
  host.card_steps = {0, 1};
  host.load_results = {0, 3};
  return title::ShowContinueScreen(host);
}

// Presets the live exit's state flags and entry door, and restores them when
// the test ends.
class PresetLiveExit {
 public:
  static constexpr char kFlag = 7;
  static constexpr u_char kDestination = 9;

  PresetLiveExit() {
    Audio_Seq_unusedStateFlags.opaqueC = kFlag;
    Audio_Seq_unusedStateFlags.opaqueD = kFlag;
    Audio_Seq_unusedStateFlags.opaqueE = kFlag;
    State_savedEntryDoor.destinationZoneId = kDestination;
    State_savedEntryDoor.destinationRoomId = kDestination;
  }
  PresetLiveExit(const PresetLiveExit&) = delete;
  PresetLiveExit& operator=(const PresetLiveExit&) = delete;
  PresetLiveExit(PresetLiveExit&&) = delete;
  PresetLiveExit& operator=(PresetLiveExit&&) = delete;
  ~PresetLiveExit() {
    Audio_Seq_unusedStateFlags = flags_;
    State_savedEntryDoor = door_;
  }

 private:
  State_StateFlags flags_ = Audio_Seq_unusedStateFlags;
  Field_MpdDoorRecord door_ = State_savedEntryDoor;
};

// Restores the settings, visits and display environments the live host
// writes when the test ends.
class LiveHostGuard {
 public:
  LiveHostGuard() {
    std::ranges::copy(Render_legacyDrawEnvironments, draw_.begin());
    std::ranges::copy(Render_legacyDisplayEnvironments, display_.begin());
  }
  LiveHostGuard(const LiveHostGuard&) = delete;
  LiveHostGuard& operator=(const LiveHostGuard&) = delete;
  LiveHostGuard(LiveHostGuard&&) = delete;
  LiveHostGuard& operator=(LiveHostGuard&&) = delete;
  ~LiveHostGuard() {
    Input_settings = settings_;
    Title_titleScreenVisitCount = visits_;
    std::ranges::copy(draw_, std::begin(Render_legacyDrawEnvironments));
    std::ranges::copy(display_, std::begin(Render_legacyDisplayEnvironments));
  }

 private:
  State_Settings settings_ = Input_settings;
  int visits_ = Title_titleScreenVisitCount;
  std::array<DRAWENV, 2> draw_{};
  std::array<DISPENV, 2> display_{};
};

void ApplyLiveGameDisplay() {
  title::LiveTitleHost().ApplyGameDisplay(title::PlanGameDisplay(
      {.width = 640,
       .height = 240,
       .projection_distance = 512,
       .background = {.red = 1, .green = 2, .blue = 3}}));
}

// Dirties the live TITLE file globals, and restores them when the test ends.
class DirtyLiveTitleFiles {
 public:
  DirtyLiveTitleFiles() {
    std::ranges::copy(Title_fileMenuElements, elements_.begin());
    Title_saveMenuText = nullptr;
    Title_fileMenuElements[3].state = 2;
    Title_fileMenuScreenFade = 9;
    Title_selectionCursorColor = 5;
    Title_selectionCursorPosition = 77;
    Title_transferProgressCounter = 4;
    Title_transferProgressTarget = 0;
    Title_memoryCardMessage = message_.data();
  }
  DirtyLiveTitleFiles(const DirtyLiveTitleFiles&) = delete;
  DirtyLiveTitleFiles& operator=(const DirtyLiveTitleFiles&) = delete;
  DirtyLiveTitleFiles(DirtyLiveTitleFiles&&) = delete;
  DirtyLiveTitleFiles& operator=(DirtyLiveTitleFiles&&) = delete;
  ~DirtyLiveTitleFiles() {
    std::ranges::copy(elements_, std::begin(Title_fileMenuElements));
    Title_fileMenuScreenFade = fade_;
    Title_selectionCursorColor = color_;
    Title_selectionCursorPosition = position_;
    Title_transferProgressCounter = counter_;
    Title_transferProgressTarget = target_;
    Title_memoryCardMessage = saved_message_;
    Title_saveMenuText = text_;
  }

 private:
  std::array<Menu_Files_FileMenuElement, 10> elements_{};
  char fade_ = Title_fileMenuScreenFade;
  u_char color_ = Title_selectionCursorColor;
  int position_ = Title_selectionCursorPosition;
  int counter_ = Title_transferProgressCounter;
  u_short target_ = Title_transferProgressTarget;
  char* saved_message_ = Title_memoryCardMessage;
  u_short* text_ = Title_saveMenuText;
  std::array<char, 4> message_{'m', 's', 'g', '\0'};
};
}  // namespace
TEST_CASE("Title menu sounds play their effects", "[title][title-menu]") {
  FakeHost host;
  title::PlayNewGameSounds(host);
  title::PlayMenuChangeSound(host);
  title::PlayMenuSelectSound(host);
  title::PlayMenuLeaveSound(host);
  CHECK(host.sounds ==
        std::vector<int>{1, 2, 3, kChangeSound, kSelectSound, kLeaveSound});
  CHECK(title::NewGameChoice() == 0);
}

TEST_CASE("Title exits plan their flags and destinations",
          "[title][title-menu]") {
  CHECK(title::PlanExit(0) ==
        title::ExitPlan{.state_flag_0c = 0,
                        .state_flag_0d = 0,
                        .state_flag_0e = std::nullopt,
                        .destination = title::Destination{.zone = 1}});
  CHECK(title::PlanExit(3) ==
        title::ExitPlan{.state_flag_0c = 0,
                        .state_flag_0d = 1,
                        .state_flag_0e = 1,
                        .destination = title::Destination{.zone = 20}});
  CHECK(title::PlanExit(4) == title::ExitPlan{.state_flag_0c = 0,
                                              .state_flag_0d = 1,
                                              .state_flag_0e = 1,
                                              .destination = title::Destination{
                                                  .zone = 2, .room = 1}});
  CHECK(title::PlanExit(1) == title::ExitPlan{});
  CHECK(title::PlanExit(-1) == title::ExitPlan{});

  FakeHost host;
  title::PrepareExit(host, 3);
  title::PrepareExit(host, 1);
  CHECK(host.exits ==
        std::vector<title::ExitPlan>{title::PlanExit(3), title::ExitPlan{}});
}

TEST_CASE("Title game display plans two buffers", "[title][title-menu]") {
  const title::GameDisplayLayout layout =
      title::PlanGameDisplay({.width = 640,
                              .height = 240,
                              .projection_distance = 512,
                              .background = {.red = 1, .green = 2, .blue = 3}});
  const ScreenRect first{.x = 0, .y = 0, .width = 640, .height = 224};
  const ScreenRect second{.x = 640, .y = 0, .width = 640, .height = 224};
  CHECK(layout.draw_areas[0] == first);
  CHECK(layout.draw_areas[1] == second);
  CHECK(layout.display_areas[0] == second);
  CHECK(layout.display_areas[1] == first);
  CHECK(layout.screen ==
        ScreenRect{.x = 0, .y = 8, .width = 256, .height = 224});
  CHECK(layout.background ==
        VS::Render::ClearColor{.red = 1, .green = 2, .blue = 3});

  FakeHost host;
  host.frame_buffer = 1;
  title::InitializeGameDisplay(
      host, {.width = 320, .height = 240, .projection_distance = 256});
  CHECK(host.events ==
        std::vector<std::string>{"DisplayOff", "GeometryOffset 160,120",
                                 "Projection 256", "ApplyGameDisplay",
                                 "ClearImage", "ShowFrameBuffer 1"});
  CHECK(host.clears == std::vector<ScreenRect>{
                           {.x = 0, .y = 0, .width = 1024, .height = 512}});
}

TEST_CASE("Title game frames swap buffers", "[title][title-menu]") {
  constexpr int kVsync = 7;
  FakeHost host;
  host.vsync_result = kVsync;
  CHECK(title::PresentGameFrame(host) == kVsync);
  CHECK(host.frame_buffer == 1);
  CHECK(title::PresentGameFrame(host) == kVsync);
  CHECK(host.frame_buffer == 0);
  CHECK(host.events ==
        std::vector<std::string>{"SetFrameBuffer 1", "AdvanceFrameClock 0",
                                 "ShowFrameBuffer 1", "DrawOrderingTable",
                                 "SetFrameBuffer 0", "AdvanceFrameClock 0",
                                 "ShowFrameBuffer 0", "DrawOrderingTable"});
}

TEST_CASE("Title new game data resets the game state", "[title][title-menu]") {
  FakeHost host;
  title::InitializeNewGameData(host);
  CHECK(host.events ==
        std::vector<std::string>{
            "ResetSession", "DisplayOff", "GeometryOffset 320,120",
            "Projection 512", "ApplyGameDisplay", "ClearImage",
            "ShowFrameBuffer 0", "ClearStateFlags",
            "RestoreInitialLearnedActions", "ClearProgressRecords",
            "SetBossTimeTrialScores", "ClearAshleySnapshot",
            "ClearSavedEntryDoor", "RestoreNewGameInventory", "SetTickSpeed",
            "SetEntryDoor"});
  CHECK(host.trial == title::TimeTrialScore{.time = -8388608});
  CHECK(host.ashley_bytes == 0x50);
  CHECK(host.tick_speed == 2);
  CHECK(host.door ==
        title::EntryDoor{
            .tile_x = 4, .tile_z = 2, .direction = 0, .elevation = 2});
}

TEST_CASE("Title settings start from the logos on the first visit",
          "[title][title-menu]") {
  Fixture fixture;
  FakeHost& host = fixture.host;
  host.visit_count = 0;
  host.settings = {.mono_sound = 1, .vibration_on = 0};
  host.record_packets = false;
  title::InitializeSettings(fixture.context);
  CHECK(host.settings ==
        title::PlayerSettings{.mono_sound = 0, .vibration_on = 1});
  CHECK(host.mono == 0);
  CHECK(host.disc_volume == 0x7F);
  CHECK(host.modes == title::GameModes{.difficulty = 1, .puzzle_mode = 1});
  CHECK(host.packet_count == 728);
  CHECK(host.Count("ClearImage") == 1);
  REQUIRE(host.events.size() >= 5);
  CHECK(std::vector<std::string>(host.events.end() - 5, host.events.end()) ==
        std::vector<std::string>{"SetSettings", "SetMonoOutput",
                                 "SetDiscMixVolume", "ClearInventoryIndices",
                                 "SetGameModes"});
}

TEST_CASE("Title settings keep the player's choices on later visits",
          "[title][title-menu]") {
  Fixture fixture;
  FakeHost& host = fixture.host;
  host.visit_count = 3;
  host.settings = {.mono_sound = 5, .vibration_on = 0};
  title::InitializeSettings(fixture.context);
  CHECK(host.settings ==
        title::PlayerSettings{.mono_sound = 1, .vibration_on = 0});
  CHECK(host.mono == 1);
  CHECK(host.events == std::vector<std::string>{
                           "Settings", "ResetPlayerSettings", "SetSettings",
                           "SetMonoOutput", "SetDiscMixVolume",
                           "ClearInventoryIndices", "SetGameModes"});
}

TEST_CASE("Title logos clear and upload their images", "[title][title-menu]") {
  FakeHost host;
  title::ShowPublisherAndDeveloper(host);
  CHECK(host.clears ==
        std::vector<ScreenRect>{{.x = 0, .y = 0, .width = 320, .height = 512}});
  CHECK(host.uploads ==
        std::vector<UploadRecord>{
            {.image = title::Image::kPublisherPalette,
             .area = {.x = 320, .y = 64, .width = 16, .height = 1}},
            {.image = title::Image::kPublisherPixels,
             .area = {.x = 320, .y = 0, .width = 64, .height = 48}},
            {.image = title::Image::kDeveloperLogo,
             .area = {.x = 0, .y = 240, .width = 32, .height = 14}}});
  CHECK(host.polls == 364);
  CHECK(host.events.back() == "DisplayOff");
}

TEST_CASE("Title logos fade the publisher and developer in and out",
          "[title][title-menu]") {
  constexpr std::uint32_t kColour = 0xFFFFFF;
  constexpr std::size_t kDeveloperStart = 364;
  FakeHost host;
  title::ShowPublisherAndDeveloper(host);
  REQUIRE(host.packets.size() == 728);
  CHECK((host.packets[0][2] & kColour) == 0x040404);
  CHECK((host.packets[31][2] & kColour) == 0x808080);
  CHECK((host.packets[363][2] & kColour) == 0);
  CHECK((host.packets[kDeveloperStart][2] & kColour) == 0x040404);
  CHECK(host.packets[0][3] == ((88U << 16U) | 32U));
  CHECK(host.packets[kDeveloperStart][3] == ((104U << 16U) | 96U));
}

TEST_CASE("Title logos alternate the display buffers", "[title][title-menu]") {
  FakeHost host;
  title::ShowPublisherAndDeveloper(host);
  CHECK(host.vblanks == 729);
  REQUIRE(host.displays.size() == 729);
  CHECK(host.displays[0] ==
        title::DisplaySetup{
            .display = {.x = 0, .y = 256, .width = 320, .height = 240},
            .screen = {.x = 0, .y = 8, .width = 0, .height = 224},
            .interlaced = false,
            .draw = {.x = 0, .y = 0, .width = 320, .height = 240}});
  CHECK(host.displays[1].display.y == 0);
  CHECK(host.displays[1].draw.y == 256);
  CHECK(host.displays[2].display.y == 256);
}

TEST_CASE("Title developer logo skips to its fade on a button",
          "[title][title-menu]") {
  FakeHost host;
  host.idle = Held(kCross);
  host.record_packets = false;
  title::ShowPublisherAndDeveloper(host);
  CHECK(host.packet_count == 429);
  CHECK(host.polls == 65);
  CHECK(host.displays.size() == 430);
}

TEST_CASE("Title menu captures its strips and copyright",
          "[title][title-menu]") {
  Fixture fixture;
  title::State& state = fixture.state;
  FakeHost& host = fixture.host;
  title::InitializeMenuItem(state, 5, 0);
  const std::span<std::uint16_t> capture =
      title::InitializeMenu(fixture.context, 3);
  CHECK(capture.size() == kCaptureHalfwords);
  CHECK(state.items[5].is_enabled == 0);
  CHECK(state.items[3] == title::MenuItem{.is_enabled = 1,
                                          .motion = MenuItemMotion::kStationary,
                                          .text_saturation = 0,
                                          .outline_saturation = 0,
                                          .text_position = 64,
                                          .target_position = 64,
                                          .outline_position = 64,
                                          .text_blend_factor = 0});
  CHECK(state.items[0].text_position == 96);
  CHECK(state.items[2].text_position == 32);
  CHECK(state.items[1].is_enabled == 0);
  CHECK(host.stores ==
        std::vector<StoreRecord>{
            {.area = {.x = 192, .y = 448, .width = 160, .height = 64},
             .offset = kStripHalfwords,
             .size = 10240},
            {.area = {.x = 352, .y = 448, .width = 160, .height = 64},
             .offset = 30720,
             .size = 10240},
            {.area = {.x = 678, .y = 376, .width = 180, .height = 24},
             .offset = kCopyrightSource,
             .size = kCopyrightHalfwords}});
  title::ReleaseMenuCapture(host, capture);
  title::ReleaseMenuCapture(host, {});
  CHECK(host.freed == std::vector<std::size_t>{kCaptureHalfwords});
}

TEST_CASE("Title menu refuses a heap without room for its capture",
          "[title][title-menu]") {
  constexpr std::size_t kShortHeap = 100;
  Fixture fixture;
  FakeHost& host = fixture.host;
  host.heap_limit = 0;
  CHECK(title::InitializeMenu(fixture.context, 3).empty());
  CHECK(fixture.state.items[0].is_enabled == 1);
  host.heap_limit = kShortHeap;
  CHECK(title::InitializeMenu(fixture.context, 3).empty());
  CHECK(host.refusals ==
        std::vector<std::string>{"no heap for the title menu capture",
                                 "no heap for the title menu capture"});
  CHECK(host.freed == std::vector<std::size_t>{kShortHeap});
  CHECK(host.stores.empty());
}

TEST_CASE("Title background decoding expands zero and literal runs",
          "[title][title-menu]") {
  constexpr std::uint16_t kUntouched = 0x7777;
  constexpr std::size_t kOutputSize = 12;
  const std::vector<std::uint32_t> encoded{0x00020001, 0xBBBBAAAA, 0xDDDDCCCC,
                                           0x00000002};
  std::vector<std::uint16_t> out(kOutputSize, kUntouched);
  CHECK(title::DecodeBackground(encoded, out) ==
        std::optional<std::size_t>{10});
  CHECK(out == std::vector<std::uint16_t>{0, 0, 0xAAAA, 0xBBBB, 0xCCCC, 0xDDDD,
                                          0, 0, 0, 0, kUntouched, kUntouched});

  std::vector<std::uint16_t> small(8);
  CHECK_FALSE(title::DecodeBackground(encoded, small).has_value());
  const std::vector<std::uint32_t> truncated{0x00030000, 1};
  CHECK_FALSE(title::DecodeBackground(truncated, out).has_value());
  CHECK(title::DecodeBackground({}, out) == std::optional<std::size_t>{0});
}

TEST_CASE("Title background textures decode into one heap block",
          "[title][title-menu]") {
  FakeHost host;
  LoadBackground(host);
  REQUIRE(host.blocks.size() == 1);
  CHECK(host.blocks[0].size() == kBackgroundImageHalfwords);
  CHECK(host.blocks[0][4] == 0x5678);
  CHECK(host.freed == std::vector<std::size_t>{kBackgroundImageHalfwords});
  CHECK(host.refusals.empty());
}

TEST_CASE("Title background textures load the image", "[title][title-menu]") {
  FakeHost host;
  LoadBackground(host);
  REQUIRE(host.loads.size() == 11);
  CHECK(host.loads[0].area ==
        ScreenRect{.x = 480, .y = 0, .width = 544, .height = 512});
  CHECK(host.loads[0].size == 278528);
  CHECK(host.loads[0].pixels[5] == 0x1234);
  CHECK(host.loads[1].area ==
        ScreenRect{.x = 0, .y = 448, .width = 544, .height = 64});
  CHECK(host.loads[1].size == 34816);
}

TEST_CASE("Title background textures load the palettes",
          "[title][title-menu]") {
  constexpr std::size_t kPaletteLoads = 8;
  FakeHost host;
  LoadBackground(host);
  REQUIRE(host.loads.size() == 11);
  for (std::size_t item = 0; item < kPaletteLoads; ++item) {
    const LoadRecord& load = host.loads[2 + item];
    CHECK(load.area == ScreenRect{.x = 480,
                                  .y = static_cast<std::int16_t>(384 + item),
                                  .width = 16,
                                  .height = 1});
    CHECK(load.pixels[1] == 0x8800);
  }
  CHECK(host.loads[10].area ==
        ScreenRect{.x = 480, .y = 394, .width = 16, .height = 1});
  CHECK(host.loads[10].pixels[15] == 0xFC62);
}

TEST_CASE("Title background textures refuse bad encodings and heaps",
          "[title][title-menu]") {
  constexpr std::size_t kShortHeap = 10;
  FakeHost overrun;
  overrun.background = {0x00010000};
  title::LoadBackgroundTextures(overrun);
  CHECK(
      overrun.refusals ==
      std::vector<std::string>{"title background encoding overruns its image"});
  CHECK(overrun.loads.size() == 11);
  CHECK(overrun.freed.size() == 1);

  for (const std::size_t limit : {std::size_t{0}, kShortHeap}) {
    FakeHost host;
    host.heap_limit = limit;
    title::LoadBackgroundTextures(host);
    CHECK(host.refusals ==
          std::vector<std::string>{"no heap for the title background"});
    CHECK(host.loads.empty());
    CHECK(host.freed == (limit == 0 ? std::vector<std::size_t>{}
                                    : std::vector<std::size_t>{limit}));
  }
}

TEST_CASE("Title display waits for the scanline before showing",
          "[title][title-menu]") {
  FakeHost host;
  title::InitializeTitleDisplay(host);
  CHECK(host.hblanks == 249);
  CHECK(host.displays ==
        std::vector<title::DisplaySetup>{
            {.display = {.x = 512, .y = 16, .width = 512, .height = 480},
             .screen = {.x = 0, .y = 8, .width = 0, .height = 224},
             .interlaced = true,
             .draw = {.x = 512, .y = 0, .width = 512, .height = 480}}});
  CHECK(host.events == std::vector<std::string>{"PutDisplay", "VSync"});
}

TEST_CASE("Title pixel blends mix their channels", "[title][title-menu]") {
  CHECK(title::BlendMenuBackgroundPixel(0x001F, 0x7C00, 16) == 0xBC0F);
  CHECK(title::BlendMenuBackgroundPixel(0x7FFF, 0, 0) == 0x8000);
  CHECK(title::BlendMenuBackgroundPixel(0x7FFF, 0, 32) == 0xFFFF);
  CHECK(title::BlendCopyrightPixel(0x1234, 0x4321, 0) == 0x4321);
  CHECK(title::BlendCopyrightPixel(0x1234, 0x4321, 32) == 0x1234);
  CHECK(title::BlendPaletteEntry(0, 0x1F, 16) == 0x801F);
  CHECK(title::BlendPaletteEntry(0, 0x1F, 8) == 0x800F);
  CHECK(title::BlendPaletteEntry(0, 0x1F, 64) == 0x801C);
}

TEST_CASE("Title menu background blends the captured strips",
          "[title][title-menu]") {
  constexpr std::size_t kBlendOffset = 40960;
  std::vector<std::uint16_t> capture = StripCapture();
  FakeHost host;
  title::BlendMenuBackground(host, capture, 16);
  CHECK(capture[kBlendOffset] == kStripBlended);
  CHECK(capture[0] == kStripTarget);
  CHECK(host.events == std::vector<std::string>{"LoadImage", "LoadImage",
                                                "VSync 4",
                                                "UpdateControllers"});
}

TEST_CASE("Title menu background uploads the blended strips",
          "[title][title-menu]") {
  std::vector<std::uint16_t> capture = StripCapture();
  FakeHost host;
  title::BlendMenuBackground(host, capture, 16);
  REQUIRE(host.loads.size() == 2);
  CHECK(host.loads[0].area ==
        ScreenRect{.x = 192, .y = 448, .width = 160, .height = 64});
  CHECK(host.loads[0].size == 10240);
  CHECK(host.loads[1].area ==
        ScreenRect{.x = 352, .y = 448, .width = 160, .height = 64});
  for (const LoadRecord& load : host.loads) {
    CHECK(std::ranges::all_of(load.pixels, [](std::uint16_t pixel) {
      return pixel == kStripBlended;
    }));
  }
}

TEST_CASE("Title menu background refuses a short capture",
          "[title][title-menu]") {
  FakeHost refused;
  std::vector<std::uint16_t> short_capture(100);
  title::BlendMenuBackground(refused, short_capture, 16);
  title::BlendMenuBackground(refused, {}, 16);
  CHECK(refused.refusals.size() == 1);
  CHECK(refused.loads.empty());
  CHECK(refused.vblanks == 8);
  CHECK(refused.polls == 2);
}

TEST_CASE("Title copyright blends over its background", "[title][title-menu]") {
  constexpr std::uint16_t kBackground = 0x1234;
  std::vector<std::uint16_t> capture(kCaptureHalfwords);
  std::ranges::fill(
      std::span(capture).subspan(kCopyrightSource, kCopyrightHalfwords),
      kBackground);
  FakeHost host;
  title::BlendCopyright(host, capture, 0);
  title::BlendCopyright(host, capture, 32);
  REQUIRE(host.loads.size() == 2);
  CHECK(host.loads[0].area ==
        ScreenRect{.x = 678, .y = 376, .width = 180, .height = 24});
  CHECK(host.loads[0].size == kCopyrightHalfwords);
  CHECK(host.loads[0].pixels.front() == FakeHost::kCopyrightPixel);
  CHECK(host.loads[1].pixels.back() == kBackground);
  CHECK(host.events ==
        std::vector<std::string>{"VSync 2", "UpdateControllers", "LoadImage",
                                 "VSync 2", "UpdateControllers", "LoadImage"});

  host.copyright.resize(10);
  title::BlendCopyright(host, capture, 0);
  CHECK(host.refusals ==
        std::vector<std::string>{"title copyright smaller than 180x24"});
  CHECK(host.loads.size() == 2);
  CHECK(host.vblanks == 6);
}

TEST_CASE("Title item palettes blend between their rows",
          "[title][title-menu]") {
  FakeHost host;
  title::BlendMenuItemClut(host, 3, 16, 0, 1);
  title::BlendMenuItemClut(host, 9, 0, 2, 3);
  title::BlendMenuItemClut(host, 0, 16, 2, 3);
  REQUIRE(host.loads.size() == 3);
  CHECK(host.loads[0].area ==
        ScreenRect{.x = 480, .y = 387, .width = 16, .height = 1});
  CHECK(host.loads[0].pixels[0] == 0);
  CHECK(host.loads[0].pixels[15] == 0xCF9F);
  CHECK(host.loads[1].area ==
        ScreenRect{.x = 480, .y = 393, .width = 16, .height = 1});
  CHECK(host.loads[1].pixels[15] == 0xFC62);
  CHECK(host.loads[2].pixels[15] == 0xAAFC);

  title::BlendMenuItemClut(host, 10, 0, 0, 1);
  title::BlendMenuItemClut(host, -1, 0, 0, 1);
  title::BlendMenuItemClut(host, 0, 0, 4, 1);
  title::BlendMenuItemClut(host, 0, 0, 0, -1);
  CHECK(host.refusals.size() == 4);
  CHECK(host.loads.size() == 3);
}

TEST_CASE("Title selected item fades its text in", "[title][title-menu]") {
  Fixture fixture;
  const title::MenuItem& item = fixture.state.items[0];
  title::InitializeMenuItem(fixture.state, 0, 64);
  Animate(fixture, 16);
  CHECK(item.text_saturation == 128);
  CHECK(item.text_blend_factor == 4);
  CHECK(item.outline_saturation == 0);
}

TEST_CASE("Title selected item settles its outline", "[title][title-menu]") {
  Fixture fixture;
  const title::MenuItem& item = fixture.state.items[0];
  title::InitializeMenuItem(fixture.state, 0, 64);
  Animate(fixture, 19);
  CHECK(item.outline_saturation == 176);
  CHECK(item.text_blend_factor == 16);
  Animate(fixture, 3);
  CHECK(item.outline_saturation == 128);
  Animate(fixture, 1);
  CHECK(item.outline_saturation == 128);
  CHECK(item.outline_position == 64);
  CHECK(fixture.host.loads.size() == 23);
}

TEST_CASE("Title menu ignores items outside its table", "[title][title-menu]") {
  title::State state;
  title::InitializeMenuItem(state, 0, 64);
  const title::State before = state;
  title::InitializeMenuItem(state, 10, 0);
  CHECK(state.items == before.items);
}

TEST_CASE("Title items take a step toward their targets",
          "[title][title-menu]") {
  Fixture fixture;
  const auto& items = fixture.state.items;
  PlaceScrollingItems(fixture.state);
  title::UpdateMenuAnimation(fixture.context);
  CHECK(items[1].text_position == 92);
  CHECK(items[2].text_position == 254);
  CHECK(items[3].text_position == 36);
  CHECK(items[5].text_blend_factor == 2);
  CHECK(fixture.host.loads.size() == 4);
}

TEST_CASE("Title items scroll to their targets", "[title][title-menu]") {
  Fixture fixture;
  const auto& items = fixture.state.items;
  PlaceScrollingItems(fixture.state);
  Animate(fixture, 8);
  CHECK(items[1].text_position == 64);
  CHECK(items[2].text_position == 250);
  CHECK(items[3].text_position == 40);
  CHECK(std::ranges::all_of(std::span(items).subspan(1, 3),
                            [](const title::MenuItem& item) {
                              return item.motion == MenuItemMotion::kStationary;
                            }));
  CHECK(items[4].text_saturation == 64);
  CHECK(items[4].motion == MenuItemMotion::kSubmenu);
  CHECK(fixture.host.loads.size() == 32);
}

TEST_CASE("Title menu items draw their outlines first", "[title][title-menu]") {
  Fixture fixture;
  const FakeHost& host = fixture.host;
  DrawOutlinedItems(fixture);
  REQUIRE(host.packets.size() == 34);
  CHECK(host.packets[0][3] == ((224U << 16U) | 112U));
  CHECK(host.packets[0][2] == 0x66101010);
  CHECK(host.packets[1][0] == 0x05FFFFFF);
  CHECK(host.packets[1][2] == 0x66404040);
  CHECK(host.packets[1][3] == ((288U << 16U) | 184U));
}

TEST_CASE("Title menu items draw text lines after their outlines",
          "[title][title-menu]") {
  Fixture fixture;
  const FakeHost& host = fixture.host;
  DrawOutlinedItems(fixture);
  REQUIRE(host.packets.size() == 34);
  CHECK(host.packets[2][2] == 0x64808080);
  CHECK(host.packets[2][3] == ((304U << 16U) | 192U));
  CHECK(host.packets[2][5] == ((1U << 16U) | 128U));
  CHECK(host.packets[33][3] >> 16U == 335);
}

TEST_CASE("Title text lines clip and fade above the strip",
          "[title][title-menu]") {
  const std::optional<title::MenuSpritePacket> shown = TextLineAt(64);
  const std::optional<title::MenuSpritePacket> above = TextLineAt(32);
  REQUIRE(shown.has_value());
  REQUIRE(above.has_value());
  const title::MenuSpritePacket shown_packet =
      shown.value_or(title::MenuSpritePacket{});
  const title::MenuSpritePacket above_packet =
      above.value_or(title::MenuSpritePacket{});
  CHECK(above_packet[2] == 0x66000000);
  CHECK((above_packet[3] & 0xFFFFU) == 176);
  CHECK(above_packet[5] == kClippedTextSize);
  CHECK(above_packet[4] == shown_packet[4] + 16);
}

TEST_CASE("Title text lines clip and fade below the strip",
          "[title][title-menu]") {
  const std::optional<title::MenuSpritePacket> below = TextLineAt(96);
  REQUIRE(below.has_value());
  const title::MenuSpritePacket below_packet =
      below.value_or(title::MenuSpritePacket{});
  CHECK(below_packet[2] == 0x667C7C7C);
  CHECK((below_packet[3] & 0xFFFFU) == 224);
  CHECK(below_packet[5] == kClippedTextSize);
}

TEST_CASE("Title text lines clip at the strip's right edge",
          "[title][title-menu]") {
  const std::optional<title::MenuSpritePacket> right = TextLineAt(220);
  REQUIRE(right.has_value());
  CHECK(right.value_or(title::MenuSpritePacket{})[5] == 0xFFFFFFF4);
  CHECK_FALSE(TextLineAt(0).has_value());
}

TEST_CASE("Title outlines skip items outside the menu and follow submenus",
          "[title][title-menu]") {
  const title::MenuItem item{.is_enabled = 1, .text_position = 64};
  CHECK_FALSE(title::TextLinePacket(item, 10, 0).has_value());
  CHECK(title::OutlinePacket(item, 10) == title::MenuSpritePacket{});
  const title::MenuItem submenu{.is_enabled = 1,
                                .motion = MenuItemMotion::kSubmenu,
                                .text_position = 64,
                                .outline_position = 32};
  CHECK(title::OutlinePacket(submenu, 0)[3] == ((256U << 16U) | 184U));
}

TEST_CASE("Title menu draws the strips and items", "[title][title-menu]") {
  Fixture fixture;
  const FakeHost& host = fixture.host;
  DrawFirstItem(fixture);
  REQUIRE(host.packets.size() == 68);
  CHECK(host.packets[0][3] == ((256U << 16U) | 176U));
  CHECK(host.packets[0][5] == ((64U << 16U) | 160U));
  CHECK(host.packets[0][2] == 0x64808080);
  CHECK(host.packets[1][3] == ((320U << 16U) | 176U));
  CHECK(host.packets[2][2] == 0x66080808);
}

TEST_CASE("Title menu draws both interlaced fields", "[title][title-menu]") {
  Fixture fixture;
  const FakeHost& host = fixture.host;
  DrawFirstItem(fixture);
  REQUIRE(host.packets.size() == 68);
  CHECK(host.packets[34] == host.packets[0]);
  CHECK(host.loads.size() == 1);
}

TEST_CASE("Title menu fields wait between their submissions",
          "[title][title-menu]") {
  Fixture fixture;
  const FakeHost& host = fixture.host;
  DrawFirstItem(fixture);
  REQUIRE(host.events.size() == 72);
  CHECK(host.events[0] == "LoadImage");
  CHECK(std::ranges::all_of(
      std::span(host.events).subspan(1, 34),
      [](const std::string& event) { return event == "Submit"; }));
  CHECK(host.events[35] == "VSync");
  CHECK(host.events[36] == "UpdateVibrationPatterns");
  CHECK(host.events[71] == "AudioTail");
}

TEST_CASE("Title menu polls input after drawing", "[title][title-menu]") {
  Fixture fixture;
  const FakeHost& host = fixture.host;
  DrawFirstItem(fixture);
  title::DrawMenuAndPollInput(fixture.context);
  CHECK(host.events.back() == "UpdateControllers");
  CHECK(host.events[host.events.size() - 2] == "VSync");
}

TEST_CASE("Title transition tile covers the screen", "[title][title-menu]") {
  const title::TransitionTilePacket tile = title::TransitionTile();
  CHECK(tile[0] == 0x04FFFFFF);
  CHECK(tile[2] == 0x62080808);
  CHECK(tile[3] == 0);
  CHECK(tile[4] == ((480U << 16U) | 640U));
}

TEST_CASE("Title vibration submenu toggles and confirms",
          "[title][title-menu]") {
  Fixture fixture;
  const FakeHost& host = fixture.host;
  ToggleVibration(fixture);
  CHECK(host.sounds == std::vector<int>{kChangeSound, kSelectSound});
  CHECK(host.settings.vibration_on == 1);
  CHECK(host.presets == std::vector<int>{0});
  CHECK(host.polls == 42);
  CHECK(host.script.empty());
}

TEST_CASE("Title vibration submenu closes its choices", "[title][title-menu]") {
  Fixture fixture;
  const auto& items = fixture.state.items;
  ToggleVibration(fixture);
  CHECK(items[5].is_enabled == 0);
  CHECK(items[6].is_enabled == 0);
  CHECK(items[2].motion == MenuItemMotion::kStationary);
  CHECK(items[2].text_position == 64);
  CHECK(items[2].outline_position == 64);
  CHECK(items[2].text_blend_factor == 16);
}

TEST_CASE("Title vibration submenu returns its neighbours",
          "[title][title-menu]") {
  Fixture fixture;
  const auto& items = fixture.state.items;
  ToggleVibration(fixture);
  CHECK(items[1].text_position == 32);
  CHECK(items[1].motion == MenuItemMotion::kStationary);
  CHECK(items[3].text_position == 96);
  CHECK(items[3].motion == MenuItemMotion::kStationary);
}

TEST_CASE("Title vibration submenu leaves on Cross", "[title][title-menu]") {
  Fixture fixture;
  FakeHost& host = fixture.host;
  host.log_frames = false;
  host.record_packets = false;
  host.settings.vibration_on = 1;
  PlaceMainItems(fixture.state, 2);
  Repeat(host.script, 24);
  host.idle = Held(kCross);
  title::ShowVibrationSettings(fixture.context);
  CHECK(host.sounds == std::vector<int>{kLeaveSound});
  CHECK(host.Count("SetSettings") == 0);
  CHECK(host.presets.empty());
  CHECK(host.polls == 35);
}

TEST_CASE("Title sound submenu switches to mono", "[title][title-menu]") {
  Fixture fixture;
  const FakeHost& host = fixture.host;
  SwitchToMono(fixture);
  CHECK(host.sounds == std::vector<int>{kChangeSound, kSelectSound});
  CHECK(host.settings.mono_sound == 1);
  CHECK(host.mono == 1);
  CHECK(host.disc_volume == 0x7F);
}

TEST_CASE("Title sound submenu closes and returns its items",
          "[title][title-menu]") {
  Fixture fixture;
  const auto& items = fixture.state.items;
  SwitchToMono(fixture);
  CHECK(items[4].is_enabled == 0);
  CHECK(items[7].is_enabled == 0);
  CHECK(items[3].text_position == 64);
  CHECK(items[2].text_position == 32);
  CHECK(items[0].text_position == 96);
}

TEST_CASE("Title submenus refuse settings outside their items",
          "[title][title-menu]") {
  Fixture fixture;
  FakeHost& host = fixture.host;
  host.settings = {.mono_sound = 3, .vibration_on = 9};
  title::ShowVibrationSettings(fixture.context);
  title::ShowSoundSettings(fixture.context);
  CHECK(host.refusals ==
        std::vector<std::string>{"title setting outside the submenu's items",
                                 "title setting outside the submenu's items"});
  CHECK(host.polls == 0);
  CHECK(host.loads.empty());
}

TEST_CASE("Title run starts a new game", "[title][title-menu]") {
  Fixture fixture;
  const FakeHost& host = fixture.host;
  CHECK(RunNewGame(fixture) == 0);
  CHECK(host.refusals ==
        std::vector<std::string>{"title selection outside the main menu"});
  CHECK(host.sounds == std::vector<int>{1, 2, 3});
  CHECK(host.exits == std::vector<title::ExitPlan>{title::PlanExit(0)});
  CHECK(host.visit_count == 2);
  CHECK(host.events.back() == "DisplayOff");
}

TEST_CASE("Title run fades out and releases its captures",
          "[title][title-menu]") {
  Fixture fixture;
  const FakeHost& host = fixture.host;
  CHECK(RunNewGame(fixture) == 0);
  CHECK(host.tiles == 64);
  CHECK(host.blocks.size() == 2);
  CHECK(host.freed.size() == 2);
  REQUIRE(host.stores.size() == 4);
  CHECK(host.stores[3] ==
        StoreRecord{.area = {.x = 688, .y = 256, .width = 160, .height = 128},
                    .offset = 0,
                    .size = kStripHalfwords});
}

TEST_CASE("Title run sets a new game up in order", "[title][title-menu]") {
  Fixture fixture;
  const FakeHost& host = fixture.host;
  CHECK(RunNewGame(fixture) == 0);
  CHECK(host.Count("StopIntroMovie") == 1);
  CHECK(host.Precedes("ResetSession", "SetGameModes"));
  CHECK(host.Precedes("SetGameModes", "ClearIntroMoviePlaying"));
  CHECK(host.Precedes("StartIntroMovie", "DisplayOn"));
}

TEST_CASE("Title run plays an opening cutscene after idling",
          "[title][title-menu]") {
  Fixture fixture;
  FakeHost& host = fixture.host;
  PrepareIdleRun(host);
  CHECK(title::Run(fixture.context) == 3);
  CHECK_FALSE(host.clear_save_pending);
  CHECK(host.exits == std::vector<title::ExitPlan>{title::PlanExit(3)});
  CHECK(host.sounds.empty());
  CHECK(host.visit_count == 1);
  CHECK(host.clears.size() == 4);
}

TEST_CASE("Title run stages the cleared game before its cutscene",
          "[title][title-menu]") {
  Fixture fixture;
  FakeHost& host = fixture.host;
  PrepareIdleRun(host);
  CHECK(title::Run(fixture.context) == 3);
  CHECK(host.Precedes("StageClearGame", "ClearPendingClearSave"));
  CHECK(host.Precedes("SetVisitCount 0", "InitializeFileDisplay"));
  CHECK(host.Precedes("InitializeFileDisplay", "ResetSession"));
}

TEST_CASE("Title run plays the second cutscene on the next idle",
          "[title][title-menu]") {
  Fixture fixture;
  FakeHost& host = fixture.host;
  PrepareIdleRun(host);
  CHECK(title::Run(fixture.context) == 3);
  CHECK(title::Run(fixture.context) == 4);
  CHECK(host.exits.back() == title::PlanExit(4));
  CHECK(host.visit_count == 2);
  CHECK(host.clears.size() == 5);
}

TEST_CASE("Title run continues from a loaded game", "[title][title-menu]") {
  constexpr std::size_t kDebugFontUpload = 4;
  Fixture fixture;
  FakeHost& host = fixture.host;
  PrepareRun(host);
  host.script = {Held(kDown)};
  Repeat(host.script, 10);
  host.script.push_back(Held(kStart));
  host.card_steps = {0, 1};
  host.load_results = {0, 0, 1};
  CHECK(title::Run(fixture.context) == 1);
  CHECK(host.sounds == std::vector<int>{kChangeSound, kSelectSound});
  CHECK(host.load_menu_calls == std::vector<int>{2, 0, 0, 0});
  CHECK(host.exits == std::vector<title::ExitPlan>{title::ExitPlan{}});
  REQUIRE(host.uploads.size() == 7);
  CHECK(host.uploads[kDebugFontUpload] ==
        UploadRecord{.image = title::Image::kDebugFont,
                     .area = {.x = 960, .y = 66, .width = 64, .height = 158}});
  CHECK(fixture.state.items[1].text_position == 64);
  CHECK(fixture.state.items[2].is_enabled == 1);
}

TEST_CASE("Title run shows the menu again after leaving Continue",
          "[title][title-menu]") {
  Fixture fixture;
  FakeHost& host = fixture.host;
  PrepareRun(host);
  host.initial_selection = 1;
  host.script = {Held(kStart)};
  Repeat(host.script, 64);
  host.script.push_back(Held(kStart));
  host.load_results = {-1, 1};
  CHECK(title::Run(fixture.context) == 1);
  CHECK(host.Count("StartIntroMovie") == 2);
  CHECK(host.sounds == std::vector<int>{kSelectSound, kSelectSound});
  CHECK(host.load_menu_calls == std::vector<int>{2, 0, 2, 0});
  CHECK(host.blocks.size() == 4);
  CHECK(host.freed.size() == 4);
  CHECK(host.exits == std::vector<title::ExitPlan>{title::ExitPlan{}});
  CHECK(host.script.empty());
}

TEST_CASE("Title clear-save screen saves then reminds", "[title][title-menu]") {
  FakeHost host;
  SaveThenRemind(host);
  CHECK(host.save_menu_calls == std::vector<int>{1, 0, 1, 0});
  CHECK(host.reminder_calls == std::vector<int>{1, 0, 0});
  CHECK(host.Count("ShowSavePrompt") == 1);
  CHECK(host.Count("ReleaseCardResources") == 1);
  CHECK(host.events.back() == "DisplayOff");
  CHECK(host.uploads.size() == 7);
}

TEST_CASE("Title clear-save screen runs a frame per menu step",
          "[title][title-menu]") {
  FakeHost host;
  SaveThenRemind(host);
  CHECK(host.clock_advances == 36);
  CHECK(host.file_swaps == 36);
  CHECK(host.cipher_steps == 36);
  CHECK(host.polls == 36);
  CHECK(host.exit_fades == 31);
  CHECK(host.file_draws == 3);
}

TEST_CASE("Title clear-save screen leaves when saving is declined",
          "[title][title-menu]") {
  FakeHost host;
  host.log_frames = false;
  host.save_results = {0, -1};
  host.reminder_results = {3};
  title::ShowClearSaveScreen(host);
  CHECK(host.clock_advances == 35);
  CHECK(host.exit_fades == 31);
  CHECK(host.file_draws == 2);
  CHECK(host.save_menu_calls == std::vector<int>{1, 0, 0});
  CHECK(host.reminder_calls == std::vector<int>{1, 0});
  CHECK(host.Precedes("StepCardResources 1", "StepCardResources 0"));
}

TEST_CASE("Title continue screen returns the load menu's result",
          "[title][title-menu]") {
  FakeHost host;
  CHECK(ContinueFromCard(host) == 3);
  using title::Image;
  CHECK(host.uploads ==
        std::vector<UploadRecord>{
            {Image::kSaveUiPageZero, {768, 0, 64, 224}},
            {Image::kSaveUiPageOne, {448, 256, 64, 256}},
            {Image::kSaveFontPageZero, {832, 0, 64, 224}},
            {Image::kSaveFontPageOne, {896, 0, 64, 224}},
            {Image::kDebugFont, {960, 66, 64, 158}},
            {Image::kSaveBackgroundPalette, {768, 227, 256, 1}},
            {Image::kSaveBackgroundPixels, {672, 256, 96, 240}}});
  CHECK(host.clears ==
        std::vector<ScreenRect>{{640, 256, 32, 240}, {768, 256, 32, 240}});
  CHECK(host.events.back() == "DisplayOff");
}

TEST_CASE("Title continue screen runs the load menu's frames",
          "[title][title-menu]") {
  FakeHost host;
  CHECK(ContinueFromCard(host) == 3);
  CHECK(host.clock_advances == 5);
  CHECK(host.polls == 5);
  CHECK(host.file_draws == 1);
  CHECK(host.load_menu_calls == std::vector<int>{2, 0, 0});
  CHECK(host.Precedes("InitializeFileDisplay", "AdvanceFrameClock 2"));
  CHECK(host.Precedes("ResetFileElements", "ReleaseCardResources"));
}

TEST_CASE("Title live exit keeps the state for a loaded game",
          "[title][title-menu]") {
  const PresetLiveExit preset;
  const State_StateFlags& flags = Audio_Seq_unusedStateFlags;
  title::PrepareLiveExit(1);
  CHECK(flags.opaqueC == 0);
  CHECK(flags.opaqueD == 0);
  CHECK(flags.opaqueE == PresetLiveExit::kFlag);
  CHECK(State_savedEntryDoor.destinationZoneId == PresetLiveExit::kDestination);
}

TEST_CASE("Title live exit writes the state flags and entry door",
          "[title][title-menu]") {
  const PresetLiveExit preset;
  const State_StateFlags& flags = Audio_Seq_unusedStateFlags;
  const Field_MpdDoorRecord& door = State_savedEntryDoor;
  title::PrepareLiveExit(1);
  title::PrepareLiveExit(4);
  CHECK(flags.opaqueD == 1);
  CHECK(flags.opaqueE == 1);
  CHECK(door.destinationZoneId == 2);
  CHECK(door.destinationRoomId == 1);
}

TEST_CASE("Title live exit sends a new game to its first room",
          "[title][title-menu]") {
  const PresetLiveExit preset;
  const State_StateFlags& flags = Audio_Seq_unusedStateFlags;
  const Field_MpdDoorRecord& door = State_savedEntryDoor;
  title::PrepareLiveExit(1);
  title::PrepareLiveExit(4);
  title::PrepareLiveExit(0);
  CHECK(flags.opaqueD == 0);
  CHECK(flags.opaqueE == 1);
  CHECK(door.destinationZoneId == 1);
  CHECK(door.destinationRoomId == 0);
}

TEST_CASE("Title live host binds settings and visits", "[title][title-menu]") {
  const LiveHostGuard guard;
  title::Host& host = title::LiveTitleHost();
  CHECK(&title::LiveTitleHost() == &host);
  host.SetSettings({.mono_sound = 1, .vibration_on = 0});
  CHECK(Input_settings.monoSound == 1);
  CHECK(Input_settings.vibrationOn == 0);
  CHECK(host.Settings() ==
        title::PlayerSettings{.mono_sound = 1, .vibration_on = 0});
  host.SetVisitCount(5);
  CHECK(Title_titleScreenVisitCount == 5);
  CHECK(host.VisitCount() == 5);
}

TEST_CASE("Title live host draws the game into the second buffer",
          "[title][title-menu]") {
  const LiveHostGuard guard;
  ApplyLiveGameDisplay();
  const DRAWENV& draw = Render_legacyDrawEnvironments[1];
  CHECK(draw.clip.x == 640);
  CHECK(draw.clip.w == 640);
  CHECK(draw.clip.h == 224);
  CHECK(draw.isbg == 1);
  CHECK(draw.dtd == 0);
  CHECK(draw.r0 == 1);
  CHECK(draw.b0 == 3);
}

TEST_CASE("Title live host shows the game from the other buffer",
          "[title][title-menu]") {
  const LiveHostGuard guard;
  ApplyLiveGameDisplay();
  const DISPENV& shown = Render_legacyDisplayEnvironments[0];
  CHECK(shown.disp.x == 640);
  CHECK(shown.screen.y == 8);
  CHECK(shown.screen.w == 256);
  CHECK(shown.screen.h == 224);
  CHECK(Render_legacyDisplayEnvironments[1].disp.x == 0);
}

TEST_CASE("Title live file elements reset to the title's defaults",
          "[title][title-savefiles]") {
  constexpr u_short kDefaultTarget = 0x180;
  const DirtyLiveTitleFiles dirty;
  VS::Menu::Files::ResetLiveTitleElements();
  CHECK(std::ranges::all_of(Title_fileMenuElements,
                            [](const Menu_Files_FileMenuElement& element) {
                              return element.state == 0;
                            }));
  CHECK(Title_fileMenuScreenFade == 0);
  CHECK(Title_selectionCursorColor == 0);
  CHECK(Title_selectionCursorPosition == 0);
  CHECK(Title_transferProgressCounter == 0);
  CHECK(Title_transferProgressTarget == kDefaultTarget);
  CHECK(Title_memoryCardMessage == nullptr);
}

TEST_CASE("Title live file animations complete after a reset",
          "[title][title-savefiles]") {
  namespace files = VS::Menu::Files;
  const DirtyLiveTitleFiles dirty;
  CHECK_FALSE(files::AreLiveTitleAnimationsComplete());
  files::ResetLiveTitleElements();
  CHECK(files::AreLiveTitleAnimationsComplete());
  CHECK_FALSE(files::InitializeLiveTitleElement(0, 0, 0, 0));
}

TEST_CASE("Title card readiness restarts and TITLE keeps no module symbols",
          "[title][title-savecard]") {
  CHECK(VS::Save::MemoryCard::AdvanceLiveTitleCardReadiness(1) == 0);
  CHECK(VS::Save::MemoryCard::AdvanceLiveTitleCardReadiness(2) == 0);
  const auto in_title = [](const char* module) {
    return module != nullptr && std::string_view(module) == "TITLE/TITLE.PRG";
  };
  CHECK(std::ranges::none_of(
      std::span(Content_moduleSymbols, Content_moduleSymbolCount),
      [&in_title](const Content_ModuleSymbol& symbol) {
        return in_title(symbol.module);
      }));
  CHECK(std::ranges::none_of(
      std::span(Content_modulePointers, Content_modulePointerCount),
      [&in_title](const Content_ModulePointerTable& table) {
        return in_title(table.module);
      }));
}
