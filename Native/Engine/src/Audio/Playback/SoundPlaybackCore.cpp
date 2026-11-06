// Hand-maintained (tools/port/owned.csv).
// Typed sound playback front end (SoundPlayback.hpp).
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <variant>

#include "Audio/SoundPlayback.hpp"

namespace VS::Audio::Playback {
namespace {
constexpr unsigned kHalfwordBits = 16;
constexpr std::uint32_t kHalfwordMask = 0xFFFF;
constexpr std::uint32_t kEffectVoiceBits = 0xFFFFFF;

std::uint32_t Masked(std::int32_t value, std::uint32_t mask) {
  return static_cast<std::uint32_t>(value) & mask;
}

std::uint32_t Dispatched(PlaybackHost& host, Command command,
                         DataRef sequence) {
  return host.Dispatch(command, {.first = sequence}).value;
}

std::uint32_t ReadWord(std::span<const std::byte> bytes) {
  std::uint32_t word = 0;
  std::memcpy(&word, bytes.data(), sizeof(word));
  if constexpr (std::endian::native == std::endian::big) {
    word = std::byteswap(word);
  }
  return word;
}

// Sound_IsNotAkaoFile over the first word of `bytes`.
std::int32_t MagicDifference(std::span<const std::byte> bytes) {
  return static_cast<std::int32_t>(ReadWord(bytes) - kAkaoMagic);
}

// The bound tables, refusing unbound ones.
std::optional<EffectTables> BoundTables(PlaybackHost& host) {
  std::optional<EffectTables> tables = host.Tables();
  if (!tables) host.Refuse(PlaybackRefusal::kEffectTablesUnbound);
  return tables;
}
}  // namespace

// decomp: SLUS_010.40 InitSound (src/SLUS_010.40/25AC.c:195)
std::int32_t Initialize(PlaybackHost& host) {
  host.StartDriver();
  return 0;
}

// decomp: SLUS_010.40 TeardownSound (src/SLUS_010.40/25AC.c:201)
std::int32_t Shutdown(PlaybackHost& host) {
  host.StopDriver();
  return 0;
}

// decomp: SLUS_010.40 Sound_BindAkaoSfxBlob (src/SLUS_010.40/25AC.c:207)
std::int32_t BindSoundEffectPrograms(PlaybackHost& host,
                                     std::span<std::byte> blob) {
  if (blob.size() < sizeof(std::uint32_t)) {
    host.Refuse(PlaybackRefusal::kBlobTooSmall);
    return -1;
  }
  const std::int32_t not_akao = MagicDifference(blob);
  if (not_akao != 0) return not_akao;
  if (blob.size() < kEffectProgramDataStart) {
    host.Refuse(PlaybackRefusal::kBlobTooSmall);
    return not_akao;
  }
  const EffectProgramBlob tables{
      .program_offsets =
          blob.subspan(kEffectBlobHeaderBytes, kEffectOffsetBytes),
      .bank_selectors = blob.subspan(
          kEffectBlobHeaderBytes + kEffectOffsetBytes, kEffectSelectorBytes),
      .program_data = blob.subspan(kEffectProgramDataStart)};
  if (!host.BindTables(tables)) {
    host.Refuse(PlaybackRefusal::kBlobMisaligned);
  }
  return not_akao;
}

// decomp: SLUS_010.40 func_80011E40 (src/SLUS_010.40/25AC.c:223)
std::int32_t ActivePlaybackFlags(PlaybackHost& host) {
  const PlaybackStatus status = host.Status();
  std::int32_t flags =
      status.active.active_channels != 0 ? kActiveLayerPlaying : 0;
  if (status.secondary && status.secondary->active_channels != 0) {
    flags |= kSecondaryLayerPlaying;
  }
  if (status.transition_queued) flags |= kTransitionQueued;
  if (status.active_voices != 0) flags |= kEffectVoicesPlaying;
  if (status.stream_voices != 0) flags |= kStreamPlaying;
  return flags;
}

// decomp: SLUS_010.40 func_80011EBC (src/SLUS_010.40/25AC.c:243)
std::int32_t MusicVolume(PlaybackHost& host, std::uint32_t music_id) {
  const PlaybackStatus status = host.Status();
  if (status.active.music_id == music_id) {
    return status.active.master_volume >> kHalfwordBits;
  }
  if (status.secondary && status.secondary->music_id == music_id) {
    return status.secondary->master_volume >> kHalfwordBits;
  }
  return 0;
}

// decomp: SLUS_010.40 func_80011F08 (src/SLUS_010.40/25AC.c:256)
std::int32_t PackedMusicSampleBankIds(PlaybackHost& host) {
  const MusicBankIds ids = host.MusicBanks();
  const std::uint32_t packed =
      Masked(ids.bank, kHalfwordMask) |
      (Masked(ids.alternate, kHalfwordMask) << kHalfwordBits);
  return static_cast<std::int32_t>(packed);
}

// decomp: SLUS_010.40 func_80011F24 (src/SLUS_010.40/25AC.c:261)
std::int32_t LongestMusicFade(PlaybackHost& host, std::int32_t fade_selectors) {
  const PlaybackStatus status = host.Status();
  std::int32_t longest = 0;
  if ((fade_selectors & kMasterVolumeFade) != 0) {
    longest = status.active.master_volume_slide_length;
  }
  if ((fade_selectors & kTempoFade) != 0 &&
      longest < status.tempo_slide_length) {
    longest = status.tempo_slide_length;
  }
  if ((fade_selectors & kPitchFade) != 0 &&
      longest < status.pitch_slide_length) {
    longest = status.pitch_slide_length;
  }
  return longest;
}

// decomp: SLUS_010.40 func_80011F90 (src/SLUS_010.40/25AC.c:278)
std::int32_t MusicPosition(PlaybackHost& host) {
  const Seq::MusicLayerState layer = host.Status().active;
  const std::uint32_t top = layer.timer_top;
  const std::uint32_t upper = layer.timer_upper_count;
  return static_cast<std::int32_t>((top << kHalfwordBits) | (upper + 1));
}

// decomp: SLUS_010.40 func_80011FB4 (src/SLUS_010.40/25AC.c:286)
std::uint32_t StartMusic(PlaybackHost& host, DataRef sequence) {
  return Dispatched(host, Command::kStartMusic, sequence);
}

// decomp: SLUS_010.40 func_80011FDC (src/SLUS_010.40/25AC.c:292)
void StopMusic(PlaybackHost& host, std::uint32_t music_id) {
  host.Dispatch(Command::kStopMusic,
                {.first = static_cast<std::intptr_t>(music_id)});
}

// decomp: SLUS_010.40 func_80012004 (src/SLUS_010.40/25AC.c:298)
void SaveMusicState(PlaybackHost& host) {
  host.Dispatch(Command::kSaveMusic, {});
}

// decomp: SLUS_010.40 func_80012024 (src/SLUS_010.40/25AC.c:300)
std::int32_t InvalidateSavedMusicId(PlaybackHost& host) {
  host.ClearSavedMusicId();
  return 0;
}

// decomp: SLUS_010.40 func_80012034 (src/SLUS_010.40/25AC.c:308)
std::uint32_t StartLayeredMusic(PlaybackHost& host, DataRef sequence,
                                std::int32_t volume) {
  const std::uint32_t music_id =
      Dispatched(host, Command::kStartLayeredMusic, sequence);
  host.SetMusicVolume(music_id, volume);
  return music_id;
}

// decomp: SLUS_010.40 func_80012080 (src/SLUS_010.40/25AC.c:318)
std::uint32_t CrossfadeMusic(PlaybackHost& host, DataRef sequence,
                             std::uint32_t fade_ticks,
                             std::int32_t target_volume) {
  return host
      .Dispatch(Command::kCrossfadeMusic,
                {.first = sequence,
                 .second = fade_ticks,
                 .third = Masked(target_volume, kVolumeMask)})
      .value;
}

// decomp: SLUS_010.40 func_800120B8 (src/SLUS_010.40/25AC.c:326)
void StartMusicAtLoop(PlaybackHost& host, DataRef sequence,
                      std::int32_t loop_count) {
  host.Dispatch(
      Command::kStartMusicAtLoop,
      {.first = sequence, .second = static_cast<std::uint32_t>(loop_count)});
}

// decomp: SLUS_010.40 func_800120E8 (src/SLUS_010.40/25AC.c:333)
std::uint32_t QueueMusicTransition(PlaybackHost& host, DataRef sequence) {
  return Dispatched(host, Command::kQueueMusicTransition, sequence);
}

// decomp: SLUS_010.40 func_80012110 (src/SLUS_010.40/25AC.c:339)
MusicPair StartMusicPair(PlaybackHost& host, DataRef primary,
                         DataRef secondary) {
  const CommandResult result =
      host.Dispatch(Command::kStartMusicPair,
                    {.first = primary, .second_sequence = secondary});
  return {.first = result.first, .second = result.second};
}

// decomp: SLUS_010.40 func_8001215C (src/SLUS_010.40/25AC.c:350)
void RequestMusicLayerSwap(PlaybackHost& host) {
  host.Dispatch(Command::kSwapMusicLayers, {});
}

// decomp: SLUS_010.40 func_8001217C (src/SLUS_010.40/25AC.c:352)
std::int32_t PlaySoundEffect(PlaybackHost& host, std::uint32_t effect_id,
                             std::int32_t effect_tag, std::int32_t pan,
                             std::int32_t volume) {
  constexpr auto kNone = static_cast<std::int32_t>(kNoEffectProgram);
  const std::uint32_t id = effect_id & kEffectIdMask;
  const std::optional<EffectTables> tables = BoundTables(host);
  if (!tables) return kNone;
  if (id >= tables->program_offsets.size()) {
    host.Refuse(PlaybackRefusal::kEffectOutOfRange);
    return kNone;
  }
  const std::uint32_t program_offset = tables->program_offsets[id];
  if (program_offset != kNoEffectProgram) {
    host.Dispatch(Command::kPlaySoundEffect,
                  {.first = static_cast<std::intptr_t>(id),
                   .second = Masked(effect_tag, kEffectTagMask),
                   .third = Masked(pan, kPanMask),
                   .fourth = Masked(volume, kVolumeMask)});
  }
  return static_cast<std::int32_t>(program_offset);
}

// decomp: SLUS_010.40 func_800121F0 (src/SLUS_010.40/25AC.c:365)
std::optional<DataRef> PlaySoundEffectBlob(PlaybackHost& host, DataRef program,
                                           std::int32_t effect_tag,
                                           std::int32_t pan,
                                           std::int32_t volume) {
  const std::span<const std::byte> bytes = host.ProgramBytes(program);
  if (bytes.size() < sizeof(std::uint32_t)) {
    host.Refuse(PlaybackRefusal::kProgramUnreadable);
    return std::nullopt;
  }
  if (MagicDifference(bytes) != 0) return std::nullopt;
  host.Dispatch(Command::kPlaySoundEffectProgram,
                {.first = program,
                 .second = Masked(effect_tag, kEffectTagMask),
                 .third = Masked(pan, kPanMask),
                 .fourth = Masked(volume, kVolumeMask)});
  return program;
}

// decomp: SLUS_010.40 func_80012288 (src/SLUS_010.40/25AC.c:381)
void StopSoundEffects(PlaybackHost& host, EffectSelector selector,
                      std::int32_t effect_tag_mask) {
  host.Dispatch(Command::kStopSoundEffects,
                {.first = static_cast<std::intptr_t>(selector),
                 .second = Masked(effect_tag_mask, kEffectTagMask)});
}

// decomp: SLUS_010.40 func_800122C4 (src/SLUS_010.40/25AC.c:388)
void PlayInterfaceSound(PlaybackHost& host, std::uint32_t effect_id) {
  host.Dispatch(
      Command::kPlayInterfaceSound,
      {.first = static_cast<std::intptr_t>(effect_id & kEffectIdMask)});
}

// decomp: SLUS_010.40 func_800122F0 (src/SLUS_010.40/25AC.c:394)
std::int32_t ActiveSoundEffectTags(PlaybackHost& host) {
  const std::uint32_t active = host.Status().active_voices;
  if (active == 0) return 0;
  std::uint32_t tags = 0;
  for (std::size_t voice = 0; voice < kEffectVoiceCount; ++voice) {
    if ((active & (kFirstEffectVoice << voice)) != 0) {
      tags |= static_cast<std::uint32_t>(host.EffectVoiceAt(voice).tags);
    }
  }
  return static_cast<std::int32_t>(tags & kEffectVoiceBits);
}

// decomp: SLUS_010.40 func_80012358 (src/SLUS_010.40/25AC.c:421)
EffectSelector SoundEffectIdForTag(PlaybackHost& host,
                                   std::int32_t effect_tag) {
  if (effect_tag == 0) return EffectSelector::kNone;
  const std::uint32_t active = host.Status().active_voices;
  if (active == 0) return EffectSelector::kNone;
  for (std::size_t voice = 0; voice < kEffectVoiceCount; ++voice) {
    if ((active & (kFirstEffectVoice << voice)) == 0) continue;
    const EffectVoice record = host.EffectVoiceAt(voice);
    if (record.tags == effect_tag) return record.effect;
  }
  return EffectSelector::kNone;
}

// decomp: SLUS_010.40 func_800123C8 (src/SLUS_010.40/25AC.c:439)
std::int32_t UsesSoundEffectTags(PlaybackHost& host, EffectSource source) {
  if (const auto* const program = std::get_if<DataRef>(&source)) {
    const std::span<const std::byte> bytes = host.ProgramBytes(*program);
    if (bytes.size() <= kProgramFlagsByte) {
      host.Refuse(PlaybackRefusal::kProgramUnreadable);
      return 0;
    }
    return std::to_integer<std::int32_t>(bytes[kProgramFlagsByte] >>
                                         kProgramUsesTagsShift);
  }
  const std::uint32_t id = std::get<SharedEffect>(source).id;
  const std::optional<EffectTables> tables = BoundTables(host);
  if (!tables) return 0;
  if (id >= tables->bank_selectors.size()) {
    host.Refuse(PlaybackRefusal::kEffectOutOfRange);
    return 0;
  }
  return (tables->bank_selectors[id] & kSharedEffectUsesTags) != 0 ? 1 : 0;
}

// decomp: SLUS_010.40 vs_sound_setMonoSound (src/SLUS_010.40/25AC.c:450)
void SetMonoOutput(PlaybackHost& host, std::int32_t mono_enabled) {
  host.Dispatch(mono_enabled == kMonoRequested ? Command::kMonoOutput
                                               : Command::kStereoOutput,
                {});
}
}  // namespace VS::Audio::Playback
