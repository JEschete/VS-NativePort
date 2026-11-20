// Hand-maintained (tools/port/owned.csv).
// Live binding for the lendable texture pages (TexturePages.cpp).
#include "Render/TexturePagesLive.hpp"

#include <bit>
#include <cinttypes>
#include <cstdint>

#include "Core/Diagnostics.hpp"

namespace VS::Render {
namespace {
// The original's table (D_800F19A8) is replaced by this one; nothing outside
// the binding reaches the pages by address. Like the original it starts all
// free until the boot sequence initializes it.
TexturePageTable livePages{};
std::uint64_t refusedRequests = 0;

void ReportRefusal(const char* request, TexturePageRange range, int owner,
                   TexturePageResult result) {
  if (std::has_single_bit(++refusedRequests)) {
    VS::Core::Errorf("render",
                     "texture page %s of %" PRIu32 "+%" PRIu32
                     " for owner %d refused (result %d, %" PRIu64 " refused)",
                     request, range.first, range.count, owner,
                     static_cast<int>(result), refusedRequests);
  }
}
}  // namespace

TexturePageTable& LiveTexturePages() { return livePages; }

void InitializeLiveTexturePages() { InitializeTexturePages(livePages); }

void AdvanceLiveTexturePageRetirements() {
  AdvanceTexturePageRetirements(livePages);
}

int ReserveLiveTexturePageRange(std::uint32_t first_page,
                                std::uint32_t page_count, int owner_tag) {
  const TexturePageRange range{.first = first_page, .count = page_count};
  const TexturePageResult result =
      ReserveTexturePages(livePages, range, owner_tag);
  if (result != TexturePageResult::kDone) {
    ReportRefusal("reservation", range, owner_tag, result);
    return 0;
  }
  return static_cast<int>(first_page);
}

int RetireLiveTexturePageRange(std::uint32_t first_page,
                               std::uint32_t page_count, int owner_tag) {
  const TexturePageRange range{.first = first_page, .count = page_count};
  const TexturePageResult result =
      RetireTexturePages(livePages, range, owner_tag);
  if (result != TexturePageResult::kDone) {
    ReportRefusal("retirement", range, owner_tag, result);
    return 0;
  }
  return 1;
}

std::uint64_t RefusedLiveTexturePageRequestCount() { return refusedRequests; }
}  // namespace VS::Render
