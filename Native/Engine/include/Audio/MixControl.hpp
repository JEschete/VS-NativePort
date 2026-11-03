#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqMusicSequence.hpp"
#include "Audio/SeqSequencer.hpp"
#include "Audio/SeqVoiceCommands.hpp"
#include "Audio/VoiceMixer.hpp"

// The mixer's driver commands (Sound_Cmd_A4_unk through func_80018C30,
// SLUS_010.40/25AC.c:3427-3900): sound-effect and music pitch scales, the
// tempo scale, music and sound-effect stops, pauses and resumes, the output
// mode, the reverb mode and the command dispatcher, over typed copies of the
// state they change. Music layer releases, volume invalidation, the voice
// output, the reverb unit, sequence memory and the command table go through
// the hosts.
namespace VS::Audio::Mixer {
// The sound-effect channels the world's effects play on (voices 12-23);
// effects tagged kFixedMixEffect are exempt from the world commands.
inline constexpr std::size_t kWorldEffectChannelCount = 12;
// The voice parameters a resumed channel writes again.
constexpr std::uint32_t ResumeDirtyFlags() {
  return Seq::kVolumeDirty | Seq::kSampleRateDirty | Seq::kAttackModeDirty |
         Seq::kSustainModeDirty | Seq::kAttackRateDirty |
         Seq::kSustainRateDirty;
}
inline constexpr std::uint32_t kResumeDirty = ResumeDirtyFlags();
// FSoundChannelConfig.statusFlags: a queued track waits for this layer.
inline constexpr std::int32_t kMusicQueued = 0x10000;
// FSoundGlobalFlags.controlLatches of a transition between the active and
// secondary layers: which layer is silenced and which resume is pending.
inline constexpr std::uint32_t kSecondarySilenced = 0x100;
inline constexpr std::uint32_t kActiveSilenced = 0x1000;
inline constexpr std::uint32_t kActiveResumePending = 0x400;
inline constexpr std::uint32_t kSecondaryResumePending = 0x4000;
inline constexpr std::uint32_t kTransitionLatches = 0x7700;
// FSoundGlobalFlags.mixBehavior output modes.
inline constexpr std::uint32_t kStereoOutput = 1;
inline constexpr std::uint32_t kMonoOutput = 2;
// g_CdStreamState.controlFlags: music or sound effects are paused.
inline constexpr std::int32_t kMusicPaused = 1;
inline constexpr std::int32_t kEffectsPaused = 2;
// The release a paused voice fades out with.
inline constexpr std::int32_t kPauseRate = 0x7F;
inline constexpr std::uint32_t kPauseAttackMode = 1;
inline constexpr std::uint32_t kPauseSustainMode = 3;

enum class CommandRefusal : std::uint8_t {
  // A slide's 16-bit length is zero while its 32-bit length is not; the
  // original divides by zero. The slide divides by one.
  kZeroLengthSlide,
  // A slide divides INT_MIN by -1; the step keeps INT_MIN.
  kSlideStepOverflow,
  // No music layer is active.
  kActiveLayerMissing,
  // Paused sound-effect voices lie below the first sound-effect channel; the
  // original never finishes its voice loop. Those voices are skipped.
  kEffectVoiceOutsideChannels,
  // A sequence argument lies outside the sequence memory.
  kSequenceUnreadable,
  // The command table has no handler for the command.
  kNoCommandHandler,
};

class CommandRefusalSink {
 public:
  virtual ~CommandRefusalSink() = default;
  virtual void Refuse(CommandRefusal refusal) = 0;
};

// A sound effect's identity (FSoundChannel.effectId, its program's native
// address): compared whole and never dereferenced.
enum class EffectId : std::uintptr_t {};

// The FSoundChannel fields the sound-effect commands read and write.
struct EffectControl {
  std::int32_t tags = 0;
  // The effect the channel plays; EffectId{} when none.
  EffectId effect_id{};
  std::uint32_t update_flags = 0;
  std::int32_t pitch_scale = 0;
  std::uint32_t pitch_scale_step = 0;
  std::uint16_t pitch_scale_length = 0;
  std::uint32_t dirty_flags = 0;

  friend bool operator==(const EffectControl&, const EffectControl&) = default;
};

// Everything the mixer's commands read or write besides the tempo and pitch
// slides.
struct CommandState {
  std::array<EffectControl, Seq::kEffectChannelCount> effects;
  // g_ActiveMusicChannels' dirty flags.
  std::array<std::uint32_t, Seq::kMusicChannelCount> music_dirty_flags{};
  // g_pActiveMusicConfig; empty when no layer is active.
  std::optional<Seq::MusicLayerState> active;
  // Whether g_pSecondaryMusicConfig names a layer.
  bool has_secondary = false;
  Seq::VoiceSchedulerState scheduler;
  std::uint32_t mix_behavior = 0;
  std::uint32_t driver_update_flags = 0;
  std::int32_t muted_music_channels = 0;
  // The cutscene stream's voices and pause flags.
  std::uint32_t stream_voices = 0;
  std::int32_t stream_control_flags = 0;
  // Whether a track waits to start (g_Sound_PendingMusicData.param1).
  bool music_queued = false;

  friend bool operator==(const CommandState&, const CommandState&) = default;
};

class ReverbHost {
 public:
  virtual ~ReverbHost() = default;
  virtual std::int32_t ReverbMode() = 0;
  virtual void EnableReverb(bool enabled) = 0;
  virtual void SetReverbMode(std::int32_t mode) = 0;
};

class ControlHost : public CommandRefusalSink {
 public:
  // Sound_KillMusicConfig over the active (kMusic) or secondary
  // (kSecondaryMusic) layer.
  virtual void ReleaseMusic(CommandState& state, Seq::ChannelGroup layer,
                            std::uint32_t music_id_filter) = 0;
  // Marks the volumes of the layer's playing channels dirty.
  virtual void InvalidateMusicVolumes(CommandState& state,
                                      Seq::ChannelGroup layer) = 0;
  // Marks the volumes of the sound-effect channels dirty.
  virtual void InvalidateSoundEffectVolumes(CommandState& state) = 0;
  virtual void SetVoiceVolume(std::uint32_t voice, std::uint32_t left,
                              std::uint32_t right, std::uint32_t scale) = 0;
  virtual void SetVoiceSampleRate(std::uint32_t voice, std::int32_t rate) = 0;
  virtual void SetVoiceAttack(std::uint32_t voice, std::int32_t rate,
                              std::uint32_t mode) = 0;
  virtual void SetVoiceSustain(std::uint32_t voice, std::int32_t rate,
                               std::uint32_t mode) = 0;
};

// The effects a sound-effect command selects (Audio_CommandParams param1 and
// param2): those carrying one of `tags`, or, without tags, those playing
// `effect`.
struct EffectSelection {
  EffectId effect{};
  std::uint32_t tags = 0;
};

// Sound_Cmd_A4_unk: the pitch scale (`pitch_word`'s low byte) of the selected
// world effects.
void SetSoundEffectPitchScale(CommandState& state, EffectSelection selection,
                              std::uint32_t pitch_word);
// Sound_Cmd_A5_unk: slides the selected world effects' pitch scales toward
// `target_word`'s low byte over `length_word`'s low 16 bits of ticks.
void SlideSoundEffectPitchScale(CommandState& state, EffectSelection selection,
                                std::uint32_t length_word,
                                std::uint32_t target_word,
                                CommandRefusalSink& sink);
// Sound_Cmd_AC_unk: every untagged world effect channel's pitch scale.
void SetWorldSoundEffectPitchScale(CommandState& state,
                                   const Seq::MusicCommand& command);
// Sound_Cmd_AD_unk.
void SlideWorldSoundEffectPitchScale(CommandState& state,
                                     const Seq::MusicCommand& command,
                                     CommandRefusalSink& sink);
// func_8001833C.
void SetTempoScale(Seq::LevelSlide& tempo_scale, std::int8_t scale);
// func_80018358.
void SlideTempoScale(Seq::LevelSlide& tempo_scale,
                     const Seq::MusicCommand& command,
                     CommandRefusalSink& sink);
// func_800183C0.
void SetTempoScaleRamp(Seq::LevelSlide& tempo_scale,
                       const Seq::MusicCommand& command,
                       CommandRefusalSink& sink);
// func_80018434.
void SetMusicPitchScale(Seq::LevelSlide& pitch_scale, std::int8_t scale);
// func_80018450.
void SlideMusicPitchScale(Seq::LevelSlide& pitch_scale,
                          const Seq::MusicCommand& command,
                          CommandRefusalSink& sink);
// func_800184B8.
void SetMusicPitchScaleRamp(Seq::LevelSlide& pitch_scale,
                            const Seq::MusicCommand& command,
                            CommandRefusalSink& sink);
// func_8001852C.
void StopMusicAndCancelReplacement(CommandState& state, ControlHost& host);
// Sound_Cmd_11_StopAllMusic.
void StopMatchingMusic(CommandState& state, const Seq::MusicCommand& command,
                       ControlHost& host);
// func_80018608.
void StopWorldSoundEffects(CommandState& state);
// Sound_Cmd_80_unk.
void EnableStereoOutput(CommandState& state, ControlHost& host);
// Sound_Cmd_81_unk.
void EnableMonoOutput(CommandState& state, ControlHost& host);
// Sound_Cmd_90_unk.
void SetMusicMuteMask(CommandState& state, const Seq::MusicCommand& command);
// Sound_Cmd_92_unk.
void SetMusicJumpThreshold(CommandState& state,
                           const Seq::MusicCommand& command,
                           CommandRefusalSink& sink);
// Sound_Cmd_9B_unk.
void PauseMusic(CommandState& state, ControlHost& host);
// Sound_Cmd_9A_unk.
void ResumeMusic(CommandState& state, CommandRefusalSink& sink);
// Sound_Cmd_9D_unk.
void PauseWorldSoundEffects(CommandState& state, ControlHost& host);
// Sound_Cmd_9C_unk.
void ResumeWorldSoundEffects(CommandState& state, CommandRefusalSink& sink);
// Sound_Cmd_11_Nop.
void IgnoreCommand();
// Sound_SetReverbMode.
void SetReverbMode(ReverbHost& host, std::int32_t mode);

// The fields of an AKAO sequence header the dispatcher reads.
struct AkaoHeader {
  std::uint16_t sequence_id = 0;
  std::uint16_t reverb_mode = 0;

  friend bool operator==(const AkaoHeader&, const AkaoHeader&) = default;
};

inline constexpr std::size_t kAkaoHeaderBytes = 10;

// The header of the sequence `bytes` starts with; empty when they hold no
// AKAO header.
[[nodiscard]] std::optional<AkaoHeader> ReadAkaoHeader(
    std::span<const std::uint8_t> bytes);

// Command words 2-6 of D_800378C0 and Audio_CommandParams; word 1 is a
// sequence pointer and stays with the host.
struct CommandWords {
  std::uint32_t param2 = 0;
  std::uint32_t param3 = 0;
  std::uint32_t param4 = 0;
  std::uint32_t ext_param1 = 0;
  std::uint32_t ext_param2 = 0;

  friend bool operator==(const CommandWords&, const CommandWords&) = default;
};

// Everything the dispatcher reads or writes itself.
struct DispatchState {
  CommandWords arguments;
  CommandWords parameters;
  std::uint32_t control_latches = 0;
  // g_pActiveMusicConfig; empty when no layer is active.
  std::optional<Seq::MusicLayerState> active;
  // g_pSecondaryMusicConfig; empty when there is none or when it names the
  // active layer's record.
  std::optional<Seq::MusicLayerState> secondary;

  friend bool operator==(const DispatchState&, const DispatchState&) = default;
};

enum class ArgumentSlot : std::uint8_t { kFirst, kSecond };

class DispatchHost : public ReverbHost, public CommandRefusalSink {
 public:
  // DisableEvent / EnableEvent around the command.
  virtual void BeginCommand() = 0;
  virtual void EndCommand() = 0;
  // The header of the sequence argument `slot` names, as the command
  // started; empty when it names no AKAO sequence.
  virtual std::optional<AkaoHeader> SequenceAt(ArgumentSlot slot) = 0;
  // Audio_CommandParams.param1 = D_800378C0.param1.
  virtual void CopyFirstArgument() = 0;
  // g_Sound_PendingMusicData.param1 = D_800378C0.param1.
  virtual void QueueFirstSequence() = 0;
  // D_800378C0.param1 = `music_id`.
  virtual void ReplaceFirstArgument(std::uint32_t music_id) = 0;
  // Sound_LoadAkaoSequence into the active layer.
  virtual void LoadSequence(DispatchState& state, ArgumentSlot slot) = 0;
  // Copies g_ActiveMusicChannels and g_PrimaryMusicConfig to the saved
  // layer.
  virtual void SaveActiveLayer(DispatchState& state) = 0;
  // Makes the saved layer the secondary layer.
  virtual void AttachSavedLayer(DispatchState& state) = 0;
  // Runs the command table's handler for `command`.
  virtual void RunHandler(DispatchState& state, std::uint32_t command) = 0;
};

// func_80018C30: the command's result (a track's id, 0 or 0xFFFFFFFF).
std::uint32_t DispatchCommand(DispatchState& state, DispatchHost& host,
                              std::uint32_t command);
}  // namespace VS::Audio::Mixer
