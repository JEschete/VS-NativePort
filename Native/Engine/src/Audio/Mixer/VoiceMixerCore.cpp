// Hand-maintained (tools/port/owned.csv).
// The voice mixer cores (VoiceMixer.hpp). The original's arithmetic is 32-bit
// two's complement; the products and sums that can leave the int range wrap
// through std::uint32_t as the MIPS mult/addu results do.
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Audio/VoiceMixer.hpp"

namespace VS::Audio::Mixer {
namespace {
using Driver::ChannelKind;
using Seq::kAutoPanActive;
using Seq::kNoiseClockUpdate;
using Seq::kPitchSidechainActive;
using Seq::kSampleRateDirty;
using Seq::kTremoloActive;
using Seq::kVibratoActive;
using Seq::kVoiceModeUpdate;
using Seq::kVolumeDirty;
using Seq::kVolumeSidechainActive;

constexpr auto kNoiseTimerUpdate = kNoiseClockUpdate | kVoiceModeUpdate;
// The fixed-point shifts: a 16.16 value's integer part, a Q15 sample or
// gain, the 7-bit volume and balance range and a byte.
constexpr int kHalfBits = 16;
constexpr int kQ15Bits = 15;
constexpr int kLevelBits = 7;
constexpr int kByteBits = 8;
// The pan gain of the opposite side.
constexpr std::size_t kMirrorPan = kPanGainCount - 1;
constexpr std::int32_t kFullLevel = 127;
// The vibrato depth bit that scales by the full pitch instead of 15/256 of
// it, and the depth bits it scales by.
constexpr std::uint16_t kFullPitchVibrato = 0x8000;
constexpr std::uint16_t kVibratoDepthBits = 0x7F00;
// The balance and pan bits whose change dirties the voice volume.
constexpr std::int32_t kBalanceDirtyBits = 0x7F00;
constexpr std::int32_t kPanDirtyBits = 0xFF00;
constexpr std::uint32_t kVolumeDirtyBits = 0xFFE00000U;
constexpr std::uint32_t kPitchSlideDirtyBits = 0xFFFF0000U;
constexpr std::uint32_t kSampleRateBits = 0x3FFF;
constexpr std::int32_t kNoPitchScaleBelow = 0x80;
constexpr std::uint32_t kEffectVoicesBelowChannels = kFirstEffectVoiceBit - 1;

std::int32_t Mul(std::int32_t left, std::int32_t right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) *
                                   static_cast<std::uint32_t>(right));
}

std::int32_t Add(std::int32_t left, std::int32_t right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) +
                                   static_cast<std::uint32_t>(right));
}

std::int32_t ShiftLeft(std::int32_t value, int bits) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(value) << bits);
}

std::uint32_t VoiceBit(std::uint32_t voice) { return 1U << voice; }

// The waveform position the next LFO read lands on: a loop marker
// (0, 0, offset) moves the read `offset` samples from the marker. Empty when
// the read or the marker's target lies outside the waveform.
std::optional<std::size_t> SamplePosition(const LfoCursor& cursor) {
  const std::span<const std::int16_t> wave = cursor.wave;
  const std::size_t position = cursor.position;
  if (position >= wave.size()) return std::nullopt;
  if (wave[position] != 0) return position;
  if (position + 1 >= wave.size()) return std::nullopt;
  if (wave[position + 1] != 0) return position;
  if (position + 2 >= wave.size()) return std::nullopt;
  const auto target = static_cast<std::int64_t>(position) + wave[position + 2];
  if (target < 0 || target >= static_cast<std::int64_t>(wave.size())) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(target);
}

// The sample the cursor reads, leaving the cursor where it is (the slides).
std::optional<std::int32_t> PeekSample(const LfoCursor& cursor,
                                       RefusalSink& sink) {
  const std::optional<std::size_t> at = SamplePosition(cursor);
  if (!at) {
    sink.Refuse(MixRefusal::kWaveformOutside);
    return std::nullopt;
  }
  return cursor.wave[*at];
}

// The sample the cursor reads, moving the cursor past it (the modulation).
std::optional<std::int32_t> NextSample(LfoCursor& cursor, RefusalSink& sink) {
  const std::optional<std::size_t> at = SamplePosition(cursor);
  if (!at) {
    sink.Refuse(MixRefusal::kWaveformOutside);
    return std::nullopt;
  }
  cursor.position = *at + 1;
  return cursor.wave[*at];
}

void SetVibratoPitch(Seq::ChannelState& channel, std::int32_t sample) {
  const std::int32_t pitch = Mul(channel.vibrato_base, sample) >> kHalfBits;
  if (pitch == channel.vibrato_pitch) return;
  channel.vibrato_pitch = static_cast<std::int16_t>(pitch);
  channel.voice.dirty_flags |= kSampleRateDirty;
  if (pitch >= 0) channel.vibrato_pitch = static_cast<std::int16_t>(pitch * 2);
}

std::int32_t TremoloLevel(std::int32_t volume, std::uint16_t depth,
                          std::int32_t sample) {
  const std::int32_t level =
      ShiftLeft(Mul(volume, depth >> kByteBits), 9) >> kHalfBits;
  return Mul(level, sample) >> kQ15Bits;
}

void SetTremoloVolume(Seq::ChannelState& channel, std::int32_t level) {
  if (level == channel.tremolo_volume) return;
  channel.tremolo_volume = static_cast<std::int16_t>(level);
  channel.voice.dirty_flags |= kVolumeDirty;
}

void SetAutoPanOffset(Seq::ChannelState& channel, std::int32_t sample) {
  const std::int32_t offset =
      Mul(channel.auto_pan_depth >> kByteBits, sample) >> kQ15Bits;
  if (offset == channel.auto_pan_offset) return;
  channel.auto_pan_offset = static_cast<std::int16_t>(offset);
  channel.voice.dirty_flags |= kVolumeDirty;
}

// The channel's level before its tremolo and pan.
std::int32_t ChannelLevel(const Seq::ChannelState& channel) {
  return Mul(channel.volume >> kHalfBits,
             channel.volume_balance >> kByteBits) >>
         kLevelBits;
}

// The steps func_8001436C and func_800147CC share up to the volume mix:
// returns the level the mix starts from.
std::int32_t ModulateLfos(MixChannel& channel, bool honor_delays,
                          std::optional<std::uint16_t> previous_sample_rate,
                          RefusalSink& sink) {
  Seq::ChannelState& state = channel.state;
  std::int32_t level = ChannelLevel(state);
  const std::uint32_t flags = state.update_flags;
  if ((flags & kVibratoActive) != 0 &&
      (!honor_delays || state.vibrato_delay_left == 0) &&
      --state.vibrato_rate_counter == 0) {
    state.vibrato_rate_counter = state.vibrato_rate;
    if (const auto sample = NextSample(channel.vibrato, sink)) {
      SetVibratoPitch(state, *sample);
    }
  }
  if ((flags & kTremoloActive) != 0 &&
      (!honor_delays || state.tremolo_delay_left == 0) &&
      --state.tremolo_rate_counter == 0) {
    state.tremolo_rate_counter = static_cast<std::int16_t>(state.tremolo_rate);
    if (const auto sample = NextSample(channel.tremolo, sink)) {
      SetTremoloVolume(state,
                       TremoloLevel(level, state.tremolo_depth, *sample));
    }
  }
  if ((flags & kAutoPanActive) != 0 && --state.auto_pan_rate_counter == 0) {
    state.auto_pan_rate_counter = state.auto_pan_rate;
    if (const auto sample = NextSample(channel.auto_pan, sink)) {
      SetAutoPanOffset(state, *sample);
    }
  }
  if ((flags & kVolumeSidechainActive) != 0) {
    if (previous_sample_rate) {
      const auto source = static_cast<std::int16_t>(*previous_sample_rate * 2);
      level = Mul(source, state.volume_balance >> kByteBits) >> kLevelBits;
      state.voice.dirty_flags |= kVolumeDirty;
    } else {
      sink.Refuse(MixRefusal::kSidechainWithoutPrevious);
    }
  }
  return level;
}

void SetVoiceVolumes(MixChannel& channel, std::int32_t level,
                     std::size_t pan_index, const MixLevels& levels) {
  if ((levels.mix_behavior & kMonoMix) != 0) {
    channel.volume_right = static_cast<std::int16_t>(
        Mul(level, levels.pan_gains[kCenterPan]) >> kQ15Bits);
    channel.volume_left = channel.volume_right;
    return;
  }
  channel.volume_left = static_cast<std::int16_t>(
      Mul(level, levels.pan_gains[pan_index]) >> kQ15Bits);
  channel.volume_right = static_cast<std::int16_t>(
      Mul(level, levels.pan_gains[pan_index ^ kMirrorPan]) >> kQ15Bits);
}

std::int32_t ScalePitch(std::int32_t pitch, std::int32_t scale) {
  if (scale == 0) return pitch;
  if (scale < kNoPitchScaleBelow) {
    return Add(pitch, Mul(pitch, scale) >> kLevelBits);
  }
  return Mul(pitch, scale) >> kByteBits;
}

// The sample rate step both modulations end with; `scale` is the pitch
// scale the channel's kind selects (0 for none).
void ModulatePitch(MixChannel& channel,
                   std::optional<std::uint16_t> previous_sample_rate,
                   std::int32_t scale, RefusalSink& sink) {
  Seq::ChannelState& state = channel.state;
  const std::int32_t offset =
      Add(state.vibrato_pitch, channel.pitch_slide.slide >> kHalfBits);
  std::optional<std::int32_t> base;
  if ((state.update_flags & kPitchSidechainActive) != 0) {
    if (previous_sample_rate) {
      base = *previous_sample_rate;
      state.voice.dirty_flags |= kSampleRateDirty;
    } else {
      sink.Refuse(MixRefusal::kSidechainWithoutPrevious);
    }
  }
  if (!base && (state.voice.dirty_flags & kSampleRateDirty) != 0) {
    base = state.pitch_base;
  }
  if (!base) return;
  const std::int32_t pitch = ScalePitch(Add(*base, offset), scale);
  channel.sample_rate = static_cast<std::uint16_t>(
      static_cast<std::uint32_t>(Add(state.fine_pitch_delta, pitch)) &
      kSampleRateBits);
}

const Seq::MusicLayerState* ActiveConfig(const MixerState& state,
                                         RefusalSink& sink) {
  if (state.active == LayerId::kPrimary) return &state.primary.config;
  if (state.secondary) return &state.secondary->config;
  sink.Refuse(MixRefusal::kSecondaryLayerMissing);
  return nullptr;
}

Seq::MusicLayerState* ActiveConfig(MixerState& state, RefusalSink& sink) {
  if (state.active == LayerId::kPrimary) return &state.primary.config;
  if (state.secondary) return &state.secondary->config;
  sink.Refuse(MixRefusal::kSecondaryLayerMissing);
  return nullptr;
}

// The first voice the active layer's voice search reads, or empty when it
// lies outside the voices.
std::optional<std::uint32_t> SearchStart(const MixerState& state,
                                         bool from_first_voice,
                                         RefusalSink& sink) {
  if (from_first_voice) return 0;
  const Seq::MusicLayerState* active = ActiveConfig(state, sink);
  if (active == nullptr) return std::nullopt;
  const std::int32_t start = active->voice_search_start;
  if (start < 0 || static_cast<std::size_t>(start) >= kVoiceCount) {
    sink.Refuse(MixRefusal::kVoiceSearchOutside);
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(start);
}

std::optional<std::uint16_t> PreviousSampleRate(
    std::span<const MixChannel> channels, std::size_t index) {
  if (index == 0) return std::nullopt;
  return channels[index - 1].sample_rate;
}

// Hands a channel to its voice; a channel without one is refused (the
// original writes the registers past the last voice).
void Configure(MixerHost& host, ChannelRef ref, MixChannel& channel) {
  if (channel.assigned_voice >= kVoiceCount) {
    host.Refuse(MixRefusal::kChannelWithoutVoice);
    return;
  }
  host.ConfigureChannel(ref, channel);
}

std::uint32_t Unsigned(std::int32_t mask) {
  return static_cast<std::uint32_t>(mask);
}

std::int32_t Signed(std::uint32_t mask) {
  return static_cast<std::int32_t>(mask);
}

// The voices nothing but the music layers may key or release.
std::uint32_t MusicVoiceFilter(const MixerState& state) {
  return ~(state.scheduler.active_voices |
           state.scheduler.paused_effect_voices | state.stream_voices);
}

// Counts `timer` down; true when that ends it.
bool Expires(std::integral auto& timer) {
  if (timer == 0) return false;
  --timer;
  return timer == 0;
}

// Sound_UpdateSlidesAndDelays's balance, volume and pan slides.
void SlideLevels(Seq::ChannelState& fields, bool music) {
  if (music && fields.volume_balance_slide_length != 0) {
    --fields.volume_balance_slide_length;
    const std::int32_t next =
        fields.volume_balance + fields.volume_balance_slide_step;
    if ((next & kBalanceDirtyBits) !=
        (fields.volume_balance & kBalanceDirtyBits)) {
      fields.voice.dirty_flags |= kVolumeDirty;
    }
    fields.volume_balance = static_cast<std::uint16_t>(next);
  }
  if (fields.volume_slide_length != 0) {
    --fields.volume_slide_length;
    const std::int32_t next = Add(fields.volume, fields.volume_slide_step);
    if ((Unsigned(next) & kVolumeDirtyBits) !=
        (Unsigned(fields.volume) & kVolumeDirtyBits)) {
      fields.voice.dirty_flags |= kVolumeDirty;
    }
    fields.volume = next;
  }
  if (fields.pan_slide_length != 0) {
    --fields.pan_slide_length;
    const std::int32_t next = fields.pan + fields.pan_slide_step;
    if ((next & kPanDirtyBits) != (fields.pan & kPanDirtyBits)) {
      fields.voice.dirty_flags |= kVolumeDirty;
    }
    fields.pan = static_cast<std::uint16_t>(next);
  }
}

// Sound_UpdateSlidesAndDelays's LFO delays and noise and FM timers.
void TickTimers(Seq::ChannelState& fields, SlideState& state,
                std::uint32_t channel_bit, bool music) {
  if (fields.vibrato_delay_left != 0) --fields.vibrato_delay_left;
  if (fields.tremolo_delay_left != 0) --fields.tremolo_delay_left;
  if (Expires(fields.noise_timer)) {
    if (music) {
      state.active.noise_channels ^= channel_bit;
    } else {
      state.scheduler.noise_voices ^= channel_bit;
    }
    state.driver_update_flags |= kNoiseTimerUpdate;
  }
  if (Expires(fields.fm_timer)) {
    if (music) {
      state.active.fm_channels ^= channel_bit;
    } else {
      state.scheduler.fm_voices ^= channel_bit;
    }
    state.driver_update_flags |= kVoiceModeUpdate;
  }
}

// Sound_UpdateSlidesAndDelays's LFO depth slides.
void SlideLfoDepths(MixChannel& channel, RefusalSink& sink) {
  Seq::ChannelState& fields = channel.state;
  if (fields.vibrato_depth_slide_length != 0) {
    --fields.vibrato_depth_slide_length;
    const auto depth = static_cast<std::uint16_t>(
        fields.vibrato_depth + fields.vibrato_depth_slide_step);
    fields.vibrato_depth = depth;
    const std::uint32_t scale = (depth & kVibratoDepthBits) >> 8;
    const auto pitch_base = static_cast<std::uint32_t>(fields.pitch_base);
    const std::uint32_t amplitude =
        (depth & kFullPitchVibrato) != 0
            ? (scale * pitch_base) >> 7
            : (scale * ((pitch_base * 15) >> 8)) >> 7;
    fields.vibrato_base = static_cast<std::uint16_t>(amplitude);
    if (fields.vibrato_delay_left == 0 && fields.vibrato_rate_counter != 1) {
      if (const auto sample = PeekSample(channel.vibrato, sink)) {
        SetVibratoPitch(fields, *sample);
      }
    }
  }
  if (fields.tremolo_depth_slide_length != 0) {
    --fields.tremolo_depth_slide_length;
    fields.tremolo_depth = static_cast<std::uint16_t>(
        fields.tremolo_depth +
        static_cast<std::uint16_t>(fields.tremolo_depth_slide_step));
    if (fields.tremolo_delay_left == 0 &&
        static_cast<std::uint16_t>(fields.tremolo_rate_counter) != 1) {
      if (const auto sample = PeekSample(channel.tremolo, sink)) {
        SetTremoloVolume(fields, TremoloLevel(ChannelLevel(fields),
                                              fields.tremolo_depth, *sample));
      }
    }
  }
  if (fields.auto_pan_depth_slide_length != 0) {
    --fields.auto_pan_depth_slide_length;
    fields.auto_pan_depth = static_cast<std::uint16_t>(
        fields.auto_pan_depth + fields.auto_pan_depth_slide_step);
    if (fields.auto_pan_rate_counter != 1) {
      if (const auto sample = PeekSample(channel.auto_pan, sink)) {
        SetAutoPanOffset(fields, *sample);
      }
    }
  }
}

// Sound_UpdateSlidesAndDelays's pitch slide.
void SlidePitch(MixChannel& channel) {
  Seq::PitchSlideState& slide = channel.pitch_slide;
  if (slide.ticks_left != 0) {
    --slide.ticks_left;
    const std::int32_t next = Add(slide.slide, slide.step);
    if ((Unsigned(next) & kPitchSlideDirtyBits) !=
        (Unsigned(slide.slide) & kPitchSlideDirtyBits)) {
      channel.state.voice.dirty_flags |= kSampleRateDirty;
    }
    slide.slide = next;
  }
}
}  // namespace

// decomp: SLUS_010.40 Sound_UpdateSlidesAndDelays
// (src/SLUS_010.40/25AC.c:1400)
void UpdateChannelSlidesAndDelays(MixChannel& channel, SlideState& state,
                                  std::uint32_t channel_bit, ChannelKind kind,
                                  RefusalSink& sink) {
  const bool music = kind == ChannelKind::Music;
  SlideLevels(channel.state, music);
  TickTimers(channel.state, state, channel_bit, music);
  SlideLfoDepths(channel, sink);
  SlidePitch(channel);
}

// decomp: SLUS_010.40 func_8001436C (src/SLUS_010.40/25AC.c:1566)
void UpdateMusicChannelModulation(
    MixChannel& channel, std::optional<std::uint16_t> previous_sample_rate,
    const MixLevels& levels, RefusalSink& sink) {
  Seq::ChannelState& state = channel.state;
  std::int32_t level = ModulateLfos(channel, true, previous_sample_rate, sink);
  if ((state.voice.dirty_flags & kVolumeDirty) != 0) {
    level = Add(level, state.tremolo_volume);
    const auto master = static_cast<std::int32_t>(
        (Unsigned(levels.master_volume) >> 16) & 0x7F);
    level = Mul(level, master) >> kLevelBits;
    const auto pan_index = static_cast<std::size_t>(
        ((state.pan >> 8) + state.auto_pan_offset) & 0xFF);
    SetVoiceVolumes(channel, level, pan_index, levels);
  }
  const auto scale = static_cast<std::int32_t>(
      (Unsigned(levels.music_pitch_scale) & 0xFF0000) >> 16);
  ModulatePitch(channel, previous_sample_rate, scale, sink);
}

// decomp: SLUS_010.40 func_800147CC (src/SLUS_010.40/25AC.c:1678)
void UpdateSoundEffectModulation(
    MixChannel& channel, std::optional<std::uint16_t> previous_sample_rate,
    const MixLevels& levels, RefusalSink& sink) {
  Seq::ChannelState& state = channel.state;
  std::int32_t level = ModulateLfos(channel, false, previous_sample_rate, sink);
  const bool fixed_mix = (channel.effect_tags & kFixedMixEffect) != 0;
  if ((state.voice.dirty_flags & kVolumeDirty) != 0) {
    level = Add(level, state.tremolo_volume);
    std::size_t pan_index = kCenterPan;
    if (!fixed_mix) {
      level = Mul(level, state.effect_volume >> kByteBits) >> kLevelBits;
      const std::int32_t pan = (state.pan + channel.effect_pan) >> 8;
      pan_index =
          static_cast<std::size_t>((pan + state.auto_pan_offset +
                                    static_cast<std::int32_t>(kCenterPan)) &
                                   static_cast<std::int32_t>(kMirrorPan));
    }
    SetVoiceVolumes(channel, level, pan_index, levels);
  }
  const std::int32_t scale =
      fixed_mix ? 0
                : static_cast<std::int32_t>(
                      (Unsigned(channel.effect_pitch_scale) & 0xFF00) >> 8);
  ModulatePitch(channel, previous_sample_rate, scale, sink);
}

// decomp: SLUS_010.40 Sound_StealQuietestVoice (src/SLUS_010.40/25AC.c:1799)
std::uint32_t StealQuietestVoice(MixerState& state, bool full_scan,
                                 RefusalSink& sink) {
  const std::optional<std::uint32_t> start =
      SearchStart(state, full_scan, sink);
  if (!start) return kNoVoice;
  std::int16_t quietest = kReservedEnvelope;
  std::uint32_t voice = kNoVoice;
  for (std::uint32_t index = *start; index < kVoiceCount; ++index) {
    if (state.envelopes[index] < quietest) {
      quietest = state.envelopes[index];
      voice = index;
    }
  }
  if (quietest == kReservedEnvelope) return kNoVoice;
  UnassignVoice(state, state.primary.channels, voice);
  return voice;
}

// decomp: SLUS_010.40 Sound_FindFreeVoice (src/SLUS_010.40/25AC.c:1832)
std::uint32_t FindFreeVoice(const MixerState& state, bool from_first_voice,
                            RefusalSink& sink) {
  const std::optional<std::uint32_t> start =
      SearchStart(state, from_first_voice, sink);
  if (!start) return kNoVoice;
  for (std::uint32_t index = *start; index < kVoiceCount; ++index) {
    if (state.envelopes[index] == 0) return index;
  }
  return kNoVoice;
}

// decomp: SLUS_010.40 func_80014D70 (src/SLUS_010.40/25AC.c:1863)
void ApplyMusicChannelVoices(MixerState& state, PanGains pan_gains,
                             LayerId layer, std::uint32_t channels,
                             std::uint32_t fixed_voices,
                             std::uint32_t& key_on_voices, MixerHost& host) {
  Seq::MusicLayerState* active = ActiveConfig(state, host);
  if (active == nullptr) return;
  MusicLayer* target = layer == LayerId::kPrimary
                           ? &state.primary
                           : (state.secondary ? &*state.secondary : nullptr);
  if (target == nullptr) {
    host.Refuse(MixRefusal::kSecondaryLayerMissing);
    return;
  }
  const ChannelRef ref_base{layer == LayerId::kPrimary
                                ? Seq::ChannelGroup::kMusic
                                : Seq::ChannelGroup::kSecondaryMusic,
                            0};
  const std::uint32_t to_key_on = channels & Unsigned(active->pending_key_on);
  const MixLevels levels{active->master_volume, state.music_pitch_scale,
                         state.mix_behavior, pan_gains};
  for (std::size_t index = 0; channels != 0 && index < kMusicChannelCount;
       ++index) {
    const std::uint32_t bit = 1U << index;
    if ((channels & bit) == 0) continue;
    channels &= ~bit;
    MixChannel& channel = target->channels[index];
    UpdateMusicChannelModulation(
        channel, PreviousSampleRate(target->channels, index), levels, host);
    if (channel.state.voice.dirty_flags == 0) continue;
    if ((Unsigned(state.muted_channels) & bit) != 0) {
      channel.volume_right = 0;
      channel.volume_left = 0;
    }
    const ChannelRef ref{ref_base.group, index};
    if ((to_key_on & bit) == 0) {
      if (channel.assigned_voice < kVoiceCount) {
        host.ConfigureChannel(ref, channel);
      }
      continue;
    }
    if ((fixed_voices & bit) != 0) {
      key_on_voices |= bit;
      channel.assigned_voice = static_cast<std::uint32_t>(index);
    } else {
      const bool keyed = (active->keyed_voices & bit) != 0;
      std::uint32_t voice = FindFreeVoice(state, keyed, host);
      if (voice == kNoVoice) {
        active->status_flags |= kVoiceStolen;
        voice = StealQuietestVoice(state, keyed, host);
      }
      channel.assigned_voice = voice;
      if (voice == kNoVoice) {
        active->status_flags |= kVoiceUnavailable;
      } else {
        key_on_voices |= VoiceBit(voice);
        state.envelopes[voice] = kReservedEnvelope;
      }
    }
    if (channel.assigned_voice < kVoiceCount) {
      host.ConfigureChannel(ref, channel);
      host.SetVoiceOwner(channel.assigned_voice, state.active);
      state.driver_update_flags |= kVoiceModeUpdate;
    }
  }
}

// decomp: SLUS_010.40 UnassignVoicesFromChannels
// (src/SLUS_010.40/25AC.c:1926)
void UnassignVoice(MixerState& state, MusicChannels& channels,
                   std::uint32_t voice) {
  for (MixChannel& channel : channels) {
    if (channel.assigned_voice == voice) channel.assigned_voice = kNoVoice;
  }
  if (!state.secondary) return;
  for (MixChannel& channel : state.secondary->channels) {
    if (channel.assigned_voice == voice) channel.assigned_voice = kNoVoice;
  }
}

// decomp: SLUS_010.40 func_8001503C (src/SLUS_010.40/25AC.c:1956)
bool IsVoiceAssignedToChannels(const MusicChannels& channels,
                               std::uint32_t channel_mask,
                               std::uint32_t voice) {
  for (std::size_t index = 0; index < kMusicChannelCount; ++index) {
    if (channels[index].assigned_voice == voice &&
        (channel_mask & (1U << index)) != 0) {
      return true;
    }
  }
  return false;
}

// decomp: SLUS_010.40 func_80015080 (src/SLUS_010.40/25AC.c:1971)
void RefreshVoiceAvailability(MixerState& state, std::uint32_t reserved_voices,
                              MixerHost& host) {
  const Seq::MusicLayerState* active = ActiveConfig(state, host);
  if (active == nullptr) return;
  std::uint32_t reserved =
      Unsigned(active->active_channels & active->allocated_voices) |
      reserved_voices;
  if (state.secondary) {
    const Seq::MusicLayerState& secondary = state.secondary->config;
    reserved |=
        Unsigned(secondary.active_channels & secondary.allocated_voices);
  }
  for (std::uint32_t voice = 0; voice < kVoiceCount; ++voice) {
    std::int16_t& envelope = state.envelopes[voice];
    if ((reserved & VoiceBit(voice)) != 0) {
      envelope = kReservedEnvelope;
      continue;
    }
    envelope = host.Envelope(voice);
    if (envelope == 0) {
      UnassignVoice(state, state.primary.channels, voice);
      continue;
    }
    if (active->keyed_voices == 0) continue;
    if (IsVoiceAssignedToChannels(state.primary.channels, active->keyed_voices,
                                  voice)) {
      envelope = kReservedEnvelope;
    }
    if (state.secondary && IsVoiceAssignedToChannels(
                               state.secondary->channels,
                               state.secondary->config.keyed_voices, voice)) {
      envelope = kReservedEnvelope;
    }
  }
}

// decomp: SLUS_010.40 Sound_ApplyMasterFadeToChannelVolume
// (src/SLUS_010.40/25AC.c:2006)
void ApplySavedMusicMasterFade(Seq::MusicLayerState& layer, MasterFade& fade) {
  fade.saved_value = layer.master_volume;
  // The division by 127 truncates toward zero (the original's
  // multiply-by-reciprocal sequence).
  const std::int32_t faded =
      Mul(layer.master_volume >> kHalfBits, fade.value >> kByteBits);
  layer.master_volume = ShiftLeft(faded / kFullLevel, kByteBits);
}

// decomp: SLUS_010.40 Sound_RestoreChannelVolumeFromMasterFade
// (src/SLUS_010.40/25AC.c:2020)
void RestoreSavedMusicVolume(Seq::MusicLayerState& layer,
                             const MasterFade& fade) {
  layer.master_volume = fade.saved_value;
}

namespace {
// The commit's music step for one layer: applies its keyed channels first,
// then (when `apply_rest`) the rest.
struct LayerCommit {
  std::uint32_t channels = 0;
  std::uint32_t voices = 0;
};

LayerCommit PlanLayerCommit(const Seq::MusicLayerState& config,
                            std::uint32_t reserved) {
  const std::uint32_t allocated = Unsigned(config.allocated_voices);
  const std::uint32_t channels =
      (Unsigned(config.active_channels) & config.active_notes) &
      ~(allocated & reserved);
  return {channels, (channels & allocated) & ~reserved};
}

// Applies the plan's keyed channels and drops them from the plan.
void ApplyKeyedChannels(MixerState& state, PanGains pan_gains, LayerId layer,
                        Seq::MusicLayerState& config, LayerCommit& plan,
                        std::uint32_t& key_on, MixerHost& host) {
  if ((plan.channels & config.keyed_voices) == 0) return;
  ApplyMusicChannelVoices(state, pan_gains, layer,
                          plan.channels & config.keyed_voices, plan.voices,
                          key_on, host);
  config.pending_key_on &= ~Signed(config.keyed_voices);
  plan.channels &= ~config.keyed_voices;
}

void CommitEffectChannels(MixerState& state, PanGains pan_gains,
                          std::uint32_t& key_on, MixerHost& host) {
  std::uint32_t voices =
      state.scheduler.active_voices & state.scheduler.keyed_voices;
  if (voices == 0) return;
  key_on |= state.scheduler.pending_key_on;
  if ((voices & kEffectVoicesBelowChannels) != 0) {
    host.Refuse(MixRefusal::kEffectVoiceOutsideChannels);
    voices &= ~kEffectVoicesBelowChannels;
  }
  const MixLevels levels{0, 0, state.mix_behavior, pan_gains};
  for (std::size_t index = 0; voices != 0 && index < kEffectChannelCount;
       ++index) {
    const std::uint32_t bit = kFirstEffectVoiceBit << index;
    if ((voices & bit) == 0) continue;
    voices &= ~bit;
    MixChannel& channel = state.effects[index];
    UpdateSoundEffectModulation(
        channel, PreviousSampleRate(state.effects, index), levels, host);
    if (channel.state.voice.dirty_flags != 0) {
      Configure(host, {Seq::ChannelGroup::kSoundEffects, index}, channel);
    }
  }
  state.scheduler.pending_key_on = 0;
}

void CommitDriverUpdates(MixerState& state, MixerHost& host) {
  const Seq::MusicLayerState* active = ActiveConfig(state, host);
  if (active == nullptr) return;
  const std::uint32_t updates = state.driver_update_flags;
  if ((updates & kReverbDepthUpdate) != 0) {
    const auto depth = static_cast<std::int16_t>(active->reverb_depth >> 12);
    host.SetReverbDepth(depth, depth);
    state.driver_update_flags &= ~kReverbDepthUpdate;
  }
  if ((updates & Seq::kNoiseClockUpdate) != 0) {
    host.SetNoiseClock(state.scheduler.active_voices != 0
                           ? state.scheduler.noise_clock
                           : active->noise_clock);
    state.driver_update_flags &= ~Seq::kNoiseClockUpdate;
  }
  if ((updates & kVoiceModeUpdate) != 0) {
    const Seq::MusicLayerState* secondary =
        state.secondary ? &state.secondary->config : nullptr;
    state.voice_modes.noise = BuildVoiceModeMask(
        state,
        {secondary != nullptr ? secondary->noise_channels : 0,
         active->noise_channels, state.scheduler.noise_voices},
        host);
    state.voice_modes.reverb = BuildVoiceModeMask(
        state,
        {secondary != nullptr ? secondary->reverb_channels : 0,
         active->reverb_channels, state.scheduler.reverb_voices},
        host);
    state.voice_modes.fm =
        BuildVoiceModeMask(state,
                           {secondary != nullptr ? secondary->fm_channels : 0,
                            active->fm_channels, state.scheduler.fm_voices},
                           host);
    host.SetReverbVoices(state.voice_modes.reverb);
    host.SetNoiseVoices(state.voice_modes.noise);
    host.SetFrequencyModulationVoices(state.voice_modes.fm);
    state.driver_update_flags &= ~kVoiceModeUpdate;
  }
}
}  // namespace

// decomp: SLUS_010.40 func_80015220 (src/SLUS_010.40/25AC.c:2027)
void CommitPlaybackVoices(MixerState& state, PanGains pan_gains,
                          MixerHost& host) {
  const Seq::MusicLayerState* entry_active = ActiveConfig(state, host);
  if (entry_active == nullptr) return;
  std::uint32_t key_on = 0;
  const std::uint32_t reserved = state.scheduler.active_voices |
                                 state.scheduler.paused_effect_voices |
                                 state.stream_voices;
  const bool active_pending =
      (entry_active->active_channels & entry_active->pending_key_on) != 0;
  const bool secondary_pending =
      state.secondary && (state.secondary->config.active_channels &
                          state.secondary->config.pending_key_on) != 0;
  if (active_pending || secondary_pending) {
    RefreshVoiceAvailability(state, reserved, host);
  }
  if (state.secondary) {
    Seq::MusicLayerState& secondary = state.secondary->config;
    if ((state.mix_behavior & Seq::kSavedMusicFadeActive) != 0) {
      ApplySavedMusicMasterFade(secondary, state.master_fade);
    }
    state.active = LayerId::kSecondary;
    LayerCommit saved = PlanLayerCommit(secondary, reserved);
    ApplyKeyedChannels(state, pan_gains, LayerId::kSecondary, secondary, saved,
                       key_on, host);
    state.active = LayerId::kPrimary;
    Seq::MusicLayerState& primary = state.primary.config;
    LayerCommit plan = PlanLayerCommit(primary, saved.voices | reserved);
    ApplyKeyedChannels(state, pan_gains, LayerId::kPrimary, primary, plan,
                       key_on, host);
    if (saved.channels != 0) {
      state.active = LayerId::kSecondary;
      saved.voices &= ~plan.voices;
      ApplyMusicChannelVoices(state, pan_gains, LayerId::kSecondary,
                              saved.channels, saved.voices, key_on, host);
      secondary.pending_key_on = 0;
      state.active = LayerId::kPrimary;
    }
    if (plan.channels != 0) {
      ApplyMusicChannelVoices(state, pan_gains, LayerId::kPrimary,
                              plan.channels, plan.voices, key_on, host);
      primary.pending_key_on = 0;
    }
    if ((state.mix_behavior & Seq::kSavedMusicFadeActive) != 0) {
      RestoreSavedMusicVolume(secondary, state.master_fade);
    }
  } else {
    Seq::MusicLayerState& primary = state.primary.config;
    LayerCommit plan = PlanLayerCommit(primary, reserved);
    ApplyKeyedChannels(state, pan_gains, LayerId::kPrimary, primary, plan,
                       key_on, host);
    if (plan.channels != 0) {
      ApplyMusicChannelVoices(state, pan_gains, LayerId::kPrimary,
                              plan.channels, plan.voices, key_on, host);
      primary.pending_key_on = 0;
    }
  }
  CommitEffectChannels(state, pan_gains, key_on, host);
  CommitDriverUpdates(state, host);
  if (key_on != 0) host.KeyOn(key_on);
}

// decomp: SLUS_010.40 ChannelMaskToVoiceMaskFiltered
// (src/SLUS_010.40/25AC.c:2168)
void AccumulateFilteredVoiceMask(const MusicChannels& channels,
                                 std::uint32_t& voice_mask,
                                 std::uint32_t channel_mask,
                                 std::uint32_t voice_filter) {
  for (std::size_t index = 0; channel_mask != 0 && index < kMusicChannelCount;
       ++index) {
    const std::uint32_t bit = 1U << index;
    if ((channel_mask & bit) != 0 &&
        channels[index].assigned_voice < kVoiceCount) {
      voice_mask |= VoiceBit(channels[index].assigned_voice);
    }
    channel_mask &= ~bit;
  }
  voice_mask &= voice_filter;
}

// decomp: SLUS_010.40 Sound_ProcessKeyOffRequests
// (src/SLUS_010.40/25AC.c:2187)
void ReleasePendingVoices(MixerState& state, MixerHost& host) {
  Seq::MusicLayerState* active = ActiveConfig(state, host);
  if (active == nullptr) return;
  const std::uint32_t filter = MusicVoiceFilter(state);
  std::uint32_t voices = 0;
  std::uint32_t secondary_off = 0;
  if (state.secondary) {
    Seq::MusicLayerState& secondary = state.secondary->config;
    secondary_off = Unsigned(secondary.pending_key_off);
    const std::uint32_t keyed = secondary_off & secondary.keyed_voices;
    if (keyed != 0) {
      AccumulateFilteredVoiceMask(state.secondary->channels, voices, keyed,
                                  filter);
      secondary.pending_key_off &= ~Signed(secondary.keyed_voices);
      secondary_off &= ~secondary.keyed_voices;
    }
  }
  std::uint32_t active_off = Unsigned(active->pending_key_off);
  const std::uint32_t active_keyed = active_off & active->keyed_voices;
  if (active_keyed != 0) {
    AccumulateFilteredVoiceMask(state.primary.channels, voices, active_keyed,
                                filter);
    active->pending_key_off &= ~Signed(active->keyed_voices);
    active_off &= ~active->keyed_voices;
  }
  if (state.secondary && secondary_off != 0) {
    AccumulateFilteredVoiceMask(state.secondary->channels, voices,
                                secondary_off, filter);
    state.secondary->config.pending_key_off = 0;
  }
  if (active_off != 0) {
    AccumulateFilteredVoiceMask(state.primary.channels, voices, active_off,
                                filter);
    active->pending_key_off = 0;
  }
  voices |= state.scheduler.pending_key_off;
  state.scheduler.pending_key_off = 0;
  if (voices != 0) host.KeyOff(voices);
}

// decomp: SLUS_010.40 func_80015970 (src/SLUS_010.40/25AC.c:2244)
std::uint32_t BuildVoiceModeMask(MixerState& state,
                                 const VoiceModeRequest& request,
                                 RefusalSink& sink) {
  state.driver_update_flags |= kVoiceModeUpdate;
  const Seq::MusicLayerState* active = ActiveConfig(state, sink);
  if (active == nullptr) return request.extra_voices;
  const std::uint32_t filter = MusicVoiceFilter(state);
  std::uint32_t voices = 0;
  std::uint32_t secondary_mask = 0;
  if (state.secondary) {
    const Seq::MusicLayerState& secondary = state.secondary->config;
    secondary_mask =
        Unsigned(secondary.active_channels) & request.secondary_channels;
    const std::uint32_t keyed = secondary_mask & secondary.keyed_voices;
    if (keyed != 0) {
      AccumulateFilteredVoiceMask(state.secondary->channels, voices, keyed,
                                  filter);
      secondary_mask &= ~secondary.keyed_voices;
    }
  }
  std::uint32_t active_mask =
      Unsigned(active->active_channels) & request.active_channels;
  const std::uint32_t active_keyed = active_mask & active->keyed_voices;
  if (active_keyed != 0) {
    AccumulateFilteredVoiceMask(state.primary.channels, voices, active_keyed,
                                filter);
    active_mask &= ~active->keyed_voices;
  }
  if (state.secondary && secondary_mask != 0) {
    AccumulateFilteredVoiceMask(state.secondary->channels, voices,
                                secondary_mask, filter);
  }
  if (active_mask != 0) {
    AccumulateFilteredVoiceMask(state.primary.channels, voices, active_mask,
                                filter);
  }
  return voices | request.extra_voices;
}
}  // namespace VS::Audio::Mixer
