// Battle targeting's state (TargetingState.hpp).
#include "Battle/TargetingState.hpp"

namespace VS::Battle {
// The tables below hold module image contents (ModuleData.cpp).
// decomp: D_800E8184 (D_800E8184@BATTLE/BATTLE.PRG)
CategoryNeedsTargetSelectionTable categoryNeedsTargetSelection;
// decomp: D_800E8194 (D_800E8194@BATTLE/BATTLE.PRG)
CategoryIsAreaTargetTable categoryIsAreaTarget;
// decomp: D_800E81B4 (D_800E81B4@BATTLE/BATTLE.PRG)
CategoryIncludesActingActorTable categoryIncludesActingActor;
// decomp: D_800E81C4 (D_800E81C4@BATTLE/BATTLE.PRG)
CategoryTargetsBodyPartTable categoryTargetsBodyParts;
// decomp: D_800E81D4 (D_800E81D4@BATTLE/BATTLE.PRG)
CategoryTargetActorMaskTable categoryTargetActorMasks;
// decomp: vs_battle_statusStrings (vs_battle_statusStrings@BATTLE/BATTLE.PRG)
std::array<std::uint16_t, kStatusStringHalfwords> statusStrings;
// decomp: D_800EA984 (D_800EA984@BATTLE/BATTLE.PRG)
std::array<char, kTargetInfoTemplateBytes> targetInfoTemplate;
// decomp: D_800EA9B6 (D_800EA9B6@BATTLE/BATTLE.PRG)
TargetStatusLineTemplateTable targetStatusLineTemplate;
// decomp: D_800EAA54 (D_800EAA54@BATTLE/BATTLE.PRG)
std::array<char, kEmptyStatusStringBytes> emptyStatusString;
// decomp: D_800EABAA (D_800EABAA@BATTLE/BATTLE.PRG)
std::uint8_t targetSelectionStep;

// decomp: D_800F4C70 (D_800F4C70@BATTLE/BATTLE.PRG)
TargetColumnEntryTable targetColumnEntries;
// decomp: D_800F4C90 (D_800F4C90@BATTLE/BATTLE.PRG)
TargetColumnGroupBytes secondColumnGroups;
// decomp: D_800F4C98 (D_800F4C98@BATTLE/BATTLE.PRG)
TargetColumnGroupBytes firstColumnGroups;
// decomp: D_800F4CA0 (D_800F4CA0@BATTLE/BATTLE.PRG)
TargetColumnGroupBytes secondColumnGroupFirstTargets;
// decomp: D_800F4CA8 (D_800F4CA8@BATTLE/BATTLE.PRG)
TargetColumnGroupBytes firstColumnGroupFirstTargets;
// decomp: D_800F4CB0 (D_800F4CB0@BATTLE/BATTLE.PRG)
std::uint8_t secondColumnEntryCount;
// decomp: D_800F4CB1 (D_800F4CB1@BATTLE/BATTLE.PRG)
std::uint8_t firstColumnEntryCount;
// decomp: D_800F4CB2 (D_800F4CB2@BATTLE/BATTLE.PRG)
std::uint8_t secondColumnGroupCount;
// decomp: D_800F4CB3 (D_800F4CB3@BATTLE/BATTLE.PRG)
std::uint8_t firstColumnGroupCount;
// decomp: D_800F4CB4 (D_800F4CB4@BATTLE/BATTLE.PRG)
std::uint8_t targetColumnTopOffset;
// decomp: vs_battle_stringContext (vs_battle_stringContext@BATTLE/BATTLE.PRG)
VS::Menu::StringContext stringContext;
}  // namespace VS::Battle
