// Hand-maintained (tools/port/owned.csv).
// Live binding for the loot screen's rows (LootRows.hpp): the working
// inventory (Menu_Loot_inventory), its rows and cursor, the pending loot
// records and Ashley's inventory for the room checks.
#include "Menu/Loot/LootRowsLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "Audio/Seq/Channel.h"
#include "Items/Inventory.hpp"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Loot/LootRows.hpp"
#include "Menu/Loot/LootScreen.h"
#include "Menu/Loot/Paging.hpp"
#include "State/ScriptFlagsLive.hpp"
#include "State/StateFlags.h"

static_assert(static_cast<std::size_t>(VS::Menu::Loot::kRowCapacity) ==
              VS::Menu::Loot::kLootRowCapacity);
static_assert(sizeof(Menu_Loot_LootListItem) ==
              sizeof(VS::Menu::Loot::LootRow));

namespace VS::Menu::Loot::Live {
namespace {
namespace items = VS::Items;
namespace item_live = VS::Items::Live;

// The batch the current rows came from and the loot records it reads.
struct Paging {
  StagingBatch batch{};
  std::vector<Items_BattleLoot*> sources;
};

Paging& PagingState() {
  static Paging paging;
  return paging;
}

// Ashley's room for each row, checked with the copy the claim makes.
class AshleyClaimCheck final : public ClaimCheck {
 public:
  explicit AshleyClaimCheck(const char* operation)
      : operation_(operation),
        ashley_(item_live::Typed(Audio_Seq_unusedInventory)),
        indices_(item_live::Typed(Audio_Seq_unusedInventoryIndices)) {}

  bool Fits(const LootRow& row,
            const items::InventoryContents& staged) override {
    const auto copy = items::Inventory::CopyItem(
        row.category, ashley_, row.item_index, staged, indices_, ashley_);
    item_live::WarnRefusal(operation_, copy.status);
    return copy.slot != 0;
  }

  // A misc check rebuilds Ashley's display order, as the original copy did.
  void Commit() const {
    item_live::Commit(Audio_Seq_unusedInventory, ashley_);
    item_live::Commit(Audio_Seq_unusedInventoryIndices, indices_);
  }

 private:
  const char* operation_;
  items::InventoryContents ashley_;
  items::InventoryIndices indices_;
};

std::span<Menu_Loot_LootListItem, kLootRowCapacity> LiveRows() {
  return std::span<Menu_Loot_LootListItem, kLootRowCapacity>(Menu_Loot_lootList,
                                                             kLootRowCapacity);
}

// The screen allocates VS::Menu::Loot::kRowCapacity rows with the working
// inventory.
std::optional<LootStaging> Load(const char* operation) {
  if (Menu_Loot_inventory == nullptr || Menu_Loot_lootList == nullptr) {
    item_live::WarnNullRecord(operation);
    return std::nullopt;
  }
  std::optional<LootStaging> staging(std::in_place);
  staging->inventory = item_live::Typed(*Menu_Loot_inventory);
  const auto rows = LiveRows();
  for (std::size_t i = 0; i < rows.size(); ++i) {
    staging->rows[i] = std::bit_cast<LootRow>(rows[i]);
  }
  staging->cursor = {
      .count = Menu_Loot_lootListCount,
      .offset = Menu_Loot_lootListOffset,
      .selected = static_cast<std::uint8_t>(Menu_Loot_selectedLoot),
  };
  staging->batch = PagingState().batch;
  staging->difficulty =
      static_cast<std::uint8_t>(Audio_Seq_unusedStateFlags.difficulty);
  return staging;
}

void Store(const LootStaging& staging) {
  item_live::Commit(*Menu_Loot_inventory, staging.inventory);
  const auto rows = LiveRows();
  for (std::size_t i = 0; i < rows.size(); ++i) {
    rows[i] = std::bit_cast<Menu_Loot_LootListItem>(staging.rows[i]);
  }
  Menu_Loot_lootListCount = staging.cursor.count;
  Menu_Loot_lootListOffset = staging.cursor.offset;
  Menu_Loot_selectedLoot = static_cast<char>(staging.cursor.selected);
  PagingState().batch = staging.batch;
}

// Runs `apply` over the loaded rows with Ashley's room checks.
template <typename Apply>
void Update(const char* operation, Apply&& apply) {
  auto staging = Load(operation);
  if (!staging) return;
  AshleyClaimCheck check(operation);
  std::forward<Apply>(apply)(*staging, check);
  check.Commit();
  Store(*staging);
}

std::vector<items::BattleLoot> TypedSources() {
  std::vector<items::BattleLoot> records;
  records.reserve(PagingState().sources.size());
  for (const auto* source : PagingState().sources) {
    records.push_back(item_live::Typed(*source));
  }
  return records;
}
}  // namespace

void RefreshClaimableRows() {
  Update("Menu::Loot::RefreshClaimableRows",
         [](LootStaging& staging, ClaimCheck& check) {
           VS::Menu::Loot::RefreshClaimableRows(staging, check);
         });
}

void AppendLootRow(int category, int slot) {
  constexpr const char* kOperation = "Menu::Loot::AppendLootRow";
  if (category < 0 ||
      category >= static_cast<int>(items::ItemCategory::kManage)) {
    item_live::WarnRefusal(kOperation, items::ItemsStatus::kCategoryOutOfRange);
    return;
  }
  auto staging = Load(kOperation);
  if (!staging) return;
  if (!VS::Menu::Loot::AppendLootRow(
          *staging, static_cast<items::ItemCategory>(category), slot)) {
    item_live::WarnRefusal(kOperation, items::ItemsStatus::kSlotOutOfRange);
    return;
  }
  Store(*staging);
}

int CountContainedCategory(int counted_category, Menu_Loot_LootListItem* row) {
  constexpr const char* kOperation = "Menu::Loot::CountContainedCategory";
  if (row == nullptr || Menu_Loot_inventory == nullptr) {
    item_live::WarnNullRecord(kOperation);
    return 0;
  }
  // The original counts nothing for the manage row or a larger category.
  if (counted_category < 0 ||
      counted_category >= static_cast<int>(items::kItemCategoryCount)) {
    return 0;
  }
  const auto count = VS::Menu::Loot::CountContainedCategory(
      static_cast<items::ItemCategory>(counted_category),
      std::bit_cast<LootRow>(*row), item_live::Typed(*Menu_Loot_inventory));
  item_live::WarnRefusal(kOperation, count.status);
  return count.count;
}

int IsDropAllowed(int difficulty_flags) {
  return DropDifficultyMatch(
      difficulty_flags,
      static_cast<std::uint8_t>(Audio_Seq_unusedStateFlags.difficulty));
}

void StageDropRecord(Items_BattleLoot* record) {
  constexpr const char* kOperation = "Menu::Loot::StageDropRecord";
  if (record == nullptr) {
    item_live::WarnNullRecord(kOperation);
    return;
  }
  auto& sources = PagingState().sources;
  std::size_t index = 0;
  while (index < sources.size() && sources[index] != record) ++index;
  if (index == sources.size()) sources.push_back(record);
  const auto typed = item_live::Typed(*record);
  Update(kOperation, [&typed, index](LootStaging& staging, ClaimCheck& check) {
    VS::Menu::Loot::StageDropRecord(typed, index, staging, check);
  });
}

void BuildStagedInventory(Items_LootListNode* node) {
  constexpr const char* kOperation = "Menu::Loot::BuildStagedInventory";
  auto& sources = PagingState().sources;
  sources.clear();
  for (; node != nullptr; node = node->next) sources.push_back(&node->loot);
  if (sources.empty()) {
    if (Menu_Framework_lootEntry == nullptr) {
      item_live::WarnNullRecord(kOperation);
    } else {
      sources.push_back(Menu_Framework_lootEntry);
    }
  }
  const auto records = TypedSources();
  Update(kOperation, [&records](LootStaging& staging, ClaimCheck& check) {
    VS::Menu::Loot::BuildStagedInventory(records, staging, check);
  });
}

void RecordClaimedKey(int item_id) {
  if (const auto flag = ClaimedKeyFlag(item_id)) {
    VS::State::SetLiveScriptFlag(*flag, 1);
  }
}

void ResetPaging() {
  auto& paging = PagingState();
  paging.batch = {};
  paging.sources.clear();
}

int AdvanceStagedInventory() {
  namespace loot = VS::Menu::Loot;
  auto& paging = PagingState();
  if (!paging.batch.deferred) return 0;
  auto records = TypedSources();
  bool advanced = false;
  Update("Menu::Loot::AdvanceStagedInventory",
         [&records, &advanced](loot::LootStaging& staging,
                               loot::ClaimCheck& check) {
           advanced = loot::AdvanceStagedInventory(records, staging, check);
         });
  for (std::size_t i = 0; i < records.size(); ++i) {
    item_live::Commit(*paging.sources[i], records[i]);
  }
  return advanced ? 1 : 0;
}
}  // namespace VS::Menu::Loot::Live
