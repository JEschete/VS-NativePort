// Hand-maintained (tools/port/owned.csv).
// The equipment menu (MENU3.PRG 16C.c:343-1233): the four slot menus, the
// handler table that picks one for a top-menu row, and the top menu.
#include "Menu/Inventory/EquipmentMenus.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Menu/Framework/Framework.hpp"

namespace VS::Menu::Inventory {
namespace {
using Framework::MenuRow;
using Framework::RowPlacement;
using Framework::RowState;

// _menuText's "none" row (VS_menuText_INDEX_none, items0.h).
constexpr std::size_t kNoneNameIndex = 20;
constexpr std::size_t kNoneDescriptionIndex = 21;
constexpr std::int32_t kNoneRowFlags = 1;
// A choice row's flags carry the 1-based slot of its item from this bit.
constexpr int kChoiceSlotShift = 19;
// vs_mainMenu_menuItemFlyoutLeft's "every row".
constexpr std::int32_t kEveryRow = -1;
// vs_mainMenu_drawClassAffinityType: show the pages, hide them.
constexpr std::int32_t kShowStatPages = 7;
constexpr std::int32_t kHideStatPages = -1;
// vs_mainMenu_renderEquipStats: slide in, slide out.
constexpr std::int32_t kOpenPrimaryPanel = 1;
constexpr std::int32_t kClosePrimaryPanel = 2;
// vs_mainMenu_renderDpPpBars: weapon/shield bars, armor bars, close.
constexpr std::int32_t kWeaponDpPpBars = 3;
constexpr std::int32_t kArmorDpPpBars = 1;
constexpr std::int32_t kCloseDpPpBars = 4;
// _processEquipSubMenu's has_sfx.
constexpr std::int32_t kChoiceSounds = 1;
constexpr std::int32_t kSilentChoices = 0;
// vs_mainMenu_getItemCount's categories.
constexpr std::int32_t kWeaponItems = 0;
constexpr std::int32_t kShieldItems = 3;
constexpr std::int32_t kArmorItems = 4;
// _loadObjectData's category for a shield.
constexpr std::int32_t kShieldModelCategory = 0;
// decomp: _equipArmorMenu's bodyPartCategories (16C.c:861).
constexpr std::array<std::uint8_t, kLastBodyPart + 1> kBodyPartCategories{
    0, 5, 5, 2, 3, 4, 6, 7,
};

// _dispatchEquipMenu's rows (16C.c:1035-1043).
constexpr std::int32_t kShieldRow = 2;
constexpr std::int32_t kAccessoryRow = 8;
constexpr std::int32_t kArmorRowOffset = 2;

// _equipMenu's rows (16C.c:1054-1231).
constexpr RowPlacement kTitlePlacement{
    .row = 31,
    .x = 320,
    .y = 18,
    .w = 126,
    .background_width = 8,
};
// VS_menuText_OFFSET_equip: "Equip".
constexpr std::uint16_t kTitleText = 53;
constexpr std::int16_t kTitleTarget = 180;
constexpr RowPlacement kHintPlacement{
    .row = 31,
    .x = 180,
    .y = 18,
    .w = 140,
    .background_width = 8,
};
// VS_menuText_INDEX_equipWeapon: the first row's hint.
constexpr std::size_t kHintTextIndex = 12;
// decomp: _equipMenu's equipMenuRowTypes (src/MENU/MENU3.PRG/16C.c:1053).
constexpr std::array<std::int32_t, kEquipmentTopRows> kTopRowTypes{
    0x78, 0x79, 0x69, 0x6A, 0x68, 0x6D, 0x6B, 0x7A,
};
constexpr int kTopRowTypeShift = 9;
// menuRowInfo(4, 3, 401).
constexpr std::int32_t kTopRowInfo = 0x19134;
constexpr std::int32_t kWeaponTopRow = 0;
constexpr std::int32_t kShieldTopRow = 1;
constexpr std::int32_t kFirstArmorTopRow = 2;
constexpr std::int32_t kAccessoryTopRow = 7;
// _startMenuTransition's row for a top-menu selection.
constexpr std::int32_t kTransitionRowOffset = 9;

// What sets one slot menu apart from the others.
struct SlotMenuRules {
  EquipmentMenu menu;
  EquipmentTable table;
  std::int32_t item_category;
  std::int32_t slots;
  std::int32_t sounds;
  // The DP/PP bars' opening flags; the accessory menu shows no bars.
  std::optional<std::int32_t> dp_pp_bars;
};
constexpr SlotMenuRules kAccessoryRules{
    .menu = EquipmentMenu::kAccessory,
    .table = EquipmentTable::kArmor,
    .item_category = kArmorItems,
    .slots = kEquipmentArmorSlots,
    .sounds = kChoiceSounds,
    .dp_pp_bars = std::nullopt,
};
constexpr SlotMenuRules kWeaponRules{
    .menu = EquipmentMenu::kWeapon,
    .table = EquipmentTable::kWeapons,
    .item_category = kWeaponItems,
    .slots = kEquipmentWeaponSlots,
    .sounds = kSilentChoices,
    .dp_pp_bars = kWeaponDpPpBars,
};
constexpr SlotMenuRules kShieldRules{
    .menu = EquipmentMenu::kShield,
    .table = EquipmentTable::kShields,
    .item_category = kShieldItems,
    .slots = kEquipmentShieldSlots,
    .sounds = kSilentChoices,
    .dp_pp_bars = kWeaponDpPpBars,
};
constexpr SlotMenuRules kArmorRules{
    .menu = EquipmentMenu::kArmor,
    .table = EquipmentTable::kArmor,
    .item_category = kArmorItems,
    .slots = kEquipmentArmorSlots,
    .sounds = kChoiceSounds,
    .dp_pp_bars = kArmorDpPpBars,
};

// The list's outcome: `selected` is the chosen row + 1 (0 while the list is
// open) and `item` the chosen item's 1-based slot once a row past "none" is
// chosen.
struct Choice {
  std::int32_t selected = 0;
  std::uint8_t item = 0;
};

[[nodiscard]] constexpr bool IsBodyPart(std::int32_t part) noexcept {
  return part >= kFirstBodyPart && part <= kLastBodyPart;
}

[[nodiscard]] constexpr bool IsWeaponCategory(std::uint8_t category) noexcept {
  return category != 0 && category <= kEquipmentWeaponCategories;
}

// The 1-based slot of the first listed item `is_equipped` accepts, 0 when
// none does; nullopt for a listed slot outside the table.
template <typename Equipped>
[[nodiscard]] std::optional<std::uint8_t> FindListedEquipped(
    EquipmentMenusHost& host, const SlotMenuRules& rules,
    Equipped is_equipped) {
  for (std::int32_t position = 0; position < rules.slots; ++position) {
    const std::uint8_t slot = host.ListedSlot(rules.table, position);
    if (slot > rules.slots) {
      return std::nullopt;
    }
    if (slot != 0 && is_equipped(slot - 1)) {
      return slot;
    }
  }
  return std::uint8_t{0};
}

// The panels a slot menu opens with its stat preview.
void OpenSlotMenu(EquipmentMenusHost& host, const SlotMenuRules& rules,
                  std::uint8_t item) {
  host.PreviewItem(rules.menu, item);
  host.UpdateStatPages(kShowStatPages);
  if (rules.dp_pp_bars) {
    host.UpdateDpPpBars(*rules.dp_pp_bars);
  }
  host.UpdatePrimaryStatPanel(kOpenPrimaryPanel);
}

// A slot menu's opening: previews the first listed item `is_equipped`
// accepts and opens the panels. Returns that item's 1-based slot (0 for
// none); nullopt, before opening anything, for a listed slot outside the
// table.
template <typename Equipped>
[[nodiscard]] std::optional<std::uint8_t> StartSlotMenu(
    EquipmentMenusHost& host, const SlotMenuRules& rules, Equipped is_equipped,
    std::uint8_t& step) {
  const auto equipped = FindListedEquipped(host, rules, is_equipped);
  if (!equipped) {
    return std::nullopt;
  }
  OpenSlotMenu(host, rules, *equipped);
  step = static_cast<std::uint8_t>(EquipmentSlotStep::kInit);
  return equipped;
}

// The rows and panels a slot menu closes once a row is chosen.
void CloseSlotMenu(EquipmentMenusHost& host, const SlotMenuRules& rules) {
  host.ClearMenuExcept(Framework::kNoRow);
  host.FlyOutRowLeft(kEveryRow);
  host.UpdateStatPages(kHideStatPages);
  if (rules.dp_pp_bars) {
    host.UpdateDpPpBars(kCloseDpPpBars);
  }
  host.UpdatePrimaryStatPanel(kClosePrimaryPanel);
}

// _menuText's "none" name and description; nullopt outside the table.
struct NoneText {
  std::uint16_t name = 0;
  std::uint16_t description = 0;
};
[[nodiscard]] std::optional<NoneText> ReadNoneText(EquipmentMenusHost& host) {
  const NoneText text{.name = host.MenuTextEntry(kNoneNameIndex),
                      .description = host.MenuTextEntry(kNoneDescriptionIndex)};
  if (text.name >= kEquipmentMenuTextEntries ||
      text.description >= kEquipmentMenuTextEntries) {
    return std::nullopt;
  }
  return text;
}

// The choice list (16C.c:385-436 and the other slot menus' copies): row 0
// shows the equipped item (or "none"), each later row a listed item
// `is_listable` accepts. Returns false, before formatting any row, for a
// count or listed slot outside the table or a "none" text outside _menuText.
template <typename Equipped, typename Listable>
[[nodiscard]] bool BuildChoiceRows(EquipmentMenusHost& host,
                                   const SlotMenuRules& rules,
                                   Equipped is_equipped, Listable is_listable) {
  const std::int32_t count = host.CountItems(rules.item_category);
  if (count < 0 || count > rules.slots) {
    return false;
  }
  std::array<std::uint8_t, kEquipmentChoiceRows - 1> listed{};
  for (std::int32_t position = 0; position < count; ++position) {
    const std::uint8_t slot = host.ListedSlot(rules.table, position);
    if (slot > rules.slots) {
      return false;
    }
    listed.at(static_cast<std::size_t>(position)) = slot;
  }
  const auto items = std::span(listed).first(static_cast<std::size_t>(count));

  // The original scans the whole table here, not the listed slots.
  bool has_equipped = false;
  for (std::int32_t slot = 0; slot < rules.slots; ++slot) {
    if (is_equipped(slot)) {
      has_equipped = true;
    }
  }
  std::array<std::int32_t, kEquipmentChoiceRows> flags{};
  bool listed_equipped = false;
  if (has_equipped) {
    // The last listed match wins, as each format overwrites row 0.
    for (const std::uint8_t slot : items) {
      if (slot != 0 && is_equipped(slot - 1)) {
        flags[0] = host.FormatRow(rules.table, slot - 1, 0) |
                   (static_cast<std::int32_t>(slot) << kChoiceSlotShift);
        listed_equipped = true;
      }
    }
  }
  // An equipped item missing from the list leaves the original's row 0 on
  // uninitialized stack; it shows "none" here.
  if (!listed_equipped) {
    const auto none = ReadNoneText(host);
    if (!none) {
      return false;
    }
    host.SetRowText(0, none->name, none->description);
    flags[0] = kNoneRowFlags;
  }

  std::size_t rows = 1;
  for (const std::uint8_t slot : items) {
    if (slot != 0 && is_listable(slot - 1)) {
      flags.at(rows) = host.FormatRow(rules.table, slot - 1,
                                      static_cast<std::int32_t>(rows)) |
                       (static_cast<std::int32_t>(slot) << kChoiceSlotShift);
      ++rows;
    }
  }
  host.CreateChoiceRows(std::span<const std::int32_t>(flags).first(rows));
  return true;
}

// A slot menu's first frame: waits for the rows to settle, then lists the
// choices. nullopt as BuildChoiceRows refuses.
template <typename Equipped, typename Listable>
[[nodiscard]] std::optional<std::int32_t> ListChoicesOnceIdle(
    EquipmentMenusHost& host, const SlotMenuRules& rules, Equipped is_equipped,
    Listable is_listable, std::uint8_t& step) {
  if (!host.RowsIdle()) {
    return 0;
  }
  if (!BuildChoiceRows(host, rules, is_equipped, is_listable)) {
    return std::nullopt;
  }
  step = static_cast<std::uint8_t>(EquipmentSlotStep::kHandleInput);
  return 0;
}

// The list's frame (16C.c:439-443): the highlighted item's preview, the
// list's input and the chosen row. nullopt for a highlighted item outside
// the table or a chosen row past the list.
[[nodiscard]] std::optional<Choice> ReadChoice(EquipmentMenusHost& host,
                                               const SlotMenuRules& rules) {
  const std::uint8_t previewed = host.HighlightedItem();
  if (previewed > rules.slots) {
    return std::nullopt;
  }
  host.PreviewItem(rules.menu, previewed);
  host.UpdateChoices(rules.sounds);
  const std::int32_t row = host.SelectedChoice();
  if (row >= static_cast<std::int32_t>(kEquipmentChoiceRows)) {
    return std::nullopt;
  }
  Choice choice{.selected = row + 1};
  if (choice.selected > kEquipmentMenuUnequip) {
    // The list's input moved the highlight to the chosen row.
    choice.item = host.HighlightedItem();
    if (choice.item == 0 || choice.item > rules.slots) {
      return std::nullopt;
    }
  }
  return choice;
}

// The waits after a choice (16C.c:487-497): 0 until the rows settle, then
// the choice.
[[nodiscard]] std::int32_t WaitMenuReady(EquipmentMenusHost& host,
                                         std::int32_t selected) {
  return host.RowsIdle() ? selected : 0;
}

// The blade model of a weapon; nullopt for a blade or category outside the
// tables (vs_mainMenu_weaponHands has no category 0).
[[nodiscard]] std::optional<BladeModel> WeaponModel(EquipmentMenusHost& host,
                                                    std::int32_t weapon) {
  const std::uint8_t blade = host.WeaponBlade(weapon);
  if (blade == 0 || blade > kEquipmentBladeSlots) {
    return std::nullopt;
  }
  const BladeModel model = host.Blade(blade - 1);
  if (!IsWeaponCategory(model.category)) {
    return std::nullopt;
  }
  return model;
}
}  // namespace

std::optional<std::int32_t> UpdateAccessoryMenu(AccessoryMenuState& state,
                                                EquipmentMenusHost& host,
                                                std::int32_t initialize) {
  const auto is_worn = [&host](std::int32_t slot) {
    return host.ArmorLimb(slot) == kAccessoryLimb;
  };
  if (initialize != 0) {
    const auto equipped =
        StartSlotMenu(host, kAccessoryRules, is_worn, state.step);
    if (!equipped) {
      return std::nullopt;
    }
    state.equipped_accessory = *equipped;
    return 0;
  }

  switch (static_cast<EquipmentSlotStep>(state.step)) {
    case EquipmentSlotStep::kInit: {
      const auto is_spare = [&host](std::int32_t slot) {
        return host.ArmorCategory(slot) == kAccessoryLimb &&
               host.ArmorLimb(slot) == 0;
      };
      return ListChoicesOnceIdle(host, kAccessoryRules, is_worn, is_spare,
                                 state.step);
    }
    case EquipmentSlotStep::kHandleInput: {
      const auto choice = ReadChoice(host, kAccessoryRules);
      if (!choice) {
        return std::nullopt;
      }
      if (choice->selected > 0 &&
          state.equipped_accessory > kEquipmentArmorSlots) {
        return std::nullopt;
      }
      state.selected = choice->selected;
      if (state.selected == 0) {
        return 0;
      }
      CloseSlotMenu(host, kAccessoryRules);
      if (state.selected < 0) {
        state.step =
            static_cast<std::uint8_t>(EquipmentSlotStep::kWaitMenuReady);
        return 0;
      }
      if (state.equipped_accessory != 0) {
        host.SetArmorLimb(state.equipped_accessory - 1, 0);
      }
      if (state.selected != kEquipmentMenuUnequip) {
        host.SetArmorLimb(choice->item - 1, kAccessoryLimb);
        host.EquipAccessory(choice->item - 1);
      } else {
        host.EquipAccessory(std::nullopt);
      }
      state.step = static_cast<std::uint8_t>(EquipmentSlotStep::kLoadObject);
      return 0;
    }
    case EquipmentSlotStep::kLoadObject:
      // An accessory has no preview model to reload.
      state.step = static_cast<std::uint8_t>(EquipmentSlotStep::kWaitMenuReady);
      [[fallthrough]];
    case EquipmentSlotStep::kWaitMenuReady:
      return WaitMenuReady(host, state.selected);
  }
  return 0;
}

std::optional<std::int32_t> UpdateWeaponMenu(WeaponMenuState& state,
                                             EquipmentMenusHost& host,
                                             std::int32_t initialize) {
  const auto is_held = [&host](std::int32_t slot) {
    return host.IsEquipped(EquipmentTable::kWeapons, slot);
  };
  if (initialize != 0) {
    if (!StartSlotMenu(host, kWeaponRules, is_held, state.step)) {
      return std::nullopt;
    }
    return 0;
  }

  switch (static_cast<EquipmentSlotStep>(state.step)) {
    case EquipmentSlotStep::kInit: {
      const auto is_spare = [&is_held](std::int32_t slot) {
        return !is_held(slot);
      };
      return ListChoicesOnceIdle(host, kWeaponRules, is_held, is_spare,
                                 state.step);
    }
    case EquipmentSlotStep::kHandleInput: {
      const auto choice = ReadChoice(host, kWeaponRules);
      if (!choice) {
        return std::nullopt;
      }
      std::optional<BladeModel> model;
      if (choice->item != 0) {
        model = WeaponModel(host, choice->item - 1);
        if (!model) {
          return std::nullopt;
        }
      }
      state.selected = choice->selected;
      if (state.selected == 0) {
        return 0;
      }
      CloseSlotMenu(host, kWeaponRules);
      if (state.selected < 0) {
        state.step =
            static_cast<std::uint8_t>(EquipmentSlotStep::kWaitMenuReady);
        return 0;
      }
      host.UnequipWeapon();
      if (!model) {
        state.category = kUnarmedCategory;
        state.model_id = 0;
        state.material = 0;
      } else {
        const std::int32_t weapon = choice->item - 1;
        host.MarkEquipped(EquipmentTable::kWeapons, weapon);
        state.category = model->category;
        state.model_id = model->model_id;
        state.material = model->material;
        // A two-handed weapon takes the shield's hand.
        if (host.IsTwoHanded(state.category)) {
          host.UnequipShield();
        }
        host.EquipWeapon(weapon);
      }
      state.step = static_cast<std::uint8_t>(EquipmentSlotStep::kLoadObject);
      return 0;
    }
    case EquipmentSlotStep::kLoadObject:
      if (host.ReloadModels(state.category, state.model_id, state.material)) {
        return 0;
      }
      state.step = static_cast<std::uint8_t>(EquipmentSlotStep::kWaitMenuReady);
      [[fallthrough]];
    case EquipmentSlotStep::kWaitMenuReady:
      return WaitMenuReady(host, state.selected);
  }
  return 0;
}

std::optional<std::int32_t> UpdateShieldMenu(ShieldMenuState& state,
                                             EquipmentMenusHost& host,
                                             std::int32_t initialize) {
  const auto is_held = [&host](std::int32_t slot) {
    return host.IsEquipped(EquipmentTable::kShields, slot);
  };
  if (initialize != 0) {
    if (!StartSlotMenu(host, kShieldRules, is_held, state.step)) {
      return std::nullopt;
    }
    return 0;
  }

  switch (static_cast<EquipmentSlotStep>(state.step)) {
    case EquipmentSlotStep::kInit: {
      const auto is_spare = [&is_held](std::int32_t slot) {
        return !is_held(slot);
      };
      return ListChoicesOnceIdle(host, kShieldRules, is_held, is_spare,
                                 state.step);
    }
    case EquipmentSlotStep::kHandleInput: {
      const auto choice = ReadChoice(host, kShieldRules);
      if (!choice) {
        return std::nullopt;
      }
      std::optional<HeldBlade> held;
      if (choice->item != 0) {
        held = host.HeldWeapon();
        if (!held || (held->id != 0 && !IsWeaponCategory(held->category))) {
          return std::nullopt;
        }
      }
      state.selected = choice->selected;
      if (state.selected == 0) {
        return 0;
      }
      CloseSlotMenu(host, kShieldRules);
      if (state.selected < 0) {
        state.step =
            static_cast<std::uint8_t>(EquipmentSlotStep::kWaitMenuReady);
        return 0;
      }
      host.UnequipShield();
      if (!held) {
        state.model_id = 0;
        state.material = 0;
      } else {
        const std::int32_t shield = choice->item - 1;
        host.MarkEquipped(EquipmentTable::kShields, shield);
        const ShieldModel model = host.Shield(shield);
        state.model_id = model.model_id;
        state.material = model.material;
        // A shield frees the hand a two-handed weapon holds.
        if (held->id != 0 && host.IsTwoHanded(held->category)) {
          host.UnequipWeapon();
        }
        host.EquipShield(shield);
      }
      state.step = static_cast<std::uint8_t>(EquipmentSlotStep::kLoadObject);
      return 0;
    }
    case EquipmentSlotStep::kLoadObject:
      if (host.ReloadModels(kShieldModelCategory, state.model_id,
                            state.material)) {
        return 0;
      }
      state.step = static_cast<std::uint8_t>(EquipmentSlotStep::kWaitMenuReady);
      [[fallthrough]];
    case EquipmentSlotStep::kWaitMenuReady:
      return WaitMenuReady(host, state.selected);
  }
  return 0;
}

std::optional<std::int32_t> UpdateArmorMenu(ArmorMenuState& state,
                                            EquipmentMenusHost& host,
                                            std::int32_t body_part) {
  if (body_part != 0) {
    if (!IsBodyPart(body_part)) {
      return std::nullopt;
    }
    const auto part = static_cast<std::uint8_t>(body_part);
    const auto equipped = StartSlotMenu(
        host, kArmorRules,
        [&host, part](std::int32_t slot) {
          return host.ArmorLimb(slot) == part;
        },
        state.step);
    if (!equipped) {
      return std::nullopt;
    }
    state.body_part = part;
    state.armor_index = *equipped;
    return 0;
  }

  const std::uint8_t part = state.body_part;
  switch (static_cast<EquipmentSlotStep>(state.step)) {
    case EquipmentSlotStep::kInit: {
      if (!IsBodyPart(part)) {
        return std::nullopt;
      }
      const std::uint8_t category = kBodyPartCategories.at(part);
      const auto is_worn = [&host, part](std::int32_t slot) {
        return host.ArmorLimb(slot) == part;
      };
      const auto is_spare = [&host, category](std::int32_t slot) {
        return host.ArmorCategory(slot) == category &&
               host.ArmorLimb(slot) == 0;
      };
      return ListChoicesOnceIdle(host, kArmorRules, is_worn, is_spare,
                                 state.step);
    }
    case EquipmentSlotStep::kHandleInput: {
      const auto choice = ReadChoice(host, kArmorRules);
      if (!choice) {
        return std::nullopt;
      }
      if (choice->selected > 0 &&
          (!IsBodyPart(part) || state.armor_index > kEquipmentArmorSlots)) {
        return std::nullopt;
      }
      state.selected = choice->selected;
      if (state.selected == 0) {
        return 0;
      }
      CloseSlotMenu(host, kArmorRules);
      if (state.selected < 0) {
        state.step =
            static_cast<std::uint8_t>(EquipmentSlotStep::kWaitMenuReady);
        return 0;
      }
      if (state.armor_index != 0) {
        host.SetArmorLimb(state.armor_index - 1, 0);
      }
      // vs_battle_equipArmor takes the 0-based part.
      if (state.selected == kEquipmentMenuUnequip) {
        host.EquipArmor(part - 1, std::nullopt);
      } else {
        host.SetArmorLimb(choice->item - 1, part);
        host.EquipArmor(part - 1, choice->item - 1);
      }
      state.step = static_cast<std::uint8_t>(EquipmentSlotStep::kLoadObject);
      return 0;
    }
    case EquipmentSlotStep::kLoadObject:
      // Armor has no preview model to reload.
      state.step = static_cast<std::uint8_t>(EquipmentSlotStep::kWaitMenuReady);
      [[fallthrough]];
    case EquipmentSlotStep::kWaitMenuReady:
      return WaitMenuReady(host, state.selected);
  }
  return 0;
}

std::optional<std::int32_t> DispatchEquipmentMenu(
    std::uint8_t& current, const EquipmentMenuHandlers& handlers,
    std::int32_t row) {
  if (row < 0 || row > static_cast<std::int32_t>(kEquipmentTopRows)) {
    return std::nullopt;
  }
  std::uint8_t next = current;
  std::int32_t argument = row;
  if (row == kAccessoryRow) {
    next = static_cast<std::uint8_t>(EquipmentMenu::kAccessory);
  } else if (row > kShieldRow) {
    // Rows 3-7 open the armor menu on body parts 1-5.
    argument = row - kArmorRowOffset;
    next = static_cast<std::uint8_t>(EquipmentMenu::kArmor);
  } else if (row != 0) {
    next = static_cast<std::uint8_t>(row);
  }
  if (next >= handlers.size() || handlers.at(next) == nullptr) {
    return std::nullopt;
  }
  current = next;
  return handlers.at(next)(argument);
}

namespace {
// _equipMenu's initMenu step (16C.c:1101-1146): every row starts as "none",
// then shows the first equipped weapon and shield in table order (not list
// order) and the armor worn on limbs 1-5 and the accessory. Returns false,
// before any row changes, for a "none" text outside _menuText.
[[nodiscard]] bool BuildTopRows(EquipmentMenusHost& host) {
  const auto none = ReadNoneText(host);
  if (!none) {
    return false;
  }
  host.SetCategoryIconOnTop(1);
  std::array<std::int32_t, kEquipmentTopRows> flags{};
  for (std::size_t row = 0; row < kEquipmentTopRows; ++row) {
    host.SetRowText(static_cast<std::int32_t>(row), none->name,
                    none->description);
  }
  for (std::int32_t slot = 0; slot < kEquipmentWeaponSlots; ++slot) {
    if (host.IsEquipped(EquipmentTable::kWeapons, slot)) {
      flags[kWeaponTopRow] =
          host.FormatRow(EquipmentTable::kWeapons, slot, kWeaponTopRow);
      break;
    }
  }
  for (std::int32_t slot = 0; slot < kEquipmentShieldSlots; ++slot) {
    if (host.IsEquipped(EquipmentTable::kShields, slot)) {
      flags[kShieldTopRow] =
          host.FormatRow(EquipmentTable::kShields, slot, kShieldTopRow);
      break;
    }
  }
  // Rows 2-6 match limb row - 1, row 7 the accessory's limb 7.
  for (std::int32_t row = kFirstArmorTopRow;
       row < static_cast<std::int32_t>(kEquipmentTopRows); ++row) {
    const std::int32_t limb =
        row == kAccessoryTopRow ? kAccessoryLimb : row - 1;
    for (std::int32_t slot = 0; slot < kEquipmentArmorSlots; ++slot) {
      if (host.ArmorLimb(slot) == limb) {
        flags.at(static_cast<std::size_t>(row)) =
            host.FormatRow(EquipmentTable::kArmor, slot, row);
        break;
      }
    }
  }
  for (std::size_t row = 0; row < kEquipmentTopRows; ++row) {
    flags.at(row) |= kTopRowTypes.at(row) << kTopRowTypeShift;
  }
  host.CreateTopRows(flags, kTopRowInfo);
  return true;
}

// _equipMenu's topMenu step (16C.c:1149-1170). Returns false, before any
// change, for a selected row past the menu.
[[nodiscard]] bool ChooseTopRow(EquipmentMenuState& state,
                                EquipmentMenusHost& host) {
  if (host.SquarePressed()) {
    host.PlayInvalidSound();
  }
  const std::int32_t row = host.SelectedRow();
  if (row >= static_cast<std::int32_t>(kEquipmentTopRows)) {
    return false;
  }
  state.selected_row = row + 1;
  if (state.selected_row == 0) {
    return true;
  }
  if (state.selected_row < 0) {
    // Back (-1) and quit (-2) light the header's icons 1 and 2.
    host.SetCategoryIconOnTop(-state.selected_row);
    host.ClearMenuExcept(Framework::kNoRow);
    state.step = static_cast<std::uint8_t>(EquipmentMenuStep::kQuit);
  } else {
    host.StartTransition(state.selected_row + kTransitionRowOffset);
    state.step = static_cast<std::uint8_t>(EquipmentMenuStep::kInitSubMenu);
  }
  return true;
}

// _equipMenu's equipSubMenu step (16C.c:1196-1216): quit leaves the menu,
// any other result reopens the top menu.
void UpdateSubMenu(EquipmentMenuState& state, EquipmentMenusHost& host) {
  state.selected_row = host.Dispatch(0);
  if (state.selected_row == 0) {
    return;
  }
  if (state.selected_row == kEquipmentMenuQuit) {
    state.step = static_cast<std::uint8_t>(EquipmentMenuStep::kQuit);
    return;
  }
  host.SetPreviewState(0);
  host.SetCategoryIconsEnabled(1);
  host.SetCategoryIconOnTop(2);
  host.SetStatusCommandState(1);
  state.step = static_cast<std::uint8_t>(EquipmentMenuStep::kInit);
}

// The highlighted row's hint in the title row (16C.c:1220-1226). Returns
// false, before the row changes, for a row past the menu or a hint outside
// _menuText.
[[nodiscard]] bool ShowTopRowHint(EquipmentMenusHost& host) {
  const std::int32_t row = host.HighlightedRow();
  if (row < 0 || row >= static_cast<std::int32_t>(kEquipmentTopRows)) {
    return false;
  }
  const std::uint16_t hint =
      host.MenuTextEntry(kHintTextIndex + static_cast<std::size_t>(row));
  if (hint >= kEquipmentMenuTextEntries) {
    return false;
  }
  host.SetMenuItem(kHintPlacement, hint).selected = 1;
  return true;
}
}  // namespace

std::optional<std::int32_t> UpdateEquipmentMenu(EquipmentMenuState& state,
                                                EquipmentMenusHost& host,
                                                std::int32_t initialize) {
  if (initialize != 0) {
    host.ClearMenuExcept(Framework::kNoRow);
    host.SetCategoryIconOnTop(1);
    state.step = static_cast<std::uint8_t>(EquipmentMenuStep::kInit);
    return 0;
  }

  bool accepted = true;
  switch (static_cast<EquipmentMenuStep>(state.step)) {
    case EquipmentMenuStep::kInit: {
      if (!host.RowsIdle()) {
        break;
      }
      MenuRow& title = host.SetMenuItem(kTitlePlacement, kTitleText);
      title.state = RowState::kSlideX;
      title.target0 = kTitleTarget;
      title.selected = 1;
      state.step = static_cast<std::uint8_t>(EquipmentMenuStep::kInitMenu);
      [[fallthrough]];
    }
    case EquipmentMenuStep::kInitMenu:
      if (!host.RowsIdle()) {
        break;
      }
      if (!BuildTopRows(host)) {
        return std::nullopt;
      }
      state.step = static_cast<std::uint8_t>(EquipmentMenuStep::kTopMenu);
      break;
    case EquipmentMenuStep::kTopMenu:
      accepted = ChooseTopRow(state, host);
      break;
    case EquipmentMenuStep::kQuit:
      // Quit closes at once; back waits for the rows to settle.
      if (state.selected_row == kEquipmentMenuQuit || host.RowsIdle()) {
        return state.selected_row;
      }
      break;
    case EquipmentMenuStep::kInitSubMenu:
      if (!host.RowsIdle()) {
        break;
      }
      host.SetPreviewState(1);
      host.ContinueTransition(state.selected_row + kTransitionRowOffset);
      host.SetCategoryIconsEnabled(0);
      host.SetStatusCommandState(0);
      (void)host.Dispatch(state.selected_row);
      state.step = static_cast<std::uint8_t>(EquipmentMenuStep::kSubMenu);
      break;
    case EquipmentMenuStep::kSubMenu:
      UpdateSubMenu(state, host);
      break;
  }
  if (!accepted) {
    return std::nullopt;
  }
  if (state.step == static_cast<std::uint8_t>(EquipmentMenuStep::kTopMenu) &&
      !ShowTopRowHint(host)) {
    return std::nullopt;
  }
  return 0;
}
}  // namespace VS::Menu::Inventory
