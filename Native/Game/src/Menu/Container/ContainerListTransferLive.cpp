#include "Menu/Container/ContainerListTransferLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

#include "Core/Log.hpp"
#include "Core/Memory/CopyLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/ContainerIndexingLive.hpp"
#include "Items/ContainerTransfersLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Container/ContainerCapacityLive.hpp"
#include "Menu/Container/ContainerDetailControllersLive.hpp"
#include "Menu/Container/ContainerDetailHelpersLive.hpp"
#include "Menu/Container/ContainerListTransfer.hpp"
#include "Menu/Container/ContainerMenuDialogsLive.hpp"
#include "Menu/Container/ContainerPendingRowsLive.hpp"
#include "Menu/Container/ItemView.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/EquipmentDetailsLiveState.hpp"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Framework/ItemRowsLiveHost.hpp"
#include "Menu/Framework/ItemRowsLiveState.hpp"
#include "Menu/Framework/Menus.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/Sounds.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/UiPacketsLive.hpp"

namespace VS::Menu::container {
namespace {
using Category = Items::ItemCategory;

// The generated callers address the workspace at these byte offsets.
constexpr std::size_t kRowTypesOffset = 0x6000;
constexpr std::size_t kRowTextsOffset = 0x6400;
static_assert(offsetof(ItemRowsWorkspace, types) == kRowTypesOffset);
static_assert(offsetof(ItemRowsWorkspace, texts) == kRowTextsOffset);

constexpr std::size_t kStorageCount = 5;
constexpr std::size_t kFirstCategoryCursor = 2;
constexpr std::size_t kTextsPerRow = 2;
// "OK" is linked 0xC bytes in front of the frame's ordering table.
constexpr std::size_t kAboveMenuChainBytes = 0xC;
static_assert(kAboveMenuChainBytes % sizeof(u_long) == 0);
constexpr auto kChainSlots = kAboveMenuChainBytes / sizeof(u_long);
constexpr int kAboveMenuChainSlot = -static_cast<int>(kChainSlots);

ItemListState g_item_list{};
TransferDirectionState g_direction{};
ScreenState g_screen{};
std::uint64_t g_refusals = 0;

struct Refusal {
  std::string_view reason;
};

[[noreturn]] void Refuse(std::string_view reason) { throw Refusal{reason}; }

void Warn(std::string_view entry, std::string_view reason) {
  if (std::has_single_bit(++g_refusals)) {
    VS::Core::LogWarn("menu.container", "{} refused: {} ({} refusals)", entry,
                      reason, g_refusals);
  }
}

// A record view whose refusals unwind to the Live entry.
class RecordView final : public Items::ContainerWorld {
 public:
  explicit RecordView(VS::Items::ContainerData* owner)
      : owner_(owner), live_(owner) {}
  std::uint16_t Capacity(Category category) override {
    return live_.Capacity(category);
  }
  Items::ContainerRecord Read(Category category, std::size_t index) override {
    if (owner_ == nullptr) Refuse("missing item records");
    return live_.Read(category, index);
  }
  void Write(Category category, std::size_t index,
             const Items::ContainerRecord& item) override {
    if (owner_ == nullptr) Refuse("missing item records");
    live_.Write(category, index, item);
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    container::Refuse(reason);
  }
  [[nodiscard]] const VS::Items::ContainerData* Owner() const { return owner_; }

 private:
  VS::Items::ContainerData* owner_;
  Items::LiveContainerWorld live_;
};

Menu_Container_InventoryStorage& Storage() {
  auto* const storage = VS::Menu::Framework::inventoryStorage;
  if (storage == nullptr) Refuse("missing inventory storage");
  return *storage;
}

VS::Items::Container* Container(TransferStorage storage) {
  switch (storage) {
    case TransferStorage::kActive:
      return Menu_Container_containerData;
    case TransferStorage::kOther:
      return Menu_Container_otherStorage;
    case TransferStorage::kContainer:
      return &Storage().container;
    default:
      return nullptr;
  }
}

VS::Items::ContainerData* Data(TransferStorage storage) {
  switch (storage) {
    case TransferStorage::kStaged:
      return &Storage().stagedTransfer;
    case TransferStorage::kPreview:
      return &Storage().transferPreview;
    default: {
      auto* const container = Container(storage);
      return container == nullptr ? nullptr : &container->items;
    }
  }
}

Menu_Framework_MenuRow& Row(std::int32_t row) {
  auto* const item = Framework::LiveMenuItem(row);
  if (item == nullptr) Refuse("menu row outside the current row workspace");
  return *item;
}

char* MenuTextPointer(TransferText text) {
  if (text.table == TransferTextTable::kMenuText) {
    if (text.offset >= std::size(Menu_Container_menuText)) {
      Refuse("menu text outside MENUD's text table");
    }
    return reinterpret_cast<char*>(&Menu_Container_menuText[text.offset]);
  }
  if (VS::Menu::Framework::itemHelp == nullptr) {
    Refuse("item help text not loaded");
  }
  return reinterpret_cast<char*>(VS::Menu::Framework::itemHelp + text.offset);
}

char* FlagByte(TransferFlag flag) {
  switch (flag) {
    case TransferFlag::kBrowsingGadgetBag:
      return reinterpret_cast<char*>(&Menu_Container_browsingGadgetBag);
    case TransferFlag::kTitleVisible:
      return &Menu_Container_categoryTitleVisible;
    case TransferFlag::kIconsVisible:
      return &Menu_Container_categoryIconsVisible;
    case TransferFlag::kControlsMode:
      return &Menu_Container_controlsPanelMode;
    case TransferFlag::kTitleSlide:
      return &Menu_Container_categoryTitleSlideStep;
    case TransferFlag::kIconsSlide:
      return &Menu_Container_categoryIconsSlideStep;
    case TransferFlag::kControlsSlide:
      return &Menu_Container_controlsPanelSlideStep;
    case TransferFlag::kDetailsPhase:
      return &Menu_Container_detailsViewPhase;
  }
  Refuse("unknown container panel flag");
}

void PlayMenuSoundFor(TransferSound sound) {
  switch (sound) {
    case TransferSound::kLeave:
      PlayLeaveSound();
      break;
    case TransferSound::kInvalid:
      PlayInvalidSound();
      break;
    case TransferSound::kSelect:
      PlaySelectSound();
      break;
    case TransferSound::kChange:
      PlayChangeSound();
      break;
  }
}

class LiveTransferWorld final : public TransferWorld {
 public:
  LiveTransferWorld() = default;
  LiveTransferWorld(const LiveTransferWorld&) = delete;
  LiveTransferWorld& operator=(const LiveTransferWorld&) = delete;
  LiveTransferWorld(LiveTransferWorld&&) = delete;
  LiveTransferWorld& operator=(LiveTransferWorld&&) = delete;
  ~LiveTransferWorld() override { ReleaseItemRows(); }

  [[noreturn]] void Refuse(std::string_view reason) override {
    container::Refuse(reason);
  }

  std::uint8_t Flag(TransferFlag flag) override {
    return static_cast<std::uint8_t>(*FlagByte(flag));
  }
  void SetFlag(TransferFlag flag, std::uint8_t value) override {
    *FlagByte(flag) = static_cast<char>(value);
  }
  std::uint8_t CursorMemorySetting() override {
    return static_cast<std::uint8_t>(VS::Input::settings.cursor_memory);
  }
  void SetCursorMemorySetting(std::uint8_t value) override {
    VS::Input::settings.cursor_memory = static_cast<char>(value);
  }
  void SetContainerEmpty(bool empty) override {
    VS::Input::settings.container_empty = empty ? 1U : 0U;
  }
  void SetFreezeTabArrows(std::uint8_t value) override {
    Menu_Inventory_freezeTabArrows = static_cast<char>(value);
  }
  std::uint8_t CategoryToken() override {
    return static_cast<std::uint8_t>(
        VS::Menu::Framework::menuMemory.opaqueA0[0]);
  }
  void SetCategoryToken(std::uint8_t token) override {
    VS::Menu::Framework::menuMemory.opaqueA0[0] = static_cast<char>(token);
  }
  void ClearCategoryMemory() override {
    std::memset(VS::Menu::Framework::menuMemory.opaqueA0, 0,
                sizeof VS::Menu::Framework::menuMemory.opaqueA0);
  }
  // The original wrote cursorMemories[(category + 0x51) * 2], past that
  // array's end into opaqueA0's row/page pair for the category.
  void SetCategoryCursor(Category category, std::uint8_t row,
                         std::uint8_t page) override {
    const auto offset =
        kFirstCategoryCursor + static_cast<std::size_t>(category) * 2;
    if (offset + 1 >= std::size(VS::Menu::Framework::menuMemory.opaqueA0)) {
      Refuse("category cursor outside menu memory");
    }
    VS::Menu::Framework::menuMemory.opaqueA0[offset] = static_cast<char>(row);
    VS::Menu::Framework::menuMemory.opaqueA0[offset + 1] =
        static_cast<char>(page);
  }
  std::uint16_t DirectionSelection() override {
    return VS::Menu::Framework::menuMemory.opaque9E;
  }
  void SetDirectionSelection(std::uint16_t selection) override {
    VS::Menu::Framework::menuMemory.opaque9E = selection;
  }
  void ReturnFromMenu(std::uint8_t return_state) override {
    VS::Menu::Framework::menuState.currentMenu =
        VS::Menu::Framework::menuState.returnState;
    VS::Menu::Framework::menuState.returnState =
        static_cast<char>(return_state);
  }
  std::uint32_t PressedButtons() override {
    return static_cast<std::uint32_t>(VS::Input::buttonsPressed);
  }
  std::uint32_t HeldButtons() override {
    return static_cast<std::uint32_t>(VS::Input::buttonsState);
  }
  std::uint32_t RepeatedButtons() override {
    return static_cast<std::uint32_t>(VS::Input::buttonRepeat);
  }
  std::uint16_t MenuTextEntry(std::size_t index) override {
    if (index >= std::size(Menu_Container_menuText)) {
      Refuse("menu text entry outside MENUD's text table");
    }
    return Menu_Container_menuText[index];
  }
  std::uint16_t ItemHelpEntry(std::size_t index) override {
    if (VS::Menu::Framework::itemHelp == nullptr) {
      Refuse("item help text not loaded");
    }
    return VS::Menu::Framework::itemHelp[index];
  }
  std::uint8_t RowAnimationStep(std::size_t index) override {
    if (index >= std::size(Menu_Framework_rowAnimationSteps)) {
      Refuse("row animation step outside its table");
    }
    return static_cast<std::uint8_t>(Menu_Framework_rowAnimationSteps[index]);
  }
  std::uint8_t InventoryCapacity(Category category) override {
    const auto index = static_cast<std::size_t>(category);
    if (index >= std::size(VS::Items::kCategoryCapacities)) {
      Refuse("inventory capacity outside its table");
    }
    return VS::Items::kCategoryCapacities[index];
  }
  std::uint16_t ContainerCapacity(Category category) override {
    const auto index = static_cast<std::size_t>(category);
    if (index >= Items::kItemCategoryCount) {
      Refuse("container capacity outside its table");
    }
    return Menu_Container_containerItemCapacities[index];
  }
  std::uint16_t TransferredMiscCount() override {
    return Menu_Container_transferredMiscCount;
  }

  Items::ContainerWorld& Records(TransferStorage storage) override {
    const auto index = static_cast<std::size_t>(storage);
    if (index >= kStorageCount) Refuse("unknown record owner");
    auto* const owner = Data(storage);
    auto& records = records_.at(index);
    if (!records || records->Owner() != owner) records.emplace(owner);
    return *records;
  }
  std::span<std::uint16_t> Indices(TransferStorage storage) override {
    auto* const container = Container(storage);
    if (container == nullptr) Refuse("missing container index owner");
    return {reinterpret_cast<std::uint16_t*>(&container->indices),
            Items::kContainerIndexEntries};
  }
  std::span<std::uint16_t> PendingTransfers() override {
    return Storage().itemsToTransfer;
  }
  void SelectStorages(bool browsing_gadget_bag) override {
    auto& storage = Storage();
    Menu_Container_containerData =
        browsing_gadget_bag ? &storage.gadgetBag : &storage.container;
    Menu_Container_otherStorage =
        browsing_gadget_bag ? &storage.container : &storage.gadgetBag;
  }
  void ClearStagedTransfers() override {
    auto& storage = Storage();
    Core::Memory::ZeroLiveWordsBackward(&storage.stagedTransfer,
                                        sizeof storage.stagedTransfer);
    Core::Memory::ZeroLiveWordsBackward(storage.itemsToTransfer,
                                        sizeof storage.itemsToTransfer);
  }
  void PublishActiveRecords() override {
    auto* const container = Menu_Container_containerData;
    if (container == nullptr) Refuse("missing active container");
    VS::Menu::Framework::weapons = container->items.weapons.data();
    VS::Menu::Framework::blades = container->items.blades.data();
    VS::Menu::Framework::grips = container->items.grips.data();
    VS::Menu::Framework::shields = container->items.shields.data();
    VS::Menu::Framework::armor = container->items.armor.data();
    VS::Menu::Framework::gems = container->items.gems.data();
  }
  void StoreContainerSnapshot() override {
    auto& storage = Storage();
    Core::Memory::CopyLiveHalfwordBlocks(&storage.container.items,
                                         &storage.storedContainer,
                                         sizeof storage.container.items);
  }
  void SnapshotGadgetBag() override { Items::SnapshotLiveGadgetBag(); }
  void CommitGadgetBag() override { Items::CommitLiveGadgetBag(); }
  void RebuildCategoryIndices(TransferStorage storage,
                              Category category) override {
    auto* const container = Container(storage);
    if (container == nullptr) Refuse("missing container index owner");
    Items::RebuildLiveContainerCategoryIndices(static_cast<int>(category),
                                               container);
  }
  void RebuildPendingTransfers() override { RebuildLivePendingTransfers(); }
  void PreviewTransferredMisc() override {
    Items::PreviewLiveTransferredMisc();
  }
  void RefreshMiscTransferCount() override {
    Items::RefreshLiveMiscTransferCount();
  }
  std::int32_t CopyItem(std::int32_t flags, TransferStorage target,
                        std::int32_t slot, TransferStorage source) override {
    auto* const destination = Data(target);
    auto* const origin = Data(source);
    if (destination == nullptr || origin == nullptr) {
      Refuse("missing item copy owner");
    }
    return Items::CopyLiveContainerItem(flags, destination, slot, origin);
  }
  std::int32_t TransferCapacityError(Category category,
                                     std::int32_t slot) override {
    return LiveTransferCapacityError(static_cast<int>(category), slot);
  }
  void StageItemTransfer(Category category, std::int32_t slot) override {
    StageLiveItemTransfer(static_cast<int>(category), slot);
  }
  void UndoPendingTransfer(std::int32_t row) override {
    UndoLivePendingTransfer(row);
  }

  void AcquireItemRows() override {
    ReleaseItemRows();
    auto* const block =
        Core::Memory::AllocateLiveFromHighEnd(sizeof(ItemRowsWorkspace));
    if (block == nullptr) Refuse("item row workspace allocation failed");
    workspace_ = new (block) ItemRowsWorkspace{};
  }
  std::int32_t BuildItemRows(Category category) override {
    auto& rows = Workspace();
    return BuildLiveItemRows(static_cast<int>(category), rows.texts.data(),
                             rows.types.data(), rows.descriptions.data());
  }
  void MarkItemRow(std::size_t row, std::uint32_t flags) override {
    auto& rows = Workspace();
    if (row >= rows.types.size()) Refuse("item row outside row workspace");
    rows.types.at(row) = static_cast<int>(
        static_cast<std::uint32_t>(rows.types.at(row)) | flags);
  }
  void CreateItemMenuRows(std::int32_t count, std::int32_t row_info) override {
    auto& rows = Workspace();
    if (count < 0 || static_cast<std::size_t>(count) > rows.types.size()) {
      Refuse("item row count outside row workspace");
    }
    Framework::CreateLiveMenuRows(count, row_info, rows.texts.data(),
                                  rows.types.data());
  }
  void ReleaseItemRows() override {
    if (workspace_ == nullptr) return;
    Core::Memory::FreeLiveFromHighEnd(workspace_);
    workspace_ = nullptr;
  }
  void RestoreItemRows(Category category) override {
    RestoreLiveItemRows(static_cast<int>(category));
  }
  void SetMenuItem(const MenuItemPlacement& placement) override {
    if (Framework::SetLiveMenuItem(
            placement.row, placement.x, placement.y, placement.width,
            placement.flags, MenuTextPointer(placement.text)) == nullptr) {
      Refuse("menu row outside the current row workspace");
    }
  }
  void SetRowSelected(std::int32_t row, bool selected) override {
    Row(row).selected = selected ? 1U : 0U;
  }
  void ClearRowOutsetIcon(std::int32_t row) override {
    Row(row).outsetIcon = 0;
  }
  std::int16_t RowX(std::int32_t row) override { return Row(row).x; }
  void SetRowX(std::int32_t row, std::int16_t x) override { Row(row).x = x; }
  void SlideRow(std::int32_t row, std::uint8_t state,
                std::int16_t target) override {
    auto& item = Row(row);
    item.state = state;
    item.targetPosition0 = target;
  }
  void SetRowCounts(std::int32_t row, std::uint16_t count,
                    std::uint16_t max) override {
    auto& item = Row(row);
    item.max = max;
    item.count = count;
  }
  void ClearMenuExcept(std::int32_t row) override {
    Framework::ClearLiveMenuExcept(row);
  }
  void FlyOutRowsLeftExcept(std::int32_t row) override {
    Framework::FlyOutLiveRowsLeftExcept(row);
  }
  std::int32_t SelectedRow() override { return Framework::LiveSelectedRow(); }
  bool RowsIdle() override { return Framework::AreLiveMenuRowsIdle() != 0; }
  std::int32_t CloseRowsAndRememberCursor() override {
    return Framework::CloseLiveRowsAndRememberCursor();
  }
  void CreateActionRows(std::int32_t count, std::int32_t menu,
                        std::span<const TransferText> texts,
                        std::span<const std::int32_t> types) override {
    if (count < 0 || static_cast<std::size_t>(count) > kActionRowCapacity ||
        texts.size() < static_cast<std::size_t>(count) * kTextsPerRow ||
        types.size() > kActionRowCapacity) {
      Refuse("action rows outside their workspace");
    }
    if (VS::Menu::Framework::rowTypeBuf == nullptr) {
      Refuse("missing row type buffer");
    }
    std::array<char*, kActionRowCapacity * kTextsPerRow> pointers{};
    for (std::size_t text = 0; text < texts.size() && text < pointers.size();
         ++text) {
      pointers.at(text) = MenuTextPointer(texts[text]);
    }
    const std::span<int> row_types(VS::Menu::Framework::rowTypeBuf,
                                   kActionRowCapacity);
    for (std::size_t row = 0; row < kActionRowCapacity; ++row) {
      row_types[row] = row < types.size() ? types[row] : 0;
    }
    Framework::CreateLiveActionRows(count, menu, pointers.data(),
                                    VS::Menu::Framework::rowTypeBuf);
  }
  std::int32_t HighlightedActionRow() override {
    return Framework::LiveHighlightedActionRow();
  }
  void ShowActionInformation(std::int32_t row) override {
    Framework::UpdateLiveInformationVisibility(
        row, VS::Menu::Framework::actionMenuState);
  }
  void UpdateActionMenu() override { Framework::UpdateLiveActionMenu(); }
  std::int32_t ActionSelectionResult() override {
    return Framework::LiveActionSelectionResult();
  }

  void InitializeInformationBox() override {
    Framework::InitializeLiveInformationBox();
  }
  void DismissInformationBox() override {
    Framework::DismissLiveInformationBox();
  }
  void SetInformationMessage(TransferText text) override {
    Framework::SetLiveInformationMessage(MenuTextPointer(text));
  }
  void SetPanelVisibility(std::uint8_t visibility) override {
    SetLivePanelVisibility(static_cast<char>(visibility));
  }
  void PlaySound(TransferSound sound) override { PlayMenuSoundFor(sound); }
  std::int32_t LoadItemText(std::int32_t load) override {
    return Framework::LoadLiveItemText(load);
  }
  bool UnloadItemTextWhenReady() override {
    return Framework::UnloadLiveItemTextWhenReady() != 0;
  }
  std::int32_t UpdateItemDetails(std::int32_t selection) override {
    return UpdateLiveItemDetails(selection);
  }
  std::int32_t UpdateTransferConfirmation(std::int32_t initialize) override {
    return UpdateLiveTransferConfirmation(initialize);
  }
  std::int32_t UpdateDiscardMenu(std::int32_t selection) override {
    return UpdateLiveDiscardMenu(selection);
  }
  std::int32_t UpdateSortMenu(std::int32_t selection) override {
    return UpdateLiveSortMenu(selection);
  }
  std::int32_t UpdateItemList(std::int32_t direction) override {
    return container::UpdateItemList(direction, g_item_list, *this);
  }
  std::int32_t UpdateTransferDirection(std::int32_t initialize) override {
    return container::UpdateTransferDirection(initialize, g_direction, *this);
  }

  void DrawCompactText(std::string_view text, std::int32_t xy,
                       std::optional<std::uint32_t> color,
                       TransferTextLayer layer) override {
    u_long* chain = nullptr;
    if (layer == TransferTextLayer::kAboveMenu) {
      chain = Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene,
                                             kAboveMenuChainSlot);
      if (chain == nullptr) Refuse("text chain outside the ordering table");
    }
    const std::string terminated(text);
    if (color) {
      Render::DrawLiveCompactTextColor(terminated.c_str(), xy,
                                       static_cast<int>(*color), chain);
    } else {
      Render::DrawLiveCompactText(terminated.c_str(), xy, chain);
    }
  }
  std::uint8_t DrawCursor(std::uint8_t animation,
                          std::int32_t position) override {
    return static_cast<std::uint8_t>(
        Framework::DrawLiveCursor(animation, position));
  }
  void DrawStorageCapacities() override { DrawLiveStorageCapacities(); }
  void DrawCategoryIcons(std::uint8_t step, std::uint8_t phase) override {
    DrawLiveCategoryIcons(step, phase);
  }
  void DrawButtonPromptBackground(std::int32_t x, std::int32_t y,
                                  std::int32_t width,
                                  std::int32_t height) override {
    Framework::DrawLiveButtonPromptBackground(x, y, width, height);
  }
  void DrawButtonIcon(std::int32_t button, std::int32_t x,
                      std::int32_t y) override {
    Framework::DrawLiveButtonIcon(button, x, y, nullptr);
  }
  void DrawCapacityCount(std::int32_t limit, std::int32_t xy,
                         std::int32_t count) override {
    DrawLiveCapacityCount(limit, xy, count);
  }

 private:
  ItemRowsWorkspace& Workspace() {
    if (workspace_ == nullptr) Refuse("item row workspace not acquired");
    return *workspace_;
  }

  ItemRowsWorkspace* workspace_ = nullptr;
  std::array<std::optional<RecordView>, kStorageCount> records_;
};

struct RowArguments {
  VS::Items::ContainerData* container;
  char** row_strings;
  int* row_types;
  char* buffer;
  int index;
};

std::span<std::uint8_t> RowBuffer(char* buffer) {
  if (buffer == nullptr) return {};
  return {reinterpret_cast<std::uint8_t*>(buffer),
          Framework::kRowDescriptionBufferBytes};
}

void StoreRowText(const Framework::ItemRowText& text,
                  const RowArguments& arguments, char* weapon_name) {
  if (text.name) {
    arguments.row_strings[0] = Framework::ResolveLiveItemRowText(
        *text.name, arguments.buffer, weapon_name);
  }
  if (text.description) {
    arguments.row_strings[1] = Framework::ResolveLiveItemRowText(
        *text.description, arguments.buffer, weapon_name);
  }
  *arguments.row_types = static_cast<int>(text.row_type);
}

template <typename Table>
void RequireRow(const RowArguments& arguments, const Table& table) {
  if (arguments.container == nullptr || arguments.row_strings == nullptr ||
      arguments.row_types == nullptr) {
    Refuse("missing item row output");
  }
  if (arguments.index < 0 ||
      static_cast<std::size_t>(arguments.index) >= std::size(table)) {
    Refuse("item row outside its record table");
  }
}

template <typename Entry>
auto Guard(std::string_view name, Entry&& entry) -> decltype(entry()) {
  try {
    return entry();
  } catch (const Refusal& refusal) {
    Warn(name, refusal.reason);
    if constexpr (!std::is_void_v<decltype(entry())>) return {};
  }
}
}  // namespace

void LiveBuildWeaponRow(VS::Items::ContainerData* container, char** row_strings,
                        int* row_types, char* buffer, int index) {
  Guard("BuildWeaponRow", [&] {
    const RowArguments arguments{container, row_strings, row_types, buffer,
                                 index};
    if (container == nullptr) Refuse("missing item row output");
    RequireRow(arguments, container->weapons);
    auto& legacy = container->weapons[index];
    RecordView records(container);
    Framework::LiveItemRowsHost host;
    const auto row = BuildWeaponRow(host, legacy, records, RowBuffer(buffer));
    Items::Live::WarnRefusal("BuildContainerWeaponRow", row.status);
    StoreRowText(row.text, arguments, legacy.name.data());
  });
}

void LiveBuildShieldRow(VS::Items::ContainerData* container, char** row_strings,
                        int* row_types, char* buffer, int index) {
  Guard("BuildShieldRow", [&] {
    const RowArguments arguments{container, row_strings, row_types, buffer,
                                 index};
    if (container == nullptr) Refuse("missing item row output");
    RequireRow(arguments, container->shields);
    RecordView records(container);
    Framework::LiveItemRowsHost host;
    const auto text = BuildShieldRow(host, container->shields[index], records,
                                     RowBuffer(buffer));
    StoreRowText(text, arguments, nullptr);
  });
}

int LiveUpdateItemList(int transfer_direction_selection) {
  return Guard("UpdateItemList", [&] {
    LiveTransferWorld world;
    return UpdateItemList(transfer_direction_selection, g_item_list, world);
  });
}

int LiveUpdateTransferDirection(int initialize_direction_menu) {
  return Guard("UpdateTransferDirection", [&] {
    LiveTransferWorld world;
    return UpdateTransferDirection(initialize_direction_menu, g_direction,
                                   world);
  });
}

int LiveUpdateScreen(u_char* screen_step) {
  return Guard("UpdateScreen", [&] {
    if (screen_step == nullptr) Refuse("missing screen step");
    LiveTransferWorld world;
    return UpdateScreen(*screen_step, g_screen, world);
  });
}

void ResetLiveContainerTransferStatics() {
  g_item_list = {};
  g_direction = {};
  g_screen = {};
}
}  // namespace VS::Menu::container
