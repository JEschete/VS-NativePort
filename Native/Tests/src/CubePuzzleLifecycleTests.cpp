#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Legacy/Psx.h"
#include "Menu/Results/CubePuzzleLifecycle.hpp"
#include "Menu/Results/CubePuzzleLifecycleLive.hpp"
#include "Menu/Results/CubePuzzleText.hpp"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Menu/Results/ResultsPalette.hpp"
#include "Render/DisplayControl.hpp"
#include "Render/TexturePagesLive.hpp"
extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/StatusEffects.h"
#include "DiscQueue.h"
#include "Field/Gimmicks.h"
#include "Field/Map.h"
#include "Field/Room.h"
#include "Menu/Results/CubePuzzle.h"
#include "Menu/Results/Results.h"
#include "Menu/Results/TimeAttack.h"
#include "State/StateFlags.h"
}

namespace {
namespace r = VS::Menu::Results;
namespace render = VS::Render;

using Clut = std::array<std::uint16_t, r::kResultsClutWords>;

// A TIM shaped like IQ.DIS's and ESC.DIS's: mode 8, a 16 x 16 palette at
// (0, 480) and a `width` x 256 image at (0, 0). Its colors and pixels count
// up from `seed`.
constexpr std::size_t kTimRows = 256;
constexpr std::size_t kTimPaletteColors = 256;

void PutWord(std::vector<std::byte>& bytes, std::size_t offset,
             std::uint32_t value) {
  for (std::size_t i = 0; i < 4; ++i) {
    bytes.at(offset + i) = static_cast<std::byte>((value >> (8 * i)) & 0xFF);
  }
}

void PutHalf(std::vector<std::byte>& bytes, std::size_t offset,
             std::uint16_t value) {
  bytes.at(offset) = static_cast<std::byte>(value & 0xFF);
  bytes.at(offset + 1) = static_cast<std::byte>(value >> 8);
}

[[nodiscard]] std::uint16_t HalfAt(std::span<const std::byte> bytes,
                                   std::size_t offset) {
  return static_cast<std::uint16_t>(
      std::to_integer<std::uint16_t>(bytes[offset]) |
      (std::to_integer<std::uint16_t>(bytes[offset + 1]) << 8));
}

// Writes the TIM at `at`; returns the palette's and the pixels' offsets.
struct TimLayout {
  std::size_t palette = 0;
  std::size_t pixels = 0;
};

TimLayout PutTim(std::vector<std::byte>& bytes, std::size_t at,
                 std::uint16_t width, std::uint16_t seed) {
  PutWord(bytes, at, 0x10);
  PutWord(bytes, at + 4, 8);
  const std::size_t clut = at + 8;
  PutWord(bytes, clut, static_cast<std::uint32_t>(12 + kTimPaletteColors * 2));
  PutHalf(bytes, clut + 4, 0);
  PutHalf(bytes, clut + 6, 480);
  PutHalf(bytes, clut + 8, 16);
  PutHalf(bytes, clut + 10, 16);
  for (std::size_t i = 0; i < kTimPaletteColors; ++i) {
    PutHalf(bytes, clut + 12 + (i * 2), static_cast<std::uint16_t>(seed + i));
  }
  const std::size_t image = clut + 12 + (kTimPaletteColors * 2);
  const std::size_t pixel_bytes = std::size_t{width} * 2 * kTimRows;
  PutWord(bytes, image, static_cast<std::uint32_t>(12 + pixel_bytes));
  PutHalf(bytes, image + 4, 0);
  PutHalf(bytes, image + 6, 0);
  PutHalf(bytes, image + 8, width);
  PutHalf(bytes, image + 10, kTimRows);
  for (std::size_t i = 0; i < pixel_bytes; ++i) {
    bytes.at(image + 12 + i) = static_cast<std::byte>((seed + i) & 0xFF);
  }
  return {.palette = clut + 12, .pixels = image + 12};
}

void PutRecord(std::vector<std::byte>& bytes, std::size_t index,
               r::CubePuzzleRecord record) {
  const std::size_t at = r::kCubePuzzleRecordOffset + (index * 8);
  PutHalf(bytes, at, static_cast<std::uint16_t>(record.zone_id));
  PutHalf(bytes, at + 2, static_cast<std::uint16_t>(record.map_id));
  PutHalf(bytes, at + 4, static_cast<std::uint16_t>(record.par_seconds));
  PutHalf(bytes, at + 6, static_cast<std::uint16_t>(record.rank_cap));
}

// An IQ.DIS of the real size: two 64-wide TIMs and the 64 records.
[[nodiscard]] std::vector<std::byte> IqAsset() {
  std::vector<std::byte> bytes(r::kCubePuzzleRecordOffset +
                               (r::kCubePuzzleRecordCount * 8));
  PutTim(bytes, 0, 64, 0x100);
  PutTim(bytes, r::kCubePuzzleTimStride, 64, 0x2000);
  return bytes;
}

struct Upload {
  render::ScreenRect area;
  std::vector<std::byte> pixels;
};

class FakeLoad final : public r::CubePuzzleLoadHost {
 public:
  std::vector<std::string> calls;
  std::vector<Upload> uploads;
  std::vector<r::CubePuzzleRefusal> refusals;
  std::vector<std::byte> asset;
  bool loaded = false;
  std::optional<r::CubePuzzleRoom> room;

  void ReserveTexturePages(std::uint32_t first_page, std::uint32_t page_count,
                           std::int32_t owner) override {
    calls.push_back(
        std::format("reserve {} {} {}", first_page, page_count, owner));
  }
  void BeginAssetRead(r::CubePuzzleAsset read) override {
    calls.push_back(read == r::CubePuzzleAsset::kIq ? "read iq" : "read esc");
  }
  bool AssetReadLoaded() override {
    calls.emplace_back("poll");
    return loaded;
  }
  std::span<std::byte> LoadedAsset() override { return asset; }
  void UploadImage(const render::ScreenRect& area,
                   std::span<const std::byte> pixels) override {
    calls.push_back(std::format("upload {} {} {} {}", area.x, area.y,
                                area.width, area.height));
    uploads.push_back({.area = area, .pixels = {pixels.begin(), pixels.end()}});
  }
  std::optional<r::CubePuzzleRoom> CurrentRoom() override {
    calls.emplace_back("room");
    return room;
  }
  void ReleaseAssetRead() override { calls.emplace_back("release"); }
  void FreeAssetBuffer() override { calls.emplace_back("free"); }
  void Refuse(r::CubePuzzleRefusal refusal) override {
    refusals.push_back(refusal);
  }
};

class FakeScreen final : public r::CubePuzzleScreenHost {
 public:
  std::vector<std::string> calls;

  void PlaySfx(std::int32_t group, std::int32_t effect) override {
    calls.push_back(std::format("play {:x} {:x}", group, effect));
  }
  void StopSfx(std::int32_t group, std::int32_t effect) override {
    calls.push_back(std::format("stop {:x} {:x}", group, effect));
  }
  void RetireTexturePages(std::uint32_t first_page, std::uint32_t page_count,
                          std::int32_t owner) override {
    calls.push_back(
        std::format("retire {} {} {}", first_page, page_count, owner));
  }
  void PulsePalette(std::int32_t blend_weight) override {
    calls.push_back(std::format("pulse {}", blend_weight));
  }
  void DrawMessage(r::CubePuzzleMessage message, r::ResultsPoint origin,
                   std::int32_t timer) override {
    calls.push_back(std::format("message {} {} {} {}",
                                static_cast<int>(message), origin.x, origin.y,
                                timer));
  }
  void DrawRank(r::ResultsPoint origin, std::int32_t timer,
                std::int32_t rank) override {
    calls.push_back(
        std::format("rank {} {} {} {}", origin.x, origin.y, timer, rank));
  }
  void DrawPrompt(r::ResultsPoint origin, std::int32_t slide_timer,
                  std::int32_t bounce_timer) override {
    calls.push_back(std::format("prompt {} {} {} {}", origin.x, origin.y,
                                slide_timer, bounce_timer));
  }
  [[nodiscard]] bool Has(const std::string& call) const {
    return std::ranges::find(calls, call) != calls.end();
  }
  [[nodiscard]] std::size_t Count(std::string_view prefix) const {
    return static_cast<std::size_t>(
        std::ranges::count_if(calls, [prefix](const std::string& call) {
          return call.starts_with(prefix);
        }));
  }
};

class RecordClut final : public r::ResultsClutTarget {
 public:
  std::vector<render::ScreenRect> areas;
  std::vector<std::uint16_t> colors;
  void Upload(const render::ScreenRect& area,
              std::span<const std::uint16_t> uploaded) override {
    areas.push_back(area);
    colors.assign(uploaded.begin(), uploaded.end());
  }
};

[[nodiscard]] Clut PulseClut() {
  Clut clut{};
  for (std::size_t i = 0; i < clut.size(); ++i) {
    clut.at(i) = static_cast<std::uint16_t>((0x1357 * (i % 11)) + (i * 3));
  }
  return clut;
}

[[nodiscard]] std::int32_t Mix(std::uint16_t weighted, std::uint16_t other,
                               std::int32_t weight, std::int32_t mask) {
  const std::int32_t a = weighted & mask;
  const std::int32_t b = other & mask;
  return (((a * weight) + (b * (16 - weight))) / 16) & mask;
}

[[nodiscard]] bool SameArea(const render::ScreenRect& area, std::int16_t x,
                            std::int16_t y, std::int16_t width,
                            std::int16_t height) {
  return area.x == x && area.y == y && area.width == width &&
         area.height == height;
}

// The first TIM's palette and pixels in IqAsset() and each image's 255 rows.
constexpr TimLayout kFirstTim{.palette = 20, .pixels = 20 + 512 + 12};
constexpr std::size_t kImageBytes = std::size_t{64} * 2 * 255;

// Both IQ.DIS images uploaded from the unmodified file, and the palette with
// its first two colors cleared in the asset, the upload and clutBuf.
void CheckIqUploads(const FakeLoad& host, std::span<const std::byte> original,
                    const Clut& clut) {
  REQUIRE(host.uploads.size() == 3);
  CHECK(std::ranges::equal(host.uploads[0].pixels,
                           original.subspan(kFirstTim.pixels, kImageBytes)));
  CHECK(std::ranges::equal(
      host.uploads[2].pixels,
      original.subspan(r::kCubePuzzleTimStride + kFirstTim.pixels,
                       kImageBytes)));
  CHECK(HalfAt(host.uploads[1].pixels, 0) == 0);
  CHECK(HalfAt(host.uploads[1].pixels, 2) == 0);
  CHECK(HalfAt(host.asset, kFirstTim.palette) == 0);
  CHECK(clut[0] == 0);
  CHECK(clut[1] == 0);
  CHECK(clut[2] == 0x102);
  CHECK(clut[159] == 0x100 + 159);
}

// Every screen field the last initializer step clears.
void CheckScreenCleared(const r::CubePuzzleScreen& screen) {
  CHECK(screen.load_step == 0);
  CHECK(screen.timer == 0);
  CHECK(screen.screen_state == 0);
  CHECK(screen.animation == 0);
  CHECK(screen.on_screen_map_completion == 0);
  CHECK(screen.on_screen_score == 0);
  CHECK(screen.buff_reel_selection == 0);
}

// ----- F03024 -----

TEST_CASE("F03024 pulse mixes the fourth row over the third into the fifth",
          "[cube-lifecycle]") {
  Clut clut = PulseClut();
  const Clut before = clut;
  RecordClut target;
  r::PulseCubePuzzleClut(clut, 5, target);
  for (std::size_t i = 0; i < 32; ++i) {
    const std::uint16_t weighted = before.at(96 + i);
    const std::uint16_t other = before.at(64 + i);
    const std::int32_t expected =
        Mix(weighted, other, 5, 0x1F) | Mix(weighted, other, 5, 0x3E0) |
        Mix(weighted, other, 5, 0x7C00) | (weighted & 0x8000);
    CHECK(clut.at(128 + i) == expected);
  }
  CHECK(std::ranges::equal(std::span(clut).first(128),
                           std::span(before).first(128)));
  REQUIRE(target.areas.size() == 1);
  CHECK(SameArea(target.areas[0], 768, 511, 160, 1));
  CHECK(std::ranges::equal(target.colors, clut));
}

TEST_CASE("F03024 pulse at full and zero weight keeps the fourth row's mask",
          "[cube-lifecycle]") {
  Clut clut = PulseClut();
  const Clut before = clut;
  RecordClut target;
  r::PulseCubePuzzleClut(clut, 16, target);
  for (std::size_t i = 0; i < 32; ++i) {
    CHECK(clut.at(128 + i) == before.at(96 + i));
  }
  r::PulseCubePuzzleClut(clut, 0, target);
  for (std::size_t i = 0; i < 32; ++i) {
    CHECK(clut.at(128 + i) ==
          ((before.at(64 + i) & 0x7FFF) | (before.at(96 + i) & 0x8000)));
  }
  CHECK(target.areas.size() == 2);
}

TEST_CASE("F03024 shared pulse refuses rows outside the palette",
          "[cube-lifecycle]") {
  Clut clut = PulseClut();
  const Clut before = clut;
  RecordClut target;
  CHECK_FALSE(r::PulseResultsClut(
      clut, {.weighted = 96, .complement = 64, .target = 129}, 8, target));
  CHECK_FALSE(r::PulseResultsClut(
      clut, {.weighted = 200, .complement = 64, .target = 128}, 8, target));
  CHECK(clut == before);
  CHECK(target.areas.empty());
}

// Restores clutBuf and its palette row in VRAM.
struct LiveClutFixture {
  Clut saved_clut{};
  Clut saved_vram{};
  RECT area{768, 511, 160, 1};
  LiveClutFixture() {
    std::ranges::copy(Menu_Results_clutBuf, saved_clut.begin());
    StoreImage(&area, reinterpret_cast<u_long*>(saved_vram.data()));
  }
  ~LiveClutFixture() {
    LoadImage(&area, reinterpret_cast<u_long*>(saved_vram.data()));
    std::ranges::copy(saved_clut, std::begin(Menu_Results_clutBuf));
  }
  LiveClutFixture(const LiveClutFixture&) = delete;
  LiveClutFixture& operator=(const LiveClutFixture&) = delete;
  LiveClutFixture(LiveClutFixture&&) = delete;
  LiveClutFixture& operator=(LiveClutFixture&&) = delete;
  [[nodiscard]] Clut Vram() {
    Clut vram{};
    StoreImage(&area, reinterpret_cast<u_long*>(vram.data()));
    return vram;
  }
};

TEST_CASE("F03024 live pulse blends clutBuf and uploads the palette row",
          "[cube-lifecycle-live]") {
  LiveClutFixture fixture;
  Clut expected = PulseClut();
  std::ranges::copy(expected, std::begin(Menu_Results_clutBuf));
  RecordClut target;
  r::PulseCubePuzzleClut(expected, 11, target);
  r::LivePulseCubePuzzleClut(11);
  CHECK(std::ranges::equal(Menu_Results_clutBuf, expected));
  CHECK(fixture.Vram() == expected);
}

// ----- F03046 -----

TEST_CASE("F03046 first step reserves two pages and starts reading IQ.DIS",
          "[cube-lifecycle]") {
  r::CubePuzzleScreen screen{.timer = 9};
  r::CubePuzzleRating rating{.par_seconds = 3, .rank_cap = 4};
  Clut clut{};
  FakeLoad host;
  CHECK(r::InitializeCubePuzzleStart(screen, rating, clut, host) ==
        r::CubePuzzleLoad::kLoading);
  CHECK(host.calls == std::vector<std::string>{"reserve 29 2 5", "read iq"});
  CHECK(screen.load_step == 1);
  CHECK(screen.timer == 9);
  // Still reading: nothing more happens.
  host.calls.clear();
  CHECK(r::InitializeCubePuzzleStart(screen, rating, clut, host) ==
        r::CubePuzzleLoad::kLoading);
  CHECK(host.calls == std::vector<std::string>{"poll"});
  CHECK(screen.load_step == 1);
  CHECK(rating.par_seconds == 3);
}

TEST_CASE(
    "F03046 loaded IQ.DIS uploads both images, the palette and the record",
    "[cube-lifecycle]") {
  r::CubePuzzleScreen screen{.load_step = 1};
  r::CubePuzzleRating rating{};
  Clut clut{};
  FakeLoad host;
  host.asset = IqAsset();
  PutRecord(host.asset, 3,
            {.zone_id = 7, .map_id = 9, .par_seconds = 1, .rank_cap = 2});
  PutRecord(host.asset, 5,
            {.zone_id = 4, .map_id = 12, .par_seconds = 45, .rank_cap = -3});
  PutRecord(host.asset, 9,
            {.zone_id = 4, .map_id = 12, .par_seconds = 99, .rank_cap = 6});
  host.loaded = true;
  host.room = r::CubePuzzleRoom{.zone_id = 4, .map_id = 12};
  const std::vector<std::byte> original = host.asset;
  CHECK(r::InitializeCubePuzzleStart(screen, rating, clut, host) ==
        r::CubePuzzleLoad::kLoading);
  CHECK(host.calls == std::vector<std::string>{"poll", "upload 832 256 64 255",
                                               "upload 768 511 160 1",
                                               "upload 896 256 64 255", "room",
                                               "release"});
  CHECK(host.refusals.empty());
  CheckIqUploads(host, original, clut);
  CHECK(rating.par_seconds == 45);
  CHECK(rating.rank_cap == -3);
  CHECK(screen.load_step == 2);
}

TEST_CASE("F03046 keeps the default rating without a record for the room",
          "[cube-lifecycle]") {
  Clut clut{};
  r::CubePuzzleScreen screen{.load_step = 1};
  r::CubePuzzleRating rating{.par_seconds = 5, .rank_cap = 9};
  FakeLoad host;
  host.asset = IqAsset();
  host.loaded = true;
  host.room = r::CubePuzzleRoom{.zone_id = 1, .map_id = 1};
  (void)r::InitializeCubePuzzleStart(screen, rating, clut, host);
  CHECK(rating.par_seconds == 600);
  CHECK(rating.rank_cap == 1);
  CHECK(host.refusals.empty());
}

TEST_CASE("F03046 refuses the record lookup without a room",
          "[cube-lifecycle]") {
  Clut clut{};
  r::CubePuzzleScreen screen{.load_step = 1};
  r::CubePuzzleRating rating{.par_seconds = 5, .rank_cap = 9};
  FakeLoad host;
  host.asset = IqAsset();
  host.loaded = true;
  (void)r::InitializeCubePuzzleStart(screen, rating, clut, host);
  CHECK(rating.par_seconds == 600);
  CHECK(host.refusals ==
        std::vector<r::CubePuzzleRefusal>{r::CubePuzzleRefusal::kRoom});
}

TEST_CASE("F03046 refuses the second image and the records of a one-TIM asset",
          "[cube-lifecycle]") {
  Clut clut{};
  r::CubePuzzleScreen screen{.load_step = 1};
  r::CubePuzzleRating rating{.par_seconds = 5, .rank_cap = 9};
  FakeLoad host;
  host.asset = IqAsset();
  host.asset.resize(r::kCubePuzzleTimStride);
  host.loaded = true;
  host.room = r::CubePuzzleRoom{};
  (void)r::InitializeCubePuzzleStart(screen, rating, clut, host);
  CHECK(host.uploads.size() == 2);
  CHECK(host.refusals ==
        std::vector<r::CubePuzzleRefusal>{r::CubePuzzleRefusal::kImage,
                                          r::CubePuzzleRefusal::kRecords});
  CHECK(rating.par_seconds == 600);
  CHECK(rating.rank_cap == 1);
  CHECK(host.calls.back() == "release");
  CHECK(screen.load_step == 2);
}

TEST_CASE("F03046 last step frees IQ.DIS, clears the screen and restarts",
          "[cube-lifecycle]") {
  r::CubePuzzleScreen screen{.load_step = 2,
                             .timer = 5,
                             .screen_state = 3,
                             .animation = 77,
                             .on_screen_map_completion = 8,
                             .on_screen_score = 9,
                             .buff_reel_selection = 10};
  r::CubePuzzleRating rating{.par_seconds = 45, .rank_cap = 2};
  Clut clut{};
  FakeLoad host;
  CHECK(r::InitializeCubePuzzleStart(screen, rating, clut, host) ==
        r::CubePuzzleLoad::kFinished);
  CHECK(host.calls == std::vector<std::string>{"free"});
  CheckScreenCleared(screen);
  CHECK(rating.par_seconds == 45);
  // Entering again starts a new read.
  host.calls.clear();
  CHECK(r::InitializeCubePuzzleStart(screen, rating, clut, host) ==
        r::CubePuzzleLoad::kLoading);
  CHECK(host.calls == std::vector<std::string>{"reserve 29 2 5", "read iq"});
  // Any step past the first two frees, as the original's else branch did.
  screen.load_step = -4;
  host.calls.clear();
  CHECK(r::InitializeCubePuzzleStart(screen, rating, clut, host) ==
        r::CubePuzzleLoad::kFinished);
  CHECK(host.calls == std::vector<std::string>{"free"});
}

// Saves and restores the module state the Live initializers and updates use.
struct LiveScreenFixture {
  int saved_step = Battle_unusedSubmenuState;
  int saved_timer = Menu_Results_screenTimer;
  int saved_state = Menu_Results_screenState;
  u_int saved_animation = Menu_Results_elementAnimationState;
  int saved_completion = Menu_Results_onScreenMapCompletion;
  int saved_score = Menu_Results_onScreenScore;
  u_int saved_reel = Menu_Results_buffReelSelection;
  std::int16_t saved_par = Menu_Results_puzzleParTime;
  std::int16_t saved_cap = Menu_Results_puzzleRankCap;
  State_StateFlags saved_flags = Audio_Seq_unusedStateFlags;
  u_int saved_misc = Audio_Seq_unusedScoredata.miscScore;
  LiveClutFixture clut;

  LiveScreenFixture() { r::ResetLiveCubePuzzleLifecycleStatics(); }
  ~LiveScreenFixture() {
    r::ResetLiveCubePuzzleLifecycleStatics();
    Battle_unusedSubmenuState = saved_step;
    Menu_Results_screenTimer = saved_timer;
    Menu_Results_screenState = saved_state;
    Menu_Results_elementAnimationState = saved_animation;
    Menu_Results_onScreenMapCompletion = saved_completion;
    Menu_Results_onScreenScore = saved_score;
    Menu_Results_buffReelSelection = saved_reel;
    Menu_Results_puzzleParTime = saved_par;
    Menu_Results_puzzleRankCap = saved_cap;
    Audio_Seq_unusedStateFlags = saved_flags;
    Audio_Seq_unusedScoredata.miscScore = saved_misc;
  }
  LiveScreenFixture(const LiveScreenFixture&) = delete;
  LiveScreenFixture& operator=(const LiveScreenFixture&) = delete;
  LiveScreenFixture(LiveScreenFixture&&) = delete;
  LiveScreenFixture& operator=(LiveScreenFixture&&) = delete;
};

TEST_CASE(
    "F03046 live poll without a read keeps the default rating and finishes",
    "[cube-lifecycle-live]") {
  LiveScreenFixture fixture;
  Battle_unusedSubmenuState = 1;
  Menu_Results_puzzleParTime = 12;
  Menu_Results_puzzleRankCap = 13;
  Menu_Results_screenTimer = 44;
  CHECK(r::LiveInitializeCubePuzzleStart() == 0);
  CHECK(Battle_unusedSubmenuState == 2);
  CHECK(Menu_Results_puzzleParTime == 600);
  CHECK(Menu_Results_puzzleRankCap == 1);
  CHECK(r::LiveInitializeCubePuzzleStart() == 1);
  CHECK(Battle_unusedSubmenuState == 0);
  CHECK(Menu_Results_screenTimer == 0);
}

TEST_CASE("F03046 live start without heap room cancels the read",
          "[cube-lifecycle-live]") {
  LiveScreenFixture fixture;
  alignas(16) std::array<std::byte, 4096> arena{};
  const VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  Battle_unusedSubmenuState = 0;
  CHECK(r::LiveInitializeCubePuzzleStart() == 0);
  CHECK(Battle_unusedSubmenuState == 1);
  // No buffer, so no request: the next poll goes on without the asset.
  CHECK(r::LiveInitializeCubePuzzleStart() == 0);
  CHECK(Battle_unusedSubmenuState == 2);
  CHECK(r::LiveInitializeCubePuzzleStart() == 1);
  CHECK(Battle_unusedSubmenuState == 0);
  (void)render::RetireLiveTexturePageRange(r::kCubePuzzleFirstPage,
                                           r::kCubePuzzleStartPages,
                                           r::kCubePuzzlePageOwner);
}

constexpr std::size_t kLiveArenaBytes = 0x20000;
constexpr std::int16_t kImageAreaWidth = 128;
constexpr std::size_t kImageAreaColumns{kImageAreaWidth};
constexpr std::size_t kImageRows{r::kCubePuzzleImageRows};
constexpr std::size_t kImageAreaPixels = kImageAreaColumns * kImageRows;

struct LiveReadArena {
  alignas(16) std::array<std::byte, kLiveArenaBytes> bytes{};
};

// Swaps in a heap with room for IQ.DIS, an idle disc queue, fresh texture
// pages and a one-room minimap scene; restores them and the image VRAM.
struct LiveReadFixture {
  std::unique_ptr<LiveReadArena> arena = std::make_unique<LiveReadArena>();
  VS::Core::Memory::ScopedLiveGameHeap heap{arena->bytes};
  render::TexturePageTable saved_pages = render::LiveTexturePages();
  decltype(std::to_array(Content_discRequestSlots)) saved_slots =
      std::to_array(Content_discRequestSlots);
  Content_DiscRequestCounts saved_counts = Content_discRequestCounts;
  Content_DiscDrive saved_drive = Content_discDriveState;
  Field_MapScene* saved_scene = Field_roomData.minimapScene;
  u_int saved_scene_length = Field_roomData.header.minimapSceneLength;
  RECT images{r::kCubePuzzleImageX, r::kCubePuzzleImageY, kImageAreaWidth,
              r::kCubePuzzleImageRows};
  std::vector<std::uint16_t> saved_vram =
      std::vector<std::uint16_t>(kImageAreaPixels);
  // The room count, then the first room: visited, data address, zone and map.
  std::array<std::uint32_t, 4> scene{1, 0, 0, 0};

  LiveReadFixture() {
    render::InitializeLiveTexturePages();
    std::fill_n(Content_discRequestSlots, saved_slots.size(),
                Content_DiscQueueSlot{});
    Content_discRequestCounts = {};
    Content_discDriveState.state =
        static_cast<char>(Content_DiscDriveState_Idle);
    StoreImage(&images, reinterpret_cast<u_long*>(saved_vram.data()));
    DrawSync(0);
  }
  ~LiveReadFixture() {
    LoadImage(&images, reinterpret_cast<u_long*>(saved_vram.data()));
    DrawSync(0);
    Field_roomData.minimapScene = saved_scene;
    Field_roomData.header.minimapSceneLength = saved_scene_length;
    std::ranges::copy(saved_slots, std::begin(Content_discRequestSlots));
    Content_discRequestCounts = saved_counts;
    Content_discDriveState = saved_drive;
    render::LiveTexturePages() = saved_pages;
  }
  LiveReadFixture(const LiveReadFixture&) = delete;
  LiveReadFixture& operator=(const LiveReadFixture&) = delete;
  LiveReadFixture(LiveReadFixture&&) = delete;
  LiveReadFixture& operator=(LiveReadFixture&&) = delete;

  void UseRoom(std::int16_t zone_id, std::int16_t map_id) {
    scene[3] = static_cast<std::uint16_t>(zone_id) |
               (std::uint32_t{static_cast<std::uint16_t>(map_id)} << 16);
    Field_roomData.minimapScene =
        reinterpret_cast<Field_MapScene*>(scene.data());
    Field_roomData.header.minimapSceneLength = sizeof scene;
  }
  // The one request the initializer queued.
  [[nodiscard]] static Content_DiscQueueSlot* Pending() {
    auto* const end = std::end(Content_discRequestSlots);
    auto* const pending =
        std::find_if(std::begin(Content_discRequestSlots), end,
                     [](const Content_DiscQueueSlot& slot) {
                       return slot.state != Content_DiscRequestState_Free;
                     });
    return pending == end ? nullptr : pending;
  }
  [[nodiscard]] std::vector<std::uint16_t> ImageRow(std::int16_t x) const {
    RECT row{x, r::kCubePuzzleImageY, 64, 1};
    std::vector<std::uint16_t> pixels(64);
    StoreImage(&row, reinterpret_cast<u_long*>(pixels.data()));
    DrawSync(0);
    return pixels;
  }
};

[[nodiscard]] std::vector<std::uint16_t> AssetRow(
    std::span<const std::byte> asset, std::size_t offset) {
  std::vector<std::uint16_t> pixels(64);
  for (std::size_t i = 0; i < pixels.size(); ++i) {
    pixels[i] = HalfAt(asset, offset + (i * 2));
  }
  return pixels;
}

// Starts the IQ.DIS read from the first step; returns its queued request.
[[nodiscard]] Content_DiscQueueSlot* StartLiveIqRead() {
  const Content_DiscFile& file =
      Menu_Results_kDisFiles[Menu_Results_DisFile_Iq];
  CHECK(r::LiveInitializeCubePuzzleStart() == 0);
  CHECK(Battle_unusedSubmenuState == 1);
  Content_DiscQueueSlot* const request = LiveReadFixture::Pending();
  REQUIRE(request != nullptr);
  CHECK(request->cdFile.lba == file.lba);
  CHECK(request->cdFile.size == file.size);
  REQUIRE(request->vram != nullptr);
  return request;
}

// Completes the read with an IQ.DIS whose third record is zone 4 map 12's;
// returns the asset.
std::vector<std::byte> CompleteLiveIqRead(Content_DiscQueueSlot& request) {
  const Content_DiscFile& file =
      Menu_Results_kDisFiles[Menu_Results_DisFile_Iq];
  std::vector<std::byte> asset = IqAsset();
  asset.resize(static_cast<std::size_t>(file.size));
  PutRecord(asset, 1,
            {.zone_id = 4, .map_id = 13, .par_seconds = 8, .rank_cap = 9});
  PutRecord(asset, 2,
            {.zone_id = 4, .map_id = 12, .par_seconds = 45, .rank_cap = 3});
  std::ranges::copy(asset, static_cast<std::byte*>(request.vram));
  request.state = Content_DiscRequestState_Loaded;
  return asset;
}

// clutBuf, the palette row and the image VRAM hold IQ.DIS's palette, its
// first two colors cleared, and both images.
void CheckLiveIqUploads(LiveScreenFixture& screen, const LiveReadFixture& read,
                        std::span<const std::byte> asset) {
  CHECK(Menu_Results_clutBuf[0] == 0);
  CHECK(Menu_Results_clutBuf[1] == 0);
  CHECK(Menu_Results_clutBuf[2] == 0x102);
  CHECK(Menu_Results_clutBuf[159] == 0x100 + 159);
  CHECK(std::ranges::equal(screen.clut.Vram(), Menu_Results_clutBuf));
  CHECK(read.ImageRow(r::kCubePuzzleImageX) ==
        AssetRow(asset, kFirstTim.pixels));
  CHECK(read.ImageRow(r::kCubePuzzleImageX + r::kCubePuzzleImageStep) ==
        AssetRow(asset, r::kCubePuzzleTimStride + kFirstTim.pixels));
}

TEST_CASE("F03046 live start reads IQ.DIS and takes the minimap room's record",
          "[cube-lifecycle-live]") {
  LiveScreenFixture screen;
  LiveReadFixture read;
  read.UseRoom(4, 12);
  Battle_unusedSubmenuState = 0;
  Menu_Results_screenTimer = 7;
  Content_DiscQueueSlot* const request = StartLiveIqRead();
  // Still reading.
  CHECK(r::LiveInitializeCubePuzzleStart() == 0);
  CHECK(Battle_unusedSubmenuState == 1);
  const std::vector<std::byte> asset = CompleteLiveIqRead(*request);
  CHECK(r::LiveInitializeCubePuzzleStart() == 0);
  CHECK(Battle_unusedSubmenuState == 2);
  CHECK(request->state != Content_DiscRequestState_Loaded);
  CHECK(Menu_Results_puzzleParTime == 45);
  CHECK(Menu_Results_puzzleRankCap == 3);
  CheckLiveIqUploads(screen, read, asset);
  CHECK(Menu_Results_screenTimer == 7);
}

TEST_CASE(
    "F03046 live start frees IQ.DIS and reads into the same block on "
    "reentry",
    "[cube-lifecycle-live]") {
  LiveScreenFixture screen;
  LiveReadFixture read;
  read.UseRoom(4, 12);
  Battle_unusedSubmenuState = 0;
  Menu_Results_screenTimer = 7;
  Content_DiscQueueSlot* const request = StartLiveIqRead();
  void* const buffer = request->vram;
  (void)CompleteLiveIqRead(*request);
  CHECK(r::LiveInitializeCubePuzzleStart() == 0);
  CHECK(r::LiveInitializeCubePuzzleStart() == 1);
  CHECK(Battle_unusedSubmenuState == 0);
  CHECK(Menu_Results_screenTimer == 0);
  // The buffer was released: entering again reads into the same block.
  CHECK(r::LiveInitializeCubePuzzleStart() == 0);
  Content_DiscQueueSlot* const again = LiveReadFixture::Pending();
  REQUIRE(again != nullptr);
  CHECK(again->vram == buffer);
  Battle_unusedSubmenuState = 2;
  CHECK(r::LiveInitializeCubePuzzleStart() == 1);
}

TEST_CASE(
    "F03046 live start refuses the record lookup without a minimap "
    "room",
    "[cube-lifecycle-live]") {
  LiveScreenFixture screen;
  LiveReadFixture read;
  read.UseRoom(4, 12);
  read.scene[0] = 0;
  Battle_unusedSubmenuState = 0;
  CHECK(r::LiveInitializeCubePuzzleStart() == 0);
  Content_DiscQueueSlot* const request = LiveReadFixture::Pending();
  REQUIRE(request != nullptr);
  std::vector<std::byte> asset = IqAsset();
  PutRecord(asset, 0,
            {.zone_id = 4, .map_id = 12, .par_seconds = 45, .rank_cap = 3});
  std::ranges::copy(asset, static_cast<std::byte*>(request->vram));
  request->state = Content_DiscRequestState_Loaded;
  Menu_Results_puzzleParTime = 12;
  CHECK(r::LiveInitializeCubePuzzleStart() == 0);
  CHECK(Menu_Results_puzzleParTime == 600);
  CHECK(Menu_Results_puzzleRankCap == 1);
  CHECK(r::LiveInitializeCubePuzzleStart() == 1);
}

// ----- F03047 -----

TEST_CASE("F03047 results set the maximum clock and clear the flag first",
          "[cube-lifecycle]") {
  r::CubePuzzleScreen screen{};
  r::CubePuzzleRating rating{};
  r::CubePuzzleTimer timer{
      .clock = {.minutes = 3, .seconds = 4, .hundredths = 5},
      .show_maximum = 1};
  Clut clut{};
  FakeLoad host;
  CHECK(r::InitializeCubePuzzleResults(screen, rating, timer, clut, host) ==
        r::CubePuzzleLoad::kLoading);
  CHECK(timer.clock.minutes == 59);
  CHECK(timer.clock.seconds == 59);
  CHECK(timer.clock.hundredths == 99);
  CHECK(timer.show_maximum == 0);
  CHECK(host.calls == std::vector<std::string>{"reserve 29 2 5", "read iq"});
}

TEST_CASE("F03047 results keep the clock while the flag is clear",
          "[cube-lifecycle]") {
  r::CubePuzzleScreen screen{.load_step = 1};
  r::CubePuzzleRating rating{};
  r::CubePuzzleTimer timer{
      .clock = {.minutes = 3, .seconds = 4, .hundredths = 5}};
  Clut clut{};
  FakeLoad host;
  CHECK(r::InitializeCubePuzzleResults(screen, rating, timer, clut, host) ==
        r::CubePuzzleLoad::kLoading);
  CHECK(timer.clock.minutes == 3);
  CHECK(timer.clock.seconds == 4);
  CHECK(timer.clock.hundredths == 5);
  CHECK(host.calls == std::vector<std::string>{"poll"});
}

TEST_CASE("F03047 results apply the flag at every step", "[cube-lifecycle]") {
  r::CubePuzzleScreen screen{.load_step = 2, .timer = 7};
  r::CubePuzzleRating rating{};
  r::CubePuzzleTimer timer{.clock = {}, .show_maximum = 0xFF};
  Clut clut{};
  FakeLoad host;
  CHECK(r::InitializeCubePuzzleResults(screen, rating, timer, clut, host) ==
        r::CubePuzzleLoad::kFinished);
  CHECK(timer.clock.minutes == 59);
  CHECK(timer.show_maximum == 0);
  CHECK(host.calls == std::vector<std::string>{"free"});
  CHECK(screen.timer == 0);
}

TEST_CASE("F03047 live results write the maximum clock to the state flags",
          "[cube-lifecycle-live]") {
  LiveScreenFixture fixture;
  Battle_unusedSubmenuState = 2;
  Audio_Seq_unusedStateFlags.puzzleTimerMins = 1;
  Audio_Seq_unusedStateFlags.puzzleTimerSeconds = 2;
  Audio_Seq_unusedStateFlags.puzzleTimerHundredths = 3;
  Audio_Seq_unusedStateFlags.puzzleSetMaxTimer = 1;
  CHECK(r::LiveInitializeCubePuzzleResults() == 1);
  CHECK(Audio_Seq_unusedStateFlags.puzzleTimerMins == 59);
  CHECK(Audio_Seq_unusedStateFlags.puzzleTimerSeconds == 59);
  CHECK(Audio_Seq_unusedStateFlags.puzzleTimerHundredths == 99);
  CHECK(Audio_Seq_unusedStateFlags.puzzleSetMaxTimer == 0);
  CHECK(Battle_unusedSubmenuState == 0);
}

// ----- F03048 -----

TEST_CASE("F03048 quit reserves one page and reads ESC.DIS",
          "[cube-lifecycle]") {
  r::CubePuzzleScreen screen{};
  Clut clut{};
  FakeLoad host;
  CHECK(r::InitializeCubePuzzleQuit(screen, clut, host) ==
        r::CubePuzzleLoad::kLoading);
  CHECK(host.calls == std::vector<std::string>{"reserve 29 1 5", "read esc"});
  CHECK(screen.load_step == 1);
}

TEST_CASE(
    "F03048 loaded ESC.DIS uploads one image and the palette without "
    "records",
    "[cube-lifecycle]") {
  r::CubePuzzleScreen screen{.load_step = 1};
  Clut clut{};
  FakeLoad host;
  host.asset.resize(r::kCubePuzzleTimStride);
  PutTim(host.asset, 0, 64, 0x300);
  host.loaded = true;
  CHECK(r::InitializeCubePuzzleQuit(screen, clut, host) ==
        r::CubePuzzleLoad::kLoading);
  CHECK(host.calls == std::vector<std::string>{"poll", "upload 832 256 64 255",
                                               "upload 768 511 160 1",
                                               "release"});
  CHECK(host.refusals.empty());
  CHECK(clut[1] == 0);
  CHECK(clut[2] == 0x302);
  CHECK(screen.load_step == 2);
}

TEST_CASE("F03048 quit refuses a short palette and an unparsable image",
          "[cube-lifecycle]") {
  Clut clut{};
  SECTION("short palette") {
    r::CubePuzzleScreen screen{.load_step = 1};
    FakeLoad host;
    host.asset.resize(r::kCubePuzzleTimStride);
    PutTim(host.asset, 0, 64, 0x300);
    // A 16 x 8 palette: 128 colors, short of 160. The image follows it.
    std::vector<std::byte> tim(r::kCubePuzzleTimStride);
    PutWord(tim, 0, 0x10);
    PutWord(tim, 4, 8);
    PutWord(tim, 8, 12 + 256);
    PutHalf(tim, 16, 16);
    PutHalf(tim, 18, 8);
    PutWord(tim, 8 + 268, 12 + (64 * 2 * 256));
    PutHalf(tim, 8 + 268 + 8, 64);
    PutHalf(tim, 8 + 268 + 10, 256);
    host.asset = tim;
    host.loaded = true;
    (void)r::InitializeCubePuzzleQuit(screen, clut, host);
    CHECK(host.refusals ==
          std::vector<r::CubePuzzleRefusal>{r::CubePuzzleRefusal::kPalette});
    CHECK(host.uploads.size() == 1);
    CHECK(clut == Clut{});
  }
  SECTION("unparsable image") {
    r::CubePuzzleScreen screen{.load_step = 1};
    FakeLoad host;
    host.asset.assign(64, std::byte{0xFF});
    host.loaded = true;
    (void)r::InitializeCubePuzzleQuit(screen, clut, host);
    CHECK(host.refusals ==
          std::vector<r::CubePuzzleRefusal>{r::CubePuzzleRefusal::kImage});
    CHECK(host.uploads.empty());
    CHECK(screen.load_step == 2);
  }
}

TEST_CASE("F03048 quit last step frees ESC.DIS and clears the screen",
          "[cube-lifecycle]") {
  r::CubePuzzleScreen screen{.load_step = 2, .timer = 3, .animation = 4};
  Clut clut{};
  FakeLoad host;
  CHECK(r::InitializeCubePuzzleQuit(screen, clut, host) ==
        r::CubePuzzleLoad::kFinished);
  CHECK(host.calls == std::vector<std::string>{"free"});
  CHECK(screen.timer == 0);
  CHECK(screen.animation == 0);
}

TEST_CASE("F03048 live quit without a read leaves the rating",
          "[cube-lifecycle-live]") {
  LiveScreenFixture fixture;
  Battle_unusedSubmenuState = 1;
  Menu_Results_puzzleParTime = 12;
  CHECK(r::LiveInitializeCubePuzzleQuit() == 0);
  CHECK(Battle_unusedSubmenuState == 2);
  CHECK(Menu_Results_puzzleParTime == 12);
  CHECK(r::LiveInitializeCubePuzzleQuit() == 1);
  CHECK(Battle_unusedSubmenuState == 0);
}

// ----- F03049 -----

TEST_CASE("F03049 start waits while the screen state is set",
          "[cube-lifecycle]") {
  r::CubePuzzleScreen screen{.timer = 0, .screen_state = 1};
  FakeScreen host;
  CHECK(r::UpdateCubePuzzleStart(screen, true, host) ==
        r::CubePuzzleScreenStep::kShowing);
  CHECK(host.calls.empty());
  CHECK(screen.timer == 0);
}

TEST_CASE("F03049 start reveals its messages and sounds on schedule",
          "[cube-lifecycle]") {
  r::CubePuzzleScreen screen{};
  FakeScreen host;
  (void)r::UpdateCubePuzzleStart(screen, false, host);
  CHECK(host.calls == std::vector<std::string>{
                          "play 7e 7d", "message 0 160 64 0",
                          "message 1 128 116 -15", "message 2 192 144 -30",
                          "prompt 214 187 -30 0"});
  CHECK(screen.timer == 1);
  for (int frame = 1; frame < 31; ++frame) {
    (void)r::UpdateCubePuzzleStart(screen, false, host);
  }
  CHECK(host.Count("play") == 2);
  CHECK(host.Has("prompt 214 187 0 30"));
  screen.timer = 0x7FFF;
  (void)r::UpdateCubePuzzleStart(screen, false, host);
  CHECK(screen.timer == 0x7FFF);
}

TEST_CASE("F03049 start skips to its end on Circle then finishes",
          "[cube-lifecycle]") {
  r::CubePuzzleScreen screen{.timer = 40};
  FakeScreen host;
  CHECK(r::UpdateCubePuzzleStart(screen, true, host) ==
        r::CubePuzzleScreenStep::kShowing);
  CHECK(screen.timer == 78);
  CHECK(host.calls.back() == "stop 7e 0");
  host.calls.clear();
  screen.timer = 77;
  CHECK(r::UpdateCubePuzzleStart(screen, true, host) ==
        r::CubePuzzleScreenStep::kFinished);
  CHECK(host.calls.back() == "retire 29 2 5");
  CHECK(host.Count("stop 7e 0") == 1);
}

TEST_CASE("F03049 live start waits while the screen state is set",
          "[cube-lifecycle-live]") {
  LiveScreenFixture fixture;
  Menu_Results_screenState = 2;
  Menu_Results_screenTimer = 12;
  CHECK(r::LiveUpdateCubePuzzleStart() == 0);
  CHECK(Menu_Results_screenTimer == 12);
}

// ----- F03050 -----

TEST_CASE("F03050 rank counts started two-second steps over par",
          "[cube-lifecycle]") {
  const r::CubePuzzleRating rating{.par_seconds = 30, .rank_cap = 2};
  CHECK(r::CubePuzzleRank({.minutes = 0, .seconds = 30, .hundredths = 0},
                          rating) == 13);
  CHECK(r::CubePuzzleRank({.minutes = 0, .seconds = 30, .hundredths = 1},
                          rating) == 12);
  CHECK(r::CubePuzzleRank({.minutes = 0, .seconds = 32, .hundredths = 0},
                          rating) == 12);
  CHECK(r::CubePuzzleRank({.minutes = 0, .seconds = 32, .hundredths = 1},
                          rating) == 11);
  CHECK(r::CubePuzzleRank({.minutes = 59, .seconds = 59, .hundredths = 99},
                          rating) == 0);
  // Over par the rank stays within the table; at par it does not.
  CHECK(r::CubePuzzleRank({.minutes = 0, .seconds = 31, .hundredths = 0},
                          {.par_seconds = 30, .rank_cap = -9}) == 15);
  CHECK(r::CubePuzzleRank({}, {.par_seconds = 30, .rank_cap = 20}) == -5);
  CHECK(r::CubePuzzleRank({}, {.par_seconds = 30, .rank_cap = -2}) == 17);
}

TEST_CASE("F03050 results pulse the palette before the screen-state wait",
          "[cube-lifecycle]") {
  r::CubePuzzleScreen screen{.timer = 5, .screen_state = 1, .animation = 0xFE0};
  FakeScreen host;
  std::uint32_t score = 7;
  CHECK(r::UpdateCubePuzzleResults(screen, {}, {}, true, score, host) ==
        r::CubePuzzleScreenStep::kShowing);
  CHECK(host.calls ==
        std::vector<std::string>{
            std::format("pulse {}", (VS::Core::Math::Sine(0xFE0) / 512) + 8)});
  CHECK(screen.animation == 0x20);
  CHECK(screen.timer == 5);
  CHECK(score == 7);
  CHECK(r::CubePuzzlePulseWeight(1024) == 16);
  CHECK(r::CubePuzzlePulseWeight(3072) == 0);
  CHECK(r::CubePuzzlePulseWeight(0) == 8);
}

TEST_CASE("F03050 results play the counting sounds on schedule",
          "[cube-lifecycle]") {
  r::CubePuzzleScreen screen{};
  FakeScreen host;
  std::uint32_t score = 0;
  const r::CubePuzzleClock clock{.minutes = 1, .seconds = 2, .hundredths = 3};
  for (int frame = 0; frame <= 91; ++frame) {
    (void)r::UpdateCubePuzzleResults(screen, {}, clock, false, score, host);
  }
  std::vector<std::string> sounds;
  std::ranges::copy_if(
      host.calls, std::back_inserter(sounds), [](const std::string& call) {
        return call.starts_with("play") || call.starts_with("stop");
      });
  CHECK(sounds == std::vector<std::string>{"play 7e 72", "stop 7e 72",
                                           "play 7e 73", "play 7e 7d",
                                           "play 7e 7d", "play 7e 7d"});
  // Rank 14 at par 600 seconds, cap 1.
  CHECK(host.Has("rank 160 124 0 14"));
  CHECK(host.Has("message 3 88 64 90"));
  CHECK(host.Has("message 4 160 100 15"));
  CHECK(host.Has("prompt 214 187 0 90"));
}

TEST_CASE("F03050 results add the rank's points when Circle finishes",
          "[cube-lifecycle]") {
  FakeScreen host;
  std::uint32_t score = 0xFFFFFFF0U;
  r::CubePuzzleScreen screen{.timer = 100};
  const r::CubePuzzleRating rating{.par_seconds = 30, .rank_cap = 0};
  CHECK(r::UpdateCubePuzzleResults(screen, rating, {}, true, score, host) ==
        r::CubePuzzleScreenStep::kShowing);
  CHECK(screen.timer == 138);
  CHECK(score == 0xFFFFFFF0U);
  host.calls.clear();
  CHECK(r::UpdateCubePuzzleResults(screen, rating, {}, true, score, host) ==
        r::CubePuzzleScreenStep::kFinished);
  // 20000 points for rank 15, wrapping as the original word did.
  CHECK(score == 20000U - 16U);
  CHECK(host.calls.back() == "retire 29 2 5");
}

// Issue #409: at or under par the original indexes the 16-entry points table
// with 15 - cap unclamped; over par it clamps. Native refuses the at-par
// rank outside the table and awards nothing.
constexpr std::array<std::int16_t, 4> kOffTableCaps{{16, -1, 0x7FFF, -0x8000}};

TEST_CASE("F03050 results refuse at-par ranks outside the table",
          "[cube-lifecycle]") {
  for (const std::int16_t cap : kOffTableCaps) {
    CAPTURE(cap);
    FakeScreen host;
    std::uint32_t score = 50;
    r::CubePuzzleScreen screen{.timer = 140};
    CHECK(r::UpdateCubePuzzleResults(
              screen, {.par_seconds = 30, .rank_cap = cap}, {}, true, score,
              host) == r::CubePuzzleScreenStep::kFinishedRankOutsideTable);
    CHECK(host.Has(std::format("rank 160 124 50 {}", 15 - cap)));
    CHECK(host.calls.back() == "retire 29 2 5");
    CHECK(score == 50);
  }
}

TEST_CASE("F03050 live results pulse clutBuf while the screen state is set",
          "[cube-lifecycle-live]") {
  LiveScreenFixture fixture;
  Clut expected = PulseClut();
  std::ranges::copy(expected, std::begin(Menu_Results_clutBuf));
  Menu_Results_screenState = 1;
  Menu_Results_screenTimer = 140;
  Menu_Results_elementAnimationState = 0x400;
  Audio_Seq_unusedScoredata.miscScore = 3;
  CHECK(r::LiveUpdateCubePuzzleResults() == 0);
  RecordClut target;
  r::PulseCubePuzzleClut(expected, 16, target);
  CHECK(std::ranges::equal(Menu_Results_clutBuf, expected));
  CHECK(fixture.clut.Vram() == expected);
  CHECK(Menu_Results_elementAnimationState == 0x440);
  CHECK(Menu_Results_screenTimer == 140);
  CHECK(Audio_Seq_unusedScoredata.miscScore == 3);
}

// ----- F03051 -----

TEST_CASE("F03051 quit pulses then waits while the screen state is set",
          "[cube-lifecycle]") {
  r::CubePuzzleScreen screen{.timer = 0, .screen_state = 4, .animation = 0xC00};
  FakeScreen host;
  CHECK(r::UpdateCubePuzzleQuit(screen, true, host) ==
        r::CubePuzzleScreenStep::kShowing);
  CHECK(host.calls == std::vector<std::string>{"pulse 0"});
  CHECK(screen.animation == 0xC40);
  CHECK(screen.timer == 0);
}

TEST_CASE("F03051 quit reveals giving up and chicken on schedule",
          "[cube-lifecycle]") {
  r::CubePuzzleScreen screen{};
  FakeScreen host;
  (void)r::UpdateCubePuzzleQuit(screen, false, host);
  CHECK(host.calls == std::vector<std::string>{
                          "pulse 8", "play 7e 7a", "message 5 0 64 0",
                          "message 6 0 100 -15", "prompt 214 187 -15 0"});
  for (int frame = 1; frame < 16; ++frame) {
    (void)r::UpdateCubePuzzleQuit(screen, false, host);
  }
  CHECK(host.Count("play 7e 7a") == 2);
  CHECK(screen.timer == 16);
}

TEST_CASE("F03051 quit skips to its end on Circle then finishes",
          "[cube-lifecycle]") {
  r::CubePuzzleScreen screen{.timer = 10};
  FakeScreen host;
  CHECK(r::UpdateCubePuzzleQuit(screen, true, host) ==
        r::CubePuzzleScreenStep::kShowing);
  CHECK(screen.timer == 63);
  CHECK(host.calls.back() == "stop 7e 0");
  host.calls.clear();
  screen.timer = 62;
  CHECK(r::UpdateCubePuzzleQuit(screen, true, host) ==
        r::CubePuzzleScreenStep::kFinished);
  CHECK(host.calls.back() == "retire 29 1 5");
}

TEST_CASE("F03051 live quit pulses clutBuf while the screen state is set",
          "[cube-lifecycle-live]") {
  LiveScreenFixture fixture;
  Clut expected = PulseClut();
  std::ranges::copy(expected, std::begin(Menu_Results_clutBuf));
  Menu_Results_screenState = 1;
  Menu_Results_screenTimer = 9;
  Menu_Results_elementAnimationState = 0xFFF;
  CHECK(r::LiveUpdateCubePuzzleQuit() == 0);
  RecordClut target;
  r::PulseCubePuzzleClut(expected, r::CubePuzzlePulseWeight(0xFFF), target);
  CHECK(std::ranges::equal(Menu_Results_clutBuf, expected));
  CHECK(Menu_Results_elementAnimationState == 0x3F);
  CHECK(Menu_Results_screenTimer == 9);
}

TEST_CASE("F03050 results clamp over-par ranks for caps outside the table",
          "[cube-lifecycle]") {
  const r::CubePuzzleClock late{.minutes = 0, .seconds = 31, .hundredths = 0};
  for (const std::int16_t cap : kOffTableCaps) {
    CAPTURE(cap);
    const r::CubePuzzleRating rating{.par_seconds = 30, .rank_cap = cap};
    const std::int32_t rank = r::CubePuzzleRank(late, rating);
    REQUIRE(rank >= 0);
    REQUIRE(std::cmp_less(rank, r::kCubePuzzleRankPoints.size()));
    FakeScreen host;
    std::uint32_t score = 50;
    r::CubePuzzleScreen screen{.timer = 140};
    CHECK(r::UpdateCubePuzzleResults(screen, rating, late, true, score, host) ==
          r::CubePuzzleScreenStep::kFinished);
    CHECK(score ==
          50 + r::kCubePuzzleRankPoints.at(static_cast<std::size_t>(rank)));
  }
}
}  // namespace
