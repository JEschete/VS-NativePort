// Hand-maintained (tools/port/owned.csv).
#include "Save/NativeContainerLive.hpp"

#include <cstdint>
#include <cstdlib>
#include <span>

#include "Battle/Hud.h"
#include "Battle/HudStateLive.hpp"
#include "Content/StringTables.h"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/Copy.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/Inventory.h"
#include "Items/Weapons.h"
#include "Menu/Container/Storage.h"
#include "Menu/FilesLive.hpp"
#include "Menu/FilesLiveState.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Sounds.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/Display.hpp"
#include "Render/Render.hpp"
#include "Save/FileMenu.hpp"
#include "Save/FileMenuLive.hpp"
#include "Save/MemoryCard/GameFiles.h"
#include "Save/MemoryCard/Resources.h"
#include "Save/MenuAssetsLive.hpp"
#include "Save/NativeContainer.hpp"
#include "Save/NativeMenu.hpp"
#include "Title/SaveFiles.h"

namespace VS::Menu::Files {
namespace {

constexpr int kContainerComparisonActor = 16;
constexpr int kConfirmContainerOpening = 5;
constexpr int kConfirmContainerDiscard = 1;
constexpr int kContainerHeadingWidth = 320;
constexpr int kContainerHeadingY = 18;
constexpr int kContainerHeadingColor = 126;
constexpr int kContainerHeadingHeight = 8;
constexpr int kContainerHeadingPosition = 180;
constexpr int kContainerInventoryMenu = 14;
constexpr int kContainerHiddenHudOffset = 0x200000;
constexpr int kContainerBackgroundPriority = 2046;
constexpr int kContainerBackgroundAlpha = 128;
constexpr int kContainerRestorePriority = -4;
constexpr int kContainerInformationBox = 7;

char* ContainerString(int offset) {
  return reinterpret_cast<char*>(&containerStrings[offset]);
}

Menu_Container_InventoryStorage& Storage() {
  if (VS::Menu::Framework::unusedInventoryStorage == nullptr) {
    VS::Core::Errorf("save", "Container save submenu used without its storage");
    std::abort();
  }
  return *VS::Menu::Framework::unusedInventoryStorage;
}

class LiveContainerSave final : public ContainerSaveHost {
 public:
  void BeginSession() override {
    VS::Menu::PlayOpenSound();
    VS::Menu::Framework::unusedContainerEmptyBackup =
        static_cast<char>(VS::Input::settings.containerEmpty);
    VS::Menu::Framework::unusedInventoryStorage =
        static_cast<Menu_Container_InventoryStorage*>(
            VS::Core::Memory::AllocateLiveFromHighEnd(
                sizeof *VS::Menu::Framework::unusedInventoryStorage));
    static_assert(
        sizeof(Menu_Container_InventoryStorage) % sizeof(std::uint32_t) == 0);
    VS::Core::Memory::ZeroWordsBackward(
        std::as_writable_bytes(std::span(&Storage(), 1)),
        static_cast<std::int32_t>(sizeof Storage()));
    VS::Menu::Framework::SetLiveActorComparisonStats(kContainerComparisonActor);
  }

  void OpenInformationBox() override {
    VS::Menu::InitLiveInformationTextBox(
        static_cast<Menu_InformationBoxHeader>(1));
  }

  void ShowPrompt(ContainerPrompt prompt) override {
    const bool opening = prompt == ContainerPrompt::kOpenContainer;
    VS::Menu::Files::UpdateLiveContainerConfirmation(
        opening ? kConfirmContainerOpening : kConfirmContainerDiscard);
    VS::Menu::Framework::SetLiveInformationMessage(
        ContainerString(opening ? Text_Menu7_container_Offset_checkContainer
                                : Text_Menu7_container_Offset_cancelWarning));
  }

  int PollPrompt() override {
    return VS::Menu::Files::UpdateLiveContainerConfirmation(0);
  }

  void ShowContainerHeading() override {
    VS::Save::MemoryCard::StepLiveMenuResources(1);
    VS::Menu::Framework::SetLiveBackgroundFade(1);
    VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_Menu);
    Menu_Framework_MenuRow* row = VS::Menu::Framework::SetLiveMenuItem(
        0, kContainerHeadingWidth, kContainerHeadingY, kContainerHeadingColor,
        kContainerHeadingHeight,
        ContainerString(Text_Menu7_container_Offset_container));
    row->state = 2;
    row->targetPosition0 = kContainerHeadingPosition;
    row->selected = 1;
  }

  bool MenuRowsIdle() override {
    return VS::Menu::Framework::AreLiveMenuRowsIdle() != 0;
  }

  void BeginResourceLoad() override {
    VS::Save::MemoryCard::StepLiveMenuResources(1);
  }

  bool ResourcesReady() override {
    return VS::Save::MemoryCard::StepLiveMenuResources(0) != 0;
  }

  void OpenFilePicker() override {
    VS::Field::LiveSetFieldRenderingEnabled(0);
    VS::Battle::SetLiveHudHidden(1);
    Battle_hudSlideOffset = kContainerHiddenHudOffset;
    VS::Menu::Framework::SetLiveBackgroundPriority(kContainerBackgroundPriority,
                                                   kContainerBackgroundAlpha);
    VS::Menu::Files::ResetLiveElements();
    VS::Game::Save::SelectContainer(1);
  }

  int PollFilePicker() override { return VS::Game::Save::SelectContainer(0); }

  void DrawFiles() override {
    VS::Menu::Files::DrawLiveFrame(VS::Render::frameBuf);
  }

  void OpenContainerScreen() override {
    Menu_Container_InventoryStorage& storage = Storage();
    VS::Core::Memory::CopyLiveHalfwordBlocks(storage.savedInventory,
                                             &VS::Items::inventory,
                                             sizeof storage.savedInventory);
    VS::Core::Memory::CopyLiveHalfwordBlocks(
        &storage.savedInventoryIndices, &VS::Items::inventoryIndices,
        sizeof storage.savedInventoryIndices);
    VS::Core::Memory::CopyLiveHalfwordBlocks(&storage.storedContainer,
                                             VS::Game::Save::ContainerData(),
                                             sizeof storage.storedContainer);
    VS::Save::MemoryCard::ReleaseLiveMenuResources();
    VS::Menu::Framework::menuState.returnState =
        VS::Menu::Framework::menuState.currentMenu;
    VS::Menu::Framework::menuState.currentMenu = kContainerInventoryMenu;
  }

  ContainerScreenResult ContainerScreenOutcome() override {
    switch (VS::Menu::Framework::menuState.returnState) {
      case 0:
        return ContainerScreenResult::kRestore;
      case 1:
        return ContainerScreenResult::kConfirmDiscard;
      default:
        return ContainerScreenResult::kSave;
    }
  }

  void BeginWrite() override {
    Menu_Container_InventoryStorage& storage = Storage();
    VS::Menu::Files::CopyLiveSelectedContainerItems(
        VS::Game::Save::ContainerData(), &storage.container.items,
        &storage.container.indices);
    UpdateLiveDataSaveMenu(kContainerSaveInit);
  }

  int PollWrite() override { return UpdateLiveDataSaveMenu(0); }

  bool DataNotSaved() override { return dataNotSaved != 0; }

  void RestoreInventory() override {
    Menu_Container_InventoryStorage& storage = Storage();
    VS::Input::settings.containerEmpty =
        VS::Menu::Framework::unusedContainerEmptyBackup;
    VS::Core::Memory::CopyLiveHalfwordBlocks(&VS::Items::inventory,
                                             storage.savedInventory,
                                             sizeof storage.savedInventory);
    VS::Core::Memory::CopyLiveHalfwordBlocks(
        &VS::Items::inventoryIndices, &storage.savedInventoryIndices,
        sizeof storage.savedInventoryIndices);
  }

  void CloseFilePicker() override {
    VS::Field::LiveSetFieldRenderingEnabled(1);
    VS::Battle::SetLiveHudHidden(0);
    Battle_hudSlideOffset = 0;
    VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);
    VS::Menu::Framework::SetLiveBackgroundPriority(kContainerRestorePriority,
                                                   kContainerBackgroundAlpha);
  }

  void DismissMenu() override {
    VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_None);
    VS::Menu::Framework::DismissLiveInformationBox();
    VS::Menu::Framework::SetLiveBackgroundFade(0);
  }

  void ReleaseResources() override {
    VS::Save::MemoryCard::ReleaseLiveMenuResources();
  }

  bool InformationBoxClosed() override {
    return VS::Menu::DismissLiveTextBox(kContainerInformationBox) == 0 &&
           VS::Menu::Framework::backgroundFadeStep == 0;
  }

  void EndSession() override {
    VS::Core::Memory::FreeLiveFromHighEnd(
        VS::Menu::Framework::unusedInventoryStorage);
    VS::Menu::Framework::unusedInventoryStorage = nullptr;
    VS::Game::Save::ReleaseContainer();
  }

  void ReportInvalidState(unsigned state) override {
    VS::Core::Errorf("save", "Invalid container menu state: %u", state);
  }
};

}  // namespace

int UpdateLiveContainerSave(char& state) {
  auto typed =
      static_cast<ContainerSaveState>(static_cast<std::uint8_t>(state));
  LiveContainerSave host;
  const bool finished = UpdateContainerSave(typed, host);
  state = static_cast<char>(static_cast<std::uint8_t>(typed));
  return finished ? 1 : 0;
}

}  // namespace VS::Menu::Files
