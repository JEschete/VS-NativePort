#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Menu/Results/GameClearLifecycle.hpp"
#include "Render/UiTexturedQuads.hpp"

namespace {
namespace results = VS::Menu::Results;
namespace render = VS::Render;

struct Music final : results::ResultsMusicHost {
  std::vector<std::pair<std::uint32_t, std::int32_t>> calls;
  void LoadMusicSlot(std::uint32_t music, std::int32_t slot) override {
    calls.emplace_back(music, slot);
  }
};

struct Rows final : results::ResultsDrawTarget {
  std::vector<render::UiTexturedQuad> quads;
  bool accept = true;
  bool DrawQuad(const render::UiTexturedQuad& quad) override {
    quads.push_back(quad);
    return accept;
  }
  void DrawText(std::string_view, std::int32_t, std::uint32_t) override {
    FAIL("texture rows must not render text");
  }
};

struct Reel final : results::BonusReelTarget {
  std::vector<std::array<std::int32_t, 6>> calls;
  std::vector<render::ScreenRect> areas;
  bool accept_area = true;
  void Tint(results::ResultsPoint origin, std::int32_t texture,
            const results::GameClearColors& colors) override {
    calls.push_back(
        {0, origin.x, origin.y, texture, colors[0].step, colors[1].step});
  }
  void Animated(results::ResultsPoint origin, std::int32_t texture) override {
    calls.push_back({1, origin.x, origin.y, texture, 0, 0});
  }
  void Stopped(results::ResultsPoint origin, std::int32_t texture,
               std::int32_t timer, std::uint16_t clut) override {
    calls.push_back({2, origin.x, origin.y, texture, timer, clut});
  }
  bool SetDrawArea(const render::ScreenRect& area) override {
    areas.push_back(area);
    return accept_area;
  }
};

struct Screen final : results::GameClearScreenHost {
  std::vector<std::array<std::int32_t, 5>> sounds;
  std::vector<std::array<std::int32_t, 5>> draws;
  std::vector<std::pair<results::BonusReelStat, std::int32_t>> awards;
  int retired = 0;
  int finished = 0;
  void Draw(results::GameClearLine line, results::ResultsPoint origin,
            std::int32_t timer, std::int32_t phase,
            results::GameClearReadouts&) override {
    draws.push_back(
        {static_cast<std::int32_t>(line), origin.x, origin.y, timer, phase});
  }
  void Reel(results::ResultsPoint origin, std::int32_t timer,
            std::int32_t phase, const results::BonusReelView&) override {
    draws.push_back({7, origin.x, origin.y, timer, phase});
  }
  void Prompt(results::ResultsPoint origin, std::int32_t slide,
              std::int32_t bounce) override {
    draws.push_back({8, origin.x, origin.y, slide, bounce});
  }
  void Sound(results::GameClearSound action, std::int32_t effect,
             std::int32_t value, std::int32_t ticks) override {
    sounds.push_back(
        {static_cast<std::int32_t>(action), 0x7E, effect, value, ticks});
  }
  void Award(results::BonusReelStat stat, std::int32_t amount) override {
    awards.emplace_back(stat, amount);
  }
  void RetireTexturePages(const results::TexturePageLoan& loan) override {
    CHECK(loan.first_page == 0x1D);
    CHECK(loan.page_count == 3);
    CHECK(loan.owner_tag == 5);
    ++retired;
  }
  void FinishScreen() override { ++finished; }
};

struct Initializer final : results::GameClearInitHost {
  results::GameClearWorld world{};
  std::vector<std::string> events;
  std::vector<render::ScreenRect> pixels;
  std::vector<render::ScreenRect> palettes;
  bool reserve = true;
  bool allocate = true;
  bool loaded = false;
  bool valid_tim = true;
  bool music_loading = false;
  std::uint32_t random = 0xFFFF'FFF8U;
  bool ReserveTexturePages(const results::TexturePageLoan&) override {
    events.emplace_back("reserve");
    return reserve;
  }
  bool BeginTextureRead() override {
    events.emplace_back("read");
    return allocate;
  }
  bool TextureReadLoaded() override { return loaded; }
  std::optional<results::ResultsTimBlocks> ParseTim(std::size_t) override {
    if (!valid_tim) return std::nullopt;
    return results::ResultsTimBlocks{
        .pixels =
            render::ScreenRect{.x = 0, .y = 0, .width = 64, .height = 256},
        .palette =
            render::ScreenRect{.x = 0, .y = 0, .width = 128, .height = 1}};
  }
  bool UploadPixels(std::size_t, const render::ScreenRect& area) override {
    pixels.push_back(area);
    return true;
  }
  bool UploadPalette(std::size_t, const render::ScreenRect& area) override {
    palettes.push_back(area);
    return true;
  }
  void ReleaseTextureRequest() override { events.emplace_back("release"); }
  void QueueMusic() override { events.emplace_back("queue-music"); }
  bool MusicLoading() override { return music_loading; }
  void PlayMusic() override { events.emplace_back("play"); }
  bool FreeTextureBlock() override {
    events.emplace_back("free");
    return true;
  }
  std::uint32_t Random() override {
    events.emplace_back("random");
    return random;
  }
  std::optional<results::GameClearWorld> ReadWorld() override {
    events.emplace_back("world");
    return world;
  }
  void WriteWorld(const results::GameClearWorld& value) override {
    world = value;
    events.emplace_back("write");
  }
};

struct ReelFixture {
  std::array<render::TextureRegion, 27> atlas{};
  std::array<results::BonusReel, results::kBonusReelCount> reels{};
  Reel target;
  ReelFixture() {
    for (auto& entry : atlas) {
      entry = {.u = 250, .v = 255, .width = 10, .height = 1};
    }
    for (auto& reel : reels) {
      reel.fill({.stat = 13, .amount = 2});
    }
  }
  results::BonusReelScene Scene() const {
    return {.atlas = atlas,
            .reels = reels,
            .back_clip = {.x = 320, .y = 0, .width = 320, .height = 240}};
  }
};
}  // namespace

TEST_CASE("F03064 healing score uses unsigned wrapping before its cap",
          "[game-clear][F03064]") {
  const auto [score, heals, expected] =
      GENERATE(std::array<std::uint32_t, 3>{123, 2, 99'923},
               std::array<std::uint32_t, 3>{0, 1000, 0},
               std::array<std::uint32_t, 3>{0, 1001, 999'999'999},
               std::array<std::uint32_t, 3>{0xFFFF'FFFFU, 0, 99'999},
               std::array<std::uint32_t, 3>{999'999'998, 0, 999'999'999},
               std::array<std::uint32_t, 3>{7'000'000, 65535, 546'500});
  const auto result = results::ApplyHealingScoreBonus(
      {.misc_score = score, .heal_count = static_cast<std::uint16_t>(heals)});
  CHECK(result.misc_score == expected);
  CHECK(result.heal_count == 0);
}

TEST_CASE("F03013 music selects only the two unsigned boss intervals",
          "[game-clear][F03013]") {
  const auto [track, expected] =
      GENERATE(std::array<std::uint32_t, 2>{7, 128},
               std::array<std::uint32_t, 2>{30, 128},
               std::array<std::uint32_t, 2>{31, 131},
               std::array<std::uint32_t, 2>{39, 139},
               std::array<std::uint32_t, 2>{40, 128},
               std::array<std::uint32_t, 2>{44, 128},
               std::array<std::uint32_t, 2>{45, 145},
               std::array<std::uint32_t, 2>{49, 149},
               std::array<std::uint32_t, 2>{50, 128},
               std::array<std::uint32_t, 2>{0xFFFF'FFFFU, 128});
  Music host;
  results::QueueResultsMusic(track, host);
  REQUIRE(host.calls.size() == 1);
  CHECK(host.calls[0] == std::pair<std::uint32_t, std::int32_t>{expected, 2});
}

TEST_CASE(
    "F03010 waits for the loaded state without inventing error completion",
    "[game-clear][F03010]") {
  Initializer host;
  results::GameClearState game;
  CHECK(results::InitializeGameClear(game, host) ==
        results::GameClearStep::kPending);
  CHECK(game.init_phase == 1);
  CHECK(host.events == std::vector<std::string>{"reserve", "read"});
  for (int poll = 0; poll < 4; ++poll) {
    CHECK(results::InitializeGameClear(game, host) ==
          results::GameClearStep::kPending);
  }
  CHECK(game.init_phase == 1);
  CHECK(host.events.size() == 2);
}

TEST_CASE("F03010 loaded TIMs retain width and replace destination rectangles",
          "[game-clear][F03010]") {
  Initializer host;
  host.loaded = true;
  results::GameClearState game{.init_phase = 1};
  CHECK(results::InitializeGameClear(game, host) ==
        results::GameClearStep::kPending);
  CHECK(game.init_phase == 2);
  REQUIRE(host.pixels.size() == 3);
  CHECK(host.pixels[0] == render::ScreenRect{832, 256, 64, 255});
  CHECK(host.pixels[2] == render::ScreenRect{960, 256, 64, 255});
  CHECK(host.palettes == std::vector<render::ScreenRect>{{768, 511, 128, 1}});
  CHECK(host.events == std::vector<std::string>{"release", "queue-music"});
}

TEST_CASE(
    "F03010 final setup preserves private counters and completes progress",
    "[game-clear][F03010]") {
  Initializer host;
  host.world.room_flags[1] = 0x800000;
  host.world.countable_room_masks[1] = 0xC00000;
  host.world.chests_opened[0] = 255;
  host.world.heal_count = 2;
  results::GameClearState game{.init_phase = 2};
  game.screen.state_timer = 100;
  game.screen.readouts = {.score = {33, 7}, .map = {44, 8}};
  CHECK(results::InitializeGameClear(game, host) ==
        results::GameClearStep::kComplete);
  CHECK(host.world.room_flags[1] == 0xC00000);
  CHECK(host.world.record.map_completion == 2);
  CHECK(host.world.record.opened_chest_count == 1);
  CHECK(host.world.record.misc_score == 99'800);
  CHECK(game.screen.state_timer == 100);
  CHECK(game.screen.readouts.score.counting == 7);
  CHECK(game.screen.readouts.map.counting == 8);
}

TEST_CASE("F03010 selects the weakest total with original tie priority",
          "[game-clear][F03010]") {
  const auto [strength, intelligence, agility, reel] = GENERATE(
      std::array<int, 4>{10, 10, 10, 0}, std::array<int, 4>{20, 10, 10, 1},
      std::array<int, 4>{10, 20, 9, 2}, std::array<int, 4>{20, 10, 9, 2});
  Initializer host;
  host.world.strength = strength;
  host.world.intelligence = intelligence;
  host.world.agility = agility;
  results::GameClearState game{.init_phase = 2};
  CHECK(results::InitializeGameClear(game, host) ==
        results::GameClearStep::kComplete);
  CHECK(game.screen.reel.reel_index == reel);
  CHECK(game.screen.reel.selection == 248);
  CHECK(game.init_phase == 0);
  CHECK(host.events ==
        std::vector<std::string>{"play", "free", "random", "world", "write"});
}

TEST_CASE("F03010 refuses malformed input without queue release or success",
          "[game-clear][F03010]") {
  Initializer host;
  host.loaded = true;
  host.valid_tim = false;
  results::GameClearState game{.init_phase = 1};
  CHECK(results::InitializeGameClear(game, host) ==
        results::GameClearStep::kRefused);
  CHECK(host.events.empty());
  CHECK(game.init_phase == 1);
  game.init_phase = -1;
  CHECK(results::InitializeGameClear(game, host) ==
        results::GameClearStep::kRefused);
  CHECK(host.events.empty());
}

TEST_CASE("F03010 music wait leaves the entire presentation untouched",
          "[game-clear][F03010]") {
  Initializer host;
  host.music_loading = true;
  results::GameClearState game{.init_phase = 2};
  game.screen.timer = 123;
  CHECK(results::InitializeGameClear(game, host) ==
        results::GameClearStep::kPending);
  CHECK(game.screen.timer == 123);
  CHECK(host.events.empty());
}

TEST_CASE("F03015 early confirm skips reveal without stopping or rerolling",
          "[game-clear][F03015]") {
  ReelFixture fixture;
  Screen host;
  results::GameClearScreen screen;
  screen.reel.selection = 248;
  screen.score = 1234;
  screen.map_completion = 87;
  screen.readouts = {.score = {33, 7}, .map = {44, 8}};
  CHECK(results::UpdateGameClear(screen, fixture.reels, true, host) ==
        results::GameClearStep::kPending);
  CHECK(screen.timer == 273);
  CHECK(screen.phase == 0);
  CHECK(screen.reel.selection == 248);
  CHECK(screen.readouts.score.shown == 1234);
  CHECK(screen.readouts.map.shown == 87);
  CHECK(screen.readouts.score.counting == 0);
  CHECK(screen.readouts.map.counting == 0);
}

TEST_CASE("F03015 stopping aligns every original unsigned selection byte",
          "[game-clear][F03015]") {
  ReelFixture fixture;
  for (std::uint32_t start = 0; start < 256; ++start) {
    Screen host;
    results::GameClearScreen screen{.timer = 1000, .phase = 1};
    screen.reel = {.selection = start, .speed = 48};
    const auto alignment = (8U - ((start + 125U) & 15U)) & 15U;
    std::uint32_t calls = 0;
    while (screen.phase == 1 && calls < 56) {
      REQUIRE(results::UpdateGameClear(screen, fixture.reels, false, host) ==
              results::GameClearStep::kPending);
      ++calls;
    }
    CHECK(calls == 40 + alignment);
    CHECK(screen.phase == 2);
    CHECK(screen.reel.speed == 8);
    CHECK(screen.reel.selection == ((start + 125 + alignment) & 255));
    CHECK(screen.state_timer == 1);
  }
}

TEST_CASE("F03015 sound fades and prompt keep original callback order",
          "[game-clear][F03015]") {
  ReelFixture fixture;
  Screen host;
  results::GameClearScreen screen{.timer = 2073};
  screen.reel.speed = 48;
  CHECK(results::UpdateGameClear(screen, fixture.reels, true, host) ==
        results::GameClearStep::kPending);
  const std::vector<std::array<std::int32_t, 5>> expected{
      {0, 126, 118, 0, 0},
      {2, 126, 116, 64, 48},
      {2, 126, 117, 64, 48},
      {3, 126, 116, -16, 48},
      {3, 126, 117, -16, 48}};
  CHECK(host.sounds == expected);
  CHECK(screen.phase == 1);
  CHECK(screen.state_timer == 1);
  CHECK(host.draws.back() ==
        std::array<std::int32_t, 5>{8, 214, 187, 48, 2073});
}

TEST_CASE("F03015 completion retains original repeat-call reward semantics",
          "[game-clear][F03015]") {
  ReelFixture fixture;
  fixture.reels[2][15] = {.stat = 17, .amount = -2};
  Screen host;
  results::GameClearScreen screen{.timer = 333, .phase = 3, .state_timer = 8};
  screen.reel = {.selection = 0x1234'FFF8, .reel_index = 2};
  CHECK(results::UpdateGameClear(screen, fixture.reels, false, host) ==
        results::GameClearStep::kComplete);
  CHECK(results::UpdateGameClear(screen, fixture.reels, false, host) ==
        results::GameClearStep::kComplete);
  CHECK(host.awards ==
        std::vector<std::pair<results::BonusReelStat, std::int32_t>>{
            {results::BonusReelStat::kMp, -2},
            {results::BonusReelStat::kMp, -2}});
  CHECK(host.retired == 2);
  CHECK(host.finished == 2);
  CHECK(screen.timer == 333);
  CHECK(screen.state_timer == 8);
}

TEST_CASE("F03015 hold threshold and timer overflow are exact",
          "[game-clear][F03015]") {
  ReelFixture fixture;
  Screen host;
  results::GameClearScreen screen{
      .timer = std::numeric_limits<std::int32_t>::max(),
      .phase = 2,
      .state_timer = 450};
  CHECK(results::UpdateGameClear(screen, fixture.reels, false, host) ==
        results::GameClearStep::kPending);
  CHECK(screen.phase == 2);
  CHECK(screen.timer == std::numeric_limits<std::int32_t>::min());
  CHECK(results::UpdateGameClear(screen, fixture.reels, false, host) ==
        results::GameClearStep::kPending);
  CHECK(screen.phase == 3);
  CHECK(screen.state_timer == 1);
}

TEST_CASE("F03015 invalid award owners do not draw retire or complete",
          "[game-clear][F03015]") {
  Screen host;
  results::GameClearScreen screen{.phase = 3, .state_timer = 8};
  CHECK(results::UpdateGameClear(screen, {}, false, host) ==
        results::GameClearStep::kRefused);
  CHECK(host.draws.empty());
  CHECK(host.awards.empty());
  CHECK(host.retired == 0);
  CHECK(screen.state_timer == 8);
}

TEST_CASE("F03043 spinning layout uses three rows and inverse draw-area order",
          "[game-clear][F03043]") {
  ReelFixture fixture;
  CHECK(results::DrawBonusReel(fixture.Scene(), {130, 190}, 100, 0,
                               {.selection = 0, .speed = 48}, fixture.target) ==
        results::ResultsDrawResult::kDrawn);
  REQUIRE(fixture.target.calls.size() == 11);
  CHECK(fixture.target.calls[0] ==
        std::array<std::int32_t, 6>{0, 103, 190, 11, 64, 0});
  CHECK(fixture.target.calls[2] ==
        std::array<std::int32_t, 6>{1, 125, 197, 13, 0, 0});
  CHECK(fixture.target.calls[5][2] == 181);
  CHECK(fixture.target.calls[8][2] == 165);
  CHECK(fixture.target.areas == std::vector<render::ScreenRect>{
                                    {320, 0, 320, 240}, {320, 184, 320, 28}});
}

TEST_CASE("F03043 stopped reel selects low unsigned byte and decimal digits",
          "[game-clear][F03043]") {
  ReelFixture fixture;
  fixture.reels[1][15] = {.stat = 17, .amount = 32767};
  CHECK(results::DrawBonusReel(fixture.Scene(), {130, 190}, 1, 2,
                               {.selection = 0xFFFF'FFF8U, .reel_index = 1},
                               fixture.target) ==
        results::ResultsDrawResult::kDrawn);
  REQUIRE(fixture.target.calls.size() == 9);
  CHECK(fixture.target.calls[2] ==
        std::array<std::int32_t, 6>{2, 125, 189, 17, 1, 0x7FF2});
  CHECK(fixture.target.calls[4][3] == 3);
  CHECK(fixture.target.calls[8][3] == 7);
  CHECK(fixture.target.calls[8][1] == fixture.target.calls[4][1] + 48);
}

TEST_CASE(
    "F03043 rejects invalid entries before partial draws and reports capacity",
    "[game-clear][F03043]") {
  ReelFixture fixture;
  fixture.reels[0][0].amount = -1;
  CHECK(results::DrawBonusReel(fixture.Scene(), {}, 1, 2, {}, fixture.target) ==
        results::ResultsDrawResult::kInvalidTexture);
  CHECK(fixture.target.calls.empty());
  CHECK(fixture.target.areas.empty());
  CHECK(results::DrawBonusReel({}, {}, 0, 2, {}, fixture.target) ==
        results::ResultsDrawResult::kHidden);
  fixture.reels[0][0].amount = 1;
  fixture.target.accept_area = false;
  CHECK(results::DrawBonusReel(fixture.Scene(), {}, 1, 2, {}, fixture.target) ==
        results::ResultsDrawResult::kFull);
  CHECK(fixture.target.areas.size() == 2);
}

TEST_CASE("F03045 rows fade additively then subtractively outside the window",
          "[game-clear][F03045]") {
  std::array<render::TextureRegion, 1> atlas{
      {{.u = 250, .v = 255, .width = 10, .height = 3, .texture_page = 0x11}}};
  Rows target;
  CHECK(results::DrawBonusReelTexture(atlas, {32767, 188}, 0, target) ==
        results::ResultsDrawResult::kDrawn);
  REQUIRE(target.quads.size() == 5);
  CHECK(target.quads[0].texture.texture_page == 0x31);
  CHECK(target.quads[1].texture.texture_page == 0x51);
  CHECK(target.quads[0].texture.clut == 0x7FF3);
  CHECK(target.quads[4].texture.clut == 0x7FF2);
  CHECK(target.quads[4].texture.v == 1);
  CHECK(target.quads[4].bottom_right.x == -32759);
  CHECK(target.quads[0].tint == render::QuadColor{112, 112, 112});
}

TEST_CASE("F03045 lower edge and refusal retain each offered row",
          "[game-clear][F03045]") {
  std::array<render::TextureRegion, 1> atlas{{{.width = 4, .height = 2}}};
  Rows target;
  target.accept = false;
  CHECK(results::DrawBonusReelTexture(atlas, {0, 205}, 0, target) ==
        results::ResultsDrawResult::kFull);
  REQUIRE(target.quads.size() == 3);
  CHECK(target.quads[1].tint == render::QuadColor{128, 128, 128});
  CHECK(results::DrawBonusReelTexture(atlas, {}, -1, target) ==
        results::ResultsDrawResult::kInvalidTexture);
  CHECK(target.quads.size() == 3);
  atlas[0].height = 0;
  CHECK(results::DrawBonusReelTexture(atlas, {}, 0, target) ==
        results::ResultsDrawResult::kHidden);
}
