// Hand-maintained (tools/port/owned.csv).
// Typed sound bank slots (SoundBanks.hpp).
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>

#include "Audio/SoundBanks.hpp"

namespace VS::Audio {
namespace {
constexpr std::size_t kRoomSfxBankIndex = kRoomSfxBankSlot - 1;

// A slot numbered from 1 as its index; nullopt outside 1 to
// kSfxBankSlotCount (the original's unsigned `slot - 1 < 3`).
std::optional<std::size_t> SfxSlotIndex(std::uint32_t slot) {
  if (slot - 1U < kSfxBankSlotCount) return slot - 1U;
  return std::nullopt;
}

BankRequest ActiveRequest(DiscRequestRef request) {
  return {.mark = RequestMark::kActive, .request = request};
}

struct HeldDiagnostics {
  std::uint32_t request = 0;
  std::uint32_t buffer = 0;
};

// The load both slot loaders start: reports a request or buffer the slot
// still held, allocates the request and the buffer from the high end, then
// enqueues the request, or reads the file without the queue when the queue
// is full.
template <typename Slot>
void StartLoad(SoundBankHost& host, const Content::DiscFileExtent& file,
               const HeldDiagnostics& diagnostics, Slot& slot) {
  if (slot.request.mark == RequestMark::kActive) {
    host.Diagnose(diagnostics.request);
  }
  const std::optional<DiscRequestRef> request = host.AllocateRequest(file);
  if (slot.data != BankBuffer::kNone) host.Diagnose(diagnostics.buffer);
  slot.data = host.AllocateBuffer(file.bytes);
  if (request) {
    host.EnqueueRequest(*request, slot.data);
    slot.request = ActiveRequest(*request);
  } else {
    slot.request = ActiveRequest(host.CompleteRefusedRequest(file, slot.data));
  }
}

// Releases the wave bank's request and buffer and empties it.
void DiscardWaveBank(SoundBankHost& host, const WaveBankSlot& bank) {
  host.ReleaseRequest(bank.request.request);
  host.FreeBuffer(bank.data);
  host.StoreWaveBank(WaveBankSlot{});
}

void StopBankSoundEffects(SoundBankHost& host, std::int32_t effect_tags) {
  host.StopSoundEffects(kUntaggedEffects, 0);
  host.StopSoundEffects(Playback::EffectSelector::kNone, effect_tags);
}

// Reads a menu file into a buffer from the high end.
BankBuffer ReadMenuFile(SoundBankHost& host, MenuSoundFile file) {
  const Content::DiscFileExtent extent = host.MenuFile(file);
  const BankBuffer buffer = host.AllocateBuffer(extent.bytes);
  host.ReadFileBlocking(extent, buffer);
  return buffer;
}
}  // namespace

// decomp: SLUS_010.40 vs_main_loadSfxSlot (src/SLUS_010.40/main.c:10045)
std::int32_t LoadSfxBankSlot(SoundBankHost& host, std::int32_t id,
                             std::uint32_t slot) {
  const std::optional<std::size_t> index = SfxSlotIndex(slot);
  if (!index) return 0;
  SfxBankSlot bank = host.SfxBank(*index);
  if (bank.data != BankBuffer::kNone) return 0;
  const std::optional<Content::DiscFileExtent> file = host.SfxBankFile(id);
  if (!file) {
    host.Refuse(SoundBankRefusal::kSfxBankFileOutOfRange);
    return 0;
  }
  bank.id = static_cast<std::int8_t>(id);
  StartLoad(host, *file,
            {.request = kSfxRequestHeldDiagnostic,
             .buffer = kSfxBufferHeldDiagnostic},
            bank);
  host.StoreSfxBank(*index, bank);
  return static_cast<std::int32_t>(slot);
}

// decomp: SLUS_010.40 _loadSfx (src/SLUS_010.40/main.c:10077)
std::int32_t LoadSfxBankIntoFreeSlot(SoundBankHost& host, std::int32_t id) {
  for (std::size_t index = 0; index < kSfxBankSlotCount; ++index) {
    if (host.SfxBank(index).data == BankBuffer::kNone) {
      return LoadSfxBankSlot(host, id, static_cast<std::uint32_t>(index + 1));
    }
  }
  return 0;
}

// decomp: SLUS_010.40 vs_main_freeSfxQueueSlot (src/SLUS_010.40/main.c:10089)
std::int32_t PollSfxBankSlotLoad(SoundBankHost& host, std::uint32_t slot) {
  const std::optional<std::size_t> index = SfxSlotIndex(slot);
  if (!index) return kLoadIdle;
  SfxBankSlot bank = host.SfxBank(*index);
  if (bank.request.mark != RequestMark::kActive) return kLoadIdle;
  switch (host.RequestState(bank.request.request)) {
    case Content::DiscRequestState::kLoaded:
      host.ReleaseRequest(bank.request.request);
      bank.request = BankRequest{};
      host.StoreSfxBank(*index, bank);
      return kLoadIdle;
    case Content::DiscRequestState::kError:
      host.ReleaseRequest(bank.request.request);
      bank.request = BankRequest{.mark = RequestMark::kFailed};
      host.StoreSfxBank(*index, bank);
      return kLoadFailed;
    default:
      return kLoadInFlight;
  }
}

// decomp: SLUS_010.40 _freeSfxSlots (src/SLUS_010.40/main.c:10111)
std::int32_t PollSfxBankLoads(SoundBankHost& host) {
  std::int32_t loads = kLoadIdle;
  for (std::uint32_t slot = 1; slot <= kSfxBankSlotCount; ++slot) {
    loads |= PollSfxBankSlotLoad(host, slot);
  }
  return loads;
}

// decomp: SLUS_010.40 _loadAndFreeSfx (src/SLUS_010.40/main.c:10121)
std::int32_t LoadSfxBankBlocking(SoundBankHost& host, std::int32_t id) {
  const std::int32_t slot = LoadSfxBankIntoFreeSlot(host, id);
  if (slot != 0) {
    while (PollSfxBankLoads(host) != kLoadIdle) host.WaitFrame();
  }
  return slot;
}

// decomp: SLUS_010.40 vs_main_setCurrentSfx (src/SLUS_010.40/main.c:10134)
std::int32_t SelectSfxBankSlot(SoundBankHost& host, std::int32_t slot) {
  const std::optional<std::size_t> index =
      SfxSlotIndex(static_cast<std::uint32_t>(slot));
  if (!index || host.SfxBank(*index).data == BankBuffer::kNone) return 0;
  host.StoreCurrentSfxBank(static_cast<std::uint32_t>(slot));
  return 1;
}

// decomp: SLUS_010.40 vs_main_freeSfx (src/SLUS_010.40/main.c:10143)
std::int32_t FreeSfxBankSlot(SoundBankHost& host, std::uint32_t slot) {
  const std::optional<std::size_t> index = SfxSlotIndex(slot);
  if (!index) return 0;
  const BankBuffer data = host.SfxBank(*index).data;
  if (data == BankBuffer::kNone) return 0;
  if (host.CurrentSfxBank() == slot) host.StoreCurrentSfxBank(0);
  StopBankSoundEffects(host, kSfxBankEffectTags);
  host.FreeBuffer(data);
  SfxBankSlot bank = host.SfxBank(*index);
  bank.id = 0;
  bank.data = BankBuffer::kNone;
  host.StoreSfxBank(*index, bank);
  return 1;
}

// decomp: SLUS_010.40 func_80046168 (src/SLUS_010.40/main.c:10161)
std::int32_t BindRoomSfxBank(SoundBankHost& host, BankBuffer bank) {
  SfxBankSlot slot = host.SfxBank(kRoomSfxBankIndex);
  if (slot.data != BankBuffer::kNone) return 0;
  slot.data = bank;
  host.StoreSfxBank(kRoomSfxBankIndex, slot);
  return 1;
}

// decomp: SLUS_010.40 func_80046194 (src/SLUS_010.40/main.c:10170)
std::int32_t UnbindRoomSfxBank(SoundBankHost& host) {
  StopBankSoundEffects(host, kRoomSfxBankEffectTags);
  SfxBankSlot slot = host.SfxBank(kRoomSfxBankIndex);
  slot.data = BankBuffer::kNone;
  host.StoreSfxBank(kRoomSfxBankIndex, slot);
  return 1;
}

// decomp: SLUS_010.40 _loadSoundFile (src/SLUS_010.40/main.c:10359)
void LoadWaveBankFile(SoundBankHost& host, std::int32_t file) {
  const Playback::MusicBankIds loaded = host.MusicBanks();
  if (loaded.bank == file || loaded.alternate == file) return;
  const std::optional<Content::DiscFileExtent> extent = host.WaveBankFile(file);
  if (!extent) {
    host.Refuse(SoundBankRefusal::kWaveBankFileOutOfRange);
    return;
  }
  WaveBankSlot bank = host.WaveBank();
  StartLoad(host, *extent,
            {.request = kWaveRequestHeldDiagnostic,
             .buffer = kWaveBufferHeldDiagnostic},
            bank);
  bank.file = file;
  host.StoreWaveBank(bank);
}

// decomp: SLUS_010.40 vs_main_loadSoundFile (src/SLUS_010.40/main.c:10394)
void LoadMappedWaveBank(SoundBankHost& host, std::int32_t id) {
  const std::optional<std::int32_t> file = host.MappedWaveBankFile(id);
  if (!file) {
    host.Refuse(SoundBankRefusal::kWaveBankMapOutOfRange);
    return;
  }
  LoadWaveBankFile(host, *file);
}

// decomp: SLUS_010.40 vs_main_processSoundQueue (src/SLUS_010.40/main.c:10396)
std::int32_t UpdateWaveBankLoad(SoundBankHost& host) {
  const WaveBankSlot bank = host.WaveBank();
  if (bank.request.mark != RequestMark::kActive) return kLoadIdle;
  switch (host.RequestState(bank.request.request)) {
    case Content::DiscRequestState::kDispatched: {
      const std::int32_t status = host.TransferStatus();
      if (status == kTransferFinished) {
        DiscardWaveBank(host, bank);
        return kLoadIdle;
      }
      if (status == kTransferFailed) {
        DiscardWaveBank(host, bank);
        return kLoadFailed;
      }
      return kLoadInFlight;
    }
    case Content::DiscRequestState::kLoaded:
      host.MarkRequestDispatched(bank.request.request);
      host.LoadMusicBank(bank.data, false);
      return kLoadInFlight;
    case Content::DiscRequestState::kError:
      DiscardWaveBank(host, bank);
      return kLoadFailed;
    default:
      return kLoadInFlight;
  }
}

// decomp: SLUS_010.40 vs_main_loadAndWaitSoundSlot
// (src/SLUS_010.40/main.c:10439)
void LoadWaveBankBlocking(SoundBankHost& host, std::int32_t id) {
  LoadMappedWaveBank(host, id);
  while (UpdateWaveBankLoad(host) != kLoadIdle) host.WaitFrame();
}

// decomp: SLUS_010.40 vs_main_setMonoSound (src/SLUS_010.40/main.c:10448)
void SetMonoOutput(SoundBankHost& host, std::int32_t mono_enabled) {
  host.SetMonoOutput(mono_enabled);
}

// decomp: SLUS_010.40 _loadMenuSound (src/SLUS_010.40/main.c:10452)
void InitializeMenuSoundBanks(SoundBankHost& host) {
  host.InitializePlayback();
  host.SetDiscMixVolume(kMenuDiscMixVolume);
  for (const MenuSoundFile file :
       {MenuSoundFile::kWave0000, MenuSoundFile::kWave0005}) {
    const BankBuffer bank = ReadMenuFile(host, file);
    host.LoadBank(bank, true);
    host.FreeBuffer(bank);
  }
  const BankBuffer effect_bank = ReadMenuFile(host, MenuSoundFile::kWave0200);
  host.LoadSoundEffectBank(effect_bank, kMenuSfxBankUploadSlot, true);
  host.FreeBuffer(effect_bank);
  const BankBuffer effects = host.MenuSoundEffects();
  host.ReadFileBlocking(host.MenuFile(MenuSoundFile::kEffect00), effects);
  host.BindSoundEffectPrograms(effects);
  host.StoreEffectTagCursors(EffectTagCursors{});
}

// decomp: SLUS_010.40 vs_main_resetSound (src/SLUS_010.40/main.c:10480)
void ResetSoundSlots(SoundBankHost& host) {
  host.ResetCutsceneStream();
  host.StoreSoundSlots(SoundSlots{});
  host.StoreEffectTagCursors(EffectTagCursors{});
}
}  // namespace VS::Audio
