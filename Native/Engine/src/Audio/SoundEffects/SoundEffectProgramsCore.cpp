// Hand-maintained (tools/port/owned.csv).
// Typed sound-effect programs (SoundEffectPrograms.hpp).
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

#include "Audio/MixControl.hpp"
#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqSequencer.hpp"
#include "Audio/SeqVoiceCommands.hpp"
#include "Audio/SoundEffectPrograms.hpp"
#include "Audio/SoundPlayback.hpp"
#include "Audio/VoiceMixer.hpp"

namespace VS::Audio::SoundEffects {
namespace {
constexpr unsigned kHalfwordBits = 16;
constexpr std::uint32_t kHalfwordMask = 0xFFFF;
constexpr unsigned kLevelShift = 8;
constexpr std::uint32_t kPanByteMask = 0xFF;
// The original's variable shifts use the low five bits of their count.
constexpr std::uint32_t kShiftCountMask = 31;
constexpr std::int64_t kLastShiftCount = 31;

std::uint32_t VoiceBit(std::size_t index) { return kFirstVoiceBit << index; }

std::uint32_t HeldVoices(const EffectState& state) {
  return state.scheduler.active_voices | state.scheduler.paused_effect_voices;
}

std::uint32_t BusyVoices(const EffectState& state) {
  return HeldVoices(state) | state.stream_voices;
}

// The command's first word as the original's 32-bit parameter.
std::uint32_t FirstWord(EffectSelector first) {
  return static_cast<std::uint32_t>(static_cast<std::intptr_t>(first));
}

std::int16_t VolumeLevel(std::uint32_t word) {
  return static_cast<std::int16_t>((word & Playback::kVolumeMask)
                                   << kLevelShift);
}

std::uint16_t PanLevel(std::uint32_t word) {
  return static_cast<std::uint16_t>((word & kPanByteMask) << kLevelShift);
}

// Releases channel `index`, or lets a channel finishing its loop stop at its
// loop end.
void ReleaseChannel(EffectState& state, std::size_t index) {
  EffectChannel& channel = state.channels[index];
  if ((channel.update_flags & Seq::kLoopBoundaryStop) != 0) {
    channel.update_flags |= Seq::kStopAtEndlessLoop;
    return;
  }
  const std::uint32_t bit = VoiceBit(index);
  state.scheduler.pending_key_off |= bit;
  Seq::EffectIdentity identity{
      .tags = channel.tags,
      .has_effect = channel.effect != EffectSelector::kNone};
  Seq::ReleaseSoundEffectVoice(state.scheduler, identity, bit);
  channel.tags = identity.tags;
  if (!identity.has_effect) {
    channel.effect = EffectSelector::kNone;
    channel.kind = EffectKind::kShared;
  }
  channel.update_flags = 0;
}

void ReleaseTagged(EffectState& state, std::int32_t flags) {
  const std::uint32_t held = HeldVoices(state);
  for (std::size_t index = 0; index < kCommandChannelCount; ++index) {
    if ((held & VoiceBit(index)) != 0 &&
        (state.channels[index].tags & flags) != 0) {
      ReleaseChannel(state, index);
    }
  }
}

// The original matches kProgramEffects by the sign of the effect word, an
// address at or above 0x80000000 (25AC.c:2605); the channel names its kind.
bool Matches(const EffectChannel& channel, EffectSelector selector) {
  if (selector == kProgramEffects) {
    return channel.kind == EffectKind::kProgram;
  }
  if (selector == kUntaggedEffects) return channel.tags == 0;
  return channel.effect == selector;
}

void ReleaseSelected(EffectState& state, EffectSelector selector) {
  const std::uint32_t held = HeldVoices(state);
  for (std::size_t index = 0; index < kCommandChannelCount; ++index) {
    if ((held & VoiceBit(index)) != 0 &&
        Matches(state.channels[index], selector)) {
      ReleaseChannel(state, index);
    }
  }
  state.driver_update_flags |= kVoiceUpdate;
}

// Releases the effects channels `selector` and `selector + 1` play. Channels
// whose voice bit shifts past bit 31 are never read.
void ReleasePair(EffectState& state, EffectHost& host,
                 EffectSelector selector) {
  const auto first = static_cast<std::int64_t>(selector);
  if (first < 0 || first > kLastShiftCount) {
    host.Refuse(EffectRefusal::kChannelOutOfRange);
    return;
  }
  const std::uint32_t held = HeldVoices(state);
  const auto start = static_cast<std::size_t>(first);
  for (std::size_t index = start; index < start + kPairChannelCount; ++index) {
    if (index < kChannelCount && (held & VoiceBit(index)) != 0) {
      ReleaseSelected(state, state.channels[index].effect);
    }
  }
}

void EvictUntagged(EffectState& state) {
  std::uint32_t held = HeldVoices(state);
  for (std::size_t index = 0; index < kCommandChannelCount; ++index) {
    if (state.channels[index].tags != 0) held &= ~VoiceBit(index);
  }
  std::int32_t highest = 0;
  for (std::size_t index = 0; index < kCommandChannelCount; ++index) {
    if ((held & VoiceBit(index)) != 0 &&
        highest < state.channels[index].priority) {
      highest = state.channels[index].priority;
    }
  }
  for (std::size_t index = 0; index < kCommandChannelCount; ++index) {
    if ((held & VoiceBit(index)) != 0 &&
        state.channels[index].priority == highest) {
      ReleaseChannel(state, index);
    }
  }
}

// Moves the active scene effects (those without kInterfaceEffectTag) to the
// paused voices.
void PauseSceneEffects(EffectState& state) {
  Seq::VoiceSchedulerState& scheduler = state.scheduler;
  for (std::size_t index = 0; index < kCommandChannelCount; ++index) {
    const std::uint32_t bit = VoiceBit(index);
    if ((scheduler.active_voices & bit) != 0 &&
        (static_cast<std::uint32_t>(state.channels[index].tags) &
         kInterfaceEffectTag) == 0) {
      scheduler.active_voices &= ~bit;
      scheduler.paused_effect_voices |= bit;
    }
  }
}

// The first channel of the highest pair whose voices are all free.
std::optional<std::size_t> FreePair(const EffectState& state) {
  const std::uint32_t busy = BusyVoices(state);
  for (std::size_t first = kLastPairChannel + kPairChannelCount; first != 0;) {
    first -= kPairChannelCount;
    if ((busy & (VoiceBit(first) | VoiceBit(first + 1))) == 0) return first;
  }
  return std::nullopt;
}

void StartOnChannel(EffectState& state, EffectHost& host, std::size_t index,
                    const EffectCommand& command, ProgramStart program) {
  StartChannelProgram(state, host, index, command, VoiceBit(index), program);
  DetachMusicVoice(state, host, state.channels[index].assigned_voice);
}

template <typename Unsigned>
std::optional<Unsigned> ReadLittle(std::span<const std::byte> bytes,
                                   std::size_t offset) {
  if (offset > bytes.size() || bytes.size() - offset < sizeof(Unsigned)) {
    return std::nullopt;
  }
  Unsigned value = 0;
  std::memcpy(&value, bytes.data() + offset, sizeof(value));
  if constexpr (std::endian::native == std::endian::big) {
    value = std::byteswap(value);
  }
  return value;
}

std::optional<ProgramStart> SharedProgram(EffectHost& host,
                                          const SharedTables& tables,
                                          std::uint32_t offset) {
  if (offset == kNoProgram) return std::nullopt;
  if (offset >= tables.program_bytes) {
    host.Refuse(EffectRefusal::kProgramOutOfRange);
    return std::nullopt;
  }
  return ProgramStart{.data = tables.program_data, .offset = offset};
}

// The sound-effect bank slot of shared effect `effect`'s selector. The
// original indexes the selectors with the unmasked effect id.
std::optional<std::uint32_t> SharedBankSlot(EffectHost& host,
                                            EffectSelector effect) {
  const std::optional<SharedTables> tables = host.Tables();
  if (!tables) return std::nullopt;
  const auto index = static_cast<std::uintptr_t>(effect);
  if (index >= tables->bank_selectors.size()) {
    host.Refuse(EffectRefusal::kEffectOutOfRange);
    return std::nullopt;
  }
  return host.BankSlot(tables->bank_selectors[index]);
}

void PlayShared(EffectState& state, EffectHost& host, EffectCommand& command,
                const ProgramPair& programs) {
  const std::optional<std::uint32_t> slot = SharedBankSlot(host, command.first);
  if (!slot) return;
  command.bank_slot = *slot;
  PlayProgramPair(state, host, command, programs, false);
}

// Blob entry `index`'s programs, which count from the end of the entry's two
// offset halfwords; empty when the entry or a program lies outside the blob.
std::optional<ProgramPair> BlobEntry(std::span<const std::byte> bytes,
                                     DataRef blob, std::size_t index) {
  const std::optional<std::uint32_t> word = ReadLittle<std::uint32_t>(
      bytes, kBlobOffsetsStart + (index * sizeof(std::uint32_t)));
  if (!word) return std::nullopt;
  const std::int64_t entry = std::int64_t{static_cast<std::int32_t>(*word)} +
                             std::int64_t{kBlobEntriesStart};
  if (entry < 0) return std::nullopt;
  const auto start = static_cast<std::size_t>(entry);
  ProgramPair programs;
  for (std::size_t half = 0; half < kPairChannelCount; ++half) {
    const std::optional<std::uint16_t> offset = ReadLittle<std::uint16_t>(
        bytes, start + (half * sizeof(std::uint16_t)));
    if (!offset) return std::nullopt;
    if (*offset == kNoProgram) continue;
    const std::size_t program = start + kBlobEntryHeaderBytes + *offset;
    if (program >= bytes.size()) return std::nullopt;
    (half == 0 ? programs.first : programs.second) =
        ProgramStart{.data = blob, .offset = program};
  }
  return programs;
}

struct FadeSlide {
  std::int16_t step = 0;
  std::uint16_t length = 0;
};

// A fade from `current` to `target` over the 16-bit `ticks` (one tick when
// zero).
FadeSlide Slide(EffectHost& host, std::uint32_t ticks, std::int32_t target,
                std::int32_t current) {
  auto length = static_cast<std::int16_t>(ticks != 0 ? ticks : 1);
  if (length == 0) {
    host.Refuse(EffectRefusal::kZeroLengthFade);
    length = 1;
  }
  const auto distance = static_cast<std::int16_t>(target - current);
  return {.step = static_cast<std::int16_t>(distance / length),
          .length = static_cast<std::uint16_t>(length)};
}

void SetVolume(EffectChannel& channel, std::uint32_t word) {
  channel.effect_volume = VolumeLevel(word);
  channel.effect_volume_length = 0;
  channel.dirty_flags |= Seq::kVolumeDirty;
}

void SetPan(EffectChannel& channel, std::uint32_t word) {
  channel.effect_pan = PanLevel(word);
  channel.effect_pan_length = 0;
  channel.dirty_flags |= Seq::kVolumeDirty;
}

void FadeVolume(EffectHost& host, EffectChannel& channel, std::uint32_t ticks,
                std::uint32_t target) {
  const FadeSlide slide =
      Slide(host, ticks, VolumeLevel(target), channel.effect_volume);
  channel.effect_volume_step = slide.step;
  channel.effect_volume_length = slide.length;
}

void FadePan(EffectHost& host, EffectChannel& channel, std::uint32_t ticks,
             std::uint32_t target) {
  const FadeSlide slide =
      Slide(host, ticks, PanLevel(target), channel.effect_pan);
  channel.effect_pan_step = slide.step;
  channel.effect_pan_length = slide.length;
}

// The program commands' channels: those carrying a tag of `second`, or,
// without tags, those playing `first`.
bool ProgramSelected(const EffectChannel& channel,
                     const EffectCommand& command) {
  if (command.second != 0) {
    return (static_cast<std::uint32_t>(channel.tags) & command.second) != 0;
  }
  return channel.effect == command.first;
}

// The scene commands' channels: the effects without kInterfaceEffectTag.
bool SceneSelected(const EffectChannel& channel) {
  return (static_cast<std::uint32_t>(channel.tags) & kInterfaceEffectTag) == 0;
}

// Applies `apply` to the active command channels `select` accepts.
template <typename Select, typename Apply>
void ForSelected(EffectState& state, Select select, Apply apply) {
  const std::uint32_t active = state.scheduler.active_voices;
  for (std::size_t index = 0; index < kCommandChannelCount; ++index) {
    EffectChannel& channel = state.channels[index];
    if ((active & VoiceBit(index)) != 0 && select(channel)) apply(channel);
  }
}
}  // namespace

// decomp: SLUS_010.40 Sound_EvictSfxVoice (src/SLUS_010.40/25AC.c:2539)
// A negative release (the channel pair path) names its channel with the
// original's shift count; counts outside 0-31 are refused.
void ReleaseMatchingVoices(EffectState& state, EffectHost& host,
                           EffectSelector selector, std::int32_t flags) {
  if ((flags & kReleaseTagMask) != 0) {
    ReleaseTagged(state, flags);
    state.driver_update_flags |= kVoiceUpdate;
  } else if (flags < 0) {
    ReleasePair(state, host, selector);
  } else if ((flags & kEvictUntagged) != 0) {
    EvictUntagged(state);
    state.driver_update_flags |= kVoiceUpdate;
  } else {
    ReleaseSelected(state, selector);
  }
}

// decomp: SLUS_010.40 func_8001653C (src/SLUS_010.40/25AC.c:2638)
// The voice mask shifts by the bank slot's low five bits, as the original's
// sllv does.
void StartChannelProgram(EffectState& state, EffectHost& host,
                         std::size_t index, const EffectCommand& command,
                         std::uint32_t voice_bit, ProgramStart program) {
  if (index >= kChannelCount) {
    host.Refuse(EffectRefusal::kChannelOutOfRange);
    return;
  }
  EffectChannel& channel = state.channels[index];
  channel.effect = command.first;
  channel.kind = command.first_kind;
  channel.tags = static_cast<std::int32_t>(command.second);
  channel.effect_pan_length = 0;
  channel.pan = kCenteredChannelPan;
  channel.pan_slide_length = 0;
  channel.effect_pan = PanLevel(command.third);
  channel.effect_volume_length = 0;
  channel.effect_volume = VolumeLevel(command.fourth);
  channel.note_length = kStartNoteLength;
  channel.voice_mask =
      static_cast<std::int32_t>(1U << (command.bank_slot & kShiftCountMask));
  channel.gate_length = kStartGateLength;
  channel.type = kEffectChannelType;
  channel.priority = kStartPriority;
  channel.pitch_scale = 0;
  channel.pitch_scale_length = 0;
  host.ResetChannel(index, channel, program);
  if (channel.assigned_voice < Mixer::kNoVoice) {
    host.ClearVoiceOwner(channel.assigned_voice);
  } else {
    host.Refuse(EffectRefusal::kVoiceOutOfRange);
  }
  host.SetVoiceRelease(channel.assigned_voice, kStartReleaseRate,
                       kStartReleaseMode);

  Seq::VoiceSchedulerState& scheduler = state.scheduler;
  scheduler.active_voices |= voice_bit;
  scheduler.pending_key_off |= voice_bit;
  const std::uint32_t keep = ~voice_bit;
  scheduler.pending_key_on &= keep;
  scheduler.keyed_voices &= keep;
  scheduler.noise_voices &= keep;
  scheduler.reverb_voices &= keep;
  scheduler.fm_voices &= keep;
  if ((state.stream_control_flags & Mixer::kEffectsPaused) != 0) {
    PauseSceneEffects(state);
  }
}

// decomp: SLUS_010.40 FreeVoiceChannels (src/SLUS_010.40/25AC.c:2690)
// The original clears the active layer's note bit through a null layer when
// no music plays; that update is refused.
void DetachMusicVoice(EffectState& state, EffectHost& host,
                      std::uint32_t voice) {
  if (voice >= Mixer::kNoVoice) return;
  for (std::size_t index = 0; index < state.music_voices.size(); ++index) {
    if (state.music_voices[index] != voice) continue;
    state.music_voices[index] = Mixer::kNoVoice;
    if (!state.active_music_notes) {
      host.Refuse(EffectRefusal::kNoActiveMusic);
      continue;
    }
    *state.active_music_notes &= ~(1U << index);
  }
}

// decomp: SLUS_010.40 Sound_PlaySfxProgram (src/SLUS_010.40/25AC.c:2709)
void PlayProgramPair(EffectState& state, EffectHost& host,
                     const EffectCommand& command, const ProgramPair& programs,
                     bool skip_release) {
  if (!programs.first && !programs.second) return;
  if (!skip_release && command.second != 0) {
    ReleaseMatchingVoices(state, host, EffectSelector::kNone,
                          static_cast<std::int32_t>(command.second));
  }
  std::optional<std::size_t> pair = FreePair(state);
  while (!pair) {
    const std::uint32_t busy = BusyVoices(state);
    ReleaseMatchingVoices(state, host, EffectSelector::kNone, kEvictUntagged);
    if (BusyVoices(state) == busy) return;
    pair = FreePair(state);
  }

  std::size_t index = *pair;
  if (programs.first) {
    StartOnChannel(state, host, index, command, *programs.first);
  }
  if (programs.second) {
    if (programs.first) ++index;
    StartOnChannel(state, host, index, command, *programs.second);
    if (programs.first) state.channels[index].update_flags |= kStereoLinked;
  }
  state.driver_update_flags |= kVoiceUpdate;
}

// decomp: SLUS_010.40 Sound_GetProgramCounters (src/SLUS_010.40/25AC.c:2777)
// The original reads ids 0x180-0x3FF past the offsets table.
ProgramPair ProgramCounters(EffectHost& host, std::uint32_t effect_id) {
  const std::optional<SharedTables> tables = host.Tables();
  if (!tables) {
    host.Refuse(EffectRefusal::kTablesUnbound);
    return {};
  }
  const std::uint32_t id = effect_id & Playback::kEffectIdMask;
  if (id >= tables->program_offsets.size()) {
    host.Refuse(EffectRefusal::kEffectOutOfRange);
    return {};
  }
  const std::uint32_t offsets = tables->program_offsets[id];
  return {.first = SharedProgram(host, *tables, offsets & kHalfwordMask),
          .second = SharedProgram(host, *tables, offsets >> kHalfwordBits)};
}

// decomp: SLUS_010.40 Sound_MarkActiveChannelsVolumeDirty
// (src/SLUS_010.40/25AC.c:2797)
void InvalidateMusicChannelVolumes(
    std::uint32_t active_channels,
    std::span<std::uint32_t, Seq::kMusicChannelCount> dirty_flags) {
  for (std::size_t index = 0; index < dirty_flags.size(); ++index) {
    if (((active_channels >> index) & 1U) != 0) {
      dirty_flags[index] |= Seq::kVolumeDirty;
    }
  }
}

// decomp: SLUS_010.40 Sound_MarkScheduledSfxChannelsVolumeDirty
// (src/SLUS_010.40/25AC.c:2822) The original loops forever over active voices
// below voice 12.
void InvalidateSoundEffectVolumes(EffectState& state, EffectHost& host) {
  const std::uint32_t active = state.scheduler.active_voices;
  if (active == 0) return;
  if ((active & (kFirstVoiceBit - 1)) != 0) {
    host.Refuse(EffectRefusal::kVoiceOutsideEffects);
  }
  for (std::size_t index = 0; index < kChannelCount; ++index) {
    if ((active & VoiceBit(index)) != 0) {
      state.channels[index].dirty_flags |= Seq::kVolumeDirty;
    }
  }
}

// decomp: SLUS_010.40 Sound_Cmd_34_unk (src/SLUS_010.40/25AC.c:3046)
// The second program arrives in the 32-bit second word.
void PlayInterfaceProgramPair(EffectState& state, EffectHost& host,
                              EffectCommand& command) {
  const auto first =
      static_cast<DataRef>(static_cast<std::uintptr_t>(command.first));
  const auto second = static_cast<DataRef>(std::uintptr_t{command.second});
  ProgramPair programs;
  if (first != DataRef::kNone) programs.first = ProgramStart{.data = first};
  if (second != DataRef::kNone) programs.second = ProgramStart{.data = second};
  command.first = kInterfacePairEffect;
  command.second = kInterfacePairTag;
  command.third = kCenterPan;
  command.fourth = kFullVolume;
  command.bank_slot = 0;
  PlayProgramPair(state, host, command, programs, false);
}

// decomp: SLUS_010.40 Sound_Cmd_30_unk (src/SLUS_010.40/25AC.c:3061)
void PlayInterfaceProgram(EffectState& state, EffectHost& host,
                          EffectCommand& command) {
  const ProgramPair programs = ProgramCounters(host, FirstWord(command.first));
  command.second = kInterfaceEffectTag;
  command.third = kCenterPan;
  command.fourth = kFullVolume;
  PlayShared(state, host, command, programs);
}

// decomp: SLUS_010.40 Sound_Cmd_20_unk (src/SLUS_010.40/25AC.c:3074)
void PlaySharedProgram(EffectState& state, EffectHost& host,
                       EffectCommand& command) {
  const ProgramPair programs = ProgramCounters(host, FirstWord(command.first));
  PlayShared(state, host, command, programs);
}

// decomp: SLUS_010.40 func_8001733C (src/SLUS_010.40/25AC.c:3085)
// The original reads every entry without bounds and counts a count of zero
// or less down through every 32-bit value; both are refused.
void PlayBlobPrograms(EffectState& state, EffectHost& host,
                      EffectCommand& command) {
  const auto blob =
      static_cast<DataRef>(static_cast<std::uintptr_t>(command.first));
  const std::span<const std::byte> bytes = host.ProgramBytes(blob);
  const std::optional<std::uint32_t> selector = ReadLittle<std::uint32_t>(
      bytes, kBlobSelectorWord * sizeof(std::uint32_t));
  const std::optional<std::uint32_t> count =
      ReadLittle<std::uint32_t>(bytes, kBlobCountWord * sizeof(std::uint32_t));
  if (!selector || !count) {
    host.Refuse(EffectRefusal::kBlobUnreadable);
    return;
  }
  command.bank_slot = host.BankSlot(static_cast<std::int32_t>(*selector));
  command.first_kind = EffectKind::kProgram;
  const auto entries = static_cast<std::int32_t>(*count);
  for (std::size_t index = 0;; ++index) {
    const std::optional<ProgramPair> programs = BlobEntry(bytes, blob, index);
    if (!programs) {
      host.Refuse(EffectRefusal::kBlobUnreadable);
      return;
    }
    PlayProgramPair(state, host, command, *programs, index != 0);
    if (entries <= 0) {
      host.Refuse(EffectRefusal::kBlobCountInvalid);
      return;
    }
    if (index + 1 >= static_cast<std::size_t>(entries)) return;
  }
}

// decomp: SLUS_010.40 Sound_Cmd_21_unk (src/SLUS_010.40/25AC.c:3118)
void StopPrograms(EffectState& state, EffectHost& host,
                  const EffectCommand& command) {
  ReleaseMatchingVoices(state, host, command.first,
                        static_cast<std::int32_t>(command.second));
}

// decomp: SLUS_010.40 Sound_Cmd_A0_unk (src/SLUS_010.40/25AC.c:3250)
void SetProgramVolume(EffectState& state, const EffectCommand& command) {
  ForSelected(
      state,
      [&](const EffectChannel& channel) {
        return ProgramSelected(channel, command);
      },
      [&](EffectChannel& channel) { SetVolume(channel, command.third); });
}

// decomp: SLUS_010.40 Sound_Cmd_A1_unk (src/SLUS_010.40/25AC.c:3276)
// A third word whose low half is zero but which is not zero divides by zero
// in the original; that fade runs over one tick.
void FadeProgramVolume(EffectState& state, EffectHost& host,
                       const EffectCommand& command) {
  ForSelected(
      state,
      [&](const EffectChannel& channel) {
        return ProgramSelected(channel, command);
      },
      [&](EffectChannel& channel) {
        FadeVolume(host, channel, command.third, command.fourth);
      });
}

// decomp: SLUS_010.40 Sound_Cmd_A8_unk (src/SLUS_010.40/25AC.c:3302)
void SetSceneVolume(EffectState& state, const EffectCommand& command) {
  ForSelected(state, SceneSelected, [&](EffectChannel& channel) {
    SetVolume(channel, FirstWord(command.first));
  });
}

// decomp: SLUS_010.40 Sound_Cmd_A9_unk (src/SLUS_010.40/25AC.c:3318)
void FadeSceneVolume(EffectState& state, EffectHost& host,
                     const EffectCommand& command) {
  ForSelected(state, SceneSelected, [&](EffectChannel& channel) {
    FadeVolume(host, channel, FirstWord(command.first), command.second);
  });
}

// decomp: SLUS_010.40 Sound_Cmd_A2_unk (src/SLUS_010.40/25AC.c:3339)
void SetProgramPan(EffectState& state, const EffectCommand& command) {
  ForSelected(
      state,
      [&](const EffectChannel& channel) {
        return ProgramSelected(channel, command);
      },
      [&](EffectChannel& channel) { SetPan(channel, command.third); });
}

// decomp: SLUS_010.40 Sound_Cmd_A3_unk (src/SLUS_010.40/25AC.c:3365)
void FadeProgramPan(EffectState& state, EffectHost& host,
                    const EffectCommand& command) {
  ForSelected(
      state,
      [&](const EffectChannel& channel) {
        return ProgramSelected(channel, command);
      },
      [&](EffectChannel& channel) {
        FadePan(host, channel, command.third, command.fourth);
      });
}

// decomp: SLUS_010.40 Sound_Cmd_AA_unk (src/SLUS_010.40/25AC.c:3391)
void SetScenePan(EffectState& state, const EffectCommand& command) {
  ForSelected(state, SceneSelected, [&](EffectChannel& channel) {
    SetPan(channel, FirstWord(command.first));
  });
}

// decomp: SLUS_010.40 Sound_Cmd_AB_unk (src/SLUS_010.40/25AC.c:3407)
void FadeScenePan(EffectState& state, EffectHost& host,
                  const EffectCommand& command) {
  ForSelected(state, SceneSelected, [&](EffectChannel& channel) {
    FadePan(host, channel, FirstWord(command.first), command.second);
  });
}
}  // namespace VS::Audio::SoundEffects
