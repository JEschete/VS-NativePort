// Hand-maintained (tools/port/owned.csv).
#include "Render/TexturePages.hpp"

#include <algorithm>
#include <span>

namespace VS::Render {
namespace {
bool InLendableRange(TexturePageRange range) noexcept {
  return range.first >= kResidentTexturePageCount &&
         range.first <= kTexturePageCount &&
         range.count <= kTexturePageCount - range.first;
}

bool IsValidOwner(int owner) noexcept {
  return owner >= kMinTexturePageOwner && owner <= kMaxTexturePageOwner;
}

std::span<TexturePage> PagesOf(TexturePageTable& pages,
                               TexturePageRange range) noexcept {
  return std::span<TexturePage>(pages).subspan(range.first, range.count);
}
}  // namespace

void InitializeTexturePages(TexturePageTable& pages) noexcept {
  pages.fill({});
  std::fill_n(pages.begin(), kResidentTexturePageCount,
              TexturePage{.state = TexturePageState::kResident});
}

void AdvanceTexturePageRetirements(TexturePageTable& pages) noexcept {
  for (TexturePage& page : pages) {
    if (page.state == TexturePageState::kRetired) {
      page = {};
    } else if (page.state == TexturePageState::kRetiring) {
      page = {.state = TexturePageState::kRetired};
    }
  }
}

TexturePageResult ReserveTexturePages(TexturePageTable& pages,
                                      TexturePageRange range, int owner) {
  if (!InLendableRange(range)) {
    return TexturePageResult::kOutOfRange;
  }
  if (!IsValidOwner(owner)) {
    return TexturePageResult::kInvalidOwner;
  }
  const std::span<TexturePage> lent = PagesOf(pages, range);
  if (!std::ranges::all_of(lent, [](const TexturePage& page) {
        return page.state == TexturePageState::kFree;
      })) {
    return TexturePageResult::kOccupied;
  }
  std::ranges::fill(lent,
                    TexturePage{.state = TexturePageState::kOwned,
                                .owner = static_cast<std::uint8_t>(owner)});
  return TexturePageResult::kDone;
}

TexturePageResult RetireTexturePages(TexturePageTable& pages,
                                     TexturePageRange range, int owner) {
  if (!InLendableRange(range)) {
    return TexturePageResult::kOutOfRange;
  }
  if (!IsValidOwner(owner)) {
    return TexturePageResult::kInvalidOwner;
  }
  const std::span<TexturePage> lent = PagesOf(pages, range);
  const TexturePage held{.state = TexturePageState::kOwned,
                         .owner = static_cast<std::uint8_t>(owner)};
  if (!std::ranges::all_of(
          lent, [&held](const TexturePage& page) { return page == held; })) {
    return TexturePageResult::kOwnerMismatch;
  }
  std::ranges::fill(lent, TexturePage{.state = TexturePageState::kRetiring});
  return TexturePageResult::kDone;
}
}  // namespace VS::Render
