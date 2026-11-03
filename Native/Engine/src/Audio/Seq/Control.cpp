// Hand-maintained (tools/port/owned.csv).
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqSequencer.hpp"
#include "Audio/SeqVoiceCommands.hpp"

namespace VS::Audio::Seq {
namespace {
constexpr std::uint32_t kByteBits = 8;
constexpr std::uint32_t kTempoLowShift = 16;
constexpr std::uint32_t kTempoHighShift = 24;
constexpr std::uint32_t kTempoWholeMask = 0xFFFF0000U;
constexpr std::uint32_t kReverbHighShift = 20;
constexpr std::uint32_t kReverbLowShift = 12;
constexpr std::uint32_t kReverbFractionMask = 0xFFFU;
constexpr std::int32_t kByteSignBit = 0x80;
constexpr std::size_t kRelativeJumpOperands = 2;
constexpr std::size_t kSlideOperands = 3;
constexpr std::size_t kLoopPassOperands = 3;
constexpr std::size_t kEmbeddedOperands = 4;
// FE0B's program offsets count from the end of their own two bytes.
constexpr std::uint32_t kEmbeddedOffsetBias = 2;
constexpr std::uint32_t kEmbeddedPanShift = 8;
constexpr std::uint32_t kEmbeddedVolumeShift = 23;

constexpr ControlResult Refused(ControlStatus status) {
  return {status, std::nullopt};
}

// The `count` operand bytes at the program counter.
std::optional<ProgramData> ReadOperands(const ControlState& state,
                                        ProgramData program,
                                        std::size_t count) {
  const std::uint32_t counter = state.program.counter;
  if (counter > program.size() || program.size() - counter < count) {
    return std::nullopt;
  }
  return program.subspan(counter, count);
}

// Whether an opcode can be read at `position`.
bool Inside(ProgramData program, std::uint32_t position) {
  return position < program.size();
}

std::uint16_t Word(std::uint8_t low, std::uint8_t high) {
  return static_cast<std::uint16_t>(low | (high << kByteBits));
}

// `origin` plus the signed little-endian offset `low`/`high` when the result
// lies in the sequence data.
std::optional<std::uint32_t> RelativeTarget(ProgramData program,
                                            std::uint32_t origin,
                                            std::uint8_t low,
                                            std::uint8_t high) {
  const auto offset = static_cast<std::int16_t>(Word(low, high));
  const std::int64_t target = std::int64_t{origin} + offset;
  if (target < 0 || !Inside(program, static_cast<std::uint32_t>(target))) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(target);
}

void ClearBits(std::int32_t& mask, std::uint32_t bits) {
  mask = static_cast<std::int32_t>(static_cast<std::uint32_t>(mask) & ~bits);
}

// The reverb depth an FE02/FE03 operand pair encodes: the signed high byte
// above the low byte, in the depth's 20.12 fixed point.
std::int32_t ReverbDepth(std::uint8_t low, std::uint8_t high) {
  const std::int32_t signed_high =
      (std::int32_t{high} ^ kByteSignBit) - kByteSignBit;
  return static_cast<std::int32_t>(
      (static_cast<std::uint32_t>(signed_high) << kReverbHighShift) |
      (std::uint32_t{low} << kReverbLowShift));
}

std::int32_t Tempo(std::uint8_t low, std::uint8_t high) {
  return static_cast<std::int32_t>((std::uint32_t{low} << kTempoLowShift) |
                                   (std::uint32_t{high} << kTempoHighShift));
}

std::uint16_t LoopDepth(const ProgramState& program) {
  return program.loop_depth & kLoopDepthMask;
}

void PopLoop(ProgramState& program) {
  program.loop_depth =
      static_cast<std::uint16_t>((program.loop_depth - 1) & kLoopDepthMask);
}

// Whether the loop on top of the stack is on its `pass`th iteration.
bool OnLoopPass(const ProgramState& program, std::uint8_t pass) {
  return std::uint32_t{program.loop_iterations[LoopDepth(program)]} + 1 ==
         TickCount(pass);
}

// Jumps by the offset after FE08/FE09's pass count on the selected pass.
ControlResult JumpOnPass(ControlState& state, ProgramData program,
                         bool pop_loop) {
  const auto operands = ReadOperands(state, program, kLoopPassOperands);
  if (!operands) return Refused(ControlStatus::kTruncated);
  ProgramState& position = state.program;
  if (!OnLoopPass(position, (*operands)[0])) {
    position.counter += kLoopPassOperands;
    return {};
  }
  const auto target = RelativeTarget(program, position.counter + 1,
                                     (*operands)[1], (*operands)[2]);
  if (!target) return Refused(ControlStatus::kTargetOutside);
  position.counter = *target;
  if (pop_loop) PopLoop(position);
  return {};
}

// Returns to the top loop's start with the step counter it saved.
ControlResult ReturnToLoopStart(ProgramState& program, ProgramData data,
                                std::uint16_t iterations) {
  const std::uint16_t depth = LoopDepth(program);
  if (!Inside(data, program.loop_starts[depth])) {
    return Refused(ControlStatus::kTargetOutside);
  }
  program.loop_iterations[depth] = iterations;
  program.counter = program.loop_starts[depth];
  program.step_counter =
      static_cast<std::uint16_t>(program.loop_step_snapshots[depth]);
  return {};
}

// A program offset FE0B names: zero names no program, anything else counts
// from the end of its own two bytes.
std::optional<std::optional<std::uint32_t>> EmbeddedProgram(
    ProgramData program, std::uint32_t position, std::uint16_t offset) {
  if (offset == 0) return std::optional<std::uint32_t>{};
  const std::uint32_t target = position + offset + kEmbeddedOffsetBias;
  if (!Inside(program, target)) return std::nullopt;
  return std::optional<std::uint32_t>{target};
}
}  // namespace

// decomp: SLUS_010.40 SoundVM_A0_FinishChannel (src/SLUS_010.40/25AC.c:4989)
ControlResult FinishChannel(ControlState& state, ProgramData /*program*/,
                            std::uint32_t voice_bit) {
  if (state.kind == Driver::ChannelKind::Music) {
    MusicLayerState& layer = state.layer;
    ClearBits(layer.active_channels, voice_bit);
    if (layer.active_channels == 0) {
      state.music_loops_remaining = 0;
      layer.music_id = 0;
      layer.status_flags = 0;
    }
    layer.active_notes &= ~voice_bit;
    layer.keyed_voices &= ~voice_bit;
    ClearBits(layer.allocated_voices, voice_bit);
    layer.noise_channels &= ~voice_bit;
    layer.reverb_channels &= ~voice_bit;
    layer.fm_channels &= ~voice_bit;
  } else {
    ReleaseSoundEffectVoice(state.scheduler, state.effect, voice_bit);
  }
  state.update_flags = 0;
  state.driver_update_flags |= kChannelFinishedUpdate;
  return {};
}

// decomp: SLUS_010.40 SoundVM_FE1F_unk (src/SLUS_010.40/25AC.c:5011)
ControlResult FinishChannelOnReplacement(ControlState& state,
                                         ProgramData program,
                                         std::uint32_t voice_bit) {
  if ((state.layer.status_flags & kFinishOnReplacement) != 0) {
    return FinishChannel(state, program, voice_bit);
  }
  return {};
}

// decomp: SLUS_010.40 SoundVM_XX_Unimplemented (src/SLUS_010.40/25AC.c:6233)
ControlResult StopOnInvalidInstruction(ControlState& state, ProgramData program,
                                       std::uint32_t voice_bit) {
  return FinishChannel(state, program, voice_bit);
}

// decomp: SLUS_010.40 SoundVM_nop (src/SLUS_010.40/25AC.c:5227)
// decomp: SLUS_010.40 SoundVM_CD_DEBUG (src/SLUS_010.40/25AC.c:5758)
// decomp: SLUS_010.40 SoundVM_D1_DEBUG (src/SLUS_010.40/25AC.c:5771)
ControlResult IgnoreInstruction(ControlState& /*state*/,
                                ProgramData /*program*/,
                                std::uint32_t /*voice_bit*/) {
  return {};
}

// decomp: SLUS_010.40 SoundVM_FE00_SetTempo (src/SLUS_010.40/25AC.c:5018)
ControlResult SetTempo(ControlState& state, ProgramData program,
                       std::uint32_t /*voice_bit*/) {
  const auto operands = ReadOperands(state, program, 2);
  if (!operands) return Refused(ControlStatus::kTruncated);
  state.layer.tempo = Tempo((*operands)[0], (*operands)[1]);
  state.program.counter += 2;
  state.layer.tempo_slide_length = 0;
  return {};
}

// decomp: SLUS_010.40 SoundVM_FE01_SetTempoSlide (src/SLUS_010.40/25AC.c:5027)
ControlResult SlideTempo(ControlState& state, ProgramData program,
                         std::uint32_t /*voice_bit*/) {
  const auto operands = ReadOperands(state, program, kSlideOperands);
  if (!operands) return Refused(ControlStatus::kTruncated);
  MusicLayerState& layer = state.layer;
  layer.tempo_slide_length = TickCount((*operands)[0]);
  const std::int32_t destination = Tempo((*operands)[1], (*operands)[2]);
  state.program.counter += kSlideOperands;
  const auto start = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(layer.tempo) & kTempoWholeMask);
  layer.tempo_slide_step =
      SlideStep(destination, start, layer.tempo_slide_length);
  layer.tempo = start;
  return {};
}

// decomp: SLUS_010.40 SoundVM_FE02_SetMasterReverbDepth
// (src/SLUS_010.40/25AC.c:5048)
ControlResult SetReverbDepth(ControlState& state, ProgramData program,
                             std::uint32_t /*voice_bit*/) {
  const auto operands = ReadOperands(state, program, 2);
  if (!operands) return Refused(ControlStatus::kTruncated);
  const std::int32_t depth = ReverbDepth((*operands)[0], (*operands)[1]);
  state.program.counter += 2;
  state.layer.reverb_depth_slide_length = 0;
  state.driver_update_flags |= kReverbDepthUpdate;
  state.layer.reverb_depth = depth;
  return {};
}

// decomp: SLUS_010.40 SoundVM_FE03_SetMasterReverbSlide
// (src/SLUS_010.40/25AC.c:5063)
ControlResult SlideReverbDepth(ControlState& state, ProgramData program,
                               std::uint32_t /*voice_bit*/) {
  const auto operands = ReadOperands(state, program, kSlideOperands);
  if (!operands) return Refused(ControlStatus::kTruncated);
  MusicLayerState& layer = state.layer;
  const std::uint16_t length = TickCount((*operands)[0]);
  layer.reverb_depth_slide_length = static_cast<std::int16_t>(length);
  const std::int32_t destination = ReverbDepth((*operands)[1], (*operands)[2]);
  state.program.counter += kSlideOperands;
  const auto start = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(layer.reverb_depth) & ~kReverbFractionMask);
  layer.reverb_depth = start;
  layer.reverb_depth_slide_step = SlideStep(destination, start, length);
  return {};
}

// decomp: SLUS_010.40 SoundVM_FE06_JumpRelativeOffset
// (src/SLUS_010.40/25AC.c:5085)
ControlResult JumpRelative(ControlState& state, ProgramData program,
                           std::uint32_t /*voice_bit*/) {
  const auto operands = ReadOperands(state, program, kRelativeJumpOperands);
  if (!operands) return Refused(ControlStatus::kTruncated);
  const auto target = RelativeTarget(program, state.program.counter,
                                     (*operands)[0], (*operands)[1]);
  if (!target) return Refused(ControlStatus::kTargetOutside);
  state.program.counter = *target;
  return {};
}

// decomp: SLUS_010.40 SoundVM_FE07_JumpRelativeWithThreshold
// (src/SLUS_010.40/25AC.c:5092)
ControlResult JumpAtThreshold(ControlState& state, ProgramData program,
                              std::uint32_t /*voice_bit*/) {
  const auto operands = ReadOperands(state, program, kRelativeJumpOperands + 1);
  if (!operands) return Refused(ControlStatus::kTruncated);
  if (state.layer.jump_threshold < (*operands)[0]) {
    state.program.counter += kRelativeJumpOperands + 1;
    return {};
  }
  const auto target = RelativeTarget(program, state.program.counter + 1,
                                     (*operands)[1], (*operands)[2]);
  if (!target) return Refused(ControlStatus::kTargetOutside);
  state.program.counter = *target;
  return {};
}

// decomp: SLUS_010.40 SoundVM_FE0E_CallRelativeOffset
// (src/SLUS_010.40/25AC.c:5111)
ControlResult CallRelative(ControlState& state, ProgramData program,
                           std::uint32_t /*voice_bit*/) {
  const auto operands = ReadOperands(state, program, kRelativeJumpOperands);
  if (!operands) return Refused(ControlStatus::kTruncated);
  const auto target = RelativeTarget(program, state.program.counter,
                                     (*operands)[0], (*operands)[1]);
  if (!target) return Refused(ControlStatus::kTargetOutside);
  state.program.return_address = state.program.counter + kRelativeJumpOperands;
  state.program.counter = *target;
  return {};
}

// decomp: SLUS_010.40 SoundVM_FE0F_Return (src/SLUS_010.40/25AC.c:5119)
ControlResult ReturnFromCall(ControlState& state, ProgramData program,
                             std::uint32_t /*voice_bit*/) {
  if (!Inside(program, state.program.return_address)) {
    return Refused(ControlStatus::kTargetOutside);
  }
  state.program.counter = state.program.return_address;
  return {};
}

// decomp: SLUS_010.40 SoundVM_FE0C_unk (src/SLUS_010.40/25AC.c:5221)
ControlResult SetSequenceControlValue(ControlState& state, ProgramData program,
                                      std::uint32_t /*voice_bit*/) {
  const auto operands = ReadOperands(state, program, 1);
  if (!operands) return Refused(ControlStatus::kTruncated);
  state.layer.sequence_control_value = (*operands)[0];
  ++state.program.counter;
  return {};
}

// decomp: SLUS_010.40 SoundVM_FE10_unk (src/SLUS_010.40/25AC.c:5892)
ControlResult SetVoiceAllocationStart(ControlState& state, ProgramData program,
                                      std::uint32_t /*voice_bit*/) {
  const auto operands = ReadOperands(state, program, 1);
  if (!operands) return Refused(ControlStatus::kTruncated);
  state.layer.voice_search_start = (*operands)[0];
  ++state.program.counter;
  return {};
}

// decomp: SLUS_010.40 SoundVM_FE11_unk (src/SLUS_010.40/25AC.c:5898)
ControlResult ResetVoiceAllocationStart(ControlState& state,
                                        ProgramData /*program*/,
                                        std::uint32_t /*voice_bit*/) {
  state.layer.voice_search_start = 0;
  return {};
}

// decomp: SLUS_010.40 SoundVM_C8_LoopPoint (src/SLUS_010.40/25AC.c:5904)
ControlResult PushLoop(ControlState& state, ProgramData /*program*/,
                       std::uint32_t /*voice_bit*/) {
  ProgramState& program = state.program;
  program.loop_depth =
      static_cast<std::uint16_t>((program.loop_depth + 1) & kLoopDepthMask);
  program.loop_starts[program.loop_depth] = program.counter;
  program.loop_iterations[program.loop_depth] = 0;
  program.loop_step_snapshots[program.loop_depth] =
      static_cast<std::int16_t>(program.step_counter);
  return {};
}

// decomp: SLUS_010.40 SoundVM_C9_LoopN (src/SLUS_010.40/25AC.c:5914)
ControlResult RepeatLoop(ControlState& state, ProgramData program,
                         std::uint32_t /*voice_bit*/) {
  const auto operands = ReadOperands(state, program, 1);
  if (!operands) return Refused(ControlStatus::kTruncated);
  ProgramState& position = state.program;
  const std::uint16_t depth = LoopDepth(position);
  const auto iterations =
      static_cast<std::uint16_t>(position.loop_iterations[depth] + 1);
  if (iterations != TickCount((*operands)[0])) {
    return ReturnToLoopStart(position, program, iterations);
  }
  position.loop_iterations[depth] = iterations;
  ++position.counter;
  PopLoop(position);
  return {};
}

// decomp: SLUS_010.40 SoundVM_FE08_JumpOnNthLoopPass
// (src/SLUS_010.40/25AC.c:5941)
ControlResult JumpOnLoopPass(ControlState& state, ProgramData program,
                             std::uint32_t /*voice_bit*/) {
  return JumpOnPass(state, program, false);
}

// decomp: SLUS_010.40 SoundVM_FE09_JumpAndPopStackOnNthLoopPass
// (src/SLUS_010.40/25AC.c:5969)
ControlResult ExitLoopOnPass(ControlState& state, ProgramData program,
                             std::uint32_t /*voice_bit*/) {
  return JumpOnPass(state, program, true);
}

// decomp: SLUS_010.40 SoundVM_CA_LoopInf (src/SLUS_010.40/25AC.c:5992)
ControlResult RepeatLoopForever(ControlState& state, ProgramData program,
                                std::uint32_t /*voice_bit*/) {
  ProgramState& position = state.program;
  const auto iterations = static_cast<std::uint16_t>(
      position.loop_iterations[LoopDepth(position)] + 1);
  return ReturnToLoopStart(position, program, iterations);
}

// decomp: SLUS_010.40 SoundVM_FE15_unk (src/SLUS_010.40/25AC.c:6050)
ControlResult SetTimerDivisors(ControlState& state, ProgramData program,
                               std::uint32_t /*voice_bit*/) {
  const auto operands = ReadOperands(state, program, 2);
  if (!operands) return Refused(ControlStatus::kTruncated);
  MusicLayerState& layer = state.layer;
  layer.timer_lower = (*operands)[0];
  layer.timer_upper = (*operands)[1];
  state.program.counter += 2;
  layer.timer_lower_count = 0;
  layer.timer_upper_count = 0;
  return {};
}

// decomp: SLUS_010.40 SoundVM_FE16_unk (src/SLUS_010.40/25AC.c:6059)
ControlResult SetTimerCount(ControlState& state, ProgramData program,
                            std::uint32_t /*voice_bit*/) {
  const auto operands = ReadOperands(state, program, 2);
  if (!operands) return Refused(ControlStatus::kTruncated);
  state.layer.timer_top = Word((*operands)[0], (*operands)[1]);
  state.program.counter += 2;
  return {};
}

// decomp: SLUS_010.40 SoundVM_FE0B_unk (src/SLUS_010.40/25AC.c:6162)
// The program counter stays on the operands: the caller starts the programs
// and then advances it past their four bytes, as the original does.
ControlResult PlayEmbeddedSoundEffect(ControlState& state, ProgramData program,
                                      std::uint32_t /*voice_bit*/) {
  const auto operands = ReadOperands(state, program, kEmbeddedOperands);
  if (!operands) return Refused(ControlStatus::kTruncated);
  const std::uint32_t counter = state.program.counter;
  const auto first =
      EmbeddedProgram(program, counter, Word((*operands)[0], (*operands)[1]));
  const auto second = EmbeddedProgram(program, counter + 2,
                                      Word((*operands)[2], (*operands)[3]));
  if (!first || !second) return Refused(ControlStatus::kTargetOutside);
  return {
      ControlStatus::kApplied,
      EmbeddedSoundEffect{
          *first, *second, std::uint32_t{state.pan} >> kEmbeddedPanShift,
          static_cast<std::uint32_t>(state.volume >> kEmbeddedVolumeShift)}};
}

// decomp: SLUS_010.40 SoundVM_FE1C_unk (src/SLUS_010.40/25AC.c:6202)
ControlResult SelectNoteRemapTable(ControlState& state, ProgramData program,
                                   std::uint32_t /*voice_bit*/) {
  const auto operands = ReadOperands(state, program, 1);
  if (!operands) return Refused(ControlStatus::kTruncated);
  state.layer.note_remap_offset = (*operands)[0];
  ++state.program.counter;
  return {};
}

// decomp: SLUS_010.40 SoundVM_FE1D_MarkVoicesKeyed
// (src/SLUS_010.40/25AC.c:6209)
ControlResult MarkChannelKeyed(ControlState& state, ProgramData /*program*/,
                               std::uint32_t voice_bit) {
  state.layer.keyed_voices |= voice_bit;
  return {};
}

// decomp: SLUS_010.40 SoundVM_FE1E_ClearVoicesKeyed
// (src/SLUS_010.40/25AC.c:6215)
ControlResult ClearChannelKeyed(ControlState& state, ProgramData /*program*/,
                                std::uint32_t voice_bit) {
  state.layer.keyed_voices &= ~voice_bit;
  return {};
}
}  // namespace VS::Audio::Seq
