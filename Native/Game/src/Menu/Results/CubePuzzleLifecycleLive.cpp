// Hand-maintained (tools/port/owned.csv).
// Binds the typed cube-puzzle start, results and quit screens
// (CubePuzzleLifecycle.cpp) to the Results module state, the disc queue, the
// heap, LoadImage, the texture pages, the sound effects and the live draws.
#include "Menu/Results/CubePuzzleLifecycleLive.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

#include "Audio/SfxLive.hpp"
#include "Battle/StatusEffects.h"
#include "Content/CdDriveLive.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "DiscQueue.h"
#include "Field/Gimmicks.h"
#include "Field/Map.h"
#include "Field/MinimapScene.hpp"
#include "Field/Room.h"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Results/CubePuzzle.hpp"
#include "Menu/Results/CubePuzzleLifecycle.hpp"
#include "Menu/Results/CubePuzzleText.hpp"
#include "Menu/Results/CubePuzzleTextLive.hpp"
#include "Menu/Results/Results.h"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Menu/Results/ResultsDrawingLive.hpp"
#include "Menu/Results/ResultsPalette.hpp"
#include "Menu/Results/TimeAttack.hpp"
#include "Render/DisplayControl.hpp"
#include "Render/TexturePagesLive.hpp"
#include "State/Progress.hpp"
#include "State/StateFlags.hpp"

namespace VS::Menu::Results {
namespace {
static_assert(sizeof(u_short) == sizeof(std::uint16_t));
static_assert(std::size(clutBuf) == kResultsClutWords);

// Refusals so far; each warning repeats at each power of two.
std::uint64_t refusals = 0;

void RefuseCall(std::string_view call, std::string_view reason) {
  if (std::has_single_bit(++refusals)) {
    VS::Core::LogWarn("menu.results", "Cube-puzzle {} refused {} ({} refusals)",
                      call, reason, refusals);
  }
}

[[nodiscard]] std::string_view Describe(CubePuzzleRefusal refusal) {
  switch (refusal) {
    case CubePuzzleRefusal::kImage:
      return "an image the asset cannot hold";
    case CubePuzzleRefusal::kPalette:
      return "a palette shorter than 160 colors";
    case CubePuzzleRefusal::kRecords:
      return "an asset too short for the puzzle records";
    case CubePuzzleRefusal::kRoom:
      return "the record lookup without a minimap room";
  }
  return "an unknown refusal";
}

// The original's per-initializer statics: the request and the file buffer.
struct AssetLoad {
  Content_DiscQueueSlot* request = nullptr;
  void* buffer = nullptr;
};

AssetLoad iq_load;
AssetLoad esc_load;

[[nodiscard]] std::span<std::uint16_t, kResultsClutWords> LiveClut() {
  return std::span<std::uint16_t, kResultsClutWords>{clutBuf};
}

class LiveCubePuzzleLoad final : public CubePuzzleLoadHost {
 public:
  LiveCubePuzzleLoad(std::string_view call, AssetLoad& load,
                     const Content_DiscFile& file)
      : call_(call), load_(load), file_(file) {}

  void ReserveTexturePages(std::uint32_t first_page, std::uint32_t page_count,
                           std::int32_t owner) override {
    Render::ReserveLiveTexturePageRange(first_page, page_count, owner);
  }
  void BeginAssetRead(CubePuzzleAsset /*asset*/) override {
    load_.buffer = Core::Memory::AllocateLiveFromHighEnd(
        static_cast<std::size_t>(file_.size));
    if (load_.buffer == nullptr) {
      RefuseCall(call_, "a read without a buffer");
      load_.request = nullptr;
      return;
    }
    load_.request = Content::AllocateLiveDiscRequest(&file_);
    if (load_.request != nullptr) {
      Content::EnqueueLiveDiscRequest(load_.request, load_.buffer);
    } else {
      load_.request =
          Content::CompleteLiveRefusedDiscRequest(&file_, load_.buffer);
    }
  }
  bool AssetReadLoaded() override {
    if (load_.request == nullptr) {
      // Nothing can arrive; the initializer goes on with no asset.
      RefuseCall(call_, "a poll without a read");
      return true;
    }
    return load_.request->state == Content_DiscRequestState_Loaded;
  }
  std::span<std::byte> LoadedAsset() override {
    if (load_.buffer == nullptr || load_.request == nullptr) {
      return {};
    }
    const std::expected<std::size_t, Core::Memory::HeapError> capacity =
        Core::Memory::LivePayloadCapacity(load_.buffer);
    if (!capacity.has_value()) {
      RefuseCall(call_, "a buffer outside the heap");
      return {};
    }
    const std::size_t bytes =
        std::min(static_cast<std::size_t>(std::max(file_.size, 0)), *capacity);
    return {static_cast<std::byte*>(load_.buffer), bytes};
  }
  void UploadImage(const Render::ScreenRect& area,
                   std::span<const std::byte> pixels) override {
    RECT rect{};
    setRECT(&rect, area.x, area.y, area.width, area.height);
    // LoadImage only reads the pixels.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    LoadImage(&rect,
              reinterpret_cast<u_long*>(const_cast<std::byte*>(pixels.data())));
  }
  // The original read the scene's first room without checking the scene.
  std::optional<CubePuzzleRoom> CurrentRoom() override {
    if (Field_roomData.minimapScene == nullptr) {
      return std::nullopt;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto* const scene =
        reinterpret_cast<const std::byte*>(Field_roomData.minimapScene);
    const std::optional<Field::SceneRoomAddress> room =
        Field::CurrentSceneRoomAddress(
            {scene, Field_roomData.header.minimapSceneLength});
    if (!room.has_value()) {
      return std::nullopt;
    }
    return CubePuzzleRoom{.zone_id = room->zone_id, .map_id = room->map_id};
  }
  void ReleaseAssetRead() override {
    Content::ReleaseLiveDiscRequest(load_.request);
    load_.request = nullptr;
  }
  void FreeAssetBuffer() override {
    if (load_.buffer == nullptr) {
      RefuseCall(call_, "a free without a buffer");
      return;
    }
    Core::Memory::FreeLiveFromHighEnd(load_.buffer);
    load_.buffer = nullptr;
  }
  void Refuse(CubePuzzleRefusal refusal) override {
    RefuseCall(call_, Describe(refusal));
  }

 private:
  std::string_view call_;
  AssetLoad& load_;
  const Content_DiscFile& file_;
};

class LiveClutTarget final : public ResultsClutTarget {
 public:
  void Upload(const Render::ScreenRect& area,
              std::span<const std::uint16_t> colors) override {
    RECT rect{};
    setRECT(&rect, area.x, area.y, area.width, area.height);
    // LoadImage only reads the colors.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    LoadImage(&rect, reinterpret_cast<u_long*>(
                         const_cast<std::uint16_t*>(colors.data())));
  }
};

class LiveCubePuzzleScreen final : public CubePuzzleScreenHost {
 public:
  void PlaySfx(std::int32_t group, std::int32_t effect) override {
    Audio::LivePlaySfxCentered(group, effect);
  }
  void StopSfx(std::int32_t group, std::int32_t effect) override {
    Audio::LiveStopSfx(group, effect);
  }
  void RetireTexturePages(std::uint32_t first_page, std::uint32_t page_count,
                          std::int32_t owner) override {
    Render::RetireLiveTexturePageRange(first_page, page_count, owner);
  }
  void PulsePalette(std::int32_t blend_weight) override {
    LivePulseCubePuzzleClut(blend_weight);
  }
  void DrawMessage(CubePuzzleMessage message, ResultsPoint origin,
                   std::int32_t timer) override {
    switch (message) {
      case CubePuzzleMessage::kEvolveOrDie:
        LiveDrawEvolveOrDie(origin.x, origin.y, timer);
        break;
      case CubePuzzleMessage::kAverageTime:
        LiveDrawAverageTimeHeading(origin.x, origin.y, timer);
        break;
      case CubePuzzleMessage::kParTime:
        LiveDrawCubePuzzleParTime(origin.x, origin.y, timer);
        break;
      case CubePuzzleMessage::kCountingTime:
        LiveDrawCountingCubePuzzleTime(origin.x, origin.y, timer);
        break;
      case CubePuzzleMessage::kFoodChain:
        LiveDrawFoodChain(origin.x, origin.y, timer);
        break;
      case CubePuzzleMessage::kGivingUp:
        LiveDrawGivingUp(origin.y, timer);
        break;
      case CubePuzzleMessage::kChicken:
        LiveDrawChicken(origin.y, timer);
        break;
    }
  }
  void DrawRank(ResultsPoint origin, std::int32_t timer,
                std::int32_t rank) override {
    LiveDrawCubePuzzleRank(origin.x, origin.y, timer, rank);
  }
  void DrawPrompt(ResultsPoint origin, std::int32_t slide_timer,
                  std::int32_t bounce_timer) override {
    LiveDrawPressButtonPrompt(origin.x, origin.y, slide_timer, bounce_timer);
  }
};

[[nodiscard]] CubePuzzleScreen LiveScreen() {
  return {.load_step = Battle_unusedSubmenuState,
          .timer = screenTimer,
          .screen_state = screenState,
          .animation = Menu_Results_elementAnimationState,
          .on_screen_map_completion = Menu_Results_onScreenMapCompletion,
          .on_screen_score = Menu_Results_onScreenScore,
          .buff_reel_selection = Menu_Results_buffReelSelection};
}

void StoreScreen(const CubePuzzleScreen& screen) {
  Battle_unusedSubmenuState = screen.load_step;
  screenTimer = screen.timer;
  screenState = screen.screen_state;
  Menu_Results_elementAnimationState = screen.animation;
  Menu_Results_onScreenMapCompletion = screen.on_screen_map_completion;
  Menu_Results_onScreenScore = screen.on_screen_score;
  Menu_Results_buffReelSelection = screen.buff_reel_selection;
}

[[nodiscard]] CubePuzzleRating LiveRating() {
  return {.par_seconds = puzzleParTime, .rank_cap = puzzleRankCap};
}

void StoreRating(CubePuzzleRating rating) {
  puzzleParTime = rating.par_seconds;
  puzzleRankCap = rating.rank_cap;
}

// The flags' fields are plain char; the original loads them unsigned.
[[nodiscard]] CubePuzzleClock LiveClock() {
  return {
      .minutes =
          static_cast<std::uint8_t>(VS::State::stateFlags.puzzle_timer_minutes),
      .seconds =
          static_cast<std::uint8_t>(VS::State::stateFlags.puzzle_timer_seconds),
      .hundredths = static_cast<std::uint8_t>(
          VS::State::stateFlags.puzzle_timer_hundredths)};
}

[[nodiscard]] bool LiveConfirmPressed() {
  return (VS::Input::buttonsPressed & PADRright) != 0;
}

[[nodiscard]] int Finished(CubePuzzleLoad load) {
  return load == CubePuzzleLoad::kFinished ? 1 : 0;
}

[[nodiscard]] int Finished(CubePuzzleScreenStep step) {
  return step == CubePuzzleScreenStep::kShowing ? 0 : 1;
}
}  // namespace

// decomp: MENUF.PRG _pulseCubePuzzleEnd (src/MENU/MENUF.PRG/3B8.c:1294),
// 0x80104A50
void LivePulseCubePuzzleClut(int blend_weight) {
  LiveClutTarget target;
  PulseCubePuzzleClut(LiveClut(), blend_weight, target);
}

// decomp: MENUF.PRG _initCubePuzzleStart (src/MENU/MENUF.PRG/3B8.c:2260),
// 0x80107A8C
int LiveInitializeCubePuzzleStart() {
  LiveCubePuzzleLoad host("start", iq_load, kDisFiles[Menu_Results_DisFile_Iq]);
  CubePuzzleScreen screen = LiveScreen();
  CubePuzzleRating rating = LiveRating();
  const CubePuzzleLoad load =
      InitializeCubePuzzleStart(screen, rating, LiveClut(), host);
  StoreScreen(screen);
  StoreRating(rating);
  return Finished(load);
}

// decomp: MENUF.PRG _initCubePuzzleEnd (src/MENU/MENUF.PRG/3B8.c:2351),
// 0x80107D4C
int LiveInitializeCubePuzzleResults() {
  LiveCubePuzzleLoad host("results", iq_load,
                          kDisFiles[Menu_Results_DisFile_Iq]);
  CubePuzzleScreen screen = LiveScreen();
  CubePuzzleRating rating = LiveRating();
  CubePuzzleTimer timer{.clock = LiveClock(),
                        .show_maximum = static_cast<std::uint8_t>(
                            VS::State::stateFlags.puzzle_set_max_timer)};
  const CubePuzzleLoad load =
      InitializeCubePuzzleResults(screen, rating, timer, LiveClut(), host);
  StoreScreen(screen);
  StoreRating(rating);
  VS::State::stateFlags.puzzle_timer_minutes =
      static_cast<char>(timer.clock.minutes);
  VS::State::stateFlags.puzzle_timer_seconds =
      static_cast<char>(timer.clock.seconds);
  VS::State::stateFlags.puzzle_timer_hundredths =
      static_cast<char>(timer.clock.hundredths);
  VS::State::stateFlags.puzzle_set_max_timer =
      static_cast<char>(timer.show_maximum);
  return Finished(load);
}

// decomp: MENUF.PRG _initCubePuzzleQuit (src/MENU/MENUF.PRG/3B8.c:2363),
// 0x80107D98
int LiveInitializeCubePuzzleQuit() {
  LiveCubePuzzleLoad host("quit", esc_load,
                          kDisFiles[Menu_Results_DisFile_Esc]);
  CubePuzzleScreen screen = LiveScreen();
  const CubePuzzleLoad load =
      InitializeCubePuzzleQuit(screen, LiveClut(), host);
  StoreScreen(screen);
  return Finished(load);
}

// decomp: MENUF.PRG _renderCubePuzzleStart (src/MENU/MENUF.PRG/3B8.c:2432),
// 0x80107FC0
int LiveUpdateCubePuzzleStart() {
  LiveCubePuzzleScreen host;
  CubePuzzleScreen screen = LiveScreen();
  const CubePuzzleScreenStep step =
      UpdateCubePuzzleStart(screen, LiveConfirmPressed(), host);
  StoreScreen(screen);
  return Finished(step);
}

// decomp: MENUF.PRG _renderCubePuzzleEnd (src/MENU/MENUF.PRG/3B8.c:2477),
// 0x801080E4
int LiveUpdateCubePuzzleResults() {
  LiveCubePuzzleScreen host;
  CubePuzzleScreen screen = LiveScreen();
  std::uint32_t score = VS::State::scoreData.misc_score;
  const CubePuzzleScreenStep step = UpdateCubePuzzleResults(
      screen, LiveRating(), LiveClock(), LiveConfirmPressed(), score, host);
  StoreScreen(screen);
  VS::State::scoreData.misc_score = score;
  if (step == CubePuzzleScreenStep::kFinishedRankOutsideTable) {
    // Issue #409: no points for a rank the table does not have.
    RefuseCall("results", "points for a rank outside the table");
  }
  return Finished(step);
}

// decomp: MENUF.PRG _renderCubePuzzleQuit (src/MENU/MENUF.PRG/3B8.c:2566),
// 0x801083AC
int LiveUpdateCubePuzzleQuit() {
  LiveCubePuzzleScreen host;
  CubePuzzleScreen screen = LiveScreen();
  const CubePuzzleScreenStep step =
      UpdateCubePuzzleQuit(screen, LiveConfirmPressed(), host);
  StoreScreen(screen);
  return Finished(step);
}

void ResetLiveCubePuzzleLifecycleStatics() {
  iq_load = {};
  esc_load = {};
}
}  // namespace VS::Menu::Results
