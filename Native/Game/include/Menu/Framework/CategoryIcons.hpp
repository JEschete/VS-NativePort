#pragma once

#include <cstdint>

#include "Menu/Framework/Framework.hpp"

namespace VS::Menu::Framework {
struct CategoryIcon {
  std::uint8_t category = 0;
  std::int32_t x = 0;
  std::int32_t y = 0;
  bool selected_palette = false;
  bool overlay = false;
  friend bool operator==(CategoryIcon, CategoryIcon) = default;
};

class CategoryIconWorld {
 public:
  virtual ~CategoryIconWorld() = default;
  [[nodiscard]] virtual std::uint8_t CurrentCategory() = 0;
  virtual void DrawListTabNavigation() = 0;
  [[nodiscard]] virtual MenuRow& CategoryHeader() = 0;
  virtual void DrawIcon(CategoryIcon icon) = 0;
};

// Each Live world retains its own category byte, row owner and draw layers.
void DrawCategoryIconStrip(CategoryIconWorld& world,
                           std::int32_t animation_step,
                           std::int32_t highlight_selected);
}  // namespace VS::Menu::Framework
