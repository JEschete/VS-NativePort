#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Audio/PlaybackReset.hpp"

// The sequencer's flow-control commands (the SoundVM loop, jump, call, tempo,
// reverb, timer and end-of-track opcode handlers, SLUS_010.40/25AC.c:4989-
// 6233) over typed copies of one channel's program position, its music layer
// and the sound-effect voice scheduler. Program positions are byte offsets
// into the sequence data the channel runs: a command whose operands or jump
// target lie outside that data refuses and changes nothing.
namespace VS::Audio::Seq {
// The sequence data a channel's program positions index.
using ProgramData = std::span<const std::uint8_t>;

// A program position outside the sequence data the channel runs.
inline constexpr std::uint32_t kOutsideProgram = 0xFFFFFFFFU;
inline constexpr std::size_t kLoopDepthCount = 4;
inline constexpr std::uint16_t kLoopDepthMask = kLoopDepthCount - 1;

// FSoundChannel's program counter, call return and loop stack.
struct ProgramState {
  std::uint32_t counter = kOutsideProgram;
  std::array<std::uint32_t, kLoopDepthCount> loop_starts{
      kOutsideProgram, kOutsideProgram, kOutsideProgram, kOutsideProgram};
  std::uint32_t return_address = kOutsideProgram;
  std::uint16_t step_counter = 0;
  std::array<std::uint16_t, kLoopDepthCount> loop_iterations{};
  std::array<std::int16_t, kLoopDepthCount> loop_step_snapshots{};
  std::uint16_t loop_depth = 0;

  friend bool operator==(const ProgramState&, const ProgramState&) = default;
};

// FSoundChannelConfig (src/include/sound.h:492): one music layer's playback
// state at the original's field widths.
struct MusicLayerState {
  std::int32_t status_flags = 0;
  std::int32_t active_channels = 0;
  std::uint32_t keyed_voices = 0;
  std::int32_t allocated_voices = 0;
  std::int32_t pending_key_on = 0;
  std::uint32_t active_notes = 0;
  std::int32_t pending_key_off = 0;
  std::uint32_t paused_channels = 0;
  std::int32_t tempo = 0;
  std::int32_t tempo_slide_step = 0;
  std::uint32_t tick_accumulator = 0;
  std::uint32_t resume_key_on_mask = 0;
  std::int32_t push_release_mask = 0;
  std::int32_t sequence_control_value = 0;
  std::int32_t voice_search_start = 0;
  std::uint32_t noise_channels = 0;
  std::uint32_t reverb_channels = 0;
  std::uint32_t fm_channels = 0;
  std::int32_t reverb_depth = 0;
  std::int32_t reverb_depth_slide_step = 0;
  std::int32_t master_volume = 0;
  std::int32_t master_volume_step = 0;
  std::int16_t master_volume_slide_length = 0;
  std::int16_t reverb_depth_slide_length = 0;
  std::uint16_t tempo_slide_length = 0;
  std::uint16_t music_id = 0;
  std::uint16_t jump_threshold = 0;
  std::uint16_t noise_clock = 0;
  std::uint16_t timer_upper = 0;
  std::uint16_t timer_upper_count = 0;
  std::uint16_t timer_lower = 0;
  std::uint16_t timer_lower_count = 0;
  std::uint16_t timer_top = 0;
  // The note remap table the layer transposes notes through, as a byte offset
  // into the remap tables; empty when the layer's table lies elsewhere.
  std::optional<std::uint16_t> note_remap_offset;

  friend bool operator==(const MusicLayerState&,
                         const MusicLayerState&) = default;
};

// FSoundVoiceSchedulerState (src/include/sound.h:274): the sound-effect
// voices' key and mode masks.
struct VoiceSchedulerState {
  std::uint32_t active_voices = 0;
  std::uint32_t pending_key_on = 0;
  std::uint32_t keyed_voices = 0;
  std::uint32_t pending_key_off = 0;
  std::uint32_t paused_effect_voices = 0;
  std::uint32_t effect_tempo = 0;
  std::uint32_t tempo_accumulator = 0;
  std::uint32_t noise_voices = 0;
  std::uint32_t reverb_voices = 0;
  std::uint32_t fm_voices = 0;
  std::uint16_t noise_clock = 0;

  friend bool operator==(const VoiceSchedulerState&,
                         const VoiceSchedulerState&) = default;
};

// A sound-effect channel's owner tags and whether it still names an effect.
struct EffectIdentity {
  std::int32_t tags = 0;
  bool has_effect = false;

  friend bool operator==(const EffectIdentity&,
                         const EffectIdentity&) = default;
};

// Everything a flow-control command reads or writes.
struct ControlState {
  ProgramState program;
  Driver::ChannelKind kind = Driver::ChannelKind::kMusic;
  std::uint32_t update_flags = 0;
  std::int32_t volume = 0;
  std::uint16_t pan = 0;
  EffectIdentity effect;
  MusicLayerState layer;
  VoiceSchedulerState scheduler;
  std::uint32_t driver_update_flags = 0;
  std::uint32_t music_loops_remaining = 0;

  friend bool operator==(const ControlState&, const ControlState&) = default;
};

enum class ControlStatus : std::uint8_t {
  kApplied,
  // The sequence data ended before the command's operands.
  kTruncated,
  // The command would move the program counter, or start a program, outside
  // the sequence data.
  kTargetOutside,
};

// Two sound-effect programs FE0B starts, as offsets into the sequence data,
// with the command parameters the original passes.
struct EmbeddedSoundEffect {
  std::optional<std::uint32_t> first_program;
  std::optional<std::uint32_t> second_program;
  std::uint32_t pan = 0;
  std::uint32_t volume = 0;

  friend bool operator==(const EmbeddedSoundEffect&,
                         const EmbeddedSoundEffect&) = default;
};

struct ControlResult {
  ControlStatus status = ControlStatus::kApplied;
  std::optional<EmbeddedSoundEffect> sound_effect;

  friend bool operator==(const ControlResult&, const ControlResult&) = default;
};

// Driver update flags the commands request
// (FSoundDriverFlags.updateFlags, src/include/sound.h:240).
inline constexpr std::uint32_t kReverbDepthUpdate = 0x80;
inline constexpr std::uint32_t kChannelFinishedUpdate = 0x110;
// Music layer status flag that finishes channels on a replacement track.
inline constexpr std::int32_t kFinishOnReplacement = 0x10000;

// Every command takes the channel state, the sequence data its program
// positions index and the channel's voice bit.
using ControlCommand = ControlResult (*)(ControlState&, ProgramData,
                                         std::uint32_t);

ControlResult FinishChannel(ControlState& state, ProgramData program,
                            std::uint32_t voice_bit);
ControlResult FinishChannelOnReplacement(ControlState& state,
                                         ProgramData program,
                                         std::uint32_t voice_bit);
ControlResult StopOnInvalidInstruction(ControlState& state, ProgramData program,
                                       std::uint32_t voice_bit);
ControlResult IgnoreInstruction(ControlState& state, ProgramData program,
                                std::uint32_t voice_bit);
ControlResult SetTempo(ControlState& state, ProgramData program,
                       std::uint32_t voice_bit);
ControlResult SlideTempo(ControlState& state, ProgramData program,
                         std::uint32_t voice_bit);
ControlResult SetReverbDepth(ControlState& state, ProgramData program,
                             std::uint32_t voice_bit);
ControlResult SlideReverbDepth(ControlState& state, ProgramData program,
                               std::uint32_t voice_bit);
ControlResult JumpRelative(ControlState& state, ProgramData program,
                           std::uint32_t voice_bit);
ControlResult JumpAtThreshold(ControlState& state, ProgramData program,
                              std::uint32_t voice_bit);
ControlResult CallRelative(ControlState& state, ProgramData program,
                           std::uint32_t voice_bit);
ControlResult ReturnFromCall(ControlState& state, ProgramData program,
                             std::uint32_t voice_bit);
ControlResult SetSequenceControlValue(ControlState& state, ProgramData program,
                                      std::uint32_t voice_bit);
ControlResult SetVoiceAllocationStart(ControlState& state, ProgramData program,
                                      std::uint32_t voice_bit);
ControlResult ResetVoiceAllocationStart(ControlState& state,
                                        ProgramData program,
                                        std::uint32_t voice_bit);
ControlResult PushLoop(ControlState& state, ProgramData program,
                       std::uint32_t voice_bit);
ControlResult RepeatLoop(ControlState& state, ProgramData program,
                         std::uint32_t voice_bit);
ControlResult JumpOnLoopPass(ControlState& state, ProgramData program,
                             std::uint32_t voice_bit);
ControlResult ExitLoopOnPass(ControlState& state, ProgramData program,
                             std::uint32_t voice_bit);
ControlResult RepeatLoopForever(ControlState& state, ProgramData program,
                                std::uint32_t voice_bit);
ControlResult SetTimerDivisors(ControlState& state, ProgramData program,
                               std::uint32_t voice_bit);
ControlResult SetTimerCount(ControlState& state, ProgramData program,
                            std::uint32_t voice_bit);
ControlResult PlayEmbeddedSoundEffect(ControlState& state, ProgramData program,
                                      std::uint32_t voice_bit);
ControlResult SelectNoteRemapTable(ControlState& state, ProgramData program,
                                   std::uint32_t voice_bit);
ControlResult MarkChannelKeyed(ControlState& state, ProgramData program,
                               std::uint32_t voice_bit);
ControlResult ClearChannelKeyed(ControlState& state, ProgramData program,
                                std::uint32_t voice_bit);
}  // namespace VS::Audio::Seq
