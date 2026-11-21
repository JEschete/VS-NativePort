#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Render/DrawPolygon.hpp"
#include "Render/PageSpace.hpp"

namespace VS::Render {

// Where a frame's typed items are drawn: the surface and the part of it they
// may write. Items are also clipped to the draw area current when each one is
// drawn, as packets are, so feedback packets that move the draw area during
// the walk still apply to them.
struct DrawTarget {
  PageSpace* surface = nullptr;
  PageSpace::Rect area{0, 0, PageSpace::Width, PageSpace::Height};
};

// Typed polygons a frame submits to its scene ordering table instead of
// building GP0 packets (#50, MA-09). An item is drawn when the ordering-table
// walk follows the link its slot entry held when it was submitted, which is
// where the equivalent prepended packet would have been drawn: after packets
// added to the slot later, before those added earlier. Packets still own the
// rest of the frame; the walk (Legacy GpuWalkList) interleaves both. Table
// entries are ordering-table tags: the low 24 bits link to the next node.
//
// Lifetime: items exist from Submit until the next Open, Close or Abandon, or
// until their table entries are cleared (DiscardEntries, called when an
// ordering table is reinitialized). Only prepend links may be made to the
// scene table while items are pending; a walk that did not pass through an
// item's slot entry does not draw it. Repeated walks draw items again, as
// they would draw packets again.
class FrameDrawList {
 public:
  static constexpr std::size_t kCapacity = 8192;

  // Starts a frame over `scene_table`, discarding earlier items (logged when
  // a previous frame was never closed). False, leaving the list closed, when
  // the table is empty or the target has no surface.
  bool Open(std::span<std::uint32_t> scene_table, const DrawTarget& target);
  // Ends the frame. Returns the number of live items never drawn (logged),
  // after logging any refused submissions.
  std::size_t Close();
  // Ends the frame without drawing, when its tables are released. Returns
  // the number of live items discarded.
  std::size_t Abandon();

  [[nodiscard]] bool IsOpen() const noexcept { return open_; }
  // Items submitted since Open, including discarded ones: the end of the
  // index range ForEachItem accepts.
  [[nodiscard]] std::size_t ItemCount() const noexcept { return items_.size(); }
  [[nodiscard]] std::size_t LiveCount() const noexcept { return live_; }
  [[nodiscard]] std::size_t RefusedCount() const noexcept { return refused_; }
  // The scene slot of `entry`, or nothing when the list is closed or the
  // entry is outside its table, so producers can address the table they
  // were handed by slot.
  [[nodiscard]] std::optional<std::size_t> SlotOf(
      const std::uint32_t* entry) const;

  // Adds `polygon` at the head of scene `slot`. Returns its item index, or
  // nothing when the list is closed, the slot is outside the table, the
  // polygon is invalid or the list is full.
  std::optional<std::size_t> Submit(const DrawPolygon& polygon,
                                    std::size_t slot);

  // Calls fn(DrawPolygon&, std::size_t slot) for each live item with an index
  // in [first, end), in submission order, so post-passes can adjust items as
  // they adjusted packets.
  template <class Fn>
  void ForEachItem(std::size_t first, std::size_t end, Fn&& fn) {
    end = end < items_.size() ? end : items_.size();
    for (std::size_t i = first; i < end; ++i) {
      if (items_[i].live) fn(items_[i].polygon, items_[i].slot);
    }
  }

  // Calls fn(DrawPolygon&, std::size_t slot) for the live items anchored at
  // `link`, which a walk is about to follow, whose slot entries `walked` has
  // passed through, newest first: the items whose equivalent packets that
  // walk would reach next.
  template <class Fn>
  void ForEachItemBefore(std::uint32_t link,
                         const std::unordered_set<const void*>& walked,
                         Fn&& fn) {
    VisitBefore(link, walked,
                [&fn](Item& item) { fn(item.polygon, item.slot); });
  }

  // Draws the live items anchored at `link`, which the walk is about to
  // follow (0xFFFFFF: the end of the list), whose slot entries `walked` has
  // passed through, newest first.
  void DrawBefore(std::uint32_t link,
                  const std::unordered_set<const void*>& walked);
  // Discards the items whose slot entries lie in [first, first + count), as
  // reinitializing those entries unlinks the packets after them. Returns the
  // number discarded.
  std::size_t DiscardEntries(const std::uint32_t* first, std::size_t count);

 private:
  static constexpr std::uint32_t kNoItem =
      std::numeric_limits<std::uint32_t>::max();
  static constexpr std::uint32_t kLinkMask = 0xFFFFFF;

  struct Item {
    DrawPolygon polygon;
    const std::uint32_t* entry = nullptr;
    std::size_t slot = 0;
    std::uint32_t older = kNoItem;  // next item with the same anchor
    bool live = true;
    bool drawn = false;
  };

  template <class Fn>
  void VisitBefore(std::uint32_t link,
                   const std::unordered_set<const void*>& walked, Fn&& fn) {
    if (live_ == 0) return;
    const auto head = newest_by_anchor_.find(link & kLinkMask);
    if (head == newest_by_anchor_.end()) return;
    for (std::uint32_t i = head->second; i != kNoItem; i = items_[i].older) {
      Item& item = items_[i];
      if (item.live && walked.contains(item.entry)) fn(item);
    }
  }

  void Reset();
  void Refuse(const char* reason, std::size_t slot);

  std::span<std::uint32_t> table_;
  DrawTarget target_;
  std::vector<Item> items_;
  std::unordered_map<std::uint32_t, std::uint32_t> newest_by_anchor_;
  std::size_t live_ = 0;
  std::size_t refused_ = 0;
  bool open_ = false;
};

// The list the gameplay frame (Boot GameplayLoopLive) opens over its scene
// table and the libgpu walk draws from.
FrameDrawList& GameFrameDrawList();

}  // namespace VS::Render
