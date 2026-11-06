// Hand-maintained (tools/port/owned.csv).
// Live binding for SoundEffectPrograms.hpp: the sound-effect channels, the
// voice scheduler, the cutscene stream's voices and pause flags, the driver
// update flags, the music channels' voices and the active layer's notes, the
// voice owners, the voice output, the shared sound-effect tables and the
// sequence memory.
#include "Audio/SoundEffectProgramsLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <type_traits>

#include "Audio/SampleBanksLive.hpp"
#include "Audio/SeqMusicSequenceLive.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SoundEffectPrograms.hpp"
#include "Audio/SoundPlaybackLive.hpp"
#include "Audio/VoiceOutputLive.hpp"

extern "C" {
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/AkaoFormat.h"
#include "Audio/Seq/Channel.h"
#include "Audio/SoundEffects/SoundEffects.h"
#include "Legacy/Psx.h"
}

namespace VS::Audio::SoundEffects {
namespace {
static_assert(std::size(Audio_SoundEffects_channels) >= kChannelCount);
static_assert(std::size(Audio_Seq_musicChannels) == Seq::kMusicChannelCount);
static_assert(std::size(Audio_Mixer_voiceOwners) == Mixer::kVoiceCount);
static_assert(sizeof(Audio_CommandParams::param1) == sizeof(EffectSelector));
static_assert(
    std::is_convertible_v<decltype(&LivePlayInterfaceProgramPair),
                          Audio_CommandHandler> &&
    std::is_convertible_v<decltype(&LivePlayInterfaceProgram),
                          Audio_CommandHandler> &&
    std::is_convertible_v<decltype(&LivePlaySharedProgram),
                          Audio_CommandHandler> &&
    std::is_convertible_v<decltype(&LivePlayBlobPrograms),
                          Audio_CommandHandler> &&
    std::is_convertible_v<decltype(&LiveStopPrograms), Audio_CommandHandler> &&
    std::is_convertible_v<decltype(&LiveSetProgramVolume),
                          Audio_CommandHandler> &&
    std::is_convertible_v<decltype(&LiveFadeProgramVolume),
                          Audio_CommandHandler> &&
    std::is_convertible_v<decltype(&LiveSetSceneVolume),
                          Audio_CommandHandler> &&
    std::is_convertible_v<decltype(&LiveFadeSceneVolume),
                          Audio_CommandHandler> &&
    std::is_convertible_v<decltype(&LiveSetProgramPan), Audio_CommandHandler> &&
    std::is_convertible_v<decltype(&LiveFadeProgramPan),
                          Audio_CommandHandler> &&
    std::is_convertible_v<decltype(&LiveSetScenePan), Audio_CommandHandler> &&
    std::is_convertible_v<decltype(&LiveFadeScenePan), Audio_CommandHandler>);

// Pins a legacy field to the width of its typed copy.
template <typename To, typename From>
To Same(From value) {
  static_assert(sizeof(To) == sizeof(From));
  return static_cast<To>(value);
}

const char* RefusalReason(EffectRefusal refusal) {
  switch (refusal) {
    case EffectRefusal::kChannelOutOfRange:
      return "the channel lies outside the sound-effect channels";
    case EffectRefusal::kZeroLengthFade:
      return "a fade of zero 16-bit ticks runs over one tick";
    case EffectRefusal::kTablesUnbound:
      return "no shared sound-effect tables are bound";
    case EffectRefusal::kEffectOutOfRange:
      return "the effect id lies past the bound sound-effect tables";
    case EffectRefusal::kProgramOutOfRange:
      return "the shared program lies past the program data";
    case EffectRefusal::kBlobUnreadable:
      return "the sound-effect blob's entries lie outside its memory";
    case EffectRefusal::kBlobCountInvalid:
      return "the sound-effect blob counts no further programs";
    case EffectRefusal::kNoActiveMusic:
      return "no music layer is active";
    case EffectRefusal::kVoiceOutsideEffects:
      return "active voices lie below the first sound-effect voice";
    case EffectRefusal::kVoiceOutOfRange:
      return "the channel's voice lies past the voice owners";
  }
  return "refused";
}

char* Pointer(DataRef data) {
  return std::bit_cast<char*>(static_cast<std::uintptr_t>(data));
}

// The handles name only kNone; the live addresses they carry are made here.
DataRef Reference(const void* data) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<DataRef>(std::bit_cast<std::uintptr_t>(data));
}

EffectSelector SelectorOf(std::uintptr_t effect_id) {
  return static_cast<EffectSelector>(Same<std::intptr_t>(effect_id));
}

EffectChannel LoadChannel(const Audio_SeqChannel& legacy) {
  EffectChannel channel;
  channel.effect = SelectorOf(legacy.effectId);
  channel.tags = Same<std::int32_t>(legacy.effectTags);
  channel.update_flags = Same<std::uint32_t>(legacy.updateFlags);
  channel.priority = Same<std::int32_t>(legacy.priority);
  channel.voice_mask = Same<std::int32_t>(legacy.effectVoiceMask);
  channel.type = Same<std::uint16_t>(legacy.type);
  channel.note_length = Same<std::uint16_t>(legacy.noteLength);
  channel.gate_length = Same<std::uint16_t>(legacy.gateLength);
  channel.pan = Same<std::uint16_t>(legacy.pan);
  channel.pan_slide_length = Same<std::uint16_t>(legacy.panSlideLength);
  channel.effect_pan = Same<std::uint16_t>(legacy.effectPan);
  channel.effect_pan_step = Same<std::int16_t>(legacy.effectPanSlideStep);
  channel.effect_pan_length = Same<std::uint16_t>(legacy.effectPanSlideLength);
  channel.effect_volume = Same<std::int16_t>(legacy.effectVolume);
  channel.effect_volume_step = Same<std::int16_t>(legacy.effectVolumeSlideStep);
  channel.effect_volume_length =
      Same<std::uint16_t>(legacy.effectVolumeSlideLength);
  channel.pitch_scale = Same<std::int32_t>(legacy.effectPitchScale);
  channel.pitch_scale_length =
      Same<std::uint16_t>(legacy.effectPitchScaleSlideLength);
  channel.assigned_voice = Same<std::uint32_t>(legacy.voice.assignedVoice);
  channel.dirty_flags = Same<std::uint32_t>(legacy.voice.dirtyFlags);
  return channel;
}

void StoreChannel(Audio_SeqChannel& legacy, const EffectChannel& channel) {
  legacy.effectId = static_cast<std::uintptr_t>(channel.effect);
  legacy.effectTags = channel.tags;
  legacy.updateFlags = channel.update_flags;
  legacy.priority = channel.priority;
  legacy.effectVoiceMask = channel.voice_mask;
  legacy.type = channel.type;
  legacy.noteLength = channel.note_length;
  legacy.gateLength = channel.gate_length;
  legacy.pan = channel.pan;
  legacy.panSlideLength = channel.pan_slide_length;
  legacy.effectPan = channel.effect_pan;
  legacy.effectPanSlideStep = channel.effect_pan_step;
  legacy.effectPanSlideLength = channel.effect_pan_length;
  legacy.effectVolume = channel.effect_volume;
  legacy.effectVolumeSlideStep = channel.effect_volume_step;
  legacy.effectVolumeSlideLength = channel.effect_volume_length;
  legacy.effectPitchScale = channel.pitch_scale;
  legacy.effectPitchScaleSlideLength = channel.pitch_scale_length;
  legacy.voice.assignedVoice = channel.assigned_voice;
  legacy.voice.dirtyFlags = channel.dirty_flags;
}

// The blob each channel last started on, or 0. The legacy channel keeps one
// identity word for shared effects and blobs; a channel plays a blob while
// that word still names the blob it started on, so any other writer of the
// word (a release, a reset, a shared start) ends the program kind.
std::array<std::uintptr_t, kChannelCount> started_blobs{};

EffectKind KindOf(std::size_t index, const Audio_SeqChannel& legacy) {
  const std::uintptr_t blob = started_blobs[index];
  return blob != 0 && legacy.effectId == blob ? EffectKind::kProgram
                                              : EffectKind::kShared;
}

void RecordKind(std::size_t index, const EffectChannel& channel) {
  started_blobs[index] = channel.kind == EffectKind::kProgram
                             ? static_cast<std::uintptr_t>(channel.effect)
                             : 0;
}

EffectState LoadState() {
  EffectState state;
  for (std::size_t i = 0; i < state.channels.size(); ++i) {
    state.channels[i] = LoadChannel(Audio_SoundEffects_channels[i]);
    state.channels[i].kind = KindOf(i, Audio_SoundEffects_channels[i]);
  }
  state.scheduler = Seq::LoadVoiceScheduler(Audio_Mixer_voiceScheduler);
  state.stream_voices =
      Same<std::uint32_t>(Audio_Cutscene_streamState.voiceMask);
  state.stream_control_flags =
      Same<std::int32_t>(Audio_Cutscene_streamState.controlFlags);
  state.driver_update_flags =
      Same<std::uint32_t>(Audio_Driver_controlLatches.updateFlags);
  for (std::size_t i = 0; i < state.music_voices.size(); ++i) {
    state.music_voices[i] =
        Same<std::uint32_t>(Audio_Seq_musicChannels[i].voice.assignedVoice);
  }
  if (const Audio_MusicConfig* active = Audio_Seq_activeMusicConfig) {
    state.active_music_notes = Same<std::uint32_t>(active->activeNotes);
  }
  return state;
}

void StoreState(const EffectState& state) {
  for (std::size_t i = 0; i < state.channels.size(); ++i) {
    StoreChannel(Audio_SoundEffects_channels[i], state.channels[i]);
    RecordKind(i, state.channels[i]);
  }
  Seq::StoreVoiceScheduler(Audio_Mixer_voiceScheduler, state.scheduler);
  Audio_Cutscene_streamState.voiceMask = static_cast<int>(state.stream_voices);
  Audio_Cutscene_streamState.controlFlags = state.stream_control_flags;
  Audio_Driver_controlLatches.updateFlags = state.driver_update_flags;
  for (std::size_t i = 0; i < state.music_voices.size(); ++i) {
    Audio_Seq_musicChannels[i].voice.assignedVoice = state.music_voices[i];
  }
  if (Audio_MusicConfig* active = Audio_Seq_activeMusicConfig;
      active != nullptr && state.active_music_notes) {
    active->activeNotes = *state.active_music_notes;
  }
}

// FSoundCommandParams' words; extParam2 is not part of a sound-effect
// command.
EffectCommand CommandOf(const Audio_CommandParams& params) {
  return {.first = SelectorOf(params.param1),
          .second = Same<std::uint32_t>(params.param2),
          .third = Same<std::uint32_t>(params.param3),
          .fourth = Same<std::uint32_t>(params.param4),
          .bank_slot = Same<std::uint32_t>(params.extParam1)};
}

void StoreCommand(Audio_CommandParams& params, const EffectCommand& command) {
  params.param1 = static_cast<std::uintptr_t>(command.first);
  params.param2 = command.second;
  params.param3 = command.third;
  params.param4 = command.fourth;
  params.extParam1 = command.bank_slot;
}

class LiveEffectHost final : public EffectHost {
 public:
  explicit LiveEffectHost(const char* operation) : operation_(operation) {}

  void ResetChannel(std::size_t index, EffectChannel& channel,
                    ProgramStart program) override {
    Audio_SeqChannel& legacy = Audio_SoundEffects_channels[index];
    StoreChannel(legacy, channel);
    char* start = program.data == DataRef::kNone
                      ? nullptr
                      : Pointer(program.data) + program.offset;
    Seq::LiveResetSoundEffectChannel(&legacy, start);
    const EffectKind kind = channel.kind;
    channel = LoadChannel(legacy);
    channel.kind = kind;
  }

  void ClearVoiceOwner(std::uint32_t voice) override {
    Audio_Mixer_voiceOwners[voice] = nullptr;
  }

  void SetVoiceRelease(std::uint32_t voice, std::int32_t rate,
                       std::uint32_t mode) override {
    Driver::SetLiveVoiceReleaseRateAndMode(static_cast<int>(voice), rate, mode);
  }

  std::optional<SharedTables> Tables() override {
    const std::optional<Playback::LiveEffectTables> tables =
        Playback::BoundLiveEffectTables();
    if (!tables) return std::nullopt;
    return SharedTables{.program_offsets = tables->program_offsets,
                        .bank_selectors = tables->bank_selectors,
                        .program_data = Reference(tables->program_data.data()),
                        .program_bytes = tables->program_data.size()};
  }

  std::uint32_t BankSlot(std::int32_t selector) override {
    return static_cast<std::uint32_t>(
        Samples::LiveSoundEffectBankSlot(selector));
  }

  // The memory from `blob` to the end of the sequence memory it lies in.
  std::span<const std::byte> ProgramBytes(DataRef blob) override {
    char* start = Pointer(blob);
    const std::optional<Seq::LiveProgramMemory> region =
        Seq::LiveProgramRegion(start);
    if (!region) return {};
    const Seq::LiveProgramMemory from = region->From(start);
    return {reinterpret_cast<const std::byte*>(from.base), from.size};
  }

  void Refuse(EffectRefusal refusal) override {
    Seq::ReportRefusal(operation_, RefusalReason(refusal));
  }

 private:
  const char* operation_;
};

// Runs a core over the live state.
template <typename Core>
void RunEffects(const char* operation, Core core) {
  LiveEffectHost host(operation);
  EffectState state = LoadState();
  core(state, host);
  StoreState(state);
}

// Runs a command core over the live state and `params`, and writes the
// command's words back as the original's handlers leave them.
template <typename Core>
void RunCommand(const char* operation, Audio_CommandParams* params, Core core) {
  if (params == nullptr) {
    Seq::ReportRefusal(operation, "no command parameters");
    return;
  }
  EffectCommand command = CommandOf(*params);
  RunEffects(operation, [&](EffectState& state, LiveEffectHost& host) {
    core(state, host, command);
  });
  StoreCommand(*params, command);
}

std::optional<ProgramStart> ProgramAt(const char* program) {
  if (program == nullptr) return std::nullopt;
  return ProgramStart{.data = Reference(program)};
}
}  // namespace

// decomp: SLUS_010.40 Sound_EvictSfxVoice (src/SLUS_010.40/25AC.c:2539)
void LiveReleaseMatchingVoices(std::intptr_t selector, int flags) {
  RunEffects("ReleaseMatchingVoices",
             [&](EffectState& state, LiveEffectHost& host) {
               ReleaseMatchingVoices(
                   state, host, static_cast<EffectSelector>(selector), flags);
             });
}

// decomp: SLUS_010.40 Sound_PlaySfxProgram (src/SLUS_010.40/25AC.c:2709)
void LivePlayProgramPair(Audio_CommandParams* params, char* first_program,
                         char* second_program, int skip_release) {
  const ProgramPair programs{.first = ProgramAt(first_program),
                             .second = ProgramAt(second_program)};
  RunCommand(
      "PlayProgramPair", params,
      [&](EffectState& state, LiveEffectHost& host, EffectCommand& command) {
        PlayProgramPair(state, host, command, programs, skip_release != 0);
      });
}

// decomp: SLUS_010.40 Sound_MarkActiveChannelsVolumeDirty
// (src/SLUS_010.40/25AC.c:2797) Only the channels up to the layer's highest
// active channel are touched, as in the original.
void LiveInvalidateMusicChannelVolumes(Audio_MusicConfig* layer,
                                       Audio_SeqChannel* channels) {
  if (layer == nullptr || channels == nullptr) {
    Seq::ReportRefusal("InvalidateMusicChannelVolumes",
                       "no music layer or channels");
    return;
  }
  const auto active = Same<std::uint32_t>(layer->activeChannels);
  const auto used = static_cast<std::size_t>(std::bit_width(active));
  const std::span<Audio_SeqChannel> table(channels, used);
  std::array<std::uint32_t, Seq::kMusicChannelCount> dirty{};
  for (std::size_t i = 0; i < table.size(); ++i) {
    dirty[i] = Same<std::uint32_t>(table[i].voice.dirtyFlags);
  }
  InvalidateMusicChannelVolumes(active, dirty);
  for (std::size_t i = 0; i < table.size(); ++i) {
    table[i].voice.dirtyFlags = dirty[i];
  }
}

// decomp: SLUS_010.40 Sound_MarkScheduledSfxChannelsVolumeDirty
// (src/SLUS_010.40/25AC.c:2822)
void LiveInvalidateSoundEffectVolumes() {
  RunEffects("InvalidateSoundEffectVolumes",
             [](EffectState& state, LiveEffectHost& host) {
               InvalidateSoundEffectVolumes(state, host);
             });
}

// decomp: SLUS_010.40 Sound_Cmd_34_unk (src/SLUS_010.40/25AC.c:3046)
void LivePlayInterfaceProgramPair(Audio_CommandParams* params) {
  RunCommand("34 PlayInterfaceProgramPair", params, PlayInterfaceProgramPair);
}

// decomp: SLUS_010.40 Sound_Cmd_30_unk (src/SLUS_010.40/25AC.c:3061)
void LivePlayInterfaceProgram(Audio_CommandParams* params) {
  RunCommand("30 PlayInterfaceProgram", params, PlayInterfaceProgram);
}

// decomp: SLUS_010.40 Sound_Cmd_20_unk (src/SLUS_010.40/25AC.c:3074)
void LivePlaySharedProgram(Audio_CommandParams* params) {
  RunCommand("20 PlaySharedProgram", params, PlaySharedProgram);
}

// decomp: SLUS_010.40 func_8001733C (src/SLUS_010.40/25AC.c:3085)
void LivePlayBlobPrograms(Audio_CommandParams* params) {
  RunCommand("24 PlayBlobPrograms", params, PlayBlobPrograms);
}

// decomp: SLUS_010.40 Sound_Cmd_21_unk (src/SLUS_010.40/25AC.c:3118)
void LiveStopPrograms(Audio_CommandParams* params) {
  RunCommand("21 StopPrograms", params, StopPrograms);
}

// decomp: SLUS_010.40 Sound_Cmd_A0_unk (src/SLUS_010.40/25AC.c:3250)
void LiveSetProgramVolume(Audio_CommandParams* params) {
  RunCommand("A0 SetProgramVolume", params,
             [](EffectState& state, EffectHost&, EffectCommand& command) {
               SetProgramVolume(state, command);
             });
}

// decomp: SLUS_010.40 Sound_Cmd_A1_unk (src/SLUS_010.40/25AC.c:3276)
void LiveFadeProgramVolume(Audio_CommandParams* params) {
  RunCommand("A1 FadeProgramVolume", params, FadeProgramVolume);
}

// decomp: SLUS_010.40 Sound_Cmd_A8_unk (src/SLUS_010.40/25AC.c:3302)
void LiveSetSceneVolume(Audio_CommandParams* params) {
  RunCommand("A8 SetSceneVolume", params,
             [](EffectState& state, EffectHost&, EffectCommand& command) {
               SetSceneVolume(state, command);
             });
}

// decomp: SLUS_010.40 Sound_Cmd_A9_unk (src/SLUS_010.40/25AC.c:3318)
void LiveFadeSceneVolume(Audio_CommandParams* params) {
  RunCommand("A9 FadeSceneVolume", params, FadeSceneVolume);
}

// decomp: SLUS_010.40 Sound_Cmd_A2_unk (src/SLUS_010.40/25AC.c:3339)
void LiveSetProgramPan(Audio_CommandParams* params) {
  RunCommand("A2 SetProgramPan", params,
             [](EffectState& state, EffectHost&, EffectCommand& command) {
               SetProgramPan(state, command);
             });
}

// decomp: SLUS_010.40 Sound_Cmd_A3_unk (src/SLUS_010.40/25AC.c:3365)
void LiveFadeProgramPan(Audio_CommandParams* params) {
  RunCommand("A3 FadeProgramPan", params, FadeProgramPan);
}

// decomp: SLUS_010.40 Sound_Cmd_AA_unk (src/SLUS_010.40/25AC.c:3391)
void LiveSetScenePan(Audio_CommandParams* params) {
  RunCommand("AA SetScenePan", params,
             [](EffectState& state, EffectHost&, EffectCommand& command) {
               SetScenePan(state, command);
             });
}

// decomp: SLUS_010.40 Sound_Cmd_AB_unk (src/SLUS_010.40/25AC.c:3407)
void LiveFadeScenePan(Audio_CommandParams* params) {
  RunCommand("AB FadeScenePan", params, FadeScenePan);
}
}  // namespace VS::Audio::SoundEffects
