#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/MixControl.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/Sfx.hpp"
#include "Audio/SfxLive.hpp"
#include "Audio/SoundBanks.hpp"
#include "Audio/SoundControl.hpp"
#include "Audio/SoundControlLive.hpp"
#include "Audio/SoundEffectPrograms.hpp"
#include "Audio/SoundEffectProgramsLive.hpp"
#include "Audio/SoundPlayback.hpp"
#include "Content/ModuleData.h"

extern "C" {
#include "Audio/SoundBanks.h"
}

namespace {
namespace Audio = VS::Audio;
namespace Control = VS::Audio::Control;
namespace Playback = VS::Audio::Playback;
namespace Seq = VS::Audio::Seq;
namespace SoundEffects = VS::Audio::SoundEffects;
using Control::Command;
using Playback::CommandArguments;
using Playback::DataRef;
using Playback::EffectSelector;

// The opaque handle enums name only kNone; the fakes' handles are made here.
template <typename Handle, typename Value>
Handle MakeHandle(Value value) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<Handle>(value);
}

DataRef Data(std::uintptr_t address) { return MakeHandle<DataRef>(address); }

EffectSelector Selector(std::intptr_t id) {
  return MakeHandle<EffectSelector>(id);
}

// The selector of the program at `address`.
EffectSelector SelectorAt(std::uintptr_t address) {
  return MakeHandle<EffectSelector>(static_cast<std::intptr_t>(address));
}

// Catch2's REQUIRE throws, but clang-tidy cannot see that; the explicit throw
// lets bugprone-unchecked-optional-access prove the accesses through it.
template <typename T>
const T& Required(const std::optional<T>& value) {
  REQUIRE(value);
  if (!value) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *value;
}

void PutWord(std::span<std::byte> bytes, std::size_t word,
             std::uint32_t value) {
  std::memcpy(bytes.subspan(word * sizeof(value), sizeof(value)).data(), &value,
              sizeof(value));
}

void PutHalf(std::span<std::byte> bytes, std::size_t offset,
             std::uint16_t value) {
  std::memcpy(bytes.subspan(offset, sizeof(value)).data(), &value,
              sizeof(value));
}

std::vector<std::byte> Words(std::initializer_list<std::uint32_t> words) {
  std::vector<std::byte> bytes(words.size() * sizeof(std::uint32_t));
  std::size_t index = 0;
  for (const std::uint32_t word : words) {
    PutWord(bytes, index, word);
    ++index;
  }
  return bytes;
}

// ---------------------------------------------------------------------------
// Sound control requests

struct Issued {
  Command command = Command::kStopAllMusic;
  CommandArguments arguments;
};

class FakeControl final : public Control::ControlHost {
 public:
  std::vector<Issued> issued;
  std::uint32_t result = 0;
  std::uint32_t behavior = 0;
  bool sequence = true;
  std::optional<DataRef> checked;
  std::uint32_t active = 0;
  std::array<EffectSelector, Playback::kEffectVoiceCount> effects{};
  std::size_t effect_reads = 0;

  std::uint32_t Dispatch(Command command,
                         const CommandArguments& arguments) override {
    issued.push_back({.command = command, .arguments = arguments});
    return result;
  }
  std::uint32_t MixBehavior() override { return behavior; }
  void SetMixBehavior(std::uint32_t value) override { behavior = value; }
  bool IsSequence(DataRef data) override {
    checked = data;
    return sequence;
  }
  std::uint32_t ActiveVoices() override { return active; }
  EffectSelector EffectAt(std::size_t voice) override {
    ++effect_reads;
    return effects.at(voice);
  }

  [[nodiscard]] std::vector<Command> Commands() const {
    std::vector<Command> commands;
    commands.reserve(issued.size());
    for (const Issued& entry : issued) commands.push_back(entry.command);
    return commands;
  }
  [[nodiscard]] const CommandArguments& Arguments(std::size_t index) const {
    return issued.at(index).arguments;
  }
};

CommandArguments Words1(std::intptr_t first) { return {.first = first}; }

CommandArguments Words2(std::intptr_t first, std::uint32_t second) {
  return {.first = first, .second = second};
}

CommandArguments Words3(std::intptr_t first, std::uint32_t second,
                        std::uint32_t third) {
  return {.first = first, .second = second, .third = third};
}

CommandArguments Words4(std::intptr_t first, std::uint32_t second,
                        std::uint32_t third, std::uint32_t fourth) {
  return {.first = first, .second = second, .third = third, .fourth = fourth};
}
}  // namespace

TEST_CASE("Pause requests map each playback group to its mixer command",
          "[audio-control-sfx][audio]") {
  FakeControl host;
  Control::PausePlaybackGroup(host, Control::PlaybackGroup::kAll);
  Control::PausePlaybackGroup(host, Control::PlaybackGroup::kMusic);
  Control::PausePlaybackGroup(host, Control::PlaybackGroup::kWorldEffects);
  Control::PausePlaybackGroup(host, Control::PlaybackGroup::kStream);
  Control::PausePlaybackGroup(host, MakeHandle<Control::PlaybackGroup>(7U));
  CHECK(host.Commands() == std::vector{Command::kPauseAll, Command::kPauseMusic,
                                       Command::kPauseWorldEffects,
                                       Command::kPauseStream,
                                       Command::kPauseAll});
  CHECK(host.Arguments(0) == CommandArguments{});
  CHECK(host.Arguments(4) == CommandArguments{});
}

TEST_CASE("Resume requests map each playback group to its mixer command",
          "[audio-control-sfx][audio]") {
  FakeControl host;
  Control::ResumePlaybackGroup(host, Control::PlaybackGroup::kAll);
  Control::ResumePlaybackGroup(host, Control::PlaybackGroup::kMusic);
  Control::ResumePlaybackGroup(host, Control::PlaybackGroup::kWorldEffects);
  Control::ResumePlaybackGroup(host, Control::PlaybackGroup::kStream);
  Control::ResumePlaybackGroup(host,
                               MakeHandle<Control::PlaybackGroup>(0x100U));
  CHECK(host.Commands() ==
        std::vector{Command::kResumeAll, Command::kResumeMusic,
                    Command::kResumeWorldEffects, Command::kResumeStream,
                    Command::kResumeAll});
  CHECK(host.Arguments(1) == CommandArguments{});
}

TEST_CASE("Effect requests mask their tags and levels into the command words",
          "[audio-control-sfx][audio]") {
  FakeControl host;
  const Control::EffectTarget target{.effect = Selector(0x1234), .tags = -1};
  Control::SetSoundEffectVolume(host, target, 0x1FF);
  Control::FadeSoundEffectVolume(host, target, {.ticks = 30, .target = 0x80});
  Control::SetSoundEffectPan(host, target, 0x1C0);
  Control::FadeSoundEffectPan(host, target, {.ticks = -2, .target = 0x1FF});
  Control::SetSoundEffectPitch(host, {.tags = 0x30}, 0x180);
  Control::FadeSoundEffectPitch(host, {.tags = 0x30},
                                {.ticks = 4, .target = 0x1234});
  CHECK(host.Commands() ==
        std::vector{Command::kSetEffectVolume, Command::kFadeEffectVolume,
                    Command::kSetEffectPan, Command::kFadeEffectPan,
                    Command::kSetEffectPitch, Command::kFadeEffectPitch});
  CHECK(host.Arguments(0) == Words3(0x1234, 0xFFFFFF, 0x7F));
  CHECK(host.Arguments(1) == Words4(0x1234, 0xFFFFFF, 30, 0));
  CHECK(host.Arguments(2) == Words3(0x1234, 0xFFFFFF, 0xC0));
  CHECK(host.Arguments(3) == Words4(0x1234, 0xFFFFFF, 0xFFFFFFFE, 0xFF));
  CHECK(host.Arguments(4) == Words3(0, 0x30, 0x80));
  CHECK(host.Arguments(5) == Words4(0, 0x30, 4, 0x34));
}

TEST_CASE("Scene effect requests pass signed ticks and masked levels",
          "[audio-control-sfx][audio]") {
  FakeControl host;
  Control::SetSceneSoundEffectVolume(host, 0xFFFFFFFFU);
  Control::FadeSceneSoundEffectVolume(host, {.ticks = -1, .target = 0x1FF});
  Control::SetSceneSoundEffectPan(host, -1);
  Control::FadeSceneSoundEffectPan(host, {.ticks = 10, .target = 0x1FF});
  Control::SetSceneSoundEffectPitch(host, 0x1234);
  Control::FadeSceneSoundEffectPitch(host, {.ticks = 5, .target = 0x1234});
  CHECK(host.Commands() ==
        std::vector{
            Command::kSetSceneEffectVolume, Command::kFadeSceneEffectVolume,
            Command::kSetSceneEffectPan, Command::kFadeSceneEffectPan,
            Command::kSetSceneEffectPitch, Command::kFadeSceneEffectPitch});
  CHECK(host.Arguments(0) == Words1(0x7F));
  CHECK(host.Arguments(1) == Words2(-1, 0x7F));
  CHECK(host.Arguments(2) == Words1(0xFF));
  CHECK(host.Arguments(3) == Words2(10, 0xFF));
  CHECK(host.Arguments(4) == Words1(0x34));
  CHECK(host.Arguments(5) == Words2(5, 0x34));
}

TEST_CASE("Music volume requests keep the music id whole and mask the volume",
          "[audio-control-sfx][audio]") {
  FakeControl host;
  Control::SetMusicVolume(host, 0x80000001U, 0xFF);
  Control::FadeMusicVolume(host, 3, {.ticks = -1, .target = 0x80});
  Control::FadeMusicVolumeFrom(host, 4,
                               {.ticks = 60, .start = 0xFF, .target = 0x40});
  CHECK(host.Commands() == std::vector{Command::kSetMusicVolume,
                                       Command::kFadeMusicVolume,
                                       Command::kFadeMusicVolumeFrom});
  CHECK(host.Arguments(0) == Words2(0x80000001, 0x7F));
  CHECK(host.Arguments(1) == Words3(3, 0xFFFFFFFF, 0));
  CHECK(host.Arguments(2) == Words4(4, 60, 0x7F, 0x40));
}

TEST_CASE("Disc volume requests keep their volume words whole",
          "[audio-control-sfx][audio]") {
  FakeControl host;
  Control::SetDiscVolume(host, -1);
  Control::FadeDiscVolume(host, {.ticks = -1, .target = 0x1FF});
  Control::FadeDiscVolumeFrom(host, {.ticks = 2, .start = -1, .target = 0x100});
  CHECK(host.Commands() == std::vector{Command::kSetDiscVolume,
                                       Command::kFadeDiscVolume,
                                       Command::kFadeDiscVolumeFrom});
  CHECK(host.Arguments(0) == Words1(-1));
  CHECK(host.Arguments(1) == Words2(-1, 0x1FF));
  CHECK(host.Arguments(2) == Words3(2, 0xFFFFFFFF, 0x100));
}

TEST_CASE("Tempo and pitch requests keep the low byte of their scales",
          "[audio-control-sfx][audio]") {
  FakeControl host;
  Control::SetMusicTempoScale(host, 0x1FF);
  Control::FadeMusicTempoScale(host, {.ticks = -1, .target = 0x180});
  Control::FadeMusicTempoScaleFrom(
      host, {.ticks = 8, .start = 0x101, .target = 0x2FF});
  Control::SetMusicPitchScale(host, 0x140);
  Control::FadeMusicPitchScaleFrom(host, {.ticks = 1, .start = 2, .target = 3});
  Control::SetMusicTempoAndPitch(host, 0x100);
  Control::FadeMusicTempoAndPitch(host, {.ticks = 6, .target = 0x1AB});
  Control::FadeMusicTempoAndPitchFrom(
      host, {.ticks = 7, .start = 0x110, .target = 0});
  CHECK(host.Commands() ==
        std::vector{Command::kSetTempoScale, Command::kFadeTempoScale,
                    Command::kFadeTempoScaleFrom, Command::kSetPitchScale,
                    Command::kFadePitchScaleFrom, Command::kSetTempoAndPitch,
                    Command::kFadeTempoAndPitch,
                    Command::kFadeTempoAndPitchFrom});
  CHECK(host.Arguments(0) == Words1(0xFF));
  CHECK(host.Arguments(1) == Words2(-1, 0x80));
  CHECK(host.Arguments(2) == Words3(8, 1, 0xFF));
  CHECK(host.Arguments(3) == Words1(0x40));
  CHECK(host.Arguments(5) == Words1(0));
  CHECK(host.Arguments(6) == Words2(6, 0xAB));
  CHECK(host.Arguments(7) == Words3(7, 0x10, 0));
}

TEST_CASE("A music pitch fade returns the dispatch result as a signed word",
          "[audio-control-sfx][audio]") {
  FakeControl host;
  host.result = 0xFFFFFFFFU;
  CHECK(Control::FadeMusicPitchScale(host, {.ticks = 4, .target = 0x180}) ==
        -1);
  host.result = 9;
  CHECK(Control::FadeMusicPitchScale(host, {.ticks = -3, .target = 1}) == 9);
  CHECK(host.Commands() ==
        std::vector{Command::kFadePitchScale, Command::kFadePitchScale});
  CHECK(host.Arguments(0) == Words2(4, 0x80));
  CHECK(host.Arguments(1) == Words2(-3, 1));
}

TEST_CASE("A cutscene stream starts only when its data is a sequence",
          "[audio-control-sfx][audio]") {
  FakeControl host;
  host.sequence = false;
  Control::StartCutsceneStream(
      host, {.stream = Data(0x2000), .pan = 0x40, .reverb = 1});
  CHECK(host.issued.empty());
  CHECK(host.checked == Data(0x2000));
  host.sequence = true;
  Control::StartCutsceneStream(
      host, {.stream = Data(0x2000), .pan = 0x1C0, .reverb = -1});
  REQUIRE(host.issued.size() == 1);
  CHECK(host.issued[0].command == Command::kStartStream);
  const CommandArguments expected{
      .first = Data(0x2000), .second = 0xC000U, .third = 0xFFFFFFFFU};
  CHECK(host.Arguments(0) == expected);
}

TEST_CASE("Cutscene stream levels move to the high byte of a 16-bit level",
          "[audio-control-sfx][audio]") {
  FakeControl host;
  Control::SetCutsceneVolume(host, 0xFF);
  Control::FadeCutsceneVolume(host, {.ticks = -1, .target = 0x80});
  Control::SetCutscenePan(host, 0x1FF);
  Control::StopCutsceneStream(host);
  CHECK(host.Commands() ==
        std::vector{Command::kSetStreamVolume, Command::kFadeStreamVolume,
                    Command::kSetStreamPan, Command::kStopStream});
  CHECK(host.Arguments(0) == Words1(0x7F00));
  CHECK(host.Arguments(1) == Words2(-1, 0));
  CHECK(host.Arguments(2) == Words1(0xFF00));
  CHECK(host.Arguments(3) == CommandArguments{});
}

TEST_CASE("Mute mask and branch threshold differ in their sign extension",
          "[audio-control-sfx][audio]") {
  FakeControl host;
  Control::SetMusicMuteMask(host, 0xFFFFFFFFU);
  Control::SetMusicBranchThreshold(host, -1);
  Control::StopAllMusic(host);
  Control::StopSceneSoundEffects(host);
  CHECK(host.Commands() == std::vector{Command::kSetMusicMuteMask,
                                       Command::kSetMusicBranchThreshold,
                                       Command::kStopAllMusic,
                                       Command::kStopSceneEffects});
  CHECK(host.Arguments(0) == Words1(std::intptr_t{0xFFFFFFFF}));
  CHECK(host.Arguments(1) == Words1(-1));
  CHECK(host.Arguments(2) == CommandArguments{});
  CHECK(host.Arguments(3) == CommandArguments{});
}

TEST_CASE("The mix option latch sets and clears only its bit",
          "[audio-control-sfx][audio]") {
  FakeControl host;
  host.behavior = 0x21;
  Control::SetMixOptionLatch(host, true);
  CHECK(host.behavior == 0x31U);
  Control::SetMixOptionLatch(host, true);
  CHECK(host.behavior == 0x31U);
  Control::SetMixOptionLatch(host, false);
  CHECK(host.behavior == 0x21U);
  CHECK(host.issued.empty());
}

TEST_CASE("Sound effect channel ids copy each voice's effect",
          "[audio-control-sfx][audio]") {
  FakeControl host;
  for (std::size_t voice = 0; voice < host.effects.size(); ++voice) {
    host.effects.at(voice) =
        Selector(0x100 + static_cast<std::intptr_t>(voice));
  }
  host.active = 0x5000;
  std::array<EffectSelector, Playback::kEffectVoiceCount> out{};
  CHECK(Control::SoundEffectChannelIds(host, out) == 0x5000U);
  CHECK(out[0] == Selector(0x100));
  CHECK(out[5] == Selector(0x105));
  CHECK(out[11] == Selector(0x10B));
  CHECK(host.effect_reads == Playback::kEffectVoiceCount);
  CHECK(host.issued.empty());
}

// ---------------------------------------------------------------------------
// Sound-effect programs

namespace {
using SoundEffects::EffectChannel;
using SoundEffects::EffectCommand;
using SoundEffects::EffectKind;
using SoundEffects::EffectRefusal;
using SoundEffects::EffectState;
using SoundEffects::ProgramPair;
using SoundEffects::ProgramStart;

constexpr std::uint32_t kNoVoice = Audio::Mixer::kNoVoice;
constexpr std::uint32_t kFirstEffectVoice = 12;
constexpr std::int32_t kPairRelease = std::numeric_limits<std::int32_t>::min();

std::uint32_t Voice(std::size_t channel) {
  return SoundEffects::kFirstVoiceBit << channel;
}

struct VoiceRelease {
  std::uint32_t voice = 0;
  std::int32_t rate = 0;
  std::uint32_t mode = 0;

  friend bool operator==(const VoiceRelease&, const VoiceRelease&) = default;
};

class FakeEffects final : public SoundEffects::EffectHost {
 public:
  std::vector<std::pair<std::size_t, ProgramStart>> resets;
  std::vector<std::uint32_t> cleared;
  std::vector<VoiceRelease> releases;
  std::optional<SoundEffects::SharedTables> tables;
  std::vector<std::byte> blob;
  DataRef blob_ref = DataRef::kNone;
  std::uint32_t slot = 0;
  std::vector<std::int32_t> slot_selectors;
  std::vector<EffectRefusal> refusals;
  // ResetChannel assigns channel `index` voice `voice_base + index`.
  std::uint32_t voice_base = kFirstEffectVoice;

  void ResetChannel(std::size_t index, EffectChannel& channel,
                    ProgramStart program) override {
    resets.emplace_back(index, program);
    channel.assigned_voice = voice_base + static_cast<std::uint32_t>(index);
  }
  void ClearVoiceOwner(std::uint32_t voice) override {
    cleared.push_back(voice);
  }
  void SetVoiceRelease(std::uint32_t voice, std::int32_t rate,
                       std::uint32_t mode) override {
    releases.push_back({.voice = voice, .rate = rate, .mode = mode});
  }
  std::optional<SoundEffects::SharedTables> Tables() override { return tables; }
  std::uint32_t BankSlot(std::int32_t selector) override {
    slot_selectors.push_back(selector);
    return slot;
  }
  std::span<const std::byte> ProgramBytes(DataRef data) override {
    if (data != blob_ref) return {};
    return blob;
  }
  void Refuse(EffectRefusal refusal) override { refusals.push_back(refusal); }
};

// No music channel holds a voice.
EffectState FreshState() {
  EffectState state;
  state.music_voices.fill(kNoVoice);
  return state;
}

ProgramStart At(std::uintptr_t data, std::size_t offset = 0) {
  return {.data = Data(data), .offset = offset};
}

using Reset = std::pair<std::size_t, ProgramStart>;
}  // namespace

TEST_CASE("Releasing by tag key-offs the held channels carrying the tag",
          "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  state.scheduler.active_voices = Voice(0) | Voice(1);
  state.scheduler.paused_effect_voices = Voice(2);
  state.channels[0] = {.effect = Selector(5), .tags = 0x10, .update_flags = 8};
  state.channels[1] = {.effect = Selector(6), .tags = 0x20};
  state.channels[2] = {.effect = Selector(7),
                       .tags = 0x30,
                       .update_flags = Seq::kLoopBoundaryStop};
  SoundEffects::ReleaseMatchingVoices(state, host, EffectSelector::kNone, 0x10);
  CHECK(state.scheduler.pending_key_off == Voice(0));
  CHECK(state.scheduler.active_voices == Voice(1));
  CHECK(state.channels[0].effect == EffectSelector::kNone);
  CHECK(state.channels[0].update_flags == 0U);
  CHECK(state.channels[1].effect == Selector(6));
  CHECK(state.channels[2].update_flags ==
        (Seq::kLoopBoundaryStop | Seq::kStopAtEndlessLoop));
  CHECK(state.scheduler.paused_effect_voices == Voice(2));
  CHECK(state.driver_update_flags == SoundEffects::kVoiceUpdate);
}

TEST_CASE(
    "Releasing by selector matches an effect, every program or the "
    "untagged effects",
    "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  state.scheduler.active_voices = Voice(0) | Voice(1) | Voice(2) | Voice(3);
  state.channels[0] = {.effect = Selector(5), .tags = 1};
  // A program's identity below the PS1 floor, and a shared word above it:
  // the kind decides, not the address (#291).
  state.channels[1] = {
      .effect = Selector(0x9000), .kind = EffectKind::kProgram, .tags = 1};
  state.channels[2] = {.effect = Selector(7)};
  state.channels[3] = {.effect = Selector(0x80010000), .tags = 1};
  SoundEffects::ReleaseMatchingVoices(state, host, Selector(5), 0);
  CHECK(state.scheduler.pending_key_off == Voice(0));
  SoundEffects::ReleaseMatchingVoices(state, host,
                                      SoundEffects::kProgramEffects, 0);
  CHECK(state.scheduler.pending_key_off == (Voice(0) | Voice(1)));
  SoundEffects::ReleaseMatchingVoices(state, host,
                                      SoundEffects::kUntaggedEffects, 0);
  CHECK(state.scheduler.pending_key_off == (Voice(0) | Voice(1) | Voice(2)));
  CHECK(state.scheduler.active_voices == Voice(3));
  CHECK(state.driver_update_flags == SoundEffects::kVoiceUpdate);
  CHECK(host.refusals.empty());
}

TEST_CASE("A negative release frees the effects of the numbered channel pair",
          "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  state.scheduler.active_voices = Voice(0) | Voice(1) | Voice(3) | Voice(4);
  state.channels[0] = {.effect = Selector(5), .tags = 1};
  state.channels[1] = {.effect = Selector(6), .tags = 1};
  state.channels[3] = {.effect = Selector(5), .tags = 2};
  state.channels[4] = {.effect = Selector(9), .tags = 2};
  SoundEffects::ReleaseMatchingVoices(state, host, Selector(0), kPairRelease);
  CHECK(state.scheduler.active_voices == Voice(4));
  CHECK(state.scheduler.pending_key_off == (Voice(0) | Voice(1) | Voice(3)));
  CHECK(state.driver_update_flags == SoundEffects::kVoiceUpdate);
  SoundEffects::ReleaseMatchingVoices(state, host, Selector(31), kPairRelease);
  CHECK(host.refusals.empty());
  SoundEffects::ReleaseMatchingVoices(state, host, Selector(32), kPairRelease);
  SoundEffects::ReleaseMatchingVoices(state, host, Selector(-1), kPairRelease);
  CHECK(host.refusals == std::vector{EffectRefusal::kChannelOutOfRange,
                                     EffectRefusal::kChannelOutOfRange});
  CHECK(state.scheduler.active_voices == Voice(4));
}

TEST_CASE("Evicting untagged channels releases those of the highest priority",
          "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  state.scheduler.active_voices = Voice(0) | Voice(1) | Voice(2) | Voice(3);
  state.channels[0] = {.effect = Selector(1), .priority = 3};
  state.channels[1] = {.effect = Selector(2), .priority = 5};
  state.channels[2] = {.effect = Selector(3), .tags = 4, .priority = 9};
  state.channels[3] = {.effect = Selector(4), .priority = 5};
  SoundEffects::ReleaseMatchingVoices(state, host, EffectSelector::kNone,
                                      SoundEffects::kEvictUntagged);
  CHECK(state.scheduler.active_voices == (Voice(0) | Voice(2)));
  CHECK(state.scheduler.pending_key_off == (Voice(1) | Voice(3)));
  CHECK(state.driver_update_flags == SoundEffects::kVoiceUpdate);
  // The highest priority starts at 0: negative priorities are never evicted.
  state.channels[0].priority = -2;
  SoundEffects::ReleaseMatchingVoices(state, host, EffectSelector::kNone,
                                      SoundEffects::kEvictUntagged);
  CHECK(state.scheduler.active_voices == (Voice(0) | Voice(2)));
}

TEST_CASE("A program pair starts on the highest free channel pair",
          "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  const EffectCommand command{
      .first = Selector(0x42), .third = 0x1C0, .fourth = 0xFF, .bank_slot = 33};
  const ProgramPair programs{.first = At(0x1000, 4), .second = At(0x1000, 8)};
  SoundEffects::PlayProgramPair(state, host, command, programs, false);
  CHECK(host.resets ==
        std::vector{Reset{10, At(0x1000, 4)}, Reset{11, At(0x1000, 8)}});
  CHECK(state.channels[10].effect == Selector(0x42));
  CHECK(state.channels[10].effect_pan == 0xC000);
  CHECK(state.channels[10].effect_volume == 0x7F00);
  CHECK(state.channels[10].voice_mask == 2);
  CHECK(state.channels[11].update_flags == SoundEffects::kStereoLinked);
  CHECK(state.scheduler.active_voices == (Voice(10) | Voice(11)));
  CHECK(host.cleared == std::vector{22U, 23U});
}

TEST_CASE("A started channel resets its sequence state and its voice",
          "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  state.scheduler.keyed_voices = Voice(4) | Voice(5);
  state.scheduler.reverb_voices = Voice(4);
  state.channels[4].pitch_scale = 9;
  SoundEffects::StartChannelProgram(state, host, 4,
                                    {.first = Selector(3), .second = 0x40},
                                    Voice(4), At(0x2000));
  CHECK(state.channels[4].priority == SoundEffects::kStartPriority);
  CHECK(state.channels[4].pan == SoundEffects::kCenteredChannelPan);
  CHECK(state.channels[4].type == SoundEffects::kEffectChannelType);
  CHECK(state.channels[4].pitch_scale == 0);
  CHECK(state.channels[4].tags == 0x40);
  CHECK(host.releases == std::vector{VoiceRelease{16, 5, 3}});
  CHECK(state.scheduler.keyed_voices == Voice(5));
  CHECK(state.scheduler.reverb_voices == 0U);
}

TEST_CASE(
    "A start refuses a channel past the effects and a voice past the "
    "voices",
    "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  SoundEffects::StartChannelProgram(state, host, SoundEffects::kChannelCount,
                                    {}, 0, At(0x2000));
  CHECK(host.resets.empty());
  CHECK(host.refusals == std::vector{EffectRefusal::kChannelOutOfRange});
  host.voice_base = kNoVoice;
  SoundEffects::StartChannelProgram(state, host, 0, {}, Voice(0), At(0x2000));
  CHECK(host.cleared.empty());
  CHECK(host.refusals.back() == EffectRefusal::kVoiceOutOfRange);
  CHECK(host.releases == std::vector{VoiceRelease{kNoVoice, 5, 3}});
  CHECK(state.scheduler.active_voices == Voice(0));
}

TEST_CASE(
    "Starts while effects are paused move the scene effects to the "
    "paused voices",
    "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  state.stream_control_flags = Audio::Mixer::kEffectsPaused;
  SoundEffects::StartChannelProgram(
      state, host, 0, {.second = SoundEffects::kInterfaceEffectTag}, Voice(0),
      At(0x2000));
  CHECK(state.scheduler.active_voices == Voice(0));
  SoundEffects::StartChannelProgram(state, host, 1, {.second = 0x10}, Voice(1),
                                    At(0x2000));
  CHECK(state.scheduler.active_voices == Voice(0));
  CHECK(state.scheduler.paused_effect_voices == Voice(1));
  CHECK(state.scheduler.pending_key_off == (Voice(0) | Voice(1)));
}

TEST_CASE("A lone second program starts on the pair's first channel unlinked",
          "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  SoundEffects::PlayProgramPair(state, host, {.first = Selector(1)},
                                {.second = At(0x3000)}, false);
  CHECK(host.resets == std::vector{Reset{10, At(0x3000)}});
  CHECK(state.channels[10].update_flags == 0U);
  SoundEffects::PlayProgramPair(state, host, {.first = Selector(1)}, {}, false);
  CHECK(host.resets.size() == 1);
  SoundEffects::PlayProgramPair(state, host, {.first = Selector(2)},
                                {.first = At(0x3000)}, false);
  CHECK(host.resets.back() == Reset{8, At(0x3000)});
}

TEST_CASE(
    "A tagged program pair first releases the channels carrying its "
    "tags",
    "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  state.scheduler.active_voices = Voice(0);
  state.channels[0] = {.effect = Selector(4), .tags = 8};
  SoundEffects::PlayProgramPair(state, host, {.second = 8},
                                {.first = At(0x3000)}, true);
  CHECK((state.scheduler.active_voices & Voice(0)) != 0U);
  SoundEffects::PlayProgramPair(state, host, {.second = 8},
                                {.first = At(0x3000)}, false);
  // The release also frees channel 10, which the first play tagged, so the
  // second play reuses pair 10.
  CHECK(state.scheduler.active_voices == Voice(10));
  CHECK(state.scheduler.pending_key_off == (Voice(0) | Voice(10)));
  CHECK(host.resets ==
        std::vector{Reset{10, At(0x3000)}, Reset{10, At(0x3000)}});
}

TEST_CASE("A full channel set evicts untagged channels until a pair frees",
          "[audio-control-sfx][audio]") {
  constexpr std::uint32_t kAllCommandVoices = 0xFFF000;
  EffectState state = FreshState();
  FakeEffects host;
  state.stream_voices = kAllCommandVoices;
  SoundEffects::PlayProgramPair(state, host, {}, {.first = At(0x3000)}, false);
  CHECK(host.resets.empty());
  state.stream_voices = 0;
  state.scheduler.active_voices = kAllCommandVoices;
  for (EffectChannel& channel : state.channels) channel.priority = -2;
  SoundEffects::PlayProgramPair(state, host, {}, {.first = At(0x3000)}, false);
  CHECK(host.resets.empty());
  for (EffectChannel& channel : state.channels) channel.priority = 1;
  SoundEffects::PlayProgramPair(state, host, {}, {.first = At(0x3000)}, false);
  CHECK(host.resets == std::vector{Reset{10, At(0x3000)}});
  CHECK(state.scheduler.active_voices == Voice(10));
}

TEST_CASE("Program counters refuse unbound tables and ids past the table",
          "[audio-control-sfx][audio]") {
  FakeEffects host;
  CHECK(SoundEffects::ProgramCounters(host, 0) == ProgramPair{});
  CHECK(host.refusals == std::vector{EffectRefusal::kTablesUnbound});
  const std::array<std::uint32_t, 2> offsets{0x0004FFFF, 0x00100008};
  host.tables = SoundEffects::SharedTables{.program_offsets = offsets,
                                           .program_data = Data(0x5000),
                                           .program_bytes = 0x20};
  CHECK(SoundEffects::ProgramCounters(host, 0) ==
        ProgramPair{.second = At(0x5000, 4)});
  CHECK(SoundEffects::ProgramCounters(host, 0x401) ==
        (ProgramPair{.first = At(0x5000, 8), .second = At(0x5000, 0x10)}));
  CHECK(SoundEffects::ProgramCounters(host, 2) == ProgramPair{});
  CHECK(host.refusals.back() == EffectRefusal::kEffectOutOfRange);
}

TEST_CASE("Program counters refuse offsets past the program data",
          "[audio-control-sfx][audio]") {
  FakeEffects host;
  const std::array<std::uint32_t, 1> offsets{0x00300020};
  host.tables = SoundEffects::SharedTables{.program_offsets = offsets,
                                           .program_data = Data(0x5000),
                                           .program_bytes = 0x20};
  CHECK(SoundEffects::ProgramCounters(host, 0) == ProgramPair{});
  CHECK(host.refusals == std::vector{EffectRefusal::kProgramOutOfRange,
                                     EffectRefusal::kProgramOutOfRange});
}

TEST_CASE("Volume invalidation marks the active music and effect channels",
          "[audio-control-sfx][audio]") {
  std::array<std::uint32_t, Seq::kMusicChannelCount> dirty{};
  SoundEffects::InvalidateMusicChannelVolumes(0x80000001U, dirty);
  CHECK(dirty[0] == Seq::kVolumeDirty);
  CHECK(dirty[1] == 0U);
  CHECK(dirty[31] == Seq::kVolumeDirty);
  EffectState state = FreshState();
  FakeEffects host;
  state.scheduler.active_voices = Voice(0) | Voice(19);
  SoundEffects::InvalidateSoundEffectVolumes(state, host);
  CHECK(state.channels[0].dirty_flags == Seq::kVolumeDirty);
  CHECK(state.channels[19].dirty_flags == Seq::kVolumeDirty);
  CHECK(state.channels[1].dirty_flags == 0U);
  CHECK(host.refusals.empty());
}

TEST_CASE("Effect volume invalidation refuses voices below the effect voices",
          "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  SoundEffects::InvalidateSoundEffectVolumes(state, host);
  CHECK(host.refusals.empty());
  state.scheduler.active_voices = 1U | Voice(1);
  SoundEffects::InvalidateSoundEffectVolumes(state, host);
  CHECK(host.refusals == std::vector{EffectRefusal::kVoiceOutsideEffects});
  CHECK(state.channels[1].dirty_flags == Seq::kVolumeDirty);
  CHECK(state.channels[0].dirty_flags == 0U);
}

TEST_CASE("Detaching a music voice clears the active layer's note bits",
          "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  state.music_voices[3] = 22;
  state.music_voices[7] = 22;
  state.music_voices[8] = 21;
  state.active_music_notes = 0xFFU;
  SoundEffects::DetachMusicVoice(state, host, 22);
  CHECK(state.music_voices[3] == kNoVoice);
  CHECK(state.music_voices[7] == kNoVoice);
  CHECK(state.music_voices[8] == 21U);
  CHECK(state.active_music_notes == 0x77U);
  SoundEffects::DetachMusicVoice(state, host, kNoVoice);
  CHECK(state.music_voices[8] == 21U);
  state.active_music_notes.reset();
  SoundEffects::DetachMusicVoice(state, host, 21);
  CHECK(state.music_voices[8] == kNoVoice);
  CHECK(host.refusals == std::vector{EffectRefusal::kNoActiveMusic});
}

namespace {
constexpr std::uintptr_t kBlobAddress = 0x9000;

// A blob of two entries: entry 0's first program at byte 0x24, entry 1's
// second program at byte 0x30.
std::vector<std::byte> TwoEntryBlob(std::uint32_t count) {
  std::vector<std::byte> blob(0x40);
  PutWord(blob, SoundEffects::kBlobCountWord, count);
  PutWord(blob, SoundEffects::kBlobSelectorWord, 5);
  PutWord(blob, 4, 0);
  PutWord(blob, 5, 8);
  PutHalf(blob, 0x20, 0);
  PutHalf(blob, 0x22, SoundEffects::kNoProgram);
  PutHalf(blob, 0x28, SoundEffects::kNoProgram);
  PutHalf(blob, 0x2A, 4);
  return blob;
}

EffectCommand BlobCommand(std::uint32_t tags) {
  return {.first = SelectorAt(kBlobAddress), .second = tags};
}
}  // namespace

TEST_CASE(
    "The interface program plays a shared effect centered at full "
    "volume",
    "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  const std::array<std::uint32_t, 2> offsets{0xFFFFFFFF, 0xFFFF0004};
  const std::array<std::uint16_t, 2> selectors{7, 9};
  host.tables = SoundEffects::SharedTables{.program_offsets = offsets,
                                           .bank_selectors = selectors,
                                           .program_data = Data(0x5000),
                                           .program_bytes = 0x20};
  host.slot = 33;
  EffectCommand command{.first = Selector(1), .third = 0x10};
  SoundEffects::PlayInterfaceProgram(state, host, command);
  CHECK(command == EffectCommand{.first = Selector(1),
                                 .second = SoundEffects::kInterfaceEffectTag,
                                 .third = 0x80,
                                 .fourth = 0x7F,
                                 .bank_slot = 33});
  CHECK(host.slot_selectors == std::vector<std::int32_t>{9});
  CHECK(host.resets == std::vector{Reset{10, At(0x5000, 4)}});
  CHECK(state.channels[10].effect_pan == 0x8000);
  CHECK(state.channels[10].effect_volume == 0x7F00);
  CHECK(state.channels[10].voice_mask == 2);
}

TEST_CASE("A shared program reads its bank selector with the unmasked id",
          "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  EffectCommand unbound{.first = Selector(1)};
  SoundEffects::PlaySharedProgram(state, host, unbound);
  CHECK(host.refusals == std::vector{EffectRefusal::kTablesUnbound});
  const std::array<std::uint32_t, 2> offsets{0xFFFFFFFF, 0xFFFF0004};
  const std::array<std::uint16_t, 2> selectors{7, 9};
  host.tables = SoundEffects::SharedTables{.program_offsets = offsets,
                                           .bank_selectors = selectors,
                                           .program_data = Data(0x5000),
                                           .program_bytes = 0x20};
  EffectCommand masked{.first = Selector(0x401), .second = 0x10};
  SoundEffects::PlaySharedProgram(state, host, masked);
  CHECK(host.refusals.back() == EffectRefusal::kEffectOutOfRange);
  CHECK(host.resets.empty());
  EffectCommand shared{.first = Selector(1), .second = 0x10};
  SoundEffects::PlaySharedProgram(state, host, shared);
  CHECK(host.resets == std::vector{Reset{10, At(0x5000, 4)}});
  CHECK(state.channels[10].tags == 0x10);
}

TEST_CASE("The interface pair plays two data programs on a fixed tag",
          "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  EffectCommand command{.first = Selector(0x6000), .second = 0x7000};
  SoundEffects::PlayInterfaceProgramPair(state, host, command);
  CHECK(command == EffectCommand{.first = SoundEffects::kInterfacePairEffect,
                                 .second = SoundEffects::kInterfacePairTag,
                                 .third = 0x80,
                                 .fourth = 0x7F});
  CHECK(host.resets ==
        std::vector{Reset{10, At(0x6000)}, Reset{11, At(0x7000)}});
  CHECK(state.channels[11].update_flags == SoundEffects::kStereoLinked);
  CHECK(state.channels[10].tags == 0x01000000);
  EffectCommand empty{};
  SoundEffects::PlayInterfaceProgramPair(state, host, empty);
  CHECK(host.resets.size() == 2);
  CHECK(empty.first == SoundEffects::kInterfacePairEffect);
}

TEST_CASE("A blob plays each entry and releases its tags only for the first",
          "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  host.blob = TwoEntryBlob(2);
  host.blob_ref = Data(kBlobAddress);
  host.slot = 4;
  EffectCommand command = BlobCommand(0x10);
  SoundEffects::PlayBlobPrograms(state, host, command);
  CHECK(host.slot_selectors == std::vector<std::int32_t>{5});
  CHECK(command.bank_slot == 4U);
  CHECK(host.resets == std::vector{Reset{10, At(kBlobAddress, 0x24)},
                                   Reset{8, At(kBlobAddress, 0x30)}});
  CHECK(state.scheduler.active_voices == (Voice(8) | Voice(10)));
  CHECK(state.channels[8].update_flags == 0U);
  CHECK(host.refusals.empty());
}

TEST_CASE("A blob's channels are programs whatever their address",
          "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  host.blob = TwoEntryBlob(2);
  host.blob_ref = Data(kBlobAddress);
  state.scheduler.active_voices = Voice(0);
  state.channels[0] = {.effect = Selector(0x80010000)};
  EffectCommand command = BlobCommand(0);
  SoundEffects::PlayBlobPrograms(state, host, command);
  CHECK(state.channels[10].effect == SelectorAt(kBlobAddress));
  CHECK(state.channels[10].kind == EffectKind::kProgram);
  CHECK(state.channels[8].kind == EffectKind::kProgram);
  SoundEffects::StopPrograms(state, host,
                             {.first = SoundEffects::kProgramEffects});
  CHECK(state.scheduler.active_voices == Voice(0));
  CHECK(state.scheduler.pending_key_off == (Voice(8) | Voice(10)));
  CHECK(state.channels[10].effect == EffectSelector::kNone);
  CHECK(state.channels[10].kind == EffectKind::kShared);
  CHECK(host.refusals.empty());
}

TEST_CASE(
    "A blob refuses a zero count after its first entry and unreadable "
    "bytes",
    "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  host.blob = TwoEntryBlob(0);
  host.blob_ref = Data(kBlobAddress);
  EffectCommand command = BlobCommand(0);
  SoundEffects::PlayBlobPrograms(state, host, command);
  CHECK(host.resets.size() == 1);
  CHECK(host.refusals == std::vector{EffectRefusal::kBlobCountInvalid});
  host.blob.resize(2 * sizeof(std::uint32_t));
  SoundEffects::PlayBlobPrograms(state, host, command);
  CHECK(host.refusals.back() == EffectRefusal::kBlobUnreadable);
  CHECK(host.slot_selectors.size() == 1);
  EffectCommand unknown{.first = Selector(0x1234)};
  SoundEffects::PlayBlobPrograms(state, host, unknown);
  CHECK(host.refusals.size() == 3);
  CHECK(host.resets.size() == 1);
}

TEST_CASE(
    "A blob entry past the blob stops the play after the earlier "
    "entries",
    "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  host.blob = TwoEntryBlob(3);
  host.blob_ref = Data(kBlobAddress);
  PutWord(host.blob, 6, 0x100);
  EffectCommand command = BlobCommand(0);
  SoundEffects::PlayBlobPrograms(state, host, command);
  CHECK(host.resets.size() == 2);
  CHECK(host.refusals == std::vector{EffectRefusal::kBlobUnreadable});
  PutWord(host.blob, 6, 0x10);
  PutHalf(host.blob, 0x30, 0x40);
  SoundEffects::PlayBlobPrograms(state, host, command);
  CHECK(host.resets.size() == 4);
  CHECK(host.refusals.size() == 2);
}

TEST_CASE("Stopping programs releases by effect, by tag or by channel pair",
          "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  state.scheduler.active_voices = Voice(0) | Voice(1);
  state.channels[0] = {.effect = Selector(5), .tags = 0x20};
  state.channels[1] = {.effect = Selector(6), .tags = 0x10};
  SoundEffects::StopPrograms(state, host, {.first = Selector(5)});
  CHECK(state.scheduler.active_voices == Voice(1));
  SoundEffects::StopPrograms(state, host, {.second = 0x10});
  CHECK(state.scheduler.active_voices == 0U);
  CHECK(state.scheduler.pending_key_off == (Voice(0) | Voice(1)));
  SoundEffects::StopPrograms(state, host,
                             {.first = Selector(40), .second = 0x80000000});
  CHECK(host.refusals == std::vector{EffectRefusal::kChannelOutOfRange});
  CHECK(state.driver_update_flags == SoundEffects::kVoiceUpdate);
}

TEST_CASE(
    "Program volume and pan apply to the active channels the command "
    "selects",
    "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  state.scheduler.active_voices = Voice(0) | Voice(1);
  state.scheduler.paused_effect_voices = Voice(2);
  state.channels[0] = {
      .effect = Selector(5), .tags = 0x10, .effect_volume_length = 9};
  state.channels[1] = {.effect = Selector(5), .tags = 0x20};
  state.channels[2] = {.effect = Selector(5)};
  SoundEffects::SetProgramVolume(
      state, {.first = Selector(5), .second = 0x10, .third = 0xFF});
  SoundEffects::SetProgramPan(state, {.first = Selector(5), .third = 0x1C0});
  CHECK(state.channels[0].effect_volume == 0x7F00);
  CHECK(state.channels[0].effect_volume_length == 0);
  CHECK(state.channels[1].effect_volume == 0);
  CHECK(state.channels[0].effect_pan == 0xC000);
  CHECK(state.channels[1].effect_pan == 0xC000);
  CHECK(state.channels[1].dirty_flags == Seq::kVolumeDirty);
  CHECK(state.channels[2].effect_pan == 0);
  CHECK(state.channels[2].dirty_flags == 0U);
}

TEST_CASE("Program fades step toward their target over the 16-bit length",
          "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  state.scheduler.active_voices = Voice(0);
  state.channels[0] = {
      .effect = Selector(5), .effect_pan = 0x8000, .effect_volume = 0x7F00};
  SoundEffects::FadeProgramVolume(
      state, host, {.first = Selector(5), .third = 16, .fourth = 0});
  CHECK(state.channels[0].effect_volume_step == -0x7F0);
  CHECK(state.channels[0].effect_volume_length == 16);
  SoundEffects::FadeProgramPan(
      state, host, {.first = Selector(5), .third = 0, .fourth = 0xC0});
  CHECK(state.channels[0].effect_pan_step == 0x4000);
  CHECK(state.channels[0].effect_pan_length == 1);
  CHECK(host.refusals.empty());
  SoundEffects::FadeProgramVolume(
      state, host, {.first = Selector(5), .third = 0x10000, .fourth = 0x40});
  CHECK(host.refusals == std::vector{EffectRefusal::kZeroLengthFade});
  CHECK(state.channels[0].effect_volume_step == -0x3F00);
  CHECK(state.channels[0].effect_volume_length == 1);
}

TEST_CASE("Scene commands skip interface effects and read the low first word",
          "[audio-control-sfx][audio]") {
  EffectState state = FreshState();
  FakeEffects host;
  state.scheduler.active_voices = Voice(0) | Voice(1) | Voice(2);
  state.channels[1].tags =
      static_cast<std::int32_t>(SoundEffects::kInterfaceEffectTag);
  state.channels[2].tags = 0x10;
  SoundEffects::SetSceneVolume(state, {.first = Selector(0x1FF)});
  SoundEffects::SetScenePan(state, {.first = Selector(0x100000040)});
  SoundEffects::FadeSceneVolume(state, host, {.first = Selector(0x100000004)});
  SoundEffects::FadeScenePan(state, host,
                             {.first = Selector(8), .second = 0x80});
  CHECK(state.channels[0].effect_volume == 0x7F00);
  CHECK(state.channels[1].effect_volume == 0);
  CHECK(state.channels[2].effect_pan == 0x4000);
  CHECK(state.channels[0].effect_volume_step == -0x1FC0);
  CHECK(state.channels[0].effect_volume_length == 4);
  CHECK(state.channels[2].effect_pan_step == 0x800);
  CHECK(state.channels[2].effect_pan_length == 8);
  CHECK(state.channels[1].effect_pan_length == 0);
}

// ---------------------------------------------------------------------------
// Sound-effect front end

namespace {
using Audio::BankSfx;
using Audio::SfxFade;
using Audio::SfxMix;
using Audio::SfxRefusal;
using Audio::SfxState;

constexpr std::uintptr_t kBankAddress = 0x10000;
constexpr std::uintptr_t kRoomBankAddress = 0x20000;
constexpr std::uintptr_t kModelBankAddress = 0x30000;

struct SfxPlay {
  std::int32_t effect_id = 0;
  DataRef program = DataRef::kNone;
  std::int32_t tag = 0;
  SfxMix mix;

  friend bool operator==(const SfxPlay&, const SfxPlay&) = default;
};

struct SfxStop {
  EffectSelector selector = EffectSelector::kNone;
  std::int32_t mask = 0;

  friend bool operator==(const SfxStop&, const SfxStop&) = default;
};

struct SfxFadeCall {
  SfxFade fade = SfxFade::kVolume;
  EffectSelector selector = EffectSelector::kNone;
  std::int32_t mask = 0;
  std::int32_t target = 0;
  std::int32_t ticks = 0;

  friend bool operator==(const SfxFadeCall&, const SfxFadeCall&) = default;
};

struct SfxRefused {
  SfxRefusal refusal = SfxRefusal::kNoSelectedBank;
  std::int32_t group = 0;
  std::int32_t effect_id = 0;
  std::uint32_t bank = 0;
  std::int32_t offset = 0;

  friend bool operator==(const SfxRefused&, const SfxRefused&) = default;
};

class FakeSfx final : public Audio::SfxHost {
 public:
  std::vector<SfxPlay> played;
  std::map<std::int32_t, EffectSelector> tag_effects;
  bool shared_tags = true;
  bool program_tags = true;
  std::vector<SfxStop> stopped;
  std::size_t scene_stops = 0;
  std::vector<SfxFadeCall> faded;
  std::map<std::uintptr_t, std::vector<std::byte>> banks;
  std::size_t bank_reads = 0;
  Audio::SfxProjection projection;
  std::vector<std::int16_t> projected_x;
  std::vector<std::uint32_t> diagnostics;
  std::vector<SfxRefused> refusals;

  void PlaySoundEffect(std::int32_t effect_id, std::int32_t effect_tag,
                       const SfxMix& mix) override {
    played.push_back({.effect_id = effect_id, .tag = effect_tag, .mix = mix});
  }
  void PlaySoundEffectProgram(DataRef program, std::int32_t effect_tag,
                              const SfxMix& mix) override {
    played.push_back({.program = program, .tag = effect_tag, .mix = mix});
  }
  EffectSelector SoundEffectIdForTag(std::int32_t effect_tag) override {
    const auto found = tag_effects.find(effect_tag);
    return found == tag_effects.end() ? EffectSelector::kNone : found->second;
  }
  bool SharedEffectUsesTags(std::int32_t /*effect_id*/) override {
    return shared_tags;
  }
  bool ProgramUsesTags(DataRef /*program*/) override { return program_tags; }
  void StopSoundEffects(EffectSelector selector,
                        std::int32_t effect_tag_mask) override {
    stopped.push_back({.selector = selector, .mask = effect_tag_mask});
  }
  void StopSceneSoundEffects() override { ++scene_stops; }
  void FadeSoundEffect(SfxFade fade, EffectSelector selector,
                       std::int32_t effect_tag_mask,
                       const Audio::SfxFadeSpec& spec) override {
    faded.push_back({.fade = fade,
                     .selector = selector,
                     .mask = effect_tag_mask,
                     .target = spec.target,
                     .ticks = spec.ticks});
  }
  std::span<const std::byte> BankBytes(DataRef bank) override {
    ++bank_reads;
    const auto found = banks.find(static_cast<std::uintptr_t>(bank));
    if (found == banks.end()) return {};
    return found->second;
  }
  Audio::SfxProjection Project(const Audio::SfxPoint& point) override {
    projected_x.push_back(point.x);
    return projection;
  }
  void Diagnose(std::uint32_t code) override { diagnostics.push_back(code); }
  void Refuse(SfxRefusal refusal, const Audio::SfxRequest& request) override {
    refusals.push_back({.refusal = refusal,
                        .group = request.group,
                        .effect_id = request.effect_id,
                        .bank = request.bank,
                        .offset = request.offset});
  }

  [[nodiscard]] std::vector<SfxRefusal> RefusalKinds() const {
    std::vector<SfxRefusal> kinds;
    kinds.reserve(refusals.size());
    for (const SfxRefused& refused : refusals) kinds.push_back(refused.refusal);
    return kinds;
  }
};

SfxPlay Effect(std::int32_t effect_id, std::int32_t tag, SfxMix mix = {}) {
  return {.effect_id = effect_id, .tag = tag, .mix = mix};
}

SfxPlay Program(std::uintptr_t address, std::int32_t tag, SfxMix mix = {}) {
  return {.program = Data(address), .tag = tag, .mix = mix};
}
}  // namespace

TEST_CASE("The primary channel plays built-in effects on its only tag",
          "[audio-control-sfx][audio]") {
  SfxState state;
  FakeSfx host;
  Audio::PlaySfxOnPrimaryChannel(state, host, 0x12345);
  Audio::PlaySfxOnPrimaryChannel(state, host, 7);
  CHECK(host.played == std::vector{Effect(0x2345, 1), Effect(7, 1)});
  CHECK(state.cursors.group_1 == 1);
  CHECK(host.refusals.empty());
}

TEST_CASE(
    "Shared group plays advance the shared cursor and wrap past its "
    "last tag",
    "[audio-control-sfx][audio]") {
  SfxState state;
  FakeSfx host;
  state.cursors.group_7e = 0x40;
  const SfxMix mix{.pan = 0x40, .volume = 0x20};
  Audio::PlaySfx(state, host, Audio::kSharedEffectTags, 0x10005, mix);
  Audio::PlaySfx(state, host, Audio::kSharedEffectGroupAlias, 6, mix);
  host.shared_tags = false;
  Audio::PlaySfxCentered(state, host, Audio::kSharedEffectTags, 7);
  CHECK(host.played ==
        std::vector{Effect(5, 0x40, mix), Effect(6, 2, mix), Effect(7, 0)});
  CHECK(state.cursors.group_7e == 4);
}

TEST_CASE(
    "Groups 0x100 and 0x200 play on the 0x200 cursor without advancing "
    "it",
    "[audio-control-sfx][audio]") {
  SfxState state;
  FakeSfx host;
  Audio::PlaySfx(state, host, Audio::kEffectTagGroup100, 3, {});
  host.tag_effects[Audio::kEffectTagGroup200] = Selector(3);
  Audio::PlaySfxCentered(state, host, Audio::kEffectTagGroup200, 3);
  Audio::PlaySfxCentered(state, host, Audio::kEffectTagGroup200, 4);
  host.shared_tags = false;
  Audio::PlaySfxCentered(state, host, Audio::kEffectTagGroup200, 3);
  CHECK(host.played ==
        std::vector{Effect(3, 0x200), Effect(4, 0x200), Effect(3, 0)});
  CHECK(state.cursors.group_200 == 0x200);
}

TEST_CASE("Channel plays skip an effect already playing on the 0x200 tag",
          "[audio-control-sfx][audio]") {
  FakeSfx host;
  host.tag_effects[Audio::kEffectTagGroup200] = Selector(0x8000);
  Audio::PlaySfxProgramChannel(host, Data(0x8000), 0x200, {});
  Audio::PlaySfxProgramChannel(host, Data(0x8004), 0x200, {});
  Audio::PlaySfxProgramChannel(host, Data(0x8000), 0x180, {});
  host.tag_effects[Audio::kEffectTagGroup200] = Selector(9);
  Audio::PlayBuiltInSfxChannel(host, 9, 0x200, {});
  Audio::PlayBuiltInSfxChannel(host, 9, 0x100, {});
  CHECK(host.played == std::vector{Program(0x8004, 0x200),
                                   Program(0x8000, 0x180), Effect(9, 0x100)});
}

TEST_CASE(
    "A private bank effect plays the selected bank's program on the "
    "bank group's tags",
    "[audio-control-sfx][audio]") {
  SfxState state;
  FakeSfx host;
  state.selected_bank = 2;
  state.banks[1] = Data(kBankAddress);
  // Count 1: effect 1's offset word is read without the count check.
  host.banks[kBankAddress] = Words({1, 0x10, 0x20});
  state.cursors.group_ff000 = 0x80000;
  Audio::PlaySfxCentered(state, host, Audio::kSfxBankEffectTags, 0);
  Audio::PlaySfxCentered(state, host, Audio::kSfxBankEffectTags, 1);
  host.program_tags = false;
  Audio::PlaySfxCentered(state, host, Audio::kSfxBankEffectTags, 1);
  CHECK(host.played == std::vector{Program(kBankAddress + 0x10, 0x80000),
                                   Program(kBankAddress + 0x20, 0x1000),
                                   Program(kBankAddress + 0x20, 0)});
  CHECK(state.cursors.group_ff000 == 0x2000);
  CHECK(host.refusals.empty());
}

TEST_CASE(
    "Private bank effects refuse a missing selection, bank or offset "
    "word",
    "[audio-control-sfx][audio]") {
  SfxState state;
  FakeSfx host;
  host.banks[kBankAddress] = Words({1, 0x10});
  Audio::PlaySfxCentered(state, host, Audio::kSfxBankEffectTags, 0);
  state.selected_bank = 4;
  Audio::PlaySfxCentered(state, host, Audio::kSfxBankEffectTags, 0);
  state.selected_bank = 1;
  Audio::PlaySfxCentered(state, host, Audio::kSfxBankEffectTags, 0);
  state.banks[0] = Data(kBankAddress);
  Audio::PlaySfxCentered(state, host, Audio::kSfxBankEffectTags, -1);
  Audio::PlaySfxCentered(state, host, Audio::kSfxBankEffectTags, 1);
  CHECK(host.played.empty());
  CHECK(host.RefusalKinds() == std::vector{SfxRefusal::kNoSelectedBank,
                                           SfxRefusal::kSelectedBankOutOfRange,
                                           SfxRefusal::kBankUnavailable,
                                           SfxRefusal::kBankUnavailable,
                                           SfxRefusal::kBankUnreadable});
  CHECK(host.refusals.at(1).bank == 4U);
  CHECK(host.refusals.at(3) ==
        SfxRefused{SfxRefusal::kBankUnavailable, 0xFF000, -1, 1, 0});
  CHECK(host.refusals.at(4) ==
        SfxRefused{SfxRefusal::kBankUnreadable, 0xFF000, 1, 1, 0});
  CHECK(state.cursors.group_ff000 == 0x1000);
}

TEST_CASE(
    "Room bank effects play from the room slot on the room group's "
    "tags",
    "[audio-control-sfx][audio]") {
  SfxState state;
  FakeSfx host;
  Audio::PlaySfxCentered(state, host, Audio::kRoomSfxBankEffectTags, 1);
  state.banks[2] = Data(kRoomBankAddress);
  host.banks[kRoomBankAddress] = Words({2, 0xC, 0x10});
  Audio::PlaySfxCentered(state, host, Audio::kRoomSfxBankEffectTags, -1);
  Audio::PlaySfxCentered(state, host, Audio::kRoomSfxBankEffectTags, 1);
  CHECK(host.played == std::vector{Program(kRoomBankAddress + 0x10, 0x100000)});
  CHECK(state.cursors.group_f00000 == 0x200000);
  CHECK(host.RefusalKinds() ==
        std::vector{SfxRefusal::kNoRoomBank, SfxRefusal::kRoomEffectNegative});
  CHECK(host.refusals.at(0) ==
        SfxRefused{SfxRefusal::kNoRoomBank, 0xF00000, 1, 3, 0});
}

TEST_CASE("An unknown channel group reports its diagnostic and plays nothing",
          "[audio-control-sfx][audio]") {
  SfxState state;
  FakeSfx host;
  Audio::PlaySfxCentered(state, host, 3, 1);
  CHECK(host.played.empty());
  CHECK(Audio::ResolveSfxProgram(state, host, 0x400, 1) == DataRef::kNone);
  CHECK(host.diagnostics == std::vector{Audio::kUnknownSfxGroupDiagnostic,
                                        Audio::kUnknownSfxGroupDiagnostic});
  CHECK(host.refusals.empty());
}

TEST_CASE("Sound effect pan follows the screen x and volume the depth",
          "[audio-control-sfx][audio]") {
  using Audio::SfxPanAndVolume;
  CHECK(SfxPanAndVolume({.screen_xy = 160, .depth = 0x300}) ==
        SfxMix{.pan = 0x80, .volume = 0x7F});
  CHECK(SfxPanAndVolume({.screen_xy = 0, .depth = 0}) ==
        SfxMix{.pan = 0x50, .volume = 0x7F});
  CHECK(SfxPanAndVolume({.screen_xy = 480, .depth = 0x320}) ==
        SfxMix{.pan = 0xE0, .volume = 0x7E});
  // The screen y in the high halfword does not move the pan.
  CHECK(SfxPanAndVolume({.screen_xy = 0x7FFF00A0, .depth = 0x300}) ==
        SfxMix{.pan = 0x80, .volume = 0x7F});
  CHECK(SfxPanAndVolume({.screen_xy = 160, .depth = 0x300 + 0xFFF}) ==
        SfxMix{.pan = 0x80, .volume = 0});
  // The original's 32-bit subtraction wraps the lowest depth to silence.
  CHECK(SfxPanAndVolume({.screen_xy = 160,
                         .depth = std::numeric_limits<std::int32_t>::min()}) ==
        SfxMix{.pan = 0x80, .volume = 0});
}

TEST_CASE("A projected effect plays only while it is audible",
          "[audio-control-sfx][audio]") {
  SfxState state;
  FakeSfx host;
  host.projection = {.screen_xy = 160, .depth = 0x300 + 0xFFF};
  Audio::PlayWorldSfx(state, host, Audio::kSharedEffectTags, 1,
                      {.x = 5, .y = 6, .z = 7});
  CHECK(host.played.empty());
  host.projection = {.screen_xy = 480, .depth = 0x320};
  Audio::PlayWorldSfx(state, host, Audio::kSharedEffectTags, 1, {.x = 8});
  CHECK(host.played ==
        std::vector{Effect(1, 2, {.pan = 0xE0, .volume = 0x7E})});
  CHECK(host.projected_x == std::vector<std::int16_t>{5, 8});
  Audio::PlayProjectedSfx(state, host, Audio::kSharedEffectTags, 2,
                          {.screen_xy = 160, .depth = 0});
  REQUIRE(host.played.size() == 2);
  CHECK(host.played.back() == Effect(2, 4));
}

TEST_CASE("Resolving an effect checks the bank selection and the count",
          "[audio-control-sfx][audio]") {
  SfxState state;
  FakeSfx host;
  const auto resolve = [&](std::int32_t group, std::int32_t effect_id) {
    return Audio::ResolveSfxProgram(state, host, group, effect_id);
  };
  CHECK(resolve(Audio::kSharedEffectTags, 9) == DataRef::kNone);
  CHECK_FALSE(resolve(Audio::kSfxBankEffectTags, 0).has_value());
  state.selected_bank = 4;
  CHECK_FALSE(resolve(Audio::kRoomSfxBankEffectTags, 0).has_value());
  state.selected_bank = 1;
  CHECK_FALSE(resolve(Audio::kSfxBankEffectTags, 0).has_value());
  state.banks[0] = Data(kBankAddress);
  host.banks[kBankAddress] = Words({2, 0xC, 0x8});
  // Both banked groups resolve within the selected bank.
  CHECK(Required(resolve(Audio::kRoomSfxBankEffectTags, 0)) ==
        Data(kBankAddress + 0xC));
  CHECK_FALSE(resolve(Audio::kSfxBankEffectTags, 2).has_value());
  CHECK_FALSE(resolve(Audio::kSfxBankEffectTags, -1).has_value());
  CHECK(host.RefusalKinds() == std::vector{SfxRefusal::kUnresolvableBank,
                                           SfxRefusal::kUnresolvableBank,
                                           SfxRefusal::kProgramUnavailable,
                                           SfxRefusal::kProgramUnavailable,
                                           SfxRefusal::kProgramUnavailable});
}

TEST_CASE("Resolving an effect refuses offsets into the offset words",
          "[audio-control-sfx][audio]") {
  SfxState state;
  FakeSfx host;
  const auto resolve = [&](std::int32_t effect_id) {
    return Audio::ResolveSfxProgram(state, host, Audio::kSfxBankEffectTags,
                                    effect_id);
  };
  state.selected_bank = 1;
  state.banks[0] = Data(kBankAddress);
  host.banks[kBankAddress] = Words({3, 0x10, 0x8, 0xFFFFFFF0});
  CHECK(Required(resolve(0)) == Data(kBankAddress + 0x10));
  CHECK_FALSE(resolve(1).has_value());
  CHECK_FALSE(resolve(2).has_value());
  REQUIRE(host.refusals.size() == 2);
  CHECK(host.refusals[0] ==
        SfxRefused{SfxRefusal::kProgramOffsetInvalid, 0xFF000, 1, 1, 8});
  CHECK(host.refusals[1].offset == -16);
  host.banks[kBankAddress].resize(2);
  CHECK_FALSE(resolve(0).has_value());
  CHECK(host.refusals.back().refusal == SfxRefusal::kBankUnreadable);
}

TEST_CASE("Sound effect stops name a group, a built-in effect or a program",
          "[audio-control-sfx][audio]") {
  SfxState state;
  FakeSfx host;
  state.selected_bank = 1;
  state.banks[0] = Data(kBankAddress);
  host.banks[kBankAddress] = Words({2, 0xC, 0x10});
  Audio::StopSfx(state, host, {.group = Audio::kSharedEffectTags});
  Audio::StopSfx(state, host,
                 {.group = Audio::kSharedEffectTags, .effect_id = 5});
  Audio::StopSfx(state, host, {.group = Audio::kRoomSfxBankEffectTags});
  Audio::StopSfx(state, host,
                 {.group = Audio::kSfxBankEffectTags, .effect_id = 1});
  Audio::StopSfx(state, host,
                 {.group = Audio::kSfxBankEffectTags, .effect_id = 2});
  Audio::StopAllSfxPlayback(host);
  CHECK(host.stopped ==
        std::vector{SfxStop{EffectSelector::kNone, 0x7E},
                    SfxStop{Selector(5), 0},
                    SfxStop{EffectSelector::kNone, 0xF00000},
                    SfxStop{SelectorAt(kBankAddress + 0x10), 0}});
  CHECK(host.RefusalKinds() == std::vector{SfxRefusal::kProgramUnavailable});
  CHECK(host.scene_stops == 1U);
}

TEST_CASE("Sound effect fades keep the low byte of their ticks",
          "[audio-control-sfx][audio]") {
  SfxState state;
  FakeSfx host;
  Audio::FadeSfxVolume(state, host,
                       {.group = Audio::kSharedEffectTags, .effect_id = 5},
                       {.target = 0x40, .ticks = 0x1FF});
  Audio::FadeSfxPitch(state, host, {.group = Audio::kEffectTagGroup200},
                      {.target = 1, .ticks = -1});
  Audio::FadeSfxPan(state, host,
                    {.group = Audio::kSfxBankEffectTags, .effect_id = 1},
                    {.target = 2, .ticks = 3});
  CHECK(host.faded ==
        std::vector{SfxFadeCall{SfxFade::kVolume, Selector(5), 0, 0x40, 0xFF},
                    SfxFadeCall{SfxFade::kPitch, EffectSelector::kNone, 0x200,
                                1, 0xFF}});
  CHECK(host.RefusalKinds() == std::vector{SfxRefusal::kUnresolvableBank});
}

TEST_CASE("A bank effect plays on the next tag of its channel group",
          "[audio-control-sfx][audio]") {
  SfxState state;
  FakeSfx host;
  host.banks[kModelBankAddress] = Words({2, 0xC, 0x10, 0x14});
  const BankSfx effect{.bank = Data(kModelBankAddress), .index = 1};
  Audio::PlayBankSfxCentered(state, host, Audio::kEffectTagGroup180, effect);
  Audio::PlayBankSfxCentered(state, host, Audio::kEffectTagGroup180, effect);
  Audio::PlayBankSfxCentered(state, host, Audio::kEffectTagGroup200, effect);
  Audio::PlayBankSfxCentered(state, host, Audio::kRoomSfxBankEffectTags,
                             effect);
  Audio::PlayBankSfx(state, host, Audio::kSharedEffectTags, effect,
                     {.pan = 1, .volume = 2});
  Audio::PlayBankSfxCentered(state, host, 7, effect);
  constexpr std::uintptr_t kProgram = kModelBankAddress + 0x10;
  CHECK(host.played ==
        std::vector{Program(kProgram, 0x80), Program(kProgram, 0x100),
                    Program(kProgram, 0x200), Program(kProgram, 0x100000),
                    Program(kProgram, 0, {.pan = 1, .volume = 2}),
                    Program(kProgram, 0)});
  CHECK(state.cursors.group_180 == 0x80);
  CHECK(state.cursors.group_200 == 0x200);
  CHECK(state.cursors.group_f00000 == 0x200000);
  CHECK(host.diagnostics == std::vector{Audio::kUnknownSfxGroupDiagnostic});
}

TEST_CASE("A bank effect index compares unsigned with the bank's count",
          "[audio-control-sfx][audio]") {
  SfxState state;
  FakeSfx host;
  host.banks[kModelBankAddress] = Words({2, 0xC, 0x10, 0x14});
  const DataRef bank = Data(kModelBankAddress);
  Audio::PlayBankSfxCentered(state, host, 0x180, {.bank = bank, .index = 2});
  Audio::PlayBankSfxCentered(state, host, 0x180, {.bank = bank, .index = -1});
  CHECK(host.refusals.empty());
  Audio::PlayBankSfxCentered(state, host, 0x180, {.bank = Data(0x1234)});
  host.banks[kModelBankAddress] = Words({5, 0xC});
  Audio::PlayBankSfxCentered(state, host, 0x180, {.bank = bank, .index = 3});
  CHECK(host.played.empty());
  CHECK(host.refusals ==
        std::vector{SfxRefused{SfxRefusal::kBankUnreadable, 0x180, 0},
                    SfxRefused{SfxRefusal::kBankUnreadable, 0x180, 3}});
  CHECK(state.cursors.group_180 == 0x80);
}

TEST_CASE("A projected bank effect pans from the screen x remainder",
          "[audio-control-sfx][audio]") {
  SfxState state;
  FakeSfx host;
  host.banks[kModelBankAddress] = Words({2, 0xC, 0x10});
  const BankSfx effect{.bank = Data(kModelBankAddress)};
  const auto play = [&](std::int32_t screen_xy, std::int32_t depth) {
    Audio::PlayProjectedBankSfx(state, host, Audio::kSharedEffectTags, effect,
                                {.screen_xy = screen_xy, .depth = depth});
  };
  play(0x140, 0);
  play(0x7FFF, 8);
  // A point above the screen's top edge leaves a negative remainder, which
  // takes the left edge's pan.
  play(-0x10000 + 0x50, 0x3F0);
  play(0x50, 0x3F8);
  host.projection = {.screen_xy = 0x20, .depth = -5};
  Audio::PlayWorldBankSfx(state, host, Audio::kSharedEffectTags, effect,
                          {.x = 3});
  constexpr std::uintptr_t kProgram = kModelBankAddress + 0xC;
  CHECK(host.played ==
        std::vector{Program(kProgram, 0, {.pan = 0xD0, .volume = 0x7F}),
                    Program(kProgram, 0, {.pan = 0xD0, .volume = 0x7E}),
                    Program(kProgram, 0, {.pan = 0x30, .volume = 1}),
                    Program(kProgram, 0, {.pan = 0x40, .volume = 0x7F})});
  CHECK(host.projected_x == std::vector<std::int16_t>{3});
}

TEST_CASE("Bank effect stops name the group at index 0 and read no count",
          "[audio-control-sfx][audio]") {
  FakeSfx host;
  host.banks[kModelBankAddress] = Words({2, 0xC, 0x10, 0x14});
  const DataRef bank = Data(kModelBankAddress);
  Audio::StopBankSfx(host, Audio::kEffectTagGroup180, {.bank = bank});
  CHECK(host.bank_reads == 0U);
  Audio::StopBankSfx(host, Audio::kEffectTagGroup180,
                     {.bank = bank, .index = 1});
  // Past the count: the offset word is read without the count check.
  Audio::StopBankSfx(host, Audio::kEffectTagGroup180,
                     {.bank = bank, .index = 2});
  Audio::StopBankSfx(host, Audio::kEffectTagGroup180,
                     {.bank = bank, .index = 3});
  Audio::StopBankSfx(host, Audio::kEffectTagGroup180,
                     {.bank = bank, .index = -1});
  CHECK(host.stopped ==
        std::vector{SfxStop{EffectSelector::kNone, 0x180},
                    SfxStop{SelectorAt(kModelBankAddress + 0x10), 0},
                    SfxStop{SelectorAt(kModelBankAddress + 0x14), 0}});
  CHECK(host.refusals ==
        std::vector{SfxRefused{SfxRefusal::kBankUnreadable, 0x180, 3},
                    SfxRefused{SfxRefusal::kBankUnreadable, 0x180, -1}});
  CHECK(host.bank_reads == 3U);
}

TEST_CASE("Bank effect volume fades keep the low byte of their ticks",
          "[audio-control-sfx][audio]") {
  FakeSfx host;
  host.banks[kModelBankAddress] = Words({2, 0xC, 0x10});
  const DataRef bank = Data(kModelBankAddress);
  Audio::FadeBankSfxVolume(host, Audio::kEffectTagGroup200, {.bank = bank},
                           {.target = 0x10, .ticks = 0x123});
  Audio::FadeBankSfxVolume(host, Audio::kEffectTagGroup200,
                           {.bank = bank, .index = 1},
                           {.target = 0x7F, .ticks = -1});
  Audio::FadeBankSfxVolume(host, Audio::kEffectTagGroup200,
                           {.bank = bank, .index = -2}, {});
  CHECK(host.faded ==
        std::vector{
            SfxFadeCall{SfxFade::kVolume, EffectSelector::kNone, 0x200, 0x10,
                        0x23},
            SfxFadeCall{SfxFade::kVolume, SelectorAt(kModelBankAddress + 0x10),
                        0, 0x7F, 0xFF}});
  CHECK(host.RefusalKinds() == std::vector{SfxRefusal::kBankUnreadable});
}

// ---------------------------------------------------------------------------
// Live entries and module bindings

namespace {
struct SoundStateScope {
  Audio_SoundState saved = Audio_soundState;

  SoundStateScope() { Audio_soundState = {}; }
  ~SoundStateScope() { Audio_soundState = saved; }
  SoundStateScope(const SoundStateScope&) = delete;
  SoundStateScope& operator=(const SoundStateScope&) = delete;
  SoundStateScope(SoundStateScope&&) = delete;
  SoundStateScope& operator=(SoundStateScope&&) = delete;
};
}  // namespace

TEST_CASE("Live sound effect entries refuse missing parameters",
          "[audio-control-sfx][audio]") {
  const std::uint64_t refused = Seq::LiveRefusalCount();
  CHECK(Control::LiveSoundEffectChannelIds(nullptr) == 0U);
  SoundEffects::LiveStopPrograms(nullptr);
  SoundEffects::LivePlayBlobPrograms(nullptr);
  SoundEffects::LiveFadeScenePan(nullptr);
  SoundEffects::LiveInvalidateMusicChannelVolumes(nullptr, nullptr);
  CHECK(Seq::LiveRefusalCount() == refused + 5);
  // A world effect without a position is logged, not counted.
  Audio::LivePlayWorldSfx(Audio::kSharedEffectTags, 1, nullptr);
  CHECK(Seq::LiveRefusalCount() == refused + 5);
}

TEST_CASE("The live pan and volume word packs the pan above the volume",
          "[audio-control-sfx][audio]") {
  CHECK(Audio::LiveSfxPanAndVolume(160, 0x300) == 0x80007F);
  CHECK(Audio::LiveSfxPanAndVolume(0, 0) == 0x50007F);
  CHECK(Audio::LiveSfxPanAndVolume(480, 0x320) == 0xE0007E);
  CHECK(Audio::LiveSfxPanAndVolume(
            160, std::numeric_limits<std::int32_t>::min()) == 0x800000);
}

TEST_CASE("The live resolve returns -1 for a refused bank and 0 for built-ins",
          "[audio-control-sfx][audio]") {
  const SoundStateScope scope;
  std::array<std::uint32_t, 2> bank{1, 8};
  CHECK(Audio::LiveResolveSfxProgram(Audio::kSharedEffectTags, 1) == 0);
  CHECK(Audio::LiveResolveSfxProgram(Audio::kSfxBankEffectTags, 0) == -1);
  Audio_soundState.currentSfxBank = 1;
  CHECK(Audio::LiveResolveSfxProgram(Audio::kSfxBankEffectTags, 0) == -1);
  // Test memory lies outside the program regions the live bank reads accept.
  Audio_soundState.sfxBankData[0] = bank.data();
  CHECK(Audio::LiveResolveSfxProgram(Audio::kSfxBankEffectTags, 0) == -1);
  CHECK(Audio_soundState.sfxBankData[0] == bank.data());
}

TEST_CASE("Promoted sound control and effect functions keep no module binding",
          "[audio-control-sfx][audio][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const std::array<std::uint32_t, 67> retired{
      0x80012440U, 0x80012468U, 0x80012490U, 0x800124FCU, 0x80012568U,
      0x800125A8U, 0x800125D4U, 0x80012608U, 0x8001264CU, 0x80012694U,
      0x800126C0U, 0x800126F4U, 0x80012738U, 0x80012780U, 0x800127ACU,
      0x800127E0U, 0x80012824U, 0x8001286CU, 0x800128A0U, 0x800128D8U,
      0x80012918U, 0x80012940U, 0x80012970U, 0x800129A4U, 0x800129D0U,
      0x80012A04U, 0x80012A40U, 0x80012A6CU, 0x80012AA0U, 0x80012ADCU,
      0x80012B08U, 0x80012B3CU, 0x80012B78U, 0x80012B98U, 0x800132C4U,
      0x80013328U, 0x80013348U, 0x80013378U, 0x800133B0U, 0x800133E0U,
      0x800161C4U, 0x8001653CU, 0x800166E8U, 0x80016744U, 0x800168F8U,
      0x80016964U, 0x800169ACU, 0x80045630U, 0x80045684U, 0x800456ECU,
      0x80045754U, 0x80045988U, 0x800459ACU, 0x80045A58U, 0x80045AA8U,
      0x80045B28U, 0x80045BFCU, 0x80045C74U, 0x80045CECU, 0x80045D64U,
      0x80045DC0U, 0x800461CCU, 0x800462E8U, 0x80046310U, 0x800463BCU,
      0x8004644CU, 0x80046494U,
  };
  for (const std::uint32_t address : retired) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40", address) ==
          nullptr);
  }
}

TEST_CASE(
    "Sound effect command handlers keep module bindings only in the "
    "command table",
    "[audio-control-sfx][audio][redirect-binding]") {
  using Handler = void (*)(Audio_CommandParams*);
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const std::array<std::pair<std::uint32_t, Handler>, 13> kept{{
      {0x80017208U, &SoundEffects::LivePlayInterfaceProgramPair},
      {0x80017254U, &SoundEffects::LivePlayInterfaceProgram},
      {0x800172D4U, &SoundEffects::LivePlaySharedProgram},
      {0x8001733CU, &SoundEffects::LivePlayBlobPrograms},
      {0x8001746CU, &SoundEffects::LiveStopPrograms},
      {0x80017870U, &SoundEffects::LiveSetProgramVolume},
      {0x80017958U, &SoundEffects::LiveFadeProgramVolume},
      {0x80017AD8U, &SoundEffects::LiveSetSceneVolume},
      {0x80017B54U, &SoundEffects::LiveFadeSceneVolume},
      {0x80017C1CU, &SoundEffects::LiveSetProgramPan},
      {0x80017CFCU, &SoundEffects::LiveFadeProgramPan},
      {0x80017E74U, &SoundEffects::LiveSetScenePan},
      {0x80017EECU, &SoundEffects::LiveFadeScenePan},
  }};
  for (const auto& [address, handler] : kept) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40", address) ==
          reinterpret_cast<void*>(handler));
  }
}
