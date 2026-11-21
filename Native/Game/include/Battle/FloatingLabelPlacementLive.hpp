#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

// The live bindings of the floating label updater, value and status leaves and
// the equipment stat-change feedback (FloatingLabelPlacement.hpp). Each entry
// keeps the original's signature. A refused record, table entry or owner is
// logged (throttled) and the entry stops without further changes.
namespace VS::Battle {
struct FloatingLabel;
// decomp: F00719 func_800941FC (src/BATTLE/BATTLE.PRG/2842C.c:1757).
void LiveUpdateAndDrawFloatingLabels();
// decomp: F00720 func_80094844 (src/BATTLE/BATTLE.PRG/2842C.c:1971).
void LivePlaceActorValueLabel(std::int16_t* position,
                              VS::Battle::FloatingLabel* label);
// decomp: F00722 func_80094B0C (src/BATTLE/BATTLE.PRG/2842C.c:2073).
void LiveDrawActorValueLabel(int x, int y, VS::Battle::FloatingLabel* label);
// decomp: F00725 func_80095258 (src/BATTLE/BATTLE.PRG/2842C.c:2210).
void LiveDrawActorStatusLabel(int x, int y, VS::Battle::FloatingLabel* label);
// decomp: F00738 func_80096768 (src/BATTLE/BATTLE.PRG/2842C.c:2694).
int LiveQueueEquipmentStatChange(int equipment_kind, int stat_label_kind,
                                 int encoded_amount);
// decomp: F00739 func_8009695C (src/BATTLE/BATTLE.PRG/2842C.c:2765).
void LiveDrawEquipmentStatChange(int x, int y,
                                 VS::Battle::FloatingLabel* label);
// decomp: F00742 func_800971D4 (src/BATTLE/BATTLE.PRG/2842C.c:2869).
void LiveCommitEquipmentStatChanges();
}  // namespace VS::Battle
