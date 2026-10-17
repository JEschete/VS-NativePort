// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Boot/OverlayReset.h"
#include "Items/Inventory.h"
#include "Menu/BattleAbilities.h"
#include "Menu/BreakArts.h"
#include "Menu/Container/ItemView.h"
#include "Menu/Container/Storage.h"
#include "Menu/Files.h"
#include "Menu/Framework/EquipmentDetails.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/ItemRows.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/ItemDetails.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/Loot/LootScreen.h"
#include "Menu/Magic.h"
#include "Menu/Manual.h"
#include "Menu/Map.h"
#include "Menu/Options.h"
#include "Menu/Rename.h"
#include "Menu/Results/CubePuzzle.h"
#include "Menu/Results/Results.h"
#include "Menu/Results/TimeAttack.h"
#include "Menu/Score/Encyclopedia.h"
#include "Menu/Score/Records.h"
#include "Menu/Score/ScoreScreen.h"
#include "Menu/Setup/Assembly.h"
#include "Menu/Setup/Combination.h"
#include "Menu/Setup/SetupScreen.h"
#include "Menu/Status/ActorStatus.h"
#include "Menu/Status/EquipmentDetails.h"
#include "Movie/Intro/Playback.h"
#include "Save/MemoryCard/GameFiles.h"

// Call where the original loads MAINMENU.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMainMenuStatics(void)
{
    Items_Inventory_ResetMainMenuStatics();
    Menu_Framework_EquipmentDetails_ResetMainMenuStatics();
    Menu_Framework_ResetMainMenuStatics();
    Menu_Framework_ItemRows_ResetMainMenuStatics();
    Menu_Inventory_ResetMainMenuStatics();
    Menu_Inventory_ItemList_ResetMainMenuStatics();
}

// Call where the original loads MENU0.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenu0Statics(void)
{
    Menu_Magic_ResetMenu0Statics();
}

// Call where the original loads MENU1.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenu1Statics(void)
{
    Menu_BreakArts_ResetMenu1Statics();
}

// Call where the original loads MENU2.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenu2Statics(void)
{
    Menu_BattleAbilities_ResetMenu2Statics();
}

// Call where the original loads MENU3.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenu3Statics(void)
{
    Menu_Framework_ResetMenu3Statics();
    Menu_Inventory_ResetMenu3Statics();
    Menu_Inventory_ItemDetails_ResetMenu3Statics();
    Menu_Inventory_ItemList_ResetMenu3Statics();
}

// Call where the original loads MENU4.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenu4Statics(void)
{
    Menu_Status_ActorStatus_ResetMenu4Statics();
    Menu_Status_EquipmentDetails_ResetMenu4Statics();
}

// Call where the original loads MENU5.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenu5Statics(void)
{
    Menu_Map_ResetMenu5Statics();
}

// Call where the original loads MENU7.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenu7Statics(void)
{
    Menu_Files_ResetMenu7Statics();
}

// Call where the original loads MENU8.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenu8Statics(void)
{
    Menu_Options_ResetMenu8Statics();
    Menu_Rename_ResetMenu8Statics();
}

// Call where the original loads MENU9.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenu9Statics(void)
{
    Menu_Score_Encyclopedia_ResetMenu9Statics();
    Menu_Score_Records_ResetMenu9Statics();
    Menu_Score_ScoreScreen_ResetMenu9Statics();
}

// Call where the original loads MENUB.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenubStatics(void)
{
    Menu_Inventory_ResetMenubStatics();
    Menu_Loot_LootScreen_ResetMenubStatics();
}

// Call where the original loads MENUC.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenucStatics(void)
{
    Menu_Setup_Assembly_ResetMenucStatics();
    Menu_Setup_Combination_ResetMenucStatics();
    Menu_Setup_SetupScreen_ResetMenucStatics();
}

// Call where the original loads MENUD.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenudStatics(void)
{
    Menu_Container_ItemView_ResetMenudStatics();
    Menu_Container_Storage_ResetMenudStatics();
}

// Call where the original loads MENUE.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenueStatics(void)
{
    Menu_Manual_ResetMenueStatics();
}

// Call where the original loads MENUF.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenufStatics(void)
{
    Menu_Results_CubePuzzle_ResetMenufStatics();
    Menu_Results_ResetMenufStatics();
    Menu_Results_TimeAttack_ResetMenufStatics();
}

// Call where the original loads TITLE.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetTitleStatics(void)
{
    Movie_Intro_Playback_ResetTitleStatics();
    Save_MemoryCard_GameFiles_ResetTitleStatics();
}

