// Hand-maintained (tools/port/owned.csv).
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Audio/PlaybackReset.hpp"
#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqSequencer.hpp"
#include "Audio/SeqVoiceCommands.hpp"
#include "Audio/VoiceOutput.hpp"

namespace VS::Audio::Seq {
namespace {
using Driver::ChannelKind;

// Step opcodes: twelve semitones of eleven note lengths, then eleven ties
// and eleven rests.
constexpr std::uint32_t kNoteLengthSlots = 11;
constexpr std::uint32_t kFirstTie = 0x84;
constexpr std::uint32_t kFirstRest = 0x8F;
// Opcodes from here to the commands end the lookahead like the track end.
constexpr std::uint32_t kFirstReserved = 0x9A;
constexpr std::uint32_t kEndOfTrack = 0xA0;
// The note the lookahead reports for an F0-FB timed note.
constexpr std::uint8_t kLastNote = 0x83;
// F0-FD: a note, tie or rest with an explicit length operand.
constexpr std::uint32_t kFirstTimedNote = 0xF0;
constexpr std::uint32_t kTimedNoteCount = 0xE;
constexpr std::uint8_t kTimedTie = 0xFC;
constexpr std::uint8_t kTimedRest = 0xFD;
constexpr std::uint8_t kExtendedOpcode = 0xFE;
constexpr std::uint8_t kStopTrack = 0xFF;
constexpr std::uint8_t kRepeatLoopOpcode = 0xC9;
constexpr std::uint8_t kRepeatLoopForeverOpcode = 0xCA;
// The one-byte commands the lookahead steps over that end held play.
constexpr std::uint8_t kHeldPlayCommandCb = 0xCB;
constexpr std::uint8_t kHeldPlayCommandCd = 0xCD;
constexpr std::uint8_t kHeldPlayCommandD1 = 0xD1;
constexpr std::uint8_t kHeldPlayCommandDb = 0xDB;
// The extended commands the lookahead follows.
constexpr std::uint8_t kJumpRelativeOpcode = 0x06;
constexpr std::uint8_t kJumpAtThresholdOpcode = 0x07;
constexpr std::uint8_t kJumpOnLoopPassOpcode = 0x08;
constexpr std::uint8_t kExitLoopOnPassOpcode = 0x09;
constexpr std::uint8_t kCallRelativeOpcode = 0x0E;
constexpr std::uint8_t kReturnFromCallOpcode = 0x0F;
constexpr std::uint8_t kFinishOnReplacementOpcode = 0x1F;
// A loop-pass jump's pass count and offset bytes.
constexpr std::int64_t kLoopPassOperands = 3;
constexpr std::int64_t kOffsetBytes = 2;

// Defined limits for streams the original would run forever.
constexpr std::uint32_t kCommandLimit = 0x10000;
constexpr std::uint32_t kLookaheadLimit = 0x10000;
constexpr std::uint32_t kFastForwardLimit = 1U << 20;

// Channel play modes: the next note ties to this one.
constexpr std::uint16_t kTiedPlay = 2;
constexpr std::uint16_t kHeldPlay = kLegatoPlay | kFullLengthPlay;
// Gate ticks a note releases before the next one starts.
constexpr std::uint16_t kGateRelease = 2;

constexpr std::uint32_t kSemitonesPerOctave = 12;
constexpr std::uint32_t kByteBits = 8;
constexpr std::uint32_t kByteMask = 0xFF;
constexpr std::uint32_t kPitchFractionBits = 8;
constexpr std::uint32_t kPitchScaleShift = 7;
constexpr std::uint32_t kNegativeFineTune = 0x80;
constexpr std::int32_t kMaxArithmeticShift = 31;
// The SPU shifts by the low five bits of a variable shift amount.
constexpr std::int32_t kShiftAmountMask = 31;

// Instrument slots a bank selection moves an instrument to.
constexpr std::uint32_t kEffectBankFirst = 0x80;
constexpr std::uint32_t kEffectBankCount = 0x20;
constexpr std::int32_t kEffectBankOneFlag = 2;
constexpr std::int32_t kEffectBankTwoFlag = 4;
constexpr std::int32_t kBankOneOffset = 0x20;
constexpr std::int32_t kBankTwoOffset = 0x40;
constexpr std::int32_t kMusicAlternateBankFlag = 0x40;
constexpr std::uint32_t kMusicAlternateFirst = 0x40;
constexpr std::uint32_t kMusicAlternateEnd = 0x80;

// Keymap entries: patch keymaps (instrument, low key, high key, attack,
// sustain, mode, release, volume scale) and drum keymaps (instrument, note,
// attack, sustain, mode, release, volume scale, pan and reverb).
constexpr std::size_t kKeymapEntryBytes = 8;
constexpr std::size_t kPatchHighKey = 2;
constexpr std::size_t kPatchEnvelope = 3;
constexpr std::size_t kPatchVolumeScale = 7;
// The next entry's low key and mode bytes; a zero mode ends the keymap.
constexpr std::size_t kNextPatchLowKey = kKeymapEntryBytes + 1;
constexpr std::size_t kNextPatchMode = kKeymapEntryBytes + 5;
constexpr std::int16_t kUnsetKey = 0xFF;
constexpr std::size_t kDrumNote = 1;
constexpr std::size_t kDrumEnvelope = 2;
constexpr std::size_t kDrumVolumeScale = 6;
constexpr std::size_t kDrumPanReverb = 7;
// Byte offsets inside an entry's envelope (attack, sustain, mode, release).
constexpr std::size_t kEnvelopeSustain = 1;
constexpr std::size_t kEnvelopeMode = 2;
constexpr std::size_t kEnvelopeRelease = 3;
constexpr std::uint32_t kDrumPanMask = 0x7F;
constexpr std::uint32_t kDrumPanCentre = 0x40;
constexpr std::uint32_t kDrumReverb = 0x80;

// ADSR register bits a keymap entry or an override keeps.
constexpr std::uint16_t kAttackOverrideKeeps = 0x7F00;
constexpr std::uint16_t kInstrumentAdsrLowerBits = 0x80FF;
constexpr std::uint16_t kSustainKeeps = 0x201F;
constexpr std::uint32_t kSustainRateShift = 6;
constexpr std::uint16_t kSustainOverrideKeeps = 0x3FDF;
constexpr std::uint16_t kReleaseKeeps = 0xFFE0;
constexpr std::uint16_t kInstrumentReleaseModeBit = 0x20;
// Keymap envelope modes and the sustain mode bits they select.
constexpr std::uint8_t kSustainDecreaseLinear = 3;
constexpr std::uint8_t kSustainIncreaseExponential = 5;
constexpr std::uint8_t kSustainDecreaseExponential = 7;
constexpr std::uint16_t kSustainDecreaseBit = 0x4000;
constexpr std::uint16_t kSustainExponentialBit = 0x8000;

// Random pitch and vibrato depth scaling.
constexpr std::uint32_t kNegativeRandomOffset = 0x80;
constexpr std::uint32_t kRandomRaiseShift = 7;
constexpr std::uint32_t kRandomLowerShift = 9;
constexpr std::uint16_t kVibratoMagnitudeMask = 0x7F00;
constexpr std::uint16_t kVibratoFullScale = 0x8000;
constexpr std::uint32_t kVibratoReducedScale = 0xF;

std::int32_t Wrap(std::int64_t value) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

void SetBits(std::int32_t& mask, std::uint32_t bits) {
  mask = static_cast<std::int32_t>(static_cast<std::uint32_t>(mask) | bits);
}

bool IsMusic(const StepState& state) {
  return state.channel.kind == ChannelKind::kMusic;
}

std::uint16_t LoopTop(const ProgramState& program) {
  return program.loop_depth & kLoopDepthMask;
}

std::uint32_t SemitonePitch(std::uint32_t semitone, std::int16_t fine_tune) {
  if (fine_tune == 0) return semitone << kPitchFractionBits;
  if (fine_tune < 0) {
    return (semitone * static_cast<std::uint16_t>(fine_tune)) >>
           kPitchFractionBits;
  }
  return ((semitone * static_cast<std::uint32_t>(fine_tune)) >>
          kPitchScaleShift) +
         (semitone << kPitchFractionBits);
}

// The fine pitch delta a channel fine tune gives; empty for a whole note.
std::optional<std::int32_t> FinePitchDelta(std::uint32_t pitch,
                                           std::int16_t fine_tune) {
  const std::uint32_t fine =
      static_cast<std::uint32_t>(std::int32_t{fine_tune}) & kByteMask;
  if (fine == 0) return std::nullopt;
  if (fine < kNegativeFineTune) {
    return static_cast<std::int32_t>((pitch * fine) >> kPitchScaleShift);
  }
  return static_cast<std::int32_t>(((pitch * fine) >> kPitchFractionBits) -
                                   pitch);
}

std::int32_t ShiftLeft(std::int32_t value, std::int32_t amount) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(value)
                                   << (amount & kShiftAmountMask));
}

// The envelope bytes of a patch or drum keymap entry.
struct KeymapEnvelope {
  std::uint8_t attack = 0;
  std::uint8_t sustain = 0;
  std::uint8_t mode = 0;
  std::uint8_t release = 0;
};

KeymapEnvelope EnvelopeAt(std::span<const std::uint8_t> entry,
                          std::size_t first) {
  return {entry[first], entry[first + kEnvelopeSustain],
          entry[first + kEnvelopeMode], entry[first + kEnvelopeRelease]};
}

// Loads `instrument` with the keymap entry's envelope, leaving the parts the
// channel's update flags override.
void ApplyKeymapEnvelope(VoiceState& voice, std::uint32_t update_flags,
                         const Samples::InstrumentRecord& instrument,
                         const KeymapEnvelope& envelope) {
  voice.sample_start = instrument.sample_start;
  voice.loop_start = instrument.loop_start;
  if ((update_flags & kAttackOverridden) == 0) {
    voice.adsr_lower = static_cast<std::uint16_t>(envelope.attack << kByteBits);
  } else {
    voice.adsr_lower &= kAttackOverrideKeeps;
  }
  voice.adsr_lower |= instrument.adsr_lower & kInstrumentAdsrLowerBits;
  if ((update_flags & kSustainOverridden) == 0) {
    voice.adsr_upper &= kSustainKeeps;
    voice.adsr_upper |=
        static_cast<std::uint16_t>(envelope.sustain << kSustainRateShift);
  } else {
    voice.adsr_upper &= kSustainOverrideKeeps;
  }
  switch (envelope.mode) {
    case kSustainDecreaseLinear:
      voice.adsr_upper |= kSustainDecreaseBit;
      break;
    case kSustainIncreaseExponential:
      voice.adsr_upper |= kSustainExponentialBit;
      break;
    case kSustainDecreaseExponential:
      voice.adsr_upper |= kSustainDecreaseBit | kSustainExponentialBit;
      break;
    default:
      break;
  }
  if ((update_flags & kReleaseOverridden) == 0) {
    voice.adsr_upper &= kReleaseKeeps;
    voice.adsr_upper |= envelope.release;
  }
  voice.adsr_upper |= instrument.adsr_upper & kInstrumentReleaseModeBit;
}
}  // namespace

// decomp: SLUS_010.40 func_8001A1F4 (src/SLUS_010.40/25AC.c:4550)
std::int32_t ResolveSoundEffectInstrument(std::int32_t sample_bank_flags,
                                          std::int32_t instrument_index) {
  if (static_cast<std::uint32_t>(instrument_index) - kEffectBankFirst <
      kEffectBankCount) {
    if ((sample_bank_flags & kEffectBankOneFlag) != 0) {
      return instrument_index + kBankOneOffset;
    }
    if ((sample_bank_flags & kEffectBankTwoFlag) != 0) {
      return instrument_index + kBankTwoOffset;
    }
  }
  return instrument_index;
}

// decomp: SLUS_010.40 func_8001A22C (src/SLUS_010.40/25AC.c:4563)
std::int32_t ResolveMusicInstrument(std::int32_t music_status_flags,
                                    std::uint32_t instrument_index) {
  if ((music_status_flags & kMusicAlternateBankFlag) != 0 &&
      instrument_index >= kMusicAlternateFirst &&
      instrument_index < kMusicAlternateEnd) {
    return static_cast<std::int32_t>(instrument_index) + kBankOneOffset;
  }
  return static_cast<std::int32_t>(instrument_index);
}

// decomp: SLUS_010.40 Sound_CopyInstrumentInfoToChannel
// (src/SLUS_010.40/25AC.c:4953)
void ApplyInstrumentParameters(VoiceState& voice,
                               const Samples::InstrumentRecord& instrument,
                               std::uint32_t sample_start) {
  voice.sample_start = sample_start;
  voice.loop_start = instrument.loop_start;
  voice.adsr_lower = instrument.adsr_lower;
  voice.adsr_upper = instrument.adsr_upper;
  voice.dirty_flags |= kInstrumentDirty;
}

// decomp: SLUS_010.40 func_8001B094 (src/SLUS_010.40/25AC.c:4963)
void SelectChannelInstrument(ChannelState& channel, std::uint16_t index,
                             const Samples::InstrumentRecord& instrument) {
  channel.instrument_index = index;
  ApplyInstrumentParameters(channel.voice, instrument, instrument.sample_start);
}

// decomp: SLUS_010.40 Sound_ClearVoiceFromSchedulerState
// (src/SLUS_010.40/25AC.c:4970)
void ReleaseSoundEffectVoice(VoiceSchedulerState& scheduler,
                             EffectIdentity& effect, std::uint32_t voice_bit) {
  const std::uint32_t keep = ~voice_bit;
  scheduler.active_voices &= keep;
  scheduler.paused_effect_voices &= keep;
  scheduler.noise_voices &= keep;
  scheduler.reverb_voices &= keep;
  scheduler.fm_voices &= keep;
  scheduler.pending_key_on &= keep;
  scheduler.keyed_voices &= keep;
  effect.tags = 0;
  effect.has_effect = false;
}

// decomp: SLUS_010.40 func_8001A4BC (src/SLUS_010.40/25AC.c:4637)
// The original halves the pitch once per octave below the root note; after
// 31 halvings the result no longer changes, so the shift is clamped there.
NotePitch CalculateNotePitch(
    std::span<const std::uint32_t, kSemitoneCount> semitone_pitches,
    const Samples::InstrumentRecord& instrument, const PitchRequest& request) {
  const std::int32_t note = request.note;
  const std::int32_t root = instrument.root_note;
  const std::int32_t interval = Wrap(std::int64_t{note} - root);
  std::int32_t semitone =
      interval % static_cast<std::int32_t>(kSemitonesPerOctave);
  if (semitone < 0) semitone += static_cast<std::int32_t>(kSemitonesPerOctave);
  const std::uint32_t pitch =
      SemitonePitch(semitone_pitches[static_cast<std::size_t>(semitone)],
                    instrument.fine_tune);
  std::int32_t delta = FinePitchDelta(pitch, request.fine_tune)
                           .value_or(request.fine_pitch_delta);
  auto shifted = static_cast<std::int32_t>(pitch);
  if (note < root) {
    const std::int64_t octaves =
        (std::int64_t{root} - note + kSemitonesPerOctave - 1) /
        kSemitonesPerOctave;
    const auto shift = static_cast<std::int32_t>(
        std::min<std::int64_t>(octaves, kMaxArithmeticShift));
    shifted >>= shift;
    delta >>= shift;
  } else {
    const std::int32_t octaves =
        interval / static_cast<std::int32_t>(kSemitonesPerOctave);
    if (octaves != 0) {
      shifted = ShiftLeft(shifted, octaves);
      delta = ShiftLeft(delta, octaves);
    }
  }
  return {static_cast<std::uint16_t>(shifted >> kPitchFractionBits),
          delta >> kPitchFractionBits};
}

namespace {
enum class Scan : std::uint8_t { kNext, kFound, kFailed };

// func_80019FC4's walk over the program: it follows jumps, calls and loops
// without running them, bounded by the sequence data and kLookaheadLimit.
class Lookahead {
 public:
  Lookahead(StepState& state, StepHost& host)
      : state_(state),
        program_(host.Program()),
        tables_(host.Tables()),
        position_(state.program.counter),
        top_(LoopTop(state.program)) {}

  std::optional<std::uint8_t> Run() {
    for (std::uint32_t steps = 0; steps < kLookaheadLimit; ++steps) {
      const Scan scan = Step();
      if (scan == Scan::kFound) return note_;
      if (scan == Scan::kFailed) return std::nullopt;
    }
    return std::nullopt;
  }

 private:
  std::optional<std::uint8_t> Byte(std::int64_t at) const {
    if (at < 0 || at >= static_cast<std::int64_t>(program_.size())) {
      return std::nullopt;
    }
    return program_[static_cast<std::size_t>(at)];
  }

  Scan Found(std::uint32_t note) {
    note_ = static_cast<std::uint8_t>(note);
    return Scan::kFound;
  }

  void EndHeldPlay() {
    state_.channel.play_modes &= static_cast<std::uint16_t>(~kHeldPlay);
  }

  bool OnLoopPass(std::uint8_t pass) const {
    return std::uint32_t{pass} ==
           std::uint32_t{state_.program.loop_iterations[top_]} + 1;
  }

  void PopLoop() {
    top_ = static_cast<std::uint16_t>((top_ - 1) & kLoopDepthMask);
  }

  Scan Step() {
    const auto opcode = Byte(position_);
    if (!opcode) return Scan::kFailed;
    if (*opcode < kFirstReserved) {
      if (*opcode >= kFirstRest) EndHeldPlay();
      return Found(*opcode);
    }
    if (*opcode < kEndOfTrack) return Found(kEndOfTrack);
    const std::size_t index = *opcode - kEndOfTrack;
    if (index >= tables_.opcode_lengths.size()) return Scan::kFailed;
    if (const std::uint8_t length = tables_.opcode_lengths[index];
        length != 0) {
      position_ += length;
      return Scan::kNext;
    }
    return Command(*opcode);
  }

  Scan Command(std::uint8_t opcode) {
    if (opcode >= kFirstTimedNote && opcode < kTimedTie) {
      return Found(kLastNote);
    }
    switch (opcode) {
      case kTimedTie:
        return Found(kFirstTie);
      case kTimedRest:
        return Found(kFirstRest);
      case kExtendedOpcode:
        return Extended();
      case kRepeatLoopOpcode:
        return RepeatLoop();
      case kHeldPlayCommandCb:
      case kHeldPlayCommandCd:
      case kHeldPlayCommandD1:
      case kHeldPlayCommandDb:
        EndHeldPlay();
        ++position_;
        return Scan::kNext;
      case kRepeatLoopForeverOpcode:
        if ((state_.channel.update_flags & kStopAtEndlessLoop) == 0) {
          position_ = state_.program.loop_starts[top_];
          return Scan::kNext;
        }
        break;
      default:
        break;
    }
    EndHeldPlay();
    return Found(kEndOfTrack);
  }

  Scan Extended() {
    ++position_;
    const auto opcode = Byte(position_);
    if (!opcode || *opcode >= tables_.extended_opcode_lengths.size()) {
      return Scan::kFailed;
    }
    if (const std::uint8_t length = tables_.extended_opcode_lengths[*opcode];
        length != 0) {
      position_ += length;
      return Scan::kNext;
    }
    switch (*opcode) {
      case kJumpOnLoopPassOpcode:
      case kExitLoopOnPassOpcode:
        return LoopPass();
      case kJumpRelativeOpcode:
      case kCallRelativeOpcode:
        ++position_;
        return JumpRelative();
      case kJumpAtThresholdOpcode:
        return JumpAtThreshold();
      case kReturnFromCallOpcode:
        position_ = state_.program.return_address;
        return Scan::kNext;
      case kFinishOnReplacementOpcode:
        ++position_;
        if ((state_.layer.status_flags & kFinishOnReplacement) != 0) {
          return Found(kEndOfTrack);
        }
        return Scan::kNext;
      default:
        // The original reads the extended opcode byte again as an opcode.
        return Scan::kNext;
    }
  }

  // Jumps by the signed offset at the position, relative to it.
  Scan JumpRelative() {
    const auto low = Byte(position_);
    const auto high = Byte(position_ + 1);
    if (!low || !high) return Scan::kFailed;
    position_ += static_cast<std::int16_t>(*low | (*high << kByteBits));
    return Scan::kNext;
  }

  Scan LoopPass() {
    ++position_;
    const auto pass = Byte(position_);
    if (!pass) return Scan::kFailed;
    if (!OnLoopPass(*pass)) {
      position_ += kLoopPassOperands;
      return Scan::kNext;
    }
    ++position_;
    PopLoop();
    return JumpRelative();
  }

  Scan JumpAtThreshold() {
    ++position_;
    const auto threshold = Byte(position_);
    if (!threshold) return Scan::kFailed;
    ++position_;
    if (*threshold <= state_.layer.jump_threshold) return JumpRelative();
    position_ += kOffsetBytes;
    return Scan::kNext;
  }

  Scan RepeatLoop() {
    ++position_;
    const auto count = Byte(position_);
    if (!count) return Scan::kFailed;
    if (OnLoopPass(*count)) {
      ++position_;
      PopLoop();
    } else {
      position_ = state_.program.loop_starts[top_];
    }
    return Scan::kNext;
  }

  StepState& state_;
  ProgramData program_;
  const SequencerTables& tables_;
  std::int64_t position_;
  std::uint16_t top_;
  std::uint8_t note_ = 0;
};
}  // namespace

// decomp: SLUS_010.40 func_80019FC4 (src/SLUS_010.40/25AC.c:4430)
std::uint8_t PeekNextNote(StepState& state, StepHost& host) {
  Lookahead lookahead(state, host);
  if (const auto note = lookahead.Run()) return *note;
  host.Refuse(StepRefusal::kLookaheadFailed);
  return kEndOfTrack;
}

namespace {
// The entry the patch keymap scan selects for `note_key`, as a byte offset;
// empty when the scan leaves the keymap or the key needs no change.
struct PatchScan {
  bool refused = false;
  std::optional<std::size_t> entry;
};

std::optional<std::uint8_t> KeymapByte(std::span<const std::uint8_t> keymap,
                                       std::size_t at) {
  if (at >= keymap.size()) return std::nullopt;
  return keymap[at];
}

PatchScan ScanPatchKeymap(std::span<const std::uint8_t> keymap,
                          std::int16_t key, std::uint32_t note_key) {
  const auto current = static_cast<std::uint32_t>(std::int32_t{key});
  const bool upward = current < note_key || key == kUnsetKey;
  if (!upward && note_key >= current) return {};
  // Each pass reads the next entry's mode first, then this entry's high key
  // or the next entry's low key, as the original does.
  for (std::size_t entry = 0;; entry += kKeymapEntryBytes) {
    const auto next_mode = KeymapByte(keymap, entry + kNextPatchMode);
    if (!next_mode) return {true, std::nullopt};
    if (*next_mode == 0) return {false, entry};
    const auto bound =
        KeymapByte(keymap, entry + (upward ? kPatchHighKey : kNextPatchLowKey));
    if (!bound) return {true, std::nullopt};
    const bool beyond = upward ? *bound < note_key : note_key >= *bound;
    if (!beyond) return {false, entry};
  }
}

std::optional<Samples::InstrumentRecord> InstrumentFor(StepHost& host,
                                                       std::int32_t index) {
  auto instrument = host.Instrument(index);
  if (!instrument) host.Refuse(StepRefusal::kInstrumentMissing);
  return instrument;
}
}  // namespace

// decomp: SLUS_010.40 func_8001A258 (src/SLUS_010.40/25AC.c:4571)
void SelectNotePatch(StepState& state, StepHost& host, std::uint32_t note_key) {
  ChannelState& channel = state.channel;
  const std::span<const std::uint8_t> keymap = host.ChannelKeymap();
  const PatchScan scan = ScanPatchKeymap(keymap, channel.key, note_key);
  if (scan.refused) host.Refuse(StepRefusal::kKeymapOutside);
  if (!scan.entry) return;
  const auto entry = keymap.subspan(*scan.entry, kKeymapEntryBytes);
  const std::int32_t index =
      ResolveMusicInstrument(state.layer.status_flags, entry[0]);
  const auto instrument = InstrumentFor(host, index);
  if (!instrument) return;
  channel.instrument_index = static_cast<std::uint16_t>(index);
  ApplyKeymapEnvelope(channel.voice, channel.update_flags, *instrument,
                      EnvelopeAt(entry, kPatchEnvelope));
  channel.voice.volume_scale = entry[kPatchVolumeScale];
}

// decomp: SLUS_010.40 func_8001A64C (src/SLUS_010.40/25AC.c:4686)
std::optional<std::uint16_t> StartDrumNote(StepState& state, StepHost& host,
                                           std::uint32_t channel_bit,
                                           std::uint32_t drum_key) {
  const std::span<const std::uint8_t> keymap = host.DrumKeymap();
  const std::uint64_t first = std::uint64_t{drum_key} * kKeymapEntryBytes;
  if (first > keymap.size() || keymap.size() - first < kKeymapEntryBytes) {
    host.Refuse(StepRefusal::kKeymapOutside);
    return std::nullopt;
  }
  const auto entry =
      keymap.subspan(static_cast<std::size_t>(first), kKeymapEntryBytes);
  MusicLayerState& layer = state.layer;
  const std::int32_t index =
      ResolveMusicInstrument(layer.status_flags, entry[0]);
  const auto instrument = InstrumentFor(host, index);
  if (!instrument) return std::nullopt;
  SetBits(layer.pending_key_on, channel_bit);
  if ((layer.active_notes & channel_bit) != 0) {
    SetBits(layer.pending_key_off, channel_bit);
  }
  ChannelState& channel = state.channel;
  channel.instrument_index = static_cast<std::uint16_t>(index);
  ApplyKeymapEnvelope(channel.voice, channel.update_flags, *instrument,
                      EnvelopeAt(entry, kDrumEnvelope));
  const NotePitch pitch = CalculateNotePitch(
      host.Tables().semitone_pitches, *instrument,
      {entry[kDrumNote], channel.fine_tune, channel.fine_pitch_delta});
  channel.fine_pitch_delta = pitch.fine_pitch_delta;
  channel.voice.volume_scale = entry[kDrumVolumeScale];
  const std::uint32_t pan_reverb = entry[kDrumPanReverb];
  channel.pan = static_cast<std::uint16_t>(
      ((pan_reverb & kDrumPanMask) + kDrumPanCentre) << kByteBits);
  if ((pan_reverb & kDrumReverb) != 0) {
    layer.reverb_channels |= channel_bit;
  } else {
    layer.reverb_channels &= ~channel_bit;
  }
  state.driver_update_flags |= kVoiceModeUpdate;
  return pitch.pitch;
}

namespace {
constexpr std::uint32_t kSlideFractionBits = 16;

std::optional<std::uint32_t> Refused(StepHost& host, StepRefusal refusal) {
  host.Refuse(refusal);
  return std::nullopt;
}

// The step a command ran as, or empty when it did not run.
std::optional<std::uint32_t> Ran(StepHost& host, CommandOutcome outcome,
                                 std::uint32_t step) {
  if (outcome == CommandOutcome::kMissing) {
    return Refused(host, StepRefusal::kCommandMissing);
  }
  if (outcome == CommandOutcome::kRefused) {
    return Refused(host, StepRefusal::kCommandRefused);
  }
  return step;
}

// The operand byte after the opcode at `at`; when the data ends first the
// counter stays at the opcode.
std::optional<std::uint8_t> OperandAfter(ProgramState& program,
                                         ProgramData data, std::uint32_t at) {
  const std::uint64_t operand = std::uint64_t{at} + 1;
  if (operand >= data.size()) {
    program.counter = at;
    return std::nullopt;
  }
  return data[static_cast<std::size_t>(operand)];
}

std::optional<std::uint32_t> RunExtended(StepState& state, StepHost& host,
                                         ProgramData data, std::uint32_t at,
                                         std::uint32_t channel_bit) {
  ProgramState& program = state.program;
  const auto opcode = OperandAfter(program, data, at);
  if (!opcode) return Refused(host, StepRefusal::kOperandsOutside);
  if (*opcode >= kExtendedOpcodeCount) {
    program.counter = at;
    return Refused(host, StepRefusal::kUnknownExtendedOpcode);
  }
  program.counter = at + 2;
  return Ran(host, host.RunExtendedCommand(state, *opcode, channel_bit),
             kExtendedOpcode);
}

// One pass of func_8001A8D8's command loop: the note, tie or rest step
// (below 0xA0), the track end (0xA0) or the command it ran.
std::optional<std::uint32_t> RunNextCommand(StepState& state, StepHost& host,
                                            std::uint32_t channel_bit) {
  ProgramState& program = state.program;
  const ProgramData data = host.Program();
  const std::uint32_t at = program.counter;
  if (at >= data.size()) return Refused(host, StepRefusal::kOpcodeOutside);
  std::uint32_t step = data[at];
  program.counter = at + 1;
  if (step < kEndOfTrack) return step;
  if (step == kExtendedOpcode) {
    return RunExtended(state, host, data, at, channel_bit);
  }
  if (const std::uint32_t timed = step - kFirstTimedNote;
      timed < kTimedNoteCount) {
    const auto length = OperandAfter(program, data, at);
    if (!length) return Refused(host, StepRefusal::kOperandsOutside);
    state.channel.note_length = *length;
    program.counter = at + 2;
    return timed * kNoteLengthSlots;
  }
  if (step == kStopTrack) {
    step = kEndOfTrack;
  } else if (step == kRepeatLoopForeverOpcode &&
             (state.channel.update_flags & kStopAtEndlessLoop) != 0) {
    step = kEndOfTrack;
    state.scheduler.pending_key_off |= channel_bit;
  }
  return Ran(
      host,
      host.RunCommand(state, static_cast<std::uint8_t>(step), channel_bit),
      step);
}

// A refused step finishes the channel as the track end does.
std::uint32_t FinishRefusedChannel(StepState& state, StepHost& host,
                                   std::uint32_t channel_bit) {
  static_cast<void>(host.RunCommand(state, kEndOfTrack, channel_bit));
  return kEndOfTrack;
}

std::uint32_t RunCommands(StepState& state, StepHost& host,
                          std::uint32_t channel_bit) {
  for (std::uint32_t count = 0; count < kCommandLimit; ++count) {
    const auto step = RunNextCommand(state, host, channel_bit);
    if (!step) return FinishRefusedChannel(state, host, channel_bit);
    ++state.program.step_counter;
    if (*step <= kEndOfTrack) return *step;
  }
  host.Refuse(StepRefusal::kCommandLimit);
  return FinishRefusedChannel(state, host, channel_bit);
}

void SetNoteLengths(StepState& state, const SequencerTables& tables,
                    std::uint32_t step, std::uint8_t next) {
  ChannelState& channel = state.channel;
  const bool held = (channel.play_modes & kHeldPlay) != 0;
  if (channel.fixed_note_length != 0) {
    channel.gate_length = static_cast<std::uint16_t>(channel.fixed_note_length);
    channel.note_length = channel.gate_length;
  }
  if (channel.note_length != 0) {
    if (next >= kFirstRest || (next < kFirstTie && !held)) {
      channel.gate_length -= kGateRelease;
    }
  } else {
    channel.note_length = tables.note_lengths[step % kNoteLengthSlots];
    channel.gate_length = channel.note_length;
    if (std::uint32_t{next} - kFirstTie >= kNoteLengthSlots && !held) {
      channel.gate_length -= kGateRelease;
    }
  }
  if (IsMusic(state) && (channel.update_flags & kFullNoteLengthActive) != 0) {
    channel.gate_length = channel.note_length;
  }
  channel.stored_note_length = static_cast<std::int16_t>(channel.note_length);
  channel.voice.dirty_flags |= kReleaseRateDirty;
}

void StartRest(StepState& state, std::uint32_t channel_bit) {
  ChannelState& channel = state.channel;
  if (IsMusic(state)) {
    state.layer.active_notes &= ~channel_bit;
    if (state.assigned_voice < kUnassignedVoice) {
      SetBits(state.layer.pending_key_off, channel_bit);
    }
  }
  channel.portamento_steps = 0;
  channel.vibrato_pitch = 0;
  channel.tremolo_volume = 0;
  channel.play_modes &= static_cast<std::uint16_t>(~kTiedPlay);
}

void RemapNote(StepState& state, StepHost& host, std::uint32_t& note) {
  const std::span<const std::int8_t> tables = host.Tables().note_remap_tables;
  const auto offset = state.layer.note_remap_offset;
  const std::size_t at =
      offset ? std::size_t{*offset} + (note % kSemitonesPerOctave)
             : tables.size();
  if (at >= tables.size()) {
    host.Refuse(StepRefusal::kNoteRemapMissing);
    return;
  }
  note += static_cast<std::uint32_t>(std::int32_t{tables[at]});
}

void KeyMusicNote(StepState& state, StepHost& host, std::uint32_t channel_bit,
                  std::uint32_t& note) {
  ChannelState& channel = state.channel;
  MusicLayerState& layer = state.layer;
  if ((state.mix_behavior & kNoteRemapActive) != 0) {
    RemapNote(state, host, note);
  }
  if ((channel.update_flags & kPatchKeymapActive) != 0) {
    SelectNotePatch(state, host, note);
  }
  SetBits(layer.pending_key_on, channel_bit);
  if ((layer.active_notes & channel_bit) != 0 &&
      state.assigned_voice < kUnassignedVoice) {
    SetBits(layer.pending_key_off, channel_bit);
  }
  if (channel.key_on_volume_slide_length != 0) {
    channel.volume_slide_length = channel.key_on_volume_slide_length;
    channel.volume = channel.key_on_volume;
    channel.volume_slide_step = channel.key_on_volume_slide_step;
  }
}

// Sets the channel's key for `note` and returns the note it plays: the
// portamento start when a portamento is set up, else the transposed note.
std::uint32_t PortamentoNote(ChannelState& channel, std::uint32_t note) {
  const auto transpose =
      static_cast<std::uint32_t>(std::int32_t{channel.transpose});
  if (channel.portamento_steps != 0 && channel.portamento_key != 0) {
    const auto start_transpose =
        static_cast<std::uint32_t>(std::int32_t{channel.portamento_transpose});
    channel.pitch_bend_slide_length = channel.portamento_steps;
    channel.pitch_bend_target = static_cast<std::int16_t>(
        transpose + note - channel.portamento_key - start_transpose);
    channel.key = static_cast<std::int16_t>(
        channel.portamento_key -
        (channel.transpose - channel.portamento_transpose));
    return channel.portamento_key + start_transpose;
  }
  channel.key = static_cast<std::int16_t>(note);
  return note + transpose;
}

NotePitch PitchOf(StepHost& host, std::uint16_t instrument_index,
                  const PitchRequest& request) {
  const auto instrument = InstrumentFor(host, instrument_index);
  if (!instrument) return {0, request.fine_pitch_delta};
  return CalculateNotePitch(host.Tables().semitone_pitches, *instrument,
                            request);
}

std::uint32_t ApplyRandomPitch(StepState& state, StepHost& host,
                               std::uint32_t pitch) {
  const std::uint32_t depth = state.channel.random_pitch_depth;
  if (depth == 0) return pitch;
  const auto offsets = host.Tables().random_pitch_offsets;
  const auto phase = static_cast<std::uint32_t>(state.random_pitch_phase);
  if (phase >= offsets.size()) {
    host.Refuse(StepRefusal::kRandomPitchPhaseOutside);
    return pitch;
  }
  const std::uint32_t offset = offsets[phase];
  const std::uint32_t scaled = ((pitch * depth) >> kPitchFractionBits) * offset;
  if ((offset & kNegativeRandomOffset) != 0) {
    return pitch - (scaled >> kRandomLowerShift);
  }
  return pitch + (scaled >> kRandomRaiseShift);
}

std::uint32_t StartMelodicNote(StepState& state, StepHost& host,
                               std::uint32_t channel_bit, std::uint32_t note) {
  ChannelState& channel = state.channel;
  if ((channel.play_modes & kTiedPlay) == 0) {
    if (IsMusic(state)) {
      KeyMusicNote(state, host, channel_bit, note);
    } else {
      state.scheduler.pending_key_on |= channel_bit;
    }
    state.pitch_slide.ticks_left = 0;
  }
  const std::uint32_t played = PortamentoNote(channel, note);
  const NotePitch pitch =
      PitchOf(host, channel.instrument_index,
              {static_cast<std::int32_t>(played), channel.fine_tune,
               channel.fine_pitch_delta});
  channel.fine_pitch_delta = pitch.fine_pitch_delta;
  return ApplyRandomPitch(state, host, pitch.pitch);
}

std::uint16_t VibratoBase(std::uint16_t depth, std::uint32_t pitch) {
  const std::uint32_t magnitude = (depth & kVibratoMagnitudeMask) >> kByteBits;
  const std::uint32_t scaled =
      (depth & kVibratoFullScale) == 0
          ? magnitude * ((pitch * kVibratoReducedScale) >> kByteBits)
          : magnitude * pitch;
  return static_cast<std::uint16_t>(scaled >> kPitchScaleShift);
}

bool WaveformMissing(StepHost& host) {
  host.Refuse(StepRefusal::kWaveformMissing);
  return false;
}

// Restarts the vibrato and tremolo for a new note; false when a waveform is
// missing, which ends the step as the port's waveform check always has.
bool StartModulation(StepState& state, StepHost& host, std::uint32_t pitch) {
  ChannelState& channel = state.channel;
  const std::uint32_t flags = channel.update_flags;
  const bool retrigger = (channel.play_modes & kTiedPlay) == 0;
  if ((flags & kVibratoActive) != 0) {
    channel.vibrato_base = VibratoBase(channel.vibrato_depth, pitch);
    if (retrigger) {
      if (!host.HasLfoWaveform(channel.vibrato_shape)) {
        return WaveformMissing(host);
      }
      channel.vibrato_wave = static_cast<std::uint8_t>(channel.vibrato_shape);
      channel.vibrato_delay_left =
          static_cast<std::uint16_t>(channel.vibrato_delay);
      channel.vibrato_rate_counter = 1;
    }
  }
  if ((flags & kTremoloActive) != 0 && retrigger) {
    if (!host.HasLfoWaveform(channel.tremolo_shape)) {
      return WaveformMissing(host);
    }
    channel.tremolo_wave = static_cast<std::uint8_t>(channel.tremolo_shape);
    channel.tremolo_delay_left =
        static_cast<std::uint16_t>(channel.tremolo_delay);
    channel.tremolo_rate_counter = 1;
  }
  return true;
}

// Starts note `step` (below 0x84); false when it was refused, which ends the
// step before the tie handling.
bool StartNote(StepState& state, StepHost& host, std::uint32_t channel_bit,
               std::uint32_t step) {
  ChannelState& channel = state.channel;
  const std::uint32_t note =
      (step / kNoteLengthSlots) +
      (std::uint32_t{channel.octave} * kSemitonesPerOctave);
  std::uint32_t pitch = 0;
  if ((channel.update_flags & kDrumKeymapActive) != 0) {
    const auto drum = StartDrumNote(state, host, channel_bit, note);
    if (!drum) return false;
    pitch = *drum;
  } else {
    pitch = StartMelodicNote(state, host, channel_bit, note);
  }
  channel.pitch_base = static_cast<std::int32_t>(pitch);
  if (IsMusic(state)) {
    state.layer.active_notes |= channel_bit;
  } else {
    state.scheduler.keyed_voices |= channel_bit;
  }
  channel.voice.dirty_flags |= kVolumeDirty | kSampleRateDirty;
  if (!StartModulation(state, host, pitch)) return false;
  channel.vibrato_pitch = 0;
  channel.tremolo_volume = 0;
  state.pitch_slide.slide = 0;
  return true;
}

void StartPitchBend(StepState& state, StepHost& host) {
  ChannelState& channel = state.channel;
  PitchSlideState& slide = state.pitch_slide;
  channel.key =
      static_cast<std::int16_t>(channel.key + channel.pitch_bend_target);
  const NotePitch target =
      PitchOf(host, channel.instrument_index,
              {channel.key + channel.transpose, channel.fine_tune, 0});
  const std::uint32_t target_rate = std::uint32_t{target.pitch}
                                    << kSlideFractionBits;
  slide.ticks_left = channel.pitch_bend_slide_length;
  channel.pitch_bend_target = 0;
  if (slide.ticks_left == 0) {
    host.Refuse(StepRefusal::kPitchBendWithoutLength);
    slide.step = 0;
    return;
  }
  const auto distance = static_cast<std::int32_t>(
      target_rate -
      (static_cast<std::uint32_t>(channel.pitch_base) << kSlideFractionBits) -
      static_cast<std::uint32_t>(slide.slide));
  slide.step = distance / std::int32_t{slide.ticks_left};
}

void FinishNote(StepState& state, StepHost& host) {
  ChannelState& channel = state.channel;
  const std::uint16_t modes = channel.play_modes;
  channel.play_modes = static_cast<std::uint16_t>((modes & ~kTiedPlay) |
                                                  ((modes & kLegatoPlay) * 2));
  if (channel.pitch_bend_target != 0) StartPitchBend(state, host);
  channel.portamento_key = static_cast<std::uint16_t>(channel.key);
  channel.portamento_transpose = channel.transpose;
}
}  // namespace

// decomp: SLUS_010.40 func_8001A8D8 (src/SLUS_010.40/25AC.c:4760)
void ExecuteChannelStep(StepState& state, StepHost& host,
                        std::uint32_t channel_bit) {
  const std::uint32_t step = RunCommands(state, host, channel_bit);
  if (step == kEndOfTrack) {
    if (IsMusic(state)) SetBits(state.layer.pending_key_off, channel_bit);
    return;
  }
  const std::uint8_t next = PeekNextNote(state, host);
  SetNoteLengths(state, host.Tables(), step, next);
  if (step >= kFirstRest) {
    StartRest(state, channel_bit);
    return;
  }
  if (step < kFirstTie && !StartNote(state, host, channel_bit, step)) return;
  FinishNote(state, host);
}

namespace {
constexpr std::int32_t kPhaseMask = 0xFF;
constexpr std::uint32_t kPitchScaleLevel = 0xFF0000;
constexpr std::uint32_t kMasterVolumeLevel = 0x7F0000;
constexpr std::uint32_t kEffectLevel = 0xFF00;
constexpr std::uint32_t kFadeLevel = 0xFFFF0000;
constexpr std::uint32_t kTickCarry = 0xFFFF0000;
constexpr std::uint32_t kTickFraction = 0xFFFF;
constexpr std::uint32_t kLevelShift = 16;
constexpr std::uint32_t kTempoScaleHalf = 0x80;
constexpr std::uint32_t kEffectBitFirst = 1U << kFirstEffectVoice;
constexpr std::uint32_t kEffectVoiceBits = ~(kEffectBitFirst - 1);
// The driver ticks the slides and the saved-music fade every fourth tick.
constexpr std::uint16_t kSlideTickMask = 3;
// GetRCnt(RCntCNT2) counts at 1/8 of the system clock and wraps here.
constexpr std::int32_t kTimerPeriod = 0x44E8;

// Cutscene stream control flags: the stream pauses every sound effect
// without kStreamEffectTag, and forces a sequencer tick every driver tick.
constexpr std::uint32_t kStreamPausesEffects = 2;
constexpr std::uint32_t kStreamForcesTick = 4;
constexpr std::int32_t kStreamEffectTag = 0x02000000;

// Driver control latches of a music transition between the active and
// secondary layers: which layer is silenced, and each layer's resume
// waiting for the next timer boundary (pending) or taken there (armed).
constexpr std::uint32_t kSecondarySilenced = 0x100;
constexpr std::uint32_t kActiveResumeArmed = 0x200;
constexpr std::uint32_t kActiveResumePending = 0x400;
constexpr std::uint32_t kActiveSilenced = 0x1000;
constexpr std::uint32_t kSecondaryResumeArmed = 0x2000;
constexpr std::uint32_t kSecondaryResumePending = 0x4000;
constexpr std::uint32_t kActiveResumes = 0x600;
constexpr std::uint32_t kSecondaryResumes = 0x6000;
constexpr auto kTransitionWaiting = kActiveResumes | kSecondaryResumes;
static_assert(kActiveResumes == (kActiveResumeArmed | kActiveResumePending));
static_assert(kSecondaryResumes ==
              (kSecondaryResumeArmed | kSecondaryResumePending));

std::int32_t Add(std::int32_t value, std::int32_t step) {
  return Wrap(std::int64_t{value} + step);
}

bool LevelChanged(std::int32_t next, std::int32_t current,
                  std::uint32_t level) {
  return (static_cast<std::uint32_t>(next) & level) !=
         (static_cast<std::uint32_t>(current) & level);
}

bool StreamForcesTick(const PlaybackState& state) {
  return (static_cast<std::uint32_t>(state.stream_control_flags) &
          kStreamForcesTick) != 0;
}

MusicLayerState* LayerOf(PlaybackState& state, ChannelGroup group) {
  if (group != ChannelGroup::kSecondaryMusic) return &state.active;
  return state.secondary ? &*state.secondary : nullptr;
}

void SlideLevel(LevelSlide& slide) {
  --slide.length;
  slide.value = Add(slide.value, slide.step);
}

void SlideMasterVolume(PlaybackState& state, PlaybackHost& host,
                       ChannelGroup group) {
  MusicLayerState* layer = LayerOf(state, group);
  if (layer == nullptr || layer->active_channels == 0 ||
      layer->master_volume_slide_length == 0) {
    return;
  }
  --layer->master_volume_slide_length;
  const std::int32_t next =
      Add(layer->master_volume, layer->master_volume_step);
  if (LevelChanged(next, layer->master_volume, kMasterVolumeLevel)) {
    host.InvalidateMusicVolumes(state, group);
    layer = LayerOf(state, group);
    if (layer == nullptr) return;
  }
  layer->master_volume = next;
}

void SlideEffect(EffectSlides& slides) {
  if (slides.volume_length != 0) {
    --slides.volume_length;
    const std::int32_t next = slides.volume + slides.volume_step;
    if (LevelChanged(next, slides.volume, kEffectLevel)) {
      slides.dirty_flags |= kVolumeDirty;
    }
    slides.volume = static_cast<std::int16_t>(next);
  }
  if (slides.pan_length != 0) {
    --slides.pan_length;
    const std::int32_t next = slides.pan + slides.pan_step;
    if (LevelChanged(next, slides.pan, kEffectLevel)) {
      slides.dirty_flags |= kVolumeDirty;
    }
    slides.pan = static_cast<std::uint16_t>(next);
  }
  if (slides.pitch_scale_length != 0) {
    --slides.pitch_scale_length;
    const auto next = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(slides.pitch_scale) +
        slides.pitch_scale_step);
    if (LevelChanged(next, slides.pitch_scale, kEffectLevel)) {
      slides.dirty_flags |= kSampleRateDirty;
    }
    slides.pitch_scale = next;
  }
}

// The sound-effect voices that name a sound-effect channel; the original
// walks the others forever.
std::uint32_t EffectChannelVoices(std::uint32_t voices, PlaybackHost& host) {
  if ((voices & ~kEffectVoiceBits) != 0) {
    host.Refuse(PlaybackRefusal::kEffectVoiceOutsideChannels);
  }
  return voices & kEffectVoiceBits;
}

void SlideEffects(PlaybackState& state, PlaybackHost& host) {
  if (state.scheduler.active_voices == 0) return;
  const std::uint32_t voices =
      EffectChannelVoices(state.scheduler.active_voices, host);
  for (std::size_t index = 0; index < kEffectChannelCount; ++index) {
    if ((voices & (kEffectBitFirst << index)) == 0) continue;
    EffectSlides slides = host.EffectSlidesOf(index);
    SlideEffect(slides);
    host.SetEffectSlides(index, slides);
  }
}
}  // namespace

// decomp: SLUS_010.40 func_8001924C (src/SLUS_010.40/25AC.c:4024)
void UpdateGlobalSlides(PlaybackState& state, PlaybackHost& host) {
  state.random_pitch_phase = Add(state.random_pitch_phase, 1) & kPhaseMask;
  if (state.disc_volume.length != 0) {
    SlideLevel(state.disc_volume);
    host.WriteDiscVolume(state);
  }
  host.UpdateCutsceneStream(state);
  if (state.tempo_scale.length != 0) SlideLevel(state.tempo_scale);
  if (LevelSlide& pitch = state.music_pitch_scale; pitch.length != 0) {
    --pitch.length;
    const std::int32_t next = Add(pitch.value, pitch.step);
    if (LevelChanged(next, pitch.value, kPitchScaleLevel)) {
      host.MarkMusicPitchDirty();
    }
    state.music_pitch_scale.value = next;
  }
  SlideMasterVolume(state, host, ChannelGroup::kMusic);
  SlideMasterVolume(state, host, ChannelGroup::kSecondaryMusic);
  SlideEffects(state, host);
}

// decomp: SLUS_010.40 func_80019614 (src/SLUS_010.40/25AC.c:4138)
void AdvanceMusicTransition(PlaybackState& state, PlaybackHost& host) {
  std::uint32_t& latches = state.control_latches;
  if ((latches & kTransitionWaiting) == 0) return;
  MusicLayerState& active = state.active;
  state.music_transition_timer = static_cast<std::int32_t>(
      std::uint32_t{active.timer_lower_count} +
      (std::uint32_t{active.timer_upper_count} << kLevelShift));
  if (state.music_transition_timer != 0) return;
  if ((latches & kSecondarySilenced) != 0) {
    if ((latches & kSecondaryResumePending) != 0) {
      latches = (latches & ~kSecondaryResumePending) | kSecondaryResumeArmed;
      return;
    }
    latches = (latches & ~(kSecondaryResumeArmed | kSecondarySilenced)) |
              kActiveSilenced;
    SetBits(active.pending_key_off,
            static_cast<std::uint32_t>(active.active_channels &
                                       ~active.push_release_mask));
    active.active_notes &= static_cast<std::uint32_t>(active.push_release_mask);
    return;
  }
  if ((latches & kActiveResumePending) != 0) {
    latches = (latches & ~kActiveResumePending) | kActiveResumeArmed;
    return;
  }
  latches =
      (latches & ~(kActiveSilenced | kActiveResumeArmed)) | kSecondarySilenced;
  if (!state.secondary) {
    host.Refuse(PlaybackRefusal::kSecondaryLayerMissing);
    return;
  }
  MusicLayerState& secondary = *state.secondary;
  secondary.active_notes = 0;
  SetBits(secondary.pending_key_off,
          static_cast<std::uint32_t>(secondary.active_channels));
}

namespace {
std::uint32_t TempoIncrement(const MusicLayerState& layer,
                             std::int32_t tempo_scale) {
  std::uint32_t increment =
      (static_cast<std::uint32_t>(layer.tempo) >> kLevelShift) & kTickFraction;
  const std::uint32_t scale =
      (static_cast<std::uint32_t>(tempo_scale) >> kLevelShift) & kByteMask;
  if (scale == 0) return increment;
  if (scale < kTempoScaleHalf) {
    increment += (increment * scale) >> kPitchScaleShift;
  } else {
    increment = (increment * scale) >> kPitchFractionBits;
  }
  return increment;
}

// Counts down one channel's note and gate, stepping the channel when its
// note ends and releasing its voice when its gate does.
void CountDownChannel(PlaybackState& state, PlaybackHost& host,
                      ChannelGroup group, std::size_t index) {
  const bool effect = group == ChannelGroup::kSoundEffects;
  const std::uint32_t bit = effect ? kEffectBitFirst << index : 1U << index;
  ChannelTimers timers = host.Timers(group, index);
  if (effect) timers.priority = Add(timers.priority, 1);
  --timers.note_length;
  --timers.gate_length;
  host.SetTimers(group, index, timers);
  if (timers.note_length == 0) {
    host.StepChannel(state, group, index);
  } else if (timers.gate_length == 0) {
    if (effect) {
      state.scheduler.pending_key_off |= bit;
      state.scheduler.keyed_voices &= ~bit;
    } else if (MusicLayerState* layer = LayerOf(state, group)) {
      SetBits(layer->pending_key_off, bit);
    }
  }
  host.UpdateChannelSlides(state, group, index);
}

void StepMusicChannels(PlaybackState& state, PlaybackHost& host,
                       ChannelGroup group) {
  const MusicLayerState* layer = LayerOf(state, group);
  if (layer == nullptr) return;
  const auto channels = static_cast<std::uint32_t>(layer->active_channels);
  for (std::size_t index = 0; index < kMusicChannelCount; ++index) {
    if ((channels & (1U << index)) != 0) {
      CountDownChannel(state, host, group, index);
    }
  }
}

// The layer's beat timer: lower ticks per upper count, upper counts per top
// count. A top count ends one requested loop of the track.
void AdvanceMusicTimer(PlaybackState& state, PlaybackHost& host,
                       ChannelGroup group) {
  MusicLayerState* layer = LayerOf(state, group);
  if (layer == nullptr || layer->timer_lower == 0) return;
  if (++layer->timer_lower_count != layer->timer_lower) return;
  layer->timer_lower_count = 0;
  if (++layer->timer_upper_count != layer->timer_upper) return;
  layer->timer_upper_count = 0;
  ++layer->timer_top;
  const bool secondary = group == ChannelGroup::kSecondaryMusic;
  if (!secondary && state.music_loops_remaining != 0) {
    --state.music_loops_remaining;
  }
  if (!secondary && state.active_is_primary) {
    AdvanceMusicTransition(state, host);
  }
}

void RunMusicTick(PlaybackState& state, PlaybackHost& host,
                  ChannelGroup group) {
  StepMusicChannels(state, host, group);
  MusicLayerState* layer = LayerOf(state, group);
  if (layer == nullptr) return;
  if (layer->tempo_slide_length != 0) {
    --layer->tempo_slide_length;
    layer->tempo = Add(layer->tempo, layer->tempo_slide_step);
  }
  if (layer->reverb_depth_slide_length != 0) {
    --layer->reverb_depth_slide_length;
    layer->reverb_depth =
        Add(layer->reverb_depth, layer->reverb_depth_slide_step);
    if (group != ChannelGroup::kSecondaryMusic) {
      state.driver_update_flags |= kReverbDepthUpdate;
    }
  }
  AdvanceMusicTimer(state, host, group);
}
}  // namespace

// decomp: SLUS_010.40 func_80019704 (src/SLUS_010.40/25AC.c:4174)
// While loops of the track remain to skip, the primary layer runs tick after
// tick in one call; kFastForwardLimit bounds a skip the timer never ends.
std::int32_t UpdateMusicSequence(PlaybackState& state, PlaybackHost& host,
                                 ChannelGroup group) {
  MusicLayerState* layer = LayerOf(state, group);
  if (layer == nullptr) {
    host.Refuse(PlaybackRefusal::kSecondaryLayerMissing);
    return 0;
  }
  layer->tick_accumulator += TempoIncrement(*layer, state.tempo_scale.value);
  if ((layer->tick_accumulator & kTickCarry) != 0 || StreamForcesTick(state)) {
    layer->tick_accumulator &= kTickFraction;
    const bool fast_forward = group != ChannelGroup::kSecondaryMusic;
    std::uint32_t passes = 0;
    do {
      if (passes++ == kFastForwardLimit) {
        host.Refuse(PlaybackRefusal::kLoopFastForwardLimit);
        break;
      }
      RunMusicTick(state, host, group);
    } while (fast_forward && state.music_loops_remaining != 0);
  }
  layer = LayerOf(state, group);
  return layer == nullptr ? 0 : layer->active_channels;
}

// decomp: SLUS_010.40 func_800199C4 (src/SLUS_010.40/25AC.c:4264)
void UpdateSavedMusicFade(PlaybackState& state, PlaybackHost& host,
                          ChannelGroup group) {
  FadeTimer& fade = state.master_fade;
  if (fade.ticks_remaining == 0) return;
  MusicLayerState* layer = LayerOf(state, group);
  if (layer == nullptr) {
    host.Refuse(PlaybackRefusal::kSecondaryLayerMissing);
    return;
  }
  fade.ticks_remaining = Add(fade.ticks_remaining, -1);
  if (fade.ticks_remaining == 0) {
    const std::int32_t release = layer->active_channels;
    layer->music_id = 0;
    layer->active_channels = 0;
    layer->pending_key_on = 0;
    layer->active_notes = 0;
    layer->pending_key_off = release;
    return;
  }
  const std::int32_t next = Add(fade.value, fade.step);
  if (LevelChanged(next, fade.value, kFadeLevel)) {
    host.InvalidateMusicVolumes(state, group);
  }
  state.master_fade.value = next;
}

namespace {
void ApplyTransitionLatches(PlaybackState& state, PlaybackHost& host) {
  const std::uint32_t latches = state.control_latches;
  if ((latches & (kSecondarySilenced | kActiveSilenced)) == 0) return;
  MusicLayerState* transition = nullptr;
  if ((latches & kSecondarySilenced) != 0) {
    if (!state.secondary) {
      host.Refuse(PlaybackRefusal::kSecondaryLayerMissing);
      return;
    }
    MusicLayerState& secondary = *state.secondary;
    const std::uint32_t keep =
        (latches & kSecondaryResumes) != 0 ? secondary.resume_key_on_mask : 0;
    secondary.pending_key_on &= static_cast<std::int32_t>(keep);
    secondary.active_notes &= keep;
    transition = &state.active;
  } else {
    MusicLayerState& active = state.active;
    const auto push = static_cast<std::uint32_t>(active.push_release_mask);
    const std::uint32_t keep = (latches & kActiveResumes) != 0
                                   ? active.resume_key_on_mask | push
                                   : push;
    active.pending_key_on &= static_cast<std::int32_t>(keep);
    active.active_notes &= keep;
    if (!state.secondary) {
      host.Refuse(PlaybackRefusal::kSecondaryLayerMissing);
      return;
    }
    transition = &*state.secondary;
  }
  const std::uint32_t resumed = ~transition->resume_key_on_mask;
  transition->pending_key_on &= static_cast<std::int32_t>(resumed);
  transition->active_notes &= resumed;
}

void SettleSecondaryLayer(PlaybackState& state, PlaybackHost& host) {
  if (!state.secondary) return;
  if (state.secondary->active_channels == 0) {
    host.DetachSecondaryLayer(state);
  } else if ((static_cast<std::uint32_t>(state.active.active_channels) |
              state.active.paused_channels) == 0) {
    host.PromoteSecondaryLayer(state);
  }
}

void StartPendingMusic(PlaybackState& state, PlaybackHost& host) {
  host.StartPendingMusic(state);
  for (std::size_t index = 0; index < kMusicChannelCount; ++index) {
    ChannelTimers timers = host.Timers(ChannelGroup::kMusic, index);
    timers.note_length = 1;
    host.SetTimers(ChannelGroup::kMusic, index, timers);
  }
}

void UpdateSecondaryLayer(PlaybackState& state, PlaybackHost& host) {
  if (!state.secondary) return;
  if (state.secondary->active_channels != 0) {
    UpdateMusicSequence(state, host, ChannelGroup::kSecondaryMusic);
    host.ActivatePrimaryLayer(state);
  }
  if ((state.mix_behavior & kSavedMusicFadeActive) == 0 ||
      (state.tick_counter & kSlideTickMask) != 0 || !state.secondary) {
    return;
  }
  UpdateSavedMusicFade(state, host, ChannelGroup::kSecondaryMusic);
  if (state.secondary && state.secondary->active_channels == 0) {
    state.mix_behavior &= ~kSavedMusicFadeActive;
  }
}

void UpdateEffectChannels(PlaybackState& state, PlaybackHost& host) {
  VoiceSchedulerState& scheduler = state.scheduler;
  if (scheduler.active_voices == 0) return;
  const std::uint32_t active = scheduler.active_voices;
  scheduler.tempo_accumulator +=
      (scheduler.effect_tempo >> kLevelShift) & kTickFraction;
  if ((scheduler.tempo_accumulator & kTickCarry) == 0 &&
      !StreamForcesTick(state)) {
    return;
  }
  scheduler.tempo_accumulator &= kTickFraction;
  const std::uint32_t voices = EffectChannelVoices(active, host);
  const bool paused = (static_cast<std::uint32_t>(state.stream_control_flags) &
                       kStreamPausesEffects) != 0;
  for (std::size_t index = 0; index < kEffectChannelCount; ++index) {
    if ((voices & (kEffectBitFirst << index)) == 0) continue;
    if (paused && (host.Timers(ChannelGroup::kSoundEffects, index).effect_tags &
                   kStreamEffectTag) == 0) {
      continue;
    }
    CountDownChannel(state, host, ChannelGroup::kSoundEffects, index);
  }
}

std::int32_t FinishTick(PlaybackState& state, PlaybackHost& host,
                        std::int32_t start) {
  std::int32_t cost = Wrap(std::int64_t{host.ReadTimer()} - start);
  if (cost <= 0) cost = Add(cost, kTimerPeriod);
  auto& costs = state.recent_tick_costs;
  std::shift_left(costs.begin(), costs.end(), 1);
  costs.back() = cost;
  std::int64_t total = 0;
  for (const std::int32_t recent : costs) total += recent;
  state.recent_tick_cost_total = Wrap(total);
  host.FinishTick(state);
  return cost;
}
}  // namespace

// decomp: SLUS_010.40 func_80019A58 (src/SLUS_010.40/25AC.c:4287)
std::int32_t TickPlayback(PlaybackState& state, PlaybackHost& host) {
  const std::int32_t start = host.ReadTimer();
  ++state.tick_counter;
  ApplyTransitionLatches(state, host);
  if (state.active.pending_key_off != 0 ||
      state.scheduler.pending_key_off != 0 ||
      (state.secondary && state.secondary->pending_key_off != 0)) {
    host.ReleasePendingVoices(state);
  }
  SettleSecondaryLayer(state, host);
  if (state.music_pending && state.active.active_channels == 0) {
    StartPendingMusic(state, host);
  }
  if ((state.driver_update_flags | state.active.active_notes |
       state.scheduler.keyed_voices) != 0 ||
      (state.secondary && state.secondary->active_notes != 0)) {
    host.CommitVoices(state);
  }
  if (state.active.active_channels != 0) {
    UpdateMusicSequence(state, host, ChannelGroup::kMusic);
  }
  UpdateSecondaryLayer(state, host);
  UpdateEffectChannels(state, host);
  if ((state.tick_counter & kSlideTickMask) == 0) {
    UpdateGlobalSlides(state, host);
  }
  return FinishTick(state, host, start);
}
}  // namespace VS::Audio::Seq
