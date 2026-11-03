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
#include "Audio/VoiceOutput.hpp"

// Starting, stopping, saving, restoring and fading music tracks
// (SLUS_010.40/25AC.c:2330-3237). Channel program positions are byte offsets
// from the start of the track's AKAO sequence, so moving a saved track to new
// sequence data leaves them unchanged.
namespace VS::Audio::Seq {
inline constexpr std::size_t kSequenceHeaderBytes = 0x40;

// One channel's playback record (FSoundChannel).
struct SequenceChannel {
  ChannelState channel;
  ProgramState program;
  PitchSlideState pitch_slide;
  std::uint32_t assigned_voice = kUnassignedVoice;
  // The channel runs the driver's idle program instead of `program.counter`.
  bool idle = false;

  friend bool operator==(const SequenceChannel&,
                         const SequenceChannel&) = default;
};

using MusicChannels = std::array<SequenceChannel, kMusicChannelCount>;

// The AKAO sequence header fields a track start reads
// (FAkaoSequence, src/include/sound.h:444).
struct SequenceHeader {
  std::uint32_t sample_bank_id = 0;
  std::uint32_t channel_enable_mask = 0;
  std::uint32_t keyed_mask = 0;
  std::uint32_t allocated_voice_mask = 0;
  std::int32_t patch_region_offset = 0;
  std::int32_t keymap_region_offset = 0;
  std::uint32_t resume_key_on_mask = 0;
  std::uint32_t push_release_mask = 0;

  friend bool operator==(const SequenceHeader&,
                         const SequenceHeader&) = default;
};

// The header at the start of `sequence`; empty when the data is shorter.
std::optional<SequenceHeader> ReadSequenceHeader(
    std::span<const std::uint8_t> sequence);

// The driver state a track start or restore reads and writes.
struct MusicStartState {
  MusicLayerState layer;
  MusicChannels channels;
  VoiceSchedulerState scheduler;
  std::uint32_t driver_update_flags = 0;
  std::int32_t stream_control_flags = 0;
  std::uint32_t stream_voices = 0;
  std::uint32_t alternate_bank_id = 0;
  // The voices the secondary layer's channels hold; 0 without one.
  std::uint32_t secondary_voices = 0;
  // The secondary layer's reverb depth; empty without one.
  std::optional<std::int32_t> secondary_reverb_depth;
  std::uint16_t saved_music_id = 0;

  friend bool operator==(const MusicStartState&,
                         const MusicStartState&) = default;
};

enum class LoadStatus : std::uint8_t {
  kLoaded,
  // The sequence data ends inside its header or channel offset table.
  kHeaderOutside,
};

// Where a loaded track's patch and drum keymap tables start, as offsets from
// the start of its sequence; empty when the sequence has none.
struct SequenceTablePositions {
  std::optional<std::int64_t> patch_table;
  std::optional<std::int64_t> keymap_table;

  friend bool operator==(const SequenceTablePositions&,
                         const SequenceTablePositions&) = default;
};

struct LoadResult {
  LoadStatus status = LoadStatus::kLoaded;
  SequenceTablePositions tables;
  // Enabled channels whose program starts outside the sequence; they run
  // the idle program.
  std::uint32_t channels_outside = 0;

  friend bool operator==(const LoadResult&, const LoadResult&) = default;
};

// func_80015C9C: points a sound-effect channel at `program_start` and resets
// it to `first_instrument` (instrument 0).
void ResetSoundEffectChannel(SequenceChannel& channel,
                             std::uint32_t program_start,
                             const Samples::InstrumentRecord& first_instrument);
// func_80015D38: the voices the channels in `channel_mask` hold.
std::uint32_t AssignedVoiceMask(
    std::span<const std::uint32_t, kMusicChannelCount> assigned_voices,
    std::uint32_t channel_mask);
// Sound_LoadAkaoSequence: starts the track in `sequence` on the active layer.
// A sequence too short for its header and channel offsets changes nothing.
LoadResult LoadMusicSequence(MusicStartState& state,
                             std::span<const std::uint8_t> sequence,
                             const Samples::InstrumentRecord& first_instrument);
// Sound_KillMusicConfig: stops the layer's track when `music_id_filter` is 0
// or names it. Returns the voices to release, `owned_voices`, when it stopped.
std::optional<std::uint32_t> ReleaseMusicConfiguration(
    MusicLayerState& layer, MusicChannels& channels, std::uint32_t owned_voices,
    std::uint32_t music_id_filter);
// Sound_SetMusicSequence: resumes the restored track on sequence data with
// header `header`, keeping the channels' program positions. With
// `swap_with_saved` the reverb depth slides from the secondary layer's.
// Returns the channels whose instruments move to the alternate bank.
std::uint32_t RestoreMusicSequence(MusicStartState& state,
                                   const SequenceHeader& header,
                                   bool swap_with_saved);

// A music driver command's parameters (Audio_CommandParams); `param1` is the
// track's sequence for the start commands.
struct MusicCommand {
  std::uint32_t param1 = 0;
  std::uint32_t param2 = 0;
  std::uint32_t param3 = 0;
  std::uint32_t param4 = 0;
  std::uint32_t ext_param1 = 0;
  std::uint32_t ext_param2 = 0;
};

// The driver state the music commands read and write.
struct MusicCommandState {
  MusicLayerState active;
  // The layer a crossfade or saved track plays beside the active one.
  std::optional<MusicLayerState> secondary;
  // The saved layer's track.
  std::uint16_t saved_music_id = 0;
  std::uint32_t mix_behavior = 0;
  FadeTimer master_fade;
  std::uint32_t music_loops_remaining = 0;
  LevelSlide disc_volume;

  friend bool operator==(const MusicCommandState&,
                         const MusicCommandState&) = default;
};

enum class MusicRefusal : std::uint8_t {
  // A crossfade of zero ticks; it runs over one tick instead.
  kZeroLengthCrossfade,
};

// Every effect that takes the state sees it current and leaves it current.
class MusicHost {
 public:
  virtual ~MusicHost() = default;
  // Starts the command's sequence on the active layer (LoadMusicSequence).
  virtual void LoadSequence(MusicCommandState& state) = 0;
  // Resumes the saved track on the command's sequence
  // (RestoreMusicSequence).
  virtual void RestoreSequence(MusicCommandState& state,
                               bool swap_with_saved) = 0;
  // Copies the active layer and its channels into the saved layer.
  virtual void SaveSnapshot(MusicCommandState& state) = 0;
  // Moves the saved channels' instruments back to the base sample bank.
  virtual void MapSavedInstrumentsToBaseBank(MusicCommandState& state) = 0;
  // Makes the saved layer the secondary layer.
  virtual void AttachSavedLayer(MusicCommandState& state) = 0;
  virtual void ReleaseSavedVoiceConflicts(MusicCommandState& state) = 0;
  virtual void InvalidateMusicVolumes(MusicCommandState& state,
                                      ChannelGroup group) = 0;
  virtual void WriteDiscVolume(MusicCommandState& state) = 0;
  virtual void Refuse(MusicRefusal refusal) = 0;
};

// Sound_Cmd_10_StartFieldMusic.
void StartOrResumeMusic(MusicCommandState& state, MusicHost& host,
                        const MusicCommand& command);
// Sound_Cmd_40_PushMusicState.
void SaveCurrentMusic(MusicCommandState& state, MusicHost& host);
// Sound_Cmd_19_Unk: starts a track and keeps the current one as the
// secondary layer.
void StartMusicLayer(MusicCommandState& state, MusicHost& host,
                     const MusicCommand& command);
// Sound_Cmd_1A_Unk: crossfades from the current track to the command's.
void CrossfadeMusicLayers(MusicCommandState& state, MusicHost& host,
                          const MusicCommand& command);
// Sound_Cmd_12_unk.
void StartMusicWithLoopCounter(MusicCommandState& state, MusicHost& host,
                               const MusicCommand& command);
// Sound_Cmd_C0_unk, Sound_Cmd_C1_unk and Sound_Cmd_C2_unk: the master volume
// of the layer playing track `param1` (0 for the active one).
void SetMusicVolume(MusicCommandState& state, MusicHost& host,
                    const MusicCommand& command);
void FadeMusicVolume(MusicCommandState& state, MusicHost& host,
                     const MusicCommand& command);
void FadeMusicVolumeFrom(MusicCommandState& state, MusicHost& host,
                         const MusicCommand& command);
// Sound_Cmd_C8_unk, Sound_Cmd_C9_unk and Sound_Cmd_CA_unk: the CD audio
// volume.
void SetDiscVolume(MusicCommandState& state, MusicHost& host,
                   const MusicCommand& command);
void FadeDiscVolume(MusicCommandState& state, const MusicCommand& command);
void FadeDiscVolumeFrom(MusicCommandState& state, const MusicCommand& command);
}  // namespace VS::Audio::Seq
