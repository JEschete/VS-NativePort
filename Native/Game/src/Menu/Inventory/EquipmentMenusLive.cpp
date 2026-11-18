// Hand-maintained (tools/port/owned.csv).
// Live binding for the equipment menu (EquipmentMenus.cpp): Ashley's
// inventory, MENU3.PRG's globals, the menu rows, the equipment stat panels,
// the choice list and Ashley's equipment.
#include "Menu/Inventory/EquipmentMenusLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/Actor.h"
#include "Core/Log.hpp"
#include "Input/ControllerLiveState.h"
#include "Items/EquipmentLive.hpp"
#include "Items/Inventory.h"
#include "Items/InventoryLive.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Framework/ItemRowsLiveState.hpp"
#include "Menu/Inventory/DetailsPoliciesLive.hpp"
#include "Menu/Inventory/EquipmentMenus.hpp"
#include "Menu/Inventory/EquipmentRowsLive.hpp"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Sounds.hpp"

namespace VS::Menu::Inventory {
namespace {
using Framework::MenuRow;
using Framework::RowPlacement;

constexpr std::uint32_t kRefusalLogLimit = 16;
constexpr std::uint32_t kRefusalLogInterval = 1024;
// The description buffer each formatted row writes (ItemRowsLive.hpp).
constexpr std::size_t kRowBufferBytes = 96;
static_assert(std::size(Menu_Inventory_menuText) == kEquipmentMenuTextEntries,
              "_menuText holds 472 halfwords");

std::uint64_t g_refusals = 0;
// decomp: the slot menus' statics (16C.c:347-350, 507-511, 687-691,
// 864-867), _dispatchEquipMenu's currentEquipMenu (16C.c:1033) and
// _equipMenu's selectedRow and state (16C.c:1060-1061).
AccessoryMenuState g_accessory;
WeaponMenuState g_weapon;
ShieldMenuState g_shield;
ArmorMenuState g_armor;
std::uint8_t g_current_menu = 0;
EquipmentMenuState g_menu;

// decomp: _dispatchEquipMenu's equipMenuHandlers (16C.c:1030).
constexpr EquipmentMenuHandlers kLiveHandlers{
    UpdateLiveArmorMenu,
    UpdateLiveWeaponMenu,
    UpdateLiveShieldMenu,
    UpdateLiveAccessoryMenu,
};

void Refuse(std::string_view reason) {
  ++g_refusals;
  if (g_refusals <= kRefusalLogLimit || g_refusals % kRefusalLogInterval == 0) {
    VS_LOG_WARN("menu.inventory", "equipment menus: {} (refusal {})", reason,
                g_refusals);
  }
}

[[nodiscard]] int Resolve(std::optional<std::int32_t> result,
                          std::string_view reason) {
  if (!result) {
    Refuse(reason);
    return 0;
  }
  return *result;
}

// The record at a 0-based slot, or nullptr outside the table.
template <typename T, std::size_t N>
[[nodiscard]] T* Slot(T (&table)[N], std::int32_t slot) {
  if (slot < 0 || static_cast<std::size_t>(slot) >= N) {
    return nullptr;
  }
  return &std::span(table)[static_cast<std::size_t>(slot)];
}

[[nodiscard]] std::span<const u_char> IndexList(EquipmentTable table) {
  switch (table) {
    case EquipmentTable::kWeapons:
      return Audio_Seq_unusedInventoryIndices.weapons;
    case EquipmentTable::kShields:
      return Audio_Seq_unusedInventoryIndices.shields;
    case EquipmentTable::kArmor:
      return Audio_Seq_unusedInventoryIndices.armor;
  }
  return {};
}

[[nodiscard]] std::size_t MenuTextIndex(std::uint16_t offset) {
  return std::min<std::size_t>(offset, kEquipmentMenuTextEntries - 1);
}

class LiveEquipmentMenusHost final : public EquipmentMenusHost {
 public:
  std::int32_t CountItems(std::int32_t category) override {
    return Items::Inventory::CountLiveItems(category, nullptr);
  }
  std::uint8_t ListedSlot(EquipmentTable table,
                          std::int32_t position) override {
    const auto list = IndexList(table);
    if (position < 0 || static_cast<std::size_t>(position) >= list.size()) {
      return 0;
    }
    return list[static_cast<std::size_t>(position)];
  }
  bool IsEquipped(EquipmentTable table, std::int32_t slot) override {
    if (table == EquipmentTable::kWeapons) {
      const auto* weapon = Slot(Audio_Seq_unusedInventory.weapons, slot);
      return weapon != nullptr && weapon->isEquipped != 0;
    }
    const auto* shield = Slot(Audio_Seq_unusedInventory.shields, slot);
    return table == EquipmentTable::kShields && shield != nullptr &&
           shield->isEquipped != 0;
  }
  void MarkEquipped(EquipmentTable table, std::int32_t slot) override {
    if (table == EquipmentTable::kWeapons) {
      if (auto* weapon = Slot(Audio_Seq_unusedInventory.weapons, slot)) {
        weapon->isEquipped = 1;
      }
    } else if (table == EquipmentTable::kShields) {
      if (auto* shield = Slot(Audio_Seq_unusedInventory.shields, slot)) {
        shield->isEquipped = 1;
      }
    }
  }
  std::uint8_t ArmorLimb(std::int32_t slot) override {
    const auto* armor = Slot(Audio_Seq_unusedInventory.armor, slot);
    return armor != nullptr ? armor->limb : 0;
  }
  std::uint8_t ArmorCategory(std::int32_t slot) override {
    const auto* armor = Slot(Audio_Seq_unusedInventory.armor, slot);
    return armor != nullptr ? static_cast<std::uint8_t>(armor->category) : 0;
  }
  void SetArmorLimb(std::int32_t slot, std::uint8_t limb) override {
    if (auto* armor = Slot(Audio_Seq_unusedInventory.armor, slot)) {
      armor->limb = limb;
    }
  }
  std::uint8_t WeaponBlade(std::int32_t slot) override {
    const auto* weapon = Slot(Audio_Seq_unusedInventory.weapons, slot);
    return weapon != nullptr ? weapon->blade : 0;
  }
  BladeModel Blade(std::int32_t slot) override {
    const auto* blade = Slot(Audio_Seq_unusedInventory.blades, slot);
    if (blade == nullptr) {
      return {};
    }
    return {.category = static_cast<std::uint8_t>(blade->category),
            .model_id = static_cast<std::uint8_t>(blade->wepId),
            .material = static_cast<std::uint8_t>(blade->material)};
  }
  ShieldModel Shield(std::int32_t slot) override {
    const auto* shield = Slot(Audio_Seq_unusedInventory.shields, slot);
    if (shield == nullptr) {
      return {};
    }
    return {.model_id = static_cast<std::uint8_t>(shield->base.wepId),
            .material = static_cast<std::uint8_t>(shield->base.material)};
  }
  std::optional<HeldBlade> HeldWeapon() override {
    const Battle_Actor* actor = Battle_characterState;
    if (actor == nullptr || actor->stats == nullptr) {
      return std::nullopt;
    }
    const auto& blade = actor->stats->weapon.blade;
    return HeldBlade{.id = blade.id,
                     .category = static_cast<std::uint8_t>(blade.category)};
  }
  bool IsTwoHanded(std::uint8_t category) override {
    if (category == 0 ||
        category > VS::Menu::Framework::kWeaponHandCategories) {
      return false;
    }
    return std::span(VS::Menu::Framework::weaponHands)[category - 1U] != 0;
  }

  bool RowsIdle() override { return Framework::AreLiveMenuRowsIdle() != 0; }
  void SetRowText(std::int32_t row, std::uint16_t name,
                  std::uint16_t description) override {
    const auto texts = RowTexts(row);
    if (texts.empty()) {
      return;
    }
    texts[0] = MenuText(name);
    texts[1] = MenuText(description);
  }
  std::uint16_t MenuTextEntry(std::size_t index) override {
    if (index >= kEquipmentMenuTextEntries) {
      return kEquipmentMenuTextEntries;
    }
    return std::span(Menu_Inventory_menuText)[index];
  }
  std::int32_t FormatRow(EquipmentTable table, std::int32_t slot,
                         std::int32_t row) override {
    const auto texts = RowTexts(row);
    if (texts.empty()) {
      return 0;
    }
    void* buffer = buffers_.at(static_cast<std::size_t>(row)).data();
    int flags = 0;
    switch (table) {
      case EquipmentTable::kWeapons:
        Framework::FormatLiveInventoryWeaponRow(
            Slot(Audio_Seq_unusedInventory.weapons, slot), texts.data(), &flags,
            buffer);
        break;
      case EquipmentTable::kShields:
        Framework::FormatLiveInventoryShieldRow(
            Slot(Audio_Seq_unusedInventory.shields, slot), texts.data(), &flags,
            buffer);
        break;
      case EquipmentTable::kArmor:
        Framework::FormatLiveInventoryArmorRow(
            Slot(Audio_Seq_unusedInventory.armor, slot), texts.data(), &flags,
            buffer);
        break;
    }
    return flags;
  }
  void CreateChoiceRows(std::span<const std::int32_t> flags) override {
    std::array<int, kEquipmentChoiceRows> row_types{};
    const std::size_t count = std::min(flags.size(), row_types.size());
    std::copy_n(flags.begin(), count, row_types.begin());
    LiveCreateEquipmentRows(static_cast<int>(count),
                            reinterpret_cast<u_short**>(text_.data()),
                            row_types.data());
  }
  void UpdateChoices(std::int32_t has_sfx) override {
    UpdateLiveEquipmentChoices(has_sfx);
  }
  std::int32_t SelectedChoice() override { return LiveSelectedEquipmentRow(); }
  std::uint8_t HighlightedItem() override {
    return static_cast<std::uint8_t>(Menu_Inventory_equippedItem);
  }

  void PreviewItem(EquipmentMenu menu, std::int32_t item) override {
    switch (menu) {
      case EquipmentMenu::kArmor:
        Framework::SetLiveArmorStats(item);
        break;
      case EquipmentMenu::kWeapon:
        Framework::SetLiveWeaponStats(item);
        break;
      case EquipmentMenu::kShield:
        Framework::SetLiveShieldStats(item);
        break;
      case EquipmentMenu::kAccessory:
        SetLiveAccessoryPreviewStats(item);
        break;
    }
  }
  void UpdateStatPages(std::int32_t command) override {
    Framework::UpdateLiveEquipmentStatPages(command);
  }
  void UpdateDpPpBars(std::int32_t flags) override {
    Framework::UpdateLiveDpPpBars(flags);
  }
  void UpdatePrimaryStatPanel(std::int32_t animate) override {
    Framework::UpdateLivePrimaryStatPanel(animate);
  }
  void ClearMenuExcept(std::int32_t row) override {
    Framework::ClearLiveMenuExcept(row);
  }
  void FlyOutRowLeft(std::int32_t row) override {
    Framework::FlyOutLiveRowLeft(row);
  }

  void UnequipWeapon() override { Items::Inventory::UnequipLiveWeapon(); }
  void UnequipShield() override { Items::Inventory::UnequipLiveShield(); }
  void EquipWeapon(std::int32_t slot) override {
    Items::EquipLiveAshleyWeapon(Slot(Audio_Seq_unusedInventory.weapons, slot));
  }
  void EquipShield(std::int32_t slot) override {
    Items::EquipLiveAshleyShield(Slot(Audio_Seq_unusedInventory.shields, slot));
  }
  void EquipArmor(std::int32_t part,
                  std::optional<std::int32_t> slot) override {
    Items::EquipLiveAshleyArmor(
        part, slot ? Slot(Audio_Seq_unusedInventory.armor, *slot) : nullptr);
  }
  void EquipAccessory(std::optional<std::int32_t> slot) override {
    Items::EquipLiveAshleyAccessory(
        slot ? Slot(Audio_Seq_unusedInventory.armor, *slot) : nullptr);
  }
  bool ReloadModels(std::int32_t category, std::int32_t model_id,
                    std::int32_t material) override {
    return ReloadLiveEquipmentModels(category, model_id, material) != 0;
  }

  MenuRow& SetMenuItem(const RowPlacement& placement,
                       std::uint16_t text) override {
    const auto bytes =
        std::as_bytes(std::span(Menu_Inventory_menuText))
            .subspan(MenuTextIndex(text) * sizeof(Menu_Inventory_menuText[0]));
    return Framework::SetLiveMenuItemText(
        placement,
        {reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()});
  }
  void CreateTopRows(std::span<const std::int32_t, kEquipmentTopRows> flags,
                     std::int32_t row_info) override {
    std::array<int, kEquipmentTopRows> row_types{};
    std::copy(flags.begin(), flags.end(), row_types.begin());
    Framework::CreateLiveMenuRows(static_cast<int>(kEquipmentTopRows), row_info,
                                  text_.data(), row_types.data());
  }
  bool SquarePressed() override {
    return (Input_buttonsPressed.allButtons & PADRleft) != 0;
  }
  void PlayInvalidSound() override { Menu::PlayInvalidSound(); }
  std::int32_t SelectedRow() override { return Framework::LiveSelectedRow(); }
  std::int32_t HighlightedRow() override {
    return Framework::LiveHighlightedRow();
  }
  void StartTransition(std::int32_t row) override {
    BeginLiveDetailsTransition(row);
  }
  void ContinueTransition(std::int32_t row) override {
    Framework::ContinueLiveItemDetailsTransition(row);
  }
  void SetPreviewState(std::int32_t state) override {
    Menu_Inventory_ashleyRenderState = state;
  }
  void SetCategoryIconsEnabled(std::uint8_t enabled) override {
    Menu_Inventory_itemCategoryIconsEnabled = static_cast<char>(enabled);
  }
  void SetStatusCommandState(std::uint8_t state) override {
    Menu_Inventory_statusCommandState = static_cast<char>(state);
  }
  void SetCategoryIconOnTop(std::int32_t icon) override {
    Menu_Inventory_selectedItemCategoryIconOnTop = static_cast<char>(icon);
  }
  std::int32_t Dispatch(std::int32_t row) override {
    return DispatchLiveEquipmentMenu(row);
  }

 private:
  // A row's name and description pointers; empty outside the list.
  [[nodiscard]] std::span<char*> RowTexts(std::int32_t row) {
    if (row < 0 || static_cast<std::size_t>(row) >= kEquipmentChoiceRows) {
      return {};
    }
    return std::span(text_).subspan(static_cast<std::size_t>(row) * 2, 2);
  }
  [[nodiscard]] static char* MenuText(std::uint16_t offset) {
    return reinterpret_cast<char*>(
        &std::span(Menu_Inventory_menuText)[MenuTextIndex(offset)]);
  }

  // The rows' texts and description buffers, as the original's stack
  // arrays: built and handed to the row builder within one call.
  std::array<char*, kEquipmentChoiceRows * 2> text_{};
  std::array<std::array<char, kRowBufferBytes>, kEquipmentChoiceRows>
      buffers_{};
};
}  // namespace

int UpdateLiveAccessoryMenu(int initialize) {
  LiveEquipmentMenusHost host;
  return Resolve(UpdateAccessoryMenu(g_accessory, host, initialize),
                 "accessory menu: an armor count, listed slot or chosen "
                 "accessory outside the inventory");
}

int UpdateLiveWeaponMenu(int initialize) {
  LiveEquipmentMenusHost host;
  return Resolve(UpdateWeaponMenu(g_weapon, host, initialize),
                 "weapon menu: a weapon count, listed slot, chosen weapon, "
                 "blade or category outside the inventory");
}

int UpdateLiveShieldMenu(int initialize) {
  LiveEquipmentMenusHost host;
  return Resolve(UpdateShieldMenu(g_shield, host, initialize),
                 "shield menu: a shield count, listed slot or chosen shield "
                 "outside the inventory, or no battle stats for Ashley");
}

int UpdateLiveArmorMenu(int body_part) {
  LiveEquipmentMenusHost host;
  return Resolve(UpdateArmorMenu(g_armor, host, body_part),
                 "armor menu: a body part, armor count, listed slot or "
                 "chosen armor outside the inventory");
}

int DispatchLiveEquipmentMenu(int row) {
  return Resolve(DispatchEquipmentMenu(g_current_menu, kLiveHandlers, row),
                 "dispatch: a row outside the equipment menu");
}

int UpdateLiveEquipmentMenu(int initialize) {
  LiveEquipmentMenusHost host;
  return Resolve(UpdateEquipmentMenu(g_menu, host, initialize),
                 "equipment menu: a selected row, hint or none text outside "
                 "the menu");
}

const EquipmentMenuHandlers& LiveEquipmentMenuHandlers() {
  return kLiveHandlers;
}

void ResetEquipmentMenusLive() {
  g_accessory = {};
  g_weapon = {};
  g_shield = {};
  g_armor = {};
  g_current_menu = 0;
  g_menu = {};
}

std::uint64_t LiveEquipmentMenusRefusalCount() { return g_refusals; }
}  // namespace VS::Menu::Inventory
