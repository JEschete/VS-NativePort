#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Audio/MixControlLive.hpp"
#include "Audio/SfxLive.hpp"
#include "Audio/SoundControlLive.hpp"
#include "Audio/SoundEffectPrograms.hpp"
#include "Audio/SoundEffectProgramsLive.hpp"
#include "Audio/SoundPlaybackLive.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Audio/Control/SoundControl.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Playback/SoundPlayback.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Audio/SoundBanks.h"
#include "Audio/SoundEffects/SoundEffects.h"
}

namespace {
namespace Core = VS::Core;
namespace Memory = VS::Core::Memory;

struct TaggedSfxBank {
  std::uint32_t count = 2;
  std::array<std::uint32_t, 2> offsets{};
  Audio_SfxProgramHeader program{};
  Audio_SfxProgramHeader second_program{};

  TaggedSfxBank() {
    offsets = {offsetof(TaggedSfxBank, program),
               offsetof(TaggedSfxBank, second_program)};
    // No AKAO magic: test offset/tag routing without starting mixer voices.
    program.flags = std::bit_cast<char>(std::uint8_t{0x80});
  }
};

struct SfxSelectionFixture {
  Audio_SoundState saved = Audio_soundState;
  Audio_CommandArguments saved_arguments = Audio_Driver_commandArguments;
  Audio_CommandParams saved_params = Audio_Mixer_commandParams;
  Audio_VoiceScheduler saved_scheduler = Audio_Mixer_voiceScheduler;
  Audio_DriverFlags saved_flags = Audio_Driver_controlLatches;
  std::array<Audio_SeqChannel, 2> saved_voices{Audio_SoundEffects_channels[0],
                                               Audio_SoundEffects_channels[1]};
  std::array<Audio_CommandHandler, 4> saved_handlers{
      Audio_Mixer_kCommandHandlers[0x21], Audio_Mixer_kCommandHandlers[0xA1],
      Audio_Mixer_kCommandHandlers[0xA3], Audio_Mixer_kCommandHandlers[0xA5]};
  int saved_channel = Audio_nextSfxChannelGroupFF000;
  int saved_room_channel = Audio_nextSfxChannelGroupF00000;
  Core::LogObserver observer = Core::SetLogObserver({});
  std::vector<Core::LogLevel> levels;
  std::vector<std::string> messages;
  // SFX banks live in game heap blocks, the sequence memory whose programs
  // the playback rules read (SeqRecordsLive.hpp LiveProgramRegion).
  alignas(Memory::kHeapUnitBytes) std::array<std::byte, 512> arena{};
  Memory::ScopedLiveGameHeap heap{arena};
  TaggedSfxBank& bank = *new (HeapBankBlock()) TaggedSfxBank{};

  static void* HeapBankBlock() {
    void* const block = Memory::AllocateLiveFromLowEnd(sizeof(TaggedSfxBank));
    REQUIRE(block != nullptr);
    return block;
  }

  // A one-entry blob (func_8001733C) in a heap block: entry 0 at byte 0x20
  // starts one program at byte 0x24 on bank selector 5.
  static std::byte* HeapBlob() {
    namespace Effects = VS::Audio::SoundEffects;
    constexpr std::size_t kBytes = 0x40;
    auto* const blob =
        static_cast<std::byte*>(Memory::AllocateLiveFromLowEnd(kBytes));
    REQUIRE(blob != nullptr);
    std::fill_n(blob, kBytes, std::byte{0});
    const auto put = [blob](std::size_t at, auto value) {
      std::memcpy(blob + at, &value, sizeof value);
    };
    put(Effects::kBlobCountWord * sizeof(std::uint32_t), std::uint32_t{1});
    put(Effects::kBlobSelectorWord * sizeof(std::uint32_t), std::uint32_t{5});
    put(Effects::kBlobOffsetsStart, std::uint32_t{0});
    put(Effects::kBlobEntriesStart, std::uint16_t{0});
    put(Effects::kBlobEntriesStart + sizeof(std::uint16_t),
        Effects::kNoProgram);
    return blob;
  }

  SfxSelectionFixture() {
    Audio_soundState = {};
    Audio_Driver_commandArguments = {};
    Audio_Mixer_commandParams = {};
    Audio_Mixer_voiceScheduler = {};
    Audio_SoundEffects_channels[0] = {};
    Audio_SoundEffects_channels[1] = {};
    Audio_Mixer_kCommandHandlers[0x21] =
        VS::Audio::SoundEffects::LiveStopPrograms;
    Audio_Mixer_kCommandHandlers[0xA1] =
        VS::Audio::SoundEffects::LiveFadeProgramVolume;
    Audio_Mixer_kCommandHandlers[0xA3] =
        VS::Audio::SoundEffects::LiveFadeProgramPan;
    Audio_Mixer_kCommandHandlers[0xA5] =
        VS::Audio::Mixer::LiveSlideSoundEffectPitchScale;
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
    Audio_Driver_commandArguments = saved_arguments;
    Audio_Mixer_commandParams = saved_params;
    Audio_Mixer_voiceScheduler = saved_scheduler;
    Audio_Driver_controlLatches = saved_flags;
    Audio_SoundEffects_channels[0] = saved_voices[0];
    Audio_SoundEffects_channels[1] = saved_voices[1];
    Audio_Mixer_kCommandHandlers[0x21] = saved_handlers[0];
    Audio_Mixer_kCommandHandlers[0xA1] = saved_handlers[1];
    Audio_Mixer_kCommandHandlers[0xA3] = saved_handlers[2];
    Audio_Mixer_kCommandHandlers[0xA5] = saved_handlers[3];
    Audio_nextSfxChannelGroupFF000 = saved_channel;
    Audio_nextSfxChannelGroupF00000 = saved_room_channel;
    Core::SetLogObserver(std::move(observer));
    Core::SetLogLevel(Core::LogLevel::Info);
    Core::SetLogChannels("*");
  }

  std::intptr_t SelectProgram() {
    Audio_soundState.currentSfxBank = 1;
    Audio_soundState.sfxBankData[0] = &bank;
    return reinterpret_cast<std::intptr_t>(&bank.second_program);
  }

  void ActivateDistinctHighAddresses(std::intptr_t program) {
    Audio_Mixer_voiceScheduler.activeVoices = 0x3000;
    Audio_SoundEffects_channels[0].effectId = program;
    Audio_SoundEffects_channels[1].effectId =
        program + (std::intptr_t{1} << 32);
    for (unsigned index = 0; index < 2; ++index) {
      Audio_SoundEffects_channels[index].updateFlags = 0x100000;
    }
  }
};

void CheckFadeLengths(const Audio_SeqChannel& channel, int operation,
                      int duration) {
  CHECK(channel.effectVolumeSlideLength == (operation == 0 ? duration : 0));
  CHECK(channel.effectPanSlideLength == (operation == 1 ? duration : 0));
  CHECK(channel.effectPitchScaleSlideLength == (operation == 2 ? duration : 0));
}
}  // namespace

TEST_CASE(
    "Selected private SFX banks retain serialized routing and channel rotation",
    "[audio][sfx-selection]") {
  for (unsigned slot = 1; slot <= 3; ++slot) {
    SfxSelectionFixture fixture;
    Audio_soundState.currentSfxBank = slot;
    Audio_soundState.sfxBankData[slot - 1] = &fixture.bank;
    VS::Audio::LivePlaySfx(0xFF000, 0, 128, 127);
    CHECK(Audio_nextSfxChannelGroupFF000 == 0x2000);
    CHECK(Audio_soundState.currentSfxBank == slot);
    CHECK(fixture.levels.empty());
  }
}

TEST_CASE("Banked SFX resolution preserves actual native program addresses",
          "[audio][sfx-selection][sfx-reference-width]") {
  SfxSelectionFixture fixture;
  const auto group = GENERATE(0xFF000, 0xF00000);
  const auto slot = GENERATE(1U, 2U, 3U);
  Audio_soundState.currentSfxBank = slot;
  Audio_soundState.sfxBankData[slot - 1] = &fixture.bank;
  const auto expected = reinterpret_cast<std::uintptr_t>(&fixture.bank.program);
  REQUIRE(expected > std::numeric_limits<std::uint32_t>::max());
  CHECK(static_cast<std::uintptr_t>(
            VS::Audio::LiveResolveSfxProgram(group, 0)) == expected);
}

TEST_CASE("Banked stop selects the exact voice rather than its low32-bit alias",
          "[audio][sfx-selection][sfx-native-dispatch]") {
  SfxSelectionFixture fixture;
  const auto program = fixture.SelectProgram();
  fixture.ActivateDistinctHighAddresses(program);
  VS::Audio::LiveStopSfx(0xFF000, 1);
  CHECK(Audio_Mixer_commandParams.param1 ==
        static_cast<std::uintptr_t>(program));
  CHECK((Audio_SoundEffects_channels[0].updateFlags & 0x200000) != 0);
  CHECK((Audio_SoundEffects_channels[1].updateFlags & 0x200000) == 0);
}

TEST_CASE("Banked fades retain the exact selector through the real mixer",
          "[audio][sfx-selection][sfx-native-dispatch]") {
  const auto operation = GENERATE(0, 1, 2);
  const std::array<void (*)(int, int, int, int), 3> fades{
      VS::Audio::LiveFadeSfxVolume, VS::Audio::LiveFadeSfxPan,
      VS::Audio::LiveFadeSfxPitch};
  SfxSelectionFixture fixture;
  const auto program = fixture.SelectProgram();
  fixture.ActivateDistinctHighAddresses(program);
  fades[operation](0xFF000, 1, 27, 17);
  CHECK(Audio_Mixer_commandParams.param1 ==
        static_cast<std::uintptr_t>(program));
  CheckFadeLengths(Audio_SoundEffects_channels[0], operation, 17);
  CheckFadeLengths(Audio_SoundEffects_channels[1], -1, 0);
}

TEST_CASE("Banked channel queries preserve full program identity",
          "[audio][sfx-selection][sfx-native-dispatch]") {
  SfxSelectionFixture fixture;
  const auto program = fixture.SelectProgram();
  fixture.ActivateDistinctHighAddresses(program);
  Audio_SoundEffects_channels[0].effectTags = 0x1000;
  CHECK(VS::Audio::Playback::LiveSoundEffectIdForTag(0x1000) == program);
  std::array<std::intptr_t, 12> identities{};
  CHECK(VS::Audio::Control::LiveSoundEffectChannelIds(identities.data()) ==
        0x3000);
  CHECK(identities[0] == program);
  CHECK(identities[1] == program + (std::intptr_t{1} << 32));
}

TEST_CASE("Program release stops the blobs channels started on, not addresses",
          "[audio][sfx-selection][sfx-native-dispatch]") {
  SfxSelectionFixture fixture;
  const auto saved_channels = std::to_array(Audio_SoundEffects_channels);
  const auto saved_owners = std::to_array(Audio_Mixer_voiceOwners);
  std::byte* const blob = fixture.HeapBlob();
  const auto blob_address = reinterpret_cast<std::uintptr_t>(blob);
  // Channel 0 plays a shared effect; channel 1's word names the blob without
  // having started on it (#291: the kind decides, not the value).
  Audio_Mixer_voiceScheduler.activeVoices = 0x3000;
  Audio_SoundEffects_channels[0].effectId = 42;
  Audio_SoundEffects_channels[1].effectId = blob_address;
  Audio_CommandParams params{};
  params.param1 = blob_address;
  VS::Audio::SoundEffects::LivePlayBlobPrograms(&params);
  constexpr std::uint32_t kStartedVoice = 0x1000U << 10;
  CHECK((Audio_Mixer_voiceScheduler.activeVoices & kStartedVoice) != 0);
  CHECK(Audio_SoundEffects_channels[10].effectId == blob_address);
  VS::Audio::Playback::LiveStopSoundEffects(-1, 0);
  CHECK((Audio_Mixer_voiceScheduler.activeVoices & kStartedVoice) == 0);
  CHECK((Audio_Mixer_voiceScheduler.pendingKeyOff & kStartedVoice) != 0);
  CHECK((Audio_Mixer_voiceScheduler.activeVoices & 0x3000U) == 0x3000U);
  std::ranges::copy(saved_channels, std::begin(Audio_SoundEffects_channels));
  std::ranges::copy(saved_owners, std::begin(Audio_Mixer_voiceOwners));
}

TEST_CASE("Rejected SFX bindings do not dispatch a built-in fallback",
          "[audio][sfx-invalid-selection][sfx-native-dispatch]") {
  SfxSelectionFixture fixture;
  const auto program = fixture.SelectProgram();
  Audio_Driver_commandArguments.param1 = reinterpret_cast<void*>(program);
  const auto invalid = GENERATE(0, 1, 2, 3, 4);
  const auto operation = GENERATE(0, 1, 2, 3);
  if (invalid == 0) Audio_soundState.currentSfxBank = 0;
  if (invalid == 1) Audio_soundState.currentSfxBank = 4;
  if (invalid == 2) Audio_soundState.sfxBankData[0] = nullptr;
  const int effect = invalid == 3 ? -1 : invalid == 4 ? 2 : 1;
  if (operation == 0) VS::Audio::LiveStopSfx(0xFF000, effect);
  if (operation == 1) VS::Audio::LiveFadeSfxVolume(0xFF000, effect, 27, 17);
  if (operation == 2) VS::Audio::LiveFadeSfxPan(0xFF000, effect, 27, 17);
  if (operation == 3) VS::Audio::LiveFadeSfxPitch(0xFF000, effect, 27, 17);
  CHECK(Audio_Driver_commandArguments.param1 ==
        reinterpret_cast<void*>(program));
  REQUIRE(fixture.levels.size() == 1);
  CHECK(fixture.levels.front() == Core::LogLevel::Error);
}

TEST_CASE("Built-in and whole-group SFX stops keep their distinct selection",
          "[audio][sfx-selection][sfx-native-dispatch]") {
  SfxSelectionFixture fixture;
  fixture.ActivateDistinctHighAddresses(17);
  Audio_SoundEffects_channels[0].effectTags = 0x1000;
  Audio_SoundEffects_channels[1].effectTags = 2;
  SECTION("built-in identifier") { VS::Audio::LiveStopSfx(0x7E, 17); }
  SECTION("private tag group without a selected bank") {
    VS::Audio::LiveStopSfx(0xFF000, 0);
  }
  CHECK((Audio_SoundEffects_channels[0].updateFlags & 0x200000) != 0);
  CHECK((Audio_SoundEffects_channels[1].updateFlags & 0x200000) == 0);
  CHECK(fixture.levels.empty());
}

TEST_CASE(
    "Unselected private SFX do not index before the bank array or choose a "
    "fallback",
    "[audio][sfx-unselected]") {
  SfxSelectionFixture fixture;
  Audio_soundState.sfxBankData[0] = &fixture.bank;
  VS::Audio::LivePlaySfx(0xFF000, 0, 128, 127);
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
  VS::Audio::LivePlaySfx(0xFF000, 0, 128, 127);
  CHECK(Audio_nextSfxChannelGroupFF000 == 0x1000);
  REQUIRE(fixture.levels.size() == 1);
  CHECK(fixture.levels.front() == Core::LogLevel::Error);
}

TEST_CASE("Room SFX bank absence and tagged routing remain explicit",
          "[audio][sfx-selection]") {
  SfxSelectionFixture fixture;
  SECTION("absent room bank never falls back to a private bank") {
    Audio_soundState.sfxBankData[0] = &fixture.bank;
    VS::Audio::LivePlaySfx(0xF00000, 0, 128, 127);
    CHECK(Audio_nextSfxChannelGroupF00000 == 0x100000);
    REQUIRE(fixture.levels.size() == 1);
    CHECK(fixture.levels.front() == Core::LogLevel::Warn);
  }
  SECTION("present room bank retains its distinct channel group") {
    Audio_soundState.sfxBankData[2] = &fixture.bank;
    VS::Audio::LivePlaySfx(0xF00000, 0, 128, 127);
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
  VS::Audio::LivePlaySfx(category, -1, 128, 127);
  REQUIRE(fixture.levels.size() == 1);
  CHECK(fixture.levels.front() == Core::LogLevel::Error);
  CHECK(Audio_nextSfxChannelGroupFF000 == 0x1000);
  CHECK(Audio_nextSfxChannelGroupF00000 == 0x100000);
}
