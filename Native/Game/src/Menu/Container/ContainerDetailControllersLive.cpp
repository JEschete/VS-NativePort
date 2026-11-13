#include "Menu/Container/ContainerDetailControllersLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "Input/Pad.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Container/ContainerDetailControllers.hpp"
#include "Menu/Container/ContainerDetailHelpersLive.hpp"
#include "Menu/Framework/DetailDispatchLiveSupport.hpp"
#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/Actor.h"
#include "Core/Diagnostics.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Container/ItemView.h"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/ItemRowsLiveState.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace VS::Menu::container {
namespace {
using Category = Items::ItemCategory;
constexpr std::size_t kControllerCount = 6;
constexpr std::size_t kGemCountTextBytes = 8;
constexpr std::size_t kTextOwnerCount = 4;
constexpr int kWeaponFirstGemRow = 3;
constexpr std::size_t kGemDigit = 6;
constexpr int kGemCountIcon = 22 | (1 << 8);
constexpr int kGemIconX = 256;
constexpr int kGemIconY = 32;
constexpr int kGemTextX = 280;
constexpr int kGemTextY = 36;
constexpr std::uint8_t kDigitZero = 0x30;
constexpr std::uint8_t kAccessoryCategory = 7;
constexpr std::uint32_t kSelectionRowShift = 8;
constexpr std::uint32_t kDispatchCategoryShift = 4;
constexpr std::uint32_t kDispatchCategoryMask = 0xF;
constexpr std::uint32_t kNonzeroSelection = 1U << 16;
constexpr std::int32_t kIconSize = 16;
constexpr std::int32_t kCategoryHeader = 31;
constexpr int kIconSceneLayer = -3;
constexpr std::uint32_t kFirstIconU = 120;
constexpr std::uint32_t kIconV = 128;
constexpr std::uint32_t kSelectedIconPalette = 0x37FD0000;
constexpr std::uint32_t kIdleIconPalette = 0x37FE0000;
std::array<ControllerState, kControllerCount> g_states{};
DispatcherState g_dispatcher;
using GemCountText = std::array<char, kGemCountTextBytes>;
GemCountText g_gem_count_text{'X', ' ', ' ', ' ', ' ', ' ', '0', '\0'};

ControllerState& State(Category category) {
  return g_states[static_cast<std::size_t>(category)];
}

[[noreturn]] void Refuse(std::string_view reason) {
  Core_Errorf("menu.container", "Container detail controller refused: %.*s",
              static_cast<int>(reason.size()), reason.data());
  std::abort();
}

void RequireRows() {
  if (Menu_Framework_menuItems == nullptr) {
    Refuse("missing current container menu row workspace");
  }
}

Framework::MenuRow& Row(DetailRow row) {
  RequireRows();
  if (row.index < 0 ||
      static_cast<std::size_t>(row.index) >= Framework::kRowCount) {
    Refuse("container detail row outside current menu workspace");
  }
  return Framework::LiveMenuRow(row.index);
}

Items_ContainerData& Records() {
  if (Menu_Container_containerData == nullptr) {
    Refuse("missing current container record owner");
  }
  return Menu_Container_containerData->items;
}

template <typename Record, std::size_t Count>
Record& Referenced(Record (&table)[Count], ContainerSlot item) {
  if (item.one_based == 0 || item.one_based > Count) {
    Refuse("container detail reference outside its used record table");
  }
  return table[item.one_based - 1];
}

template <typename Record, std::size_t Count>
void RequireBound(const Record* bound, Record (&current)[Count]) {
  if (bound != current) {
    Refuse("container detail bound table is not the current container");
  }
}

void RequireAshleyStats() {
  if (Battle_characterState == nullptr ||
      Battle_characterState->stats == nullptr) {
    Refuse("missing current equipped actor statistics");
  }
}

void RequireText(bool names, bool buffer) {
  if (Menu_Framework_itemHelp == nullptr ||
      (names && Menu_Framework_itemNames == nullptr)) {
    Refuse("missing used container item name or help text");
  }
  if (buffer && Menu_Framework_stringBuf == nullptr) {
    Refuse("missing current container row description workspace");
  }
}

std::optional<ControllerEntry> Selection(int selection) {
  if (selection == 0) {
    return std::nullopt;
  }
  const auto command = static_cast<std::uint32_t>(selection);
  return ControllerEntry{
      DetailRow{static_cast<std::uint8_t>(command >> kSelectionRowShift)},
      DetailPage{static_cast<std::uint8_t>(command - 1)}};
}

int ControllerCommand(std::optional<ControllerEntry> selection) {
  if (!selection.has_value()) {
    return 0;
  }
  const auto entry = selection.value();
  const auto page = static_cast<std::uint8_t>(
      static_cast<std::uint32_t>(entry.page.index) + 1);
  const auto command =
      (static_cast<std::uint32_t>(static_cast<std::uint8_t>(entry.row.index))
       << kSelectionRowShift) |
      page;
  return static_cast<int>(command != 0 ? command : kNonzeroSelection);
}

std::optional<DispatchEntry> Dispatch(int selection) {
  if (selection == 0) {
    return std::nullopt;
  }
  return DispatchEntry{
      static_cast<Category>(static_cast<std::uint32_t>(selection) &
                            kDispatchCategoryMask),
      Selection(selection >> kDispatchCategoryShift)};
}

using TextOwners = std::array<std::span<char>, kTextOwnerCount>;

TextOwners BorrowTextOwners(std::span<char> weapon_name) {
  return {
      weapon_name,
      std::span<char>{Menu_Framework_stringBuf,
                      Menu_Framework_stringBuf == nullptr
                          ? 0
                          : Framework::kStringBufferBytes},
      std::span<char>{
          reinterpret_cast<char*>(Menu_Framework_itemHelp),
          Menu_Framework_itemHelp == nullptr ? 0 : Framework::kItemHelpBytes},
      std::span<char>{reinterpret_cast<char*>(Menu_Framework_itemNames),
                      Menu_Framework_itemNames == nullptr
                          ? 0
                          : Framework::kItemNamesBytes}};
}

std::span<char> BorrowedText(char* text, const TextOwners& owners) {
  if (text == nullptr) {
    Refuse("formatter did not supply used container row text");
  }
  const auto address = reinterpret_cast<std::uintptr_t>(text);
  for (const auto table : owners) {
    const auto first = reinterpret_cast<std::uintptr_t>(table.data());
    if (!table.empty() && address >= first && address - first < table.size()) {
      return {text, table.size() - (address - first)};
    }
  }
  Refuse("formatter text outside its borrowed container owners");
}

void RequireFrame(bool overlay) {
  if (Render_frameScratch.primitiveCursor == nullptr ||
      (overlay ? Render_frameScratch.overlayOrderingTable
               : Render_frameScratch.orderingTable) == nullptr) {
    Refuse("missing used container presentation frame");
  }
}

void RequireGem(std::uint8_t reference) {
  if (reference == 0) {
    return;
  }
  auto& table = Records().gems;
  RequireBound(Menu_Framework_gems, table);
  (void)Referenced(table, {reference});
}

class LiveControllerWorld final : public ControllerWorld {
 public:
  explicit LiveControllerWorld(Category category) : category_(category) {}
  std::int32_t RowsReady() override {
    RequireRows();
    return Framework::AreLiveMenuRowsIdle();
  }
  std::uint32_t PressedButtons() override {
    return Input_buttonsPressed.allButtons;
  }
  void BeginTransition(DetailRow row) override {
    BeginLiveDetailsTransition(row.index);
  }
  void InitializeLayout(DetailRow row, std::int32_t pages) override {
    InitializeLiveDetailsLayout(row.index, pages);
  }
  ContainerSlot CurrentItem(DetailPage page) override {
    const auto order = CurrentOrder();
    if (page.index < 0 ||
        static_cast<std::size_t>(page.index) >= order.size()) {
      Refuse("current page outside direct container display order");
    }
    return {order[static_cast<std::size_t>(page.index)]};
  }
  void SetStatistics(ContainerSlot item) override {
    switch (category_) {
      case Category::kWeapon:
        if (item.one_based != 0) {
          auto& table = Records().weapons;
          RequireBound(Menu_Framework_weapons, table);
          if (Referenced(table, item).isEquipped != 0) {
            RequireAshleyStats();
          }
        }
        Framework::SetLiveWeaponStats(item.one_based);
        break;
      case Category::kBlade:
        RequireBound(Menu_Framework_blades, Records().blades);
        (void)Referenced(Records().blades, item);
        Framework::SetLiveBladeStats(item.one_based);
        break;
      case Category::kGrip:
        RequireBound(Menu_Framework_grips, Records().grips);
        (void)Referenced(Records().grips, item);
        Framework::SetLiveGripStats(item.one_based);
        break;
      case Category::kShield:
        if (item.one_based != 0) {
          auto& table = Records().shields;
          RequireBound(Menu_Framework_shields, table);
          if (Referenced(table, item).isEquipped != 0) {
            RequireAshleyStats();
          }
        }
        Framework::SetLiveShieldStats(item.one_based);
        break;
      case Category::kArmor:
        SetArmorStatistics(item);
        break;
      case Category::kGem:
        RequireBound(Menu_Framework_gems, Records().gems);
        (void)Referenced(Records().gems, item);
        Framework::SetLiveGemStats(item.one_based);
        break;
      default:
        Refuse("unsupported container statistics category");
    }
  }
  void OpenDpPpBars() override {
    Framework::UpdateLiveDpPpBars(Framework::dp_pp_bars::kBoth);
  }
  void OpenDpBar() override {
    Framework::UpdateLiveDpPpBars(Framework::dp_pp_bars::kDp);
  }
  void CreateComponentRow(std::int32_t row, ContainerSlot item,
                          bool slide_in) override {
    RequireRows();
    if (category_ == Category::kWeapon) {
      RequireWeaponRow(row, item);
      (void)Framework::CreateLiveWeaponComponentRow(row, item.one_based,
                                                    slide_in ? 1 : 0);
    } else if (category_ == Category::kShield) {
      RequireShieldRow(row, item);
      (void)Framework::CreateLiveShieldGemRow(row, item.one_based,
                                              slide_in ? 1 : 0);
    } else {
      Refuse("component row requested for a container component");
    }
  }
  void Exit(bool hide_dp_pp) override {
    ExitLiveDetailsView(hide_dp_pp ? 1 : 0);
  }
  void RememberTriangleClose(bool triangle) override {
    Menu_Container_detailsClosedWithTriangle = triangle ? Input::Triangle : 0;
  }
  DetailPage PollPaging(DetailPage page) override {
    return {PollLiveDetailsPaging(static_cast<int>(category_), page.index)};
  }
  ContainerSlot SelectItem(DetailPage page) override {
    return {static_cast<std::uint16_t>(
        SelectLiveDetailsItem(static_cast<int>(category_), page.index))};
  }
  DetailRowText FormatItem(ContainerSlot item) override {
    std::array<char*, 2> text{};
    int row_type = 0;
    auto& records = Records();
    std::span<char> name;
    if (category_ == Category::kWeapon) {
      name = Referenced(records.weapons, item).name;
    }
    const auto owners = BorrowTextOwners(name);
    const bool buffer =
        category_ != Category::kArmor ||
        Items::Live::Typed(Referenced(records.armor, item)).category !=
            kAccessoryCategory;
    RequireText(category_ != Category::kGrip, buffer);
    FormatRecord(item, text, row_type);
    return {.name = BorrowedText(text[0], owners),
            .description = BorrowedText(text[1], owners),
            .row_type = static_cast<std::uint32_t>(row_type)};
  }
  void ChangePage(DetailRow row, const DetailRowText& text,
                  DetailPage page) override {
    (void)Row(row);
    std::array<char*, 2> borrowed{text.name.data(), text.description.data()};
    Framework::UpdateLiveItemDetailsSelection(row.index, borrowed.data(),
                                              text.row_type, page.index);
  }
  std::uint8_t GripGemSlots(ContainerSlot item) override {
    return Items::Live::Typed(Referenced(Records().grips, item)).gem_slots;
  }
  void DrawGripGemSlotCount(std::uint8_t slots) override {
    g_gem_count_text[kGemDigit] =
        static_cast<char>(static_cast<std::uint8_t>(slots + kDigitZero));
    RequireFrame(true);
    Framework::DrawLiveRowIcon(kGemCountIcon, kGemIconX, kGemIconY);
    Render::DrawLiveCompactText(
        g_gem_count_text.data(),
        static_cast<int>(Framework::PackXy(kGemTextX, kGemTextY)), nullptr);
  }
  bool SelectedArmorShowsDp(ContainerSlot item) override {
    // #366: the original inventory owner disagrees with this container view.
    return Items::Live::Typed(Referenced(Records().armor, item)).category !=
           kAccessoryCategory;
  }

 private:
  std::span<const std::uint16_t> CurrentOrder() const {
    if (Menu_Container_containerData == nullptr) {
      Refuse("missing current container display-order owner");
    }
    const auto& indices = Menu_Container_containerData->indices;
    switch (category_) {
      case Category::kWeapon:
        return indices.weapons;
      case Category::kBlade:
        return indices.blades;
      case Category::kGrip:
        return indices.grips;
      case Category::kShield:
        return indices.shields;
      case Category::kArmor:
        return indices.armor;
      case Category::kGem:
        return indices.gems;
      default:
        Refuse("unsupported direct container display-order category");
    }
  }
  void SetArmorStatistics(ContainerSlot item) const {
    if (item.one_based != 0) {
      RequireBound(Menu_Framework_armor, Records().armor);
      const auto armor = Items::Live::Typed(Referenced(Records().armor, item));
      if (armor.limb >= 1 && armor.limb <= Items::kLimbCount) {
        RequireAshleyStats();
      }
    }
    Framework::SetLiveArmorStats(item.one_based);
  }
  void FormatRecord(ContainerSlot item, std::array<char*, 2>& text,
                    int& row_type) const {
    auto& records = Records();
    char* const buffer = Menu_Framework_stringBuf;
    switch (category_) {
      case Category::kWeapon:
        RequireBound(Menu_Framework_weapons, records.weapons);
        Framework::FormatLiveInventoryWeaponRow(
            &Referenced(records.weapons, item), text.data(), &row_type, buffer);
        break;
      case Category::kBlade:
        Framework::FormatLiveBladeRow(&Referenced(records.blades, item),
                                      text.data(), &row_type, buffer);
        break;
      case Category::kGrip:
        Framework::FormatLiveGripRow(&Referenced(records.grips, item),
                                     text.data(), &row_type, buffer);
        break;
      case Category::kShield:
        RequireBound(Menu_Framework_shields, records.shields);
        Framework::FormatLiveInventoryShieldRow(
            &Referenced(records.shields, item), text.data(), &row_type, buffer);
        break;
      case Category::kArmor:
        Framework::FormatLiveInventoryArmorRow(&Referenced(records.armor, item),
                                               text.data(), &row_type, buffer);
        break;
      case Category::kGem:
        Framework::FormatLiveGemRow(&Referenced(records.gems, item),
                                    text.data(), &row_type, buffer);
        break;
      default:
        Refuse("unsupported container row formatter");
    }
  }
  void RequireWeaponRow(int row, ContainerSlot item) const {
    RequireBound(Menu_Framework_weapons, Records().weapons);
    const auto weapon = Items::Live::Typed(Referenced(Records().weapons, item));
    if (row == 1) {
      RequireBound(Menu_Framework_blades, Records().blades);
      (void)Referenced(Records().blades, {weapon.blade});
      RequireText(true, true);
      return;
    }
    RequireBound(Menu_Framework_grips, Records().grips);
    const auto grip =
        Items::Live::Typed(Referenced(Records().grips, {weapon.grip}));
    if (row == 2) {
      RequireText(false, true);
    } else if (row >= kWeaponFirstGemRow &&
               std::cmp_less(row,
                             kWeaponFirstGemRow + Items::kWeaponGemCount) &&
               row - kWeaponFirstGemRow < grip.gem_slots) {
      const auto gem =
          weapon.gems[static_cast<std::size_t>(row - kWeaponFirstGemRow)];
      RequireGem(gem);
      if (gem != 0) {
        RequireText(true, true);
      }
    }
  }
  void RequireShieldRow(int row, ContainerSlot item) const {
    RequireBound(Menu_Framework_shields, Records().shields);
    const auto shield = Items::Live::Typed(Referenced(Records().shields, item));
    if (row >= 1 && std::cmp_less_equal(row, Items::kWeaponGemCount) &&
        row - 1 < shield.base.gem_slots) {
      const auto gem = shield.gems[static_cast<std::size_t>(row - 1)];
      RequireGem(gem);
      if (gem != 0) {
        RequireText(true, true);
      }
    }
  }
  Category category_;
};

class LiveIconWorld final : public CategoryIconWorld {
 public:
  std::uint8_t CurrentCategory() override {
    return static_cast<std::uint8_t>(Menu_Framework_menuMemory.opaqueA0[0]);
  }
  void DrawListTabNavigation() override {
    RequireFrame(true);
    Framework::DrawLiveTabNavigation(0);
  }
  Framework::MenuRow& CategoryHeader() override {
    return Row({kCategoryHeader});
  }
  void DrawIcon(CategoryIcon icon) override {
    RequireFrame(icon.overlay);
    const auto xy = static_cast<int>(Framework::PackXy(icon.x, icon.y));
    const auto size = static_cast<int>(Framework::PackXy(kIconSize, kIconSize));
    u_long* chain = nullptr;
    if (!icon.overlay) {
      chain = Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene,
                                             kIconSceneLayer);
      if (chain == nullptr) {
        Refuse("container icon outside current scene ordering table");
      }
    }
    auto* packet = icon.overlay
                       ? Render::QueueLiveDefaultSprite(size, xy)
                       : Render::QueueLiveSpriteWithTexturePage(
                             Render::kDefaultSpriteBrightness, xy, size, chain);
    packet[Render::kSpriteTextureWord] =
        (kFirstIconU + icon.category * kIconSize) |
        (kIconV << Framework::kByteShift) |
        (icon.selected_palette ? kSelectedIconPalette : kIdleIconPalette);
  }
};

class LiveDispatcherWorld final
    : public Framework::LiveDetailDispatcherWorld<DispatcherWorld, RequireRows,
                                                  RequireFrame> {
  using Base = Framework::LiveDetailDispatcherWorld<DispatcherWorld,
                                                    RequireRows, RequireFrame>;

 public:
  LiveDispatcherWorld()
      : Base("used container detail cursor outside its elements") {}
  void SetViewMode(DetailViewMode mode) override {
    Menu_Container_detailsViewPhase = static_cast<char>(mode);
  }
  void RememberTriangleClose(bool triangle) override {
    Menu_Container_detailsClosedWithTriangle = triangle ? Input::Triangle : 0;
  }
  Framework::MenuRow& CategoryHeader() override {
    return Row({kCategoryHeader});
  }
  void SetCommand(DetailCommand command) override {
    Framework::SetLiveDetailCommand(command == DetailCommand::kMenu);
  }
  std::int32_t RunController(
      Category category, std::optional<ControllerEntry> selection) override {
    const int command = ControllerCommand(selection);
    static constexpr std::array kControllers{
        UpdateLiveWeaponDetails, UpdateLiveBladeDetails, UpdateLiveGripDetails,
        UpdateLiveShieldDetails, UpdateLiveArmorDetails, UpdateLiveGemDetails};
    const auto result = Framework::RunLiveDetailController(
        static_cast<std::uint8_t>(category), command, kControllers);
    if (!result.has_value()) {
      container::Refuse("container dispatcher category outside controllers");
    }
    return result.value();
  }
  bool ClosedWithTriangle() override {
    return Menu_Container_detailsClosedWithTriangle != 0;
  }
  void SetIconsVisibility(std::uint8_t visibility) override {
    Menu_Container_categoryIconsVisible = static_cast<char>(visibility);
  }
  void SetControlsVisibility(std::uint8_t visibility) override {
    Menu_Container_controlsPanelMode = static_cast<char>(visibility);
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    container::Refuse(reason);
  }
};
}  // namespace

// decomp: func_80103E24 (src/MENU/MENUD.PRG/234.c:700).
void DrawLiveCategoryIcons(int animation_step, int details_phase) {
  LiveIconWorld world;
  DrawCategoryIcons(world, animation_step, details_phase);
}

// decomp: _weaponNavigation (src/MENU/MENUD.PRG/234.c:829).
int UpdateLiveWeaponDetails(int selection) {
  LiveControllerWorld world(Category::kWeapon);
  return UpdateWeaponDetails(State(Category::kWeapon), world,
                             Selection(selection));
}

// decomp: _bladeNavigation (src/MENU/MENUD.PRG/234.c:902).
int UpdateLiveBladeDetails(int selection) {
  LiveControllerWorld world(Category::kBlade);
  return UpdateBladeDetails(State(Category::kBlade), world,
                            Selection(selection));
}

// decomp: _gripNavigation (src/MENU/MENUD.PRG/234.c:965).
int UpdateLiveGripDetails(int selection) {
  LiveControllerWorld world(Category::kGrip);
  return UpdateGripDetails(State(Category::kGrip), world, Selection(selection));
}

// decomp: _shieldNavigation (src/MENU/MENUD.PRG/234.c:1037).
int UpdateLiveShieldDetails(int selection) {
  LiveControllerWorld world(Category::kShield);
  return UpdateShieldDetails(State(Category::kShield), world,
                             Selection(selection));
}

// decomp: _armorNavigation (src/MENU/MENUD.PRG/234.c:1110).
int UpdateLiveArmorDetails(int selection) {
  LiveControllerWorld world(Category::kArmor);
  return UpdateArmorDetails(State(Category::kArmor), world,
                            Selection(selection));
}

// decomp: _gemNavigation (src/MENU/MENUD.PRG/234.c:1176).
int UpdateLiveGemDetails(int selection) {
  LiveControllerWorld world(Category::kGem);
  return UpdateGemDetails(State(Category::kGem), world, Selection(selection));
}

// decomp: func_80105008 (src/MENU/MENUD.PRG/234.c:1235).
int UpdateLiveItemDetails(int selection) {
  LiveDispatcherWorld world;
  return UpdateItemDetails(g_dispatcher, world, Dispatch(selection));
}

void ResetLiveContainerDetailControllers() {
  g_states = {};
  g_dispatcher = {};
  g_gem_count_text = {'X', ' ', ' ', ' ', ' ', ' ', '0', '\0'};
}
}  // namespace VS::Menu::container
