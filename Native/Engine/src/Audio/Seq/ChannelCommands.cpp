// Hand-maintained (tools/port/owned.csv).
#include <cstdint>
#include <optional>

#include "Audio/SeqChannelCommands.hpp"
#include "Audio/VoiceOutput.hpp"

namespace VS::Audio::Seq {
namespace {
constexpr int kByteShift = 8;
constexpr std::uint16_t kVolumeBalanceSlideMask = 0x7F00;
// Channel volumes are signed 8-bit levels in bits 23-30.
constexpr int kVolumeShift = 23;
constexpr std::uint32_t kVolumeSlideMask = 0xFFFF0000;
constexpr std::uint32_t kHighByteMask = 0xFF00;
constexpr std::uint32_t kByteMask = 0xFF;
// The pan operand is signed around the centre; 0x40 moves it to 0-255.
constexpr std::uint32_t kPanCentre = 0x40;
constexpr std::uint16_t kOctaveMask = 0xF;
// The key a patch change leaves so the next note always reselects.
constexpr std::int16_t kUnsetKey = 0xFF;
// A fine tune scales the base pitch by its low byte over 128 when sharp, and
// over 256 less the base pitch when flat.
constexpr int kSharpFineTuneShift = 7;
constexpr int kFlatFineTuneShift = 8;
constexpr std::int32_t kShortestFixedNote = 1;
constexpr std::int32_t kLongestFixedNote = 255;
constexpr std::size_t kKeyOnRampOperands = 3;

// The operand bytes the original loads with lb: sign-extend bit 7.
std::int16_t Signed(std::uint8_t value) {
  constexpr int kSignBit = 0x80;
  return static_cast<std::int16_t>((value ^ kSignBit) - kSignBit);
}

std::int32_t VolumeLevel(std::uint8_t value) {
  return static_cast<std::int32_t>(
      static_cast<std::uint32_t>(static_cast<std::int32_t>(Signed(value)))
      << kVolumeShift);
}

std::uint16_t PanPosition(std::uint8_t value) {
  return static_cast<std::uint16_t>(((value + kPanCentre) & kByteMask)
                                    << kByteShift);
}

// decomp: SLUS_010.40 Sound_CopyInstrumentInfoToChannel
// (src/SLUS_010.40/25AC.c:4953)
void ApplyInstrument(VoiceState& voice,
                     const Samples::InstrumentRecord& instrument,
                     std::uint32_t sample_start) {
  voice.sample_start = sample_start;
  voice.loop_start = instrument.loop_start;
  voice.adsr_lower = instrument.adsr_lower;
  voice.adsr_upper = instrument.adsr_upper;
  voice.dirty_flags |= kInstrumentDirty;
}

// A1 and FE0A: an empty sample start takes the instrument's own.
CommandResult SelectInstrument(ChannelState& channel, VoiceCommandHost& host,
                               std::int32_t index,
                               std::optional<std::uint32_t> sample_start) {
  const std::optional<Samples::InstrumentRecord> instrument =
      host.Instrument(index);
  if (!instrument) return {CommandStatus::kInstrumentMissing, 1};
  ApplyInstrument(channel.voice, *instrument,
                  sample_start.value_or(instrument->sample_start));
  channel.instrument_index = static_cast<std::uint16_t>(index);
  channel.voice.volume_scale = 0;
  channel.update_flags &= ~kInstrumentFlags;
  return Applied(1);
}

// D8 and D9 (SLUS_010.40/25AC.c:5423, 5442): the multiply wraps and both
// shifts are logical, as the original's unsigned arithmetic.
std::int32_t FinePitchDelta(std::int32_t pitch_base, std::int16_t fine_tune) {
  const std::uint32_t base = static_cast<std::uint32_t>(pitch_base);
  const std::uint32_t scaled =
      base * (static_cast<std::uint32_t>(fine_tune) & kByteMask);
  if (fine_tune < 0) {
    return static_cast<std::int32_t>((scaled >> kFlatFineTuneShift) - base);
  }
  return static_cast<std::int32_t>(scaled >> kSharpFineTuneShift);
}

void ApplyFineTune(ChannelState& channel, std::int16_t fine_tune) {
  channel.fine_tune = fine_tune;
  channel.fine_pitch_delta = FinePitchDelta(channel.pitch_base, fine_tune);
  channel.voice.dirty_flags |= kSampleRateDirty;
}
}  // namespace

// decomp: SLUS_010.40 SoundVM_A3_ChannelMasterVolume
// (src/SLUS_010.40/25AC.c:5125)
CommandResult SetVolumeBalance(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  channel.volume_balance =
      static_cast<std::uint16_t>(operands[0] << kByteShift);
  channel.voice.dirty_flags |= kVolumeDirty;
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_FE12_VolumeBalanceSlide
// (src/SLUS_010.40/25AC.c:5132)
CommandResult SlideVolumeBalance(ChannelState& channel, Operands operands) {
  if (operands.size() < 2) return Truncated();
  const std::uint16_t length = TickCount(operands[0]);
  channel.volume_balance_slide_length = length;
  const auto start = static_cast<std::uint16_t>(channel.volume_balance &
                                                kVolumeBalanceSlideMask);
  channel.volume_balance = start;
  channel.volume_balance_slide_step = static_cast<std::int16_t>(
      SlideStep(operands[1] << kByteShift, start, length));
  return Applied(2);
}

// decomp: SLUS_010.40 SoundVM_A8_ChannelVolume (src/SLUS_010.40/25AC.c:5154)
CommandResult SetVolume(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  channel.volume = VolumeLevel(operands[0]);
  channel.volume_slide_length = 0;
  channel.voice.dirty_flags |= kVolumeDirty;
  channel.key_on_volume_slide_length = 0;
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_A9_ChannelVolumeSlide
// (src/SLUS_010.40/25AC.c:5163)
CommandResult SlideVolume(ChannelState& channel, Operands operands) {
  if (operands.size() < 2) return Truncated();
  const std::uint16_t length = TickCount(operands[0]);
  channel.volume_slide_length = length;
  const auto start = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(channel.volume) & kVolumeSlideMask);
  channel.volume = start;
  channel.volume_slide_step =
      SlideStep(VolumeLevel(operands[1]), start, length);
  channel.key_on_volume_slide_length = 0;
  return Applied(2);
}

// decomp: SLUS_010.40 SoundVM_FE19_unk (src/SLUS_010.40/25AC.c:5187)
CommandResult SetKeyOnVolumeRamp(ChannelState& channel, Operands operands) {
  if (operands.size() < kKeyOnRampOperands) return Truncated();
  channel.key_on_volume = VolumeLevel(operands[0]);
  const std::uint16_t length = TickCount(operands[1]);
  channel.key_on_volume_slide_length = length;
  channel.key_on_volume_slide_step =
      SlideStep(VolumeLevel(operands[2]), channel.key_on_volume, length);
  return Applied(kKeyOnRampOperands);
}

// decomp: SLUS_010.40 SoundVM_FE1A_unk (src/SLUS_010.40/25AC.c:5209)
void EnableFullNoteLength(ChannelState& channel) {
  channel.update_flags |= kFullNoteLengthActive;
}

// decomp: SLUS_010.40 SoundVM_FE1B_unk (src/SLUS_010.40/25AC.c:5215)
void DisableFullNoteLength(ChannelState& channel) {
  channel.update_flags &= ~kFullNoteLengthActive;
}

// decomp: SLUS_010.40 SoundVM_unused1 (src/SLUS_010.40/25AC.c:5234)
CommandResult SetEffectVolumeScale(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  channel.effect_volume = static_cast<std::int16_t>(operands[0] << kByteShift);
  channel.effect_volume_slide_length = 0;
  if ((channel.update_flags & kOverlayChannel) != 0) {
    channel.voice.dirty_flags |= kVolumeDirty;
  }
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_unused2 (src/SLUS_010.40/25AC.c:5246)
CommandResult SlideEffectVolumeScale(ChannelState& channel, Operands operands) {
  if (operands.size() < 2) return Truncated();
  const std::uint16_t length = TickCount(operands[0]);
  channel.effect_volume_slide_length = length;
  const auto start = static_cast<std::int16_t>(
      static_cast<std::uint16_t>(channel.effect_volume) & kHighByteMask);
  channel.effect_volume_slide_step = static_cast<std::int16_t>(
      SlideStep(operands[1] << kByteShift, start, length));
  channel.effect_volume = start;
  return Applied(2);
}

// decomp: SLUS_010.40 SoundVM_AA_ChannelPan (src/SLUS_010.40/25AC.c:5264)
CommandResult SetPan(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  channel.pan = PanPosition(operands[0]);
  channel.pan_slide_length = 0;
  channel.voice.dirty_flags |= kVolumeDirty;
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_AB_ChannelPanSlide (src/SLUS_010.40/25AC.c:5274)
CommandResult SlidePan(ChannelState& channel, Operands operands) {
  if (operands.size() < 2) return Truncated();
  const std::uint16_t length = TickCount(operands[0]);
  channel.pan_slide_length = length;
  const auto start = static_cast<std::uint16_t>(channel.pan & kHighByteMask);
  channel.pan_slide_step = static_cast<std::int16_t>(
      SlideStep(PanPosition(operands[1]), start, length));
  channel.pan = start;
  return Applied(2);
}

// decomp: SLUS_010.40 SoundVM_A5_SetOctave (src/SLUS_010.40/25AC.c:5296)
CommandResult SetOctave(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  channel.octave = operands[0];
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_A6_IncreaseOctave (src/SLUS_010.40/25AC.c:5302)
void IncreaseOctave(ChannelState& channel) {
  channel.octave =
      static_cast<std::uint16_t>((channel.octave + 1) & kOctaveMask);
}

// decomp: SLUS_010.40 SoundVM_A7_DecreaseOctave (src/SLUS_010.40/25AC.c:5308)
void DecreaseOctave(ChannelState& channel) {
  channel.octave =
      static_cast<std::uint16_t>((channel.octave - 1) & kOctaveMask);
}

// decomp: SLUS_010.40 SoundVM_A1_LoadInstrument (src/SLUS_010.40/25AC.c:5314)
CommandResult LoadInstrument(ChannelState& channel, Operands operands,
                             VoiceCommandHost& host) {
  if (operands.empty()) return Truncated();
  const std::int32_t index = host.ResolveInstrument(
      channel.kind, channel.effect_voice_mask, operands[0]);
  return SelectInstrument(channel, host, index, std::nullopt);
}

// decomp: SLUS_010.40 SoundVM_FE0A_ClearInstrument
// (src/SLUS_010.40/25AC.c:5329)
CommandResult LoadSilentInstrument(ChannelState& channel, Operands operands,
                                   VoiceCommandHost& host) {
  if (operands.empty()) return Truncated();
  return SelectInstrument(channel, host, operands[0], kSilentSampleStart);
}

// decomp: SLUS_010.40 SoundVM_FE14_ChangePatch (src/SLUS_010.40/25AC.c:5343)
CommandResult SelectPatch(ChannelState& channel, Operands operands,
                          VoiceCommandHost& host) {
  if (operands.empty()) return Truncated();
  const PatchEntry entry = host.FindPatch(operands[0]);
  if (entry.lookup == PatchLookup::kNoTable) return Applied(1);
  if (entry.lookup == PatchLookup::kOutOfRange) {
    return {CommandStatus::kPatchOutOfRange, 1};
  }
  if (entry.offset > kMutedPatch) {
    channel.voice.volume_scale = 0;
    channel.update_flags &= ~kPatchKeymapActive;
    return Applied(1);
  }
  const std::uint32_t keymap = entry.offset + kKeymapHeaderBytes;
  if (!host.HasPatchKeymap(keymap)) return {CommandStatus::kPatchOutOfRange, 1};
  channel.keymap_offset = keymap;
  channel.update_flags =
      (channel.update_flags & ~kInstrumentFlags) | kPatchKeymapActive;
  channel.key = kUnsetKey;
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_C0_ChannelTranspose_Absolute
// (src/SLUS_010.40/25AC.c:5378)
CommandResult SetTranspose(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  channel.transpose = Signed(operands[0]);
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_C1_ChannelTranspose_Relative
// (src/SLUS_010.40/25AC.c:5384)
CommandResult AddTranspose(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  channel.transpose =
      static_cast<std::int16_t>(channel.transpose + Signed(operands[0]));
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_A4_PitchBendSlide (src/SLUS_010.40/25AC.c:5390)
CommandResult SetPitchBendSlide(ChannelState& channel, Operands operands) {
  if (operands.size() < 2) return Truncated();
  channel.pitch_bend_slide_length = TickCount(operands[0]);
  channel.pitch_bend_target = Signed(operands[1]);
  return Applied(2);
}

// decomp: SLUS_010.40 SoundVM_DA_EnablePortamento (src/SLUS_010.40/25AC.c:5402)
CommandResult EnablePortamento(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  channel.portamento_steps = TickCount(operands[0]);
  channel.portamento_transpose = 0;
  channel.portamento_key = 0;
  channel.play_modes = kLegatoPlay;
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_DB_DisablePortamento
// (src/SLUS_010.40/25AC.c:5417)
void DisablePortamento(ChannelState& channel) { channel.portamento_steps = 0; }

// decomp: SLUS_010.40 SoundVM_D8_ChannelFineTune_Absolute
// (src/SLUS_010.40/25AC.c:5423)
CommandResult SetFineTune(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  ApplyFineTune(channel, Signed(operands[0]));
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_D9_ChannelFineTune_Relative
// (src/SLUS_010.40/25AC.c:5442)
CommandResult AddFineTune(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  ApplyFineTune(channel, static_cast<std::int16_t>(channel.fine_tune +
                                                   Signed(operands[0])));
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_CC_EnableLegato (src/SLUS_010.40/25AC.c:5752)
void EnableLegato(ChannelState& channel) { channel.play_modes = kLegatoPlay; }

// decomp: SLUS_010.40 SoundVM_D0_EnableSustainedNote
// (src/SLUS_010.40/25AC.c:5763)
void EnableSustainedSoundEffect(ChannelState& channel) {
  if (channel.kind != Driver::ChannelKind::Music) {
    channel.play_modes = kFullLengthPlay;
  }
}

// decomp: SLUS_010.40 SoundVM_A2_OverwriteNextNoteLength
// (src/SLUS_010.40/25AC.c:6001)
CommandResult SetNextNoteLength(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  const std::uint8_t length = operands[0];
  channel.fixed_note_length = 0;
  channel.gate_length = length;
  channel.note_length = length;
  channel.stored_note_length = length;
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_DC_FixNoteLength (src/SLUS_010.40/25AC.c:6015)
CommandResult SetFixedNoteLength(ChannelState& channel, Operands operands) {
  if (operands.empty()) return Truncated();
  std::int32_t length = Signed(operands[0]);
  if (length != 0) {
    length += channel.stored_note_length;
    if (length < kShortestFixedNote) {
      length = kShortestFixedNote;
    } else if (length > kLongestFixedNote) {
      length = kLongestFixedNote;
    }
  }
  channel.fixed_note_length = static_cast<std::int16_t>(length);
  return Applied(1);
}

// decomp: SLUS_010.40 SoundVM_FE04_ClearKeymapTable
// (src/SLUS_010.40/25AC.c:6032)
void EnableDrumKeymap(ChannelState& channel, VoiceCommandHost& host) {
  if (!host.HasDrumKeymap()) return;
  channel.update_flags =
      (channel.update_flags & ~kInstrumentFlags) | kDrumKeymapActive;
}

// decomp: SLUS_010.40 SoundVM_FE05_MuteVoice (src/SLUS_010.40/25AC.c:6043)
void MuteInstrument(ChannelState& channel) {
  channel.voice.volume_scale = 0;
  channel.update_flags &= ~kDrumKeymapActive;
}

// decomp: SLUS_010.40 SoundVM_E0_unk (src/SLUS_010.40/25AC.c:6196)
void EnableLoopBoundaryStop(ChannelState& channel) {
  channel.update_flags |= kLoopBoundaryStop;
}
}  // namespace VS::Audio::Seq
