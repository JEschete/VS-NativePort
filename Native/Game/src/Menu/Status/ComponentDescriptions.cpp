#include "Menu/Status/ComponentDescriptions.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Items/EquipmentStats.hpp"
#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Render/EncodedText.hpp"

namespace VS::Menu::Status {  // NOLINT(readability-identifier-naming)
namespace {
using Bytes = std::span<const std::uint8_t>;
namespace text = Framework::item_text;
constexpr std::size_t kMaterialArgument = 9;
constexpr std::size_t kCategoryArgument = 8;
constexpr std::size_t kDamageArgument = 7;
constexpr std::size_t kHandsArgument = 6;
constexpr std::size_t kGripArgument = 5;
constexpr std::array<std::size_t, 5> kArgumentOrder{
    kMaterialArgument, kCategoryArgument, kDamageArgument,
    kHandsArgument,    kGripArgument,
};
constexpr int kGripDescriptionBias = 6;
constexpr int kGemDescriptionBias = 261 - text::kGemFeldspar;
constexpr int kComponentRowBase = 20;
constexpr int kBladeRow = 1;
constexpr int kGripRow = 2;
constexpr int kFirstWeaponGemRow = 3;
constexpr int kWeaponRows = 6;
constexpr int kFirstShieldGemRow = 1;
constexpr int kShieldRows = 4;
constexpr int kFirstSocketElement = 9;
constexpr std::uint32_t kSocketMask = 0xF;
constexpr std::uint32_t kPpFlag = 1U << 5;
constexpr std::uint32_t kDpFlag = 1U << 6;
constexpr std::uint32_t kRangeRiskFlag = 1U << 4;
constexpr Framework::TextRef kBufferText{
    .source = Framework::TextSource::kBuffer,
};

enum class DetailElement {
  kPp = 0,
  kDp = 1,
  kStat0 = 2,
  kStat1 = 3,
  kStat2 = 4,
  kStat3 = 5,
  kStat4 = 6,
  kStat5 = 7,
  kStat6 = 8,
  kComponent0 = 9,
  kComponent1 = 10,
  kComponent2 = 11,
  kComponent3 = 12,
  kComponent4 = 13,
  kComponent5 = 14,
  kComponent6 = 15,
  kRange = 16,
  kRisk = 17,
  kPrimary0 = 18,
  kPrimary1 = 19,
  kPrimary2 = 20,
};

ComponentDescription Refuse(Framework::ItemTextHost& host,
                            std::string_view reason) {
  host.Refuse(reason);
  return std::nullopt;
}

Bytes Help(Framework::ItemTextHost& host, int entry) {
  const auto table = host.HelpText();
  const auto reference = Framework::ItemHelpEntry(table, entry);
  return reference ? table.subspan(reference->offset) : Bytes{};
}

// Stage both expansions before publishing: a malformed template cannot leave
// an apparently successful partial description in the shared 96-byte buffer.
class DescriptionBuilder {
 public:
  explicit DescriptionBuilder(Framework::ItemTextHost& host)
      : host_(host), arguments_(host.TextArguments()) {}

  void Argument(std::size_t slot, Bytes value) {
    valid_ &= !value.empty();
    arguments_.strings[slot] = value;
    changed_[slot] = true;
  }

  ComponentDescription Format(std::span<std::uint8_t> destination, Bytes first,
                              std::size_t template_halfword) {
    const auto table = host_.HelpText();
    const auto second = Framework::ItemHelpText(table, template_halfword);
    if (!valid_ || first.empty() || !second) {
      return Refuse(host_, "status component: unavailable description text");
    }
    std::array<std::uint8_t, Framework::kRowDescriptionBufferBytes> scratch{};
    const auto first_end =
        Render::FormatEncodedText(scratch, first, arguments_);
    if (!first_end) {
      return Refuse(host_, "status component: invalid first template");
    }
    const auto second_end =
        Render::FormatEncodedText(std::span(scratch).subspan(*first_end),
                                  table.subspan(second->offset), arguments_);
    if (!second_end || *first_end + *second_end >= destination.size()) {
      return Refuse(host_, "status component: description exceeds its buffer");
    }
    for (const auto slot : kArgumentOrder) {
      if (changed_[slot]) {
        host_.SetStringArgument(slot, arguments_.strings[slot]);
      }
    }
    std::copy_n(scratch.begin(), *first_end + *second_end + 1,
                destination.begin());
    return kBufferText;
  }

 private:
  Framework::ItemTextHost& host_;
  Render::EncodedTextArguments arguments_;
  std::array<bool, Render::kEncodedTextArgumentCount> changed_{};
  bool valid_ = true;
};

ComponentDescription RowDescription(const Framework::ItemRowText& row) {
  if (!row.description ||
      row.description->source == Framework::TextSource::kEmpty ||
      row.description->source == Framework::TextSource::kNone) {
    return std::nullopt;
  }
  return row.description;
}

ComponentDescription NoGems(Framework::ItemTextHost& host) {
  const auto result =
      Framework::ItemHelpText(host.HelpText(), text::kNoGemsOffset);
  return result ? result
                : Refuse(host, "status component: no-gems text unavailable");
}

bool ValidMenuRow(Framework::ItemTextHost& host, int row) {
  if (row < -kComponentRowBase ||
      row >= static_cast<int>(Framework::kRowCount) - kComponentRowBase) {
    host.Refuse("status component: selection outside the menu workspace");
    return false;
  }
  return true;
}

ComponentDescription Highlight(ComponentContext& context, int row,
                               ComponentDescription description) {
  if (!description) {
    return std::nullopt;
  }
  if (!context.display.SelectRow(row + kComponentRowBase)) {
    return Refuse(context.text, "status component: missing menu workspace");
  }
  return description;
}

ComponentDescription SelectGem(ComponentContext& context,
                               const Items::EquipmentPart& gem) {
  if (gem.id == 0) {
    const auto description = NoGems(context.text);
    if (description) {
      context.display.HideComponentBars();
      context.display.ClearComponentStats();
    }
    return description;
  }
  const auto description =
      CopyGemDescription(context.text, gem, context.buffer);
  if (description) {
    context.display.ShowGemStats(gem);
  }
  return description;
}
}  // namespace

// decomp: initBladeDescription (src/MENU/MENU4.PRG/120.c:234; was
// _initBladeDescription); original MENU4.PRG [0x80102F64, 0x80103080).
ComponentDescription FormatBladeDescription(Framework::ItemTextHost& host,
                                            const Items::EquipmentPart& blade,
                                            std::span<std::uint8_t> buffer) {
  const auto hands = host.WeaponHands();
  if (blade.category == 0 || blade.category > hands.size()) {
    return Refuse(host, "status component: blade category outside hand table");
  }
  const auto names = host.NameText();
  const auto material_row =
      static_cast<std::size_t>(blade.material) + text::kWoodName - 1;
  if (material_row >= names.size() / Framework::kItemNameRowBytes) {
    return Refuse(host, "status component: material name unavailable");
  }
  DescriptionBuilder description(host);
  description.Argument(
      kMaterialArgument,
      names.subspan(material_row * Framework::kItemNameRowBytes));
  description.Argument(kCategoryArgument,
                       Help(host, blade.category + text::kDagger - 1));
  description.Argument(kDamageArgument,
                       Help(host, blade.damage_type + text::kBlunt - 1));
  description.Argument(
      kHandsArgument, Help(host, hands[blade.category - 1] + text::kOneHanded));
  return description.Format(buffer, Help(host, blade.id - 1),
                            text::kWeaponDescTemplateOffset);
}

// decomp: initGripInfo (src/MENU/MENU4.PRG/120.c:260; was _initGripInfo);
// original MENU4.PRG [0x80103080, 0x80103118).
ComponentDescription FormatGripDescription(Framework::ItemTextHost& host,
                                           const Items::EquipmentPart& grip,
                                           std::span<std::uint8_t> buffer) {
  DescriptionBuilder description(host);
  description.Argument(kGripArgument,
                       Help(host, grip.category + text::kShortGrip - 1));
  return description.Format(buffer, Help(host, grip.id - kGripDescriptionBias),
                            text::kGripTemplateOffset);
}

// decomp: initGemInfo (src/MENU/MENU4.PRG/120.c:280; was _initGemInfo);
// original MENU4.PRG [0x80103118, 0x8010317C).
ComponentDescription CopyGemDescription(Framework::ItemTextHost& host,
                                        const Items::EquipmentPart& gem,
                                        std::span<std::uint8_t> buffer) {
  const auto source = Help(host, gem.id - kGemDescriptionBias);
  constexpr auto kCopyBytes = Framework::kRowDescriptionBufferBytes;
  if (source.size() < kCopyBytes || buffer.size() < kCopyBytes) {
    return Refuse(host, "status component: gem copy outside text or buffer");
  }
  std::copy_n(source.begin(), kCopyBytes, buffer.begin());
  return kBufferText;
}

// decomp: drawWeaponInfoRow (src/MENU/MENU4.PRG/120.c:297; was
// _drawWeaponInfoRow); original MENU4.PRG [0x8010317C, 0x801032C4).
ComponentDescription SelectWeaponComponent(ComponentContext& context, int row,
                                           const Items::WeaponLoadout& weapon) {
  if (!ValidMenuRow(context.text, row)) {
    return std::nullopt;
  }
  ComponentDescription description;
  if (row == 0) {
    description = RowDescription(
        Framework::FormatWeaponRow(context.text, weapon, context.buffer));
  } else if (row == kBladeRow) {
    description =
        FormatBladeDescription(context.text, weapon.blade, context.buffer);
    if (description) {
      context.display.ShowBladeStats(weapon);
    }
  } else if (row == kGripRow) {
    description =
        FormatGripDescription(context.text, weapon.grip, context.buffer);
    if (description) {
      context.display.ShowGripStats(weapon.grip);
    }
  } else if (row >= kFirstWeaponGemRow && row < kWeaponRows) {
    description = SelectGem(
        context,
        weapon.gems[static_cast<std::size_t>(row - kFirstWeaponGemRow)]);
  } else {
    description = NoGems(context.text);
  }
  return Highlight(context, row, description);
}

// decomp: drawShieldInfoRow (src/MENU/MENU4.PRG/120.c:343; was
// _drawShieldInfoRow); original MENU4.PRG [0x801032C4, 0x801033A4).
ComponentDescription SelectShieldComponent(ComponentContext& context, int row,
                                           const Items::ShieldLoadout& shield) {
  if (!ValidMenuRow(context.text, row)) {
    return std::nullopt;
  }
  ComponentDescription description;
  if (row == 0) {
    description = RowDescription(
        Framework::FormatShieldRow(context.text, shield, context.buffer));
  } else if (row >= kFirstShieldGemRow && row < kShieldRows) {
    description = SelectGem(
        context,
        shield.gems[static_cast<std::size_t>(row - kFirstShieldGemRow)]);
  } else {
    description = NoGems(context.text);
  }
  return Highlight(context, row, description);
}

// decomp: drawArmorInfoRow (src/MENU/MENU4.PRG/120.c:372; was
// _drawArmorInfoRow); original MENU4.PRG [0x801033A4, 0x801033D4).
ComponentDescription ArmorDescription(Framework::ItemTextHost& host,
                                      const Items::ArmorLoadout& armor,
                                      std::span<std::uint8_t> buffer) {
  return RowDescription(Framework::FormatArmorRow(host, armor, buffer));
}

// decomp: drawAccessoryInfoRow (src/MENU/MENU4.PRG/120.c:387; was
// _drawAccessoryInfoRow); original MENU4.PRG [0x801033D4, 0x8010341C).
ComponentDescription AccessoryDescription(
    Framework::ItemTextHost& host,
    const Items::AccessorySpawnLoadout& accessory,
    std::span<std::uint8_t> buffer) {
  if (accessory.accessory.id == 0) {
    return Refuse(host, "status component: absent accessory (#418)");
  }
  Items::InventoryArmor inventory{};
  Items::StoreInventoryAccessoryStats(inventory, accessory);
  return RowDescription(
      Framework::FormatInventoryArmorRow(host, inventory, buffer));
}

// decomp: equipmentDetailElementSelectable (src/MENU/MENU4.PRG/120.c:405; was
// _equipmentDetailElementSelectable); original MENU4.PRG [0x8010341C,
// 0x801034BC).
bool IsDetailElementSelectable(int element, std::uint32_t flags,
                               std::uint8_t stat_page) {
  switch (static_cast<DetailElement>(element)) {
    case DetailElement::kPp:
      return (flags & kPpFlag) != 0;
    case DetailElement::kDp:
      return (flags & kDpFlag) != 0;
    case DetailElement::kStat3:
    case DetailElement::kStat4:
    case DetailElement::kStat5:
      return stat_page != Framework::stat_page::kType;
    case DetailElement::kStat6:
      return stat_page == Framework::stat_page::kAffinity;
    case DetailElement::kComponent0:
    case DetailElement::kComponent1:
    case DetailElement::kComponent2:
    case DetailElement::kComponent3:
    case DetailElement::kComponent4:
    case DetailElement::kComponent5:
    case DetailElement::kComponent6:
      return (flags & kSocketMask) >=
             static_cast<std::uint32_t>(element - kFirstSocketElement);
    case DetailElement::kRange:
    case DetailElement::kRisk:
      return (flags & kRangeRiskFlag) != 0;
    case DetailElement::kStat0:
    case DetailElement::kStat1:
    case DetailElement::kStat2:
    case DetailElement::kPrimary0:
    case DetailElement::kPrimary1:
    case DetailElement::kPrimary2:
      return true;
    default:
      return false;
  }
}
}  // namespace VS::Menu::Status
