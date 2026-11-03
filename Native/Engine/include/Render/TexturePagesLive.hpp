#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Render/TexturePages.hpp"

// Live binding for the lendable texture pages (TexturePages.cpp): one table
// for the engine, behind the original call shapes. A range outside pages
// 25-31, an owner outside 1-0xFC, an occupied page or another owner's page is
// refused, counted and reported, and the table is left as it was.
namespace VS::Render {
// The engine's table, for tests and diagnostics.
[[nodiscard]] TexturePageTable& LiveTexturePages();
// func_8007DF48 (146C.c:9480). The original also released the overlay
// workspace lock; Content owns that, so its one caller (INITBTL 18.c:502)
// clears it after this call.
void InitializeLiveTexturePages();
// func_8007DF98 (146C.c:9495).
void AdvanceLiveTexturePageRetirements();
// func_8007DFF0 (146C.c:9509): the first page when the range was lent, or 0.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int ReserveLiveTexturePageRange(std::uint32_t first_page,
                                std::uint32_t page_count, int owner_tag);
// func_8007E0A8 (146C.c:9532): 1 when the range started retiring, or 0.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int RetireLiveTexturePageRange(std::uint32_t first_page,
                               std::uint32_t page_count, int owner_tag);
// Requests refused since start-up.
[[nodiscard]] std::uint64_t RefusedLiveTexturePageRequestCount();
}  // namespace VS::Render
