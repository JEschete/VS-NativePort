// Hand-maintained (tools/port/owned.csv).
// Live binding for the loot screen's capacity panel, visible rows, row
// description and leave and organize prompts (LootPolicies.cpp): MENUB.PRG's
// loot list, cursor and staged inventory, Ashley's inventory, the menu rows,
// the information box and the controller.
#include "Menu/Loot/LootPoliciesLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "Audio/SfxLive.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Inventory/InventoryTerminalLive.hpp"
#include "Menu/Inventory/ItemDetailLiveSupport.hpp"
#include "Menu/Loot/LootPolicies.hpp"
#include "Menu/Loot/LootRows.hpp"
#include "Menu/Loot/LootRowsLive.hpp"
#include "Menu/Sounds.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/GameFontLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/TargetingState.h"
#include "Content/StringTables.h"
#include "Input/ControllerLiveState.h"
#include "Items/InventoryLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/ItemRowsLiveState.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Loot/LootScreen.h"
#include "Menu/Loot/Paging.h"
}

namespace VS::Menu::Loot::Live {
namespace {
namespace items = VS::Items;
namespace item_live = VS::Items::Live;
using Framework::MenuRow;
using Framework::RowPlacement;

constexpr std::uint32_t kRefusalLogLimit = 16;
constexpr std::uint32_t kRefusalLogInterval = 1024;
constexpr std::size_t kNameTableBytes = Framework::kItemNamesBytes;
constexpr std::size_t kItemNameCount = kNameTableBytes / kMenuItemNameRowBytes;
// decomp: _menuText[(availableAction + 4) * 2] names the action's row text.
constexpr std::size_t kActionTextBase = 4;
constexpr int kDiscardSoundGroup = 0x7E;
constexpr int kDiscardSound = 0x1C;
// vs_mainMenu_initInformationTextBox(1).
constexpr auto kCautionHeader = static_cast<enum Menu_InformationBoxHeader>(1);

std::uint64_t g_refusals = 0;

// _discardItems' and _organizeInventory's statics.
struct PolicyStatics {
  LeaveConfirmation leave;
  OrganizeMenu organize;
};

PolicyStatics& Statics() {
  static PolicyStatics statics;
  return statics;
}

void Refuse(std::string_view reason) {
  ++g_refusals;
  if (g_refusals <= kRefusalLogLimit || g_refusals % kRefusalLogInterval == 0) {
    VS_LOG_WARN("menu.loot", "loot policies: {} (refusal {})", reason,
                g_refusals);
  }
}

[[nodiscard]] std::span<Menu_Loot_LootListItem, kLootRowCapacity> LiveRows() {
  return std::span<Menu_Loot_LootListItem, kLootRowCapacity>(Menu_Loot_lootList,
                                                             kLootRowCapacity);
}

[[nodiscard]] std::vector<LootRow> LoadRows() {
  std::vector<LootRow> rows(kLootRowCapacity);
  const auto live = LiveRows();
  for (std::size_t i = 0; i < live.size(); ++i) {
    rows[i] = std::bit_cast<LootRow>(live[i]);
  }
  return rows;
}

void StoreRows(std::span<const LootRow> rows) {
  const auto live = LiveRows();
  for (std::size_t i = 0; i < live.size(); ++i) {
    live[i] = std::bit_cast<Menu_Loot_LootListItem>(rows[i]);
  }
}

[[nodiscard]] LootCursor LiveCursor() {
  return {.count = Menu_Loot_lootListCount,
          .offset = Menu_Loot_lootListOffset,
          .selected = static_cast<std::uint8_t>(Menu_Loot_selectedLoot)};
}

[[nodiscard]] std::span<const std::uint8_t> ItemNameText(std::uint16_t id) {
  if (Menu_Framework_itemNames == nullptr || id >= kItemNameCount) {
    Refuse("an item name outside vs_mainMenu_itemNames");
    return {};
  }
  const auto* names =
      reinterpret_cast<const std::uint8_t*>(Menu_Framework_itemNames);
  const std::size_t first = std::size_t{id} * kMenuItemNameRowBytes;
  return std::span(names, Framework::kItemNamesBytes).subspan(first);
}

[[nodiscard]] std::span<const std::uint8_t> MenuText(std::size_t offset) {
  const auto bytes = Inventory::LiveLootInventoryText();
  const auto text = Framework::ItemHelpText(bytes, offset);
  if (!text) {
    Refuse("a prompt text outside _menuText");
    return {};
  }
  return bytes.subspan(text->offset);
}

[[nodiscard]] std::span<const std::uint8_t> ActionText(std::size_t action) {
  const auto bytes = Inventory::LiveLootInventoryText();
  const auto text = Framework::ItemHelpEntry(
      bytes, static_cast<std::int32_t>((action + kActionTextBase) * 2));
  if (!text) {
    Refuse("an action text outside _menuText");
    return {};
  }
  return bytes.subspan(text->offset);
}

[[nodiscard]] std::span<const std::uint8_t> PromptTextBytes(PromptText text) {
  switch (text) {
    case PromptText::kOptionYes:
      return MenuText(Text_Menub_menuText_Offset_optionYes);
    case PromptText::kOptionNo:
      return MenuText(Text_Menub_menuText_Offset_optionNo);
    case PromptText::kItemsRemaining:
      return MenuText(Text_Menub_menuText_Offset_itemsRemaining);
    case PromptText::kInvalidDiscard:
      return MenuText(Text_Menub_menuText_Offset_invalidDiscard);
    case PromptText::kDiscardAllConfirm:
      return MenuText(Text_Menub_menuText_Offset_discardAllConfirm);
    case PromptText::kDisassemble:
      return ActionText(1);
    case PromptText::kOrganize:
      return ActionText(2);
    case PromptText::kOrganizeInventory:
      return MenuText(Text_Menub_menuText_Offset_organizeInventory);
    case PromptText::kDisassembleInfo:
      return MenuText(Text_Menub_menuText_Offset_disassembleInfo);
    case PromptText::kOrganizeInfo:
      return MenuText(Text_Menub_menuText_Offset_organizeInfo);
    case PromptText::kOrganizeInventoryInfo:
      return MenuText(Text_Menub_menuText_Offset_organizeInventoryInfo);
  }
  return {};
}

class LiveCapacityHost final : public CapacityHost {
 public:
  void DrawLabel(CapacityLabel label, std::uint32_t xy) override {
    Render::DrawLiveCompactText(Label(label, 0), static_cast<int>(xy), nullptr);
  }
  void DrawColoredLabel(CapacityLabel label, std::uint8_t category,
                        std::uint32_t xy, std::uint32_t color) override {
    Render::DrawLiveCompactTextColor(Label(label, category),
                                     static_cast<int>(xy),
                                     static_cast<int>(color), nullptr);
  }
  std::int32_t CountItems(std::uint8_t category) override {
    return items::Inventory::CountLiveItems(category, nullptr);
  }
  std::int32_t CountContained(std::uint8_t category,
                              const LootRow& row) override {
    return Live::CountContainedCategory(category, LiveRow(row));
  }
  void DrawInteger(std::int32_t value, std::uint32_t xy,
                   std::uint32_t color) override {
    (void)Framework::DrawLiveColoredInteger(value, static_cast<int>(xy),
                                            static_cast<int>(color), nullptr);
  }
  void DrawButtonBackground(std::int32_t x, std::int32_t y, std::int32_t w,
                            std::int32_t h) override {
    Framework::DrawLiveButtonPromptBackground(x, y, w, h);
  }
  void DrawButton(PromptButton button, std::int32_t x,
                  std::int32_t y) override {
    Framework::DrawLiveButtonIcon(static_cast<int>(button), x, y, nullptr);
  }

  explicit LiveCapacityHost(std::span<const LootRow> rows) : rows_(rows) {}

 private:
  [[nodiscard]] static const char* Label(CapacityLabel label,
                                         std::uint8_t category) {
    switch (label) {
      case CapacityLabel::kGadgetBag:
        return "GADGET BAG";
      case CapacityLabel::kSlash:
        return "/";
      case CapacityLabel::kCategory:
        return Menu_Loot_itemCategoryHeaders[category];
      case CapacityLabel::kGet:
        return "GET";
      case CapacityLabel::kAll:
        return "ALL";
      case CapacityLabel::kEnd:
        return "END";
    }
    return "";
  }

  // The live row the typed copy came from (CountContainedCategory reads it).
  [[nodiscard]] Menu_Loot_LootListItem* LiveRow(const LootRow& row) const {
    const auto index = static_cast<std::size_t>(&row - rows_.data());
    return &LiveRows()[index];
  }

  std::span<const LootRow> rows_;
};

class LiveLootRowsHost final : public LootRowsHost {
 public:
  MenuRow& Row(int row) override { return Framework::LiveMenuRow(row); }
  MenuRow& SetRow(const RowPlacement& placement,
                  std::span<const std::uint8_t> text) override {
    return Framework::SetLiveMenuItemText(placement, text);
  }
  std::span<const std::uint8_t> ItemName(std::uint16_t id) override {
    return ItemNameText(id);
  }
};

class LiveDescriptionHost final : public DescriptionHost {
 public:
  void BackupInventory() override {
    Core::Memory::CopyLiveHalfwordBlocks(Menu_Loot_inventoryBackup,
                                         &Audio_Seq_unusedInventory,
                                         sizeof *Menu_Loot_inventoryBackup);
  }
  void LoadStagedInventory() override {
    Core::Memory::CopyLiveHalfwordBlocks(&Audio_Seq_unusedInventory,
                                         Menu_Loot_inventory,
                                         sizeof Audio_Seq_unusedInventory);
  }
  std::span<const std::uint8_t> FormatDescription(items::ItemCategory category,
                                                  std::uint8_t index) override {
    std::array<char*, 2> text{};
    int row_type = 0;
    auto& ashley = Audio_Seq_unusedInventory;
    switch (category) {
      case items::ItemCategory::kWeapon:
        Framework::FormatLiveInventoryWeaponRow(&ashley.weapons[index],
                                                text.data(), &row_type,
                                                Menu_Framework_stringBuf);
        break;
      case items::ItemCategory::kBlade:
        Framework::FormatLiveBladeRow(&ashley.blades[index], text.data(),
                                      &row_type, Menu_Framework_stringBuf);
        break;
      case items::ItemCategory::kGrip:
        Framework::FormatLiveGripRow(&ashley.grips[index], text.data(),
                                     &row_type, Menu_Framework_stringBuf);
        break;
      case items::ItemCategory::kShield:
        Framework::FormatLiveInventoryShieldRow(&ashley.shields[index],
                                                text.data(), &row_type,
                                                Menu_Framework_stringBuf);
        break;
      case items::ItemCategory::kArmor:
        Framework::FormatLiveInventoryArmorRow(&ashley.armor[index],
                                               text.data(), &row_type,
                                               Menu_Framework_stringBuf);
        break;
      case items::ItemCategory::kGem:
        Framework::FormatLiveGemRow(&ashley.gems[index], text.data(), &row_type,
                                    Menu_Framework_stringBuf);
        break;
      case items::ItemCategory::kMisc:
        Framework::FormatLiveMiscItemRow(&ashley.misc[index], text.data(),
                                         &row_type, Menu_Framework_stringBuf);
        break;
      case items::ItemCategory::kManage:
        break;
    }
    return DescriptionBytes(text[1]);
  }
  void ShowInformation(std::span<const std::uint8_t> message) override {
    Framework::SetLiveInformationMessageText(message);
  }
  void RestoreInventory() override {
    Core::Memory::CopyLiveHalfwordBlocks(&Audio_Seq_unusedInventory,
                                         Menu_Loot_inventoryBackup,
                                         sizeof Audio_Seq_unusedInventory);
  }

 private:
  // The formatters point into the string buffer or the item help table; the
  // original trusts any other pointer as far as its terminator.
  [[nodiscard]] static std::span<const std::uint8_t> DescriptionBytes(
      char* text) {
    if (text == nullptr) return {};
    auto found = Inventory::FindLiveItemDetailText(text, {});
    if (found.empty()) found = {text, Render::kTrustedEncodedTextBytes};
    return {reinterpret_cast<const std::uint8_t*>(found.data()), found.size()};
  }
};

class LivePromptHost final : public PromptHost {
 public:
  MenuRow& SetRow(const RowPlacement& placement, PromptText text) override {
    return Framework::SetLiveMenuItemText(placement, PromptTextBytes(text));
  }
  MenuRow& Row(int row) override { return Framework::LiveMenuRow(row); }
  void InitInformationTextBox() override {
    InitLiveInformationTextBox(kCautionHeader);
  }
  void InitInformationBox() override {
    Framework::InitializeLiveInformationBox();
  }
  void ShowInformation(PromptText text) override {
    Framework::SetLiveInformationMessageText(PromptTextBytes(text));
  }
  bool RowsIdle() override { return Framework::AreLiveMenuRowsIdle() != 0; }
  void FlyOutRight(int row) override { Framework::FlyOutLiveRowRight(row); }
  void DismissInformationBox() override {
    Framework::DismissLiveInformationBox();
  }
  void PlayLeaveSound() override { Menu::PlayLeaveSound(); }
  void PlayChangeSound() override { Menu::PlayChangeSound(); }
  void PlaySelectSound() override { Menu::PlaySelectSound(); }
  void PlayDiscardSound() override {
    Audio::LivePlaySfxCentered(kDiscardSoundGroup, kDiscardSound);
  }
  std::uint8_t DrawCursor(std::uint8_t step, int position) override {
    return static_cast<std::uint8_t>(
        Framework::DrawLiveMainMenuCursor(step, position));
  }
  void SetStringItem(std::uint16_t item_id) override {
    if (Menu_Framework_itemNames == nullptr || item_id >= kItemNameCount) {
      Refuse("a kept item outside vs_mainMenu_itemNames");
      return;
    }
    Battle_stringContext.strings[0] = Menu_Framework_itemNames[item_id];
  }
};

[[nodiscard]] PromptInput LiveInput() {
  return {.pressed = Input_buttonsPressed.allButtons,
          .repeat = static_cast<std::uint32_t>(Input_buttonRepeat),
          .information_shown = Input_settings.information != 0};
}

[[nodiscard]] bool HaveLootList(const char* operation) {
  if (Menu_Loot_lootList == nullptr || Menu_Loot_inventory == nullptr) {
    item_live::WarnNullRecord(operation);
    return false;
  }
  return true;
}
}  // namespace

void RenderBagCapacities(int edge_x) {
  if (Menu_Loot_lootList == nullptr) {
    item_live::WarnNullRecord("RenderBagCapacities");
    return;
  }
  const auto rows = LoadRows();
  const CapacityPanel panel{
      .rows = rows,
      .cursor = LiveCursor(),
      .previewed = static_cast<std::uint8_t>(Menu_Loot_previewedLoot),
      .controls_enabled = static_cast<std::uint8_t>(Menu_Loot_controlsEnabled),
      .capacities = std::span(Items_Inventory_inventoryItemCapacities)};
  LiveCapacityHost host(rows);
  if (!VS::Menu::Loot::RenderBagCapacities(edge_x, panel, host)) {
    Refuse("the previewed loot row is outside the loot list");
  }
}

void RenderLootRows(int x) {
  if (!HaveLootList("RenderLootRows")) return;
  auto rows = LoadRows();
  LootRowsView view{.rows = rows,
                    .cursor = LiveCursor(),
                    .gradient_state_index = static_cast<std::uint8_t>(
                        Menu_Loot_gradientStateIndex)};
  const auto inventory = item_live::Typed(*Menu_Loot_inventory);
  LiveLootRowsHost host;
  item_live::WarnRefusal("RenderLootRows", VS::Menu::Loot::RenderLootRows(
                                               x, view, inventory, host));
  StoreRows(rows);
  Menu_Loot_gradientStateIndex = static_cast<char>(view.gradient_state_index);
}

void ShowLootDescription(int loot_index) {
  if (!HaveLootList("ShowLootDescription")) return;
  if (Menu_Loot_inventoryBackup == nullptr) {
    item_live::WarnNullRecord("ShowLootDescription");
    return;
  }
  if (loot_index < 0 ||
      static_cast<std::size_t>(loot_index) >= kLootRowCapacity) {
    Refuse("a described loot row outside the loot list");
    return;
  }
  const auto row =
      std::bit_cast<LootRow>(LiveRows()[static_cast<std::size_t>(loot_index)]);
  LiveDescriptionHost host;
  item_live::WarnRefusal("ShowLootDescription",
                         VS::Menu::Loot::ShowLootDescription(row, host));
}

int UpdateLeaveConfirmation(int init) {
  auto& prompt = Statics().leave;
  const bool claimed = Menu_Loot_lootClaimed != 0;
  if (init != 0) {
    if (!HaveLootList("UpdateLeaveConfirmation")) return 0;
    const auto rows = LoadRows();
    const LeaveContext context{
        .rows = rows,
        .count = Menu_Loot_lootListCount,
        .claimed = claimed,
        .single_entry = Menu_Framework_lootEntry != nullptr};
    item_live::WarnRefusal(
        "UpdateLeaveConfirmation",
        InitLeaveConfirmation(prompt, context,
                              item_live::Typed(*Menu_Loot_inventory)));
    return 0;
  }
  LivePromptHost host;
  const auto result = VS::Menu::Loot::UpdateLeaveConfirmation(
      prompt, claimed, LiveInput(), host);
  if (!result) {
    Refuse("a leave choice outside yes and no");
    return 0;
  }
  return *result;
}

int UpdateOrganizationMenu(Menu_Loot_LootListItem* loot) {
  auto& menu = Statics().organize;
  LivePromptHost host;
  if (loot != nullptr) {
    if (Menu_Loot_inventory == nullptr) {
      item_live::WarnNullRecord("UpdateOrganizationMenu");
      return 0;
    }
    item_live::WarnRefusal(
        "UpdateOrganizationMenu",
        InitOrganizeMenu(menu, std::bit_cast<LootRow>(*loot),
                         item_live::Typed(*Menu_Loot_inventory), host));
    return 0;
  }
  const auto result = UpdateOrganizeMenu(menu, LiveInput(), host);
  if (!result) {
    Refuse("an organize row outside the two choices");
    return 0;
  }
  return *result;
}

void ResetPolicyStatics() {
  Statics() = {};
  g_refusals = 0;
}
}  // namespace VS::Menu::Loot::Live
