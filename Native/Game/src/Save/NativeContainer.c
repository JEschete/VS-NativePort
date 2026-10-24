// Hand-maintained (tools/port/owned.csv).
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

// decomp: MENU7.PRG vs_menu7_saveContainerMenu (src/MENU/MENU7.PRG/260.c:3944)
int Menu_Files_UpdateContainerSave(u_char* state) {
  enum Menu_Framework_InitOnlyState {
    Menu_Framework_InitOnlyState_Init,
  };

  int option;

  switch (*state) {
    case Menu_Framework_InitOnlyState_Init:
      Menu_PlayOpenSound();
      Menu_Framework_unusedContainerEmptyBackup = Input_settings.containerEmpty;
      Menu_Framework_unusedInventoryStorage = Core_Memory_AllocateFromHighEnd(
          sizeof *Menu_Framework_unusedInventoryStorage);
      Core_Memory_ZeroWordsBackward(
          Menu_Framework_unusedInventoryStorage,
          sizeof *Menu_Framework_unusedInventoryStorage);
      Menu_Framework_SetActorComparisonStats(16);
      Menu_InitInformationTextBox(1);
      Menu_Files_UpdateContainerConfirmation(5);
      Menu_Framework_SetInformationMessage(
          (char*)&Menu_Files_containerStrings
              [Text_Menu7_container_Offset_checkContainer]);
      *state = 1;
      break;
    case 1:
      option = Menu_Files_UpdateContainerConfirmation(0);
      if (option != 0) {
        if (option >= 0) {
          Menu_Framework_MenuRow* menuItem;
          Save_MemoryCard_InitializeMenuResourcesStep(1);
          Menu_Framework_SetBackgroundFade(1);
          Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_Menu);
          menuItem = Menu_Framework_SetMenuItem(
              0, 0x140, 0x12, 0x7E, 8,
              (char*)&Menu_Files_containerStrings
                  [Text_Menu7_container_Offset_container]);
          menuItem->state = 2;
          menuItem->targetPosition0 = 180;
          menuItem->selected = 1;
          *state = 2;
        } else {
          *state = 16;
        }
      }
      break;
    case 2:
      if ((Menu_Framework_AreMenuRowsIdle() != 0) &&
          (Save_MemoryCard_InitializeMenuResourcesStep(0) != 0)) {
        Field_SetFieldRenderingEnabled(0);
        Battle_SetHudHidden(1);
        Battle_hudSlideOffset = 0x200000;
        Menu_Framework_SetBackgroundPriority(2046, 128);
        Menu_Files_ResetElements();
        Save_NativeSelectContainer(1);
        *state = 3;
      }
      break;
    case 3:
      option = Save_NativeSelectContainer(0);
      if (option != 0) {
        if (option < 0) {
          *state = 14;
        } else {
          *state = 7;
        }
      }
      Menu_Files_DrawFrame(Render_frameBuf);
      break;
    case 7:
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
      *state = 8;
      Menu_Framework_menuState.returnState =
          Menu_Framework_menuState.currentMenu;
      Menu_Framework_menuState.currentMenu = 14;
      break;
    case 8:
      if (Menu_Framework_AreMenuRowsIdle() != 0) {
        Save_MemoryCard_InitializeMenuResourcesStep(1);
        *state = 9;
      }
      break;
    case 9:
      if (Save_MemoryCard_InitializeMenuResourcesStep(0) != 0) {
        option = Menu_Framework_menuState.returnState;
        if (option != 0) {
          Menu_InitInformationTextBox(1);
          if (option == 1) {
            Menu_Files_UpdateContainerConfirmation(1);
            Menu_Framework_SetInformationMessage(
                (char*)&Menu_Files_containerStrings
                    [Text_Menu7_container_Offset_cancelWarning]);
            *state = 12;
          } else {
            *state = 10;
          }
        } else {
          *state = 13;
        }
      }
      break;
    case 10:
      Menu_Files_CopySelectedContainerItems(
          Save_NativeContainerData(),
          &Menu_Framework_unusedInventoryStorage->container.items,
          &Menu_Framework_unusedInventoryStorage->container.indices);
      Menu_Files_UpdateDataSaveMenu(2);
      *state = 11;
      break;
    case 11:
      option = Menu_Files_UpdateDataSaveMenu(0);
      if (option != 0) {
        if ((Menu_Files_dataNotSaved == 0) && (option < 0)) {
          Menu_InitInformationTextBox(1);
          Menu_Files_UpdateContainerConfirmation(1);
          Menu_Framework_SetInformationMessage(
              (char*)&Menu_Files_containerStrings
                  [Text_Menu7_container_Offset_cancelWarning]);
          *state = 12;
        } else {
          *state = 14;
        }
      }
      Menu_Files_DrawFrame(Render_frameBuf);
      break;
    case 12:
      option = Menu_Files_UpdateContainerConfirmation(0);
      if (option != 0) {
        if (option == 1) {
          *state = 10;
          break;
        }
        *state = 13;
      }
      break;
    case 13:
      Input_settings.containerEmpty = Menu_Framework_unusedContainerEmptyBackup;
      Core_Memory_CopyHalfwordBlocks(
          &Audio_Seq_unusedInventory,
          Menu_Framework_unusedInventoryStorage->savedInventory,
          sizeof Menu_Framework_unusedInventoryStorage->savedInventory);
      Core_Memory_CopyHalfwordBlocks(
          &Audio_Seq_unusedInventoryIndices,
          &Menu_Framework_unusedInventoryStorage->savedInventoryIndices,
          sizeof Menu_Framework_unusedInventoryStorage->savedInventoryIndices);
      /* fallthrough */
    case 14:
      Field_SetFieldRenderingEnabled(1);
      Battle_SetHudHidden(0);
      Battle_hudSlideOffset = 0;
      Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
      Menu_Framework_SetBackgroundPriority(-4, 0x80);
      *state = 15;
      break;
    case 15:
      Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
      Menu_Framework_DismissInformationBox();
      Menu_Framework_SetBackgroundFade(0);
      if (Menu_Framework_AreMenuRowsIdle() != 0) {
        Save_MemoryCard_ReleaseMenuResources();
        *state = 16;
      }
      break;
    case 16:
      if ((Menu_DismissTextBox(7) == 0) &&
          (Menu_Framework_backgroundFadeStep == 0)) {
        Core_Memory_FreeFromHighEnd(Menu_Framework_unusedInventoryStorage);
        Save_NativeReleaseContainer();
        *state = Menu_Framework_InitOnlyState_Init;
        return 1;
      }
      break;
  }
  return 0;
}
