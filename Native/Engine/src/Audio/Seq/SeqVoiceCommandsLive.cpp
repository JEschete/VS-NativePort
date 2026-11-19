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

#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/LfoWaveforms.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/PlaybackReset.hpp"
#include "Audio/Samples/SampleBanks.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/Seq/Sequencer.hpp"
#include "Audio/SeqChannelCommands.hpp"
#include "Audio/SeqEnvelopeCommands.hpp"
#include "Audio/SeqModulationCommands.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SeqSequencer.hpp"
#include "Audio/SeqVoiceCommands.hpp"
#include "Audio/VoiceOutput.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"

namespace VS::Audio::Seq {
namespace {
using Driver::ChannelKind;
using Driver::VoiceMode;

// Every byte selector names a waveform slot and every instrument index the
// commands accept names a table entry.
static_assert(std::size(VS::Audio::Seq::kLfoWaveforms) ==
              std::size_t{std::numeric_limits<std::uint8_t>::max()} + 1);
static_assert(std::size(VS::Audio::Samples::instruments) ==
              Samples::kInstrumentSlotCount);
static_assert(
    std::is_convertible_v<
        decltype(&LiveSetVolume),
        std::remove_all_extents_t<decltype(VS::Audio::Seq::kOpcodeHandlers)>>);
static_assert(std::is_convertible_v<
              decltype(&LiveSetVolume),
              std::remove_all_extents_t<
                  decltype(VS::Audio::Seq::kExtendedOpcodeHandlers)>>);

constexpr std::size_t kKeyOnRampOperands = 3;
constexpr std::size_t kOscillatorOperands = 3;

// The active music layer's sequence data when it is a live game heap block.
std::optional<std::span<const std::uint8_t>> MusicData() {
  const VS::Audio::MusicConfig* layer = VS::Audio::Seq::activeMusicConfig;
  if (layer == nullptr || layer->sequence_base == 0) return std::nullopt;
  const auto* base =
      reinterpret_cast<const std::uint8_t*>(layer->sequence_base);
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
Operands OperandWindow(const VS::Audio::SeqChannel& channel,
                       std::size_t operand_count) {
  std::uint8_t* counter = channel.program_counter;
  if (counter == nullptr) return {};
  if (const auto memory = LiveChannelProgram(channel)) {
    return memory->From(counter).View();
  }
  return {counter, operand_count};
}

std::uint32_t& VoiceModeMask(ChannelKind kind, VoiceMode mode) {
  const bool music = kind == ChannelKind::kMusic;
  if (mode == VoiceMode::kNoise) {
    return music ? VS::Audio::Seq::activeMusicConfig->noise_channels
                 : VS::Audio::Mixer::voiceScheduler.noise_voices;
  }
  if (mode == VoiceMode::kFrequencyModulation) {
    return music ? VS::Audio::Seq::activeMusicConfig->fm_channels
                 : VS::Audio::Mixer::voiceScheduler.fm_voices;
  }
  return music ? VS::Audio::Seq::activeMusicConfig->reverb_channels
               : VS::Audio::Mixer::voiceScheduler.reverb_voices;
}

std::uint16_t& NoiseClockOf(ChannelKind kind) {
  return kind == ChannelKind::kMusic
             ? VS::Audio::Seq::activeMusicConfig->noise_clock
             : VS::Audio::Mixer::voiceScheduler.noise_clock;
}

// The commands reach the music layer only after Run has checked that the
// channel has one (LayerUse).
class LiveVoiceCommandHost final : public VoiceCommandHost {
 public:
  std::int32_t ResolveInstrument(ChannelKind kind,
                                 std::int32_t effect_voice_mask,
                                 std::uint8_t instrument) override {
    if (kind == ChannelKind::kMusic) {
      return ResolveMusicInstrument(
          VS::Audio::Seq::activeMusicConfig->status_flags, instrument);
    }
    return ResolveSoundEffectInstrument(effect_voice_mask, instrument);
  }

  std::optional<Samples::InstrumentRecord> Instrument(
      std::int32_t index) override {
    return LiveInstrument(index);
  }

  PatchEntry FindPatch(std::uint8_t patch) override {
    const auto* table = reinterpret_cast<const std::uint8_t*>(
        VS::Audio::Seq::activeMusicConfig->patch_table);
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
        VS::Audio::Seq::activeMusicConfig->patch_table);
    return InMusicData(table + offset, 1);
  }

  bool HasDrumKeymap() override {
    return VS::Audio::Seq::activeMusicConfig->keymap_table != nullptr;
  }

  bool HasLfoWaveform(std::uint8_t shape) override {
    return LfoWaveform(shape) != nullptr;
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
    VS::Audio::Driver::controlLatches.update_flags |= flags;
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

bool MissingMusicLayer(const VS::Audio::SeqChannel& channel, LayerUse layer) {
  if (VS::Audio::Seq::activeMusicConfig != nullptr) return false;
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
void Run(VS::Audio::SeqChannel* channel, int channel_bit,
         const CommandInfo& info, Command command) {
  if (channel == nullptr) {
    Refused(info.name, "no channel");
    return;
  }
  const Operands operands = OperandWindow(*channel, info.operands);
  if (MissingMusicLayer(*channel, info.layer)) {
    Refused(info.name, "no active music layer");
    if (operands.size() >= info.operands) {
      channel->program_counter += info.operands;
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
  channel->program_counter += result.consumed;
}
}  // namespace

void LiveSetVolumeBalance(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"A3 SetVolumeBalance", 1}, SetVolumeBalance);
}

void LiveSlideVolumeBalance(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"FE12 SlideVolumeBalance", 2}, SlideVolumeBalance);
}

void LiveSetVolume(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"A8 SetVolume", 1}, SetVolume);
}

void LiveSlideVolume(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"A9 SlideVolume", 2}, SlideVolume);
}

void LiveSetKeyOnVolumeRamp(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"FE19 SetKeyOnVolumeRamp", kKeyOnRampOperands},
      SetKeyOnVolumeRamp);
}

void LiveEnableFullNoteLength(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"FE1A EnableFullNoteLength", 0},
      EnableFullNoteLength);
}

void LiveDisableFullNoteLength(VS::Audio::SeqChannel* channel,
                               int channel_bit) {
  Run(channel, channel_bit, {"FE1B DisableFullNoteLength", 0},
      DisableFullNoteLength);
}

void LiveSetPan(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"AA SetPan", 1}, SetPan);
}

void LiveSlidePan(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"AB SlidePan", 2}, SlidePan);
}

void LiveSetOctave(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"A5 SetOctave", 1}, SetOctave);
}

void LiveIncreaseOctave(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"A6 IncreaseOctave", 0}, IncreaseOctave);
}

void LiveDecreaseOctave(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"A7 DecreaseOctave", 0}, DecreaseOctave);
}

void LiveLoadInstrument(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"A1 LoadInstrument", 1, LayerUse::kMusicChannels},
      LoadInstrument);
}

void LiveLoadSilentInstrument(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"FE0A LoadSilentInstrument", 1},
      LoadSilentInstrument);
}

void LiveSelectPatch(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"FE14 SelectPatch", 1, LayerUse::kAlways},
      SelectPatch);
}

void LiveSetTranspose(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"C0 SetTranspose", 1}, SetTranspose);
}

void LiveAddTranspose(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"C1 AddTranspose", 1}, AddTranspose);
}

void LiveSetPitchBendSlide(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"A4 SetPitchBendSlide", 2}, SetPitchBendSlide);
}

void LiveEnablePortamento(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"DA EnablePortamento", 1}, EnablePortamento);
}

void LiveDisablePortamento(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"DB DisablePortamento", 0}, DisablePortamento);
}

void LiveSetFineTune(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"D8 SetFineTune", 1}, SetFineTune);
}

void LiveAddFineTune(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"D9 AddFineTune", 1}, AddFineTune);
}

void LiveEnableLegato(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"CC EnableLegato", 0}, EnableLegato);
}

void LiveEnableSustainedSoundEffect(VS::Audio::SeqChannel* channel,
                                    int channel_bit) {
  Run(channel, channel_bit, {"D0 EnableSustainedSoundEffect", 0},
      EnableSustainedSoundEffect);
}

void LiveSetNextNoteLength(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"A2 SetNextNoteLength", 1}, SetNextNoteLength);
}

void LiveSetFixedNoteLength(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"DC SetFixedNoteLength", 1}, SetFixedNoteLength);
}

void LiveEnableDrumKeymap(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"FE04 EnableDrumKeymap", 0, LayerUse::kAlways},
      EnableDrumKeymap);
}

void LiveMuteInstrument(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"FE05 MuteInstrument", 0}, MuteInstrument);
}

void LiveEnableLoopBoundaryStop(VS::Audio::SeqChannel* channel,
                                int channel_bit) {
  Run(channel, channel_bit, {"E0 EnableLoopBoundaryStop", 0},
      EnableLoopBoundaryStop);
}

void LiveResetEnvelope(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"B3 ResetEnvelope", 0}, ResetEnvelope);
}

void LiveSetAttackRate(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"AD SetAttackRate", 1}, SetAttackRate);
}

void LiveSetDecayRate(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"AE SetDecayRate", 1}, SetDecayRate);
}

void LiveSetSustainLevel(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"AF SetSustainLevel", 1}, SetSustainLevel);
}

void LiveSetSustainRate(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"B1 SetSustainRate", 1}, SetSustainRate);
}

void LiveSetReleaseRate(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"B2 SetReleaseRate", 1}, SetReleaseRate);
}

void LiveSetAttackMode(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"B7 SetAttackMode", 1}, SetAttackMode);
}

void LiveSetSustainMode(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"BB SetSustainMode", 1}, SetSustainMode);
}

void LiveSetReleaseMode(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"BF SetReleaseMode", 1}, SetReleaseMode);
}

void LiveSetDecayAndSustain(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"B0 SetDecayAndSustain", 2}, SetDecayAndSustain);
}

void LiveEnableVibrato(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"B4 EnableVibrato", kOscillatorOperands},
      EnableVibrato);
}

void LiveSetVibratoDepth(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"B5 SetVibratoDepth", 1}, SetVibratoDepth);
}

void LiveSlideVibratoDepth(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"DD SlideVibratoDepth", 2}, SlideVibratoDepth);
}

void LiveDisableVibrato(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"B6 DisableVibrato", 0}, DisableVibrato);
}

void LiveEnableTremolo(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"B8 EnableTremolo", kOscillatorOperands},
      EnableTremolo);
}

void LiveSetTremoloDepth(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"B9 SetTremoloDepth", 1}, SetTremoloDepth);
}

void LiveSlideTremoloDepth(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"DE SlideTremoloDepth", 2}, SlideTremoloDepth);
}

void LiveDisableTremolo(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"BA DisableTremolo", 0}, DisableTremolo);
}

void LiveEnableAutoPan(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"BC EnableAutoPan", 2}, EnableAutoPan);
}

void LiveSetAutoPanDepth(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"BD SetAutoPanDepth", 1}, SetAutoPanDepth);
}

void LiveSlideAutoPanDepth(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"DF SlideAutoPanDepth", 2}, SlideAutoPanDepth);
}

void LiveDisableAutoPan(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"BE DisableAutoPan", 0}, DisableAutoPan);
}

void LiveEnableNoise(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"C4 EnableNoise", 0, LayerUse::kMusicChannels},
      EnableNoise);
}

void LiveDisableNoise(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"C5 DisableNoise", 0, LayerUse::kMusicChannels},
      DisableNoise);
}

void LiveEnableFrequencyModulation(VS::Audio::SeqChannel* channel,
                                   int channel_bit) {
  Run(channel, channel_bit,
      {"C6 EnableFrequencyModulation", 0, LayerUse::kMusicChannels},
      EnableFrequencyModulation);
}

void LiveDisableFrequencyModulation(VS::Audio::SeqChannel* channel,
                                    int channel_bit) {
  Run(channel, channel_bit,
      {"C7 DisableFrequencyModulation", 0, LayerUse::kMusicChannels},
      DisableFrequencyModulation);
}

void LiveEnableReverb(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"C2 EnableReverb", 0, LayerUse::kMusicChannels},
      EnableReverb);
}

void LiveDisableReverb(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"C3 DisableReverb", 0, LayerUse::kMusicChannels},
      DisableReverb);
}

void LiveSetNoiseClock(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"AC SetNoiseClock", 1, LayerUse::kMusicChannels},
      SetNoiseClock);
}

void LiveEnableTimedNoise(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit,
      {"CE EnableTimedNoise", 1, LayerUse::kMusicChannels}, EnableTimedNoise);
}

void LiveSetNoiseToggleDelay(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"CF SetNoiseToggleDelay", 1}, SetNoiseToggleDelay);
}

void LiveEnableTimedFrequencyModulation(VS::Audio::SeqChannel* channel,
                                        int channel_bit) {
  Run(channel, channel_bit,
      {"D2 EnableTimedFrequencyModulation", 1, LayerUse::kMusicChannels},
      EnableTimedFrequencyModulation);
}

void LiveSetFrequencyModulationToggleDelay(VS::Audio::SeqChannel* channel,
                                           int channel_bit) {
  Run(channel, channel_bit, {"D3 SetFrequencyModulationToggleDelay", 1},
      SetFrequencyModulationToggleDelay);
}

void LiveResetVoiceEffects(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit,
      {"CB ResetVoiceEffects", 0, LayerUse::kMusicChannels}, ResetVoiceEffects);
}

void LiveEnablePitchSidechain(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"D4 EnablePitchSidechain", 0},
      EnablePitchSidechain);
}

void LiveDisablePitchSidechain(VS::Audio::SeqChannel* channel,
                               int channel_bit) {
  Run(channel, channel_bit, {"D5 DisablePitchSidechain", 0},
      DisablePitchSidechain);
}

void LiveEnableVolumeSidechain(VS::Audio::SeqChannel* channel,
                               int channel_bit) {
  Run(channel, channel_bit, {"D6 EnableVolumeSidechain", 0},
      EnableVolumeSidechain);
}

void LiveDisableVolumeSidechain(VS::Audio::SeqChannel* channel,
                                int channel_bit) {
  Run(channel, channel_bit, {"D7 DisableVolumeSidechain", 0},
      DisableVolumeSidechain);
}

void LiveSetRandomPitchDepth(VS::Audio::SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, {"E1 SetRandomPitchDepth", 1}, SetRandomPitchDepth);
}

void LiveResetRandomPitchDepth(VS::Audio::SeqChannel* channel,
                               int channel_bit) {
  Run(channel, channel_bit, {"E2 ResetRandomPitchDepth", 0},
      ResetRandomPitchDepth);
}

std::uint64_t RefusedLiveVoiceCommandCount() noexcept {
  return LiveRefusalCount();
}
}  // namespace VS::Audio::Seq
