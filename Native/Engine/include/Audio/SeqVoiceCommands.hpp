#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Audio/PlaybackReset.hpp"
#include "Audio/VoiceOutput.hpp"

// The sequencer's channel, envelope and modulation commands (the SoundVM
// opcode handlers, SLUS_010.40/25AC.c:5125-6227) over a typed copy of one
// channel's state. A command reads its operands from a bounded span: it
// consumes nothing and changes nothing when the span is shorter than its
// operands, and the sequencer layers it reaches go through VoiceCommandHost.
namespace VS::Audio::Seq {
// The operand bytes that follow the opcode, up to the end of the sequence.
using Operands = std::span<const std::uint8_t>;

// Channel update flags (FSoundChannel.updateFlags, src/include/sound.h:65-81).
inline constexpr std::uint32_t kVibratoActive = 1U << 0;
inline constexpr std::uint32_t kTremoloActive = 1U << 1;
inline constexpr std::uint32_t kAutoPanActive = 1U << 2;
inline constexpr std::uint32_t kDrumKeymapActive = 1U << 3;
inline constexpr std::uint32_t kPitchSidechainActive = 1U << 4;
inline constexpr std::uint32_t kVolumeSidechainActive = 1U << 5;
inline constexpr std::uint32_t kFullNoteLengthActive = 1U << 6;
inline constexpr std::uint32_t kOverlayChannel = 1U << 8;
inline constexpr std::uint32_t kPatchKeymapActive = 1U << 12;
inline constexpr std::uint32_t kFrequencyModulationAllowed = 1U << 16;
inline constexpr std::uint32_t kLoopBoundaryStop = 1U << 20;
inline constexpr std::uint32_t kAttackOverridden = 1U << 24;
inline constexpr std::uint32_t kSustainOverridden = 1U << 27;
inline constexpr std::uint32_t kReleaseOverridden = 1U << 28;
// The flags an instrument or keymap change clears.
inline constexpr std::uint32_t kInstrumentFlags{
    kDrumKeymapActive | kPatchKeymapActive | kAttackOverridden |
        kSustainOverridden | kReleaseOverridden,
};

// Voice parameter dirty flags (Audio_VoiceParams.dirtyFlags).
inline constexpr std::uint32_t kVolumeDirty = 0x3;
inline constexpr std::uint32_t kSampleRateDirty = 0x10;
inline constexpr std::uint32_t kAttackModeDirty = 0x100;
inline constexpr std::uint32_t kSustainModeDirty = 0x200;
inline constexpr std::uint32_t kReleaseModeDirty = 0x400;
inline constexpr std::uint32_t kAttackRateDirty = 0x800;
inline constexpr std::uint32_t kDecayRateDirty = 0x1000;
inline constexpr std::uint32_t kSustainRateDirty = 0x2000;
inline constexpr std::uint32_t kReleaseRateDirty = 0x4000;
inline constexpr std::uint32_t kSustainLevelDirty = 0x8000;
inline constexpr std::uint32_t kEnvelopeDirty = 0xFF00;
// Sound_CopyInstrumentInfoToChannel (SLUS_010.40/25AC.c:4953).
inline constexpr std::uint32_t kInstrumentDirty = 0x1FF80;

// Driver update requests (Audio_DriverFlags.updateFlags).
inline constexpr std::uint32_t kNoiseClockUpdate = 0x10;
inline constexpr std::uint32_t kVoiceModeUpdate = 0x100;

// Channel play modes (src/include/sound.h:50-51).
inline constexpr std::uint16_t kLegatoPlay = 1;
inline constexpr std::uint16_t kFullLengthPlay = 4;

// The sample start FE0A gives the instrument it loads
// (SLUS_010.40/25AC.c:5329).
inline constexpr std::uint32_t kSilentSampleStart = 0x1010;

// One channel voice's SPU parameters (Audio_VoiceParams).
struct VoiceState {
  std::uint32_t dirty_flags = 0;
  std::uint32_t sample_start = 0;
  std::uint32_t loop_start = 0;
  std::uint16_t adsr_lower = 0;
  std::uint16_t adsr_upper = 0;
  std::uint16_t volume_scale = 0;

  friend bool operator==(const VoiceState&, const VoiceState&) = default;
};

// The FSoundChannel fields the commands read and write, at the original
// widths and signedness. `kind`, `pitch_base` and `effect_voice_mask` are
// inputs only.
struct ChannelState {
  Driver::ChannelKind kind = Driver::ChannelKind::Music;
  std::int32_t pitch_base = 0;
  std::int32_t effect_voice_mask = 0;

  std::uint32_t update_flags = 0;
  std::int32_t volume = 0;
  std::int32_t volume_slide_step = 0;
  std::int32_t fine_pitch_delta = 0;
  std::int32_t key_on_volume = 0;
  std::int32_t key_on_volume_slide_step = 0;

  std::uint16_t note_length = 0;
  std::uint16_t gate_length = 0;
  std::uint16_t instrument_index = 0;
  std::uint16_t volume_balance = 0;
  std::uint16_t volume_balance_slide_length = 0;
  std::uint16_t volume_slide_length = 0;
  std::uint16_t key_on_volume_slide_length = 0;
  std::uint16_t effect_volume_slide_length = 0;
  std::uint16_t pan = 0;
  std::uint16_t pan_slide_length = 0;
  std::uint16_t octave = 0;
  std::uint16_t pitch_bend_slide_length = 0;
  std::uint16_t portamento_key = 0;
  std::uint16_t portamento_steps = 0;
  std::uint16_t play_modes = 0;

  std::int16_t vibrato_delay = 0;
  std::uint16_t vibrato_delay_left = 0;
  std::uint16_t vibrato_rate = 0;
  std::uint16_t vibrato_rate_counter = 0;
  std::uint16_t vibrato_shape = 0;
  std::uint16_t vibrato_base = 0;
  std::uint16_t vibrato_depth = 0;
  std::uint16_t vibrato_depth_slide_length = 0;
  std::uint16_t vibrato_depth_slide_step = 0;

  std::int16_t tremolo_delay = 0;
  std::uint16_t tremolo_delay_left = 0;
  std::uint16_t tremolo_rate = 0;
  std::int16_t tremolo_rate_counter = 0;
  std::uint16_t tremolo_shape = 0;
  std::uint16_t tremolo_depth = 0;
  std::uint16_t tremolo_depth_slide_length = 0;
  std::int16_t tremolo_depth_slide_step = 0;

  std::uint16_t auto_pan_rate = 0;
  std::uint16_t auto_pan_rate_counter = 0;
  std::uint16_t auto_pan_shape = 0;
  std::uint16_t auto_pan_depth = 0;
  std::uint16_t auto_pan_depth_slide_length = 0;
  std::int16_t auto_pan_depth_slide_step = 0;

  std::uint16_t noise_timer = 0;
  std::uint16_t fm_timer = 0;
  std::uint16_t random_pitch_depth = 0;

  std::int16_t stored_note_length = 0;
  std::int16_t fixed_note_length = 0;
  std::int16_t volume_balance_slide_step = 0;
  std::int16_t effect_volume = 0;
  std::int16_t effect_volume_slide_step = 0;
  std::int16_t pan_slide_step = 0;
  std::int16_t transpose = 0;
  std::int16_t fine_tune = 0;
  std::int16_t key = 0;
  std::int16_t pitch_bend_target = 0;
  std::int16_t portamento_transpose = 0;
  std::int16_t vibrato_pitch = 0;
  std::int16_t tremolo_volume = 0;
  std::int16_t auto_pan_offset = 0;

  VoiceState voice;

  // The LFO waveform selector a command switched to; empty keeps the
  // channel's current waveform.
  std::optional<std::uint8_t> vibrato_wave;
  std::optional<std::uint8_t> tremolo_wave;
  std::optional<std::uint8_t> auto_pan_wave;
  // The keymap a patch change selected, as a byte offset from the start of
  // the music layer's patch table; empty keeps the current keymap.
  std::optional<std::uint32_t> keymap_offset;

  friend bool operator==(const ChannelState&, const ChannelState&) = default;
};

enum class CommandStatus : std::uint8_t {
  kApplied,
  // The span ended before the command's operands.
  kTruncated,
  // A modulation command named a waveform selector with no waveform.
  kWaveformMissing,
  // The instrument table has no entry at the selected index.
  kInstrumentMissing,
  // The patch table entry or its keymap lies outside the music data.
  kPatchOutOfRange,
};

// What a command did and how many operand bytes it consumed. A truncated
// command consumes none; a refused one with complete operands consumes them.
struct CommandResult {
  CommandStatus status = CommandStatus::kApplied;
  std::size_t consumed = 0;

  friend bool operator==(const CommandResult&, const CommandResult&) = default;
};

enum class PatchLookup : std::uint8_t {
  // The music layer has no patch table.
  kNoTable,
  kFound,
  // The patch table entry lies outside the music data.
  kOutOfRange,
};

struct PatchEntry {
  PatchLookup lookup = PatchLookup::kNoTable;
  // The entry's keymap offset from the start of the patch table; values
  // above kMutedPatch mute the channel.
  std::uint16_t offset = 0;
};

class VoiceCommandHost {
 public:
  virtual ~VoiceCommandHost() = default;
  // The instrument table slot sequence instrument `instrument` selects for a
  // channel of `kind` (func_8001A1F4 and func_8001A22C).
  virtual std::int32_t ResolveInstrument(Driver::ChannelKind kind,
                                         std::int32_t effect_voice_mask,
                                         std::uint8_t instrument) = 0;
  virtual std::optional<Samples::InstrumentRecord> Instrument(
      std::int32_t index) = 0;
  // Entry `patch` of the music layer's patch table.
  virtual PatchEntry FindPatch(std::uint8_t patch) = 0;
  // Whether a keymap starting `offset` bytes into the patch table lies in the
  // music data.
  virtual bool HasPatchKeymap(std::uint32_t offset) = 0;
  // Whether the music layer has a drum keymap table.
  virtual bool HasDrumKeymap() = 0;
  virtual bool HasLfoWaveform(std::uint8_t shape) = 0;
  // Sets or clears `mode` for the channels in `channel_bits` of the music
  // layer or the sound-effect voices, as `kind` selects.
  virtual void SetVoiceMode(Driver::ChannelKind kind, Driver::VoiceMode mode,
                            std::uint32_t channel_bits, bool enabled) = 0;
  virtual std::uint16_t NoiseClock(Driver::ChannelKind kind) = 0;
  virtual void SetNoiseClock(Driver::ChannelKind kind, std::uint16_t clock) = 0;
  virtual void RequestVoiceUpdate(std::uint32_t flags) = 0;
};

// Patch table entries above this value mute the channel instead of naming a
// keymap; the keymap follows its entry's 0x20-byte header.
inline constexpr std::uint16_t kMutedPatch = 0x8000;
inline constexpr std::uint32_t kKeymapHeaderBytes = 0x20;

// Slide lengths, portamento steps and oscillator rates count 256 ticks for a
// zero operand.
inline constexpr std::uint16_t kDefaultTickCount = 0x100;

[[nodiscard]] constexpr CommandResult Applied(std::size_t consumed) {
  return {CommandStatus::kApplied, consumed};
}

[[nodiscard]] constexpr CommandResult Truncated() {
  return {CommandStatus::kTruncated, 0};
}

[[nodiscard]] constexpr std::uint16_t TickCount(std::uint8_t ticks) {
  return ticks == 0 ? kDefaultTickCount : ticks;
}

// (destination - start) / length with the original's wrapping 32-bit
// subtraction and truncating signed division; length is never zero.
[[nodiscard]] constexpr std::int32_t SlideStep(std::int32_t destination,
                                               std::int32_t start,
                                               std::uint16_t length) {
  const auto difference =
      static_cast<std::int32_t>(static_cast<std::uint32_t>(destination) -
                                static_cast<std::uint32_t>(start));
  return difference / static_cast<std::int32_t>(length);
}
}  // namespace VS::Audio::Seq
