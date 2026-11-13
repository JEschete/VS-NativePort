#include "Menu/Container/ContainerDetailHelpers.hpp"

#include <cstddef>
#include <variant>

#include "Core/Math/ScalarMath.hpp"
#include "Input/Pad.hpp"
#include "Items/EquipmentStats.hpp"

namespace VS::Menu::container {
namespace {
constexpr std::uint32_t kShoulders = Input::L1 | Input::R1;
constexpr DetailRow kCategoryHeader{31};
constexpr std::int16_t kHeaderY = 18;
constexpr std::int16_t kSelectedX = 155;

template <typename Record>
Record ReadPart(Items::ContainerWorld& records, Items::ItemCategory category,
                std::uint32_t reference) {
  const auto part = records.Read(category, reference - 1U);
  const auto* typed = std::get_if<Record>(&part);
  if (typed == nullptr) {
    records.Refuse("detail part type disagrees with container category");
  }
  return *typed;
}
}  // namespace

// decomp: vs_menuD_initUiWeapon, MENU/MENUD.PRG/234.c:21-55;
// original instructions 80102A34..80102BAF.
Items::ItemsStatus BuildWeaponStats(Items::WeaponLoadout& target,
                                    const Items::InventoryWeapon* source,
                                    Items::ContainerWorld& records) {
  Items::WeaponSpawnLoadout parts{};
  if (source != nullptr) {
    parts.index = source->index;
    if (source->blade != 0) {
      const auto blade = ReadPart<Items::InventoryBlade>(
          records, Items::ItemCategory::kBlade, source->blade);
      Items::ReadInventoryBladeStats(parts.blade, blade);
      parts.material = blade.material;
    }
    if (source->grip != 0) {
      Items::ReadInventoryGripStats(
          parts.grip, ReadPart<Items::InventoryGrip>(
                          records, Items::ItemCategory::kGrip, source->grip));
    }
    for (std::size_t gem = 0; gem < parts.gems.size(); ++gem) {
      if (source->gems[gem] != 0) {
        Items::ReadInventoryGemStats(
            parts.gems[gem],
            ReadPart<Items::InventoryGem>(records, Items::ItemCategory::kGem,
                                          source->gems[gem]));
      }
    }
  }
  if (source == nullptr) records.Refuse("missing mandatory weapon name source");
  parts.name = source->name;
  return Items::BuildWeaponStatsFromParts(target, parts);
}

// decomp: vs_menuD_initUiShield, MENU/MENUD.PRG/234.c:57-78;
// original instructions 80102BB0..80102C93.
void BuildShieldStats(Items::ShieldLoadout& target,
                      const Items::InventoryShield* source,
                      Items::ContainerWorld& records) {
  Items::ShieldSpawnLoadout parts{};
  if (source != nullptr) {
    parts.index = source->index;
    Items::ReadInventoryArmorStats(parts.base, source->base);
    parts.material = source->base.material;
    for (std::size_t gem = 0; gem < parts.gems.size(); ++gem) {
      if (source->gems[gem] != 0) {
        Items::ReadInventoryGemStats(
            parts.gems[gem],
            ReadPart<Items::InventoryGem>(
                records, Items::ItemCategory::kGem,
                source->gems[gem] & Items::kShieldGemSlotMask));
      }
    }
  }
  Items::BuildShieldStatsFromParts(target, parts);
}

// decomp: func_80103D50, MENU/MENUD.PRG/234.c:659-664;
// original instructions 80103D50..80103D6B.
void SetPanelVisibility(PanelVisibility& panels, std::uint8_t visibility) {
  panels = {.title = visibility, .icons = visibility, .controls = visibility};
}

// decomp: _pollItemNavigation, MENU/MENUD.PRG/234.c:666-698;
// original instructions 80103D6C..80103E23.
DetailPage PollDetailsPaging(DetailsWorld& world, DetailPage page) {
  if ((world.HeldButtons() & kShoulders) == kShoulders) return page;
  const auto count = world.ItemCount();
  const auto repeat = world.RepeatButtons();
  std::int32_t step = 0;
  if ((repeat & Input::L1) != 0) {
    step = Core::Math::WrappingSubtract(count, 1);
  }
  if ((repeat & Input::R1) != 0) step = Core::Math::WrappingAdd(step, 1);
  auto selected = Core::Math::WrappingAdd(page.index, step);
  if (selected >= count) {
    selected = Core::Math::WrappingSubtract(selected, count);
  }
  if ((world.PressedButtons() & kShoulders) != 0 && selected == page.index) {
    world.PlayInvalidSound();
  }
  return {selected};
}

// decomp: func_80103F64, MENU/MENUD.PRG/234.c:735-751;
// original instructions 80103F64..80103FD3.
void BeginDetailsTransition(DetailsWorld& world, DetailRow selected) {
  world.ClearRows();
  auto& header = world.Row(kCategoryHeader);
  header.state = Framework::RowState::kSlideY;
  header.target0 = kHeaderY;
  auto& item = world.Row(selected);
  item.state = Framework::RowState::kSlideX;
  item.target0 = kSelectedX;
  item.selected = 1;
  item.sub_text = nullptr;
  item.unselectable = 0;
}

// decomp: func_80104034, MENU/MENUD.PRG/234.c:767-774;
// original instructions 80104034..80104077.
void InitializeDetailsLayout(DetailsWorld& world, DetailRow selected,
                             std::int32_t stat_pages) {
  world.SetIconsVisibility(0);
  world.SetControlsVisibility(0);
  world.ContinueTransition(selected);
  world.ShowStatPages(stat_pages);
  world.MovePrimaryStats(PrimaryStatsMotion::kOpen);
}

// decomp: func_80104114, MENU/MENUD.PRG/234.c:789-794;
// original instructions 80104114..8010416F.
std::uint16_t SelectDetailsItem(DetailsWorld& world, DetailPage page) {
  world.RememberPage({0, static_cast<std::uint8_t>(page.index)});
  const auto order = world.DisplayOrder();
  if (page.index < 0 || static_cast<std::size_t>(page.index) >= order.size()) {
    world.Refuse("detail page outside current container display storage");
  }
  return order[static_cast<std::size_t>(page.index)];
}

// decomp: func_80104170, MENU/MENUD.PRG/234.c:796-808;
// original instructions 80104170..801041DF.
void ExitDetailsView(DetailsWorld& world, bool hide_dp_pp) {
  world.PlayLeaveSound();
  world.ClearRows();
  world.DismissCommandRow();
  world.ShowStatPages(-1);
  world.MovePrimaryStats(PrimaryStatsMotion::kClose);
  world.SetPhase(DetailPhase::kReturning);
  world.RememberTriangleClose((world.PressedButtons() & Input::Triangle) != 0);
  if (hide_dp_pp) world.HideDpPpBars();
}
}  // namespace VS::Menu::container
