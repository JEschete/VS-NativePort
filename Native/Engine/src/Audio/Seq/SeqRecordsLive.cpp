// Hand-maintained (tools/port/owned.csv).
// Live binding for SeqRecordsLive.hpp: FSoundChannel, FSoundChannelConfig and
// FSoundVoiceSchedulerState as the copied code lays them out, the instrument
// table and the memory sequence programs run from.
#include "Audio/SeqRecordsLive.hpp"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <type_traits>

#include "Audio/PlaybackReset.hpp"
#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqMusicSequence.hpp"
#include "Audio/SeqVoiceCommands.hpp"
#include "Audio/VoiceOutput.hpp"
#include "Core/Arena.h"
#include "Core/Diagnostics.h"
#include "Core/Memory/HeapLive.hpp"

extern "C" {
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/Sequencer.h"
#include "Audio/SoundBanks.h"
}

namespace VS::Audio::Seq {
namespace {
// FSoundChannel.type of a music channel (src/include/sound.h:42).
constexpr unsigned kMusicChannelType = 0;

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
    FieldPin{offsetof(Audio_SeqChannel, loopStarts) / kNativePointerBytes,
             0x04 / kPs1PointerBytes},
    FieldPin{offsetof(Audio_SeqChannel, returnAddress) / kNativePointerBytes,
             0x14 / kPs1PointerBytes},
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
static_assert(std::size(Audio_SeqChannel{}.loopStarts) == kLoopDepthCount);

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
// The effect identity is pointer-wide here; the fields after it are pinned
// from the next one.
static_assert(offsetof(Audio_SeqChannel, effectId) + sizeof(std::uintptr_t) ==
              offsetof(Audio_SeqChannel, effectPitchScale));

constexpr FieldPin kChannelScaleAnchor{
    offsetof(Audio_SeqChannel, effectPitchScale),
    0x40,
};
constexpr std::array kChannelLevelFields{
    FieldPin{offsetof(Audio_SeqChannel, effectPitchScaleSlideStep), 0x44},
    FieldPin{offsetof(Audio_SeqChannel, volume), 0x48},
    FieldPin{offsetof(Audio_SeqChannel, volumeSlideStep), 0x4C},
    FieldPin{offsetof(Audio_SeqChannel, pitchSlideStep), 0x50},
    FieldPin{offsetof(Audio_SeqChannel, finePitchDelta), 0x54},
    FieldPin{offsetof(Audio_SeqChannel, priority), 0x58},
    FieldPin{offsetof(Audio_SeqChannel, keyOnVolume), 0x5C},
    FieldPin{offsetof(Audio_SeqChannel, keyOnVolumeSlideStep), 0x60},
    FieldPin{offsetof(Audio_SeqChannel, type), 0x64},
    FieldPin{offsetof(Audio_SeqChannel, noteLength), 0x66},
    FieldPin{offsetof(Audio_SeqChannel, gateLength), 0x68},
    FieldPin{offsetof(Audio_SeqChannel, instrumentIndex), 0x6A},
    FieldPin{offsetof(Audio_SeqChannel, effectPan), 0x6C},
    FieldPin{offsetof(Audio_SeqChannel, effectPanSlideLength), 0x6E},
    FieldPin{offsetof(Audio_SeqChannel, opcodeStepCounter), 0x70},
    FieldPin{offsetof(Audio_SeqChannel, loopIterations), 0x72},
    FieldPin{offsetof(Audio_SeqChannel, loopStepSnapshots), 0x7A},
    FieldPin{offsetof(Audio_SeqChannel, volumeBalance), 0x82},
    FieldPin{offsetof(Audio_SeqChannel, volumeBalanceSlideLength), 0x84},
    FieldPin{offsetof(Audio_SeqChannel, effectPitchScaleSlideLength), 0x86},
    FieldPin{offsetof(Audio_SeqChannel, volumeSlideLength), 0x88},
    FieldPin{offsetof(Audio_SeqChannel, keyOnVolumeSlideLength), 0x8A},
    FieldPin{offsetof(Audio_SeqChannel, effectVolumeSlideLength), 0x8C},
    FieldPin{offsetof(Audio_SeqChannel, pan), 0x8E},
    FieldPin{offsetof(Audio_SeqChannel, panSlideLength), 0x90},
    FieldPin{offsetof(Audio_SeqChannel, pitchSlideTicksLeft), 0x92},
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
    FieldPin{offsetof(Audio_SeqChannel, loopDepth), 0xD0},
    FieldPin{offsetof(Audio_SeqChannel, randomPitchDepth), 0xD2},
};
static_assert(PinsMatch(kChannelOscillatorFields, kChannelScaleAnchor));

constexpr std::array kChannelTailFields{
    FieldPin{offsetof(Audio_SeqChannel, storedNoteLength), 0xD4},
    FieldPin{offsetof(Audio_SeqChannel, fixedNoteLength), 0xD6},
    FieldPin{offsetof(Audio_SeqChannel, volumeBalanceSlideStep), 0xD8},
    FieldPin{offsetof(Audio_SeqChannel, effectPanSlideStep), 0xDA},
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
    FieldPin{offsetof(Audio_VoiceParams, assignedVoice), 0x00},
    FieldPin{offsetof(Audio_VoiceParams, dirtyFlags), 0x04},
    FieldPin{offsetof(Audio_VoiceParams, sampleStart), 0x08},
    FieldPin{offsetof(Audio_VoiceParams, loopStart), 0x0C},
    FieldPin{offsetof(Audio_VoiceParams, sampleRate), 0x10},
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

// FSoundChannelConfig (src/include/sound.h:492): the masks and tempo before
// the sequence pointers, then the fields after them from voiceSearchStart.
constexpr std::array kMusicLayerFields{
    FieldPin{offsetof(Audio_MusicConfig, statusFlags), 0x00},
    FieldPin{offsetof(Audio_MusicConfig, activeChannels), 0x04},
    FieldPin{offsetof(Audio_MusicConfig, keyedVoices), 0x08},
    FieldPin{offsetof(Audio_MusicConfig, allocatedVoices), 0x0C},
    FieldPin{offsetof(Audio_MusicConfig, pendingKeyOn), 0x10},
    FieldPin{offsetof(Audio_MusicConfig, activeNotes), 0x14},
    FieldPin{offsetof(Audio_MusicConfig, pendingKeyOff), 0x18},
    FieldPin{offsetof(Audio_MusicConfig, pausedChannels), 0x1C},
    FieldPin{offsetof(Audio_MusicConfig, tempo), 0x20},
    FieldPin{offsetof(Audio_MusicConfig, tempoSlideStep), 0x24},
    FieldPin{offsetof(Audio_MusicConfig, tickAccumulator), 0x28},
    FieldPin{offsetof(Audio_MusicConfig, resumeKeyOnMask), 0x2C},
    FieldPin{offsetof(Audio_MusicConfig, pushReleaseMask), 0x30},
    FieldPin{offsetof(Audio_MusicConfig, reserved34), 0x34},
    FieldPin{offsetof(Audio_MusicConfig, sequenceBase), 0x38},
};
static_assert(PinsMatch(kMusicLayerFields, {0, 0}));
static_assert(offsetof(Audio_MusicConfig, patchTable) ==
              offsetof(Audio_MusicConfig, sequenceBase) +
                  sizeof(std::uintptr_t));
static_assert(offsetof(Audio_MusicConfig, keymapTable) ==
              offsetof(Audio_MusicConfig, patchTable) + kNativePointerBytes);
static_assert(offsetof(Audio_MusicConfig, noteRemapTable) ==
              offsetof(Audio_MusicConfig, keymapTable) + kNativePointerBytes);
static_assert(offsetof(Audio_MusicConfig, voiceSearchStart) ==
              offsetof(Audio_MusicConfig, noteRemapTable) +
                  kNativePointerBytes);
constexpr FieldPin kMusicLayerModeAnchor{
    offsetof(Audio_MusicConfig, voiceSearchStart),
    0x48,
};
constexpr std::array kMusicLayerModeFields{
    FieldPin{offsetof(Audio_MusicConfig, noiseChannels), 0x4C},
    FieldPin{offsetof(Audio_MusicConfig, reverbChannels), 0x50},
    FieldPin{offsetof(Audio_MusicConfig, fmChannels), 0x54},
    FieldPin{offsetof(Audio_MusicConfig, reverbDepth), 0x58},
    FieldPin{offsetof(Audio_MusicConfig, reverbDepthSlideStep), 0x5C},
    FieldPin{offsetof(Audio_MusicConfig, masterVolume), 0x60},
    FieldPin{offsetof(Audio_MusicConfig, masterVolumeStep), 0x64},
    FieldPin{offsetof(Audio_MusicConfig, masterVolumeSlideLength), 0x68},
    FieldPin{offsetof(Audio_MusicConfig, reverbDepthSlideLength), 0x6A},
    FieldPin{offsetof(Audio_MusicConfig, tempoSlideLength), 0x6C},
    FieldPin{offsetof(Audio_MusicConfig, musicId), 0x6E},
    FieldPin{offsetof(Audio_MusicConfig, jumpThreshold), 0x70},
    FieldPin{offsetof(Audio_MusicConfig, noiseClock), 0x72},
    FieldPin{offsetof(Audio_MusicConfig, timerUpper), 0x74},
    FieldPin{offsetof(Audio_MusicConfig, timerUpperCount), 0x76},
    FieldPin{offsetof(Audio_MusicConfig, timerLower), 0x78},
    FieldPin{offsetof(Audio_MusicConfig, timerLowerCount), 0x7A},
    FieldPin{offsetof(Audio_MusicConfig, timerTop), 0x7C},
    FieldPin{offsetof(Audio_MusicConfig, reserved80), 0x80},
    FieldPin{sizeof(Audio_MusicConfig), 0xA0},
};
static_assert(PinsMatch(kMusicLayerModeFields, kMusicLayerModeAnchor));

// FSoundVoiceSchedulerState (src/include/sound.h:274) and the driver flags.
constexpr std::array kSchedulerFields{
    FieldPin{offsetof(Audio_VoiceScheduler, activeVoices), 0x00},
    FieldPin{offsetof(Audio_VoiceScheduler, pendingKeyOn), 0x04},
    FieldPin{offsetof(Audio_VoiceScheduler, keyedVoices), 0x08},
    FieldPin{offsetof(Audio_VoiceScheduler, pendingKeyOff), 0x0C},
    FieldPin{offsetof(Audio_VoiceScheduler, pausedEffectVoices), 0x10},
    FieldPin{offsetof(Audio_VoiceScheduler, effectTempo), 0x14},
    FieldPin{offsetof(Audio_VoiceScheduler, tempoAccumulator), 0x18},
    FieldPin{offsetof(Audio_VoiceScheduler, noiseVoices), 0x1C},
    FieldPin{offsetof(Audio_VoiceScheduler, reverbVoices), 0x20},
    FieldPin{offsetof(Audio_VoiceScheduler, fmVoices), 0x24},
    FieldPin{offsetof(Audio_VoiceScheduler, noiseClock), 0x28},
    FieldPin{sizeof(Audio_VoiceScheduler), 0x30},
    FieldPin{offsetof(Audio_DriverFlags, controlLatches), 0x00},
    FieldPin{offsetof(Audio_DriverFlags, mixBehavior), 0x04},
    FieldPin{offsetof(Audio_DriverFlags, updateFlags), 0x08},
    FieldPin{sizeof(Audio_DriverFlags), 0x0C},
};
static_assert(PinsMatch(kSchedulerFields, {0, 0}));

// Every byte selector names a waveform slot and every instrument index the
// table check accepts names an entry.
static_assert(std::size(Audio_Seq_kLfoWaveforms) ==
              std::size_t{std::numeric_limits<std::uint8_t>::max()} + 1);
static_assert(std::size(Audio_Samples_instruments) ==
              Samples::kInstrumentSlotCount);

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
template <typename Typed, typename Legacy, typename Visit>
void VisitLevelFields(Typed& typed, Legacy& legacy, Visit visit) {
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

template <typename Typed, typename Legacy, typename Visit>
void VisitOscillatorFields(Typed& typed, Legacy& legacy, Visit visit) {
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

template <typename Typed, typename Legacy, typename Visit>
void VisitVoiceFields(Typed& typed, Legacy& legacy, Visit visit) {
  visit(typed.dirty_flags, legacy.dirtyFlags);
  visit(typed.sample_start, legacy.sampleStart);
  visit(typed.loop_start, legacy.loopStart);
  visit(typed.adsr_lower, legacy.adsrLower);
  visit(typed.adsr_upper, legacy.adsrUpper);
  visit(typed.volume_scale, legacy.volumeScale);
}

template <typename Typed, typename Legacy, typename Visit>
void VisitChannelFields(Typed& typed, Legacy& legacy, Visit visit) {
  VisitLevelFields(typed, legacy, visit);
  VisitOscillatorFields(typed, legacy, visit);
  VisitVoiceFields(typed.voice, legacy.voice, visit);
}

// The loop stack's counters; its positions are pointers (LoadProgram).
template <typename Typed, typename Legacy, typename Visit>
void VisitProgramFields(Typed& typed, Legacy& legacy, Visit visit) {
  visit(typed.step_counter, legacy.opcodeStepCounter);
  for (std::size_t depth = 0; depth < kLoopDepthCount; ++depth) {
    visit(typed.loop_iterations[depth], legacy.loopIterations[depth]);
    visit(typed.loop_step_snapshots[depth], legacy.loopStepSnapshots[depth]);
  }
  visit(typed.loop_depth, legacy.loopDepth);
}

template <typename Typed, typename Legacy, typename Visit>
void VisitPitchSlideFields(Typed& typed, Legacy& legacy, Visit visit) {
  visit(typed.slide, legacy.pitchSlide);
  visit(typed.step, legacy.pitchSlideStep);
  visit(typed.ticks_left, legacy.pitchSlideTicksLeft);
}

template <typename Typed, typename Legacy, typename Visit>
void VisitLayerMasks(Typed& typed, Legacy& legacy, Visit visit) {
  visit(typed.status_flags, legacy.statusFlags);
  visit(typed.active_channels, legacy.activeChannels);
  visit(typed.keyed_voices, legacy.keyedVoices);
  visit(typed.allocated_voices, legacy.allocatedVoices);
  visit(typed.pending_key_on, legacy.pendingKeyOn);
  visit(typed.active_notes, legacy.activeNotes);
  visit(typed.pending_key_off, legacy.pendingKeyOff);
  visit(typed.paused_channels, legacy.pausedChannels);
  visit(typed.tempo, legacy.tempo);
  visit(typed.tempo_slide_step, legacy.tempoSlideStep);
  visit(typed.tick_accumulator, legacy.tickAccumulator);
  visit(typed.resume_key_on_mask, legacy.resumeKeyOnMask);
  visit(typed.push_release_mask, legacy.pushReleaseMask);
  visit(typed.sequence_control_value, legacy.reserved34);
  visit(typed.voice_search_start, legacy.voiceSearchStart);
  visit(typed.noise_channels, legacy.noiseChannels);
  visit(typed.reverb_channels, legacy.reverbChannels);
  visit(typed.fm_channels, legacy.fmChannels);
}

template <typename Typed, typename Legacy, typename Visit>
void VisitLayerLevels(Typed& typed, Legacy& legacy, Visit visit) {
  visit(typed.reverb_depth, legacy.reverbDepth);
  visit(typed.reverb_depth_slide_step, legacy.reverbDepthSlideStep);
  visit(typed.master_volume, legacy.masterVolume);
  visit(typed.master_volume_step, legacy.masterVolumeStep);
  visit(typed.master_volume_slide_length, legacy.masterVolumeSlideLength);
  visit(typed.reverb_depth_slide_length, legacy.reverbDepthSlideLength);
  visit(typed.tempo_slide_length, legacy.tempoSlideLength);
  visit(typed.music_id, legacy.musicId);
  visit(typed.jump_threshold, legacy.jumpThreshold);
  visit(typed.noise_clock, legacy.noiseClock);
  visit(typed.timer_upper, legacy.timerUpper);
  visit(typed.timer_upper_count, legacy.timerUpperCount);
  visit(typed.timer_lower, legacy.timerLower);
  visit(typed.timer_lower_count, legacy.timerLowerCount);
  visit(typed.timer_top, legacy.timerTop);
}

template <typename Typed, typename Legacy, typename Visit>
void VisitSchedulerFields(Typed& typed, Legacy& legacy, Visit visit) {
  visit(typed.active_voices, legacy.activeVoices);
  visit(typed.pending_key_on, legacy.pendingKeyOn);
  visit(typed.keyed_voices, legacy.keyedVoices);
  visit(typed.pending_key_off, legacy.pendingKeyOff);
  visit(typed.paused_effect_voices, legacy.pausedEffectVoices);
  visit(typed.effect_tempo, legacy.effectTempo);
  visit(typed.tempo_accumulator, legacy.tempoAccumulator);
  visit(typed.noise_voices, legacy.noiseVoices);
  visit(typed.reverb_voices, legacy.reverbVoices);
  visit(typed.fm_voices, legacy.fmVoices);
  visit(typed.noise_clock, legacy.noiseClock);
}
std::int16_t* Waveform(std::uint8_t shape) {
  return Audio_Seq_kLfoWaveforms[shape];
}

// The note remap table `table` names as an offset into the remap tables.
std::optional<std::uint16_t> NoteRemapOffset(const Audio_KeymapEntry* table) {
  const auto address = reinterpret_cast<std::uintptr_t>(table);
  const auto start =
      reinterpret_cast<std::uintptr_t>(std::data(Audio_Seq_kNoteRemapTables));
  if (table == nullptr || address < start ||
      address - start >= sizeof(Audio_Seq_kNoteRemapTables)) {
    return std::nullopt;
  }
  return static_cast<std::uint16_t>(address - start);
}

// The block of `size` bytes at `base` when `address` lies in it.
std::optional<LiveProgramMemory> Containing(char* address, char* base,
                                            std::size_t size) {
  const std::less<const char*> before;
  if (before(address, base) || !before(address, base + size)) {
    return std::nullopt;
  }
  return LiveProgramMemory{base, size};
}

// The offset of `pointer` in `memory`; its end is a position too.
std::uint32_t OffsetOf(const char* pointer, const LiveProgramMemory& memory) {
  const std::less<const char*> before;
  if (pointer == nullptr || memory.base == nullptr ||
      before(pointer, memory.base) ||
      before(memory.base + memory.size, pointer)) {
    return kOutsideProgram;
  }
  return static_cast<std::uint32_t>(pointer - memory.base);
}

void StorePosition(char*& pointer, std::uint32_t offset,
                   const LiveProgramMemory& memory) {
  if (offset == kOutsideProgram || memory.base == nullptr ||
      offset > memory.size) {
    return;
  }
  pointer = memory.base + offset;
}

std::uint64_t refusals = 0;
const Audio_SeqChannel* scoped_channel = nullptr;
LiveProgramMemory scoped_memory;
}  // namespace

ProgramData LiveProgramMemory::View() const {
  if (base == nullptr) return {};
  return {reinterpret_cast<const std::uint8_t*>(base), size};
}

LiveProgramMemory LiveProgramMemory::From(char* start) const {
  const std::uint32_t offset = OffsetOf(start, *this);
  if (offset == kOutsideProgram) return {};
  return {start, size - offset};
}

Driver::ChannelKind LiveChannelKind(const Audio_SeqChannel& legacy) {
  return legacy.type == kMusicChannelType ? Driver::ChannelKind::Music
                                          : Driver::ChannelKind::SoundEffect;
}

ChannelState LoadChannelState(const Audio_SeqChannel& legacy) {
  ChannelState state;
  state.kind = LiveChannelKind(legacy);
  Transfer(state.pitch_base, legacy.pitchBase);
  Transfer(state.effect_voice_mask, legacy.effectVoiceMask);
  VisitChannelFields(state, legacy, LoadField{});
  return state;
}

void StoreChannelState(Audio_SeqChannel& legacy, const ChannelState& state) {
  Transfer(legacy.pitchBase, state.pitch_base);
  VisitChannelFields(state, legacy, StoreField{});
  if (state.vibrato_wave) legacy.vibratoWave = Waveform(*state.vibrato_wave);
  if (state.tremolo_wave) legacy.tremoloWave = Waveform(*state.tremolo_wave);
  if (state.auto_pan_wave) {
    legacy.autoPanWave = Waveform(*state.auto_pan_wave);
  }
  const Audio_MusicConfig* layer = Audio_Seq_activeMusicConfig;
  if (state.keymap_offset && layer != nullptr && layer->patchTable != nullptr) {
    legacy.keymap =
        reinterpret_cast<char*>(layer->patchTable) + *state.keymap_offset;
  }
}

MusicLayerState LoadMusicLayer(const Audio_MusicConfig& legacy) {
  MusicLayerState state;
  VisitLayerMasks(state, legacy, LoadField{});
  VisitLayerLevels(state, legacy, LoadField{});
  state.note_remap_offset = NoteRemapOffset(legacy.noteRemapTable);
  return state;
}

void StoreMusicLayer(Audio_MusicConfig& legacy, const MusicLayerState& state) {
  VisitLayerMasks(state, legacy, StoreField{});
  VisitLayerLevels(state, legacy, StoreField{});
  if (state.note_remap_offset &&
      *state.note_remap_offset < sizeof(Audio_Seq_kNoteRemapTables)) {
    legacy.noteRemapTable = reinterpret_cast<Audio_KeymapEntry*>(
        std::data(Audio_Seq_kNoteRemapTables) + *state.note_remap_offset);
  }
}

VoiceSchedulerState LoadVoiceScheduler(const Audio_VoiceScheduler& legacy) {
  VoiceSchedulerState state;
  VisitSchedulerFields(state, legacy, LoadField{});
  return state;
}

void StoreVoiceScheduler(Audio_VoiceScheduler& legacy,
                         const VoiceSchedulerState& state) {
  VisitSchedulerFields(state, legacy, StoreField{});
}

std::optional<Samples::InstrumentRecord> LiveInstrument(std::int32_t index) {
  if (index < 0 ||
      static_cast<std::size_t>(index) >= std::size(Audio_Samples_instruments)) {
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

std::optional<LiveProgramMemory> LiveProgramRegion(char* address) {
  if (address == nullptr) return std::nullopt;
  if (const auto payload = Core::Memory::LivePayloadContaining(address)) {
    const auto offset = static_cast<std::size_t>(
        reinterpret_cast<const std::byte*>(address) - payload->data());
    return LiveProgramMemory{address - offset, payload->size()};
  }
  if (auto menu = Containing(
          address, reinterpret_cast<char*>(std::data(Audio_menuSoundEffects)),
          sizeof(Audio_menuSoundEffects))) {
    return menu;
  }
  if (auto idle = Containing(address, LiveIdleProgram(),
                             sizeof(Audio_Seq_kIdleProgram))) {
    return idle;
  }
  return Containing(address,
                    reinterpret_cast<char*>(std::data(Core_overlayArena)),
                    sizeof(Core_overlayArena));
}

char* LiveIdleProgram() {
  return reinterpret_cast<char*>(&Audio_Seq_kIdleProgram);
}

ScopedChannelProgram::ScopedChannelProgram(const Audio_SeqChannel& channel,
                                           const LiveProgramMemory& memory)
    : previous_channel_(scoped_channel), previous_memory_(scoped_memory) {
  scoped_channel = &channel;
  scoped_memory = memory;
}

ScopedChannelProgram::~ScopedChannelProgram() {
  scoped_channel = previous_channel_;
  scoped_memory = previous_memory_;
}

std::optional<LiveProgramMemory> LiveChannelProgram(
    const Audio_SeqChannel& channel) {
  if (&channel == scoped_channel && scoped_memory.base != nullptr &&
      OffsetOf(channel.programCounter, scoped_memory) != kOutsideProgram) {
    return scoped_memory;
  }
  char* const counter = channel.programCounter;
  if (auto region = LiveProgramRegion(counter)) return region;
  if (counter == nullptr) return std::nullopt;
  // A program that ran to the end of its memory still names that memory.
  const auto previous = LiveProgramRegion(counter - 1);
  if (previous && previous->base + previous->size == counter) return previous;
  return std::nullopt;
}

ProgramState LoadProgram(const Audio_SeqChannel& legacy,
                         const LiveProgramMemory& memory) {
  ProgramState program;
  program.counter = OffsetOf(legacy.programCounter, memory);
  for (std::size_t depth = 0; depth < kLoopDepthCount; ++depth) {
    program.loop_starts[depth] = OffsetOf(legacy.loopStarts[depth], memory);
  }
  program.return_address = OffsetOf(legacy.returnAddress, memory);
  VisitProgramFields(program, legacy, LoadField{});
  return program;
}

void StoreProgram(Audio_SeqChannel& legacy, const ProgramState& program,
                  const LiveProgramMemory& memory) {
  StorePosition(legacy.programCounter, program.counter, memory);
  for (std::size_t depth = 0; depth < kLoopDepthCount; ++depth) {
    StorePosition(legacy.loopStarts[depth], program.loop_starts[depth], memory);
  }
  StorePosition(legacy.returnAddress, program.return_address, memory);
  VisitProgramFields(program, legacy, StoreField{});
}

SequenceChannel LoadSequenceChannel(const Audio_SeqChannel& legacy,
                                    const LiveProgramMemory& memory) {
  SequenceChannel record;
  record.channel = LoadChannelState(legacy);
  record.program = LoadProgram(legacy, memory);
  VisitPitchSlideFields(record.pitch_slide, legacy, LoadField{});
  Transfer(record.assigned_voice, legacy.voice.assignedVoice);
  record.idle = legacy.programCounter == LiveIdleProgram();
  return record;
}

void StoreSequenceChannel(Audio_SeqChannel& legacy,
                          const SequenceChannel& record,
                          const LiveProgramMemory& memory) {
  StoreChannelState(legacy, record.channel);
  StoreProgram(legacy, record.program, memory);
  if (record.idle) legacy.programCounter = LiveIdleProgram();
  VisitPitchSlideFields(record.pitch_slide, legacy, StoreField{});
  Transfer(legacy.voice.assignedVoice, record.assigned_voice);
}

std::uint64_t LiveRefusalCount() noexcept { return refusals; }

void ReportRefusal(const char* operation, const char* reason) {
  ++refusals;
  if ((refusals & (refusals - 1)) != 0) return;
  Core_Warnf("audio", "sequencer %s refused: %s (refusal %" PRIu64 ")",
             operation, reason, refusals);
}
}  // namespace VS::Audio::Seq
