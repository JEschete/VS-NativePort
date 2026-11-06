#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqSequencer.hpp"
#include "Audio/SeqVoiceCommands.hpp"
#include "Audio/SoundPlayback.hpp"
#include "Audio/VoiceMixer.hpp"

// The sound-effect programs (Sound_EvictSfxVoice through Sound_Cmd_AB_unk,
// SLUS_010.40/25AC.c:2539-3425): starting an effect's one or two programs on
// a free pair of sound-effect channels, releasing the channels a stop names,
// the play, stop, volume and pan commands, and the volume invalidation the
// mixer and sequencer request. The commands run over typed copies of the
// channels, the voice scheduler and the active music layer's voices; channel
// resets, voice owners and releases, the shared tables, the bank slots and
// the program bytes go through EffectHost.
namespace VS::Audio::SoundEffects {
using Playback::DataRef;
using Playback::EffectSelector;

// The channels voice bits 12-31 name, and the twelve the commands walk.
inline constexpr std::size_t kChannelCount = Seq::kEffectChannelCount;
inline constexpr std::size_t kCommandChannelCount = Playback::kEffectVoiceCount;
inline constexpr std::uint32_t kFirstVoiceBit = Playback::kFirstEffectVoice;

// Sound_EvictSfxVoice's flags: any of the low 28 bits releases the voices
// carrying one of those tags; a negative word releases the effects of the
// channel pair the selector numbers; kEvictUntagged releases the untagged
// voices of the highest priority.
inline constexpr std::int32_t kReleaseTagMask = 0x0FFFFFFF;
inline constexpr std::int32_t kEvictUntagged = 0x40000000;
// Release selectors without flags: every program effect (a channel whose
// EffectKind is kProgram), and every untagged effect.
inline constexpr EffectSelector kProgramEffects{-1};
inline constexpr EffectSelector kUntaggedEffects{-2};

// What a channel's effect identity names. The original stored a shared
// effect id or a blob's address in one word and told them apart by address
// (a program lies at or above 0x80000000); the channel names its kind.
enum class EffectKind : std::uint8_t {
  kShared,
  // Command 0x24's blob (func_8001733C).
  kProgram,
};

// The tags and levels the interface commands (0x30, 0x34) play with.
// Command 0x30 tags its effect kFixedMixEffect, which the scene commands
// skip.
inline constexpr std::uint32_t kInterfaceEffectTag{Mixer::kFixedMixEffect};
inline constexpr std::uint32_t kInterfacePairTag = 0x01000000;
inline constexpr EffectSelector kInterfacePairEffect{0x400};
inline constexpr std::uint32_t kCenterPan = 0x80;
inline constexpr std::uint32_t kFullVolume = 0x7F;

// func_8001653C's channel start.
inline constexpr std::uint16_t kCenteredChannelPan = 0x8000;
inline constexpr std::uint16_t kStartNoteLength = 2;
inline constexpr std::uint16_t kStartGateLength = 1;
inline constexpr std::uint16_t kEffectChannelType = 1;
inline constexpr std::int32_t kStartPriority = -2;
inline constexpr std::int32_t kStartReleaseRate = 5;
inline constexpr std::uint32_t kStartReleaseMode = 3;
// The channel update flag of a pair's second channel.
inline constexpr std::uint32_t kStereoLinked = 0x10000;
// The driver update a release or a start requests.
inline constexpr std::uint32_t kVoiceUpdate(Seq::kVoiceModeUpdate |
                                            Seq::kNoiseClockUpdate);

// A program pair's channels: the six even channels from channel 10 down.
inline constexpr std::size_t kPairChannelCount = 2;
inline constexpr std::size_t kLastPairChannel = 10;

// An absent program in the shared offsets and in a blob's entries.
inline constexpr std::uint16_t kNoProgram = 0xFFFF;

// func_8001733C's blob: the program count word, the bank selector word, the
// entry offsets from byte 0x10 and the entries from byte 0x20. An entry's
// programs start after its two offset halfwords.
inline constexpr std::size_t kBlobCountWord = 1;
inline constexpr std::size_t kBlobSelectorWord = 2;
inline constexpr std::size_t kBlobOffsetsStart = 0x10;
inline constexpr std::size_t kBlobEntriesStart = 0x20;
inline constexpr std::size_t kBlobEntryHeaderBytes = 4;

enum class EffectRefusal : std::uint8_t {
  // A negative release names a channel outside the voice bits, or a start
  // names a channel past the sound-effect channels.
  kChannelOutOfRange,
  // A fade's 16-bit length is zero while its 32-bit length is not; the
  // original divides by zero. The fade runs over one tick, as a zero length
  // does.
  kZeroLengthFade,
  // No shared tables are bound.
  kTablesUnbound,
  // A shared effect id lies past the bound tables.
  kEffectOutOfRange,
  // A shared program's offset lies past the program data.
  kProgramOutOfRange,
  // A blob's header, offsets or entries lie outside its bytes.
  kBlobUnreadable,
  // A blob counts no programs after its first; the original counts down
  // through every 32-bit value.
  kBlobCountInvalid,
  // A music voice is detached while no music layer is active.
  kNoActiveMusic,
  // A started channel's voice lies past the voices; the original writes
  // past the voice owner table.
  kVoiceOutOfRange,
  // Active voices lie below the first sound-effect voice; the original never
  // finishes its voice loop. Those voices are skipped.
  kVoiceOutsideEffects,
};

// One sound-effect channel's fields the programs read or write
// (FSoundChannel).
struct EffectChannel {
  EffectSelector effect = EffectSelector::kNone;
  EffectKind kind = EffectKind::kShared;
  std::int32_t tags = 0;
  std::uint32_t update_flags = 0;
  std::int32_t priority = 0;
  std::int32_t voice_mask = 0;
  std::uint16_t type = 0;
  std::uint16_t note_length = 0;
  std::uint16_t gate_length = 0;
  std::uint16_t pan = 0;
  std::uint16_t pan_slide_length = 0;
  std::uint16_t effect_pan = 0;
  std::int16_t effect_pan_step = 0;
  std::uint16_t effect_pan_length = 0;
  std::int16_t effect_volume = 0;
  std::int16_t effect_volume_step = 0;
  std::uint16_t effect_volume_length = 0;
  std::int32_t pitch_scale = 0;
  std::uint16_t pitch_scale_length = 0;
  std::uint32_t assigned_voice = 0;
  std::uint32_t dirty_flags = 0;

  friend bool operator==(const EffectChannel&, const EffectChannel&) = default;
};

// Everything the programs read or write besides the program bytes.
struct EffectState {
  std::array<EffectChannel, kChannelCount> channels{};
  Seq::VoiceSchedulerState scheduler;
  // The cutscene stream's voices and pause flags.
  std::uint32_t stream_voices = 0;
  std::int32_t stream_control_flags = 0;
  std::uint32_t driver_update_flags = 0;
  // The music channels' assigned voices, and the active layer's notes
  // (empty when no layer is active).
  std::array<std::uint32_t, Seq::kMusicChannelCount> music_voices{};
  std::optional<std::uint32_t> active_music_notes;

  friend bool operator==(const EffectState&, const EffectState&) = default;
};

// FSoundCommandParams: the effect, its tags, pan and volume and the bank
// slot its samples use (extParam1).
struct EffectCommand {
  EffectSelector first = EffectSelector::kNone;
  // What `first` names; the channels the command starts take this kind.
  EffectKind first_kind = EffectKind::kShared;
  std::uint32_t second = 0;
  std::uint32_t third = 0;
  std::uint32_t fourth = 0;
  std::uint32_t bank_slot = 0;

  friend bool operator==(const EffectCommand&, const EffectCommand&) = default;
};

// Where a channel's program starts: `offset` bytes into `data`.
struct ProgramStart {
  DataRef data = DataRef::kNone;
  std::size_t offset = 0;

  friend bool operator==(const ProgramStart&, const ProgramStart&) = default;
};

// An effect's one or two programs.
struct ProgramPair {
  std::optional<ProgramStart> first = std::nullopt;
  std::optional<ProgramStart> second = std::nullopt;

  friend bool operator==(const ProgramPair&, const ProgramPair&) = default;
};

// The bound shared tables (Sound_BindAkaoSfxBlob) and the program data their
// offsets count from.
struct SharedTables {
  std::span<const std::uint32_t> program_offsets = {};
  std::span<const std::uint16_t> bank_selectors = {};
  DataRef program_data = DataRef::kNone;
  std::size_t program_bytes = 0;
};

class EffectHost {
 public:
  virtual ~EffectHost() = default;
  // func_80015C9C: points `channel` (channel `index`) at `program` and
  // resets its sequence state.
  virtual void ResetChannel(std::size_t index, EffectChannel& channel,
                            ProgramStart program) = 0;
  // g_Sound_VoiceChannelConfigs[voice] = NULL: the voice no longer belongs
  // to a music layer.
  virtual void ClearVoiceOwner(std::uint32_t voice) = 0;
  virtual void SetVoiceRelease(std::uint32_t voice, std::int32_t rate,
                               std::uint32_t mode) = 0;
  // The bound shared tables, or nullopt when none are bound.
  virtual std::optional<SharedTables> Tables() = 0;
  // The sound-effect bank slot a sample bank selector names.
  virtual std::uint32_t BankSlot(std::int32_t selector) = 0;
  // The bytes from `blob` to the end of the memory it lies in; empty when
  // none can be read.
  virtual std::span<const std::byte> ProgramBytes(DataRef blob) = 0;
  virtual void Refuse(EffectRefusal refusal) = 0;
};

// Sound_EvictSfxVoice: releases the active or paused channels `flags` and
// `selector` name; a channel finishing its loop first stops at its loop end.
void ReleaseMatchingVoices(EffectState& state, EffectHost& host,
                           EffectSelector selector, std::int32_t flags);
// func_8001653C: starts channel `index` (voice bit `voice_bit`) on `program`
// with the command's effect, tags, pan, volume and bank slot.
void StartChannelProgram(EffectState& state, EffectHost& host,
                         std::size_t index, const EffectCommand& command,
                         std::uint32_t voice_bit, ProgramStart program);
// FreeVoiceChannels: takes voice `voice` from the music channels assigned it.
void DetachMusicVoice(EffectState& state, EffectHost& host,
                      std::uint32_t voice);
// Sound_PlaySfxProgram: starts the programs on the highest free channel pair,
// evicting untagged voices until one is free or none more can be evicted.
void PlayProgramPair(EffectState& state, EffectHost& host,
                     const EffectCommand& command, const ProgramPair& programs,
                     bool skip_release);
// Sound_GetProgramCounters: shared effect `effect_id`'s (its low 10 bits)
// programs. An id past the bound tables, unbound tables or an offset past the
// program data is refused as no program.
ProgramPair ProgramCounters(EffectHost& host, std::uint32_t effect_id);
// Sound_MarkActiveChannelsVolumeDirty: marks the volume of every music
// channel `active_channels` names.
void InvalidateMusicChannelVolumes(
    std::uint32_t active_channels,
    std::span<std::uint32_t, Seq::kMusicChannelCount> dirty_flags);
// Sound_MarkScheduledSfxChannelsVolumeDirty: marks the volume of every
// active sound-effect channel.
void InvalidateSoundEffectVolumes(EffectState& state, EffectHost& host);

// The sound-effect commands (Audio_Mixer_kCommandHandlers).
// Sound_Cmd_34_unk (0x34): the two programs `first` and `second` name.
void PlayInterfaceProgramPair(EffectState& state, EffectHost& host,
                              EffectCommand& command);
// Sound_Cmd_30_unk (0x30): shared effect `first`, centered at full volume.
void PlayInterfaceProgram(EffectState& state, EffectHost& host,
                          EffectCommand& command);
// Sound_Cmd_20_unk (0x20): shared effect `first`.
void PlaySharedProgram(EffectState& state, EffectHost& host,
                       EffectCommand& command);
// func_8001733C (0x24): every program pair of blob `first`.
void PlayBlobPrograms(EffectState& state, EffectHost& host,
                      EffectCommand& command);
// Sound_Cmd_21_unk (0x21).
void StopPrograms(EffectState& state, EffectHost& host,
                  const EffectCommand& command);
// Sound_Cmd_A0_unk (0xA0) and Sound_Cmd_A2_unk (0xA2): the volume or pan of
// the channels carrying a tag of `second`, or without tags, playing `first`.
void SetProgramVolume(EffectState& state, const EffectCommand& command);
void SetProgramPan(EffectState& state, const EffectCommand& command);
// Sound_Cmd_A1_unk (0xA1) and Sound_Cmd_A3_unk (0xA3): fades over `third`
// ticks toward `fourth`.
void FadeProgramVolume(EffectState& state, EffectHost& host,
                       const EffectCommand& command);
void FadeProgramPan(EffectState& state, EffectHost& host,
                    const EffectCommand& command);
// Sound_Cmd_A8_unk (0xA8) and Sound_Cmd_AA_unk (0xAA): the scene effects
// (those without kFixedMixEffect) at `first`'s volume or pan.
void SetSceneVolume(EffectState& state, const EffectCommand& command);
void SetScenePan(EffectState& state, const EffectCommand& command);
// Sound_Cmd_A9_unk (0xA9) and Sound_Cmd_AB_unk (0xAB): fades over `first`
// ticks toward `second`.
void FadeSceneVolume(EffectState& state, EffectHost& host,
                     const EffectCommand& command);
void FadeScenePan(EffectState& state, EffectHost& host,
                  const EffectCommand& command);
}  // namespace VS::Audio::SoundEffects
