#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "Items/ContainerTables.hpp"
#include "Menu/Container/ContainerListTransfer.hpp"
#include "Menu/ContainerPendingRowsWorld.hpp"

namespace {
namespace rows_tests = container_pending_rows_tests;
namespace transfer = VS::Menu::container;
namespace items = VS::Items;
namespace framework = VS::Menu::Framework;
using Category = items::ItemCategory;
using transfer::TransferFlag;
using transfer::TransferStorage;
using transfer::TransferText;

constexpr std::array<std::size_t, 7> kOffsets{0,      0xA00,  0x1500, 0x400,
                                              0x1900, 0x2300, 0x3800};
constexpr std::size_t kBlockBytes = 0x3C00;
constexpr std::size_t kDescriptionBytes = 96;
constexpr std::size_t kCodes = 64;
constexpr std::size_t kRows = 128;
constexpr std::size_t kMenuTextEntries = 48;
constexpr std::size_t kStorages = 7;
constexpr std::uint32_t kMask = 0xFFFFFFFFU;
using Bytes = std::array<std::uint8_t, kBlockBytes>;

// Call codes shared with container_transfer_common.py.
enum Code : std::uint32_t {
  kSound = 1,
  kClearMenu,
  kFlyOut,
  kRowsIdle,
  kMenuItem,
  kMenuItemText,
  kText,
  kTextStyle,
  kCursor,
  kInformation,
  kCapacities,
  kLoadText,
  kInitInformation,
  kPanel,
  kStoreSnapshot,
  kSnapshotBag,
  kCommitBag,
  kRebuildIndices,
  kUnloadText,
  kItemList,
  kDirectionMenu,
  kIcons,
  kRefreshMisc,
  kPrompt,
  kPromptSize,
  kButton,
  kCapacityCount,
  kClearMemory,
  kClearWords,
  kAcquire,
  kRelease,
  kBuildRows,
  kMarkRow,
  kCreateRows,
  kRestoreRows,
  kSelectedRow,
  kCloseRows,
  kDetails,
  kUndo,
  kStage,
  kCapacityError,
  kPreviewMisc,
  kCopy,
  kCopySource,
  kRebuildPending,
  kActionRows,
  kActionText,
  kActionTypes,
  kHighlighted,
  kShowAction,
  kUpdateActions,
  kActionResult,
  kDismiss,
  kDiscard,
  kSort,
  kConfirm,
};
static_assert(kConfirm == 56);

// The record fields container_transfer_list.py writes by byte offset.
static_assert(offsetof(items::InventoryWeapon, blade) == 1);
static_assert(offsetof(items::InventoryWeapon, grip) == 2);
static_assert(offsetof(items::InventoryWeapon, is_equipped) == 3);
static_assert(offsetof(items::InventoryBlade, assembled_weapon_index) == 42);
static_assert(offsetof(items::InventoryGrip, assembled_weapon_index) == 12);
static_assert(offsetof(items::InventoryShield, is_equipped) == 1);
static_assert(offsetof(items::InventoryShield, base) == 4);
static_assert(offsetof(items::InventoryArmor, limb) == 38);
static_assert(offsetof(items::InventoryGem, set_item_index) == 24);
static_assert(offsetof(items::InventoryMisc, count) == 2);
constexpr std::size_t kMenuMemoryBytes = 0x12;
constexpr std::size_t kHelpEntries = 4;
constexpr std::size_t kDiscardHelpEntry = 663;
constexpr std::uint32_t kHeapBytes = 0x6C00;
constexpr std::uint32_t kHeapPointers = 7;
constexpr std::uint32_t kPendingWord = 7U << 24U;
constexpr std::uint32_t kPendingBytes = 0x80;

std::int64_t Read() {
  std::int64_t value = 0;
  if (!(std::cin >> value)) std::exit(2);
  return value;
}

template <typename Range>
void ReadRange(Range& values) {
  for (auto& value : values) {
    value = static_cast<std::remove_reference_t<decltype(value)>>(Read());
  }
}

Bytes Encode(items::ContainerStorage& storage) {
  Bytes bytes{};
  for (std::size_t category = 0; category < kOffsets.size(); ++category) {
    items::Live::VisitContainerTable(
        storage.items, static_cast<Category>(category), [&](auto& table) {
          std::memcpy(bytes.data() + kOffsets[category], table.data(),
                      table.size() * sizeof(table[0]));
        });
  }
  return bytes;
}

std::uint32_t Hash(std::span<const std::uint8_t> data) {
  std::uint32_t value = 0x811C9DC5U;
  for (const auto byte : data) value = (value ^ byte) * 0x01000193U;
  return value & 0x7FFFFFFFU;
}

template <typename Halfwords>
void AppendHalfwords(std::vector<std::uint8_t>& bytes,
                     const Halfwords& values) {
  for (const auto value : values) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8U));
  }
}

void Decode(items::ContainerStorage& storage, const Bytes& bytes) {
  for (std::size_t category = 0; category < kOffsets.size(); ++category) {
    items::Live::VisitContainerTable(
        storage.items, static_cast<Category>(category), [&](auto& table) {
          std::memcpy(table.data(), bytes.data() + kOffsets[category],
                      table.size() * sizeof(table[0]));
        });
  }
}

std::uint32_t StringHash(std::string_view text) {
  std::uint32_t value = 0x811C9DC5U;
  for (const auto character : text) {
    value = (value ^ static_cast<std::uint8_t>(character)) * 0x01000193U;
  }
  return value & 0x7FFFFFFFU;
}

std::uint32_t TextWord(TransferText text) {
  return (text.table == transfer::TransferTextTable::kItemHelp ? 1U << 24U
                                                                : 0U) |
         text.offset;
}

std::uint32_t OwnerWord(TransferStorage storage) {
  return static_cast<std::uint32_t>(storage) << 24U;
}

struct Row {
  std::uint8_t state = 0;
  std::uint8_t selected = 0;
  std::uint8_t outset = 0;
  std::uint16_t count = 0;
  std::uint16_t max = 0;
  std::int16_t x = 0;
  std::int16_t target = 0;
};

struct Storage final : items::ContainerWorld {
  items::ContainerStorage storage;
  std::uint16_t Capacity(Category category) override {
    return storage.Capacity(category);
  }
  items::ContainerRecord Read(Category category, std::size_t index) override {
    return storage.Read(category, index);
  }
  void Write(Category category, std::size_t index,
             const items::ContainerRecord& record) override {
    storage.Write(category, index, record);
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    throw std::out_of_range(std::string(reason));
  }
};

// Records every side effect as the four words the original's stubs record and
// answers queries from the vector's per-code queues.
struct Recorder final : transfer::TransferWorld {
  std::vector<std::array<std::uint32_t, 4>> calls;
  std::array<std::deque<std::int32_t>, kCodes> queues{};
  std::array<std::uint8_t, 8> flags{};
  std::uint32_t settings = 0;
  std::uint16_t menu_state = 0;
  std::uint16_t selection = 0;
  std::uint8_t token = 0;
  std::uint32_t pressed = 0;
  std::uint32_t held = 0;
  std::uint32_t repeated = 0;
  std::uint16_t misc = 0;
  bool published = false;
  std::array<std::uint16_t, kMenuTextEntries> menu_text{};
  std::array<std::uint8_t, 11> animation{};
  std::array<std::uint8_t, 7> inventory_capacities{};
  std::array<std::uint16_t, 7> container_capacities{};
  std::array<Storage, kStorages> storages{};
  std::array<std::uint16_t, transfer::kPendingTransferCount> pending{};
  std::array<Row, kRows> rows{};
  std::array<std::uint8_t, kMenuMemoryBytes> memory{};
  std::array<std::uint16_t, kHelpEntries> help{};
  std::array<std::uint32_t, kRows * 2> marks{};
  std::uint8_t freeze = 0;
  std::uint32_t active_word = 0;
  std::uint32_t other_word = 1U << 24U;

  void Record(std::uint32_t code, std::uint32_t a = 0, std::uint32_t b = 0,
              std::uint32_t c = 0) {
    calls.push_back({code, a, b, c});
  }
  std::int32_t Answer(std::uint32_t code) {
    auto& queue = queues.at(code);
    if (queue.empty()) return 0;
    const auto value = queue.front();
    if (queue.size() > 1) queue.pop_front();
    return value;
  }
  Row& RowAt(std::int32_t row) {
    return rows.at(static_cast<std::size_t>(row) & (kRows - 1));
  }

  [[noreturn]] void Refuse(std::string_view reason) override {
    throw std::out_of_range(std::string(reason));
  }
  std::uint8_t Flag(TransferFlag flag) override {
    return flags.at(static_cast<std::size_t>(flag));
  }
  void SetFlag(TransferFlag flag, std::uint8_t value) override {
    flags.at(static_cast<std::size_t>(flag)) = value;
  }
  std::uint8_t CursorMemorySetting() override {
    return static_cast<std::uint8_t>(settings >> 8U);
  }
  void SetCursorMemorySetting(std::uint8_t value) override {
    settings = (settings & ~0xFF00U) | static_cast<std::uint32_t>(value) << 8U;
  }
  void SetContainerEmpty(bool empty) override {
    settings = (settings & ~0x10U) | (empty ? 0x10U : 0U);
  }
  void SetFreezeTabArrows(std::uint8_t value) override { freeze = value; }
  std::uint8_t CategoryToken() override { return token; }
  void SetCategoryToken(std::uint8_t value) override { token = value; }
  void ClearCategoryMemory() override {
    token = 0;
    memory.fill(0);
    Record(kClearMemory, kMenuMemoryBytes);
  }
  void SetCategoryCursor(Category category, std::uint8_t row,
                         std::uint8_t page) override {
    const auto index = static_cast<std::size_t>(category) * 2 + 2;
    memory.at(index) = row;
    memory.at(index + 1) = page;
  }
  std::uint16_t DirectionSelection() override { return selection; }
  void SetDirectionSelection(std::uint16_t value) override {
    selection = value;
  }
  void ReturnFromMenu(std::uint8_t state) override {
    menu_state = static_cast<std::uint16_t>((menu_state >> 8U) |
                                            static_cast<unsigned>(state) << 8U);
  }
  std::uint32_t PressedButtons() override { return pressed; }
  std::uint32_t HeldButtons() override { return held; }
  std::uint32_t RepeatedButtons() override { return repeated; }
  std::uint16_t MenuTextEntry(std::size_t index) override {
    return menu_text.at(index);
  }
  std::uint16_t ItemHelpEntry(std::size_t index) override {
    return help.at(index - kDiscardHelpEntry);
  }
  std::uint8_t RowAnimationStep(std::size_t index) override {
    return animation.at(index);
  }
  std::uint8_t InventoryCapacity(Category category) override {
    return inventory_capacities.at(static_cast<std::size_t>(category));
  }
  std::uint16_t ContainerCapacity(Category category) override {
    return container_capacities.at(static_cast<std::size_t>(category));
  }
  std::uint16_t TransferredMiscCount() override { return misc; }

  items::ContainerWorld& Records(TransferStorage storage) override {
    return storages.at(static_cast<std::size_t>(storage));
  }
  std::span<std::uint16_t> Indices(TransferStorage storage) override {
    return storages.at(static_cast<std::size_t>(storage)).storage.indices;
  }
  std::span<std::uint16_t> PendingTransfers() override { return pending; }
  void SelectStorages(bool browsing) override {
    constexpr std::uint32_t kContainerWord = 4U << 24U;
    constexpr std::uint32_t kBagWord = 5U << 24U;
    active_word = browsing ? kBagWord : kContainerWord;
    other_word = browsing ? kContainerWord : kBagWord;
  }
  void ClearStagedTransfers() override {
    Record(kClearWords, OwnerWord(TransferStorage::kStaged), kBlockBytes);
    Record(kClearWords, kPendingWord, kPendingBytes);
    auto& staged = storages.at(static_cast<std::size_t>(TransferStorage::kStaged));
    staged.storage.items = {};
    pending.fill(0);
  }
  void PublishActiveRecords() override { published = true; }
  void StoreContainerSnapshot() override {
    Record(kStoreSnapshot, OwnerWord(TransferStorage::kContainer), 6U << 24U,
           kBlockBytes);
  }
  void SnapshotGadgetBag() override { Record(kSnapshotBag); }
  void CommitGadgetBag() override { Record(kCommitBag); }
  void RebuildCategoryIndices(TransferStorage storage,
                              Category category) override {
    Record(kRebuildIndices, OwnerWord(storage),
           static_cast<std::uint32_t>(category));
  }
  void RebuildPendingTransfers() override {
    Record(kRebuildPending);
    std::copy(pending.begin() + 1, pending.end(), pending.begin());
    pending.back() = 0;
  }
  void PreviewTransferredMisc() override { Record(kPreviewMisc); }
  void RefreshMiscTransferCount() override { Record(kRefreshMisc); }
  std::int32_t CopyItem(std::int32_t flags, TransferStorage target,
                        std::int32_t slot, TransferStorage source) override {
    Record(kCopy, static_cast<std::uint32_t>(flags), OwnerWord(target),
           static_cast<std::uint32_t>(slot));
    Record(kCopySource, OwnerWord(source));
    return Answer(kCopy);
  }
  std::int32_t TransferCapacityError(Category category,
                                     std::int32_t slot) override {
    Record(kCapacityError, static_cast<std::uint32_t>(category),
           static_cast<std::uint32_t>(slot));
    return Answer(kCapacityError);
  }
  void StageItemTransfer(Category category, std::int32_t slot) override {
    Record(kStage, static_cast<std::uint32_t>(category),
           static_cast<std::uint32_t>(slot));
  }
  void UndoPendingTransfer(std::int32_t row) override {
    Record(kUndo, static_cast<std::uint32_t>(row));
  }

  void AcquireItemRows() override {
    marks.fill(0);
    Record(kAcquire, kHeapBytes);
  }
  std::int32_t BuildItemRows(Category category) override {
    Record(kBuildRows, static_cast<std::uint32_t>(category), kHeapPointers);
    return Answer(kBuildRows);
  }
  void MarkItemRow(std::size_t row, std::uint32_t flags) override {
    marks.at(row) |= flags;
  }
  void CreateItemMenuRows(std::int32_t count, std::int32_t info) override {
    for (std::size_t row = 0; row < marks.size(); ++row) {
      if (marks[row] != 0) {
        Record(kMarkRow, static_cast<std::uint32_t>(row), marks[row]);
      }
    }
    Record(kCreateRows, static_cast<std::uint32_t>(count),
           static_cast<std::uint32_t>(info),
           ((settings >> 8U) & 0xFFU) | 0x100U);
  }
  void ReleaseItemRows() override { Record(kRelease, 1); }
  void RestoreItemRows(Category category) override {
    Record(kRestoreRows, static_cast<std::uint32_t>(category));
  }
  void SetMenuItem(const transfer::MenuItemPlacement& placement) override {
    Record(kMenuItem, static_cast<std::uint32_t>(placement.row),
           static_cast<std::uint32_t>(placement.x),
           static_cast<std::uint32_t>(placement.y));
    Record(kMenuItemText, static_cast<std::uint32_t>(placement.width),
           static_cast<std::uint32_t>(placement.flags),
           TextWord(placement.text));
  }
  void SetRowSelected(std::int32_t row, bool value) override {
    RowAt(row).selected = value ? 1 : 0;
  }
  void ClearRowOutsetIcon(std::int32_t row) override { RowAt(row).outset = 0; }
  std::int16_t RowX(std::int32_t row) override { return RowAt(row).x; }
  void SetRowX(std::int32_t row, std::int16_t x) override { RowAt(row).x = x; }
  void SlideRow(std::int32_t row, std::uint8_t state,
                std::int16_t target) override {
    RowAt(row).state = state;
    RowAt(row).target = target;
  }
  void SetRowCounts(std::int32_t row, std::uint16_t count,
                    std::uint16_t max) override {
    RowAt(row).count = count;
    RowAt(row).max = max;
  }
  void ClearMenuExcept(std::int32_t row) override {
    Record(kClearMenu, static_cast<std::uint32_t>(row));
  }
  void FlyOutRowsLeftExcept(std::int32_t row) override {
    Record(kFlyOut, static_cast<std::uint32_t>(row));
  }
  std::int32_t SelectedRow() override {
    Record(kSelectedRow);
    return Answer(kSelectedRow);
  }
  bool RowsIdle() override {
    Record(kRowsIdle);
    return Answer(kRowsIdle) != 0;
  }
  std::int32_t CloseRowsAndRememberCursor() override {
    Record(kCloseRows);
    return Answer(kCloseRows);
  }
  void CreateActionRows(std::int32_t count, std::int32_t menu,
                        std::span<const TransferText> texts,
                        std::span<const std::int32_t> types) override {
    Record(kActionRows, static_cast<std::uint32_t>(count),
           static_cast<std::uint32_t>(menu));
    for (const auto text : texts) Record(kActionText, TextWord(text));
    const auto type = [&](std::size_t index) {
      return static_cast<std::uint32_t>(types[index]);
    };
    Record(kActionTypes, type(0), type(1), type(2));
    Record(kActionTypes, type(3));
  }
  std::int32_t HighlightedActionRow() override {
    Record(kHighlighted);
    return Answer(kHighlighted);
  }
  void ShowActionInformation(std::int32_t row) override {
    Record(kShowAction, static_cast<std::uint32_t>(row));
  }
  void UpdateActionMenu() override { Record(kUpdateActions); }
  std::int32_t ActionSelectionResult() override {
    Record(kActionResult);
    return Answer(kActionResult);
  }

  void InitializeInformationBox() override { Record(kInitInformation); }
  void DismissInformationBox() override { Record(kDismiss); }
  void SetInformationMessage(TransferText text) override {
    Record(kInformation, TextWord(text));
  }
  void SetPanelVisibility(std::uint8_t value) override { Record(kPanel, value); }
  void PlaySound(transfer::TransferSound sound) override {
    Record(kSound, static_cast<std::uint32_t>(sound));
  }
  std::int32_t LoadItemText(std::int32_t load) override {
    Record(kLoadText, static_cast<std::uint32_t>(load));
    return Answer(kLoadText);
  }
  bool UnloadItemTextWhenReady() override {
    Record(kUnloadText);
    return Answer(kUnloadText) != 0;
  }
  std::int32_t Dialog(std::uint32_t code, std::int32_t value) {
    Record(code, static_cast<std::uint32_t>(value));
    return Answer(code);
  }
  std::int32_t UpdateItemDetails(std::int32_t value) override {
    return Dialog(kDetails, value);
  }
  std::int32_t UpdateTransferConfirmation(std::int32_t value) override {
    return Dialog(kConfirm, value);
  }
  std::int32_t UpdateDiscardMenu(std::int32_t value) override {
    return Dialog(kDiscard, value);
  }
  std::int32_t UpdateSortMenu(std::int32_t value) override {
    return Dialog(kSort, value);
  }
  std::int32_t UpdateItemList(std::int32_t value) override {
    Record(kItemList, static_cast<std::uint32_t>(value));
    return Answer(kItemList);
  }
  std::int32_t UpdateTransferDirection(std::int32_t initialize) override {
    Record(kDirectionMenu, static_cast<std::uint32_t>(initialize));
    return Answer(kDirectionMenu);
  }

  void DrawCompactText(std::string_view text, std::int32_t xy,
                       std::optional<std::uint32_t> color,
                       transfer::TransferTextLayer layer) override {
    Record(kText, StringHash(text), static_cast<std::uint32_t>(xy));
    Record(kTextStyle, color ? *color : kMask,
           layer == transfer::TransferTextLayer::kAboveMenu ? 1U : 0U);
  }
  std::uint8_t DrawCursor(std::uint8_t animation_step,
                          std::int32_t position) override {
    Record(kCursor, animation_step, static_cast<std::uint32_t>(position));
    return static_cast<std::uint8_t>(Answer(kCursor));
  }
  void DrawStorageCapacities() override { Record(kCapacities); }
  void DrawCategoryIcons(std::uint8_t step, std::uint8_t phase) override {
    Record(kIcons, step, phase);
  }
  void DrawButtonPromptBackground(std::int32_t x, std::int32_t y,
                                  std::int32_t width,
                                  std::int32_t height) override {
    Record(kPrompt, static_cast<std::uint32_t>(x),
           static_cast<std::uint32_t>(y));
    Record(kPromptSize, static_cast<std::uint32_t>(width),
           static_cast<std::uint32_t>(height));
  }
  void DrawButtonIcon(std::int32_t button, std::int32_t x,
                      std::int32_t y) override {
    Record(kButton, static_cast<std::uint32_t>(button),
           static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
  }
  void DrawCapacityCount(std::int32_t limit, std::int32_t xy,
                         std::int32_t count) override {
    Record(kCapacityCount, static_cast<std::uint32_t>(limit),
           static_cast<std::uint32_t>(xy), static_cast<std::uint32_t>(count));
  }
};

void ReadScript(Recorder& world) {
  for (auto& queue : world.queues) {
    const auto count = Read();
    for (std::int64_t index = 0; index < count; ++index) {
      queue.push_back(static_cast<std::int32_t>(Read()));
    }
  }
}

void ReadSparseRecords(Recorder& world) {
  constexpr std::array<TransferStorage, 4> kOwners{
      TransferStorage::kActive, TransferStorage::kOther,
      TransferStorage::kStaged, TransferStorage::kContainer};
  std::array<Bytes, kOwners.size()> blocks{};
  const auto count = Read();
  for (std::int64_t entry = 0; entry < count; ++entry) {
    const auto owner = static_cast<std::size_t>(Read());
    const auto offset = static_cast<std::size_t>(Read());
    blocks.at(owner).at(offset) = static_cast<std::uint8_t>(Read());
  }
  for (std::size_t owner = 0; owner < kOwners.size(); ++owner) {
    Decode(world.storages.at(static_cast<std::size_t>(kOwners[owner])).storage,
           blocks[owner]);
  }
}

void PrintRows(const Recorder& world) {
  for (const auto& row : world.rows) {
    std::cout << ' ' << unsigned{row.state} << ' ' << unsigned{row.selected}
              << ' ' << unsigned{row.outset} << ' ' << row.count << ' '
              << row.max << ' ' << static_cast<std::uint16_t>(row.x) << ' '
              << static_cast<std::uint16_t>(row.target);
  }
}

void PrintCalls(const Recorder& world) {
  std::cout << ' ' << world.calls.size();
  for (const auto& call : world.calls) {
    for (const auto word : call) std::cout << ' ' << word;
  }
  std::cout << '\n';
}

void PrintText(const std::optional<framework::TextRef>& text, std::size_t slot,
               bool description) {
  if (!text) {
    std::cout << " 0 " << 0x50000000U + (description ? 4U : 0U);
    return;
  }
  auto offset = text->offset;
  if (text->source == framework::TextSource::kWeaponName) {
    offset = slot * sizeof(items::InventoryWeapon) +
             offsetof(items::InventoryWeapon, name);
  }
  std::cout << ' ' << static_cast<unsigned>(text->source) << ' ' << offset;
}

void RunRow(int kind) {
  const auto slot = static_cast<std::size_t>(Read());
  rows_tests::Records records;
  Bytes bytes{};
  ReadRange(bytes);
  Decode(records.storage, bytes);
  rows_tests::RowTextHost host;
  std::array<std::uint8_t, kDescriptionBytes> buffer{};
  framework::ItemRowText text{};
  if (kind == 0) {
    text = transfer::BuildWeaponRow(host,
                                    records.storage.items.weapons.at(slot),
                                    records, buffer)
               .text;
  } else {
    text = transfer::BuildShieldRow(
        host, records.storage.items.shields.at(slot), records, buffer);
  }
  std::cout << text.row_type;
  PrintText(text.name, slot, false);
  PrintText(text.description, slot, true);
  for (const auto byte : buffer) std::cout << ' ' << unsigned{byte};
  std::cout << '\n';
}

void RunDirection() {
  Recorder world;
  const auto initialize = static_cast<std::int32_t>(Read());
  transfer::TransferDirectionState state{};
  state.step = static_cast<std::uint8_t>(Read());
  state.ok_blink = static_cast<std::uint8_t>(Read());
  state.cursor_animation = static_cast<std::uint8_t>(Read());
  state.cursor_reset_pending = Read() != 0;
  world.settings = static_cast<std::uint32_t>(Read()) << 8U;
  world.selection = static_cast<std::uint16_t>(Read());
  world.pressed = static_cast<std::uint32_t>(Read());
  world.repeated = static_cast<std::uint32_t>(Read());
  world.rows.at(10).selected = static_cast<std::uint8_t>(Read());
  world.rows.at(11).selected = static_cast<std::uint8_t>(Read());
  ReadRange(world.menu_text);
  ReadScript(world);
  const auto result =
      transfer::UpdateTransferDirection(initialize, state, world);
  std::cout << result << ' ' << unsigned{state.step} << ' '
            << unsigned{state.ok_blink} << ' '
            << unsigned{state.cursor_animation} << ' '
            << (state.cursor_reset_pending ? 1 : 0) << ' ' << world.selection;
  PrintRows(world);
  PrintCalls(world);
}

void ReadTables(Recorder& world) {
  ReadRange(world.menu_text);
  ReadRange(world.animation);
  ReadRange(world.inventory_capacities);
  ReadRange(world.container_capacities);
}

void RunScreen() {
  Recorder world;
  auto step = static_cast<std::uint8_t>(Read());
  transfer::ScreenState state{};
  state.result = static_cast<std::uint32_t>(Read());
  state.reopen_delay = static_cast<std::uint8_t>(Read());
  state.capacity_slide = static_cast<std::uint8_t>(Read());
  ReadRange(world.flags);
  world.token = static_cast<std::uint8_t>(Read());
  world.settings = static_cast<std::uint32_t>(Read());
  world.menu_state = static_cast<std::uint16_t>(Read());
  world.misc = static_cast<std::uint16_t>(Read());
  const auto pending = static_cast<std::size_t>(Read());
  for (std::size_t index = 0; index < pending; ++index) {
    world.pending.at(index) = static_cast<std::uint16_t>(0x101 + index);
  }
  ReadTables(world);
  ReadSparseRecords(world);
  ReadScript(world);
  const auto result = transfer::UpdateScreen(step, state, world);
  std::cout << result << ' ' << unsigned{step} << ' ' << state.result << ' '
            << unsigned{state.reopen_delay} << ' '
            << unsigned{state.capacity_slide};
  for (const auto flag : world.flags) std::cout << ' ' << unsigned{flag};
  std::cout << ' ' << world.settings << ' ' << world.menu_state << ' '
            << (world.published ? 1 : 0);
  PrintRows(world);
  PrintCalls(world);
}
void ReadList(Recorder& world, std::int32_t& direction,
              transfer::ItemListState& state) {
  direction = static_cast<std::int32_t>(Read());
  state.step = static_cast<std::uint8_t>(Read());
  state.category_empty = Read() != 0;
  state.first_entry = Read() != 0;
  state.selected_row = static_cast<std::int32_t>(Read());
  ReadRange(state.action_ids);
  ReadRange(world.flags);
  ReadRange(world.memory);
  world.token = world.memory[0];
  world.settings = static_cast<std::uint32_t>(Read());
  world.freeze = static_cast<std::uint8_t>(Read());
  world.pressed = static_cast<std::uint32_t>(Read());
  world.held = static_cast<std::uint32_t>(Read());
  world.repeated = static_cast<std::uint32_t>(Read());
  for (auto& row : world.rows) row.outset = 1;
  world.rows[0].x = static_cast<std::int16_t>(Read());
  ReadRange(world.menu_text);
  ReadRange(world.help);
  ReadSparseRecords(world);
  auto& active =
      world.storages.at(static_cast<std::size_t>(TransferStorage::kActive));
  const auto indices = Read();
  for (std::int64_t entry = 0; entry < indices; ++entry) {
    const auto index = static_cast<std::size_t>(Read());
    active.storage.indices.at(index) = static_cast<std::uint16_t>(Read());
  }
  const auto pending = Read();
  for (std::int64_t entry = 0; entry < pending; ++entry) {
    world.pending.at(static_cast<std::size_t>(entry)) =
        static_cast<std::uint16_t>(Read());
  }
  ReadScript(world);
}

void RunList() {
  Recorder world;
  std::int32_t direction = 0;
  transfer::ItemListState state{};
  ReadList(world, direction, state);
  const auto result = transfer::UpdateItemList(direction, state, world);
  world.memory[0] = world.token;
  std::cout << result << ' ' << unsigned{state.step} << ' '
            << (state.category_empty ? 1 : 0) << ' '
            << (state.first_entry ? 1 : 0) << ' ' << state.selected_row;
  for (const auto id : state.action_ids) std::cout << ' ' << unsigned{id};
  for (const auto flag : world.flags) std::cout << ' ' << unsigned{flag};
  for (const auto byte : world.memory) std::cout << ' ' << unsigned{byte};
  std::cout << ' ' << world.settings << ' ' << unsigned{world.freeze} << ' '
            << world.active_word << ' ' << world.other_word;
  auto& active =
      world.storages.at(static_cast<std::size_t>(TransferStorage::kActive));
  const auto active_bytes = Encode(active.storage);
  std::vector<std::uint8_t> active_block(active_bytes.begin(),
                                         active_bytes.end());
  AppendHalfwords(active_block, active.storage.indices);
  const auto staged_bytes = Encode(
      world.storages.at(static_cast<std::size_t>(TransferStorage::kStaged))
          .storage);
  std::vector<std::uint8_t> pending_block;
  AppendHalfwords(pending_block, world.pending);
  std::cout << ' ' << Hash(active_block) << ' ' << Hash(staged_bytes) << ' '
            << Hash(pending_block);
  PrintRows(world);
  PrintCalls(world);
}
}  // namespace

int main() {
  int kind = 0;
  std::size_t vector = 0;
  try {
    while (std::cin >> kind) {
      switch (kind) {
        case 0:
        case 1:
          RunRow(kind);
          break;
        case 2:
          RunList();
          break;
        case 3:
          RunDirection();
          break;
        case 4:
          RunScreen();
          break;
        default:
          std::exit(2);
      }
      ++vector;
    }
  } catch (const std::exception& error) {
    std::cerr << "vector " << vector << ": " << error.what() << '\n';
    return 3;
  }
}
