// Hand-maintained (tools/port/owned.csv).
// Live binding for the sequencer's voice commands (ChannelCommands.cpp,
// Envelope.cpp and Modulation.cpp): the channel records, the
// active music layer, the sound-effect voice scheduler, the instrument table,
// the LFO waveform table and the driver's update requests.
#include "Audio/SeqVoiceCommandsLive.hpp"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <type_traits>

#include "Audio/LfoWaveforms.h"
#include "Audio/PlaybackReset.hpp"
#include "Audio/SeqChannelCommands.hpp"
#include "Audio/SeqEnvelopeCommands.hpp"
#include "Audio/SeqModulationCommands.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SeqSequencer.hpp"
#include "Audio/SeqVoiceCommands.hpp"
#include "Audio/VoiceOutput.hpp"
#include "Core/Diagnostics.h"
#include "Core/Memory/HeapLive.hpp"

extern "C" {
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/Sequencer.h"
}

namespace VS::Audio::Seq {
namespace {
using Driver::ChannelKind;
using Driver::VoiceMode;

// Every byte selector names a waveform slot and every instrument index the
// commands accept names a table entry.
static_assert(std::size(Audio_Seq_kLfoWaveforms) ==
              std::size_t{std::numeric_limits<std::uint8_t>::max()} + 1);
static_assert(std::size(Audio_Samples_instruments) ==
              Samples::kInstrumentSlotCount);
static_assert(std::is_convertible_v<
              decltype(&LiveSetVolume),
              std::remove_all_extents_t<decltype(Audio_Seq_kOpcodeHandlers)>>);
static_assert(
    std::is_convertible_v<decltype(&LiveSetVolume),
                          std::remove_all_extents_t<
                              decltype(Audio_Seq_kExtendedOpcodeHandlers)>>);

constexpr std::size_t kKeyOnRampOperands = 3;
constexpr std::size_t kOscillatorOperands = 3;

// The active music layer's sequence data when it is a live game heap block.
std::optional<std::span<const std::uint8_t>> MusicData() {
  const Audio_MusicConfig* layer = Audio_Seq_activeMusicConfig;
  if (layer == nullptr || layer->sequenceBase == 0) return std::nullopt;
  const auto* base = reinterpret_cast<const std::uint8_t*>(layer->sequenceBase);
  const auto capacity = Core::Memory::LivePayloadCapacity(base);
  if (!capacity) return std::nullopt;
  return std::span{base, *capacity};
}

// The offset of `first` in `data` when `bytes` bytes from it lie inside.
std::optional<std::size_t> OffsetIn(std::span<const std::uint8_t> data,
                                    const std::uint8_t* first,
                                    std::size_t bytes) {
  const auto start = reinterpret_cast<std::uintptr_t>(data.data());
  const auto address = reinterpret_cast<std::uintptr_t>(first);
  if (address < start || address - start > data.size()) return std::nullopt;
  const std::size_t offset = address - start;
  if (data.size() - offset < bytes) return std::nullopt;
  return offset;
}

// Whether `bytes` bytes at `first` lie in the active music layer's sequence
// data. Data outside the game heap is trusted, as the original trusted it.
bool InMusicData(const std::uint8_t* first, std::size_t bytes) {
  const std::optional<std::span<const std::uint8_t>> data = MusicData();
  return !data || OffsetIn(*data, first, bytes).has_value();
}

// The bytes from the channel's program counter to the end of the memory its
// program lies in (LiveChannelProgram). A program anywhere else gets the
// `operand_count` bytes the original read unchecked; the sequencer step
// refuses such programs before any command runs.
Operands OperandWindow(const Audio_SeqChannel& channel,
                       std::size_t operand_count) {
  char* counter = channel.programCounter;
  if (counter == nullptr) return {};
  if (const auto memory = LiveChannelProgram(channel)) {
    return memory->From(counter).View();
  }
  return {reinterpret_cast<const std::uint8_t*>(counter), operand_count};
}

std::uint32_t& VoiceModeMask(ChannelKind kind, VoiceMode mode) {
  const bool music = kind == ChannelKind::kMusic;
  if (mode == VoiceMode::kNoise) {
    return music ? Audio_Seq_activeMusicConfig->noiseChannels
                 : Audio_Mixer_voiceScheduler.noiseVoices;
  }
  if (mode == VoiceMode::kFrequencyModulation) {
    return music ? Audio_Seq_activeMusicConfig->fmChannels
                 : Audio_Mixer_voiceScheduler.fmVoices;
  }
  return music ? Audio_Seq_activeMusicConfig->reverbChannels
               : Audio_Mixer_voiceScheduler.reverbVoices;
}

std::uint16_t& NoiseClockOf(ChannelKind kind) {
  return kind == ChannelKind::kMusic ? Audio_Seq_activeMusicConfig->noiseClock
                                     : Audio_Mixer_voiceScheduler.noiseClock;
}

// The commands reach the music layer only after Run has checked that the
// channel has one (LayerUse).
class LiveVoiceCommandHost final : public VoiceCommandHost {
 public:
  std::int32_t ResolveInstrument(ChannelKind kind,
                                 std::int32_t effect_voice_mask,
                                 std::uint8_t instrument) override {
    if (kind == ChannelKind::kMusic) {
      return ResolveMusicInstrument(Audio_Seq_activeMusicConfig->statusFlags,
                                    instrument);
    }
    return ResolveSoundEffectInstrument(effect_voice_mask, instrument);
  }

  std::optional<Samples::InstrumentRecord> Instrument(
      std::int32_t index) override {
    return LiveInstrument(index);
  }

  PatchEntry FindPatch(std::uint8_t patch) override {
    const auto* table = reinterpret_cast<const std::uint8_t*>(
        Audio_Seq_activeMusicConfig->patchTable);
    if (table == nullptr) return {};
    const std::uint8_t* entry = table + patch * sizeof(std::uint16_t);
    if (!InMusicData(entry, sizeof(std::uint16_t))) {
      return {PatchLookup::kOutOfRange, 0};
    }
    std::uint16_t offset = 0;
    std::memcpy(&offset, entry, sizeof offset);
    return {PatchLookup::kFound, offset};
  }

  bool HasPatchKeymap(std::uint32_t offset) override {
    const auto* table = reinterpret_cast<const std::uint8_t*>(
        Audio_Seq_activeMusicConfig->patchTable);
    return InMusicData(table + offset, 1);
  }

  bool HasDrumKeymap() override {
    return Audio_Seq_activeMusicConfig->keymapTable != nullptr;
  }

  bool HasLfoWaveform(std::uint8_t shape) override {
    return Audio_Seq_LfoWaveform(shape) != nullptr;
  }

  void SetVoiceMode(ChannelKind kind, VoiceMode mode,
                    std::uint32_t channel_bits, bool enabled) override {
    std::uint32_t& mask = VoiceModeMask(kind, mode);
    mask = enabled ? mask | channel_bits : mask & ~channel_bits;
  }

  std::uint16_t NoiseClock(ChannelKind kind) override {
    return NoiseClockOf(kind);
  }

  void SetNoiseClock(ChannelKind kind, std::uint16_t clock) override {
    NoiseClockOf(kind) = clock;
  }

  void RequestVoiceUpdate(std::uint32_t flags) override {
    Audio_Driver_controlLatches.updateFlags |= flags;
  }
};

void Refused(const char* command, const char* reason) {
  ReportRefusal(command, reason);
}

const char* RefusalReason(CommandStatus status) {
  switch (status) {
    case CommandStatus::kTruncated:
      return "the sequence data ends inside its operands";
    case CommandStatus::kWaveformMissing:
      return "no LFO waveform for the selector";
    case CommandStatus::kInstrumentMissing:
      return "the instrument index is outside the instrument table";
    case CommandStatus::kPatchOutOfRange:
      return "the patch entry or its keymap lies outside the music data";
    case CommandStatus::kApplied:
      break;
  }
  return "applied";
}

// The music layer state a command reads: none, the layer of music channels
// only (sound effects use the voice scheduler), or the layer for any channel.
enum class LayerUse : std::uint8_t { kNone, kMusicChannels, kAlways };

struct CommandInfo {
  const char* name;
  std::size_t operands;
  LayerUse layer = LayerUse::kNone;
};

bool MissingMusicLayer(const Audio_SeqChannel& channel, LayerUse layer) {
  if (Audio_Seq_activeMusicConfig != nullptr) return false;
  if (layer == LayerUse::kAlways) return true;
  return layer == LayerUse::kMusicChannels &&
         LiveChannelKind(channel) == ChannelKind::kMusic;
}

// Each command shape, as (channel, operands, host, channel bits) -> result.
CommandResult Invoke(void (*command)(ChannelState&), ChannelState& state,
                     Operands /*operands*/, VoiceCommandHost& /*host*/,
                     std::uint32_t /*channel_bits*/) {
  command(state);
  return Applied(0);
}

CommandResult Invoke(CommandResult (*command)(ChannelState&, Operands),
                     ChannelState& state, Operands operands,
                     VoiceCommandHost& /*host*/,
                     std::uint32_t /*channel_bits*/) {
  return command(state, operands);
}

CommandResult Invoke(CommandResult (*command)(ChannelState&, Operands,
                                              VoiceCommandHost&),
                     ChannelState& state, Operands operands,
                     VoiceCommandHost& host, std::uint32_t /*channel_bits*/) {
  return command(state, operands, host);
}

CommandResult Invoke(CommandStatus (*command)(ChannelState&, VoiceCommandHost&),
                     ChannelState& state, Operands /*operands*/,
                     VoiceCommandHost& host, std::uint32_t /*channel_bits*/) {
  return {command(state, host), 0};
}

CommandResult Invoke(void (*command)(ChannelState&, VoiceCommandHost&),
                     ChannelState& state, Operands /*operands*/,
                     VoiceCommandHost& host, std::uint32_t /*channel_bits*/) {
  command(state, host);
  return Applied(0);
}

CommandResult Invoke(void (*command)(ChannelState&, VoiceCommandHost&,
                                     std::uint32_t),
                     ChannelState& state, Operands /*operands*/,
                     VoiceCommandHost& host, std::uint32_t channel_bits) {
  command(state, host, channel_bits);
  return Applied(0);
}

CommandResult Invoke(CommandResult (*command)(ChannelState&, Operands,
                                              VoiceCommandHost&, std::uint32_t),
                     ChannelState& state, Operands operands,
                     VoiceCommandHost& host, std::uint32_t channel_bits) {
  return command(state, operands, host, channel_bits);
}

// Runs `command` over the channel's record: a refused command changes none of
// its state, and the program counter advances by the operands it consumed.
template <typename Command>
void Run(Audio_SeqChannel* channel, int channel_bit, const CommandInfo& info,
         Command command) {
  if (channel == nullptr) {
    Refused(info.name, "no channel");
    return;
  }
  const Operands operands = OperandWindow(*channel, info.operands);
  if (MissingMusicLayer(*channel, info.layer)) {
    Refused(info.name, "no active music layer");
    if (operands.size() >= info.operands) {
      channel->programCounter += info.operands;
    }
    return;
  }
  ChannelState state = LoadChannelState(*channel);
  LiveVoiceCommandHost host;
  const CommandResult result = Invoke(command, state, operands, host,
                                      static_cast<std::uint32_t>(channel_bit));
  if (result.status != CommandStatus::kApplied) {
    Refused(info.name, RefusalReason(result.status));
  }
  StoreChannelState(*channel, state);
  channel->programCounter += result.consumed;
}
}  // namespace

void LiveSetVolumeBalance(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"A3 SetVolumeBalance", 1}, SetVolumeBalance);
}

void LiveSlideVolumeBalance(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"FE12 SlideVolumeBalance", 2}, SlideVolumeBalance);
}

void LiveSetVolume(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"A8 SetVolume", 1}, SetVolume);
}

void LiveSlideVolume(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"A9 SlideVolume", 2}, SlideVolume);
}

void LiveSetKeyOnVolumeRamp(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"FE19 SetKeyOnVolumeRamp", kKeyOnRampOperands},
      SetKeyOnVolumeRamp);
}

void LiveEnableFullNoteLength(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"FE1A EnableFullNoteLength", 0},
      EnableFullNoteLength);
}

void LiveDisableFullNoteLength(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"FE1B DisableFullNoteLength", 0},
      DisableFullNoteLength);
}

void LiveSetPan(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"AA SetPan", 1}, SetPan);
}

void LiveSlidePan(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"AB SlidePan", 2}, SlidePan);
}

void LiveSetOctave(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"A5 SetOctave", 1}, SetOctave);
}

void LiveIncreaseOctave(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"A6 IncreaseOctave", 0}, IncreaseOctave);
}

void LiveDecreaseOctave(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"A7 DecreaseOctave", 0}, DecreaseOctave);
}

void LiveLoadInstrument(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"A1 LoadInstrument", 1, LayerUse::kMusicChannels},
      LoadInstrument);
}

void LiveLoadSilentInstrument(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"FE0A LoadSilentInstrument", 1},
      LoadSilentInstrument);
}

void LiveSelectPatch(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"FE14 SelectPatch", 1, LayerUse::kAlways},
      SelectPatch);
}

void LiveSetTranspose(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"C0 SetTranspose", 1}, SetTranspose);
}

void LiveAddTranspose(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"C1 AddTranspose", 1}, AddTranspose);
}

void LiveSetPitchBendSlide(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"A4 SetPitchBendSlide", 2}, SetPitchBendSlide);
}

void LiveEnablePortamento(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"DA EnablePortamento", 1}, EnablePortamento);
}

void LiveDisablePortamento(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"DB DisablePortamento", 0}, DisablePortamento);
}

void LiveSetFineTune(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"D8 SetFineTune", 1}, SetFineTune);
}

void LiveAddFineTune(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"D9 AddFineTune", 1}, AddFineTune);
}

void LiveEnableLegato(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"CC EnableLegato", 0}, EnableLegato);
}

void LiveEnableSustainedSoundEffect(Audio_SeqChannel* channel,
                                    int channel_bit) {
  Run(channel, channel_bit, {"D0 EnableSustainedSoundEffect", 0},
      EnableSustainedSoundEffect);
}

void LiveSetNextNoteLength(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"A2 SetNextNoteLength", 1}, SetNextNoteLength);
}

void LiveSetFixedNoteLength(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"DC SetFixedNoteLength", 1}, SetFixedNoteLength);
}

void LiveEnableDrumKeymap(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"FE04 EnableDrumKeymap", 0, LayerUse::kAlways},
      EnableDrumKeymap);
}

void LiveMuteInstrument(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"FE05 MuteInstrument", 0}, MuteInstrument);
}

void LiveEnableLoopBoundaryStop(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"E0 EnableLoopBoundaryStop", 0},
      EnableLoopBoundaryStop);
}

void LiveResetEnvelope(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"B3 ResetEnvelope", 0}, ResetEnvelope);
}

void LiveSetAttackRate(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"AD SetAttackRate", 1}, SetAttackRate);
}

void LiveSetDecayRate(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"AE SetDecayRate", 1}, SetDecayRate);
}

void LiveSetSustainLevel(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"AF SetSustainLevel", 1}, SetSustainLevel);
}

void LiveSetSustainRate(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"B1 SetSustainRate", 1}, SetSustainRate);
}

void LiveSetReleaseRate(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"B2 SetReleaseRate", 1}, SetReleaseRate);
}

void LiveSetAttackMode(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"B7 SetAttackMode", 1}, SetAttackMode);
}

void LiveSetSustainMode(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"BB SetSustainMode", 1}, SetSustainMode);
}

void LiveSetReleaseMode(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"BF SetReleaseMode", 1}, SetReleaseMode);
}

void LiveSetDecayAndSustain(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"B0 SetDecayAndSustain", 2}, SetDecayAndSustain);
}

void LiveEnableVibrato(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"B4 EnableVibrato", kOscillatorOperands},
      EnableVibrato);
}

void LiveSetVibratoDepth(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"B5 SetVibratoDepth", 1}, SetVibratoDepth);
}

void LiveSlideVibratoDepth(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"DD SlideVibratoDepth", 2}, SlideVibratoDepth);
}

void LiveDisableVibrato(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"B6 DisableVibrato", 0}, DisableVibrato);
}

void LiveEnableTremolo(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"B8 EnableTremolo", kOscillatorOperands},
      EnableTremolo);
}

void LiveSetTremoloDepth(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"B9 SetTremoloDepth", 1}, SetTremoloDepth);
}

void LiveSlideTremoloDepth(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"DE SlideTremoloDepth", 2}, SlideTremoloDepth);
}

void LiveDisableTremolo(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"BA DisableTremolo", 0}, DisableTremolo);
}

void LiveEnableAutoPan(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"BC EnableAutoPan", 2}, EnableAutoPan);
}

void LiveSetAutoPanDepth(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"BD SetAutoPanDepth", 1}, SetAutoPanDepth);
}

void LiveSlideAutoPanDepth(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"DF SlideAutoPanDepth", 2}, SlideAutoPanDepth);
}

void LiveDisableAutoPan(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"BE DisableAutoPan", 0}, DisableAutoPan);
}

void LiveEnableNoise(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"C4 EnableNoise", 0, LayerUse::kMusicChannels},
      EnableNoise);
}

void LiveDisableNoise(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"C5 DisableNoise", 0, LayerUse::kMusicChannels},
      DisableNoise);
}

void LiveEnableFrequencyModulation(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit,
      {"C6 EnableFrequencyModulation", 0, LayerUse::kMusicChannels},
      EnableFrequencyModulation);
}

void LiveDisableFrequencyModulation(Audio_SeqChannel* channel,
                                    int channel_bit) {
  Run(channel, channel_bit,
      {"C7 DisableFrequencyModulation", 0, LayerUse::kMusicChannels},
      DisableFrequencyModulation);
}

void LiveEnableReverb(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"C2 EnableReverb", 0, LayerUse::kMusicChannels},
      EnableReverb);
}

void LiveDisableReverb(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"C3 DisableReverb", 0, LayerUse::kMusicChannels},
      DisableReverb);
}

void LiveSetNoiseClock(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"AC SetNoiseClock", 1, LayerUse::kMusicChannels},
      SetNoiseClock);
}

void LiveEnableTimedNoise(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit,
      {"CE EnableTimedNoise", 1, LayerUse::kMusicChannels}, EnableTimedNoise);
}

void LiveSetNoiseToggleDelay(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"CF SetNoiseToggleDelay", 1}, SetNoiseToggleDelay);
}

void LiveEnableTimedFrequencyModulation(Audio_SeqChannel* channel,
                                        int channel_bit) {
  Run(channel, channel_bit,
      {"D2 EnableTimedFrequencyModulation", 1, LayerUse::kMusicChannels},
      EnableTimedFrequencyModulation);
}

void LiveSetFrequencyModulationToggleDelay(Audio_SeqChannel* channel,
                                           int channel_bit) {
  Run(channel, channel_bit, {"D3 SetFrequencyModulationToggleDelay", 1},
      SetFrequencyModulationToggleDelay);
}

void LiveResetVoiceEffects(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit,
      {"CB ResetVoiceEffects", 0, LayerUse::kMusicChannels}, ResetVoiceEffects);
}

void LiveEnablePitchSidechain(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"D4 EnablePitchSidechain", 0},
      EnablePitchSidechain);
}

void LiveDisablePitchSidechain(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"D5 DisablePitchSidechain", 0},
      DisablePitchSidechain);
}

void LiveEnableVolumeSidechain(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"D6 EnableVolumeSidechain", 0},
      EnableVolumeSidechain);
}

void LiveDisableVolumeSidechain(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"D7 DisableVolumeSidechain", 0},
      DisableVolumeSidechain);
}

void LiveSetRandomPitchDepth(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"E1 SetRandomPitchDepth", 1}, SetRandomPitchDepth);
}

void LiveResetRandomPitchDepth(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"E2 ResetRandomPitchDepth", 0},
      ResetRandomPitchDepth);
}

std::uint64_t RefusedLiveVoiceCommandCount() noexcept {
  return LiveRefusalCount();
}
}  // namespace VS::Audio::Seq
