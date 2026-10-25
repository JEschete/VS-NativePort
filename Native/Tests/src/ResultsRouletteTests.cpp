#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/BiosRand.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Playback/SoundPlayback.h"
#include "Audio/Sfx.h"
#include "Battle/Abilities.h"
#include "Battle/Actor.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Menu/Results/Results.h"
#include "Menu/Results/TimeAttack.h"
#include "Render/Textures.h"
}

namespace {
void IgnoreAudio(Audio_CommandParams*) {}

struct RouletteFixture {
  Battle_Actor* saved_actor = Battle_characterState;
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  unsigned saved_random = Core_Random_State();
  decltype(Input_buttonsPressed.allButtons) saved_input =
      Input_buttonsPressed.allButtons;
  int saved_state = Menu_Results_screenState;
  int saved_timer = Menu_Results_screenTimer;
  int saved_reel = Menu_Results_buffReelIndex;
  u_int saved_selection = Menu_Results_buffReelSelection;
  u_int saved_animation = Menu_Results_elementAnimationState;
  int saved_counting_score = Menu_Results_incrementingScore;
  int saved_counting_map = Menu_Results_incrementingMapCompletion;
  int saved_visible_score = Menu_Results_onScreenScore;
  int saved_visible_map = Menu_Results_onScreenMapCompletion;
  std::array<u_char, 40> saved_reservations =
      std::to_array(Render_legacyTexturePageReservations);
  decltype(std::to_array(Audio_Mixer_kCommandHandlers)) saved_handlers =
      std::to_array(Audio_Mixer_kCommandHandlers);
  u_int* saved_offsets = Audio_Playback_soundEffectProgramOffsets;
  u_short* saved_selectors = Audio_Playback_soundEffectBankSelectors;
  Audio_CommandArguments saved_arguments = Audio_Driver_commandArguments;
  Audio_CommandParams saved_parameters = Audio_Mixer_commandParams;
  int saved_channel = Audio_nextSfxChannelGroup7E;
  std::array<u_int, 256> offsets{};
  std::array<u_short, 256> selectors{};

  RouletteFixture() {
    std::memset(&stats, 0, sizeof stats);
    actor.stats = &stats;
    Battle_characterState = &actor;
    selectors.fill(0x8000);
    Audio_Playback_soundEffectProgramOffsets = offsets.data();
    Audio_Playback_soundEffectBankSelectors = selectors.data();
    std::fill(std::begin(Audio_Mixer_kCommandHandlers),
              std::end(Audio_Mixer_kCommandHandlers), IgnoreAudio);
    Audio_nextSfxChannelGroup7E = 2;
    Input_buttonsPressed.allButtons = 0;
    Menu_Results_screenTimer = -4096;
  }

  ~RouletteFixture() {
    Battle_characterState = saved_actor;
    Core_Random_Seed(saved_random);
    Input_buttonsPressed.allButtons = saved_input;
    Menu_Results_screenState = saved_state;
    Menu_Results_screenTimer = saved_timer;
    Menu_Results_buffReelIndex = saved_reel;
    Menu_Results_buffReelSelection = saved_selection;
    Menu_Results_elementAnimationState = saved_animation;
    Menu_Results_incrementingScore = saved_counting_score;
    Menu_Results_incrementingMapCompletion = saved_counting_map;
    Menu_Results_onScreenScore = saved_visible_score;
    Menu_Results_onScreenMapCompletion = saved_visible_map;
    std::copy(saved_reservations.begin(), saved_reservations.end(),
              Render_legacyTexturePageReservations);
    std::copy(saved_handlers.begin(), saved_handlers.end(),
              Audio_Mixer_kCommandHandlers);
    Audio_Playback_soundEffectProgramOffsets = saved_offsets;
    Audio_Playback_soundEffectBankSelectors = saved_selectors;
    Audio_Driver_commandArguments = saved_arguments;
    Audio_Mixer_commandParams = saved_parameters;
    Audio_nextSfxChannelGroup7E = saved_channel;
  }

  // Original MENUF/3B8.c:755 keeps a private per-state timer. Confirming state2
  // resets it; negative presentation time isolates the real logic from assets.
  void PrimeStateTimer(int ticks) {
    Menu_Results_screenTimer = -4096;
    Menu_Results_screenState = 2;
    Input_buttonsPressed.allButtons = PADRright;
    REQUIRE(Menu_Results_UpdateGameClear() == 0);
    Input_buttonsPressed.allButtons = 0;
    for (int tick = 1; tick < ticks; ++tick) {
      Menu_Results_screenState = 2;
      REQUIRE(Menu_Results_UpdateGameClear() == 0);
    }
  }
};
}  // namespace

TEST_CASE("Roulette uses the full BIOS state and unsigned byte reel positions",
          "[menu][roulette][random]") {
  RouletteFixture fixture;
  Core_Random_Seed(1);
  const std::array expected{200, 120, 136, 104};
  for (const auto position : expected) {
    const auto selected = (Core_Random_Rand() & 0xF0) | 8;
    CHECK(selected == position);
    CHECK(static_cast<unsigned char>(selected) / 16 == selected / 16);
  }
  const auto saved = Core_Random_State();
  std::array<int, 32> continuation{};
  for (auto& position : continuation) {
    position = (Core_Random_Rand() & 0xF0) | 8;
  }
  Core_Random_Seed(saved);
  for (const auto position : continuation) {
    CHECK(((Core_Random_Rand() & 0xF0) | 8) == position);
  }
}

TEST_CASE("Original roulette tables favor the selected attribute and HP",
          "[menu][roulette][random]") {
  const std::array<std::array<int, 5>, 3> expected{{
      {4, 1, 1, 7, 3},
      {1, 1, 4, 8, 2},
      {1, 4, 1, 7, 3},
  }};
  for (int reel = 0; reel < 3; ++reel) {
    std::array<int, 5> counts{};
    for (const auto& award : Menu_Results_buffReels[reel]) {
      REQUIRE(award.stat >= Menu_Results_BuffReelStat_Str);
      REQUIRE(award.stat <= Menu_Results_BuffReelStat_Mp);
      CHECK(award.amount >= 1);
      CHECK(award.amount <= 5);
      ++counts[award.stat - Menu_Results_BuffReelStat_Str];
    }
    CHECK(counts == expected[reel]);
  }
}

TEST_CASE("Native reel deceleration preserves original per-call stop alignment",
          "[menu][roulette][timing]") {
  RouletteFixture fixture;
  for (unsigned start = 0; start < 256; ++start) {
    fixture.PrimeStateTimer(48);
    Menu_Results_screenState = 1;
    Menu_Results_elementAnimationState = 48;
    Menu_Results_buffReelSelection = start;
    const auto random = Core_Random_State();
    // Speeds48..9 contribute125 units; speed8 then advances to low nibble8.
    const unsigned alignment = (8 - ((start + 125) & 15)) & 15;
    const auto expected_calls = 40 + alignment;
    unsigned calls = 0;
    while (Menu_Results_screenState == 1 && calls < 56) {
      REQUIRE(Menu_Results_UpdateGameClear() == 0);
      ++calls;
    }
    INFO("starting byte " << start);
    CHECK(calls == expected_calls);
    CHECK(Menu_Results_screenState == 2);
    CHECK(Menu_Results_elementAnimationState == 8);
    CHECK(Menu_Results_buffReelSelection == ((start + 125 + alignment) & 255));
    CHECK((Menu_Results_buffReelSelection & 15) == 8);
    CHECK(Core_Random_State() == random);
  }
}

TEST_CASE(
    "Early roulette Circle skips presentation without stopping or rerolling",
    "[menu][roulette][timing]") {
  RouletteFixture fixture;
  Menu_Results_screenState = 0;
  Menu_Results_screenTimer = 0;
  Menu_Results_elementAnimationState = 0;
  Menu_Results_buffReelSelection = 248;
  Input_buttonsPressed.allButtons = PADRright;
  const auto random = Core_Random_State();
  CHECK(Menu_Results_UpdateGameClear() == 0);
  CHECK(Menu_Results_screenState == 0);
  CHECK(Menu_Results_screenTimer == 273);
  CHECK(Menu_Results_elementAnimationState == 0);
  CHECK(Menu_Results_buffReelSelection == 248);
  CHECK(Core_Random_State() == random);
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
    const auto award = Menu_Results_buffReels[reel][slot];
    const auto increment = [award](auto& value) {
      value = static_cast<std::remove_reference_t<decltype(value)>>(
          std::min(999, static_cast<int>(value) + award.amount));
    };
    switch (award.stat) {
      case Menu_Results_BuffReelStat_Str:
        increment(expected.strength);
        increment(expected.totalStrength);
        break;
      case Menu_Results_BuffReelStat_Agi:
        increment(expected.agility);
        increment(expected.totalAgility);
        break;
      case Menu_Results_BuffReelStat_Int:
        increment(expected.intelligence);
        increment(expected.totalIntelligence);
        break;
      case Menu_Results_BuffReelStat_Hp:
        increment(expected.currentHp);
        increment(expected.maxHp);
        break;
      case Menu_Results_BuffReelStat_Mp:
        increment(expected.currentMp);
        increment(expected.maxMp);
        break;
      default:
        FAIL("Original reel contains an unsupported stat");
    }
    fixture.PrimeStateTimer(8);
    Menu_Results_screenState = 3;
    Menu_Results_buffReelIndex = reel;
    Menu_Results_buffReelSelection = slot * 16 + 8;
    INFO("reel " << reel << " slot " << slot << " cap " << near_cap);
    CHECK(Menu_Results_UpdateGameClear() == 1);
    CHECK(std::memcmp(&stats, &expected, sizeof stats) == 0);
  }
}
