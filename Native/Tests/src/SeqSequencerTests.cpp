#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SeqSequencer.hpp"
#include "Audio/SeqSequencerLive.hpp"
#include "Audio/VoiceOutput.hpp"
#include "Content/ModuleData.h"
#include "Core/Memory/Heap.hpp"
#include "Core/Memory/HeapLive.hpp"

extern "C" {
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/Sequencer.h"
}

namespace {
namespace Seq = VS::Audio::Seq;
using Seq::ChannelGroup;
using Seq::CommandOutcome;
using Seq::PlaybackRefusal;
using Seq::StepRefusal;
using VS::Audio::Samples::InstrumentRecord;

constexpr std::uint32_t kChannelBit = 0x2;
constexpr std::uint8_t kEndOfTrack = 0xA0;
// A command the fake host runs by returning to the start of the program.
constexpr std::uint8_t kRewindOpcode = 0xC8;

using ExtendedLengths = std::array<std::uint8_t, Seq::kExtendedOpcodeCount>;
using NoteLengths = std::array<std::uint16_t, Seq::kNoteLengthCount>;
using Semitones = std::array<std::uint32_t, Seq::kSemitoneCount>;
using PitchOffsets = std::array<std::uint8_t, Seq::kRandomPitchOffsetCount>;

constexpr Semitones SemitoneTable() {
  Semitones table{};
  for (std::size_t i = 0; i < table.size(); ++i) {
    table[i] = 0x1000 + static_cast<std::uint32_t>(i) * 0x100;
  }
  return table;
}

constexpr std::array<std::uint8_t, Seq::kOpcodeCount> kOpcodeLengths{};
constexpr ExtendedLengths kExtendedOpcodeLengths{};
constexpr NoteLengths kNoteLengths{192, 96, 48, 24, 12, 6, 3, 32, 16, 8, 4, 0};
constexpr Semitones kSemitones = SemitoneTable();
constexpr std::array<std::int8_t, 16> kNoteRemaps{};
constexpr PitchOffsets kRandomPitchOffsets{};

// Runs the step over `program` with zeroed opcode lengths, so every command
// is one byte, and records every command and refusal.
class FakeStepHost final : public Seq::StepHost {
 public:
  explicit FakeStepHost(Seq::ProgramData program) : program_(program) {}

  Seq::ProgramData Program() override { return program_; }
  const Seq::SequencerTables& Tables() override { return tables_; }

  CommandOutcome RunCommand(Seq::StepState& state, std::uint8_t opcode,
                            std::uint32_t /*channel_bit*/) override {
    commands.push_back(opcode);
    if (opcode == kEndOfTrack) return CommandOutcome::kRan;
    if (opcode == kRewindOpcode) state.program.counter = 0;
    return outcome;
  }

  CommandOutcome RunExtendedCommand(Seq::StepState& /*state*/,
                                    std::uint8_t opcode,
                                    std::uint32_t /*channel_bit*/) override {
    extended_commands.push_back(opcode);
    return CommandOutcome::kRan;
  }

  std::optional<InstrumentRecord> Instrument(std::int32_t /*index*/) override {
    return std::nullopt;
  }
  std::span<const std::uint8_t> ChannelKeymap() override { return {}; }
  std::span<const std::uint8_t> DrumKeymap() override { return {}; }
  bool HasLfoWaveform(std::uint16_t /*shape*/) override { return true; }
  void Refuse(StepRefusal refusal) override { refusals.push_back(refusal); }

  CommandOutcome outcome = CommandOutcome::kRan;
  std::vector<std::uint8_t> commands;
  std::vector<std::uint8_t> extended_commands;
  std::vector<StepRefusal> refusals;

 private:
  Seq::ProgramData program_;
  Seq::SequencerTables tables_{kOpcodeLengths, kExtendedOpcodeLengths,
                               kNoteLengths,   kSemitones,
                               kNoteRemaps,    kRandomPitchOffsets};
};

Seq::StepState AtCounter(std::uint32_t counter) {
  Seq::StepState state;
  state.program.counter = counter;
  return state;
}

using Refusals = std::vector<StepRefusal>;
using Commands = std::vector<std::uint8_t>;
}  // namespace

TEST_CASE("Instrument lookups move bank instruments to their sample bank",
          "[audio-seq-control][audio]") {
  CHECK(Seq::ResolveSoundEffectInstrument(2, 0x80) == 0xA0);
  CHECK(Seq::ResolveSoundEffectInstrument(4, 0x9F) == 0xDF);
  CHECK(Seq::ResolveSoundEffectInstrument(6, 0x85) == 0xA5);
  CHECK(Seq::ResolveMusicInstrument(0x40, 0x40) == 0x60);
  CHECK(Seq::ResolveMusicInstrument(0x40, 0x7F) == 0x9F);
}

TEST_CASE("Instrument lookups keep instruments outside a bank's range",
          "[audio-seq-control][audio]") {
  CHECK(Seq::ResolveSoundEffectInstrument(0, 0x80) == 0x80);
  CHECK(Seq::ResolveSoundEffectInstrument(2, 0xA0) == 0xA0);
  CHECK(Seq::ResolveSoundEffectInstrument(2, -1) == -1);
  CHECK(Seq::ResolveMusicInstrument(0x40, 0x80) == 0x80);
  CHECK(Seq::ResolveMusicInstrument(0x40, 0x3F) == 0x3F);
  CHECK(Seq::ResolveMusicInstrument(0, 0x50) == 0x50);
}

TEST_CASE("Note pitches shift by octaves from the instrument's root note",
          "[audio-seq-control][audio]") {
  const InstrumentRecord instrument{0, 0, 0, 60, 0, 0};
  const auto pitch = [&instrument](std::int32_t note, std::int16_t fine_tune) {
    return Seq::CalculateNotePitch(kSemitones, instrument,
                                   {note, fine_tune, 0x700});
  };
  CHECK(pitch(60, 0) == Seq::NotePitch{0x1000, 7});
  CHECK(pitch(72, 0) == Seq::NotePitch{0x2000, 0xE});
  CHECK(pitch(59, 0) == Seq::NotePitch{0xD80, 3});
  CHECK(pitch(61, 0) == Seq::NotePitch{0x1100, 7});
  // A shift of 32 or more octaves acts as the SPU's five-bit shift amount
  // up, and halves to nothing down.
  CHECK(pitch(60 + 12 * 33, 0) == Seq::NotePitch{0x2000, 0xE});
  CHECK(pitch(-1000, 0) == Seq::NotePitch{0, 0});
  CHECK(pitch(60, 0x40) == Seq::NotePitch{0x1000, 0x800});
  CHECK(pitch(60, -0x40) == Seq::NotePitch{0x1000, -0x400});
}

TEST_CASE("Instrument fine tune scales the semitone pitch",
          "[audio-seq-control][audio]") {
  const Seq::PitchRequest request{60, 0, 0};
  CHECK(Seq::CalculateNotePitch(kSemitones, {0, 0, 0x10, 60, 0, 0}, request)
            .pitch == 0x1002);
  CHECK(Seq::CalculateNotePitch(kSemitones, {0, 0, -256, 60, 0, 0}, request)
            .pitch == 0xFF0);
}

TEST_CASE("Selecting an instrument copies its sample and envelope",
          "[audio-seq-control][audio]") {
  Seq::ChannelState channel;
  channel.voice.dirty_flags = 1;
  Seq::SelectChannelInstrument(channel, 0x61,
                               {0x4000, 0x4100, 3, 60, 0x8F00, 0x1234});
  CHECK(channel.instrument_index == 0x61);
  CHECK(channel.voice == Seq::VoiceState{1 | Seq::kInstrumentDirty, 0x4000,
                                         0x4100, 0x8F00, 0x1234, 0});
}

TEST_CASE("A channel step refuses opcodes outside the sequence and finishes",
          "[audio-seq-control][audio]") {
  const std::array<std::uint8_t, 2> program{0x10, 0xF0};
  FakeStepHost host(program);
  Seq::StepState state = AtCounter(2);
  Seq::ExecuteChannelStep(state, host, kChannelBit);
  CHECK(host.refusals == Refusals{StepRefusal::kOpcodeOutside});
  CHECK(host.commands == Commands{kEndOfTrack});
  CHECK(state.program.counter == 2);
  CHECK(state.program.step_counter == 0);
  CHECK(state.layer.pending_key_off == static_cast<std::int32_t>(kChannelBit));

  // A timed note whose length byte lies past the end keeps its counter.
  FakeStepHost timed(program);
  state = AtCounter(1);
  Seq::ExecuteChannelStep(state, timed, kChannelBit);
  CHECK(timed.refusals == Refusals{StepRefusal::kOperandsOutside});
  CHECK(timed.commands == Commands{kEndOfTrack});
  CHECK(state.program.counter == 1);
}

TEST_CASE("A channel step refuses an extended opcode without its operand",
          "[audio-seq-control][audio]") {
  const std::array<std::uint8_t, 1> trailing{0xFE};
  FakeStepHost truncated(trailing);
  Seq::StepState state = AtCounter(0);
  Seq::ExecuteChannelStep(state, truncated, kChannelBit);
  CHECK(truncated.refusals == Refusals{StepRefusal::kOperandsOutside});
  CHECK(state.program.counter == 0);
}

TEST_CASE("A channel step refuses extended opcodes it cannot run",
          "[audio-seq-control][audio]") {
  const std::array<std::uint8_t, 2> unknown{0xFE, 0x20};
  FakeStepHost unknown_host(unknown);
  Seq::StepState state = AtCounter(0);
  Seq::ExecuteChannelStep(state, unknown_host, kChannelBit);
  CHECK(unknown_host.refusals == Refusals{StepRefusal::kUnknownExtendedOpcode});
  CHECK(unknown_host.extended_commands.empty());
  CHECK(unknown_host.commands == Commands{kEndOfTrack});
  CHECK(state.program.counter == 0);
}

TEST_CASE("A channel step runs known extended opcodes after their prefix",
          "[audio-seq-control][audio]") {
  const std::array<std::uint8_t, 3> known{0xFE, 0x06, kEndOfTrack};
  FakeStepHost known_host(known);
  Seq::StepState state = AtCounter(0);
  Seq::ExecuteChannelStep(state, known_host, kChannelBit);
  CHECK(known_host.refusals.empty());
  CHECK(known_host.extended_commands == Commands{0x06});
  CHECK(known_host.commands == Commands{kEndOfTrack});
  CHECK(state.program.counter == 3);
  CHECK(state.program.step_counter == 2);
}

TEST_CASE("A refused or missing command finishes the channel",
          "[audio-seq-control][audio]") {
  const std::array<std::uint8_t, 2> program{0xC0, 0x10};
  for (const auto& [outcome, refusal] :
       {std::pair(CommandOutcome::kRefused, StepRefusal::kCommandRefused),
        std::pair(CommandOutcome::kMissing, StepRefusal::kCommandMissing)}) {
    FakeStepHost host(program);
    host.outcome = outcome;
    Seq::StepState state = AtCounter(0);
    Seq::ExecuteChannelStep(state, host, kChannelBit);
    CHECK(host.refusals == Refusals{refusal});
    CHECK(host.commands == Commands{0xC0, kEndOfTrack});
    CHECK(state.layer.pending_key_off ==
          static_cast<std::int32_t>(kChannelBit));
  }
}

TEST_CASE("A channel that never reaches a note stops at the command limit",
          "[audio-seq-control][audio]") {
  const std::array<std::uint8_t, 1> program{kRewindOpcode};
  FakeStepHost host(program);
  Seq::StepState state = AtCounter(0);
  Seq::ExecuteChannelStep(state, host, kChannelBit);
  CHECK(host.refusals == Refusals{StepRefusal::kCommandLimit});
  CHECK(host.commands.size() == 0x10001U);
  CHECK(host.commands.back() == kEndOfTrack);
}

TEST_CASE("Track stops and stopping endless loops end the track",
          "[audio-seq-control][audio]") {
  const std::array<std::uint8_t, 1> stop{0xFF};
  FakeStepHost stop_host(stop);
  Seq::StepState state = AtCounter(0);
  Seq::ExecuteChannelStep(state, stop_host, kChannelBit);
  CHECK(stop_host.commands == Commands{kEndOfTrack});
  CHECK(stop_host.refusals.empty());

  const std::array<std::uint8_t, 1> endless{0xCA};
  FakeStepHost endless_host(endless);
  state = AtCounter(0);
  state.channel.update_flags = Seq::kStopAtEndlessLoop;
  Seq::ExecuteChannelStep(state, endless_host, kChannelBit);
  CHECK(endless_host.commands == Commands{kEndOfTrack});
  CHECK(state.scheduler.pending_key_off == kChannelBit);
  CHECK(state.program.counter == 1);
}

TEST_CASE("A timed rest takes its length operand and silences the channel",
          "[audio-seq-control][audio]") {
  const std::array<std::uint8_t, 3> program{0xFD, 0x30, 0x05};
  FakeStepHost host(program);
  Seq::StepState state = AtCounter(0);
  state.channel.gate_length = 0x10;
  state.channel.play_modes = 2;
  state.layer.active_notes = kChannelBit | 1;
  Seq::ExecuteChannelStep(state, host, kChannelBit);
  CHECK(host.refusals.empty());
  CHECK(host.commands.empty());
  CHECK(state.program.counter == 2);
  CHECK(state.program.step_counter == 1);
  CHECK(state.channel.note_length == 0x30);
  CHECK(state.channel.gate_length == 0x0E);
  CHECK(state.channel.stored_note_length == 0x30);
  CHECK(state.channel.play_modes == 0);
  CHECK(state.layer.active_notes == 1);
}

TEST_CASE("The next-note lookahead follows jumps inside the sequence",
          "[audio-seq-control][audio]") {
  const std::array<std::uint8_t, 6> jump{0xFE, 0x06, 0x03, 0x00, 0x11, 0x22};
  FakeStepHost jump_host(jump);
  Seq::StepState state = AtCounter(0);
  CHECK(Seq::PeekNextNote(state, jump_host) == 0x22);
  CHECK(jump_host.refusals.empty());
}

TEST_CASE("The next-note lookahead refuses jumps outside or to themselves",
          "[audio-seq-control][audio]") {
  Seq::StepState state = AtCounter(0);
  const std::array<std::uint8_t, 4> outside{0xFE, 0x06, 0x10, 0x00};
  FakeStepHost outside_host(outside);
  CHECK(Seq::PeekNextNote(state, outside_host) == kEndOfTrack);
  CHECK(outside_host.refusals == Refusals{StepRefusal::kLookaheadFailed});

  const std::array<std::uint8_t, 4> self{0xFE, 0x06, 0xFE, 0xFF};
  FakeStepHost self_host(self);
  CHECK(Seq::PeekNextNote(state, self_host) == kEndOfTrack);
  CHECK(self_host.refusals == Refusals{StepRefusal::kLookaheadFailed});
}

TEST_CASE("The next-note lookahead ends at endless loops",
          "[audio-seq-control][audio]") {
  const std::array<std::uint8_t, 1> endless{0xCA};
  FakeStepHost endless_host(endless);
  Seq::StepState state = AtCounter(0);
  state.program.loop_starts[0] = 0;
  CHECK(Seq::PeekNextNote(state, endless_host) == kEndOfTrack);
  CHECK(endless_host.refusals == Refusals{StepRefusal::kLookaheadFailed});
  FakeStepHost stopping_host(endless);
  state.channel.update_flags = Seq::kStopAtEndlessLoop;
  CHECK(Seq::PeekNextNote(state, stopping_host) == kEndOfTrack);
  CHECK(stopping_host.refusals.empty());
}

namespace {
// Records the driver effects of a tick; channel timers and effect slides are
// plain arrays.
class FakePlaybackHost final : public Seq::PlaybackHost {
 public:
  std::int32_t ReadTimer() override {
    return timer_reads[timer_index++ % timer_reads.size()];
  }
  void ReleasePendingVoices(Seq::PlaybackState& /*state*/) override {
    ++released;
  }
  void DetachSecondaryLayer(Seq::PlaybackState& state) override {
    state.secondary.reset();
  }
  void PromoteSecondaryLayer(Seq::PlaybackState& state) override {
    state.active = *state.secondary;
    state.secondary.reset();
  }
  void StartPendingMusic(Seq::PlaybackState& state) override {
    state.music_pending = false;
  }
  void CommitVoices(Seq::PlaybackState& /*state*/) override { ++committed; }
  Seq::ChannelTimers Timers(ChannelGroup group, std::size_t index) override {
    return group == ChannelGroup::kSoundEffects ? effect_timers.at(index)
                                                : music_timers.at(index);
  }
  void SetTimers(ChannelGroup group, std::size_t index,
                 const Seq::ChannelTimers& timers) override {
    (group == ChannelGroup::kSoundEffects ? effect_timers.at(index)
                                          : music_timers.at(index)) = timers;
  }
  void StepChannel(Seq::PlaybackState& /*state*/, ChannelGroup group,
                   std::size_t index) override {
    stepped.emplace_back(group, index);
  }
  void UpdateChannelSlides(Seq::PlaybackState& /*state*/,
                           ChannelGroup /*group*/,
                           std::size_t /*index*/) override {}
  void ActivatePrimaryLayer(Seq::PlaybackState& /*state*/) override {}
  void InvalidateMusicVolumes(Seq::PlaybackState& /*state*/,
                              ChannelGroup /*group*/) override {}
  void MarkMusicPitchDirty() override { ++pitch_marks; }
  Seq::EffectSlides EffectSlidesOf(std::size_t index) override {
    ++effect_slide_reads;
    return effect_slides.at(index);
  }
  void SetEffectSlides(std::size_t index,
                       const Seq::EffectSlides& slides) override {
    effect_slides.at(index) = slides;
  }
  void WriteDiscVolume(Seq::PlaybackState& /*state*/) override {
    ++disc_writes;
  }
  void UpdateCutsceneStream(Seq::PlaybackState& /*state*/) override {}
  void FinishTick(Seq::PlaybackState& /*state*/) override { ++finished; }
  void Refuse(PlaybackRefusal refusal) override { refusals.push_back(refusal); }

  std::vector<std::int32_t> timer_reads{0};
  std::size_t timer_index = 0;
  std::array<Seq::ChannelTimers, Seq::kMusicChannelCount> music_timers{};
  std::array<Seq::ChannelTimers, Seq::kEffectChannelCount> effect_timers{};
  std::array<Seq::EffectSlides, Seq::kEffectChannelCount> effect_slides{};
  std::vector<std::pair<ChannelGroup, std::size_t>> stepped;
  std::vector<PlaybackRefusal> refusals;
  int released = 0;
  int committed = 0;
  int pitch_marks = 0;
  int effect_slide_reads = 0;
  int disc_writes = 0;
  int finished = 0;
};

using PlaybackRefusals = std::vector<PlaybackRefusal>;
}  // namespace

TEST_CASE("Global slides step the disc volume and the music pitch",
          "[audio-seq-control][audio]") {
  FakePlaybackHost host;
  Seq::PlaybackState state;
  state.random_pitch_phase = 0xFF;
  state.disc_volume = {0x100000, 0x8000, 2};
  state.music_pitch_scale = {0xFF8000, 0x8000, 1};
  Seq::UpdateGlobalSlides(state, host);
  CHECK(state.random_pitch_phase == 0);
  CHECK(state.disc_volume == Seq::LevelSlide{0x108000, 0x8000, 1});
  CHECK(host.disc_writes == 1);
  CHECK(state.music_pitch_scale.value == 0x1000000);
  CHECK(host.pitch_marks == 1);
}

TEST_CASE("Global slides step only the sound-effect channels' levels",
          "[audio-seq-control][audio]") {
  FakePlaybackHost host;
  Seq::PlaybackState state;
  state.scheduler.active_voices = 0x1001;
  host.effect_slides[0].volume_length = 1;
  host.effect_slides[0].volume_step = 0x100;
  Seq::UpdateGlobalSlides(state, host);
  // Voice 0 names no sound-effect channel; the original walks it forever.
  CHECK(host.refusals ==
        PlaybackRefusals{PlaybackRefusal::kEffectVoiceOutsideChannels});
  CHECK(host.effect_slide_reads == 1);
  CHECK(host.effect_slides[0].volume == 0x100);
  CHECK(host.effect_slides[0].dirty_flags == Seq::kVolumeDirty);
}

TEST_CASE("A music tick steps channels whose note ends",
          "[audio-seq-control][audio]") {
  FakePlaybackHost host;
  Seq::PlaybackState state;
  state.active.active_channels = 0x6;
  state.active.tempo = static_cast<std::int32_t>(0xFFFF0000U);
  state.active.tick_accumulator = 1;
  host.music_timers[1] = {1, 5, 0, 0};
  host.music_timers[2] = {4, 1, 0, 0};
  CHECK(Seq::UpdateMusicSequence(state, host, ChannelGroup::kMusic) == 0x6);
  CHECK(state.active.tick_accumulator == 0);
  CHECK(host.stepped == std::vector<std::pair<ChannelGroup, std::size_t>>{
                            {ChannelGroup::kMusic, 1}});
  CHECK(host.music_timers[2].note_length == 3);
  CHECK(state.active.pending_key_off == 0x4);
  CHECK(host.refusals.empty());
}

TEST_CASE("Skipping track loops is bounded when the timer never ends one",
          "[audio-seq-control][audio]") {
  FakePlaybackHost host;
  Seq::PlaybackState state;
  state.active.tempo = 0x10000;
  state.active.tick_accumulator = 0xFFFF;
  state.active.timer_lower = 1;
  state.active.timer_upper = 1;
  state.music_loops_remaining = 3;
  Seq::UpdateMusicSequence(state, host, ChannelGroup::kMusic);
  CHECK(state.active.timer_top == 3);
  CHECK(state.music_loops_remaining == 0);
  CHECK(host.refusals.empty());

  state.active.tick_accumulator = 0xFFFF;
  state.active.timer_lower = 0;
  state.music_loops_remaining = 2;
  Seq::UpdateMusicSequence(state, host, ChannelGroup::kMusic);
  CHECK(host.refusals ==
        PlaybackRefusals{PlaybackRefusal::kLoopFastForwardLimit});
  CHECK(state.music_loops_remaining == 2);

  // The secondary layer's tick needs a secondary layer.
  CHECK(Seq::UpdateMusicSequence(state, host, ChannelGroup::kSecondaryMusic) ==
        0);
  CHECK(host.refusals.back() == PlaybackRefusal::kSecondaryLayerMissing);
}

TEST_CASE("A driver tick records its cost and refuses a missing layer",
          "[audio-seq-control][audio]") {
  FakePlaybackHost host;
  host.timer_reads = {100, 50};
  Seq::PlaybackState state;
  state.tick_counter = 3;
  state.recent_tick_costs = {1, 2, 3, 4};
  CHECK(Seq::TickPlayback(state, host) == 0x44E8 - 50);
  CHECK(state.tick_counter == 4);
  CHECK(state.random_pitch_phase == 1);
  CHECK(state.recent_tick_costs ==
        std::array<std::int32_t, 4>{2, 3, 4, 0x44E8 - 50});
  CHECK(state.recent_tick_cost_total == 9 + 0x44E8 - 50);
  CHECK(host.finished == 1);
  CHECK(host.refusals.empty());

  // A silenced secondary layer that is missing is refused, not read.
  state.control_latches = 0x100;
  host.timer_index = 0;
  Seq::TickPlayback(state, host);
  CHECK(host.refusals ==
        PlaybackRefusals{PlaybackRefusal::kSecondaryLayerMissing});
  CHECK(host.finished == 2);
}

namespace {
using OpcodeHandler = std::decay_t<decltype(Audio_Seq_kOpcodeHandlers[0])>;

int track_end_calls = 0;

void RecordTrackEnd(Audio_SeqChannel* /*channel*/, int /*channel_bit*/) {
  ++track_end_calls;
}

// A music channel stepping a program in a live game heap block, over saved
// and restored sequencer, mixer and driver state. The track end handler
// only counts its calls.
struct LiveStepFixture {
  Audio_MusicConfig* saved_config = Audio_Seq_activeMusicConfig;
  Audio_MusicConfig* saved_secondary = Audio_Seq_secondaryMusicConfig;
  Audio_VoiceScheduler saved_scheduler = Audio_Mixer_voiceScheduler;
  Audio_DriverFlags saved_latches = Audio_Driver_controlLatches;
  OpcodeHandler saved_track_end = Audio_Seq_kOpcodeHandlers[0];
  alignas(VS::Core::Memory::kHeapUnitBytes) std::array<std::byte, 256> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  Audio_MusicConfig config{};
  Audio_SeqChannel channel{};
  std::span<std::uint8_t> block;

  LiveStepFixture() {
    Audio_Seq_activeMusicConfig = &config;
    Audio_Seq_secondaryMusicConfig = nullptr;
    Audio_Mixer_voiceScheduler = {};
    Audio_Driver_controlLatches = {};
    Audio_Seq_kOpcodeHandlers[0] = &RecordTrackEnd;
    track_end_calls = 0;
    auto* const data =
        static_cast<std::uint8_t*>(VS::Core::Memory::AllocateLiveFromLowEnd(8));
    REQUIRE(data != nullptr);
    block = {data, VS::Core::Memory::LivePayloadCapacity(data).value()};
    config.sequenceBase = reinterpret_cast<std::uintptr_t>(data);
    channel.programCounter = At(0);
  }

  ~LiveStepFixture() {
    Audio_Seq_kOpcodeHandlers[0] = saved_track_end;
    Audio_Seq_activeMusicConfig = saved_config;
    Audio_Seq_secondaryMusicConfig = saved_secondary;
    Audio_Mixer_voiceScheduler = saved_scheduler;
    Audio_Driver_controlLatches = saved_latches;
  }

  LiveStepFixture(const LiveStepFixture&) = delete;
  LiveStepFixture& operator=(const LiveStepFixture&) = delete;

  [[nodiscard]] char* At(std::size_t offset) const {
    return reinterpret_cast<char*>(block.data()) + offset;
  }
};
}  // namespace

TEST_CASE("A live channel step stays inside its program's heap block",
          "[audio-seq-control][audio]") {
  LiveStepFixture fixture;
  fixture.block[0] = 0x8F;
  fixture.block[1] = 0x8F;
  const std::uint64_t refused = Seq::LiveRefusalCount();
  Seq::LiveExecuteChannelStep(&fixture.channel, static_cast<int>(kChannelBit));
  CHECK(fixture.channel.programCounter == fixture.At(1));
  CHECK(fixture.channel.opcodeStepCounter == 1);
  CHECK(Seq::LiveRefusalCount() == refused);
  CHECK(track_end_calls == 0);
}

TEST_CASE("A live channel step at the end of its heap block ends the track",
          "[audio-seq-control][audio]") {
  LiveStepFixture fixture;
  const std::uint64_t refused = Seq::LiveRefusalCount();
  char* const end = fixture.At(fixture.block.size());
  fixture.channel.programCounter = end;
  fixture.channel.opcodeStepCounter = 1;
  fixture.config.pendingKeyOff = 0;
  Seq::LiveExecuteChannelStep(&fixture.channel, static_cast<int>(kChannelBit));
  CHECK(fixture.channel.programCounter == end);
  CHECK(fixture.channel.opcodeStepCounter == 1);
  CHECK(track_end_calls == 1);
  CHECK(fixture.config.pendingKeyOff == static_cast<int>(kChannelBit));
  CHECK(Seq::LiveRefusalCount() == refused + 1);
  Seq::LiveExecuteChannelStep(nullptr, static_cast<int>(kChannelBit));
  CHECK(Seq::LiveRefusalCount() == refused + 2);
}

TEST_CASE("The live driver tick refuses the active layer as its secondary",
          "[audio-seq-control][audio]") {
  LiveStepFixture fixture;
  Audio_Seq_secondaryMusicConfig = &fixture.config;
  fixture.config.tempo = 0x1234;
  const std::uint64_t refused = Seq::LiveRefusalCount();
  CHECK(Seq::LiveTickPlayback() == 0);
  Seq::LiveUpdateGlobalSlides();
  CHECK(Seq::LiveRefusalCount() == refused + 2);
  CHECK(fixture.config.tempo == 0x1234);
  Audio_Seq_activeMusicConfig = nullptr;
  CHECK(Seq::LiveTickPlayback() == 0);
  CHECK(Seq::LiveRefusalCount() == refused + 3);
}

TEST_CASE("Sequencer functions keep no module bindings",
          "[audio-seq-control][audio]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  for (const std::uint32_t address :
       {0x8001924CU, 0x80019614U, 0x80019704U, 0x800199C4U, 0x80019A58U,
        0x80019FC4U, 0x8001A1F4U, 0x8001A22CU, 0x8001A258U, 0x8001A4BCU,
        0x8001A64CU, 0x8001A8D8U, 0x8001B060U, 0x8001B094U, 0x8001B0CCU}) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40", address) ==
          nullptr);
  }
}
