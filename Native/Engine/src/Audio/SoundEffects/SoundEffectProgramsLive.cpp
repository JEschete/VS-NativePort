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

#include "Audio/Cutscene/Stream.hpp"
#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/Mixer/MixControl.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/SampleBanksLive.hpp"
#include "Audio/Seq/AkaoFormat.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/SeqMusicSequenceLive.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SoundEffectPrograms.hpp"
#include "Audio/SoundEffects/SoundEffects.hpp"
#include "Audio/SoundPlaybackLive.hpp"
#include "Audio/VoiceOutputLive.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Audio::SoundEffects {
namespace {
static_assert(std::size(VS::Audio::SoundEffects::channels) >= kChannelCount);
static_assert(std::size(VS::Audio::Seq::musicChannels) ==
              Seq::kMusicChannelCount);
static_assert(std::size(VS::Audio::Mixer::voiceOwners) == Mixer::kVoiceCount);
static_assert(sizeof(VS::Audio::CommandParams::param1) ==
              sizeof(EffectSelector));
static_assert(std::is_convertible_v<decltype(&LivePlayInterfaceProgramPair),
                                    VS::Audio::CommandHandler> &&
              std::is_convertible_v<decltype(&LivePlayInterfaceProgram),
                                    VS::Audio::CommandHandler> &&
              std::is_convertible_v<decltype(&LivePlaySharedProgram),
                                    VS::Audio::CommandHandler> &&
              std::is_convertible_v<decltype(&LivePlayBlobPrograms),
                                    VS::Audio::CommandHandler> &&
              std::is_convertible_v<decltype(&LiveStopPrograms),
                                    VS::Audio::CommandHandler> &&
              std::is_convertible_v<decltype(&LiveSetProgramVolume),
                                    VS::Audio::CommandHandler> &&
              std::is_convertible_v<decltype(&LiveFadeProgramVolume),
                                    VS::Audio::CommandHandler> &&
              std::is_convertible_v<decltype(&LiveSetSceneVolume),
                                    VS::Audio::CommandHandler> &&
              std::is_convertible_v<decltype(&LiveFadeSceneVolume),
                                    VS::Audio::CommandHandler> &&
              std::is_convertible_v<decltype(&LiveSetProgramPan),
                                    VS::Audio::CommandHandler> &&
              std::is_convertible_v<decltype(&LiveFadeProgramPan),
                                    VS::Audio::CommandHandler> &&
              std::is_convertible_v<decltype(&LiveSetScenePan),
                                    VS::Audio::CommandHandler> &&
              std::is_convertible_v<decltype(&LiveFadeScenePan),
                                    VS::Audio::CommandHandler>);

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

std::uint8_t* Pointer(DataRef data) {
  return std::bit_cast<std::uint8_t*>(static_cast<std::uintptr_t>(data));
}

// The handles name only kNone; the live addresses they carry are made here.
DataRef Reference(const void* data) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<DataRef>(std::bit_cast<std::uintptr_t>(data));
}

EffectSelector SelectorOf(std::uintptr_t effect_id) {
  return static_cast<EffectSelector>(Same<std::intptr_t>(effect_id));
}

EffectChannel LoadChannel(const VS::Audio::SeqChannel& legacy) {
  EffectChannel channel;
  channel.effect = SelectorOf(legacy.effect_id);
  channel.tags = Same<std::int32_t>(legacy.effect_tags);
  channel.update_flags = Same<std::uint32_t>(legacy.update_flags);
  channel.priority = Same<std::int32_t>(legacy.priority);
  channel.voice_mask = Same<std::int32_t>(legacy.effect_voice_mask);
  channel.type = Same<std::uint16_t>(legacy.type);
  channel.note_length = Same<std::uint16_t>(legacy.note_length);
  channel.gate_length = Same<std::uint16_t>(legacy.gate_length);
  channel.pan = Same<std::uint16_t>(legacy.pan);
  channel.pan_slide_length = Same<std::uint16_t>(legacy.pan_slide_length);
  channel.effect_pan = Same<std::uint16_t>(legacy.effect_pan);
  channel.effect_pan_step = Same<std::int16_t>(legacy.effect_pan_slide_step);
  channel.effect_pan_length =
      Same<std::uint16_t>(legacy.effect_pan_slide_length);
  channel.effect_volume = Same<std::int16_t>(legacy.effect_volume);
  channel.effect_volume_step =
      Same<std::int16_t>(legacy.effect_volume_slide_step);
  channel.effect_volume_length =
      Same<std::uint16_t>(legacy.effect_volume_slide_length);
  channel.pitch_scale = Same<std::int32_t>(legacy.effect_pitch_scale);
  channel.pitch_scale_length =
      Same<std::uint16_t>(legacy.effect_pitch_scale_slide_length);
  channel.assigned_voice = Same<std::uint32_t>(legacy.voice.assigned_voice);
  channel.dirty_flags = Same<std::uint32_t>(legacy.voice.dirty_flags);
  return channel;
}

void StoreChannel(VS::Audio::SeqChannel& legacy, const EffectChannel& channel) {
  legacy.effect_id = static_cast<std::uintptr_t>(channel.effect);
  legacy.effect_tags = channel.tags;
  legacy.update_flags = channel.update_flags;
  legacy.priority = channel.priority;
  legacy.effect_voice_mask = channel.voice_mask;
  legacy.type = channel.type;
  legacy.note_length = channel.note_length;
  legacy.gate_length = channel.gate_length;
  legacy.pan = channel.pan;
  legacy.pan_slide_length = channel.pan_slide_length;
  legacy.effect_pan = channel.effect_pan;
  legacy.effect_pan_slide_step = channel.effect_pan_step;
  legacy.effect_pan_slide_length = channel.effect_pan_length;
  legacy.effect_volume = channel.effect_volume;
  legacy.effect_volume_slide_step = channel.effect_volume_step;
  legacy.effect_volume_slide_length = channel.effect_volume_length;
  legacy.effect_pitch_scale = channel.pitch_scale;
  legacy.effect_pitch_scale_slide_length = channel.pitch_scale_length;
  legacy.voice.assigned_voice = channel.assigned_voice;
  legacy.voice.dirty_flags = channel.dirty_flags;
}

// The blob each channel last started on, or 0. The legacy channel keeps one
// identity word for shared effects and blobs; a channel plays a blob while
// that word still names the blob it started on, so any other writer of the
// word (a release, a reset, a shared start) ends the program kind.
std::array<std::uintptr_t, kChannelCount> startedBlobs{};

EffectKind KindOf(std::size_t index, const VS::Audio::SeqChannel& legacy) {
  const std::uintptr_t blob = startedBlobs[index];
  return blob != 0 && legacy.effect_id == blob ? EffectKind::kProgram
                                               : EffectKind::kShared;
}

void RecordKind(std::size_t index, const EffectChannel& channel) {
  startedBlobs[index] = channel.kind == EffectKind::kProgram
                            ? static_cast<std::uintptr_t>(channel.effect)
                            : 0;
}

EffectState LoadState() {
  EffectState state;
  for (std::size_t i = 0; i < state.channels.size(); ++i) {
    state.channels[i] = LoadChannel(VS::Audio::SoundEffects::channels[i]);
    state.channels[i].kind = KindOf(i, VS::Audio::SoundEffects::channels[i]);
  }
  state.scheduler = Seq::LoadVoiceScheduler(VS::Audio::Mixer::voiceScheduler);
  state.stream_voices =
      Same<std::uint32_t>(VS::Audio::Cutscene::streamState.voice_mask);
  state.stream_control_flags =
      Same<std::int32_t>(VS::Audio::Cutscene::streamState.control_flags);
  state.driver_update_flags =
      Same<std::uint32_t>(VS::Audio::Driver::controlLatches.update_flags);
  for (std::size_t i = 0; i < state.music_voices.size(); ++i) {
    state.music_voices[i] = Same<std::uint32_t>(
        VS::Audio::Seq::musicChannels[i].voice.assigned_voice);
  }
  if (const VS::Audio::MusicConfig* active =
          VS::Audio::Seq::activeMusicConfig) {
    state.active_music_notes = Same<std::uint32_t>(active->active_notes);
  }
  return state;
}

void StoreState(const EffectState& state) {
  for (std::size_t i = 0; i < state.channels.size(); ++i) {
    StoreChannel(VS::Audio::SoundEffects::channels[i], state.channels[i]);
    RecordKind(i, state.channels[i]);
  }
  Seq::StoreVoiceScheduler(VS::Audio::Mixer::voiceScheduler, state.scheduler);
  VS::Audio::Cutscene::streamState.voice_mask =
      static_cast<int>(state.stream_voices);
  VS::Audio::Cutscene::streamState.control_flags = state.stream_control_flags;
  VS::Audio::Driver::controlLatches.update_flags = state.driver_update_flags;
  for (std::size_t i = 0; i < state.music_voices.size(); ++i) {
    VS::Audio::Seq::musicChannels[i].voice.assigned_voice =
        state.music_voices[i];
  }
  if (VS::Audio::MusicConfig* active = VS::Audio::Seq::activeMusicConfig;
      active != nullptr && state.active_music_notes) {
    active->active_notes = *state.active_music_notes;
  }
}

// FSoundCommandParams' words; extParam2 is not part of a sound-effect
// command.
EffectCommand CommandOf(const VS::Audio::CommandParams& params) {
  return {.first = SelectorOf(params.param1),
          .second = Same<std::uint32_t>(params.param2),
          .third = Same<std::uint32_t>(params.param3),
          .fourth = Same<std::uint32_t>(params.param4),
          .bank_slot = Same<std::uint32_t>(params.ext_param1)};
}

void StoreCommand(VS::Audio::CommandParams& params,
                  const EffectCommand& command) {
  params.param1 = static_cast<std::uintptr_t>(command.first);
  params.param2 = command.second;
  params.param3 = command.third;
  params.param4 = command.fourth;
  params.ext_param1 = command.bank_slot;
}

class LiveEffectHost final : public EffectHost {
 public:
  explicit LiveEffectHost(const char* operation) : operation_(operation) {}

  void ResetChannel(std::size_t index, EffectChannel& channel,
                    ProgramStart program) override {
    VS::Audio::SeqChannel& legacy = VS::Audio::SoundEffects::channels[index];
    StoreChannel(legacy, channel);
    std::uint8_t* start = program.data == DataRef::kNone
                              ? nullptr
                              : Pointer(program.data) + program.offset;
    Seq::LiveResetSoundEffectChannel(&legacy, start);
    const EffectKind kind = channel.kind;
    channel = LoadChannel(legacy);
    channel.kind = kind;
  }

  void ClearVoiceOwner(std::uint32_t voice) override {
    VS::Audio::Mixer::voiceOwners[voice] = nullptr;
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
    std::uint8_t* start = Pointer(blob);
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
void RunCommand(const char* operation, VS::Audio::CommandParams* params,
                Core core) {
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

std::optional<ProgramStart> ProgramAt(const std::uint8_t* program) {
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
void LivePlayProgramPair(VS::Audio::CommandParams* params,
                         std::uint8_t* first_program,
                         std::uint8_t* second_program, int skip_release) {
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
void LiveInvalidateMusicChannelVolumes(VS::Audio::MusicConfig* layer,
                                       VS::Audio::SeqChannel* channels) {
  if (layer == nullptr || channels == nullptr) {
    Seq::ReportRefusal("InvalidateMusicChannelVolumes",
                       "no music layer or channels");
    return;
  }
  const auto active = Same<std::uint32_t>(layer->active_channels);
  const auto used = static_cast<std::size_t>(std::bit_width(active));
  const std::span<VS::Audio::SeqChannel> table(channels, used);
  std::array<std::uint32_t, Seq::kMusicChannelCount> dirty{};
  for (std::size_t i = 0; i < table.size(); ++i) {
    dirty[i] = Same<std::uint32_t>(table[i].voice.dirty_flags);
  }
  InvalidateMusicChannelVolumes(active, dirty);
  for (std::size_t i = 0; i < table.size(); ++i) {
    table[i].voice.dirty_flags = dirty[i];
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
void LivePlayInterfaceProgramPair(VS::Audio::CommandParams* params) {
  RunCommand("34 PlayInterfaceProgramPair", params, PlayInterfaceProgramPair);
}

// decomp: SLUS_010.40 Sound_Cmd_30_unk (src/SLUS_010.40/25AC.c:3061)
void LivePlayInterfaceProgram(VS::Audio::CommandParams* params) {
  RunCommand("30 PlayInterfaceProgram", params, PlayInterfaceProgram);
}

// decomp: SLUS_010.40 Sound_Cmd_20_unk (src/SLUS_010.40/25AC.c:3074)
void LivePlaySharedProgram(VS::Audio::CommandParams* params) {
  RunCommand("20 PlaySharedProgram", params, PlaySharedProgram);
}

// decomp: SLUS_010.40 func_8001733C (src/SLUS_010.40/25AC.c:3085)
void LivePlayBlobPrograms(VS::Audio::CommandParams* params) {
  RunCommand("24 PlayBlobPrograms", params, PlayBlobPrograms);
}

// decomp: SLUS_010.40 Sound_Cmd_21_unk (src/SLUS_010.40/25AC.c:3118)
void LiveStopPrograms(VS::Audio::CommandParams* params) {
  RunCommand("21 StopPrograms", params, StopPrograms);
}

// decomp: SLUS_010.40 Sound_Cmd_A0_unk (src/SLUS_010.40/25AC.c:3250)
void LiveSetProgramVolume(VS::Audio::CommandParams* params) {
  RunCommand("A0 SetProgramVolume", params,
             [](EffectState& state, EffectHost&, EffectCommand& command) {
               SetProgramVolume(state, command);
             });
}

// decomp: SLUS_010.40 Sound_Cmd_A1_unk (src/SLUS_010.40/25AC.c:3276)
void LiveFadeProgramVolume(VS::Audio::CommandParams* params) {
  RunCommand("A1 FadeProgramVolume", params, FadeProgramVolume);
}

// decomp: SLUS_010.40 Sound_Cmd_A8_unk (src/SLUS_010.40/25AC.c:3302)
void LiveSetSceneVolume(VS::Audio::CommandParams* params) {
  RunCommand("A8 SetSceneVolume", params,
             [](EffectState& state, EffectHost&, EffectCommand& command) {
               SetSceneVolume(state, command);
             });
}

// decomp: SLUS_010.40 Sound_Cmd_A9_unk (src/SLUS_010.40/25AC.c:3318)
void LiveFadeSceneVolume(VS::Audio::CommandParams* params) {
  RunCommand("A9 FadeSceneVolume", params, FadeSceneVolume);
}

// decomp: SLUS_010.40 Sound_Cmd_A2_unk (src/SLUS_010.40/25AC.c:3339)
void LiveSetProgramPan(VS::Audio::CommandParams* params) {
  RunCommand("A2 SetProgramPan", params,
             [](EffectState& state, EffectHost&, EffectCommand& command) {
               SetProgramPan(state, command);
             });
}

// decomp: SLUS_010.40 Sound_Cmd_A3_unk (src/SLUS_010.40/25AC.c:3365)
void LiveFadeProgramPan(VS::Audio::CommandParams* params) {
  RunCommand("A3 FadeProgramPan", params, FadeProgramPan);
}

// decomp: SLUS_010.40 Sound_Cmd_AA_unk (src/SLUS_010.40/25AC.c:3391)
void LiveSetScenePan(VS::Audio::CommandParams* params) {
  RunCommand("AA SetScenePan", params,
             [](EffectState& state, EffectHost&, EffectCommand& command) {
               SetScenePan(state, command);
             });
}

// decomp: SLUS_010.40 Sound_Cmd_AB_unk (src/SLUS_010.40/25AC.c:3407)
void LiveFadeScenePan(VS::Audio::CommandParams* params) {
  RunCommand("AB FadeScenePan", params, FadeScenePan);
}
}  // namespace VS::Audio::SoundEffects
