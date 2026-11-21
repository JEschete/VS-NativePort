#pragma once
// Hand-maintained (tools/port/owned.csv).

// Bindings from the MENU0.PRG cost, teleport and header cores (MagicCosts.hpp)
// to the game state, keeping the original call shapes for the legacy callers.
// NOLINTNEXTLINE(readability-identifier-naming): established subsystem API.
namespace VS::Menu::Magic {
// decomp: MENU0.PRG teleportMenu (src/MENU/MENU0.PRG/15AC.c:23; was
// _teleportMenu). Returns the packed destination word vs_menu0_exec expects.
int UpdateTeleportMenuLive(int init);
// decomp: MENU0.PRG setMenuHeader (src/MENU/MENU0.PRG/15AC.c:148; was
// _setMenuHeader).
void CreateHeaderLive();
// decomp: MENU0.PRG setMPCost (src/MENU/MENU0.PRG/84.c:23; was _setMPCost).
void SetSpellMpCostLive(int id);
// decomp: MENU0.PRG setMPCostDirect (src/MENU/MENU0.PRG/84.c:69; was
// _setMPCostDirect).
void SetTeleportMpCostLive(int cost_decimal, int disabled);
// decomp: MENU0.PRG getTeleportCost (src/MENU/MENU0.PRG/84.c:128; was
// _getTeleportCost). Returns 0 after a logged refusal.
int TeleportMpCostLive(int target_save_point);
// Clears _teleportMenu's statics.
void ResetTeleportMenuLiveState();
}  // namespace VS::Menu::Magic
