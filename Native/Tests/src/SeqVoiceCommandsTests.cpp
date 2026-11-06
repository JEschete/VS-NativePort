#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/LfoWaveforms.h"
#include "Audio/PlaybackReset.hpp"
#include "Audio/SeqChannelCommands.hpp"
#include "Audio/SeqEnvelopeCommands.hpp"
#include "Audio/SeqModulationCommands.hpp"
#include "Audio/SeqVoiceCommands.hpp"
#include "Audio/SeqVoiceCommandsLive.hpp"
#include "Audio/VoiceOutput.hpp"
#include "Content/ModuleData.h"
#include "Core/Log.hpp"
#include "Core/Memory/Heap.hpp"
#include "Core/Memory/HeapLive.hpp"

extern "C" {
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/Seq/Channel.h"
}

namespace {
namespace Seq = VS::Audio::Seq;
using Seq::ChannelState;
using Seq::CommandResult;
using Seq::CommandStatus;
using Seq::Operands;
using Seq::PatchEntry;
using Seq::PatchLookup;
using VS::Audio::Driver::ChannelKind;
using VS::Audio::Driver::VoiceMode;
using VS::Audio::Samples::InstrumentRecord;

constexpr std::uint32_t kChannelBit = 0x4;
constexpr std::uint16_t kSoundEffectType = 1;

template <typename... Values>
std::array<std::uint8_t, sizeof...(Values)> Bytes(Values... values) {
  return {static_cast<std::uint8_t>(values)...};
}

// A channel with every field the commands touch set away from its default.
ChannelState SampleChannel(ChannelKind kind = ChannelKind::kMusic) {
  ChannelState channel;
  channel.kind = kind;
  channel.pitch_base = 0x1000;
  channel.effect_voice_mask = 6;
  channel.update_flags = Seq::kInstrumentFlags | Seq::kVibratoActive |
                         Seq::kTremoloActive | Seq::kAutoPanActive;
  channel.volume = 0x12345678;
  channel.volume_balance = 0x1234;
  channel.pan = 0x12AB;
  channel.effect_volume = 0x12AB;
  channel.octave = 4;
  channel.transpose = -2;
  channel.fine_tune = 100;
  channel.stored_note_length = 5;
  channel.fixed_note_length = 7;
  channel.instrument_index = 2;
  channel.play_modes = Seq::kLegatoPlay | Seq::kFullLengthPlay;
  channel.vibrato_depth = 0x4000;
  channel.tremolo_depth = 0x2000;
  channel.noise_timer = 5;
  channel.fm_timer = 6;
  channel.random_pitch_depth = 9;
  channel.voice.dirty_flags = 0x80000000U;
  channel.voice.adsr_lower = 0x80FF;
  channel.voice.adsr_upper = 0xFFE0;
  channel.voice.volume_scale = 0x7F;
  return channel;
}

InstrumentRecord TestInstrument(std::int32_t index) {
  const auto step = static_cast<std::uint32_t>(index) * 0x100U;
  InstrumentRecord record;
  record.sample_start = 0x2000U + step;
  record.loop_start = 0x3000U + step;
  record.fine_tune = static_cast<std::int16_t>(index);
  record.root_note = 60;
  record.adsr_lower = static_cast<std::uint16_t>(0x8000 + index);
  record.adsr_upper = static_cast<std::uint16_t>(0x4000 + index);
  return record;
}

struct ModeChange {
  ChannelKind kind = ChannelKind::kMusic;
  VoiceMode mode = VoiceMode::kReverb;
  std::uint32_t channel_bits = 0;
  bool enabled = false;

  friend bool operator==(const ModeChange&, const ModeChange&) = default;
};

// Answers from its fields and records every request.
class FakeVoiceHost final : public Seq::VoiceCommandHost {
 public:
  static constexpr std::int32_t kInstrumentCount = 8;

  std::int32_t resolve_offset = 0;
  std::vector<std::uint8_t> resolved;
  ChannelKind resolved_kind = ChannelKind::kMusic;
  std::int32_t resolved_mask = 0;
  PatchEntry patch;
  std::vector<std::uint8_t> patches;
  bool patch_keymap = true;
  std::vector<std::uint32_t> keymaps;
  bool drum_keymap = true;
  std::optional<std::uint8_t> missing_waveform;
  std::vector<ModeChange> modes;
  std::uint16_t noise_clock = 0x3E;
  ChannelKind clock_kind = ChannelKind::kMusic;
  std::vector<std::uint16_t> clocks;
  std::vector<std::uint32_t> updates;

  std::int32_t ResolveInstrument(ChannelKind kind,
                                 std::int32_t effect_voice_mask,
                                 std::uint8_t instrument) override {
    resolved.push_back(instrument);
    resolved_kind = kind;
    resolved_mask = effect_voice_mask;
    return instrument + resolve_offset;
  }

  std::optional<InstrumentRecord> Instrument(std::int32_t index) override {
    if (index < 0 || index >= kInstrumentCount) return std::nullopt;
    return TestInstrument(index);
  }

  PatchEntry FindPatch(std::uint8_t patch_index) override {
    patches.push_back(patch_index);
    return patch;
  }

  bool HasPatchKeymap(std::uint32_t offset) override {
    keymaps.push_back(offset);
    return patch_keymap;
  }

  bool HasDrumKeymap() override { return drum_keymap; }

  bool HasLfoWaveform(std::uint8_t shape) override {
    return shape != missing_waveform;
  }

  void SetVoiceMode(ChannelKind kind, VoiceMode mode,
                    std::uint32_t channel_bits, bool enabled) override {
    modes.push_back({kind, mode, channel_bits, enabled});
  }

  std::uint16_t NoiseClock(ChannelKind kind) override {
    clock_kind = kind;
    return noise_clock;
  }

  void SetNoiseClock(ChannelKind kind, std::uint16_t clock) override {
    clock_kind = kind;
    noise_clock = clock;
    clocks.push_back(clock);
  }

  void RequestVoiceUpdate(std::uint32_t flags) override {
    updates.push_back(flags);
  }

  [[nodiscard]] bool Untouched() const {
    return resolved.empty() && patches.empty() && keymaps.empty() &&
           modes.empty() && clocks.empty() && updates.empty();
  }
};

void Expect(const CommandResult& result, const CommandResult& expected_result,
            const ChannelState& channel, const ChannelState& expected) {
  CHECK(result == expected_result);
  CHECK(channel == expected);
}

void ExpectRefused(const CommandResult& result, CommandStatus status,
                   std::size_t consumed, const ChannelState& channel,
                   const ChannelState& before) {
  CHECK(result.status == status);
  CHECK(result.consumed == consumed);
  CHECK(channel == before);
}

using PlainCommand = CommandResult (*)(ChannelState&, Operands);
using HostCommand = CommandResult (*)(ChannelState&, Operands,
                                      Seq::VoiceCommandHost&);
using MaskCommand = CommandResult (*)(ChannelState&, Operands,
                                      Seq::VoiceCommandHost&, std::uint32_t);

CommandResult Call(PlainCommand command, ChannelState& channel,
                   Operands operands, Seq::VoiceCommandHost& /*host*/) {
  return command(channel, operands);
}

CommandResult Call(HostCommand command, ChannelState& channel,
                   Operands operands, Seq::VoiceCommandHost& host) {
  return command(channel, operands, host);
}

CommandResult Call(MaskCommand command, ChannelState& channel,
                   Operands operands, Seq::VoiceCommandHost& host) {
  return command(channel, operands, host, kChannelBit);
}

template <typename Command>
struct OperandCase {
  const char* name;
  std::size_t operands;
  Command command;
};

// Each command refuses one byte short of its operands without touching the
// channel or the host, and consumes exactly its operands when more follow.
template <typename Command>
void CheckOperandBounds(std::span<const OperandCase<Command>> cases) {
  constexpr std::array<std::uint8_t, 4> kBytes{1, 2, 3, 4};
  const Operands bytes(kBytes);
  for (const OperandCase<Command>& entry : cases) {
    CAPTURE(entry.name);
    const ChannelState before = SampleChannel();
    ChannelState channel = before;
    FakeVoiceHost host;
    CHECK(Call(entry.command, channel, bytes.first(entry.operands - 1), host) ==
          Seq::Truncated());
    CHECK(channel == before);
    CHECK(host.Untouched());
    CHECK(Call(entry.command, channel, bytes, host) ==
          Seq::Applied(entry.operands));
  }
}

std::span<const OperandCase<PlainCommand>> ChannelOperandCases() {
  static constexpr std::array<OperandCase<PlainCommand>, 18> kCases{{
      {"A3 SetVolumeBalance", 1, Seq::SetVolumeBalance},
      {"FE12 SlideVolumeBalance", 2, Seq::SlideVolumeBalance},
      {"A8 SetVolume", 1, Seq::SetVolume},
      {"A9 SlideVolume", 2, Seq::SlideVolume},
      {"FE19 SetKeyOnVolumeRamp", 3, Seq::SetKeyOnVolumeRamp},
      {"SetEffectVolumeScale", 1, Seq::SetEffectVolumeScale},
      {"SlideEffectVolumeScale", 2, Seq::SlideEffectVolumeScale},
      {"AA SetPan", 1, Seq::SetPan},
      {"AB SlidePan", 2, Seq::SlidePan},
      {"A5 SetOctave", 1, Seq::SetOctave},
      {"C0 SetTranspose", 1, Seq::SetTranspose},
      {"C1 AddTranspose", 1, Seq::AddTranspose},
      {"A4 SetPitchBendSlide", 2, Seq::SetPitchBendSlide},
      {"DA EnablePortamento", 1, Seq::EnablePortamento},
      {"D8 SetFineTune", 1, Seq::SetFineTune},
      {"D9 AddFineTune", 1, Seq::AddFineTune},
      {"A2 SetNextNoteLength", 1, Seq::SetNextNoteLength},
      {"DC SetFixedNoteLength", 1, Seq::SetFixedNoteLength},
  }};
  return kCases;
}

std::span<const OperandCase<PlainCommand>> EnvelopeAndDepthOperandCases() {
  static constexpr std::array<OperandCase<PlainCommand>, 18> kCases{{
      {"AD SetAttackRate", 1, Seq::SetAttackRate},
      {"AE SetDecayRate", 1, Seq::SetDecayRate},
      {"AF SetSustainLevel", 1, Seq::SetSustainLevel},
      {"B1 SetSustainRate", 1, Seq::SetSustainRate},
      {"B2 SetReleaseRate", 1, Seq::SetReleaseRate},
      {"B7 SetAttackMode", 1, Seq::SetAttackMode},
      {"BB SetSustainMode", 1, Seq::SetSustainMode},
      {"BF SetReleaseMode", 1, Seq::SetReleaseMode},
      {"B0 SetDecayAndSustain", 2, Seq::SetDecayAndSustain},
      {"B5 SetVibratoDepth", 1, Seq::SetVibratoDepth},
      {"DD SlideVibratoDepth", 2, Seq::SlideVibratoDepth},
      {"B9 SetTremoloDepth", 1, Seq::SetTremoloDepth},
      {"DE SlideTremoloDepth", 2, Seq::SlideTremoloDepth},
      {"BD SetAutoPanDepth", 1, Seq::SetAutoPanDepth},
      {"DF SlideAutoPanDepth", 2, Seq::SlideAutoPanDepth},
      {"CF SetNoiseToggleDelay", 1, Seq::SetNoiseToggleDelay},
      {"D3 SetFrequencyModulationToggleDelay", 1,
       Seq::SetFrequencyModulationToggleDelay},
      {"E1 SetRandomPitchDepth", 1, Seq::SetRandomPitchDepth},
  }};
  return kCases;
}

std::span<const OperandCase<HostCommand>> HostOperandCases() {
  static constexpr std::array<OperandCase<HostCommand>, 7> kCases{{
      {"A1 LoadInstrument", 1, Seq::LoadInstrument},
      {"FE0A LoadSilentInstrument", 1, Seq::LoadSilentInstrument},
      {"FE14 SelectPatch", 1, Seq::SelectPatch},
      {"B4 EnableVibrato", 3, Seq::EnableVibrato},
      {"B8 EnableTremolo", 3, Seq::EnableTremolo},
      {"BC EnableAutoPan", 2, Seq::EnableAutoPan},
      {"AC SetNoiseClock", 1, Seq::SetNoiseClock},
  }};
  return kCases;
}

std::span<const OperandCase<MaskCommand>> MaskOperandCases() {
  static constexpr std::array<OperandCase<MaskCommand>, 2> kCases{{
      {"CE EnableTimedNoise", 1, Seq::EnableTimedNoise},
      {"D2 EnableTimedFrequencyModulation", 1,
       Seq::EnableTimedFrequencyModulation},
  }};
  return kCases;
}
}  // namespace

TEST_CASE("Voice commands with truncated operands consume and change nothing",
          "[audio-seq-voices][audio]") {
  CheckOperandBounds(ChannelOperandCases());
  CheckOperandBounds(EnvelopeAndDepthOperandCases());
  CheckOperandBounds(HostOperandCases());
  CheckOperandBounds(MaskOperandCases());
}

TEST_CASE("Volume commands store signed levels and wrapping slide steps",
          "[audio-seq-voices][audio]") {
  ChannelState channel = SampleChannel();
  channel.volume_slide_length = 9;
  channel.key_on_volume_slide_length = 9;
  ChannelState expected = channel;
  expected.volume = static_cast<std::int32_t>(0xC0000000U);
  expected.volume_slide_length = 0;
  expected.key_on_volume_slide_length = 0;
  expected.voice.dirty_flags |= Seq::kVolumeDirty;
  Expect(Seq::SetVolume(channel, Bytes(0x80)), Seq::Applied(1), channel,
         expected);

  channel.volume = 0x12345678;
  expected = channel;
  expected.volume = 0x12340000;
  expected.volume_slide_length = 256;
  expected.volume_slide_step = -5387264;
  Expect(Seq::SlideVolume(channel, Bytes(0, 0x80)), Seq::Applied(2), channel,
         expected);

  // 0xC0000000 - 0x7FFF0000 wraps to a positive difference.
  channel.volume = 0x7FFF1234;
  expected = channel;
  expected.volume = 0x7FFF0000;
  expected.volume_slide_length = 7;
  expected.volume_slide_step = 153401051;
  Expect(Seq::SlideVolume(channel, Bytes(7, 0x80)), Seq::Applied(2), channel,
         expected);

  expected = channel;
  expected.key_on_volume = 0x20000000;
  expected.key_on_volume_slide_length = 256;
  expected.key_on_volume_slide_step = -4194304;
  Expect(Seq::SetKeyOnVolumeRamp(channel, Bytes(0x40, 0, 0xC0)),
         Seq::Applied(3), channel, expected);
}

TEST_CASE("Balance and pan slides start from the high byte and truncate",
          "[audio-seq-voices][audio]") {
  ChannelState channel = SampleChannel();
  ChannelState expected = channel;
  expected.volume_balance = 0x8100;
  expected.voice.dirty_flags |= Seq::kVolumeDirty;
  Expect(Seq::SetVolumeBalance(channel, Bytes(0x81)), Seq::Applied(1), channel,
         expected);

  channel.volume_balance = 0x1234;
  expected = channel;
  expected.volume_balance = 0x1200;
  expected.volume_balance_slide_length = 7;
  expected.volume_balance_slide_step = -658;
  Expect(Seq::SlideVolumeBalance(channel, Bytes(7, 0)), Seq::Applied(2),
         channel, expected);

  channel.pan_slide_length = 3;
  expected = channel;
  expected.pan = 0x8000;
  expected.pan_slide_length = 0;
  Expect(Seq::SetPan(channel, Bytes(0x40)), Seq::Applied(1), channel, expected);
  CHECK(Seq::SetPan(channel, Bytes(0xC0)) == Seq::Applied(1));
  CHECK(channel.pan == 0);

  channel.pan = 0x12AB;
  expected = channel;
  expected.pan = 0x1200;
  expected.pan_slide_length = 256;
  expected.pan_slide_step = 109;
  Expect(Seq::SlidePan(channel, Bytes(0, 0x3F)), Seq::Applied(2), channel,
         expected);
}

TEST_CASE("Effect volume scale commands mark only overlay channels dirty",
          "[audio-seq-voices][audio]") {
  ChannelState channel = SampleChannel();
  channel.update_flags = 0;
  channel.effect_volume_slide_length = 4;
  ChannelState expected = channel;
  expected.effect_volume = -32768;
  expected.effect_volume_slide_length = 0;
  Expect(Seq::SetEffectVolumeScale(channel, Bytes(0x80)), Seq::Applied(1),
         channel, expected);

  channel.update_flags = Seq::kOverlayChannel;
  expected = channel;
  expected.effect_volume = 0x1000;
  expected.voice.dirty_flags |= Seq::kVolumeDirty;
  Expect(Seq::SetEffectVolumeScale(channel, Bytes(0x10)), Seq::Applied(1),
         channel, expected);

  channel.effect_volume = 0x12AB;
  expected = channel;
  expected.effect_volume = 0x1200;
  expected.effect_volume_slide_length = 2;
  expected.effect_volume_slide_step = -256;
  Expect(Seq::SlideEffectVolumeScale(channel, Bytes(2, 0x10)), Seq::Applied(2),
         channel, expected);
}

TEST_CASE("Octave commands store the operand and step within four bits",
          "[audio-seq-voices][audio]") {
  ChannelState channel = SampleChannel();
  CHECK(Seq::SetOctave(channel, Bytes(0x15)) == Seq::Applied(1));
  CHECK(channel.octave == 0x15);
  channel.octave = 15;
  Seq::IncreaseOctave(channel);
  CHECK(channel.octave == 0);
  Seq::DecreaseOctave(channel);
  CHECK(channel.octave == 15);
}

TEST_CASE("Transpose and pitch bend commands keep operand signedness",
          "[audio-seq-voices][audio]") {
  ChannelState channel = SampleChannel();
  CHECK(Seq::SetTranspose(channel, Bytes(0xFE)) == Seq::Applied(1));
  CHECK(Seq::AddTranspose(channel, Bytes(0xFF)) == Seq::Applied(1));
  CHECK(channel.transpose == -3);
  channel.transpose = 0x7FFF;
  CHECK(Seq::AddTranspose(channel, Bytes(1)) == Seq::Applied(1));
  CHECK(channel.transpose == -32768);

  ChannelState expected = channel;
  expected.pitch_bend_slide_length = 256;
  expected.pitch_bend_target = -127;
  Expect(Seq::SetPitchBendSlide(channel, Bytes(0, 0x81)), Seq::Applied(2),
         channel, expected);
}

TEST_CASE("Portamento commands replace the play mode and default to 256 steps",
          "[audio-seq-voices][audio]") {
  ChannelState channel = SampleChannel();
  channel.portamento_key = 40;
  channel.portamento_transpose = 3;
  ChannelState expected = channel;
  expected.portamento_steps = 256;
  expected.portamento_key = 0;
  expected.portamento_transpose = 0;
  expected.play_modes = Seq::kLegatoPlay;
  Expect(Seq::EnablePortamento(channel, Bytes(0)), Seq::Applied(1), channel,
         expected);
  CHECK(Seq::EnablePortamento(channel, Bytes(3)) == Seq::Applied(1));
  CHECK(channel.portamento_steps == 3);
  Seq::DisablePortamento(channel);
  CHECK(channel.portamento_steps == 0);
}

TEST_CASE("Fine tune scales the base pitch with unsigned wrapping arithmetic",
          "[audio-seq-voices][audio]") {
  ChannelState channel = SampleChannel();
  ChannelState expected = channel;
  expected.fine_tune = 64;
  expected.fine_pitch_delta = 0x800;
  expected.voice.dirty_flags |= Seq::kSampleRateDirty;
  Expect(Seq::SetFineTune(channel, Bytes(0x40)), Seq::Applied(1), channel,
         expected);

  CHECK(Seq::SetFineTune(channel, Bytes(0xC0)) == Seq::Applied(1));
  CHECK(std::pair(channel.fine_tune, channel.fine_pitch_delta) ==
        std::pair<std::int16_t, std::int32_t>(-64, -1024));

  // The sharp product shifts logically: 0xFE00007F >> 7.
  channel.pitch_base = 0x02000001;
  CHECK(Seq::SetFineTune(channel, Bytes(0x7F)) == Seq::Applied(1));
  CHECK(channel.fine_pitch_delta == 0x01FC0000);
  // 0x04000000 * 0x40 wraps to zero.
  channel.pitch_base = 0x04000000;
  CHECK(Seq::SetFineTune(channel, Bytes(0x40)) == Seq::Applied(1));
  CHECK(channel.fine_pitch_delta == 0);
}

TEST_CASE("Relative fine tune adds the signed operand to the stored tune",
          "[audio-seq-voices][audio]") {
  ChannelState channel = SampleChannel();
  CHECK(Seq::AddFineTune(channel, Bytes(0x50)) == Seq::Applied(1));
  CHECK(std::pair(channel.fine_tune, channel.fine_pitch_delta) ==
        std::pair<std::int16_t, std::int32_t>(180, 5760));
  channel.fine_tune = 100;
  CHECK(Seq::AddFineTune(channel, Bytes(0x80)) == Seq::Applied(1));
  CHECK(std::pair(channel.fine_tune, channel.fine_pitch_delta) ==
        std::pair<std::int16_t, std::int32_t>(-28, -448));
  channel.fine_tune = 0x7FFF;
  CHECK(Seq::AddFineTune(channel, Bytes(1)) == Seq::Applied(1));
  CHECK(channel.fine_tune == -32768);
}

TEST_CASE("Note length commands clamp fixed lengths to the original range",
          "[audio-seq-voices][audio]") {
  ChannelState channel = SampleChannel();
  ChannelState expected = channel;
  expected.fixed_note_length = 0;
  expected.gate_length = 200;
  expected.note_length = 200;
  expected.stored_note_length = 200;
  Expect(Seq::SetNextNoteLength(channel, Bytes(0xC8)), Seq::Applied(1), channel,
         expected);

  struct FixedCase {
    std::int16_t stored;
    std::uint8_t operand;
    std::int16_t fixed;
  };
  constexpr std::array<FixedCase, 5> kFixedCases{{
      {5, 0x00, 0},
      {5, 0xF6, 1},
      {5, 0xFB, 1},
      {200, 0x7F, 255},
      {5, 0x05, 10},
  }};
  for (const FixedCase& entry : kFixedCases) {
    CAPTURE(entry.stored, entry.operand);
    channel.stored_note_length = entry.stored;
    channel.fixed_note_length = 99;
    CHECK(Seq::SetFixedNoteLength(channel, Bytes(entry.operand)) ==
          Seq::Applied(1));
    CHECK(channel.fixed_note_length == entry.fixed);
  }
}

TEST_CASE("Play mode and flag commands change only their own state",
          "[audio-seq-voices][audio]") {
  ChannelState channel = SampleChannel();
  const ChannelState before = channel;
  Seq::EnableFullNoteLength(channel);
  CHECK(channel.update_flags ==
        (before.update_flags | Seq::kFullNoteLengthActive));
  Seq::DisableFullNoteLength(channel);
  CHECK(channel == before);
  Seq::EnableLoopBoundaryStop(channel);
  CHECK(channel.update_flags == (before.update_flags | Seq::kLoopBoundaryStop));

  Seq::EnableLegato(channel);
  CHECK(channel.play_modes == Seq::kLegatoPlay);
  Seq::EnableSustainedSoundEffect(channel);
  CHECK(channel.play_modes == Seq::kLegatoPlay);
  channel.kind = ChannelKind::kSoundEffect;
  Seq::EnableSustainedSoundEffect(channel);
  CHECK(channel.play_modes == Seq::kFullLengthPlay);

  ChannelState expected = channel;
  expected.voice.volume_scale = 0;
  expected.update_flags &= ~Seq::kDrumKeymapActive;
  Seq::MuteInstrument(channel);
  CHECK(channel == expected);
}

TEST_CASE(
    "Instrument commands copy the selected record or refuse a missing one",
    "[audio-seq-voices][audio]") {
  FakeVoiceHost host;
  host.resolve_offset = 2;
  ChannelState channel = SampleChannel(ChannelKind::kSoundEffect);
  const InstrumentRecord record = TestInstrument(5);
  ChannelState expected = channel;
  expected.voice.sample_start = record.sample_start;
  expected.voice.loop_start = record.loop_start;
  expected.voice.adsr_lower = record.adsr_lower;
  expected.voice.adsr_upper = record.adsr_upper;
  expected.voice.dirty_flags |= Seq::kInstrumentDirty;
  expected.voice.volume_scale = 0;
  expected.instrument_index = 5;
  expected.update_flags &= ~Seq::kInstrumentFlags;
  Expect(Seq::LoadInstrument(channel, Bytes(3), host), Seq::Applied(1), channel,
         expected);
  CHECK(host.resolved == std::vector<std::uint8_t>{3});
  CHECK(host.resolved_kind == ChannelKind::kSoundEffect);
  CHECK(host.resolved_mask == 6);

  const ChannelState before = channel;
  ExpectRefused(Seq::LoadInstrument(channel, Bytes(6), host),
                CommandStatus::kInstrumentMissing, 1, channel, before);

  expected = channel;
  expected.voice.sample_start = Seq::kSilentSampleStart;
  expected.voice.loop_start = TestInstrument(4).loop_start;
  expected.voice.adsr_lower = TestInstrument(4).adsr_lower;
  expected.voice.adsr_upper = TestInstrument(4).adsr_upper;
  expected.instrument_index = 4;
  Expect(Seq::LoadSilentInstrument(channel, Bytes(4), host), Seq::Applied(1),
         channel, expected);
  // The silent load uses its operand directly, without the resolver.
  CHECK(host.resolved == std::vector<std::uint8_t>{3, 6});
  ExpectRefused(Seq::LoadSilentInstrument(channel, Bytes(8), host),
                CommandStatus::kInstrumentMissing, 1, channel, expected);
}

TEST_CASE(
    "Patch selection mutes, refuses or selects the keymap after its header",
    "[audio-seq-voices][audio]") {
  FakeVoiceHost host;
  ChannelState channel = SampleChannel();
  const ChannelState before = channel;
  Expect(Seq::SelectPatch(channel, Bytes(9), host), Seq::Applied(1), channel,
         before);
  host.patch = {PatchLookup::kOutOfRange, 0};
  ExpectRefused(Seq::SelectPatch(channel, Bytes(9), host),
                CommandStatus::kPatchOutOfRange, 1, channel, before);

  // Entries above 0x8000 mute; 0x8000 itself names a keymap.
  host.patch = {PatchLookup::kFound, 0x8000};
  host.patch_keymap = false;
  ExpectRefused(Seq::SelectPatch(channel, Bytes(9), host),
                CommandStatus::kPatchOutOfRange, 1, channel, before);
  CHECK(host.keymaps == std::vector<std::uint32_t>{0x8020});

  host.patch = {PatchLookup::kFound, 0x8001};
  ChannelState expected = channel;
  expected.voice.volume_scale = 0;
  expected.update_flags &= ~Seq::kPatchKeymapActive;
  Expect(Seq::SelectPatch(channel, Bytes(9), host), Seq::Applied(1), channel,
         expected);

  host.patch = {PatchLookup::kFound, 0x10};
  host.patch_keymap = true;
  channel = before;
  expected = channel;
  expected.keymap_offset = 0x30;
  expected.update_flags =
      (before.update_flags & ~Seq::kInstrumentFlags) | Seq::kPatchKeymapActive;
  expected.key = 0xFF;
  Expect(Seq::SelectPatch(channel, Bytes(9), host), Seq::Applied(1), channel,
         expected);
  CHECK(host.patches == std::vector<std::uint8_t>(5, 9));
}

TEST_CASE("Drum keymaps and envelope resets need their tables",
          "[audio-seq-voices][audio]") {
  FakeVoiceHost host;
  host.drum_keymap = false;
  ChannelState channel = SampleChannel();
  const ChannelState before = channel;
  Seq::EnableDrumKeymap(channel, host);
  CHECK(channel == before);
  host.drum_keymap = true;
  Seq::EnableDrumKeymap(channel, host);
  CHECK(channel.update_flags ==
        ((before.update_flags & ~Seq::kInstrumentFlags) |
         Seq::kDrumKeymapActive));

  channel = before;
  ChannelState expected = channel;
  expected.voice.adsr_lower = TestInstrument(2).adsr_lower;
  expected.voice.adsr_upper = TestInstrument(2).adsr_upper;
  expected.voice.dirty_flags |= Seq::kEnvelopeDirty;
  expected.update_flags &= ~(Seq::kAttackOverridden | Seq::kSustainOverridden |
                             Seq::kReleaseOverridden);
  CHECK(Seq::ResetEnvelope(channel, host) == CommandStatus::kApplied);
  CHECK(channel == expected);

  channel.instrument_index = 20;
  const ChannelState missing = channel;
  CHECK(Seq::ResetEnvelope(channel, host) == CommandStatus::kInstrumentMissing);
  CHECK(channel == missing);
}

namespace {
struct RateCase {
  const char* name;
  PlainCommand command;
  std::uint8_t operand;
  std::uint16_t lower;
  std::uint16_t upper;
  std::uint16_t expected_lower;
  std::uint16_t expected_upper;
  std::uint32_t overridden;
  std::uint32_t dirty;
};

// Operand bits beyond each field are not masked and reach the next field.
std::span<const RateCase> EnvelopeRateCases() {
  static constexpr std::array<RateCase, 6> kCases{{
      {"AD rate", Seq::SetAttackRate, 0x25, 0x80FF, 0, 0xA5FF, 0,
       Seq::kAttackOverridden, Seq::kAttackModeDirty | Seq::kAttackRateDirty},
      {"AD bit 7 sets the attack mode", Seq::SetAttackRate, 0xFF, 0x00FF, 0,
       0xFFFF, 0, Seq::kAttackOverridden,
       Seq::kAttackModeDirty | Seq::kAttackRateDirty},
      {"AE", Seq::SetDecayRate, 0x1F, 0, 0, 0x01F0, 0, 0, Seq::kDecayRateDirty},
      {"AF", Seq::SetSustainLevel, 0x1F, 0, 0, 0x001F, 0, 0,
       Seq::kSustainLevelDirty},
      {"B1", Seq::SetSustainRate, 0xFF, 0, 0, 0, 0x3FC0,
       Seq::kSustainOverridden,
       Seq::kSustainRateDirty | Seq::kSustainModeDirty},
      {"B2", Seq::SetReleaseRate, 0x3F, 0, 0xFFE0, 0, 0xFFFF,
       Seq::kReleaseOverridden,
       Seq::kReleaseRateDirty | Seq::kReleaseModeDirty},
  }};
  return kCases;
}
}  // namespace

TEST_CASE("Envelope rate commands replace unmasked fields and mark overrides",
          "[audio-seq-voices][audio]") {
  for (const RateCase& entry : EnvelopeRateCases()) {
    CAPTURE(entry.name);
    ChannelState channel = SampleChannel();
    channel.update_flags = 0;
    channel.voice.dirty_flags = 0;
    channel.voice.adsr_lower = entry.lower;
    channel.voice.adsr_upper = entry.upper;
    ChannelState expected = channel;
    expected.voice.adsr_lower = entry.expected_lower;
    expected.voice.adsr_upper = entry.expected_upper;
    expected.update_flags = entry.overridden;
    expected.voice.dirty_flags = entry.dirty;
    Expect(entry.command(channel, Bytes(entry.operand)), Seq::Applied(1),
           channel, expected);
  }

  ChannelState channel = SampleChannel();
  channel.voice.adsr_lower = 0xFFFF;
  ChannelState expected = channel;
  expected.voice.adsr_lower = 0xFF35;
  expected.voice.dirty_flags |= Seq::kDecayRateDirty | Seq::kSustainLevelDirty;
  Expect(Seq::SetDecayAndSustain(channel, Bytes(3, 5)), Seq::Applied(2),
         channel, expected);
}

TEST_CASE("Envelope mode commands map sequence modes to SPU mode bits",
          "[audio-seq-voices][audio]") {
  struct ModeCase {
    std::uint8_t mode;
    std::uint16_t attack;
    std::uint16_t sustain;
    std::uint16_t release;
  };
  constexpr std::array<ModeCase, 4> kModeCases{{
      {1, 0x7FFF, 0x3FFF, 0xFFDF},
      {3, 0x7FFF, 0x7FFF, 0xFFDF},
      {5, 0xFFFF, 0xBFFF, 0xFFDF},
      {7, 0x7FFF, 0xFFFF, 0xFFFF},
  }};
  for (const ModeCase& entry : kModeCases) {
    CAPTURE(entry.mode);
    ChannelState channel = SampleChannel();
    channel.voice.adsr_lower = 0xFFFF;
    channel.voice.adsr_upper = 0xFFFF;
    channel.voice.dirty_flags = 0;
    CHECK(Seq::SetAttackMode(channel, Bytes(entry.mode)) == Seq::Applied(1));
    CHECK(Seq::SetSustainMode(channel, Bytes(entry.mode)) == Seq::Applied(1));
    CHECK(channel.voice.adsr_upper == entry.sustain);
    channel.voice.adsr_upper = 0xFFFF;
    CHECK(Seq::SetReleaseMode(channel, Bytes(entry.mode)) == Seq::Applied(1));
    CHECK(std::pair(channel.voice.adsr_lower, channel.voice.adsr_upper) ==
          std::pair(entry.attack, entry.release));
    CHECK(channel.voice.dirty_flags == 0x700);
  }
}

TEST_CASE("Vibrato uses the base pitch's low half for B4 and all of it for B5",
          "[audio-seq-voices][audio]") {
  FakeVoiceHost host;
  ChannelState channel = SampleChannel();
  channel.pitch_base = 0x00011000;
  ChannelState expected = channel;
  expected.vibrato_delay = 0x10;
  expected.vibrato_delay_left = 0x10;
  expected.vibrato_rate = 256;
  expected.vibrato_rate_counter = 1;
  expected.vibrato_shape = 0x80;
  expected.vibrato_wave = 0x80;
  expected.vibrato_base = 120;
  Expect(Seq::EnableVibrato(channel, Bytes(0x10, 0, 0x80), host),
         Seq::Applied(3), channel, expected);

  CHECK(Seq::SetVibratoDepth(channel, Bytes(0x40)) == Seq::Applied(1));
  CHECK(channel.vibrato_base == 2040);
  CHECK(Seq::SetVibratoDepth(channel, Bytes(0xC0)) == Seq::Applied(1));
  CHECK(std::pair(channel.vibrato_depth, channel.vibrato_base) ==
        std::pair<std::uint16_t, std::uint16_t>(0xC000, 0x8800));
  CHECK(Seq::EnableVibrato(channel, Bytes(0, 1, 0x80), host) ==
        Seq::Applied(3));
  CHECK(channel.vibrato_base == 2048);
}

TEST_CASE("Sound effect LFOs take depth from the delay operand",
          "[audio-seq-voices][audio]") {
  FakeVoiceHost host;
  ChannelState channel = SampleChannel(ChannelKind::kSoundEffect);
  channel.vibrato_delay = 3;
  ChannelState expected = channel;
  expected.vibrato_delay = 0;
  expected.vibrato_delay_left = 0;
  expected.vibrato_depth = 0x2000;
  expected.vibrato_base = 60;
  expected.vibrato_rate = 5;
  expected.vibrato_rate_counter = 1;
  expected.vibrato_shape = 9;
  expected.vibrato_wave = 9;
  Expect(Seq::EnableVibrato(channel, Bytes(0x20, 5, 9), host), Seq::Applied(3),
         channel, expected);
  CHECK(Seq::EnableVibrato(channel, Bytes(0, 5, 9), host) == Seq::Applied(3));
  CHECK(channel.vibrato_depth == 0x2000);

  CHECK(Seq::EnableTremolo(channel, Bytes(0x85, 0, 9), host) ==
        Seq::Applied(3));
  CHECK(std::array<std::uint16_t, 3>{channel.tremolo_depth,
                                     channel.tremolo_rate,
                                     channel.tremolo_delay_left} ==
        std::array<std::uint16_t, 3>{0x0500, 256, 0});
  CHECK(channel.tremolo_delay == 0);

  channel.kind = ChannelKind::kMusic;
  CHECK(Seq::EnableTremolo(channel, Bytes(0x85, 1, 9), host) ==
        Seq::Applied(3));
  CHECK(std::array<std::uint16_t, 3>{channel.tremolo_depth,
                                     channel.tremolo_rate,
                                     channel.tremolo_delay_left} ==
        std::array<std::uint16_t, 3>{0x0500, 1, 0x85});
}

TEST_CASE("LFO enables refuse a missing waveform and keep the channel",
          "[audio-seq-voices][audio]") {
  FakeVoiceHost host;
  host.missing_waveform = 9;
  ChannelState channel = SampleChannel();
  channel.update_flags = 0;
  const ChannelState before = channel;
  ExpectRefused(Seq::EnableVibrato(channel, Bytes(1, 2, 9), host),
                CommandStatus::kWaveformMissing, 3, channel, before);
  ExpectRefused(Seq::EnableTremolo(channel, Bytes(1, 2, 9), host),
                CommandStatus::kWaveformMissing, 3, channel, before);
  ExpectRefused(Seq::EnableAutoPan(channel, Bytes(2, 9), host),
                CommandStatus::kWaveformMissing, 2, channel, before);

  ChannelState expected = channel;
  expected.update_flags = Seq::kAutoPanActive;
  expected.auto_pan_rate = 256;
  expected.auto_pan_rate_counter = 1;
  expected.auto_pan_shape = 0xFF;
  expected.auto_pan_wave = 0xFF;
  Expect(Seq::EnableAutoPan(channel, Bytes(0, 0xFF), host), Seq::Applied(2),
         channel, expected);
}

TEST_CASE("Modulation depth commands scale and slides truncate their steps",
          "[audio-seq-voices][audio]") {
  ChannelState channel = SampleChannel();
  CHECK(Seq::SlideVibratoDepth(channel, Bytes(4, 0x20)) == Seq::Applied(2));
  CHECK(std::pair(channel.vibrato_depth_slide_step,
                  channel.vibrato_depth_slide_length) ==
        std::pair<std::uint16_t, std::uint16_t>(0xF800, 4));
  CHECK(Seq::SetTremoloDepth(channel, Bytes(0xFF)) == Seq::Applied(1));
  CHECK(channel.tremolo_depth == 0x7F00);
  channel.tremolo_depth = 0x2000;
  CHECK(Seq::SlideTremoloDepth(channel, Bytes(3, 0x10)) == Seq::Applied(2));
  CHECK(channel.tremolo_depth_slide_step == -1365);
}

TEST_CASE("Auto pan depth commands scale and slide from the stored depth",
          "[audio-seq-voices][audio]") {
  ChannelState channel = SampleChannel();
  CHECK(Seq::SetAutoPanDepth(channel, Bytes(0xFF)) == Seq::Applied(1));
  CHECK(channel.auto_pan_depth == 0x7F80);
  channel.auto_pan_depth = 0;
  CHECK(Seq::SlideAutoPanDepth(channel, Bytes(0, 0x80)) == Seq::Applied(2));
  CHECK(channel.auto_pan_depth_slide_step == 64);
}

TEST_CASE("Modulation disables clear their output and mark the voice dirty",
          "[audio-seq-voices][audio]") {
  ChannelState channel = SampleChannel();
  channel.vibrato_pitch = 5;
  channel.tremolo_volume = 6;
  channel.auto_pan_offset = 7;
  ChannelState expected = channel;
  expected.vibrato_pitch = 0;
  expected.tremolo_volume = 0;
  expected.auto_pan_offset = 0;
  expected.update_flags &=
      ~(Seq::kVibratoActive | Seq::kTremoloActive | Seq::kAutoPanActive);
  expected.voice.dirty_flags |= Seq::kSampleRateDirty | Seq::kVolumeDirty;
  Seq::DisableVibrato(channel);
  Seq::DisableTremolo(channel);
  Seq::DisableAutoPan(channel);
  CHECK(channel == expected);
}

TEST_CASE("Voice mode commands reach the host for the channel kind",
          "[audio-seq-voices][audio]") {
  FakeVoiceHost host;
  ChannelState music = SampleChannel();
  ChannelState effect = SampleChannel(ChannelKind::kSoundEffect);
  Seq::EnableNoise(music, host, kChannelBit);
  Seq::DisableNoise(music, host, kChannelBit);
  Seq::EnableReverb(effect, host, kChannelBit);
  Seq::DisableReverb(effect, host, kChannelBit);
  Seq::EnableFrequencyModulation(music, host, kChannelBit);
  Seq::EnableFrequencyModulation(effect, host, kChannelBit);
  effect.update_flags |= Seq::kFrequencyModulationAllowed;
  Seq::EnableFrequencyModulation(effect, host, kChannelBit);
  Seq::DisableFrequencyModulation(effect, host, kChannelBit);
  CHECK(host.modes ==
        std::vector<ModeChange>{
            {ChannelKind::kMusic, VoiceMode::kNoise, kChannelBit, true},
            {ChannelKind::kMusic, VoiceMode::kNoise, kChannelBit, false},
            {ChannelKind::kSoundEffect, VoiceMode::kReverb, kChannelBit, true},
            {ChannelKind::kSoundEffect, VoiceMode::kReverb, kChannelBit, false},
            {ChannelKind::kMusic, VoiceMode::kFrequencyModulation, kChannelBit,
             true},
            {ChannelKind::kSoundEffect, VoiceMode::kFrequencyModulation,
             kChannelBit, true},
            {ChannelKind::kSoundEffect, VoiceMode::kFrequencyModulation,
             kChannelBit, false},
        });
  CHECK(host.updates == std::vector<std::uint32_t>{0x110, 0x110, 0x100, 0x100,
                                                   0x100, 0x100, 0x100, 0x100});
  CHECK(std::pair(music.noise_timer, effect.fm_timer) ==
        std::pair<std::uint16_t, std::uint16_t>(0, 0));
}

TEST_CASE("Noise clock steps add within six bits and absolute values replace",
          "[audio-seq-voices][audio]") {
  FakeVoiceHost host;
  ChannelState channel = SampleChannel(ChannelKind::kSoundEffect);
  const ChannelState before = channel;
  for (const std::uint8_t frequency : {0x20, 0xC5, 0x45, 0x9F}) {
    CHECK(Seq::SetNoiseClock(channel, Bytes(frequency), host) ==
          Seq::Applied(1));
  }
  CHECK(host.clocks == std::vector<std::uint16_t>{0x20, 0x25, 0x2A, 0x09});
  CHECK(host.updates == std::vector<std::uint32_t>(4, 0x10));
  CHECK(host.clock_kind == ChannelKind::kSoundEffect);
  CHECK(channel == before);
}

TEST_CASE("Timed noise counts one tick beyond its delay",
          "[audio-seq-voices][audio]") {
  FakeVoiceHost host;
  ChannelState channel = SampleChannel(ChannelKind::kSoundEffect);
  CHECK(Seq::EnableTimedNoise(channel, Bytes(0), host, kChannelBit) ==
        Seq::Applied(1));
  CHECK(channel.noise_timer == 0x101);
  CHECK(Seq::SetNoiseToggleDelay(channel, Bytes(0xFF)) == Seq::Applied(1));
  CHECK(channel.noise_timer == 0x100);
  CHECK(host.modes ==
        std::vector<ModeChange>{
            {ChannelKind::kSoundEffect, VoiceMode::kNoise, kChannelBit, true}});
  CHECK(host.updates == std::vector<std::uint32_t>{0x110});
}

TEST_CASE("Timed frequency modulation counts one tick beyond its delay",
          "[audio-seq-voices][audio]") {
  FakeVoiceHost host;
  ChannelState channel = SampleChannel(ChannelKind::kSoundEffect);
  CHECK(Seq::EnableTimedFrequencyModulation(channel, Bytes(5), host,
                                            kChannelBit) == Seq::Applied(1));
  CHECK(channel.fm_timer == 6);
  CHECK(Seq::SetFrequencyModulationToggleDelay(channel, Bytes(0)) ==
        Seq::Applied(1));
  CHECK(channel.fm_timer == 0x101);
  // Sound effects without bit 16 request the update but keep their FM mask.
  CHECK(host.modes.empty());
  CHECK(host.updates == std::vector<std::uint32_t>{0x100});
}

TEST_CASE("Resetting voice effects clears modes, flags, timers and play modes",
          "[audio-seq-voices][audio]") {
  FakeVoiceHost host;
  ChannelState channel = SampleChannel();
  channel.update_flags = 0xFFFFFFFFU;
  channel.play_modes = 0xFFFF;
  ChannelState expected = channel;
  expected.update_flags = 0xFFFFFFC8U;
  expected.noise_timer = 0;
  expected.fm_timer = 0;
  expected.play_modes = 0xFFFA;
  Seq::ResetVoiceEffects(channel, host, kChannelBit);
  CHECK(channel == expected);
  CHECK(host.modes ==
        std::vector<ModeChange>{
            {ChannelKind::kMusic, VoiceMode::kNoise, kChannelBit, false},
            {ChannelKind::kMusic, VoiceMode::kFrequencyModulation, kChannelBit,
             false},
            {ChannelKind::kMusic, VoiceMode::kReverb, kChannelBit, false}});
  CHECK(host.updates == std::vector<std::uint32_t>{0x110, 0x100, 0x100});
}

TEST_CASE("Sidechain and random pitch commands toggle only their state",
          "[audio-seq-voices][audio]") {
  ChannelState channel = SampleChannel();
  const ChannelState before = channel;
  Seq::EnablePitchSidechain(channel);
  Seq::EnableVolumeSidechain(channel);
  CHECK(channel.update_flags ==
        (before.update_flags | Seq::kPitchSidechainActive |
         Seq::kVolumeSidechainActive));
  Seq::DisablePitchSidechain(channel);
  CHECK(channel.update_flags ==
        (before.update_flags | Seq::kVolumeSidechainActive));
  Seq::DisableVolumeSidechain(channel);
  CHECK(channel == before);
  CHECK(Seq::SetRandomPitchDepth(channel, Bytes(0xAB)) == Seq::Applied(1));
  CHECK(channel.random_pitch_depth == 0xAB);
  Seq::ResetRandomPitchDepth(channel);
  CHECK(channel.random_pitch_depth == 0);
}

namespace {
// Saves and restores the sequencer, mixer and driver state the Live binding
// reads and writes.
struct LiveVoiceFixture {
  Audio_MusicConfig* saved_config = Audio_Seq_activeMusicConfig;
  Audio_VoiceScheduler saved_scheduler = Audio_Mixer_voiceScheduler;
  Audio_DriverFlags saved_latches = Audio_Driver_controlLatches;
  decltype(std::to_array(Audio_Seq_kLfoWaveforms)) saved_waves =
      std::to_array(Audio_Seq_kLfoWaveforms);
  decltype(std::to_array(Audio_Samples_instruments)) saved_instruments =
      std::to_array(Audio_Samples_instruments);
  Audio_MusicConfig config{};
  Audio_SeqChannel channel{};
  std::array<std::uint8_t, 8> sequence{};

  LiveVoiceFixture() {
    Audio_Seq_activeMusicConfig = &config;
    Audio_Mixer_voiceScheduler = {};
    Audio_Driver_controlLatches = {};
    channel.programCounter = reinterpret_cast<char*>(sequence.data());
  }

  ~LiveVoiceFixture() {
    Audio_Seq_activeMusicConfig = saved_config;
    Audio_Mixer_voiceScheduler = saved_scheduler;
    Audio_Driver_controlLatches = saved_latches;
    std::ranges::copy(saved_waves, Audio_Seq_kLfoWaveforms);
    std::ranges::copy(saved_instruments, Audio_Samples_instruments);
  }

  LiveVoiceFixture(const LiveVoiceFixture&) = delete;
  LiveVoiceFixture& operator=(const LiveVoiceFixture&) = delete;

  [[nodiscard]] std::ptrdiff_t Advance() const {
    return channel.programCounter -
           reinterpret_cast<const char*>(sequence.data());
  }
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

TEST_CASE(
    "Live voice commands advance the program counter by consumed operands",
    "[audio-seq-voices][audio]") {
  LiveVoiceFixture fixture;
  fixture.sequence = {0x80, 0x40, 0x00, 0xC0};
  const std::uint64_t refused = Seq::RefusedLiveVoiceCommandCount();
  Seq::LiveSetVolume(&fixture.channel, 1);
  CHECK(fixture.Advance() == 1);
  CHECK(fixture.channel.volume == static_cast<std::int32_t>(0xC0000000U));
  Seq::LiveIncreaseOctave(&fixture.channel, 1);
  CHECK(fixture.Advance() == 1);
  CHECK(fixture.channel.octave == 1);
  Seq::LiveSetKeyOnVolumeRamp(&fixture.channel, 1);
  CHECK(fixture.Advance() == 4);
  CHECK(std::pair(fixture.channel.keyOnVolume,
                  fixture.channel.keyOnVolumeSlideStep) ==
        std::pair(0x20000000, -4194304));
  CHECK(fixture.channel.keyOnVolumeSlideLength == 256);
  CHECK(Seq::RefusedLiveVoiceCommandCount() == refused);
}

TEST_CASE("Live music operands stop at the end of the sequence heap block",
          "[audio-seq-voices][audio]") {
  namespace Memory = VS::Core::Memory;
  LiveVoiceFixture fixture;
  alignas(Memory::kHeapUnitBytes) std::array<std::byte, 256> arena{};
  const Memory::ScopedLiveGameHeap heap(arena);
  auto* const data =
      static_cast<std::uint8_t*>(Memory::AllocateLiveFromLowEnd(8));
  REQUIRE(data != nullptr);
  const std::size_t capacity = Memory::LivePayloadCapacity(data).value();
  const std::span<std::uint8_t> block(data, capacity);
  block.back() = 0x40;
  fixture.config.sequenceBase = reinterpret_cast<std::uintptr_t>(data);
  auto* const last = reinterpret_cast<char*>(&block.back());
  fixture.channel.programCounter = last;
  fixture.channel.volumeSlideLength = 3;

  const std::uint64_t refused = Seq::RefusedLiveVoiceCommandCount();
  Seq::LiveSlideVolume(&fixture.channel, 1);
  CHECK(fixture.channel.programCounter == last);
  CHECK(fixture.channel.volumeSlideLength == 3);
  CHECK(Seq::RefusedLiveVoiceCommandCount() == refused + 1);

  Seq::LiveSetVolume(&fixture.channel, 1);
  CHECK(fixture.channel.programCounter == last + 1);
  CHECK(fixture.channel.volume == 0x20000000);
  Seq::LiveSetOctave(&fixture.channel, 1);
  CHECK(fixture.channel.programCounter == last + 1);
  CHECK(Seq::RefusedLiveVoiceCommandCount() == refused + 2);
}

TEST_CASE("Live commands needing a music layer refuse without one and skip",
          "[audio-seq-voices][audio]") {
  LiveVoiceFixture fixture;
  int warnings = 0;
  const VS::Core::LogObserver saved_observer =
      VS::Core::SetLogObserver([&warnings](const VS::Core::LogRecord& record) {
        if (record.channel == "audio" &&
            record.level == VS::Core::LogLevel::Warn) {
          ++warnings;
        }
      });
  Audio_Seq_activeMusicConfig = nullptr;
  fixture.sequence = {5, 6};
  fixture.channel.instrumentIndex = 2;
  const std::uint64_t refused = Seq::RefusedLiveVoiceCommandCount();
  Seq::LiveLoadInstrument(&fixture.channel, 1);
  CHECK(fixture.Advance() == 1);
  CHECK(fixture.channel.instrumentIndex == 2);

  fixture.channel.type = kSoundEffectType;
  Seq::LiveSelectPatch(&fixture.channel, 1);
  Seq::LiveEnableDrumKeymap(&fixture.channel, 1);
  CHECK(fixture.Advance() == 2);
  Seq::LiveEnableNoise(&fixture.channel, 8);
  CHECK(Audio_Mixer_voiceScheduler.noiseVoices == 8);
  CHECK(Audio_Driver_controlLatches.updateFlags == 0x110);
  Seq::LiveSetVolume(nullptr, 1);

  const std::uint64_t last = Seq::RefusedLiveVoiceCommandCount();
  VS::Core::SetLogObserver(saved_observer);
  CHECK(last == refused + 4);
  CHECK(warnings == ReportedRefusals(refused, last));
}

TEST_CASE("Live voice modes update the music layer or the voice scheduler",
          "[audio-seq-voices][audio]") {
  LiveVoiceFixture fixture;
  fixture.config.reverbChannels = 0xC;
  fixture.config.noiseClock = 0x3F;
  fixture.sequence = {0xC1, 0x12};
  Seq::LiveEnableNoise(&fixture.channel, 8);
  Seq::LiveDisableReverb(&fixture.channel, 8);
  Seq::LiveSetNoiseClock(&fixture.channel, 8);
  CHECK(std::array<std::uint32_t, 3>{fixture.config.noiseChannels,
                                     fixture.config.reverbChannels,
                                     fixture.config.fmChannels} ==
        std::array<std::uint32_t, 3>{8, 4, 0});
  CHECK(fixture.config.noiseClock == 0);

  fixture.channel.type = kSoundEffectType;
  Seq::LiveEnableFrequencyModulation(&fixture.channel, 8);
  CHECK(Audio_Mixer_voiceScheduler.fmVoices == 0);
  fixture.channel.updateFlags = Seq::kFrequencyModulationAllowed;
  Seq::LiveEnableFrequencyModulation(&fixture.channel, 8);
  Seq::LiveSetNoiseClock(&fixture.channel, 8);
  CHECK(Audio_Mixer_voiceScheduler.fmVoices == 8);
  CHECK(Audio_Mixer_voiceScheduler.noiseClock == 0x12);
  CHECK(fixture.Advance() == 2);
  CHECK(Audio_Driver_controlLatches.updateFlags == 0x110);
}

TEST_CASE("Live patch selection points the keymap into the patch table",
          "[audio-seq-voices][audio]") {
  LiveVoiceFixture fixture;
  std::array<std::uint8_t, 0x40> patches{};
  patches[0] = 0x01;
  patches[1] = 0x80;
  patches[2] = 0x10;
  fixture.config.patchTable =
      reinterpret_cast<Audio_AkaoSequenceFile*>(patches.data());
  fixture.sequence = {1, 0};
  fixture.channel.voice.volumeScale = 0x7F;
  Seq::LiveSelectPatch(&fixture.channel, 1);
  CHECK(fixture.channel.keymap ==
        reinterpret_cast<char*>(patches.data()) + 0x30);
  CHECK(fixture.channel.updateFlags == Seq::kPatchKeymapActive);
  CHECK(fixture.channel.key == 0xFF);
  CHECK(fixture.channel.voice.volumeScale == 0x7F);
  Seq::LiveSelectPatch(&fixture.channel, 1);
  CHECK(fixture.channel.voice.volumeScale == 0);
  CHECK(fixture.channel.updateFlags == 0);
  CHECK(fixture.Advance() == 2);
}

TEST_CASE("Live instrument and LFO commands read their legacy tables",
          "[audio-seq-voices][audio]") {
  LiveVoiceFixture fixture;
  Audio_Instrument& entry = Audio_Samples_instruments[0x61];
  entry = {0x4000, 0x4100, 3, 60, 0x8F00, 0x1234};
  fixture.config.statusFlags = 0x40;
  std::array<std::int16_t, 4> wave{};
  Audio_Seq_kLfoWaveforms[0x21] = wave.data();
  fixture.sequence = {0x41, 0, 2, 0x21};
  Seq::LiveLoadInstrument(&fixture.channel, 1);
  CHECK(fixture.channel.instrumentIndex == 0x61);
  CHECK(std::pair(fixture.channel.voice.sampleStart,
                  fixture.channel.voice.loopStart) ==
        std::pair<std::uint32_t, std::uint32_t>(0x4000, 0x4100));
  fixture.channel.voice.adsrLower = 0;
  fixture.channel.voice.adsrUpper = 0;
  Seq::LiveResetEnvelope(&fixture.channel, 1);
  CHECK(std::pair(fixture.channel.voice.adsrLower,
                  fixture.channel.voice.adsrUpper) ==
        std::pair<std::uint16_t, std::uint16_t>(0x8F00, 0x1234));
  Seq::LiveEnableTremolo(&fixture.channel, 1);
  CHECK(fixture.channel.tremoloWave == wave.data());
  CHECK(fixture.Advance() == 4);
}

TEST_CASE(
    "Voice handlers keep module bindings only where opcode tables store them",
    "[audio-seq-voices][audio]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40",
                                          0x8001B734U) == nullptr);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40",
                                          0x8001B778U) == nullptr);
  CHECK(
      VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40", 0x8001B4ECU) ==
      reinterpret_cast<void*>(&Seq::LiveSetVolumeBalance));
}
