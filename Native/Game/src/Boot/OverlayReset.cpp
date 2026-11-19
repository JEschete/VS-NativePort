// Hand-maintained.
// Restores the statics each menu overlay's reload re-read from the disc.
#include "Boot/OverlayReset.hpp"

#include <cstdlib>

#include "Menu/BattleAbilities.h"
#include "Menu/BattleAbilitiesTerminalLive.hpp"
#include "Menu/BattleAbilitiesUnlockLive.hpp"
#include "Menu/Container/ContainerListTransferLive.hpp"
#include "Menu/Container/ContainerMenuDialogsLive.hpp"
#include "Menu/Container/Storage.hpp"
#include "Menu/Inventory/EquipmentMenusLive.hpp"
#include "Menu/Inventory/EquipmentRowsLive.hpp"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/InventoryTerminalLive.hpp"
#include "Menu/Inventory/ItemList.hpp"
#include "Menu/Loot/LootPoliciesLive.hpp"
#include "Menu/Loot/LootScreen.hpp"
#include "Menu/Loot/LootTerminalLive.hpp"
#include "Menu/ManualScreenLive.hpp"
#include "Menu/Map.h"
#include "Menu/MapTerminalLive.hpp"
#include "Menu/Results/CubePuzzleLifecycleLive.hpp"
#include "Menu/Results/GameClearLifecycleLive.hpp"
#include "Menu/Results/TimeAttackTerminalLive.hpp"
#include "Menu/Score/GazetteRowsLive.hpp"
#include "Menu/Score/GazetteStatisticsLive.hpp"
#include "Menu/Score/GazetteTerminalLive.hpp"
#include "Menu/Score/GazetteTimesLive.hpp"
#include "Menu/Status/ActorStatus.h"
#include "Menu/Status/ActorSwitchingLive.hpp"
#include "Menu/Status/LimbControllersLive.hpp"
#include "Menu/Status/TerminalControllersLive.hpp"

namespace VS::Boot {
// MAINMENU.PRG.
void ResetMainMenuStatics() {
  VS::Menu::Inventory::ResetItemListStatics();
  VS::Menu::Inventory::ResetMiscItemShortcutModuleData();
}

// MENU2.PRG.
void ResetMenu2Statics() {
  Menu_BattleAbilities_ResetMenu2Statics();
  VS::Menu::BattleAbilities::ResetLiveAbilityFoundationState();
  VS::Menu::BattleAbilities::ResetLiveAbilityTerminalModuleState();
}

// MENU3.PRG.
void ResetMenu3Statics() {
  Menu_Inventory_ResetMenu3Statics();
  VS::Menu::Inventory::ResetEquipmentRowsLive();
  VS::Menu::Inventory::ResetEquipmentMenusLive();
  VS::Menu::Inventory::ResetInventoryTerminalModuleData();
}

// MENU4.PRG.
void ResetMenu4Statics() {
  Menu_Status_ActorStatus_ResetMenu4Statics();
  VS::Menu::Status::ResetLiveActorSwitchPhase();
  VS::Menu::Status::ResetLiveLimbControllersModuleData();
  VS::Menu::Status::ResetLiveTerminalControllersModuleData();
}

// MENU5.PRG.
void ResetMenu5Statics() {
  if (!VS::Menu::map::ResetMapTerminalModuleData()) {
    std::abort();
  }
  Menu_Map_ResetMenu5Statics();
}

// MENU9.PRG.
void ResetMenu9Statics() {
  if (!VS::Menu::Score::ResetLiveGazetteTerminalModuleState()) {
    std::abort();
  }
  VS::Menu::Score::ResetGazetteRowsLive();
  VS::Menu::Score::ResetLiveGazetteStatisticsStatics();
  VS::Menu::Score::ResetLiveTimeTrialStatics();
}

// MENUB.PRG.
void ResetMenubStatics() {
  Menu_Inventory_ResetMenubStatics();
  VS::Menu::Loot::ResetMenubStatics();
  VS::Menu::Loot::Live::ResetPolicyStatics();
  VS::Menu::Loot::ResetLootTerminalModuleData();
  VS::Menu::Inventory::ResetLootInventoryTerminalModuleData();
}

// MENUD.PRG.
void ResetMenudStatics() {
  VS::Menu::container::ResetMenudStatics();
  VS::Menu::container::ResetLiveContainerDialogStatics();
  VS::Menu::container::ResetLiveContainerTransferStatics();
}

// MENUE.PRG.
void ResetMenueStatics() { VS::Menu::manual::ResetLiveManualScreenStatics(); }

// MENUF.PRG.
void ResetMenufStatics() {
  VS::Menu::Results::ResetLiveCubePuzzleLifecycleStatics();
  VS::Menu::Results::ResetLiveGameClearModuleData();
  VS::Menu::Results::ResetLiveTimeAttackModuleData();
}
}  // namespace VS::Boot
