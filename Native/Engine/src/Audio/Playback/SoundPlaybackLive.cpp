// Hand-maintained (tools/port/owned.csv).
// Live binding for SoundPlayback.hpp: the sound driver, D_800378C0 and
// func_80018C30, the active, secondary and saved music layers, the voice
// scheduler, the sound-effect channels, the cutscene stream's voices, the
// music sample bank ids and the shared sound-effect tables.
#include "Audio/SoundPlaybackLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>

#include "Audio/MixControlLive.hpp"
#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SoundControlLive.hpp"
#include "Audio/SoundDriverLive.hpp"
#include "Audio/SoundPlayback.hpp"

extern "C" {
#include "Audio/Control/SoundControl.h"
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Playback/SoundPlayback.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/Seq/Channel.h"
#include "Audio/SoundEffects/SoundEffects.h"
#include "Legacy/Psx.h"
}

namespace VS::Audio::Playback {
namespace {
static_assert(std::size(Audio_SoundEffects_channels) >= kEffectVoiceCount);
static_assert(sizeof(*Audio_Playback_soundEffectProgramOffsets) ==
              sizeof(std::uint32_t));
static_assert(sizeof(*Audio_Playback_soundEffectBankSelectors) ==
              sizeof(std::uint16_t));

// The bound shared sound-effect tables (BindLiveEffectTables).
std::optional<LiveEffectTables> bound_tables;

const char* RefusalReason(PlaybackRefusal refusal) {
  switch (refusal) {
    case PlaybackRefusal::kEffectTablesUnbound:
      return "no shared sound-effect tables are bound";
    case PlaybackRefusal::kEffectOutOfRange:
      return "the effect id lies past the bound sound-effect tables";
    case PlaybackRefusal::kProgramUnreadable:
      return "the effect program lies outside the sequence memory";
    case PlaybackRefusal::kBlobTooSmall:
      return "the sound-effect blob is shorter than its tables";
    case PlaybackRefusal::kBlobMisaligned:
      return "the sound-effect blob's tables are not word aligned";
  }
  return "refused";
}

char* Pointer(DataRef data) {
  return std::bit_cast<char*>(static_cast<std::uintptr_t>(data));
}

DataRef Reference(const void* data) {
  return static_cast<DataRef>(std::bit_cast<std::uintptr_t>(data));
}

// The memory from `start` to the end of the sequence memory it lies in.
std::span<std::byte> SequenceBytes(char* start) {
  const std::optional<Seq::LiveProgramMemory> region =
      Seq::LiveProgramRegion(start);
  if (!region) return {};
  const Seq::LiveProgramMemory from = region->From(start);
  return {reinterpret_cast<std::byte*>(from.base), from.size};
}

template <typename Element>
bool Aligned(std::span<std::byte> bytes) {
  return std::bit_cast<std::uintptr_t>(bytes.data()) % alignof(Element) == 0;
}

template <typename Element>
std::span<Element> Elements(std::span<std::byte> bytes) {
  return {reinterpret_cast<Element*>(bytes.data()),
          bytes.size() / sizeof(Element)};
}

class LivePlayback final : public PlaybackHost {
 public:
  explicit LivePlayback(const char* operation) : operation_(operation) {}

  void StartDriver() override { Driver::LiveStart(); }
  void StopDriver() override { Driver::LiveStop(); }

  CommandResult Dispatch(Command command,
                         const CommandArguments& arguments) override {
    Audio_CommandArguments& words = Audio_Driver_commandArguments;
    LiveWriteCommandArguments(arguments);
    const auto code = static_cast<unsigned>(command);
    CommandResult result;
    if (arguments.second_sequence != DataRef::kNone) {
      words.param2 = 0;
      result.value =
          Mixer::LiveDispatchCommand(code, Pointer(arguments.second_sequence));
    } else {
      result.value = Mixer::LiveDispatchCommand(code);
    }
    result.first =
        static_cast<std::uint32_t>(std::bit_cast<std::uintptr_t>(words.param1));
    result.second = static_cast<std::uint32_t>(words.param2);
    return result;
  }

  void SetMusicVolume(std::uint32_t music_id, std::int32_t volume) override {
    Control::LiveSetMusicVolume(music_id, volume);
  }

  PlaybackStatus Status() override {
    PlaybackStatus status;
    if (Audio_Seq_activeMusicConfig != nullptr) {
      status.active = Seq::LoadMusicLayer(*Audio_Seq_activeMusicConfig);
    } else {
      Seq::ReportRefusal(operation_, "no music layer is active");
    }
    if (Audio_Seq_secondaryMusicConfig != nullptr) {
      status.secondary = Seq::LoadMusicLayer(*Audio_Seq_secondaryMusicConfig);
    }
    status.transition_queued = Audio_Seq_pendingMusicData.param1 != 0;
    status.active_voices = Audio_Mixer_voiceScheduler.activeVoices;
    status.stream_voices = Audio_Cutscene_streamVoices;
    status.tempo_slide_length = Audio_Mixer_tempoScaleSlideLength;
    status.pitch_slide_length = Audio_Mixer_musicPitchScaleSlideLength;
    return status;
  }

  EffectVoice EffectVoiceAt(std::size_t voice) override {
    if (voice >= kEffectVoiceCount) return {};
    const Audio_SeqChannel& channel = Audio_SoundEffects_channels[voice];
    return {.tags = channel.effectTags,
            .effect = static_cast<EffectSelector>(
                static_cast<std::intptr_t>(channel.effectId))};
  }

  std::span<const std::byte> ProgramBytes(DataRef data) override {
    return SequenceBytes(Pointer(data));
  }

  std::optional<EffectTables> Tables() override {
    const std::optional<LiveEffectTables> tables = BoundLiveEffectTables();
    if (!tables) return std::nullopt;
    return EffectTables{.program_offsets = tables->program_offsets,
                        .bank_selectors = tables->bank_selectors};
  }

  bool BindTables(const EffectProgramBlob& blob) override {
    if (!Aligned<std::uint32_t>(blob.program_offsets) ||
        !Aligned<std::uint16_t>(blob.bank_selectors)) {
      return false;
    }
    BindLiveEffectTables(
        {.program_offsets = Elements<std::uint32_t>(blob.program_offsets),
         .bank_selectors = Elements<std::uint16_t>(blob.bank_selectors),
         .program_data = Elements<char>(blob.program_data)});
    return true;
  }

  void ClearSavedMusicId() override { Audio_Seq_savedMusicId = 0; }

  MusicBankIds MusicBanks() override {
    return {.bank = Audio_Samples_musicSampleBankIds.bankId,
            .alternate = Audio_Samples_musicSampleBankIds.alternateBankId};
  }

  void Refuse(PlaybackRefusal refusal) override {
    Seq::ReportRefusal(operation_, RefusalReason(refusal));
  }

 private:
  const char* operation_;
};
}  // namespace

void BindLiveEffectTables(const LiveEffectTables& tables) {
  bound_tables = tables;
  Audio_Playback_soundEffectProgramOffsets = tables.program_offsets.data();
  Audio_Playback_soundEffectBankSelectors = tables.bank_selectors.data();
  Audio_Playback_soundEffectProgramData = tables.program_data.data();
}

std::optional<LiveEffectTables> BoundLiveEffectTables() {
  if (!bound_tables || bound_tables->program_offsets.data() == nullptr ||
      bound_tables->program_offsets.data() !=
          Audio_Playback_soundEffectProgramOffsets ||
      bound_tables->bank_selectors.data() !=
          Audio_Playback_soundEffectBankSelectors) {
    return std::nullopt;
  }
  return bound_tables;
}

ScopedLiveEffectTables::ScopedLiveEffectTables(const LiveEffectTables& tables)
    : saved_binding_(bound_tables),
      saved_offsets_(Audio_Playback_soundEffectProgramOffsets),
      saved_selectors_(Audio_Playback_soundEffectBankSelectors),
      saved_program_data_(Audio_Playback_soundEffectProgramData) {
  BindLiveEffectTables(tables);
}

ScopedLiveEffectTables::~ScopedLiveEffectTables() {
  bound_tables = saved_binding_;
  Audio_Playback_soundEffectProgramOffsets = saved_offsets_;
  Audio_Playback_soundEffectBankSelectors = saved_selectors_;
  Audio_Playback_soundEffectProgramData = saved_program_data_;
}

// decomp: SLUS_010.40 InitSound (src/SLUS_010.40/25AC.c:195)
int LiveInitialize() {
  LivePlayback host("Initialize");
  return Initialize(host);
}

// decomp: SLUS_010.40 TeardownSound (src/SLUS_010.40/25AC.c:201)
int LiveShutdown() {
  LivePlayback host("Shutdown");
  return Shutdown(host);
}

// decomp: SLUS_010.40 Sound_BindAkaoSfxBlob (src/SLUS_010.40/25AC.c:207)
int LiveBindSoundEffectPrograms(void* blob) {
  LivePlayback host("BindSoundEffectPrograms");
  return BindSoundEffectPrograms(host, SequenceBytes(static_cast<char*>(blob)));
}

// decomp: SLUS_010.40 func_80011EBC (src/SLUS_010.40/25AC.c:243)
int LiveMusicVolume(unsigned music_id) {
  LivePlayback host("MusicVolume");
  return MusicVolume(host, music_id);
}

// decomp: SLUS_010.40 func_80011F08 (src/SLUS_010.40/25AC.c:256)
int LivePackedMusicSampleBankIds() {
  LivePlayback host("PackedMusicSampleBankIds");
  return PackedMusicSampleBankIds(host);
}

// decomp: SLUS_010.40 func_80011F90 (src/SLUS_010.40/25AC.c:278)
int LiveMusicPosition() {
  LivePlayback host("MusicPosition");
  return MusicPosition(host);
}

// decomp: SLUS_010.40 func_80011FB4 (src/SLUS_010.40/25AC.c:286)
unsigned LiveStartMusic(void* sequence) {
  LivePlayback host("StartMusic");
  return StartMusic(host, Reference(sequence));
}

// decomp: SLUS_010.40 func_80011FDC (src/SLUS_010.40/25AC.c:292)
void LiveStopMusic(unsigned music_id) {
  LivePlayback host("StopMusic");
  StopMusic(host, music_id);
}

// decomp: SLUS_010.40 func_80012004 (src/SLUS_010.40/25AC.c:298)
void LiveSaveMusicState() {
  LivePlayback host("SaveMusicState");
  SaveMusicState(host);
}

// decomp: SLUS_010.40 func_80012024 (src/SLUS_010.40/25AC.c:300)
int LiveInvalidateSavedMusicId() {
  LivePlayback host("InvalidateSavedMusicId");
  return InvalidateSavedMusicId(host);
}

// decomp: SLUS_010.40 func_80012080 (src/SLUS_010.40/25AC.c:318)
unsigned LiveCrossfadeMusic(void* sequence, unsigned fade_ticks,
                            int target_volume) {
  LivePlayback host("CrossfadeMusic");
  return CrossfadeMusic(host, Reference(sequence), fade_ticks, target_volume);
}

// decomp: SLUS_010.40 func_800120E8 (src/SLUS_010.40/25AC.c:333)
unsigned LiveQueueMusicTransition(void* sequence) {
  LivePlayback host("QueueMusicTransition");
  return QueueMusicTransition(host, Reference(sequence));
}

// decomp: SLUS_010.40 func_8001217C (src/SLUS_010.40/25AC.c:352)
int LivePlaySoundEffect(std::uint32_t effect_id, int effect_tag, int pan,
                        int volume) {
  LivePlayback host("PlaySoundEffect");
  return PlaySoundEffect(host, static_cast<std::uint32_t>(effect_id),
                         effect_tag, pan, volume);
}

// decomp: SLUS_010.40 func_800121F0 (src/SLUS_010.40/25AC.c:365)
std::uintptr_t LivePlaySoundEffectBlob(void* program, int effect_tag, int pan,
                                       int volume) {
  LivePlayback host("PlaySoundEffectBlob");
  const std::optional<DataRef> played =
      PlaySoundEffectBlob(host, Reference(program), effect_tag, pan, volume);
  if (!played) return static_cast<std::uintptr_t>(-1);
  return static_cast<std::uintptr_t>(*played);
}

// decomp: SLUS_010.40 func_80012288 (src/SLUS_010.40/25AC.c:381)
void LiveStopSoundEffects(std::intptr_t selector, int effect_tag_mask) {
  LivePlayback host("StopSoundEffects");
  StopSoundEffects(host, static_cast<EffectSelector>(selector),
                   effect_tag_mask);
}

// decomp: SLUS_010.40 func_80012358 (src/SLUS_010.40/25AC.c:421)
std::intptr_t LiveSoundEffectIdForTag(int effect_tag) {
  LivePlayback host("SoundEffectIdForTag");
  return static_cast<std::intptr_t>(SoundEffectIdForTag(host, effect_tag));
}

// decomp: SLUS_010.40 func_800123C8 (src/SLUS_010.40/25AC.c:439)
int LiveSharedEffectUsesTags(std::int32_t effect_id) {
  LivePlayback host("UsesSoundEffectTags");
  return UsesSoundEffectTags(
      host, SharedEffect{.id = static_cast<std::uint32_t>(effect_id)});
}

// decomp: SLUS_010.40 func_800123C8 (src/SLUS_010.40/25AC.c:439)
int LiveProgramUsesTags(void* program) {
  LivePlayback host("UsesSoundEffectTags");
  return UsesSoundEffectTags(host, Reference(program));
}

// decomp: SLUS_010.40 vs_sound_setMonoSound (src/SLUS_010.40/25AC.c:450)
void LiveSetMonoOutput(int mono_enabled) {
  LivePlayback host("SetMonoOutput");
  SetMonoOutput(host, mono_enabled);
}

void LiveWriteCommandArguments(const CommandArguments& arguments) {
  Audio_CommandArguments& words = Audio_Driver_commandArguments;
  if (const auto* const data = std::get_if<DataRef>(&arguments.first)) {
    words.param1 = Pointer(*data);
  } else if (const auto* const number =
                 std::get_if<std::intptr_t>(&arguments.first)) {
    words.param1 = std::bit_cast<void*>(*number);
  }
  if (arguments.second) words.param2 = static_cast<int>(*arguments.second);
  if (arguments.third) words.param3 = static_cast<int>(*arguments.third);
  if (arguments.fourth) words.param4 = static_cast<int>(*arguments.fourth);
}
}  // namespace VS::Audio::Playback
