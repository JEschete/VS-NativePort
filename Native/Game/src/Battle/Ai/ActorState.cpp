// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Actors/SkeletonLive.hpp"
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/ActorSettingsLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/PlanningWorkspaceLive.hpp"
#include "Battle/Ai/RelationsLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

// decomp: D_80069C18 (D_80069C18@BATTLE/BATTLE.PRG)
unsigned char Battle_Ai_kSettingsBits30Values[1284]; /* contents: module image (ModuleData.cpp) */

// decomp: D_80069C1C (D_80069C1C@BATTLE/BATTLE.PRG)
unsigned int Battle_Ai_kPerceptionRangesSquared[320]; /* contents: module image (ModuleData.cpp) */

// decomp: D_80069C3C (D_80069C3C@BATTLE/BATTLE.PRG)
Battle_Ai_WanderTiming Battle_Ai_kWanderTimingPresets[208]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F1790 (D_800F1790@BATTLE/BATTLE.PRG)
int (*Battle_Ai_decisionCallbacks[9])(Battle_Ai_ActorAiState*); /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_800F17B8 (D_800F17B8@BATTLE/BATTLE.PRG)
unsigned short Battle_Ai_viewAngles[45][2]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F17C8 (D_800F17C8@BATTLE/BATTLE.PRG)
unsigned short Battle_Ai_settingsBits8Values[41][2]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F17D8 (D_800F17D8@BATTLE/BATTLE.PRG)
unsigned char Battle_Ai_facingDelayScales[148]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F5878 (D_800F5878@BATTLE/BATTLE.PRG)
Battle_Ai_ActorAiState* Battle_Ai_actorAiStates[16];

// decomp: D_800F58BC (D_800F58BC@BATTLE/BATTLE.PRG)
Battle_Ai_RoomAiState* Battle_Ai_roomAiState;

// decomp: D_800F58C0 (D_800F58C0@BATTLE/BATTLE.PRG)
void* Battle_Ai_roomHasWaypoints;

// decomp: D_800F590C (D_800F590C@BATTLE/BATTLE.PRG)
int Battle_Ai_attributeActorMask;

// decomp: D_80069C08 (D_80069C08@BATTLE/BATTLE.PRG)
const unsigned char Battle_Ai_kFlags34Bit14Values[4] = { 9, 7, 0, 0 };

// decomp: BATTLE.PRG func_800E6178 (src/BATTLE/BATTLE.PRG/7CD68.c:934)
int Battle_Ai_CreateActorAiState(Battle_Ai_AiActorView* actor, int options)
{
    Battle_Ai_AiCreationView* state;
    Battle_Ai_CreationModelView* model;
    Battle_Ai_CreationStatsView* stats;
    int id = actor->actorId;
    int i, kind, temp, packed;
    unsigned int radius, range;
    SVECTOR result;
    Core_Debugf("ai", "create actor=%p nativeId=%d viewId=%d options=0x%X nativeModel=%p stats=%p",
        (void*)actor, ((Battle_Actor*)actor)->id, id, (unsigned int)options,
        (void*)((Battle_Actor*)actor)->model, (void*)((Battle_Actor*)actor)->stats);
    if (actor->aiControlFlags & 0x15) {
        Battle_Ai_attributeActorMask |= 1 << id;
    }
    if (!actor->listExemptFlag) {
        kind = actor->model->kind & 7;
        if (kind == 2 || kind == 4) {
            for (i = 0; i < 4; i++) {
                if (!Battle_Ai_roomAiState->specialActors[i]) {
                    Battle_Ai_roomAiState->specialActors[i] = (Battle_Actor*)actor;
                    break;
                }
            }
        }
    }
    if (!(actor->aiControlFlags & 5)) {
        return 0;
    }
    state = (Battle_Ai_AiCreationView *)(VS::Core::Memory::AllocateLiveFromLowEnd(sizeof(Battle_Ai_ActorAiState)));
    Battle_Ai_actorAiStates[id] = (Battle_Ai_ActorAiState *)((void*)state);
    VS::Core::Memory::ClearLiveAlignedWords(state, sizeof(Battle_Ai_ActorAiState));
    state->actorId = id;
    state->actor = actor;
    model = actor->model;
    state->model = model;
    state->stats = actor->stats;
    Core_Debugf("ai", "created state=%p id=%u viewModel=%p viewStats=%p",
        (void*)state, (unsigned int)state->actorId, (void*)model, (void*)state->stats);
    VS::Actors::LiveActorTaggedBonePosition(state->actorId, 0xFA, &result, 0);
    state->pointHeight = result.vy - model->position[1];
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
    state->hasTaggedBone3 = VS::Actors::FindLiveActorBoneTag(id, 2, 3) >= 0;
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
    VS::Battle::Ai::InitializeLiveActorLocomotionMode(state);
    range = state->opaque226;
    if (range < state->opaque227) {
        range = state->opaque227;
    }
    state->maxRange = range;
    packed = actor->aiSettings;
    if (actor->savedAiSettingsFlags & 1) {
        VS::Battle::Ai::ApplyLiveSavedAiSettingsFlags(&packed, (Battle_Ai_SavedSettingsPair *)((void*)&actor->savedAiSettings));
    }
    VS::Battle::Ai::ApplyLiveActorAiSettings((Battle_Ai_SettingsView *)((void*)state), (Battle_Ai_SettingsWord *)((void*)&packed));
    VS::Battle::Ai::InitializeLiveActorAiRelations(state);
    temp = state->moveDistance - 64;
    if (state->farDistanceSquared < (unsigned int)(temp * temp)) {
        state->moveDistance = VS::Core::Math::SquareRootInteger(state->farDistanceSquared) - 64;
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
            VS::Battle::Ai::IssueLiveActorCommand((Battle_Ai_ActorAiState *)((void*)state), 12);
        } else if ((short)options < 0 || !(((int)(short)options << 26) < 0)) {
            if (actor->startCommandFlag) {
                VS::Battle::Ai::IssueLiveActorCommand((Battle_Ai_ActorAiState *)((void*)state), 1);
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
    VS::Battle::Ai::ApplyLiveSavedActorAiRelations();
    if (state->actorId) {
        while (VS::Battle::Ai::RebuildLiveActorActionPlans(0))
            ;
    }
    Core_Debugf("ai", "initialized actor=%d state=%p bytes=%u radius=%u height=%u",
        id, (void*)state, (unsigned int)sizeof(Battle_Ai_ActorAiState),
        (unsigned int)state->radius, (unsigned int)state->pointHeight);
    return (short)options >= 0 && (((int)(short)options << 26) < 0);
}

}  // extern "C"
