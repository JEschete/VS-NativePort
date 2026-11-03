// Hand-maintained (tools/port/owned.csv).
#include <cstddef>
#include <cstdint>

#include "Audio/PlaybackReset.hpp"
#include "Audio/SeqModulationCommands.hpp"
#include "Audio/VoiceOutput.hpp"

namespace VS::Audio::Seq {
namespace {
using Driver::ChannelKind;
using Driver::VoiceMode;

constexpr int kByteShift = 8;
constexpr std::size_t kOscillatorOperands = 3;
constexpr std::uint16_t kVibratoDepthMask = 0x7F00;
// A vibrato depth with bit 15 set scales the base pitch directly; otherwise
// by 15/256 of it.
constexpr std::uint16_t kAbsoluteVibrato = 0x8000;
constexpr std::uint32_t kRelativeVibratoScale = 15;
constexpr int kVibratoBaseShift = 7;
constexpr std::uint32_t kTremoloDepthMask = 0x7F;
constexpr int kAutoPanDepthShift = 7;
constexpr std::uint32_t kNoiseClockSteps = 0xC0;
constexpr std::uint32_t kNoiseClockMask = 0x3F;
// A toggle delay operand counts one tick more than it says; zero means 257.
constexpr std::uint16_t kDefaultToggleDelay = 0x101;
constexpr std::uint32_t kVoiceEffectFlags{
    kPitchSidechainActive | kVolumeSidechainActive | kAutoPanActive |
        kTremoloActive | kVibratoActive,
};

bool IsMusic(const ChannelState& channel) {
  return channel.kind == ChannelKind::Music;
}

// B4 and B5: the oscillator amplitude in pitch units. B4 passes the base
// pitch's low 16 bits; B5 passes all 32, so its multiply by 15 can wrap and
// its shift is arithmetic.
std::uint16_t VibratoBase(std::uint16_t depth, std::int32_t pitch) {
  const std::uint32_t amount = (depth & kVibratoDepthMask) >> kByteShift;
  std::int32_t scale = pitch;
  if ((depth & kAbsoluteVibrato) == 0) {
    scale = static_cast<std::int32_t>(static_cast<std::uint32_t>(pitch) *
                                      kRelativeVibratoScale) >>
            kByteShift;
  }
  return static_cast<std::uint16_t>(
      (amount * static_cast<std::uint32_t>(scale)) >> kVibratoBaseShift);
}

std::uint16_t TremoloDepth(std::uint8_t depth) {
  return static_cast<std::uint16_t>((depth & kTremoloDepthMask) << kByteShift);
}

std::uint16_t ToggleDelay(std::uint8_t delay) {
  return delay == 0 ? kDefaultToggleDelay
                    : static_cast<std::uint16_t>(delay + 1);
}
}  // namespace

// decomp: SLUS_010.40 SoundVM_B4_Vibrato (src/SLUS_010.40/25AC.c:5462)
CommandResult EnableVibrato(ChannelState& channel, Operands operands,
                            VoiceCommandHost& host) {
  if (operands.size() < kOscillatorOperands) return Truncated();
  const std::uint8_t shape = operands[2];
  if (!host.HasLfoWaveform(shape)) {
    return {CommandStatus::kWaveformMissing, kOscillatorOperands};
  }
  channel.update_flags |= kVibratoActive;
  if (IsMusic(channel)) {
    channel.vibrato_delay = operands[0];
  } else {
    channel.vibrato_delay = 0;
    if (operands[0] != 0) {
      channel.vibrato_depth =
          static_cast<std::uint16_t>(operands[0] << kByteShift);
    }
  }
  channel.vibrato_rate = TickCount(operands[1]);
  channel.vibrato_shape = shape;
  channel.vibrato_base = VibratoBase(
      channel.vibrato_depth, static_cast<std::uint16_t>(channel.pitch_base));
  channel.vibrato_wave = shape;
  channel.vibrato_delay_left =
      static_cast<std::uint16_t>(channel.vibrato_delay);
  channel.vibrato_rate_counter = 1;
  return Applied(kOscillatorOperands);
}

// decomp: SLUS_010.40 SoundVM_B5_VibratoDepth (src/SLUS_010.40/25AC.c:5507)
CommandResult SetVibratoDepth(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  channel.vibrato_depth = static_cast<std::uint16_t>(operands[0] << kByteShift);
  channel.vibrato_base = VibratoBase(channel.vibrato_depth, channel.pitch_base);
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_DD_VibratoDepthSlide
// (src/SLUS_010.40/25AC.c:5540)
CommandResult SlideVibratoDepth(ChannelState& channel, Operands operands) {
  if (operands.size() < 2) return Truncated();
  const std::uint16_t length = TickCount(operands[0]);
  channel.vibrato_depth_slide_step = static_cast<std::uint16_t>(
      SlideStep(operands[1] << kByteShift, channel.vibrato_depth, length));
  channel.vibrato_depth_slide_length = length;
  return Applied(2);
}

// decomp: SLUS_010.40 SoundVM_B6_DisableVibrato (src/SLUS_010.40/25AC.c:5560)
void DisableVibrato(ChannelState& channel) {
  channel.vibrato_pitch = 0;
  channel.update_flags &= ~kVibratoActive;
  channel.voice.dirty_flags |= kSampleRateDirty;
}

// decomp: SLUS_010.40 SoundVM_B8_Tremelo (src/SLUS_010.40/25AC.c:5568)
CommandResult EnableTremolo(ChannelState& channel, Operands operands,
                            VoiceCommandHost& host) {
  if (operands.size() < kOscillatorOperands) return Truncated();
  const std::uint8_t shape = operands[2];
  if (!host.HasLfoWaveform(shape)) {
    return {CommandStatus::kWaveformMissing, kOscillatorOperands};
  }
  channel.update_flags |= kTremoloActive;
  const std::uint8_t delay = operands[0];
  if (IsMusic(channel)) {
    channel.tremolo_delay = delay;
  } else {
    channel.tremolo_delay = 0;
    if (delay != 0) channel.tremolo_depth = TremoloDepth(delay);
  }
  channel.tremolo_rate = TickCount(operands[1]);
  channel.tremolo_shape = shape;
  channel.tremolo_wave = shape;
  channel.tremolo_delay_left =
      static_cast<std::uint16_t>(channel.tremolo_delay);
  channel.tremolo_rate_counter = 1;
  return Applied(kOscillatorOperands);
}

// decomp: SLUS_010.40 SoundVM_B9_TremeloDepth (src/SLUS_010.40/25AC.c:5598)
CommandResult SetTremoloDepth(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  channel.tremolo_depth = TremoloDepth(operands[0]);
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_DE_TremeloDepthSlide
// (src/SLUS_010.40/25AC.c:5604)
CommandResult SlideTremoloDepth(ChannelState& channel, Operands operands) {
  if (operands.size() < 2) return Truncated();
  const std::uint16_t length = TickCount(operands[0]);
  channel.tremolo_depth_slide_step = static_cast<std::int16_t>(
      SlideStep(TremoloDepth(operands[1]), channel.tremolo_depth, length));
  channel.tremolo_depth_slide_length = length;
  return Applied(2);
}

// decomp: SLUS_010.40 SoundVM_BA_DisableTremelo (src/SLUS_010.40/25AC.c:5623)
void DisableTremolo(ChannelState& channel) {
  channel.tremolo_volume = 0;
  channel.update_flags &= ~kTremoloActive;
  channel.voice.dirty_flags |= kVolumeDirty;
}

// decomp: SLUS_010.40 SoundVM_BC_AutoPan (src/SLUS_010.40/25AC.c:5631)
CommandResult EnableAutoPan(ChannelState& channel, Operands operands,
                            VoiceCommandHost& host) {
  if (operands.size() < 2) return Truncated();
  const std::uint8_t shape = operands[1];
  if (!host.HasLfoWaveform(shape)) return {CommandStatus::kWaveformMissing, 2};
  channel.update_flags |= kAutoPanActive;
  channel.auto_pan_rate = TickCount(operands[0]);
  channel.auto_pan_shape = shape;
  channel.auto_pan_wave = shape;
  channel.auto_pan_rate_counter = 1;
  return Applied(2);
}

// decomp: SLUS_010.40 SoundVM_BD_AutoPanDepth (src/SLUS_010.40/25AC.c:5649)
CommandResult SetAutoPanDepth(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  channel.auto_pan_depth =
      static_cast<std::uint16_t>(operands[0] << kAutoPanDepthShift);
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_DF_AutoPanDepthSlide
// (src/SLUS_010.40/25AC.c:5655)
CommandResult SlideAutoPanDepth(ChannelState& channel, Operands operands) {
  if (operands.size() < 2) return Truncated();
  const std::uint16_t length = TickCount(operands[0]);
  channel.auto_pan_depth_slide_step = static_cast<std::int16_t>(SlideStep(
      operands[1] << kAutoPanDepthShift, channel.auto_pan_depth, length));
  channel.auto_pan_depth_slide_length = length;
  return Applied(2);
}

// decomp: SLUS_010.40 SoundVM_BE_DisableAutoPan (src/SLUS_010.40/25AC.c:5676)
void DisableAutoPan(ChannelState& channel) {
  channel.auto_pan_offset = 0;
  channel.update_flags &= ~kAutoPanActive;
  channel.voice.dirty_flags |= kVolumeDirty;
}

// decomp: SLUS_010.40 SoundVM_C4_EnableNoiseVoices
// (src/SLUS_010.40/25AC.c:5684)
void EnableNoise(ChannelState& channel, VoiceCommandHost& host,
                 std::uint32_t channel_bits) {
  host.SetVoiceMode(channel.kind, VoiceMode::kNoise, channel_bits, true);
  host.RequestVoiceUpdate(kNoiseClockUpdate | kVoiceModeUpdate);
}

// decomp: SLUS_010.40 SoundVM_C5_DisableNoiseVoices
// (src/SLUS_010.40/25AC.c:5695)
void DisableNoise(ChannelState& channel, VoiceCommandHost& host,
                  std::uint32_t channel_bits) {
  host.SetVoiceMode(channel.kind, VoiceMode::kNoise, channel_bits, false);
  host.RequestVoiceUpdate(kNoiseClockUpdate | kVoiceModeUpdate);
  channel.noise_timer = 0;
}

// decomp: SLUS_010.40 SoundVM_C6_EnableFmVoices (src/SLUS_010.40/25AC.c:5707)
void EnableFrequencyModulation(ChannelState& channel, VoiceCommandHost& host,
                               std::uint32_t channel_bits) {
  if (IsMusic(channel) ||
      (channel.update_flags & kFrequencyModulationAllowed) != 0) {
    host.SetVoiceMode(channel.kind, VoiceMode::kFrequencyModulation,
                      channel_bits, true);
  }
  host.RequestVoiceUpdate(kVoiceModeUpdate);
}

// decomp: SLUS_010.40 SoundVM_C7_DisableFmVoices (src/SLUS_010.40/25AC.c:5718)
void DisableFrequencyModulation(ChannelState& channel, VoiceCommandHost& host,
                                std::uint32_t channel_bits) {
  host.SetVoiceMode(channel.kind, VoiceMode::kFrequencyModulation, channel_bits,
                    false);
  host.RequestVoiceUpdate(kVoiceModeUpdate);
  channel.fm_timer = 0;
}

// decomp: SLUS_010.40 SoundVM_C2_EnableReverbVoices
// (src/SLUS_010.40/25AC.c:5730)
void EnableReverb(ChannelState& channel, VoiceCommandHost& host,
                  std::uint32_t channel_bits) {
  host.SetVoiceMode(channel.kind, VoiceMode::kReverb, channel_bits, true);
  host.RequestVoiceUpdate(kVoiceModeUpdate);
}

// decomp: SLUS_010.40 SoundVM_C3_DisableReverbVoices
// (src/SLUS_010.40/25AC.c:5741)
void DisableReverb(ChannelState& channel, VoiceCommandHost& host,
                   std::uint32_t channel_bits) {
  host.SetVoiceMode(channel.kind, VoiceMode::kReverb, channel_bits, false);
  host.RequestVoiceUpdate(kVoiceModeUpdate);
}

// decomp: SLUS_010.40 SoundVM_AC_NoiseClockFrequency
// (src/SLUS_010.40/25AC.c:5776)
CommandResult SetNoiseClock(ChannelState& channel, Operands operands,
                            VoiceCommandHost& host) {
  if (operands.empty()) return Truncated();
  const std::uint8_t frequency = operands[0];
  std::uint32_t clock = frequency;
  if ((frequency & kNoiseClockSteps) != 0) {
    clock = (host.NoiseClock(channel.kind) + (frequency & kNoiseClockMask)) &
            kNoiseClockMask;
  }
  host.SetNoiseClock(channel.kind, static_cast<std::uint16_t>(clock));
  host.RequestVoiceUpdate(kNoiseClockUpdate);
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_CE_EnableNoiseAndDelayToggle
// (src/SLUS_010.40/25AC.c:6072)
CommandResult EnableTimedNoise(ChannelState& channel, Operands operands,
                               VoiceCommandHost& host,
                               std::uint32_t channel_bits) {
  if (operands.empty()) return Truncated();
  channel.noise_timer = ToggleDelay(operands[0]);
  EnableNoise(channel, host, channel_bits);
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_CF_ToggleNoiseOnDelay
// (src/SLUS_010.40/25AC.c:6089)
CommandResult SetNoiseToggleDelay(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  channel.noise_timer = ToggleDelay(operands[0]);
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_D2_EnableFmAndDelayToggle
// (src/SLUS_010.40/25AC.c:6100)
CommandResult EnableTimedFrequencyModulation(ChannelState& channel,
                                             Operands operands,
                                             VoiceCommandHost& host,
                                             std::uint32_t channel_bits) {
  if (operands.empty()) return Truncated();
  channel.fm_timer = ToggleDelay(operands[0]);
  EnableFrequencyModulation(channel, host, channel_bits);
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_D3_ToggleFmDelay (src/SLUS_010.40/25AC.c:6116)
CommandResult SetFrequencyModulationToggleDelay(ChannelState& channel,
                                                Operands operands) {
  if (operands.empty()) return Truncated();
  channel.fm_timer = ToggleDelay(operands[0]);
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_CB_DisableVoiceModes
// (src/SLUS_010.40/25AC.c:6127)
void ResetVoiceEffects(ChannelState& channel, VoiceCommandHost& host,
                       std::uint32_t channel_bits) {
  channel.update_flags &= ~kVoiceEffectFlags;
  DisableNoise(channel, host, channel_bits);
  DisableFrequencyModulation(channel, host, channel_bits);
  DisableReverb(channel, host, channel_bits);
  channel.play_modes &=
      static_cast<std::uint16_t>(~(kLegatoPlay | kFullLengthPlay));
}

// decomp: SLUS_010.40 SoundVM_D4_EnablePlaybackRateSidechain
// (src/SLUS_010.40/25AC.c:6138)
void EnablePitchSidechain(ChannelState& channel) {
  channel.update_flags |= kPitchSidechainActive;
}

// decomp: SLUS_010.40 SoundVM_D5_DisablePlaybackRateSidechain
// (src/SLUS_010.40/25AC.c:6144)
void DisablePitchSidechain(ChannelState& channel) {
  channel.update_flags &= ~kPitchSidechainActive;
}

// decomp: SLUS_010.40 SoundVM_D6_EnablePitchVolumeSidechain
// (src/SLUS_010.40/25AC.c:6150)
void EnableVolumeSidechain(ChannelState& channel) {
  channel.update_flags |= kVolumeSidechainActive;
}

// decomp: SLUS_010.40 SoundVM_D7_DisablePitchVolumeSidechain
// (src/SLUS_010.40/25AC.c:6156)
void DisableVolumeSidechain(ChannelState& channel) {
  channel.update_flags &= ~kVolumeSidechainActive;
}

// decomp: SLUS_010.40 SoundVM_E1_SetRandomPitchDepth
// (src/SLUS_010.40/25AC.c:6221)
CommandResult SetRandomPitchDepth(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  channel.random_pitch_depth = operands[0];
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_E2_ResetRandomPitchDepth
// (src/SLUS_010.40/25AC.c:6227)
void ResetRandomPitchDepth(ChannelState& channel) {
  channel.random_pitch_depth = 0;
}
}  // namespace VS::Audio::Seq
