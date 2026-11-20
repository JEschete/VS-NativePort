#include "Menu/Inventory/ItemListRowsLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "Battle/Action.hpp"
#include "Battle/ActionTable.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Inventory/Inventory.hpp"
#include "Menu/Inventory/ItemList.hpp"
#include "Menu/Inventory/ItemListRows.hpp"

namespace VS::Menu::Inventory {
namespace {
using Items::ItemCategory;

constexpr std::uint64_t kRefusalLogLimit = 16;
constexpr std::uint64_t kRefusalLogInterval = 1024;
// VS::Menu::Inventory::categoryOrders names the seven item tables.
constexpr auto kManage = ItemCategory::kManage;
constexpr auto kOrderedCategories = static_cast<std::size_t>(kManage);
std::uint64_t g_refusals = 0;

void Refuse(std::string_view reason) {
  ++g_refusals;
  if (g_refusals <= kRefusalLogLimit || g_refusals % kRefusalLogInterval == 0) {
    VS::Core::LogWarn("menu.inventory", "{} (refusal {})", reason, g_refusals);
  }
}

std::string_view Describe(ItemRowsRefusal refusal) {
  switch (refusal) {
    case ItemRowsRefusal::kOrder:
      return "item list order outside its index table";
    case ItemRowsRefusal::kSlot:
      return "item list entry outside its inventory table";
    case ItemRowsRefusal::kMiscItem:
      return "misc item outside the action map";
    case ItemRowsRefusal::kParentWeapon:
      return "item list part names a weapon outside the inventory";
    case ItemRowsRefusal::kParentShield:
      return "item list gem names a shield outside the inventory";
    case ItemRowsRefusal::kRows:
      return "item list longer than its row buffers";
    case ItemRowsRefusal::kMiscRows:
      return "misc list longer than the misc actions";
    case ItemRowsRefusal::kCursorMemory:
      return "restored item list cursor outside the menu memory";
    case ItemRowsRefusal::kRow:
      return "restored item list without menu rows";
    case ItemRowsRefusal::kStalled:
      return "item list animation made no progress";
  }
  return "item list refused";
}

std::span<const std::uint8_t> OrderTable(ItemCategory category) {
  const auto& order = VS::Items::inventoryIndices;
  const std::array<std::span<const std::uint8_t>, kOrderedCategories> tables{
      order.weapons, order.blades, order.grips, order.shields,
      order.armor,   order.gems,   order.misc};
  return tables[static_cast<std::size_t>(category)];
}

// The order from VS::Menu::Inventory::categoryOrders[category] to the end of
// the index table holding it, or nullopt outside every table.
std::optional<std::span<const std::uint8_t>> KnownOrder(ItemCategory category) {
  const auto* pointer =
      VS::Menu::Inventory::categoryOrders[static_cast<std::size_t>(category)];
  if (pointer == nullptr) return std::nullopt;
  for (std::size_t table = 0; table < kOrderedCategories; ++table) {
    const auto entries = OrderTable(static_cast<ItemCategory>(table));
    for (std::size_t offset = 0; offset < entries.size(); ++offset) {
      if (pointer == &entries[offset]) return entries.subspan(offset);
    }
  }
  return std::nullopt;
}

// The `stride`-byte rows a live heap payload holds from `buffer` on.
std::size_t HeapRows(const void* buffer, std::size_t stride) {
  if (buffer == nullptr) return 0;
  const auto payload = Core::Memory::LivePayloadContaining(buffer);
  if (!payload) return 0;
  const auto* start = static_cast<const std::byte*>(buffer);
  const auto offset = static_cast<std::size_t>(start - payload->data());
  return (payload->size() - offset) / stride;
}

std::size_t RowCapacity(char* menu_text[], int row_types[], char* text_buf) {
  const auto text_rows =
      HeapRows(static_cast<const void*>(menu_text), 2 * sizeof(menu_text[0]));
  const auto type_rows = HeapRows(row_types, sizeof(row_types[0]));
  const auto buffer_rows =
      HeapRows(text_buf, Framework::kRowDescriptionBufferBytes);
  return std::min({text_rows, type_rows, buffer_rows});
}

class LiveItemRowsWorld final : public ItemRowsWorld {
 public:
  LiveItemRowsWorld(char** menu_text, int* row_types, char* text_buf)
      : menu_text_(menu_text), row_types_(row_types), text_buf_(text_buf) {}

  bool ActionUnlocked(std::uint32_t action) override {
    const std::span actions(VS::Battle::actionDefinitions);
    return action < actions.size() && actions[action].unlocked != 0;
  }

  void FormatRow(ItemCategory category, std::size_t slot,
                 std::size_t row) override {
    auto& inventory = VS::Items::inventory;
    // The core bounded every row by the caller's heap payloads.
    char** text = menu_text_ + (row * 2);
    int* type = row_types_ + row;
    void* buffer = text_buf_ + (row * Framework::kRowDescriptionBufferBytes);
    switch (category) {
      case ItemCategory::kWeapon:
        Framework::FormatLiveInventoryWeaponRow(
            &std::span(inventory.weapons)[slot], text, type, buffer);
        break;
      case ItemCategory::kBlade:
        Framework::FormatLiveBladeRow(&std::span(inventory.blades)[slot], text,
                                      type, buffer);
        break;
      case ItemCategory::kGrip:
        Framework::FormatLiveGripRow(&std::span(inventory.grips)[slot], text,
                                     type, buffer);
        break;
      case ItemCategory::kShield:
        Framework::FormatLiveInventoryShieldRow(
            &std::span(inventory.shields)[slot], text, type, buffer);
        break;
      case ItemCategory::kArmor:
        Framework::FormatLiveInventoryArmorRow(
            &std::span(inventory.armor)[slot], text, type, buffer);
        break;
      case ItemCategory::kGem:
        Framework::FormatLiveGemRow(&std::span(inventory.gems)[slot], text,
                                    type, buffer);
        break;
      case ItemCategory::kMisc:
        Framework::FormatLiveMiscItemRow(&std::span(inventory.misc)[slot], text,
                                         type, buffer);
        break;
      case ItemCategory::kManage:
        break;
    }
  }

  void AddRowFlags(std::size_t row, std::uint32_t flags) override {
    int& type = row_types_[row];
    type = static_cast<int>(static_cast<std::uint32_t>(type) | flags);
  }

  void StoreMiscAction(std::size_t row, std::uint8_t action) override {
    const std::span<std::uint8_t> actions(VS::Menu::Inventory::miscActions);
    actions[row] = action;
  }

 private:
  char** menu_text_;
  int* row_types_;
  char* text_buf_;
};

class LiveRestoreWorld final : public ItemRowsRestoreWorld {
 public:
  std::uint8_t ListAnimationRow() override {
    return static_cast<std::uint8_t>(VS::Menu::Framework::itemsListRow);
  }
  void AdvanceListRows() override { Framework::UpdateLiveRowSelection(); }
  Framework::MenuRow* Row(std::int32_t index) override {
    if (VS::Menu::Framework::menuItems == nullptr || index < 0 ||
        static_cast<std::size_t>(index) >= Framework::kRowCount) {
      return nullptr;
    }
    return &Framework::LiveMenuRow(index);
  }
  std::span<const std::uint8_t> CursorMemory() override {
    return Framework::LiveMenuCursorMemory();
  }
};

int BuildLiveRows(ItemRowsContext context, int item_category, char* menu_text[],
                  int row_types[], char* text_buf) {
  if (item_category < 0 ||
      static_cast<std::size_t>(item_category) >= kOrderedCategories) {
    Refuse("item list category outside the inventory tables");
    return 0;
  }
  const auto category = static_cast<ItemCategory>(item_category);
  const auto order = KnownOrder(category);
  if (!order) {
    Refuse("item list order outside the inventory index tables");
    return 0;
  }
  const ItemRowsInput input{
      .category = category,
      .context = context,
      .order = *order,
      .capacity =
          std::span(VS::Items::kCategoryCapacities)[static_cast<std::size_t>(
              item_category)],
      .misc_action_map = VS::Menu::Inventory::kMiscItemToActionMap,
      .row_capacity = RowCapacity(menu_text, row_types, text_buf),
  };
  LiveItemRowsWorld world(menu_text, row_types, text_buf);
  const auto rows = BuildItemRows(input, VS::Items::inventory, world);
  if (!rows) {
    Refuse(Describe(rows.error()));
    return 0;
  }
  return *rows;
}
}  // namespace

// decomp: MENU3.PRG _getParentWeaponIndex (items2.h:11), 80104EC0.
int LiveParentWeaponIndex(int item_category, int index) {
  const auto parent =
      ParentWeaponIndex(item_category, index, VS::Items::inventory);
  if (!parent || *parent > Items::kWeaponSlots) {
    Refuse(parent ? Describe(ItemRowsRefusal::kParentWeapon)
                  : Describe(ItemRowsRefusal::kSlot));
    return 0;
  }
  return *parent;
}

// decomp: MENU3.PRG _getParentShieldIndex (items2.h:38), 80104F50.
int LiveParentShieldIndex(int item_category, int item_index) {
  const auto parent =
      ParentShieldIndex(item_category, item_index, VS::Items::inventory);
  if (!parent || *parent > Items::kShieldSlots) {
    Refuse(parent ? Describe(ItemRowsRefusal::kParentShield)
                  : Describe(ItemRowsRefusal::kSlot));
    return 0;
  }
  return *parent;
}

// decomp: MENU3.PRG _populateItemRows (items2.h:60), 80104F94.
int LiveBuildItemRows(int item_category, char* menu_text[], int row_types[],
                      char* text_buf) {
  return BuildLiveRows(ItemRowsContext::kItemMenu, item_category, menu_text,
                       row_types, text_buf);
}

// decomp: MENUB.PRG _populateItemRows (items2.h:60), 80105088.
int LiveBuildLootItemRows(int item_category, char* menu_text[], int row_types[],
                          char* text_buf) {
  return BuildLiveRows(ItemRowsContext::kLoot, item_category, menu_text,
                       row_types, text_buf);
}

// decomp: MENU3.PRG _restoreItemMenu (items2.h:175), 80105314.
void LiveRestoreItemRows(int item_category) {
  LiveRestoreWorld world;
  const auto restored = RestoreItemRows(world, item_category);
  if (!restored) Refuse(Describe(restored.error()));
}

std::uint64_t ItemListRowsLiveRefusalCount() noexcept { return g_refusals; }
}  // namespace VS::Menu::Inventory
