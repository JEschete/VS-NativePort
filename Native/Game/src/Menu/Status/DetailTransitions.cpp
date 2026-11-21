#include "Menu/Status/DetailTransitions.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Items/EquipmentStats.hpp"
#include "Menu/Status/ComponentDescriptions.hpp"
#include "Render/EncodedText.hpp"

namespace VS::Menu::Status {  // NOLINT(readability-identifier-naming)
namespace {
constexpr std::array kDirectionButtons{0x1000U, 0x4000U, 0x8000U, 0x2000U};
constexpr int kFirstBackwardElement = 2;
constexpr int kLastBackwardElement = 15;
constexpr int kEquipmentRowBase = 10;
constexpr int kComponentRowBase = 20;
constexpr int kActorNameRow = 4;
constexpr int kTitleRow = 32;
constexpr int kTitleWidth = 164;
constexpr int kTitleBackgroundWidth = 8;
constexpr int kTitleTarget = 16;
constexpr int kRowTop = 18;
constexpr int kHoistedRowX = 155;
constexpr int kScreenWidth = 320;
constexpr int kPartWidth = 158;
constexpr int kGemWidth = 151;
constexpr int kRowPitch = 16;
constexpr int kBladeRow = 1;
constexpr int kGripRow = 2;
constexpr int kFirstWeaponGem = 3;
constexpr int kFirstShieldGem = 1;
constexpr int kWeaponTitle = 9;
constexpr int kArmorTitle = 11;
constexpr std::uint8_t kWeaponIcon = 24;
constexpr std::uint8_t kShieldIcon = 27;
constexpr std::uint8_t kArmorIcon = 28;
constexpr std::uint8_t kGemIcon = 22;
constexpr unsigned kIconShift = 26;
constexpr unsigned kMaterialShift = 16;
constexpr std::uint32_t kMaterialMask = 7;
constexpr int kPrimarySlideIn = 1;
constexpr int kPrimarySlideOut = 2;

std::optional<std::size_t> Direction(std::uint32_t repeat) {
  for (std::size_t index = 0; index < kDirectionButtons.size(); ++index) {
    if ((repeat & kDirectionButtons[index]) != 0) {
      return index;
    }
  }
  return std::nullopt;
}

std::optional<std::uint8_t> StepToSelectable(const DetailNavigation& input,
                                             int element) {
  const int step =
      element >= kFirstBackwardElement && element <= kLastBackwardElement ? -1
                                                                          : 1;
  for (element += step;
       element >= 0 &&
       element < static_cast<int>(Framework::kStatusViewElementCount);
       element += step) {
    if (IsDetailElementSelectable(element, input.flags, input.stat_page)) {
      return static_cast<std::uint8_t>(element);
    }
  }
  return std::nullopt;
}

Framework::MenuRow* EquipmentRow(DetailTransitionHost& host, int row) {
  const auto rows = host.Rows();
  if (row < 0 ||
      row >= static_cast<int>(Framework::kRowCount) - kEquipmentRowBase ||
      static_cast<std::size_t>(row) + kEquipmentRowBase >= rows.size()) {
    host.Refuse("status transition: equipment row unavailable");
    return nullptr;
  }
  return &rows[static_cast<std::size_t>(row) + kEquipmentRowBase];
}

std::span<const std::uint8_t> TitleText(DetailTransitionHost& host, int row) {
  const auto text = host.StatusText();
  const int entry = row < 2 ? row + kWeaponTitle : kArmorTitle;
  const auto title = Framework::ItemHelpEntry(text, entry);
  if (!title) {
    return {};
  }
  const auto tail = text.subspan(title->offset);
  for (std::size_t length = 0; length < tail.size(); ++length) {
    if (tail[length] == Render::kTextTerminator) {
      return tail.first(length + 1);
    }
  }
  return {};
}

bool ValidComponentRow(Framework::ItemRowsHost& host, int row) {
  if (row < 1 ||
      row >= static_cast<int>(Framework::kRowCount) - kComponentRowBase) {
    host.Refuse("status transition: component row outside menu workspace");
    return false;
  }
  return true;
}

std::optional<Framework::ItemRowText> GemText(Framework::ItemRowsHost& host,
                                              const Items::EquipmentPart& gem) {
  if (gem.id != 0) {
    Items::InventoryGem inventory{};
    Items::WriteInventoryGemStats(inventory, gem);
    return Framework::FormatGemRow(host, inventory, host.StringBuffer());
  }
  const auto name = Framework::ItemHelpText(host.HelpText(),
                                            Framework::item_text::kNoneOffset);
  if (!name) {
    host.Refuse("status transition: empty socket text unavailable");
    return std::nullopt;
  }
  return Framework::ItemRowText{.name = name,
                                .description = Framework::TextRef{},
                                .row_type = static_cast<std::uint32_t>(kGemIcon)
                                            << kIconShift};
}

Framework::MenuRow* PlaceComponent(Framework::ItemRowsHost& host, int row,
                                   int width,
                                   const Framework::ItemRowText& text,
                                   bool slide_in) {
  if (!text.name || text.name->source == Framework::TextSource::kEmpty ||
      text.name->source == Framework::TextSource::kNone ||
      (text.description &&
       text.description->source == Framework::TextSource::kEmpty)) {
    host.Refuse("status transition: component formatting unavailable");
    return nullptr;
  }
  const int menu_row = row + kComponentRowBase;
  host.DeactivateRow(menu_row);
  const int x = kScreenWidth - width;
  auto* item = host.SetMenuItem({.row = menu_row,
                                 .x = x,
                                 .y = row * kRowPitch + kRowTop,
                                 .w = width,
                                 .background_width = 0},
                                *text.name);
  if (item != nullptr) {
    item->unselectable = static_cast<std::uint8_t>(
        !text.description ||
        text.description->source == Framework::TextSource::kNone);
    if (slide_in) {
      item->x = kScreenWidth;
      item->state = Framework::RowState::kSlideX;
      item->target0 = static_cast<std::int16_t>(x);
    }
  }
  return item;
}

std::optional<Framework::ItemRowText> WeaponText(
    Framework::ItemRowsHost& host, int row,
    const Items::WeaponLoadout& weapon) {
  if (row == kBladeRow) {
    Items::InventoryBlade blade{};
    Items::WriteInventoryBladeStats(blade, weapon.blade);
    return Framework::FormatBladeRow(host, blade, host.StringBuffer());
  }
  if (row == kGripRow) {
    Items::InventoryGrip grip{};
    Items::WriteInventoryGripStats(grip, weapon.grip);
    return Framework::FormatGripRow(host, grip, host.StringBuffer());
  }
  const auto slot = static_cast<std::size_t>(row - kFirstWeaponGem);
  if (slot >= weapon.gems.size()) {
    host.Refuse("status transition: gem slot outside weapon");
    return std::nullopt;
  }
  return GemText(host, weapon.gems[slot]);
}
}  // namespace

// decomp: updateEquipmentDetailSelection (src/MENU/MENU4.PRG/120.c:466; was
// _updateEquipmentDetailSelection); original MENU4.PRG [0x801034BC,
// 0x80103608).
std::optional<std::uint8_t> NavigateDetailElements(
    const DetailNavigation& input) {
  if (input.selected >= Framework::kStatusViewElementCount ||
      input.links.size() != Framework::kStatusViewElementCount) {
    return std::nullopt;
  }
  const auto direction = Direction(input.repeat);
  auto previous = static_cast<std::uint8_t>(input.selected);
  auto selected = direction ? input.links[previous][*direction] : previous;
  std::array<bool, Framework::kStatusViewElementCount> visited{};
  while (selected < visited.size()) {
    if (IsDetailElementSelectable(selected, input.flags, input.stat_page)) {
      return selected;
    }
    if (selected == previous) {
      return StepToSelectable(input, selected);
    }
    if (visited[selected]) {
      return std::nullopt;
    }
    visited[selected] = true;
    previous = selected;
    if (!direction) {
      return std::nullopt;
    }
    selected = input.links[selected][*direction];
  }
  return std::nullopt;
}

// decomp: setActiveRow (src/MENU/MENU4.PRG/120.c:1302; was _setActiveRow);
// original MENU4.PRG [0x80104AEC, 0x80104B38).
bool HoistEquipmentRow(DetailTransitionHost& host, int row) {
  auto* item = EquipmentRow(host, row);
  if (item == nullptr || host.Rows().size() <= kActorNameRow) {
    return false;
  }
  host.FlyOutRight(kActorNameRow);
  item->state = Framework::RowState::kSlideX;
  item->target0 = kHoistedRowX;
  item->selected = 1;
  item->sub_text = nullptr;
  return true;
}

// decomp: animateEquipmentDetailTransition (src/MENU/MENU4.PRG/120.c:1321; was
// _animateEquipmentDetailTransition); original MENU4.PRG [0x80104B38,
// 0x80104C0C).
bool BeginDetailTransition(DetailTransitionHost& host, int row) {
  auto* item = EquipmentRow(host, row);
  if (item == nullptr) {
    return false;
  }
  const auto text = TitleText(host, row);
  if (text.empty() || host.Rows().size() <= kTitleRow) {
    host.Refuse("status transition: title text or row unavailable");
    return false;
  }
  auto* title = host.CreateTitle({.row = kTitleRow,
                                  .x = -kTitleWidth,
                                  .y = kRowTop,
                                  .w = kTitleWidth,
                                  .background_width = kTitleBackgroundWidth},
                                 text);
  if (title == nullptr) {
    host.Refuse("status transition: title creation refused");
    return false;
  }
  title->state = Framework::RowState::kDismiss;
  title->target0 = kTitleTarget;
  title->selected = 1;
  title->row_icon =
      row == 0 ? kWeaponIcon : (row == 1 ? kShieldIcon : kArmorIcon);
  item->state = Framework::RowState::kSlideY;
  item->target0 = kRowTop;
  return true;
}

// decomp: initUi (src/MENU/MENU4.PRG/120.c:1351; was _initUi);
// original MENU4.PRG [0x80104C0C, 0x80104C40).
bool InitializeDetailPanel(DetailTransitionHost& host, int row,
                           int class_affinity) {
  if (!BeginDetailTransition(host, row)) {
    return false;
  }
  host.SetStatPages(class_affinity);
  host.AnimatePrimaryStats(kPrimarySlideIn);
  return true;
}

// decomp: setWeaponRow (src/MENU/MENU4.PRG/120.c:1361; was _setWeaponRow);
// original MENU4.PRG [0x80104C40, 0x80104DFC).
Framework::MenuRow* SetWeaponComponentRow(Framework::ItemRowsHost& host,
                                          int row,
                                          const Items::WeaponLoadout& weapon,
                                          bool slide_in) {
  if (!ValidComponentRow(host, row)) {
    return nullptr;
  }
  if (row >= kFirstWeaponGem &&
      row - kFirstWeaponGem >= weapon.grip.gem_slots) {
    host.DeactivateRow(row + kComponentRowBase);
    return nullptr;
  }
  const auto text = WeaponText(host, row, weapon);
  if (!text) {
    return nullptr;
  }
  auto* item =
      PlaceComponent(host, row, row < kFirstWeaponGem ? kPartWidth : kGemWidth,
                     *text, slide_in);
  if (item != nullptr) {
    item->row_icon = static_cast<std::uint8_t>(text->row_type >> kIconShift);
    item->material = static_cast<std::uint8_t>(
        (text->row_type >> kMaterialShift) & kMaterialMask);
  }
  return item;
}

// decomp: setShieldRow (src/MENU/MENU4.PRG/120.c:1421; was _setShieldRow);
// original MENU4.PRG [0x80104DFC, 0x80104F2C).
Framework::MenuRow* SetShieldGemRow(Framework::ItemRowsHost& host, int row,
                                    const Items::ShieldLoadout& shield,
                                    bool slide_in) {
  if (!ValidComponentRow(host, row)) {
    return nullptr;
  }
  const auto slot = static_cast<std::size_t>(row - kFirstShieldGem);
  if (slot >= shield.base.gem_slots) {
    host.DeactivateRow(row + kComponentRowBase);
    return nullptr;
  }
  if (slot >= shield.gems.size()) {
    host.Refuse("status transition: gem slot outside shield");
    return nullptr;
  }
  const auto text = GemText(host, shield.gems[slot]);
  if (!text) {
    return nullptr;
  }
  auto* item = PlaceComponent(host, row, kGemWidth, *text, slide_in);
  if (item != nullptr) {
    item->row_icon = kGemIcon;
  }
  return item;
}

// decomp: exitEquipmentDetail (src/MENU/MENU4.PRG/120.c:1466; was
// _exitEquipmentDetail); original MENU4.PRG [0x80104F2C, 0x80104F80).
void ExitEquipmentDetails(DetailTransitionHost& host, bool clear_dp_pp) {
  host.PlayLeaveSound();
  host.ClearRowsExcept(Framework::kNoRow);
  host.FlyOutLeft(0);
  host.SetStatPages(-1);
  host.AnimatePrimaryStats(kPrimarySlideOut);
  if (clear_dp_pp) {
    host.AnimateDpPpBars(Framework::dp_pp_bars::kSlideOut);
  }
}
}  // namespace VS::Menu::Status
