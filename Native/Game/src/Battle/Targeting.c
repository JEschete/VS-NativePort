// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/Appearance.h"
#include "Actors/Lifecycle.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Abilities.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/Hit.h"
#include "Battle/Hud.h"
#include "Battle/Sphere.h"
#include "Battle/StatusEffects.h"
#include "Battle/TargetVolume.h"
#include "Battle/Targeting.h"
#include "Battle/TextBoxes.h"
#include "Content/StringTables.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/WordCopy.h"
#include "Field/Camera.h"
#include "Field/Collision.h"
#include "Field/GameMode.h"
#include "Field/Gimmicks.h"
#include "Field/Interactions.h"
#include "Field/Map.h"
#include "Field/Mpd.h"
#include "Field/Platforms.h"
#include "Field/RoomRendering.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "Items/Weapons.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Sounds.h"
#include "Menu/TextBox.h"
#include "Render/Font.h"
#include "Render/Palette.h"
#include "Render/Sprites.h"

// decomp: D_800E8184 (D_800E8184@BATTLE/BATTLE.PRG)
char Battle_categoryNeedsTargetSelection[96]; /* contents: module image (ModuleData.c) */

// decomp: D_800E8194 (D_800E8194@BATTLE/BATTLE.PRG)
u_char Battle_categoryIsAreaTarget[80]; /* contents: module image (ModuleData.c) */

// decomp: D_800E81B4 (D_800E81B4@BATTLE/BATTLE.PRG)
u_char Battle_categoryIncludesActingActor[48]; /* contents: module image (ModuleData.c) */

// decomp: D_800E81C4 (D_800E81C4@BATTLE/BATTLE.PRG)
u_char Battle_categoryTargetsBodyParts[32]; /* contents: module image (ModuleData.c) */

// decomp: D_800E81D4 (D_800E81D4@BATTLE/BATTLE.PRG)
u_char Battle_categoryTargetActorMasks[16]; /* contents: module image (ModuleData.c) */

// decomp: vs_battle_statusStrings (vs_battle_statusStrings@BATTLE/BATTLE.PRG)
u_short Battle_statusStrings[1712]; /* contents: module image (ModuleData.c) */

// decomp: D_800EA984 (D_800EA984@BATTLE/BATTLE.PRG)
char Battle_targetInfoTemplate[3140]; /* contents: module image (ModuleData.c) */

// decomp: D_800EA9B6 (D_800EA9B6@BATTLE/BATTLE.PRG)
char Battle_targetStatusLineTemplate[3090]; /* contents: module image (ModuleData.c) */

// decomp: D_800EAA54 (D_800EAA54@BATTLE/BATTLE.PRG)
char Battle_emptyStatusString[2932]; /* contents: module image (ModuleData.c) */

// decomp: D_800EABAA (D_800EABAA@BATTLE/BATTLE.PRG)
u_char Battle_targetSelectionStep; /* contents: module image (ModuleData.c) */

// decomp: D_800F4C70 (D_800F4C70@BATTLE/BATTLE.PRG)
u_char Battle_targetColumnEntries[2][16];

// decomp: D_800F4C90 (D_800F4C90@BATTLE/BATTLE.PRG)
u_char Battle_secondColumnGroups[8];

// decomp: D_800F4C98 (D_800F4C98@BATTLE/BATTLE.PRG)
u_char Battle_firstColumnGroups[8];

// decomp: D_800F4CA0 (D_800F4CA0@BATTLE/BATTLE.PRG)
u_char Battle_secondColumnGroupFirstTargets[8];

// decomp: D_800F4CA8 (D_800F4CA8@BATTLE/BATTLE.PRG)
u_char Battle_firstColumnGroupFirstTargets[8];

// decomp: D_800F4CB0 (D_800F4CB0@BATTLE/BATTLE.PRG)
u_char Battle_secondColumnEntryCount;

// decomp: D_800F4CB1 (D_800F4CB1@BATTLE/BATTLE.PRG)
u_char Battle_firstColumnEntryCount;

// decomp: D_800F4CB2 (D_800F4CB2@BATTLE/BATTLE.PRG)
u_char Battle_secondColumnGroupCount;

// decomp: D_800F4CB3 (D_800F4CB3@BATTLE/BATTLE.PRG)
u_char Battle_firstColumnGroupCount;

// decomp: D_800F4CB4 (D_800F4CB4@BATTLE/BATTLE.PRG)
u_char Battle_targetColumnTopOffset;

// decomp: vs_battle_stringContext (vs_battle_stringContext@BATTLE/BATTLE.PRG)
Menu_StringContext Battle_stringContext;

// decomp: BATTLE.PRG func_8006F630 (src/BATTLE/BATTLE.PRG/146C.c:3235)
void Battle_PopulateActionTargetPreview(Battle_TargetPreview* arg0, Battle_ActionRequest* arg1, Battle_HitResult* arg2)
{
    int effect = Battle_actionDefinitions[arg1->actionId].hits[0].effectId;
    u_short var_v0;

    switch (effect) {
    case 0x3A:
    case 0x3E:
        var_v0 = arg2->target.hp;
        arg0->effect = effect;
        arg0->previewAmount = var_v0;
        break;
    case 0x3B:
        var_v0 = arg2->target.mp;
        arg0->effect = effect;
        arg0->previewAmount = var_v0;
        break;
    case 0x3F:
        var_v0 = arg2->target.mp;
        arg0->effect = effect;
        arg0->previewAmount = var_v0;
        break;
    default:
        arg0->effect = effect;
        break;
    }

    arg0->hitChance = arg2->target.hitThreshold;
    arg0->removedStatuses = arg2->removeStatuses & 0x1FFFFFE0;
    arg0->actionKind = Battle_actionDefinitions[arg1->actionId].category;
    arg0->opaque15 = arg1->damageType;
    arg0->opaque16 = arg1->effectVariant;
    arg0->enemyClass = Battle_actors[arg2->target.targetActor & 15]->stats->enemyClass;
}

// decomp: BATTLE.PRG func_800704D8 (src/BATTLE/BATTLE.PRG/146C.c:3578)
void Battle_ReturnToBattleActionMenu(void)
{
    Field_controlMode = 5;
    Battle_actionState->selection.actionId = 0;
    Battle_actionFlowMode = 1;
    Battle_SetHudBattleMode((Battle_characterState->weaponDrawn & 1) | 2);
    Menu_Framework_ResumeAfterActionSelection();
    Field_PauseFieldForMenu();
}

// decomp: BATTLE.PRG func_8007053C (src/BATTLE/BATTLE.PRG/146C.c:3588)
void Battle_FilterAreaActionTargets(void)
{
    Battle_ActionRequest result;
    int i;
    int kept;
    int action;

    Battle_actionState->savedAreaCenter = Battle_actionState->areaCenter;
    Battle_ClearPrimarySphereVolume();

    if (Battle_actionState->savedAreaCenter.pad) {
        ((void (*)())Battle_ClearSphereTargets)();
        Battle_ClearPrimarySphereVolume();
        Battle_ClearSecondarySphereVolume();
        Battle_FreeSphere();

        if (!Battle_actionDefinitions[Battle_actionState->selection.actionId].returnsToMenuOnCancel) {
            Menu_Framework_ReleaseMenuRows();
            Battle_BeginPostActionRecovery(0);
            return;
        }

        Battle_ReturnToBattleActionMenu();
        return;
    }

    action = Battle_actionState->selection.actionId;
    Battle_actionState->areaOrigin = Battle_actionState->savedAreaCenter;
    ((void (*)(void*))Battle_SetSecondarySphereVolume)(&Battle_actionState->areaShape);
    kept = 0;

    for (i = 0; i < Battle_actionState->previewCount; ++i) {
        if (Battle_categoryTargetsBodyParts[Battle_actionDefinitions[action].category]) {
            if (Battle_IsPointInTargetVolume(
                    (void*)&Battle_actionState->areaShape, (void*)&Battle_actionState->previews[i])) {
                Core_Memory_CopyAlignedWords(&Battle_actionState->previews[kept], &Battle_actionState->previews[i],
                    sizeof(Battle_TargetPreview));
                Battle_PreviewActionAgainstLimb(action, Battle_actionState->selection.primaryTarget.target.targetActor,
                    Battle_actionState->previews[kept].actorId, Battle_actionState->previews[kept].limb,
                    &result);
                Battle_PopulateActionTargetPreview(&Battle_actionState->previews[kept], &result, &result.targets[0]);
                ++kept;
            }
        } else if (Battle_IsPointInTargetVolume(
                       (void*)&Battle_actionState->areaShape, (void*)&Battle_actionState->previews[i])) {
            Battle_actionState->previews[kept].kind = 16;
            Battle_actionState->previews[kept].limb = -1;
            Battle_actionState->previews[kept].actorId = Battle_actionState->previews[i].actorId;
            Battle_actionState->previews[kept].position = Battle_actionState->previews[i].position;
            Battle_PreviewActionAgainstLimb(action, Battle_actionState->selection.primaryTarget.target.targetActor,
                Battle_actionState->previews[kept].actorId,
                Battle_actors[Battle_actionState->previews[kept].actorId]->stats->defaultTargetLimb, &result);
            Battle_PopulateActionTargetPreview(&Battle_actionState->previews[kept], &result, &result.targets[0]);
            ++kept;
        }
    }

    Battle_actionState->previewCount = kept;
    Battle_SetSphereTargets(kept, Battle_actionState->previews, 1);
    Battle_actionFlowMode = 3;
}

// decomp: BATTLE.PRG func_8007087C (src/BATTLE/BATTLE.PRG/146C.c:3650)
void Battle_AimActionBehindSource(Battle_ActionRequest* arg0)
{
    Actors_ActorLocationQuery sp10;

    Actors_ActorPlacement(arg0->primaryTarget.target.targetActor, &sp10);
    arg0->focusPosition.vx = sp10.location.position.vx + Core_Gte_Sin(sp10.location.position.pad + ONE / 2);
    arg0->focusPosition.vz = sp10.location.position.vz + Core_Gte_Cos(sp10.location.position.pad + ONE / 2);
    arg0->focusPosition.vy = sp10.location.position.vy;
}

// decomp: BATTLE.PRG func_800708EC (src/BATTLE/BATTLE.PRG/146C.c:3660)
void Battle_SetSingleActionTarget(void)
{
    char temp_v1;

    Battle_actionState->selection.targetCount = 1;
    temp_v1 = Battle_actionState->previews[Battle_actionState->selectedPreview].actorId;
    if (temp_v1 < 16) {
        Battle_actionState->selection.targets[0].targetsPosition = 0;
        Battle_actionState->selection.targets[0].target.targetActor =
            Battle_actionState->previews[Battle_actionState->selectedPreview].actorId;
        Battle_actionState->selection.targets[0].target.targetLimb =
            Battle_actionState->previews[Battle_actionState->selectedPreview].limb;
        if (Battle_actionState->selection.targets[0].target.targetLimb == -1) {
            Battle_actionState->selection.targets[0].target.targetLimb =
                Battle_actors[Battle_actionState->selection.targets[0].target.targetActor]
                    ->stats->defaultTargetLimb;
        }
    } else {
        Battle_actionState->selection.targets[0].targetsPosition = temp_v1 & 0x10 ? 4 : 1;
        *(SVECTOR*)&Battle_actionState->selection.targets[0].target =
            Battle_actionState->previews[Battle_actionState->selectedPreview].position;
        Battle_actionState->selection.targets[0].target.mp = Battle_actionState->previews[Battle_actionState->selectedPreview].actorId;
    }
    if (Battle_actionState->selection.primaryTarget.targetsPosition == 0) {
        if (Battle_actionState->previews[Battle_actionState->selectedPreview].actorId
            == Battle_actionState->selection.primaryTarget.target.targetActor) {
            Battle_AimActionBehindSource(&Battle_actionState->selection);
            return;
        }
        Battle_actionState->selection.focusPosition = Battle_actionState->previews[Battle_actionState->selectedPreview].position;
        return;
    }
    Battle_actionState->selection.focusPosition = Battle_actionState->previews[Battle_actionState->selectedPreview].position;
}

// decomp: BATTLE.PRG func_80070B04 (src/BATTLE/BATTLE.PRG/146C.c:3695)
void Battle_SetAllActionTargets(void)
{
    int i;

    Battle_actionState->selection.targetCount = Battle_actionState->previewCount;

    for (i = 0; i < Battle_actionState->selection.targetCount; ++i) {
        if (Battle_actionState->previews[i].actorId < 0x10) {
            Battle_actionState->selection.targets[i].targetsPosition = 0;
            Battle_actionState->selection.targets[i].target.targetActor = Battle_actionState->previews[i].actorId;
            Battle_actionState->selection.targets[i].target.targetLimb = Battle_actionState->previews[i].limb;

            if (Battle_actionState->selection.targets[i].target.targetLimb == -1) {
                Battle_actionState->selection.targets[i].target.targetLimb =
                    Battle_actors[Battle_actionState->selection.targets[i].target.targetActor]
                        ->stats->defaultTargetLimb;
            }
        } else {
            Battle_actionState->selection.targets[i].targetsPosition = 1;
            *(SVECTOR*)&Battle_actionState->selection.targets[i].target = Battle_actionState->previews[i].position;
            Battle_actionState->selection.targets[i].target.mp = Battle_actionState->previews[i].actorId;
        }
    }

    if (Battle_actionState->selection.primaryTarget.target.targetActor != 0) {
        Battle_actionState->selection.focusPosition =
            *(SVECTOR*)&Battle_actors[Battle_actionState->selection.primaryTarget.target.targetActor]->commandArg1;
        return;
    }
    Battle_actionState->selection.focusPosition = Battle_actionState->savedAreaCenter;
}

// decomp: BATTLE.PRG func_80070CAC (src/BATTLE/BATTLE.PRG/146C.c:3728)
void Battle_CollectEnvironmentActionTargets(void)
{
    Field_PlatformContactPoint points[4];
    int count;
    int i;
    Field_MpdFloorTrigger* room;
    Battle_EnvironmentTargetList* list = (Battle_EnvironmentTargetList*)&Battle_actionState->selection;

    list->count = 0;

    switch (list->action) {
    case 93:
    case 241:
        room = Field_ActiveRoomTriggers(&count);

        for (i = 0; i < count; ++i) {
            if (room[i].kind != 16) {
                list->targets[list->count].kind = 3;
                list->targets[list->count].x = room[i].tileX * 128 + 64;
                list->targets[list->count].y =
                    (Field_FloorHeightAt((room[i].tileX << 7) | 64, (room[i].tileZ << 7) | 64)
                        << 17)
                    >> 17;
                list->targets[list->count].z = room[i].tileZ * 128 + 64;
                list->targets[list->count].radius = 128;
                ++list->count;
            }
        }
        break;

    case 79:
        count = Field_CollectMovingPlatformPositions(points);

        for (i = 0; i < count; ++i) {
            list->targets[list->count].kind = 5;
            list->targets[list->count].x = points[i].x;
            list->targets[list->count].y = points[i].height;
            list->targets[list->count].z = points[i].z;
            list->targets[list->count].radius = 64;
            ++list->count;
        }
        break;
    }

    Battle_AimActionBehindSource(&Battle_actionState->selection);
}

// decomp: BATTLE.PRG func_800719FC (src/BATTLE/BATTLE.PRG/146C.c:4086)
void Battle_CollectSourceAreaActionTargets(int arg0 __attribute__((unused)))
{
    SVECTOR origin;
    Battle_ActionRequest result;
    int action = Battle_actionState->selection.actionId;
    int count;
    int i;
    Battle_ActionDefinition* spec;

    Actors_ActorBonePosition(Battle_actionState->selection.primaryTarget.target.targetActor, 0, &origin, 0);
    spec = &Battle_actionDefinitions[action];
    Battle_actionState->areaShape = spec->shape;
    Battle_actionState->areaKind = 1;
    Battle_actionState->areaX = spec->rangeX;
    Battle_actionState->areaY = spec->rangeY;
    Battle_actionState->areaZ = spec->rangeZ;
    if (Battle_actionState->selection.primaryTarget.target.targetActor) {
        short dx;
        short dz;
        short dy;

        Battle_actionState->selection.focusPosition =
            *(SVECTOR*)&Battle_actors[Battle_actionState->selection.primaryTarget.target.targetActor]->commandArg1;
        dx = Battle_actionState->selection.focusPosition.vx - origin.vx;
        dz = Battle_actionState->selection.focusPosition.vz - origin.vz;
        Battle_actionState->areaFacing = Core_Gte_Atan2(dx, dz);
        dy = Battle_actionState->selection.focusPosition.vy - origin.vy;
        Battle_actionState->areaAngle = Core_Gte_Atan2(dy, Core_Math_SquareRootInteger(dx * dx + dz * dz)) / 16 + 64;
        Battle_actionState->areaOrigin =
            *(SVECTOR*)&Battle_actors[Battle_actionState->selection.primaryTarget.target.targetActor]->commandArg1;
    } else {
        Battle_actionState->areaAngle = 64;
        Battle_actionState->areaFacing = 0;
    }
    Battle_actionState->areaOrigin = origin;
    if (Battle_actionState->isPlayerAction) {
        ((void (*)(void*))Battle_SetSecondarySphereVolume)(&Battle_actionState->areaShape);
    }
    count = 0;
    for (i = 0; i < 16; ++i) {
        u_int status = Actors_ActorStateFlags(i);

        if (Battle_actors[i]
            && (i != Battle_actionState->selection.primaryTarget.target.targetActor
                || Battle_categoryIncludesActingActor[Battle_actionDefinitions[action].category])
            && (status & 0x3000001) == 1
            && (Battle_actors[i]->kind & Battle_categoryTargetActorMasks[Battle_actionDefinitions[action].category])
            && !(Battle_actors[i]->kind & 0x8000)) {
            if (Battle_categoryTargetsBodyParts[Battle_actionDefinitions[action].category]) {
                int limb;

                for (limb = 0; limb < 6; ++limb) {
                    int part = Actors_FindActorBoneTag(i, limb, 2);

                    if (part >= 0) {
                        Actors_ActorLowGroupBonePosition(i, limb, &Battle_actionState->previews[count].position, 0);
                        if (Battle_IsPointInTargetVolume((void*)&Battle_actionState->areaShape,
                                (void*)&Battle_actionState->previews[count])) {
                            Battle_actionState->previews[count].kind = Actors_ActorBoneTag(i, part, 0);
                            Battle_actionState->previews[count].limb = limb;
                            Battle_actionState->previews[count].actorId = i;
                            Battle_PreviewActionAgainstLimb(action, Battle_actionState->selection.primaryTarget.target.targetActor,
                                i, limb, &result);
                            Battle_PopulateActionTargetPreview(
                                &Battle_actionState->previews[count], &result, &result.targets[0]);
                            ++count;
                        }
                    }
                }
            } else {
                Actors_ActorTaggedBonePosition(i, 255, &Battle_actionState->previews[count].position, 0);
                if (Battle_IsPointInTargetVolume((void*)&Battle_actionState->areaShape,
                        (void*)&Battle_actionState->previews[count])) {
                    Battle_actionState->previews[count].kind = 16;
                    Battle_actionState->previews[count].limb = -1;
                    Battle_actionState->previews[count].actorId = i;
                    Battle_PreviewActionAgainstLimb(action, Battle_actionState->selection.primaryTarget.target.targetActor, i,
                        Battle_actors[i]->stats->defaultTargetLimb, &result);
                    Battle_PopulateActionTargetPreview(&Battle_actionState->previews[count], &result, &result.targets[0]);
                    ++count;
                }
            }
        }
    }
    Battle_actionState->previewCount = count;
    Battle_SetSphereTargets(count, Battle_actionState->previews, 1);
    Battle_actionState->selectedPreview = 240;
    Battle_actionFlowMode = 3;
}

// decomp: BATTLE.PRG func_80071F68 (src/BATTLE/BATTLE.PRG/146C.c:4176)
void Battle_CollectDirectActionTargets(int arg0)
{
    Actors_ActorLocationQuery state;
    Battle_ActionRequest result;
    int i;
    int action = Battle_actionState->selection.actionId;
    Battle_ActorStats* attacker;
    Battle_ActionDefinition* spec;
    int count;
    int j;

    Battle_actionState->sourceActorId = arg0;
    Battle_actionState->rangeShape = Battle_actionDefinitions[action].shape;
    spec = &Battle_actionDefinitions[action];
    Battle_actionState->rangeKind = 1;
    Actors_ActorPlacement(Battle_actionState->selection.primaryTarget.target.targetActor, &state);
    Battle_actionState->rangeFacing = state.location.position.pad;
    attacker = Battle_actors[Battle_actionState->selection.primaryTarget.target.targetActor]->stats;

    if (spec->rangeX == 0xFF) {
        Battle_actionState->rangeX = attacker->currentRange;
        Battle_actionState->rangeY = attacker->currentRangeY;
        Battle_actionState->rangeZ = attacker->currentRangeZ;
        Battle_actionState->rangeAngle = attacker->currentAttackShapeAngle * 8;
    } else {
        Battle_actionState->rangeX = spec->rangeX;
        Battle_actionState->rangeY = spec->rangeY;
        Battle_actionState->rangeZ = spec->rangeZ;
        Battle_actionState->rangeAngle = spec->angle * 8;
    }

    Actors_ActorTaggedBonePosition(Battle_actionState->selection.primaryTarget.target.targetActor, 250, &Battle_actionState->rangeOrigin, 0);

    if (Battle_actionState->isPlayerAction) {
        ((void (*)(void*, void*, int, void*))Battle_SetSphereVolumes)(&Battle_actionState->rangeShape, NULL, 0, NULL);
    }

    count = 0;

    for (i = 0; i < 16; ++i) {
        u_int status = Actors_ActorStateFlags(i);

        if (Battle_actors[i]
            && (i != Battle_actionState->selection.primaryTarget.target.targetActor
                || Battle_categoryIncludesActingActor[Battle_actionDefinitions[action].category])
            && (status & 0x3000001) == 1
            && (Battle_actors[i]->kind & Battle_categoryTargetActorMasks[Battle_actionDefinitions[action].category])
            && !(Battle_actors[i]->kind & 0x8000)) {
            if (Battle_categoryTargetsBodyParts[Battle_actionDefinitions[action].category]) {
                for (j = 0; j < 6; ++j) {
                    int part = Actors_FindActorBoneTag(i, j, 2);

                    if (part >= 0) {
                        Actors_ActorLowGroupBonePosition(i, j, &Battle_actionState->previews[count].position, 0);

                        if (Battle_IsPointInTargetVolume((void*)&Battle_actionState->rangeShape,
                                (void*)&Battle_actionState->previews[count])) {
                            Battle_actionState->previews[count].kind = Actors_ActorBoneTag(i, part, 0);
                            Battle_actionState->previews[count].limb = j;
                            Battle_actionState->previews[count].actorId = i;
                            Battle_PreviewActionAgainstLimb(action, Battle_actionState->selection.primaryTarget.target.targetActor,
                                i, j, &result);
                            Battle_PopulateActionTargetPreview(
                                &Battle_actionState->previews[count], &result, &result.targets[0]);
                            ++count;
                        }
                    }
                }
            } else {
                Actors_ActorTaggedBonePosition(i, 255, &Battle_actionState->previews[count].position, 0);

                if (Battle_IsPointInTargetVolume((void*)&Battle_actionState->rangeShape,
                        (void*)&Battle_actionState->previews[count])) {
                    Battle_actionState->previews[count].kind = 16;
                    Battle_actionState->previews[count].limb = -1;
                    Battle_actionState->previews[count].actorId = i;
                    Battle_PreviewActionAgainstLimb(action, Battle_actionState->selection.primaryTarget.target.targetActor, i,
                        Battle_actors[i]->stats->defaultTargetLimb, &result);
                    Battle_PopulateActionTargetPreview(&Battle_actionState->previews[count], &result, &result.targets[0]);
                    ++count;
                }
            }
        }
    }

    if (Battle_categoryTargetActorMasks[Battle_actionDefinitions[action].category] & 0x80) {
        Items_TreasureChest* room = Items_RoomTreasureChests(&i);

        for (j = 0; j < i; ++j) {
            Battle_actionState->previews[count].position.vx = room[j].tileX * 128 + 64;
            Battle_actionState->previews[count].position.vz = room[j].tileZ * 128 + 64;
            Battle_actionState->previews[count].position.vy =
                (Field_FloorHeightAt((room[j].tileX << 7) | 64, (room[j].tileZ << 7) | 64) << 17)
                >> 17;

            if (room[j].openedFlag < 64
                && Battle_IsPointInTargetVolume(
                    (void*)&Battle_actionState->rangeShape, (void*)&Battle_actionState->previews[count])) {
                Battle_TargetPreview* target = &Battle_actionState->previews[count];

                target->kind = 16;
                target->limb = 16;
                target->actorId = j | 0x20;
                target->previewAmount = 0;

                if ((u_short)room[j].lockLevel - 2 < 2u && room[j].isOpen == 0) {
                    target->hitChance = ((attacker->risk + 150) * 100) / 256;
                } else {
                    target->hitChance = 0;
                }

                ++count;
                target->actionKind = 10;
                target->enemyClass = 0;
                target->opaque16 = 0;
                target->opaque15 = 0;
                target->removedStatuses = 0;
                target->effect = 0;
            }
        }
    }

    Battle_actionState->previewCount = count;

    if (Battle_actionState->isPlayerAction) {
        Battle_SetSphereTargets(count, Battle_actionState->previews, 0);
    }

    if (Battle_actionState->selection.primaryTarget.target.targetActor) {
        for (count = 0; count < Battle_actionState->previewCount; ++count) {
            if (Battle_actionState->previews[count].actorId
                    == Battle_actors[Battle_actionState->selection.primaryTarget.target.targetActor]
                        ->commandArg1.halves[0]
                && Battle_actionState->previews[count].limb
                       == Battle_actors[Battle_actionState->selection.primaryTarget.target.targetActor]
                           ->commandArg1.halves[1]) {
                Battle_actionState->selectedPreview = count;
            }
        }
        if (Battle_actionState->selectedPreview == 0xFF) {
            for (count = 0; count < Battle_actionState->previewCount; ++count) {
                if (Battle_actionState->previews[count].actorId
                    == Battle_actors[Battle_actionState->selection.primaryTarget.target.targetActor]
                        ->commandArg1.halves[0]) {
                    Battle_actionState->selectedPreview = count;
                }
            }
        }
    }

    Battle_actionFlowMode = 1;
}

// decomp: BATTLE.PRG func_80072734 (src/BATTLE/BATTLE.PRG/146C.c:4329)
void Battle_CollectGroundAreaActionTargets(int arg0)
{
    Actors_ActorLocationQuery state;
    Battle_ActorStats* attacker =
        Battle_actors[Battle_actionState->selection.primaryTarget.target.targetActor]->stats;
    Battle_ActionDefinition* spec;
    int action = Battle_actionState->selection.actionId;
    int count;
    int i;

    Battle_actionState->sourceActorId = arg0;
    Battle_actionState->rangeShape = Battle_actionDefinitions[action].shape;
    spec = &Battle_actionDefinitions[action];
    Battle_actionState->rangeKind = 2;
    Actors_ActorPlacement(Battle_actionState->selection.primaryTarget.target.targetActor, &state);
    Battle_actionState->rangeFacing = state.location.position.pad;
    count = 0;

    // BUG: the angle bits of the range word can never equal 0xFF
    if ((*(u_int*)&spec->rangeX >> 27) == 0xFF) {
        Battle_actionState->rangeX = attacker->currentRange;
        Battle_actionState->rangeY = attacker->currentRangeY;
        Battle_actionState->rangeZ = attacker->currentRangeZ;
        Battle_actionState->rangeAngle = attacker->currentAttackShapeAngle * 8;
    } else {
        Battle_actionState->rangeX = spec->rangeX;
        Battle_actionState->rangeY = spec->rangeY;
        Battle_actionState->rangeZ = spec->rangeZ;
        Battle_actionState->rangeAngle = spec->angle * 8;
        Battle_actionState->areaFacing = 0;
        Battle_actionState->areaShape = spec->areaShape & 7;
        Battle_actionState->areaX = spec->areaRangeX;
        Battle_actionState->areaY = spec->areaRangeY;
        Battle_actionState->areaZ = spec->areaRangeZ;
        Battle_actionState->areaAngle = spec->areaShape & 0xF8;

        for (i = 0; i < 10; ++i) {
            u_int status = Actors_ActorStateFlags(i);

            if (Battle_actors[i] && (status & 0x3000001) == 1
                && (i != Battle_actionState->selection.primaryTarget.target.targetActor
                    || Battle_categoryIncludesActingActor[Battle_actionDefinitions[action].category])
                && (Battle_actors[i]->kind
                    & Battle_categoryTargetActorMasks[Battle_actionDefinitions[action].category])
                && !(Battle_actors[i]->kind & 0x8000)) {
                if (Battle_categoryTargetsBodyParts[Battle_actionDefinitions[action].category]) {
                    int limb;

                    for (limb = 0; limb < 6; ++limb) {
                        int part = Actors_FindActorBoneTag(i, limb, 2);

                        if (part >= 0) {
                            Actors_ActorLowGroupBonePosition(i, limb, &Battle_actionState->previews[count].position, 0);
                            Battle_actionState->previews[count].kind = Actors_ActorBoneTag(i, part, 0);
                            Battle_actionState->previews[count].limb = limb;
                            Battle_actionState->previews[count].actorId = i;
                            ++count;
                        }
                    }
                } else {
                    Actors_ActorTaggedBonePosition(i, 255, &Battle_actionState->previews[count].position, 0);
                    Battle_actionState->previews[count].limb = -1;
                    Battle_actionState->previews[count].actorId = i;
                    ++count;
                }
            }
        }
    }

    Battle_actionState->previewCount = count;
    Actors_ActorTaggedBonePosition(Battle_actionState->selection.primaryTarget.target.targetActor, 250, &Battle_actionState->rangeOrigin, 0);

    if (Battle_actionState->selection.primaryTarget.target.targetActor) {
        Battle_actionState->areaCenter =
            *(SVECTOR*)&Battle_actors[Battle_actionState->selection.primaryTarget.target.targetActor]->commandArg1;
        Battle_actionState->areaCenter.pad = 0;
    }
    if (Battle_actionState->isPlayerAction) {
        ((void (*)(void*, void*, int, void*))Battle_SetSphereVolumes)(
            &Battle_actionState->rangeShape, &Battle_actionState->areaShape, count, Battle_actionState->previews);
    }

    Battle_actionFlowMode = 2;
}

// decomp: BATTLE.PRG func_80072BA8 (src/BATTLE/BATTLE.PRG/146C.c:4416)
void Battle_InitializeActionTargeting(int action)
{
    int _[2] __attribute__((unused));
    Actors_ActorLocationQuery state;
    int limb;
    int part;
    int count;
    u_char category;

    Battle_actionState->selection.actionId = action;
    ((void (*)())Battle_ClearSphereTargets)();
    Battle_ClearPrimarySphereVolume();
    Battle_ClearSecondarySphereVolume();

    if (action == 0) {
        Battle_actionState->rangeShape = 1;
        Battle_actionState->rangeKind = 3;
        Actors_ActorPlacement(0, &state);
        Battle_actionState->rangeFacing = state.location.position.pad;
        Battle_actionState->rangeX = 4;
        Battle_actionState->rangeY = 4;
        Battle_actionState->rangeZ = 4;
        Battle_actionState->rangeAngle = 0;
        Actors_ActorTaggedBonePosition(0, 250, &Battle_actionState->rangeOrigin, 0);
        ((void (*)(void*, void*, int, void*))Battle_SetSphereVolumes)(&Battle_actionState->rangeShape, NULL, 0, NULL);

        Battle_actionState->areaShape = 2;
        Battle_actionState->areaKind = 3;
        Actors_ActorPlacement(1, &state);
        Battle_actionState->areaFacing = state.location.position.pad;
        Battle_actionState->areaX = 4;
        Battle_actionState->areaY = 4;
        Battle_actionState->areaZ = 4;
        Battle_actionState->areaAngle = 0;
        Actors_ActorTaggedBonePosition(1, 250, &Battle_actionState->areaOrigin, 0);
        ((void (*)(void*))Battle_SetSecondarySphereVolume)(&Battle_actionState->areaShape);

        for (limb = 0, count = 0; limb < 6; ++limb) {
            part = Actors_FindActorBoneTag(1, limb, 2);
            if (part >= 0) {
                Actors_ActorLowGroupBonePosition(1, limb, &Battle_actionState->previews[count].position, 0);
                Battle_actionState->previews[count].kind = Actors_ActorBoneTag(1, part, 0);
                Battle_actionState->previews[count].limb = limb;
                Battle_actionState->previews[count].actorId = 1;
                ++count;
            }
        }
        Battle_actionState->previewCount = count;
        Battle_SetSphereTargets(count, Battle_actionState->previews, 0);
        Battle_actionFlowMode = 1;
        return;
    }

    category = Battle_actionDefinitions[action].category;
    if (Battle_categoryNeedsTargetSelection[category]) {
        if (Battle_categoryIsAreaTarget[category]) {
            if (!Battle_categoryTargetsBodyParts[category]) {
                Battle_CollectGroundAreaActionTargets(0);
            } else {
                Battle_CollectGroundAreaActionTargets(1);
            }
        } else if (!Battle_categoryTargetsBodyParts[category]) {
            Battle_CollectDirectActionTargets(2);
        } else {
            Battle_CollectDirectActionTargets(3);
        }
    } else if (Battle_categoryIsAreaTarget[category]) {
        Battle_CollectSourceAreaActionTargets(4);
    } else {
        Battle_CommitEnvironmentAction(6);
    }
}

// decomp: BATTLE.PRG func_80072EC4 (src/BATTLE/BATTLE.PRG/146C.c:4489)
void Battle_BeginActorActionSelection(int arg0, u_short arg1)
{
    int i;
    u_int temp_v0;
    Battle_Actor* temp_a0;

    Field_controlMode = 2;
    Core_Memory_ZeroAlignedWords(&Battle_actionState->selection, 0x84C);
    Battle_actionState->selection.primaryTarget.targetsPosition = 0;
    Battle_actionState->selection.primaryTarget.target.targetActor = arg0;
    Battle_actionState->selection.actionId = arg1;
    Field_StorePreferredActionCameraDistance();
    Battle_ResetTargetingRiskPenaltyTimer();
    if (arg0 != 0) {
        Battle_actionState->isPlayerAction = 0;
    } else {
        Battle_actionState->isPlayerAction = 1;
    }
    Battle_actionState->selectedPreview = Battle_actionState->cameraPreview = -1;
    temp_a0 = Battle_actors[arg0];
    Battle_actionState->previews[0].actorId = temp_a0->commandArg1.bytes[0];
    Battle_actionState->previews[0].limb = temp_a0->commandArg1.bytes[2];
    Battle_actionState->selection.targetCount = 0;
    Battle_actionState->selectionFrames = 0;
    Battle_actionState->chainStep = 0;
    Actors_RestoreActorIdlePose(Battle_actionState->selection.primaryTarget.target.targetActor);

    if (Battle_actionState->isPlayerAction != 0) {
        Battle_InitSphere();
        Render_TransformRoomPalettes(0xD, 4, -4, -4, 0);
    }

    Actors_FreezeAllActorAnimations();

    for (i = 0; i < 16; ++i) {
        if (i != Battle_actionState->selection.primaryTarget.target.targetActor) {
            temp_v0 = Actors_ActorStateFlags(i);
            if (temp_v0 & 4) {
                Actors_SetActorDisplayFlags(i, temp_v0 & ~4);
            }
        }
    }

    if (arg0 == 0) {
        Field_SetMinimapVisible(0);
    }
    Field_SetRoomAnimationsPaused(1);
    Field_SetMovingPlatformsPaused(1);
    if (Battle_actionState->isPlayerAction != 0) {
        Battle_SetActorMarkersHidden(1);
    }
    Battle_InitializeActionTargeting(Battle_actionState->selection.actionId);
    Render_SetClutAnimationEnabled(0);
}

// decomp: BATTLE.PRG func_800C4794 (src/BATTLE/BATTLE.PRG/5BF94.c:640)
int Battle_UpdateSphereTargetPosition(SVECTOR* arg0)
{
    Battle_TargetSphereLabelView* state = (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere);
    SVECTOR offset;
    int stickX = ((VECTOR*)&state->opaque38)->vx;
    int stickY = ((VECTOR*)&state->opaque38)->vy;
    Battle_TargetVolumeFlags flags = state->opaque0[0].scale.flags;
    int result;
    int lockPitch;
    int scale;
    int value;
    MATRIX* m;

    ((VECTOR*)&offset)->vx = stickX;
    ((VECTOR*)&offset)->vy = stickY;
    arg0->pad = 0;
    result = 0;

    if (state->opaque2A != 0) {
        setVector(
            arg0, state->opaque0[0].position[0], state->opaque0[0].position[1], state->opaque0[0].position[2]);
        ((void (*)(SVECTOR*, int))Battle_CacheSphereTerrainRow)((SVECTOR*)state->opaque0[0].position, state->opaque2A - 1);
        return 0;
    }

    Battle_BuildSphereCameraBasis();
    lockPitch = 0;
    stickX = Input_MapStickDeadZone(Input_stickPosBuf.rStickX);
    stickY = Input_MapStickDeadZone(Input_stickPosBuf.rStickY);
    arg0->pad = 0;

    if ((offset.pad == 0) || (Input_buttonsState & PADRleft)) {
        lockPitch = 1;
    }

    if (lockPitch) {
        stickX = stickY >> 3;

        if (stickX == 0) {
            if (Input_buttonsState & PADLup) {
                stickX = -8;
            }
            if (Input_buttonsState & PADLdown) {
                stickX += 8;
            }
        }

        stickX += offset.vy;
        stickY = (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[0].kind & 0x7F;

        if (stickY - 1 < 2u) {
            stickY = flags.opaque1Bit0 * 32;
        } else {
            stickY = 0;
        }
        if (stickX < -flags.opaque1Bit0 * 32) {
            stickX = -flags.opaque1Bit0 * 32;
        }
        if (stickY < stickX) {
            stickX = stickY;
        }

        (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque38.vy = stickX;
    } else {
        int oldX = offset.vx;
        int oldZ = offset.vz;
        int angle = -(*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[0].angle - (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque22;

        if ((stickX == 0) && (stickY == 0)) {
            if (Input_buttonsState & PADLup) {
                stickY = -64;
            }
            if (Input_buttonsState & PADLdown) {
                stickY = 64;
            }
            if (Input_buttonsState & PADLleft) {
                stickX = -64;
            }
            if (Input_buttonsState & PADLright) {
                stickX = 64;
            }
        }

        offset.vx += (-stickX * Core_Gte_Cos(angle) - stickY * Core_Gte_Sin(angle) + ONE / 8) >> 10;
        offset.vz += (-stickX * Core_Gte_Sin(angle) + stickY * Core_Gte_Cos(angle) + ONE / 8) >> 10;
        scale = Core_Math_SquareRootInteger(offset.vx * offset.vx + offset.vz * offset.vz);

        if (scale > ONE) {
            stickX = offset.vx - oldX;
            stickY = offset.vz - oldZ;
            offset.vx = offset.vx * ONE / scale;
            offset.vz = offset.vz * ONE / scale;
            scale = (Core_Gte_Atan2(offset.vx, offset.vz) - Core_Gte_Atan2(stickX, stickY)) & 0xFFF;

            if (scale < 8 || scale > 4087) {
                offset.vx = oldX;
                offset.vz = oldZ;
            }
        }

        (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque38.vx = offset.vx;
        (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque38.vz = offset.vz;
    }

    if (offset.pad != 0) {
        ((void (*)(Battle_TargetInfoLine*, int))Battle_UpdateSphereTargetHighlights)((*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque44, (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2E);
    }

    if (offset.pad == 0) {
        if (Input_buttonsPressed.allButtons & PADRdown) {
            Menu_PlayLeaveSound();
            arg0->pad = 1;
            return 1;
        }
        if (Input_buttonsPressed.allButtons & PADRright) {
            Menu_PlaySelectSound();
            (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque38.pad = 1;
            (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[0].kind |= 0x80;
            (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[1].kind &= 0x7F;
        }
    } else if (Input_buttonsPressed.allButtons & PADRdown) {
        Menu_PlayLeaveSound();
        (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[0].kind &= 0x7F;
        (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[1].kind |= 0x80;
        (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque38.pad = 0;

        for (stickX = 0; stickX < (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2E; ++stickX) {
            Battle_TargetInfoLine* trigger = &(*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque44[stickX];

            if ((trigger->opaque9 >> 4) == 0) {
                Actors_DisableModelPalettePulse(trigger->opaque9, trigger->opaqueA);
            }
        }
    } else if (Input_buttonsPressed.allButtons & PADRright) {
        Menu_PlaySelectSound();
        result = 1;
    }

    m = Battle_BuildSphereVolumeTransform((*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[0].scale.extents, (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[0].angle);
    Render_LoadFrameViewMatrix();

    for (;;) {
        switch ((*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[0].kind & 0x7F) {
        case 1:
            scale = ((*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque38.vy * 2) / flags.opaque1Bit0;
            scale = Core_Gte_SquareRoot12(ONE - scale * scale);
            break;
        case 2:
        case 3:
            scale = ONE;
            break;
        case 4:
            scale = (flags.opaque1Bit0 * 32 + (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque38.vy) * 128 / flags.opaque1Bit0;
            break;
        case 5:
            scale = -(*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque38.vy * 128 / flags.opaque1Bit0;
            break;
        }

        setVector(arg0, (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[0].position[0],
            (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[0].position[1] + (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque38.vy,
            (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[0].position[2]);

        if (arg0->vy < -1) {
            break;
        }

        (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque38.vy = -(*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[0].position[1] - 2;
    }

    Battle_DrawSphereTerrainIntersection(arg0, m, scale);
    stickX = ((*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque38.vx * scale + ONE / 2) >> 12;
    stickY = ((*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque38.vz * scale + ONE / 2) >> 12;
    arg0->vx += (stickX * m->m[0][0] + stickY * m->m[2][0] + ONE / 2) >> 12;
    arg0->vz += (stickX * m->m[0][2] + stickY * m->m[2][2] + ONE / 2) >> 12;

    value = Field_CollisionMapDimensions(0);

    if (((u_int)arg0->vx >= (value & 0xFFFF) * 8)
        || ((u_int)arg0->vz >= (value >> 16) * 8)) {
        if (arg0->vx < 0) {
            arg0->vx = 0;
        }
        if (arg0->vx >= (value & 0xFFFF) * 8) {
            arg0->vx = (value & 0xFFFF) * 8 - 1;
        }
        if (arg0->vz < 0) {
            arg0->vz = 0;
        }
        if (arg0->vz >= (value >> 16) * 8) {
            arg0->vz = (value >> 16) * 8 - 1;
        }

        stickX = arg0->vx - (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[0].position[0];
        stickY = arg0->vz - (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[0].position[2];
        value = (m->m[0][0] * m->m[2][2] - m->m[0][2] * m->m[2][0] + ONE - 1) / ONE;

        if ((scale != 0) && (value != 0)) {
            (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque38.vx =
                (stickX * m->m[2][2] - stickY * m->m[2][0]) / value * ONE / scale;
            (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque38.vz =
                (-stickX * m->m[0][2] + stickY * m->m[0][0]) / value * ONE / scale;
        }

        stickX = ((*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque38.vx * scale + ONE / 2) >> 12;
        stickY = ((*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque38.vz * scale + ONE / 2) >> 12;
        arg0->vx = (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[0].position[0]
                 + ((stickX * m->m[0][0] + stickY * m->m[2][0] + ONE / 2) >> 12);
        arg0->vz = (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[0].position[2]
                 + ((stickX * m->m[0][2] + stickY * m->m[2][2] + ONE / 2) >> 12);
    }

    Battle_DrawSpherePositionCursor(arg0, lockPitch);

    if (!((*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[1].kind & 0x80)) {
        (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[1].opaque1 = 0;
        stickX = ((VECTOR*)arg0)->vx;
        stickY = ((VECTOR*)arg0)->vy;
        ((VECTOR*)(*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[1].position)->vx = stickX;
        ((VECTOR*)(*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[1].position)->vy = stickY;
    }

    if (result) {
        ((void (*)(Battle_TargetInfoLine*, int))Battle_UpdateSphereTargetHighlights)((*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque44, (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2E);
    }

    return result;
}

// decomp: BATTLE.PRG func_800C513C (src/BATTLE/BATTLE.PRG/5BF94.c:869)
void Battle_ClearTargetActorHighlight(int arg0, int arg1)
{
    if (!(arg0 & 0xF0)) {
        Actors_DisableModelPalettePulse(arg0, arg1);
    }
}

// decomp: BATTLE.PRG func_800C5164 (src/BATTLE/BATTLE.PRG/5BF94.c:876)
void Battle_SetTargetActorHighlight(int arg0, int arg1, int arg2)
{
    if (!(arg0 & 0xF0)) {
        Actors_EnableModelPalettePulse(arg0, arg1, arg2);
    }
}

// decomp: BATTLE.PRG func_800C518C (src/BATTLE/BATTLE.PRG/5BF94.c:883)
void Battle_SetTargetActorColorOverride(int arg0, void* arg1)
{
    if (!(arg0 & 0xF0)) {
        Actors_SetActorTint(arg0, arg1);
    }
}

// decomp: BATTLE.PRG func_800C51B4 (src/BATTLE/BATTLE.PRG/5BF94.c:890)
void Battle_UpdateTargetSelectionHighlight(int mode)
{
    Battle_TargetInfoLine* entries = (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque40;
    int i;
    int group;
    if ((*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque31) {
        if (mode == 3) {
            return;
        }
        for (i = 0; i < (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2C; ++i) {
            void* data;
            group = entries[i].opaque9;
            if (group >> 4) {
                continue;
            }
            if (mode == 1) {
                Battle_ClearTargetActorHighlight(group, entries[i].opaqueA);
                data = NULL;
            } else {
                Battle_SetTargetActorHighlight(group, 0, entries[i].opaqueA);
                data = &(*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque34;
            }
            Battle_SetTargetActorColorOverride(group, data);
        }
        return;
    }
    i = (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D;
    if (i == 255) {
        return;
    }
    if (mode & 1) {
        group = (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2F;
        if (!(group >> 4)) {
            Battle_ClearTargetActorHighlight(group, (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque30);
            Battle_SetTargetActorColorOverride(group, NULL);
        }
    }
    if (mode & 2) {
        (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2F = entries[i].opaque9;
        (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque30 = entries[i].opaqueA;
        group = (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2F;
        if (group >> 4) {
            return;
        }
        Battle_SetTargetActorHighlight(group, 0, (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque30);
        Battle_SetTargetActorColorOverride(group, &(*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque34);
    }
}

// decomp: BATTLE.PRG func_800C5360 (src/BATTLE/BATTLE.PRG/5BF94.c:939)
void Battle_AppendTargetGroupToColumn(int group)
{
    int first = 0;
    int i;
    if (Battle_firstColumnEntryCount > Battle_secondColumnEntryCount) {
        for (i = 0; i < (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2C; ++i) {
            if ((*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque40[i].opaque9 != group) {
                continue;
            }
            if (!first) {
                first = i + 1;
            }
            Battle_targetColumnEntries[1][Battle_secondColumnEntryCount++] = i;
        }
        Battle_secondColumnGroupFirstTargets[Battle_secondColumnGroupCount] = first - 1;
        Battle_secondColumnGroups[Battle_secondColumnGroupCount++] = group;
        return;
    }
    for (i = 0; i < (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2C; ++i) {
        if ((*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque40[i].opaque9 != group) {
            continue;
        }
        if (!first) {
            first = i + 1;
        }
        Battle_targetColumnEntries[0][Battle_firstColumnEntryCount++] = i;
    }
    Battle_firstColumnGroupFirstTargets[Battle_firstColumnGroupCount] = first - 1;
    Battle_firstColumnGroups[Battle_firstColumnGroupCount++] = group;
}

// decomp: BATTLE.PRG func_800C553C (src/BATTLE/BATTLE.PRG/5BF94.c:970)
void Battle_BuildTargetSelectionColumns(void)
{
    int size;
    int group;
    int i;
    int count;
    Battle_secondColumnEntryCount = 0;
    Battle_firstColumnEntryCount = 0;
    Battle_secondColumnGroupCount = 0;
    Battle_firstColumnGroupCount = 0;
    for (size = 6; size > 0; --size) {
        for (group = 0; group < 16; ++group) {
            count = 0;
            for (i = 0; i < (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2C; ++i) {
                if ((*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque40[i].opaque9 == group) {
                    ++count;
                }
            }
            if (count == size) {
                Battle_AppendTargetGroupToColumn(group);
            }
        }
    }
    for (i = 0; i < (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2C; ++i) {
        group = (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque40[i].opaque9;
        if (group >> 4) {
            Battle_AppendTargetGroupToColumn(group);
        }
    }
    i = Battle_firstColumnEntryCount;
    if (i < Battle_secondColumnEntryCount) {
        i = Battle_secondColumnEntryCount;
    }
    Battle_targetColumnTopOffset = ((16 - i) * 11) >> 1;
}

// decomp: BATTLE.PRG func_800C56C0 (src/BATTLE/BATTLE.PRG/5BF94.c:1006)
void Battle_DismissTargetSelectionRows(void)
{
    int i;

    Battle_UpdateTargetSelectionHighlight(1);

    for (i = 0; i < Battle_firstColumnEntryCount; ++i) {
        (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque990[Battle_targetColumnEntries[0][i]].opaque0 = 3;
    }

    for (i = 0; i < Battle_secondColumnEntryCount; ++i) {
        (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque990[Battle_targetColumnEntries[1][i]].opaque0 = 2;
    }
}

// decomp: BATTLE.PRG func_800C5798 (src/BATTLE/BATTLE.PRG/5BF94.c:1021)
Battle_TargetSphereLabelEntry* Battle_InitTargetSelectionRow(int arg0, int arg1, int arg2)
{
    int temp_v1;
    Battle_TargetSphereLabelEntry* item = &(*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque990[arg0];
    Battle_TargetInfoLine* src = &(*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque40[arg0];

    Core_Memory_ZeroWordsBackward(item, sizeof *item);
    item->opaque0 = arg1;
    item->opaque6 = arg2 * 11 + 32;
    temp_v1 = src->opaque4;
    item->opaque8 = src->opaque0;
    item->opaqueC = temp_v1;

    if (arg1 == 2) {
        item->opaque4 = 320;
        item->limbName = Battle_limbNames[src->opaque8] + 1;
    } else {
        item->opaque4 = -72;
        item->limbName = Battle_limbNames[src->opaque8];
        item->opaque7 = 1;
    }
    return item;
}

// decomp: BATTLE.PRG _getStatusString (src/BATTLE/BATTLE.PRG/5BF94.c:1045)
char* Battle_FirstTargetStatusText(uint arg0)
{
    int i;
    arg0 >>= 5;
    for (i = 0; i < 24; i++) {
        if ((arg0 >> i) & 1) {
            return (char*)&Battle_statusStrings
                [Battle_statusStrings[Text_Battle_statusStrings_Index_strDown + i]];
        }
    }
    return (char*)&Battle_statusStrings[Text_Battle_statusStrings_Offset_empty];
}

// decomp: BATTLE.PRG func_800C58F8 (src/BATTLE/BATTLE.PRG/5BF94.c:1058)
int Battle_UpdateTargetSelectionMenu(u_char* target)
{
    Battle_TargetSphereLabelEntry* item;
    Battle_TargetInfoLine* entry;
    int i;
    int value;
    int prev;
    int type;

    if (target == NULL) {
        Battle_targetSelectionStep = 0;
        return 0;
    }
    if (Battle_targetSelectionStep == 0) {
        (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque34 = 0x808080;
        Menu_InitInformationTextBox(0);
        Battle_UpdateTargetSelectionHighlight(2);
        Battle_BuildTargetSelectionColumns();
        if ((*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2C == 0) {
            Battle_targetSelectionStep = 16;
        }
    }

    *target = 0xFF;

    if (Battle_targetSelectionStep < 16) {
        if ((Battle_targetSelectionStep >= Battle_firstColumnEntryCount) && (Battle_targetSelectionStep >= Battle_secondColumnEntryCount)) {
            Battle_targetSelectionStep = 18;
        } else {
            if (Battle_targetSelectionStep < Battle_firstColumnEntryCount) {
                i = Battle_targetColumnEntries[0][Battle_targetSelectionStep];
                item = Battle_InitTargetSelectionRow(i, 2, Battle_targetSelectionStep);
                if (Battle_targetSelectionStep != 0) {
                    prev = Battle_targetColumnEntries[0][Battle_targetSelectionStep - 1];
                    if ((*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque40[i].opaque9 != (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque40[prev].opaque9) {
                        item->opaque7 = 1 - (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque990[prev].opaque7;
                    } else {
                        item->opaque7 = (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque990[prev].opaque7;
                    }
                }
            }
            if (Battle_targetSelectionStep < Battle_secondColumnEntryCount) {
                i = Battle_targetColumnEntries[1][Battle_targetSelectionStep];
                item = Battle_InitTargetSelectionRow(i, 3, Battle_targetSelectionStep);
                if (Battle_targetSelectionStep != 0) {
                    prev = Battle_targetColumnEntries[1][Battle_targetSelectionStep - 1];
                    if ((*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque40[i].opaque9 != (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque40[prev].opaque9) {
                        item->opaque7 = 1 - (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque990[prev].opaque7;
                    } else {
                        item->opaque7 = (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque990[prev].opaque7;
                    }
                }
            }
        nextState:
            ++Battle_targetSelectionStep;
        }
    } else {
        switch (Battle_targetSelectionStep) {
        case 16:
            Menu_textBoxes[7].flags.frameShape = 1;
            Menu_SetTextBoxText(7, (char*)&Battle_doorStrings[Battle_doorStrings[0]]);
            if (Input_buttonsPressed.allButtons & 0x60) {
                Menu_PlayLeaveSound();
                Battle_targetSelectionStep = 17;
            }
            break;
        case 17:
            (*(char (*))&Battle_reuseSphereVolumes) = (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[0].opaque1 == 2;
            Battle_targetSelectionStep = 21;
            break;
        case 18:
            if (Battle_AreTargetLabelsSettled() == 0) {
                break;
            }
            goto nextState;
        case 19:
            if (++(*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque990[(*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D].opaque1[2] == 0x22) {
                (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque990[(*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D].opaque1[2] = 0x12;
            }
            (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque990[(*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D].opaque1[0] = 0;
            if (Input_buttonsPressed.allButtons & 0x40) {
                Menu_PlayLeaveSound();
                Battle_DismissTargetSelectionRows();
                (*(char (*))&Battle_reuseSphereVolumes) = (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque0[0].opaque1 == 2;
                Battle_targetSelectionStep = 21;
                break;
            }
            *target = (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D;
            if (Input_buttonsPressed.allButtons & 0x20) {
                Battle_DismissTargetSelectionRows();
                Battle_targetSelectionStep = 20;
                break;
            }
            value = (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D;
            if (Input_buttonRepeat & 1) {
                for (i = 0; i < Battle_firstColumnGroupCount; ++i) {
                    if (Battle_firstColumnGroups[i] == (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque40[value].opaque9) {
                        break;
                    }
                }
                if (i != Battle_firstColumnGroupCount) {
                    if (++i == Battle_firstColumnGroupCount) {
                        if (Battle_secondColumnGroupCount == 0) {
                            goto selected;
                        }
                        goto firstOfRow1;
                    }
                    (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D = Battle_firstColumnGroupFirstTargets[i];
                    goto selected;
                }
                for (i = 0; i < Battle_secondColumnGroupCount; ++i) {
                    if (Battle_secondColumnGroups[i] == (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque40[value].opaque9) {
                        break;
                    }
                }
                if (++i == Battle_secondColumnGroupCount) {
                    goto firstOfRow0;
                }
                (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D = Battle_secondColumnGroupFirstTargets[i];
                goto selected;
            }
            for (i = 0; i < Battle_firstColumnEntryCount; ++i) {
                if (Battle_targetColumnEntries[0][i] == value) {
                    break;
                }
            }
            if (i != Battle_firstColumnEntryCount) {
                if ((Input_buttonsPressed.allButtons & 0x8000) && (Battle_secondColumnEntryCount != 0)) {
                    if (i >= Battle_secondColumnEntryCount) {
                        i = Battle_secondColumnEntryCount - 1;
                    }
                    goto row1;
                }
                do {
                    if (Input_buttonRepeat & 0x1000) {
                        if (i == 0) {
                            if (Battle_secondColumnEntryCount != 0) {
                                goto lastOfRow1;
                            }
                            i = Battle_firstColumnEntryCount - 1;
                        } else {
                            --i;
                        }
                    }
                } while (0);
                if (Input_buttonRepeat & 0x4000) {
                    if (i != Battle_firstColumnEntryCount - 1) {
                        ++i;
                    } else {
                        i = 0;
                        if (Battle_secondColumnEntryCount != 0) {
                            goto firstOfRow1;
                        }
                    }
                }
                (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D = Battle_targetColumnEntries[0][i];
                goto selected;
            lastOfRow1:
                (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D = Battle_targetColumnEntries[1][Battle_secondColumnEntryCount - 1];
                goto selected;
            firstOfRow1:
                (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D = Battle_targetColumnEntries[1][0];
                goto selected;
            lastOfRow0:
                (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D = Battle_targetColumnEntries[0][Battle_firstColumnEntryCount - 1];
                goto selected;
            firstOfRow0:
                (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D = Battle_targetColumnEntries[0][0];
                goto selected;
            }
            for (i = 0; i < Battle_secondColumnEntryCount; ++i) {
                if (Battle_targetColumnEntries[1][i] == value) {
                    break;
                }
            }
            if (Input_buttonsPressed.allButtons & 0x2000) {
                if (i >= Battle_firstColumnEntryCount) {
                    i = Battle_firstColumnEntryCount - 1;
                }
                (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D = Battle_targetColumnEntries[0][i];
                goto selected;
            }
            if (Input_buttonRepeat & 0x1000) {
                if (i == 0) {
                    goto lastOfRow0;
                }
                --i;
            }
            if (Input_buttonRepeat & 0x4000) {
                if (i == Battle_secondColumnEntryCount - 1) {
                    goto firstOfRow0;
                }
                ++i;
            }
        row1:
            (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D = Battle_targetColumnEntries[1][i];
        selected:
            (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque990[(*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D].opaque1[0] = 1;
            if (value != (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D) {
                (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque990[(*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D].opaque1[2] = 0;
                Menu_PlayChangeSound();
                Battle_UpdateTargetSelectionHighlight(3);
            }
            entry = &(*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque40[(*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D];
            i = entry->opaque9;
            type = entry->opaque8;
            if (i >> 4) {
                Battle_stringContext.strings[0] = (char*)&Battle_statusStrings
                    [Battle_statusStrings[(i >> 5) + Text_Battle_statusStrings_Index_blocker]];
            } else if (type == 16) {
                Battle_stringContext.strings[0] = Battle_actors[i]->stats->name;
            } else {
                Battle_stringContext.strings[0] =
                    (char*)&Battle_statusStrings[Battle_statusStrings[type]];
            }
            for (value = 1; value < 6; ++value) {
                Battle_stringContext.strings[value] = Battle_emptyStatusString;
            }
            value = entry->opaqueB;
            if ((type == 16) && !(i >> 4) && (Battle_actors[i]->kind & 0x10)) {
                value = 5;
                Battle_stringContext
                    .strings[1] = (char*)&Battle_statusStrings[Battle_statusStrings
                        [(u_short)entry->opaqueE == 0 ? Text_Battle_statusStrings_Index_indestructable
                                                   : Text_Battle_statusStrings_Index__3]];
            } else if (value == 0x3A) {
                value = 1;
            } else if (value == 0x3B) {
                value = 2;
            } else if (value == 0x3E) {
                value = 3;
            } else if (value == 0x3F) {
                value = 4;
            } else {
                Battle_stringContext.strings[1] = Battle_targetStatusLineTemplate;
                Battle_stringContext.strings[6] = Battle_FirstTargetStatusText(entry->opaque10);
                value = 0;
            }
            Battle_stringContext.strings[2] =
                (char*)&Battle_statusStrings[Battle_statusStrings
                        [entry->opaque14 + Text_Battle_statusStrings_Index_indestructable]];
            if (value != 0) {
                if (value < 5) {
                    Battle_stringContext.strings[1] =
                        (char*)&Battle_statusStrings[Battle_statusStrings
                                [Text_Battle_statusStrings_Index_mpTemplate - (value & 1)]];
                    Battle_stringContext.strings[5] =
                        (char*)&Battle_statusStrings[Battle_statusStrings
                                [entry->opaque17 + Text_Battle_statusStrings_Index_human]];
                }
                Battle_stringContext.strings[3] =
                    (char*)&Battle_statusStrings[Battle_statusStrings
                            [entry->opaque15 + Text_Battle_statusStrings_Index_empty]];
                Battle_stringContext.strings[4] =
                    (char*)&Battle_statusStrings[Battle_statusStrings
                            [entry->opaque16 + Text_Battle_statusStrings_Index_physical]];
            }
            Battle_stringContext.integers[0] = *(u_short*)&entry->opaqueC;
            Battle_stringContext.integers[1] = (u_short)entry->opaqueE;
            Render_FormatEncodedText((*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->textBuffer, Battle_targetInfoTemplate);
            Menu_SetTextBoxText(7, (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->textBuffer);
            break;
        case 20:
            *target = (*(Battle_TargetSphereLabelView* (*))&Battle_targetSphere)->opaque2D;
            // fallthrough
        case 21:
            Menu_DismissTextBox(7);
            Battle_ClearPrimarySphereVolume();
            return 1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_800E5568 (src/BATTLE/BATTLE.PRG/7CD68.c:437)
int Battle_CalculateRangeShapeExtent(Battle_Ai_ActionRangeShape* arg0)
{
    int (*fn)(int);
    int mask;
    int r;
    int v;

    mask = 1 << arg0->shape;
    if (mask & 0x16) {
        r = arg0->rangeZ;
        fn = Core_Gte_Cos;
    } else if (mask & 0x8) {
        r = arg0->rangeY;
        fn = Core_Gte_Cos;
    } else {
        r = arg0->rangeY;
        fn = Core_Gte_Sin;
    }
    v = fn(arg0->angle << 7);
    return ((r << 5) * ABS(v)) >> 12;
}

// decomp: BATTLE.PRG func_800E5600 (src/BATTLE/BATTLE.PRG/7CD68.c:459)
int Battle_CalculateActionAreaReach(intptr_t arg0, int actionId, u_int arg2)
{
    Battle_ActionDefinition* action = &Battle_actionDefinitions[actionId];
    int r = 0;
    int v;

    if (action->aiFlags.radialArea << 0x1D) {
        r = action->areaRangeY << 5;
    } else if (action->aiFlags.shapedArea << 0x17) {
        v = Battle_CalculateRangeShapeExtent((Battle_Ai_ActionRangeShape*)&action->areaRangeX);
        if (v < arg2) {
            r = v;
        }
    }
    return r;
}

// decomp: BATTLE.PRG func_800E5698 (src/BATTLE/BATTLE.PRG/7CD68.c:476)
int Battle_CalculateActionRangeSquared(Battle_Ai_ActionRangeView* arg0, int actionId)
{
    Battle_ActionDefinition* action = &Battle_actionDefinitions[actionId];
    Battle_Ai_ActionRangeShape* range;
    u_int v;

    if (action->rangeX == 0xFF) {
        range = &arg0->opaque5C->opaque38;
    } else {
        range = (Battle_Ai_ActionRangeShape*)&action->rangeX;
    }
    v = Battle_CalculateRangeShapeExtent(range);
    if (v > 0x20) {
        v -= 0x20;
    }
    return v * v;
}

