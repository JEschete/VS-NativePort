#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>

#include "Audio/SeqControlCommands.hpp"

// The sound playback front end (InitSound through vs_sound_setMonoSound,
// SLUS_010.40/25AC.c:195-450): music and sound-effect requests through the
// mixer's command dispatch, playback queries over the music layers and the
// sound-effect voices, and the shared sound-effect program tables. The sound
// driver, the dispatch, the layers, the voices and the tables go through
// PlaybackHost.
namespace VS::Audio::Playback {
// The mixer commands the front end issues (func_80018C30's command words).
enum class Command : std::uint8_t {
  kStartMusic = 0x10,
  kStopMusic = 0x11,
  kStartMusicAtLoop = 0x12,
  kQueueMusicTransition = 0x14,
  kStartMusicPair = 0x16,
  kSwapMusicLayers = 0x17,
  kStartLayeredMusic = 0x19,
  kCrossfadeMusic = 0x1A,
  kPlaySoundEffect = 0x20,
  kStopSoundEffects = 0x21,
  kPlaySoundEffectProgram = 0x24,
  kPlayInterfaceSound = 0x30,
  kSaveMusic = 0x40,
  kStereoOutput = 0x80,
  kMonoOutput = 0x81,
};

// A sequence or effect program in memory, as an opaque handle: the core
// hands it to the host and never dereferences it.
enum class DataRef : std::uintptr_t { kNone = 0 };

// What a sound-effect voice plays, or what to stop: a shared effect id, a
// program's address or one of the original's negative selectors.
enum class EffectSelector : std::intptr_t { kNone = 0 };

// The shared effect tables Sound_BindAkaoSfxBlob binds: one program offset
// word and one bank selector halfword per effect id.
inline constexpr std::size_t kEffectTableEntries = 0x180;
inline constexpr std::uint32_t kEffectIdMask = 0x3FF;
inline constexpr std::uint32_t kNoEffectProgram = 0xFFFFFFFF;
// A bank selector with this bit marks an effect that uses its voice tags.
inline constexpr std::uint16_t kSharedEffectUsesTags = 0x8000;
// The blob: its AKAO header, the offsets, the selectors, then the programs.
inline constexpr std::size_t kEffectBlobHeaderBytes = 0x10;
inline constexpr std::size_t kEffectOffsetBytes(sizeof(std::uint32_t) *
                                                kEffectTableEntries);
inline constexpr std::size_t kEffectSelectorBytes(sizeof(std::uint16_t) *
                                                  kEffectTableEntries);
inline constexpr std::size_t kEffectProgramDataStart(kEffectBlobHeaderBytes +
                                                     kEffectOffsetBytes +
                                                     kEffectSelectorBytes);
// "AKAO" read as a little-endian word.
inline constexpr std::uint32_t kAkaoMagic = 0x4F414B41;
// An effect program's flags byte; its top bit marks a program that uses its
// voice tags.
inline constexpr std::size_t kProgramFlagsByte = 9;
inline constexpr unsigned kProgramUsesTagsShift = 7;

// The command argument masks.
inline constexpr std::uint32_t kEffectTagMask = 0xFFFFFF;
inline constexpr std::uint32_t kPanMask = 0xFF;
inline constexpr std::uint32_t kVolumeMask = 0x7F;

// The sound-effect voices: twelve voices from voice bit 12.
inline constexpr std::size_t kEffectVoiceCount = 12;
inline constexpr std::uint32_t kFirstEffectVoice = 0x1000;

// func_80011E40's playback flags.
inline constexpr std::int32_t kActiveLayerPlaying = 1;
inline constexpr std::int32_t kSecondaryLayerPlaying = 2;
inline constexpr std::int32_t kTransitionQueued = 0x10;
inline constexpr std::int32_t kEffectVoicesPlaying = 0x100;
inline constexpr std::int32_t kStreamPlaying = 0x10000;

// func_80011F24's fade selectors.
inline constexpr std::int32_t kMasterVolumeFade = 1;
inline constexpr std::int32_t kTempoFade = 2;
inline constexpr std::int32_t kPitchFade = 4;

// vs_sound_setMonoSound's mono request.
inline constexpr std::int32_t kMonoRequested = 1;

enum class PlaybackRefusal : std::uint8_t {
  kEffectTablesUnbound,
  kEffectOutOfRange,
  kProgramUnreadable,
  kBlobTooSmall,
  kBlobMisaligned,
};

// A command's first argument: none, a sequence or program, or a number (an
// id, a music id or an effect selector).
using FirstArgument = std::variant<std::monostate, DataRef, std::intptr_t>;

// D_800378C0: the arguments of one command. A command writes only the words
// it passes; the rest keep their earlier values. kStartMusicPair passes its
// second sequence as `second_sequence` instead of the 32-bit `second` word.
struct CommandArguments {
  FirstArgument first{};
  std::optional<std::uint32_t> second{};
  std::optional<std::uint32_t> third{};
  std::optional<std::uint32_t> fourth{};
  DataRef second_sequence = DataRef::kNone;

  friend bool operator==(const CommandArguments&,
                         const CommandArguments&) = default;
};

// The command's result, and after kStartMusicPair the two sequences' music
// ids (D_800378C0.param1 and param2).
struct CommandResult {
  std::uint32_t value = 0;
  std::uint32_t first = 0;
  std::uint32_t second = 0;
};

struct MusicPair {
  std::uint32_t first = 0;
  std::uint32_t second = 0;

  friend bool operator==(const MusicPair&, const MusicPair&) = default;
};

// The music sample bank ids (D_80036770).
struct MusicBankIds {
  std::int32_t bank = 0;
  std::int32_t alternate = 0;
};

// The playback state the queries read.
struct PlaybackStatus {
  Seq::MusicLayerState active;
  std::optional<Seq::MusicLayerState> secondary;
  bool transition_queued = false;
  std::uint32_t active_voices = 0;
  std::int32_t stream_voices = 0;
  std::int16_t tempo_slide_length = 0;
  std::int16_t pitch_slide_length = 0;
};

// One sound-effect voice's tags and the effect it plays.
struct EffectVoice {
  std::int32_t tags = 0;
  EffectSelector effect = EffectSelector::kNone;
};

// The bound shared effect tables; each holds one entry per effect id it
// covers (kEffectTableEntries for a bound blob).
struct EffectTables {
  std::span<const std::uint32_t> program_offsets;
  std::span<const std::uint16_t> bank_selectors;
};

// An AKAO effect blob split into its tables.
struct EffectProgramBlob {
  std::span<std::byte> program_offsets;
  std::span<std::byte> bank_selectors;
  std::span<std::byte> program_data;
};

// A shared effect, by id into the bound tables.
struct SharedEffect {
  std::uint32_t id = 0;
};
// The effect UsesSoundEffectTags asks about: a shared effect or a program.
// The original told them apart by address (a program lies at or above
// 0x80000000); the caller names which it passes.
using EffectSource = std::variant<SharedEffect, DataRef>;

class PlaybackHost {
 public:
  virtual ~PlaybackHost() = default;

  virtual void StartDriver() = 0;
  virtual void StopDriver() = 0;
  // Stores the arguments and runs `command` through the mixer's dispatch.
  virtual CommandResult Dispatch(Command command,
                                 const CommandArguments& arguments) = 0;
  // func_8001286C (SoundControl.hpp SetMusicVolume).
  virtual void SetMusicVolume(std::uint32_t music_id, std::int32_t volume) = 0;

  virtual PlaybackStatus Status() = 0;
  // Voice `voice` (below kEffectVoiceCount) of the sound-effect voices.
  virtual EffectVoice EffectVoiceAt(std::size_t voice) = 0;
  // The bytes of the program at `data` the host can read; empty when none.
  virtual std::span<const std::byte> ProgramBytes(DataRef data) = 0;

  // The bound tables, or nullopt when none are bound.
  virtual std::optional<EffectTables> Tables() = 0;
  // Binds the blob's tables; false when the host cannot read them as
  // words and halfwords.
  virtual bool BindTables(const EffectProgramBlob& blob) = 0;

  virtual void ClearSavedMusicId() = 0;
  virtual MusicBankIds MusicBanks() = 0;

  virtual void Refuse(PlaybackRefusal refusal) = 0;
};

std::int32_t Initialize(PlaybackHost& host);
std::int32_t Shutdown(PlaybackHost& host);
// Sound_BindAkaoSfxBlob: returns the blob's AKAO magic difference and binds
// its tables when it is zero. A blob shorter than its tables, or one the
// host cannot read, is refused and leaves the tables as they were.
std::int32_t BindSoundEffectPrograms(PlaybackHost& host,
                                     std::span<std::byte> blob);

std::int32_t ActivePlaybackFlags(PlaybackHost& host);
std::int32_t MusicVolume(PlaybackHost& host, std::uint32_t music_id);
std::int32_t PackedMusicSampleBankIds(PlaybackHost& host);
std::int32_t LongestMusicFade(PlaybackHost& host, std::int32_t fade_selectors);
std::int32_t MusicPosition(PlaybackHost& host);

std::uint32_t StartMusic(PlaybackHost& host, DataRef sequence);
void StopMusic(PlaybackHost& host, std::uint32_t music_id);
void SaveMusicState(PlaybackHost& host);
std::int32_t InvalidateSavedMusicId(PlaybackHost& host);
std::uint32_t StartLayeredMusic(PlaybackHost& host, DataRef sequence,
                                std::int32_t volume);
std::uint32_t CrossfadeMusic(PlaybackHost& host, DataRef sequence,
                             std::uint32_t fade_ticks,
                             std::int32_t target_volume);
void StartMusicAtLoop(PlaybackHost& host, DataRef sequence,
                      std::int32_t loop_count);
std::uint32_t QueueMusicTransition(PlaybackHost& host, DataRef sequence);
MusicPair StartMusicPair(PlaybackHost& host, DataRef primary,
                         DataRef secondary);
void RequestMusicLayerSwap(PlaybackHost& host);

// Returns the effect's program offset, kNoEffectProgram (as -1) when it has
// none. An id past the bound tables, or unbound tables, is refused as none.
std::int32_t PlaySoundEffect(PlaybackHost& host, std::uint32_t effect_id,
                             std::int32_t effect_tag, std::int32_t pan,
                             std::int32_t volume);
// Returns the program, or nullopt when it is not AKAO (the original's -1).
std::optional<DataRef> PlaySoundEffectBlob(PlaybackHost& host, DataRef program,
                                           std::int32_t effect_tag,
                                           std::int32_t pan,
                                           std::int32_t volume);
void StopSoundEffects(PlaybackHost& host, EffectSelector selector,
                      std::int32_t effect_tag_mask);
void PlayInterfaceSound(PlaybackHost& host, std::uint32_t effect_id);
std::int32_t ActiveSoundEffectTags(PlaybackHost& host);
EffectSelector SoundEffectIdForTag(PlaybackHost& host, std::int32_t effect_tag);
// 1 when the effect uses its voice tags. A shared id past the bound tables,
// unbound tables or an unreadable program is refused as 0.
std::int32_t UsesSoundEffectTags(PlaybackHost& host, EffectSource source);
void SetMonoOutput(PlaybackHost& host, std::int32_t mono_enabled);
}  // namespace VS::Audio::Playback
