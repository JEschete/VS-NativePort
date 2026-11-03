// Hand-maintained live binding for VS::Audio::Driver::ResetPlaybackState.
// Each store names the field its PS1 offset lands on (FIELD_LEDGER via the
// type layouts); the comment on a store is that offset.
#include "Audio/PlaybackResetLive.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>

#include "Audio/MixControlLive.hpp"
#include "Audio/PlaybackReset.hpp"
extern "C" {
#include "Audio/Control/SoundControl.h"
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/Sequencer.h"
#include "Audio/SequencerOutput.h"
#include "Audio/SoundEffects/SoundEffects.h"
}  // extern "C"

namespace VS::Audio::Driver {
namespace {
static_assert(std::size(Audio_Seq_musicChannels) >= kMusicChannelCount);
static_assert(std::size(Audio_SoundEffects_channels) >=
              kSoundEffectChannelCount);

void InitializePendingModulation(Audio_SeqChannel& channel) {
  // Original B4/B8/BC commands (SLUS_010.40/25AC.c) use phase 1 until the
  // first waveform sample. Depth slides can precede those commands; keep
  // unconfigured native oscillators pending without altering cached ones.
  if (channel.vibratoWave == nullptr) channel.vibratoRateCounter = 1;
  if (channel.tremoloWave == nullptr) channel.tremoloRateCounter = 1;
  if (channel.autoPanWave == nullptr) channel.autoPanRateCounter = 1;
}

class LivePlaybackReset final : public PlaybackResetHost {
 public:
  void ResetOutput() override { Audio_Native_Reset(); }

  void ResetMusicLayers(const MusicLayerReset& reset) override {
    Audio_Seq_activeMusicConfig = &Audio_Seq_primaryMusicConfig;
    Audio_Seq_secondaryMusicConfig = nullptr;
    // the original also stores &musicChannels in D_800377FC, which nothing
    // reads
    Audio_Seq_secondaryMusicChannels = nullptr;
    Audio_Seq_randomPitchPhase = 0;
    Audio_Seq_pendingMusicData.param1 = 0;
    Audio_MusicConfig& primary = Audio_Seq_primaryMusicConfig;
    primary.activeChannels = 0;                     // +0x04
    primary.keyedVoices = 0;                        // +0x08
    primary.musicId = 0;                            // +0x6E
    primary.pausedChannels = 0;                     // +0x1C
    primary.masterVolume = reset.masterVolume;      // +0x60
    primary.masterVolumeSlideLength = 0;            // +0x68
    primary.noiseChannels = 0;                      // +0x4C
    primary.reverbChannels = 0;                     // +0x50
    primary.fmChannels = 0;                         // +0x54
    primary.timerLower = 0;                         // +0x78
    primary.timerUpperCount = 0;                    // +0x76
    primary.timerUpper = 0;                         // +0x74
    primary.timerTop = 0;                           // +0x7C
    primary.pendingKeyOff = 0;                      // +0x18
    primary.activeNotes = 0;                        // +0x14
    primary.pendingKeyOn = 0;                       // +0x10
    primary.reverbDepth = reset.reverbDepth;        // +0x58
    primary.reverbDepthSlideStep = 0;               // +0x5C
    primary.reverbDepthSlideLength = 0;             // +0x6A
    Audio_Seq_savedMusicConfig.musicId = 0;         // +0x6E
    Audio_Seq_savedMusicConfig.activeChannels = 0;  // +0x04
    Audio_Mixer_musicPitchScaleSlideLength = 0;
    Audio_Mixer_musicPitchScale = 0;
    Audio_Mixer_tempoScaleSlideLength = 0;
    Audio_Mixer_tempoScale = 0;
    Audio_Seq_musicLoopsRemaining = 0;
    Audio_Mixer_mutedMusicChannels = 0;
  }

  void ResetVoiceScheduler(const VoiceSchedulerReset& reset) override {
    Audio_VoiceScheduler& scheduler = Audio_Mixer_voiceScheduler;
    scheduler.activeVoices = 0;
    scheduler.pausedEffectVoices = 0;                     // +0x10
    scheduler.noiseVoices = 0;                            // +0x1C
    scheduler.reverbVoices = 0;                           // +0x20
    scheduler.fmVoices = 0;                               // +0x24
    scheduler.tempoAccumulator = reset.tempoAccumulator;  // +0x18
    scheduler.effectTempo = reset.effectTempo;            // +0x14
    scheduler.pendingKeyOff = 0;                          // +0x0C
    scheduler.keyedVoices = 0;                            // +0x08
    scheduler.pendingKeyOn = 0;                           // +0x04
    Audio_Mixer_voiceModes.fm = 0;
    Audio_Mixer_voiceModes.noise = 0;
    Audio_Mixer_voiceModes.reverb = 0;
  }

  void ResetDriverControls(const DriverControlReset& reset) override {
    Audio_Driver_controlLatches.controlLatches = 0;
    Audio_Driver_controlLatches.mixBehavior = reset.mixBehavior;
    Audio_Samples_musicSampleBankIds.alternateBankId = 0;
    Audio_Samples_musicSampleBankIds.bankId = 0;
    Audio_Samples_soundEffectBankIds[2] = 0;
    Audio_Samples_soundEffectBankIds[1] = 0;
    Audio_Samples_soundEffectBankIds[0] = 0;
    Audio_Seq_discVolume = reset.discVolume;
    Audio_Seq_discVolumeFadeLength = 0;
    Audio_Cutscene_streamState.controlFlags = 0;             // +0x5C
    Audio_Cutscene_streamState.volume = reset.streamVolume;  // +0x40
    Audio_Cutscene_streamState.volumeFadeTicksLeft = 0;      // +0x48
  }

  void ResetChannel(std::size_t index, const ChannelReset& reset) override {
    const bool effect = reset.kind == ChannelKind::SoundEffect;
    Audio_SeqChannel& channel = effect ? Audio_SoundEffects_channels[index]
                                       : Audio_Seq_musicChannels[index];
    channel.updateFlags = 0;                                // +0x34
    channel.voice.assignedVoice = reset.voice;              // +0xF4
    channel.type = static_cast<std::uint16_t>(reset.kind);  // +0x64
    channel.priority = 0;                                   // +0x58
    if (effect) {
      channel.effectVolume = reset.effectVolume;  // +0xDC
      channel.effectVolumeSlideLength = 0;        // +0x8C
      channel.effectPitchScaleSlideLength = 0;    // +0x86
      channel.effectPitchScale = 0;               // +0x40
      channel.keyOnVolumeSlideLength = 0;         // +0x8A
    }
    InitializePendingModulation(channel);
  }

  void RequestVoiceModeUpdate(std::uint32_t flags) override {
    Audio_Driver_controlLatches.updateFlags |= flags;
  }

  void SetReverbMode(int mode) override { Mixer::LiveSetReverbMode(mode); }

  void EnableReverb(bool enabled) override {
    Audio_Native_EnableReverb(enabled ? 1 : 0);
  }
};
}  // namespace

void ResetLivePlaybackState() {
  LivePlaybackReset host;
  ResetPlaybackState(host);
}
}  // namespace VS::Audio::Driver
