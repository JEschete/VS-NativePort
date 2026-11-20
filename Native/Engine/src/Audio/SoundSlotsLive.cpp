// Hand-maintained (tools/port/owned.csv).
// The slot record's handles and the disc queue and heap services the sound
// bank and music slot bindings share (SoundSlotsLive.hpp).
#include "Audio/SoundSlotsLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Audio/SoundBanks.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscQueue.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Legacy/Psx.h"

namespace VS::Audio {
namespace {
static_assert(sizeof(VS::Content::DiscFile::lba) == sizeof(std::int32_t));

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

VS::Content::DiscFile ToDiscFile(const Content::DiscFileExtent& extent) {
  return {.lba = extent.sector, .size = extent.bytes};
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

std::optional<Content::DiscFileExtent> LiveTableFile(const int* sectors,
                                                     const int* sizes,
                                                     std::int32_t entries,
                                                     std::int32_t index) {
  if (index < 0 || index >= entries) return std::nullopt;
  const auto at = static_cast<std::size_t>(index);
  return Content::DiscFileExtent{.sector = sectors[at], .bytes = sizes[at]};
}

std::optional<DiscRequestRef> AllocateLiveSlotRequest(
    const Content::DiscFileExtent& file) {
  const VS::Content::DiscFile disc_file = ToDiscFile(file);
  VS::Content::DiscQueueSlot* request =
      Content::AllocateLiveDiscRequest(&disc_file);
  if (request == nullptr) return std::nullopt;
  return ToRequestRef(request);
}

void EnqueueLiveSlotRequest(DiscRequestRef request, BankBuffer destination) {
  Content::EnqueueLiveDiscRequest(RequestSlot(request),
                                  LiveBufferPointer(destination));
}

DiscRequestRef CompleteLiveRefusedSlotRequest(
    const Content::DiscFileExtent& file, BankBuffer destination) {
  const VS::Content::DiscFile disc_file = ToDiscFile(file);
  return ToRequestRef(Content::CompleteLiveRefusedDiscRequest(
      &disc_file, LiveBufferPointer(destination)));
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
