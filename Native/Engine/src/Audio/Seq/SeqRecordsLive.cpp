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

#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/PlaybackReset.hpp"
#include "Audio/Samples/SampleBanks.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/Seq/Sequencer.hpp"
#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqMusicSequence.hpp"
#include "Audio/SeqVoiceCommands.hpp"
#include "Audio/SoundState.hpp"
#include "Audio/VoiceOutput.hpp"
#include "Core/Arena.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"

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
    FieldPin{
        offsetof(VS::Audio::SeqChannel, program_counter) / kNativePointerBytes,
        0x00 / kPs1PointerBytes},
    FieldPin{offsetof(VS::Audio::SeqChannel, loop_starts) / kNativePointerBytes,
             0x04 / kPs1PointerBytes},
    FieldPin{
        offsetof(VS::Audio::SeqChannel, return_address) / kNativePointerBytes,
        0x14 / kPs1PointerBytes},
    FieldPin{offsetof(VS::Audio::SeqChannel, keymap) / kNativePointerBytes,
             0x18 / kPs1PointerBytes},
    FieldPin{
        offsetof(VS::Audio::SeqChannel, vibrato_wave) / kNativePointerBytes,
        0x1C / kPs1PointerBytes},
    FieldPin{
        offsetof(VS::Audio::SeqChannel, tremolo_wave) / kNativePointerBytes,
        0x20 / kPs1PointerBytes},
    FieldPin{
        offsetof(VS::Audio::SeqChannel, auto_pan_wave) / kNativePointerBytes,
        0x24 / kPs1PointerBytes},
    FieldPin{offsetof(VS::Audio::SeqChannel, effect_tags) / kNativePointerBytes,
             0x28 / kPs1PointerBytes},
};
static_assert(PinsMatch(kChannelPointerSlots, {0, 0}));
static_assert(offsetof(VS::Audio::SeqChannel, effect_tags) %
                  kNativePointerBytes ==
              0);
static_assert(std::size(VS::Audio::SeqChannel{}.loop_starts) ==
              kLoopDepthCount);

constexpr FieldPin kChannelTagsAnchor{
    offsetof(VS::Audio::SeqChannel, effect_tags),
    0x28,
};
constexpr std::array kChannelTagFields{
    FieldPin{offsetof(VS::Audio::SeqChannel, pitch_base), 0x2C},
    FieldPin{offsetof(VS::Audio::SeqChannel, pitch_slide), 0x30},
    FieldPin{offsetof(VS::Audio::SeqChannel, update_flags), 0x34},
    FieldPin{offsetof(VS::Audio::SeqChannel, effect_voice_mask), 0x38},
};
static_assert(PinsMatch(kChannelTagFields, kChannelTagsAnchor));
// The effect identity is pointer-wide here; the fields after it are pinned
// from the next one.
static_assert(offsetof(VS::Audio::SeqChannel, effect_id) +
                  sizeof(std::uintptr_t) ==
              offsetof(VS::Audio::SeqChannel, effect_pitch_scale));

constexpr FieldPin kChannelScaleAnchor{
    offsetof(VS::Audio::SeqChannel, effect_pitch_scale),
    0x40,
};
constexpr std::array kChannelLevelFields{
    FieldPin{offsetof(VS::Audio::SeqChannel, effect_pitch_scale_slide_step),
             0x44},
    FieldPin{offsetof(VS::Audio::SeqChannel, volume), 0x48},
    FieldPin{offsetof(VS::Audio::SeqChannel, volume_slide_step), 0x4C},
    FieldPin{offsetof(VS::Audio::SeqChannel, pitch_slide_step), 0x50},
    FieldPin{offsetof(VS::Audio::SeqChannel, fine_pitch_delta), 0x54},
    FieldPin{offsetof(VS::Audio::SeqChannel, priority), 0x58},
    FieldPin{offsetof(VS::Audio::SeqChannel, key_on_volume), 0x5C},
    FieldPin{offsetof(VS::Audio::SeqChannel, key_on_volume_slide_step), 0x60},
    FieldPin{offsetof(VS::Audio::SeqChannel, type), 0x64},
    FieldPin{offsetof(VS::Audio::SeqChannel, note_length), 0x66},
    FieldPin{offsetof(VS::Audio::SeqChannel, gate_length), 0x68},
    FieldPin{offsetof(VS::Audio::SeqChannel, instrument_index), 0x6A},
    FieldPin{offsetof(VS::Audio::SeqChannel, effect_pan), 0x6C},
    FieldPin{offsetof(VS::Audio::SeqChannel, effect_pan_slide_length), 0x6E},
    FieldPin{offsetof(VS::Audio::SeqChannel, opcode_step_counter), 0x70},
    FieldPin{offsetof(VS::Audio::SeqChannel, loop_iterations), 0x72},
    FieldPin{offsetof(VS::Audio::SeqChannel, loop_step_snapshots), 0x7A},
    FieldPin{offsetof(VS::Audio::SeqChannel, volume_balance), 0x82},
    FieldPin{offsetof(VS::Audio::SeqChannel, volume_balance_slide_length),
             0x84},
    FieldPin{offsetof(VS::Audio::SeqChannel, effect_pitch_scale_slide_length),
             0x86},
    FieldPin{offsetof(VS::Audio::SeqChannel, volume_slide_length), 0x88},
    FieldPin{offsetof(VS::Audio::SeqChannel, key_on_volume_slide_length), 0x8A},
    FieldPin{offsetof(VS::Audio::SeqChannel, effect_volume_slide_length), 0x8C},
    FieldPin{offsetof(VS::Audio::SeqChannel, pan), 0x8E},
    FieldPin{offsetof(VS::Audio::SeqChannel, pan_slide_length), 0x90},
    FieldPin{offsetof(VS::Audio::SeqChannel, pitch_slide_ticks_left), 0x92},
    FieldPin{offsetof(VS::Audio::SeqChannel, octave), 0x94},
    FieldPin{offsetof(VS::Audio::SeqChannel, pitch_bend_slide_length), 0x96},
    FieldPin{offsetof(VS::Audio::SeqChannel, portamento_key), 0x98},
    FieldPin{offsetof(VS::Audio::SeqChannel, portamento_steps), 0x9A},
    FieldPin{offsetof(VS::Audio::SeqChannel, play_modes), 0x9C},
};
static_assert(PinsMatch(kChannelLevelFields, kChannelScaleAnchor));

constexpr std::array kChannelOscillatorFields{
    FieldPin{offsetof(VS::Audio::SeqChannel, vibrato_delay), 0x9E},
    FieldPin{offsetof(VS::Audio::SeqChannel, vibrato_delay_left), 0xA0},
    FieldPin{offsetof(VS::Audio::SeqChannel, vibrato_rate), 0xA2},
    FieldPin{offsetof(VS::Audio::SeqChannel, vibrato_rate_counter), 0xA4},
    FieldPin{offsetof(VS::Audio::SeqChannel, vibrato_shape), 0xA6},
    FieldPin{offsetof(VS::Audio::SeqChannel, vibrato_base), 0xA8},
    FieldPin{offsetof(VS::Audio::SeqChannel, vibrato_depth), 0xAA},
    FieldPin{offsetof(VS::Audio::SeqChannel, vibrato_depth_slide_length), 0xAC},
    FieldPin{offsetof(VS::Audio::SeqChannel, vibrato_depth_slide_step), 0xAE},
    FieldPin{offsetof(VS::Audio::SeqChannel, tremolo_delay), 0xB0},
    FieldPin{offsetof(VS::Audio::SeqChannel, tremolo_delay_left), 0xB2},
    FieldPin{offsetof(VS::Audio::SeqChannel, tremolo_rate), 0xB4},
    FieldPin{offsetof(VS::Audio::SeqChannel, tremolo_rate_counter), 0xB6},
    FieldPin{offsetof(VS::Audio::SeqChannel, tremolo_shape), 0xB8},
    FieldPin{offsetof(VS::Audio::SeqChannel, tremolo_depth), 0xBA},
    FieldPin{offsetof(VS::Audio::SeqChannel, tremolo_depth_slide_length), 0xBC},
    FieldPin{offsetof(VS::Audio::SeqChannel, tremolo_depth_slide_step), 0xBE},
    FieldPin{offsetof(VS::Audio::SeqChannel, auto_pan_rate), 0xC0},
    FieldPin{offsetof(VS::Audio::SeqChannel, auto_pan_rate_counter), 0xC2},
    FieldPin{offsetof(VS::Audio::SeqChannel, auto_pan_shape), 0xC4},
    FieldPin{offsetof(VS::Audio::SeqChannel, auto_pan_depth), 0xC6},
    FieldPin{offsetof(VS::Audio::SeqChannel, auto_pan_depth_slide_length),
             0xC8},
    FieldPin{offsetof(VS::Audio::SeqChannel, auto_pan_depth_slide_step), 0xCA},
    FieldPin{offsetof(VS::Audio::SeqChannel, noise_timer), 0xCC},
    FieldPin{offsetof(VS::Audio::SeqChannel, fm_timer), 0xCE},
    FieldPin{offsetof(VS::Audio::SeqChannel, loop_depth), 0xD0},
    FieldPin{offsetof(VS::Audio::SeqChannel, random_pitch_depth), 0xD2},
};
static_assert(PinsMatch(kChannelOscillatorFields, kChannelScaleAnchor));

constexpr std::array kChannelTailFields{
    FieldPin{offsetof(VS::Audio::SeqChannel, stored_note_length), 0xD4},
    FieldPin{offsetof(VS::Audio::SeqChannel, fixed_note_length), 0xD6},
    FieldPin{offsetof(VS::Audio::SeqChannel, volume_balance_slide_step), 0xD8},
    FieldPin{offsetof(VS::Audio::SeqChannel, effect_pan_slide_step), 0xDA},
    FieldPin{offsetof(VS::Audio::SeqChannel, effect_volume), 0xDC},
    FieldPin{offsetof(VS::Audio::SeqChannel, effect_volume_slide_step), 0xDE},
    FieldPin{offsetof(VS::Audio::SeqChannel, pan_slide_step), 0xE0},
    FieldPin{offsetof(VS::Audio::SeqChannel, transpose), 0xE2},
    FieldPin{offsetof(VS::Audio::SeqChannel, fine_tune), 0xE4},
    FieldPin{offsetof(VS::Audio::SeqChannel, key), 0xE6},
    FieldPin{offsetof(VS::Audio::SeqChannel, pitch_bend_target), 0xE8},
    FieldPin{offsetof(VS::Audio::SeqChannel, portamento_transpose), 0xEA},
    FieldPin{offsetof(VS::Audio::SeqChannel, vibrato_pitch), 0xEC},
    FieldPin{offsetof(VS::Audio::SeqChannel, tremolo_volume), 0xEE},
    FieldPin{offsetof(VS::Audio::SeqChannel, auto_pan_offset), 0xF0},
    FieldPin{offsetof(VS::Audio::SeqChannel, voice), 0xF4},
    FieldPin{sizeof(VS::Audio::SeqChannel), 0x110},
};
static_assert(PinsMatch(kChannelTailFields, kChannelScaleAnchor));

// FSoundVoiceParams (src/include/sound.h:316).
constexpr std::array kVoiceFields{
    FieldPin{offsetof(VS::Audio::VoiceParams, assigned_voice), 0x00},
    FieldPin{offsetof(VS::Audio::VoiceParams, dirty_flags), 0x04},
    FieldPin{offsetof(VS::Audio::VoiceParams, sample_start), 0x08},
    FieldPin{offsetof(VS::Audio::VoiceParams, loop_start), 0x0C},
    FieldPin{offsetof(VS::Audio::VoiceParams, sample_rate), 0x10},
    FieldPin{offsetof(VS::Audio::VoiceParams, adsr_lower), 0x12},
    FieldPin{offsetof(VS::Audio::VoiceParams, adsr_upper), 0x14},
    FieldPin{offsetof(VS::Audio::VoiceParams, volume_scale), 0x16},
    FieldPin{sizeof(VS::Audio::VoiceParams), 0x1C},
};
static_assert(PinsMatch(kVoiceFields, {0, 0}));

// FSoundInstrumentInfo, the instrument table entries.
constexpr std::array kInstrumentFields{
    FieldPin{offsetof(VS::Audio::Instrument, sample_start), 0x00},
    FieldPin{offsetof(VS::Audio::Instrument, loop_start), 0x04},
    FieldPin{offsetof(VS::Audio::Instrument, fine_tune), 0x08},
    FieldPin{offsetof(VS::Audio::Instrument, root_note), 0x0A},
    FieldPin{offsetof(VS::Audio::Instrument, adsr_lower), 0x0C},
    FieldPin{offsetof(VS::Audio::Instrument, adsr_upper), 0x0E},
    FieldPin{sizeof(VS::Audio::Instrument), 0x10},
};
static_assert(PinsMatch(kInstrumentFields, {0, 0}));

// FSoundChannelConfig (src/include/sound.h:492): the masks and tempo before
// the sequence pointers, then the fields after them from voiceSearchStart.
constexpr std::array kMusicLayerFields{
    FieldPin{offsetof(VS::Audio::MusicConfig, status_flags), 0x00},
    FieldPin{offsetof(VS::Audio::MusicConfig, active_channels), 0x04},
    FieldPin{offsetof(VS::Audio::MusicConfig, keyed_voices), 0x08},
    FieldPin{offsetof(VS::Audio::MusicConfig, allocated_voices), 0x0C},
    FieldPin{offsetof(VS::Audio::MusicConfig, pending_key_on), 0x10},
    FieldPin{offsetof(VS::Audio::MusicConfig, active_notes), 0x14},
    FieldPin{offsetof(VS::Audio::MusicConfig, pending_key_off), 0x18},
    FieldPin{offsetof(VS::Audio::MusicConfig, paused_channels), 0x1C},
    FieldPin{offsetof(VS::Audio::MusicConfig, tempo), 0x20},
    FieldPin{offsetof(VS::Audio::MusicConfig, tempo_slide_step), 0x24},
    FieldPin{offsetof(VS::Audio::MusicConfig, tick_accumulator), 0x28},
    FieldPin{offsetof(VS::Audio::MusicConfig, resume_key_on_mask), 0x2C},
    FieldPin{offsetof(VS::Audio::MusicConfig, push_release_mask), 0x30},
    FieldPin{offsetof(VS::Audio::MusicConfig, reserved34), 0x34},
    FieldPin{offsetof(VS::Audio::MusicConfig, sequence_base), 0x38},
};
static_assert(PinsMatch(kMusicLayerFields, {0, 0}));
static_assert(offsetof(VS::Audio::MusicConfig, patch_table) ==
              offsetof(VS::Audio::MusicConfig, sequence_base) +
                  sizeof(std::uintptr_t));
static_assert(offsetof(VS::Audio::MusicConfig, keymap_table) ==
              offsetof(VS::Audio::MusicConfig, patch_table) +
                  kNativePointerBytes);
static_assert(offsetof(VS::Audio::MusicConfig, note_remap_table) ==
              offsetof(VS::Audio::MusicConfig, keymap_table) +
                  kNativePointerBytes);
static_assert(offsetof(VS::Audio::MusicConfig, voice_search_start) ==
              offsetof(VS::Audio::MusicConfig, note_remap_table) +
                  kNativePointerBytes);
constexpr FieldPin kMusicLayerModeAnchor{
    offsetof(VS::Audio::MusicConfig, voice_search_start),
    0x48,
};
constexpr std::array kMusicLayerModeFields{
    FieldPin{offsetof(VS::Audio::MusicConfig, noise_channels), 0x4C},
    FieldPin{offsetof(VS::Audio::MusicConfig, reverb_channels), 0x50},
    FieldPin{offsetof(VS::Audio::MusicConfig, fm_channels), 0x54},
    FieldPin{offsetof(VS::Audio::MusicConfig, reverb_depth), 0x58},
    FieldPin{offsetof(VS::Audio::MusicConfig, reverb_depth_slide_step), 0x5C},
    FieldPin{offsetof(VS::Audio::MusicConfig, master_volume), 0x60},
    FieldPin{offsetof(VS::Audio::MusicConfig, master_volume_step), 0x64},
    FieldPin{offsetof(VS::Audio::MusicConfig, master_volume_slide_length),
             0x68},
    FieldPin{offsetof(VS::Audio::MusicConfig, reverb_depth_slide_length), 0x6A},
    FieldPin{offsetof(VS::Audio::MusicConfig, tempo_slide_length), 0x6C},
    FieldPin{offsetof(VS::Audio::MusicConfig, music_id), 0x6E},
    FieldPin{offsetof(VS::Audio::MusicConfig, jump_threshold), 0x70},
    FieldPin{offsetof(VS::Audio::MusicConfig, noise_clock), 0x72},
    FieldPin{offsetof(VS::Audio::MusicConfig, timer_upper), 0x74},
    FieldPin{offsetof(VS::Audio::MusicConfig, timer_upper_count), 0x76},
    FieldPin{offsetof(VS::Audio::MusicConfig, timer_lower), 0x78},
    FieldPin{offsetof(VS::Audio::MusicConfig, timer_lower_count), 0x7A},
    FieldPin{offsetof(VS::Audio::MusicConfig, timer_top), 0x7C},
    FieldPin{offsetof(VS::Audio::MusicConfig, reserved80), 0x80},
    FieldPin{sizeof(VS::Audio::MusicConfig), 0xA0},
};
static_assert(PinsMatch(kMusicLayerModeFields, kMusicLayerModeAnchor));

// FSoundVoiceSchedulerState (src/include/sound.h:274) and the driver flags.
constexpr std::array kSchedulerFields{
    FieldPin{offsetof(VS::Audio::VoiceScheduler, active_voices), 0x00},
    FieldPin{offsetof(VS::Audio::VoiceScheduler, pending_key_on), 0x04},
    FieldPin{offsetof(VS::Audio::VoiceScheduler, keyed_voices), 0x08},
    FieldPin{offsetof(VS::Audio::VoiceScheduler, pending_key_off), 0x0C},
    FieldPin{offsetof(VS::Audio::VoiceScheduler, paused_effect_voices), 0x10},
    FieldPin{offsetof(VS::Audio::VoiceScheduler, effect_tempo), 0x14},
    FieldPin{offsetof(VS::Audio::VoiceScheduler, tempo_accumulator), 0x18},
    FieldPin{offsetof(VS::Audio::VoiceScheduler, noise_voices), 0x1C},
    FieldPin{offsetof(VS::Audio::VoiceScheduler, reverb_voices), 0x20},
    FieldPin{offsetof(VS::Audio::VoiceScheduler, fm_voices), 0x24},
    FieldPin{offsetof(VS::Audio::VoiceScheduler, noise_clock), 0x28},
    FieldPin{sizeof(VS::Audio::VoiceScheduler), 0x30},
    FieldPin{offsetof(VS::Audio::DriverFlags, control_latches), 0x00},
    FieldPin{offsetof(VS::Audio::DriverFlags, mix_behavior), 0x04},
    FieldPin{offsetof(VS::Audio::DriverFlags, update_flags), 0x08},
    FieldPin{sizeof(VS::Audio::DriverFlags), 0x0C},
};
static_assert(PinsMatch(kSchedulerFields, {0, 0}));

// Every byte selector names a waveform slot and every instrument index the
// table check accepts names an entry.
static_assert(std::size(VS::Audio::Seq::kLfoWaveforms) ==
              std::size_t{std::numeric_limits<std::uint8_t>::max()} + 1);
static_assert(std::size(VS::Audio::Samples::instruments) ==
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
  visit(typed.update_flags, legacy.update_flags);
  visit(typed.volume, legacy.volume);
  visit(typed.volume_slide_step, legacy.volume_slide_step);
  visit(typed.fine_pitch_delta, legacy.fine_pitch_delta);
  visit(typed.key_on_volume, legacy.key_on_volume);
  visit(typed.key_on_volume_slide_step, legacy.key_on_volume_slide_step);
  visit(typed.note_length, legacy.note_length);
  visit(typed.gate_length, legacy.gate_length);
  visit(typed.instrument_index, legacy.instrument_index);
  visit(typed.volume_balance, legacy.volume_balance);
  visit(typed.volume_balance_slide_length, legacy.volume_balance_slide_length);
  visit(typed.volume_slide_length, legacy.volume_slide_length);
  visit(typed.key_on_volume_slide_length, legacy.key_on_volume_slide_length);
  visit(typed.effect_volume_slide_length, legacy.effect_volume_slide_length);
  visit(typed.pan, legacy.pan);
  visit(typed.pan_slide_length, legacy.pan_slide_length);
  visit(typed.octave, legacy.octave);
  visit(typed.pitch_bend_slide_length, legacy.pitch_bend_slide_length);
  visit(typed.portamento_key, legacy.portamento_key);
  visit(typed.portamento_steps, legacy.portamento_steps);
  visit(typed.play_modes, legacy.play_modes);
  visit(typed.stored_note_length, legacy.stored_note_length);
  visit(typed.fixed_note_length, legacy.fixed_note_length);
  visit(typed.volume_balance_slide_step, legacy.volume_balance_slide_step);
  visit(typed.effect_volume, legacy.effect_volume);
  visit(typed.effect_volume_slide_step, legacy.effect_volume_slide_step);
  visit(typed.pan_slide_step, legacy.pan_slide_step);
  visit(typed.transpose, legacy.transpose);
  visit(typed.fine_tune, legacy.fine_tune);
  visit(typed.key, legacy.key);
  visit(typed.pitch_bend_target, legacy.pitch_bend_target);
  visit(typed.portamento_transpose, legacy.portamento_transpose);
}

template <typename Typed, typename Legacy, typename Visit>
void VisitOscillatorFields(Typed& typed, Legacy& legacy, Visit visit) {
  visit(typed.vibrato_delay, legacy.vibrato_delay);
  visit(typed.vibrato_delay_left, legacy.vibrato_delay_left);
  visit(typed.vibrato_rate, legacy.vibrato_rate);
  visit(typed.vibrato_rate_counter, legacy.vibrato_rate_counter);
  visit(typed.vibrato_shape, legacy.vibrato_shape);
  visit(typed.vibrato_base, legacy.vibrato_base);
  visit(typed.vibrato_depth, legacy.vibrato_depth);
  visit(typed.vibrato_depth_slide_length, legacy.vibrato_depth_slide_length);
  visit(typed.vibrato_depth_slide_step, legacy.vibrato_depth_slide_step);
  visit(typed.vibrato_pitch, legacy.vibrato_pitch);
  visit(typed.tremolo_delay, legacy.tremolo_delay);
  visit(typed.tremolo_delay_left, legacy.tremolo_delay_left);
  visit(typed.tremolo_rate, legacy.tremolo_rate);
  visit(typed.tremolo_rate_counter, legacy.tremolo_rate_counter);
  visit(typed.tremolo_shape, legacy.tremolo_shape);
  visit(typed.tremolo_depth, legacy.tremolo_depth);
  visit(typed.tremolo_depth_slide_length, legacy.tremolo_depth_slide_length);
  visit(typed.tremolo_depth_slide_step, legacy.tremolo_depth_slide_step);
  visit(typed.tremolo_volume, legacy.tremolo_volume);
  visit(typed.auto_pan_rate, legacy.auto_pan_rate);
  visit(typed.auto_pan_rate_counter, legacy.auto_pan_rate_counter);
  visit(typed.auto_pan_shape, legacy.auto_pan_shape);
  visit(typed.auto_pan_depth, legacy.auto_pan_depth);
  visit(typed.auto_pan_depth_slide_length, legacy.auto_pan_depth_slide_length);
  visit(typed.auto_pan_depth_slide_step, legacy.auto_pan_depth_slide_step);
  visit(typed.auto_pan_offset, legacy.auto_pan_offset);
  visit(typed.noise_timer, legacy.noise_timer);
  visit(typed.fm_timer, legacy.fm_timer);
  visit(typed.random_pitch_depth, legacy.random_pitch_depth);
}

template <typename Typed, typename Legacy, typename Visit>
void VisitVoiceFields(Typed& typed, Legacy& legacy, Visit visit) {
  visit(typed.dirty_flags, legacy.dirty_flags);
  visit(typed.sample_start, legacy.sample_start);
  visit(typed.loop_start, legacy.loop_start);
  visit(typed.adsr_lower, legacy.adsr_lower);
  visit(typed.adsr_upper, legacy.adsr_upper);
  visit(typed.volume_scale, legacy.volume_scale);
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
  visit(typed.step_counter, legacy.opcode_step_counter);
  for (std::size_t depth = 0; depth < kLoopDepthCount; ++depth) {
    visit(typed.loop_iterations[depth], legacy.loop_iterations[depth]);
    visit(typed.loop_step_snapshots[depth], legacy.loop_step_snapshots[depth]);
  }
  visit(typed.loop_depth, legacy.loop_depth);
}

template <typename Typed, typename Legacy, typename Visit>
void VisitPitchSlideFields(Typed& typed, Legacy& legacy, Visit visit) {
  visit(typed.slide, legacy.pitch_slide);
  visit(typed.step, legacy.pitch_slide_step);
  visit(typed.ticks_left, legacy.pitch_slide_ticks_left);
}

template <typename Typed, typename Legacy, typename Visit>
void VisitLayerMasks(Typed& typed, Legacy& legacy, Visit visit) {
  visit(typed.status_flags, legacy.status_flags);
  visit(typed.active_channels, legacy.active_channels);
  visit(typed.keyed_voices, legacy.keyed_voices);
  visit(typed.allocated_voices, legacy.allocated_voices);
  visit(typed.pending_key_on, legacy.pending_key_on);
  visit(typed.active_notes, legacy.active_notes);
  visit(typed.pending_key_off, legacy.pending_key_off);
  visit(typed.paused_channels, legacy.paused_channels);
  visit(typed.tempo, legacy.tempo);
  visit(typed.tempo_slide_step, legacy.tempo_slide_step);
  visit(typed.tick_accumulator, legacy.tick_accumulator);
  visit(typed.resume_key_on_mask, legacy.resume_key_on_mask);
  visit(typed.push_release_mask, legacy.push_release_mask);
  visit(typed.sequence_control_value, legacy.reserved34);
  visit(typed.voice_search_start, legacy.voice_search_start);
  visit(typed.noise_channels, legacy.noise_channels);
  visit(typed.reverb_channels, legacy.reverb_channels);
  visit(typed.fm_channels, legacy.fm_channels);
}

template <typename Typed, typename Legacy, typename Visit>
void VisitLayerLevels(Typed& typed, Legacy& legacy, Visit visit) {
  visit(typed.reverb_depth, legacy.reverb_depth);
  visit(typed.reverb_depth_slide_step, legacy.reverb_depth_slide_step);
  visit(typed.master_volume, legacy.master_volume);
  visit(typed.master_volume_step, legacy.master_volume_step);
  visit(typed.master_volume_slide_length, legacy.master_volume_slide_length);
  visit(typed.reverb_depth_slide_length, legacy.reverb_depth_slide_length);
  visit(typed.tempo_slide_length, legacy.tempo_slide_length);
  visit(typed.music_id, legacy.music_id);
  visit(typed.jump_threshold, legacy.jump_threshold);
  visit(typed.noise_clock, legacy.noise_clock);
  visit(typed.timer_upper, legacy.timer_upper);
  visit(typed.timer_upper_count, legacy.timer_upper_count);
  visit(typed.timer_lower, legacy.timer_lower);
  visit(typed.timer_lower_count, legacy.timer_lower_count);
  visit(typed.timer_top, legacy.timer_top);
}

template <typename Typed, typename Legacy, typename Visit>
void VisitSchedulerFields(Typed& typed, Legacy& legacy, Visit visit) {
  visit(typed.active_voices, legacy.active_voices);
  visit(typed.pending_key_on, legacy.pending_key_on);
  visit(typed.keyed_voices, legacy.keyed_voices);
  visit(typed.pending_key_off, legacy.pending_key_off);
  visit(typed.paused_effect_voices, legacy.paused_effect_voices);
  visit(typed.effect_tempo, legacy.effect_tempo);
  visit(typed.tempo_accumulator, legacy.tempo_accumulator);
  visit(typed.noise_voices, legacy.noise_voices);
  visit(typed.reverb_voices, legacy.reverb_voices);
  visit(typed.fm_voices, legacy.fm_voices);
  visit(typed.noise_clock, legacy.noise_clock);
}
std::int16_t* Waveform(std::uint8_t shape) {
  return VS::Audio::Seq::kLfoWaveforms[shape];
}

// The note remap table `table` names as an offset into the remap tables.
std::optional<std::uint16_t> NoteRemapOffset(
    const VS::Audio::KeymapEntry* table) {
  const auto address = reinterpret_cast<std::uintptr_t>(table);
  const auto start = reinterpret_cast<std::uintptr_t>(
      std::data(VS::Audio::Seq::kNoteRemapTables));
  if (table == nullptr || address < start ||
      address - start >= sizeof(VS::Audio::Seq::kNoteRemapTables)) {
    return std::nullopt;
  }
  return static_cast<std::uint16_t>(address - start);
}

// The block of `size` bytes at `base` when `address` lies in it.
std::optional<LiveProgramMemory> Containing(std::uint8_t* address,
                                            std::uint8_t* base,
                                            std::size_t size) {
  const std::less<const std::uint8_t*> before;
  if (before(address, base) || !before(address, base + size)) {
    return std::nullopt;
  }
  return LiveProgramMemory{base, size};
}

// The offset of `pointer` in `memory`; its end is a position too.
std::uint32_t OffsetOf(const std::uint8_t* pointer,
                       const LiveProgramMemory& memory) {
  const std::less<const std::uint8_t*> before;
  if (pointer == nullptr || memory.base == nullptr ||
      before(pointer, memory.base) ||
      before(memory.base + memory.size, pointer)) {
    return kOutsideProgram;
  }
  return static_cast<std::uint32_t>(pointer - memory.base);
}

void StorePosition(std::uint8_t*& pointer, std::uint32_t offset,
                   const LiveProgramMemory& memory) {
  if (offset == kOutsideProgram || memory.base == nullptr ||
      offset > memory.size) {
    return;
  }
  pointer = memory.base + offset;
}

std::uint64_t refusals = 0;
const VS::Audio::SeqChannel* scoped_channel = nullptr;
LiveProgramMemory scoped_memory;
}  // namespace

ProgramData LiveProgramMemory::View() const {
  if (base == nullptr) return {};
  return {base, size};
}

LiveProgramMemory LiveProgramMemory::From(std::uint8_t* start) const {
  const std::uint32_t offset = OffsetOf(start, *this);
  if (offset == kOutsideProgram) return {};
  return {start, size - offset};
}

Driver::ChannelKind LiveChannelKind(const VS::Audio::SeqChannel& legacy) {
  return legacy.type == kMusicChannelType ? Driver::ChannelKind::kMusic
                                          : Driver::ChannelKind::kSoundEffect;
}

ChannelState LoadChannelState(const VS::Audio::SeqChannel& legacy) {
  ChannelState state;
  state.kind = LiveChannelKind(legacy);
  Transfer(state.pitch_base, legacy.pitch_base);
  Transfer(state.effect_voice_mask, legacy.effect_voice_mask);
  VisitChannelFields(state, legacy, LoadField{});
  return state;
}

void StoreChannelState(VS::Audio::SeqChannel& legacy,
                       const ChannelState& state) {
  Transfer(legacy.pitch_base, state.pitch_base);
  VisitChannelFields(state, legacy, StoreField{});
  if (state.vibrato_wave) legacy.vibrato_wave = Waveform(*state.vibrato_wave);
  if (state.tremolo_wave) legacy.tremolo_wave = Waveform(*state.tremolo_wave);
  if (state.auto_pan_wave) {
    legacy.auto_pan_wave = Waveform(*state.auto_pan_wave);
  }
  const VS::Audio::MusicConfig* layer = VS::Audio::Seq::activeMusicConfig;
  if (state.keymap_offset && layer != nullptr &&
      layer->patch_table != nullptr) {
    legacy.keymap = reinterpret_cast<std::uint8_t*>(layer->patch_table) +
                    *state.keymap_offset;
  }
}

MusicLayerState LoadMusicLayer(const VS::Audio::MusicConfig& legacy) {
  MusicLayerState state;
  VisitLayerMasks(state, legacy, LoadField{});
  VisitLayerLevels(state, legacy, LoadField{});
  state.note_remap_offset = NoteRemapOffset(legacy.note_remap_table);
  return state;
}

void StoreMusicLayer(VS::Audio::MusicConfig& legacy,
                     const MusicLayerState& state) {
  VisitLayerMasks(state, legacy, StoreField{});
  VisitLayerLevels(state, legacy, StoreField{});
  if (state.note_remap_offset &&
      *state.note_remap_offset < sizeof(VS::Audio::Seq::kNoteRemapTables)) {
    legacy.note_remap_table = reinterpret_cast<VS::Audio::KeymapEntry*>(
        std::data(VS::Audio::Seq::kNoteRemapTables) + *state.note_remap_offset);
  }
}

VoiceSchedulerState LoadVoiceScheduler(
    const VS::Audio::VoiceScheduler& legacy) {
  VoiceSchedulerState state;
  VisitSchedulerFields(state, legacy, LoadField{});
  return state;
}

void StoreVoiceScheduler(VS::Audio::VoiceScheduler& legacy,
                         const VoiceSchedulerState& state) {
  VisitSchedulerFields(state, legacy, StoreField{});
}

std::optional<Samples::InstrumentRecord> LiveInstrument(std::int32_t index) {
  if (index < 0 || static_cast<std::size_t>(index) >=
                       std::size(VS::Audio::Samples::instruments)) {
    return std::nullopt;
  }
  const VS::Audio::Instrument& entry =
      VS::Audio::Samples::instruments[static_cast<std::size_t>(index)];
  Samples::InstrumentRecord record;
  Transfer(record.sample_start, entry.sample_start);
  Transfer(record.loop_start, entry.loop_start);
  Transfer(record.fine_tune, entry.fine_tune);
  Transfer(record.root_note, entry.root_note);
  Transfer(record.adsr_lower, entry.adsr_lower);
  Transfer(record.adsr_upper, entry.adsr_upper);
  return record;
}

std::optional<LiveProgramMemory> LiveProgramRegion(std::uint8_t* address) {
  if (address == nullptr) return std::nullopt;
  if (const auto payload = Core::Memory::LivePayloadContaining(address)) {
    const auto offset = static_cast<std::size_t>(
        reinterpret_cast<const std::byte*>(address) - payload->data());
    return LiveProgramMemory{address - offset, payload->size()};
  }
  if (auto menu = Containing(address,
                             reinterpret_cast<std::uint8_t*>(
                                 std::data(VS::Audio::menuSoundEffects)),
                             sizeof(VS::Audio::menuSoundEffects))) {
    return menu;
  }
  if (auto idle = Containing(address, LiveIdleProgram(),
                             sizeof(VS::Audio::Seq::kIdleProgram))) {
    return idle;
  }
  return Containing(address,
                    reinterpret_cast<std::uint8_t*>(Core::overlay_arena.data()),
                    Core::overlay_arena.size());
}

std::uint8_t* LiveIdleProgram() {
  return reinterpret_cast<std::uint8_t*>(&VS::Audio::Seq::kIdleProgram);
}

ScopedChannelProgram::ScopedChannelProgram(const VS::Audio::SeqChannel& channel,
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
    const VS::Audio::SeqChannel& channel) {
  if (&channel == scoped_channel && scoped_memory.base != nullptr &&
      OffsetOf(channel.program_counter, scoped_memory) != kOutsideProgram) {
    return scoped_memory;
  }
  std::uint8_t* const counter = channel.program_counter;
  if (auto region = LiveProgramRegion(counter)) return region;
  if (counter == nullptr) return std::nullopt;
  // A program that ran to the end of its memory still names that memory.
  const auto previous = LiveProgramRegion(counter - 1);
  if (previous && previous->base + previous->size == counter) return previous;
  return std::nullopt;
}

ProgramState LoadProgram(const VS::Audio::SeqChannel& legacy,
                         const LiveProgramMemory& memory) {
  ProgramState program;
  program.counter = OffsetOf(legacy.program_counter, memory);
  for (std::size_t depth = 0; depth < kLoopDepthCount; ++depth) {
    program.loop_starts[depth] = OffsetOf(legacy.loop_starts[depth], memory);
  }
  program.return_address = OffsetOf(legacy.return_address, memory);
  VisitProgramFields(program, legacy, LoadField{});
  return program;
}

void StoreProgram(VS::Audio::SeqChannel& legacy, const ProgramState& program,
                  const LiveProgramMemory& memory) {
  StorePosition(legacy.program_counter, program.counter, memory);
  for (std::size_t depth = 0; depth < kLoopDepthCount; ++depth) {
    StorePosition(legacy.loop_starts[depth], program.loop_starts[depth],
                  memory);
  }
  StorePosition(legacy.return_address, program.return_address, memory);
  VisitProgramFields(program, legacy, StoreField{});
}

SequenceChannel LoadSequenceChannel(const VS::Audio::SeqChannel& legacy,
                                    const LiveProgramMemory& memory) {
  SequenceChannel record;
  record.channel = LoadChannelState(legacy);
  record.program = LoadProgram(legacy, memory);
  VisitPitchSlideFields(record.pitch_slide, legacy, LoadField{});
  Transfer(record.assigned_voice, legacy.voice.assigned_voice);
  record.idle = legacy.program_counter == LiveIdleProgram();
  return record;
}

void StoreSequenceChannel(VS::Audio::SeqChannel& legacy,
                          const SequenceChannel& record,
                          const LiveProgramMemory& memory) {
  StoreChannelState(legacy, record.channel);
  StoreProgram(legacy, record.program, memory);
  if (record.idle) legacy.program_counter = LiveIdleProgram();
  VisitPitchSlideFields(record.pitch_slide, legacy, StoreField{});
  Transfer(legacy.voice.assigned_voice, record.assigned_voice);
}

std::uint64_t LiveRefusalCount() noexcept { return refusals; }

void ReportRefusal(const char* operation, const char* reason) {
  ++refusals;
  if ((refusals & (refusals - 1)) != 0) return;
  VS::Core::Warnf("audio", "sequencer %s refused: %s (refusal %" PRIu64 ")",
                  operation, reason, refusals);
}
}  // namespace VS::Audio::Seq
