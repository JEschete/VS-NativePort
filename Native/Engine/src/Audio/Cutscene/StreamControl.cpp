// Hand-maintained (tools/port/owned.csv).
#include "Audio/StreamControl.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace VS::Audio::Cutscene {
namespace {
constexpr std::size_t kCenterPan = 128;
constexpr std::uint32_t kPanByteShift = 8;
constexpr std::uint32_t kPanByteMask = 0xFF;
constexpr unsigned kPanGainShift = 16;
constexpr std::uint32_t kVoicePairMask = 3;
constexpr std::uint32_t kStereoFormat = 1;
constexpr std::uint32_t kRepeatFormat = 2;
constexpr float kPitchUnity = 4096;
constexpr float kOriginalRate = 44100;

std::uint32_t PanGain(std::uint32_t volume,
                      std::span<const std::int16_t, kPanGainCount> pan_gains,
                      std::size_t pan) {
  // The original's unsigned multiply of the signed table entry.
  return (volume * static_cast<std::uint32_t>(pan_gains[pan])) >> kPanGainShift;
}

bool ValidPan(std::uint32_t pan) {
  return pan <= kMaximumPackedPan && (pan & kPanByteMask) == 0;
}

void PausePair(StreamHost& host, const StreamVoices& voices, bool paused) {
  host.PauseVoice(voices.first, paused);
  host.PauseVoice(voices.first + 1, paused);
}

bool SetPairPaused(StreamHost& host, bool paused) {
  const auto voices = host.Voices();
  if (voices.mask == 0) return false;
  PausePair(host, voices, paused);
  return true;
}
}  // namespace

StreamGains MixStreamGains(
    const StreamMix& mix,
    std::span<const std::int16_t, kPanGainCount> pan_gains) {
  if (mix.mono_output) {
    const auto gain = PanGain(mix.volume, pan_gains, kCenterPan);
    return {gain, gain, gain, gain};
  }
  if (mix.stereo_source) return {mix.volume / 2, 0, 0, mix.volume / 2};
  const std::size_t pan = (mix.pan >> kPanByteShift) & kPanByteMask;
  const auto left = PanGain(mix.volume, pan_gains, pan);
  const auto right = PanGain(mix.volume, pan_gains, pan ^ kPanByteMask);
  return {left, right, left, right};
}

bool ApplyStreamGains(StreamHost& host) {
  const auto voices = host.Voices();
  if (voices.mask == 0) return false;
  const auto gains = MixStreamGains(host.Mix(), host.PanGains());
  if (host.AppliedGains() == gains) return false;
  const auto [first_left, first_right, second_left, second_right] = gains;
  host.SetVoiceGains(voices.first, first_left, first_right);
  host.SetVoiceGains(voices.first + 1, second_left, second_right);
  host.SetAppliedGains(gains);
  return true;
}

// decomp: Sound_Cmd_9F_unk (src/SLUS_010.40/25AC.c:3786). Pausing the voices
// replaces zeroing their sample rates.
bool PauseStream(StreamHost& host) { return SetPairPaused(host, true); }

// decomp: Sound_Cmd_9E_unk (src/SLUS_010.40/25AC.c:3794).
bool ResumeStream(StreamHost& host) { return SetPairPaused(host, false); }

// decomp: IRQCallbackProc (src/SLUS_010.40/25AC.c:6238). The voices are
// unpaused first so a paused stream does not hold its pair.
std::uint32_t StopStreamVoices(StreamHost& host) {
  const auto voices = host.Voices();
  if (voices.mask == 0) return 0;
  PausePair(host, voices, false);
  host.KeyOffVoices(voices.mask);
  host.ReleaseVoices(voices.mask);
  host.SetAppliedGains(std::nullopt);
  return voices.mask;
}

// decomp: func_8001D698 (src/SLUS_010.40/25AC.c:6511).
std::uint32_t StopStream(StreamHost& host) { return StopStreamVoices(host); }

// decomp: Sound_Cutscene_StartStream (src/SLUS_010.40/25AC.c:6294). The bound
// stream's decoded channels replace the SPU transfer the original starts.
StreamStatus StartStream(StreamHost& host, StreamSourceId source,
                         std::uint32_t pan, bool reverb) {
  const auto format = host.BoundFormat(source);
  if (!format) return StreamStatus::kUnbound;
  if (!ValidPan(pan)) return StreamStatus::kInvalidPan;
  if (format->sample_rate == 0 ||
      (format->channels != 1 && format->channels != kStreamChannelCount)) {
    return StreamStatus::kInvalidFormat;
  }
  const auto first = host.FindFreeVoicePair();
  if (!first || *first < 0 || *first > kLastPairVoice) {
    return StreamStatus::kNoVoicePair;
  }
  const StreamVoices voices{*first, kVoicePairMask << *first};
  const std::uint32_t format_flags =
      (format->channels == kStreamChannelCount ? kStereoFormat : 0) |
      (format->repeat ? kRepeatFormat : 0);
  host.BeginStream({voices, pan, format_flags, format->source_pitch, reverb});
  const auto gains = MixStreamGains(host.Mix(), host.PanGains());
  host.SetAppliedGains(gains);
  const float rate = static_cast<float>(format->source_pitch) * kOriginalRate /
                     (static_cast<float>(format->sample_rate) * kPitchUnity);
  for (std::size_t channel = 0; channel < kStreamChannelCount; ++channel) {
    if (!host.StartVoice(
            channel, voices.first + static_cast<std::int32_t>(channel), rate,
            gains[channel * 2], gains[(channel * 2) + 1], reverb)) {
      StopStreamVoices(host);
      return StreamStatus::kVoiceFailed;
    }
  }
  return StreamStatus::kApplied;
}

// decomp: func_8001D644 (src/SLUS_010.40/25AC.c:6503). An invalid pan is
// refused before the playing stream stops.
StreamStatus ReplaceStream(StreamHost& host, StreamSourceId source,
                           std::uint32_t pan, bool reverb) {
  if (!ValidPan(pan)) return StreamStatus::kInvalidPan;
  StopStreamVoices(host);
  return StartStream(host, source, pan, reverb);
}

// decomp: Sound_Cmd_E4_SetCutsceneVolume (src/SLUS_010.40/25AC.c:6513).
StreamStatus SetStreamVolume(StreamHost& host, std::uint32_t volume) {
  if (volume > kMaximumStreamVolume) return StreamStatus::kInvalidVolume;
  host.SetVolume(volume);
  ApplyStreamGains(host);
  return StreamStatus::kApplied;
}

// decomp: Sound_Cmd_E5_FadeOutCutscene (src/SLUS_010.40/25AC.c:6525). A zero
// duration slides in one tick.
StreamStatus SlideStreamVolume(StreamHost& host, std::uint32_t ticks,
                               std::uint32_t target) {
  if (ticks > kMaximumFadeTicks || target > kMaximumStreamVolume) {
    return StreamStatus::kInvalidFade;
  }
  const auto steps = std::max<std::uint32_t>(1, ticks);
  const auto volume = static_cast<std::int32_t>(
      std::min(host.Mix().volume, kMaximumStreamVolume));
  const auto step = (static_cast<std::int32_t>(target) - volume) /
                    static_cast<std::int32_t>(steps);
  host.StartFade({step, steps, target});
  return StreamStatus::kApplied;
}

// decomp: func_8001D7A8 (src/SLUS_010.40/25AC.c:6537).
StreamStatus SetStreamPan(StreamHost& host, std::uint32_t pan) {
  if (!ValidPan(pan)) return StreamStatus::kInvalidPan;
  host.SetPan(pan);
  ApplyStreamGains(host);
  return StreamStatus::kApplied;
}
}  // namespace VS::Audio::Cutscene

namespace VS::Audio {
// decomp: func_80046608 (src/SLUS_010.40/main.c:10339). The original stores
// the stream pointer in unk14[22]; the native slot also requires a valid AKAO
// stream with exported channel assets.
RoomStreamBinding BindRoomCutsceneStream(Cutscene::StreamHost& host,
                                         std::span<const std::uint8_t> bytes) {
  if (host.SlotBound()) return {0, RoomStreamStatus::kSlotOccupied, {}};
  if (bytes.empty()) return {0, RoomStreamStatus::kMissingData, {}};
  auto info = Content::InspectAudioStream(bytes);
  if (!info) return {0, RoomStreamStatus::kInvalidStream, info.error()};
  if (auto valid = host.ValidateAssets(*info); !valid) {
    return {0, RoomStreamStatus::kMissingAssets, valid.error()};
  }
  host.BindSlot(*info);
  return {kRoomStreamHandle, RoomStreamStatus::kBound, {}};
}

// decomp: func_80046634 (src/SLUS_010.40/main.c:10348). func_80013328's 0xE2
// stop command becomes a direct stop.
int UnbindRoomCutsceneStream(Cutscene::StreamHost& host) {
  if (!host.SlotBound()) return 0;
  Cutscene::StopStreamVoices(host);
  host.UnbindSlot();
  return 1;
}
}  // namespace VS::Audio
