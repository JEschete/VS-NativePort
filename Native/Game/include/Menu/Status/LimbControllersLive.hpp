#pragma once

#include <cstdint>

#include "Menu/Status/LimbControllers.hpp"

struct Battle_ActorStats;

// Checked bindings over the existing actor roster, MENU4 text and menu
// workspace. No actor, string bank or render storage is allocated or freed.
// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status {
void LiveDrawLimbConditionIndicator(int condition, int position, int dimming);
void LiveDrawLimbLeaderLine(int limb, int position, int unused_fade,
                            int opacity);
void LiveDrawStatusEffectIcons(Battle_ActorStats* actor, int animation_step);
int LiveNavigateStatusEffects(Battle_ActorStats* actor, int selection_active);
int LiveUpdateLimbPanel(int request);
void LiveShowSelectedLimbCondition();
void LivePopulateSelectedLimbDefense();

// MENU4 bytes at 801080C4..C6, 80108138..815F, 80108168..817F and
// 801081ED are physically zero in its 80102800..80108208 image (#420).
// Invoke on every MENU4 overlay load AND cold reset. The BATTLE-owned
// selection/dimming byte and MAINMENU workspace/text are not reset here.
void ResetLiveLimbControllersModuleData();
[[nodiscard]] const LimbPanelState& LiveLimbPanelControllerState();
[[nodiscard]] const StatusEffectNavigationState&
LiveStatusEffectNavigationControllerState();
[[nodiscard]] std::uint64_t RefusedLiveLimbControllerCount();
}  // namespace VS::Menu::Status
