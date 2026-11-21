// Hand-maintained (tools/port/owned.csv).
// Live binding for SoundBanks.hpp: the slot record (vs_main_soundData), the
// sound-effect and wave bank file tables, _soundFileMap, the menu sound
// effects (_sfxData), the channel groups' next tags (D_8005FE70-D_8005FE84),
// the disc request queue, the high end of the game heap and the playback and
// sample bank bindings.
#include "Audio/SoundBanksLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>

#include "Audio/LegacyFiles.hpp"
#include "Audio/SampleBanksLive.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SfxChannelGroups.hpp"
#include "Audio/SoundBanks.hpp"
#include "Audio/SoundPlayback.hpp"
#include "Audio/SoundPlaybackLive.hpp"
#include "Audio/SoundSlotsLive.hpp"
#include "Audio/SoundState.hpp"
#include "Audio/StreamPlayback.hpp"
#include "Audio/XaMixLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscLayout.h"
#include "Content/DiscQueue.hpp"
#include "Core/Runtime.hpp"
#include "Input/Settings.hpp"
#include "Legacy/Psx.hpp"
#include "State/GameClockLive.hpp"

namespace VS::Audio {
namespace {
static_assert(std::size(VS::Audio::soundState.sfx_bank_data) ==
              kSfxBankSlotCount);
static_assert(std::size(VS::Audio::soundState.sfx_bank_requests) ==
              kSfxBankSlotCount);
static_assert(std::size(VS::Audio::soundState.music_handles) ==
              kMusicSlotCount);
static_assert(std::size(VS::Audio::soundState.sfx_stream_data) ==
              kEffectStreamSlotCount);

// The file tables' entries (ModulePointers.cpp: 400-byte word tables at
// 0x8004A9E0, 0x8004B020, 0x8004A530 and 0x8004AB70, and the 200-byte map at
// 0x8004B1B0); the generated declarations leave them unsized.
constexpr std::int32_t kSfxBankFileCount = 100;
constexpr std::int32_t kWaveBankFileCount = 100;
constexpr std::int32_t kWaveBankMapEntries = 200;

const char* RefusalReason(SoundBankRefusal refusal) {
  switch (refusal) {
    case SoundBankRefusal::kSfxBankFileOutOfRange:
      return "the sound-effect bank id is past the file table";
    case SoundBankRefusal::kWaveBankFileOutOfRange:
      return "the wave bank file is past the file table";
    case SoundBankRefusal::kWaveBankMapOutOfRange:
      return "the wave bank id is past _soundFileMap";
  }
  return "refused";
}

// The room effect bank's address as its handle.
BankBuffer BufferAt(std::uintptr_t address) {
  return LiveBankBuffer(std::bit_cast<const void*>(address));
}

class LiveSoundBanks final : public SoundBankHost {
 public:
  explicit LiveSoundBanks(const char* operation) : operation_(operation) {}

  SfxBankSlot SfxBank(std::size_t index) override {
    if (index >= kSfxBankSlotCount) return {};
    return {
        .id =
            static_cast<std::int8_t>(VS::Audio::soundState.sfx_bank_ids[index]),
        .data = LiveBankBuffer(VS::Audio::soundState.sfx_bank_data[index]),
        .request =
            LiveBankRequest(VS::Audio::soundState.sfx_bank_requests[index])};
  }

  void StoreSfxBank(std::size_t index, const SfxBankSlot& slot) override {
    if (index >= kSfxBankSlotCount) return;
    VS::Audio::soundState.sfx_bank_ids[index] =
        static_cast<std::int8_t>(slot.id);
    VS::Audio::soundState.sfx_bank_data[index] = LiveBufferPointer(slot.data);
    VS::Audio::soundState.sfx_bank_requests[index] =
        LiveRequestSlot(slot.request);
  }

  std::uint32_t CurrentSfxBank() override {
    return VS::Audio::soundState.current_sfx_bank;
  }

  void StoreCurrentSfxBank(std::uint32_t slot) override {
    VS::Audio::soundState.current_sfx_bank = slot;
  }

  WaveBankSlot WaveBank() override {
    return {
        .file = VS::Audio::soundState.wave_bank_id,
        .data = LiveBankBuffer(VS::Audio::soundState.wave_bank_data),
        .request = LiveBankRequest(VS::Audio::soundState.wave_bank_request)};
  }

  void StoreWaveBank(const WaveBankSlot& bank) override {
    VS::Audio::soundState.wave_bank_id = bank.file;
    VS::Audio::soundState.wave_bank_data = LiveBufferPointer(bank.data);
    VS::Audio::soundState.wave_bank_request = LiveRequestSlot(bank.request);
  }

  void StoreSoundSlots(const SoundSlots& slots) override {
    VS::Audio::SoundState& state = VS::Audio::soundState;
    state.current_music_slot = slots.current_music_slot;
    state.playing_music_handle = slots.playing_music_handle;
    state.current_music_id = slots.current_music_id;
    state.current_music_data = LiveBufferPointer(slots.current_music_data);
    for (std::size_t slot = 0; slot < kMusicSlotCount; ++slot) {
      const MusicSlot& music = slots.music.at(slot);
      state.music_ids[slot] = static_cast<std::uint8_t>(music.id);
      state.music_handles[slot] = music.handle;
      state.music_data[slot] = LiveBufferPointer(music.data);
      state.music_requests[slot] = LiveRequestSlot(music.request);
    }
    state.current_sfx_bank = slots.current_sfx_bank;
    for (std::size_t slot = 0; slot < kSfxBankSlotCount; ++slot) {
      StoreSfxBank(slot, slots.sfx_banks.at(slot));
    }
    for (std::size_t slot = 0; slot < kEffectStreamSlotCount; ++slot) {
      const EffectStreamSlot& stream = slots.effect_streams.at(slot);
      state.sfx_stream_data[slot] = LiveBufferPointer(stream.data);
      state.reserved70[slot] =
          static_cast<int*>(LiveBufferPointer(stream.reserved));
    }
    StoreWaveBank(slots.wave_bank);
  }

  void StoreEffectTagCursors(const EffectTagCursors& cursors) override {
    VS::Audio::nextSfxChannelGroup1 = cursors.group_1;
    VS::Audio::nextSfxChannelGroup7E = cursors.group_7e;
    VS::Audio::nextSfxChannelGroup180 = cursors.group_180;
    VS::Audio::nextSfxChannelGroup200 = cursors.group_200;
    VS::Audio::nextSfxChannelGroupFF000 = cursors.group_ff000;
    VS::Audio::nextSfxChannelGroupF00000 = cursors.group_f00000;
  }

  std::optional<Content::DiscFileExtent> SfxBankFile(std::int32_t id) override {
    return LiveTableFile(VS::Audio::legacySfxBankDiscOffsets,
                         VS::Audio::legacySfxBankFileSizes, kSfxBankFileCount,
                         id);
  }

  std::optional<Content::DiscFileExtent> WaveBankFile(
      std::int32_t file) override {
    return LiveTableFile(VS::Audio::legacyWaveBankDiscOffsets,
                         VS::Audio::legacyWaveBankFileSizes, kWaveBankFileCount,
                         file);
  }

  std::optional<std::int32_t> MappedWaveBankFile(std::int32_t id) override {
    if (id < 0 || id >= kWaveBankMapEntries) return std::nullopt;
    return VS::Audio::kWaveBankFileMap[static_cast<std::size_t>(id)];
  }

  Content::DiscFileExtent MenuFile(MenuSoundFile file) override {
    switch (file) {
      case MenuSoundFile::kWave0000:
        return {.sector = VS_WAVE0000_DAT_LBA, .bytes = VS_WAVE0000_DAT_SIZE};
      case MenuSoundFile::kWave0005:
        return {.sector = VS_WAVE0005_DAT_LBA, .bytes = VS_WAVE0005_DAT_SIZE};
      case MenuSoundFile::kWave0200:
        return {.sector = VS_WAVE0200_DAT_LBA, .bytes = VS_WAVE0200_DAT_SIZE};
      case MenuSoundFile::kEffect00:
        return {.sector = VS_EFFECT00_DAT_LBA, .bytes = VS_EFFECT00_DAT_SIZE};
    }
    return {};
  }

  BankBuffer MenuSoundEffects() override {
    return LiveBankBuffer(VS::Audio::menuSoundEffects);
  }

  std::optional<DiscRequestRef> AllocateRequest(
      const Content::DiscFileExtent& file) override {
    return AllocateLiveSlotRequest(file);
  }

  void EnqueueRequest(DiscRequestRef request, BankBuffer destination) override {
    EnqueueLiveSlotRequest(request, destination);
  }

  DiscRequestRef CompleteRefusedRequest(const Content::DiscFileExtent& file,
                                        BankBuffer destination) override {
    return CompleteLiveRefusedSlotRequest(file, destination);
  }

  Content::DiscRequestState RequestState(DiscRequestRef request) override {
    return LiveSlotRequestState(request);
  }

  void MarkRequestDispatched(DiscRequestRef request) override {
    if (VS::Content::DiscQueueSlot* slot = LiveRequestSlot(
            {.mark = RequestMark::kActive, .request = request})) {
      slot->state = VS::Content::DiscRequestState::kDispatched;
    }
  }

  void ReleaseRequest(DiscRequestRef request) override {
    ReleaseLiveSlotRequest(request);
  }

  void ReadFileBlocking(const Content::DiscFileExtent& file,
                        BankBuffer destination) override {
    Content::ReadLiveDiscFileBlocking(file.sector, file.bytes,
                                      LiveBufferPointer(destination));
  }

  void WaitFrame() override { State::UpdateLivePlayTimeAndLoading(); }

  BankBuffer AllocateBuffer(std::int32_t bytes) override {
    return AllocateLiveSlotBuffer(bytes);
  }

  void FreeBuffer(BankBuffer buffer) override { FreeLiveSlotBuffer(buffer); }

  void InitializePlayback() override { Playback::LiveInitialize(); }

  void SetDiscMixVolume(std::uint32_t volume) override {
    Control::SetLiveDiscMixVolume(volume);
  }

  void BindSoundEffectPrograms(BankBuffer blob) override {
    Playback::LiveBindSoundEffectPrograms(LiveBufferPointer(blob));
  }

  void StopSoundEffects(Playback::EffectSelector selector,
                        std::int32_t effect_tag_mask) override {
    Playback::LiveStopSoundEffects(static_cast<std::intptr_t>(selector),
                                   effect_tag_mask);
  }

  void SetMonoOutput(std::int32_t mono_enabled) override {
    Playback::LiveSetMonoOutput(mono_enabled);
  }

  // _loadSoundFile's split of the packed ids: the alternate bank in the
  // high half, rounded toward zero, and the bank below it.
  Playback::MusicBankIds MusicBanks() override {
    constexpr std::int32_t kHalf = 0x10000;
    const std::int32_t packed = Playback::LivePackedMusicSampleBankIds();
    return {.bank = packed % kHalf, .alternate = packed / kHalf};
  }

  void LoadBank(BankBuffer bank, bool wait) override {
    Samples::LiveLoadBank(LiveBufferPointer(bank), wait ? 1 : 0);
  }

  void LoadSoundEffectBank(BankBuffer bank, std::int32_t slot,
                           bool wait) override {
    Samples::LiveLoadSoundEffectBank(LiveBufferPointer(bank), slot,
                                     wait ? 1 : 0);
  }

  void LoadMusicBank(BankBuffer bank, bool wait) override {
    Samples::LiveLoadMusicBank(LiveBufferPointer(bank), wait ? 1 : 0);
  }

  std::int32_t TransferStatus() override {
    return Samples::LiveTransferStatus();
  }

  void ResetCutsceneStream() override { VS::Audio::ResetCutsceneStream(); }

  void Diagnose(std::uint32_t code) override {
    Core::IgnoreDiagnostic(static_cast<int>(code), 0);
  }

  void Refuse(SoundBankRefusal refusal) override {
    Seq::ReportRefusal(operation_, RefusalReason(refusal));
  }

 private:
  const char* operation_;
};
}  // namespace

// decomp: SLUS_010.40 vs_main_loadSfxSlot (src/SLUS_010.40/main.c:10045)
int LiveLoadSfxBankSlot(int id, unsigned slot) {
  LiveSoundBanks host("LoadSfxBankSlot");
  return LoadSfxBankSlot(host, id, slot);
}

// decomp: SLUS_010.40 _loadSfx (src/SLUS_010.40/main.c:10077)
int LiveLoadSfxBankIntoFreeSlot(int id) {
  LiveSoundBanks host("LoadSfxBankIntoFreeSlot");
  return LoadSfxBankIntoFreeSlot(host, id);
}

// decomp: SLUS_010.40 vs_main_freeSfxQueueSlot (src/SLUS_010.40/main.c:10089)
int LivePollSfxBankSlotLoad(unsigned slot) {
  LiveSoundBanks host("PollSfxBankSlotLoad");
  return PollSfxBankSlotLoad(host, slot);
}

// decomp: SLUS_010.40 _freeSfxSlots (src/SLUS_010.40/main.c:10111)
int LivePollSfxBankLoads() {
  LiveSoundBanks host("PollSfxBankLoads");
  return PollSfxBankLoads(host);
}

// decomp: SLUS_010.40 _loadAndFreeSfx (src/SLUS_010.40/main.c:10121)
int LiveLoadSfxBankBlocking(int id) {
  LiveSoundBanks host("LoadSfxBankBlocking");
  return LoadSfxBankBlocking(host, id);
}

// decomp: SLUS_010.40 vs_main_setCurrentSfx (src/SLUS_010.40/main.c:10134)
int LiveSelectSfxBankSlot(int slot) {
  LiveSoundBanks host("SelectSfxBankSlot");
  return SelectSfxBankSlot(host, slot);
}

// decomp: SLUS_010.40 vs_main_freeSfx (src/SLUS_010.40/main.c:10143)
int LiveFreeSfxBankSlot(unsigned slot) {
  LiveSoundBanks host("FreeSfxBankSlot");
  return FreeSfxBankSlot(host, slot);
}

// decomp: SLUS_010.40 func_80046168 (src/SLUS_010.40/main.c:10161)
int LiveBindRoomSfxBank(std::uintptr_t bank_address) {
  LiveSoundBanks host("BindRoomSfxBank");
  return BindRoomSfxBank(host, BufferAt(bank_address));
}

// decomp: SLUS_010.40 func_80046194 (src/SLUS_010.40/main.c:10170)
int LiveUnbindRoomSfxBank() {
  LiveSoundBanks host("UnbindRoomSfxBank");
  return UnbindRoomSfxBank(host);
}

// decomp: SLUS_010.40 _loadSoundFile (src/SLUS_010.40/main.c:10359)
void LiveLoadWaveBankFile(int file) {
  LiveSoundBanks host("LoadWaveBankFile");
  LoadWaveBankFile(host, file);
}

// decomp: SLUS_010.40 vs_main_loadSoundFile (src/SLUS_010.40/main.c:10394)
void LiveLoadMappedWaveBank(int id) {
  LiveSoundBanks host("LoadMappedWaveBank");
  LoadMappedWaveBank(host, id);
}

// decomp: SLUS_010.40 vs_main_processSoundQueue (src/SLUS_010.40/main.c:10396)
int LiveUpdateWaveBankLoad() {
  LiveSoundBanks host("UpdateWaveBankLoad");
  return UpdateWaveBankLoad(host);
}

// decomp: SLUS_010.40 vs_main_loadAndWaitSoundSlot
// (src/SLUS_010.40/main.c:10439)
void LiveLoadWaveBankBlocking(int id) {
  LiveSoundBanks host("LoadWaveBankBlocking");
  LoadWaveBankBlocking(host, id);
}

// decomp: SLUS_010.40 vs_main_setMonoSound (src/SLUS_010.40/main.c:10448)
void LiveSetMonoOutput(int mono_enabled) {
  LiveSoundBanks host("SetMonoOutput");
  SetMonoOutput(host, mono_enabled);
}

void LiveSetSoundOutput(Input::SoundOutput output) {
  LiveSetMonoOutput(
      output == Input::SoundOutput::kMono ? Playback::kMonoRequested : 0);
}

// decomp: SLUS_010.40 _loadMenuSound (src/SLUS_010.40/main.c:10452)
void LiveInitializeMenuSoundBanks() {
  LiveSoundBanks host("InitializeMenuSoundBanks");
  InitializeMenuSoundBanks(host);
}

// decomp: SLUS_010.40 vs_main_resetSound (src/SLUS_010.40/main.c:10480)
void LiveResetSoundSlots() {
  LiveSoundBanks host("ResetSoundSlots");
  ResetSoundSlots(host);
}
}  // namespace VS::Audio
