#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "Battle/TargetPages.h"

// Native target pages. The original sphere shows one bounded list of target
// candidates; native pages keep every candidate and split them into pages of
// whole actor groups that fit the sphere's visible labels.
namespace VS::Battle {
// The most candidates one action can preview, and so bind to the sphere.
inline constexpr std::size_t kMaxSphereTargets = 255;

// One page of candidates: a run of whole actor groups.
struct TargetPage {
  std::size_t first = 0;
  std::size_t count = 0;
};

// The pages of the bound candidates and the page on show.
struct TargetPageBook {
  std::vector<TargetPage> pages;
  std::size_t current = 0;
  std::size_t total = 0;
};

// The target sphere a page of candidates is shown on.
class TargetSphere {
 public:
  virtual ~TargetSphere() = default;
  // False when no sphere is active; nothing is bound then.
  [[nodiscard]] virtual bool Active() const = 0;
  // The labels one page can show.
  [[nodiscard]] virtual std::size_t VisibleSlots() const = 0;
  // Shows the candidates of page in the given targeting mode.
  virtual void ShowPage(TargetPage page, int mode) = 0;
};

// Splits candidates, given by actor ID, into pages of whole actor groups, each
// holding at most page_size entries. Aborts when one actor's group alone
// exceeds page_size.
[[nodiscard]] std::vector<TargetPage> PartitionTargetPages(
    std::span<const std::uint8_t> actor_ids, std::size_t page_size);

// Binds candidates, given by actor ID, to sphere, showing the first page
// (func_800C1664). book is replaced; it stays empty when no sphere is active.
// Aborts when there are more candidates than kMaxSphereTargets.
void SetSphereTargets(TargetPageBook& book,
                      std::span<const std::uint8_t> actor_ids, int mode,
                      TargetSphere& sphere);

// Applies SetSphereTargets to the active battle sphere. The pages view entries
// without owning them: entries must stay valid until the next binding or
// Battle_ResetTargetPages.
void SetLiveSphereTargets(std::span<Battle_TargetPreview> entries, int mode);

// The first count of the action previews; aborts when count is negative or
// exceeds them.
[[nodiscard]] std::span<Battle_TargetPreview> CheckedTargetPreviews(
    std::span<Battle_TargetPreview> previews, int count);
}  // namespace VS::Battle
