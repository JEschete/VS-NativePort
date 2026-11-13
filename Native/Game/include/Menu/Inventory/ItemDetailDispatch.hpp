#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/CategoryIcons.hpp"
#include "Menu/Inventory/EquipmentDetailControllers.hpp"

namespace VS::Menu::Inventory {
class ItemDetailWorld {
 public:
  ItemDetailWorld() = default;
  ItemDetailWorld(const ItemDetailWorld&) = delete;
  ItemDetailWorld& operator=(const ItemDetailWorld&) = delete;
  ItemDetailWorld(ItemDetailWorld&&) = delete;
  ItemDetailWorld& operator=(ItemDetailWorld&&) = delete;
  virtual ~ItemDetailWorld() = default;

  [[nodiscard]] virtual std::int32_t RowsReady() = 0;
  [[nodiscard]] virtual std::uint32_t PressedButtons() = 0;
  virtual void BeginTransition(DetailRowId row) = 0;
  virtual void InitializeLayout(DetailRowId row, DetailStatPages pages) = 0;
  [[nodiscard]] virtual InventorySlot CurrentItem(DetailPage page) = 0;
  virtual void SetStatistics(InventorySlot slot) = 0;
  [[nodiscard]] virtual bool ArmorShowsDp(InventorySlot slot) = 0;
  virtual void OpenDpBar() = 0;
  virtual void Exit(bool clear_dp_pp) = 0;
  virtual void SetBattleReturnIntent(BattleReturnIntent intent) = 0;
  [[nodiscard]] virtual DetailPage PollPaging(DetailPage page) = 0;
  [[nodiscard]] virtual InventorySlot SelectItem(DetailPage page) = 0;
  [[nodiscard]] virtual DetailRowView FormatItem(InventorySlot slot) = 0;
  virtual void ChangePage(DetailRowId row, const DetailRowView& text,
                          DetailPage page) = 0;
};

using CategoryIcon = Framework::CategoryIcon;
using CategoryIconWorld = Framework::CategoryIconWorld;

enum class DetailHeaderPlacement : std::uint8_t { kList, kDetails };
enum class DetailCommand : std::uint8_t { kNone, kMenu };

struct ItemDetailDispatcherState {
  Items::ItemCategory category = Items::ItemCategory::kWeapon;
};

struct DetailDispatchEntry {
  Items::ItemCategory category;
  std::optional<DetailSelection> selection;
};

class ItemDetailDispatcherWorld {
 public:
  ItemDetailDispatcherWorld() = default;
  ItemDetailDispatcherWorld(const ItemDetailDispatcherWorld&) = delete;
  ItemDetailDispatcherWorld& operator=(const ItemDetailDispatcherWorld&) =
      delete;
  ItemDetailDispatcherWorld(ItemDetailDispatcherWorld&&) = delete;
  ItemDetailDispatcherWorld& operator=(ItemDetailDispatcherWorld&&) = delete;
  virtual ~ItemDetailDispatcherWorld() = default;

  virtual void SetHeaderPlacement(DetailHeaderPlacement placement) = 0;
  virtual void SetBattleReturnIntent(BattleReturnIntent intent) = 0;
  virtual void ResetDetailCursor() = 0;
  [[nodiscard]] virtual Framework::MenuRow& CategoryHeader() = 0;
  virtual void SetCommand(DetailCommand command) = 0;
  [[nodiscard]] virtual std::int32_t RunController(
      Items::ItemCategory category,
      std::optional<DetailSelection> selection) = 0;
  [[nodiscard]] virtual bool ReturnsToBattle() = 0;
  virtual void SetCategoryVisibility(std::uint8_t visibility) = 0;
  virtual void SetCommandVisibility(std::uint8_t visibility) = 0;
  virtual void SetPreviewLayout(PreviewLayout layout) = 0;
  [[nodiscard]] virtual std::int32_t RowsReady() = 0;
  [[nodiscard]] virtual std::uint8_t SelectedDetailElement() = 0;
  virtual void SetCursorBright(bool bright) = 0;
  virtual void DrawDetailTabNavigation() = 0;
  virtual void DrawItemDetails() = 0;
  [[noreturn]] virtual void Refuse(std::string_view reason) = 0;
};

void DrawItemCategoryIcons(CategoryIconWorld& world,
                           std::int32_t animation_step,
                           std::int32_t highlight_selected);
[[nodiscard]] std::int32_t UpdateArmorDetails(
    EquipmentDetailState& state, ItemDetailWorld& world,
    std::optional<DetailSelection> selection = std::nullopt);
[[nodiscard]] std::int32_t UpdateGemDetails(
    EquipmentDetailState& state, ItemDetailWorld& world,
    std::optional<DetailSelection> selection = std::nullopt);
[[nodiscard]] std::int32_t UpdateItemDetails(
    ItemDetailDispatcherState& state, ItemDetailDispatcherWorld& world,
    std::optional<DetailDispatchEntry> entry = std::nullopt);
}  // namespace VS::Menu::Inventory
