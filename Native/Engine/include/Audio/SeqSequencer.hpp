#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Audio/MusicVoiceConflicts.hpp"
#include "Audio/PlaybackReset.hpp"
#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqVoiceCommands.hpp"
#include "Audio/VoiceOutput.hpp"

// The sequencer (SLUS_010.40/25AC.c:4024-4989): the driver tick that counts
// down every channel's note, the channel step that runs a channel's commands
// up to its next note, and the note pitch and patch lookups. Program reads
// are bounded by the sequence data the channel runs: a step whose opcode or
// operands lie outside it refuses and finishes the channel.
namespace VS::Audio::Seq {
inline constexpr std::size_t kMusicChannelCount = 32;
// Sound-effect channels use voice bits 12 and up (Audio_SoundEffects_channels).
inline constexpr std::uint32_t kFirstEffectVoice = 12;
inline constexpr std::size_t kEffectChannelCount = 32 - kFirstEffectVoice;
inline constexpr std::size_t kSemitoneCount = 12;
inline constexpr std::size_t kNoteLengthCount = 12;
inline constexpr std::size_t kRandomPitchOffsetCount = 256;
inline constexpr std::size_t kOpcodeCount = 0x60;
inline constexpr std::size_t kExtendedOpcodeCount = 0x20;
// The driver keeps the costs of its last four ticks and their sum.
inline constexpr std::size_t kRecentTickCostCount = 4;
// Channel update flag that turns a CA loop into the end of the track.
inline constexpr std::uint32_t kStopAtEndlessLoop = 1U << 21;
// Driver mix behaviour flag that transposes notes through the layer's note
// remap table, and the one that runs the saved music layer's fade.
inline constexpr std::uint32_t kNoteRemapActive = 0x10;
inline constexpr std::uint32_t kSavedMusicFadeActive = 0x100;

// The sequencer's constant tables (module image data).
struct SequencerTables {
  std::span<const std::uint8_t> opcode_lengths;
  std::span<const std::uint8_t> extended_opcode_lengths;
  std::span<const std::uint16_t, kNoteLengthCount> note_lengths;
  std::span<const std::uint32_t, kSemitoneCount> semitone_pitches;
  std::span<const std::int8_t> note_remap_tables;
  std::span<const std::uint8_t, kRandomPitchOffsetCount> random_pitch_offsets;
};

// func_8001A1F4: the instrument slot a sound effect's instrument selects.
std::int32_t ResolveSoundEffectInstrument(std::int32_t sample_bank_flags,
                                          std::int32_t instrument_index);
// func_8001A22C: the instrument slot a music instrument selects.
std::int32_t ResolveMusicInstrument(std::int32_t music_status_flags,
                                    std::uint32_t instrument_index);

// Sound_CopyInstrumentInfoToChannel.
void ApplyInstrumentParameters(VoiceState& voice,
                               const Samples::InstrumentRecord& instrument,
                               std::uint32_t sample_start);
// func_8001B094: loads instrument `index`, whose record is `instrument`.
void SelectChannelInstrument(ChannelState& channel, std::uint16_t index,
                             const Samples::InstrumentRecord& instrument);
// Sound_ClearVoiceFromSchedulerState.
void ReleaseSoundEffectVoice(VoiceSchedulerState& scheduler,
                             EffectIdentity& effect, std::uint32_t voice_bit);

struct PitchRequest {
  std::int32_t note = 0;
  std::int16_t fine_tune = 0;
  // The channel's fine pitch delta, kept when `fine_tune` is a whole number.
  std::int32_t fine_pitch_delta = 0;
};

struct NotePitch {
  std::uint16_t pitch = 0;
  std::int32_t fine_pitch_delta = 0;

  friend bool operator==(const NotePitch&, const NotePitch&) = default;
};

// func_8001A4BC: the sample rate `request.note` plays `instrument` at.
NotePitch CalculateNotePitch(
    std::span<const std::uint32_t, kSemitoneCount> semitone_pitches,
    const Samples::InstrumentRecord& instrument, const PitchRequest& request);

// The pitch slide fields the step resets (FSoundChannel.pitchSlide*).
struct PitchSlideState {
  std::int32_t slide = 0;
  std::int32_t step = 0;
  std::uint16_t ticks_left = 0;

  friend bool operator==(const PitchSlideState&,
                         const PitchSlideState&) = default;
};

// Everything one channel step reads or writes: the channel, its program
// position, the active music layer, the sound-effect voice scheduler and
// the driver flags. `mix_behavior` and `random_pitch_phase` are inputs.
struct StepState {
  ChannelState channel;
  ProgramState program;
  PitchSlideState pitch_slide;
  std::uint32_t assigned_voice = kUnassignedVoice;
  MusicLayerState layer;
  VoiceSchedulerState scheduler;
  std::uint32_t mix_behavior = 0;
  std::uint32_t driver_update_flags = 0;
  std::int32_t random_pitch_phase = 0;

  friend bool operator==(const StepState&, const StepState&) = default;
};

enum class StepRefusal : std::uint8_t {
  // The program counter lies outside the sequence data.
  kOpcodeOutside,
  // The sequence data ends inside an opcode's operands.
  kOperandsOutside,
  // An FE opcode names no extended command.
  kUnknownExtendedOpcode,
  // The channel ran this many commands without reaching a note.
  kCommandLimit,
  // The lookahead for the next note left the sequence data or ran too long.
  kLookaheadFailed,
  // The note's keymap entry lies outside the keymap.
  kKeymapOutside,
  kInstrumentMissing,
  // The layer selects note remapping without a remap table.
  kNoteRemapMissing,
  kWaveformMissing,
  kRandomPitchPhaseOutside,
  // A pitch bend started with no slide length.
  kPitchBendWithoutLength,
  // The handler table has no command for the opcode.
  kCommandMissing,
  // The opcode's command refused its operands or state.
  kCommandRefused,
};

enum class CommandOutcome : std::uint8_t {
  kRan,
  // The handler table has no command for the opcode.
  kMissing,
  // The command ran and refused, leaving the state unchanged.
  kRefused,
};

class StepHost {
 public:
  virtual ~StepHost() = default;
  // The sequence data the channel's program positions currently index.
  virtual ProgramData Program() = 0;
  virtual const SequencerTables& Tables() = 0;
  // Runs basic opcode `opcode` (0xA0-0xFF) or extended opcode `opcode` from
  // the handler tables over the state.
  virtual CommandOutcome RunCommand(StepState& state, std::uint8_t opcode,
                                    std::uint32_t channel_bit) = 0;
  virtual CommandOutcome RunExtendedCommand(StepState& state,
                                            std::uint8_t opcode,
                                            std::uint32_t channel_bit) = 0;
  virtual std::optional<Samples::InstrumentRecord> Instrument(
      std::int32_t index) = 0;
  // The bytes from the channel's patch keymap, and from the layer's drum
  // keymap table, to the end of the data they lie in.
  virtual std::span<const std::uint8_t> ChannelKeymap() = 0;
  virtual std::span<const std::uint8_t> DrumKeymap() = 0;
  virtual bool HasLfoWaveform(std::uint16_t shape) = 0;
  virtual void Refuse(StepRefusal refusal) = 0;
};

// func_80019FC4: the note, tie or rest the program reaches next without
// running a command; 0xA0 for the end of the track.
std::uint8_t PeekNextNote(StepState& state, StepHost& host);
// func_8001A258: selects the patch keymap entry for `note_key`.
void SelectNotePatch(StepState& state, StepHost& host, std::uint32_t note_key);
// func_8001A64C: keys drum `drum_key` on and returns its pitch; empty when
// it was refused.
std::optional<std::uint16_t> StartDrumNote(StepState& state, StepHost& host,
                                           std::uint32_t channel_bit,
                                           std::uint32_t drum_key);
// func_8001A8D8: runs the channel's commands up to its next note and starts
// it.
void ExecuteChannelStep(StepState& state, StepHost& host,
                        std::uint32_t channel_bit);

// A level the driver slides every fourth tick.
struct LevelSlide {
  std::int32_t value = 0;
  std::int32_t step = 0;
  std::int16_t length = 0;

  friend bool operator==(const LevelSlide&, const LevelSlide&) = default;
};

// g_Sound_MasterFadeTimer.
struct FadeTimer {
  std::int32_t value = 0;
  std::int32_t step = 0;
  std::int32_t ticks_remaining = 0;

  friend bool operator==(const FadeTimer&, const FadeTimer&) = default;
};

// One sound-effect channel's effect level slides.
struct EffectSlides {
  std::int16_t volume = 0;
  std::int16_t volume_step = 0;
  std::uint16_t volume_length = 0;
  std::uint16_t pan = 0;
  std::int16_t pan_step = 0;
  std::uint16_t pan_length = 0;
  std::int32_t pitch_scale = 0;
  std::uint32_t pitch_scale_step = 0;
  std::uint16_t pitch_scale_length = 0;
  std::uint32_t dirty_flags = 0;

  friend bool operator==(const EffectSlides&, const EffectSlides&) = default;
};

// One channel's note countdown.
struct ChannelTimers {
  std::uint16_t note_length = 0;
  std::uint16_t gate_length = 0;
  std::int32_t priority = 0;
  std::int32_t effect_tags = 0;

  friend bool operator==(const ChannelTimers&, const ChannelTimers&) = default;
};

enum class ChannelGroup : std::uint8_t {
  kMusic,
  kSecondaryMusic,
  kSoundEffects,
};

// The driver state one tick reads and writes. `active` is the active music
// layer; `secondary` the layer a crossfade or saved track plays beside it.
struct PlaybackState {
  std::uint32_t control_latches = 0;
  std::uint32_t mix_behavior = 0;
  std::uint32_t driver_update_flags = 0;
  std::int32_t stream_control_flags = 0;
  MusicLayerState active;
  bool active_is_primary = true;
  std::optional<MusicLayerState> secondary;
  VoiceSchedulerState scheduler;
  std::uint32_t music_loops_remaining = 0;
  std::int32_t music_transition_timer = 0;
  std::uint16_t tick_counter = 0;
  bool music_pending = false;
  FadeTimer master_fade;
  std::int32_t random_pitch_phase = 0;
  LevelSlide disc_volume;
  LevelSlide tempo_scale;
  LevelSlide music_pitch_scale;
  std::array<std::int32_t, kRecentTickCostCount> recent_tick_costs{};
  std::int32_t recent_tick_cost_total = 0;

  friend bool operator==(const PlaybackState&, const PlaybackState&) = default;
};

enum class PlaybackRefusal : std::uint8_t {
  // A transition named the secondary layer while none plays.
  kSecondaryLayerMissing,
  // Sound-effect voices below the first sound-effect channel are active.
  kEffectVoiceOutsideChannels,
  // The fast-forward to the requested loop never reached it.
  kLoopFastForwardLimit,
};

// Every effect that takes the state sees it current and leaves it current.
class PlaybackHost {
 public:
  virtual ~PlaybackHost() = default;
  virtual std::int32_t ReadTimer() = 0;
  virtual void ReleasePendingVoices(PlaybackState& state) = 0;
  virtual void DetachSecondaryLayer(PlaybackState& state) = 0;
  // Copies the secondary layer and its channels over the active ones, clears
  // the secondary layer's track and detaches it.
  virtual void PromoteSecondaryLayer(PlaybackState& state) = 0;
  // Starts the pending track (StartOrResumeMusic) and clears it.
  virtual void StartPendingMusic(PlaybackState& state) = 0;
  virtual void CommitVoices(PlaybackState& state) = 0;
  virtual ChannelTimers Timers(ChannelGroup group, std::size_t index) = 0;
  virtual void SetTimers(ChannelGroup group, std::size_t index,
                         const ChannelTimers& timers) = 0;
  virtual void StepChannel(PlaybackState& state, ChannelGroup group,
                           std::size_t index) = 0;
  virtual void UpdateChannelSlides(PlaybackState& state, ChannelGroup group,
                                   std::size_t index) = 0;
  // Makes the primary layer the active one again after the secondary pass.
  virtual void ActivatePrimaryLayer(PlaybackState& state) = 0;
  virtual void InvalidateMusicVolumes(PlaybackState& state,
                                      ChannelGroup group) = 0;
  virtual void MarkMusicPitchDirty() = 0;
  virtual EffectSlides EffectSlidesOf(std::size_t index) = 0;
  virtual void SetEffectSlides(std::size_t index,
                               const EffectSlides& slides) = 0;
  virtual void WriteDiscVolume(PlaybackState& state) = 0;
  virtual void UpdateCutsceneStream(PlaybackState& state) = 0;
  // Hands the tick to the native output (Audio_Native_Tick).
  virtual void FinishTick(PlaybackState& state) = 0;
  virtual void Refuse(PlaybackRefusal refusal) = 0;
};

// func_8001924C: the slides the driver runs every fourth tick.
void UpdateGlobalSlides(PlaybackState& state, PlaybackHost& host);
// func_80019614: advances a pending music transition at a timer boundary.
void AdvanceMusicTransition(PlaybackState& state, PlaybackHost& host);
// func_80019704: one tempo tick of a music layer; returns its active
// channels.
std::int32_t UpdateMusicSequence(PlaybackState& state, PlaybackHost& host,
                                 ChannelGroup group);
// func_800199C4: the master fade of the layer `group` names.
void UpdateSavedMusicFade(PlaybackState& state, PlaybackHost& host,
                          ChannelGroup group);
// func_80019A58: one driver tick; returns its cost in timer counts.
std::int32_t TickPlayback(PlaybackState& state, PlaybackHost& host);
}  // namespace VS::Audio::Seq
