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

#include "Audio/SampleBanksLive.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SoundBanks.hpp"
#include "Audio/SoundPlaybackLive.hpp"
#include "Audio/SoundSlotsLive.hpp"
#include "Audio/StreamPlayback.h"
#include "Audio/XaMixLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Core/Runtime.hpp"
#include "State/GameClockLive.hpp"

extern "C" {
#include "Audio/LegacyFiles.h"
#include "Audio/Sfx.h"
#include "Audio/SoundBanks.h"
#include "Content/DiscLayout.h"
#include "DiscQueue.h"
#include "Legacy/Psx.h"
}

namespace VS::Audio {
namespace {
static_assert(std::size(Audio_soundState.sfxBankData) == kSfxBankSlotCount);
static_assert(std::size(Audio_soundState.sfxBankRequests) == kSfxBankSlotCount);
static_assert(std::size(Audio_soundState.musicHandles) == kMusicSlotCount);
static_assert(std::size(Audio_soundState.sfxStreamData) ==
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
        .id = static_cast<std::int8_t>(Audio_soundState.sfxBankIds[index]),
        .data = LiveBankBuffer(Audio_soundState.sfxBankData[index]),
        .request = LiveBankRequest(Audio_soundState.sfxBankRequests[index])};
  }

  void StoreSfxBank(std::size_t index, const SfxBankSlot& slot) override {
    if (index >= kSfxBankSlotCount) return;
    Audio_soundState.sfxBankIds[index] = static_cast<char>(slot.id);
    Audio_soundState.sfxBankData[index] = LiveBufferPointer(slot.data);
    Audio_soundState.sfxBankRequests[index] = LiveRequestSlot(slot.request);
  }

  std::uint32_t CurrentSfxBank() override {
    return Audio_soundState.currentSfxBank;
  }

  void StoreCurrentSfxBank(std::uint32_t slot) override {
    Audio_soundState.currentSfxBank = slot;
  }

  WaveBankSlot WaveBank() override {
    return {.file = Audio_soundState.waveBankId,
            .data = LiveBankBuffer(Audio_soundState.waveBankData),
            .request = LiveBankRequest(Audio_soundState.waveBankRequest)};
  }

  void StoreWaveBank(const WaveBankSlot& bank) override {
    Audio_soundState.waveBankId = bank.file;
    Audio_soundState.waveBankData = LiveBufferPointer(bank.data);
    Audio_soundState.waveBankRequest = LiveRequestSlot(bank.request);
  }

  void StoreSoundSlots(const SoundSlots& slots) override {
    Audio_SoundState& state = Audio_soundState;
    state.currentMusicSlot = slots.current_music_slot;
    state.playingMusicHandle = slots.playing_music_handle;
    state.currentMusicId = slots.current_music_id;
    state.currentMusicData = LiveBufferPointer(slots.current_music_data);
    for (std::size_t slot = 0; slot < kMusicSlotCount; ++slot) {
      const MusicSlot& music = slots.music.at(slot);
      state.musicIds[slot] = static_cast<char>(music.id);
      state.musicHandles[slot] = music.handle;
      state.musicData[slot] = LiveBufferPointer(music.data);
      state.musicRequests[slot] = LiveRequestSlot(music.request);
    }
    state.currentSfxBank = slots.current_sfx_bank;
    for (std::size_t slot = 0; slot < kSfxBankSlotCount; ++slot) {
      StoreSfxBank(slot, slots.sfx_banks.at(slot));
    }
    for (std::size_t slot = 0; slot < kEffectStreamSlotCount; ++slot) {
      const EffectStreamSlot& stream = slots.effect_streams.at(slot);
      state.sfxStreamData[slot] = LiveBufferPointer(stream.data);
      state.reserved70[slot] =
          static_cast<int*>(LiveBufferPointer(stream.reserved));
    }
    StoreWaveBank(slots.wave_bank);
  }

  void StoreEffectTagCursors(const EffectTagCursors& cursors) override {
    Audio_nextSfxChannelGroup1 = cursors.group_1;
    Audio_nextSfxChannelGroup7E = cursors.group_7e;
    Audio_nextSfxChannelGroup180 = cursors.group_180;
    Audio_nextSfxChannelGroup200 = cursors.group_200;
    Audio_nextSfxChannelGroupFF000 = cursors.group_ff000;
    Audio_nextSfxChannelGroupF00000 = cursors.group_f00000;
  }

  std::optional<Content::DiscFileExtent> SfxBankFile(std::int32_t id) override {
    return LiveTableFile(Audio_legacySfxBankDiscOffsets,
                         Audio_legacySfxBankFileSizes, kSfxBankFileCount, id);
  }

  std::optional<Content::DiscFileExtent> WaveBankFile(
      std::int32_t file) override {
    return LiveTableFile(Audio_legacyWaveBankDiscOffsets,
                         Audio_legacyWaveBankFileSizes, kWaveBankFileCount,
                         file);
  }

  std::optional<std::int32_t> MappedWaveBankFile(std::int32_t id) override {
    if (id < 0 || id >= kWaveBankMapEntries) return std::nullopt;
    return Audio_waveBankFileMap[static_cast<std::size_t>(id)];
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
    return LiveBankBuffer(Audio_menuSoundEffects);
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
    if (Content_DiscQueueSlot* slot = LiveRequestSlot(
            {.mark = RequestMark::kActive, .request = request})) {
      slot->state = Content_DiscRequestState_Dispatched;
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

  void WaitFrame() override { State::UpdateLivePlayTimeAndLoading(0); }

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

  void ResetCutsceneStream() override { Audio_Native_ResetCutsceneStream(); }

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
