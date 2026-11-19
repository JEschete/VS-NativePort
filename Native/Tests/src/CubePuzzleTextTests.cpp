#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Legacy/Psx.h"
#include "Menu/Results/CubePuzzle.h"
#include "Menu/Results/CubePuzzleText.hpp"
#include "Menu/Results/CubePuzzleTextLive.hpp"
#include "Menu/Results/Results.h"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Menu/Results/ResultsDrawingLive.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/Images.hpp"
#include "Render/Render.hpp"
#include "Render/TextureRegionDraw.hpp"
#include "Render/TextureRegionDrawLive.hpp"
#include "Render/UiPrimitives.hpp"
#include "State/StateFlags.h"

namespace {
namespace r = VS::Menu::Results;
namespace render = VS::Render;

enum class Kind : std::uint8_t { kFade, kFlash, kTime };

// One recorded draw; a time readout keeps its packed time in `index` and its
// timer in `level`.
struct Call {
  Kind kind = Kind::kFade;
  std::size_t index = 0;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t level = 0;
  r::CubeTextColors colors{};

  [[nodiscard]] bool Steps(std::uint8_t left, std::uint8_t right) const {
    const auto& [first, second] = colors;
    return first.step == left && second.step == right;
  }
  [[nodiscard]] bool At(std::int32_t at_x, std::int32_t at_y,
                        std::size_t at_index) const {
    return index == at_index && x == at_x && y == at_y;
  }
};

class RecordHost final : public r::CubePuzzleTextHost {
 public:
  std::vector<Call> calls;

  void DrawFadingTexture(r::ResultsPoint origin, std::size_t texture_index,
                         std::int32_t brightness) override {
    calls.push_back({.kind = Kind::kFade,
                     .index = texture_index,
                     .x = origin.x,
                     .y = origin.y,
                     .level = brightness});
  }
  void DrawFlashFade(r::ResultsPoint origin, std::size_t texture_index,
                     const r::CubeTextColors& colors) override {
    calls.push_back({.kind = Kind::kFlash,
                     .index = texture_index,
                     .x = origin.x,
                     .y = origin.y,
                     .colors = colors});
  }
  void DrawTime(r::ResultsPoint origin, std::int32_t timer,
                std::int32_t time) override {
    calls.push_back({.kind = Kind::kTime,
                     .index = static_cast<std::size_t>(time),
                     .x = origin.x,
                     .y = origin.y,
                     .level = timer});
  }
};

// Every entry is 16 wide except the ones the layouts centre on.
std::vector<render::TextureRegion> Atlas() {
  std::vector<render::TextureRegion> atlas(127, {.width = 16});
  atlas[r::kCubeTextTimeLabel].width = 40;
  atlas[r::kCubeTextEvolveOrDie].width = 50;
  atlas[r::kCubeTextEvolveOrDie + 1].width = 31;
  atlas[r::kCubeTextAverageTime].width = 91;
  atlas[r::kCubeTextFoodChain].width = 170;
  atlas[r::kCubeTextFoodChain + 1].width = 116;
  atlas[r::kCubeTextFirstRank + 3].width = 77;
  atlas[0].width = 9;
  atlas[126].width = 21;
  return atlas;
}

std::vector<render::TextureRegion> ShortAtlas(std::size_t entries) {
  auto atlas = Atlas();
  atlas.resize(entries);
  return atlas;
}

// 1:02.50.
constexpr r::CubePuzzleClock kClock{1, 2, 50};

TEST_CASE("F03052 giving up fades both halves in with the clamped timer",
          "[cube-text]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawGivingUp(atlas, 30, 90, host) == r::ResultsDrawResult::kDrawn);
  CHECK(r::DrawGivingUp(atlas, -5, 7, host) == r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 4);
  CHECK(host.calls[0].At(0, 30, 123));
  CHECK(host.calls[1].At(160, 30, 124));
  CHECK(host.calls[0].level == 64);
  CHECK(host.calls[2].At(0, -5, 123));
  CHECK(host.calls[3].level == 7);
}

TEST_CASE("F03052 giving up hides at no timer and refuses a short atlas",
          "[cube-text]") {
  RecordHost host;
  CHECK(r::DrawGivingUp(Atlas(), 30, 0, host) == r::ResultsDrawResult::kHidden);
  CHECK(r::DrawGivingUp(Atlas(), 30, std::numeric_limits<std::int32_t>::min(),
                        host) == r::ResultsDrawResult::kHidden);
  CHECK(r::DrawGivingUp(ShortAtlas(124), 30, 5, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawGivingUp(ShortAtlas(125), 30, 5, host) ==
        r::ResultsDrawResult::kDrawn);
  CHECK(host.calls.size() == 2);
}

TEST_CASE("F03053 chicken fades both halves in at x 0 and 160", "[cube-text]") {
  RecordHost host;
  CHECK(r::DrawChicken(Atlas(), 100, 64, host) == r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 2);
  CHECK(host.calls[0].At(0, 100, 125));
  CHECK(host.calls[1].At(160, 100, 126));
  CHECK(host.calls[1].level == 64);
  CHECK(host.calls[0].kind == Kind::kFade);
}

TEST_CASE("F03053 chicken hides below one and needs the last atlas entry",
          "[cube-text]") {
  RecordHost host;
  CHECK(r::DrawChicken(Atlas(), 100, -1, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(r::DrawChicken(ShortAtlas(126), 100, 1, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(host.calls.empty());
  CHECK(r::DrawChicken(Atlas(), 100, 1, host) == r::ResultsDrawResult::kDrawn);
  CHECK(host.calls.size() == 2);
}

TEST_CASE("F03054 evolve or die flashes both halves centred on x",
          "[cube-text]") {
  RecordHost host;
  CHECK(r::DrawEvolveOrDie(Atlas(), 160, 64, 20, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 2);
  // (50 + 31) >> 1 = 40.
  CHECK(host.calls[0].At(120, 64, 102));
  CHECK(host.calls[1].At(170, 64, 103));
  CHECK(host.calls[0].Steps(20, 20));
  CHECK(host.calls[1].Steps(20, 0));
  CHECK(host.calls[1].colors[0].r == 200);
  CHECK(host.calls[1].colors[1].g == 96);
}

TEST_CASE("F03054 evolve or die clamps its step and refuses a short atlas",
          "[cube-text]") {
  RecordHost host;
  CHECK(r::DrawEvolveOrDie(Atlas(), 160, 64, 300, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 2);
  CHECK(host.calls[0].Steps(64, 64));
  CHECK(r::DrawEvolveOrDie(Atlas(), 160, 64, 0, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(r::DrawEvolveOrDie(ShortAtlas(103), 160, 64, 5, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(host.calls.size() == 2);
}

TEST_CASE("F03055 food chain flashes both halves centred on x", "[cube-text]") {
  RecordHost host;
  CHECK(r::DrawFoodChain(Atlas(), 160, 100, 9, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 2);
  // (170 + 116) >> 1 = 143.
  CHECK(host.calls[0].At(17, 100, 105));
  CHECK(host.calls[1].At(187, 100, 106));
  CHECK(host.calls[0].Steps(9, 9));
  CHECK(host.calls[1].Steps(9, 0));
}

TEST_CASE("F03055 food chain wraps its centring and refuses a short atlas",
          "[cube-text]") {
  RecordHost host;
  const auto lowest = std::numeric_limits<std::int32_t>::min();
  CHECK(r::DrawFoodChain(Atlas(), lowest, 0, 64, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 2);
  CHECK(host.calls[0].x == std::numeric_limits<std::int32_t>::max() - 142);
  CHECK(r::DrawFoodChain(ShortAtlas(106), 0, 0, 64, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawFoodChain(Atlas(), 0, 0, -3, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(host.calls.size() == 2);
}

TEST_CASE("F03056 average time heading fades in centred by half its width",
          "[cube-text]") {
  RecordHost host;
  CHECK(r::DrawAverageTimeHeading(Atlas(), 128, 116, 70, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 1);
  CHECK(host.calls[0].At(83, 116, 104));
  CHECK(host.calls[0].level == 64);
  CHECK(host.calls[0].kind == Kind::kFade);
}

TEST_CASE(
    "F03056 average time heading hides below one and refuses a short "
    "atlas",
    "[cube-text]") {
  RecordHost host;
  CHECK(r::DrawAverageTimeHeading(Atlas(), 128, 116, 0, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(r::DrawAverageTimeHeading(Atlas(), 128, 116, -15, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(r::DrawAverageTimeHeading(ShortAtlas(104), 128, 116, 1, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(host.calls.empty());
}

TEST_CASE("F03057 par time packs minutes and seconds", "[cube-text]") {
  CHECK(r::PackedParTime(0) == 0);
  CHECK(r::PackedParTime(59) == 59 << 8);
  CHECK(r::PackedParTime(60) == 1 << 16);
  CHECK(r::PackedParTime(125) == ((2 << 16) | (5 << 8)));
  CHECK(r::PackedParTime(32767) == ((546 << 16) | (7 << 8)));
}

TEST_CASE("F03057 negative par time sign-extends its seconds over the minutes",
          "[cube-text]") {
  CHECK(r::PackedParTime(-1) == -256);
  CHECK(r::PackedParTime(-60) == static_cast<std::int32_t>(0xFFFF0000U));
  CHECK(r::PackedParTime(-61) == -256);
}

TEST_CASE("F03057 par time draws the packed time with the clamped timer",
          "[cube-text]") {
  RecordHost host;
  CHECK(r::DrawCubePuzzleParTime(192, 144, 70, 125, host) ==
        r::ResultsDrawResult::kDrawn);
  CHECK(r::DrawCubePuzzleParTime(192, 144, 0, 125, host) ==
        r::ResultsDrawResult::kHidden);
  REQUIRE(host.calls.size() == 1);
  CHECK(host.calls[0].At(192, 144, (2 << 16) | (5 << 8)));
  CHECK(host.calls[0].level == 64);
  CHECK(host.calls[0].kind == Kind::kTime);
}

TEST_CASE("F03058 rank flashes its title centred on x", "[cube-text]") {
  RecordHost host;
  CHECK(r::DrawCubePuzzleRank(Atlas(), 160, 124, 12, 3, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 1);
  CHECK(host.calls[0].At(122, 124, 110));
  CHECK(host.calls[0].Steps(12, 0));
  CHECK(host.calls[0].colors[1].r == 128);
  CHECK(host.calls[0].kind == Kind::kFlash);
}

TEST_CASE("F03058 rank reaches every atlas entry and refuses the rest",
          "[cube-text]") {
  RecordHost host;
  CHECK(r::DrawCubePuzzleRank(Atlas(), 0, 0, 64, 19, host) ==
        r::ResultsDrawResult::kDrawn);
  CHECK(r::DrawCubePuzzleRank(Atlas(), 0, 0, 64, -107, host) ==
        r::ResultsDrawResult::kDrawn);
  CHECK(r::DrawCubePuzzleRank(Atlas(), 0, 0, 64, 20, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawCubePuzzleRank(Atlas(), 0, 0, 64, -108, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawCubePuzzleRank(Atlas(), 0, 0, 64,
                              std::numeric_limits<std::int32_t>::max(),
                              host) == r::ResultsDrawResult::kInvalidTexture);
  REQUIRE(host.calls.size() == 2);
  CHECK(host.calls[0].At(-10, 0, 126));
  CHECK(host.calls[1].At(-4, 0, 0));
}

TEST_CASE("F03058 rank hides before reading its title", "[cube-text]") {
  RecordHost host;
  CHECK(r::DrawCubePuzzleRank(Atlas(), 0, 0, 0, 99, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(r::DrawCubePuzzleRank(Atlas(), 0, 0, -90, 0, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(host.calls.empty());
}

TEST_CASE("F03059 counted time stays zero until step 16 and then rises",
          "[cube-text]") {
  CHECK(r::CountedCubePuzzleTime(15, kClock) == 0);
  CHECK(r::CountedCubePuzzleTime(16, kClock) == 0);
  // 6250 hundredths * 24 / 48 = 3125: 0'31"25.
  CHECK(r::CountedCubePuzzleTime(40, kClock) == ((31 << 8) | 25));
  CHECK(r::CountedCubePuzzleTime(64, kClock) == ((1 << 16) | (2 << 8) | 50));
  const r::CubePuzzleClock largest{
      .minutes = 255, .seconds = 255, .hundredths = 255};
  // 1555755 hundredths: 259 minutes, 17 seconds and 55 hundredths.
  CHECK(r::CountedCubePuzzleTime(64, largest) ==
        ((259 << 16) | (17 << 8) | 55));
}

TEST_CASE("F03059 counting time fades its label and counts beside it",
          "[cube-text]") {
  RecordHost host;
  CHECK(r::DrawCountingCubePuzzleTime(Atlas(), 88, 64, 100, kClock, host) ==
        r::ResultsDrawResult::kDrawn);
  CHECK(r::DrawCountingCubePuzzleTime(Atlas(), 88, 64, 10, kClock, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 4);
  CHECK(host.calls[0].At(88, 64, 72));
  CHECK(host.calls[1].At(128, 66, (1 << 16) | (2 << 8) | 50));
  CHECK(host.calls[1].level == 64);
  CHECK(host.calls[3].At(128, 66, 0));
  CHECK(host.calls[3].level == 10);
}

TEST_CASE("F03059 counting time hides below one and refuses a short atlas",
          "[cube-text]") {
  RecordHost host;
  CHECK(r::DrawCountingCubePuzzleTime(Atlas(), 88, 64, 0, kClock, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(
      r::DrawCountingCubePuzzleTime(ShortAtlas(72), 88, 64, 5, kClock, host) ==
      r::ResultsDrawResult::kInvalidTexture);
  CHECK(host.calls.empty());
}

constexpr std::size_t kLivePrimitiveWords = 0x20000 / sizeof(u_long);
constexpr std::size_t kLiveOrderingWords = 0x2088 / sizeof(u_long);
constexpr std::size_t kLiveSceneHead = 16;

// A frame whose primitives land in `storage`, and the par time and puzzle
// clock the Live entries read, restored afterwards.
struct LiveCubeTextFixture {
  VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  void* saved_buffer = VS::Render::legacyPrimitiveBuffers[0];
  void* saved_ordering = VS::Render::legacyOrderingTables[0];
  int saved_framebuffer = VS::Render::frameBuf;
  std::int16_t saved_par = Menu_Results_puzzleParTime;
  State_StateFlags saved_flags = VS::State::stateFlags;
  std::vector<u_long> storage = std::vector<u_long>(kLivePrimitiveWords, 0);
  std::vector<u_long> ordering = std::vector<u_long>(kLiveOrderingWords, 0);

  LiveCubeTextFixture() { Reset(); }
  ~LiveCubeTextFixture() {
    VS::Render::FrameScratchpad() = saved_frame;
    VS::Render::legacyPrimitiveBuffers[0] = saved_buffer;
    VS::Render::legacyOrderingTables[0] = saved_ordering;
    VS::Render::frameBuf = saved_framebuffer;
    Menu_Results_puzzleParTime = saved_par;
    VS::State::stateFlags = saved_flags;
  }
  LiveCubeTextFixture(const LiveCubeTextFixture&) = delete;
  LiveCubeTextFixture& operator=(const LiveCubeTextFixture&) = delete;
  LiveCubeTextFixture(LiveCubeTextFixture&&) = delete;
  LiveCubeTextFixture& operator=(LiveCubeTextFixture&&) = delete;

  void Reset() {
    VS::Render::legacyOrderingTables[0] = ordering.data();
    VS::Render::legacyPrimitiveBuffers[0] = storage.data();
    VS::Render::frameBuf = 0;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    std::ranges::fill(storage, 0);
    VS::Render::FrameScratchpad().ordering_table =
        ordering.data() + kLiveSceneHead;
    VS::Render::FrameScratchpad().primitive_cursor = storage.data();
  }
  // The primitive words written since the last reset.
  [[nodiscard]] std::vector<u_long> Used() const {
    const auto* const cursor = static_cast<const u_long*>(
        VS::Render::FrameScratchpad().primitive_cursor);
    return {storage.data(), cursor};
  }
};

// Replays recorded draws through the live atlas and time draws.
void Replay(const RecordHost& host) {
  for (const auto& call : host.calls) {
    const auto index = static_cast<int>(call.index);
    if (call.kind == Kind::kFade) {
      r::LiveDrawFadingTexture(call.x, call.y, index, call.level);
    } else if (call.kind == Kind::kFlash) {
      const std::array<CVECTOR, 2> colors{{
          {call.colors[0].r, call.colors[0].g, call.colors[0].b,
           call.colors[0].step},
          {call.colors[1].r, call.colors[1].g, call.colors[1].b,
           call.colors[1].step},
      }};
      render::DrawLiveTextureFlashFade(Menu_Results_disMap, call.x, call.y,
                                       index, colors);
    } else {
      r::LiveDrawTime(call.x, call.y, call.level, index, 0);
    }
  }
}

TEST_CASE("F03052 live giving up draws both atlas halves", "[cube-text-live]") {
  LiveCubeTextFixture frame;
  r::LiveDrawGivingUp(64, 30);
  const auto live = frame.Used();
  REQUIRE(!live.empty());
  RecordHost host;
  r::DrawGivingUp(r::LiveResultsAtlas(), 64, 30, host);
  frame.Reset();
  Replay(host);
  CHECK(frame.Used() == live);
  frame.Reset();
  r::LiveDrawGivingUp(64, 0);
  CHECK(frame.Used().empty());
}

TEST_CASE("F03053 live chicken draws both atlas halves", "[cube-text-live]") {
  LiveCubeTextFixture frame;
  r::LiveDrawChicken(100, 5);
  const auto live = frame.Used();
  REQUIRE(!live.empty());
  RecordHost host;
  r::DrawChicken(r::LiveResultsAtlas(), 100, 5, host);
  frame.Reset();
  Replay(host);
  CHECK(frame.Used() == live);
}

TEST_CASE("F03054 live evolve or die flashes the banner", "[cube-text-live]") {
  LiveCubeTextFixture frame;
  r::LiveDrawEvolveOrDie(160, 64, 12);
  const auto live = frame.Used();
  REQUIRE(!live.empty());
  RecordHost host;
  r::DrawEvolveOrDie(r::LiveResultsAtlas(), 160, 64, 12, host);
  frame.Reset();
  Replay(host);
  CHECK(frame.Used() == live);
}

TEST_CASE("F03055 live food chain flashes the banner", "[cube-text-live]") {
  LiveCubeTextFixture frame;
  r::LiveDrawFoodChain(160, 100, 64);
  const auto live = frame.Used();
  REQUIRE(!live.empty());
  RecordHost host;
  r::DrawFoodChain(r::LiveResultsAtlas(), 160, 100, 64, host);
  frame.Reset();
  Replay(host);
  CHECK(frame.Used() == live);
}

TEST_CASE("F03056 live average time heading fades the heading in",
          "[cube-text-live]") {
  LiveCubeTextFixture frame;
  r::LiveDrawAverageTimeHeading(128, 116, 3);
  const auto live = frame.Used();
  REQUIRE(!live.empty());
  RecordHost host;
  r::DrawAverageTimeHeading(r::LiveResultsAtlas(), 128, 116, 3, host);
  frame.Reset();
  Replay(host);
  CHECK(frame.Used() == live);
}

TEST_CASE("F03057 live par time reads the module par time",
          "[cube-text-live]") {
  LiveCubeTextFixture frame;
  Menu_Results_puzzleParTime = 125;
  r::LiveDrawCubePuzzleParTime(192, 144, 70);
  const auto live = frame.Used();
  REQUIRE(!live.empty());
  frame.Reset();
  r::LiveDrawTime(192, 144, 64, (2 << 16) | (5 << 8), 0);
  CHECK(frame.Used() == live);
}

TEST_CASE("F03058 live rank draws the title and refuses one outside the atlas",
          "[cube-text-live]") {
  LiveCubeTextFixture frame;
  r::LiveDrawCubePuzzleRank(160, 124, 64, 15);
  const auto live = frame.Used();
  REQUIRE(!live.empty());
  RecordHost host;
  r::DrawCubePuzzleRank(r::LiveResultsAtlas(), 160, 124, 64, 15, host);
  frame.Reset();
  Replay(host);
  CHECK(frame.Used() == live);
  frame.Reset();
  r::LiveDrawCubePuzzleRank(160, 124, 64, 20);
  CHECK(frame.Used().empty());
}

TEST_CASE("F03059 live counting time reads the puzzle clock",
          "[cube-text-live]") {
  LiveCubeTextFixture frame;
  VS::State::stateFlags.puzzleTimerMins = 1;
  VS::State::stateFlags.puzzleTimerSeconds = 2;
  VS::State::stateFlags.puzzleTimerHundredths = 50;
  r::LiveDrawCountingCubePuzzleTime(88, 64, 40);
  const auto live = frame.Used();
  REQUIRE(!live.empty());
  RecordHost host;
  r::DrawCountingCubePuzzleTime(r::LiveResultsAtlas(), 88, 64, 40, kClock,
                                host);
  REQUIRE(host.calls.size() == 2);
  CHECK(host.calls[1].index == ((31U << 8) | 25U));
  frame.Reset();
  Replay(host);
  CHECK(frame.Used() == live);
}
}  // namespace
