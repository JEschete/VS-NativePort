#pragma once
// Hand-maintained (tools/port/owned.csv).

// Bindings from the MENU0.PRG cost, teleport and header cores (MagicCosts.hpp)
// to the game state, keeping the original call shapes for the legacy callers.
// NOLINTNEXTLINE(readability-identifier-naming): established subsystem API.
namespace VS::Menu::Magic {
// decomp: MENU0.PRG _teleportMenu (src/MENU/MENU0.PRG/15AC.c:22). Returns the
// packed destination word vs_menu0_exec expects.
int UpdateTeleportMenuLive(int init);
// decomp: MENU0.PRG _setMenuHeader (src/MENU/MENU0.PRG/15AC.c:148).
void CreateHeaderLive();
// decomp: MENU0.PRG _setMPCost (src/MENU/MENU0.PRG/84.c:18).
void SetSpellMpCostLive(int id);
// decomp: MENU0.PRG _setMPCostDirect (src/MENU/MENU0.PRG/84.c:64).
void SetTeleportMpCostLive(int cost_decimal, int disabled);
// decomp: MENU0.PRG _getTeleportCost (src/MENU/MENU0.PRG/84.c:117). Returns
// 0 after a logged refusal.
int TeleportMpCostLive(int target_save_point);
// Clears _teleportMenu's statics.
void ResetTeleportMenuLiveState();
}  // namespace VS::Menu::Magic
