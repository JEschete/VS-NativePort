#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Audio/SoundBanks.hpp"
#include "Content/CdDrive.hpp"

struct Content_DiscQueueSlot;

// The slot record's buffers and disc requests as the typed handles
// (SoundBanks.hpp), and the disc queue and heap services the sound bank and
// music slot bindings share. A buffer or request handle carries the legacy
// pointer; the failed request marker ((vs_main_CdQueueSlot*)-1) is the one
// pointer value told apart, and only by equality.
namespace VS::Audio {
BankBuffer LiveBankBuffer(const void* data);
void* LiveBufferPointer(BankBuffer buffer);
BankRequest LiveBankRequest(Content_DiscQueueSlot* slot);
Content_DiscQueueSlot* LiveRequestSlot(const BankRequest& request);
// Entry `index` of a file table of `entries` sectors and sizes; nullopt past
// it.
std::optional<Content::DiscFileExtent> LiveTableFile(const int* sectors,
                                                     const int* sizes,
                                                     std::int32_t entries,
                                                     std::int32_t index);

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
