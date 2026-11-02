#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/StreamControl.hpp"
#include "Audio/StreamControlLive.hpp"
#include "Content/AudioStream.hpp"
extern "C" {
#include "Audio/Cutscene/Stream.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/SoundBanks.h"
}  // extern "C"

namespace {

using VS::Audio::RoomStreamStatus;
using VS::Audio::Cutscene::StreamFade;
using VS::Audio::Cutscene::StreamFormat;
using VS::Audio::Cutscene::StreamGains;
using VS::Audio::Cutscene::StreamHost;
using VS::Audio::Cutscene::StreamMix;
using VS::Audio::Cutscene::StreamStart;
using VS::Audio::Cutscene::StreamStatus;
using VS::Audio::Cutscene::StreamVoices;

namespace Cutscene = VS::Audio::Cutscene;

constexpr auto kBoundSource = static_cast<Cutscene::StreamSourceId>(0x1000);
constexpr auto kOtherSource = static_cast<Cutscene::StreamSourceId>(0x2000);
constexpr std::uint32_t kFullVolume = Cutscene::kMaximumStreamVolume;
// (0x7F00 * pan gain) >> 16 for the fixture's centre and hard-left gains.
constexpr std::uint32_t kCentreGain = 11494;
constexpr std::uint32_t kHardGain = 16255;

struct VoiceStart {
  std::size_t channel = 0;
  std::int32_t voice = 0;
  float rate = 0;
  std::uint32_t left = 0;
  std::uint32_t right = 0;
  bool reverb = false;
};

struct GainWrite {
  std::int32_t voice = 0;
  std::uint32_t left = 0;
  std::uint32_t right = 0;

  friend bool operator==(const GainWrite&, const GainWrite&) = default;
};

class RecordingStream final : public StreamHost {
 public:
  std::vector<std::string> calls;
  StreamVoices voices;
  StreamMix mix;
  std::array<std::int16_t, Cutscene::kPanGainCount> pan_gains{};
  std::optional<StreamFormat> format = StreamFormat{1, false, 2048, 44100};
  std::optional<std::int32_t> free_pair = 4;
  std::vector<std::pair<std::int32_t, bool>> paused;
  std::uint32_t keyed_off = 0;
  std::uint32_t released = 0;
  std::optional<StreamStart> started;
  std::vector<VoiceStart> voice_starts;
  std::optional<std::size_t> failing_channel;
  std::vector<GainWrite> gain_writes;
  std::optional<StreamGains> applied;
  std::optional<StreamFade> fade;
  bool slot_bound = false;
  std::optional<std::string> asset_error;
  std::optional<VS::Content::AudioStreamInfo> bound_info;

  RecordingStream() {
    pan_gains[0] = 32767;
    pan_gains[1] = -1;
    pan_gains[127] = 23170;
    pan_gains[128] = 23170;
  }

  [[nodiscard]] StreamVoices Voices() const override { return voices; }
  [[nodiscard]] StreamMix Mix() const override { return mix; }
  [[nodiscard]] std::span<const std::int16_t, Cutscene::kPanGainCount>
  PanGains() const override {
    return pan_gains;
  }
  [[nodiscard]] std::optional<StreamFormat> BoundFormat(
      Cutscene::StreamSourceId source) const override {
    if (source != kBoundSource) return std::nullopt;
    return format;
  }
  std::optional<std::int32_t> FindFreeVoicePair() override {
    calls.emplace_back("FindPair");
    return free_pair;
  }
  void PauseVoice(std::int32_t voice, bool value) override {
    calls.emplace_back("Pause");
    paused.emplace_back(voice, value);
  }
  void KeyOffVoices(std::uint32_t mask) override {
    calls.emplace_back("KeyOff");
    keyed_off = mask;
  }
  void ReleaseVoices(std::uint32_t mask) override {
    calls.emplace_back("Release");
    released = mask;
    voices.mask = 0;
  }
  void BeginStream(const StreamStart& start) override {
    calls.emplace_back("Begin");
    started = start;
    voices = start.voices;
    mix.pan = start.pan;
    mix.stereo_source = (start.format_flags & 1) != 0;
  }
  bool StartVoice(std::size_t channel, std::int32_t voice, float rate,
                  std::uint32_t left, std::uint32_t right,
                  bool reverb) override {
    calls.emplace_back("StartVoice");
    voice_starts.push_back({channel, voice, rate, left, right, reverb});
    return failing_channel != channel;
  }
  void SetVoiceGains(std::int32_t voice, std::uint32_t left,
                     std::uint32_t right) override {
    calls.emplace_back("Gains");
    gain_writes.push_back({voice, left, right});
  }
  [[nodiscard]] std::optional<StreamGains> AppliedGains() const override {
    return applied;
  }
  void SetAppliedGains(const std::optional<StreamGains>& gains) override {
    calls.emplace_back("Applied");
    applied = gains;
  }
  void SetVolume(std::uint32_t volume) override {
    calls.emplace_back("Volume");
    mix.volume = volume;
  }
  [[nodiscard]] StreamFade Fade() const { return fade.value_or(StreamFade{}); }
  void StartFade(const StreamFade& value) override {
    calls.emplace_back("Fade");
    fade = value;
  }
  void SetPan(std::uint32_t pan) override {
    calls.emplace_back("Pan");
    mix.pan = pan;
  }
  [[nodiscard]] bool SlotBound() const override { return slot_bound; }
  std::expected<void, std::string> ValidateAssets(
      const VS::Content::AudioStreamInfo& /*info*/) override {
    calls.emplace_back("Validate");
    if (asset_error) return std::unexpected(*asset_error);
    return {};
  }
  void BindSlot(const VS::Content::AudioStreamInfo& info) override {
    calls.emplace_back("Bind");
    bound_info = info;
    slot_bound = true;
  }
  void UnbindSlot() override {
    calls.emplace_back("Unbind");
    slot_bound = false;
  }
};

using Calls = std::vector<std::string>;
using GainWrites = std::vector<GainWrite>;

Calls StopCalls() { return {"Pause", "Pause", "KeyOff", "Release", "Applied"}; }

void Put32(std::vector<std::uint8_t>& bytes, std::size_t offset,
           std::uint32_t value) {
  for (std::size_t byte = 0; byte < 4; ++byte) {
    bytes[offset + byte] = static_cast<std::uint8_t>(value >> (byte * 8));
  }
}

// A one-block mono AKAO stream at pitch 2048.
std::vector<std::uint8_t> MonoStreamBytes() {
  std::vector<std::uint8_t> bytes(VS::Content::kAudioStreamHeaderBytes + 16);
  Put32(bytes, 0, 0x4F414B41);
  Put32(bytes, 16, 16);
  Put32(bytes, 28, 2048);
  return bytes;
}

// Saves and restores the live driver state the Live bindings touch.
class LiveStreamState {
 public:
  LiveStreamState()
      : stream_(Audio_Cutscene_streamState),
        slot_(Audio_soundState.sfxStreamData[2]) {
    Audio_Cutscene_streamState.voiceMask = 0;
    Audio_Cutscene_streamState.volume = 0x1234;
    Audio_Cutscene_streamState.volumeFadeStep = 0;
    Audio_Cutscene_streamState.volumeFadeTicksLeft = 0;
    Audio_Cutscene_streamState.pan.word = 0x4000;
    Audio_soundState.sfxStreamData[2] = nullptr;
  }
  LiveStreamState(const LiveStreamState&) = delete;
  LiveStreamState& operator=(const LiveStreamState&) = delete;
  ~LiveStreamState() {
    Audio_Cutscene_streamState = stream_;
    Audio_soundState.sfxStreamData[2] = slot_;
  }

 private:
  Audio_CutsceneStreamState stream_;
  void* slot_;
};

}  // namespace

TEST_CASE("Stream gains follow the mono output, stereo and panned layouts",
          "[audio][stream]") {
  const RecordingStream host;
  StreamMix mix{kFullVolume, 0x0000, true, true};
  CHECK(Cutscene::MixStreamGains(mix, host.pan_gains) ==
        StreamGains{kCentreGain, kCentreGain, kCentreGain, kCentreGain});
  mix.mono_output = false;
  CHECK(Cutscene::MixStreamGains(mix, host.pan_gains) ==
        StreamGains{0x3F80, 0, 0, 0x3F80});
  mix.stereo_source = false;
  CHECK(Cutscene::MixStreamGains(mix, host.pan_gains) ==
        StreamGains{kHardGain, 0, kHardGain, 0});
  mix.pan = 0x8000;
  CHECK(Cutscene::MixStreamGains(mix, host.pan_gains) ==
        StreamGains{kCentreGain, kCentreGain, kCentreGain, kCentreGain});
  // The original multiplies the signed table entry as unsigned.
  mix.pan = 0x0100;
  CHECK(Cutscene::MixStreamGains(mix, host.pan_gains) ==
        StreamGains{0xFFFF, 0, 0xFFFF, 0});
}

TEST_CASE("Stream gains are written only while playing and when changed",
          "[audio][stream]") {
  RecordingStream host;
  host.mix.pan = 0x8000;
  CHECK_FALSE(Cutscene::ApplyStreamGains(host));
  CHECK(host.calls.empty());
  host.voices = {4, 0x30};
  CHECK(Cutscene::ApplyStreamGains(host));
  CHECK(host.gain_writes == GainWrites{{4, kCentreGain, kCentreGain},
                                       {5, kCentreGain, kCentreGain}});
  CHECK(host.calls == Calls{"Gains", "Gains", "Applied"});
  CHECK_FALSE(Cutscene::ApplyStreamGains(host));
  CHECK(host.calls.size() == 3);
}

TEST_CASE("Stream pause and resume touch both voices of a playing pair",
          "[audio][stream]") {
  RecordingStream host;
  CHECK_FALSE(Cutscene::PauseStream(host));
  CHECK_FALSE(Cutscene::ResumeStream(host));
  CHECK(host.calls.empty());
  host.voices = {6, 0xC0};
  CHECK(Cutscene::PauseStream(host));
  CHECK(Cutscene::ResumeStream(host));
  CHECK(host.paused == std::vector<std::pair<std::int32_t, bool>>{
                           {6, true}, {7, true}, {6, false}, {7, false}});
}

TEST_CASE("Stopping a stream unpauses, keys off and releases its pair",
          "[audio][stream]") {
  RecordingStream host;
  CHECK(Cutscene::StopStream(host) == 0);
  CHECK(host.calls.empty());
  host.voices = {10, 0xC00};
  host.applied = StreamGains{1, 2, 3, 4};
  CHECK(Cutscene::StopStream(host) == 0xC00);
  CHECK(host.calls == StopCalls());
  CHECK(host.paused ==
        std::vector<std::pair<std::int32_t, bool>>{{10, false}, {11, false}});
  CHECK(host.keyed_off == 0xC00);
  CHECK(host.released == 0xC00);
  CHECK_FALSE(host.applied);
  CHECK(Cutscene::StopStreamVoices(host) == 0);
}

TEST_CASE("Stream starts require the bound source and a valid packed pan",
          "[audio][stream]") {
  RecordingStream host;
  CHECK(Cutscene::StartStream(host, kOtherSource, 0, false) ==
        StreamStatus::kUnbound);
  for (const std::uint32_t pan : {0x0080U, 0xFF01U, 0x10000U, 0xFFFFFFFFU}) {
    CHECK(Cutscene::StartStream(host, kBoundSource, pan, false) ==
          StreamStatus::kInvalidPan);
  }
  CHECK(host.calls.empty());
}

TEST_CASE("Stream starts refuse formats the SPU cannot play",
          "[audio][stream]") {
  RecordingStream host;
  host.format = StreamFormat{1, false, 2048, 0};
  CHECK(Cutscene::StartStream(host, kBoundSource, 0, false) ==
        StreamStatus::kInvalidFormat);
  host.format = StreamFormat{3, false, 2048, 22050};
  CHECK(Cutscene::StartStream(host, kBoundSource, 0, false) ==
        StreamStatus::kInvalidFormat);
  host.format = StreamFormat{0, false, 2048, 22050};
  CHECK(Cutscene::StartStream(host, kBoundSource, 0, false) ==
        StreamStatus::kInvalidFormat);
  CHECK(host.calls.empty());
}

TEST_CASE("Stream starts refuse voice pairs outside the 24 SPU voices",
          "[audio][stream]") {
  RecordingStream host;
  for (const auto pair :
       {std::optional<std::int32_t>{}, std::optional{-1}, std::optional{23}}) {
    host.calls.clear();
    host.free_pair = pair;
    CHECK(Cutscene::StartStream(host, kBoundSource, 0, false) ==
          StreamStatus::kNoVoicePair);
    CHECK(host.calls == Calls{"FindPair"});
  }
  CHECK_FALSE(host.started);
}

TEST_CASE("Stream starts reserve the pair and record the stereo stream",
          "[audio][stream]") {
  RecordingStream host;
  host.format = StreamFormat{2, true, 0x1000, 44100};
  host.free_pair = 22;
  CHECK(Cutscene::StartStream(host, kBoundSource, 0x4000, true) ==
        StreamStatus::kApplied);
  CHECK(host.calls ==
        Calls{"FindPair", "Begin", "Applied", "StartVoice", "StartVoice"});
  REQUIRE(host.started);
  const auto started = host.started.value_or(StreamStart{});
  CHECK(started.voices.first == 22);
  CHECK(started.voices.mask == 0xC00000);
  CHECK(started.pan == 0x4000);
  CHECK(started.format_flags == 3);
  CHECK(started.sample_rate == 0x1000);
  CHECK(started.reverb);
}

TEST_CASE("Stereo stream starts route each channel to its own voice",
          "[audio][stream]") {
  RecordingStream host;
  host.format = StreamFormat{2, true, 0x1000, 44100};
  host.free_pair = 22;
  CHECK(Cutscene::StartStream(host, kBoundSource, 0x4000, true) ==
        StreamStatus::kApplied);
  CHECK(host.applied == StreamGains{0x3F80, 0, 0, 0x3F80});
  REQUIRE(host.voice_starts.size() == 2);
  const auto& left = host.voice_starts[0];
  const auto& right = host.voice_starts[1];
  CHECK(left.voice == 22);
  CHECK(left.rate == 1.0F);
  CHECK((left.left == 0x3F80 && left.right == 0));
  CHECK((right.channel == 1 && right.voice == 23));
  CHECK((right.left == 0 && right.right == 0x3F80));
  CHECK(right.reverb);
}

TEST_CASE("Mono stream starts pan both voices at the source pitch",
          "[audio][stream]") {
  RecordingStream host;
  host.format = StreamFormat{1, false, 2048, 44100};
  CHECK(Cutscene::StartStream(host, kBoundSource, 0x8000, false) ==
        StreamStatus::kApplied);
  REQUIRE(host.started);
  const auto started = host.started.value_or(StreamStart{});
  CHECK(started.voices.mask == 0x30);
  CHECK(started.format_flags == 0);
  REQUIRE(host.voice_starts.size() == 2);
  CHECK(host.voice_starts[0].rate == 0.5F);
  CHECK(host.voice_starts[1].left == kCentreGain);
  CHECK(host.voice_starts[1].right == kCentreGain);
  CHECK_FALSE(host.voice_starts[1].reverb);
}

TEST_CASE("A stream voice that fails to start stops the pair",
          "[audio][stream]") {
  RecordingStream host;
  host.failing_channel = 1;
  CHECK(Cutscene::StartStream(host, kBoundSource, 0, false) ==
        StreamStatus::kVoiceFailed);
  CHECK(host.keyed_off == 0x30);
  CHECK(host.released == 0x30);
  CHECK(host.voices.mask == 0);
  CHECK_FALSE(host.applied);
}

TEST_CASE("Replacing a stream refuses an invalid pan before stopping",
          "[audio][stream]") {
  RecordingStream host;
  host.voices = {8, 0x300};
  CHECK(Cutscene::ReplaceStream(host, kBoundSource, 0x0080, false) ==
        StreamStatus::kInvalidPan);
  CHECK(host.calls.empty());
  CHECK(host.voices.mask == 0x300);
  CHECK(Cutscene::ReplaceStream(host, kBoundSource, 0x8000, false) ==
        StreamStatus::kApplied);
  Calls expected = StopCalls();
  expected.insert(expected.end(),
                  {"FindPair", "Begin", "Applied", "StartVoice", "StartVoice"});
  CHECK(host.calls == expected);
  CHECK(host.keyed_off == 0x300);
  CHECK(host.voices.mask == 0x30);
}

TEST_CASE("Stream volume refuses values above 0x7F00", "[audio][stream]") {
  RecordingStream host;
  CHECK(Cutscene::SetStreamVolume(host, 0x7F01) ==
        StreamStatus::kInvalidVolume);
  CHECK(host.calls.empty());
  CHECK(Cutscene::SetStreamVolume(host, 0x4000) == StreamStatus::kApplied);
  CHECK(host.calls == Calls{"Volume"});
  host.voices = {4, 0x30};
  CHECK(Cutscene::SetStreamVolume(host, 0x4000) == StreamStatus::kApplied);
  CHECK(host.calls == Calls{"Volume", "Volume", "Gains", "Gains", "Applied"});
  CHECK(host.gain_writes == GainWrites{{4, 8191, 0}, {5, 8191, 0}});
}

TEST_CASE("Stream fades refuse long durations and loud targets",
          "[audio][stream]") {
  RecordingStream host;
  CHECK(Cutscene::SlideStreamVolume(host, 0x8000, 0) ==
        StreamStatus::kInvalidFade);
  CHECK(Cutscene::SlideStreamVolume(host, 0, 0x7F01) ==
        StreamStatus::kInvalidFade);
  CHECK(host.calls.empty());
  CHECK_FALSE(host.fade);
}

TEST_CASE("Stream fades split the volume change over their ticks",
          "[audio][stream]") {
  RecordingStream host;
  CHECK(Cutscene::SlideStreamVolume(host, 4, 0) == StreamStatus::kApplied);
  REQUIRE(host.fade);
  CHECK(host.Fade().step == -0x1FC0);
  CHECK(host.Fade().ticks == 4);
  CHECK(host.Fade().target == 0);
  CHECK(Cutscene::SlideStreamVolume(host, 3, 0) == StreamStatus::kApplied);
  CHECK(host.Fade().step == -10837);
}

TEST_CASE("Stream fades take at least one tick and clamp loud sources",
          "[audio][stream]") {
  RecordingStream host;
  // A zero duration slides in one tick.
  CHECK(Cutscene::SlideStreamVolume(host, 0, 0x1000) == StreamStatus::kApplied);
  CHECK(host.Fade().ticks == 1);
  CHECK(host.Fade().step == 0x1000 - 0x7F00);
  CHECK(Cutscene::SlideStreamVolume(host, 0x7FFF, 0x7F00) ==
        StreamStatus::kApplied);
  CHECK(host.Fade().step == 0);
  host.mix.volume = 0xFFFF;
  CHECK(Cutscene::SlideStreamVolume(host, 1, 0x7F00) == StreamStatus::kApplied);
  CHECK(host.Fade().step == 0);
}

TEST_CASE("Stream pan refuses fractional or wide pans", "[audio][stream]") {
  RecordingStream host;
  for (const std::uint32_t pan : {0x0001U, 0xFF80U, 0x10000U}) {
    CHECK(Cutscene::SetStreamPan(host, pan) == StreamStatus::kInvalidPan);
  }
  CHECK(host.calls.empty());
  CHECK(Cutscene::SetStreamPan(host, 0xFF00) == StreamStatus::kApplied);
  CHECK(host.calls == Calls{"Pan"});
  host.voices = {4, 0x30};
  CHECK(Cutscene::SetStreamPan(host, 0xFF00) == StreamStatus::kApplied);
  CHECK(host.gain_writes == GainWrites{{4, 0, kHardGain}, {5, 0, kHardGain}});
}

TEST_CASE("Room stream binding refuses an occupied slot and missing data",
          "[audio][stream]") {
  RecordingStream host;
  const auto bytes = MonoStreamBytes();
  host.slot_bound = true;
  const auto result = VS::Audio::BindRoomCutsceneStream(host, bytes);
  CHECK(result.status == RoomStreamStatus::kSlotOccupied);
  CHECK(result.handle == 0);
  host.slot_bound = false;
  CHECK(VS::Audio::BindRoomCutsceneStream(host, {}).status ==
        RoomStreamStatus::kMissingData);
  CHECK(host.calls.empty());
}

TEST_CASE("Room stream binding validates the stream and its assets",
          "[audio][stream]") {
  RecordingStream host;
  const auto bytes = MonoStreamBytes();
  auto invalid = bytes;
  invalid[0] = 0;
  auto result = VS::Audio::BindRoomCutsceneStream(host, invalid);
  CHECK(result.status == RoomStreamStatus::kInvalidStream);
  CHECK_FALSE(result.error.empty());
  CHECK(host.calls.empty());
  host.asset_error = "missing stream assets";
  result = VS::Audio::BindRoomCutsceneStream(host, bytes);
  CHECK(result.status == RoomStreamStatus::kMissingAssets);
  CHECK(result.error == "missing stream assets");
  CHECK(host.calls == Calls{"Validate"});
  CHECK_FALSE(host.slot_bound);
}

TEST_CASE("Room stream binding returns handle 3 for a valid stream",
          "[audio][stream]") {
  RecordingStream host;
  const auto result =
      VS::Audio::BindRoomCutsceneStream(host, MonoStreamBytes());
  CHECK(result.status == RoomStreamStatus::kBound);
  CHECK(result.handle == VS::Audio::kRoomStreamHandle);
  CHECK(host.calls == Calls{"Validate", "Bind"});
  REQUIRE(host.bound_info);
  const auto info = host.bound_info.value_or(VS::Content::AudioStreamInfo{});
  CHECK(info.channels == 1);
  CHECK(info.source_pitch == 2048);
  CHECK(info.sample_rate == 22050);
  CHECK(host.slot_bound);
}

TEST_CASE("Room stream unbinding stops the pair before clearing the slot",
          "[audio][stream]") {
  RecordingStream host;
  CHECK(VS::Audio::UnbindRoomCutsceneStream(host) == 0);
  CHECK(host.calls.empty());
  host.slot_bound = true;
  CHECK(VS::Audio::UnbindRoomCutsceneStream(host) == 1);
  CHECK(host.calls == Calls{"Unbind"});
  host.calls.clear();
  host.slot_bound = true;
  host.voices = {4, 0x30};
  CHECK(VS::Audio::UnbindRoomCutsceneStream(host) == 1);
  Calls expected = StopCalls();
  expected.emplace_back("Unbind");
  CHECK(host.calls == expected);
  CHECK_FALSE(host.slot_bound);
}

TEST_CASE("Live stream commands ignore null parameter blocks",
          "[audio][stream]") {
  const LiveStreamState state;
  Cutscene::ReplaceLiveStream(nullptr);
  Cutscene::SetLiveStreamVolume(nullptr);
  Cutscene::SlideLiveStreamVolume(nullptr);
  Cutscene::SetLiveStreamPan(nullptr);
  Cutscene::PauseLiveStream(nullptr);
  Cutscene::ResumeLiveStream(nullptr);
  Cutscene::StopLiveStream(nullptr);
  CHECK(Audio_Cutscene_streamState.volume == 0x1234);
  CHECK(Audio_Cutscene_streamState.volumeFadeTicksLeft == 0);
  CHECK(Audio_Cutscene_streamState.pan.word == 0x4000);
  CHECK(Audio_Cutscene_streamState.voiceMask == 0);
}

TEST_CASE("Live stream commands refuse parameters wider than a command word",
          "[audio][stream]") {
  const LiveStreamState state;
  Audio_CommandParams params{};
  if constexpr (sizeof(std::uintptr_t) > sizeof(std::uint32_t)) {
    params.param1 = (std::uintptr_t{1} << 32U) | 0x4000U;
    Cutscene::SetLiveStreamVolume(&params);
    CHECK(Audio_Cutscene_streamState.volume == 0x1234);
    params.param1 = (std::uintptr_t{1} << 32U) | 0x8000U;
    Cutscene::SetLiveStreamPan(&params);
    CHECK(Audio_Cutscene_streamState.pan.word == 0x4000);
  }
  params.param1 = 0x4000;
  Cutscene::SetLiveStreamVolume(&params);
  CHECK(Audio_Cutscene_streamState.volume == 0x4000);
  params.param1 = 8;
  params.param2 = 0;
  Cutscene::SlideLiveStreamVolume(&params);
  CHECK(Audio_Cutscene_streamState.volumeFadeTicksLeft == 8);
  CHECK(Audio_Cutscene_streamState.volumeFadeStep == -0x800);
}

TEST_CASE("Live room stream bindings report an empty slot and missing data",
          "[audio][stream]") {
  const LiveStreamState state;
  CHECK(VS::Audio::UnbindLiveRoomCutsceneStream() == 0);
  CHECK(VS::Audio::BindLiveRoomCutsceneStream(nullptr, 0) == 0);
  CHECK(Audio_soundState.sfxStreamData[2] == nullptr);
}
