#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Items/ContainerIndexing.hpp"
#include "Menu/Framework/Framework.hpp"

namespace VS::Menu::container {
enum class ActionDialogStep : std::uint8_t { kCreate, kInput, kWait };
enum class DialogSound : std::uint8_t { kSelect, kLeave, kChange, kDiscard };

struct SortMenuState {
  ActionDialogStep step = ActionDialogStep::kCreate;
  std::uint8_t category = 0;
};

enum class DiscardStep : std::uint8_t {
  kWaitInformation,
  kCreateSingle,
  kCreateQuantity,
  kSingleInput,
  kRevealQuantity,
  kQuantityInput,
  kCreateConfirmation,
  kConfirmationInput,
};

struct DiscardEntry {
  Items::ItemCategory category;
  std::uint8_t record_index;
  std::uint8_t kind;
};

// These fields were MENUD BSS and survive ordinary overlay loads.
struct DiscardMenuState {
  DiscardStep step = DiscardStep::kWaitInformation;
  Items::ItemCategory category = Items::ItemCategory::kWeapon;
  std::uint8_t record_index = 0;
  std::uint8_t kind = 0;
  std::uint8_t place = 0;
  std::uint8_t quantity = 0;
  std::uint8_t maximum = 0;
};

// Initialized module data, unlike DiscardMenuState. Space is an encoded glyph,
// not the authoring syntax or an ASCII blank.
inline constexpr std::size_t kDiscardCountTextBytes = 12;
inline constexpr auto kInitialDiscardCountText = std::to_array<std::uint8_t>({
    0xFA,
    0x30,
    0,
    0,
    0,
    0xA1,
    0,
    0,
    0,
    0xE7,
    0,
    0,
});
struct DiscardPresentation {
  std::array<std::uint8_t, kDiscardCountTextBytes> text =
      kInitialDiscardCountText;
  std::uint8_t cursor_animation = 0;
};

struct TransferConfirmationState {
  std::int32_t result = 0;
  ActionDialogStep step = ActionDialogStep::kCreate;
};

// Text spans borrow their real owner across CreateActions. Record access
// captures one current owner; later service callbacks may change that owner.
class MenuDialogWorld {
 public:
  virtual ~MenuDialogWorld() = default;
  [[nodiscard]] virtual std::int32_t RowsReady() = 0;
  virtual void InitializeInformationBox() = 0;
  [[nodiscard]] virtual bool InformationDone() = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> HelpText() = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> ContainerText() = 0;
  virtual void SetInformation(std::span<const std::uint8_t> text) = 0;
  virtual void CreateActions(std::int32_t menu,
                             std::span<const Framework::RowText> rows) = 0;
  virtual void UpdateActions() = 0;
  [[nodiscard]] virtual std::int32_t ActionResult() = 0;
  virtual void PlaySound(DialogSound sound) = 0;
  virtual void Sort(Items::ItemCategory category, std::int32_t stat) = 0;
  [[nodiscard]] virtual Items::ContainerWorld& ActiveRecords() = 0;
  [[nodiscard]] virtual Framework::MenuRow& QuantityRow(
      std::span<const std::uint8_t> text, std::int32_t x) = 0;
  [[nodiscard]] virtual std::uint32_t PressedButtons() = 0;
  [[nodiscard]] virtual std::uint32_t RepeatedButtons() = 0;
  virtual void SetQuantityFont() = 0;
  virtual void DrawQuantityDigit(std::uint32_t digit, std::int32_t x) = 0;
  [[nodiscard]] virtual std::int32_t DrawQuantityCursor(
      std::uint8_t animation, std::int32_t position) = 0;
  [[nodiscard]] virtual std::uint8_t TransferDirection() = 0;
  virtual void PrepareTransferControls() = 0;
  virtual void FlyOutAction(std::int32_t row) = 0;
  virtual void ClearTransferRows() = 0;
  virtual void HideTransferPanels() = 0;
  [[noreturn]] virtual void Refuse(std::string_view reason) = 0;
};

class ItemRowRestoreWorld {
 public:
  virtual ~ItemRowRestoreWorld() = default;
  [[nodiscard]] virtual std::uint8_t ListAnimationRow() = 0;
  virtual void AdvanceListRows() = 0;
  [[nodiscard]] virtual Framework::MenuRow& Row(std::int32_t index) = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> CursorMemory() = 0;
  [[noreturn]] virtual void Refuse(std::string_view reason) = 0;
};

[[nodiscard]] std::int32_t UpdateSortMenu(
    SortMenuState& state, MenuDialogWorld& world,
    std::optional<Items::ItemCategory> entry = std::nullopt);
[[nodiscard]] std::int32_t UpdateDiscardMenu(
    DiscardMenuState& state, DiscardPresentation& presentation,
    MenuDialogWorld& world, std::optional<DiscardEntry> entry = std::nullopt);
void RestoreItemRows(ItemRowRestoreWorld& world, std::int32_t category);
[[nodiscard]] std::int32_t UpdateTransferConfirmation(
    TransferConfirmationState& state, MenuDialogWorld& world,
    bool initialize = false);
}  // namespace VS::Menu::container
