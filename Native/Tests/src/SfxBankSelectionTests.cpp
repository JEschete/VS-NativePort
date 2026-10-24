#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Log.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Audio/Playback/SoundPlayback.h"
#include "Audio/Sfx.h"
#include "Audio/SoundBanks.h"
}

namespace {
namespace Core = VS::Core;

struct TaggedSfxBank {
  std::uint32_t count = 1;
  std::uint32_t offset = 0;
  Audio_SfxProgramHeader program{};

  TaggedSfxBank() {
    offset = offsetof(TaggedSfxBank, program);
    // No AKAO magic: test offset/tag routing without starting mixer voices.
    program.flags = std::bit_cast<char>(std::uint8_t{0x80});
  }
};

struct SfxSelectionFixture {
  Audio_SoundState saved = Audio_soundState;
  int saved_channel = Audio_nextSfxChannelGroupFF000;
  int saved_room_channel = Audio_nextSfxChannelGroupF00000;
  Core::LogObserver observer = Core::SetLogObserver({});
  std::vector<Core::LogLevel> levels;
  std::vector<std::string> messages;
  TaggedSfxBank bank;

  SfxSelectionFixture() {
    Audio_soundState = {};
    Audio_nextSfxChannelGroupFF000 = 0x1000;
    Audio_nextSfxChannelGroupF00000 = 0x100000;
    Core::SetLogLevel(Core::LogLevel::Trace);
    Core::SetLogChannels("*");
    Core::SetLogObserver([this](const Core::LogRecord& record) {
      if (record.channel != "audio.sfx") return;
      levels.push_back(record.level);
      messages.emplace_back(record.message);
    });
  }

  ~SfxSelectionFixture() {
    Audio_soundState = saved;
    Audio_nextSfxChannelGroupFF000 = saved_channel;
    Audio_nextSfxChannelGroupF00000 = saved_room_channel;
    Core::SetLogObserver(std::move(observer));
    Core::SetLogLevel(Core::LogLevel::Info);
    Core::SetLogChannels("*");
  }
};
}  // namespace

TEST_CASE(
    "Selected private SFX banks retain serialized routing and channel rotation",
    "[audio][sfx-selection]") {
  for (unsigned slot = 1; slot <= 3; ++slot) {
    SfxSelectionFixture fixture;
    Audio_soundState.currentSfxBank = slot;
    Audio_soundState.sfxBankData[slot - 1] = &fixture.bank;
    Audio_PlaySfx(0xFF000, 0, 128, 127);
    CHECK(Audio_nextSfxChannelGroupFF000 == 0x2000);
    CHECK(Audio_soundState.currentSfxBank == slot);
    CHECK(fixture.levels.empty());
  }
}

TEST_CASE(
    "Unselected private SFX do not index before the bank array or choose a "
    "fallback",
    "[audio][sfx-unselected]") {
  SfxSelectionFixture fixture;
  Audio_soundState.sfxBankData[0] = &fixture.bank;
  Audio_PlaySfx(0xFF000, 0, 128, 127);
  CHECK(Audio_nextSfxChannelGroupFF000 == 0x1000);
  REQUIRE(fixture.levels.size() == 1);
  CHECK(fixture.levels.front() == Core::LogLevel::Warn);
  CHECK_FALSE(fixture.messages.front().empty());
}

TEST_CASE("Invalid private SFX selections surface errors before lookup",
          "[audio][sfx-invalid-selection]") {
  const auto slot = GENERATE(1U, 4U, std::numeric_limits<unsigned>::max());
  SfxSelectionFixture fixture;
  Audio_soundState.currentSfxBank = slot;
  Audio_PlaySfx(0xFF000, 0, 128, 127);
  CHECK(Audio_nextSfxChannelGroupFF000 == 0x1000);
  REQUIRE(fixture.levels.size() == 1);
  CHECK(fixture.levels.front() == Core::LogLevel::Error);
}

TEST_CASE("Room SFX bank absence and tagged routing remain explicit",
          "[audio][sfx-selection]") {
  SfxSelectionFixture fixture;
  SECTION("absent room bank never falls back to a private bank") {
    Audio_soundState.sfxBankData[0] = &fixture.bank;
    Audio_PlaySfx(0xF00000, 0, 128, 127);
    CHECK(Audio_nextSfxChannelGroupF00000 == 0x100000);
    REQUIRE(fixture.levels.size() == 1);
    CHECK(fixture.levels.front() == Core::LogLevel::Warn);
  }
  SECTION("present room bank retains its distinct channel group") {
    Audio_soundState.sfxBankData[2] = &fixture.bank;
    Audio_PlaySfx(0xF00000, 0, 128, 127);
    CHECK(Audio_nextSfxChannelGroupF00000 == 0x200000);
    CHECK(fixture.levels.empty());
  }
}

TEST_CASE("Negative banked SFX IDs are rejected before serialized offsets",
          "[audio][sfx-invalid-selection]") {
  SfxSelectionFixture fixture;
  Audio_soundState.currentSfxBank = 1;
  Audio_soundState.sfxBankData[0] = &fixture.bank;
  Audio_soundState.sfxBankData[2] = &fixture.bank;
  const auto category = GENERATE(0xFF000, 0xF00000);
  Audio_PlaySfx(category, -1, 128, 127);
  REQUIRE(fixture.levels.size() == 1);
  CHECK(fixture.levels.front() == Core::LogLevel::Error);
  CHECK(Audio_nextSfxChannelGroupFF000 == 0x1000);
  CHECK(Audio_nextSfxChannelGroupF00000 == 0x100000);
}
