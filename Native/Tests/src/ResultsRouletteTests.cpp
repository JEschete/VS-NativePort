#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/SfxChannelGroups.hpp"
#include "Audio/SoundPlaybackLive.hpp"
#include "Battle/AbilityState.hpp"
#include "Battle/Actor.h"
#include "Core/RandomLive.hpp"
#include "Input/Buttons.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Results/GameClearLifecycleLive.hpp"
#include "Menu/Results/Results.hpp"
#include "Menu/Results/TimeAttack.hpp"
#include "Render/TexturePagesLive.hpp"

namespace {
void IgnoreAudio(VS::Audio::CommandParams*) {}

struct RouletteFixture {
  Battle_Actor* saved_actor = VS::Battle::characterState;
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  unsigned saved_random = VS::Core::BiosRand().State();
  decltype(VS::Input::buttonsPressed) saved_input = VS::Input::buttonsPressed;
  int saved_state = VS::Menu::Results::screenState;
  int saved_timer = VS::Menu::Results::screenTimer;
  int saved_reel = VS::Menu::Results::buffReelIndex;
  u_int saved_selection = VS::Menu::Results::buffReelSelection;
  u_int saved_animation = VS::Menu::Results::elementAnimationState;
  int saved_counting_score = VS::Menu::Results::incrementingScore;
  int saved_counting_map = VS::Menu::Results::incrementingMapCompletion;
  int saved_visible_score = VS::Menu::Results::onScreenScore;
  int saved_visible_map = VS::Menu::Results::onScreenMapCompletion;
  decltype(VS::Menu::Results::score) saved_score = VS::Menu::Results::score;
  decltype(VS::Menu::Results::mapCompletion) saved_completion =
      VS::Menu::Results::mapCompletion;
  decltype(VS::Menu::Results::rank) saved_rank = VS::Menu::Results::rank;
  decltype(VS::Menu::Results::clearCount) saved_clear_count =
      VS::Menu::Results::clearCount;
  VS::Render::TexturePageTable saved_pages = VS::Render::LiveTexturePages();
  decltype(std::to_array(VS::Audio::Mixer::kCommandHandlers)) saved_handlers =
      std::to_array(VS::Audio::Mixer::kCommandHandlers);
  VS::Audio::LegacyCommandArguments saved_arguments =
      VS::Audio::Driver::commandArguments;
  VS::Audio::CommandParams saved_parameters = VS::Audio::Mixer::commandParams;
  int saved_channel = VS::Audio::nextSfxChannelGroup7E;
  std::array<u_int, 256> offsets{};
  std::array<u_short, 256> selectors{};
  VS::Audio::Playback::ScopedLiveEffectTables sound_tables{
      {.program_offsets = offsets,
       .bank_selectors = selectors,
       .program_data = {}}};

  RouletteFixture() {
    REQUIRE(VS::Menu::Results::InspectLiveGameClear().state_timer == 0);
    std::memset(&stats, 0, sizeof stats);
    actor.stats = &stats;
    VS::Battle::characterState = &actor;
    selectors.fill(0x8000);
    std::fill(std::begin(VS::Audio::Mixer::kCommandHandlers),
              std::end(VS::Audio::Mixer::kCommandHandlers), IgnoreAudio);
    VS::Audio::nextSfxChannelGroup7E = 2;
    VS::Input::buttonsPressed = 0;
    VS::Menu::Results::screenTimer = -4096;
  }

  ~RouletteFixture() {
    VS::Menu::Results::ResetLiveGameClearModuleData();
    VS::Battle::characterState = saved_actor;
    VS::Core::SeedBiosRand(saved_random);
    VS::Input::buttonsPressed = saved_input;
    VS::Menu::Results::screenState = saved_state;
    VS::Menu::Results::screenTimer = saved_timer;
    VS::Menu::Results::buffReelIndex = saved_reel;
    VS::Menu::Results::buffReelSelection = saved_selection;
    VS::Menu::Results::elementAnimationState = saved_animation;
    VS::Menu::Results::incrementingScore = saved_counting_score;
    VS::Menu::Results::incrementingMapCompletion = saved_counting_map;
    VS::Menu::Results::onScreenScore = saved_visible_score;
    VS::Menu::Results::onScreenMapCompletion = saved_visible_map;
    VS::Menu::Results::score = saved_score;
    VS::Menu::Results::mapCompletion = saved_completion;
    VS::Menu::Results::rank = saved_rank;
    VS::Menu::Results::clearCount = saved_clear_count;
    VS::Render::LiveTexturePages() = saved_pages;
    std::copy(saved_handlers.begin(), saved_handlers.end(),
              VS::Audio::Mixer::kCommandHandlers);
    VS::Audio::Driver::commandArguments = saved_arguments;
    VS::Audio::Mixer::commandParams = saved_parameters;
    VS::Audio::nextSfxChannelGroup7E = saved_channel;
  }

  // Original MENUF/3B8.c:755 keeps a private per-state timer. Confirming state2
  // resets it; negative presentation time isolates the real logic from assets.
  void PrimeStateTimer(int ticks) {
    VS::Menu::Results::screenTimer = -4096;
    VS::Menu::Results::screenState = 2;
    VS::Input::buttonsPressed = VS::Legacy::kPadRright;
    REQUIRE(VS::Menu::Results::LiveUpdateGameClear() == 0);
    VS::Input::buttonsPressed = 0;
    for (int tick = 1; tick < ticks; ++tick) {
      VS::Menu::Results::screenState = 2;
      REQUIRE(VS::Menu::Results::LiveUpdateGameClear() == 0);
    }
  }
};
}  // namespace

TEST_CASE("Roulette uses the full BIOS state and unsigned byte reel positions",
          "[menu][roulette][random]") {
  RouletteFixture fixture;
  VS::Core::SeedBiosRand(1);
  const std::array expected{200, 120, 136, 104};
  for (const auto position : expected) {
    const auto selected = (VS::Core::NextBiosRand() & 0xF0) | 8;
    CHECK(selected == position);
    CHECK(static_cast<unsigned char>(selected) / 16 == selected / 16);
  }
  const auto saved = VS::Core::BiosRand().State();
  std::array<int, 32> continuation{};
  for (auto& position : continuation) {
    position = (VS::Core::NextBiosRand() & 0xF0) | 8;
  }
  VS::Core::SeedBiosRand(saved);
  for (const auto position : continuation) {
    CHECK(((VS::Core::NextBiosRand() & 0xF0) | 8) == position);
  }
}

TEST_CASE("Original roulette tables favor the selected attribute and HP",
          "[menu][roulette][random]") {
  constexpr auto kFirstStat =
      static_cast<std::int16_t>(VS::Menu::Results::BonusReelStat::kStrength);
  constexpr auto kLastStat =
      static_cast<std::int16_t>(VS::Menu::Results::BonusReelStat::kMp);
  const std::array<std::array<int, 5>, 3> expected{{
      {4, 1, 1, 7, 3},
      {1, 1, 4, 8, 2},
      {1, 4, 1, 7, 3},
  }};
  for (int reel = 0; reel < 3; ++reel) {
    std::array<int, 5> counts{};
    for (const auto& award : VS::Menu::Results::kBuffReels[reel]) {
      REQUIRE(award.stat >= kFirstStat);
      REQUIRE(award.stat <= kLastStat);
      CHECK(award.amount >= 1);
      CHECK(award.amount <= 5);
      ++counts[award.stat - kFirstStat];
    }
    CHECK(counts == expected[reel]);
  }
}

TEST_CASE("Native reel deceleration preserves original per-call stop alignment",
          "[menu][roulette][timing]") {
  RouletteFixture fixture;
  for (unsigned start = 0; start < 256; ++start) {
    fixture.PrimeStateTimer(48);
    VS::Menu::Results::screenState = 1;
    VS::Menu::Results::elementAnimationState = 48;
    VS::Menu::Results::buffReelSelection = start;
    const auto random = VS::Core::BiosRand().State();
    // Speeds48..9 contribute125 units; speed8 then advances to low nibble8.
    const unsigned alignment = (8 - ((start + 125) & 15)) & 15;
    const auto expected_calls = 40 + alignment;
    unsigned calls = 0;
    while (VS::Menu::Results::screenState == 1 && calls < 56) {
      REQUIRE(VS::Menu::Results::LiveUpdateGameClear() == 0);
      ++calls;
    }
    INFO("starting byte " << start);
    CHECK(calls == expected_calls);
    CHECK(VS::Menu::Results::screenState == 2);
    CHECK(VS::Menu::Results::elementAnimationState == 8);
    CHECK(VS::Menu::Results::buffReelSelection ==
          ((start + 125 + alignment) & 255));
    CHECK((VS::Menu::Results::buffReelSelection & 15) == 8);
    CHECK(VS::Core::BiosRand().State() == random);
  }
}

TEST_CASE(
    "Early roulette Circle skips presentation without stopping or rerolling",
    "[menu][roulette][timing]") {
  RouletteFixture fixture;
  VS::Menu::Results::screenState = 0;
  VS::Menu::Results::screenTimer = 0;
  VS::Menu::Results::elementAnimationState = 0;
  VS::Menu::Results::buffReelSelection = 248;
  VS::Input::buttonsPressed = VS::Legacy::kPadRright;
  const auto random = VS::Core::BiosRand().State();
  CHECK(VS::Menu::Results::LiveUpdateGameClear() == 0);
  CHECK(VS::Menu::Results::screenState == 0);
  CHECK(VS::Menu::Results::screenTimer == 273);
  CHECK(VS::Menu::Results::elementAnimationState == 0);
  CHECK(VS::Menu::Results::buffReelSelection == 248);
  CHECK(VS::Core::BiosRand().State() == random);
}

TEST_CASE("Native roulette awards every original slot and caps only its stat",
          "[menu][roulette][awards]") {
  const int reel = GENERATE(0, 1, 2);
  const bool near_cap = GENERATE(false, true);
  RouletteFixture fixture;
  for (unsigned slot = 0; slot < 16; ++slot) {
    std::memset(&fixture.stats, 0, sizeof fixture.stats);
    auto& stats = fixture.stats;
    stats.strength = near_cap ? 998 : 12;
    stats.totalStrength = near_cap ? 998 : 17;
    stats.intelligence = near_cap ? 998 : 57;
    stats.totalIntelligence = near_cap ? 998 : 76;
    stats.agility = near_cap ? 998 : 31;
    stats.totalAgility = near_cap ? 998 : 45;
    stats.currentHp = near_cap ? 998 : 200;
    stats.maxHp = near_cap ? 998 : 250;
    stats.currentMp = near_cap ? 998 : 40;
    stats.maxMp = near_cap ? 998 : 60;
    auto expected = stats;
    const auto award = VS::Menu::Results::kBuffReels[reel][slot];
    const auto increment = [award](auto& value) {
      value = static_cast<std::remove_reference_t<decltype(value)>>(
          std::min(999, static_cast<int>(value) + award.amount));
    };
    using enum VS::Menu::Results::BonusReelStat;
    switch (static_cast<VS::Menu::Results::BonusReelStat>(award.stat)) {
      case kStrength:
        increment(expected.strength);
        increment(expected.totalStrength);
        break;
      case kAgility:
        increment(expected.agility);
        increment(expected.totalAgility);
        break;
      case kIntelligence:
        increment(expected.intelligence);
        increment(expected.totalIntelligence);
        break;
      case kHp:
        increment(expected.currentHp);
        increment(expected.maxHp);
        break;
      case kMp:
        increment(expected.currentMp);
        increment(expected.maxMp);
        break;
      default:
        FAIL("Original reel contains an unsupported stat");
    }
    fixture.PrimeStateTimer(8);
    VS::Menu::Results::screenState = 3;
    VS::Menu::Results::buffReelIndex = reel;
    VS::Menu::Results::buffReelSelection = slot * 16 + 8;
    INFO("reel " << reel << " slot " << slot << " cap " << near_cap);
    CHECK(VS::Menu::Results::LiveUpdateGameClear() == 1);
    CHECK(std::memcmp(&stats, &expected, sizeof stats) == 0);
  }
}
