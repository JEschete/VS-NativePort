#include "Menu/Container/ContainerListTransfer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <variant>

#include "Core/Math/ScalarMath.hpp"
#include "Items/ContainerTransfers.hpp"
#include "Menu/Container/ContainerDetailHelpers.hpp"

namespace VS::Menu::container {
namespace {
using Category = Items::ItemCategory;
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
using Core::Math::WrappingSubtract;

// Controller bits of VS::Input::buttonsPressed/State/Repeat.
constexpr std::uint32_t kButtonL1 = 0x4;
constexpr std::uint32_t kButtonR1 = 0x8;
constexpr std::uint32_t kButtonTriangle = 0x10;
constexpr std::uint32_t kButtonCircle = 0x20;
constexpr std::uint32_t kButtonCross = 0x40;
constexpr std::uint32_t kButtonSquare = 0x80;
constexpr std::uint32_t kButtonUp = 0x1000;
constexpr std::uint32_t kButtonRight = 0x2000;
constexpr std::uint32_t kButtonDown = 0x4000;
constexpr std::uint32_t kButtonLeft = 0x8000;
constexpr std::uint32_t kPreviousCategory = kButtonLeft | kButtonL1;
constexpr std::uint32_t kNextCategory = kButtonRight | kButtonR1;

constexpr std::int32_t kExit = -2;
constexpr std::int32_t kCancel = -1;
constexpr std::int32_t kNoMenuItem = 40;
constexpr std::int32_t kCategoryTitleRow = 0;
constexpr std::int32_t kCategoryHeaderRow = 0x1F;
constexpr std::uint32_t kCategoryMask = 7;
constexpr std::int32_t kRecordCategoryCount = 7;
constexpr auto kSlideToTarget = Framework::RowState::kSlideX;

// func_80107490's step numbers (234.c:2393).
enum ListStep : std::uint8_t {
  kListInitialize = 0,
  kListBuild = 1,
  kListBrowse = 2,
  kListSwitchCategory = 3,
  kListAwaitRebuild = 4,
  kListAwaitReinitialize = 5,
  kListMove = 6,
  kListDetails = 7,
  kListActions = 8,
  kListActionMenu = 9,
  kListJumpToParent = 10,
  kListDiscard = 11,
  kListSort = 12,
  kListPendingRows = 13,
  kListAwaitConfirmation = 14,
  kListConfirm = 15,
  kListClose = 17,
};

enum ItemAction : std::uint8_t {
  kActionTransfer = 1,
  kActionJumpToParent = 2,
  kActionDiscard = 3,
  kActionDiscardOne = 4,
  kActionSort = 5,
};

// _menuText entry positions and direct offsets.
constexpr std::size_t kEmptyCategoryMessage = 27;
constexpr std::size_t kEmptyPendingMessage = 0x1D;
constexpr std::size_t kTransferActionName = 0x1F;
constexpr std::size_t kTransferActionDescription = 0x20;
constexpr std::size_t kCapacityErrorMessage = 5;
constexpr std::size_t kParentEquippedMessage = 4;
constexpr std::size_t kParentStoredMessage = 5;
constexpr std::size_t kDirectionName = 12;
constexpr std::size_t kDirectionHelp = 13;
constexpr std::size_t kScreenDirectionName = 0xF;
// Direction results 1 and 2 open a list; 3 ends the transfer.
constexpr std::uint32_t kEndTransferResult = 3;
constexpr std::size_t kCategoryName = 0x12;
constexpr std::uint32_t kItemCannotTransfer = 0x33;
constexpr std::uint32_t kItemInUse = 0x4A;
constexpr std::uint32_t kMiscStorageFull = 0xAB;
constexpr std::uint32_t kMiscStackFull = 0x82;
constexpr std::uint32_t kContainerTitle = 35;
constexpr std::uint32_t kEndTransferHelp = 40;

// ITEMHELP.BIN offsets of the action rows.
constexpr std::uint32_t kJumpActionName = 0x3527;
constexpr std::uint32_t kJumpActionHelp = 0x352C;
constexpr std::uint32_t kJumpActionUnattached = 0x35B2;
constexpr std::uint32_t kDiscardActionName = 0x354A;
constexpr std::uint32_t kDiscardActionHelp = 0x354F;
constexpr std::size_t kDiscardBlockedHelp = 663;
constexpr std::uint32_t kDiscardOneName = 0x356D;
constexpr std::uint32_t kDiscardOneHelp = 0x3574;
constexpr std::uint32_t kKeyItemHelp = 0x350E;
constexpr std::uint32_t kSortActionName = 0x3586;
constexpr std::uint32_t kSortActionHelp = 0x358A;

constexpr std::int32_t kItemRowsBase = 0x51;
constexpr std::int32_t kItemRowsLayout = 0x19200;
constexpr std::int32_t kActionMenuBase = 0x59;
constexpr std::uint32_t kPickedRowFlag = 8;
constexpr std::uint16_t kFirstKeyItem = 0x1CA;
constexpr std::int32_t kKeyItemDiscardBlock = 3;
constexpr std::int32_t kStoredParentDiscardBlock = 2;
constexpr std::uint8_t kMiscStackLimit = 100;
constexpr std::int32_t kMiscStackOverflowBase = 100;
constexpr std::int32_t kStagedTotalLimit = 0x40;
constexpr std::int32_t kContainerMiscTransferLimit = 0x40;
constexpr std::int32_t kDetailsRowScale = 0x10;
constexpr std::uint32_t kDiscardCategoryShift = 12;
constexpr std::int32_t kDiscardStack = 0x10000;
constexpr std::int32_t kDiscardSingle = 0x20000;
constexpr std::int16_t kTitleRowMinimumX = 0xB5;
constexpr std::int16_t kTitleRowOverflowX = 0xCD;
constexpr std::int16_t kTitleRowShift = 0x18;
constexpr std::int16_t kTitleRowTargetX = 0xB4;
constexpr std::int32_t kVisibleItemRows = 8;
constexpr std::uint32_t kTokenCategoryShift = 8;
constexpr std::uint32_t kTokenSlotMask = 0xFF;
constexpr std::int32_t kCopyItem = 0x10;

std::int32_t Index(Category category) {
  return static_cast<std::int32_t>(category);
}

Category CategoryAt(std::int32_t value) {
  return static_cast<Category>(static_cast<std::uint32_t>(value) &
                               kCategoryMask);
}

Category SelectedCategory(TransferWorld& world) {
  return CategoryAt(WrappingSubtract(world.CategoryToken(), 1));
}

TransferText MenuText(std::uint32_t offset) {
  return {TransferTextTable::kMenuText, offset};
}

TransferText MenuTextAt(TransferWorld& world, std::size_t entry) {
  return MenuText(world.MenuTextEntry(entry));
}

TransferText ItemHelp(std::uint32_t offset) {
  return {TransferTextTable::kItemHelp, offset};
}

std::int32_t ItemRowsInfo(Category category) {
  return WrappingAdd(Index(category), kItemRowsBase) | kItemRowsLayout;
}

std::span<std::uint16_t> CategoryOrder(TransferWorld& world,
                                       Category category) {
  const auto indices = world.Indices(TransferStorage::kActive);
  const auto first = Items::ContainerCategoryIndices(
      category, world.Records(TransferStorage::kActive));
  if (first > indices.size()) world.Refuse("category order outside indices");
  return indices.subspan(first);
}

std::uint16_t OrderToken(TransferWorld& world, Category category,
                         std::int32_t row) {
  const auto order = CategoryOrder(world, category);
  if (row < 1 || static_cast<std::size_t>(row) > order.size()) {
    world.Refuse("item row outside the category order");
  }
  return order[static_cast<std::size_t>(row) - 1];
}

template <typename Record>
Record ReadRecord(Items::ContainerWorld& records, Category category,
                  std::int32_t slot) {
  if (slot < 0 || slot >= records.Capacity(category)) {
    records.Refuse("item record outside its category");
  }
  const auto record = records.Read(category, static_cast<std::size_t>(slot));
  const auto* typed = std::get_if<Record>(&record);
  if (typed == nullptr) records.Refuse("item record type disagrees");
  return *typed;
}

template <typename Record>
void WriteRecord(Items::ContainerWorld& records, Category category,
                 std::int32_t slot, const Record& record) {
  if (slot < 0 || slot >= records.Capacity(category)) {
    records.Refuse("item record outside its category");
  }
  records.Write(category, static_cast<std::size_t>(slot), record);
}

Items::ContainerWorld& Active(TransferWorld& world) {
  return world.Records(TransferStorage::kActive);
}

void CreateRows(TransferWorld& world, std::int32_t count, Category category,
                bool remember_cursor) {
  const auto saved = world.CursorMemorySetting();
  if (remember_cursor) world.SetCursorMemorySetting(1);
  world.CreateItemMenuRows(count, ItemRowsInfo(category));
  world.SetCursorMemorySetting(saved);
}

// --- func_80107490 (234.c:2393) step helpers ----------------------------

struct StepResult {
  std::int32_t value = 0;
  bool restart = false;
};

constexpr StepResult Done(std::int32_t value = 0) { return {value, false}; }

void InitializeItemList(std::int32_t direction, ItemListState& state,
                        TransferWorld& world) {
  state.first_entry = true;
  world.SetPanelVisibility(1);
  world.SetFlag(TransferFlag::kTitleSlide, 0);
  if (world.CursorMemorySetting() == 0) world.ClearCategoryMemory();
  world.ClearStagedTransfers();
  world.SetFlag(TransferFlag::kBrowsingGadgetBag,
                static_cast<std::uint8_t>(WrappingSubtract(direction, 1)));
  world.SelectStorages(world.Flag(TransferFlag::kBrowsingGadgetBag) != 0);
  state.step = kListBuild;
}

// decomp: case 0/1 of func_80107490 (234.c:2433).
void BuildList(Category category, ItemListState& state, TransferWorld& world) {
  if (state.step == kListInitialize) world.InitializeInformationBox();
  world.AcquireItemRows();
  world.SetFreezeTabArrows(1);
  for (std::int32_t value = 0; value < kRecordCategoryCount; ++value) {
    world.RebuildCategoryIndices(TransferStorage::kActive, CategoryAt(value));
  }
  world.RebuildPendingTransfers();
  const auto count = world.BuildItemRows(category);
  state.category_empty = count == 0;
  if (state.category_empty) {
    const auto browsing = world.Flag(TransferFlag::kBrowsingGadgetBag);
    world.SetInformationMessage(
        MenuTextAt(world, browsing + kEmptyCategoryMessage +
                              (category == Category::kManage ? 2U : 0U)));
  } else {
    const auto remember = !state.first_entry;
    state.first_entry = false;
    const auto saved = world.CursorMemorySetting();
    if (remember) world.SetCursorMemorySetting(1);
    world.CreateItemMenuRows(count, ItemRowsInfo(category));
    if (state.step == kListInitialize) world.RestoreItemRows(category);
    world.SetCursorMemorySetting(saved);
  }
  world.ReleaseItemRows();
  state.step = kListBrowse;
}

void BeginTransfer(Category category, ItemListState& state,
                   TransferWorld& world) {
  if (category == Category::kManage) {
    static_cast<void>(world.UpdateTransferConfirmation(1));
    state.step = kListConfirm;
    return;
  }
  world.SetCategoryToken(0);
  world.ClearMenuExcept(kCategoryTitleRow);
  state.step = kListPendingRows;
}

bool PendingTransfersEmpty(TransferWorld& world) {
  const auto pending = world.PendingTransfers();
  if (pending.empty()) world.Refuse("missing pending-transfer list");
  return pending[0] == 0;
}

std::optional<StepResult> BrowseEmpty(Category category, ItemListState& state,
                                      TransferWorld& world) {
  const auto pressed = world.PressedButtons();
  if ((pressed & kButtonTriangle) != 0) {
    world.PlaySound(TransferSound::kLeave);
    return Done(kExit);
  }
  if ((pressed & kButtonCross) != 0) {
    world.PlaySound(TransferSound::kLeave);
    if (PendingTransfersEmpty(world)) return Done(kCancel);
    BeginTransfer(category, state, world);
    return Done();
  }
  if ((pressed & (kButtonSquare | kButtonCircle)) != 0) {
    world.PlaySound(TransferSound::kInvalid);
  }
  return std::nullopt;
}

void OpenDetails(Category category, ItemListState& state,
                 TransferWorld& world) {
  world.PlaySound(TransferSound::kSelect);
  const auto closed = world.CloseRowsAndRememberCursor();
  world.ClearRowOutsetIcon(closed >> kTokenCategoryShift);
  static_cast<void>(world.UpdateItemDetails(
      Index(category) |
      WrappingMultiply(WrappingAdd(closed, 1), kDetailsRowScale)));
  world.SetFlag(TransferFlag::kTitleVisible, 0);
  state.step = kListDetails;
}

void PickRow(Category category, ItemListState& state, TransferWorld& world) {
  if (category == Category::kManage) {
    world.UndoPendingTransfer(state.selected_row - 1);
    world.RefreshMiscTransferCount();
    world.ClearMenuExcept(kCategoryTitleRow);
    state.step = kListAwaitRebuild;
    return;
  }
  world.AcquireItemRows();
  const auto count = world.BuildItemRows(category);
  world.MarkItemRow(static_cast<std::size_t>(state.selected_row - 1),
                    kPickedRowFlag);
  CreateRows(world, count, category, true);
  world.RestoreItemRows(category);
  world.SetFlag(TransferFlag::kControlsMode, 1);
  world.SetFreezeTabArrows(0);
  world.ReleaseItemRows();
  state.step = kListMove;
}

// decomp: case 2 of func_80107490 (234.c:2476). nullopt falls into case 3.
std::optional<StepResult> Browse(Category category, ItemListState& state,
                                 TransferWorld& world) {
  world.SetFlag(TransferFlag::kControlsMode, 1);
  if (state.category_empty) return BrowseEmpty(category, state, world);
  const auto has_details = category < Category::kMisc;
  world.SetFlag(TransferFlag::kControlsMode, has_details ? 2 : 1);
  state.selected_row = WrappingAdd(world.SelectedRow(), 1);
  if (state.selected_row == 0) {
    if ((world.PressedButtons() & kButtonSquare) == 0) return std::nullopt;
    if (has_details && world.RowsIdle()) {
      OpenDetails(category, state, world);
      return Done();
    }
    world.PlaySound(TransferSound::kInvalid);
    return std::nullopt;
  }
  if (state.selected_row > 0) {
    PickRow(category, state, world);
    return std::nullopt;
  }
  if (state.selected_row == kExit) return Done(kExit);
  if (PendingTransfersEmpty(world)) {
    world.ClearMenuExcept(kCategoryTitleRow);
    return Done(kCancel);
  }
  BeginTransfer(category, state, world);
  return Done();
}

// decomp: case 3 of func_80107490 (234.c:2560).
StepResult SwitchCategory(Category category, ItemListState& state,
                          TransferWorld& world) {
  const auto held = world.HeldButtons();
  const auto previous_held = held & kPreviousCategory;
  const auto next_held = held & kNextCategory;
  const auto previous = previous_held != 0;
  const auto one_side = next_held != 0 ? !previous : previous;
  if (one_side) {
    auto value = Index(category);
    const auto repeated = world.RepeatedButtons();
    const auto previous_mask =
        previous_held == kPreviousCategory ? kButtonLeft : kPreviousCategory;
    if ((repeated & previous_mask) != 0) {
      value = static_cast<std::int32_t>(static_cast<std::uint32_t>(value - 1) &
                                        kCategoryMask);
    }
    const auto next_mask =
        next_held == kNextCategory ? kButtonRight : kNextCategory;
    if ((repeated & next_mask) != 0) {
      value = static_cast<std::int32_t>(static_cast<std::uint32_t>(value + 1) &
                                        kCategoryMask);
    }
    if (CategoryAt(value) != SelectedCategory(world)) {
      world.PlaySound(TransferSound::kChange);
      world.SetCategoryToken(
          static_cast<std::uint8_t>((value + 1) & kCategoryMask));
      if (state.step == kListBrowse) {
        if (!state.category_empty) {
          static_cast<void>(world.CloseRowsAndRememberCursor());
          world.ClearMenuExcept(kCategoryTitleRow);
        }
        state.step = kListSwitchCategory;
      }
    }
  }
  if (state.step != kListSwitchCategory || !world.RowsIdle()) return Done();
  if ((world.HeldButtons() & (kPreviousCategory | kNextCategory)) != 0 &&
      one_side) {
    return Done();
  }
  state.step = kListBuild;
  return {0, true};
}

void MergeMisc(std::int32_t dropped_slot, std::int32_t kept_slot,
               Items::ContainerWorld& records) {
  auto kept =
      ReadRecord<Items::InventoryMisc>(records, Category::kMisc, kept_slot);
  kept.count = static_cast<std::uint8_t>(
      kept.count +
      ReadRecord<Items::InventoryMisc>(records, Category::kMisc, dropped_slot)
          .count);
  WriteRecord(records, Category::kMisc, kept_slot, kept);
  const auto sum =
      ReadRecord<Items::InventoryMisc>(records, Category::kMisc, kept_slot)
          .count;
  if (sum <= kMiscStackLimit) {
    Items::ClearContainerItem(Category::kMisc, dropped_slot, records);
    return;
  }
  auto overflow =
      ReadRecord<Items::InventoryMisc>(records, Category::kMisc, dropped_slot);
  overflow.count = static_cast<std::uint8_t>(sum - kMiscStackOverflowBase);
  WriteRecord(records, Category::kMisc, dropped_slot, overflow);
  auto full =
      ReadRecord<Items::InventoryMisc>(records, Category::kMisc, kept_slot);
  full.count = kMiscStackLimit;
  WriteRecord(records, Category::kMisc, kept_slot, full);
}

bool SameMisc(std::int32_t first_slot, std::int32_t second_slot,
              Items::ContainerWorld& records) {
  return ReadRecord<Items::InventoryMisc>(records, Category::kMisc, first_slot)
             .id ==
         ReadRecord<Items::InventoryMisc>(records, Category::kMisc, second_slot)
             .id;
}

void MoveRow(Category category, std::int32_t source, std::int32_t target,
             TransferWorld& world) {
  const auto target_token = OrderToken(world, category, target);
  const auto source_token = OrderToken(world, category, source);
  auto& records = Active(world);
  const auto dropped_slot = static_cast<std::int32_t>(target_token) - 1;
  const auto kept_slot = static_cast<std::int32_t>(source_token) - 1;
  if (category == Category::kMisc &&
      SameMisc(dropped_slot, kept_slot, records)) {
    MergeMisc(dropped_slot, kept_slot, records);
    return;
  }
  const auto order = CategoryOrder(world, category);
  order[static_cast<std::size_t>(target) - 1] = source_token;
  order[static_cast<std::size_t>(source) - 1] = target_token;
}

// decomp: case 6 of func_80107490 (234.c:2611).
std::int32_t MoveSelectedRow(Category category, ItemListState& state,
                             TransferWorld& world) {
  if ((world.PressedButtons() & kButtonSquare) != 0) {
    world.PlaySound(TransferSound::kInvalid);
  }
  const auto target = WrappingAdd(world.SelectedRow(), 1);
  if (target == 0) return 0;
  if (state.selected_row == target) {
    state.step = kListActions;
    return 0;
  }
  if (target > 0) {
    MoveRow(category, state.selected_row, target, world);
    world.ClearMenuExcept(kCategoryTitleRow);
    state.step = kListAwaitRebuild;
    return 0;
  }
  if (target != kCancel) return kExit;
  state.step = kListInitialize;
  return 0;
}

// decomp: case 7 of func_80107490 (234.c:2647).
std::int32_t CloseDetails(ItemListState& state, TransferWorld& world) {
  const auto result = world.UpdateItemDetails(0);
  if (result == 0) return 0;
  if (result == kExit) return kExit;
  world.SetFlag(TransferFlag::kTitleVisible, 1);
  const auto x = world.RowX(kCategoryTitleRow);
  if (x >= kTitleRowMinimumX) {
    if (x >= kTitleRowOverflowX) {
      world.SetRowX(kCategoryTitleRow,
                    static_cast<std::int16_t>(x - kTitleRowShift));
    }
    world.SlideRow(kCategoryTitleRow, kSlideToTarget, kTitleRowTargetX);
  }
  state.step = kListBuild;
  return 0;
}

// --- case 8: the selected item's action rows (234.c:2667) ---------------

struct ActionRows {
  std::array<TransferText, 2 * kActionRowCapacity> texts{};
  std::array<std::int32_t, kActionRowCapacity> types{};
  std::size_t count = 0;
};

std::int32_t OwnState(Category category, std::int32_t slot,
                      Items::ContainerWorld& records) {
  switch (category) {
    case Category::kWeapon:
      return ReadRecord<Items::InventoryWeapon>(records, category, slot)
          .is_equipped;
    case Category::kShield:
      return ReadRecord<Items::InventoryShield>(records, category, slot)
          .is_equipped;
    case Category::kArmor:
      return ReadRecord<Items::InventoryArmor>(records, category, slot).limb;
    default:
      return 0;
  }
}

bool KeyItem(std::int32_t slot, Items::ContainerWorld& records) {
  return ReadRecord<Items::InventoryMisc>(records, Category::kMisc, slot).id >=
         kFirstKeyItem;
}

bool ParentEquipped(Category parent_category, std::int32_t parent,
                    Items::ContainerWorld& records) {
  if (parent_category == Category::kWeapon) {
    return ReadRecord<Items::InventoryWeapon>(records, parent_category,
                                              parent - 1)
               .is_equipped != 0;
  }
  return ReadRecord<Items::InventoryShield>(records, parent_category,
                                            parent - 1)
             .is_equipped != 0;
}

struct TransferBlock {
  std::int32_t blocked = 0;
  std::uint32_t message = kItemCannotTransfer;
};

TransferBlock FindTransferBlock(Category category, std::int32_t slot,
                                TransferWorld& world) {
  auto& records = Active(world);
  TransferBlock block{};
  if (category == Category::kWeapon || category == Category::kShield ||
      category == Category::kArmor) {
    block.blocked = OwnState(category, slot, records);
    block.message = kItemInUse;
  } else if (category == Category::kMisc) {
    block.blocked = KeyItem(slot, records) ? 1 : 0;
  }
  const auto weapon =
      Items::ContainerParentWeaponIndex(category, slot, records);
  if (weapon != 0) {
    block.blocked = 1;
    block.message =
        world.MenuTextEntry(ParentEquipped(Category::kWeapon, weapon, records)
                                ? kParentEquippedMessage
                                : kParentStoredMessage);
  }
  const auto shield =
      Items::ContainerParentShieldIndex(category, slot, records);
  if (shield != 0) {
    block.blocked = 1;
    block.message =
        world.MenuTextEntry(ParentEquipped(Category::kShield, shield, records)
                                ? kParentEquippedMessage
                                : kParentStoredMessage);
  }
  return block;
}

std::int32_t StagedItemCount(TransferWorld& world) {
  std::int32_t total = 0;
  for (std::int32_t value = 0; value < kRecordCategoryCount; ++value) {
    total = WrappingAdd(
        total, Items::CountContainerItems(
                   CategoryAt(value), world.Records(TransferStorage::kStaged)));
  }
  return total;
}

// The misc stack's count left over once every staged stack of the same item
// is filled to 100.
std::int32_t MiscOverflow(std::int32_t slot, TransferWorld& world) {
  const auto item =
      ReadRecord<Items::InventoryMisc>(Active(world), Category::kMisc, slot);
  auto& staged = world.Records(TransferStorage::kStaged);
  std::int32_t remaining = item.count;
  for (std::int32_t row = 0; row < staged.Capacity(Category::kMisc); ++row) {
    const auto stack =
        ReadRecord<Items::InventoryMisc>(staged, Category::kMisc, row);
    if (stack.id == item.id) {
      remaining = WrappingAdd(remaining, stack.count - kMiscStackOverflowBase);
    }
  }
  return remaining;
}

// Returns whether the misc item cannot be transferred and the row's message.
std::pair<std::int32_t, std::uint32_t> MiscTransferBlock(std::int32_t slot,
                                                         TransferWorld& world) {
  const auto staged_total = StagedItemCount(world);
  world.PreviewTransferredMisc();
  auto room = world.CopyItem(Index(Category::kMisc), TransferStorage::kPreview,
                             slot, TransferStorage::kActive);
  if (world.Flag(TransferFlag::kBrowsingGadgetBag) == 0 &&
      room > kContainerMiscTransferLimit) {
    room = 0;
  }
  auto message = kMiscStorageFull;
  if (room != 0 && staged_total == kStagedTotalLimit) {
    room = MiscOverflow(slot, world) < 1 ? 1 : 0;
    message = kMiscStackFull;
  }
  return {room == 0 ? 1 : 0, message};
}

void AddTransferRow(Category category, std::int32_t slot, ActionRows& rows,
                    TransferWorld& world) {
  const auto block = FindTransferBlock(category, slot, world);
  std::int32_t unavailable = block.blocked;
  if (block.blocked != 0) {
    rows.texts[1] = MenuText(block.message);
  } else if (category == Category::kMisc) {
    const auto [misc_blocked, message] = MiscTransferBlock(slot, world);
    unavailable = misc_blocked;
    rows.texts[1] = MenuText(message);
  } else {
    unavailable = world.TransferCapacityError(category, slot);
    rows.texts[1] = MenuTextAt(
        world, static_cast<std::size_t>(unavailable) + kCapacityErrorMessage);
  }
  const std::size_t browsing = world.Flag(TransferFlag::kBrowsingGadgetBag);
  rows.texts[0] = MenuTextAt(world, browsing * 2 + kTransferActionName);
  if (unavailable != 0) {
    rows.types[0] = 1;
  } else {
    rows.texts[1] =
        MenuTextAt(world, browsing * 2 + kTransferActionDescription);
  }
  rows.count = 1;
}

std::int32_t DiscardBlock(Category category, std::int32_t slot,
                          Items::ContainerWorld& records) {
  std::int32_t block = 0;
  if (category == Category::kWeapon || category == Category::kShield ||
      category == Category::kArmor) {
    block = OwnState(category, slot, records) != 0 ? 1 : 0;
  } else if (category == Category::kMisc && KeyItem(slot, records)) {
    block = kKeyItemDiscardBlock;
  }
  const auto weapon =
      Items::ContainerParentWeaponIndex(category, slot, records);
  if (weapon != 0) {
    block = ParentEquipped(Category::kWeapon, weapon, records)
                ? 1
                : kStoredParentDiscardBlock;
  }
  const auto shield =
      Items::ContainerParentShieldIndex(category, slot, records);
  if (shield != 0) {
    block = ParentEquipped(Category::kShield, shield, records)
                ? 1
                : kStoredParentDiscardBlock;
  }
  return block;
}

void AddRow(ActionRows& rows, ItemListState& state, TransferText name,
            TransferText help, ItemAction action) {
  rows.texts[rows.count * 2] = name;
  rows.texts[rows.count * 2 + 1] = help;
  ++rows.count;
  state.action_ids[rows.count] = action;
}

void AddOptionalRows(Category category, std::int32_t slot, ActionRows& rows,
                     ItemListState& state, TransferWorld& world) {
  auto& records = Active(world);
  if (category == Category::kBlade || category == Category::kGrip ||
      category == Category::kGem) {
    auto help = ItemHelp(kJumpActionHelp);
    if (Items::ContainerParentWeaponIndex(category, slot, records) == 0 &&
        Items::ContainerParentShieldIndex(category, slot, records) == 0) {
      help = ItemHelp(kJumpActionUnattached);
      rows.types[1] = 1;
    }
    AddRow(rows, state, ItemHelp(kJumpActionName), help, kActionJumpToParent);
  }
  const auto discard_block = DiscardBlock(category, slot, records);
  auto discard_help = ItemHelp(kDiscardActionHelp);
  if (discard_block != 0) {
    discard_help = ItemHelp(world.ItemHelpEntry(
        static_cast<std::size_t>(discard_block) + kDiscardBlockedHelp));
  }
  rows.types[rows.count] = discard_block != 0 ? 1 : 0;
  AddRow(rows, state, ItemHelp(kDiscardActionName), discard_help,
         kActionDiscard);
  if (category == Category::kMisc) {
    auto help = ItemHelp(kDiscardOneHelp);
    if (KeyItem(slot, records)) {
      help = ItemHelp(kKeyItemHelp);
      rows.types[rows.count] = 1;
    }
    AddRow(rows, state, ItemHelp(kDiscardOneName), help, kActionDiscardOne);
  }
  AddRow(rows, state, ItemHelp(kSortActionName), ItemHelp(kSortActionHelp),
         kActionSort);
}

void BuildActionRows(Category category, ItemListState& state,
                     TransferWorld& world) {
  world.SetFlag(TransferFlag::kControlsMode, 0);
  const auto slot = static_cast<std::int32_t>(
                        OrderToken(world, category, state.selected_row)) -
                    1;
  ActionRows rows{};
  AddTransferRow(category, slot, rows, world);
  state.action_ids[1] = kActionTransfer;
  AddOptionalRows(category, slot, rows, state, world);
  world.CreateActionRows(
      static_cast<std::int32_t>(rows.count), Index(category) + kActionMenuBase,
      std::span(rows.texts).first(rows.count * 2), rows.types);
  state.step = kListActionMenu;
}

// --- case 9: the chosen action (234.c:2826) -----------------------------

std::int32_t SelectedSlot(Category category, ItemListState& state,
                          TransferWorld& world) {
  return static_cast<std::int32_t>(
             OrderToken(world, category, state.selected_row)) -
         1;
}

std::int32_t DiscardToken(Category category, ItemListState& state,
                          TransferWorld& world) {
  return static_cast<std::int32_t>(
             OrderToken(world, category, state.selected_row)) |
         static_cast<std::int32_t>(static_cast<std::uint32_t>(category)
                                   << kDiscardCategoryShift);
}

void RunAction(std::uint8_t action, Category category, ItemListState& state,
               TransferWorld& world) {
  switch (action) {
    case kActionTransfer:
      world.InitializeInformationBox();
      world.StageItemTransfer(category, SelectedSlot(category, state, world));
      world.ClearMenuExcept(kCategoryTitleRow);
      state.step = kListAwaitRebuild;
      break;
    case kActionJumpToParent:
      world.InitializeInformationBox();
      world.ClearMenuExcept(kCategoryTitleRow);
      state.step = kListJumpToParent;
      break;
    case kActionDiscard: {
      auto token = DiscardToken(category, state, world);
      if (category == Category::kMisc &&
          ReadRecord<Items::InventoryMisc>(Active(world), category,
                                           SelectedSlot(category, state, world))
                  .count >= 2) {
        token |= kDiscardStack;
      }
      static_cast<void>(world.UpdateDiscardMenu(token));
      state.step = kListDiscard;
      break;
    }
    case kActionDiscardOne:
      static_cast<void>(world.UpdateDiscardMenu(
          DiscardToken(category, state, world) | kDiscardSingle));
      state.step = kListDiscard;
      break;
    case kActionSort:
      world.InitializeInformationBox();
      static_cast<void>(world.UpdateSortMenu(Index(category) + 1));
      state.step = kListSort;
      break;
    default:
      break;
  }
}

std::int32_t UpdateActionChoice(Category category, ItemListState& state,
                                TransferWorld& world) {
  world.ShowActionInformation(world.HighlightedActionRow());
  world.UpdateActionMenu();
  const auto choice = WrappingAdd(world.ActionSelectionResult(), 1);
  if (choice == 0) return 0;
  world.FlyOutRowsLeftExcept(kNoMenuItem);
  if (choice > 0) {
    if (static_cast<std::size_t>(choice) >= state.action_ids.size()) {
      world.Refuse("action row outside the action identifiers");
    }
    world.PlaySound(TransferSound::kSelect);
    RunAction(state.action_ids[static_cast<std::size_t>(choice)], category,
              state, world);
    return 0;
  }
  world.PlaySound(TransferSound::kLeave);
  if (choice == kExit) {
    world.DismissInformationBox();
    return kExit;
  }
  state.step = kListInitialize;
  return 0;
}

// decomp: case 10 of func_80107490 (234.c:2880).
void JumpToParent(Category category, ItemListState& state,
                  TransferWorld& world) {
  if (!world.RowsIdle()) return;
  const auto slot = SelectedSlot(category, state, world);
  auto& records = Active(world);
  std::int32_t parent =
      Items::ContainerParentWeaponIndex(category, slot, records);
  auto parent_category = Category::kWeapon;
  if (parent == 0) {
    parent = Items::ContainerParentShieldIndex(category, slot, records);
    parent_category = Category::kShield;
  }
  const auto count = Items::CountContainerItems(parent_category, records);
  const auto found = Items::FindContainerItemRow(
      parent_category, parent - 1, CategoryOrder(world, parent_category),
      records);
  auto row = found - 1;
  std::int32_t page = 0;
  if (count > kVisibleItemRows && row >= kVisibleItemRows) {
    const auto last_page = count - kVisibleItemRows;
    if (last_page < row) {
      page = last_page;
      row -= page;
    } else {
      page = found - 2;
      row = 1;
    }
  }
  world.SetCategoryToken(
      static_cast<std::uint8_t>((Index(parent_category) + 1) & kCategoryMask));
  world.SetCategoryCursor(parent_category, static_cast<std::uint8_t>(row),
                          static_cast<std::uint8_t>(page));
  state.step = kListBuild;
}

std::int32_t AwaitDialog(std::int32_t result, bool reset_information,
                         ItemListState& state, TransferWorld& world) {
  if (result == 0) return 0;
  if (reset_information) world.InitializeInformationBox();
  world.FlyOutRowsLeftExcept(kNoMenuItem);
  world.ClearMenuExcept(kCategoryTitleRow);
  if (result == kExit) return kExit;
  state.step = kListAwaitRebuild;
  return 0;
}

// decomp: case 13 of func_80107490 (234.c:2934).
void BuildPendingList(Category category, ItemListState& state,
                      TransferWorld& world) {
  if (!world.RowsIdle()) return;
  world.AcquireItemRows();
  const auto count = world.BuildItemRows(Category::kManage);
  state.category_empty = count == 0;
  if (state.category_empty) {
    world.SetInformationMessage(MenuTextAt(
        world,
        world.Flag(TransferFlag::kBrowsingGadgetBag) + kEmptyPendingMessage));
  } else {
    world.CreateItemMenuRows(count, ItemRowsInfo(category));
  }
  world.ReleaseItemRows();
  state.step = kListAwaitConfirmation;
}

void CommitPendingTransfers(TransferWorld& world) {
  // The original loops until RebuildPendingTransfers drains the list; a list
  // that never drains is refused instead of hanging.
  for (std::size_t round = 0;; ++round) {
    const auto pending = world.PendingTransfers();
    if (pending.empty()) world.Refuse("missing pending-transfer list");
    const std::uint32_t token = pending[0];
    if (token == 0) return;
    if (round > kPendingTransferCount) {
      world.Refuse("pending transfers did not drain");
    }
    const auto category_value = token >> kTokenCategoryShift;
    if (category_value >= static_cast<std::uint32_t>(kRecordCategoryCount)) {
      world.Refuse("pending transfer category outside records");
    }
    const auto slot = static_cast<std::int32_t>((token - 1U) & kTokenSlotMask);
    static_cast<void>(world.CopyItem(
        static_cast<std::int32_t>(category_value) | kCopyItem,
        TransferStorage::kOther, slot, TransferStorage::kStaged));
    Items::ClearContainerItem(static_cast<Category>(category_value), slot,
                              world.Records(TransferStorage::kStaged));
    world.RebuildPendingTransfers();
  }
}

// decomp: case 15 of func_80107490 (234.c:2964).
std::int32_t ConfirmTransfers(ItemListState& state, TransferWorld& world) {
  const auto result = world.UpdateTransferConfirmation(0);
  if (result == 0) return 0;
  world.InitializeInformationBox();
  if (result < 0) {
    if (result == kExit) return kExit;
    state.step = kListAwaitReinitialize;
    return 0;
  }
  CommitPendingTransfers(world);
  for (std::int32_t value = 0; value < kRecordCategoryCount; ++value) {
    world.RebuildCategoryIndices(TransferStorage::kOther, CategoryAt(value));
  }
  world.ClearMenuExcept(kCategoryTitleRow);
  state.step = kListClose;
  return 0;
}

void AwaitRows(ItemListState& state, TransferWorld& world, ListStep next) {
  if (world.RowsIdle()) state.step = next;
}

StepResult RunEarlyStep(Category category, ItemListState& state,
                        TransferWorld& world) {
  switch (state.step) {
    case kListInitialize:
    case kListBuild:
      BuildList(category, state, world);
      return Done();
    case kListBrowse: {
      const auto browsed = Browse(category, state, world);
      if (browsed) return *browsed;
      return SwitchCategory(category, state, world);
    }
    case kListSwitchCategory:
      return SwitchCategory(category, state, world);
    case kListAwaitRebuild:
      AwaitRows(state, world, kListBuild);
      return Done();
    case kListAwaitReinitialize:
      AwaitRows(state, world, kListInitialize);
      return Done();
    case kListMove:
      return Done(MoveSelectedRow(category, state, world));
    case kListDetails:
      return Done(CloseDetails(state, world));
    default:
      return Done();
  }
}

std::int32_t RunLateStep(Category category, ItemListState& state,
                         TransferWorld& world) {
  switch (state.step) {
    case kListActions:
      BuildActionRows(category, state, world);
      return 0;
    case kListActionMenu:
      return UpdateActionChoice(category, state, world);
    case kListJumpToParent:
      JumpToParent(category, state, world);
      return 0;
    case kListDiscard:
      return AwaitDialog(world.UpdateDiscardMenu(0), true, state, world);
    case kListSort:
      return AwaitDialog(world.UpdateSortMenu(0), false, state, world);
    case kListPendingRows:
      BuildPendingList(category, state, world);
      return 0;
    case kListAwaitConfirmation:
      if (world.RowsIdle()) {
        static_cast<void>(world.CloseRowsAndRememberCursor());
        static_cast<void>(world.UpdateTransferConfirmation(1));
        state.step = kListConfirm;
      }
      return 0;
    case kListConfirm:
      return ConfirmTransfers(state, world);
    case kListClose:
      return world.RowsIdle() ? 1 : 0;
    default:
      return 0;
  }
}

// --- vs_menuD_exec (234.c:3130) helpers ---------------------------------

constexpr std::uint8_t kSlideSteps = 10;
constexpr std::uint8_t kIconSlideSteps = 4;
constexpr std::int32_t kSlideHiddenShift = 5;
constexpr std::uint32_t kPendingTitleCapacity = 64;
constexpr std::int32_t kTitleRowX = 0xB4;
constexpr std::int32_t kTitleRowY = 0x12;
constexpr std::int32_t kHeaderRowY = 0x22;
constexpr std::int32_t kTitleRowWidth = 0x8C;
constexpr std::int32_t kTitleRowFlags = 8;
constexpr std::uint8_t kReopenDelay = 5;

enum ScreenStep : std::uint8_t {
  kScreenOpen = 0,
  kScreenLoadText = 1,
  kScreenOpenDirection = 2,
  kScreenDirection = 3,
  kScreenOpenList = 4,
  kScreenList = 5,
  kScreenClose = 6,
};

constexpr std::uint32_t kYShift = 16U;

constexpr std::int32_t XY(std::int32_t x, std::int32_t y) {
  return WrappingAdd(
      x, static_cast<std::int32_t>(static_cast<std::uint32_t>(y) << kYShift));
}

void PlaceTitleRow(TransferText text, TransferWorld& world) {
  world.SetMenuItem({kCategoryTitleRow, kTitleRowX, kTitleRowY, kTitleRowWidth,
                     kTitleRowFlags, text});
  world.SetRowSelected(kCategoryTitleRow, true);
}

void CloseScreen(std::uint8_t& step, ScreenState& state, TransferWorld& world) {
  if (!world.UnloadItemTextWhenReady()) return;
  world.CommitGadgetBag();
  for (std::int32_t value = 0; value < kRecordCategoryCount; ++value) {
    world.RebuildCategoryIndices(TransferStorage::kContainer,
                                 CategoryAt(value));
  }
  std::int32_t value = 0;
  for (; value < kRecordCategoryCount; ++value) {
    if (Items::CountContainerItems(
            CategoryAt(value), world.Records(TransferStorage::kContainer)) !=
        0) {
      break;
    }
  }
  world.SetContainerEmpty(value == kRecordCategoryCount);
  world.ClearMenuExcept(kCategoryTitleRow);
  world.ReturnFromMenu(static_cast<std::uint8_t>(state.result + 2U));
  step = kScreenOpen;
}

void OpenScreen(std::uint8_t& step, ScreenState& state, TransferWorld& world) {
  static_cast<void>(world.LoadItemText(1));
  world.InitializeInformationBox();
  world.SetPanelVisibility(0);
  world.StoreContainerSnapshot();
  world.SnapshotGadgetBag();
  state.capacity_slide = 0;
  state.reopen_delay = 0;
  world.SetFlag(TransferFlag::kTitleSlide, 0);
  world.SetFlag(TransferFlag::kIconsSlide, 0);
  world.SetFlag(TransferFlag::kControlsSlide, 0);
  step = kScreenLoadText;
}

void UpdateDirectionStep(std::uint8_t& step, ScreenState& state,
                         TransferWorld& world) {
  state.result = static_cast<std::uint32_t>(world.UpdateTransferDirection(0));
  if (state.result == 0) return;
  if (state.result < kEndTransferResult) {
    world.ClearMenuExcept(kCategoryTitleRow);
    PlaceTitleRow(MenuTextAt(world, state.result + kScreenDirectionName),
                  world);
    step = kScreenOpenList;
  } else {
    step = kScreenClose;
  }
}

void UpdateListStep(std::uint8_t& step, ScreenState& state,
                    TransferWorld& world) {
  state.result = static_cast<std::uint32_t>(world.UpdateItemList(0));
  if (state.result == 0) return;
  world.SetPanelVisibility(0);
  if (state.result == static_cast<std::uint32_t>(kExit)) {
    step = kScreenClose;
    return;
  }
  PlaceTitleRow(MenuText(kContainerTitle), world);
  state.reopen_delay = kReopenDelay;
  step = kScreenOpenDirection;
}

// Returns true when vs_menuD_exec returns before drawing its panels.
bool RunScreenStep(std::uint8_t& step, ScreenState& state,
                   TransferWorld& world) {
  switch (step) {
    case kScreenOpen:
      OpenScreen(step, state, world);
      return true;
    case kScreenLoadText:
      step = static_cast<std::uint8_t>(step + world.LoadItemText(0));
      return false;
    case kScreenOpenDirection:
      if (state.reopen_delay != 0) {
        --state.reopen_delay;
      } else {
        static_cast<void>(world.UpdateTransferDirection(1));
        step = kScreenDirection;
      }
      return false;
    case kScreenDirection:
      UpdateDirectionStep(step, state, world);
      return false;
    case kScreenOpenList:
      if (world.RowsIdle()) {
        static_cast<void>(
            world.UpdateItemList(static_cast<std::int32_t>(state.result)));
        step = kScreenList;
      }
      return false;
    case kScreenList:
      UpdateListStep(step, state, world);
      return false;
    case kScreenClose: {
      const auto before = step;
      CloseScreen(step, state, world);
      return step != before;
    }
    default:
      return false;
  }
}

// Steps a slide counter toward kSlideSteps while shown and back to zero while
// hidden; returns the panel's offset.
std::int32_t SlidePanel(TransferFlag slide, bool shown, TransferWorld& world) {
  auto value = world.Flag(slide);
  if (shown) {
    if (value < kSlideSteps) world.SetFlag(slide, ++value);
    return world.RowAnimationStep(kSlideSteps - world.Flag(slide));
  }
  if (value != 0) world.SetFlag(slide, --value);
  return (kSlideSteps - world.Flag(slide)) << kSlideHiddenShift;
}

void UpdateCategoryHeader(std::int32_t offset, TransferWorld& world) {
  const auto category = SelectedCategory(world);
  const auto browsing = world.Flag(TransferFlag::kBrowsingGadgetBag) != 0;
  const auto pending_name = category == Category::kManage && browsing;
  world.SetMenuItem(
      {kCategoryHeaderRow, WrappingAdd(offset, kTitleRowX), kHeaderRowY,
       kTitleRowWidth, kTitleRowFlags,
       MenuTextAt(world, static_cast<std::size_t>(Index(category)) +
                             kCategoryName + (pending_name ? 1U : 0U))});
  world.SetRowSelected(kCategoryHeaderRow, true);
  if (category == Category::kManage) {
    const auto pending = world.PendingTransfers();
    std::uint16_t count = 0;
    while (count < kPendingTitleCapacity) {
      if (count >= pending.size()) world.Refuse("short pending-transfer list");
      if (pending[count] == 0) break;
      ++count;
    }
    world.SetRowCounts(kCategoryHeaderRow, count, kPendingTitleCapacity);
    return;
  }
  const std::uint16_t max = browsing ? world.InventoryCapacity(category)
                                     : world.ContainerCapacity(category);
  world.SetRowCounts(kCategoryHeaderRow,
                     static_cast<std::uint16_t>(
                         Items::CountContainerItems(category, Active(world))),
                     max);
}

void UpdateCategoryTitle(std::uint8_t step, TransferWorld& world) {
  const auto offset =
      SlidePanel(TransferFlag::kTitleSlide,
                 world.Flag(TransferFlag::kTitleVisible) != 0, world);
  if (world.Flag(TransferFlag::kTitleSlide) == 0 ||
      world.Flag(TransferFlag::kDetailsPhase) != 0 || step == kScreenClose) {
    return;
  }
  UpdateCategoryHeader(offset, world);
}

void UpdateCategoryIcons(TransferWorld& world) {
  auto value = world.Flag(TransferFlag::kIconsSlide);
  if (world.Flag(TransferFlag::kIconsVisible) != 0) {
    if (value < kIconSlideSteps) {
      world.SetFlag(TransferFlag::kIconsSlide, ++value);
    }
  } else if (value != 0) {
    world.SetFlag(TransferFlag::kIconsSlide, --value);
  }
  world.DrawCategoryIcons(world.Flag(TransferFlag::kIconsSlide),
                          world.Flag(TransferFlag::kDetailsPhase));
}

using Label = std::string_view;
constexpr std::array<Label, 2> kStorageNames{"GADGET BAG", "CONTAINER"};
constexpr auto kCategoryLabels = std::to_array<std::string_view>(
    {"WEAPON", "BLADE", "GRIP", "SHIELD", "ARMOR", "GEM", "MISC"});
static_assert(kCategoryLabels.size() == kRecordCategoryCount);
// vs_mainMenu_buttonIDs positions.
constexpr std::int32_t kIconTriangle = 0;
constexpr std::int32_t kIconSquare = 1;
constexpr std::int32_t kIconCross = 3;
constexpr std::int32_t kPromptBaseX = 0x10;
constexpr std::int32_t kPromptTop = 0x26;
constexpr std::int32_t kPromptBottom = 0x36;
constexpr std::int32_t kPromptTopWidth = 0x90;
constexpr std::int32_t kPromptBottomWidth = 0x48;
constexpr std::int32_t kPromptHeight = 0xA;
constexpr std::int32_t kIconLead = 8;
constexpr std::int32_t kIconTop = 0x24;
constexpr std::int32_t kIconBottom = 0x34;
constexpr std::int32_t kTriangleIconX = 0x40;
constexpr std::int32_t kPromptTextLead = 0xC;
constexpr std::int32_t kCancelTextX = 0x48;
constexpr std::uint32_t kHalfwordMask = 0xFFFF;
constexpr std::uint32_t kStatusColor = 0x202020;
constexpr std::uint32_t kEndColor = 0x808080;
constexpr std::uint32_t kStatusShiftMask = 3;
constexpr std::uint32_t kCapacityModeShift = 2;
constexpr std::int32_t kStorageNameY = 0x4C;
constexpr std::int32_t kCapacityRowsY = 0x58;
constexpr std::int32_t kCapacityRowStep = 12;
constexpr std::int32_t kCapacityCountX = 0x70;
constexpr std::int32_t kPendingCountY = 0xA0;
constexpr std::int32_t kGadgetBagMiscLimit = 0x100;
constexpr std::int32_t kContainerMiscLimit = 0x40;

void DrawControlsPrompt(std::int32_t x, TransferWorld& world) {
  world.DrawButtonPromptBackground(x, kPromptTop, kPromptTopWidth,
                                   kPromptHeight);
  world.DrawButtonPromptBackground(x, kPromptBottom, kPromptBottomWidth,
                                   kPromptHeight);
  const auto icon_x = WrappingSubtract(x, kIconLead);
  world.DrawButtonIcon(kIconSquare, icon_x, kIconTop);
  world.DrawButtonIcon(kIconTriangle, WrappingAdd(x, kTriangleIconX), kIconTop);
  world.DrawButtonIcon(kIconCross, icon_x, kIconBottom);
  const auto text_x = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(WrappingAdd(x, kPromptTextLead)) &
      kHalfwordMask);
  const auto mode = world.Flag(TransferFlag::kControlsMode);
  world.DrawCompactText("STATUS", text_x | XY(0, kPromptTop),
                        kStatusColor << (mode & kStatusShiftMask),
                        TransferTextLayer::kDefault);
  world.DrawCompactText("CANCEL", (text_x + kCancelTextX) | XY(0, kPromptTop),
                        std::nullopt, TransferTextLayer::kDefault);
  world.DrawCompactText("END", text_x | XY(0, kPromptBottom),
                        kEndColor >> (world.Flag(TransferFlag::kControlsMode) >>
                                      kCapacityModeShift),
                        TransferTextLayer::kDefault);
}

void DrawCapacityList(std::int32_t x, TransferWorld& world) {
  const auto browsing = world.Flag(TransferFlag::kBrowsingGadgetBag);
  if (browsing >= kStorageNames.size()) {
    world.Refuse("storage name outside table");
  }
  world.DrawCompactText(kStorageNames[browsing], XY(x, kStorageNameY),
                        std::nullopt, TransferTextLayer::kDefault);
  for (std::int32_t value = 0; value < kRecordCategoryCount; ++value) {
    const auto row_y = kCapacityRowsY + value * kCapacityRowStep;
    world.DrawCompactText(kCategoryLabels[static_cast<std::size_t>(value)],
                          XY(WrappingAdd(x, kIconLead), row_y), std::nullopt,
                          TransferTextLayer::kDefault);
    if (value == Index(Category::kMisc)) break;
    const auto category = CategoryAt(value);
    const auto projected =
        WrappingAdd(Items::CountContainerItems(
                        category, world.Records(TransferStorage::kOther)),
                    Items::CountContainerItems(
                        category, world.Records(TransferStorage::kStaged)));
    const std::int32_t capacity =
        world.Flag(TransferFlag::kBrowsingGadgetBag) != 0
            ? world.ContainerCapacity(category)
            : world.InventoryCapacity(category);
    world.DrawCapacityCount(
        capacity, WrappingAdd(x, XY(kCapacityCountX, row_y)), projected);
  }
  world.DrawCapacityCount(world.Flag(TransferFlag::kBrowsingGadgetBag) != 0
                              ? kGadgetBagMiscLimit
                              : kContainerMiscLimit,
                          WrappingAdd(x, XY(kCapacityCountX, kPendingCountY)),
                          world.TransferredMiscCount());
}

void UpdateControls(ScreenState& state, TransferWorld& world) {
  const auto shown = world.Flag(TransferFlag::kControlsMode) != 0;
  if (shown && world.Flag(TransferFlag::kControlsSlide) == 0) {
    world.RefreshMiscTransferCount();
  }
  const auto offset = SlidePanel(TransferFlag::kControlsSlide, shown, world);
  if (world.Flag(TransferFlag::kControlsSlide) == 0) return;
  auto x = WrappingSubtract(kPromptBaseX, offset);
  DrawControlsPrompt(x, world);
  std::int32_t capacity_offset = 0;
  if ((world.Flag(TransferFlag::kControlsMode) >> kCapacityModeShift) != 0) {
    if (state.capacity_slide < kSlideSteps) ++state.capacity_slide;
    capacity_offset = state.capacity_slide << kSlideHiddenShift;
  } else {
    if (state.capacity_slide != 0) --state.capacity_slide;
    capacity_offset = world.RowAnimationStep(state.capacity_slide);
  }
  x = WrappingSubtract(x, capacity_offset);
  DrawCapacityList(x, world);
}

// --- func_801089BC (234.c:3037) helpers ---------------------------------

constexpr std::int32_t kDirectionRowBase = 0xA;
constexpr std::int32_t kDirectionRowX = 0x140;
constexpr std::int32_t kDirectionRowY = 0x22;
constexpr std::int32_t kDirectionRowSpacing = 0x10;
constexpr std::int32_t kDirectionRowWidth = 0x7E;
constexpr std::int16_t kDirectionRowTargetX = 0xC2;
constexpr std::int32_t kDirectionCount = 3;
constexpr std::int32_t kEndTransferDirection = 2;
constexpr std::uint8_t kOkBlinkStep = 16;
constexpr std::int32_t kOkBlinkScale = 8;
constexpr std::int32_t kOkBrightnessShift = 5;
constexpr std::int32_t kOkBrightnessBase = 0x40;
constexpr std::int32_t kGreyScale = 0x10101;
constexpr std::int32_t kOkTextPosition = 0x4200C8;
constexpr std::int32_t kCursorX = 0xB4;
constexpr std::int32_t kCursorBaseY = 0x1A;
constexpr std::int32_t kCursorGapMask = 4;

enum DirectionStep : std::uint8_t {
  kDirectionPlaceFirst = 0,
  kDirectionPlaceSecond = 1,
  kDirectionAwaitRows = 2,
  kDirectionChoose = 3,
};

void PlaceDirectionRow(TransferDirectionState& state, TransferWorld& world) {
  const auto row = kDirectionRowBase + state.step;
  world.SetMenuItem(
      {row, kDirectionRowX, kDirectionRowY + state.step * kDirectionRowSpacing,
       kDirectionRowWidth, 0,
       MenuTextAt(world,
                  static_cast<std::size_t>(state.step) * 2 + kDirectionName)});
  world.SlideRow(row, kSlideToTarget, kDirectionRowTargetX);
  ++state.step;
}

void DrawDirectionPrompt(std::int32_t selection, TransferDirectionState& state,
                         TransferWorld& world) {
  state.ok_blink = static_cast<std::uint8_t>(state.ok_blink + kOkBlinkStep);
  const auto brightness =
      (Core::Math::Sine(state.ok_blink * kOkBlinkScale) >> kOkBrightnessShift) +
      kOkBrightnessBase;
  world.DrawCompactText(
      "OK", kOkTextPosition,
      static_cast<std::uint32_t>(WrappingMultiply(brightness, kGreyScale)),
      TransferTextLayer::kAboveMenu);
  const auto y = WrappingSubtract(
      WrappingMultiply(selection, kDirectionRowSpacing),
      WrappingSubtract(WrappingMultiply(selection, 2) & kCursorGapMask,
                       kCursorBaseY));
  state.cursor_animation = world.DrawCursor(
      state.cursor_animation,
      static_cast<std::int32_t>(static_cast<std::uint32_t>(y) << kYShift) |
          kCursorX);
}

std::optional<std::int32_t> ChooseDirection(TransferDirectionState& state,
                                            TransferWorld& world) {
  std::int32_t selection = world.DirectionSelection();
  for (std::int32_t row = 0; row < 2; ++row) {
    world.SetRowSelected(row + kDirectionRowBase, row == selection);
  }
  const auto pressed = world.PressedButtons();
  if ((pressed & kButtonTriangle) != 0) {
    world.PlaySound(TransferSound::kLeave);
    world.ClearMenuExcept(kNoMenuItem);
    return kExit;
  }
  if ((pressed & kButtonCross) != 0) {
    world.PlaySound(TransferSound::kLeave);
    return kCancel;
  }
  if ((pressed & kButtonCircle) != 0) {
    world.PlaySound(TransferSound::kSelect);
    return selection + 1;
  }
  DrawDirectionPrompt(selection, state, world);
  const auto repeated = world.RepeatedButtons();
  if ((repeated & kButtonUp) != 0) selection += 2;
  if ((repeated & kButtonDown) != 0) ++selection;
  if (selection >= kDirectionCount) selection -= kDirectionCount;
  if (selection != world.DirectionSelection()) {
    world.PlaySound(TransferSound::kChange);
    world.SetDirectionSelection(static_cast<std::uint16_t>(selection));
  }
  world.SetInformationMessage(
      selection != kEndTransferDirection
          ? MenuTextAt(world,
                       static_cast<std::size_t>(selection) * 2 + kDirectionHelp)
          : MenuText(kEndTransferHelp));
  return std::nullopt;
}
}  // namespace

// decomp: MENUD.PRG _setWeaponUi (234.c:810)
WeaponRow BuildWeaponRow(Framework::ItemTextHost& host,
                         const Items::InventoryWeapon& weapon,
                         Items::ContainerWorld& records,
                         std::span<std::uint8_t> buffer) {
  Items::WeaponLoadout loadout{};
  const auto status = BuildWeaponStats(loadout, &weapon, records);
  auto text = Framework::FormatWeaponRow(host, loadout, buffer);
  text.name = Framework::TextRef{.source = Framework::TextSource::kWeaponName};
  return {text, status};
}

// decomp: MENUD.PRG _setShieldUi (234.c:820)
Framework::ItemRowText BuildShieldRow(Framework::ItemTextHost& host,
                                      const Items::InventoryShield& shield,
                                      Items::ContainerWorld& records,
                                      std::span<std::uint8_t> buffer) {
  Items::ShieldLoadout loadout{};
  BuildShieldStats(loadout, &shield, records);
  return Framework::FormatShieldRow(host, loadout, buffer);
}

// decomp: MENUD.PRG func_80107490 (234.c:2393)
std::int32_t UpdateItemList(std::int32_t direction, ItemListState& state,
                            TransferWorld& world) {
  if (direction != 0) {
    InitializeItemList(direction, state, world);
    return 0;
  }
  for (;;) {
    const auto category = SelectedCategory(world);
    if (state.step >= kListActions) return RunLateStep(category, state, world);
    const auto result = RunEarlyStep(category, state, world);
    if (!result.restart) return result.value;
  }
}

// decomp: MENUD.PRG func_801089BC (234.c:3037)
std::int32_t UpdateTransferDirection(std::int32_t initialize,
                                     TransferDirectionState& state,
                                     TransferWorld& world) {
  if (initialize != 0) {
    if (state.cursor_reset_pending) {
      state.cursor_reset_pending = false;
      if (world.CursorMemorySetting() == 0) world.SetDirectionSelection(0);
    }
    state.ok_blink = 0;
    state.step = kDirectionPlaceFirst;
    return 0;
  }
  switch (state.step) {
    case kDirectionPlaceFirst:
    case kDirectionPlaceSecond:
      PlaceDirectionRow(state, world);
      break;
    case kDirectionAwaitRows:
      if (world.RowsIdle()) ++state.step;
      break;
    case kDirectionChoose: {
      const auto chosen = ChooseDirection(state, world);
      if (chosen) return *chosen;
      break;
    }
    default:
      break;
  }
  world.DrawStorageCapacities();
  return 0;
}

// decomp: MENUD.PRG vs_menuD_exec (234.c:3130)
std::int32_t UpdateScreen(std::uint8_t& step, ScreenState& state,
                          TransferWorld& world) {
  world.PublishActiveRecords();
  if (RunScreenStep(step, state, world)) return 0;
  UpdateCategoryTitle(step, world);
  UpdateCategoryIcons(world);
  UpdateControls(state, world);
  return 0;
}
}  // namespace VS::Menu::container
