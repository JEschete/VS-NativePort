#include "Menu/Framework/CategoryIcons.hpp"

#include "Core/Math/ScalarMath.hpp"
#include "Menu/Container/ContainerDetailControllers.hpp"
#include "Menu/Inventory/ItemDetailDispatch.hpp"

namespace VS::Menu::Framework {
namespace {
constexpr std::int32_t kIconCount = 8;
constexpr std::int32_t kIconPitch = 16;
constexpr std::int32_t kIconStartX = 32;
constexpr std::int32_t kIconMergeInset = 12;
constexpr std::int32_t kFirstCategoryRowIcon = 24;
constexpr std::int32_t kLastIconAnimationStep = 4;
constexpr std::int32_t kIconAnimationPitch = 8;
constexpr std::int32_t kDetailHeaderY = 16;
}  // namespace

// decomp: _renderItemCategory (src/MENU/items0.h:46-81), instantiated in
// MENU3.PRG 80102A3C..80102B77 and MENUB.PRG 80102B14..80102C4F;
// func_80103E24 (src/MENU/MENUD.PRG/234.c:700-733), 80103E24..80103F63.
void DrawCategoryIconStrip(CategoryIconWorld& world,
                           std::int32_t animation_step,
                           std::int32_t highlight_selected) {
  const auto selected = (world.CurrentCategory() - 1) & (kIconCount - 1);
  if (animation_step == kLastIconAnimationStep) {
    world.DrawListTabNavigation();
  }
  const auto y = Core::Math::WrappingSubtract(
      Core::Math::WrappingMultiply(animation_step, kIconAnimationPitch),
      kIconPitch);
  for (std::int32_t category = 0; category < kIconCount; ++category) {
    const auto x =
        (((category + 1) & (kIconCount - 1)) * kIconPitch) + kIconStartX;
    const bool overlay = category == selected && highlight_selected == 1;
    if (overlay) {
      auto& header = world.CategoryHeader();
      if (x >= header.x - kIconMergeInset) {
        header.row_icon =
            static_cast<std::uint8_t>(category + kFirstCategoryRowIcon);
        continue;
      }
    }
    world.DrawIcon({.category = static_cast<std::uint8_t>(category),
                    .x = x,
                    .y = overlay ? kDetailHeaderY : y,
                    .selected_palette = category == selected,
                    .overlay = overlay});
  }
}
}  // namespace VS::Menu::Framework

namespace VS::Menu::Inventory {
// decomp: _renderItemCategory (src/MENU/items0.h:46-81); MENU3/MENUB.
void DrawItemCategoryIcons(CategoryIconWorld& world,
                           std::int32_t animation_step,
                           std::int32_t highlight_selected) {
  Framework::DrawCategoryIconStrip(world, animation_step, highlight_selected);
}
}  // namespace VS::Menu::Inventory

namespace VS::Menu::container {
// decomp: func_80103E24 (src/MENU/MENUD.PRG/234.c:700-733).
void DrawCategoryIcons(CategoryIconWorld& world, std::int32_t animation_step,
                       std::int32_t details_phase) {
  Framework::DrawCategoryIconStrip(world, animation_step, details_phase);
}
}  // namespace VS::Menu::container
