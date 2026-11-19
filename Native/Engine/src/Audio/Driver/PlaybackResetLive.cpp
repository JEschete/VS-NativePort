// Hand-maintained live binding for VS::Audio::Driver::ResetPlaybackState.
// Each store names the field its PS1 offset lands on (FIELD_LEDGER via the
// type layouts); the comment on a store is that offset.
#include "Audio/PlaybackResetLive.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>

#include "Audio/Control/SoundControl.hpp"
#include "Audio/Cutscene/Stream.hpp"
#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/MixControlLive.hpp"
#include "Audio/Mixer/MixControl.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/PlaybackReset.hpp"
#include "Audio/Samples/SampleBanks.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/Seq/Sequencer.hpp"
#include "Audio/SoundEffects/SoundEffects.hpp"
#include "Audio/VoiceOutputLive.hpp"

namespace VS::Audio::Driver {
namespace {
static_assert(std::size(VS::Audio::Seq::musicChannels) >= kMusicChannelCount);
static_assert(std::size(VS::Audio::SoundEffects::channels) >=
              kSoundEffectChannelCount);

void InitializePendingModulation(VS::Audio::SeqChannel& channel) {
  // Original B4/B8/BC commands (SLUS_010.40/25AC.c) use phase 1 until the
  // first waveform sample. Depth slides can precede those commands; keep
  // unconfigured native oscillators pending without altering cached ones.
  if (channel.vibrato_wave == nullptr) channel.vibrato_rate_counter = 1;
  if (channel.tremolo_wave == nullptr) channel.tremolo_rate_counter = 1;
  if (channel.auto_pan_wave == nullptr) channel.auto_pan_rate_counter = 1;
}

class LivePlaybackReset final : public PlaybackResetHost {
 public:
  void ResetOutput() override { VS::Audio::Driver::ResetVoiceOutput(); }

  void ResetMusicLayers(const MusicLayerReset& reset) override {
    VS::Audio::Seq::activeMusicConfig = &VS::Audio::Seq::primaryMusicConfig;
    VS::Audio::Seq::secondaryMusicConfig = nullptr;
    // the original also stores &musicChannels in D_800377FC, which nothing
    // reads
    VS::Audio::Seq::secondaryMusicChannels = nullptr;
    VS::Audio::Seq::randomPitchPhase = 0;
    VS::Audio::Seq::pendingMusicData.param1 = 0;
    VS::Audio::MusicConfig& primary = VS::Audio::Seq::primaryMusicConfig;
    primary.active_channels = 0;                           // +0x04
    primary.keyed_voices = 0;                              // +0x08
    primary.music_id = 0;                                  // +0x6E
    primary.paused_channels = 0;                           // +0x1C
    primary.master_volume = reset.master_volume;           // +0x60
    primary.master_volume_slide_length = 0;                // +0x68
    primary.noise_channels = 0;                            // +0x4C
    primary.reverb_channels = 0;                           // +0x50
    primary.fm_channels = 0;                               // +0x54
    primary.timer_lower = 0;                               // +0x78
    primary.timer_upper_count = 0;                         // +0x76
    primary.timer_upper = 0;                               // +0x74
    primary.timer_top = 0;                                 // +0x7C
    primary.pending_key_off = 0;                           // +0x18
    primary.active_notes = 0;                              // +0x14
    primary.pending_key_on = 0;                            // +0x10
    primary.reverb_depth = reset.reverb_depth;             // +0x58
    primary.reverb_depth_slide_step = 0;                   // +0x5C
    primary.reverb_depth_slide_length = 0;                 // +0x6A
    VS::Audio::Seq::savedMusicConfig.music_id = 0;         // +0x6E
    VS::Audio::Seq::savedMusicConfig.active_channels = 0;  // +0x04
    VS::Audio::Mixer::musicPitchScaleSlideLength = 0;
    VS::Audio::Mixer::musicPitchScale = 0;
    VS::Audio::Mixer::tempoScaleSlideLength = 0;
    VS::Audio::Mixer::tempoScale = 0;
    VS::Audio::Seq::musicLoopsRemaining = 0;
    VS::Audio::Mixer::mutedMusicChannels = 0;
  }

  void ResetVoiceScheduler(const VoiceSchedulerReset& reset) override {
    VS::Audio::VoiceScheduler& scheduler = VS::Audio::Mixer::voiceScheduler;
    scheduler.active_voices = 0;
    scheduler.paused_effect_voices = 0;                     // +0x10
    scheduler.noise_voices = 0;                             // +0x1C
    scheduler.reverb_voices = 0;                            // +0x20
    scheduler.fm_voices = 0;                                // +0x24
    scheduler.tempo_accumulator = reset.tempo_accumulator;  // +0x18
    scheduler.effect_tempo = reset.effect_tempo;            // +0x14
    scheduler.pending_key_off = 0;                          // +0x0C
    scheduler.keyed_voices = 0;                             // +0x08
    scheduler.pending_key_on = 0;                           // +0x04
    VS::Audio::Mixer::voiceModes.fm = 0;
    VS::Audio::Mixer::voiceModes.noise = 0;
    VS::Audio::Mixer::voiceModes.reverb = 0;
  }

  void ResetDriverControls(const DriverControlReset& reset) override {
    VS::Audio::Driver::controlLatches.control_latches = 0;
    VS::Audio::Driver::controlLatches.mix_behavior = reset.mix_behavior;
    VS::Audio::Samples::musicSampleBankIds.alternate_bank_id = 0;
    VS::Audio::Samples::musicSampleBankIds.bank_id = 0;
    VS::Audio::Samples::soundEffectBankIds[2] = 0;
    VS::Audio::Samples::soundEffectBankIds[1] = 0;
    VS::Audio::Samples::soundEffectBankIds[0] = 0;
    VS::Audio::Seq::discVolume = reset.disc_volume;
    VS::Audio::Seq::discVolumeFadeLength = 0;
    VS::Audio::Cutscene::streamState.control_flags = 0;             // +0x5C
    VS::Audio::Cutscene::streamState.volume = reset.stream_volume;  // +0x40
    VS::Audio::Cutscene::streamState.volume_fade_ticks_left = 0;    // +0x48
  }

  void ResetChannel(std::size_t index, const ChannelReset& reset) override {
    const bool effect = reset.kind == ChannelKind::kSoundEffect;
    VS::Audio::SeqChannel& channel =
        effect ? VS::Audio::SoundEffects::channels[index]
               : VS::Audio::Seq::musicChannels[index];
    channel.update_flags = 0;                               // +0x34
    channel.voice.assigned_voice = reset.voice;             // +0xF4
    channel.type = static_cast<std::uint16_t>(reset.kind);  // +0x64
    channel.priority = 0;                                   // +0x58
    if (effect) {
      channel.effect_volume = reset.effect_volume;  // +0xDC
      channel.effect_volume_slide_length = 0;       // +0x8C
      channel.effect_pitch_scale_slide_length = 0;  // +0x86
      channel.effect_pitch_scale = 0;               // +0x40
      channel.key_on_volume_slide_length = 0;       // +0x8A
    }
    InitializePendingModulation(channel);
  }

  void RequestVoiceModeUpdate(std::uint32_t flags) override {
    VS::Audio::Driver::controlLatches.update_flags |= flags;
  }

  void SetReverbMode(int mode) override { Mixer::LiveSetReverbMode(mode); }

  void EnableReverb(bool enabled) override {
    VS::Audio::Driver::EnableOutputReverb(enabled ? 1 : 0);
  }
};
}  // namespace

void ResetLivePlaybackState() {
  LivePlaybackReset host;
  ResetPlaybackState(host);
}
}  // namespace VS::Audio::Driver
