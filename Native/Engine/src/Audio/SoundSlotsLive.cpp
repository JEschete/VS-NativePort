// Hand-maintained (tools/port/owned.csv).
// The slot record's handles and the disc queue and heap services the sound
// bank and music slot bindings share (SoundSlotsLive.hpp).
#include "Audio/SoundSlotsLive.hpp"

#include <bit>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Audio/Music.hpp"
#include "Audio/SoundBanks.hpp"
#include "Content/AssetCatalog.hpp"
#include "Content/AssetHandle.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DataStore.hpp"
#include "Content/DiscQueue.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Audio {
namespace {
static_assert(kMusicFileCount == Content::kMusicAssetCount);

std::uint64_t missingCatalogReads = 0;

const Content::AssetCatalog* LoadedCatalog() {
  const Content::DataStore* store = Content::DataStore::Get();
  return store != nullptr ? store->Catalog() : nullptr;
}

std::size_t TableSize(AudioFileTable table) {
  switch (table) {
    case AudioFileTable::kMusic:
      return Content::kMusicAssetCount;
    case AudioFileTable::kSfxBanks:
      return Content::kSfxBankAssetCount;
    case AudioFileTable::kWaveBanks:
      return Content::kWaveBankAssetCount;
  }
  return 0;
}

const char* TableName(AudioFileTable table) {
  switch (table) {
    case AudioFileTable::kMusic:
      return "music";
    case AudioFileTable::kSfxBanks:
      return "sound-effect bank";
    case AudioFileTable::kWaveBanks:
      return "wave bank";
  }
  return "audio";
}

std::span<const Content::AssetHandle> TableOf(
    const Content::AudioCatalog& audio, AudioFileTable table) {
  switch (table) {
    case AudioFileTable::kMusic:
      return audio.music;
    case AudioFileTable::kSfxBanks:
      return audio.sfx_banks;
    case AudioFileTable::kWaveBanks:
      return audio.wave_banks;
  }
  return {};
}

Content::AssetHandle MenuAsset(const Content::MenuSoundAssets& menu,
                               MenuSoundFile file) {
  switch (file) {
    case MenuSoundFile::kWave0000:
      return menu.sample_banks[0];
    case MenuSoundFile::kWave0005:
      return menu.sample_banks[1];
    case MenuSoundFile::kWave0200:
      return menu.effect_sample_bank;
    case MenuSoundFile::kEffect00:
      return menu.effect_programs;
  }
  return {};
}

// The asset's extent: no sector and the whole-sector load size the
// original's tables gave.
std::optional<Content::DiscFileExtent> AssetExtent(
    const Content::AssetCatalog* catalog, Content::AssetHandle handle,
    const char* table, std::int32_t index) {
  const Content::AssetRecord* record =
      catalog != nullptr ? catalog->Record(handle) : nullptr;
  if (record == nullptr) {
    if (std::has_single_bit(++missingCatalogReads)) {
      VS::Core::Warnf("audio",
                      "%s file %d refused: no loaded asset catalog names it; "
                      "%" PRIu64 " refused",
                      table, index, missingCatalogReads);
    }
    return std::nullopt;
  }
  return Content::DiscFileExtent{
      .sector = 0,
      .bytes = static_cast<std::int32_t>(record->LoadBytes()),
      .asset = handle};
}

DiscRequestRef ToRequestRef(const VS::Content::DiscQueueSlot* slot) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<DiscRequestRef>(std::bit_cast<std::uintptr_t>(slot));
}

// The marker a slot holds once its read failed.
VS::Content::DiscQueueSlot* FailedRequestMarker() {
  return std::bit_cast<VS::Content::DiscQueueSlot*>(~std::uintptr_t{0});
}

VS::Content::DiscQueueSlot* RequestSlot(DiscRequestRef request) {
  return std::bit_cast<VS::Content::DiscQueueSlot*>(
      static_cast<std::uintptr_t>(request));
}

}  // namespace

// The handles name only kNone; the live addresses they carry are made here.
BankBuffer LiveBankBuffer(const void* data) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<BankBuffer>(std::bit_cast<std::uintptr_t>(data));
}

void* LiveBufferPointer(BankBuffer buffer) {
  return std::bit_cast<void*>(static_cast<std::uintptr_t>(buffer));
}

BankRequest LiveBankRequest(VS::Content::DiscQueueSlot* slot) {
  if (slot == nullptr) return {};
  if (slot == FailedRequestMarker()) return {.mark = RequestMark::kFailed};
  return {.mark = RequestMark::kActive, .request = ToRequestRef(slot)};
}

VS::Content::DiscQueueSlot* LiveRequestSlot(const BankRequest& request) {
  switch (request.mark) {
    case RequestMark::kNone:
      return nullptr;
    case RequestMark::kFailed:
      return FailedRequestMarker();
    case RequestMark::kActive:
      return RequestSlot(request.request);
  }
  return nullptr;
}

std::optional<Content::DiscFileExtent> LiveAudioFile(AudioFileTable table,
                                                     std::int32_t index) {
  if (index < 0 || static_cast<std::size_t>(index) >= TableSize(table)) {
    return std::nullopt;
  }
  const Content::AssetCatalog* catalog = LoadedCatalog();
  const Content::AssetHandle handle =
      catalog != nullptr
          ? TableOf(catalog->Audio(), table)[static_cast<std::size_t>(index)]
          : Content::AssetHandle{};
  return AssetExtent(catalog, handle, TableName(table), index);
}

std::optional<Content::DiscFileExtent> LiveMenuSoundFile(MenuSoundFile file) {
  const Content::AssetCatalog* catalog = LoadedCatalog();
  const Content::AssetHandle handle =
      catalog != nullptr ? MenuAsset(catalog->Audio().menu, file)
                         : Content::AssetHandle{};
  return AssetExtent(catalog, handle, "menu sound",
                     static_cast<std::int32_t>(file));
}

std::optional<DiscRequestRef> AllocateLiveSlotRequest(
    const Content::DiscFileExtent& file) {
  VS::Content::DiscQueueSlot* request = Content::AllocateLiveDiscRequest(file);
  if (request == nullptr) return std::nullopt;
  return ToRequestRef(request);
}

void EnqueueLiveSlotRequest(DiscRequestRef request, BankBuffer destination) {
  Content::EnqueueLiveDiscRequest(RequestSlot(request),
                                  LiveBufferPointer(destination));
}

DiscRequestRef CompleteLiveRefusedSlotRequest(
    const Content::DiscFileExtent& file, BankBuffer destination) {
  return ToRequestRef(Content::CompleteLiveRefusedDiscRequest(
      file, LiveBufferPointer(destination)));
}

Content::DiscRequestState LiveSlotRequestState(DiscRequestRef request) {
  const VS::Content::DiscQueueSlot* slot = RequestSlot(request);
  if (slot == nullptr) return Content::DiscRequestState::kFree;
  return slot->state;
}

void ReleaseLiveSlotRequest(DiscRequestRef request) {
  Content::ReleaseLiveDiscRequest(RequestSlot(request));
}

BankBuffer AllocateLiveSlotBuffer(std::int32_t bytes) {
  return LiveBankBuffer(
      Core::Memory::AllocateLiveFromHighEnd(static_cast<std::size_t>(bytes)));
}

void FreeLiveSlotBuffer(BankBuffer buffer) {
  Core::Memory::FreeLiveFromHighEnd(LiveBufferPointer(buffer));
}
}  // namespace VS::Audio
