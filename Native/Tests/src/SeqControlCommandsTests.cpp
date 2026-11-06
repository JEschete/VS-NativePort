#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqControlCommandsLive.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Content/ModuleData.h"
#include "Core/Log.hpp"
#include "Core/Memory/Heap.hpp"
#include "Core/Memory/HeapLive.hpp"

extern "C" {
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Channel.h"
}

namespace {
namespace Seq = VS::Audio::Seq;
using Seq::ControlResult;
using Seq::ControlState;
using Seq::ControlStatus;
using Seq::kOutsideProgram;

constexpr std::uint32_t kChannelBit = 0x4;
constexpr ControlResult kApplied{};

constexpr ControlResult Refused(ControlStatus status) {
  return {status, std::nullopt};
}

constexpr auto kTruncated = Refused(ControlStatus::kTruncated);
constexpr auto kTargetOutside = Refused(ControlStatus::kTargetOutside);

template <typename... Values>
std::array<std::uint8_t, sizeof...(Values)> Bytes(Values... values) {
  return {static_cast<std::uint8_t>(values)...};
}

ControlState AtCounter(std::uint32_t counter) {
  ControlState state;
  state.program.counter = counter;
  return state;
}

struct OperandCommand {
  const char* name;
  Seq::ControlCommand command;
  std::size_t operands;
};

constexpr std::array kOperandCommands{
    OperandCommand{"SetTempo", Seq::SetTempo, 2},
    OperandCommand{"SlideTempo", Seq::SlideTempo, 3},
    OperandCommand{"SetReverbDepth", Seq::SetReverbDepth, 2},
    OperandCommand{"SlideReverbDepth", Seq::SlideReverbDepth, 3},
    OperandCommand{"JumpRelative", Seq::JumpRelative, 2},
    OperandCommand{"JumpAtThreshold", Seq::JumpAtThreshold, 3},
    OperandCommand{"CallRelative", Seq::CallRelative, 2},
    OperandCommand{"SetSequenceControlValue", Seq::SetSequenceControlValue, 1},
    OperandCommand{"SetVoiceAllocationStart", Seq::SetVoiceAllocationStart, 1},
    OperandCommand{"RepeatLoop", Seq::RepeatLoop, 1},
    OperandCommand{"JumpOnLoopPass", Seq::JumpOnLoopPass, 3},
    OperandCommand{"ExitLoopOnPass", Seq::ExitLoopOnPass, 3},
    OperandCommand{"SetTimerDivisors", Seq::SetTimerDivisors, 2},
    OperandCommand{"SetTimerCount", Seq::SetTimerCount, 2},
    OperandCommand{"PlayEmbeddedSoundEffect", Seq::PlayEmbeddedSoundEffect, 4},
    OperandCommand{"SelectNoteRemapTable", Seq::SelectNoteRemapTable, 1},
};
}  // namespace

TEST_CASE("Tempo and reverb commands decode operands at the original widths",
          "[audio-seq-control][audio]") {
  const auto program = Bytes(0x34, 0x12, 0x10, 0xFF);
  ControlState state = AtCounter(0);
  state.layer.tempo_slide_length = 9;
  CHECK(Seq::SetTempo(state, program, kChannelBit) == kApplied);
  CHECK(state.layer.tempo == 0x12340000);
  CHECK(state.layer.tempo_slide_length == 0);
  CHECK(state.program.counter == 2);
  state.layer.reverb_depth_slide_length = 4;
  CHECK(Seq::SetReverbDepth(state, program, kChannelBit) == kApplied);
  CHECK(state.layer.reverb_depth == static_cast<std::int32_t>(0xFFF10000U));
  CHECK(state.layer.reverb_depth_slide_length == 0);
  CHECK(state.driver_update_flags == Seq::kReverbDepthUpdate);
  CHECK(state.program.counter == 4);
}

TEST_CASE("Tempo slides start from the whole part of the tempo",
          "[audio-seq-control][audio]") {
  ControlState state = AtCounter(0);
  state.layer.tempo = 0x01001234;
  CHECK(Seq::SlideTempo(state, Bytes(0x00, 0x00, 0x02), kChannelBit) ==
        kApplied);
  CHECK(state.layer.tempo == 0x01000000);
  CHECK(state.layer.tempo_slide_length == 0x100);
  CHECK(state.layer.tempo_slide_step == 0x10000);
  CHECK(state.program.counter == 3);
}

TEST_CASE("Reverb depth slides start from the whole part of the depth",
          "[audio-seq-control][audio]") {
  ControlState state = AtCounter(0);
  state.layer.reverb_depth = 0x00012345;
  CHECK(Seq::SlideReverbDepth(state, Bytes(0x04, 0x00, 0x80), kChannelBit) ==
        kApplied);
  CHECK(state.layer.reverb_depth == 0x00012000);
  CHECK(state.layer.reverb_depth_slide_length == 4);
  CHECK(state.layer.reverb_depth_slide_step == -33572864);
  CHECK(state.driver_update_flags == 0);
}

TEST_CASE("Commands refuse a sequence that ends inside their operands",
          "[audio-seq-control][audio]") {
  std::array<std::uint8_t, 8> program{};
  program.fill(1);
  for (const OperandCommand& entry : kOperandCommands) {
    CAPTURE(entry.name);
    const auto short_by_one =
        static_cast<std::uint32_t>(program.size() - entry.operands + 1);
    for (const std::uint32_t counter : {short_by_one, kOutsideProgram}) {
      ControlState state = AtCounter(counter);
      state.layer.jump_threshold = 0xFF;
      const ControlState before = state;
      CHECK(entry.command(state, program, kChannelBit) == kTruncated);
      CHECK(state == before);
    }
    ControlState state =
        AtCounter(static_cast<std::uint32_t>(program.size() - entry.operands));
    CHECK(entry.command(state, program, kChannelBit).status !=
          ControlStatus::kTruncated);
  }
}

TEST_CASE("Relative jumps and calls refuse targets outside the sequence",
          "[audio-seq-control][audio]") {
  std::array<std::uint8_t, 16> program{};
  ControlState state = AtCounter(4);
  const ControlState before = state;
  program[4] = 0xF0;
  program[5] = 0xFF;
  CHECK(Seq::JumpRelative(state, program, kChannelBit) == kTargetOutside);
  CHECK(state == before);
  program[4] = 0x0C;
  program[5] = 0x00;
  CHECK(Seq::JumpRelative(state, program, kChannelBit) == kTargetOutside);
  CHECK(Seq::CallRelative(state, program, kChannelBit) == kTargetOutside);
  CHECK(state == before);
  program[4] = 0x0B;
  CHECK(Seq::JumpRelative(state, program, kChannelBit) == kApplied);
  CHECK(state.program.counter == 15);
}

TEST_CASE("Calls return to the byte after their operands",
          "[audio-seq-control][audio]") {
  std::array<std::uint8_t, 16> program{};
  ControlState state = AtCounter(4);
  program[4] = 0xFC;
  program[5] = 0xFF;
  CHECK(Seq::CallRelative(state, program, kChannelBit) == kApplied);
  CHECK(state.program.counter == 0);
  CHECK(state.program.return_address == 6);
  CHECK(Seq::ReturnFromCall(state, program, kChannelBit) == kApplied);
  CHECK(state.program.counter == 6);
  state.program.return_address = 16;
  CHECK(Seq::ReturnFromCall(state, program, kChannelBit) == kTargetOutside);
  CHECK(state.program.counter == 6);
}

TEST_CASE("Threshold jumps wait until the layer reaches the operand",
          "[audio-seq-control][audio]") {
  std::array<std::uint8_t, 8> program{};
  program[2] = 5;
  program[3] = 3;
  ControlState state = AtCounter(2);
  state.layer.jump_threshold = 4;
  CHECK(Seq::JumpAtThreshold(state, program, kChannelBit) == kApplied);
  CHECK(state.program.counter == 5);
  state.program.counter = 2;
  state.layer.jump_threshold = 5;
  CHECK(Seq::JumpAtThreshold(state, program, kChannelBit) == kApplied);
  CHECK(state.program.counter == 6);
  program[3] = 5;
  state.program.counter = 2;
  const ControlState before = state;
  CHECK(Seq::JumpAtThreshold(state, program, kChannelBit) == kTargetOutside);
  CHECK(state == before);
}

TEST_CASE("Pushing a loop past the stack wraps to its first entry",
          "[audio-seq-control][audio]") {
  const std::array<std::uint8_t, 8> program{};
  ControlState state = AtCounter(3);
  state.program.loop_depth = 3;
  state.program.step_counter = 9;
  CHECK(Seq::PushLoop(state, program, kChannelBit) == kApplied);
  CHECK(state.program.loop_depth == 0);
  CHECK(state.program.loop_starts[0] == 3);
  CHECK(state.program.loop_step_snapshots[0] == 9);
}

TEST_CASE("Loop commands keep the loop depth inside the four-entry stack",
          "[audio-seq-control][audio]") {
  std::array<std::uint8_t, 8> program{};
  program[6] = 2;
  // A depth past the stack selects the entry its low bits name.
  ControlState state = AtCounter(6);
  state.program.loop_depth = 5;
  state.program.loop_starts[1] = 2;
  state.program.loop_step_snapshots[1] = 7;
  CHECK(Seq::RepeatLoop(state, program, kChannelBit) == kApplied);
  CHECK(state.program.counter == 2);
  CHECK(state.program.loop_iterations[1] == 1);
  CHECK(state.program.step_counter == 7);
  state.program.counter = 6;
  CHECK(Seq::RepeatLoop(state, program, kChannelBit) == kApplied);
  CHECK(state.program.counter == 7);
  CHECK(state.program.loop_iterations[1] == 2);
  CHECK(state.program.loop_depth == 0);
}

TEST_CASE("Loop pass jumps act only on the selected pass",
          "[audio-seq-control][audio]") {
  std::array<std::uint8_t, 8> program{};
  program[0] = 2;
  program[1] = 4;
  ControlState state = AtCounter(0);
  state.program.loop_depth = 1;
  CHECK(Seq::JumpOnLoopPass(state, program, kChannelBit) == kApplied);
  CHECK(state.program.counter == 3);
  state.program.counter = 0;
  state.program.loop_iterations[1] = 1;
  CHECK(Seq::JumpOnLoopPass(state, program, kChannelBit) == kApplied);
  CHECK(state.program.counter == 5);
  CHECK(state.program.loop_depth == 1);
}

TEST_CASE("Exiting a loop on its pass leaves the loop",
          "[audio-seq-control][audio]") {
  std::array<std::uint8_t, 8> program{};
  program[0] = 2;
  program[1] = 4;
  ControlState state = AtCounter(0);
  state.program.loop_depth = 1;
  state.program.loop_iterations[1] = 1;
  CHECK(Seq::ExitLoopOnPass(state, program, kChannelBit) == kApplied);
  CHECK(state.program.counter == 5);
  CHECK(state.program.loop_depth == 0);

  // Pass 0 counts as pass 256.
  program[0] = 0;
  state = AtCounter(0);
  state.program.loop_iterations[0] = 0xFF;
  CHECK(Seq::JumpOnLoopPass(state, program, kChannelBit) == kApplied);
  CHECK(state.program.counter == 5);
}

TEST_CASE("Endless loops return to the loop start only inside the sequence",
          "[audio-seq-control][audio]") {
  const std::array<std::uint8_t, 8> program{};
  ControlState state = AtCounter(6);
  state.program.loop_depth = 2;
  state.program.loop_starts[2] = 3;
  state.program.loop_iterations[2] = 9;
  state.program.loop_step_snapshots[2] = -2;
  CHECK(Seq::RepeatLoopForever(state, program, kChannelBit) == kApplied);
  CHECK(state.program.counter == 3);
  CHECK(state.program.loop_iterations[2] == 10);
  CHECK(state.program.step_counter == 0xFFFE);
  state.program.loop_starts[2] = kOutsideProgram;
  const ControlState before = state;
  CHECK(Seq::RepeatLoopForever(state, program, kChannelBit) == kTargetOutside);
  CHECK(state == before);
  state.program.loop_iterations[2] = 0;
  CHECK(Seq::RepeatLoop(state, Bytes(5), kChannelBit) == kTruncated);
  state.program.counter = 0;
  CHECK(Seq::RepeatLoop(state, Bytes(5), kChannelBit) == kTargetOutside);
}

namespace {
// A track whose layer plays the test channel and channel 0.
ControlState TwoChannelTrack() {
  ControlState state;
  state.update_flags = 0x1234;
  state.music_loops_remaining = 3;
  state.layer.status_flags = 0x41;
  state.layer.music_id = 7;
  state.layer.active_channels = kChannelBit | 1;
  state.layer.active_notes = kChannelBit | 1;
  state.layer.keyed_voices = kChannelBit;
  state.layer.reverb_channels = kChannelBit;
  return state;
}
}  // namespace

TEST_CASE("Finishing a music channel clears only its bits from the layer",
          "[audio-seq-control][audio]") {
  ControlState state = TwoChannelTrack();
  CHECK(Seq::FinishChannel(state, {}, kChannelBit) == kApplied);
  CHECK(state.layer.active_channels == 1);
  CHECK(state.layer.active_notes == 1);
  CHECK(state.layer.keyed_voices == 0);
  CHECK(state.layer.reverb_channels == 0);
  CHECK(state.layer.music_id == 7);
  CHECK(state.update_flags == 0);
  CHECK(state.driver_update_flags == Seq::kChannelFinishedUpdate);
}

TEST_CASE("Finishing the last music channel of a track ends the track",
          "[audio-seq-control][audio]") {
  ControlState state = TwoChannelTrack();
  REQUIRE(Seq::FinishChannel(state, {}, kChannelBit) == kApplied);
  CHECK(Seq::StopOnInvalidInstruction(state, {}, 1) == kApplied);
  CHECK(state.layer.active_channels == 0);
  CHECK(state.layer.music_id == 0);
  CHECK(state.layer.status_flags == 0);
  CHECK(state.music_loops_remaining == 0);
}

TEST_CASE("Finishing a sound effect channel releases its scheduler voice",
          "[audio-seq-control][audio]") {
  ControlState state;
  state.kind = VS::Audio::Driver::ChannelKind::kSoundEffect;
  constexpr std::uint32_t kVoices = kChannelBit | 1;
  state.scheduler = {kVoices, kVoices, kVoices, kVoices, kVoices, 3,
                     4,       kVoices, kVoices, kVoices, 5};
  state.effect = {0x55, true};
  state.layer.active_channels = kChannelBit;
  const Seq::MusicLayerState layer = state.layer;
  CHECK(Seq::FinishChannel(state, {}, kChannelBit) == kApplied);
  CHECK(state.scheduler ==
        Seq::VoiceSchedulerState{1, 1, 1, kVoices, 1, 3, 4, 1, 1, 1, 5});
  CHECK(state.effect == Seq::EffectIdentity{});
  CHECK(state.layer == layer);
}

TEST_CASE("Replacement finishes and ignored opcodes follow the layer flags",
          "[audio-seq-control][audio]") {
  ControlState state;
  state.layer.active_channels = kChannelBit;
  const ControlState before = state;
  CHECK(Seq::FinishChannelOnReplacement(state, {}, kChannelBit) == kApplied);
  CHECK(state == before);
  CHECK(Seq::IgnoreInstruction(state, {}, kChannelBit) == kApplied);
  CHECK(state == before);
  state.layer.status_flags = Seq::kFinishOnReplacement;
  CHECK(Seq::FinishChannelOnReplacement(state, {}, kChannelBit) == kApplied);
  CHECK(state.layer.active_channels == 0);
  CHECK(state.layer.status_flags == 0);
}

TEST_CASE("Embedded sound effects name programs inside the sequence only",
          "[audio-seq-control][audio]") {
  std::array<std::uint8_t, 16> program{};
  program[2] = 4;
  ControlState state = AtCounter(2);
  state.pan = 0x7F00;
  state.volume = 0x3F800000;
  const ControlResult started =
      Seq::PlayEmbeddedSoundEffect(state, program, kChannelBit);
  CHECK(started.status == ControlStatus::kApplied);
  CHECK(started.sound_effect ==
        Seq::EmbeddedSoundEffect{8, std::nullopt, 0x7F, 0x7F});
  CHECK(state.program.counter == 2);

  state.volume = -0x40000000;
  program[4] = 0x0A;
  const ControlState before = state;
  CHECK(Seq::PlayEmbeddedSoundEffect(state, program, kChannelBit) ==
        kTargetOutside);
  CHECK(state == before);
  program[4] = 0x09;
  const ControlResult both =
      Seq::PlayEmbeddedSoundEffect(state, program, kChannelBit);
  CHECK(both.sound_effect ==
        Seq::EmbeddedSoundEffect{8, 15, 0x7F, 0xFFFFFF80U});
}

TEST_CASE("Timer commands store their divisors and count",
          "[audio-seq-control][audio]") {
  ControlState state = AtCounter(0);
  CHECK(Seq::SetTimerDivisors(state, Bytes(3, 5), kChannelBit) == kApplied);
  CHECK(std::pair(state.layer.timer_lower, state.layer.timer_upper) ==
        std::pair<std::uint16_t, std::uint16_t>(3, 5));
  state.program.counter = 0;
  CHECK(Seq::SetTimerCount(state, Bytes(0x34, 0x12), kChannelBit) == kApplied);
  CHECK(state.layer.timer_top == 0x1234);
}

TEST_CASE("Control value and voice allocation commands store their operands",
          "[audio-seq-control][audio]") {
  ControlState state = AtCounter(0);
  CHECK(Seq::SetSequenceControlValue(state, Bytes(0x81), kChannelBit) ==
        kApplied);
  CHECK(state.layer.sequence_control_value == 0x81);
  state.program.counter = 0;
  CHECK(Seq::SetVoiceAllocationStart(state, Bytes(9), kChannelBit) == kApplied);
  CHECK(state.layer.voice_search_start == 9);
  CHECK(Seq::ResetVoiceAllocationStart(state, {}, kChannelBit) == kApplied);
  CHECK(state.layer.voice_search_start == 0);
}

TEST_CASE("Remap and keyed commands store the table and the channel bit",
          "[audio-seq-control][audio]") {
  ControlState state = AtCounter(0);
  CHECK(Seq::SelectNoteRemapTable(state, Bytes(0x20), kChannelBit) == kApplied);
  CHECK(state.layer.note_remap_offset == std::optional<std::uint16_t>(0x20));
  CHECK(state.program.counter == 1);
  CHECK(Seq::MarkChannelKeyed(state, {}, kChannelBit) == kApplied);
  CHECK(state.layer.keyed_voices == kChannelBit);
  CHECK(Seq::ClearChannelKeyed(state, {}, kChannelBit) == kApplied);
  CHECK(state.layer.keyed_voices == 0);
}

namespace {
// A music channel whose program lies in a live game heap block, over saved
// and restored sequencer, mixer and driver state.
struct LiveControlFixture {
  Audio_MusicConfig* saved_config = Audio_Seq_activeMusicConfig;
  Audio_VoiceScheduler saved_scheduler = Audio_Mixer_voiceScheduler;
  Audio_DriverFlags saved_latches = Audio_Driver_controlLatches;
  u_int saved_loops = Audio_Seq_musicLoopsRemaining;
  alignas(VS::Core::Memory::kHeapUnitBytes) std::array<std::byte, 256> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  Audio_MusicConfig config{};
  Audio_SeqChannel channel{};
  std::span<std::uint8_t> block;

  LiveControlFixture() {
    Audio_Seq_activeMusicConfig = &config;
    Audio_Mixer_voiceScheduler = {};
    Audio_Driver_controlLatches = {};
    auto* const data =
        static_cast<std::uint8_t*>(VS::Core::Memory::AllocateLiveFromLowEnd(8));
    REQUIRE(data != nullptr);
    block = {data, VS::Core::Memory::LivePayloadCapacity(data).value()};
    config.sequenceBase = reinterpret_cast<std::uintptr_t>(data);
    channel.programCounter = At(0);
  }

  ~LiveControlFixture() {
    Audio_Seq_activeMusicConfig = saved_config;
    Audio_Mixer_voiceScheduler = saved_scheduler;
    Audio_Driver_controlLatches = saved_latches;
    Audio_Seq_musicLoopsRemaining = saved_loops;
  }

  LiveControlFixture(const LiveControlFixture&) = delete;
  LiveControlFixture& operator=(const LiveControlFixture&) = delete;

  [[nodiscard]] char* At(std::size_t offset) const {
    return reinterpret_cast<char*>(block.data()) + offset;
  }
};

// Counts the warnings the audio channel logs while it lives.
class WarningCounter {
 public:
  WarningCounter()
      : saved_(
            VS::Core::SetLogObserver([this](const VS::Core::LogRecord& record) {
              if (record.channel == "audio" &&
                  record.level == VS::Core::LogLevel::Warn) {
                ++count_;
              }
            })) {}
  ~WarningCounter() { VS::Core::SetLogObserver(saved_); }
  WarningCounter(const WarningCounter&) = delete;
  WarningCounter& operator=(const WarningCounter&) = delete;

  [[nodiscard]] int Count() const { return count_; }

 private:
  int count_ = 0;
  VS::Core::LogObserver saved_;
};

// How many refusal counts in (after, last] are reported (powers of two).
int ReportedRefusals(std::uint64_t after, std::uint64_t last) {
  int reports = 0;
  for (std::uint64_t count = after + 1; count <= last; ++count) {
    if ((count & (count - 1)) == 0) ++reports;
  }
  return reports;
}
}  // namespace

TEST_CASE("Live flow-control commands run over the channel's heap block",
          "[audio-seq-control][audio]") {
  LiveControlFixture fixture;
  fixture.block[0] = 0x34;
  fixture.block[1] = 0x12;
  fixture.block[2] = 0xFE;
  fixture.block[3] = 0xFF;
  const std::uint64_t refused = Seq::LiveRefusalCount();
  Seq::LiveSetTempo(&fixture.channel, static_cast<int>(kChannelBit));
  CHECK(fixture.config.tempo == 0x12340000);
  CHECK(fixture.channel.programCounter == fixture.At(2));
  Seq::LivePushLoop(&fixture.channel, static_cast<int>(kChannelBit));
  CHECK(fixture.channel.loopDepth == 1);
  CHECK(fixture.channel.loopStarts[1] == fixture.At(2));
  Seq::LiveJumpRelative(&fixture.channel, static_cast<int>(kChannelBit));
  CHECK(fixture.channel.programCounter == fixture.At(0));
  Seq::LiveMarkChannelKeyed(&fixture.channel, static_cast<int>(kChannelBit));
  CHECK(fixture.config.keyedVoices == kChannelBit);
  CHECK(Seq::LiveRefusalCount() == refused);
}

TEST_CASE("Live flow-control commands refuse reads past the heap block",
          "[audio-seq-control][audio]") {
  LiveControlFixture fixture;
  const WarningCounter warnings;
  char* const last = fixture.At(fixture.block.size() - 1);
  fixture.channel.programCounter = last;
  fixture.config.tempo = 0x100;
  const std::uint64_t refused = Seq::LiveRefusalCount();
  Seq::LiveSetTempo(&fixture.channel, static_cast<int>(kChannelBit));
  CHECK(fixture.channel.programCounter == last);
  CHECK(fixture.config.tempo == 0x100);
  Seq::LivePlayEmbeddedSoundEffect(&fixture.channel,
                                   static_cast<int>(kChannelBit));
  CHECK(fixture.channel.programCounter == last);

  // A jump back past the start of the block is refused too.
  fixture.channel.programCounter = fixture.At(0);
  fixture.block[0] = 0xF0;
  fixture.block[1] = 0xFF;
  Seq::LiveJumpRelative(&fixture.channel, static_cast<int>(kChannelBit));
  CHECK(fixture.channel.programCounter == fixture.At(0));
  CHECK(Seq::LiveRefusalCount() == refused + 3);
  CHECK(warnings.Count() == ReportedRefusals(refused, Seq::LiveRefusalCount()));
}

TEST_CASE("Live layer commands refuse without an active music layer",
          "[audio-seq-control][audio]") {
  LiveControlFixture fixture;
  Audio_Seq_activeMusicConfig = nullptr;
  fixture.block[0] = 0x34;
  fixture.block[1] = 0x12;
  fixture.block[2] = 0x01;
  fixture.block[3] = 0x00;
  const std::uint64_t refused = Seq::LiveRefusalCount();
  Seq::LiveSetTempo(&fixture.channel, static_cast<int>(kChannelBit));
  CHECK(fixture.channel.programCounter == fixture.At(0));
  Seq::LiveFinishChannel(&fixture.channel, static_cast<int>(kChannelBit));
  CHECK(Seq::LiveRefusalCount() == refused + 2);
  // Jumps need no layer.
  fixture.channel.programCounter = fixture.At(2);
  Seq::LiveJumpRelative(&fixture.channel, static_cast<int>(kChannelBit));
  CHECK(fixture.channel.programCounter == fixture.At(3));
  Seq::LiveSetTempo(nullptr, static_cast<int>(kChannelBit));
  CHECK(Seq::LiveRefusalCount() == refused + 3);
}

TEST_CASE("Flow-control handlers keep module bindings only in opcode tables",
          "[audio-seq-control][audio]") {
  using Handler = void (*)(Audio_SeqChannel*, int);
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const std::array<std::pair<std::uint32_t, Handler>, 27> kept{{
      {0x8001B138U, &Seq::LiveFinishChannel},
      {0x8001B20CU, &Seq::LiveFinishChannelOnReplacement},
      {0x8001B248U, &Seq::LiveSetTempo},
      {0x8001B294U, &Seq::LiveSlideTempo},
      {0x8001B334U, &Seq::LiveSetReverbDepth},
      {0x8001B37CU, &Seq::LiveSlideReverbDepth},
      {0x8001B424U, &Seq::LiveJumpRelative},
      {0x8001B450U, &Seq::LiveJumpAtThreshold},
      {0x8001B4A8U, &Seq::LiveCallRelative},
      {0x8001B4E0U, &Seq::LiveReturnFromCall},
      {0x8001B70CU, &Seq::LiveSetSequenceControlValue},
      {0x8001C378U, &Seq::LiveIgnoreInstruction},
      {0x8001C39CU, &Seq::LiveIgnoreInstruction},
      {0x8001C66CU, &Seq::LiveSetVoiceAllocationStart},
      {0x8001C68CU, &Seq::LiveResetVoiceAllocationStart},
      {0x8001C69CU, &Seq::LivePushLoop},
      {0x8001C6ECU, &Seq::LiveRepeatLoop},
      {0x8001C780U, &Seq::LiveJumpOnLoopPass},
      {0x8001C7ECU, &Seq::LiveExitLoopOnPass},
      {0x8001C868U, &Seq::LiveRepeatLoopForever},
      {0x8001C97CU, &Seq::LiveSetTimerDivisors},
      {0x8001C9B4U, &Seq::LiveSetTimerCount},
      {0x8001CBCCU, &Seq::LivePlayEmbeddedSoundEffect},
      {0x8001CC98U, &Seq::LiveSelectNoteRemapTable},
      {0x8001CCC8U, &Seq::LiveMarkChannelKeyed},
      {0x8001CCE8U, &Seq::LiveClearChannelKeyed},
      {0x8001CD2CU, &Seq::LiveStopOnInvalidInstruction},
  }};
  for (const auto& [address, handler] : kept) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40", address) ==
          reinterpret_cast<void*>(handler));
  }
  // SoundVM_nop is stored in no table; its callers call the typed command.
  CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40",
                                          0x8001B72CU) == nullptr);
}
