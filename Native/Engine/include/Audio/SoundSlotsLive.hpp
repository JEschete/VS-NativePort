#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Audio/SoundBanks.hpp"
#include "Content/CdDrive.hpp"

namespace VS::Content {
struct DiscQueueSlot;
}  // namespace VS::Content

// The slot record's buffers and disc requests as the typed handles
// (SoundBanks.hpp), and the disc queue and heap services the sound bank and
// music slot bindings share. A buffer or request handle carries the legacy
// pointer; the failed request marker ((vs_main_CdQueueSlot*)-1) is the one
// pointer value told apart, and only by equality.
namespace VS::Audio {
BankBuffer LiveBankBuffer(const void* data);
void* LiveBufferPointer(BankBuffer buffer);
BankRequest LiveBankRequest(VS::Content::DiscQueueSlot* slot);
VS::Content::DiscQueueSlot* LiveRequestSlot(const BankRequest& request);
// The asset catalog's audio file tables (#47), indexed by the original ids.
enum class AudioFileTable : std::uint8_t { kMusic, kSfxBanks, kWaveBanks };
// Entry `index` of a table, or a menu sound file, as the asset and the
// whole-sector size the original's tables gave. Nullopt past the table, or,
// with a warning, when no loaded asset catalog names the file.
std::optional<Content::DiscFileExtent> LiveAudioFile(AudioFileTable table,
                                                     std::int32_t index);
std::optional<Content::DiscFileExtent> LiveMenuSoundFile(MenuSoundFile file);

std::optional<DiscRequestRef> AllocateLiveSlotRequest(
    const Content::DiscFileExtent& file);
void EnqueueLiveSlotRequest(DiscRequestRef request, BankBuffer destination);
DiscRequestRef CompleteLiveRefusedSlotRequest(
    const Content::DiscFileExtent& file, BankBuffer destination);
Content::DiscRequestState LiveSlotRequestState(DiscRequestRef request);
void ReleaseLiveSlotRequest(DiscRequestRef request);
// The high end of the game heap.
BankBuffer AllocateLiveSlotBuffer(std::int32_t bytes);
void FreeLiveSlotBuffer(BankBuffer buffer);
}  // namespace VS::Audio
