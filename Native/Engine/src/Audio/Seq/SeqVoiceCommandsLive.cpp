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
#include "Audio/SeqVoiceCommands.hpp"
#include "Audio/VoiceOutput.hpp"
#include "Core/Diagnostics.h"
#include "Core/Memory/HeapLive.hpp"

extern "C" {
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/Control.h"
#include "Audio/Seq/Sequencer.h"
}

namespace VS::Audio::Seq {
namespace {
using Driver::ChannelKind;
using Driver::VoiceMode;

// A field's offset in this build and on the PS1.
struct FieldPin {
  std::size_t native;
  std::size_t original;
};

// Whether each field lies at its PS1 distance from `anchor`. The channel's and
// music layer's pointers are wider here, so the fields after them are pinned
// from the first field that follows them.
constexpr bool PinsMatch(std::span<const FieldPin> pins, FieldPin anchor) {
  return std::ranges::all_of(pins, [anchor](const FieldPin& pin) {
    return pin.native >= anchor.native && pin.original >= anchor.original &&
           pin.native - anchor.native == pin.original - anchor.original;
  });
}

constexpr std::size_t kPs1PointerBytes = 4;
constexpr std::size_t kNativePointerBytes = sizeof(void*);

// FSoundChannel (src/include/sound.h:410): the leading pointers by slot.
constexpr std::array kChannelPointerSlots{
    FieldPin{offsetof(Audio_SeqChannel, programCounter) / kNativePointerBytes,
             0x00 / kPs1PointerBytes},
    FieldPin{offsetof(Audio_SeqChannel, keymap) / kNativePointerBytes,
             0x18 / kPs1PointerBytes},
    FieldPin{offsetof(Audio_SeqChannel, vibratoWave) / kNativePointerBytes,
             0x1C / kPs1PointerBytes},
    FieldPin{offsetof(Audio_SeqChannel, tremoloWave) / kNativePointerBytes,
             0x20 / kPs1PointerBytes},
    FieldPin{offsetof(Audio_SeqChannel, autoPanWave) / kNativePointerBytes,
             0x24 / kPs1PointerBytes},
    FieldPin{offsetof(Audio_SeqChannel, effectTags) / kNativePointerBytes,
             0x28 / kPs1PointerBytes},
};
static_assert(PinsMatch(kChannelPointerSlots, {0, 0}));
static_assert(offsetof(Audio_SeqChannel, effectTags) % kNativePointerBytes ==
              0);

constexpr FieldPin kChannelTagsAnchor{
    offsetof(Audio_SeqChannel, effectTags),
    0x28,
};
constexpr std::array kChannelTagFields{
    FieldPin{offsetof(Audio_SeqChannel, pitchBase), 0x2C},
    FieldPin{offsetof(Audio_SeqChannel, pitchSlide), 0x30},
    FieldPin{offsetof(Audio_SeqChannel, updateFlags), 0x34},
    FieldPin{offsetof(Audio_SeqChannel, effectVoiceMask), 0x38},
};
static_assert(PinsMatch(kChannelTagFields, kChannelTagsAnchor));

constexpr FieldPin kChannelScaleAnchor{
    offsetof(Audio_SeqChannel, effectPitchScale),
    0x40,
};
constexpr std::array kChannelLevelFields{
    FieldPin{offsetof(Audio_SeqChannel, volume), 0x48},
    FieldPin{offsetof(Audio_SeqChannel, volumeSlideStep), 0x4C},
    FieldPin{offsetof(Audio_SeqChannel, finePitchDelta), 0x54},
    FieldPin{offsetof(Audio_SeqChannel, keyOnVolume), 0x5C},
    FieldPin{offsetof(Audio_SeqChannel, keyOnVolumeSlideStep), 0x60},
    FieldPin{offsetof(Audio_SeqChannel, type), 0x64},
    FieldPin{offsetof(Audio_SeqChannel, noteLength), 0x66},
    FieldPin{offsetof(Audio_SeqChannel, gateLength), 0x68},
    FieldPin{offsetof(Audio_SeqChannel, instrumentIndex), 0x6A},
    FieldPin{offsetof(Audio_SeqChannel, volumeBalance), 0x82},
    FieldPin{offsetof(Audio_SeqChannel, volumeBalanceSlideLength), 0x84},
    FieldPin{offsetof(Audio_SeqChannel, volumeSlideLength), 0x88},
    FieldPin{offsetof(Audio_SeqChannel, keyOnVolumeSlideLength), 0x8A},
    FieldPin{offsetof(Audio_SeqChannel, effectVolumeSlideLength), 0x8C},
    FieldPin{offsetof(Audio_SeqChannel, pan), 0x8E},
    FieldPin{offsetof(Audio_SeqChannel, panSlideLength), 0x90},
    FieldPin{offsetof(Audio_SeqChannel, octave), 0x94},
    FieldPin{offsetof(Audio_SeqChannel, pitchBendSlideLength), 0x96},
    FieldPin{offsetof(Audio_SeqChannel, portamentoKey), 0x98},
    FieldPin{offsetof(Audio_SeqChannel, portamentoSteps), 0x9A},
    FieldPin{offsetof(Audio_SeqChannel, playModes), 0x9C},
};
static_assert(PinsMatch(kChannelLevelFields, kChannelScaleAnchor));

constexpr std::array kChannelOscillatorFields{
    FieldPin{offsetof(Audio_SeqChannel, vibratoDelay), 0x9E},
    FieldPin{offsetof(Audio_SeqChannel, vibratoDelayLeft), 0xA0},
    FieldPin{offsetof(Audio_SeqChannel, vibratoRate), 0xA2},
    FieldPin{offsetof(Audio_SeqChannel, vibratoRateCounter), 0xA4},
    FieldPin{offsetof(Audio_SeqChannel, vibratoShape), 0xA6},
    FieldPin{offsetof(Audio_SeqChannel, vibratoBase), 0xA8},
    FieldPin{offsetof(Audio_SeqChannel, vibratoDepth), 0xAA},
    FieldPin{offsetof(Audio_SeqChannel, vibratoDepthSlideLength), 0xAC},
    FieldPin{offsetof(Audio_SeqChannel, vibratoDepthSlideStep), 0xAE},
    FieldPin{offsetof(Audio_SeqChannel, tremoloDelay), 0xB0},
    FieldPin{offsetof(Audio_SeqChannel, tremoloDelayLeft), 0xB2},
    FieldPin{offsetof(Audio_SeqChannel, tremoloRate), 0xB4},
    FieldPin{offsetof(Audio_SeqChannel, tremoloRateCounter), 0xB6},
    FieldPin{offsetof(Audio_SeqChannel, tremoloShape), 0xB8},
    FieldPin{offsetof(Audio_SeqChannel, tremoloDepth), 0xBA},
    FieldPin{offsetof(Audio_SeqChannel, tremoloDepthSlideLength), 0xBC},
    FieldPin{offsetof(Audio_SeqChannel, tremoloDepthSlideStep), 0xBE},
    FieldPin{offsetof(Audio_SeqChannel, autoPanRate), 0xC0},
    FieldPin{offsetof(Audio_SeqChannel, autoPanRateCounter), 0xC2},
    FieldPin{offsetof(Audio_SeqChannel, autoPanShape), 0xC4},
    FieldPin{offsetof(Audio_SeqChannel, autoPanDepth), 0xC6},
    FieldPin{offsetof(Audio_SeqChannel, autoPanDepthSlideLength), 0xC8},
    FieldPin{offsetof(Audio_SeqChannel, autoPanDepthSlideStep), 0xCA},
    FieldPin{offsetof(Audio_SeqChannel, noiseTimer), 0xCC},
    FieldPin{offsetof(Audio_SeqChannel, fmTimer), 0xCE},
    FieldPin{offsetof(Audio_SeqChannel, randomPitchDepth), 0xD2},
};
static_assert(PinsMatch(kChannelOscillatorFields, kChannelScaleAnchor));

constexpr std::array kChannelTailFields{
    FieldPin{offsetof(Audio_SeqChannel, storedNoteLength), 0xD4},
    FieldPin{offsetof(Audio_SeqChannel, fixedNoteLength), 0xD6},
    FieldPin{offsetof(Audio_SeqChannel, volumeBalanceSlideStep), 0xD8},
    FieldPin{offsetof(Audio_SeqChannel, effectVolume), 0xDC},
    FieldPin{offsetof(Audio_SeqChannel, effectVolumeSlideStep), 0xDE},
    FieldPin{offsetof(Audio_SeqChannel, panSlideStep), 0xE0},
    FieldPin{offsetof(Audio_SeqChannel, transpose), 0xE2},
    FieldPin{offsetof(Audio_SeqChannel, fineTune), 0xE4},
    FieldPin{offsetof(Audio_SeqChannel, key), 0xE6},
    FieldPin{offsetof(Audio_SeqChannel, pitchBendTarget), 0xE8},
    FieldPin{offsetof(Audio_SeqChannel, portamentoTranspose), 0xEA},
    FieldPin{offsetof(Audio_SeqChannel, vibratoPitch), 0xEC},
    FieldPin{offsetof(Audio_SeqChannel, tremoloVolume), 0xEE},
    FieldPin{offsetof(Audio_SeqChannel, autoPanOffset), 0xF0},
    FieldPin{offsetof(Audio_SeqChannel, voice), 0xF4},
    FieldPin{sizeof(Audio_SeqChannel), 0x110},
};
static_assert(PinsMatch(kChannelTailFields, kChannelScaleAnchor));

// FSoundVoiceParams (src/include/sound.h:316).
constexpr std::array kVoiceFields{
    FieldPin{offsetof(Audio_VoiceParams, dirtyFlags), 0x04},
    FieldPin{offsetof(Audio_VoiceParams, sampleStart), 0x08},
    FieldPin{offsetof(Audio_VoiceParams, loopStart), 0x0C},
    FieldPin{offsetof(Audio_VoiceParams, adsrLower), 0x12},
    FieldPin{offsetof(Audio_VoiceParams, adsrUpper), 0x14},
    FieldPin{offsetof(Audio_VoiceParams, volumeScale), 0x16},
    FieldPin{sizeof(Audio_VoiceParams), 0x1C},
};
static_assert(PinsMatch(kVoiceFields, {0, 0}));

// FSoundInstrumentInfo, the instrument table entries.
constexpr std::array kInstrumentFields{
    FieldPin{offsetof(Audio_Instrument, sampleStart), 0x00},
    FieldPin{offsetof(Audio_Instrument, loopStart), 0x04},
    FieldPin{offsetof(Audio_Instrument, fineTune), 0x08},
    FieldPin{offsetof(Audio_Instrument, rootNote), 0x0A},
    FieldPin{offsetof(Audio_Instrument, adsrLower), 0x0C},
    FieldPin{offsetof(Audio_Instrument, adsrUpper), 0x0E},
    FieldPin{sizeof(Audio_Instrument), 0x10},
};
static_assert(PinsMatch(kInstrumentFields, {0, 0}));

// FSoundChannelConfig (src/include/sound.h:492): the sequence pointers, then
// the voice mode masks and noise clock after them.
constexpr std::array kMusicLayerFields{
    FieldPin{offsetof(Audio_MusicConfig, statusFlags), 0x00},
    FieldPin{offsetof(Audio_MusicConfig, sequenceBase), 0x38},
};
static_assert(PinsMatch(kMusicLayerFields, {0, 0}));
static_assert(offsetof(Audio_MusicConfig, patchTable) ==
              offsetof(Audio_MusicConfig, sequenceBase) +
                  sizeof(std::uintptr_t));
static_assert(offsetof(Audio_MusicConfig, keymapTable) ==
              offsetof(Audio_MusicConfig, patchTable) + kNativePointerBytes);
constexpr FieldPin kMusicLayerModeAnchor{
    offsetof(Audio_MusicConfig, voiceSearchStart),
    0x48,
};
constexpr std::array kMusicLayerModeFields{
    FieldPin{offsetof(Audio_MusicConfig, noiseChannels), 0x4C},
    FieldPin{offsetof(Audio_MusicConfig, reverbChannels), 0x50},
    FieldPin{offsetof(Audio_MusicConfig, fmChannels), 0x54},
    FieldPin{offsetof(Audio_MusicConfig, noiseClock), 0x72},
};
static_assert(PinsMatch(kMusicLayerModeFields, kMusicLayerModeAnchor));

// FSoundVoiceSchedulerState (src/include/sound.h:274) and the driver flags.
constexpr std::array kSchedulerFields{
    FieldPin{offsetof(Audio_VoiceScheduler, noiseVoices), 0x1C},
    FieldPin{offsetof(Audio_VoiceScheduler, reverbVoices), 0x20},
    FieldPin{offsetof(Audio_VoiceScheduler, fmVoices), 0x24},
    FieldPin{offsetof(Audio_VoiceScheduler, noiseClock), 0x28},
    FieldPin{sizeof(Audio_VoiceScheduler), 0x30},
    FieldPin{offsetof(Audio_DriverFlags, updateFlags), 0x08},
    FieldPin{sizeof(Audio_DriverFlags), 0x0C},
};
static_assert(PinsMatch(kSchedulerFields, {0, 0}));

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

// Copies between a typed field and the legacy field it mirrors, which keep
// the same width and signedness.
template <typename To, typename From>
void Transfer(To& to, const From& from) {
  static_assert(std::is_integral_v<To> && std::is_integral_v<From>);
  static_assert(sizeof(To) == sizeof(From));
  static_assert(std::is_signed_v<To> == std::is_signed_v<From>);
  to = static_cast<To>(from);
}

struct LoadField {
  template <typename Typed, typename Legacy>
  void operator()(Typed& typed, const Legacy& legacy) const {
    Transfer(typed, legacy);
  }
};

struct StoreField {
  template <typename Typed, typename Legacy>
  void operator()(const Typed& typed, Legacy& legacy) const {
    Transfer(legacy, typed);
  }
};

template <typename Visit>
void VisitLevelFields(ChannelState& typed, Audio_SeqChannel& legacy,
                      Visit visit) {
  visit(typed.update_flags, legacy.updateFlags);
  visit(typed.volume, legacy.volume);
  visit(typed.volume_slide_step, legacy.volumeSlideStep);
  visit(typed.fine_pitch_delta, legacy.finePitchDelta);
  visit(typed.key_on_volume, legacy.keyOnVolume);
  visit(typed.key_on_volume_slide_step, legacy.keyOnVolumeSlideStep);
  visit(typed.note_length, legacy.noteLength);
  visit(typed.gate_length, legacy.gateLength);
  visit(typed.instrument_index, legacy.instrumentIndex);
  visit(typed.volume_balance, legacy.volumeBalance);
  visit(typed.volume_balance_slide_length, legacy.volumeBalanceSlideLength);
  visit(typed.volume_slide_length, legacy.volumeSlideLength);
  visit(typed.key_on_volume_slide_length, legacy.keyOnVolumeSlideLength);
  visit(typed.effect_volume_slide_length, legacy.effectVolumeSlideLength);
  visit(typed.pan, legacy.pan);
  visit(typed.pan_slide_length, legacy.panSlideLength);
  visit(typed.octave, legacy.octave);
  visit(typed.pitch_bend_slide_length, legacy.pitchBendSlideLength);
  visit(typed.portamento_key, legacy.portamentoKey);
  visit(typed.portamento_steps, legacy.portamentoSteps);
  visit(typed.play_modes, legacy.playModes);
  visit(typed.stored_note_length, legacy.storedNoteLength);
  visit(typed.fixed_note_length, legacy.fixedNoteLength);
  visit(typed.volume_balance_slide_step, legacy.volumeBalanceSlideStep);
  visit(typed.effect_volume, legacy.effectVolume);
  visit(typed.effect_volume_slide_step, legacy.effectVolumeSlideStep);
  visit(typed.pan_slide_step, legacy.panSlideStep);
  visit(typed.transpose, legacy.transpose);
  visit(typed.fine_tune, legacy.fineTune);
  visit(typed.key, legacy.key);
  visit(typed.pitch_bend_target, legacy.pitchBendTarget);
  visit(typed.portamento_transpose, legacy.portamentoTranspose);
}

template <typename Visit>
void VisitOscillatorFields(ChannelState& typed, Audio_SeqChannel& legacy,
                           Visit visit) {
  visit(typed.vibrato_delay, legacy.vibratoDelay);
  visit(typed.vibrato_delay_left, legacy.vibratoDelayLeft);
  visit(typed.vibrato_rate, legacy.vibratoRate);
  visit(typed.vibrato_rate_counter, legacy.vibratoRateCounter);
  visit(typed.vibrato_shape, legacy.vibratoShape);
  visit(typed.vibrato_base, legacy.vibratoBase);
  visit(typed.vibrato_depth, legacy.vibratoDepth);
  visit(typed.vibrato_depth_slide_length, legacy.vibratoDepthSlideLength);
  visit(typed.vibrato_depth_slide_step, legacy.vibratoDepthSlideStep);
  visit(typed.vibrato_pitch, legacy.vibratoPitch);
  visit(typed.tremolo_delay, legacy.tremoloDelay);
  visit(typed.tremolo_delay_left, legacy.tremoloDelayLeft);
  visit(typed.tremolo_rate, legacy.tremoloRate);
  visit(typed.tremolo_rate_counter, legacy.tremoloRateCounter);
  visit(typed.tremolo_shape, legacy.tremoloShape);
  visit(typed.tremolo_depth, legacy.tremoloDepth);
  visit(typed.tremolo_depth_slide_length, legacy.tremoloDepthSlideLength);
  visit(typed.tremolo_depth_slide_step, legacy.tremoloDepthSlideStep);
  visit(typed.tremolo_volume, legacy.tremoloVolume);
  visit(typed.auto_pan_rate, legacy.autoPanRate);
  visit(typed.auto_pan_rate_counter, legacy.autoPanRateCounter);
  visit(typed.auto_pan_shape, legacy.autoPanShape);
  visit(typed.auto_pan_depth, legacy.autoPanDepth);
  visit(typed.auto_pan_depth_slide_length, legacy.autoPanDepthSlideLength);
  visit(typed.auto_pan_depth_slide_step, legacy.autoPanDepthSlideStep);
  visit(typed.auto_pan_offset, legacy.autoPanOffset);
  visit(typed.noise_timer, legacy.noiseTimer);
  visit(typed.fm_timer, legacy.fmTimer);
  visit(typed.random_pitch_depth, legacy.randomPitchDepth);
}

template <typename Visit>
void VisitVoiceFields(VoiceState& typed, Audio_VoiceParams& legacy,
                      Visit visit) {
  visit(typed.dirty_flags, legacy.dirtyFlags);
  visit(typed.sample_start, legacy.sampleStart);
  visit(typed.loop_start, legacy.loopStart);
  visit(typed.adsr_lower, legacy.adsrLower);
  visit(typed.adsr_upper, legacy.adsrUpper);
  visit(typed.volume_scale, legacy.volumeScale);
}

ChannelState Load(Audio_SeqChannel& legacy) {
  ChannelState state;
  state.kind = legacy.type == SOUND_CHANNEL_TYPE_MUSIC
                   ? ChannelKind::Music
                   : ChannelKind::SoundEffect;
  Transfer(state.pitch_base, legacy.pitchBase);
  Transfer(state.effect_voice_mask, legacy.effectVoiceMask);
  VisitLevelFields(state, legacy, LoadField{});
  VisitOscillatorFields(state, legacy, LoadField{});
  VisitVoiceFields(state.voice, legacy.voice, LoadField{});
  return state;
}

std::int16_t* Waveform(std::uint8_t shape) {
  return Audio_Seq_kLfoWaveforms[shape];
}

void Store(Audio_SeqChannel& legacy, ChannelState& state) {
  VisitLevelFields(state, legacy, StoreField{});
  VisitOscillatorFields(state, legacy, StoreField{});
  VisitVoiceFields(state.voice, legacy.voice, StoreField{});
  if (state.vibrato_wave) legacy.vibratoWave = Waveform(*state.vibrato_wave);
  if (state.tremolo_wave) legacy.tremoloWave = Waveform(*state.tremolo_wave);
  if (state.auto_pan_wave) {
    legacy.autoPanWave = Waveform(*state.auto_pan_wave);
  }
  if (state.keymap_offset) {
    legacy.keymap =
        reinterpret_cast<char*>(Audio_Seq_activeMusicConfig->patchTable) +
        *state.keymap_offset;
  }
}

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

// The bytes from the channel's program counter to the end of its music
// layer's sequence data. Sound effects, and music outside the game heap, get
// the `operand_count` bytes the original read unchecked.
Operands OperandWindow(const Audio_SeqChannel& channel,
                       std::size_t operand_count) {
  const auto* counter =
      reinterpret_cast<const std::uint8_t*>(channel.programCounter);
  if (counter == nullptr) return {};
  if (channel.type == SOUND_CHANNEL_TYPE_MUSIC) {
    if (const auto data = MusicData()) {
      if (const auto offset = OffsetIn(*data, counter, 0)) {
        return data->subspan(*offset);
      }
    }
  }
  return {counter, operand_count};
}

std::uint32_t& VoiceModeMask(ChannelKind kind, VoiceMode mode) {
  const bool music = kind == ChannelKind::Music;
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
  return kind == ChannelKind::Music ? Audio_Seq_activeMusicConfig->noiseClock
                                    : Audio_Mixer_voiceScheduler.noiseClock;
}

// The commands reach the music layer only after Run has checked that the
// channel has one (LayerUse).
class LiveVoiceCommandHost final : public VoiceCommandHost {
 public:
  std::int32_t ResolveInstrument(ChannelKind kind,
                                 std::int32_t effect_voice_mask,
                                 std::uint8_t instrument) override {
    if (kind == ChannelKind::Music) {
      return Audio_Seq_ResolveMusicInstrument(
          Audio_Seq_activeMusicConfig->statusFlags, instrument);
    }
    return Audio_Seq_ResolveSoundEffectInstrument(effect_voice_mask,
                                                  instrument);
  }

  std::optional<Samples::InstrumentRecord> Instrument(
      std::int32_t index) override {
    if (index < 0 || static_cast<std::size_t>(index) >=
                         std::size(Audio_Samples_instruments)) {
      return std::nullopt;
    }
    const Audio_Instrument& entry =
        Audio_Samples_instruments[static_cast<std::size_t>(index)];
    Samples::InstrumentRecord record;
    Transfer(record.sample_start, entry.sampleStart);
    Transfer(record.loop_start, entry.loopStart);
    Transfer(record.fine_tune, entry.fineTune);
    Transfer(record.root_note, entry.rootNote);
    Transfer(record.adsr_lower, entry.adsrLower);
    Transfer(record.adsr_upper, entry.adsrUpper);
    return record;
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

std::uint64_t refused_commands = 0;

// Reports the first refusal and then every power of two.
bool Reportable(std::uint64_t count) noexcept {
  return (count & (count - 1)) == 0;
}

void Refused(const char* command, const char* reason) {
  ++refused_commands;
  if (Reportable(refused_commands)) {
    Core_Warnf("audio",
               "sequence command %s refused: %s; the channel is unchanged "
               "(refusal %" PRIu64 ")",
               command, reason, refused_commands);
  }
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
         channel.type == SOUND_CHANNEL_TYPE_MUSIC;
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
  ChannelState state = Load(*channel);
  LiveVoiceCommandHost host;
  const CommandResult result = Invoke(command, state, operands, host,
                                      static_cast<std::uint32_t>(channel_bit));
  if (result.status != CommandStatus::kApplied) {
    Refused(info.name, RefusalReason(result.status));
  }
  Store(*channel, state);
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
  return refused_commands;
}
}  // namespace VS::Audio::Seq
