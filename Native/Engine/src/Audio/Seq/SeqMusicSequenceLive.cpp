// Hand-maintained (tools/port/owned.csv).
// Live binding for SeqMusicSequenceLive.hpp: the primary, saved and secondary
// music layers and their channel tables, the voice scheduler and voice
// owners, the master fade, the CD audio volume and the music sequences the
// game heap holds.
#include "Audio/SeqMusicSequenceLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <type_traits>

#include "Audio/MusicVoiceConflicts.hpp"
#include "Audio/SampleBanksLive.hpp"
#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqMusicSequence.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SeqSequencer.hpp"
#include "Audio/SoundEffectProgramsLive.hpp"
#include "Audio/VoiceOutputLive.hpp"
#include "Audio/XaMixLive.hpp"

extern "C" {
#include "Audio/Control/SoundControl.h"
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/MusicState.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/Seq/AkaoFormat.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/Sequencer.h"
#include "Audio/SoundEffects/SoundEffects.h"
}

namespace VS::Audio::Seq {
namespace {
static_assert(std::is_convertible_v<decltype(&LiveStartOrResumeMusic),
                                    Audio_CommandHandler>);
static_assert(std::size(Audio_Seq_musicChannels) == kMusicChannelCount);
static_assert(std::size(Audio_Seq_savedMusicChannels) == kMusicChannelCount);

// Pins a legacy field to the width of its typed copy.
template <typename To, typename From>
To Same(From value) {
  static_assert(sizeof(To) == sizeof(From));
  return static_cast<To>(value);
}

// Sound_KillMusicConfig releases a stopped layer's voices at this rate and
// mode.
constexpr int kReleaseRate = 5;
constexpr unsigned kReleaseMode = 3U;

// The memory from `sequence` to the end of the block it lies in; empty when
// it lies in no sequence memory.
std::optional<LiveProgramMemory> SequenceMemory(
    Audio_AkaoSequenceFile* sequence) {
  auto* const start = reinterpret_cast<char*>(sequence);
  const std::optional<LiveProgramMemory> region = LiveProgramRegion(start);
  if (!region) return std::nullopt;
  return region->From(start);
}

// Moves `pointer` from `from` to the same offset in `to`, which is as large;
// pointers outside `from` keep their value.
template <typename T>
void Rebase(T*& pointer, const LiveProgramMemory& from,
            const LiveProgramMemory& to) {
  const LiveProgramMemory rest = from.From(reinterpret_cast<char*>(pointer));
  if (rest.base == nullptr) return;
  pointer = reinterpret_cast<T*>(to.base + (from.size - rest.size));
}

std::array<std::uint32_t, kMusicChannelCount> AssignedVoices(
    const Audio_SeqChannel* channels) {
  std::array<std::uint32_t, kMusicChannelCount> voices{};
  for (std::size_t i = 0; i < kMusicChannelCount; ++i) {
    voices[i] = Same<std::uint32_t>(channels[i].voice.assignedVoice);
  }
  return voices;
}

// func_80015D38 over the secondary layer's channels.
std::uint32_t SecondaryVoices() {
  const Audio_MusicConfig* secondary = Audio_Seq_secondaryMusicConfig;
  if (secondary == nullptr || Audio_Seq_secondaryMusicChannels == nullptr) {
    return 0;
  }
  return AssignedVoiceMask(AssignedVoices(Audio_Seq_secondaryMusicChannels),
                           Same<std::uint32_t>(secondary->activeChannels));
}

// The music layer state a track start or restore reads, with each channel's
// program as offsets into `memory`.
MusicStartState LoadMusicStart(const LiveProgramMemory& memory) {
  MusicStartState state;
  state.layer = LoadMusicLayer(*Audio_Seq_activeMusicConfig);
  for (std::size_t i = 0; i < kMusicChannelCount; ++i) {
    state.channels[i] = LoadSequenceChannel(Audio_Seq_musicChannels[i], memory);
  }
  state.scheduler = LoadVoiceScheduler(Audio_Mixer_voiceScheduler);
  state.driver_update_flags = Audio_Driver_controlLatches.updateFlags;
  state.stream_control_flags = Audio_Cutscene_streamControlFlags;
  state.stream_voices = Same<std::uint32_t>(Audio_Cutscene_streamVoices);
  state.alternate_bank_id =
      Same<std::uint32_t>(Audio_Samples_musicSampleBankIds.alternateBankId);
  state.secondary_voices = SecondaryVoices();
  if (const Audio_MusicConfig* secondary = Audio_Seq_secondaryMusicConfig) {
    state.secondary_reverb_depth = secondary->reverbDepth;
  }
  state.saved_music_id = Audio_Seq_savedMusicConfig.musicId;
  return state;
}

void StoreMusicStart(const MusicStartState& state,
                     const LiveProgramMemory& memory) {
  Audio_Seq_savedMusicConfig.musicId = state.saved_music_id;
  StoreMusicLayer(*Audio_Seq_activeMusicConfig, state.layer);
  for (std::size_t i = 0; i < kMusicChannelCount; ++i) {
    StoreSequenceChannel(Audio_Seq_musicChannels[i], state.channels[i], memory);
  }
  StoreVoiceScheduler(Audio_Mixer_voiceScheduler, state.scheduler);
  Audio_Driver_controlLatches.updateFlags = state.driver_update_flags;
}

// A loaded track's table at `position` in `memory`; null when it has none or
// it lies outside the sequence.
template <typename T>
T* TableAt(const std::optional<std::int64_t>& position,
           const LiveProgramMemory& memory, const char* table) {
  if (!position) return nullptr;
  if (*position < 0 || static_cast<std::uint64_t>(*position) >= memory.size) {
    ReportRefusal(table, "the track's table lies outside its sequence");
    return nullptr;
  }
  return reinterpret_cast<T*>(memory.base + *position);
}

MusicCommandState LoadMusicCommand() {
  MusicCommandState state;
  state.active = LoadMusicLayer(*Audio_Seq_activeMusicConfig);
  if (const Audio_MusicConfig* secondary = Audio_Seq_secondaryMusicConfig) {
    state.secondary = LoadMusicLayer(*secondary);
  }
  state.saved_music_id = Audio_Seq_savedMusicConfig.musicId;
  state.mix_behavior = Audio_Driver_controlLatches.mixBehavior;
  state.master_fade = {Audio_Seq_masterFade.value, Audio_Seq_masterFade.step,
                       Audio_Seq_masterFade.ticksRemaining};
  state.music_loops_remaining = Audio_Seq_musicLoopsRemaining;
  state.disc_volume = {Audio_Seq_discVolume, Audio_Seq_discVolumeFadeStep,
                       Audio_Seq_discVolumeFadeLength};
  return state;
}

// The saved layer's track is stored first: a layer that is the saved one
// stores its own track over it.
void StoreMusicCommand(const MusicCommandState& state) {
  Audio_Seq_savedMusicConfig.musicId = state.saved_music_id;
  StoreMusicLayer(*Audio_Seq_activeMusicConfig, state.active);
  if (Audio_MusicConfig* secondary = Audio_Seq_secondaryMusicConfig;
      secondary != nullptr && state.secondary &&
      secondary != Audio_Seq_activeMusicConfig) {
    StoreMusicLayer(*secondary, *state.secondary);
  }
  Audio_Driver_controlLatches.mixBehavior = state.mix_behavior;
  Audio_Seq_masterFade.value = state.master_fade.value;
  Audio_Seq_masterFade.step = state.master_fade.step;
  Audio_Seq_masterFade.ticksRemaining = state.master_fade.ticks_remaining;
  Audio_Seq_musicLoopsRemaining = state.music_loops_remaining;
  Audio_Seq_discVolume = state.disc_volume.value;
  Audio_Seq_discVolumeFadeStep = state.disc_volume.step;
  Audio_Seq_discVolumeFadeLength = state.disc_volume.length;
}

MusicCommand CommandOf(const Audio_CommandParams& params) {
  return {static_cast<std::uint32_t>(params.param1),
          params.param2,
          params.param3,
          params.param4,
          params.extParam1,
          params.extParam2};
}

// One music command over the live driver state. Every effect that takes the
// state stores it before and loads it after.
class LiveMusicHost final : public MusicHost {
 public:
  explicit LiveMusicHost(const Audio_CommandParams* params) : params_(params) {}

  void LoadSequence(MusicCommandState& state) override {
    StoreMusicCommand(state);
    LiveLoadMusicSequence(Sequence());
    state = LoadMusicCommand();
  }

  void RestoreSequence(MusicCommandState& state,
                       bool swap_with_saved) override {
    StoreMusicCommand(state);
    LiveRestoreMusicSequence(Sequence(), swap_with_saved ? 1 : 0);
    state = LoadMusicCommand();
  }

  void SaveSnapshot(MusicCommandState& state) override {
    StoreMusicCommand(state);
    Audio_Native_SaveMusicSnapshot();
    state = LoadMusicCommand();
  }

  void MapSavedInstrumentsToBaseBank(MusicCommandState& state) override {
    StoreMusicCommand(state);
    const auto flags = Same<u_int>(Audio_Seq_savedMusicConfig.statusFlags);
    for (Audio_SeqChannel& channel : Audio_Seq_savedMusicChannels) {
      Samples::LiveMapInstrumentToBaseBank(flags, &channel);
    }
    state = LoadMusicCommand();
  }

  void AttachSavedLayer(MusicCommandState& state) override {
    StoreMusicCommand(state);
    Audio_Seq_secondaryMusicConfig = &Audio_Seq_savedMusicConfig;
    Audio_Seq_secondaryMusicChannels = std::data(Audio_Seq_savedMusicChannels);
    state = LoadMusicCommand();
  }

  void ReleaseSavedVoiceConflicts(MusicCommandState& state) override {
    StoreMusicCommand(state);
    ReleaseLiveSavedVoiceConflicts();
    state = LoadMusicCommand();
  }

  void InvalidateMusicVolumes(MusicCommandState& state,
                              ChannelGroup group) override {
    Audio_MusicConfig* config = Audio_Seq_activeMusicConfig;
    Audio_SeqChannel* channels = std::data(Audio_Seq_musicChannels);
    if (group == ChannelGroup::kSecondaryMusic) {
      config = Audio_Seq_secondaryMusicConfig;
      channels = Audio_Seq_secondaryMusicChannels;
    }
    if (config == nullptr || channels == nullptr) return;
    StoreMusicCommand(state);
    SoundEffects::LiveInvalidateMusicChannelVolumes(config, channels);
    state = LoadMusicCommand();
  }

  void WriteDiscVolume(MusicCommandState& state) override {
    StoreMusicCommand(state);
    WriteLiveDiscVolume();
    state = LoadMusicCommand();
  }

  void Refuse(MusicRefusal refusal) override {
    if (refusal == MusicRefusal::kZeroLengthCrossfade) {
      ReportRefusal("CrossfadeMusicLayers",
                    "a crossfade of zero ticks runs over one tick");
    }
  }

 private:
  [[nodiscard]] Audio_AkaoSequenceFile* Sequence() const {
    if (params_ == nullptr) return nullptr;
    return reinterpret_cast<Audio_AkaoSequenceFile*>(params_->param1);
  }

  const Audio_CommandParams* params_;
};

using CommandCore = void (*)(MusicCommandState&, MusicHost&,
                             const MusicCommand&);

void RunCommand(const char* operation, Audio_CommandParams* params,
                CommandCore command) {
  if (params == nullptr || Audio_Seq_activeMusicConfig == nullptr) {
    ReportRefusal(operation, "no command parameters or active music layer");
    return;
  }
  LiveMusicHost host(params);
  MusicCommandState state = LoadMusicCommand();
  command(state, host, CommandOf(*params));
  StoreMusicCommand(state);
}
}  // namespace

// decomp: SLUS_010.40 func_80015C9C (src/SLUS_010.40/25AC.c:2330)
void LiveResetSoundEffectChannel(Audio_SeqChannel* channel,
                                 char* program_start) {
  const std::optional<LiveProgramMemory> region =
      LiveProgramRegion(program_start);
  const std::optional<Samples::InstrumentRecord> first_instrument =
      LiveInstrument(0);
  if (channel == nullptr || !region || !first_instrument) {
    ReportRefusal("ResetSoundEffectChannel",
                  "the program lies outside the sound-effect data");
    return;
  }
  SequenceChannel record = LoadSequenceChannel(*channel, *region);
  const LiveProgramMemory rest = region->From(program_start);
  ResetSoundEffectChannel(record,
                          static_cast<std::uint32_t>(region->size - rest.size),
                          *first_instrument);
  StoreSequenceChannel(*channel, record, *region);
}

// decomp: SLUS_010.40 Sound_LoadAkaoSequence (src/SLUS_010.40/25AC.c:2380)
void LiveLoadMusicSequence(Audio_AkaoSequenceFile* sequence) {
  Audio_MusicConfig* active = Audio_Seq_activeMusicConfig;
  const std::optional<LiveProgramMemory> memory = SequenceMemory(sequence);
  const std::optional<Samples::InstrumentRecord> first_instrument =
      LiveInstrument(0);
  if (active == nullptr || !memory || !first_instrument) {
    ReportRefusal("LoadMusicSequence",
                  "the track lies outside the sequence data");
    return;
  }
  MusicStartState state = LoadMusicStart(*memory);
  const LoadResult result =
      LoadMusicSequence(state, memory->View(), *first_instrument);
  if (result.status == LoadStatus::kHeaderOutside) {
    ReportRefusal("LoadMusicSequence",
                  "the sequence ends inside its header or channel offsets");
    return;
  }
  StoreMusicStart(state, *memory);
  active->sequenceBase = reinterpret_cast<std::uintptr_t>(sequence);
  active->patchTable = TableAt<Audio_AkaoSequenceFile>(
      result.tables.patch_table, *memory, "LoadMusicSequence");
  active->keymapTable = TableAt<u_short>(result.tables.keymap_table, *memory,
                                         "LoadMusicSequence");
  if (result.channels_outside != 0) {
    ReportRefusal("LoadMusicSequence",
                  "a channel's program starts outside the sequence");
  }
}

// decomp: SLUS_010.40 Sound_KillMusicConfig (src/SLUS_010.40/25AC.c:2506)
void LiveReleaseMusicConfiguration(Audio_MusicConfig* config,
                                   Audio_SeqChannel* channels,
                                   unsigned music_id_filter) {
  if (config == nullptr || channels == nullptr) {
    ReportRefusal("ReleaseMusicConfiguration", "no music layer or channels");
    return;
  }
  std::uint32_t owned = 0;
  for (std::size_t voice = 0; voice < std::size(Audio_Mixer_voiceOwners);
       ++voice) {
    if (Audio_Mixer_voiceOwners[voice] == config) owned |= 1U << voice;
  }
  const LiveProgramMemory unchanged;
  MusicLayerState layer = LoadMusicLayer(*config);
  MusicChannels records;
  for (std::size_t i = 0; i < kMusicChannelCount; ++i) {
    records[i] = LoadSequenceChannel(channels[i], unchanged);
  }
  const std::optional<std::uint32_t> released =
      ReleaseMusicConfiguration(layer, records, owned, music_id_filter);
  if (!released) return;
  StoreMusicLayer(*config, layer);
  for (std::size_t i = 0; i < kMusicChannelCount; ++i) {
    StoreSequenceChannel(channels[i], records[i], unchanged);
  }
  for (std::size_t voice = 0; voice < std::size(Audio_Mixer_voiceOwners);
       ++voice) {
    if ((*released & (1U << voice)) == 0) continue;
    Audio_Mixer_voiceOwners[voice] = nullptr;
    Driver::SetLiveVoiceReleaseRateAndMode(static_cast<int>(voice),
                                           kReleaseRate, kReleaseMode);
  }
}

// decomp: SLUS_010.40 Sound_SetMusicSequence (src/SLUS_010.40/25AC.c:2847)
// The original moves every program pointer of the restored track by the
// distance between the old and new sequence; here each pointer into the old
// sequence moves to the same offset in the new one, and pointers elsewhere
// keep their value.
void LiveRestoreMusicSequence(Audio_AkaoSequenceFile* sequence,
                              int swap_with_saved) {
  const std::optional<LiveProgramMemory> memory = SequenceMemory(sequence);
  const std::optional<SequenceHeader> header =
      memory ? ReadSequenceHeader(memory->View()) : std::nullopt;
  if (Audio_Seq_activeMusicConfig == nullptr || !header) {
    ReportRefusal("RestoreMusicSequence",
                  "the track lies outside the sequence data");
    return;
  }
  Audio_Native_RestoreMusicSnapshot(swap_with_saved);
  Audio_MusicConfig* active = Audio_Seq_activeMusicConfig;
  const LiveProgramMemory previous{
      reinterpret_cast<char*>(Audio_Seq_primaryMusicConfig.sequenceBase),
      memory->size};
  const auto playing = Same<std::uint32_t>(active->activeChannels);
  const LiveProgramMemory unchanged;
  MusicStartState state = LoadMusicStart(unchanged);
  for (std::size_t i = 0; i < kMusicChannelCount; ++i) {
    if ((playing & (1U << i)) == 0) continue;
    SequenceChannel& record = state.channels[i];
    record = LoadSequenceChannel(Audio_Seq_musicChannels[i], previous);
    if (!record.idle && record.program.counter == kOutsideProgram) {
      ReportRefusal("RestoreMusicSequence",
                    "a channel's program lies outside the restored track");
      record.idle = true;
    }
  }
  Rebase(active->patchTable, previous, *memory);
  Rebase(active->keymapTable, previous, *memory);
  active->sequenceBase = reinterpret_cast<std::uintptr_t>(sequence);
  const std::uint32_t remapped =
      RestoreMusicSequence(state, *header, swap_with_saved != 0);
  Audio_Seq_savedMusicConfig.musicId = state.saved_music_id;
  StoreMusicLayer(*active, state.layer);
  for (std::size_t i = 0; i < kMusicChannelCount; ++i) {
    Audio_SeqChannel& channel = Audio_Seq_musicChannels[i];
    const bool moved = (playing & (1U << i)) != 0;
    StoreSequenceChannel(channel, state.channels[i],
                         moved ? *memory : unchanged);
    if (moved) Rebase(channel.keymap, previous, *memory);
  }
  StoreVoiceScheduler(Audio_Mixer_voiceScheduler, state.scheduler);
  Audio_Driver_controlLatches.updateFlags = state.driver_update_flags;
  for (std::size_t i = 0; i < kMusicChannelCount; ++i) {
    if ((remapped & (1U << i)) != 0) {
      Samples::LiveMapInstrumentToAlternateBank(
          Same<u_int>(active->statusFlags), &Audio_Seq_musicChannels[i]);
    }
  }
}

// decomp: SLUS_010.40 Sound_Cmd_10_StartFieldMusic
// (src/SLUS_010.40/25AC.c:2958)
void LiveStartOrResumeMusic(Audio_CommandParams* params) {
  RunCommand("StartOrResumeMusic", params, &StartOrResumeMusic);
}

// decomp: SLUS_010.40 Sound_Cmd_40_PushMusicState (src/SLUS_010.40/25AC.c:2969)
void LiveSaveCurrentMusic(Audio_CommandParams* params) {
  if (Audio_Seq_activeMusicConfig == nullptr) {
    ReportRefusal("SaveCurrentMusic", "no active music layer");
    return;
  }
  LiveMusicHost host(params);
  MusicCommandState state = LoadMusicCommand();
  SaveCurrentMusic(state, host);
  StoreMusicCommand(state);
}

// decomp: SLUS_010.40 Sound_Cmd_19_Unk (src/SLUS_010.40/25AC.c:2983)
void LiveStartMusicLayer(Audio_CommandParams* params) {
  RunCommand("StartMusicLayer", params, &StartMusicLayer);
}

// decomp: SLUS_010.40 Sound_Cmd_1A_Unk (src/SLUS_010.40/25AC.c:3001)
void LiveCrossfadeMusicLayers(Audio_CommandParams* params) {
  RunCommand("CrossfadeMusicLayers", params, &CrossfadeMusicLayers);
}

// decomp: SLUS_010.40 Sound_Cmd_12_unk (src/SLUS_010.40/25AC.c:3034)
void LiveStartMusicWithLoopCounter(Audio_CommandParams* params) {
  RunCommand("StartMusicWithLoopCounter", params, &StartMusicWithLoopCounter);
}

// decomp: SLUS_010.40 Sound_Cmd_C0_unk (src/SLUS_010.40/25AC.c:3123)
void LiveSetMusicVolume(Audio_CommandParams* params) {
  RunCommand("SetMusicVolume", params, &SetMusicVolume);
}

// decomp: SLUS_010.40 Sound_Cmd_C1_unk (src/SLUS_010.40/25AC.c:3147)
void LiveFadeMusicVolume(Audio_CommandParams* params) {
  RunCommand("FadeMusicVolume", params, &FadeMusicVolume);
}

// decomp: SLUS_010.40 Sound_Cmd_C2_unk (src/SLUS_010.40/25AC.c:3184)
void LiveFadeMusicVolumeFrom(Audio_CommandParams* params) {
  RunCommand("FadeMusicVolumeFrom", params, &FadeMusicVolumeFrom);
}

// decomp: SLUS_010.40 Sound_Cmd_C8_unk (src/SLUS_010.40/25AC.c:3216)
void LiveSetDiscVolume(Audio_CommandParams* params) {
  RunCommand("SetDiscVolume", params, &SetDiscVolume);
}

// decomp: SLUS_010.40 Sound_Cmd_C9_unk (src/SLUS_010.40/25AC.c:3223)
void LiveFadeDiscVolume(Audio_CommandParams* params) {
  RunCommand(
      "FadeDiscVolume", params,
      [](MusicCommandState& state, MusicHost& /*host*/,
         const MusicCommand& command) { FadeDiscVolume(state, command); });
}

// decomp: SLUS_010.40 Sound_Cmd_CA_unk (src/SLUS_010.40/25AC.c:3237)
void LiveFadeDiscVolumeFrom(Audio_CommandParams* params) {
  RunCommand(
      "FadeDiscVolumeFrom", params,
      [](MusicCommandState& state, MusicHost& /*host*/,
         const MusicCommand& command) { FadeDiscVolumeFrom(state, command); });
}
}  // namespace VS::Audio::Seq
