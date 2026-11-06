// Hand-maintained (tools/port/owned.csv).
#include "Menu/Setup/Assembly.hpp"  // NOLINT(build/include): typed API

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string_view>

#include "Items/Inventory.hpp"
#include "Render/EncodedText.hpp"

namespace VS::Menu::Setup {
namespace {
using Framework::MenuRow;
using Framework::PackXy;
using Framework::RowPlacement;
using Framework::RowState;
using Framework::RowText;
using Items::ItemCategory;

// The assembly screen's options: the blade, the grip, then the gems and
// "OK", option n on row 11 + n.
constexpr int kBladeOption = 0;
constexpr int kGripOption = 1;
constexpr int kFirstGemOption = 2;
constexpr int kGemOptionCount = 3;
constexpr int kOptionRowBase = 11;
constexpr int kOptionRowCount = 5;
// _popSelectionHistory's answer: 1 the blade, 2 the grip, from 3 a gem.
constexpr int kFirstGemPart = 3;
// The gem rows 13-15 fly out when the blade or the grip changes.
constexpr int kFirstGemRow = 13;
constexpr int kGemRowEnd = 16;
// _assembleBladeMenuHeader's argument: MENU12 entry 13 + text (bits 0-3),
// and for text 0 the blade (bits 4-9) and grip (bits 10-15) to show.
constexpr int kHeaderTextMask = 0xF;
constexpr int kHeaderPartMask = 0x3F;
constexpr int kHeaderBladeShift = 4;
constexpr int kHeaderGripShift = 10;
constexpr int kBladeHeader = 1;
constexpr int kGripHeader = 2;
constexpr int kGemHeader = 3;
constexpr int kFinishedHeader = 4;
constexpr RowPlacement kAssemblyHeader{
    .row = kHeaderRow,
    .x = 16,
    .y = 18,
    .w = 164,
    .background_width = 8,
};
// The blade and grip lists' argument: the current part (bits 0-6), the part
// it must fit (bits 8-15) and a flag keeping it nonzero.
constexpr int kPartOpenFlag = 0x80;
constexpr int kPartIdMask = 0x7F;
constexpr int kByteBits = 8;
// _attachGem's: its header row - 10 (bits 0-2) and the shift (bits 3-7)
// bringing the current gem to the low byte of the packed gems.
constexpr std::uint32_t kGemHeaderMask = 7;
constexpr std::uint32_t kGemShiftMask = 0xF8;
constexpr std::uint32_t kWordBits = 32;
constexpr int kGemShiftScale = 8;
constexpr int kBladeListHeader = 11;
constexpr int kGripListHeader = 12;
// The stats the lists and the screens show.
constexpr int kBladeNoStatPages = 3;
constexpr int kGripNoStatPages = 4;
constexpr int kGemNoStatPages = 3;
constexpr int kAllStatPages = 7;
constexpr int kHideStatPages = -1;
constexpr int kListDpPp = 8;
constexpr int kAssemblyDpPp = 11;
constexpr int kShowDpPp = 3;
constexpr int kHideDpPp = 4;
constexpr int kShowPanel = 1;
constexpr int kHidePanel = 2;
// The assembly rows: the weapon, the blade and the grip slide in from the
// right edge, the gems from x 334, all 16 apart from y 18.
constexpr RowPlacement kWeaponRow{
    .row = kDetailRow,
    .x = 320,
    .y = 18,
    .w = 165,
    .background_width = 0,
};
constexpr int kWeaponRowX = 155;
constexpr RowPlacement kBladeRow{
    .row = kOptionRowBase,
    .x = 327,
    .y = 34,
    .w = 158,
    .background_width = 0,
};
constexpr RowPlacement kGripRow{
    .row = kOptionRowBase + 1,
    .x = 327,
    .y = 50,
    .w = 158,
    .background_width = 0,
};
constexpr int kPartRowX = 162;
constexpr int kGemStartX = 334;
constexpr int kGemRowX = 169;
constexpr int kGemRowWidth = 151;
constexpr int kOffscreenX = 320;
constexpr int kRowTopY = 18;
constexpr int kRowPitch = 16;
constexpr int kGripIconBase = 10;
constexpr std::uint8_t kGemIcon = 22;
// "OK" follows the last gem row; the cursor rises 4 onto it.
constexpr int kOkLabelOffset = 2;
constexpr int kOkCursorRaise = 4;
constexpr std::uint8_t kBlinkStep = 16;
constexpr int kInformationRowState = 16;
// UpdateThreeChoicePrompt opened without a cancel row, and its answers.
constexpr int kNoCancelRow = 3;
constexpr int kAnswerYes = 1;
constexpr int kAnswerNo = 2;
constexpr int kAnswerCancel = 3;
// The gem screen: its equipment list, the equipment's part rows (1-5), its
// header, the gem rows' offset after a weapon's blade and grip, and the
// cursor.
constexpr int kGemListRowInfo = 0x19137;
constexpr int kComponentRows = 5;
constexpr RowPlacement kGemEquipmentRow{
    .row = kDetailRow,
    .x = 320,
    .y = 18,
    .w = 137,
    .background_width = 0,
};
constexpr int kGemEquipmentX = 155;
constexpr int kWeaponGemRowOffset = 2;
constexpr int kGemCursorX = 155;
constexpr int kGemCursorTopY = 26;
constexpr int kShieldTextStride = 2;
constexpr int kRowIconShift = 26;
constexpr int kMaterialShift = 16;
constexpr std::int32_t kMaterialMask = 7;
// _attachGems relinks the first three slots.
constexpr std::size_t kRestoredGemSlots = 3;
// _attachGemsTopMenu.
constexpr int kGemCategoryRowInfo = 566;
constexpr int kGemSubtitleRow = 11;
constexpr std::array<ItemCategory, 2> kGemCategories{
    ItemCategory::kWeapon,
    ItemCategory::kShield,
};

static_assert(Items::kWeaponSlots == Items::kShieldSlots);

enum AssemblyStep : std::uint8_t {
  kAssemblyTitle,
  kAssemblyRows,
  kAssemblyChoose,
  kAssemblyBlade,
  kAssemblyGrip,
  kAssemblyGem,
  kAssemblyAsk,
  kAssemblyAnswer,
  kAssemblyFinish,
};
// _attachGemsMenu's steps after the shared ones.
enum GemStep : std::uint8_t {
  kGemChoose = kScreenParts + 1,
  kGemSelect,
  kGemAsk,
  kGemAnswer,
};
// The part lists' steps.
enum PartStep : std::uint8_t { kPartList, kPartChoose, kPartChosen };

void RebuildWeaponOrder(Context& context) {
  const Items::ItemsStatus status = Items::Inventory::RebuildDisplayIndices(
      ItemCategory::kWeapon, context.host.Indices(), context.host.Inventory());
  if (status != Items::ItemsStatus::kOk) {
    context.host.Refuse(Items::Describe(status));
  }
}

// A record's gem slots, at most the four it holds.
std::size_t GemSlotCount(Context& context, int slots) {
  if (slots > static_cast<int>(Items::kShieldGemSlots)) {
    context.host.Refuse(
        std::format("{} gem slots past the four a record holds", slots));
    return Items::kShieldGemSlots;
  }
  return static_cast<std::size_t>(slots);
}

// Points 1-based gem `gem` at its owner (0 for none).
void SetGemOwner(Context& context, int gem, int owner) {
  Items::InventoryGem* record =
      InventorySlot(context, context.host.Inventory().gems, gem, "gem");
  if (record != nullptr) {
    record->set_item_index = static_cast<std::uint16_t>(owner);
  }
}

// Unlinks the attached gems and empties their slots.
void ClearGemLinks(Context& context, std::span<std::uint8_t> attached) {
  for (std::uint8_t& gem : attached) {
    if (gem != 0) {
      SetGemOwner(context, gem, 0);
      gem = 0;
    }
  }
}

// The gem slots of 1-based weapon or shield `reference`: the shield's or
// the weapon grip's; 0 when refused.
int EquipmentGemSlots(Context& context, bool is_shield, int reference) {
  Items::InventoryContents& inventory = context.host.Inventory();
  if (is_shield) {
    const Items::InventoryShield* shield =
        InventorySlot(context, inventory.shields, reference, "gem shield");
    return shield != nullptr ? shield->base.gem_slots : 0;
  }
  const Items::InventoryWeapon* weapon =
      InventorySlot(context, inventory.weapons, reference, "gem weapon");
  if (weapon == nullptr) {
    return 0;
  }
  const Items::InventoryGrip* grip =
      InventorySlot(context, inventory.grips, weapon->grip, "weapon grip");
  return grip != nullptr ? grip->gem_slots : 0;
}

// The combining weapon's or shield's gems; empty when refused.
std::span<std::uint8_t> AttachedGems(Context& context, bool is_shield) {
  Items::InventoryContents& inventory = context.host.Inventory();
  const int reference = context.state.combining_item + 1;
  if (is_shield) {
    Items::InventoryShield* shield =
        InventorySlot(context, inventory.shields, reference, "gem shield");
    return shield != nullptr ? std::span<std::uint8_t>(shield->gems)
                             : std::span<std::uint8_t>{};
  }
  Items::InventoryWeapon* weapon =
      InventorySlot(context, inventory.weapons, reference, "gem weapon");
  return weapon != nullptr ? std::span<std::uint8_t>(weapon->gems)
                           : std::span<std::uint8_t>{};
}

// Ashley takes the changed weapon or shield again when she holds it.
void ReequipCombining(Context& context, bool is_shield) {
  const auto slot = static_cast<std::size_t>(context.state.combining_item);
  if (slot >= Items::kWeaponSlots) {
    context.host.Refuse(
        std::format("equipment slot {} outside the table", slot));
    return;
  }
  const Items::InventoryContents& inventory = context.host.Inventory();
  if (is_shield) {
    if (inventory.shields[slot].is_equipped != 0) {
      context.host.EquipShield(slot);
    }
  } else if (inventory.weapons[slot].is_equipped != 0) {
    context.host.EquipWeapon(slot);
  }
}

// The row types the information box reads from `first`: an empty slot is
// unselectable, and so is the row after the last ("OK").
void MarkEmptyRows(Context& context, std::size_t first,
                   std::span<const std::uint8_t> slots) {
  const std::span<std::int32_t> types = context.host.RowTypes();
  if (types.size() <= first + slots.size()) {
    context.host.Refuse("row type buffer shorter than the screen's rows");
    return;
  }
  for (std::size_t i = 0; i < slots.size(); ++i) {
    types[first + i] = slots[i] == 0 ? 1 : 0;
  }
  types[first + slots.size()] = 1;
}

// _assembleMenu's and _attachGemsMenu's question: the stat panel hides and
// the prompt asks to attach.
void AskAttachment(Context& context, std::uint8_t& step) {
  context.host.UpdatePrimaryStatPanel(kHidePanel);
  UpdateThreeChoicePrompt(context, kNoCancelRow);
  context.host.SetInformationMessage(
      Menu12Text(context, menu12::kAttachmentConfirm));
  ++step;
}

void FlyOutGemRows(Context& context) {
  for (int row = kFirstGemRow; row < kGemRowEnd; ++row) {
    context.host.FlyOutRowRight(row);
  }
}

void AssembleStaged(Context& context) {
  const AssemblyState& assembly = context.state.assembly;
  AssembleWeapon(context, assembly.blade - 1, assembly.grip - 1, assembly.gems);
}

// _attachGem's argument: the header row - 10, gem `gem`'s shift (8 a gem
// from 1) and the packed gems from bit 8.
int GemListParams(int header, int gem, const GemIds& gems) {
  const std::uint32_t packed =
      static_cast<std::uint32_t>(header) |
      (static_cast<std::uint32_t>(gem) * kGemShiftScale) |
      (static_cast<std::uint32_t>(PackGems(gems)) << kByteBits);
  return static_cast<int>(packed);
}

// The three part lists' differences.
struct BladePart {
  using Record = Items::InventoryBlade;
  static constexpr std::size_t kPlaceholderName = menu12::kBlades;
  static constexpr std::size_t kPlaceholderText = menu12::kBladesDesc;
  static constexpr std::string_view kName = "listed blade";

  static PartSelectionState& Selection(State& state) {
    return state.blade_selection;
  }
  static auto& Table(Items::InventoryContents& inventory) {
    return inventory.blades;
  }
  static std::span<const std::uint8_t> Order(
      const Items::InventoryIndices& order) {
    return order.blades;
  }
  static bool InUse(const Record& part) {
    return part.assembled_weapon_index != 0;
  }
  static Framework::ItemRowText Format(Context& context, const Record& part,
                                       std::span<std::uint8_t> buffer) {
    return Framework::FormatBladeRow(context.host.ItemRows(), part, buffer);
  }
  static void Open(Context& context, PartSelectionState& selection,
                   int params) {
    selection.current = static_cast<std::uint8_t>(params & kPartIdMask);
    selection.partner = static_cast<std::uint8_t>(params >> kByteBits);
    AnimateItemHeader(context, kBladeListHeader);
  }
  // A blade must fit the grip already chosen.
  static std::optional<std::size_t> Invalid(Context& context,
                                            const PartSelectionState& selection,
                                            const Record& part) {
    const Items::InventoryGrip* grip =
        selection.partner != 0
            ? InventorySlot(context, context.host.Inventory().grips,
                            selection.partner, "chosen grip")
            : nullptr;
    if (IsGripCompatible(context, &part, grip)) {
      return std::nullopt;
    }
    return menu12::kInvalidBlade;
  }
  static void ShowListed(Context& context, int id) {
    if (id != 0) {
      context.host.SetBladeStats(id);
      return;
    }
    context.host.SetEnabledStatPages(kBladeNoStatPages);
    context.host.ResetEquipmentStats();
    context.host.SetRangeAndRisk(0, 0, 0, 1);
  }
};

struct GripPart {
  using Record = Items::InventoryGrip;
  static constexpr std::size_t kPlaceholderName = menu12::kGrips;
  static constexpr std::size_t kPlaceholderText = menu12::kGripsDesc;
  static constexpr std::string_view kName = "listed grip";

  static PartSelectionState& Selection(State& state) {
    return state.grip_selection;
  }
  static auto& Table(Items::InventoryContents& inventory) {
    return inventory.grips;
  }
  static std::span<const std::uint8_t> Order(
      const Items::InventoryIndices& order) {
    return order.grips;
  }
  static bool InUse(const Record& part) {
    return part.assembled_weapon_index != 0;
  }
  static Framework::ItemRowText Format(Context& context, const Record& part,
                                       std::span<std::uint8_t> buffer) {
    return Framework::FormatGripRow(context.host.ItemRows(), part, buffer);
  }
  static void Open(Context& context, PartSelectionState& selection,
                   int params) {
    selection.current = static_cast<std::uint8_t>(params & kPartIdMask);
    selection.partner = static_cast<std::uint8_t>(params >> kByteBits);
    AnimateItemHeader(context, kGripListHeader);
  }
  // A grip must hold the blade already chosen.
  static std::optional<std::size_t> Invalid(Context& context,
                                            const PartSelectionState& selection,
                                            const Record& part) {
    const Items::InventoryBlade* blade =
        selection.partner != 0
            ? InventorySlot(context, context.host.Inventory().blades,
                            selection.partner, "chosen blade")
            : nullptr;
    if (IsGripCompatible(context, blade, &part)) {
      return std::nullopt;
    }
    return menu12::kInvalidGrip;
  }
  static void ShowListed(Context& context, int id) {
    context.host.UpdateDpPpBars(kListDpPp);
    if (id != 0) {
      context.host.SetGripStats(id);
      return;
    }
    context.host.SetEnabledStatPages(kGripNoStatPages);
    context.host.ResetEquipmentStats();
  }
};

struct GemPart {
  using Record = Items::InventoryGem;
  static constexpr std::size_t kPlaceholderName = menu12::kGems;
  static constexpr std::size_t kPlaceholderText = menu12::kGemsDesc;
  static constexpr std::string_view kName = "listed gem";

  static PartSelectionState& Selection(State& state) {
    return state.gem_selection;
  }
  static auto& Table(Items::InventoryContents& inventory) {
    return inventory.gems;
  }
  static std::span<const std::uint8_t> Order(
      const Items::InventoryIndices& order) {
    return order.gems;
  }
  static bool InUse(const Record& part) { return part.set_item_index != 0; }
  static Framework::ItemRowText Format(Context& context, const Record& part,
                                       std::span<std::uint8_t> buffer) {
    return Framework::FormatGemRow(context.host.ItemRows(), part, buffer);
  }
  static void Open(Context& context, PartSelectionState& selection,
                   int params) {
    const auto bits = static_cast<std::uint32_t>(params);
    const std::uint32_t shift = bits & kGemShiftMask;
    if (shift >= kWordBits) {
      context.host.Refuse(
          std::format("gem list shift {} past the packed gems", shift));
      selection.current = 0;
    } else {
      selection.current = static_cast<std::uint8_t>(bits >> shift);
    }
    AnimateItemHeader(context,
                      static_cast<int>(bits & kGemHeaderMask) + kDetailRow);
  }
  // Any free gem fits.
  static std::optional<std::size_t> Invalid(
      Context& /*context*/, const PartSelectionState& /*selection*/,
      const Record& /*part*/) {
    return std::nullopt;
  }
  static void ShowListed(Context& context, int id) {
    context.host.UpdateDpPpBars(kListDpPp);
    if (id != 0) {
      context.host.SetGemStats(id);
      return;
    }
    context.host.SetEnabledStatPages(kGemNoStatPages);
    context.host.ResetEquipmentStats();
  }
};

// Row 0 shows the current part (or the placeholder), then every free part
// in display order, those that do not fit unselectable with the reason.
template <typename Part>
void ListParts(Context& context, const PartSelectionState& selection) {
  auto& parts = Part::Table(context.host.Inventory());
  PartListRows rows;
  const typename Part::Record* shown =
      selection.current != 0
          ? InventorySlot(context, parts, selection.current, Part::kName)
          : nullptr;
  if (shown != nullptr) {
    AddPartRow(context, rows,
               Part::Format(context, *shown, NextPartBuffer(rows)),
               selection.current);
  } else {
    AddPlaceholderRow(context, rows, Part::kPlaceholderName,
                      Part::kPlaceholderText);
  }
  for (const std::uint8_t part_id : Part::Order(context.host.Indices())) {
    if (part_id == 0 || part_id == selection.current) {
      continue;
    }
    const typename Part::Record* part =
        InventorySlot(context, parts, part_id, Part::kName);
    if (part == nullptr || Part::InUse(*part)) {
      continue;
    }
    const std::span<std::uint8_t> text_buffer = NextPartBuffer(rows);
    if (text_buffer.empty()) {
      context.host.Refuse("part list full");
      break;
    }
    AddPartRow(context, rows, Part::Format(context, *part, text_buffer),
               part_id);
    if (const std::optional<std::size_t> reason =
            Part::Invalid(context, selection, *part)) {
      MarkLastPartInvalid(context, rows, *reason);
    }
  }
  ShowPartList(context, rows);
}

// _assembleBladeMenu, _assembleGripMenu and _attachGem: an argument opens
// the list; then 0 while choosing, the chosen 1-based part (or < 0) once
// the rows settle, or -2 when the list was left for the field.
template <typename Part>
int SelectPart(Context& context, int params) {
  PartSelectionState& selection = Part::Selection(context.state);
  if (params != 0) {
    Part::Open(context, selection, params);
    selection.step = kPartList;
    return 0;
  }
  if (selection.step == kPartList) {
    if (context.host.RowsIdle()) {
      ListParts<Part>(context, selection);
      selection.step = kPartChoose;
    }
    return 0;
  }
  if (selection.step == kPartChoose) {
    Part::ShowListed(context, ListedPart(context));
    return ChoosePart(context, selection.selected_row, selection.step);
  }
  if (selection.step == kPartChosen && context.host.RowsIdle()) {
    return selection.selected_row;
  }
  return 0;
}

// The staged blade and grip and the grip's gem slots once both are chosen.
struct AssemblyParts {
  const Items::InventoryBlade* blade = nullptr;
  const Items::InventoryGrip* grip = nullptr;
  bool ready = false;
  int gem_slots = 0;
};

AssemblyParts StagedParts(Context& context) {
  const AssemblyState& assembly = context.state.assembly;
  Items::InventoryContents& inventory = context.host.Inventory();
  AssemblyParts parts;
  if (assembly.blade != 0) {
    parts.blade = InventorySlot(context, inventory.blades, assembly.blade,
                                "staged blade");
  }
  if (assembly.grip != 0) {
    parts.grip =
        InventorySlot(context, inventory.grips, assembly.grip, "staged grip");
  }
  parts.ready = parts.blade != nullptr && parts.grip != nullptr;
  if (parts.ready) {
    parts.gem_slots =
        static_cast<int>(GemSlotCount(context, parts.grip->gem_slots));
  }
  return parts;
}

void StartAssembly(Context& context) {
  context.host.InitInformationTextBox(1);
  RecordSlotSelection(context, static_cast<int>(kSlotHistoryCount));
  context.host.ClearMenuExcept(kNoRow);
  const std::size_t slot = Items::Inventory::FirstEmptySlot(
      ItemCategory::kWeapon, context.host.Inventory());
  if (slot >= Items::kWeaponSlots) {
    context.host.Refuse("no empty weapon slot to assemble into");
  }
  context.state.combining_item = static_cast<std::uint8_t>(slot);
  AssemblyState& assembly = context.state.assembly;
  assembly.blade = 0;
  assembly.grip = 0;
  assembly.gems = {};
  assembly.selected_option = kBladeOption;
  assembly.step = kAssemblyTitle;
}

// Step 1: the weapon, blade, grip and gem rows slide in.
void ShowAssemblyRows(Context& context, const AssemblyParts& parts) {
  MenuRow& weapon = context.host.SetMenuItem(
      kWeaponRow, Menu12Text(context, menu12::kWeapon));
  weapon.state = RowState::kSlideX;
  weapon.selected = 1;
  weapon.target0 = kWeaponRowX;
  MenuRow& blade = context.host.SetMenuItem(
      kBladeRow, parts.blade != nullptr ? ItemName(context, parts.blade->id)
                                        : Menu12Text(context, menu12::kBlades));
  blade.state = RowState::kSlideX;
  blade.target0 = kPartRowX;
  if (parts.blade != nullptr) {
    blade.row_icon = parts.blade->category;
    blade.material = static_cast<std::uint8_t>(parts.blade->material);
  } else {
    blade.font_color = 1;
  }
  MenuRow& grip = context.host.SetMenuItem(
      kGripRow, parts.grip != nullptr ? ItemName(context, parts.grip->id)
                                      : Menu12Text(context, menu12::kGrips));
  grip.state = RowState::kSlideX;
  grip.font_color = parts.grip == nullptr ? 1 : 0;
  grip.target0 = kPartRowX;
  if (parts.grip != nullptr) {
    grip.row_icon =
        static_cast<std::uint8_t>(parts.grip->category + kGripIconBase);
  }
  const AssemblyState& assembly = context.state.assembly;
  for (int i = 0; i < parts.gem_slots; ++i) {
    const int gem_id = assembly.gems[static_cast<std::size_t>(i)];
    const Items::InventoryGem* gem =
        gem_id != 0 ? InventorySlot(context, context.host.Inventory().gems,
                                    gem_id, "staged gem")
                    : nullptr;
    MenuRow& row = context.host.SetMenuItem(
        {.row = kFirstGemRow + i,
         .x = kGemStartX,
         .y = ((i + kFirstGemPart) * kRowPitch) + kRowTopY,
         .w = kGemRowWidth,
         .background_width = 0},
        gem != nullptr ? ItemName(context, gem->id)
                       : Menu12Text(context, menu12::kGems));
    row.state = RowState::kSlideX;
    row.target0 = kGemRowX;
    if (gem != nullptr) {
      row.row_icon = kGemIcon;
    } else {
      row.font_color = 1;
    }
  }
}

// Cross: the last chosen part goes back (or, with none, the screen
// closes).
int ReturnAssemblyPart(Context& context) {
  AssemblyState& assembly = context.state.assembly;
  const int part = PopSlotSelection(context, kSlotHistoryCount);
  DisassembleWeapon(context, context.state.combining_item);
  if (part == 0) {
    return CancelAssembly(context);
  }
  std::size_t placeholder = menu12::kGems;
  if (part < kFirstGemPart) {
    (part == kBladeOption + 1 ? assembly.blade : assembly.grip) = 0;
    assembly.gems = {};
    FlyOutGemRows(context);
    placeholder = part == kBladeOption + 1 ? menu12::kBlades : menu12::kGrips;
  } else {
    assembly.gems[static_cast<std::size_t>(part - kFirstGemPart)] = 0;
    AssembleStaged(context);
  }
  context.host.PlaySound(MenuSound::kLeave);
  assembly.selected_option = static_cast<std::uint8_t>(part - 1);
  MenuRow& row = context.host.SetMenuItem(
      {.row = part + kDetailRow,
       .x = part < kFirstGemPart ? kPartRowX : kGemRowX,
       .y = (part * kRowPitch) + kRowTopY,
       .w = kGemRowWidth,
       .background_width = 0},
      Menu12Text(context, placeholder));
  row.selected = 1;
  row.font_color = 1;
  SetAssemblyStats(context, assembly.blade, assembly.grip);
  return 0;
}

// Circle on a part: its list opens.
void OpenAssemblyPart(Context& context) {
  AssemblyState& assembly = context.state.assembly;
  const int option = assembly.selected_option;
  context.host.ClearMenuExcept(kNoRow);
  if (option == kBladeOption) {
    SetAssemblyHeader(context, kBladeHeader);
    SelectAssemblyBlade(
        context, assembly.blade + (assembly.grip << kByteBits) + kPartOpenFlag);
    assembly.step = kAssemblyBlade;
  } else if (option == kGripOption) {
    SetAssemblyHeader(context, kGripHeader);
    SelectAssemblyGrip(
        context, assembly.grip + (assembly.blade << kByteBits) + kPartOpenFlag);
    assembly.step = kAssemblyGrip;
  } else {
    SetAssemblyHeader(context, kGemHeader);
    SelectGem(context, GemListParams(option + 1, option - 1, assembly.gems));
    assembly.step = kAssemblyGem;
  }
}

// The selected part's row, whose description replaces the prompt; nothing
// on an empty option or past the gems.
std::optional<Framework::ItemRowText> SelectedPartRow(
    Context& context, std::span<std::uint8_t> buffer) {
  const AssemblyState& assembly = context.state.assembly;
  Items::InventoryContents& inventory = context.host.Inventory();
  const int option = assembly.selected_option;
  if (option == kBladeOption && assembly.blade != 0) {
    if (const Items::InventoryBlade* blade = InventorySlot(
            context, inventory.blades, assembly.blade, "staged blade")) {
      return Framework::FormatBladeRow(context.host.ItemRows(), *blade, buffer);
    }
  } else if (option == kGripOption && assembly.grip != 0) {
    if (const Items::InventoryGrip* grip = InventorySlot(
            context, inventory.grips, assembly.grip, "staged grip")) {
      return Framework::FormatGripRow(context.host.ItemRows(), *grip, buffer);
    }
  } else if (option >= kFirstGemOption &&
             option < kFirstGemOption + kGemOptionCount) {
    const int gem_id =
        assembly.gems[static_cast<std::size_t>(option - kFirstGemOption)];
    const Items::InventoryGem* gem =
        gem_id != 0
            ? InventorySlot(context, inventory.gems, gem_id, "staged gem")
            : nullptr;
    if (gem != nullptr) {
      return Framework::FormatGemRow(context.host.ItemRows(), *gem, buffer);
    }
  }
  return std::nullopt;
}

void ShowAssemblyMessage(Context& context, int gem_slots) {
  const int option = context.state.assembly.selected_option;
  int entry = option + menu12::kSelectBladeToAssembleIndex;
  if (option >= kFirstGemOption) {
    entry = option == gem_slots + kFirstGemOption ? menu12::kPressOkIndex
                                                  : menu12::kSelectGemIndex;
  }
  std::span<const std::uint8_t> message = Menu12Entry(context, entry);
  const std::span<std::uint8_t> buffer = context.host.StringBuffer();
  const std::optional<Framework::ItemRowText> row =
      SelectedPartRow(context, buffer);
  if (row && row->description) {
    message = RowTextBytes(context, row->description, buffer);
  }
  context.host.SetInformationMessage(message);
}

void DrawAssemblyCursor(Context& context, int gem_slots) {
  AssemblyState& assembly = context.state.assembly;
  const std::size_t entry = assembly.selected_option + std::size_t{1};
  if (entry >= kAssemblyCursorPositions.size()) {
    context.host.Refuse(std::format("assembly option {} past the cursor table",
                                    assembly.selected_option));
    return;
  }
  std::uint32_t position = kAssemblyCursorPositions[entry];
  if (assembly.selected_option == gem_slots + kFirstGemOption) {
    position -= PackXy(0, kOkCursorRaise);
  }
  assembly.cursor = static_cast<std::uint8_t>(
      context.host.DrawCursor(assembly.cursor, position));
}

// Up and down move over the parts, the gems and (once ready) "OK".
void MoveAssemblyOption(Context& context, const AssemblyParts& parts,
                        const Framework::Buttons& buttons) {
  AssemblyState& assembly = context.state.assembly;
  const int last = parts.gem_slots + (parts.ready ? 1 : 0) + 1;
  int option = assembly.selected_option;
  if (Repeated(buttons, pad::kUp)) {
    option = option == 0 ? last : option - 1;
  }
  if (Repeated(buttons, pad::kDown)) {
    option = option == last ? 0 : option + 1;
  }
  if (option != assembly.selected_option) {
    context.host.PlaySound(MenuSound::kChange);
    assembly.selected_option = static_cast<std::uint8_t>(option);
  }
  for (int row = 0; row < kOptionRowCount; ++row) {
    context.host.MenuItem(row + kOptionRowBase).selected =
        row == assembly.selected_option ? 1 : 0;
  }
  // The blade and the grip first (the third row's mark gives way to the
  // gems'), then the gem slots and "OK".
  const std::array<std::uint8_t, 2> chosen{assembly.blade, assembly.grip};
  MarkEmptyRows(context, 0, chosen);
  MarkEmptyRows(context, kFirstGemOption,
                std::span(assembly.gems)
                    .first(static_cast<std::size_t>(parts.gem_slots)));
  context.host.UpdateInformationVisibility(assembly.selected_option,
                                           kInformationRowState);
  ShowAssemblyMessage(context, parts.gem_slots);
  DrawAssemblyCursor(context, parts.gem_slots);
}

// Step 2: the rows take the pad.
int RunAssemblyRows(Context& context, const AssemblyParts& parts) {
  AssemblyState& assembly = context.state.assembly;
  if (!context.host.RowsIdle()) {
    assembly.ok_blink = 0;
    return 0;
  }
  const Framework::Buttons buttons = context.host.Input();
  if (Pressed(buttons, pad::kTriangle)) {
    return CancelAssembly(context);
  }
  if (parts.ready) {
    AssembleStaged(context);
    DrawConfirmationLabel(context, parts.gem_slots + kOkLabelOffset,
                          assembly.ok_blink);
  }
  if (Pressed(buttons, pad::kCross)) {
    return ReturnAssemblyPart(context);
  }
  const int ok_option = parts.gem_slots + kFirstGemOption;
  if (Pressed(buttons, pad::kStart) && parts.ready) {
    context.host.PlaySound(MenuSound::kSelect);
    context.host.MenuItem(assembly.selected_option + kOptionRowBase).selected =
        0;
    assembly.selected_option = static_cast<std::uint8_t>(ok_option);
    assembly.step = kAssemblyAsk;
    return 0;
  }
  if (Pressed(buttons, pad::kCircle)) {
    context.host.PlaySound(MenuSound::kSelect);
    if (assembly.selected_option != ok_option) {
      OpenAssemblyPart(context);
      return 0;
    }
    assembly.step = kAssemblyAsk;
  }
  MoveAssemblyOption(context, parts, buttons);
  return 0;
}

void ReturnToAssemblyRows(Context& context) {
  AssemblyState& assembly = context.state.assembly;
  SetAssemblyHeader(context, (assembly.blade << kHeaderBladeShift) |
                                 (assembly.grip << kHeaderGripShift));
  assembly.step = kAssemblyRows;
}

// Steps 3 and 4: the blade or grip list's choice. Choosing the current part
// takes it off; another replaces it and the staged gems.
int TakeChosenPart(Context& context, bool is_blade) {
  AssemblyState& assembly = context.state.assembly;
  const int chosen = is_blade ? SelectAssemblyBlade(context, 0)
                              : SelectAssemblyGrip(context, 0);
  if (chosen == 0) {
    return 0;
  }
  if (chosen == -2) {
    return CancelAssembly(context);
  }
  if (chosen > 0) {
    std::uint8_t& part = is_blade ? assembly.blade : assembly.grip;
    const std::uint8_t other = is_blade ? assembly.grip : assembly.blade;
    const int history = is_blade ? kBladeOption : kGripOption;
    assembly.gems = {};
    FlyOutGemRows(context);
    if (chosen == part) {
      part = 0;
      PopSlotSelection(context, history);
      DisassembleWeapon(context, context.state.combining_item);
    } else {
      part = static_cast<std::uint8_t>(chosen);
      RecordSlotSelection(context, history);
      if (is_blade) {
        assembly.selected_option = kGripOption;
      } else {
        assembly.selected_option = assembly.blade != 0 ? kFirstGemOption : 0;
      }
      if (other != 0) {
        AssembleStaged(context);
      }
    }
  }
  ReturnToAssemblyRows(context);
  return 0;
}

// Step 5: the gem list's choice. Choosing the slot's gem takes it off;
// another goes in and the cursor moves on.
int TakeChosenGem(Context& context) {
  AssemblyState& assembly = context.state.assembly;
  const int chosen = SelectGem(context, 0);
  if (chosen == 0) {
    return 0;
  }
  if (chosen == -2) {
    return CancelAssembly(context);
  }
  if (chosen > 0) {
    const int option = assembly.selected_option;
    const auto gem = static_cast<std::size_t>(option - kFirstGemOption);
    if (option < kFirstGemOption || gem >= assembly.gems.size()) {
      context.host.Refuse(
          std::format("assembly option {} is not a gem slot", option));
    } else if (chosen == assembly.gems[gem]) {
      assembly.gems[gem] = 0;
      PopSlotSelection(context, option);
    } else {
      assembly.gems[gem] = static_cast<std::uint8_t>(chosen);
      RecordSlotSelection(context, option);
      ++assembly.selected_option;
    }
  }
  AssembleStaged(context);
  ReturnToAssemblyRows(context);
  return 0;
}

// Step 7: yes keeps the weapon, no returns to the rows, cancel takes it
// apart.
int AnswerAssembly(Context& context) {
  AssemblyState& assembly = context.state.assembly;
  switch (UpdateThreeChoicePrompt(context, 0)) {
    case kAnswerYes:
      context.host.PlaySound(MenuSound::kSelect);
      context.host.ClearMenuExcept(kDetailRow);
      context.host.UpdateEquipmentStatPages(kHideStatPages);
      context.host.UpdateDpPpBars(kHideDpPp);
      assembly.step = kAssemblyFinish;
      break;
    case kAnswerNo:
      context.host.UpdatePrimaryStatPanel(kShowPanel);
      assembly.step = kAssemblyChoose;
      break;
    case kAnswerCancel:
      return CancelAssembly(context);
    default:
      break;
  }
  return 0;
}

// Step 8: the new weapon goes to the menu that names it.
int FinishAssembly(Context& context) {
  if (!context.host.RowsIdle()) {
    return 0;
  }
  context.host.SetCursorColor(0);
  const std::span<std::uint8_t> buffer = context.host.StringBuffer();
  if (buffer.size() >= 2) {
    buffer[0] = 0;
    buffer[1] = context.state.combining_item;
  }
  SetAssemblyHeader(context, kFinishedHeader);
  context.host.SetMenuCommandLabel();
  return 1;
}

// _attachGemsMenu's list: each weapon or shield with its gem slot count.
void ListGemEquipment(Context& context, bool is_shield) {
  EquipmentRows rows;
  const Items::InventoryIndices& indices = context.host.Indices();
  const std::span<const std::uint8_t> order =
      is_shield ? std::span<const std::uint8_t>(indices.shields)
                : std::span<const std::uint8_t>(indices.weapons);
  for (const std::uint8_t equipment : order) {
    if (equipment == 0 ||
        !CheckReference(context, equipment, kEquipmentListCapacity,
                        "gem equipment")) {
      continue;
    }
    const std::size_t row = rows.count;
    const std::span<std::uint8_t> description = rows.descriptions[row];
    FormatEquipmentRow(context, rows, is_shield,
                       static_cast<std::size_t>(equipment - 1), description);
    const int slots = EquipmentGemSlots(context, is_shield, equipment);
    context.host.SetTextInteger(0, slots);
    const int entry = (is_shield ? kShieldTextStride : 0) +
                      (slots != 0 ? menu12::kWeaponGemSlotsIndex
                                  : menu12::kWeaponNoGemSlotsIndex);
    if (Render::FormatEncodedText(description, Menu12Entry(context, entry),
                                  context.host.ItemRows().TextArguments())) {
      rows.texts[row].description = description;
    } else {
      context.host.Refuse("gem slot text did not format");
    }
    rows.types[row] |= slots == 0 ? 1 : 0;
    ++rows.count;
    context.state.available_items[rows.count] = equipment;
  }
  context.host.CreateMenuRows(kGemListRowInfo + (is_shield ? 1 : 0),
                              std::span(rows.texts).first(rows.count),
                              std::span(rows.types).first(rows.count));
}

// Step 3: the chosen equipment settles into the header and its gems are
// staged.
void SettleGemEquipment(Context& context) {
  GemAttachmentState& attachment = context.state.gem_attachment;
  const bool is_shield = attachment.screen.is_shield;
  const int id = SettleEquipmentRow(context, attachment.screen,
                                    is_shield ? kShieldIcon : kWeaponIcon);
  if (is_shield) {
    context.host.SetShieldStats(id);
  } else {
    context.host.SetWeaponStats(id);
  }
  attachment.gem_slots = static_cast<std::uint8_t>(
      GemSlotCount(context, EquipmentGemSlots(context, is_shield, id)));
  context.host.UpdateEquipmentStatPages(kAllStatPages);
  context.host.UpdateDpPpBars(kShowDpPp);
  context.host.UpdatePrimaryStatPanel(kShowPanel);
  attachment.staged = {};
  const std::span<const std::uint8_t> attached =
      AttachedGems(context, is_shield);
  std::copy_n(attached.begin(),
              std::min<std::size_t>(attachment.gem_slots, attached.size()),
              attachment.staged.begin());
  attachment.screen.appear_step = 0;
  attachment.selected_slot = 0;
  attachment.screen.step = kScreenParts;
  attachment.target = attachment.staged;
}

// Step 4: the equipment's part rows appear one a frame; an empty gem slot
// gets the "gems" row.
void ShowGemRows(Context& context) {
  EquipmentScreenState& screen = context.state.gem_attachment.screen;
  if (screen.appear_step >= kComponentRows) {
    screen.step = kGemChoose;
    return;
  }
  ++screen.appear_step;
  const int reference = context.state.combining_item + 1;
  const MenuRow* row =
      screen.is_shield
          ? Framework::CreateShieldGemRow(context.host.ItemRows(),
                                          screen.appear_step, reference, true)
          : Framework::CreateWeaponComponentRow(
                context.host.ItemRows(), screen.appear_step, reference, true);
  if (row != nullptr && row->unselectable != 0) {
    CreateEmptyGemSlotRow(context, screen.appear_step);
  }
}

int GemRowOffset(bool is_shield) { return is_shield ? 0 : kWeaponGemRowOffset; }

void ShowGemMessage(Context& context) {
  const GemAttachmentState& attachment = context.state.gem_attachment;
  std::array<std::uint8_t, kRowTextBytes> buffer{};
  std::span<const std::uint8_t> message =
      Menu12Text(context, menu12::kSelectGem);
  const std::size_t slot = attachment.selected_slot;
  if (slot == attachment.gem_slots) {
    message = Menu12Text(context, menu12::kPressOk);
  } else if (slot < attachment.staged.size() && attachment.staged[slot] != 0) {
    const Items::InventoryGem* gem =
        InventorySlot(context, context.host.Inventory().gems,
                      attachment.staged[slot], "staged gem");
    if (gem != nullptr) {
      const Framework::ItemRowText row =
          Framework::FormatGemRow(context.host.ItemRows(), *gem, buffer);
      if (row.description) {
        message = RowTextBytes(context, row.description, buffer);
      }
    }
  }
  context.host.SetInformationMessage(message);
}

// Up and down move over the gem slots and "OK".
void MoveGemSlot(Context& context, const Framework::Buttons& buttons) {
  GemAttachmentState& attachment = context.state.gem_attachment;
  const int last = attachment.gem_slots;
  int slot = attachment.selected_slot;
  if (Repeated(buttons, pad::kUp)) {
    slot = slot == 0 ? last : slot - 1;
  }
  if (Repeated(buttons, pad::kDown)) {
    slot = slot == last ? 0 : slot + 1;
  }
  if (slot != attachment.selected_slot) {
    context.host.PlaySound(MenuSound::kChange);
    attachment.selected_slot = static_cast<std::uint8_t>(slot);
  }
  const int offset = GemRowOffset(attachment.screen.is_shield);
  for (int i = 0; i < last; ++i) {
    context.host.MenuItem(kOptionRowBase + offset + i).selected =
        i == attachment.selected_slot ? 1 : 0;
  }
  MarkEmptyRows(
      context, 0,
      std::span(attachment.staged).first(static_cast<std::size_t>(last)));
  context.host.UpdateInformationVisibility(attachment.selected_slot,
                                           kInformationRowState);
  ShowGemMessage(context);
  int y = ((offset + attachment.selected_slot) * kRowPitch) + kGemCursorTopY;
  if (attachment.selected_slot == last) {
    y -= kOkCursorRaise;
  }
  attachment.cursor = static_cast<std::uint8_t>(
      context.host.DrawCursor(attachment.cursor, PackXy(kGemCursorX, y)));
}

// Step 5: the gem rows take the pad.
int RunGemRows(Context& context) {
  GemAttachmentState& attachment = context.state.gem_attachment;
  EquipmentScreenState& screen = attachment.screen;
  if (!context.host.RowsIdle()) {
    attachment.ok_blink = 0;
    return 0;
  }
  const Framework::Buttons buttons = context.host.Input();
  if (Pressed(buttons, pad::kTriangle)) {
    return RestoreGemsAndLeaveSelection(context, attachment.target,
                                        screen.is_shield);
  }
  const int offset = GemRowOffset(screen.is_shield);
  attachment.ok_blink =
      static_cast<std::uint8_t>(attachment.ok_blink + kBlinkStep);
  DrawConfirmationLabel(context, attachment.gem_slots + offset,
                        attachment.ok_blink);
  if (Pressed(buttons, pad::kCross)) {
    RestoreGemsAndLeaveSelection(context, attachment.target, screen.is_shield);
    screen.step = kScreenTitle;
  } else if (Pressed(buttons, pad::kStart)) {
    context.host.PlaySound(MenuSound::kSelect);
    context.host.MenuItem(offset + attachment.selected_slot + kOptionRowBase)
        .selected = 0;
    screen.step = kGemAsk;
    attachment.selected_slot = attachment.gem_slots;
  } else if (Pressed(buttons, pad::kCircle)) {
    context.host.PlaySound(MenuSound::kSelect);
    const int slot = attachment.selected_slot;
    if (slot == attachment.gem_slots) {
      screen.step = kGemAsk;
    } else {
      context.host.ClearMenuExcept(kNoRow);
      SelectGem(context,
                GemListParams(slot + offset + 1, slot + 1, attachment.staged));
      screen.step = kGemSelect;
    }
  } else {
    MoveGemSlot(context, buttons);
  }
  return 0;
}

// The chosen gem goes into the selected slot (or, when it is the slot's,
// comes out) at once, and the cursor moves on.
void SwapSlotGem(Context& context, int chosen) {
  GemAttachmentState& attachment = context.state.gem_attachment;
  const bool is_shield = attachment.screen.is_shield;
  const std::span<std::uint8_t> attached = AttachedGems(context, is_shield);
  const std::size_t slot = attachment.selected_slot;
  if (slot >= attachment.staged.size() || slot >= attached.size()) {
    context.host.Refuse(std::format("gem slot {} outside the record", slot));
    return;
  }
  const int previous = attachment.staged[slot];
  if (chosen == previous) {
    SetGemOwner(context, chosen, 0);
    attached[slot] = 0;
    attachment.staged[slot] = 0;
  } else {
    if (previous != 0) {
      SetGemOwner(context, previous, 0);
    }
    SetGemOwner(context, chosen,
                context.state.combining_item + 1 +
                    (is_shield ? Items::kShieldGemFlag : 0));
    attached[slot] = static_cast<std::uint8_t>(chosen);
    attachment.staged[slot] = static_cast<std::uint8_t>(chosen);
    ++attachment.selected_slot;
  }
  ReequipCombining(context, is_shield);
}

// The changed equipment's header row and stats.
void ShowGemEquipment(Context& context) {
  const bool is_shield = context.state.gem_attachment.screen.is_shield;
  const int reference = context.state.combining_item + 1;
  context.host.SetEnabledStatPages(kAllStatPages);
  context.host.UpdateDpPpBars(kAssemblyDpPp);
  if (is_shield) {
    context.host.SetShieldStats(reference);
  } else {
    context.host.SetWeaponStats(reference);
  }
  EquipmentRows rows;
  if (CheckReference(context, reference, kEquipmentListCapacity,
                     "gem equipment")) {
    FormatEquipmentRow(context, rows, is_shield,
                       static_cast<std::size_t>(reference - 1),
                       rows.descriptions[0]);
  }
  MenuRow& row = context.host.SetMenuItem(kGemEquipmentRow, rows.texts[0].name);
  row.state = RowState::kSlideX;
  row.target0 = kGemEquipmentX;
  row.selected = 1;
  const std::int32_t type = rows.types[0];
  row.row_icon = static_cast<std::uint8_t>(type >> kRowIconShift);
  row.material =
      static_cast<std::uint8_t>((type >> kMaterialShift) & kMaterialMask);
}

// Step 6: the gem list's choice.
int TakeSlotGem(Context& context) {
  GemAttachmentState& attachment = context.state.gem_attachment;
  const int chosen = SelectGem(context, 0);
  if (chosen == 0) {
    return 0;
  }
  if (chosen == -2) {
    return RestoreGemsAndLeaveSelection(context, attachment.target,
                                        attachment.screen.is_shield);
  }
  if (chosen > 0) {
    SwapSlotGem(context, chosen);
  }
  ShowGemEquipment(context);
  attachment.screen.appear_step = 0;
  attachment.screen.step = kScreenParts;
  return 0;
}

// Step 8: yes keeps the gems, no returns to the slots, cancel restores
// them.
int AnswerGemAttachment(Context& context) {
  GemAttachmentState& attachment = context.state.gem_attachment;
  switch (UpdateThreeChoicePrompt(context, 0)) {
    case kAnswerYes:
      context.host.PlaySoundEffect(sfx::kCombine);
      context.host.InitializeInformationBox();
      context.host.UpdateEquipmentStatPages(kHideStatPages);
      context.host.UpdateDpPpBars(kHideDpPp);
      context.host.ClearMenuExcept(kNoRow);
      context.host.FlyOutRowLeft(kTitleRow);
      return 1;
    case kAnswerNo:
      context.host.PlaySound(MenuSound::kLeave);
      context.host.UpdatePrimaryStatPanel(kShowPanel);
      attachment.screen.step = kGemChoose;
      break;
    case kAnswerCancel:
      return RestoreGemsAndLeaveSelection(context, attachment.target,
                                          attachment.screen.is_shield);
    default:
      break;
  }
  return 0;
}

// _attachGemsTopMenu's rows: weapons and shields, "no items" when there are
// none.
void ShowGemCategoryRows(Context& context, CategoryState& menu) {
  std::array<RowText, kGemCategories.size()> texts{};
  std::array<std::int32_t, kGemCategories.size()> types{};
  const Items::InventoryContents& inventory = context.host.Inventory();
  for (std::size_t row = 0; row < kGemCategories.size(); ++row) {
    const int name_entry = menu12::kWeaponsIndex + static_cast<int>(row * 2);
    texts[row] = {.name = Menu12Entry(context, name_entry),
                  .description = Menu12Entry(context, name_entry + 1)};
    if (Items::Inventory::CountItems(kGemCategories[row], inventory) == 0) {
      types[row] = 1;
      texts[row].description = Menu12Text(context, menu12::kNoItems);
    }
  }
  CreateCategoryRows(context, menu, kGemCategoryRowInfo, texts, types);
}

int OpenGemScreen(Context& context, CategoryState& menu) {
  const int category = context.host.SelectedRow() + 1;
  if (category < 0) {
    context.host.ClearMenuExcept(kTitleRow);
    return category;
  }
  if (category > 0) {
    context.host.ClearMenuExcept(kNoRow);
    UpdateGemAttachment(context, category);
    menu.step = kCategoryRun;
  }
  return 0;
}

int RunGemScreen(Context& context, CategoryState& menu) {
  const int result = UpdateGemAttachment(context, 0);
  if (result != -1) {
    return result;
  }
  menu.step = kCategoryReturn;
  return 0;
}
}  // namespace

std::int32_t PackGems(const GemIds& gems) {
  std::uint32_t packed = 0;
  for (std::size_t i = 0; i < gems.size(); ++i) {
    packed |= static_cast<std::uint32_t>(gems[i]) << (i * kByteBits);
  }
  return static_cast<std::int32_t>(packed);
}

// decomp: MENUC.PRG _pushSelectionHistory (src/MENU/MENUC.PRG/168.c:56)
void RecordSlotSelection(Context& context, int index) {
  std::array<std::uint32_t, kSlotHistoryCount>& history =
      context.state.slot_history;
  if (index < 0) {
    context.host.Refuse(std::format("selection history slot {}", index));
    return;
  }
  if (static_cast<std::size_t>(index) >= history.size()) {
    history.fill(0);
    return;
  }
  history[static_cast<std::size_t>(index)] =
      *std::ranges::max_element(history) + 1;
}

// decomp: MENUC.PRG _popSelectionHistory (src/MENU/MENUC.PRG/168.c:83)
int PopSlotSelection(Context& context, int index) {
  std::array<std::uint32_t, kSlotHistoryCount>& history =
      context.state.slot_history;
  if (index < 0) {
    context.host.Refuse(std::format("selection history slot {}", index));
    return 0;
  }
  if (static_cast<std::size_t>(index) < history.size()) {
    history[static_cast<std::size_t>(index)] = 0;
    return 0;
  }
  // The latest selection is the first greatest entry.
  const auto latest = std::ranges::max_element(history);
  if (*latest == 0) {
    return 0;
  }
  *latest = 0;
  return static_cast<int>(latest - history.begin()) + 1;
}

// decomp: MENUC.PRG _disassembleWeapon (src/MENU/MENUC.PRG/168.c:310)
void DisassembleWeapon(Context& context, int weapon_index) {
  Items::InventoryContents& inventory = context.host.Inventory();
  Items::InventoryWeapon* weapon = InventorySlot(
      context, inventory.weapons, weapon_index + 1, "disassembled weapon");
  if (weapon == nullptr || weapon->blade == 0) {
    return;
  }
  Items::InventoryGrip* grip =
      InventorySlot(context, inventory.grips, weapon->grip, "weapon grip");
  // Without its grip every gem the weapon holds comes off.
  const std::size_t slots = grip != nullptr
                                ? GemSlotCount(context, grip->gem_slots)
                                : weapon->gems.size();
  ClearGemLinks(context, std::span(weapon->gems).first(slots));
  if (grip != nullptr) {
    grip->assembled_weapon_index = 0;
  }
  Items::InventoryBlade* blade =
      InventorySlot(context, inventory.blades, weapon->blade, "weapon blade");
  if (blade != nullptr) {
    blade->assembled_weapon_index = 0;
  }
  const std::uint8_t index = weapon->index;
  *weapon = Items::InventoryWeapon{};
  weapon->index = index;
  RebuildWeaponOrder(context);
}

// decomp: MENUC.PRG _assembleWeapon (src/MENU/MENUC.PRG/168.c:345)
void AssembleWeapon(Context& context, int blade_index, int grip_index,
                    const GemIds& gems) {
  Items::InventoryContents& inventory = context.host.Inventory();
  const int owner = context.state.combining_item + 1;
  Items::InventoryWeapon* weapon =
      InventorySlot(context, inventory.weapons, owner, "assembled weapon");
  Items::InventoryBlade* blade = InventorySlot(
      context, inventory.blades, blade_index + 1, "assembled blade");
  Items::InventoryGrip* grip =
      InventorySlot(context, inventory.grips, grip_index + 1, "assembled grip");
  if (weapon == nullptr || blade == nullptr || grip == nullptr) {
    return;
  }
  const std::size_t slots = GemSlotCount(context, grip->gem_slots);
  DisassembleWeapon(context, context.state.combining_item);
  weapon->blade = static_cast<std::uint8_t>(blade_index + 1);
  weapon->grip = static_cast<std::uint8_t>(grip_index + 1);
  std::copy_n(gems.begin(), slots, weapon->gems.begin());
  blade->assembled_weapon_index = static_cast<std::uint8_t>(owner);
  grip->assembled_weapon_index = static_cast<std::uint16_t>(owner);
  for (const std::uint8_t gem : std::span(weapon->gems).first(slots)) {
    if (gem != 0) {
      SetGemOwner(context, gem, owner);
    }
  }
  weapon->name.fill(static_cast<char>(Render::kTextTerminator));
  RebuildWeaponOrder(context);
}

// decomp: MENUC.PRG _disassembleShield (src/MENU/MENUC.PRG/168.c:384)
void DetachShieldGems(Context& context, int shield_index) {
  Items::InventoryShield* shield = InventorySlot(
      context, context.host.Inventory().shields, shield_index + 1, "shield");
  if (shield == nullptr) {
    return;
  }
  ClearGemLinks(context,
                std::span(shield->gems)
                    .first(GemSlotCount(context, shield->base.gem_slots)));
}

// decomp: MENUC.PRG _isValidGrip (src/MENU/MENUC.PRG/168.c:403)
bool IsGripCompatible(Context& context, const Items::InventoryBlade* blade,
                      const Items::InventoryGrip* grip) {
  if (blade == nullptr || grip == nullptr) {
    return true;
  }
  if (grip->category >= kGripBladeCategories.size()) {
    context.host.Refuse(std::format("grip category {} outside the {} kinds",
                                    grip->category,
                                    kGripBladeCategories.size()));
    return false;
  }
  if (blade->category >= kWordBits) {
    context.host.Refuse(std::format("blade category {} past the category bits",
                                    blade->category));
    return false;
  }
  return ((1U << blade->category) & kGripBladeCategories[grip->category]) != 0;
}

// decomp: MENUC.PRG _setUiStats (src/MENU/MENUC.PRG/168.c:730)
void SetAssemblyStats(Context& context, int blade, int grip) {
  context.host.ResetEquipmentStats();
  context.host.SetRangeAndRisk(0, 0, 0, 1);
  context.host.SetEquipmentSubtype(0);
  if (blade != 0 && grip != 0) {
    context.host.SetWeaponStats(context.state.combining_item + 1);
  } else if (blade != 0) {
    context.host.SetBladeStats(blade);
  } else if (grip != 0) {
    context.host.SetGripStats(grip);
  }
  context.host.SetEnabledStatPages(kAllStatPages);
  context.host.UpdateDpPpBars(kAssemblyDpPp);
}

// decomp: MENUC.PRG _assembleBladeMenuHeader (src/MENU/MENUC.PRG/168.c:764)
void SetAssemblyHeader(Context& context, int blade_grip) {
  const int text = blade_grip & kHeaderTextMask;
  MenuRow& row = context.host.SetMenuItem(
      kAssemblyHeader, Menu12Entry(context, text + menu12::kAssembleIndex));
  row.row_icon = static_cast<std::uint8_t>(kWeaponIcon);
  row.selected = 1;
  if (text == 0) {
    SetAssemblyStats(context,
                     (blade_grip >> kHeaderBladeShift) & kHeaderPartMask,
                     (blade_grip >> kHeaderGripShift) & kHeaderPartMask);
  }
}

// decomp: MENUC.PRG _assembleBladeMenu (src/MENU/MENUC.PRG/168.c:802)
int SelectAssemblyBlade(Context& context, int params) {
  return SelectPart<BladePart>(context, params);
}

// decomp: MENUC.PRG _assembleGripMenu (src/MENU/MENUC.PRG/168.c:923)
int SelectAssemblyGrip(Context& context, int params) {
  return SelectPart<GripPart>(context, params);
}

// decomp: MENUC.PRG _attachGem (src/MENU/MENUC.PRG/168.c:1045)
int SelectGem(Context& context, int params) {
  return SelectPart<GemPart>(context, params);
}

// decomp: MENUC.PRG _leaveAssembleMenu (src/MENU/MENUC.PRG/168.c:1150)
int CancelAssembly(Context& context) {
  DisassembleWeapon(context, context.state.combining_item);
  return LeaveItemSelection(context, true);
}

// decomp: MENUC.PRG _assembleMenu (src/MENU/MENUC.PRG/168.c:1164)
int UpdateAssembly(Context& context, bool initialize) {
  AssemblyState& assembly = context.state.assembly;
  if (initialize) {
    StartAssembly(context);
    return 0;
  }
  const AssemblyParts parts = StagedParts(context);
  assembly.ok_blink = static_cast<std::uint8_t>(assembly.ok_blink + kBlinkStep);
  if (assembly.step != kAssemblyChoose) {
    assembly.ok_blink = 0;
  }
  switch (assembly.step) {
    case kAssemblyTitle:
      if (context.host.RowsIdle()) {
        InitializeEquipmentHeader(context, menu12::kAssemble, kWeaponIcon);
        context.host.SetEquipmentSubtype(0);
        assembly.step = kAssemblyRows;
      }
      break;
    case kAssemblyRows:
      ShowAssemblyRows(context, parts);
      assembly.step = kAssemblyChoose;
      break;
    case kAssemblyChoose:
      return RunAssemblyRows(context, parts);
    case kAssemblyBlade:
      return TakeChosenPart(context, true);
    case kAssemblyGrip:
      return TakeChosenPart(context, false);
    case kAssemblyGem:
      return TakeChosenGem(context);
    case kAssemblyAsk:
      AskAttachment(context, assembly.step);
      break;
    case kAssemblyAnswer:
      return AnswerAssembly(context);
    case kAssemblyFinish:
      return FinishAssembly(context);
    default:
      break;
  }
  return 0;
}

// decomp: MENUC.PRG _addGemSelector (src/MENU/MENUC.PRG/168.c:1599)
void CreateEmptyGemSlotRow(Context& context, int row) {
  MenuRow& slot = context.host.SetMenuItem({.row = row + kDetailRow,
                                            .x = kOffscreenX,
                                            .y = (row * kRowPitch) + kRowTopY,
                                            .w = kGemRowWidth,
                                            .background_width = 0},
                                           Menu12Text(context, menu12::kGems));
  slot.state = RowState::kSlideX;
  slot.target0 = kGemRowX;
  slot.font_color = 1;
}

// decomp: MENUC.PRG _attachGems (src/MENU/MENUC.PRG/168.c:1616)
int RestoreGemsAndLeaveSelection(Context& context, const GemIds& gems,
                                 bool is_shield) {
  const std::span<std::uint8_t> attached = AttachedGems(context, is_shield);
  const std::size_t count = std::min(attached.size(), kRestoredGemSlots);
  ClearGemLinks(context, attached.first(count));
  const int owner = context.state.combining_item + 1 +
                    (is_shield ? Items::kShieldGemFlag : 0);
  for (std::size_t i = 0; i < count; ++i) {
    if (gems[i] != 0) {
      SetGemOwner(context, gems[i], owner);
      attached[i] = gems[i];
    }
  }
  ReequipCombining(context, is_shield);
  return LeaveItemSelection(context, true);
}

// decomp: MENUC.PRG _attachGemsMenu (src/MENU/MENUC.PRG/168.c:1666)
int UpdateGemAttachment(Context& context, int category) {
  GemAttachmentState& attachment = context.state.gem_attachment;
  EquipmentScreenState& screen = attachment.screen;
  if (category != 0) {
    context.host.InitInformationTextBox(1);
    screen.is_shield = category - 1 != 0;
    screen.step = kScreenTitle;
    return 0;
  }
  switch (screen.step) {
    case kScreenTitle:
      if (context.host.RowsIdle()) {
        SetTitleRow(context, kTitleRow,
                    Menu12EntryOffset(context, (screen.is_shield ? 1 : 0) +
                                                   menu12::kAttachWeaponIndex));
        screen.step = kScreenList;
      }
      break;
    case kScreenList:
      if (context.host.RowsIdle()) {
        ListGemEquipment(context, screen.is_shield);
        screen.step = kScreenPick;
      }
      break;
    case kScreenPick:
      return PickEquipmentRow(context, screen);
    case kScreenSettle:
      if (context.host.RowsIdle()) {
        SettleGemEquipment(context);
      }
      break;
    case kScreenParts:
      ShowGemRows(context);
      break;
    case kGemChoose:
      return RunGemRows(context);
    case kGemSelect:
      return TakeSlotGem(context);
    case kGemAsk:
      AskAttachment(context, screen.step);
      break;
    case kGemAnswer:
      return AnswerGemAttachment(context);
    default:
      break;
  }
  return 0;
}

// decomp: MENUC.PRG _attachGemsTopMenu (src/MENU/MENUC.PRG/168.c:2016)
int UpdateGemCategoryMenu(Context& context, bool initialize) {
  CategoryState& menu = context.state.gem_category;
  if (initialize) {
    menu.keep_cursor = true;
    context.host.FlyOutRowsAndHoistSelection(1, 1);
    menu.step = kCategoryRows;
    return 0;
  }
  if (menu.step == kCategoryRows) {
    if (context.host.RowsIdle()) {
      ShowGemCategoryRows(context, menu);
    }
  } else if (menu.step == kCategoryChoose) {
    return OpenGemScreen(context, menu);
  } else if (menu.step == kCategoryRun) {
    return RunGemScreen(context, menu);
  } else if (menu.step == kCategoryReturn) {
    ReturnToCategory(context, menu, kGemSubtitleRow, menu12::kAttachGems);
  }
  return 0;
}
}  // namespace VS::Menu::Setup
