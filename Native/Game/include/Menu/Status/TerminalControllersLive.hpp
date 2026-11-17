#pragma once

#include <cstdint>

#include "Menu/Status/TerminalControllers.hpp"

// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status {
int LiveUpdateEquipmentDetails(int row);
int LiveUpdateEquipmentList(int element);
void LiveDrawModelSlideFade();
void LiveDrawBackground();
void LiveDrawFrame();
// The pointer must be one of Framework's live submenu-state bytes.
int LiveUpdateStatusTerminal(std::uint8_t* state);

// MENU4 file-backed zeros: cursor 801080C7; controller bytes 80108180..85;
// wait/enabled/cursor 8010812C..37; projection 80108188; view/camera
// 80108198..81EB; prompt 801081EC and selected-stat row 801081EE (#422).
// Invoke on every MENU4 reload and on cold reset. Never restore the saved
// camera here, free borrowed storage, or reset the limb packet's state.
void ResetLiveTerminalControllersModuleData();
[[nodiscard]] const EquipmentDetailState& LiveEquipmentDetailControllerState();
[[nodiscard]] const EquipmentListState& LiveEquipmentListControllerState();
[[nodiscard]] const StatusTerminalState& LiveStatusTerminalControllerState();
[[nodiscard]] std::uint64_t RefusedLiveTerminalControllerCount();
}  // namespace VS::Menu::Status
