// Hand-maintained (tools/port/owned.csv).
// Live binding for the lendable texture pages (TexturePages.cpp).
#include "Render/TexturePagesLive.hpp"

#include <bit>
#include <cinttypes>
#include <cstdint>

extern "C" {
#include "Core/Diagnostics.h"
}

namespace VS::Render {
namespace {
// The original's table (D_800F19A8) is replaced by this one; nothing outside
// the binding reaches the pages by address. Like the original it starts all
// free until the boot sequence initializes it.
TexturePageTable live_pages{};
std::uint64_t refused_requests = 0;

void ReportRefusal(const char* request, TexturePageRange range, int owner,
                   TexturePageResult result) {
  if (std::has_single_bit(++refused_requests)) {
    Core_Errorf("render",
                "texture page %s of %" PRIu32 "+%" PRIu32
                " for owner %d refused (result %d, %" PRIu64 " refused)",
                request, range.first, range.count, owner,
                static_cast<int>(result), refused_requests);
  }
}
}  // namespace

TexturePageTable& LiveTexturePages() { return live_pages; }

void InitializeLiveTexturePages() { InitializeTexturePages(live_pages); }

void AdvanceLiveTexturePageRetirements() {
  AdvanceTexturePageRetirements(live_pages);
}

int ReserveLiveTexturePageRange(std::uint32_t first_page,
                                std::uint32_t page_count, int owner_tag) {
  const TexturePageRange range{.first = first_page, .count = page_count};
  const TexturePageResult result =
      ReserveTexturePages(live_pages, range, owner_tag);
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
      RetireTexturePages(live_pages, range, owner_tag);
  if (result != TexturePageResult::kDone) {
    ReportRefusal("retirement", range, owner_tag, result);
    return 0;
  }
  return 1;
}

std::uint64_t RefusedLiveTexturePageRequestCount() { return refused_requests; }
}  // namespace VS::Render
