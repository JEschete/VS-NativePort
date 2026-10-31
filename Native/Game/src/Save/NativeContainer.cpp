// Hand-maintained (tools/port/owned.csv).
extern "C" {
#include "Save/NativeContainer.h"

#include "Audio/Music.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Audio/SoundBanks.h"
#include "Battle/Feedback.h"
#include "Battle/Hud.h"
#include "Battle/TextBoxes.h"
#include "Boot/Lifecycle.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Content/StringTables.h"
#include "Core/Diagnostics.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "Core/Random.h"
#include "Core/Text/TextCode.h"
#include "DiscQueue.h"
#include "Event/ScriptAudio.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/RoomRendering.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Inventory.h"
#include "Items/Weapons.h"
#include "Legacy/Psx.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/EquipmentDetails.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Sounds.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Render/Font.h"
#include "Render/LegacyPackets.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/Sprites.h"
#include "Render/TextureAnimation.h"
#include "Render/Textures.h"
#include "Render/UiPrimitives.h"
#include "Save/MemoryCard/GameFiles.h"
#include "Save/MemoryCard/Resources.h"
#include "Save/MenuAssets.h"
#include "Save/NativeMenu.h"
#include "State/Ashley.h"
#include "State/GameTime.h"
#include "State/PlayTime.h"
#include "State/Progress.h"
#include "State/Settings.h"
#include "State/StateFlags.h"
#include "Title/SaveCard.h"
#include "Title/SaveFiles.h"
#include "Title/TitleMenu.h"

enum { kContainerComparisonActor = 16, kConfirmContainerOpening = 5 };
enum ContainerSaveState {
  kInitializeContainer,
  kConfirmContainer,
  kLoadContainerFiles,
  kSelectContainerFile,
  kStageContainerItems = 7,
  kWaitForContainerScreen,
  kReloadContainerFiles,
  kBeginContainerWrite,
  kWaitForContainerWrite,
  kConfirmContainerDiscard,
  kRestoreContainerItems,
  kCloseContainerFiles,
  kDismissContainerMenu,
  kReleaseContainerStorage
};
enum {
  kContainerHeadingWidth = 320,
  kContainerHeadingY = 18,
  kContainerHeadingColor = 126,
  kContainerHeadingHeight = 8,
  kContainerHeadingPosition = 180,
  kContainerInventoryMenu = 14,
  kContainerHiddenHudOffset = 0x200000,
  kContainerBackgroundPriority = 2046,
  kContainerBackgroundAlpha = 128,
  kContainerRestorePriority = -4,
  kContainerInformationBox = 7
};

static void InitializeContainerSave(u_char* state) {
  Menu_PlayOpenSound();
  Menu_Framework_unusedContainerEmptyBackup = Input_settings.containerEmpty;
  Menu_Framework_unusedInventoryStorage =
      reinterpret_cast<Menu_Container_InventoryStorage*>(
          Core_Memory_AllocateFromHighEnd(
              sizeof *Menu_Framework_unusedInventoryStorage));
  Core_Memory_ZeroWordsBackward(Menu_Framework_unusedInventoryStorage,
                                sizeof *Menu_Framework_unusedInventoryStorage);
  Menu_Framework_SetActorComparisonStats(kContainerComparisonActor);
  Menu_InitInformationTextBox((enum Menu_InformationBoxHeader)(1));
  Menu_Files_UpdateContainerConfirmation(kConfirmContainerOpening);
  Menu_Framework_SetInformationMessage(reinterpret_cast<char*>(
      &Menu_Files_containerStrings
          [Text_Menu7_container_Offset_checkContainer]));
  *state = kConfirmContainer;
}

static void OpenContainerFiles(u_char* state) {
  Save_MemoryCard_InitializeMenuResourcesStep(1);
  Menu_Framework_SetBackgroundFade(1);
  Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_Menu);
  Menu_Framework_MenuRow* menu_item = Menu_Framework_SetMenuItem(
      0, kContainerHeadingWidth, kContainerHeadingY, kContainerHeadingColor,
      kContainerHeadingHeight,
      reinterpret_cast<char*>(
          &Menu_Files_containerStrings[Text_Menu7_container_Offset_container]));
  menu_item->state = 2;
  menu_item->targetPosition0 = kContainerHeadingPosition;
  menu_item->selected = 1;
  *state = kLoadContainerFiles;
}

static void StageContainerInventory(u_char* state) {
  Core_Memory_CopyHalfwordBlocks(
      Menu_Framework_unusedInventoryStorage->savedInventory,
      &Audio_Seq_unusedInventory,
      sizeof Menu_Framework_unusedInventoryStorage->savedInventory);
  Core_Memory_CopyHalfwordBlocks(
      &Menu_Framework_unusedInventoryStorage->savedInventoryIndices,
      &Audio_Seq_unusedInventoryIndices,
      sizeof Menu_Framework_unusedInventoryStorage->savedInventoryIndices);
  Core_Memory_CopyHalfwordBlocks(
      &Menu_Framework_unusedInventoryStorage->storedContainer,
      Save_NativeContainerData(),
      sizeof Menu_Framework_unusedInventoryStorage->storedContainer);
  Save_MemoryCard_ReleaseMenuResources();
  *state = kWaitForContainerScreen;
  Menu_Framework_menuState.returnState = Menu_Framework_menuState.currentMenu;
  Menu_Framework_menuState.currentMenu = kContainerInventoryMenu;
}

static int UpdateContainerSaveCompletion(u_char* state);

static void RestoreContainerInventory(void) {
  Input_settings.containerEmpty = Menu_Framework_unusedContainerEmptyBackup;
  Core_Memory_CopyHalfwordBlocks(
      &Audio_Seq_unusedInventory,
      Menu_Framework_unusedInventoryStorage->savedInventory,
      sizeof Menu_Framework_unusedInventoryStorage->savedInventory);
  Core_Memory_CopyHalfwordBlocks(
      &Audio_Seq_unusedInventoryIndices,
      &Menu_Framework_unusedInventoryStorage->savedInventoryIndices,
      sizeof Menu_Framework_unusedInventoryStorage->savedInventoryIndices);
}

// decomp: MENU7.PRG vs_menu7_saveContainerMenu (src/MENU/MENU7.PRG/260.c:3944)
int Menu_Files_UpdateContainerSave(u_char* state) {
  int option;
  switch (*state) {
    case kInitializeContainer:
      InitializeContainerSave(state);
      break;
    case kConfirmContainer:
      option = Menu_Files_UpdateContainerConfirmation(0);
      if (option != 0) {
        if (option >= 0) {
          OpenContainerFiles(state);
        } else {
          *state = kReleaseContainerStorage;
        }
      }
      break;
    case kLoadContainerFiles:
      if ((Menu_Framework_AreMenuRowsIdle() != 0) &&
          (Save_MemoryCard_InitializeMenuResourcesStep(0) != 0)) {
        Field_SetFieldRenderingEnabled(0);
        Battle_SetHudHidden(1);
        Battle_hudSlideOffset = kContainerHiddenHudOffset;
        Menu_Framework_SetBackgroundPriority(kContainerBackgroundPriority,
                                             kContainerBackgroundAlpha);
        Menu_Files_ResetElements();
        Save_NativeSelectContainer(1);
        *state = kSelectContainerFile;
      }
      break;
    case kSelectContainerFile:
      option = Save_NativeSelectContainer(0);
      if (option != 0) {
        if (option < 0) {
          *state = kCloseContainerFiles;
        } else {
          *state = kStageContainerItems;
        }
      }
      Menu_Files_DrawFrame(Render_frameBuf);
      break;
    case kStageContainerItems:
      StageContainerInventory(state);
      break;
    default:
      return UpdateContainerSaveCompletion(state);
  }
  return 0;
}

static int UpdateContainerSaveCompletion(u_char* state) {
  int option;
  switch (*state) {
    case kWaitForContainerScreen:
      if (Menu_Framework_AreMenuRowsIdle() != 0) {
        Save_MemoryCard_InitializeMenuResourcesStep(1);
        *state = kReloadContainerFiles;
      }
      break;
    case kReloadContainerFiles:
      if (Save_MemoryCard_InitializeMenuResourcesStep(0) != 0) {
        option = Menu_Framework_menuState.returnState;
        if (option != 0) {
          Menu_InitInformationTextBox((enum Menu_InformationBoxHeader)(1));
          if (option == 1) {
            Menu_Files_UpdateContainerConfirmation(1);
            Menu_Framework_SetInformationMessage(reinterpret_cast<char*>(
                &Menu_Files_containerStrings
                    [Text_Menu7_container_Offset_cancelWarning]));
            *state = kConfirmContainerDiscard;
          } else {
            *state = kBeginContainerWrite;
          }
        } else {
          *state = kRestoreContainerItems;
        }
      }
      break;
    case kBeginContainerWrite:
      Menu_Files_CopySelectedContainerItems(
          Save_NativeContainerData(),
          &Menu_Framework_unusedInventoryStorage->container.items,
          &Menu_Framework_unusedInventoryStorage->container.indices);
      Menu_Files_UpdateDataSaveMenu(2);
      *state = kWaitForContainerWrite;
      break;
    case kWaitForContainerWrite:
      option = Menu_Files_UpdateDataSaveMenu(0);
      if (option != 0) {
        if ((Menu_Files_dataNotSaved == 0) && (option < 0)) {
          Menu_InitInformationTextBox((enum Menu_InformationBoxHeader)(1));
          Menu_Files_UpdateContainerConfirmation(1);
          Menu_Framework_SetInformationMessage(reinterpret_cast<char*>(
              &Menu_Files_containerStrings
                  [Text_Menu7_container_Offset_cancelWarning]));
          *state = kConfirmContainerDiscard;
        } else {
          *state = kCloseContainerFiles;
        }
      }
      Menu_Files_DrawFrame(Render_frameBuf);
      break;
    case kConfirmContainerDiscard:
      option = Menu_Files_UpdateContainerConfirmation(0);
      if (option != 0) {
        if (option == 1) {
          *state = kBeginContainerWrite;
          break;
        }
        *state = kRestoreContainerItems;
      }
      break;
    case kRestoreContainerItems:
      RestoreContainerInventory();
      /* fallthrough */
    case kCloseContainerFiles:
      Field_SetFieldRenderingEnabled(1);
      Battle_SetHudHidden(0);
      Battle_hudSlideOffset = 0;
      Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
      Menu_Framework_SetBackgroundPriority(kContainerRestorePriority,
                                           kContainerBackgroundAlpha);
      *state = kDismissContainerMenu;
      break;
    case kDismissContainerMenu:
      Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
      Menu_Framework_DismissInformationBox();
      Menu_Framework_SetBackgroundFade(0);
      if (Menu_Framework_AreMenuRowsIdle() != 0) {
        Save_MemoryCard_ReleaseMenuResources();
        *state = kReleaseContainerStorage;
      }
      break;
    case kReleaseContainerStorage:
      if ((Menu_DismissTextBox(kContainerInformationBox) == 0) &&
          (Menu_Framework_backgroundFadeStep == 0)) {
        Core_Memory_FreeFromHighEnd(Menu_Framework_unusedInventoryStorage);
        Save_NativeReleaseContainer();
        *state = kInitializeContainer;
        return 1;
      }
      break;
    default:
      Core_Errorf("save", "Invalid container menu state: %u", (unsigned)*state);
      break;
  }
  return 0;
}
}  // extern "C"
