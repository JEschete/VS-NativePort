#include "Menu/Loot/LootTerminalLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
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
#include "Battle/HudStateLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Items/LootLive.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Inventory/DetailsPoliciesLive.hpp"
#include "Menu/Inventory/InventoryTerminalLive.hpp"
#include "Menu/Inventory/ItemDetailDispatchLive.hpp"
#include "Menu/Loot/LootPoliciesLive.hpp"
#include "Menu/Loot/LootRowsLive.hpp"
#include "Menu/Loot/LootTerminal.hpp"
#include "Menu/Sounds.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/Hud.h"
#include "Battle/TargetingState.h"
#include "Content/StringTables.h"
#include "Core/Diagnostics.h"
#include "Input/ControllerLiveState.h"
#include "Items/InventoryLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/ItemRowsLiveState.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Loot/LootScreen.h"
#include "Menu/Loot/Paging.h"
#include "Menu/TextBox.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

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
constexpr std::int32_t kMenuCommand = 2;

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
  Items_Inventory inventory{};
  Items_Inventory inventory_backup{};
  std::array<Menu_Loot_LootListItem, kLootRowCapacity> rows{};
  std::array<Menu_Loot_LootListItem, kLootRowCapacity> working_rows{};
};
Controllers g_controllers;
std::unique_ptr<WorkspaceViews> g_workspace;
std::uint64_t g_refusals = 0;

[[noreturn]] void Refuse(std::string_view reason) {
  ++g_refusals;
  Core_Errorf("menu.loot", "Loot terminal refused: %.*s",
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
  if (Menu_Framework_menuItems == nullptr) Refuse("missing menu row workspace");
}
void RequireFrame() {
  if (Render_frameScratch.primitiveCursor == nullptr ||
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kOverlay, 0) ==
          nullptr) {
    Refuse("missing loot presentation frame");
  }
}
void ReadRuntime(LootTerminalRuntime& runtime) {
  runtime.cursor = {Menu_Loot_lootListCount, Menu_Loot_lootListOffset,
                    static_cast<std::uint8_t>(Menu_Loot_selectedLoot)};
  runtime.inventory_ui = {
      static_cast<std::uint8_t>(Menu_Inventory_secondaryTitleEnabled),
      static_cast<std::uint8_t>(Menu_Inventory_itemCategoryIconsEnabled),
      static_cast<std::uint8_t>(Menu_Inventory_statusCommandState),
      static_cast<std::uint8_t>(Menu_Inventory_secondaryTitleAnimationStep),
      static_cast<std::uint8_t>(Menu_Inventory_itemCategoryIconsAnimationStep),
      static_cast<std::uint8_t>(Menu_Inventory_statusCommandAnimationStep),
      static_cast<std::uint8_t>(Menu_Inventory_selectedItemCategoryIconOnTop),
      Menu_Inventory_ashleyRenderState};
  runtime.inventory_category = static_cast<std::uint8_t>(
      Menu_Framework_menuMemory.opaque3A.currentCategory);
  runtime.show_inventory = static_cast<std::uint8_t>(Menu_Loot_showInventory);
  runtime.inventory_step = Menu_Loot_inventoryCapacityFlyinTimer;
  runtime.show_loot = static_cast<std::uint8_t>(Menu_Loot_showLoot);
  runtime.loot_step = Menu_Loot_lootLeftEdge;
  runtime.controls_enabled =
      static_cast<std::uint8_t>(Menu_Loot_controlsEnabled);
  runtime.previewed = static_cast<std::uint8_t>(Menu_Loot_previewedLoot);
  runtime.claimed = static_cast<std::uint8_t>(Menu_Loot_lootClaimed);
  runtime.information_shown =
      static_cast<std::uint8_t>(Input_settings.information);
  runtime.hud_hidden = static_cast<std::uint8_t>(Battle_hudHidden);
  runtime.hud_slide_offset = Battle_hudSlideOffset;
  runtime.result_kind = Menu_Framework_actionSelection.selectionKind;
}
void WriteRuntime(const LootTerminalRuntime& runtime) {
  Menu_Loot_lootListCount = runtime.cursor.count;
  Menu_Loot_lootListOffset = runtime.cursor.offset;
  Menu_Loot_selectedLoot = static_cast<char>(runtime.cursor.selected);
  const auto& ui = runtime.inventory_ui;
  Menu_Inventory_secondaryTitleEnabled = static_cast<char>(ui.secondary_title);
  Menu_Inventory_itemCategoryIconsEnabled =
      static_cast<char>(ui.category_icons);
  Menu_Inventory_statusCommandState = static_cast<char>(ui.status_command);
  Menu_Inventory_secondaryTitleAnimationStep =
      static_cast<char>(ui.secondary_step);
  Menu_Inventory_itemCategoryIconsAnimationStep =
      static_cast<char>(ui.icons_step);
  Menu_Inventory_statusCommandAnimationStep = static_cast<char>(ui.status_step);
  Menu_Inventory_selectedItemCategoryIconOnTop =
      static_cast<char>(ui.icon_on_top);
  Menu_Inventory_ashleyRenderState = ui.ashley_layout;
  Menu_Framework_menuMemory.opaque3A.currentCategory =
      static_cast<char>(runtime.inventory_category);
  Menu_Loot_showInventory = static_cast<char>(runtime.show_inventory);
  Menu_Loot_inventoryCapacityFlyinTimer = runtime.inventory_step;
  Menu_Loot_showLoot = static_cast<char>(runtime.show_loot);
  Menu_Loot_lootLeftEdge = runtime.loot_step;
  Menu_Loot_controlsEnabled = static_cast<char>(runtime.controls_enabled);
  Menu_Loot_previewedLoot = static_cast<char>(runtime.previewed);
  Menu_Loot_lootClaimed = static_cast<char>(runtime.claimed);
  Input_settings.information = static_cast<char>(runtime.information_shown);
  Battle_hudHidden = static_cast<char>(runtime.hud_hidden);
  Battle_hudSlideOffset = runtime.hud_slide_offset;
  Menu_Framework_actionSelection.selectionKind = runtime.result_kind;
}
void InvalidateWorkspace() {
  if (g_workspace) {
    for (auto& weapon : g_workspace->inventory.weapons) {
      if (Battle_stringContext.strings[0] == weapon.name) {
        Battle_stringContext.strings[0] = nullptr;
      }
    }
  }
  Menu_Loot_inventory = nullptr;
  Menu_Loot_inventoryBackup = nullptr;
  Menu_Loot_lootList = nullptr;
  Menu_Loot_lootListWorkingCopy = nullptr;
  g_workspace.reset();
}
void RequireOwnedWorkspace() {
  if (!g_workspace || Menu_Loot_inventory != &g_workspace->inventory ||
      Menu_Loot_inventoryBackup != &g_workspace->inventory_backup ||
      Menu_Loot_lootList != g_workspace->rows.data() ||
      Menu_Loot_lootListWorkingCopy != g_workspace->working_rows.data()) {
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
    if (Menu_Loot_lootListWorkingCopy == nullptr) {
      Refuse("missing loot row backup");
    }
    if (!rows_backup_loaded_) {
      ReadRowBackup();
      rows_backup_loaded_ = true;
    }
    if (inventory) {
      if (Menu_Loot_inventoryBackup == nullptr) {
        Refuse("missing staged inventory backup");
      }
      if (!inventory_backup_loaded_) {
        workspace_.inventory_backup =
            Items::Live::Typed(*Menu_Loot_inventoryBackup);
        inventory_backup_loaded_ = true;
      }
    }
  }
  void Publish() {
    WriteRuntime(runtime_);
    if (primary_loaded_) {
      if (Menu_Loot_inventory == nullptr || Menu_Loot_lootList == nullptr) {
        Refuse("staged workspace was invalidated during a loot call");
      }
      Items::Live::Commit(*Menu_Loot_inventory, workspace_.inventory);
      const auto rows = std::span(Menu_Loot_lootList, kLootRowCapacity);
      for (std::size_t index = 0; index < rows.size(); ++index) {
        rows[index] =
            std::bit_cast<Menu_Loot_LootListItem>(workspace_.rows[index]);
      }
    }
    if (rows_backup_loaded_) {
      if (Menu_Loot_lootListWorkingCopy == nullptr) {
        Refuse("row backup was invalidated");
      }
      const auto rows =
          std::span(Menu_Loot_lootListWorkingCopy, kLootRowCapacity);
      for (std::size_t index = 0; index < rows.size(); ++index) {
        rows[index] = std::bit_cast<Menu_Loot_LootListItem>(
            workspace_.working_rows[index]);
      }
    }
    if (inventory_backup_loaded_) {
      if (Menu_Loot_inventoryBackup == nullptr) {
        Refuse("inventory backup was invalidated");
      }
      Items::Live::Commit(*Menu_Loot_inventoryBackup,
                          workspace_.inventory_backup);
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
    return Items::Live::Typed(Audio_Seq_unusedInventory);
  }
  Input::PadState Buttons() override {
    return {.pressed = Input_buttonsPressed.allButtons,
            .repeat = static_cast<std::uint32_t>(Input_buttonRepeat),
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
      return EntryText({reinterpret_cast<const std::uint8_t*>(Menu_menuStrings),
                        sizeof(Menu_menuStrings)},
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
    Battle_stringContext.strings[0] = Menu_Loot_inventory->weapons[slot].name;
  }
  void SetItemName(std::uint16_t id) override {
    constexpr auto kItemNameCount =
        Framework::kItemNamesBytes / Framework::kItemNameRowBytes;
    if (Menu_Framework_itemNames == nullptr || id >= kItemNameCount) {
      Refuse("loot item name outside its loaded table");
    }
    Battle_stringContext.strings[0] = Menu_Framework_itemNames[id];
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
    Call([&] { Framework::SetLiveCommandLabel(enabled ? kMenuCommand : 0); });
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
  void RenderAshley(std::int32_t mode) override {
    if (mode == 0) RequireFrame();
    Call([&] { Framework::RenderLiveAshley(mode); });
  }
  std::int32_t LootInventory(bool initialize) override {
    return Call(
        [&] { return Inventory::UpdateLiveLootItemList(initialize ? 1 : 0); });
  }
  std::int32_t AnimationOffset(std::size_t step) override {
    const auto curve = std::span(Menu_Framework_rowAnimationSteps);
    if (step >= curve.size()) Refuse("loot animation outside its curve");
    return static_cast<std::uint8_t>(curve[step]);
  }
  std::uint8_t Capacity(LootCategory category) override {
    const auto capacities = std::span(Items_Inventory_inventoryItemCapacities);
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
      Framework::DrawLiveButtonIcon(1, kStatusButtonX - offset, kStatusButtonY,
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
    if (Menu_Loot_inventoryBackup == nullptr) {
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
                 row.category | kWriteItem, &Audio_Seq_unusedInventory,
                 row.item_index, Menu_Loot_inventory) != 0;
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
      auto* entry = row ? &Menu_Loot_lootList[*row] : nullptr;
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
    return Call([] { return Menu_Loot_AdvanceStagedInventory() != 0; });
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
    if (g_workspace || Menu_Loot_inventory != nullptr ||
        Menu_Loot_inventoryBackup != nullptr || Menu_Loot_lootList != nullptr ||
        Menu_Loot_lootListWorkingCopy != nullptr) {
      Refuse("loot workspace already acquired or borrowed by another owner");
    }
    g_workspace = std::make_unique<WorkspaceViews>();
    Menu_Loot_inventory = &g_workspace->inventory;
    Menu_Loot_inventoryBackup = &g_workspace->inventory_backup;
    Menu_Loot_lootList = g_workspace->rows.data();
    Menu_Loot_lootListWorkingCopy = g_workspace->working_rows.data();
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
    Call([] { Menu_Loot_ResetPaging(); });
  }
  void BuildStagedInventory() override {
    Call([] { Live::BuildStagedInventory(Menu_Framework_lootLists); });
  }
  bool HaveRoomLoot() override { return Menu_Framework_lootLists != nullptr; }
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
    if (Menu_Loot_inventory == nullptr || Menu_Loot_lootList == nullptr) {
      Refuse("missing staged inventory or loot rows");
    }
    workspace_.inventory = Items::Live::Typed(*Menu_Loot_inventory);
    const auto rows = std::span(Menu_Loot_lootList, kLootRowCapacity);
    for (std::size_t index = 0; index < rows.size(); ++index) {
      workspace_.rows[index] = std::bit_cast<LootRow>(rows[index]);
    }
  }
  void ReadRowBackup() {
    if (Menu_Loot_lootListWorkingCopy == nullptr) {
      Refuse("missing loot row backup");
    }
    const auto rows =
        std::span(Menu_Loot_lootListWorkingCopy, kLootRowCapacity);
    for (std::size_t index = 0; index < rows.size(); ++index) {
      workspace_.working_rows[index] = std::bit_cast<LootRow>(rows[index]);
    }
  }
  void Refresh() {
    ReadRuntime(runtime_);
    if (primary_loaded_) ReadPrimary();
    if (rows_backup_loaded_) ReadRowBackup();
    if (inventory_backup_loaded_) {
      if (Menu_Loot_inventoryBackup == nullptr) {
        Refuse("inventory backup was invalidated");
      }
      workspace_.inventory_backup =
          Items::Live::Typed(*Menu_Loot_inventoryBackup);
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
  Menu_Loot_ResetPaging();
  InvalidateWorkspace();
  g_controllers = {};
  Menu_Loot_lootListCount = 0;
  Menu_Loot_lootListOffset = 0;
  Menu_Loot_selectedLoot = 0;
  Menu_Loot_previewedLoot = 0;
  Menu_Loot_lootClaimed = 0;
  Menu_Loot_controlsEnabled = 0;
  Menu_Loot_showInventory = 0;
  Menu_Loot_showLoot = 0;
  Menu_Loot_inventoryCapacityFlyinTimer = 0;
  Menu_Loot_lootLeftEdge = 0;
}
std::uint64_t LootTerminalLiveRefusalCount() noexcept { return g_refusals; }
}  // namespace VS::Menu::Loot
