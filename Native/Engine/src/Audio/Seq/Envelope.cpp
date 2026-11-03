// Hand-maintained (tools/port/owned.csv).
#include <cstdint>
#include <optional>

#include "Audio/SeqEnvelopeCommands.hpp"
#include "Audio/VoiceOutput.hpp"

namespace VS::Audio::Seq {
namespace {
// SPU ADSR lower word: attack mode 15, attack rate 8-14, decay rate 4-7,
// sustain level 0-3. Upper word: sustain mode 14-15, sustain rate 6-12,
// release mode 5, release rate 0-4.
constexpr std::uint32_t kAttackRateMask = 0x7F00;
constexpr int kAttackRateShift = 8;
constexpr std::uint32_t kDecayRateMask = 0xF0;
constexpr int kDecayRateShift = 4;
constexpr std::uint32_t kSustainLevelMask = 0xF;
constexpr std::uint32_t kSustainRateMask = 0x1FC0;
constexpr int kSustainRateShift = 6;
constexpr std::uint32_t kReleaseRateMask = 0x1F;
constexpr std::uint16_t kAttackExponential = 0x8000;
constexpr std::uint16_t kSustainModeMask = 0xC000;
constexpr std::uint16_t kSustainDecreasing = 0x4000;
constexpr std::uint16_t kSustainExponential = 0x8000;
constexpr std::uint16_t kReleaseExponential = 0x20;

// Sequence envelope mode numbers.
constexpr std::uint8_t kLinearDecreaseMode = 3;
constexpr std::uint8_t kExponentialIncreaseMode = 5;
constexpr std::uint8_t kExponentialDecreaseMode = 7;

std::uint16_t ReplaceField(std::uint16_t word, std::uint32_t mask,
                           std::uint32_t value) {
  return static_cast<std::uint16_t>((word & ~mask) | value);
}
}  // namespace

// decomp: SLUS_010.40 SoundVM_B3_ResetAdsr (src/SLUS_010.40/25AC.c:5366)
CommandStatus ResetEnvelope(ChannelState& channel, VoiceCommandHost& host) {
  const std::optional<Samples::InstrumentRecord> instrument =
      host.Instrument(channel.instrument_index);
  if (!instrument) return CommandStatus::kInstrumentMissing;
  channel.voice.adsr_lower = instrument->adsr_lower;
  channel.voice.adsr_upper = instrument->adsr_upper;
  channel.voice.dirty_flags |= kEnvelopeDirty;
  channel.update_flags &=
      ~(kAttackOverridden | kSustainOverridden | kReleaseOverridden);
  return CommandStatus::kApplied;
}

// decomp: SLUS_010.40 SoundVM_AD_AttackRate (src/SLUS_010.40/25AC.c:5796)
CommandResult SetAttackRate(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  channel.voice.adsr_lower =
      ReplaceField(channel.voice.adsr_lower, kAttackRateMask,
                   static_cast<std::uint32_t>(operands[0]) << kAttackRateShift);
  channel.voice.dirty_flags |= kAttackModeDirty | kAttackRateDirty;
  channel.update_flags |= kAttackOverridden;
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_AE_DecayRate (src/SLUS_010.40/25AC.c:5807)
CommandResult SetDecayRate(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  channel.voice.adsr_lower =
      ReplaceField(channel.voice.adsr_lower, kDecayRateMask,
                   static_cast<std::uint32_t>(operands[0]) << kDecayRateShift);
  channel.voice.dirty_flags |= kDecayRateDirty;
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_AF_SustainLevel (src/SLUS_010.40/25AC.c:5816)
CommandResult SetSustainLevel(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  channel.voice.adsr_lower =
      ReplaceField(channel.voice.adsr_lower, kSustainLevelMask, operands[0]);
  channel.voice.dirty_flags |= kSustainLevelDirty;
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_B1_SustainRate (src/SLUS_010.40/25AC.c:5825)
CommandResult SetSustainRate(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  channel.voice.adsr_upper = ReplaceField(
      channel.voice.adsr_upper, kSustainRateMask,
      static_cast<std::uint32_t>(operands[0]) << kSustainRateShift);
  channel.voice.dirty_flags |= kSustainRateDirty | kSustainModeDirty;
  channel.update_flags |= kSustainOverridden;
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_B2_ReleaseRate (src/SLUS_010.40/25AC.c:5836)
CommandResult SetReleaseRate(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  channel.voice.adsr_upper =
      ReplaceField(channel.voice.adsr_upper, kReleaseRateMask, operands[0]);
  channel.voice.dirty_flags |= kReleaseRateDirty | kReleaseModeDirty;
  channel.update_flags |= kReleaseOverridden;
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_B7_AttackMode (src/SLUS_010.40/25AC.c:5847)
CommandResult SetAttackMode(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  const std::uint16_t mode =
      operands[0] == kExponentialIncreaseMode ? kAttackExponential : 0;
  channel.voice.adsr_lower =
      ReplaceField(channel.voice.adsr_lower, kAttackExponential, mode);
  channel.voice.dirty_flags |= kAttackModeDirty;
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_BB_SustainMode (src/SLUS_010.40/25AC.c:5860)
CommandResult SetSustainMode(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  std::uint16_t mode = 0;
  if (operands[0] == kLinearDecreaseMode) {
    mode = kSustainDecreasing;
  } else if (operands[0] == kExponentialIncreaseMode) {
    mode = kSustainExponential;
  } else if (operands[0] == kExponentialDecreaseMode) {
    mode = kSustainExponential | kSustainDecreasing;
  }
  channel.voice.adsr_upper =
      ReplaceField(channel.voice.adsr_upper, kSustainModeMask, mode);
  channel.voice.dirty_flags |= kSustainModeDirty;
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_BF_ReleaseMode (src/SLUS_010.40/25AC.c:5880)
CommandResult SetReleaseMode(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  const std::uint16_t mode =
      operands[0] == kExponentialDecreaseMode ? kReleaseExponential : 0;
  channel.voice.adsr_upper =
      ReplaceField(channel.voice.adsr_upper, kReleaseExponential, mode);
  channel.voice.dirty_flags |= kReleaseModeDirty;
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_B0_DecayRateAndSustainLevel
// (src/SLUS_010.40/25AC.c:6066)
CommandResult SetDecayAndSustain(ChannelState& channel, Operands operands) {
  if (operands.size() < 2) return Truncated();
  static_cast<void>(SetDecayRate(channel, operands));
  static_cast<void>(SetSustainLevel(channel, operands.subspan(1)));
  return Applied(2);
}
}  // namespace VS::Audio::Seq
