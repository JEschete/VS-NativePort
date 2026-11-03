// Hand-maintained (tools/port/owned.csv).
#include <cstddef>
#include <cstdint>
#include <span>

#include "Audio/SoundDriver.hpp"
#include "Audio/VoiceOutput.hpp"

namespace VS::Audio::Driver {
namespace {
// Bits 15 (attack mode) and 8-9 (attack step) of the ADSR lower word, bits
// 4-7 (decay rate) and 0-3 (sustain level); bits 14 (sustain direction) and
// 6-12 (sustain rate) of the upper word (SLUS_010.40/25AC.c:1178-1256).
constexpr std::uint32_t kAttackModeShift = 15;
constexpr std::uint32_t kAttackStepShift = 8;
constexpr std::uint32_t kAttackKeptBits = 0xFF;
constexpr std::uint32_t kDecayRateShift = 4;
constexpr std::uint32_t kDecayKeptBits = 0xFF0F;
constexpr std::uint32_t kSustainLevelKeptBits = 0xFFF0;
constexpr std::uint32_t kSustainDirectionShift = 14;
constexpr std::uint32_t kSustainRateShift = 6;
constexpr std::uint32_t kSustainKeptBits = 0x3F;
// SPU addresses are written in 8-byte units; volumes are 15-bit.
constexpr std::uint32_t kAddressShift = 3;
constexpr std::uint32_t kVolumeMask = 0x7FFF;
constexpr std::int32_t kVolumeScaleShift = 7;

std::uint32_t Bits(std::int32_t value) {
  return static_cast<std::uint32_t>(value);
}

// The voice a register write names, or nothing (refused) outside the 24 SPU
// voices: the original indexes past the voice blocks into the SPU's common
// registers.
bool ValidVoice(SoundDriverHost& host, std::int32_t voice) {
  if (voice >= 0 && Bits(voice) < kVoiceCount) return true;
  host.Refuse(DriverRefusal::kInvalidVoice);
  return false;
}

std::uint16_t Halfword(std::uint32_t value) {
  return static_cast<std::uint16_t>(value);
}

// The original's `while (call() == 0) ;`, bounded.
template <typename Call>
bool Retry(SoundDriverHost& host, DriverRefusal refusal, Call call) {
  for (std::uint32_t attempt = 0; attempt < kSdkRetryLimit; ++attempt) {
    if (call()) return true;
  }
  host.Refuse(refusal);
  return false;
}

// `mult` then `sra 7`: the product wraps to 32 bits.
std::int32_t ScaledVolume(std::int16_t volume, std::int32_t scale) {
  const std::uint32_t product =
      Bits(static_cast<std::int32_t>(volume)) * Bits(scale);
  return static_cast<std::int32_t>(product) >> kVolumeScaleShift;
}
}  // namespace

// decomp: SLUS_010.40 _spuWriteComplete (src/SLUS_010.40/25AC.c:989)
void CompleteSampleTransfer(SoundDriverHost& host) {
  host.SetTransferCallback(TransferCallback::kNone);
  host.SetTransferState(kTransferIdle);
}

// decomp: SLUS_010.40 spuSetTransferCallback (src/SLUS_010.40/25AC.c:995)
void ArmSampleTransferCallback(SoundDriverHost& host) {
  host.SetTransferState(kTransferBusy);
  host.SetTransferCallback(TransferCallback::kCompleteTransfer);
}

// decomp: SLUS_010.40 _writeSpu (src/SLUS_010.40/25AC.c:1001)
void WriteSampleMemory(SoundDriverHost& host, std::span<const std::byte> data) {
  host.SetTransferState(kTransferBusy);
  host.SetTransferCallback(TransferCallback::kCompleteTransfer);
  host.WriteSampleMemory(data);
}

// decomp: SLUS_010.40 _readSpu (src/SLUS_010.40/25AC.c:1008)
void ReadSampleMemory(SoundDriverHost& host, std::span<std::byte> data) {
  ArmSampleTransferCallback(host);
  host.ReadSampleMemory(data);
}

// decomp: SLUS_010.40 _waitTransferAvailable (src/SLUS_010.40/25AC.c:1014)
bool WaitForSampleTransfer(SoundDriverHost& host) {
  for (std::uint32_t poll = 0; poll < kTransferPollLimit; ++poll) {
    if (host.TransferState() != kTransferBusy) return true;
  }
  host.Refuse(DriverRefusal::kTransferNeverEnded);
  return false;
}

// decomp: SLUS_010.40 StartSound (src/SLUS_010.40/25AC.c:1060)
bool Start(SoundDriverHost& host) {
  host.ResetPlayback();
  if (!Retry(host, DriverRefusal::kTickTimerUnavailable,
             [&host] { return host.SetTickTimer(kTickTimerTarget); }) ||
      !Retry(host, DriverRefusal::kTickTimerUnavailable,
             [&host] { return host.StartTickTimer(); })) {
    return false;
  }
  const bool opened =
      Retry(host, DriverRefusal::kTickEventUnavailable, [&host] {
        const std::int32_t event = host.OpenTickEvent();
        host.SetTickEvent(event);
        return event != kNoTickEvent;
      });
  return opened && Retry(host, DriverRefusal::kTickEventUnavailable, [&host] {
           return host.EnableTickEvent(host.TickEvent());
         });
}

// decomp: SLUS_010.40 StopSound (src/SLUS_010.40/25AC.c:1085)
bool Stop(SoundDriverHost& host) {
  const bool stopped =
      Retry(host, DriverRefusal::kTickTimerUnavailable,
            [&host] { return host.StopTickTimer(); }) &&
      Retry(host, DriverRefusal::kTickEventUnavailable,
            [&host] { return host.DisableTickEvent(host.TickEvent()); }) &&
      Retry(host, DriverRefusal::kTickEventUnavailable,
            [&host] { return host.CloseTickEvent(host.TickEvent()); });
  if (stopped) host.ResetOutput();
  return stopped;
}

// decomp: SLUS_010.40 SetVoiceStartAddr (src/SLUS_010.40/25AC.c:1153)
void SetVoiceStartAddress(SoundDriverHost& host, std::int32_t voice,
                          std::uint32_t address) {
  if (!ValidVoice(host, voice)) return;
  host.WriteVoiceRegister(Bits(voice), VoiceRegister::kStartAddress,
                          Halfword(address >> kAddressShift));
}

// decomp: SLUS_010.40 SetVoiceRepeatAddr (src/SLUS_010.40/25AC.c:1158)
void SetVoiceLoopAddress(SoundDriverHost& host, std::int32_t voice,
                         std::uint32_t address) {
  if (!ValidVoice(host, voice)) return;
  host.WriteVoiceRegister(Bits(voice), VoiceRegister::kLoopAddress,
                          Halfword(address >> kAddressShift));
}

// decomp: SLUS_010.40 SetVoiceAdsrLower (src/SLUS_010.40/25AC.c:1163)
void SetVoiceEnvelopeLower(SoundDriverHost& host, std::int32_t voice,
                           std::uint16_t value) {
  if (!ValidVoice(host, voice)) return;
  host.WriteVoiceRegister(Bits(voice), VoiceRegister::kEnvelopeLower, value);
}

// decomp: SLUS_010.40 SetVoiceAdsrUpper (src/SLUS_010.40/25AC.c:1168)
void SetVoiceEnvelopeUpper(SoundDriverHost& host, std::int32_t voice,
                           std::uint16_t value) {
  if (!ValidVoice(host, voice)) return;
  host.WriteVoiceRegister(Bits(voice), VoiceRegister::kEnvelopeUpper, value);
}

// decomp: SLUS_010.40 SetVoiceAdsrAttackRateAndMode
// (src/SLUS_010.40/25AC.c:1262)
void SetVoiceAttackRateAndMode(SoundDriverHost& host, std::int32_t voice,
                               std::int32_t attack_step,
                               std::uint32_t attack_mode) {
  if (!ValidVoice(host, voice)) return;
  const std::uint32_t lower =
      host.ReadVoiceRegister(Bits(voice), VoiceRegister::kEnvelopeLower);
  const std::uint32_t mode = (attack_mode >> 2) << kAttackModeShift;
  const std::uint32_t step = Bits(attack_step) << kAttackStepShift;
  host.WriteVoiceRegister(Bits(voice), VoiceRegister::kEnvelopeLower,
                          Halfword(mode | step | (lower & kAttackKeptBits)));
}

// decomp: SLUS_010.40 SetVoiceAdsrDecayRate (src/SLUS_010.40/25AC.c:1274)
void SetVoiceDecayRate(SoundDriverHost& host, std::int32_t voice,
                       std::int32_t decay_rate) {
  if (!ValidVoice(host, voice)) return;
  const std::uint32_t lower =
      host.ReadVoiceRegister(Bits(voice), VoiceRegister::kEnvelopeLower);
  const std::uint32_t rate = Bits(decay_rate) << kDecayRateShift;
  host.WriteVoiceRegister(Bits(voice), VoiceRegister::kEnvelopeLower,
                          Halfword((lower & kDecayKeptBits) | rate));
}

// decomp: SLUS_010.40 SetVoiceAdsrSustainLevel (src/SLUS_010.40/25AC.c:1283)
void SetVoiceSustainLevel(SoundDriverHost& host, std::int32_t voice,
                          std::int32_t sustain_level) {
  if (!ValidVoice(host, voice)) return;
  const std::uint32_t lower =
      host.ReadVoiceRegister(Bits(voice), VoiceRegister::kEnvelopeLower);
  host.WriteVoiceRegister(
      Bits(voice), VoiceRegister::kEnvelopeLower,
      Halfword((lower & kSustainLevelKeptBits) | Bits(sustain_level)));
}

// decomp: SLUS_010.40 SetVoiceAdsrSustainRateAndDirection
// (src/SLUS_010.40/25AC.c:1290)
void SetVoiceSustainRateAndMode(SoundDriverHost& host, std::int32_t voice,
                                std::int32_t sustain_rate,
                                std::uint32_t sustain_direction) {
  if (!ValidVoice(host, voice)) return;
  const std::uint32_t upper =
      host.ReadVoiceRegister(Bits(voice), VoiceRegister::kEnvelopeUpper);
  const std::uint32_t direction = (sustain_direction >> 1)
                                  << kSustainDirectionShift;
  const std::uint32_t rate = Bits(sustain_rate) << kSustainRateShift;
  host.WriteVoiceRegister(
      Bits(voice), VoiceRegister::kEnvelopeUpper,
      Halfword((upper & kSustainKeptBits) | direction | rate));
}

// decomp: SLUS_010.40 SetVoiceParams (src/SLUS_010.40/25AC.c:1312)
void WriteVoiceParameters(SoundDriverHost& host, std::int32_t voice,
                          VoiceParameters& parameters,
                          std::int32_t volume_scale) {
  if (!ValidVoice(host, voice)) return;
  parameters.dirty_flags = 0;
  std::int32_t left = parameters.volume_left;
  std::int32_t right = parameters.volume_right;
  if (volume_scale != 0) {
    left = ScaledVolume(parameters.volume_left, volume_scale);
    right = ScaledVolume(parameters.volume_right, volume_scale);
  }
  const std::uint32_t index = Bits(voice);
  host.WriteVoiceRegister(index, VoiceRegister::kVolumeLeft,
                          Halfword(Bits(left) & kVolumeMask));
  host.WriteVoiceRegister(index, VoiceRegister::kVolumeRight,
                          Halfword(Bits(right) & kVolumeMask));
  host.WriteVoiceRegister(index, VoiceRegister::kSampleRate,
                          parameters.sample_rate);
  host.WriteVoiceRegister(index, VoiceRegister::kStartAddress,
                          Halfword(parameters.sample_start >> kAddressShift));
  host.WriteVoiceRegister(index, VoiceRegister::kEnvelopeLower,
                          parameters.adsr_lower);
  host.WriteVoiceRegister(index, VoiceRegister::kEnvelopeUpper,
                          parameters.adsr_upper);
  host.WriteVoiceRegister(index, VoiceRegister::kLoopAddress,
                          Halfword(parameters.loop_start >> kAddressShift));
}

// decomp: SLUS_010.40 SetVoiceParamsByFlags (src/SLUS_010.40/25AC.c:1342)
void WriteDirtyVoiceParameters(SoundDriverHost& host, std::uint32_t voice,
                               VoiceParameters& parameters) {
  std::uint32_t flags = parameters.dirty_flags;
  if (flags == 0) return;
  if (voice >= kVoiceCount) {
    host.Refuse(DriverRefusal::kInvalidVoice);
    return;
  }
  parameters.dirty_flags = 0;
  // Each group clears its flags; the original returns once none are left.
  const auto take = [&flags](std::uint32_t group) {
    const bool dirty = (flags & group) != 0;
    flags &= ~group;
    return dirty;
  };
  const auto signed_voice = static_cast<std::int32_t>(voice);
  if (take(kSampleRateParameter)) {
    host.SetVoiceSampleRate(voice, parameters.sample_rate);
  }
  if (flags != 0 && take(kVolumeParameters)) {
    host.SetVoiceVolume(voice, Bits(parameters.volume_left),
                        Bits(parameters.volume_right), parameters.volume_scale);
  }
  if (flags != 0 && take(kStartAddressParameter)) {
    SetVoiceStartAddress(host, signed_voice, parameters.sample_start);
  }
  if (flags != 0 && take(kLoopAddressParameter)) {
    SetVoiceLoopAddress(host, signed_voice, parameters.loop_start);
  }
  if (flags != 0 && take(kEnvelopeUpperParameters)) {
    SetVoiceEnvelopeUpper(host, signed_voice, parameters.adsr_upper);
  }
  if (flags != 0 && take(kEnvelopeLowerParameters)) {
    SetVoiceEnvelopeLower(host, signed_voice, parameters.adsr_lower);
  }
}
}  // namespace VS::Audio::Driver
