// Hand-maintained (tools/port/owned.csv).
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqMusicSequence.hpp"
#include "Audio/SeqSequencer.hpp"
#include "Audio/SeqVoiceCommands.hpp"
#include "Audio/VoiceOutput.hpp"

namespace VS::Audio::Seq {
namespace {
constexpr std::uint32_t kByteBits = 8;
constexpr std::uint32_t kAllVoices = 0xFFFFFF;
constexpr std::uint32_t kVoiceAssignmentUpdate = 0x100;
constexpr std::uint32_t kRestoreUpdate = 0x90;
constexpr std::uint32_t kStreamPausesMusic = 1;
// FSoundChannelConfig.statusFlags: the sample bank the track uses.
constexpr std::int32_t kBaseBankTrack = 0x20;
constexpr std::int32_t kAlternateBankTrack = 0x40;
constexpr std::int32_t kLoadClearedStatus = 0x61;
constexpr std::int32_t kRestoreClearedStatus = 0x60;

// FAkaoSequence (src/include/sound.h:444) field offsets.
constexpr std::size_t kSampleBankIdAt = 0x14;
constexpr std::size_t kChannelEnableMaskAt = 0x20;
constexpr std::size_t kKeyedMaskAt = 0x24;
constexpr std::size_t kAllocatedVoiceMaskAt = 0x28;
constexpr std::size_t kPatchRegionOffsetAt = 0x30;
constexpr std::size_t kKeymapRegionOffsetAt = 0x34;
constexpr std::size_t kResumeKeyOnMaskAt = 0x38;
constexpr std::size_t kPushReleaseMaskAt = 0x3C;
constexpr std::size_t kChannelOffsetBytes = 2;

constexpr std::uint16_t kStartNoteLength = 4;
constexpr std::uint16_t kStartGateLength = 2;
constexpr std::uint16_t kIdleNoteLength = 3;
constexpr std::uint16_t kIdleGateLength = 1;
constexpr std::uint16_t kRestoredLengthBias = 2;
constexpr std::uint16_t kMusicVolumeBalance = 0x7F00;
constexpr std::int32_t kMusicVolume = 0x3FFF0000;
constexpr std::int16_t kMusicEffectVolume = 0x4000;
constexpr std::uint16_t kCentrePan = 0x8000;
constexpr std::uint16_t kEffectVolumeBalance = 0x6E00;
constexpr std::int32_t kEffectVolume = 0x32000000;
constexpr std::uint16_t kFreshStepCounter = 0xFFFF;
constexpr std::int32_t kStartTempo = static_cast<std::int32_t>(0xFFFF0000U);
// An idle channel releases its voice quickly at the next key-off.
constexpr std::uint32_t kIdleVoiceDirty = 0x4400;
constexpr std::uint16_t kAdsrReleaseRateMask = 0xFFE0;
constexpr std::uint16_t kIdleReleaseRate = 5;
// Every voice parameter a restored channel rewrites.
constexpr std::uint32_t kRestoredVoiceDirty = 0x1FF93;

constexpr std::int32_t kReverbFractionMask = 0xFFF;
constexpr std::int16_t kReverbSwapTicks = 0x30;

std::uint32_t U32At(std::span<const std::uint8_t> data, std::size_t at) {
  std::uint32_t value = 0;
  for (std::size_t i = 0; i < sizeof value; ++i) {
    value |= std::uint32_t{data[at + i]} << (i * kByteBits);
  }
  return value;
}

std::uint16_t U16At(std::span<const std::uint8_t> data, std::size_t at) {
  return static_cast<std::uint16_t>(data[at] | (data[at + 1] << kByteBits));
}

std::int32_t Wrap(std::int64_t value) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

// The original's signed division; its divisor is never zero here.
std::int32_t Divide(std::int32_t dividend, std::int32_t divisor) {
  return Wrap(std::int64_t{dividend} / divisor);
}

void SetBits(std::int32_t& mask, std::uint32_t bits) {
  mask = static_cast<std::int32_t>(static_cast<std::uint32_t>(mask) | bits);
}

// The status flags naming the sample bank the track's instruments use.
std::int32_t BankStatus(std::uint32_t sample_bank_id,
                        std::uint32_t alternate_bank_id) {
  return sample_bank_id == alternate_bank_id ? kAlternateBankTrack
                                             : kBaseBankTrack;
}

void MarkVoicesForKeyOff(MusicStartState& state) {
  state.scheduler.pending_key_off |=
      ~state.secondary_voices & kAllVoices &
      ~(state.scheduler.active_voices | state.stream_voices);
}

// The table a region offset names; 0 means the sequence has none.
std::optional<std::int64_t> RegionPosition(std::int32_t offset,
                                           std::size_t field_at) {
  if (offset == 0) return std::nullopt;
  return Wrap(std::int64_t{offset} + static_cast<std::int64_t>(field_at));
}

// Sound_LoadAkaoSequence's reset of one enabled channel.
void StartChannel(SequenceChannel& record, bool allocated,
                  const Samples::InstrumentRecord& first_instrument) {
  ChannelState& channel = record.channel;
  channel.note_length = kStartNoteLength;
  channel.gate_length = kStartGateLength;
  channel.volume_balance = kMusicVolumeBalance;
  channel.volume = kMusicVolume;
  channel.effect_volume = kMusicEffectVolume;
  channel.fine_tune = 0;
  channel.transpose = 0;
  channel.portamento_steps = 0;
  record.pitch_slide.slide = 0;
  channel.pitch_bend_target = 0;
  record.pitch_slide.ticks_left = 0;
  channel.fixed_note_length = 0;
  channel.stored_note_length = 0;
  channel.pan = kCentrePan;
  channel.pan_slide_length = 0;
  channel.effect_volume_slide_length = 0;
  channel.volume_slide_length = 0;
  channel.fine_pitch_delta = 0;
  channel.random_pitch_depth = 0;
  channel.key_on_volume_slide_length = 0;
  channel.play_modes = 0;
  channel.auto_pan_offset = 0;
  record.program.loop_depth = 0;
  channel.auto_pan_depth = 0;
  channel.tremolo_depth = 0;
  channel.vibrato_depth = 0;
  channel.auto_pan_depth_slide_length = 0;
  channel.update_flags = allocated ? 0 : kFullNoteLengthActive;
  channel.tremolo_depth_slide_length = 0;
  channel.vibrato_depth_slide_length = 0;
  channel.fm_timer = 0;
  channel.noise_timer = 0;
  SelectChannelInstrument(channel, 0, first_instrument);
}

void IdleChannel(SequenceChannel& record) {
  record.channel.note_length = kIdleNoteLength;
  record.channel.gate_length = kIdleGateLength;
  record.idle = true;
  record.channel.voice.dirty_flags |= kIdleVoiceDirty;
  record.channel.voice.adsr_upper = static_cast<std::uint16_t>(
      (record.channel.voice.adsr_upper & kAdsrReleaseRateMask) |
      kIdleReleaseRate);
}

void ResetLayerPlayback(MusicLayerState& layer) {
  layer.tempo = kStartTempo;
  layer.tick_accumulator = 1;
  layer.tempo_slide_length = 0;
  layer.reverb_depth = 0;
  layer.reverb_depth_slide_length = 0;
  layer.reverb_depth_slide_step = 0;
  layer.timer_lower_count = 0;
  layer.timer_lower = 0;
  layer.timer_upper_count = 0;
  layer.timer_top = 0;
  layer.noise_channels = 0;
  layer.reverb_channels = 0;
  layer.fm_channels = 0;
  layer.jump_threshold = 0;
  layer.active_notes = 0;
  layer.pending_key_on = 0;
}

void StartLayer(MusicStartState& state, const SequenceHeader& header) {
  MusicLayerState& layer = state.layer;
  layer.pending_key_off = 0;
  if ((state.stream_control_flags & kStreamPausesMusic) != 0) {
    layer.active_channels = 0;
    layer.paused_channels |= header.channel_enable_mask;
  } else {
    layer.paused_channels = 0;
    SetBits(layer.active_channels, header.channel_enable_mask);
  }
  layer.keyed_voices = header.keyed_mask;
  layer.allocated_voices =
      static_cast<std::int32_t>(header.allocated_voice_mask);
  layer.status_flags &= ~kLoadClearedStatus;
  layer.status_flags |=
      BankStatus(header.sample_bank_id, state.alternate_bank_id);
  layer.voice_search_start = 0;
  layer.resume_key_on_mask = header.resume_key_on_mask;
  layer.push_release_mask = static_cast<std::int32_t>(header.push_release_mask);
}

std::uint32_t EnabledChannels(std::uint32_t mask) {
  std::uint32_t count = 0;
  for (; mask != 0; mask &= mask - 1) ++count;
  return count;
}
}  // namespace

// decomp: SLUS_010.40 func_80015C9C (src/SLUS_010.40/25AC.c:2330)
void ResetSoundEffectChannel(
    SequenceChannel& channel, std::uint32_t program_start,
    const Samples::InstrumentRecord& first_instrument) {
  ChannelState& state = channel.channel;
  state.volume_balance = kEffectVolumeBalance;
  state.volume = kEffectVolume;
  channel.program.counter = program_start;
  channel.idle = false;
  state.transpose = 0;
  state.fine_tune = 0;
  state.portamento_steps = 0;
  channel.pitch_slide.slide = 0;
  state.pitch_bend_target = 0;
  channel.pitch_slide.ticks_left = 0;
  state.fixed_note_length = 0;
  state.stored_note_length = 0;
  state.volume_slide_length = 0;
  state.fine_pitch_delta = 0;
  state.random_pitch_depth = 0;
  channel.program.loop_depth = 0;
  state.update_flags = 0;
  state.auto_pan_offset = 0;
  state.play_modes = 0;
  channel.program.step_counter = kFreshStepCounter;
  state.voice.volume_scale = 0;
  state.auto_pan_depth = 0;
  state.tremolo_depth = 0;
  state.vibrato_depth = 0;
  state.auto_pan_depth_slide_length = 0;
  state.tremolo_depth_slide_length = 0;
  state.vibrato_depth_slide_length = 0;
  state.fm_timer = 0;
  state.noise_timer = 0;
  SelectChannelInstrument(state, 0, first_instrument);
}

// decomp: SLUS_010.40 func_80015D38 (src/SLUS_010.40/25AC.c:2363)
std::uint32_t AssignedVoiceMask(
    std::span<const std::uint32_t, kMusicChannelCount> assigned_voices,
    std::uint32_t channel_mask) {
  std::uint32_t voices = 0;
  for (std::size_t i = 0; i < kMusicChannelCount; ++i) {
    if ((channel_mask & (1U << i)) != 0 &&
        assigned_voices[i] < kUnassignedVoice) {
      voices |= 1U << assigned_voices[i];
    }
  }
  return voices;
}

std::optional<SequenceHeader> ReadSequenceHeader(
    std::span<const std::uint8_t> sequence) {
  if (sequence.size() < kSequenceHeaderBytes) return std::nullopt;
  SequenceHeader header;
  header.sample_bank_id = U32At(sequence, kSampleBankIdAt);
  header.channel_enable_mask = U32At(sequence, kChannelEnableMaskAt);
  header.keyed_mask = U32At(sequence, kKeyedMaskAt);
  header.allocated_voice_mask = U32At(sequence, kAllocatedVoiceMaskAt);
  header.patch_region_offset =
      static_cast<std::int32_t>(U32At(sequence, kPatchRegionOffsetAt));
  header.keymap_region_offset =
      static_cast<std::int32_t>(U32At(sequence, kKeymapRegionOffsetAt));
  header.resume_key_on_mask = U32At(sequence, kResumeKeyOnMaskAt);
  header.push_release_mask = U32At(sequence, kPushReleaseMaskAt);
  return header;
}

// decomp: SLUS_010.40 Sound_LoadAkaoSequence (src/SLUS_010.40/25AC.c:2380)
LoadResult LoadMusicSequence(
    MusicStartState& state, std::span<const std::uint8_t> sequence,
    const Samples::InstrumentRecord& first_instrument) {
  const std::optional<SequenceHeader> header = ReadSequenceHeader(sequence);
  if (!header ||
      sequence.size() - kSequenceHeaderBytes <
          EnabledChannels(header->channel_enable_mask) * kChannelOffsetBytes) {
    return {LoadStatus::kHeaderOutside, {}, 0};
  }
  MarkVoicesForKeyOff(state);
  StartLayer(state, *header);
  LoadResult result;
  result.tables.patch_table =
      RegionPosition(header->patch_region_offset, kPatchRegionOffsetAt);
  result.tables.keymap_table =
      RegionPosition(header->keymap_region_offset, kKeymapRegionOffsetAt);

  std::size_t offset_at = kSequenceHeaderBytes;
  for (std::size_t i = 0; i < kMusicChannelCount; ++i) {
    SequenceChannel& record = state.channels[i];
    const std::uint32_t bit = 1U << i;
    if ((header->channel_enable_mask & bit) != 0) {
      const std::size_t start = offset_at + U16At(sequence, offset_at);
      offset_at += kChannelOffsetBytes;
      record.idle = start >= sequence.size();
      record.program.counter =
          record.idle ? kOutsideProgram : static_cast<std::uint32_t>(start);
      if (record.idle) result.channels_outside |= bit;
      StartChannel(record, (header->allocated_voice_mask & bit) != 0,
                   first_instrument);
    } else {
      IdleChannel(record);
    }
    record.assigned_voice = kUnassignedVoice;
  }
  ResetLayerPlayback(state.layer);
  state.driver_update_flags |= kVoiceAssignmentUpdate;
  return result;
}

// decomp: SLUS_010.40 Sound_KillMusicConfig (src/SLUS_010.40/25AC.c:2506)
std::optional<std::uint32_t> ReleaseMusicConfiguration(
    MusicLayerState& layer, MusicChannels& channels, std::uint32_t owned_voices,
    std::uint32_t music_id_filter) {
  if (layer.active_channels == 0 ||
      (music_id_filter != 0 && music_id_filter != layer.music_id)) {
    return std::nullopt;
  }
  layer.pending_key_off = -1;
  for (SequenceChannel& record : channels) {
    record.channel.note_length = kIdleNoteLength;
    record.channel.gate_length = kIdleGateLength;
    record.idle = true;
  }
  layer.music_id = 0;
  layer.active_notes = 0;
  layer.pending_key_on = 0;
  return owned_voices;
}

namespace {
// The reverb depth a swapped-in track slides to from the secondary layer's.
void SlideReverbFromSecondary(MusicStartState& state) {
  MusicLayerState& layer = state.layer;
  std::int32_t target = Wrap(std::int64_t{layer.reverb_depth} +
                             std::int64_t{layer.reverb_depth_slide_step} *
                                 layer.reverb_depth_slide_length);
  target &= ~kReverbFractionMask;
  const std::int16_t ticks = layer.reverb_depth_slide_length != 0
                                 ? layer.reverb_depth_slide_length
                                 : kReverbSwapTicks;
  if (state.secondary_reverb_depth) {
    layer.reverb_depth = *state.secondary_reverb_depth;
  }
  layer.reverb_depth &= ~kReverbFractionMask;
  const std::int32_t change =
      Wrap(std::int64_t{target} - std::int64_t{layer.reverb_depth});
  if (change != 0) {
    layer.reverb_depth_slide_step = Divide(change, ticks);
    layer.reverb_depth_slide_length = ticks;
  } else {
    layer.reverb_depth_slide_length = 0;
  }
}
}  // namespace

// decomp: SLUS_010.40 Sound_SetMusicSequence (src/SLUS_010.40/25AC.c:2847)
std::uint32_t RestoreMusicSequence(MusicStartState& state,
                                   const SequenceHeader& header,
                                   bool swap_with_saved) {
  MusicLayerState& layer = state.layer;
  layer.status_flags &= ~kRestoreClearedStatus;
  layer.status_flags |=
      BankStatus(header.sample_bank_id, state.alternate_bank_id);
  state.driver_update_flags |= kRestoreUpdate;
  const auto active = static_cast<std::uint32_t>(layer.active_channels);
  layer.pending_key_on = static_cast<std::int32_t>(layer.active_notes);
  for (std::size_t i = 0; i < kMusicChannelCount; ++i) {
    SequenceChannel& record = state.channels[i];
    if ((active & (1U << i)) != 0) {
      record.channel.note_length = static_cast<std::uint16_t>(
          record.channel.note_length + kRestoredLengthBias);
      record.channel.gate_length = static_cast<std::uint16_t>(
          record.channel.gate_length + kRestoredLengthBias);
      record.channel.voice.dirty_flags |= kRestoredVoiceDirty;
    } else {
      record.channel.note_length = kStartNoteLength;
      record.channel.gate_length = kStartGateLength;
      record.idle = true;
    }
    record.assigned_voice = kUnassignedVoice;
  }
  layer.pending_key_off = 0;
  state.saved_music_id = 0;
  MarkVoicesForKeyOff(state);
  state.driver_update_flags |= kVoiceAssignmentUpdate;
  if (swap_with_saved) SlideReverbFromSecondary(state);
  if ((state.stream_control_flags & kStreamPausesMusic) != 0) {
    layer.paused_channels = static_cast<std::uint32_t>(layer.active_channels);
    layer.active_channels = 0;
  }
  return active;
}

namespace {
constexpr std::uint32_t kVolumeMask = 0x7F;
constexpr std::uint32_t kVolumeShift = 16;
constexpr std::uint32_t kDiscVolumeMask = 0xFFFF;
constexpr std::uint32_t kSavedMusicFade = 0x100;
constexpr std::int32_t kFadeOutLevel = 0x7F0000;
constexpr std::int32_t kFadeOutChange = -kFadeOutLevel;

std::int32_t MusicVolume(std::uint32_t level) {
  return static_cast<std::int32_t>((level & kVolumeMask) << kVolumeShift);
}

std::int32_t DiscVolume(std::uint32_t level) {
  return static_cast<std::int32_t>((level & kDiscVolumeMask) << kVolumeShift);
}

// A fade's tick count: the command's, or one for none.
std::int32_t FadeTicks(std::uint32_t ticks) {
  return ticks != 0 ? static_cast<std::int32_t>(ticks) : 1;
}

// The layer playing track `music_id`: the active one for 0 or its track,
// else the secondary layer when it plays that track.
std::optional<ChannelGroup> LayerPlaying(const MusicCommandState& state,
                                         std::uint32_t music_id) {
  if (music_id == 0 || music_id == state.active.music_id) {
    return ChannelGroup::kMusic;
  }
  if (state.secondary && music_id == state.secondary->music_id) {
    return ChannelGroup::kSecondaryMusic;
  }
  return std::nullopt;
}

MusicLayerState& Layer(MusicCommandState& state, ChannelGroup group) {
  return group == ChannelGroup::kMusic ? state.active : *state.secondary;
}

// Keeps the current track as the secondary layer while another starts.
void KeepCurrentTrack(MusicCommandState& state, MusicHost& host) {
  if (state.active.active_channels != 0 &&
      (!state.secondary || state.secondary->music_id == 0)) {
    host.AttachSavedLayer(state);
    host.SaveSnapshot(state);
  }
}
}  // namespace

// decomp: SLUS_010.40 Sound_Cmd_10_StartFieldMusic
// (src/SLUS_010.40/25AC.c:2958)
void StartOrResumeMusic(MusicCommandState& state, MusicHost& host,
                        const MusicCommand& command) {
  if (state.saved_music_id != 0 && state.saved_music_id == command.param3) {
    host.RestoreSequence(state, false);
    return;
  }
  host.LoadSequence(state);
  state.active.music_id = static_cast<std::uint16_t>(command.param3);
}

// decomp: SLUS_010.40 Sound_Cmd_40_PushMusicState (src/SLUS_010.40/25AC.c:2969)
void SaveCurrentMusic(MusicCommandState& state, MusicHost& host) {
  if (state.active.active_channels == 0) return;
  host.SaveSnapshot(state);
  host.MapSavedInstrumentsToBaseBank(state);
}

// decomp: SLUS_010.40 Sound_Cmd_19_Unk (src/SLUS_010.40/25AC.c:2983)
void StartMusicLayer(MusicCommandState& state, MusicHost& host,
                     const MusicCommand& command) {
  KeepCurrentTrack(state, host);
  host.LoadSequence(state);
  state.active.master_volume = MusicVolume(command.ext_param1);
  state.active.master_volume_slide_length = 0;
  state.active.music_id = static_cast<std::uint16_t>(command.param3);
  host.ReleaseSavedVoiceConflicts(state);
  state.mix_behavior &= ~kSavedMusicFade;
}

// decomp: SLUS_010.40 Sound_Cmd_1A_Unk (src/SLUS_010.40/25AC.c:3001)
void CrossfadeMusicLayers(MusicCommandState& state, MusicHost& host,
                          const MusicCommand& command) {
  if (state.saved_music_id != 0 && state.saved_music_id == command.param3) {
    host.AttachSavedLayer(state);
    host.RestoreSequence(state, true);
  } else {
    KeepCurrentTrack(state, host);
    host.LoadSequence(state);
    state.active.music_id = static_cast<std::uint16_t>(command.param3);
  }
  // The original divides by the tick count unchecked.
  std::int32_t ticks = static_cast<std::int32_t>(command.ext_param1);
  if (ticks == 0) {
    host.Refuse(MusicRefusal::kZeroLengthCrossfade);
    ticks = 1;
  }
  state.mix_behavior &= ~kSavedMusicFade;
  if (state.secondary) {
    state.master_fade.value = kFadeOutLevel;
    state.mix_behavior |= kSavedMusicFade;
    state.master_fade.ticks_remaining = ticks;
    state.master_fade.step = Divide(kFadeOutChange, ticks);
  }
  state.active.master_volume = 0;
  state.active.master_volume_slide_length = static_cast<std::int16_t>(ticks);
  state.active.master_volume_step = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(MusicVolume(command.ext_param2)) /
      static_cast<std::uint32_t>(ticks));
  host.ReleaseSavedVoiceConflicts(state);
}

// decomp: SLUS_010.40 Sound_Cmd_12_unk (src/SLUS_010.40/25AC.c:3034)
void StartMusicWithLoopCounter(MusicCommandState& state, MusicHost& host,
                               const MusicCommand& command) {
  StartOrResumeMusic(state, host, command);
  state.music_loops_remaining = command.param4 != 0 ? command.param4 - 1 : 0;
}

// decomp: SLUS_010.40 Sound_Cmd_C0_unk (src/SLUS_010.40/25AC.c:3123)
void SetMusicVolume(MusicCommandState& state, MusicHost& host,
                    const MusicCommand& command) {
  const std::optional<ChannelGroup> group = LayerPlaying(state, command.param1);
  if (!group) return;
  MusicLayerState& layer = Layer(state, *group);
  layer.master_volume = MusicVolume(command.param2);
  layer.master_volume_slide_length = 0;
  host.InvalidateMusicVolumes(state, *group);
}

// decomp: SLUS_010.40 Sound_Cmd_C1_unk (src/SLUS_010.40/25AC.c:3147)
void FadeMusicVolume(MusicCommandState& state, MusicHost& host,
                     const MusicCommand& command) {
  const std::int32_t ticks = FadeTicks(command.param2);
  const std::optional<ChannelGroup> group = LayerPlaying(state, command.param1);
  if (!group) return;
  MusicLayerState& layer = Layer(state, *group);
  const std::int32_t change =
      Wrap(std::int64_t{MusicVolume(command.param3)} - layer.master_volume);
  layer.master_volume_slide_length = static_cast<std::int16_t>(ticks);
  layer.master_volume_step = Divide(change, ticks);
  host.InvalidateMusicVolumes(state, *group);
}

// decomp: SLUS_010.40 Sound_Cmd_C2_unk (src/SLUS_010.40/25AC.c:3184)
void FadeMusicVolumeFrom(MusicCommandState& state, MusicHost& host,
                         const MusicCommand& command) {
  const std::int32_t ticks = FadeTicks(command.param2);
  const std::optional<ChannelGroup> group = LayerPlaying(state, command.param1);
  if (!group) return;
  MusicLayerState& layer = Layer(state, *group);
  const std::int32_t start = MusicVolume(command.param3);
  layer.master_volume = start;
  layer.master_volume_step = Divide(MusicVolume(command.param4) - start, ticks);
  layer.master_volume_slide_length = static_cast<std::int16_t>(ticks);
  host.InvalidateMusicVolumes(state, *group);
}

// decomp: SLUS_010.40 Sound_Cmd_C8_unk (src/SLUS_010.40/25AC.c:3216)
void SetDiscVolume(MusicCommandState& state, MusicHost& host,
                   const MusicCommand& command) {
  state.disc_volume.length = 0;
  state.disc_volume.value = DiscVolume(command.param1);
  host.WriteDiscVolume(state);
}

// decomp: SLUS_010.40 Sound_Cmd_C9_unk (src/SLUS_010.40/25AC.c:3223)
void FadeDiscVolume(MusicCommandState& state, const MusicCommand& command) {
  const std::int32_t ticks = FadeTicks(command.param1);
  state.disc_volume.length = static_cast<std::int16_t>(ticks);
  state.disc_volume.step = Divide(
      Wrap(std::int64_t{DiscVolume(command.param2)} - state.disc_volume.value),
      ticks);
}

// decomp: SLUS_010.40 Sound_Cmd_CA_unk (src/SLUS_010.40/25AC.c:3237)
void FadeDiscVolumeFrom(MusicCommandState& state, const MusicCommand& command) {
  const std::int32_t ticks = FadeTicks(command.param1);
  state.disc_volume.length = static_cast<std::int16_t>(ticks);
  const std::int32_t target = DiscVolume(command.param3);
  state.disc_volume.value = DiscVolume(command.param2);
  state.disc_volume.step =
      Divide(Wrap(std::int64_t{target} - state.disc_volume.value), ticks);
}
}  // namespace VS::Audio::Seq
