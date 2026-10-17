// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActionSelection.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Behavior.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/Heap.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

// decomp: D_80069C18 (D_80069C18@BATTLE/BATTLE.PRG)
unsigned char Battle_Ai_kSettingsBits30Values[1284]; /* contents: module image (ModuleData.c) */

// decomp: D_80069C1C (D_80069C1C@BATTLE/BATTLE.PRG)
unsigned int Battle_Ai_kPerceptionRangesSquared[320]; /* contents: module image (ModuleData.c) */

// decomp: D_80069C3C (D_80069C3C@BATTLE/BATTLE.PRG)
Battle_Ai_WanderTiming Battle_Ai_kWanderTimingPresets[208]; /* contents: module image (ModuleData.c) */

// decomp: D_800F1790 (D_800F1790@BATTLE/BATTLE.PRG)
int (*Battle_Ai_decisionCallbacks[55])(Battle_Ai_ActorAiState*); /* data: contents loaded in Step 5 */

// decomp: D_800F17B8 (D_800F17B8@BATTLE/BATTLE.PRG)
unsigned short Battle_Ai_viewAngles[45][2]; /* contents: module image (ModuleData.c) */

// decomp: D_800F17C8 (D_800F17C8@BATTLE/BATTLE.PRG)
unsigned short Battle_Ai_settingsBits8Values[41][2]; /* contents: module image (ModuleData.c) */

// decomp: D_800F17D8 (D_800F17D8@BATTLE/BATTLE.PRG)
unsigned char Battle_Ai_facingDelayScales[148]; /* contents: module image (ModuleData.c) */

// decomp: D_800F5878 (D_800F5878@BATTLE/BATTLE.PRG)
Battle_Ai_ActorAiState* Battle_Ai_actorAiStates[];

// decomp: D_800F58BC (D_800F58BC@BATTLE/BATTLE.PRG)
Battle_Ai_RoomAiState* Battle_Ai_roomAiState;

// decomp: D_800F58C0 (D_800F58C0@BATTLE/BATTLE.PRG)
int Battle_Ai_roomHasWaypoints;

// decomp: D_800F590C (D_800F590C@BATTLE/BATTLE.PRG)
int Battle_Ai_attributeActorMask;

// decomp: D_80069C08 (D_80069C08@BATTLE/BATTLE.PRG)
const unsigned char Battle_Ai_kFlags34Bit14Values[4] = { 9, 7, 0, 0 };

// decomp: BATTLE.PRG func_800E45D4 (src/BATTLE/BATTLE.PRG/7BDD4.s:0) [asm-file: not yet ported]
int Battle_Ai_NextActorRandomMasked(int)
{
    VS_UNPORTED("Battle::Ai::NextActorRandomMasked");
    return 0;
}

// decomp: BATTLE.PRG func_800E48A8 (src/BATTLE/BATTLE.PRG/7BDD4.s:0) [asm-file: not yet ported]
void Battle_Ai_SetActorAiReactionWithDelay(Battle_Ai_PerceptionUpdateView*, int, int)
{
    VS_UNPORTED("Battle::Ai::SetActorAiReactionWithDelay");
}

// decomp: BATTLE.PRG func_800E4C64 (src/BATTLE/BATTLE.PRG/7BDD4.s:0) [asm-file: not yet ported]
void Battle_Ai_ResetActorAiReactionDelay(int)
{
    VS_UNPORTED("Battle::Ai::ResetActorAiReactionDelay");
}

// decomp: BATTLE.PRG func_800E4F14 (src/BATTLE/BATTLE.PRG/7C4F4.s:0) [asm-file: not yet ported]
void Battle_Ai_RefreshAiResourceRestrictions(Battle_Ai_ActorAiState*)
{
    VS_UNPORTED("Battle::Ai::RefreshAiResourceRestrictions");
}

// decomp: BATTLE.PRG func_800E511C (src/BATTLE/BATTLE.PRG/7C7E0.s:0) [asm-file: not yet ported]
void Battle_Ai_RunActorAiUpdateStages(void)
{
    VS_UNPORTED("Battle::Ai::RunActorAiUpdateStages");
}

// decomp: BATTLE.PRG func_800E5998 (src/BATTLE/BATTLE.PRG/7CD68.c:573)
void Battle_Ai_ResolveAiLinkedActors(void)
{
    Battle_Actor* actor = Battle_actors[0]->next;

    while (actor != NULL) {
        if (Battle_Ai_actorAiStates[actor->id]->linkedActor == 0x10) {
            Battle_Actor* other = Battle_actors[0]->next;

            while (other != NULL) {
                if (other->aiGroup == actor->aiLinkedGroup) {
                    break;
                }
                other = other->next;
            }
            if (other != NULL) {
                Battle_Ai_actorAiStates[actor->id]->linkedActor = other->id;
            }
        }
        actor = actor->next;
    }
}

// decomp: BATTLE.PRG func_800E5A74 (src/BATTLE/BATTLE.PRG/7CD68.c:595)
void Battle_Ai_ApplySavedAiSettingsFlags(int* arg0, Battle_Ai_SavedSettingsPair* arg1)
{
    *arg0 = ((arg1->opaque4 & 0xFFE) << 20) | (*arg0 & 0x1FFFFF);
}

// decomp: BATTLE.PRG func_800E5A9C (src/BATTLE/BATTLE.PRG/7CD68.c:689)
void Battle_Ai_ApplyActorAiSettings(Battle_Ai_SettingsView* state, Battle_Ai_SettingsWord* settings)
{
    Battle_Ai_SettingsActorView* actor = state->actor;
    unsigned int rangeSquared, step;
    int i, temp;
    rangeSquared = Battle_Ai_kPerceptionRangesSquared[(settings->halves[1] & 28) >> 2];
    state->farDistanceSquared = rangeSquared;
    if (rangeSquared > 0x23FFFF) {
        rangeSquared = 0x240000;
    }
    state->nearDistanceSquared = rangeSquared;
    state->angle = Battle_Ai_viewAngles[settings->halves[1] & 3][0];
    state->viewCosine = Core_Gte_Cos((state->angle / 2) * 4096 / 360);
    state->settingsBit15 = (settings->word >> 15) & 1;
    state->settingsBit14 = (settings->word >> 14) & 1;
    state->settingsBit13 = (settings->word >> 13) & 1;
    state->opaque2D = 0;
    if (Battle_Ai_roomHasWaypoints) {
        for (i = 0; i < 8; i++) {
            if (Battle_Ai_roomWaypoints[i] >> 15) {
                break;
            }
        }
        if (i != 8) {
            state->opaque2D = (settings->word >> 12) & 1;
        }
    }
    state->opaqueB2 = ((settings->word >> 10) & 3) + 1;
    state->opaqueB0 = Battle_Ai_settingsBits8Values[(settings->word >> 8) & 3][0];
    step = 128;
    if (state->collisionRadius > 128) {
        step = 32;
    }
    state->facingDelay = (step * Battle_Ai_facingDelayScales[(settings->word >> 2) & 3]) >> 4;
    state->settingsConstant23 = 1;
    state->settingsByte3Bit0 = settings->bytes[3] & 1;
    state->settingsBit23 = (settings->word >> 23) & 1;
    state->settingsBits21 = (settings->word >> 21) & 3;
    state->settingsBits28 = (settings->word >> 28) & 3;
    state->settingsBits30 = settings->word >> 30;
    state->settingsBit26 = (settings->word >> 26) & 1;
    state->movePeriod = Battle_Ai_kWanderTimingPresets[(settings->word >> 4) & 7].movePeriod;
    state->restPeriod = Battle_Ai_kWanderTimingPresets[(settings->word >> 4) & 7].restPeriod;
    state->wanderRate = Battle_Ai_kWanderTimingPresets[(settings->word >> 4) & 7].rate;
    state->settingsBit27 = (settings->word >> 27) & 1;
    state->settingsBit25 = (settings->word >> 25) & 1;
    if (!(state->movementFlags & 0xC0)) {
        state->settingsBits30Value = Battle_Ai_kSettingsBits30Values[state->settingsBits30];
    }
    state->settingsByte0Bit7 = settings->bytes[0] >> 7;
    temp = actor->opaque34.bits.mode;
    state->allowFallbackAction = temp >> 1;
    temp &= 1;
    state->settingsModeLow = temp;
    state->opaque102 = Battle_Ai_kFlags34Bit14Values[(actor->opaque34.word >> 14) & 1];
    state->opaque101 = (actor->opaque30.word >> 14) & 3;
    state->opaqueC8 = actor->opaque30.halves[1] & 63;
    state->opaqueCC = (actor->opaque30.word >> 22) & 31;
    state->opaqueCE = actor->opaque30.word >> 27;
    if ((unsigned int)(actor->kind - 0xAC) < 2) {
        state->decisionCallback = Battle_Ai_UpdateOrbitingBossBehavior;
        state->settingsConstant19 = 1;
        Battle_Ai_AllocateOrbitingBossState(state);
    } else {
        state->decisionCallback = Battle_Ai_decisionCallbacks[(actor->opaque30.word >> 4) & 31];
    }
    state->linkedActor = 16;
    Battle_Ai_ResolveAiLinkedActors();
    Battle_Ai_BuildAiActionMetadata((void*)state);
}

// decomp: BATTLE.PRG func_800E6178 (src/BATTLE/BATTLE.PRG/7CD68.c:934)
int Battle_Ai_CreateActorAiState(Battle_Ai_AiActorView* actor, int options)
{
    Battle_Ai_AiCreationView* state;
    Battle_Ai_CreationModelView* model;
    Battle_Ai_CreationStatsView* stats;
    int id = actor->actorId;
    int i, kind, temp, packed;
    unsigned int radius, range;
    unsigned short result[4];
    if (actor->aiControlFlags & 0x15) {
        Battle_Ai_attributeActorMask |= 1 << id;
    }
    if (!actor->listExemptFlag) {
        kind = actor->model->kind & 7;
        if (kind == 2 || kind == 4) {
            for (i = 0; i < 4; i++) {
                if (!((int*)Battle_Ai_roomAiState)[10 + i]) {
                    ((int*)Battle_Ai_roomAiState)[10 + i] = (int)actor;
                    break;
                }
            }
        }
    }
    if (!(actor->aiControlFlags & 5)) {
        return 0;
    }
    state = Core_Memory_AllocateFromLowEnd(0x484);
    Battle_Ai_actorAiStates[id] = (void*)state;
    Core_Memory_ClearAlignedWords(state, 0x484);
    state->actorId = id;
    state->actor = actor;
    model = actor->model;
    state->model = model;
    state->stats = actor->stats;
    ((void (*)(unsigned char, int, void*, int))Actors_ActorTaggedBonePosition)(state->actorId, 250, result, 0);
    state->pointHeight = result[1] - model->position[1];
    radius = model->radius;
    if (!id) {
        state->radius = 40;
    } else if (radius < 63) {
        state->radius = radius;
    } else {
        state->radius = 62;
    }
    if (radius > 128) {
        state->stepHeight = 32;
        state->collisionRadius = radius;
    } else {
        radius = state->radius;
        state->stepHeight = 128;
        state->collisionRadius = radius;
    }
    state->positionSource = model->position;
    state->tileSource = &model->tile.word;
    state->opaque89 = 0;
    state->opaque8A = 0;
    state->opaque103 = 1;
    state->hasTaggedBone3 = Actors_FindActorBoneTag(id, 2, 3) >= 0;
    stats = actor->stats;
    state->opaque224 = stats->opaque30;
    state->opaque225 = stats->opaque32;
    state->opaque226 = stats->opaque31;
    state->opaque227 = stats->opaque33;
    if (state->opaque224 & 8) {
        state->moveDistance = state->opaque226 << 7;
    } else if (state->opaque225 & 8) {
        state->moveDistance = state->opaque227 << 7;
    }
    Battle_Ai_InitializeActorLocomotionMode(state);
    range = state->opaque226;
    if (range < state->opaque227) {
        range = state->opaque227;
    }
    state->maxRange = range;
    packed = actor->aiSettings;
    if (actor->savedAiSettingsFlags & 1) {
        Battle_Ai_ApplySavedAiSettingsFlags(&packed, (void*)&actor->savedAiSettings);
    }
    Battle_Ai_ApplyActorAiSettings((void*)state, (void*)&packed);
    Battle_Ai_InitializeActorAiRelations(state);
    temp = state->moveDistance - 64;
    if (state->farDistanceSquared < (unsigned int)(temp * temp)) {
        state->moveDistance = Core_Math_SquareRootInteger(state->farDistanceSquared) - 64;
    }
    state->groundClearance = -256;
    state->flightHeight = -128;
    state->bodyHeight = model->opaque63E;
    state->tile = *state->tileSource;
    state->opaque110 = *state->tileSource;
    state->opaque194 = -1;
    state->opaque9C = -1;
    state->moveY = state->positionSource[1] << 12;
    if (id) {
        if ((state->movementFlags & 0x30) && (state->movementFlags & 0xC6)
            && (Battle_Ai_tileFlagMap[model->tile.tile.tileZ][model->tile.tile.tileX] & 0x10)
            && !(model->opaque8 & 0x200000)) {
            state->nextTile = model->tile.word;
            Battle_Ai_IssueActorCommand((void*)state, 12);
        } else if ((short)options < 0 || !(((int)(short)options << 26) < 0)) {
            if (actor->startCommandFlag) {
                Battle_Ai_IssueActorCommand((void*)state, 1);
                state->behaviorMode = 1;
            }
        }
    }
    actor->stateFlags.bytes.byte1 = 0;
    actor->stateFlags.bits.bits26 = 0;
    actor->stateFlags.bits.bits24 = 0;
    state->opaque124 = actor->stateFlags.bytes.byte1;
    state->previousTile = -1;
    state->options = (short)options;
    Battle_Ai_ApplySavedActorAiRelations();
    if (state->actorId) {
        while (Battle_Ai_RebuildActorActionPlans(0))
            ;
    }
    return (short)options >= 0 && (((int)(short)options << 26) < 0);
}

// decomp: BATTLE.PRG func_800E65DC (src/BATTLE/BATTLE.PRG/7DDDC.s:0) [asm-file: not yet ported]
int Battle_Ai_CacheActorAiWorkingState()
{
    VS_UNPORTED("Battle::Ai::CacheActorAiWorkingState");
    return 0;
}

// decomp: BATTLE.PRG func_800E6694 (src/BATTLE/BATTLE.PRG/7DE94.s:0) [asm-file: not yet ported]
short Battle_Ai_PackActorAiPersistenceFlags(int)
{
    VS_UNPORTED("Battle::Ai::PackActorAiPersistenceFlags");
    return 0;
}

// decomp: BATTLE.PRG func_800E6B24 (src/BATTLE/BATTLE.PRG/7E05C.s:0) [asm-file: not yet ported]
int Battle_Ai_ClearActorAiNavigationReset()
{
    VS_UNPORTED("Battle::Ai::ClearActorAiNavigationReset");
    return 0;
}

// decomp: BATTLE.PRG func_800E6EAC (src/BATTLE/BATTLE.PRG/7E05C.s:0) [asm-file: not yet ported]
void Battle_Ai_InitializeActorAiExtensionHook(void)
{
    VS_UNPORTED("Battle::Ai::InitializeActorAiExtensionHook");
}

// decomp: BATTLE.PRG func_800E6F1C (src/BATTLE/BATTLE.PRG/7E05C.s:0) [asm-file: not yet ported]
void Battle_Ai_FreeRoomAiState(void)
{
    VS_UNPORTED("Battle::Ai::FreeRoomAiState");
}

// decomp: BATTLE.PRG func_800E72D0 (src/BATTLE/BATTLE.PRG/7E05C.s:0) [asm-file: not yet ported]
void Battle_Ai_ApplySavedActorAiRelations(void)
{
    VS_UNPORTED("Battle::Ai::ApplySavedActorAiRelations");
}

// decomp: BATTLE.PRG func_800E7370 (src/BATTLE/BATTLE.PRG/7E05C.s:0) [asm-file: not yet ported]
void Battle_Ai_InitializeActorAiRelations(Battle_Ai_AiCreationView*)
{
    VS_UNPORTED("Battle::Ai::InitializeActorAiRelations");
}

// decomp: BATTLE.PRG func_800E7454 (src/BATTLE/BATTLE.PRG/7E05C.s:0) [asm-file: not yet ported]
void Battle_Ai_FreeActorAiState(Battle_Actor*)
{
    VS_UNPORTED("Battle::Ai::FreeActorAiState");
}

// decomp: BATTLE.PRG func_800E7608 (src/BATTLE/BATTLE.PRG/7E05C.s:0) [asm-file: not yet ported]
void Battle_Ai_ClearActorAiLinksAndPlan(int)
{
    VS_UNPORTED("Battle::Ai::ClearActorAiLinksAndPlan");
}

