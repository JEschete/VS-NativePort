// Hand-maintained (tools/port/owned.csv).
#include "Menu/Framework/EquipmentDetails.hpp"  // NOLINT(build/include): typed API

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Render/UiPackets.hpp"

namespace VS::Menu::Framework {
namespace {
constexpr std::uint32_t kPadUp = 0x1000;
constexpr std::uint32_t kPadRight = 0x2000;
constexpr std::uint32_t kPadDown = 0x4000;
constexpr std::uint32_t kPadLeft = 0x8000;
constexpr std::uint32_t kPadSquare = 0x80;
constexpr int kDirectionUp = 1;
constexpr int kDirectionDown = 2;
constexpr int kDirectionLeft = 3;
constexpr int kDirectionRight = 4;

// vs_mainMenu_equipmentStats' layout.
constexpr std::size_t kClassStats = 0;
constexpr std::size_t kAffinityStats = 16;
constexpr std::size_t kClassAffinityStats = 32;
constexpr std::size_t kTypeStats = 32;
constexpr std::size_t kDamageTypeStat = 36;
constexpr std::size_t kComparisonEnabledStat = 63;
constexpr std::size_t kTotalsSetStride = 8;

// The detail screen's elements (vs_mainMenu_statusViewNavigationMap): the
// DP (0) and PP (1) gauges, the stat page's rows (2-8), the item and its
// component rows (9-14), the unused element 15, range and risk (16-17) and
// strength, intelligence and agility (18-20).
constexpr int kLastGaugeElement = 1;
constexpr int kFirstStatPageElement = 2;
constexpr int kLastStatPageElement = 8;
constexpr int kLastShieldGemElement = 12;
constexpr int kFirstWeaponGemElement = 12;
constexpr int kLastComponentElement = 14;
constexpr int kUnusedElement = 15;
constexpr int kFirstRangeElement = 16;
constexpr int kFirstPrimaryElement = 18;
constexpr int kShieldFirstGemElement = 10;
constexpr std::uint32_t kStatPageElementSpan = 14;
constexpr std::uint32_t kShieldSlotNudgeSpan = 2;
constexpr std::uint32_t kShieldSlotNudge = 7;
constexpr int kElement15Shift = 20;
constexpr std::uint8_t kGaugeElementsSubtypes = 0xB;
constexpr std::uint8_t kPpElementSubtypes = 0x1B;
constexpr std::uint8_t kRangeRiskSubtypes = 3;
constexpr std::uint8_t kAttackSubtypes = 7;
constexpr std::uint8_t kDefenseSubtypes = 0x18;
constexpr int kSubtypeBits = 7;
// vs_mainMenu_renderStatusView's dispatch: one description per subtype bit.
constexpr int kWeaponBit = 0;
constexpr int kBladeBit = 1;
constexpr int kGripBit = 2;
constexpr int kShieldBit = 3;
constexpr int kArmorBit = 4;
constexpr int kAccessoryBit = 5;

// The detail screen's menu rows: the item at 10, its components from 11.
constexpr int kDetailMenuRowBase = 10;
constexpr int kFirstComponentMenuRow = 11;
constexpr int kLastComponentMenuRow = 15;
constexpr int kWeaponBladeRow = 1;
constexpr int kWeaponGripRow = 2;
constexpr int kWeaponFirstGemRow = 3;
constexpr int kWeaponLastGemRow = 5;
constexpr int kShieldFirstGemRow = 1;
constexpr int kShieldLastGemRow = 3;

constexpr std::uint8_t kAccessoryCategory = 7;
constexpr std::uint32_t kEquippedLimbs = 5;
constexpr int kDpPercent = 100;
constexpr int kDpRounding = 99;

// vs_mainMenu_drawClassAffinityType's panel and page turn.
constexpr std::uint8_t kPanelSlideSteps = 10;
constexpr int kPanelSlidePitch = 24;
constexpr int kStatPagesCycle = 4;
constexpr std::uint32_t kStatPageMask = 3;

// _renderStatPage's layout.
constexpr int kStatRowPitch = 16;
constexpr int kStatRowShift = 4;
constexpr int kStatRowTop = 64;
constexpr int kClassRowsHeight = 0x60;
constexpr int kAffinityRowsHeight = 0x70;
constexpr int kTypeRowsHeight = 48;
constexpr std::size_t kHighestAffinityRows = 7;
constexpr int kNoHighestAffinity = 0xFFFF;
constexpr std::size_t kClassComparison = 48;
constexpr std::size_t kAffinityComparison = 56;
constexpr std::size_t kDisplayedTypes = 33;
constexpr int kStatNumberX = 88;
constexpr int kStatComparisonX = 0xA4;
constexpr int kStatHeaderX = 0x40;
constexpr std::uint32_t kStatHeadersPerPage = 8;
constexpr int kBarOriginX = 116;
constexpr int kBarFrameTop = 0x42;
constexpr int kBarFrameBottom = 0x45;
constexpr int kBarShadowBottom = 0x47;
constexpr std::uint32_t kBarFrameColor = 0x40D0E030;
constexpr std::uint32_t kBarShadowColor = 0x40978918;
constexpr int kBarScale = 24;
constexpr int kBarPercent = 100;
constexpr int kBarLines = 4;
constexpr int kPositiveBarTop = 66;
constexpr int kNegativeBarBottom = 0x45;
// decomp: local statics of _renderStatPage (C48.c:819, C48.c:831):
// vs_getRGB0(LineF2, r, g, b) words.
constexpr std::array<std::uint32_t, kBarLines> kPositiveBarColors{
    0x40817318,
    0x40D0E030,
    0x40D0E030,
    0x40817318,
};
constexpr std::array<std::uint32_t, kBarLines> kNegativeBarColors{
    0x40693F6E,
    0x409F78DC,
    0x409F78DC,
    0x40693F6E,
};
constexpr int kBackgroundLeftEdge = 97;
constexpr int kBackgroundInnerEdge = 142;
constexpr int kBackgroundOuterEdge = 145;
constexpr int kInnerBackgroundFirst = 2;
constexpr int kInnerBackgroundEnd = 6;
constexpr int kOuterBackgroundEnd = 8;
constexpr std::uint32_t kInnerBackgroundColor = 0x40330500;
constexpr std::uint32_t kOuterBackgroundColor = 0x405D3200;
// vs_getTpage(0, 0, 4-bit, average, dithering off / on).
constexpr std::uint32_t kGradientDrawMode = 0xE1000000;
constexpr std::uint32_t kDitheredDrawMode = 0xE1000200;
constexpr int kGradientLines = 11;
constexpr int kGradientRight = 0x60;
constexpr std::uint32_t kLineG2Tag = 4U << Render::kPacketLengthShift;
constexpr std::uint32_t kGradientLeftColor = 0x50330500;
constexpr std::uint32_t kGradientRightColor = 0x5D3200;
constexpr int kPromptX = 24;
constexpr int kPromptY = 48;
constexpr int kPromptWidth = 135;
constexpr int kPromptHeight = 12;
constexpr int kSquareIcon = 1;
constexpr int kSquareIconX = 16;
constexpr int kSquareIconY = 46;
constexpr int kHeaderX = 0x22;
constexpr int kHeaderY = 0x30;
constexpr std::uint8_t kFontStyleEscape = 0xFB;
constexpr int kFontStyleBias = 4;
constexpr std::uint8_t kEnabledPagePalette = 0;
constexpr std::uint8_t kDisabledPagePalette = 3;
constexpr std::uint8_t kCurrentPagePalette = 1;

// decomp: local statics of _renderStatPage (C48.c:797, C48.c:882): the
// header texts (null between the pages) and the page tabs, whose font style
// bytes 1, 11 and 24 mark the class, affinity and type tabs.
constexpr std::array<const char*, 19> kStatHeaders{
    "#HUMAN", "#BEAST", "#UNDEAD", "#PHANTOM",  "#DRAGON",
    "#EVIL",  nullptr,  nullptr,   "#PHYSICAL", "#AIR",
    "#FIRE",  "#EARTH", "#WATER",  "#LIGHT",    "#DARK",
    nullptr,  "#BLUNT", "#EDGED",  "#PIERCING",
};
constexpr std::array<std::uint8_t, 29> kStatPageTabs{
    0xFB, 0x00, 0x0C, 0x15, 0x0A, 0x1C, 0x1C, 0xFB, 0x00, 0xA1,
    0xFB, 0x00, 0x0A, 0x0F, 0x0F, 0x12, 0x17, 0x12, 0x1D, 0x22,
    0xFB, 0x00, 0xA1, 0xFB, 0x00, 0x1D, 0x22, 0x19, 0x0E,
};
constexpr std::array<std::size_t, 3> kStatPageTabStyles{1, 11, 24};

// vs_mainMenu_renderEquipStats' panel.
constexpr std::uint32_t kPrimaryPanelOrigin = PackXy(248, 126);
constexpr std::uint32_t kPrimaryPanelLine = PackXy(0, 10);
constexpr std::uint32_t kPrimaryPanelSkip = PackXy(0, 20);
constexpr std::uint32_t kPanelHeaderX = 4;
constexpr std::uint32_t kPanelOriginalX = 28;
constexpr std::uint32_t kPanelEquippedX = 60;
constexpr int kPrimarySlideShift = 5;
constexpr int kPrimarySlideEnd = 8;
constexpr int kPrimarySlideBase = 15;
constexpr int kPrimarySlidePerStep = 4;
// vs_mainMenu_strIntAgi's values the primary stats compare with.
constexpr std::size_t kComparedPrimaryStats = kPrimaryStats + 1;
constexpr std::size_t kPrimaryKnown = 3;
constexpr std::size_t kRangeRiskShown = 3;
constexpr std::size_t kComparisonActor = 3;
constexpr std::uint8_t kActorMask = 0xF;
constexpr int kBattleActorCount = 16;
constexpr std::size_t kAttackStatNames = 4;
constexpr std::size_t kDefenseStatNames = 8;
// decomp: local statics of vs_mainMenu_renderEquipStats (C48.c:1074,
// C48.c:1096).
constexpr std::array<const char*, 2> kRangeRiskNames{"#RANGE", "#RISK"};
constexpr std::array<const char*, 11> kPrimaryStatNames{
    "#STRENGTH",    "#INTELLIGENCE", "#AGILITY", nullptr,
    "#ATTACK/STR",  "#ATTACK/INT",   "#AGILITY", nullptr,
    "#DEFENSE/STR", "#DEFENSE/INT",  "#AGILITY",
};

// vs_mainMenu_renderDpPpBars' layout.
constexpr std::uint16_t kBarsShown = 5;
constexpr std::uint8_t kBarsSlideIn = 0;
constexpr std::uint8_t kBarsSlideOut = 1;
constexpr std::uint8_t kBarsStatic = 2;
constexpr int kBarsStateShift = 2;
constexpr std::uint8_t kBarsTargetMask = 3;
constexpr int kBarsPitch = 8;
constexpr std::uint32_t kBarsLabelLift = PackXy(0, -1);
constexpr std::uint32_t kDpLabelX = 10;
constexpr std::uint32_t kPpLabelX = 80;
constexpr std::uint32_t kDpNumberX = 66;
constexpr std::uint32_t kPpNumberX = 136;
constexpr std::uint32_t kCurrentNumberOffset = PackXy(-7, -2);
constexpr int kMaxFont = 0;
constexpr int kCurrentFont = 1;
constexpr int kSeparatorFont = 2;
constexpr int kDpGaugeColor = 0;
constexpr int kPpGaugeColor = 1;
constexpr int kGaugeWidthShift = 6;

[[nodiscard]] std::uint32_t Half(int value) {
  return static_cast<std::uint32_t>(value) & kHalfwordMask;
}

[[nodiscard]] std::uint32_t Line(int y) {
  return static_cast<std::uint32_t>(y) << kHalfwordShift;
}

template <typename Classes, typename Affinities>
void CopyPartClassAffinity(EquipmentDetailsState& state, const Classes& classes,
                           const Affinities& affinities) {
  // The parts' class and affinity values are signed bytes (lb), widened
  // with their sign.
  for (std::size_t i = 0; i < kAffinityStats; ++i) {
    // NOLINTNEXTLINE(bugprone-signed-char-misuse)
    state.stats[kClassStats + i] = classes[i % classes.size()];
    // NOLINTNEXTLINE(bugprone-signed-char-misuse)
    state.stats[kAffinityStats + i] = affinities[i % affinities.size()];
  }
}

void CopyClassAffinityTotals(EquipmentDetailsState& state,
                             const Items::ClassAffinityCurrent& totals) {
  std::size_t stat = kClassStats;
  for (const auto& set : totals.classes) {
    for (const std::int16_t value : set) {
      state.stats[stat++] = value;
    }
  }
  for (const auto& set : totals.affinities) {
    for (const std::int16_t value : set) {
      state.stats[stat++] = value;
    }
  }
  static_assert(sizeof(Items::ClassAffinityCurrent) ==
                kClassAffinityStats * sizeof(std::int16_t));
}

template <typename Types>
void CopyTypes(EquipmentDetailsState& state, const Types& types) {
  // The type values are signed bytes (lb), widened with their sign.
  for (std::size_t i = 0; i < types.size(); ++i) {
    // NOLINTNEXTLINE(bugprone-signed-char-misuse)
    state.stats[kTypeStats + i] = static_cast<std::int16_t>(types[i]);
  }
}

void SetBaseStats(EquipmentDetailsState& state,
                  const Items::LoadoutAttributes& attributes) {
  state.primary[kComparedPrimaryStats] =
      static_cast<std::int16_t>(attributes.base_strength);
  state.primary[kComparedPrimaryStats + 1] =
      static_cast<std::int16_t>(attributes.base_intelligence);
  state.primary[kComparedPrimaryStats + 2] =
      static_cast<std::int16_t>(attributes.base_agility);
}

void SetCurrentStats(EquipmentDetailsState& state,
                     const Items::LoadoutAttributes& attributes, int known) {
  SetPrimaryStats(state,
                  {attributes.current_strength, attributes.current_intelligence,
                   attributes.current_agility, known});
}

std::optional<Items::WeaponLoadout> WeaponLoadoutOf(
    EquipmentDetailsContext& ctx, int weapon) {
  auto& rows = ctx.host.Rows();
  const auto record = rows.Weapon(weapon);
  if (!record) {
    ctx.host.Refuse("weapon stats of a weapon past the table");
    return std::nullopt;
  }
  auto loadout = record->is_equipped != 0
                     ? ctx.host.EquippedWeapon()
                     : rows.InventoryWeaponLoadout(*record);
  if (!loadout) {
    ctx.host.Refuse("weapon stats without the weapon's loadout");
  }
  return loadout;
}

std::optional<Items::ShieldLoadout> ShieldLoadoutOf(
    EquipmentDetailsContext& ctx, int shield) {
  auto& rows = ctx.host.Rows();
  const auto record = rows.Shield(shield);
  if (!record) {
    ctx.host.Refuse("shield stats of a shield past the table");
    return std::nullopt;
  }
  auto loadout = record->is_equipped != 0
                     ? ctx.host.EquippedShield()
                     : rows.InventoryShieldLoadout(*record);
  if (!loadout) {
    ctx.host.Refuse("shield stats without the shield's loadout");
  }
  return loadout;
}

std::optional<Items::ArmorLoadout> ArmorLoadoutOf(EquipmentDetailsContext& ctx,
                                                  int armor) {
  const auto record = ctx.host.Rows().Armor(armor);
  if (!record) {
    ctx.host.Refuse("armor stats of an armor past the table");
    return std::nullopt;
  }
  const std::uint32_t limb = static_cast<std::uint32_t>(record->limb) - 1U;
  auto loadout = limb < kEquippedLimbs
                     ? ctx.host.EquippedArmor(static_cast<int>(limb))
                     : ctx.host.InventoryArmorLoadout(*record);
  if (!loadout) {
    ctx.host.Refuse("armor stats without the armor's loadout");
  }
  return loadout;
}

// The 1-based gem's row and stats; no gem resets the stats.
std::optional<TextRef> SelectGemDetail(EquipmentDetailsContext& ctx, int gem) {
  if (gem == 0) {
    ResetEquipmentStats(ctx.state);
    return std::nullopt;
  }
  auto& rows = ctx.host.Rows();
  const auto record = rows.Gem(gem);
  if (!record) {
    ctx.host.Refuse("detail row of a gem past the table");
    return std::nullopt;
  }
  auto description = FormatGemRow(rows, *record, rows.StringBuffer());
  SetGemStats(ctx, gem);
  return description.description;
}

std::optional<TextRef> SelectWeaponComponent(
    EquipmentDetailsContext& ctx, const Items::InventoryWeapon& weapon,
    int row) {
  auto& rows = ctx.host.Rows();
  if (row == 0) {
    return FormatInventoryWeaponRow(rows, weapon, rows.StringBuffer())
        .description;
  }
  if (row == kWeaponBladeRow) {
    UpdateDpPpBars(ctx, dp_pp_bars::kBoth | dp_pp_bars::kStatic);
    const auto blade = rows.Blade(weapon.blade);
    if (!blade) {
      ctx.host.Refuse("detail row of a blade past the table");
      return std::nullopt;
    }
    auto description = FormatBladeRow(rows, *blade, rows.StringBuffer());
    SetBladeStats(ctx, weapon.blade);
    return description.description;
  }
  UpdateDpPpBars(ctx, dp_pp_bars::kStatic);
  if (row != kWeaponGripRow) {
    return SelectGemDetail(
        ctx, weapon.gems[static_cast<std::size_t>(row - kWeaponFirstGemRow)]);
  }
  const auto grip = rows.Grip(weapon.grip);
  if (!grip) {
    ctx.host.Refuse("detail row of a grip past the table");
    return std::nullopt;
  }
  auto description = FormatGripRow(rows, *grip, rows.StringBuffer());
  SetGripStats(ctx, weapon.grip);
  return description.description;
}

std::optional<TextRef> NoGemsText(EquipmentDetailsContext& ctx) {
  return ItemHelpText(ctx.host.Rows().HelpText(), item_text::kNoGemsOffset);
}

[[nodiscard]] bool IsComponentElementReachable(EquipmentDetailsContext& ctx,
                                               int element) {
  const auto& state = ctx.state;
  auto& rows = ctx.host.Rows();
  if (element <= kLastShieldGemElement &&
      (state.subtype & equipment_subtype::kShield) != 0 && state.item != 0) {
    const int slot = element - kShieldFirstGemElement;
    if (slot < 0) {
      return true;
    }
    const auto shield = rows.Shield(state.item);
    if (!shield) {
      ctx.host.Refuse("detail element of a shield past the table");
      return false;
    }
    return slot < shield->base.gem_slots;
  }
  if ((state.subtype & equipment_subtype::kWeapon) != 0 && state.item != 0) {
    const int slot = element - kFirstWeaponGemElement;
    if (slot < 0) {
      return true;
    }
    const auto weapon = rows.Weapon(state.item);
    const auto grip = weapon ? rows.Grip(weapon->grip) : std::nullopt;
    if (!grip) {
      ctx.host.Refuse("detail element of a weapon past the tables");
      return false;
    }
    return slot < grip->gem_slots;
  }
  return element == kFirstDetailElement;
}

[[nodiscard]] int InputDirection(std::uint32_t repeat) {
  if ((repeat & kPadUp) != 0) {
    return kDirectionUp;
  }
  if ((repeat & kPadDown) != 0) {
    return kDirectionDown;
  }
  if ((repeat & kPadLeft) != 0) {
    return kDirectionLeft;
  }
  return (repeat & kPadRight) != 0 ? kDirectionRight : 0;
}

// The element `direction` leads to from `element`; nothing past the map.
std::optional<std::uint8_t> NavigateFrom(EquipmentDetailsContext& ctx,
                                         std::uint8_t element, int direction) {
  const auto& map = ctx.state.navigation;
  if (element >= map.size() || direction < kDirectionUp ||
      direction > kDirectionRight) {
    ctx.host.Refuse("detail navigation past the map");
    return std::nullopt;
  }
  const std::uint8_t next =
      map[element][static_cast<std::size_t>(direction - 1)];
  if (next >= map.size()) {
    ctx.host.Refuse("detail navigation to an element past the map");
    return std::nullopt;
  }
  return next;
}

// Steps one element at a time until one is reachable.
bool StepToReachableElement(EquipmentDetailsContext& ctx, int step) {
  auto& selected = ctx.state.selected;
  for (std::size_t tries = 0; tries <= kStatusViewElementCount; ++tries) {
    selected = static_cast<std::uint8_t>(selected + step);
    if (IsItemDetailElementReachable(ctx, selected)) {
      return true;
    }
  }
  ctx.host.Refuse("detail element search found nothing reachable");
  return false;
}

// Follows the navigation map in `direction` until an element is reachable;
// where it ends on an unreachable element, steps to the nearest one.
bool SelectReachableElement(EquipmentDetailsContext& ctx, int direction) {
  auto& selected = ctx.state.selected;
  std::uint8_t current = selected;
  if (direction != 0) {
    const auto next = NavigateFrom(ctx, current, direction);
    if (!next) {
      return false;
    }
    selected = *next;
  }
  for (std::size_t tries = 0; !IsItemDetailElementReachable(ctx, selected);
       ++tries) {
    if (current == selected) {
      const bool middle =
          static_cast<std::uint32_t>(current) - 2U < kStatPageElementSpan;
      return StepToReachableElement(ctx, middle ? -1 : 1);
    }
    const auto next = NavigateFrom(ctx, selected, direction);
    if (!next || tries == kStatusViewElementCount) {
      ctx.host.Refuse("detail navigation found nothing reachable");
      return false;
    }
    current = selected;
    selected = *next;
  }
  return true;
}

void RefreshSelectedItemStats(EquipmentDetailsContext& ctx) {
  auto& state = ctx.state;
  if ((state.subtype & equipment_subtype::kWeapon) != 0) {
    UpdateDpPpBars(ctx, dp_pp_bars::kBoth | dp_pp_bars::kStatic);
    SetWeaponStats(ctx, state.item);
  }
  if ((state.subtype & equipment_subtype::kShield) != 0) {
    UpdateDpPpBars(ctx, dp_pp_bars::kBoth | dp_pp_bars::kStatic);
    SetShieldStats(ctx, state.item);
  }
  if ((state.subtype & equipment_subtype::kArmor) != 0) {
    UpdateDpPpBars(ctx, dp_pp_bars::kDp | dp_pp_bars::kStatic);
  }
  if ((state.subtype & equipment_subtype::kAccessory) != 0) {
    UpdateDpPpBars(ctx, dp_pp_bars::kStatic);
  }
}

// vs_mainMenu_renderStatusView's dispatch over the subtype's bits, each
// re-read after the previous description; the last one wins.
std::optional<TextRef> ComponentDescription(EquipmentDetailsContext& ctx,
                                            int row) {
  std::optional<TextRef> description;
  bool described = false;
  for (int bit = 0; bit < kSubtypeBits; ++bit) {
    if (((ctx.state.subtype >> bit) & 1) == 0) {
      continue;
    }
    described = true;
    switch (bit) {
      case kWeaponBit:
        description = SelectWeaponDetailRow(ctx, row);
        break;
      case kBladeBit:
        description = BladeDetailDescription(ctx);
        break;
      case kGripBit:
        description = GripDetailDescription(ctx);
        break;
      case kShieldBit:
        description = SelectShieldDetailRow(ctx, row);
        break;
      case kArmorBit:
      case kAccessoryBit:
        description = ArmorDetailDescription(ctx);
        break;
      default:
        description = GemDetailDescription(ctx);
        break;
    }
  }
  if (!described) {
    ctx.host.Refuse("detail description without an item subtype");
  }
  return description;
}

std::optional<TextRef> StatPageHelp(EquipmentDetailsContext& ctx, int element) {
  const auto help = ctx.host.Rows().HelpText();
  const int row = element - kFirstStatPageElement;
  switch (ctx.state.page) {
    case stat_page::kClass:
      return ItemHelpEntry(help, row + item_text::kHumanClassDesc);
    case stat_page::kAffinity:
      return ItemHelpEntry(help, row + item_text::kPhysicalAffinityDesc);
    case stat_page::kType:
      return ItemHelpEntry(help, row + item_text::kBluntTypeDesc);
    default:
      ctx.host.Refuse("detail help of a stat page past the type page");
      return std::nullopt;
  }
}

std::optional<TextRef> ItemDetailHelp(EquipmentDetailsContext& ctx) {
  const int element = ctx.state.selected;
  if (element <= kLastGaugeElement) {
    return ItemHelpEntry(ctx.host.Rows().HelpText(),
                         item_text::kPhantomPointsDesc - element);
  }
  if (element <= kLastStatPageElement) {
    return StatPageHelp(ctx, element);
  }
  if (element <= kLastComponentElement) {
    return ComponentDescription(ctx, element - kFirstDetailElement);
  }
  const auto help = ctx.host.Rows().HelpText();
  if (element == kUnusedElement) {
    return ItemHelpText(help, item_text::kEndsSetupOffset);
  }
  auto text =
      ItemHelpEntry(help, element - kFirstRangeElement + item_text::kRangeDesc);
  if (element >= kFirstPrimaryElement) {
    const int row = element - kFirstPrimaryElement;
    if ((ctx.state.subtype & kAttackSubtypes) != 0) {
      text = ItemHelpEntry(help, row + item_text::kAttackStr);
    }
    if ((ctx.state.subtype & kDefenseSubtypes) != 0) {
      text = ItemHelpEntry(help, row + item_text::kDefStr);
    }
  }
  return text;
}

void DrawItemDetailCursor(EquipmentDetailsContext& ctx) {
  auto& state = ctx.state;
  const std::uint8_t element = state.selected;
  std::uint32_t position = state.cursor_positions[element];
  if (element == kUnusedElement) {
    position += static_cast<std::uint32_t>(state.element15_enabled)
                << kElement15Shift;
  }
  if ((state.subtype & equipment_subtype::kShield) != 0 &&
      static_cast<std::uint32_t>(element) - kShieldFirstGemElement <
          kShieldSlotNudgeSpan) {
    position += kShieldSlotNudge;
  }
  state.cursor_step = static_cast<std::uint8_t>(
      ctx.host.DrawCursor(state.cursor_step, position));
  state.element15_enabled = 0;
}

struct StatPageView {
  std::size_t displayed = 0;
  std::size_t thresholds = 0;
  std::size_t comparison = 0;
  int height = 0;
  // The affinity page's greatest value until a row shows it.
  int highest = 0;
};

std::optional<StatPageView> StatPageViewOf(const EquipmentDetailsState& state) {
  switch (state.page) {
    case stat_page::kClass:
      return StatPageView{kClassStats, kClassStats + kTotalsSetStride,
                          kClassComparison, kClassRowsHeight, 0};
    case stat_page::kAffinity: {
      const auto first =
          std::span(state.stats).subspan(kAffinityStats, kHighestAffinityRows);
      return StatPageView{kAffinityStats, kAffinityStats + kTotalsSetStride,
                          kAffinityComparison, kAffinityRowsHeight,
                          *std::max_element(first.begin(), first.end())};
    }
    case stat_page::kType:
      return StatPageView{kDisplayedTypes, kDisplayedTypes, 0, kTypeRowsHeight,
                          0};
    default:
      return std::nullopt;
  }
}

void DrawStatComparison(EquipmentDetailsContext& ctx, const StatPageView& view,
                        int x_offset, int row) {
  const auto& stats = ctx.state.stats;
  const auto index = static_cast<std::size_t>(row);
  const int value = stats[view.displayed + index];
  const int compared = stats[view.comparison + index];
  const std::uint32_t end =
      ctx.host.DrawComparedInteger(value - compared,
                                   Half(x_offset + kStatComparisonX) |
                                       Line(row * kStatRowPitch + kStatRowTop),
                                   0, detail_layer::kStatPage);
  if (value > compared) {
    ctx.host.DrawCompactText("#+", end + 1, kStatTextGain,
                             detail_layer::kStatPage);
  }
  if (value == compared) {
    ctx.host.DrawCompactText("#*", end + 1, kStatTextGray,
                             detail_layer::kStatPage);
  }
}

// Draws the row's value; returns its header's colour.
std::uint32_t DrawStatValue(EquipmentDetailsContext& ctx, StatPageView& view,
                            int x_offset, int row) {
  const auto& state = ctx.state;
  const auto index = static_cast<std::size_t>(row);
  const int value = state.stats[view.displayed + index];
  const int threshold = state.stats[view.thresholds + index];
  const std::uint32_t xy =
      Half(x_offset + kStatNumberX) | Line(row * kStatRowPitch + kStatRowTop);
  if (state.subtype != equipment_subtype::kWeapon ||
      state.enabled_pages != stat_page::kAllEnabled ||
      state.page == stat_page::kClass) {
    ctx.host.DrawComparedInteger(value, xy, threshold, detail_layer::kStatPage);
    return kStatTextGray;
  }
  if (state.page == stat_page::kAffinity) {
    std::uint32_t color = kStatTextGray;
    if (threshold != value) {
      color = threshold < value ? kStatTextGain : kStatTextLoss;
    } else if (threshold != view.highest) {
      color = kStatTextDim;
    }
    ctx.host.DrawColoredInteger(value, xy, color, detail_layer::kStatPage);
    if (value != view.highest) {
      return kStatTextDim;
    }
    view.highest = kNoHighestAffinity;
    return kStatTextGray;
  }
  const std::uint32_t color =
      row != state.stats[kDamageTypeStat] - 1 ? kStatTextDim : kStatTextGray;
  ctx.host.DrawColoredInteger(value, xy, color, detail_layer::kStatPage);
  return color;
}

void InsertLine(Render::PacketQueue& queue, std::uint32_t color,
                std::uint32_t from, std::uint32_t to) {
  Render::InsertThreeWordPrimitive(queue, {color, from, to});
}

void DrawStatBar(Render::PacketQueue& queue, int x_offset, int y,
                 std::int16_t value) {
  const std::uint32_t origin = Half(x_offset + kBarOriginX);
  InsertLine(queue, kBarFrameColor, origin | Line(y + kBarFrameTop),
             origin | Line(y + kBarFrameBottom));
  InsertLine(queue, kBarShadowColor, origin | Line(y + kStatRowTop),
             origin | Line(y + kBarShadowBottom));
  int length =
      std::clamp(value * kBarScale / kBarPercent, -kBarScale, kBarScale);
  if (length >= 0) {
    for (int i = 0; i < kBarLines; ++i) {
      const std::uint32_t line = Line(y + i + kPositiveBarTop);
      InsertLine(queue, kPositiveBarColors[static_cast<std::size_t>(i)],
                 origin | line, Half(x_offset + length + kBarOriginX) | line);
      if (length == kBarScale - i) {
        --length;
      }
    }
    return;
  }
  for (int i = 0; i < kBarLines; ++i) {
    const std::uint32_t line = Line(y - i + kNegativeBarBottom);
    InsertLine(queue, kNegativeBarColors[static_cast<std::size_t>(i)],
               origin | line, Half(x_offset + length + kBarOriginX) | line);
    if (length == i - kBarScale) {
      length = i - kBarScale + 1;
    }
  }
}

void DrawStatRowBackground(Render::PacketQueue& queue, int x_offset, int y) {
  for (int i = kInnerBackgroundFirst; i < kInnerBackgroundEnd; ++i) {
    const std::uint32_t line = Line(y + i + kStatRowTop);
    InsertLine(
        queue, kInnerBackgroundColor,
        OffsetStatBarCoordinate(x_offset, i, kBackgroundLeftEdge) | line,
        OffsetStatBarCoordinate(x_offset, i, kBackgroundInnerEdge) | line);
  }
  for (int i = 0; i < kOuterBackgroundEnd; ++i) {
    const std::uint32_t line = Line(y + i + kStatRowTop);
    InsertLine(
        queue, kOuterBackgroundColor,
        OffsetStatBarCoordinate(x_offset, i, kBackgroundLeftEdge) | line,
        OffsetStatBarCoordinate(x_offset, i, kBackgroundOuterEdge) | line);
  }
  Render::InsertDrawModeCommand(queue, kGradientDrawMode);
  for (int i = 0; i < kGradientLines; ++i) {
    const std::uint32_t line = Line(y + i + kStatRowTop);
    const std::array<std::uint32_t, 5> packet{
        (queue.ChainLink() & Render::kPacketLinkMask) | kLineG2Tag,
        kGradientLeftColor, Half(x_offset) | line, kGradientRightColor,
        Half(x_offset - (i - kGradientRight)) | line};
    queue.Emit(packet);
  }
  Render::InsertDrawModeCommand(queue, kDitheredDrawMode);
}

void DrawStatRow(EquipmentDetailsContext& ctx, Render::PacketQueue& queue,
                 StatPageView& view, int x_offset, int y) {
  const auto& state = ctx.state;
  const int row = y >> kStatRowShift;
  if (state.stats[kComparisonEnabledStat] != 0 &&
      state.enabled_pages == stat_page::kAllEnabled &&
      state.page != stat_page::kType) {
    DrawStatComparison(ctx, view, x_offset, row);
  }
  const std::uint32_t color = DrawStatValue(ctx, view, x_offset, row);
  ctx.host.DrawCompactText(
      kStatHeaders[static_cast<std::size_t>(row) +
                   (static_cast<std::size_t>(state.page) *
                    kStatHeadersPerPage)],
      Half(x_offset + kStatHeaderX) | Line(y + kStatRowTop), color,
      detail_layer::kStatPage);
  DrawStatBar(queue, x_offset, y,
              state.stats[view.displayed + static_cast<std::size_t>(row)]);
  DrawStatRowBackground(queue, x_offset, y);
}

// The page tabs, each tab's style by whether its page is enabled or shown.
void DrawStatPageTabs(EquipmentDetailsContext& ctx, int x_offset) {
  ctx.host.DrawButtonPromptBackground(x_offset + kPromptX, kPromptY,
                                      kPromptWidth, kPromptHeight);
  ctx.host.DrawButtonIcon(kSquareIcon, x_offset + kSquareIconX, kSquareIconY,
                          detail_layer::kButtonIcon);
  auto tabs = kStatPageTabs;
  for (std::size_t page = 0; page < kStatPageTabStyles.size(); ++page) {
    tabs[kStatPageTabStyles[page]] =
        ((ctx.state.enabled_pages >> page) & 1) != 0 ? kEnabledPagePalette
                                                     : kDisabledPagePalette;
  }
  tabs[kStatPageTabStyles[ctx.state.page]] = kCurrentPagePalette;
  int x = static_cast<int>(Half(x_offset)) + kHeaderX;
  std::size_t i = 0;
  while (i < tabs.size()) {
    if (tabs[i] == kFontStyleEscape && i + 1 < tabs.size()) {
      ctx.host.SetFontStyle(tabs[i + 1] + kFontStyleBias);
      i += 2;
      continue;
    }
    x = ctx.host.DrawGlyph(tabs[i], x, kHeaderY, detail_layer::kHeaderGlyphs);
    ++i;
  }
}

void DrawRangeRiskRows(EquipmentDetailsContext& ctx, std::uint32_t& position,
                       bool known) {
  auto& host = ctx.host;
  host.DrawCompactText("ORG/EQP", position + kPanelHeaderX, kStatTextGray,
                       detail_layer::kPrimaryPanel);
  for (std::size_t i = 0; i < kRangeRiskNames.size(); ++i) {
    position += kPrimaryPanelLine;
    host.DrawCompactText(kRangeRiskNames[i], position, kStatTextGray,
                         detail_layer::kPrimaryPanel);
    if (!known) {
      host.DrawCompactText("#??", position + kPanelOriginalX, kStatTextGray,
                           detail_layer::kPrimaryPanel);
      host.DrawCompactText("#??", position + kPanelEquippedX, kStatTextGray,
                           detail_layer::kPrimaryPanel);
      continue;
    }
    int stat = ctx.state.range_risk[i];
    host.DrawNeutralInteger(stat, position + kPanelOriginalX,
                            detail_layer::kPrimaryPanel);
    if (i == 0) {
      const auto actor =
          host.ActorStats(ctx.state.comparison_deltas[kComparisonActor]);
      if (!actor) {
        host.Refuse("range comparison of an absent actor");
        continue;
      }
      stat += actor->current_range - actor->weapon_range;
    }
    host.DrawNeutralInteger(stat, position + kPanelEquippedX,
                            detail_layer::kPrimaryPanel);
  }
}

void DrawPrimaryStatRows(EquipmentDetailsContext& ctx, std::uint32_t position,
                         int known) {
  const auto& state = ctx.state;
  auto& host = ctx.host;
  const std::size_t names =
      ((state.subtype & kAttackSubtypes) != 0 ? kAttackStatNames : 0) +
      ((state.subtype & kDefenseSubtypes) != 0 ? kDefenseStatNames : 0);
  if (names + kPrimaryStats > kPrimaryStatNames.size()) {
    host.Refuse("primary stat names past the table");
    return;
  }
  for (std::size_t i = 0; i < kPrimaryStats; ++i) {
    position += kPrimaryPanelLine;
    host.DrawCompactText(kPrimaryStatNames[names + i], position, kStatTextGray,
                         detail_layer::kPrimaryPanel);
    if (known == 0) {
      host.DrawCompactText("#??", position + kPanelOriginalX, kStatTextGray,
                           detail_layer::kPrimaryPanel);
      host.DrawCompactText("#??", position + kPanelEquippedX, kStatTextGray,
                           detail_layer::kPrimaryPanel);
      continue;
    }
    const int stat = state.primary[i];
    host.DrawComparedInteger(stat, position + kPanelOriginalX,
                             state.primary[kComparedPrimaryStats + i],
                             detail_layer::kPrimaryPanel);
    const int total = stat + state.comparison_totals[i];
    const int accessory = (known & 1) != 0 ? state.comparison_accessory[i] : 0;
    host.DrawComparedInteger(stat + state.comparison_deltas[i] + accessory,
                             position + kPanelEquippedX, total + accessory,
                             detail_layer::kPrimaryPanel);
  }
}
}  // namespace

// decomp: MAINMENU.PRG vs_mainMenu_resetStats
// (src/MENU/MAINMENU.PRG/2D10.c:351)
void ResetEquipmentStats(EquipmentDetailsState& state) {
  state.stats.fill(0);
  SetDpPp(state, {0, 0, 0, 0});
  SetPrimaryStats(state, {0, 0, 0, 1});
}

// decomp: MAINMENU.PRG vs_mainMenu_setStatsFromWeapon
// (src/MENU/MAINMENU.PRG/2D10.c:358)
void SetWeaponStats(EquipmentDetailsContext& ctx, int weapon) {
  auto& state = ctx.state;
  ResetEquipmentStats(state);
  SetRangeAndRisk(state, {0, 0, 0, 1});
  if (weapon != 0) {
    if (const auto loadout = WeaponLoadoutOf(ctx, weapon)) {
      CopyClassAffinityTotals(state, loadout->class_affinity);
      CopyTypes(state, loadout->grip.types);
      state.stats[kDamageTypeStat] = loadout->damage_type;
      SetDpPp(state, {loadout->current_dp, loadout->max_dp, loadout->current_pp,
                      loadout->max_pp});
      SetCurrentStats(state, loadout->attributes, 1);
      SetRangeAndRisk(state, {loadout->range.range, loadout->risk, 0, 1});
      SetBaseStats(state, loadout->attributes);
    }
  }
  state.subtype = equipment_subtype::kWeapon;
  state.item = static_cast<std::uint8_t>(weapon);
  SetEnabledStatPages(ctx, stat_page::kAllEnabled);
}

// decomp: MAINMENU.PRG vs_mainMenu_setStatsFromBlade
// (src/MENU/MAINMENU.PRG/2D10.c:407)
void SetBladeStats(EquipmentDetailsContext& ctx, int blade) {
  auto& state = ctx.state;
  const auto record = ctx.host.Rows().Blade(blade);
  if (!record) {
    ctx.host.Refuse("blade stats of a blade past the table");
    return;
  }
  CopyPartClassAffinity(state, record->classes, record->affinities);
  SetDpPp(state, {record->current_dp, record->max_dp, record->current_pp,
                  record->max_pp});
  SetPrimaryStats(state,
                  {record->strength, record->intelligence, record->agility, 1});
  SetRangeAndRisk(state, {record->range.range, record->cost, 0, 1});
  state.subtype = equipment_subtype::kBlade;
  state.item = static_cast<std::uint8_t>(blade);
  SetEnabledStatPages(ctx,
                      stat_page::kClassEnabled | stat_page::kAffinityEnabled);
}

// decomp: MAINMENU.PRG vs_mainMenu_setStatsFromGrip
// (src/MENU/MAINMENU.PRG/2D10.c:427)
void SetGripStats(EquipmentDetailsContext& ctx, int grip) {
  auto& state = ctx.state;
  const auto record = ctx.host.Rows().Grip(grip);
  if (!record) {
    ctx.host.Refuse("grip stats of a grip past the table");
    return;
  }
  CopyTypes(state, record->types);
  SetPrimaryStats(state,
                  {record->strength, record->intelligence, record->agility, 1});
  state.subtype = equipment_subtype::kGrip;
  state.item = static_cast<std::uint8_t>(grip);
  SetEnabledStatPages(ctx, stat_page::kTypeEnabled);
}

// decomp: MAINMENU.PRG vs_mainMenu_setStatsFromShield
// (src/MENU/MAINMENU.PRG/2D10.c:443)
void SetShieldStats(EquipmentDetailsContext& ctx, int shield) {
  auto& state = ctx.state;
  ResetEquipmentStats(state);
  if (shield != 0) {
    if (const auto loadout = ShieldLoadoutOf(ctx, shield)) {
      CopyClassAffinityTotals(state, loadout->class_affinity);
      CopyTypes(state, loadout->types);
      SetDpPp(state, {loadout->current_dp, loadout->max_dp, loadout->current_pp,
                      loadout->max_pp});
      SetCurrentStats(state, loadout->attributes, 1);
      SetBaseStats(state, loadout->attributes);
    }
  }
  state.subtype = equipment_subtype::kShield;
  state.item = static_cast<std::uint8_t>(shield);
  SetEnabledStatPages(ctx, stat_page::kAllEnabled);
}

// decomp: MAINMENU.PRG vs_mainMenu_setStatsFromArmor
// (src/MENU/MAINMENU.PRG/2D10.c:484)
void SetArmorStats(EquipmentDetailsContext& ctx, int armor) {
  auto& state = ctx.state;
  bool accessory = false;
  ResetEquipmentStats(state);
  if (armor != 0) {
    if (const auto loadout = ArmorLoadoutOf(ctx, armor)) {
      accessory = loadout->armor.category == kAccessoryCategory;
      CopyClassAffinityTotals(state, loadout->class_affinity);
      CopyTypes(state, loadout->types);
      SetDpPp(state, {loadout->current_dp, loadout->max_dp, 0, 0});
      SetCurrentStats(state, loadout->attributes, accessory ? 2 : 1);
      SetBaseStats(state, loadout->attributes);
    }
  }
  state.subtype =
      accessory ? equipment_subtype::kAccessory : equipment_subtype::kArmor;
  state.item = static_cast<std::uint8_t>(armor);
  SetEnabledStatPages(ctx, stat_page::kAllEnabled);
}

// decomp: MAINMENU.PRG vs_mainMenu_setStatsFromGem
// (src/MENU/MAINMENU.PRG/2D10.c:531)
void SetGemStats(EquipmentDetailsContext& ctx, int gem) {
  auto& state = ctx.state;
  const auto record = ctx.host.Rows().Gem(gem);
  if (!record) {
    ctx.host.Refuse("gem stats of a gem past the table");
    return;
  }
  CopyPartClassAffinity(state, record->classes, record->affinities);
  SetPrimaryStats(state,
                  {record->strength, record->intelligence, record->agility, 1});
  state.subtype = equipment_subtype::kGem;
  state.item = static_cast<std::uint8_t>(gem);
  SetEnabledStatPages(ctx,
                      stat_page::kClassEnabled | stat_page::kAffinityEnabled);
}

// decomp: MAINMENU.PRG _weaponStatusView (src/MENU/MAINMENU.PRG/2D10.c:556)
std::optional<TextRef> SelectWeaponDetailRow(EquipmentDetailsContext& ctx,
                                             int row) {
  auto description = NoGemsText(ctx);
  const std::uint8_t weapon_id = ctx.state.item;
  if (row >= 0 && row <= kWeaponLastGemRow) {
    if (const auto weapon = ctx.host.Rows().Weapon(weapon_id)) {
      if (const auto text = SelectWeaponComponent(ctx, *weapon, row)) {
        description = text;
      }
    } else {
      ctx.host.Refuse("detail row of a weapon past the table");
    }
  }
  ctx.state.subtype = equipment_subtype::kWeapon;
  ctx.state.item = weapon_id;
  ctx.host.SetRowSelected(row + kDetailMenuRowBase, true);
  return description;
}

// decomp: MAINMENU.PRG _bladeStatusView (src/MENU/MAINMENU.PRG/2D10.c:620)
std::optional<TextRef> BladeDetailDescription(EquipmentDetailsContext& ctx) {
  auto& rows = ctx.host.Rows();
  const auto blade = rows.Blade(ctx.state.item);
  if (!blade) {
    ctx.host.Refuse("detail description of a blade past the table");
    return std::nullopt;
  }
  return FormatBladeRow(rows, *blade, rows.StringBuffer()).description;
}

// decomp: MAINMENU.PRG _gripStatusView (src/MENU/MAINMENU.PRG/2D10.c:638)
std::optional<TextRef> GripDetailDescription(EquipmentDetailsContext& ctx) {
  auto& rows = ctx.host.Rows();
  const auto grip = rows.Grip(ctx.state.item);
  if (!grip) {
    ctx.host.Refuse("detail description of a grip past the table");
    return std::nullopt;
  }
  return FormatGripRow(rows, *grip, rows.StringBuffer()).description;
}

// decomp: MAINMENU.PRG _shieldStatusView (src/MENU/MAINMENU.PRG/2D10.c:656)
std::optional<TextRef> SelectShieldDetailRow(EquipmentDetailsContext& ctx,
                                             int row) {
  auto description = NoGemsText(ctx);
  const std::uint8_t shield_id = ctx.state.item;
  if (row >= 0 && row <= kShieldLastGemRow) {
    auto& rows = ctx.host.Rows();
    const auto shield = rows.Shield(shield_id);
    std::optional<TextRef> text;
    if (!shield) {
      ctx.host.Refuse("detail row of a shield past the table");
    } else if (row == 0) {
      text = FormatInventoryShieldRow(rows, *shield, rows.StringBuffer())
                 .description;
    } else {
      UpdateDpPpBars(ctx, dp_pp_bars::kStatic);
      text = SelectGemDetail(
          ctx,
          shield->gems[static_cast<std::size_t>(row - kShieldFirstGemRow)]);
    }
    if (text) {
      description = text;
    }
  }
  ctx.state.subtype = equipment_subtype::kShield;
  ctx.state.item = shield_id;
  ctx.host.SetRowSelected(row + kDetailMenuRowBase, true);
  return description;
}

// decomp: MAINMENU.PRG _armorStatusView (src/MENU/MAINMENU.PRG/2D10.c:703)
std::optional<TextRef> ArmorDetailDescription(EquipmentDetailsContext& ctx) {
  auto& rows = ctx.host.Rows();
  const auto armor = rows.Armor(ctx.state.item);
  if (!armor) {
    ctx.host.Refuse("detail description of an armor past the table");
    return std::nullopt;
  }
  return FormatInventoryArmorRow(rows, *armor, rows.StringBuffer()).description;
}

// decomp: MAINMENU.PRG _gemStatusView (src/MENU/MAINMENU.PRG/2D10.c:722)
std::optional<TextRef> GemDetailDescription(EquipmentDetailsContext& ctx) {
  auto& rows = ctx.host.Rows();
  const auto gem = rows.Gem(ctx.state.item);
  if (!gem) {
    ctx.host.Refuse("detail description of a gem past the table");
    return std::nullopt;
  }
  return FormatGemRow(rows, *gem, rows.StringBuffer()).description;
}

// decomp: MAINMENU.PRG vs_mainMenu_resetStatusViewCursor
// (src/MENU/MAINMENU.PRG/2D10.c:734)
void ResetItemDetailCursor(EquipmentDetailsState& state) {
  state.selected = kFirstDetailElement;
}

// decomp: MAINMENU.PRG _isUiElementReachable (src/MENU/MAINMENU.PRG/2D10.c:750)
bool IsItemDetailElementReachable(EquipmentDetailsContext& ctx, int element) {
  const auto& state = ctx.state;
  if (element >= kFirstDetailElement && element <= kLastComponentElement) {
    return IsComponentElementReachable(ctx, element);
  }
  if (element >= kFirstStatPageElement && element <= kLastStatPageElement) {
    constexpr int kLastFixedRow = 4;
    constexpr int kLastClassRow = 7;
    if (element <= kLastFixedRow) {
      return true;
    }
    return element <= kLastClassRow ? state.page != stat_page::kType
                                    : state.page == stat_page::kAffinity;
  }
  switch (element) {
    case 0:
      return (state.subtype & kGaugeElementsSubtypes) != 0;
    case kLastGaugeElement:
      return (state.subtype & kPpElementSubtypes) != 0;
    case kUnusedElement:
      return state.element15_enabled != 0;
    case kFirstRangeElement:
    case kFirstRangeElement + 1:
      return (state.subtype & kRangeRiskSubtypes) != 0;
    default:
      return element >= kFirstPrimaryElement &&
             element < static_cast<int>(kStatusViewElementCount);
  }
}

// decomp: MAINMENU.PRG vs_mainMenu_renderStatusView
// (src/MENU/MAINMENU.PRG/2D10.c:837)
void UpdateItemDetails(EquipmentDetailsContext& ctx) {
  auto& state = ctx.state;
  if (state.selected >= kStatusViewElementCount) {
    ctx.host.Refuse("item details with a selection past the elements");
    return;
  }
  const std::uint8_t original = state.selected;
  if (!SelectReachableElement(ctx, InputDirection(ctx.host.Input().repeat))) {
    return;
  }
  if (state.previous_item == state.item) {
    if (original != state.selected) {
      ctx.host.PlaySound(MenuSound::kChange);
    }
  } else {
    state.previous_item = state.item;
  }
  for (int row = kFirstComponentMenuRow; row <= kLastComponentMenuRow; ++row) {
    ctx.host.SetRowSelected(row, false);
  }
  RefreshSelectedItemStats(ctx);
  if (const auto help = ItemDetailHelp(ctx)) {
    ctx.host.SetInformationMessage(*help);
  } else {
    ctx.host.Refuse("item details without a help text");
  }
  DrawItemDetailCursor(ctx);
}

// decomp: MAINMENU.PRG _subtract (src/MENU/MAINMENU.PRG/C48.c:668)
std::uint16_t OffsetStatBarCoordinate(int panel_x, int scanline_offset,
                                      int edge_base_x) {
  return static_cast<std::uint16_t>(panel_x - (scanline_offset - edge_base_x));
}

// decomp: MAINMENU.PRG _renderStatPage (src/MENU/MAINMENU.PRG/C48.c:673)
void DrawEquipmentStatPage(EquipmentDetailsContext& ctx, int x_offset) {
  auto view = StatPageViewOf(ctx.state);
  if (!view) {
    ctx.host.Refuse("stat page past the type page");
    return;
  }
  auto& queue = ctx.host.SceneQueue(detail_layer::kStatPage);
  for (int y = 0; y < view->height; y += kStatRowPitch) {
    DrawStatRow(ctx, queue, *view, x_offset, y);
  }
  DrawStatPageTabs(ctx, x_offset);
}

// decomp: MAINMENU.PRG _getNextStatPage (src/MENU/MAINMENU.PRG/C48.c:902)
std::optional<std::uint8_t> NextEnabledStatPage(
    const EquipmentDetailsState& state, int page) {
  auto next = static_cast<std::uint32_t>(page);
  for (int tries = 0; tries < kStatPagesCycle; ++tries) {
    next = (next + 1) & kStatPageMask;
    if (((state.enabled_pages >> next) & 1U) != 0) {
      return static_cast<std::uint8_t>(next);
    }
  }
  return std::nullopt;
}

// decomp: MAINMENU.PRG vs_mainMenu_updateStatPage
// (src/MENU/MAINMENU.PRG/C48.c:911)
void SetEnabledStatPages(EquipmentDetailsContext& ctx, int enabled) {
  auto& state = ctx.state;
  constexpr std::uint32_t kShiftMask = 31;
  state.enabled_pages = static_cast<std::uint8_t>(enabled);
  if (((static_cast<std::uint32_t>(enabled) >> (state.page & kShiftMask)) &
       1U) != 0) {
    return;
  }
  if (const auto next = NextEnabledStatPage(state, state.page)) {
    state.page = *next;
  } else {
    ctx.host.Refuse("stat pages with none enabled");
  }
}

// decomp: MAINMENU.PRG vs_mainMenu_drawClassAffinityType
// (src/MENU/MAINMENU.PRG/C48.c:920)
void UpdateEquipmentStatPages(EquipmentDetailsContext& ctx, int command) {
  auto& state = ctx.state;
  if (command != 0) {
    if (command > 0) {
      state.page = stat_page::kNone;
      SetEnabledStatPages(ctx, command);
    }
    state.panel_direction = command;
    state.panel_slide = kPanelSlideSteps;
    return;
  }
  std::uint8_t page = state.page;
  int x_offset = 1;
  if (state.panel_direction > 0) {
    const auto steps = ctx.host.RowAnimationSteps();
    if (state.panel_slide >= steps.size()) {
      ctx.host.Refuse("stat page slide past the animation steps");
      return;
    }
    x_offset = -static_cast<int>(steps[state.panel_slide]);
    if (state.panel_slide != 0) {
      --state.panel_slide;
    } else if ((ctx.host.Input().pressed & kPadSquare) != 0) {
      const auto next = NextEnabledStatPage(state, page);
      if (!next) {
        ctx.host.Refuse("stat page turn with none enabled");
      } else if (*next != state.page) {
        page = *next;
        ctx.host.PlayPageSound();
      } else {
        ctx.host.PlaySound(MenuSound::kInvalid);
      }
    }
  } else if (state.panel_slide != 0) {
    --state.panel_slide;
    x_offset = (state.panel_slide - kPanelSlideSteps) * kPanelSlidePitch;
  }
  if (x_offset <= 0) {
    DrawEquipmentStatPage(ctx, x_offset);
  }
  state.page = page;
}

// decomp: MAINMENU.PRG vs_mainMenu_setRangeRisk
// (src/MENU/MAINMENU.PRG/C48.c:973)
void SetRangeAndRisk(EquipmentDetailsState& state,
                     const RangeRiskValues& range_risk) {
  for (std::size_t i = 0; i < range_risk.size(); ++i) {
    state.range_risk[i] = static_cast<std::uint8_t>(range_risk[i]);
  }
}

// decomp: MAINMENU.PRG vs_mainMenu_setStrIntAgi
// (src/MENU/MAINMENU.PRG/C48.c:981)
void SetPrimaryStats(EquipmentDetailsState& state,
                     const PrimaryStatValues& primary) {
  for (std::size_t i = 0; i < primary.size(); ++i) {
    state.primary[i] = static_cast<std::int16_t>(primary[i]);
  }
  SetRangeAndRisk(state, {0, 0, 0, 0});
  for (std::size_t i = 0; i < primary.size(); ++i) {
    state.primary[primary.size() + i] = state.primary[i];
  }
}

// decomp: MAINMENU.PRG vs_mainMenu_populateBaseStats
// (src/MENU/MAINMENU.PRG/C48.c:992)
void SetActorComparisonStats(EquipmentDetailsContext& ctx, int actor) {
  auto& state = ctx.state;
  const int id = actor & kActorMask;
  const auto stats = ctx.host.ActorStats(id);
  if (!stats) {
    ctx.host.Refuse("comparison stats of an absent actor");
    return;
  }
  state.comparison_deltas[kComparisonActor] = static_cast<std::int16_t>(id);
  for (std::size_t i = 0; i < kPrimaryStats; ++i) {
    state.comparison_accessory[i] = stats->accessory[i];
    state.comparison_deltas[i] = static_cast<std::int16_t>(stats->base[i]);
    state.comparison_totals[i] = static_cast<std::int16_t>(stats->totals[i]);
  }
  state.subtype = 0;
  if (actor >= kBattleActorCount) {
    return;
  }
  for (std::size_t i = 0; i < kPrimaryStats; ++i) {
    state.comparison_deltas[i] = static_cast<std::int16_t>(
        state.comparison_deltas[i] - state.comparison_totals[i]);
  }
  SetPrimaryStats(state, {stats->totals[0], stats->totals[1], stats->totals[2],
                          (stats->analyzed || actor == 0) ? 1 : 0});
  state.comparison_totals.fill(0);
}

// decomp: MAINMENU.PRG vs_mainMenu_renderEquipStats
// (src/MENU/MAINMENU.PRG/C48.c:1025)
void UpdatePrimaryStatPanel(EquipmentDetailsContext& ctx, int animate) {
  auto& state = ctx.state;
  if (animate != 0) {
    if (static_cast<int>(state.primary_animation) != animate) {
      state.primary_animation = static_cast<std::uint8_t>(animate);
      state.primary_slide = static_cast<std::uint8_t>(
          kPrimarySlideBase - animate * kPrimarySlidePerStep);
    }
    return;
  }
  const int known = state.primary[kPrimaryKnown];
  std::uint32_t position = kPrimaryPanelOrigin;
  if (state.primary_animation == 1) {
    if (state.primary_slide != 0) {
      const auto steps = ctx.host.RowAnimationSteps();
      if (state.primary_slide >= steps.size()) {
        ctx.host.Refuse("primary stat slide past the animation steps");
        return;
      }
      position += steps[state.primary_slide];
      --state.primary_slide;
    }
  } else if (state.primary_slide == 0) {
    return;
  } else {
    position +=
        static_cast<std::uint32_t>(kPrimarySlideEnd - state.primary_slide)
        << kPrimarySlideShift;
    --state.primary_slide;
  }
  if (state.range_risk[kRangeRiskShown] != 0) {
    DrawRangeRiskRows(ctx, position, known != 0);
  } else {
    position += kPrimaryPanelSkip;
    ctx.host.DrawCompactText("ORG/EQP", position + kPanelHeaderX, kStatTextGray,
                             detail_layer::kPrimaryPanel);
  }
  DrawPrimaryStatRows(ctx, position, known);
}

// decomp: MAINMENU.PRG vs_mainMenu_setDpPp (src/MENU/MAINMENU.PRG/C48.c:1126)
void SetDpPp(EquipmentDetailsState& state, const DpPpValues& dp_pp) {
  const auto [current_dp, max_dp, current_pp, max_pp] = dp_pp;
  state.current_pp = static_cast<std::uint16_t>(current_pp);
  state.max_pp = static_cast<std::uint16_t>(max_pp);
  state.current_dp =
      static_cast<std::uint16_t>((current_dp + kDpRounding) / kDpPercent);
  state.max_dp =
      static_cast<std::uint16_t>((max_dp + kDpRounding) / kDpPercent);
}

// decomp: MAINMENU.PRG vs_mainMenu_renderDpPpBars
// (src/MENU/MAINMENU.PRG/C48.c:1134)
void UpdateDpPpBars(EquipmentDetailsContext& ctx, int flags) {
  auto& state = ctx.state;
  auto& host = ctx.host;
  if (flags != 0) {
    state.bars_state = static_cast<std::uint8_t>(flags >> kBarsStateShift);
    if ((state.bars_state & 1) == 0) {
      state.bars_target = static_cast<std::uint8_t>(
          static_cast<std::uint32_t>(flags) & kBarsTargetMask);
    }
    return;
  }
  if (state.bars_state == kBarsSlideIn && state.bars_animation < kBarsShown) {
    ++state.bars_animation;
  } else if (state.bars_state == kBarsSlideOut && state.bars_animation != 0) {
    --state.bars_animation;
  } else if (state.bars_state == kBarsStatic) {
    state.bars_animation = kBarsShown;
  }
  if (state.bars_animation == 0) {
    return;
  }
  const std::array<bool, 2> shown{(state.bars_target & dp_pp_bars::kDp) != 0,
                                  (state.bars_target & dp_pp_bars::kPp) != 0};
  const std::array<std::uint32_t, 2> label_x{kDpLabelX, kPpLabelX};
  const std::array<std::uint32_t, 2> number_x{kDpNumberX, kPpNumberX};
  const std::array<int, 2> current{state.current_dp, state.current_pp};
  const std::array<int, 2> maximum{state.max_dp, state.max_pp};
  const std::array<const char*, 2> labels{"DP", "PP"};
  std::uint32_t y = Line(state.bars_animation * kBarsPitch - kBarsPitch);
  for (std::size_t bar = 0; bar < shown.size(); ++bar) {
    if (!shown[bar]) {
      continue;
    }
    host.DrawCompactText(labels[bar], (y + kBarsLabelLift) | label_x[bar],
                         kStatTextGray, detail_layer::kStatPage);
    const std::uint32_t end = host.DrawRightAlignedInteger(
        kMaxFont, y | number_x[bar], maximum[bar], detail_layer::kStatPage);
    host.DrawRightAlignedInteger(kSeparatorFont, end, 0,
                                 detail_layer::kStatPage);
    host.DrawRightAlignedInteger(kCurrentFont, end + kCurrentNumberOffset,
                                 current[bar], detail_layer::kStatPage);
  }
  y += Line(kBarsPitch);
  const std::array<int, 2> colors{kDpGaugeColor, kPpGaugeColor};
  for (std::size_t bar = 0; bar < shown.size(); ++bar) {
    if (shown[bar]) {
      const int divisor = std::max(maximum[bar], 1);
      host.DrawStatGauge(
          colors[bar],
          ((current[bar] << kGaugeWidthShift) + divisor - 1) / divisor,
          y | label_x[bar], detail_layer::kStatPage);
    }
  }
}
}  // namespace VS::Menu::Framework
