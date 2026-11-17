// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Menu/BattleAbilitiesTerminalLive.hpp"
#include "Menu/BattleAbilitiesUnlockLive.hpp"
#include "Menu/Container/ContainerListTransferLive.hpp"
#include "Menu/Container/ContainerMenuDialogsLive.hpp"
#include "Menu/Inventory/EquipmentRowsLive.hpp"
#include "Menu/Inventory/EquipmentMenusLive.hpp"
#include "Menu/Inventory/InventoryTerminalLive.hpp"
#include "Menu/Loot/LootPoliciesLive.hpp"
#include "Menu/Loot/LootTerminalLive.hpp"
#include "Menu/ManualScreenLive.hpp"
#include "Menu/Results/CubePuzzleLifecycleLive.hpp"
#include "Menu/Score/GazetteRowsLive.hpp"
#include "Menu/Status/ActorSwitchingLive.hpp"
#include "Menu/Status/LimbControllersLive.hpp"
#include "Menu/Status/TerminalControllersLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Boot/OverlayReset.h"
#include "Menu/BattleAbilities.h"
#include "Menu/Container/Storage.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/Loot/LootScreen.h"
#include "Menu/Map.h"
#include "Menu/Results/Results.h"
#include "Menu/Results/TimeAttack.h"
#include "Menu/Score/Encyclopedia.h"
#include "Menu/Score/Records.h"
#include "Menu/Status/ActorStatus.h"

// Call where the original loads MAINMENU.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMainMenuStatics(void)
{
    Menu_Inventory_ItemList_ResetMainMenuStatics();
    VS::Menu::Inventory::ResetMiscItemShortcutModuleData();
}

// Call where the original loads MENU2.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenu2Statics(void)
{
    Menu_BattleAbilities_ResetMenu2Statics();
    VS::Menu::BattleAbilities::ResetLiveAbilityFoundationState();
    VS::Menu::BattleAbilities::ResetLiveAbilityTerminalModuleState();
}

// Call where the original loads MENU3.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenu3Statics(void)
{
    Menu_Inventory_ResetMenu3Statics();
    VS::Menu::Inventory::ResetEquipmentRowsLive();
    VS::Menu::Inventory::ResetEquipmentMenusLive();
    VS::Menu::Inventory::ResetInventoryTerminalModuleData();
}

// Call where the original loads MENU4.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenu4Statics(void)
{
    Menu_Status_ActorStatus_ResetMenu4Statics();
    VS::Menu::Status::ResetLiveActorSwitchPhase();
    VS::Menu::Status::ResetLiveLimbControllersModuleData();
    VS::Menu::Status::ResetLiveTerminalControllersModuleData();
}

// Call where the original loads MENU5.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenu5Statics(void)
{
    Menu_Map_ResetMenu5Statics();
}

// Call where the original loads MENU9.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenu9Statics(void)
{
    Menu_Score_Encyclopedia_ResetMenu9Statics();
    Menu_Score_Records_ResetMenu9Statics();
    VS::Menu::Score::ResetGazetteRowsLive();
}

// Call where the original loads MENUB.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenubStatics(void)
{
    Menu_Inventory_ResetMenubStatics();
    Menu_Loot_LootScreen_ResetMenubStatics();
    VS::Menu::Loot::Live::ResetPolicyStatics();
    VS::Menu::Loot::ResetLootTerminalModuleData();
    VS::Menu::Inventory::ResetLootInventoryTerminalModuleData();
}

// Call where the original loads MENUD.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenudStatics(void)
{
    Menu_Container_Storage_ResetMenudStatics();
    VS::Menu::container::ResetLiveContainerDialogStatics();
    VS::Menu::container::ResetLiveContainerTransferStatics();
}

// Call where the original loads MENUE.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenueStatics(void)
{
    VS::Menu::manual::ResetLiveManualScreenStatics();
}

// Call where the original loads MENUF.PRG: restores the statics its reload re-read from the disc.
void Boot_ResetMenufStatics(void)
{
    Menu_Results_ResetMenufStatics();
    Menu_Results_TimeAttack_ResetMenufStatics();
    VS::Menu::Results::ResetLiveCubePuzzleLifecycleStatics();
}

}  // extern "C"
