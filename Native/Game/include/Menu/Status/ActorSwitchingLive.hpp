#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Legacy/Psx.h"
#include "Menu/Status/ActorSwitching.hpp"

// Live binding for the status screen's actor switching and vitals
// (ActorSwitching.hpp) over the copied MENU4.PRG globals
// (Menu/Status/ActorStatus.hpp), Battle_actors and their stats records, the
// HUD's slide offset and label tables, the actor preview, the menu rows and
// the UI packets. The entries keep the original call shapes (MENU4.PRG
// 120.c:528-843 and 1114-1142). A slot outside Battle_actors, a missing actor
// or stats record, and a search that would never stop are refused with a
// diagnostic instead of being read through.
// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status {
// _initEquipmentScreen (120.c:528).
void LiveBeginVitalsTransition(int actor);
// _getNextValidActor (120.c:548): `current_actor` unchanged when refused.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int LiveFindNextInspectableActor(int current_actor, int target_actor);
// _switchCurrentActor (120.c:579).
int LiveAnimateActorSwitch(int init);
// _renderUnknownValue (120.c:692).
void LiveDrawUnanalyzedValue(int xy, int with_max, u_long* before);
// _renderStatBar (120.c:721).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveDrawVitalBar(int color_index, int current, int max, int xy);
// _renderBasicStats (120.c:741).
void LiveDrawVitals();
// _getLimbCount (120.c:1114): 0 when refused.
int LiveLimbCount(int actor);
// _getEquipmentListRowCount (120.c:1132): 0 when refused.
int LiveEquipmentRowCount();
// Returns _switchCurrentActor's state to menuLoaded, as a MENU4.PRG reload
// does.
void ResetLiveActorSwitchPhase();
}  // namespace VS::Menu::Status
