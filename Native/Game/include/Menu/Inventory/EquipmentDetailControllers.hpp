#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "Menu/Inventory/DetailsPolicies.hpp"

namespace VS::Menu::Inventory {
enum class DetailStage : std::uint8_t {
  kInitialize,
  kAnimate,
  kInput,
  kReturn,
};

struct EquipmentDetailState {
  DetailStage stage = DetailStage::kInitialize;
  std::uint8_t animation_step = 0;
  std::uint8_t selected_row = 0;
  std::uint8_t selected_page = 0;
  // The original's never-read entry byte is reset by three controllers.
  std::uint8_t unused_entry_byte = 0;
  friend bool operator==(const EquipmentDetailState&,
                         const EquipmentDetailState&) = default;
};

struct DetailSelection {
  DetailRowId row;
  DetailPage page;
};

struct DetailRowView {
  std::span<char> name;
  std::span<char> description;
  std::uint32_t row_type = 0;
};

enum class DetailStatPages : std::uint8_t {
  kClassAndAffinity = 3,
  kType = 4,
  kAll = 7,
};

// The category is fixed by the controller binding, not a packed command.
// Each service reads its current owner; the row views retain the formatter's
// borrowed text across the intervening statistics callback.
class EquipmentDetailWorld {
 public:
  EquipmentDetailWorld() = default;
  EquipmentDetailWorld(const EquipmentDetailWorld&) = delete;
  EquipmentDetailWorld& operator=(const EquipmentDetailWorld&) = delete;
  EquipmentDetailWorld(EquipmentDetailWorld&&) = delete;
  EquipmentDetailWorld& operator=(EquipmentDetailWorld&&) = delete;
  virtual ~EquipmentDetailWorld() = default;

  [[nodiscard]] virtual std::int32_t RowsReady() = 0;
  [[nodiscard]] virtual std::uint32_t PressedButtons() = 0;
  virtual void BeginTransition(DetailRowId row) = 0;
  virtual void InitializeLayout(DetailRowId row, DetailStatPages pages) = 0;
  [[nodiscard]] virtual InventorySlot CurrentItem(DetailPage page) = 0;
  virtual void SetStatistics(InventorySlot slot) = 0;
  virtual void OpenDpPpBars() = 0;
  virtual void CreateComponentRow(std::int32_t row, InventorySlot slot,
                                  bool slide_in) = 0;
  virtual void Exit(bool clear_dp_pp) = 0;
  [[nodiscard]] virtual DetailPage PollPaging(DetailPage page) = 0;
  [[nodiscard]] virtual InventorySlot SelectItem(DetailPage page) = 0;
  [[nodiscard]] virtual DetailRowView FormatItem(InventorySlot slot) = 0;
  virtual void ChangePage(DetailRowId row, const DetailRowView& text,
                          DetailPage page) = 0;
  [[nodiscard]] virtual std::uint8_t GripGemSlots(InventorySlot slot) = 0;
  virtual void DrawGripGemSlotCount(std::uint8_t slots) = 0;
};

[[nodiscard]] std::int32_t UpdateWeaponDetails(
    EquipmentDetailState& state, EquipmentDetailWorld& world,
    std::optional<DetailSelection> selection = std::nullopt);
[[nodiscard]] std::int32_t UpdateBladeDetails(
    EquipmentDetailState& state, EquipmentDetailWorld& world,
    std::optional<DetailSelection> selection = std::nullopt);
[[nodiscard]] std::int32_t UpdateGripDetails(
    EquipmentDetailState& state, EquipmentDetailWorld& world,
    std::optional<DetailSelection> selection = std::nullopt);
[[nodiscard]] std::int32_t UpdateShieldDetails(
    EquipmentDetailState& state, EquipmentDetailWorld& world,
    std::optional<DetailSelection> selection = std::nullopt);
}  // namespace VS::Menu::Inventory
