#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "Items/ContainerIndexing.hpp"
#include "Menu/Framework/DetailInput.hpp"
#include "Menu/Framework/Framework.hpp"

namespace VS::Menu::container {
struct PanelVisibility {
  std::uint8_t title = 0;
  std::uint8_t icons = 0;
  std::uint8_t controls = 0;
  friend bool operator==(const PanelVisibility&,
                         const PanelVisibility&) = default;
};

struct DetailPage {
  std::int32_t index = 0;
  friend bool operator==(DetailPage, DetailPage) = default;
};

struct DetailRow {
  std::int32_t index = 0;
};

struct DetailCursor {
  std::uint8_t row = 0;
  std::uint8_t page = 0;
};

enum class DetailPhase : std::uint8_t { kReturning = 2 };
enum class PrimaryStatsMotion : std::uint8_t { kOpen = 1, kClose };

// Each service resolves its current owner at the original call point.
class DetailsWorld : public Framework::DetailInputWorld {
 public:
  virtual void RememberPage(DetailCursor cursor) = 0;
  [[nodiscard]] virtual std::span<const std::uint16_t> DisplayOrder() = 0;
  [[noreturn]] virtual void Refuse(std::string_view reason) = 0;

  virtual void ClearRows() = 0;
  [[nodiscard]] virtual Framework::MenuRow& Row(DetailRow row) = 0;
  virtual void SetIconsVisibility(std::uint8_t visibility) = 0;
  virtual void SetControlsVisibility(std::uint8_t visibility) = 0;
  virtual void ContinueTransition(DetailRow selected) = 0;
  virtual void ShowStatPages(std::int32_t pages) = 0;
  virtual void MovePrimaryStats(PrimaryStatsMotion motion) = 0;
  virtual void PlayLeaveSound() = 0;
  virtual void PlayInvalidSound() = 0;
  virtual void DismissCommandRow() = 0;
  virtual void SetPhase(DetailPhase phase) = 0;
  virtual void RememberTriangleClose(bool triangle) = 0;
  virtual void HideDpPpBars() = 0;
};

// The weapon name is mandatory even when no parts are present. Shield's
// absent source is the original empty-loadout path.
[[nodiscard]] Items::ItemsStatus BuildWeaponStats(
    Items::WeaponLoadout& target, const Items::InventoryWeapon* source,
    Items::ContainerWorld& records);
void BuildShieldStats(Items::ShieldLoadout& target,
                      const Items::InventoryShield* source,
                      Items::ContainerWorld& records);
void SetPanelVisibility(PanelVisibility& panels, std::uint8_t visibility);
[[nodiscard]] DetailPage PollDetailsPaging(DetailsWorld& world,
                                           DetailPage page);
void BeginDetailsTransition(DetailsWorld& world, DetailRow selected);
void InitializeDetailsLayout(DetailsWorld& world, DetailRow selected,
                             std::int32_t stat_pages);
[[nodiscard]] std::uint16_t SelectDetailsItem(DetailsWorld& world,
                                              DetailPage page);
void ExitDetailsView(DetailsWorld& world, bool hide_dp_pp);
}  // namespace VS::Menu::container
