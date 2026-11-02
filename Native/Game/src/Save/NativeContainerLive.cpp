// Hand-maintained (tools/port/owned.csv).
#include "Save/NativeContainerLive.hpp"

#include <cstdint>
#include <cstdlib>

#include "Core/Memory/WordCopyLive.hpp"
#include "Menu/Sounds.hpp"
#include "Save/MenuAssetsLive.hpp"
#include "Save/NativeContainer.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/Hud.h"
#include "Content/StringTables.h"
#include "Core/Diagnostics.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/Heap.h"
#include "Field/RoomRendering.h"
#include "Input/Controller.h"
#include "Items/Inventory.h"
#include "Items/Weapons.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/EquipmentDetails.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Render/Render.h"
#include "Save/MemoryCard/GameFiles.h"
#include "Save/MemoryCard/Resources.h"
#include "Save/NativeMenu.h"
#include "Title/SaveFiles.h"
}

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
  return reinterpret_cast<char*>(&Menu_Files_containerStrings[offset]);
}

Menu_Container_InventoryStorage& Storage() {
  if (Menu_Framework_unusedInventoryStorage == nullptr) {
    Core_Errorf("save", "Container save submenu used without its storage");
    std::abort();
  }
  return *Menu_Framework_unusedInventoryStorage;
}

class LiveContainerSave final : public ContainerSaveHost {
 public:
  void BeginSession() override {
    VS::Menu::PlayOpenSound();
    Menu_Framework_unusedContainerEmptyBackup =
        static_cast<char>(Input_settings.containerEmpty);
    Menu_Framework_unusedInventoryStorage =
        static_cast<Menu_Container_InventoryStorage*>(
            Core_Memory_AllocateFromHighEnd(
                sizeof *Menu_Framework_unusedInventoryStorage));
    Core_Memory_ZeroWordsBackward(&Storage(), sizeof Storage());
    Menu_Framework_SetActorComparisonStats(kContainerComparisonActor);
  }

  void OpenInformationBox() override {
    Menu_InitInformationTextBox(static_cast<Menu_InformationBoxHeader>(1));
  }

  void ShowPrompt(ContainerPrompt prompt) override {
    const bool opening = prompt == ContainerPrompt::OpenContainer;
    Menu_Files_UpdateContainerConfirmation(opening ? kConfirmContainerOpening
                                                   : kConfirmContainerDiscard);
    Menu_Framework_SetInformationMessage(
        ContainerString(opening ? Text_Menu7_container_Offset_checkContainer
                                : Text_Menu7_container_Offset_cancelWarning));
  }

  int PollPrompt() override {
    return Menu_Files_UpdateContainerConfirmation(0);
  }

  void ShowContainerHeading() override {
    VS::Save::MemoryCard::StepLiveMenuResources(1);
    Menu_Framework_SetBackgroundFade(1);
    Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_Menu);
    Menu_Framework_MenuRow* row = Menu_Framework_SetMenuItem(
        0, kContainerHeadingWidth, kContainerHeadingY, kContainerHeadingColor,
        kContainerHeadingHeight,
        ContainerString(Text_Menu7_container_Offset_container));
    row->state = 2;
    row->targetPosition0 = kContainerHeadingPosition;
    row->selected = 1;
  }

  bool MenuRowsIdle() override { return Menu_Framework_AreMenuRowsIdle() != 0; }

  void BeginResourceLoad() override {
    VS::Save::MemoryCard::StepLiveMenuResources(1);
  }

  bool ResourcesReady() override {
    return VS::Save::MemoryCard::StepLiveMenuResources(0) != 0;
  }

  void OpenFilePicker() override {
    Field_SetFieldRenderingEnabled(0);
    Battle_SetHudHidden(1);
    Battle_hudSlideOffset = kContainerHiddenHudOffset;
    Menu_Framework_SetBackgroundPriority(kContainerBackgroundPriority,
                                         kContainerBackgroundAlpha);
    Menu_Files_ResetElements();
    Save_NativeSelectContainer(1);
  }

  int PollFilePicker() override { return Save_NativeSelectContainer(0); }

  void DrawFiles() override { Menu_Files_DrawFrame(Render_frameBuf); }

  void OpenContainerScreen() override {
    Menu_Container_InventoryStorage& storage = Storage();
    VS::Core::Memory::CopyLiveHalfwordBlocks(storage.savedInventory,
                                             &Audio_Seq_unusedInventory,
                                             sizeof storage.savedInventory);
    VS::Core::Memory::CopyLiveHalfwordBlocks(
        &storage.savedInventoryIndices, &Audio_Seq_unusedInventoryIndices,
        sizeof storage.savedInventoryIndices);
    VS::Core::Memory::CopyLiveHalfwordBlocks(&storage.storedContainer,
                                             Save_NativeContainerData(),
                                             sizeof storage.storedContainer);
    VS::Save::MemoryCard::ReleaseLiveMenuResources();
    Menu_Framework_menuState.returnState = Menu_Framework_menuState.currentMenu;
    Menu_Framework_menuState.currentMenu = kContainerInventoryMenu;
  }

  ContainerScreenResult ContainerScreenOutcome() override {
    switch (Menu_Framework_menuState.returnState) {
      case 0:
        return ContainerScreenResult::Restore;
      case 1:
        return ContainerScreenResult::ConfirmDiscard;
      default:
        return ContainerScreenResult::Save;
    }
  }

  void BeginWrite() override {
    Menu_Container_InventoryStorage& storage = Storage();
    Menu_Files_CopySelectedContainerItems(Save_NativeContainerData(),
                                          &storage.container.items,
                                          &storage.container.indices);
    Menu_Files_UpdateDataSaveMenu(2);
  }

  int PollWrite() override { return Menu_Files_UpdateDataSaveMenu(0); }

  bool DataNotSaved() override { return Menu_Files_dataNotSaved != 0; }

  void RestoreInventory() override {
    Menu_Container_InventoryStorage& storage = Storage();
    Input_settings.containerEmpty = Menu_Framework_unusedContainerEmptyBackup;
    VS::Core::Memory::CopyLiveHalfwordBlocks(&Audio_Seq_unusedInventory,
                                             storage.savedInventory,
                                             sizeof storage.savedInventory);
    VS::Core::Memory::CopyLiveHalfwordBlocks(
        &Audio_Seq_unusedInventoryIndices, &storage.savedInventoryIndices,
        sizeof storage.savedInventoryIndices);
  }

  void CloseFilePicker() override {
    Field_SetFieldRenderingEnabled(1);
    Battle_SetHudHidden(0);
    Battle_hudSlideOffset = 0;
    Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
    Menu_Framework_SetBackgroundPriority(kContainerRestorePriority,
                                         kContainerBackgroundAlpha);
  }

  void DismissMenu() override {
    Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
    Menu_Framework_DismissInformationBox();
    Menu_Framework_SetBackgroundFade(0);
  }

  void ReleaseResources() override {
    VS::Save::MemoryCard::ReleaseLiveMenuResources();
  }

  bool InformationBoxClosed() override {
    return Menu_DismissTextBox(kContainerInformationBox) == 0 &&
           Menu_Framework_backgroundFadeStep == 0;
  }

  void EndSession() override {
    Core_Memory_FreeFromHighEnd(Menu_Framework_unusedInventoryStorage);
    Menu_Framework_unusedInventoryStorage = nullptr;
    Save_NativeReleaseContainer();
  }

  void ReportInvalidState(unsigned state) override {
    Core_Errorf("save", "Invalid container menu state: %u", state);
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
