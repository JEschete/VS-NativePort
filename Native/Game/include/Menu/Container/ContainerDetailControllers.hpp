#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Items/ItemRecords.hpp"
#include "Menu/Container/ContainerDetailHelpers.hpp"
#include "Menu/Framework/CategoryIcons.hpp"

namespace VS::Menu::container {
struct ContainerSlot {
  std::uint16_t one_based = 0;
  friend bool operator==(ContainerSlot, ContainerSlot) = default;
};

enum class ControllerStage : std::uint8_t {
  kInitialize,
  kAnimate,
  kInput,
  kReturn,
  kUnrecognized = 0xFF,
};

struct ControllerState {
  ControllerStage stage = ControllerStage::kInitialize;
  std::uint8_t animation_step = 0;
  std::uint8_t selected_row = 0;
  std::uint8_t selected_page = 0;
  std::uint8_t unused_entry_byte = 0;
  friend bool operator==(const ControllerState&,
                         const ControllerState&) = default;
};

struct ControllerEntry {
  DetailRow row;
  DetailPage page;
};

struct DetailRowText {
  std::span<char> name;
  std::span<char> description;
  std::uint32_t row_type = 0;
};

// Each operation resolves its owner at the original call point. Formatter
// spans stay borrowed from that call's owners across the statistics callback.
class ControllerWorld {
 public:
  virtual ~ControllerWorld() = default;
  [[nodiscard]] virtual std::int32_t RowsReady() = 0;
  [[nodiscard]] virtual std::uint32_t PressedButtons() = 0;
  virtual void BeginTransition(DetailRow row) = 0;
  virtual void InitializeLayout(DetailRow row, std::int32_t stat_pages) = 0;
  [[nodiscard]] virtual ContainerSlot CurrentItem(DetailPage page) = 0;
  virtual void SetStatistics(ContainerSlot item) = 0;
  virtual void OpenDpPpBars() = 0;
  virtual void OpenDpBar() = 0;
  virtual void CreateComponentRow(std::int32_t row, ContainerSlot item,
                                  bool slide_in) = 0;
  virtual void Exit(bool hide_dp_pp) = 0;
  virtual void RememberTriangleClose(bool triangle) = 0;
  [[nodiscard]] virtual DetailPage PollPaging(DetailPage page) = 0;
  [[nodiscard]] virtual ContainerSlot SelectItem(DetailPage page) = 0;
  [[nodiscard]] virtual DetailRowText FormatItem(ContainerSlot item) = 0;
  virtual void ChangePage(DetailRow row, const DetailRowText& text,
                          DetailPage page) = 0;
  [[nodiscard]] virtual std::uint8_t GripGemSlots(ContainerSlot item) = 0;
  virtual void DrawGripGemSlotCount(std::uint8_t slots) = 0;
  // Resolve this selection against the current container when called.
  [[nodiscard]] virtual bool SelectedArmorShowsDp(ContainerSlot item) = 0;
};

using CategoryIcon = Framework::CategoryIcon;
using CategoryIconWorld = Framework::CategoryIconWorld;

struct DispatcherState {
  Items::ItemCategory category = Items::ItemCategory::kWeapon;
};

struct DispatchEntry {
  Items::ItemCategory category;
  std::optional<ControllerEntry> selection;
};

enum class DetailViewMode : std::uint8_t { kList, kDetails };
enum class DetailCommand : std::uint8_t { kNone, kMenu };

class DispatcherWorld {
 public:
  virtual ~DispatcherWorld() = default;
  virtual void SetViewMode(DetailViewMode mode) = 0;
  virtual void RememberTriangleClose(bool triangle) = 0;
  virtual void ResetDetailCursor() = 0;
  [[nodiscard]] virtual Framework::MenuRow& CategoryHeader() = 0;
  virtual void SetCommand(DetailCommand command) = 0;
  [[nodiscard]] virtual std::int32_t RunController(
      Items::ItemCategory category,
      std::optional<ControllerEntry> selection) = 0;
  [[nodiscard]] virtual bool ClosedWithTriangle() = 0;
  virtual void SetIconsVisibility(std::uint8_t visibility) = 0;
  virtual void SetControlsVisibility(std::uint8_t visibility) = 0;
  [[nodiscard]] virtual std::int32_t RowsReady() = 0;
  [[nodiscard]] virtual std::uint8_t SelectedDetailElement() = 0;
  virtual void SetCursorBright(bool bright) = 0;
  virtual void DrawDetailTabNavigation() = 0;
  virtual void DrawItemDetails() = 0;
  [[noreturn]] virtual void Refuse(std::string_view reason) = 0;
};

void DrawCategoryIcons(CategoryIconWorld& world, std::int32_t animation_step,
                       std::int32_t details_phase);
[[nodiscard]] std::int32_t UpdateWeaponDetails(
    ControllerState& state, ControllerWorld& world,
    std::optional<ControllerEntry> entry = std::nullopt);
[[nodiscard]] std::int32_t UpdateBladeDetails(
    ControllerState& state, ControllerWorld& world,
    std::optional<ControllerEntry> entry = std::nullopt);
[[nodiscard]] std::int32_t UpdateGripDetails(
    ControllerState& state, ControllerWorld& world,
    std::optional<ControllerEntry> entry = std::nullopt);
[[nodiscard]] std::int32_t UpdateShieldDetails(
    ControllerState& state, ControllerWorld& world,
    std::optional<ControllerEntry> entry = std::nullopt);
[[nodiscard]] std::int32_t UpdateArmorDetails(
    ControllerState& state, ControllerWorld& world,
    std::optional<ControllerEntry> entry = std::nullopt);
[[nodiscard]] std::int32_t UpdateGemDetails(
    ControllerState& state, ControllerWorld& world,
    std::optional<ControllerEntry> entry = std::nullopt);
[[nodiscard]] std::int32_t UpdateItemDetails(
    DispatcherState& state, DispatcherWorld& world,
    std::optional<DispatchEntry> entry = std::nullopt);
}  // namespace VS::Menu::container
