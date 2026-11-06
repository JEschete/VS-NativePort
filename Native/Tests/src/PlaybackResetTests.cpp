#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/PlaybackReset.hpp"
#include "Audio/PlaybackResetLive.hpp"
extern "C" {
#include "Audio/Control/SoundControl.h"
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Channel.h"
#include "Audio/SoundEffects/SoundEffects.h"
}  // extern "C"

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
    std::memcpy(saved_music_, Audio_Seq_musicChannels, sizeof(saved_music_));
    std::memcpy(saved_effects_, Audio_SoundEffects_channels,
                sizeof(saved_effects_));
    for (auto& channel : Audio_Seq_musicChannels) {
      channel.voice.assignedVoice = 3;
      channel.type = 1;
      channel.updateFlags = 0xFF;
    }
    for (auto& channel : Audio_SoundEffects_channels) {
      channel.voice.assignedVoice = 2;
      channel.effectVolume = 5;
      channel.effectPitchScale = 9;
    }
    Audio_Driver_controlLatches.updateFlags = 0x01;
    Audio_Driver_controlLatches.controlLatches = 0xFF;
    Audio_Seq_activeMusicConfig = &Audio_Seq_savedMusicConfig;
  }
  ~LiveDriverFixture() {
    std::memcpy(Audio_Seq_musicChannels, saved_music_, sizeof(saved_music_));
    std::memcpy(Audio_SoundEffects_channels, saved_effects_,
                sizeof(saved_effects_));
  }
  LiveDriverFixture(const LiveDriverFixture&) = delete;
  LiveDriverFixture& operator=(const LiveDriverFixture&) = delete;

 private:
  Audio_SeqChannel saved_music_[std::size(Audio_Seq_musicChannels)]{};
  Audio_SeqChannel saved_effects_[kEffectTableSize]{};
};

}  // namespace

TEST_CASE("Live playback reset makes the primary music layer active",
          "[audio][driver]") {
  const LiveDriverFixture fixture;
  VS::Audio::Driver::ResetLivePlaybackState();
  CHECK(Audio_Seq_activeMusicConfig == &Audio_Seq_primaryMusicConfig);
  CHECK(Audio_Seq_secondaryMusicConfig == nullptr);
  CHECK(Audio_Seq_secondaryMusicChannels == nullptr);
  CHECK(Audio_Seq_primaryMusicConfig.masterVolume == 0x7F0000);
  CHECK(Audio_Seq_primaryMusicConfig.reverbDepth == 0x3FFF000);
}

TEST_CASE("Live playback reset restores the mixer and driver controls",
          "[audio][driver]") {
  const LiveDriverFixture fixture;
  VS::Audio::Driver::ResetLivePlaybackState();
  CHECK(Audio_Mixer_voiceScheduler.effectTempo == 0x66A80000U);
  CHECK(Audio_Mixer_voiceScheduler.tempoAccumulator == 1);
  CHECK(Audio_Seq_discVolume == 0x7FFF0000);
  CHECK(Audio_Cutscene_streamState.volume == 0x7F00);
  CHECK(Audio_Driver_controlLatches.controlLatches == 0);
  CHECK(Audio_Driver_controlLatches.mixBehavior == 1);
  CHECK(Audio_Driver_controlLatches.updateFlags == 0x81);
}

TEST_CASE("Live playback reset frees the music channels", "[audio][driver]") {
  const LiveDriverFixture fixture;
  VS::Audio::Driver::ResetLivePlaybackState();
  for (const auto& channel : Audio_Seq_musicChannels) {
    CHECK(channel.voice.assignedVoice == 24);
    CHECK(channel.type == 0);
    CHECK(channel.updateFlags == 0);
  }
}

TEST_CASE("Live playback reset leaves effect slots past the twelve voices",
          "[audio][driver]") {
  const LiveDriverFixture fixture;
  VS::Audio::Driver::ResetLivePlaybackState();
  for (std::size_t index = 0; index < 12; ++index) {
    const auto& channel = Audio_SoundEffects_channels[index];
    CHECK(channel.voice.assignedVoice == 12 + index);
    CHECK(channel.type == 1);
    CHECK(channel.effectVolume == 0x7F00);
    CHECK(channel.effectPitchScale == 0);
  }
  for (std::size_t index = 12; index < kEffectTableSize; ++index) {
    const auto& channel = Audio_SoundEffects_channels[index];
    CHECK(channel.voice.assignedVoice == 2);
    CHECK(channel.effectVolume == 5);
    CHECK(channel.effectPitchScale == 9);
  }
}
