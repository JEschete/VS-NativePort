#pragma once
// Hand-maintained (tools/port/owned.csv).
// The loot screen's rows (MENUB.PRG): staging each enemy's or chest's drops
// into a working inventory, one row per staged item, whether Ashley has room
// for each row, and the visible window over the rows.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Items/ItemRecords.hpp"

namespace VS::Menu::Loot {  // NOLINT(readability-identifier-naming)
// A working inventory holds at most one item per inventory slot, so one row
// per slot covers every staged item.
inline constexpr std::size_t kLootRowCapacity = sizeof(Items::InventoryIndices);
// Rows the loot window shows at once.
inline constexpr std::int32_t kVisibleLootRows = 8;
// Claimed key items at or above this id raise their script flag.
inline constexpr std::int32_t kFirstKeyItemId = 458;
// The script flag of key item `id` is id + kKeyItemFlagBase.
inline constexpr std::int32_t kKeyItemFlagBase = 438;

// decomp: _lootListItem (src/MENU/MENUB.PRG/260.c:29)
struct LootRow {
  // An Items::ItemCategory value.
  std::uint8_t category{};
  // The 0-based slot of the staged item in its category's table.
  std::uint8_t item_index{};
  // The screen's slide-in step; staging clears it.
  std::uint8_t slide_step{};
  // 1 when Ashley has room for the item, then the screen's row state.
  std::uint8_t state{};
  friend bool operator==(const LootRow&, const LootRow&) = default;
};

// decomp: _lootListCount, _lootListOffset and _selectedLoot.
struct LootCursor {
  std::uint8_t count{};
  // The first visible row.
  std::uint8_t offset{};
  // The selected row relative to `offset`; 0xFF with no rows.
  std::uint8_t selected{};
  friend bool operator==(const LootCursor&, const LootCursor&) = default;
};

// Whether Ashley's inventory has room for a row's item.
class ClaimCheck {
 public:
  ClaimCheck() = default;
  ClaimCheck(const ClaimCheck&) = delete;
  ClaimCheck& operator=(const ClaimCheck&) = delete;
  ClaimCheck(ClaimCheck&&) = delete;
  ClaimCheck& operator=(ClaimCheck&&) = delete;
  virtual ~ClaimCheck() = default;

  [[nodiscard]] virtual bool Fits(const LootRow& row,
                                  const Items::InventoryContents& staged) = 0;
};

// The drops of one loot record, in the order the original stages them.
enum class DropSlot : std::uint8_t {
  kWeapon,
  kBlade,
  kGrip,
  kShield,
  kArmor0,
  kArmor1,
  kAccessory,
  kGem,
  kMisc0,
  kMisc1,
  kMisc2,
};

struct StagedDrop {
  // The record's index in the batch's source records.
  std::size_t record{};
  DropSlot slot{};
  friend bool operator==(const StagedDrop&, const StagedDrop&) = default;
};

// The drops the current rows came from. Drops that did not fit (no row or
// no bag room) stay in their records for the next batch.
struct StagingBatch {
  std::array<StagedDrop, kLootRowCapacity> drops{};
  std::size_t count{};
  bool deferred{};
};

struct LootStaging {
  Items::InventoryContents inventory{};
  std::array<LootRow, kLootRowCapacity> rows{};
  LootCursor cursor{};
  StagingBatch batch{};
  // decomp: vs_main_stateFlags.difficulty
  std::uint8_t difficulty{};
};

// A count and whether the row named an item outside its table.
struct CategoryCount {
  std::int32_t count{};
  Items::ItemsStatus status{Items::ItemsStatus::kOk};
  friend bool operator==(const CategoryCount&, const CategoryCount&) = default;
};

void RefreshClaimableRows(LootStaging& staging, ClaimCheck& check);
// Appends a row for 1-based `slot`; refuses (and changes nothing) when every
// row is used.
[[nodiscard]] bool AppendLootRow(LootStaging& staging,
                                 Items::ItemCategory category,
                                 std::int32_t slot);
// The items of `counted` that claiming `row` would add, including a staged
// weapon's parts and set gems.
[[nodiscard]] CategoryCount CountContainedCategory(
    Items::ItemCategory counted, const LootRow& row,
    const Items::InventoryContents& inventory);
// The difficulty bits of `difficulty_flags` that allow the drop; nonzero
// allows it.
[[nodiscard]] std::int32_t DropDifficultyMatch(std::int32_t difficulty_flags,
                                               std::uint8_t difficulty);
void StageDropRecord(const Items::BattleLoot& record, std::size_t record_index,
                     LootStaging& staging, ClaimCheck& check);
void BuildStagedInventory(std::span<const Items::BattleLoot> records,
                          LootStaging& staging, ClaimCheck& check);
// Clears the drops of the current batch from `records` and stages the next
// batch; false when there was no deferred drop or the next batch is empty.
[[nodiscard]] bool AdvanceStagedInventory(std::span<Items::BattleLoot> records,
                                          LootStaging& staging,
                                          ClaimCheck& check);
// The script flag a claimed key item raises.
[[nodiscard]] std::optional<std::int16_t> ClaimedKeyFlag(std::int32_t item_id);
}  // namespace VS::Menu::Loot
