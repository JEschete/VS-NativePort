#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/Control/SoundControl.hpp"
#include "Audio/Cutscene/Stream.hpp"
#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/PlaybackReset.hpp"
#include "Audio/PlaybackResetLive.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/SoundEffects/SoundEffects.hpp"

namespace {

using VS::Audio::Driver::ChannelKind;
using VS::Audio::Driver::ChannelReset;
using VS::Audio::Driver::ChannelResetFor;
using VS::Audio::Driver::DefaultPlaybackReset;
using VS::Audio::Driver::DriverControlReset;
using VS::Audio::Driver::MusicLayerReset;
using VS::Audio::Driver::PlaybackResetHost;
using VS::Audio::Driver::ResetPlaybackState;
using VS::Audio::Driver::VoiceSchedulerReset;

class RecordingHost final : public PlaybackResetHost {
 public:
  std::vector<std::string> calls;
  std::vector<ChannelReset> music;
  std::vector<ChannelReset> effects;
  std::vector<std::size_t> music_slots;
  std::vector<std::size_t> effect_slots;
  MusicLayerReset layers;
  VoiceSchedulerReset scheduler;
  DriverControlReset controls;
  std::uint32_t voice_mode_update = 0;
  int reverb_mode = -1;
  bool reverb_enabled = false;

  void ResetOutput() override { calls.emplace_back("Output"); }
  void ResetMusicLayers(const MusicLayerReset& reset) override {
    calls.emplace_back("MusicLayers");
    layers = reset;
  }
  void ResetVoiceScheduler(const VoiceSchedulerReset& reset) override {
    calls.emplace_back("VoiceScheduler");
    scheduler = reset;
  }
  void ResetDriverControls(const DriverControlReset& reset) override {
    calls.emplace_back("DriverControls");
    controls = reset;
  }
  void ResetChannel(std::size_t index, const ChannelReset& reset) override {
    const bool effect = reset.kind == ChannelKind::kSoundEffect;
    if (calls.empty() || calls.back() != (effect ? "Effects" : "Music")) {
      calls.emplace_back(effect ? "Effects" : "Music");
    }
    (effect ? effects : music).push_back(reset);
    (effect ? effect_slots : music_slots).push_back(index);
  }
  void RequestVoiceModeUpdate(std::uint32_t flags) override {
    calls.emplace_back("VoiceModeUpdate");
    voice_mode_update = flags;
  }
  void SetReverbMode(int mode) override {
    calls.emplace_back("ReverbMode");
    reverb_mode = mode;
  }
  void EnableReverb(bool enabled) override {
    calls.emplace_back("EnableReverb");
    reverb_enabled = enabled;
  }
};

}  // namespace

TEST_CASE("Playback reset uses the original driver defaults",
          "[audio][driver]") {
  const auto reset = DefaultPlaybackReset();
  CHECK(reset.music.master_volume == 0x7F0000);
  CHECK(reset.music.reverb_depth == 0x3FFF000);
  CHECK(reset.voices.effect_tempo == 0x66A80000U);
  CHECK(reset.voices.tempo_accumulator == 1);
  CHECK(reset.controls.mix_behavior == 1);
  CHECK(reset.controls.disc_volume == 0x7FFF0000);
  CHECK(reset.controls.stream_volume == 0x7F00);
  CHECK(reset.voice_mode_update == 0x80);
  CHECK(reset.reverb_mode == 4);
}

TEST_CASE("Playback reset frees every music channel's voice",
          "[audio][driver]") {
  ChannelReset reset;
  REQUIRE(ChannelResetFor(ChannelKind::kMusic, 31, reset));
  CHECK(reset.kind == ChannelKind::kMusic);
  CHECK(reset.voice == VS::Audio::Driver::kNoVoice);
  CHECK(reset.effect_volume == 0);
}

TEST_CASE("Playback reset maps sound effects to the upper voices",
          "[audio][driver]") {
  ChannelReset reset;
  REQUIRE(ChannelResetFor(ChannelKind::kSoundEffect, 0, reset));
  CHECK(reset.kind == ChannelKind::kSoundEffect);
  CHECK(reset.voice == 12);
  CHECK(reset.effect_volume == 0x7F00);
  REQUIRE(ChannelResetFor(ChannelKind::kSoundEffect, 11, reset));
  CHECK(reset.voice == 23);
}

TEST_CASE("Playback reset rejects channel slots outside its tables",
          "[audio][driver]") {
  ChannelReset reset{
      .kind = ChannelKind::kSoundEffect, .voice = 7, .effect_volume = 3};
  CHECK_FALSE(ChannelResetFor(ChannelKind::kMusic, 32, reset));
  CHECK_FALSE(ChannelResetFor(ChannelKind::kSoundEffect, 12, reset));
  CHECK(reset.voice == 7);
  CHECK(reset.effect_volume == 3);
}

TEST_CASE("Playback reset resets output first and enables reverb last",
          "[audio][driver]") {
  RecordingHost host;
  ResetPlaybackState(host);
  CHECK(host.calls == std::vector<std::string>{
                          "Output", "MusicLayers", "VoiceScheduler",
                          "DriverControls", "Music", "Effects",
                          "VoiceModeUpdate", "ReverbMode", "EnableReverb"});
  CHECK(host.layers.master_volume == 0x7F0000);
  CHECK(host.scheduler.effect_tempo == 0x66A80000U);
  CHECK(host.controls.disc_volume == 0x7FFF0000);
  CHECK(host.voice_mode_update == 0x80);
  CHECK(host.reverb_mode == 4);
  CHECK(host.reverb_enabled);
}

TEST_CASE("Playback reset visits each channel slot once in order",
          "[audio][driver]") {
  RecordingHost host;
  ResetPlaybackState(host);
  REQUIRE(host.music.size() == 32);
  REQUIRE(host.effects.size() == 12);
  for (std::size_t index = 0; index < host.music.size(); ++index) {
    CHECK(host.music_slots[index] == index);
    CHECK(host.music[index].voice == VS::Audio::Driver::kNoVoice);
  }
  for (std::size_t index = 0; index < host.effects.size(); ++index) {
    CHECK(host.effect_slots[index] == index);
    CHECK(host.effects[index].voice == 12 + index);
  }
}

namespace {

constexpr std::size_t kEffectTableSize = 22;

// Saves the channel tables, marks every channel stale and restores the tables
// when the test ends.
class LiveDriverFixture {
 public:
  LiveDriverFixture() {
    std::memcpy(saved_music_, VS::Audio::Seq::musicChannels,
                sizeof(saved_music_));
    std::memcpy(saved_effects_, VS::Audio::SoundEffects::channels,
                sizeof(saved_effects_));
    for (auto& channel : VS::Audio::Seq::musicChannels) {
      channel.voice.assigned_voice = 3;
      channel.type = 1;
      channel.update_flags = 0xFF;
    }
    for (auto& channel : VS::Audio::SoundEffects::channels) {
      channel.voice.assigned_voice = 2;
      channel.effect_volume = 5;
      channel.effect_pitch_scale = 9;
    }
    VS::Audio::Driver::controlLatches.update_flags = 0x01;
    VS::Audio::Driver::controlLatches.control_latches = 0xFF;
    VS::Audio::Seq::activeMusicConfig = &VS::Audio::Seq::savedMusicConfig;
  }
  ~LiveDriverFixture() {
    std::memcpy(VS::Audio::Seq::musicChannels, saved_music_,
                sizeof(saved_music_));
    std::memcpy(VS::Audio::SoundEffects::channels, saved_effects_,
                sizeof(saved_effects_));
  }
  LiveDriverFixture(const LiveDriverFixture&) = delete;
  LiveDriverFixture& operator=(const LiveDriverFixture&) = delete;

 private:
  VS::Audio::SeqChannel
      saved_music_[std::size(VS::Audio::Seq::musicChannels)]{};
  VS::Audio::SeqChannel saved_effects_[kEffectTableSize]{};
};

}  // namespace

TEST_CASE("Live playback reset makes the primary music layer active",
          "[audio][driver]") {
  const LiveDriverFixture fixture;
  VS::Audio::Driver::ResetLivePlaybackState();
  CHECK(VS::Audio::Seq::activeMusicConfig ==
        &VS::Audio::Seq::primaryMusicConfig);
  CHECK(VS::Audio::Seq::secondaryMusicConfig == nullptr);
  CHECK(VS::Audio::Seq::secondaryMusicChannels == nullptr);
  CHECK(VS::Audio::Seq::primaryMusicConfig.master_volume == 0x7F0000);
  CHECK(VS::Audio::Seq::primaryMusicConfig.reverb_depth == 0x3FFF000);
}

TEST_CASE("Live playback reset restores the mixer and driver controls",
          "[audio][driver]") {
  const LiveDriverFixture fixture;
  VS::Audio::Driver::ResetLivePlaybackState();
  CHECK(VS::Audio::Mixer::voiceScheduler.effect_tempo == 0x66A80000U);
  CHECK(VS::Audio::Mixer::voiceScheduler.tempo_accumulator == 1);
  CHECK(VS::Audio::Seq::discVolume == 0x7FFF0000);
  CHECK(VS::Audio::Cutscene::streamState.volume == 0x7F00);
  CHECK(VS::Audio::Driver::controlLatches.control_latches == 0);
  CHECK(VS::Audio::Driver::controlLatches.mix_behavior == 1);
  CHECK(VS::Audio::Driver::controlLatches.update_flags == 0x81);
}

TEST_CASE("Live playback reset frees the music channels", "[audio][driver]") {
  const LiveDriverFixture fixture;
  VS::Audio::Driver::ResetLivePlaybackState();
  for (const auto& channel : VS::Audio::Seq::musicChannels) {
    CHECK(channel.voice.assigned_voice == 24);
    CHECK(channel.type == 0);
    CHECK(channel.update_flags == 0);
  }
}

TEST_CASE("Live playback reset leaves effect slots past the twelve voices",
          "[audio][driver]") {
  const LiveDriverFixture fixture;
  VS::Audio::Driver::ResetLivePlaybackState();
  for (std::size_t index = 0; index < 12; ++index) {
    const auto& channel = VS::Audio::SoundEffects::channels[index];
    CHECK(channel.voice.assigned_voice == 12 + index);
    CHECK(channel.type == 1);
    CHECK(channel.effect_volume == 0x7F00);
    CHECK(channel.effect_pitch_scale == 0);
  }
  for (std::size_t index = 12; index < kEffectTableSize; ++index) {
    const auto& channel = VS::Audio::SoundEffects::channels[index];
    CHECK(channel.voice.assigned_voice == 2);
    CHECK(channel.effect_volume == 5);
    CHECK(channel.effect_pitch_scale == 9);
  }
}
