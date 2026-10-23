// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionSelection.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Targeting.h"
#include "Core/Math/ScalarMath.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"

// decomp: BATTLE.PRG func_800E4C8C (src/BATTLE/BATTLE.PRG/7BDD4.s:0) [asm-file: not yet ported]
void Battle_Ai_RememberAiActionTarget(Battle_Ai_ActorAiState*)
{
    VS_UNPORTED("Battle::Ai::RememberAiActionTarget");
}

// decomp: BATTLE.PRG func_800E4CE8 (src/BATTLE/BATTLE.PRG/7C4E8.c:407)
void Battle_Ai_ForgetAiActionTarget(Battle_Ai_ActorAiState* arg0) { arg0->previousTile = -1; }

// decomp: BATTLE.PRG func_800E5710 (src/BATTLE/BATTLE.PRG/7CD68.c:526)
void Battle_Ai_BuildAiActionMetadata(Battle_Ai_ActionMetadataView* state)
{
    int row, column, id, mask;
    Battle_Ai_MetadataActionView* action;
    Battle_Ai_ActionMetadataEntry* entry;
    if (!state->actorId) {
        state->actionCostEnabled = 1;
        return;
    }
    for (row = 0; row < 6; row++) {
        for (column = 0; column < 4; column++) {
            id = state->stats->slots[row][column].id;
            action = (void*)&Battle_actionDefinitions[id];
            entry = &state->actionSlots[row][column];
            mask = 1 << ((action->opaque0.word >> 20) & 15);
            entry->rangeSquared = Battle_CalculateActionRangeSquared((void*)state, id);
            entry->radius =
                Battle_CalculateActionAreaReach((intptr_t)state, id, Core_Math_SquareRootInteger(entry->rangeSquared));
            entry->longRange =
                entry->rangeSquared > 0x51000 && !(action->opaqueC & 0x20000000);
            if (mask & 0x4198) {
                entry->opaque0Bit1 = 1;
            }
            if (mask & 0x154) {
                entry->opaque0Bit2 = 1;
            }
            if (mask & 0xA) {
                entry->kind = 1;
            } else if (mask & 0x40A0) {
                entry->kind = 2;
            } else if (mask & 0x1954) {
                entry->kind = 3;
            }
            entry->special = ((unsigned char*)Battle_Ai_roomAiState)[10]
                          && (action->opaque0.word & 0xE0000) != 0x20000
                          && (action->opaque0.word & 0xE0000) != 0x60000
                          && (mask & 0x4000) != 0;
            if ((action->opaque0.word & 0xE0000) == 0x20000) {
                unsigned int value = action->opaque0.bytes[3];
                if (state->actionCostEnabled < value) {
                    state->actionCostEnabled = value;
                }
            }
        }
    }
}

