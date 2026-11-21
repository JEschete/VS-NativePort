#pragma once

#include <cstddef>
#include <vector>

#include "Render/DrawList.hpp"
#include "Render/DrawPolygon.hpp"

// Copies of the game frame draw list's items, so tests can inspect what the
// producers submitted.
namespace render_tests {
struct ListedItem {
  VS::Render::DrawPolygon polygon{};
  std::size_t slot = 0;
};

// The live items of the game frame draw list, in submission order.
[[nodiscard]] inline std::vector<ListedItem> ListedItems() {
  std::vector<ListedItem> items;
  VS::Render::FrameDrawList& list = VS::Render::GameFrameDrawList();
  list.ForEachItem(
      0, list.ItemCount(),
      [&items](VS::Render::DrawPolygon& polygon, std::size_t slot) {
        items.push_back({polygon, slot});
      });
  return items;
}

// The item submitted at `index` and its scene slot; a polygon with no corners
// at slot 0 when there is no such live item.
[[nodiscard]] inline ListedItem ListedItemAt(std::size_t index) {
  ListedItem found{.polygon = {.corner_count = 0}};
  VS::Render::GameFrameDrawList().ForEachItem(
      index, index + 1,
      [&found](VS::Render::DrawPolygon& polygon, std::size_t slot) {
        found = {polygon, slot};
      });
  return found;
}
}  // namespace render_tests
