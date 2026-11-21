#include "Render/DrawList.hpp"

#include <functional>
#include <unordered_set>

#include "Core/Log.hpp"

namespace VS::Render {

void FrameDrawList::Reset() {
  table_ = {};
  target_ = {};
  items_.clear();
  newest_by_anchor_.clear();
  live_ = 0;
  refused_ = 0;
  open_ = false;
}

bool FrameDrawList::Open(std::span<std::uint32_t> scene_table,
                         const DrawTarget& target) {
  if (open_) {
    VS::Core::LogWarn("render",
                      "draw list reopened with {} items of an unclosed frame",
                      live_);
  }
  Reset();
  if (scene_table.empty() || target.surface == nullptr) {
    VS::Core::LogError(
        "render", "draw list refused: {}",
        scene_table.empty() ? "no scene table" : "no target surface");
    return false;
  }
  table_ = scene_table;
  target_ = target;
  items_.reserve(kCapacity);
  open_ = true;
  return true;
}

std::size_t FrameDrawList::Close() {
  std::size_t undrawn = 0;
  for (const Item& item : items_) {
    if (item.live && !item.drawn) ++undrawn;
  }
  if (refused_ != 0) {
    VS::Core::LogWarn("render", "draw list refused {} submissions this frame",
                      refused_);
  }
  if (undrawn != 0) {
    VS::Core::LogWarn("render", "draw list closed with {} items never drawn",
                      undrawn);
  }
  Reset();
  return undrawn;
}

std::size_t FrameDrawList::Abandon() {
  const std::size_t discarded = live_;
  if (discarded != 0) {
    VS::Core::LogDebug("render", "draw list abandoned {} items", discarded);
  }
  Reset();
  return discarded;
}

void FrameDrawList::Refuse(const char* reason, std::size_t slot) {
  // The first refusal of a frame says why; Close reports the count.
  if (refused_++ == 0) {
    VS::Core::LogWarn("render", "draw list refused an item for slot {}: {}",
                      slot, reason);
  }
}

std::optional<std::size_t> FrameDrawList::SlotOf(
    const std::uint32_t* entry) const {
  if (!open_ || entry == nullptr) return std::nullopt;
  const std::less<const std::uint32_t*> before;
  const std::uint32_t* first = table_.data();
  if (before(entry, first) || !before(entry, first + table_.size())) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(entry - first);
}

std::optional<std::size_t> FrameDrawList::Submit(const DrawPolygon& polygon,
                                                 std::size_t slot) {
  if (!open_) {
    Refuse("no frame is open", slot);
    return std::nullopt;
  }
  if (slot >= table_.size()) {
    Refuse("slot outside the scene table", slot);
    return std::nullopt;
  }
  if (!polygon.HasValidCornerCount()) {
    Refuse("invalid corner count", slot);
    return std::nullopt;
  }
  if (items_.size() >= kCapacity) {
    Refuse("list full", slot);
    return std::nullopt;
  }
  const std::uint32_t* entry = &table_[slot];
  // The link the entry holds now; later packets go in front of its node.
  const std::uint32_t anchor = *entry & kLinkMask;
  const auto index = static_cast<std::uint32_t>(items_.size());
  auto [head, added] = newest_by_anchor_.try_emplace(anchor, index);
  items_.push_back(Item{.polygon = polygon,
                        .entry = entry,
                        .slot = slot,
                        .older = added ? kNoItem : head->second});
  head->second = index;
  ++live_;
  return index;
}

void FrameDrawList::DrawBefore(std::uint32_t link,
                               const std::unordered_set<const void*>& walked) {
  VisitBefore(link, walked, [this](Item& item) {
    static_cast<void>(target_.surface->Draw(item.polygon, target_.area));
    item.drawn = true;
  });
}

std::size_t FrameDrawList::DiscardEntries(const std::uint32_t* first,
                                          std::size_t count) {
  if (live_ == 0 || first == nullptr || count == 0) return 0;
  const std::less<const std::uint32_t*> before;
  const std::uint32_t* end = first + count;
  std::size_t discarded = 0;
  for (Item& item : items_) {
    if (item.live && !before(item.entry, first) && before(item.entry, end)) {
      item.live = false;
      ++discarded;
    }
  }
  live_ -= discarded;
  return discarded;
}

FrameDrawList& GameFrameDrawList() {
  static FrameDrawList list;
  return list;
}

}  // namespace VS::Render
