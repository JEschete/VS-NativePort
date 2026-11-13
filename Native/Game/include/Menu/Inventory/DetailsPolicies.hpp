#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "Menu/Framework/Framework.hpp"

namespace VS::Menu::Inventory {
struct PanelVisibility {
  std::uint8_t secondary_title = 0;
  std::uint8_t category_icons = 0;
  std::uint8_t status_command = 0;
  friend bool operator==(const PanelVisibility&,
                         const PanelVisibility&) = default;
};

struct DetailPage {
  std::int32_t index = 0;
  friend bool operator==(DetailPage, DetailPage) = default;
};

struct DetailRowId {
  std::int32_t index = 0;
};

struct InventorySlot {
  std::uint8_t one_based = 0;
  friend bool operator==(InventorySlot, InventorySlot) = default;
};

struct CursorMemory {
  std::uint8_t row = 0;
  std::uint8_t page = 0;
  friend bool operator==(CursorMemory, CursorMemory) = default;
};

enum class PreviewLayout : std::int32_t { kItems, kDetails };
enum class StatPanelMotion : std::uint8_t { kOpen = 1, kClose };
enum class CategoryHeaderPlacement : std::uint8_t { kReturning = 2 };
enum class BattleReturnIntent : std::uint8_t { kMenu, kBattle };

inline constexpr DetailRowId kCategoryHeaderRow{31};

// Services resolve their current owner when called. Paging's category is
// needed only after the simultaneous-L1/R1 gate, not on entry.
class DetailsWorld {
 public:
  DetailsWorld() = default;
  DetailsWorld(const DetailsWorld&) = delete;
  DetailsWorld& operator=(const DetailsWorld&) = delete;
  DetailsWorld(DetailsWorld&&) = delete;
  DetailsWorld& operator=(DetailsWorld&&) = delete;
  virtual ~DetailsWorld() = default;

  [[nodiscard]] virtual std::uint32_t HeldButtons() = 0;
  [[nodiscard]] virtual std::uint32_t RepeatButtons() = 0;
  [[nodiscard]] virtual std::uint32_t PressedButtons() = 0;
  [[nodiscard]] virtual std::int32_t ItemCount() = 0;
  virtual void RememberPage(CursorMemory memory) = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> DisplayOrder() = 0;
  [[noreturn]] virtual void Refuse(std::string_view reason) = 0;

  virtual void ClearRows() = 0;
  [[nodiscard]] virtual Framework::MenuRow& Row(DetailRowId row) = 0;
  virtual void SetPreviewLayout(PreviewLayout layout) = 0;
  virtual void SetCategoryVisibility(std::uint8_t visibility) = 0;
  virtual void SetCommandVisibility(std::uint8_t visibility) = 0;
  virtual void ContinueTransition(DetailRowId row) = 0;
  virtual void ShowStatPages(std::int32_t pages) = 0;
  virtual void MovePrimaryStats(StatPanelMotion motion) = 0;
  virtual void PlayLeaveSound() = 0;
  virtual void PlayInvalidSound() = 0;
  virtual void DismissCommandRow() = 0;
  virtual void SetHeaderPlacement(CategoryHeaderPlacement placement) = 0;
  virtual void SetBattleReturnIntent(BattleReturnIntent intent) = 0;
  virtual void HideDpPpBars() = 0;
};

void SetPanelVisibility(PanelVisibility& panels, std::uint8_t visibility);
[[nodiscard]] DetailPage PollDetailsPaging(DetailsWorld& world,
                                           DetailPage page);
void BeginDetailsTransition(DetailsWorld& world, DetailRowId selected);
void InitializeDetailsLayout(DetailsWorld& world, DetailRowId selected,
                             std::int32_t stat_pages);
[[nodiscard]] InventorySlot SelectDetailsItem(DetailsWorld& world,
                                              DetailPage page);
void ExitDetailsView(DetailsWorld& world, bool clear_dp_pp);
}  // namespace VS::Menu::Inventory
