#include "Menu/Loot/LootTerminalLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include "Audio/SfxLive.hpp"
#include "Battle/Hud.h"
#include "Battle/HudStateLive.hpp"
#include "Battle/TargetingState.h"
#include "Content/StringTables.h"
#include "Core/Diagnostics.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Items/LootLive.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Framework/ItemRowsLiveState.hpp"
#include "Menu/Framework/Menus.hpp"
#include "Menu/Inventory/DetailsPoliciesLive.hpp"
#include "Menu/Inventory/Inventory.hpp"
#include "Menu/Inventory/InventoryTerminalLive.hpp"
#include "Menu/Inventory/ItemDetailDispatchLive.hpp"
#include "Menu/Loot/LootPoliciesLive.hpp"
#include "Menu/Loot/LootRowsLive.hpp"
#include "Menu/Loot/LootScreen.hpp"
#include "Menu/Loot/LootTerminal.hpp"
#include "Menu/Loot/Paging.hpp"
#include "Menu/Sounds.hpp"
#include "Menu/TextBox.h"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/Render.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.hpp"

namespace VS::Menu::Loot {
namespace {
constexpr std::size_t kControllerCount = 5;
constexpr std::size_t kInformationBox = 7;
constexpr std::uint32_t kStatusColor = 0x202020;
constexpr unsigned kShiftMask = 31;
constexpr std::int32_t kStatusBackgroundX = 16;
constexpr std::int32_t kStatusBackgroundY = 38;
constexpr std::int32_t kStatusBackgroundWidth = 88;
constexpr std::int32_t kStatusBackgroundHeight = 10;
constexpr std::int32_t kStatusButtonX = 8;
constexpr std::int32_t kStatusButtonY = 36;
constexpr std::int32_t kStatusTextX = 28;
constexpr std::int32_t kSoundGroup = 126;
constexpr std::int32_t kClaimSound = 13;
constexpr std::int32_t kDisassembleSound = 25;

enum class Controller : std::size_t {
  kTransition,
  kDisassembly,
  kConsolidation,
  kSelection,
  kScreen,
};
struct Controllers {
  InventoryTransitionState transition;
  DisassemblyState disassembly;
  StackConsolidationState consolidation;
  LootSelectionState selection;
  std::array<bool, kControllerCount> active{};
};
// These are views for the remaining checked Live helpers, not a PS1 heap
// layout. Ownership is one native allocation, invalidated on release/reset.
struct WorkspaceViews {
  VS::Items::InventoryContents inventory{};
  VS::Items::InventoryContents inventory_backup{};
  std::array<LootRow, kLootRowCapacity> rows{};
  std::array<LootRow, kLootRowCapacity> working_rows{};
};
Controllers g_controllers;
std::unique_ptr<WorkspaceViews> g_workspace;
std::uint64_t g_refusals = 0;

[[noreturn]] void Refuse(std::string_view reason) {
  ++g_refusals;
  VS::Core::Errorf("menu.loot", "Loot terminal refused: %.*s",
                   static_cast<int>(reason.size()), reason.data());
  std::abort();
}
void RequireIdleControllers() {
  if (std::ranges::find(g_controllers.active, true) !=
      g_controllers.active.end()) {
    Refuse("reset during an active loot terminal call");
  }
}
void RequireRows() {
  if (VS::Menu::Framework::menuItems == nullptr) {
    Refuse("missing menu row workspace");
  }
}
void RequireFrame() {
  if (VS::Render::FrameScratchpad().primitive_cursor == nullptr ||
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kOverlay, 0) ==
          nullptr) {
    Refuse("missing loot presentation frame");
  }
}
void ReadRuntime(LootTerminalRuntime& runtime) {
  runtime.cursor = {lootListCount, lootListOffset, selectedLoot};
  runtime.inventory_ui = {VS::Menu::Inventory::secondaryTitleEnabled,
                          VS::Menu::Inventory::itemCategoryIconsEnabled,
                          VS::Menu::Inventory::statusCommandState,
                          VS::Menu::Inventory::secondaryTitleAnimationStep,
                          VS::Menu::Inventory::itemCategoryIconsAnimationStep,
                          VS::Menu::Inventory::statusCommandAnimationStep,
                          VS::Menu::Inventory::selectedItemCategoryIconOnTop,
                          VS::Menu::Inventory::ashleyRenderState};
  runtime.inventory_category =
      VS::Menu::Framework::menuMemory.inventory_cursors.category;
  runtime.show_inventory = showInventory;
  runtime.inventory_step = inventoryCapacityFlyinTimer;
  runtime.show_loot = showLoot;
  runtime.loot_step = lootLeftEdge;
  runtime.controls_enabled = controlsEnabled;
  runtime.previewed = previewedLoot;
  runtime.claimed = lootClaimed;
  runtime.information_shown =
      static_cast<std::uint8_t>(VS::Input::settings.information);
  runtime.hud_hidden = static_cast<std::uint8_t>(Battle_hudHidden);
  runtime.hud_slide_offset = Battle_hudSlideOffset;
  runtime.result_kind = VS::Menu::Framework::actionSelection.selection_kind;
}
void WriteRuntime(const LootTerminalRuntime& runtime) {
  lootListCount = runtime.cursor.count;
  lootListOffset = runtime.cursor.offset;
  selectedLoot = runtime.cursor.selected;
  const auto& ui = runtime.inventory_ui;
  VS::Menu::Inventory::secondaryTitleEnabled =
      static_cast<std::uint8_t>(ui.secondary_title);
  VS::Menu::Inventory::itemCategoryIconsEnabled =
      static_cast<std::uint8_t>(ui.category_icons);
  VS::Menu::Inventory::statusCommandState =
      static_cast<std::uint8_t>(ui.status_command);
  VS::Menu::Inventory::secondaryTitleAnimationStep =
      static_cast<std::uint8_t>(ui.secondary_step);
  VS::Menu::Inventory::itemCategoryIconsAnimationStep =
      static_cast<std::uint8_t>(ui.icons_step);
  VS::Menu::Inventory::statusCommandAnimationStep =
      static_cast<std::uint8_t>(ui.status_step);
  VS::Menu::Inventory::selectedItemCategoryIconOnTop =
      static_cast<std::uint8_t>(ui.icon_on_top);
  VS::Menu::Inventory::ashleyRenderState = ui.ashley_layout;
  VS::Menu::Framework::menuMemory.inventory_cursors.category =
      runtime.inventory_category;
  showInventory = runtime.show_inventory;
  inventoryCapacityFlyinTimer = runtime.inventory_step;
  showLoot = runtime.show_loot;
  lootLeftEdge = runtime.loot_step;
  controlsEnabled = runtime.controls_enabled;
  previewedLoot = runtime.previewed;
  lootClaimed = runtime.claimed;
  VS::Input::settings.information =
      static_cast<char>(runtime.information_shown);
  Battle_hudHidden = static_cast<char>(runtime.hud_hidden);
  Battle_hudSlideOffset = runtime.hud_slide_offset;
  VS::Menu::Framework::actionSelection.selection_kind = runtime.result_kind;
}
void InvalidateWorkspace() {
  if (g_workspace) {
    for (auto& weapon : g_workspace->inventory.weapons) {
      if (Battle_stringContext.strings[0] == weapon.name.data()) {
        Battle_stringContext.strings[0] = nullptr;
      }
    }
  }
  stagedInventory = nullptr;
  inventoryBackup = nullptr;
  lootList = nullptr;
  lootListWorkingCopy = nullptr;
  g_workspace.reset();
}
void RequireOwnedWorkspace() {
  if (!g_workspace || stagedInventory != &g_workspace->inventory ||
      inventoryBackup != &g_workspace->inventory_backup ||
      lootList != g_workspace->rows.data() ||
      lootListWorkingCopy != g_workspace->working_rows.data()) {
    Refuse("loot workspace cannot be released by a different owner");
  }
}
std::span<const std::uint8_t> OffsetText(std::span<const std::uint8_t> table,
                                         std::size_t halfword) {
  const auto text = Framework::ItemHelpText(table, halfword);
  if (!text) Refuse("loot text offset outside its loaded asset");
  return table.subspan(text->offset);
}
std::span<const std::uint8_t> EntryText(std::span<const std::uint8_t> table,
                                        std::int32_t entry) {
  const auto text = Framework::ItemHelpEntry(table, entry);
  if (!text) Refuse("loot text entry outside its loaded asset");
  return table.subspan(text->offset);
}
int LegacyRow(std::optional<std::size_t> row) {
  if (!row) return 0;
  if (*row >= kLootRowCapacity) Refuse("nested loot row outside owned storage");
  return static_cast<int>(*row + 1);
}
class CallGuard {
 public:
  explicit CallGuard(Controller controller)
      : active_(g_controllers.active[static_cast<std::size_t>(controller)]) {
    if (active_) Refuse("same loot controller reentered before returning");
    active_ = true;
  }
  ~CallGuard() { active_ = false; }
  CallGuard(const CallGuard&) = delete;
  CallGuard& operator=(const CallGuard&) = delete;
  CallGuard(CallGuard&&) = delete;
  CallGuard& operator=(CallGuard&&) = delete;

 private:
  bool& active_;
};

class LiveTerminalHost final : public LootTerminalHost {
 public:
  LiveTerminalHost() { ReadRuntime(runtime_); }
  LootTerminalRuntime& Runtime() override { return runtime_; }
  LootTerminalWorkspace& Workspace() override {
    if (!primary_loaded_) {
      ReadPrimary();
      primary_loaded_ = true;
    }
    return workspace_;
  }
  void RequireBackups(bool inventory) override {
    if (lootListWorkingCopy == nullptr) {
      Refuse("missing loot row backup");
    }
    if (!rows_backup_loaded_) {
      ReadRowBackup();
      rows_backup_loaded_ = true;
    }
    if (inventory) {
      if (inventoryBackup == nullptr) {
        Refuse("missing staged inventory backup");
      }
      if (!inventory_backup_loaded_) {
        workspace_.inventory_backup = *inventoryBackup;
        inventory_backup_loaded_ = true;
      }
    }
  }
  void Publish() {
    WriteRuntime(runtime_);
    if (primary_loaded_) {
      if (stagedInventory == nullptr || lootList == nullptr) {
        Refuse("staged workspace was invalidated during a loot call");
      }
      *stagedInventory = workspace_.inventory;
      const auto rows = std::span(lootList, kLootRowCapacity);
      for (std::size_t index = 0; index < rows.size(); ++index) {
        rows[index] = workspace_.rows[index];
      }
    }
    if (rows_backup_loaded_) {
      if (lootListWorkingCopy == nullptr) {
        Refuse("row backup was invalidated");
      }
      const auto rows = std::span(lootListWorkingCopy, kLootRowCapacity);
      for (std::size_t index = 0; index < rows.size(); ++index) {
        rows[index] = workspace_.working_rows[index];
      }
    }
    if (inventory_backup_loaded_) {
      if (inventoryBackup == nullptr) {
        Refuse("inventory backup was invalidated");
      }
      *inventoryBackup = workspace_.inventory_backup;
    }
  }
  template <typename Function, typename... Arguments>
  auto Call(Function&& function, Arguments&&... arguments)
      -> std::invoke_result_t<Function, Arguments...> {
    Publish();
    if constexpr (std::is_void_v<
                      std::invoke_result_t<Function, Arguments...>>) {
      std::invoke(std::forward<Function>(function),
                  std::forward<Arguments>(arguments)...);
      Refresh();
    } else {
      std::invoke_result_t<Function, Arguments...> result =
          std::invoke(std::forward<Function>(function),
                      std::forward<Arguments>(arguments)...);
      Refresh();
      return result;
    }
  }
  Items::InventoryContents AshleyInventory() override {
    return VS::Items::inventory;
  }
  Input::PadState Buttons() override {
    return {.pressed = VS::Input::buttonsPressed,
            .repeat = static_cast<std::uint32_t>(VS::Input::buttonRepeat),
            .sticks = {}};
  }
  bool RowsReady() override {
    RequireRows();
    return Call(Framework::AreLiveMenuRowsIdle) != 0;
  }
  bool InformationDone() override {
    return Menu_textBoxes[kInformationBox].flags.done != 0;
  }
  std::span<const std::uint8_t> Text(LootTerminalText text,
                                     std::int32_t index) override {
    if (text == LootTerminalText::kItemsTitle) {
      return EntryText(
          {reinterpret_cast<const std::uint8_t*>(VS::Menu::menuStrings),
           sizeof(VS::Menu::menuStrings)},
          Text_Battle_menuStrings_Index_items);
    }
    const auto menu = Inventory::LiveLootInventoryText();
    switch (text) {
      case LootTerminalText::kCategoryTitle:
        if (index < 0 || index >= static_cast<int>(Items::kItemCategoryCount)) {
          Refuse("loot category-title index outside text table");
        }
        return EntryText(menu, Text_Menub_menuText_Index_weapons + index);
      case LootTerminalText::kYes:
        return OffsetText(menu, Text_Menub_menuText_Offset_optionYes);
      case LootTerminalText::kNo:
        return OffsetText(menu, Text_Menub_menuText_Offset_optionNo);
      case LootTerminalText::kDisassembleConfirm:
        return OffsetText(menu, Text_Menub_menuText_Offset_disassembleConfirm);
      case LootTerminalText::kOrganizeConfirm:
        return OffsetText(menu, Text_Menub_menuText_Offset_organizeConfirm);
      case LootTerminalText::kEmptyChest:
        return OffsetText(menu, Text_Menub_menuText_Offset_emptyChest);
      case LootTerminalText::kItemsTitle:
        break;
    }
    Refuse("unknown loot text selector");
  }
  Framework::MenuRow& Row(std::int32_t row) override {
    RequireRows();
    if (row < 0 || static_cast<std::size_t>(row) >= Framework::kRowCount) {
      Refuse("loot menu row outside the active table");
    }
    return Framework::LiveMenuRow(row);
  }
  Framework::MenuRow& SetRow(const Framework::RowPlacement& placement,
                             std::span<const std::uint8_t> text) override {
    static_cast<void>(Row(placement.row));
    if (text.empty()) Refuse("loot menu row has no encoded text");
    return Call(Framework::SetLiveMenuItemText, placement, text);
  }
  void SetWeaponName(std::size_t slot) override {
    static_cast<void>(Workspace());
    if (slot >= Items::kWeaponSlots) {
      Refuse("weapon name outside staged inventory");
    }
    Publish();
    Battle_stringContext.strings[0] =
        stagedInventory->weapons[slot].name.data();
  }
  void SetItemName(std::uint16_t id) override {
    constexpr auto kItemNameCount =
        Framework::kItemNamesBytes / Framework::kItemNameRowBytes;
    if (VS::Menu::Framework::itemNames == nullptr || id >= kItemNameCount) {
      Refuse("loot item name outside its loaded table");
    }
    Battle_stringContext.strings[0] = VS::Menu::Framework::itemNames[id];
  }
  void InitializeInformation(bool caution) override {
    Call([&] {
      if (caution) {
        Menu::InitLiveInformationTextBox(
            static_cast<Menu_InformationBoxHeader>(1));
      } else {
        Framework::InitializeLiveInformationBox();
      }
    });
  }
  void SetInformation(std::span<const std::uint8_t> text) override {
    Call([&] { Framework::SetLiveInformationMessageText(text); });
  }
  void DismissInformation() override {
    Call([] { Framework::DismissLiveInformationBox(); });
  }
  void FlyOutRight(std::int32_t row) override {
    static_cast<void>(Row(row));
    Call([&] { Framework::FlyOutLiveRowRight(row); });
  }
  void PlaySound(LootTerminalSound sound) override {
    Call([&] {
      switch (sound) {
        case LootTerminalSound::kOpen:
          Menu::PlayOpenSound();
          break;
        case LootTerminalSound::kSelect:
          Menu::PlaySelectSound();
          break;
        case LootTerminalSound::kLeave:
          Menu::PlayLeaveSound();
          break;
        case LootTerminalSound::kChange:
          Menu::PlayChangeSound();
          break;
        case LootTerminalSound::kInvalid:
          Menu::PlayInvalidSound();
          break;
        case LootTerminalSound::kClaim:
          Audio::LivePlaySfxCentered(kSoundGroup, kClaimSound);
          break;
        case LootTerminalSound::kDisassemble:
          Audio::LivePlaySfxCentered(kSoundGroup, kDisassembleSound);
          break;
      }
    });
  }
  std::uint8_t DrawPromptCursor(std::uint8_t animation,
                                std::int32_t position) override {
    RequireFrame();
    return Call([&] {
      return static_cast<std::uint8_t>(
          Framework::DrawLiveMainMenuCursor(animation, position));
    });
  }
  std::uint8_t DrawLootCursor(std::uint8_t animation, std::int32_t x,
                              std::int32_t y) override {
    RequireFrame();
    return Call([&] {
      return static_cast<std::uint8_t>(Framework::DrawLiveCursor(
          animation, static_cast<int>(Framework::PackXy(x, y))));
    });
  }
  void SetBackgroundFade(bool enabled) override {
    Call([&] { Framework::SetLiveBackgroundFade(enabled ? 1 : 0); });
  }
  void SetMenuCommand(bool enabled) override {
    Call([&] {
      Framework::SetLiveCommandLabel(enabled ? Framework::MenuCommand::kMenu
                                             : Framework::MenuCommand::kNone);
    });
  }
  void HideInventoryPanels() override {
    Call([] { Inventory::SetLivePanelVisibility(0); });
  }
  void ClearMenu() override {
    RequireRows();
    Call([] {
      Framework::ClearLiveMenuExcept(static_cast<int>(Framework::kRowCount));
    });
  }
  void SetFieldRenderingEnabled(bool enabled) override {
    Call(Field::LiveSetFieldRenderingEnabled, enabled ? 1 : 0);
  }
  void SetHudHidden(bool hidden) override {
    Call(Battle::SetLiveHudHidden, hidden ? 1 : 0);
  }
  void RenderAshley(Framework::AshleyRenderMode mode) override {
    if (mode == Framework::AshleyRenderMode::kRender) RequireFrame();
    Call([&] { Framework::RenderLiveAshley(mode); });
  }
  std::int32_t LootInventory(bool initialize) override {
    return Call(
        [&] { return Inventory::UpdateLiveLootItemList(initialize ? 1 : 0); });
  }
  std::int32_t AnimationOffset(std::size_t step) override {
    const auto curve = std::span(VS::Menu::Framework::rowAnimationSteps);
    if (step >= curve.size()) Refuse("loot animation outside its curve");
    return curve[step];
  }
  std::uint8_t Capacity(LootCategory category) override {
    const auto capacities = std::span(VS::Items::kCategoryCapacities);
    const auto index = static_cast<std::size_t>(category);
    if (index >= capacities.size()) {
      Refuse("loot category outside capacity table");
    }
    return capacities[index];
  }
  std::int32_t CountItems(LootCategory category) override {
    return Call([&] {
      return Items::Inventory::CountLiveItems(static_cast<int>(category),
                                              nullptr);
    });
  }
  void DrawCategoryIcons(std::uint8_t step, std::uint8_t on_top) override {
    RequireFrame();
    Call([&] { Inventory::DrawLiveItemCategoryIcons(step, on_top); });
  }
  void DrawStatusCommand(std::int32_t offset, std::uint8_t state) override {
    RequireFrame();
    const auto text_position =
        Framework::PackXy(kStatusTextX - offset, kStatusBackgroundY);
    const auto color = kStatusColor << (state & kShiftMask);
    Call([&] {
      Framework::DrawLiveButtonPromptBackground(
          kStatusBackgroundX - offset, kStatusBackgroundY,
          kStatusBackgroundWidth, kStatusBackgroundHeight);
      Framework::DrawLiveButtonIcon(Framework::ButtonIcon::kSquare,
                                    kStatusButtonX - offset, kStatusButtonY,
                                    nullptr);
      Render::DrawLiveCompactTextColor("STATUS",
                                       static_cast<int>(text_position),
                                       static_cast<int>(color), nullptr);
    });
  }
  void RefreshClaimableRows() override {
    static_cast<void>(Workspace());
    Call([] { Live::RefreshClaimableRows(); });
  }
  void ShowDescription(std::size_t row) override {
    static_cast<void>(Workspace());
    if (inventoryBackup == nullptr) {
      Refuse("missing description inventory backup");
    }
    if (row >= runtime_.cursor.count) {
      Refuse("description outside staged loot rows");
    }
    Call([&] { Live::ShowLootDescription(static_cast<int>(row)); });
  }
  bool ClaimItem(const LootRow& row) override {
    static_cast<void>(Workspace());
    return Call([&] {
      constexpr int kWriteItem = 16;
      return Items::Inventory::CopyLiveItem(
                 row.category | kWriteItem, &VS::Items::inventory,
                 row.item_index, stagedInventory) != 0;
    });
  }
  void RecordKey(std::uint16_t id) override {
    Call([&] { Live::RecordClaimedKey(id); });
  }
  std::int32_t LeaveMenu(bool initialize) override {
    static_cast<void>(Workspace());
    return Call(
        [&] { return Live::UpdateLeaveConfirmation(initialize ? 1 : 0); });
  }
  std::int32_t OrganizeMenu(std::optional<std::size_t> row) override {
    static_cast<void>(Workspace());
    if (row && *row >= runtime_.cursor.count) {
      Refuse("organization row outside loot");
    }
    return Call([&] {
      auto* entry = row ? &lootList[*row] : nullptr;
      return Live::UpdateOrganizationMenu(entry);
    });
  }
  std::int32_t InventoryTransition(
      std::optional<LootCategory> category) override {
    return Call([&] {
      return UpdateLiveInventoryTransition(
          category ? static_cast<int>(*category) + 1 : 0);
    });
  }
  std::int32_t Disassemble(std::optional<std::size_t> row) override {
    return Call([&] { return UpdateLiveDisassembly(LegacyRow(row)); });
  }
  std::int32_t Consolidate(std::optional<std::size_t> row) override {
    return Call([&] { return UpdateLiveStackConsolidation(LegacyRow(row)); });
  }
  bool AdvanceBatch() override {
    static_cast<void>(Workspace());
    return Call(
        [] { return VS::Menu::Loot::Live::AdvanceStagedInventory() != 0; });
  }
  void PopulateBaseStats(std::int32_t actor) override {
    Call([&] { Framework::SetLiveActorComparisonStats(actor); });
  }
  void RebuildInventory(LootCategory category) override {
    Call([&] {
      Items::Inventory::RebuildLiveDisplayIndices(static_cast<int>(category));
    });
  }
  void AcquireWorkspace() override {
    Publish();
    if (g_workspace || stagedInventory != nullptr ||
        inventoryBackup != nullptr || lootList != nullptr ||
        lootListWorkingCopy != nullptr) {
      Refuse("loot workspace already acquired or borrowed by another owner");
    }
    g_workspace = std::make_unique<WorkspaceViews>();
    stagedInventory = &g_workspace->inventory;
    inventoryBackup = &g_workspace->inventory_backup;
    lootList = g_workspace->rows.data();
    lootListWorkingCopy = g_workspace->working_rows.data();
    ForgetWorkspace();
    ReadRuntime(runtime_);
  }
  void ReleaseWorkspace() override {
    Publish();
    RequireOwnedWorkspace();
    InvalidateWorkspace();
    ForgetWorkspace();
  }
  void ResetPaging() override {
    Call([] { VS::Menu::Loot::Live::ResetPaging(); });
  }
  void BuildStagedInventory() override {
    Call([] { Live::BuildStagedInventory(VS::Menu::Framework::lootLists); });
  }
  bool HaveRoomLoot() override {
    return VS::Menu::Framework::lootLists != nullptr;
  }
  void ClearRoomLoot() override {
    Call([] { Items::ClearLiveRoomLootList(); });
  }
  bool LoadItemText(bool initialize) override {
    return Call(
        [&] { return Framework::LoadLiveItemText(initialize ? 1 : 0) != 0; });
  }
  bool UnloadItemText() override {
    return Call([] { return Framework::UnloadLiveItemTextWhenReady() != 0; });
  }
  bool DismissScreenText() override {
    return Call([] { return Menu::DismissLiveTextBox(kInformationBox) != 0; });
  }
  std::int32_t Selection(bool initialize) override {
    return Call([&] { return UpdateLiveLootSelection(initialize ? 1 : 0); });
  }
  void RenderCapacities(std::int32_t x) override {
    RequireFrame();
    static_cast<void>(Workspace());
    Call([&] { Live::RenderBagCapacities(x); });
  }
  void RenderLoot(std::int32_t x) override {
    RequireRows();
    RequireFrame();
    static_cast<void>(Workspace());
    Call([&] { Live::RenderLootRows(x); });
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    Loot::Refuse(reason);
  }

 private:
  void ReadPrimary() {
    if (stagedInventory == nullptr || lootList == nullptr) {
      Refuse("missing staged inventory or loot rows");
    }
    workspace_.inventory = *stagedInventory;
    const auto rows = std::span(lootList, kLootRowCapacity);
    for (std::size_t index = 0; index < rows.size(); ++index) {
      workspace_.rows[index] = rows[index];
    }
  }
  void ReadRowBackup() {
    if (lootListWorkingCopy == nullptr) {
      Refuse("missing loot row backup");
    }
    const auto rows = std::span(lootListWorkingCopy, kLootRowCapacity);
    for (std::size_t index = 0; index < rows.size(); ++index) {
      workspace_.working_rows[index] = rows[index];
    }
  }
  void Refresh() {
    ReadRuntime(runtime_);
    if (primary_loaded_) ReadPrimary();
    if (rows_backup_loaded_) ReadRowBackup();
    if (inventory_backup_loaded_) {
      if (inventoryBackup == nullptr) {
        Refuse("inventory backup was invalidated");
      }
      workspace_.inventory_backup = *inventoryBackup;
    }
  }
  void ForgetWorkspace() {
    primary_loaded_ = false;
    rows_backup_loaded_ = false;
    inventory_backup_loaded_ = false;
  }
  LootTerminalRuntime runtime_;
  LootTerminalWorkspace workspace_;
  bool primary_loaded_ = false;
  bool rows_backup_loaded_ = false;
  bool inventory_backup_loaded_ = false;
};

template <typename Function>
int Execute(Controller controller, Function&& function) {
  const CallGuard guard(controller);
  try {
    LiveTerminalHost host;
    const auto result = std::invoke(std::forward<Function>(function), host);
    host.Publish();
    return result;
  } catch (const std::bad_alloc&) {
    Refuse("allocation failed while executing a loot controller");
  }
}
std::optional<std::size_t> RowEntry(int initialize) {
  if (initialize == 0) return std::nullopt;
  if (initialize < 1 || initialize > static_cast<int>(kLootRowCapacity)) {
    Refuse("loot entry is not a one-based row in owned storage");
  }
  return static_cast<std::size_t>(initialize - 1);
}
}  // namespace

// decomp: _topLevelMenuTransition (src/MENU/MENUB.PRG/260.c:61).
int UpdateLiveInventoryTransition(int initialize) {
  if (initialize < 0 || initialize > static_cast<int>(LootCategory::kManage)) {
    Refuse("loot inventory entry is not an item category");
  }
  const auto category =
      initialize != 0 ? std::optional(static_cast<LootCategory>(initialize - 1))
                      : std::nullopt;
  return Execute(Controller::kTransition, [&](auto& host) {
    return UpdateInventoryTransition(g_controllers.transition, host, category);
  });
}
// decomp: _disassembleItem (src/MENU/MENUB.PRG/260.c:232).
int UpdateLiveDisassembly(int initialize) {
  const auto row = RowEntry(initialize);
  return Execute(Controller::kDisassembly, [&](auto& host) {
    return UpdateDisassembly(g_controllers.disassembly, host, row);
  });
}
// decomp: _consolidateMiscItems (src/MENU/MENUB.PRG/260.c:458).
int UpdateLiveStackConsolidation(int initialize) {
  const auto row = RowEntry(initialize);
  return Execute(Controller::kConsolidation, [&](auto& host) {
    return UpdateStackConsolidation(g_controllers.consolidation, host, row);
  });
}
// decomp: _processLootMenu (src/MENU/MENUB.PRG/260.c:1622).
int UpdateLiveLootSelection(int initialize) {
  return Execute(Controller::kSelection, [&](auto& host) {
    return UpdateLootSelection(g_controllers.selection, host, initialize != 0);
  });
}
// decomp: vs_menuB_exec (src/MENU/MENUB.PRG/260.c:1951).
int UpdateLiveLootScreen(unsigned char* step) {
  if (step == nullptr) Refuse("loot screen without a caller-owned state");
  return Execute(Controller::kScreen,
                 [&](auto& host) { return UpdateLootScreen(*step, host); });
}
void ResetLootTerminalModuleData() {
  RequireIdleControllers();
  g_controllers = {};
}
void ResetLootTerminalSession() {
  RequireIdleControllers();
  VS::Menu::Loot::Live::ResetPaging();
  InvalidateWorkspace();
  g_controllers = {};
  lootListCount = 0;
  lootListOffset = 0;
  selectedLoot = 0;
  previewedLoot = 0;
  lootClaimed = 0;
  controlsEnabled = 0;
  showInventory = 0;
  showLoot = 0;
  inventoryCapacityFlyinTimer = 0;
  lootLeftEdge = 0;
}
std::uint64_t LootTerminalLiveRefusalCount() noexcept { return g_refusals; }
}  // namespace VS::Menu::Loot
