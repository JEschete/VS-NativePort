// Hand-maintained (tools/port/owned.csv).
#include "Menu/Framework/ItemRows.hpp"  // NOLINT(build/include): typed API

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Items/EquipmentStats.hpp"
#include "Menu/Framework/Framework.hpp"

namespace VS::Menu::Framework {
namespace {
using Bytes = std::span<const std::uint8_t>;

// decomp: local statics of vs_mainMenu_getEquipmentMaxStats (C48.c:204,
// C48.c:213); the max table's fourth entry (8192) is never read.
constexpr std::array<int, 11> kSubmaxThresholds{
    10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 32767,
};
constexpr std::array<int, 3> kMaxThresholds{50, 95, 32767};
constexpr int kRankShift = 8;
constexpr int kKeyShift = 16;
constexpr int kAffinityKeyBase = 6;
constexpr std::uint32_t kRankMask = 0xFF;
// The ranks word's bits 15 and up: twice the key (the max rank stays below
// bit 15).
constexpr int kPrestigeKeyShift = 15;

constexpr std::size_t kHalfwordBytes = 2;

// The template string arguments the part rows set: the grip's category, the
// blade's hand text and damage type, the category and the material's name.
constexpr std::size_t kGripCategoryString = 5;
constexpr std::size_t kHandString = 6;
constexpr std::size_t kDamageTypeString = 7;
constexpr std::size_t kCategoryString = 8;
constexpr std::size_t kMaterialString = 9;
constexpr int kHalfwordHighShift = 8;

// Row types: the icon in bits 26-31, the material in bits 16-18 and a misc
// item's count from bit 9.
constexpr int kRowIconShift = 26;
constexpr int kRowMaterialShift = 16;
constexpr std::uint32_t kRowMaterialMask = 7;
constexpr int kRowCountShift = 9;
constexpr std::uint32_t kGemRowType = 0x58000000;
constexpr std::uint32_t kShieldRowType = 0x3C000000;
constexpr std::uint32_t kGripIconBase = 10;
constexpr std::uint32_t kArmorIconBase = 14;
constexpr std::uint8_t kShieldGemIcon = 0x16;

// ITEMHELP's first gem, misc item and accessory descriptions belong to these
// item ids; grips' descriptions start six entries before their ids.
constexpr int kFirstGemId = 261;
constexpr int kFirstMiscId = 323;
constexpr int kFirstAccessoryId = 223;
constexpr int kGripDescriptionBias = 6;
constexpr std::uint8_t kAccessoryCategory = 7;

// The detail screens' component rows: rows 11 and up, sixteen pixels apart,
// right-aligned to the screen edge.
constexpr int kComponentRowBase = 10;
constexpr int kComponentRowPitch = 16;
constexpr int kComponentRowTop = 18;
constexpr int kScreenWidth = 320;
constexpr int kPartRowWidth = 158;
constexpr int kGemRowWidth = 151;
constexpr int kBladeRow = 1;
constexpr int kGripRow = 2;
constexpr int kFirstWeaponGemRow = 3;
constexpr int kFirstShieldGemRow = 1;

// Formats one row: resolves the ITEMHELP and ITEMNAME texts the row and its
// template strings use, then expands the templates into the buffer. The
// first refusal stops the row and empties its texts.
class RowFormatter {
 public:
  RowFormatter(ItemTextHost& host, std::span<std::uint8_t> buffer)
      : host_(host),
        help_(host.HelpText()),
        names_(host.NameText()),
        arguments_(host.TextArguments()),
        buffer_(buffer) {}

  // The ITEMHELP text the offset table's entry `index` names.
  TextRef HelpEntry(int index) {
    const std::optional<TextRef> text = ItemHelpEntry(help_, index);
    return text ? *text : Fail("item text: ITEMHELP entry past the text");
  }

  TextRef HelpAt(std::size_t halfword) {
    const std::optional<TextRef> text = ItemHelpText(help_, halfword);
    return text ? *text : Fail("item text: ITEMHELP offset past the text");
  }

  // ITEMNAME row `row`.
  TextRef Name(int row) {
    if (row < 0 ||
        static_cast<std::size_t>(row) * kItemNameRowBytes >= names_.size()) {
      return Fail("item text: ITEMNAME row past the names");
    }
    return TextRef{.source = TextSource::kNames,
                   .offset = static_cast<std::size_t>(row) * kItemNameRowBytes};
  }

  // vs_mainMenu_weaponHands' ITEMHELP hand text for the blade category.
  TextRef HandText(int category) {
    const auto hands = host_.WeaponHands();
    const int index = category - 1;
    if (index < 0 || static_cast<std::size_t>(index) >= hands.size()) {
      return Fail("item text: blade category past the hand table");
    }
    return HelpEntry(hands[static_cast<std::size_t>(index)] +
                     item_text::kOneHanded);
  }

  void SetString(std::size_t slot, TextRef text) {
    if (!ok_) {
      return;
    }
    const Bytes view = View(text);
    arguments_.strings.at(slot) = view;
    host_.SetStringArgument(slot, view);
  }

  // vs_battle_printf(vs_battle_printf(buffer, first), second).
  void Format(TextRef first, TextRef second) {
    if (!ok_) {
      return;
    }
    const auto end =
        Render::FormatEncodedText(buffer_, View(first), arguments_);
    if (!end) {
      Fail("item text: the row template does not fit the buffer");
      return;
    }
    if (!Render::FormatEncodedText(buffer_.subspan(*end), View(second),
                                   arguments_)) {
      Fail("item text: the description template does not fit the buffer");
    }
  }

  // Copies the 96-byte ITEMHELP description `text` into the buffer.
  void Copy(TextRef text) {
    if (!ok_) {
      return;
    }
    const Bytes source = View(text);
    if (source.size() < kRowDescriptionBufferBytes ||
        buffer_.size() < kRowDescriptionBufferBytes) {
      Fail("item text: a description copy past the text or the buffer");
      return;
    }
    std::ranges::copy(source.first(kRowDescriptionBufferBytes),
                      buffer_.begin());
  }

  // The row, its texts emptied when anything was refused.
  ItemRowText Finish(ItemRowText row) {
    if (ok_) {
      return row;
    }
    host_.Refuse(what_);
    if (!buffer_.empty()) {
      buffer_.front() = Render::kTextTerminator;
    }
    const TextRef empty{.source = TextSource::kEmpty};
    if (row.name) {
      row.name = empty;
    }
    if (row.description) {
      row.description = empty;
    }
    return row;
  }

 private:
  [[nodiscard]] Bytes View(const TextRef& text) const {
    switch (text.source) {
      case TextSource::kHelp:
        return help_.subspan(text.offset);
      case TextSource::kNames:
        return names_.subspan(text.offset);
      case TextSource::kBuffer:
        return buffer_;
      default:
        return {};
    }
  }

  TextRef Fail(std::string_view what) {
    if (ok_) {
      ok_ = false;
      what_ = what;
    }
    return {.source = TextSource::kEmpty};
  }

  ItemTextHost& host_;
  Bytes help_;
  Bytes names_;
  Render::EncodedTextArguments arguments_;
  std::span<std::uint8_t> buffer_;
  bool ok_ = true;
  std::string_view what_;
};

constexpr TextRef kBufferText{.source = TextSource::kBuffer};
}  // namespace

std::optional<TextRef> ItemHelpText(std::span<const std::uint8_t> help,
                                    std::size_t halfword) {
  const std::size_t offset = halfword * kHalfwordBytes;
  if (offset >= help.size()) {
    return std::nullopt;
  }
  return TextRef{.source = TextSource::kHelp, .offset = offset};
}

std::optional<TextRef> ItemHelpEntry(std::span<const std::uint8_t> help,
                                     int index) {
  if (index < 0) {
    return std::nullopt;
  }
  const std::size_t byte = static_cast<std::size_t>(index) * kHalfwordBytes;
  if (byte + kHalfwordBytes > help.size()) {
    return std::nullopt;
  }
  return ItemHelpText(
      help, static_cast<std::size_t>(help[byte] |
                                     (help[byte + 1] << kHalfwordHighShift)));
}

namespace {

std::uint32_t PartRowType(std::uint32_t category, std::uint32_t material) {
  return (category << kRowIconShift) + (material << kRowMaterialShift);
}

// The title word's ITEMHELP strings 0 and 1: the runner-up's rank, then the
// greatest key's prestige title or, below rank 1, the category's base title.
template <typename BaseTitle>
void SetTitleStrings(RowFormatter& text, int ranks, int prestige_entry,
                     const BaseTitle& base_title) {
  const auto ranks_bits = static_cast<std::uint32_t>(ranks);
  text.SetString(0, text.HelpEntry(static_cast<int>(ranks_bits & kRankMask) +
                                   item_text::kWarriorEquipment - 1));
  const int max_rank = static_cast<int>((ranks_bits >> kRankShift) & kRankMask);
  text.SetString(
      1, max_rank != 0
             ? text.HelpEntry(max_rank + (ranks >> kPrestigeKeyShift) - 1 +
                              prestige_entry)
             : base_title());
}

ItemRowText RefusedRow(ItemTextHost& host, std::span<std::uint8_t> buffer,
                       std::string_view what, std::optional<TextRef> name) {
  host.Refuse(what);
  if (!buffer.empty()) {
    buffer.front() = Render::kTextTerminator;
  }
  return {.name = name, .description = TextRef{.source = TextSource::kEmpty}};
}

// The row SetMenuItem places at the right edge; slides in from the edge.
MenuRow* PlaceComponentRow(ItemRowsHost& host, int row, int width,
                           const ItemRowText& text, bool slide_in) {
  const int menu_row = row + kComponentRowBase;
  host.DeactivateRow(menu_row);
  if (width == 0) {
    return nullptr;
  }
  const int x = kScreenWidth - width;
  MenuRow* item =
      host.SetMenuItem({.row = menu_row,
                        .x = x,
                        .y = (row * kComponentRowPitch) + kComponentRowTop,
                        .w = width,
                        .background_width = 0},
                       text.name.value_or(TextRef{}));
  if (item == nullptr) {
    return nullptr;
  }
  item->unselectable = static_cast<std::uint8_t>(
      !text.description || text.description->source == TextSource::kNone);
  if (slide_in) {
    item->x = kScreenWidth;
    item->state = RowState::kSlideX;
    item->target0 = static_cast<std::int16_t>(x);
  }
  return item;
}

// An empty gem slot's row: ITEMHELP "none" and no description.
ItemRowText EmptyGemRow() {
  return {.name = TextRef{.source = TextSource::kHelp,
                          .offset = item_text::kNoneOffset * kHalfwordBytes},
          .description = TextRef{.source = TextSource::kNone},
          .row_type = kGemRowType};
}

// The gem `reference` sets in a component slot, or the empty slot's row.
std::optional<ItemRowText> GemSlotRow(ItemRowsHost& host, int reference) {
  if (reference == 0) {
    return EmptyGemRow();
  }
  const auto gem = host.Gem(reference);
  if (!gem) {
    return std::nullopt;
  }
  return FormatGemRow(host, *gem, host.StringBuffer());
}
}  // namespace

// decomp: MAINMENU.PRG vs_mainMenu_getEquipmentMaxStats
// (src/MENU/MAINMENU.PRG/C48.c:87)
int EquipmentTitleRanks(const Items::ClassAffinityCurrent& totals) {
  int max_value = 0;
  int submax_value = 0;
  int key = 0;
  int is_affinity = 0;
  const auto consider = [&](int value, int index, bool affinity) {
    if (max_value < value) {
      submax_value = max_value;
      max_value = value;
      key = index;
      if (affinity) {
        is_affinity = 1;
      }
    } else if (submax_value < value) {
      submax_value = value;
    }
  };
  for (std::size_t index = 0; index < Items::kClassCount; ++index) {
    consider(totals.classes[1][index], static_cast<int>(index), false);
  }
  for (std::size_t index = 0; index < Items::kAffinityCount; ++index) {
    consider(totals.affinities[1][index], static_cast<int>(index), true);
  }
  const auto rank = [](int value, std::span<const int> thresholds) {
    const auto found = std::ranges::find_if(
        thresholds, [value](int threshold) { return value < threshold; });
    return found == thresholds.end()
               ? 0
               : static_cast<int>(found - thresholds.begin());
  };
  const int submax_rank = rank(submax_value, kSubmaxThresholds);
  const int max_rank = rank(max_value, kMaxThresholds);
  return submax_rank + (max_rank << kRankShift) +
         (((is_affinity * kAffinityKeyBase) + key) << kKeyShift);
}

// decomp: MAINMENU.PRG vs_mainMenu_setWeaponRow
// (src/MENU/MAINMENU.PRG/2D10.c:130)
ItemRowText FormatWeaponRow(ItemTextHost& host,
                            const Items::WeaponLoadout& weapon,
                            std::span<std::uint8_t> buffer) {
  const Items::EquipmentPart& blade = weapon.blade;
  RowFormatter text(host, buffer);
  SetTitleStrings(text, EquipmentTitleRanks(weapon.class_affinity),
                  item_text::kWeaponPrestige, [&] {
                    return text.HelpEntry(blade.category - 1 +
                                          item_text::kWeaponBasePrestige);
                  });
  text.SetString(kMaterialString,
                 text.Name(blade.material - 1 + item_text::kWoodName));
  text.SetString(kCategoryString,
                 text.HelpEntry(blade.category - 1 + item_text::kDagger));
  text.SetString(kDamageTypeString,
                 text.HelpEntry(blade.damage_type - 1 + item_text::kBlunt));
  text.SetString(kHandString, text.HandText(blade.category));
  text.Format(text.HelpAt(item_text::kClassTemplateOffset),
              text.HelpAt(item_text::kWeaponDescTemplateOffset));
  return text.Finish({.name = std::nullopt,
                      .description = kBufferText,
                      .row_type = PartRowType(blade.category, blade.material)});
}

// decomp: MAINMENU.PRG vs_mainMenu_setWeaponRowFromInventory
// (src/MENU/MAINMENU.PRG/2D10.c:169)
ItemRowText FormatInventoryWeaponRow(ItemTextHost& host,
                                     const Items::InventoryWeapon& weapon,
                                     std::span<std::uint8_t> buffer) {
  const TextRef name{.source = TextSource::kWeaponName};
  const auto loadout = host.InventoryWeaponLoadout(weapon);
  if (!loadout) {
    return RefusedRow(host, buffer,
                      "item text: no stats for the weapon's inventory table",
                      name);
  }
  ItemRowText row = FormatWeaponRow(host, *loadout, buffer);
  row.name = name;
  return row;
}

// decomp: MAINMENU.PRG vs_mainMenu_setBladeRow
// (src/MENU/MAINMENU.PRG/2D10.c:185)
ItemRowText FormatBladeRow(ItemTextHost& host,
                           const Items::InventoryBlade& blade,
                           std::span<std::uint8_t> buffer) {
  RowFormatter text(host, buffer);
  text.SetString(kMaterialString,
                 text.Name(blade.material - 1 + item_text::kWoodName));
  text.SetString(kCategoryString,
                 text.HelpEntry(blade.category - 1 + item_text::kDagger));
  text.SetString(kDamageTypeString,
                 text.HelpEntry(blade.DamageType() - 1 + item_text::kBlunt));
  text.SetString(kHandString, text.HandText(blade.category));
  text.Format(text.HelpEntry(blade.id - 1 + item_text::kBattleKnifeDesc),
              text.HelpAt(item_text::kWeaponDescTemplateOffset));
  return text.Finish({.name = text.Name(blade.id + item_text::kItem0Name),
                      .description = kBufferText,
                      .row_type = PartRowType(blade.category, blade.material)});
}

// decomp: MAINMENU.PRG vs_mainMenu_setGripRow
// (src/MENU/MAINMENU.PRG/2D10.c:209)
ItemRowText FormatGripRow(ItemTextHost& host, const Items::InventoryGrip& grip,
                          std::span<std::uint8_t> buffer) {
  RowFormatter text(host, buffer);
  text.SetString(kGripCategoryString,
                 text.HelpEntry(grip.category - 1 + item_text::kShortGrip));
  text.Format(text.HelpEntry(grip.id - kGripDescriptionBias),
              text.HelpAt(item_text::kGripTemplateOffset));
  return text.Finish(
      {.name = text.Name(grip.id + item_text::kItem0Name),
       .description = kBufferText,
       .row_type = (grip.category + kGripIconBase) << kRowIconShift});
}

// decomp: MAINMENU.PRG vs_mainMenu_setShieldRow
// (src/MENU/MAINMENU.PRG/2D10.c:226)
ItemRowText FormatShieldRow(ItemTextHost& host,
                            const Items::ShieldLoadout& shield,
                            std::span<std::uint8_t> buffer) {
  const Items::EquipmentPart& base = shield.base;
  RowFormatter text(host, buffer);
  SetTitleStrings(text, EquipmentTitleRanks(shield.class_affinity),
                  item_text::kArmorPrestige, [&] {
                    return text.HelpAt(item_text::kArmorBasePrestigeOffset);
                  });
  text.SetString(kCategoryString, text.HelpAt(item_text::kArmorCategoryOffset));
  text.SetString(kMaterialString,
                 text.Name(base.material - 1 + item_text::kWoodName));
  text.Format(text.HelpAt(item_text::kClassTemplateOffset),
              text.HelpAt(item_text::kArmorTemplateOffset));
  return text.Finish({.name = text.Name(base.id + item_text::kItem0Name),
                      .description = kBufferText,
                      .row_type = (static_cast<std::uint32_t>(base.material)
                                   << kRowMaterialShift) +
                                  kShieldRowType});
}

// decomp: MAINMENU.PRG vs_mainMenu_setShieldRowFromInventory
// (src/MENU/MAINMENU.PRG/2D10.c:259)
ItemRowText FormatInventoryShieldRow(ItemTextHost& host,
                                     const Items::InventoryShield& shield,
                                     std::span<std::uint8_t> buffer) {
  const auto loadout = host.InventoryShieldLoadout(shield);
  if (!loadout) {
    return RefusedRow(host, buffer,
                      "item text: no stats for the shield's inventory table",
                      TextRef{.source = TextSource::kEmpty});
  }
  return FormatShieldRow(host, *loadout, buffer);
}

// decomp: MAINMENU.PRG vs_mainMenu_setArmorRow
// (src/MENU/MAINMENU.PRG/2D10.c:273)
ItemRowText FormatArmorRow(ItemTextHost& host, const Items::ArmorLoadout& armor,
                           std::span<std::uint8_t> buffer) {
  const Items::EquipmentPart& base = armor.armor;
  RowFormatter text(host, buffer);
  ItemRowText row{
      .name = text.Name(base.id),
      .description = std::nullopt,
      .row_type = PartRowType(base.category + kArmorIconBase, base.material)};
  if (base.category == kAccessoryCategory) {
    row.description =
        text.HelpEntry(base.id - kFirstAccessoryId + item_text::kRoodNecklace);
    return text.Finish(row);
  }
  SetTitleStrings(text, EquipmentTitleRanks(armor.class_affinity),
                  item_text::kArmorPrestige, [&] {
                    return text.HelpEntry(base.category - 1 +
                                          item_text::kArmorBasePrestige);
                  });
  text.SetString(kCategoryString,
                 text.HelpEntry(base.category - 1 + item_text::kArmorCategory));
  text.SetString(kMaterialString,
                 text.Name(base.material - 1 + item_text::kWoodName));
  text.Format(text.HelpAt(item_text::kClassTemplateOffset),
              text.HelpAt(item_text::kArmorTemplateOffset));
  row.description = kBufferText;
  return text.Finish(row);
}

// decomp: MAINMENU.PRG vs_mainMenu_setArmorRowFromInventory
// (src/MENU/MAINMENU.PRG/2D10.c:316)
ItemRowText FormatInventoryArmorRow(ItemTextHost& host,
                                    const Items::InventoryArmor& armor,
                                    std::span<std::uint8_t> buffer) {
  Items::ArmorLoadout loadout;
  Items::LoadInventoryArmorStats(loadout, &armor);
  return FormatArmorRow(host, loadout, buffer);
}

// decomp: MAINMENU.PRG vs_mainMenu_setGemRow
// (src/MENU/MAINMENU.PRG/2D10.c:325)
ItemRowText FormatGemRow(ItemTextHost& host, const Items::InventoryGem& gem,
                         std::span<std::uint8_t> buffer) {
  RowFormatter text(host, buffer);
  text.Copy(text.HelpEntry(gem.id - kFirstGemId + item_text::kGemFeldspar));
  return text.Finish({.name = text.Name(gem.id),
                      .description = kBufferText,
                      .row_type = kGemRowType});
}

// decomp: MAINMENU.PRG vs_mainMenu_setMiscRow
// (src/MENU/MAINMENU.PRG/2D10.c:338)
ItemRowText FormatMiscItemRow(ItemTextHost& host,
                              const Items::InventoryMisc& misc,
                              std::span<std::uint8_t> buffer) {
  RowFormatter text(host, buffer);
  text.Copy(text.HelpEntry(misc.id - kFirstMiscId + item_text::kCureRootDesc));
  return text.Finish(
      {.name = text.Name(misc.id),
       .description = kBufferText,
       .row_type = static_cast<std::uint32_t>(misc.count) << kRowCountShift});
}

// decomp: MAINMENU.PRG vs_mainMenu_initWeaponDetailsRow
// (src/MENU/MAINMENU.PRG/2D10.c:14)
MenuRow* CreateWeaponComponentRow(ItemRowsHost& host, int row, int weapon_index,
                                  bool slide_in) {
  const auto weapon = host.Weapon(weapon_index);
  if (!weapon) {
    host.Refuse("weapon component row: no such weapon");
    return nullptr;
  }
  ItemRowText text;
  int width = kPartRowWidth;
  if (row == kBladeRow) {
    const auto blade = host.Blade(weapon->blade);
    if (!blade) {
      host.Refuse("weapon component row: the weapon's blade is missing");
      return nullptr;
    }
    text = FormatBladeRow(host, *blade, host.StringBuffer());
  } else {
    const auto grip = host.Grip(weapon->grip);
    if (!grip) {
      host.Refuse("weapon component row: the weapon's grip is missing");
      return nullptr;
    }
    const int slot = row - kFirstWeaponGemRow;
    if (row == kGripRow) {
      text = FormatGripRow(host, *grip, host.StringBuffer());
    } else if (slot < grip->gem_slots) {
      if (slot < 0 || static_cast<std::size_t>(slot) >= weapon->gems.size()) {
        host.Refuse("weapon component row: a gem slot past the weapon");
        return nullptr;
      }
      const auto gem =
          GemSlotRow(host, weapon->gems[static_cast<std::size_t>(slot)]);
      if (!gem) {
        host.Refuse("weapon component row: the slot's gem is missing");
        return nullptr;
      }
      text = *gem;
      width = kGemRowWidth;
    } else {
      width = 0;
    }
  }
  MenuRow* item = PlaceComponentRow(host, row, width, text, slide_in);
  if (item != nullptr) {
    item->row_icon = static_cast<std::uint8_t>(text.row_type >> kRowIconShift);
    item->material = static_cast<std::uint8_t>(
        (text.row_type >> kRowMaterialShift) & kRowMaterialMask);
  }
  return item;
}

// decomp: MAINMENU.PRG vs_mainMenu_initShieldDetailsRow
// (src/MENU/MAINMENU.PRG/2D10.c:80)
MenuRow* CreateShieldGemRow(ItemRowsHost& host, int row, int shield_index,
                            bool slide_in) {
  const auto shield = host.Shield(shield_index);
  if (!shield) {
    host.Refuse("shield gem row: no such shield");
    return nullptr;
  }
  const int slot = row - kFirstShieldGemRow;
  ItemRowText text;
  int width = 0;
  if (slot < shield->base.gem_slots) {
    if (slot < 0 || static_cast<std::size_t>(slot) >= shield->gems.size()) {
      host.Refuse("shield gem row: a gem slot past the shield");
      return nullptr;
    }
    const auto gem =
        GemSlotRow(host, shield->gems[static_cast<std::size_t>(slot)]);
    if (!gem) {
      host.Refuse("shield gem row: the slot's gem is missing");
      return nullptr;
    }
    text = *gem;
    width = kGemRowWidth;
  }
  MenuRow* item = PlaceComponentRow(host, row, width, text, slide_in);
  if (item != nullptr) {
    item->row_icon = kShieldGemIcon;
  }
  return item;
}

// decomp: MAINMENU.PRG vs_mainMenu_loadItemText
// (src/MENU/MAINMENU.PRG/2D10.c:1119)
int LoadItemText(ItemTextLoadState& state, ItemTextLoadHost& host, bool load) {
  if (load) {
    host.StartItemTextLoad();
    state.loading = true;
    return 0;
  }
  if (!state.loading) {
    return 1;
  }
  const auto loaded = host.ItemTextLoaded();
  if (!loaded) {
    host.Refuse("item text: polled with no queued read");
    state.loading = false;
    return 1;
  }
  if (*loaded) {
    host.ReleaseItemTextRequest();
    state.loading = false;
    return 1;
  }
  return 0;
}

// decomp: MAINMENU.PRG vs_mainMenu_ensureItemTextUnloaded
// (src/MENU/MAINMENU.PRG/2D10.c:1161)
int UnloadItemTextWhenReady(ItemTextLoadState& state, ItemTextLoadHost& host) {
  if (!host.ItemTextAllocated()) {
    return 1;
  }
  if (LoadItemText(state, host, false) != 0) {
    host.FreeItemText();
    return 1;
  }
  return 0;
}
}  // namespace VS::Menu::Framework
