// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ActorLoading.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/CharacterInit.h"
#include "Actors/Lifecycle.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/RoomActorCache.h"
#include "Actors/RoomState.h"
#include "Actors/Skeleton.h"
#include "Actors/Spawning.h"
#include "Actors/Zud.h"
#include "Battle/ActionAnimation.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Damage.h"
#include "Battle/StatusEffects.h"
#include "CdDrive.h"
#include "Core/Callbacks.h"
#include "Core/Memory/Heap.h"
#include "Core/Random.h"
#include "Core/Runtime.h"
#include "DiscQueue.h"
#include "Field/Actors.h"
#include "Field/Mpd.h"
#include "Field/RoomLoading.h"
#include "Field/Znd.h"
#include "Field/Zone.h"
#include "Items/Categories.h"
#include "Items/Equipment.h"
#include "Items/EquipmentInit.h"
#include "Items/EquipmentStats.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"
#include "State/PlayTime.h"
#include "State/RoomCache.h"
#include "State/RoomMemory.h"

// decomp: D_800F188C (D_800F188C@BATTLE/BATTLE.PRG)
Actors_CharacterInit* Actors_zoneCharacterInitData;

// decomp: BATTLE.PRG func_800765B0 (src/BATTLE/BATTLE.PRG/146C.c:6064)
Battle_Actor* Actors_CreateAuxiliaryActor(
    int index, int actorId, Field_ActorPlacement* arg2, int material, int arg4)
{
    Content_ActorAssetRequest objData;
    int i;
    int var_v1_2;
    Actors_ActorCore* temp_v0;

    if (index == 0xFF) {
        for (i = 2; i < 16; ++i) {
            if (Battle_actors[i] == NULL) {
                index = i;
                break;
            }
        }
        if (i >= 16) {
            Core_IgnoreDiagnostic(0x86, 0);
            return NULL;
        }
    }

    if (Battle_actors[index] == NULL) {
        temp_v0 = Core_Memory_AllocateFromLowEnd(sizeof *temp_v0);
        Core_Memory_ZeroAlignedWords(temp_v0, sizeof temp_v0->actor);
        temp_v0->actor.actor.stats = &temp_v0->actor.stats;
        temp_v0->actor.actor.model = &temp_v0->model;
        temp_v0->actor.actor.modelId = 0xFE;
        temp_v0->actor.actor.kind = 0x10;
        temp_v0->actor.actor.allocationKind = 0;
        temp_v0->actor.actor.id = index;
        temp_v0->actor.actor.next = NULL;
        temp_v0->actor.actor.retainAfterDefeat = 0;
        objData.assetKind = 6;
        objData.targetSlot = index;
        objData.placement = *arg2;
        objData.placement.direction &= 1;
        var_v1_2 = actorId;
        objData.destination = temp_v0->actor.actor.model;
        objData.subKind = actorId;
        if (actorId < 0) {
            var_v1_2 = actorId + 0xFF;
        }
        objData.textureSlot = var_v1_2 >> 8;
        objData.material = material;
        objData.variant = arg2->direction >> 4;

        Content_QueueActorAssetRequest(&objData);

        while (Content_FindFreeActorAssetRequest() != 0) {
            Content_UpdateActorAssetQueue();
            State_UpdatePlayTimeAndLoading(0);
        }

        Battle_actors[index] = &temp_v0->actor.actor;

        Actors_InitializePlaceholderActorStats(index, actorId);
        Battle_Ai_CreateActorAiState(&temp_v0->actor.actor, -1);
        Actors_SetActorDisplayFlags(index, 0x1846);

        return &temp_v0->actor.actor;
    }
    return NULL;
}

// decomp: BATTLE.PRG func_80076784 (src/BATTLE/BATTLE.PRG/146C.c:6128)
void Actors_ApplyActorInitialStats(
    int arg0, Battle_ActorStats* actor, Actors_CharacterInit* init, int arg3)
{
    int j;
    int i;
    short var_s0;

    actor->actorId = arg0;

    for (i = 0; i < 24; ++i) {
        actor->name[i] = init->name[i];
    }

    var_s0 = init->hp;
    if (arg3 != 0) {
        var_s0 += -10 + Core_Random_NextAveraged(21);
    }
    if (var_s0 < 0) {
        var_s0 = 0;
    } else if (var_s0 > 999) {
        var_s0 = 999;
    }

    actor->maxHp = var_s0;
    actor->currentHp = var_s0;
    actor->risk = 0;

    var_s0 = init->mp;
    if (var_s0 != 0) {
        if (arg3 != 0) {
            var_s0 += -10 + Core_Random_NextAveraged(21);
        }
        if (var_s0 < 0) {
            var_s0 = 0;
        } else if (var_s0 > 999) {
            var_s0 = 999;
        }
    }

    actor->maxMp = var_s0;
    actor->currentMp = var_s0;

    var_s0 = init->strength;
    if (arg3 != 0) {
        var_s0 += -10 + Core_Random_NextAveraged(21);
    }
    if (var_s0 < 0) {
        var_s0 = 0;
    } else if (var_s0 > 999) {
        var_s0 = 999;
    }

    actor->totalStrength = var_s0;
    actor->strength = var_s0;

    var_s0 = init->intelligence;
    if (arg3 != 0) {
        var_s0 += -10 + Core_Random_NextAveraged(21);
    }
    if (var_s0 < 0) {
        var_s0 = 0;
    } else if (var_s0 > 999) {
        var_s0 = 999;
    }

    actor->totalIntelligence = var_s0;
    actor->intelligence = var_s0;

    var_s0 = init->agility;
    if (arg3 != 0) {
        var_s0 += -10 + Core_Random_NextAveraged(21);
    }
    if (var_s0 < 0) {
        var_s0 = 0;
    } else if (var_s0 > 999) {
        var_s0 = 999;
    }

    actor->totalAgility = var_s0;
    actor->agility = var_s0;
    actor->movementClass = init->material;
    actor->gaitClass = init->opaque26;
    actor->walkSpeed = actor->baseWalkSpeed = (init->walkSpeed * 9) / 10;
    actor->runSpeed = actor->baseRunSpeed = (init->runSpeed * 9) / 10;
    actor->enemyClass = init->enemyClass;
    actor->reach = init->reach;
    actor->recoveryDelayBonus = init->opaque23;
    actor->mobilityLimb0 = init->opaque30;
    actor->mobilityLimb1 = init->opaque31;
    actor->defaultTargetLimb = init->opaque32;
    actor->cancelsHitResults = 0;
    actor->isAnalyzed = 0;
    Items_BuildWeaponStatsFromParts(&actor->weapon, &init->weapon);
    actor->currentRange = actor->weapon.range.range + actor->reach;
    actor->currentRangeY = actor->weapon.range.secondRange + actor->reach;
    actor->currentRangeZ = actor->weapon.range.thirdRange + actor->reach;
    actor->currentAttackShapeAngle = actor->weapon.range.attackShapeAngle;
    actor->currentAttackShape = 1;
    Items_BuildShieldStatsFromParts(&actor->shield, &init->shield);
    Items_BuildAccessoryStatsFromPart(&actor->accessory, &init->accessory);

    for (i = 0; i < 6; ++i) {
        actor->limbs[i].hp = actor->limbs[i].maxHp = init->limbs[i].hp;
        actor->limbs[i].agilityDefenseBonus = init->limbs[i].agilityDefenseBonus;

        j = Actors_FindActorBoneTag(arg0, i, 2);

        if (j >= 0) {
            actor->limbs[i].anchorBone = j;
            actor->limbs[i].nameIndex = Actors_ActorBoneTag(arg0, j, 0);
        } else {
            actor->limbs[i].nameIndex = 0;
            actor->limbs[i].anchorBone = 0;
        }

        actor->limbs[i].chainEvasion = init->limbs[i].chainEvasion;

        for (j = 0; j < 4; ++j) {
            actor->limbs[i].types[j] = init->limbs[i].types[j];
        }

        for (j = 0; j < 7; ++j) {
            actor->limbs[i].affinities[j] = init->limbs[i].affinities[j];
        }

        Items_BuildArmorStatsFromPart(&actor->limbs[i].armor, &init->limbs[i].armor);
    }

    Core_IgnoreCallback(actor);

    for (i = 0; i < 6; ++i) {
        for (j = 0; j < 4; ++j) {
            actor->limbActions[i][j].actionId = init->limbs[i].actions[j].actionId;
            actor->limbActions[i][j].index = init->limbs[i].actions[j].index;
            actor->limbActions[i][j].effectModelId = init->limbs[i].actions[j].opaque2;
            actor->limbActions[i][j].opaque3 = 1;
        }
    }

    for (i = 0; i < 6; ++i) {
        for (j = 0; j < 6; ++j) {
            actor->dpSplash[i][j] = init->limbs[i].dpSplash[j];
        }
    }

    actor->statusImmunities = init->opaque28;
    {
        int v = 0x80000000;
        do {
            actor->statuses = 0;

            if (actor->statusImmunities < 0) {
                actor->statuses = v;
            }
        } while (0);
    }

    for (i = 0; i < 5; ++i) {
        actor->statusTimers[i] = 0;
    }

    actor->actionRecoveryTimer = 0;
}

// decomp: BATTLE.PRG func_80076D50 (src/BATTLE/BATTLE.PRG/146C.c:6292)
void Actors_InitializeActorDefaults(u_int id, int arg1, int arg2, int arg3, int arg4)
{
    Battle_Actor* temp_s1 = Battle_actors[id];
    Battle_ActorStats* temp_s0 = temp_s1->stats;

    temp_s1->id = id;
    temp_s1->modelId = arg1;
    temp_s1->subType = 0;
    temp_s1->bladeId = arg2;
    temp_s1->shieldId = arg3;
    temp_s1->weaponDrawn = 0;

    switch (id) {
    case 0:
        temp_s1->kind = 1;
        break;
    case 1:
        temp_s1->kind = 2;
        break;
    default:
        switch (arg4 & 3) {
        case 1:
            temp_s1->kind = 4;
            temp_s1->weaponDrawn |= 1;
            Actors_SetActorWeaponDrawn(id, 1, 0);
            break;
        case 0:
            temp_s1->kind = 2;
            break;
        case 2:
        case 3:
            temp_s1->kind = 8;
            break;
        }
    }

    temp_s1->interactionFlags = arg4 & 0x1C;
    temp_s1->lastTriggerTile.bytes[3] = 0;
    temp_s1->lastTriggerTile.bytes[2] = 0;
    temp_s1->lastTriggerTile.bytes[1] = 0;
    temp_s1->lastTriggerTile.bytes[0] = 0;
    temp_s1->facing = 0;
    temp_s1->commandState = 0;
    temp_s0->maxHp = 100;
    temp_s0->currentHp = 100;
    temp_s0->maxMp = 100;
    temp_s0->currentMp = 100;
    temp_s0->risk = 0;
    temp_s0->actionRecoveryTimer = 0;

    if (arg1 == 41) {
        temp_s0->movementClass = 16;
        temp_s0->gaitClass = 32;
    } else if (arg1 == 48) {
        temp_s0->movementClass = 2;
        temp_s0->gaitClass = 8;
    } else {
        temp_s0->movementClass = 2;
        temp_s0->gaitClass = 4;
    }

    temp_s0->walkSpeed = 9;
    temp_s0->runSpeed = 18;
    temp_s0->weapon.recoveryDelay = 20;
    temp_s0->weapon.range.range = 5;
    temp_s0->weapon.range.secondRange = 4;
    temp_s0->weapon.range.thirdRange = 5;
    temp_s0->cancelsHitResults = 0;
    temp_s0->weapon.range.attackShapeAngle = 0;
    temp_s1->roomSpawnIndex = 128;
    temp_s1->killCounted = 0;
    temp_s1->defeated = 0;
    temp_s0->actorId = 128;
}

// decomp: BATTLE.PRG func_80076F24 (src/BATTLE/BATTLE.PRG/146C.c:6367)
void Actors_InitializeActorFromTemplate(
    int id, Actors_CharacterInit* arg1, int arg2, int arg3, int arg4, int arg5)
{
    int temp_v1;
    Battle_Actor* temp_s0;
    Battle_ActorStats* temp_s4;

    temp_s0 = Battle_actors[id];
    temp_s0->id = id;
    temp_s4 = temp_s0->stats;
    temp_s0->modelId = arg1->characterKind;
    temp_s0->subType = arg1->subType;
    temp_s0->bladeId = arg2;
    temp_s0->shieldId = arg3;
    temp_s0->weaponDrawn = 0;

    switch (id) {
    case 0:
        temp_s0->kind = 1;
        break;
    case 1:
        temp_v1 = 2;
        temp_s0->kind = temp_v1;
        break;
    default:
        switch (arg4 & 3) {
        case 1:
            temp_s0->kind = 4;
            temp_s0->weaponDrawn |= 1;
            Actors_SetActorWeaponDrawn(id, 1, 0);
            break;
        case 2:
        case 3:
            temp_s0->kind = 8;
            break;
        case 0:
            temp_s0->kind = 2;
            break;
        }
    }

    if (arg4 & 0x80) {
        temp_s0->retainAfterDefeat = 1;
    } else {
        temp_s0->retainAfterDefeat = 0;
    }

    temp_s0->interactionFlags = arg4 & 0x1C;
    temp_s0->commandState = 0;
    temp_s0->lastTriggerTile.bytes[3] = 0;
    temp_s0->lastTriggerTile.bytes[2] = 0;
    temp_s0->lastTriggerTile.bytes[1] = 0;
    temp_s0->lastTriggerTile.bytes[0] = 0;
    Actors_ApplyActorInitialStats(id, temp_s4, arg1, arg5);
}

// decomp: BATTLE.PRG func_80077078 (src/BATTLE/BATTLE.PRG/146C.c:6423)
int Actors_QueueActorModelLoad(Battle_Actor* arg0, int index, int wepId,
    Field_ActorPlacement* arg3, int arg4)
{
    Content_ActorAssetRequest sp10;

    sp10.assetKind = 1;
    sp10.targetSlot = index;
    sp10.modelId = wepId;
    sp10.placement = *arg3;
    sp10.destination = arg0->model;
    if (arg4 & 0x80) {
        sp10.subKind = (arg4 & 0xF00) >> 8;
        sp10.textureSlot = (arg4 & 0xF000) >> 0xC;
    } else {
        sp10.subKind = 0xFF;
    }
    sp10.variant = 0;
    sp10.material = 2;
    return Content_QueueActorAssetRequest(&sp10);
}

// decomp: BATTLE.PRG func_80077130 (src/BATTLE/BATTLE.PRG/146C.c:6455)
void Actors_QueueEquipmentModelLoad(
    Battle_Actor* actor, int actorId, int wepId, int isShield, int material)
{
    Content_ActorAssetRequest sp10;

    if (wepId != 0xFF) {
        int temp_s0 = (actorId * 2) + isShield;
        Content_ReleaseWeaponModel(temp_s0);
        if (wepId != 0) {
            sp10.assetKind = 3;
            sp10.targetSlot = temp_s0;
            sp10.modelId = wepId;
            sp10.destination = actor->equipmentModels[isShield];
            sp10.subKind = actorId;
            if (isShield == 0) {
                sp10.textureSlot = 0xF0;
            } else {
                sp10.textureSlot = 0xF1;
            }
            sp10.material = material;
            Content_QueueActorAssetRequest(&sp10);
        }
    }
}

// decomp: BATTLE.PRG func_80077240 (src/BATTLE/BATTLE.PRG/146C.c:6493)
Actors_ActorAllocation* Actors_AllocateActorWithEquipment(int arg0, int arg1, int bladeWepId, int bladeMaterial,
    int shieldWepId, int shieldMaterial, Field_ActorPlacement* arg6, int arg7)
{
    int i;
    Actors_ActorAllocation* temp_v0;
    int _[6] __attribute__((unused));

    if (Battle_actors[arg0] != NULL) {
        return NULL;
    }
    if (arg0 < 0xA) {
        temp_v0 = Core_Memory_AllocateFromLowEnd(sizeof *temp_v0);
        Core_Memory_ZeroAlignedWords(temp_v0, sizeof temp_v0->core.actor);
        i = 0xA;
        temp_v0->core.actor.actor.stats = &temp_v0->core.actor.stats;
        temp_v0->core.actor.actor.allocationKind = 1;
        temp_v0->core.actor.actor.model = &temp_v0->core.model;
        temp_v0->core.actor.actor.equipmentModels[0] = &temp_v0->modelSlots[0];
        temp_v0->core.actor.actor.equipmentModels[1] = &temp_v0->modelSlots[1];

        if (bladeWepId != 0) {
            for (i = 1; i < 11; ++i) {
                if (Items_weaponCategoryUpperIds[i] >= bladeWepId) {
                    break;
                }
            }
        }

        temp_v0->core.actor.actor.equippedWeaponCategory = i;

        Actors_QueueActorModelLoad(&temp_v0->core.actor.actor, arg0, arg1, arg6, arg7);
        if (arg1 != 0x7F) {
            Actors_QueueActorBaseSequenceLoad(&temp_v0->core.actor.actor, arg0);
            Actors_QueueEquipmentModelLoad(&temp_v0->core.actor.actor, arg0, bladeWepId, 0, bladeMaterial);
            Actors_QueueEquipmentModelLoad(&temp_v0->core.actor.actor, arg0, shieldWepId, 1, shieldMaterial);
            Actors_QueueActorWeaponSequenceLoad((char*)&temp_v0->core, arg0,
                temp_v0->core.actor.actor.equippedWeaponCategory, arg7);
        }
        return temp_v0;
    }
    return NULL;
}

// decomp: BATTLE.PRG func_800773BC (src/BATTLE/BATTLE.PRG/146C.c:6536)
void Actors_RegisterLoadedActor(
    Battle_Actor* arg0, int arg1, int arg2, int arg3, int arg4, int arg5)
{
    Actors_PlayActorAnimation(arg1, 0, 0, 0);
    Actors_SetActorDisplayFlags(arg1, 0x1847);
    Battle_actors[arg1] = arg0;
    Actors_InitializeActorDefaults(arg1, arg2, arg3, arg4, arg5);
    Actors_SetActorWalkRunSpeeds(arg1, 0x9000, 0x12000);
    Battle_SetEnemyCombatPose(arg1, 0);
    if (arg0->kind & 7) {
        if (arg1 != 0) {
            arg0->next = Battle_actors[0]->next;
            Battle_actors[0]->next = arg0;
        } else {
            arg0->next = NULL;
        }
    }
    if (arg0->weaponDrawn & 1) {
        Actors_SetActorDisplayFlags(arg1, 0x1846);
    } else {
        Actors_SetActorDisplayFlags(arg1, 0x46);
    }
    arg0->templateWord2C = 0;
    Battle_Ai_CreateActorAiState(arg0, -1);
}

// decomp: BATTLE.PRG func_800774FC (src/BATTLE/BATTLE.PRG/146C.c:6562)
Battle_Actor* Actors_CreateActorWithEquipment(int arg0, int arg1, int bladeWepId, int bladeMaterial,
    int shieldWepId, int shieldMaterial, Field_ActorPlacement* arg6, int arg7)
{
    Battle_Actor* temp_v0 = (Battle_Actor*)Actors_AllocateActorWithEquipment(
        arg0, arg1, bladeWepId, bladeMaterial, shieldWepId, shieldMaterial, arg6, arg7);
    if (temp_v0 != NULL) {
        while (Content_FindFreeActorAssetRequest() != 0) {
            Content_UpdateActorAssetQueue();
            State_UpdatePlayTimeAndLoading(0);
        }
        Actors_RegisterLoadedActor(temp_v0, arg0, arg1, bladeWepId, shieldWepId, arg7);
        return temp_v0;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800775C0 (src/BATTLE/BATTLE.PRG/146C.c:6578)
Battle_Actor* Actors_SpawnRoomEnemy(
    int id, Field_MpdEnemySpawn* spawn, Actors_CharacterInit* init, int flags)
{
    Content_ActorAssetRequest object;
    Content_DiscFile file;
    Battle_Actor* actor;
    Actors_ZudHeader* zud;
    Actors_RoomActorRecord* saved;
    int zudIndex;
    int bladeId;
    int shieldId;
    int loaded;
    int i;
    int category;
    int height;

    if (Battle_actors[id] == NULL) {
        zudIndex = init->zudIndex;
        bladeId = (u_char)init->weapon.blade.weaponModelId;
        shieldId = (u_char)init->shield.base.weaponModelId;
        if (Actors_usesSecondaryBodyModel[(u_char)init->characterKind]) {
            actor = Core_Memory_AllocateFromLowEnd(0x3BB4);
            actor->stats = (Battle_ActorStats*)(actor + 1);
            actor->allocationKind = 2;
            actor->model = (void*)((char*)actor + 0x9B4);
            actor->equipmentModels[0] = (void*)((char*)actor + 0x22B4);
        } else {
            actor = Core_Memory_AllocateFromLowEnd(0x2E84);
            actor->stats = (Battle_ActorStats*)(actor + 1);
            actor->allocationKind = 1;
            actor->model = (void*)((char*)actor + 0x9B4);
            actor->equipmentModels[0] = (void*)((char*)actor + 0x22B4);
            actor->equipmentModels[1] = (void*)((char*)actor + 0x289C);
        }
        category = 10;
        if (bladeId) {
            category = init->weapon.blade.category;
        }
        actor->equippedWeaponCategory = category;

        loaded = 0;
        if (Field_zoneContext.zudData != NULL) {
            if (zudIndex + 1 == Field_zoneContext.loadedZudIndexPlusOne) {
                loaded = 1;
            } else {
                Core_Memory_FreeFromHighEnd(Field_zoneContext.zudData);
                Field_zoneContext.zudData = NULL;
                Field_zoneContext.loadedZudIndexPlusOne = 0;
            }
        }
        if (!loaded) {
            file.lba = Field_zoneContext.zudFiles[zudIndex].lba;
            file.size = Field_zoneContext.zudFiles[zudIndex].size;
            if (Field_zoneContext.zudDiscRequest != NULL) {
                Core_IgnoreDiagnostic(0xA7, 0);
            }
            Field_zoneContext.zudDiscRequest = Content_AllocateDiscRequest(&file);
            Field_zoneContext.zudData = Core_Memory_AllocateFromHighEnd(file.size);
            Content_EnqueuePriorityDiscRequest(Field_zoneContext.zudDiscRequest, Field_zoneContext.zudData);
            while (Field_zoneContext.zudDiscRequest->state != 4) {
                State_UpdatePlayTimeAndLoading(0);
            }
            Content_ReleaseDiscRequest(Field_zoneContext.zudDiscRequest);
            Field_zoneContext.zudDiscRequest = NULL;
            Field_zoneContext.loadedZudIndexPlusOne = zudIndex + 1;
        }

        zud = Field_zoneContext.zudData;
        object.assetKind = 2;
        object.targetSlot = id;
        object.dataAddress = zud->characterShapeOffset + (uintptr_t)zud;
        object.modelId = (u_char)Field_zoneContext.zndEnemies[zudIndex].recordBytes[2];
        if ((State_recentRoomEnemyStates[State_currentRoomStateSlot].zoneId == Field_zoneContext.zndId)
            && (State_recentRoomEnemyStates[State_currentRoomStateSlot].mapId == Field_zoneContext.mapId)
            && ((saved = Actors_FindCachedRoomActorState(spawn->index)) != NULL)) {
            object.placement.tileX = saved->tileX;
            object.placement.tileZ = saved->tileZ;
            object.placement.direction = saved->direction;
            height = saved->height;
            object.placement.direction &= 1;
        } else {
            object.placement.tileX = spawn->tileX;
            object.placement.tileZ = spawn->tileZ;
            object.placement.direction = spawn->direction;
            height = spawn->height << 6;
            object.placement.direction &= 1;
        }
        object.placement.height = height;
        object.destination = actor->model;
        object.subKind = 0xFF;
        object.variant = spawn->variant & 1;
        object.material = init->material;
        Content_QueueActorAssetRequest(&object);

        if ((u_char)init->characterKind != 0x7F) {
            if (Actors_usesSecondaryBodyModel[(u_char)init->characterKind]) {
                object.assetKind = 1;
                object.targetSlot = id + 4;
                object.modelId = (u_char)Actors_zoneCharacterInitData[zudIndex].characterKind + 1;
                object.destination = actor->equipmentModels[0];
                object.subKind = id;
                object.textureSlot = 0xFC;
                object.variant = 0;
                Content_QueueActorAssetRequest(&object);
            } else {
                if (bladeId) {
                    object.assetKind = 4;
                    object.targetSlot = id * 2;
                    object.dataAddress = zud->weaponOffset + (uintptr_t)zud;
                    object.modelId = bladeId;
                    object.destination = actor->equipmentModels[0];
                    object.subKind = id;
                    object.textureSlot = 0xF0;
                    object.material = init->weapon.material;
                    Content_QueueActorAssetRequest(&object);
                }
                if (shieldId) {
                    object.assetKind = 4;
                    object.targetSlot = id * 2 + 1;
                    object.dataAddress = zud->shieldOffset + (uintptr_t)zud;
                    object.modelId = shieldId;
                    object.destination = actor->equipmentModels[1];
                    object.subKind = id;
                    object.textureSlot = 0xF1;
                    object.material = init->shield.material;
                    Content_QueueActorAssetRequest(&object);
                }
            }
            if (((id < 2) || ((flags & 3) == 1)) && zud->battleAnimationSize) {
                object.assetKind = 8;
                object.targetSlot = id;
                object.dataAddress = zud->battleAnimationOffset + (uintptr_t)zud;
                object.subKind = 0;
                object.modelId = actor->equippedWeaponCategory;
                Content_QueueActorAssetRequest(&object);
            }
            if (zud->commonAnimationSize) {
                object.assetKind = 8;
                object.targetSlot = id;
                object.dataAddress = zud->commonAnimationOffset + (uintptr_t)zud;
                object.subKind = 0;
                object.modelId = 0;
                Content_QueueActorAssetRequest(&object);
            }
        }

        while (Content_FindFreeActorAssetRequest()) {
            Content_UpdateActorAssetQueue();
            State_UpdatePlayTimeAndLoading(0);
        }

        Battle_actors[id] = actor;
        Actors_InitializeActorFromTemplate(id, init, bladeId, shieldId, flags, 1);
        if (actor->weaponDrawn & 1) {
            Actors_SetActorDisplayFlags(id, 0x1846);
        } else {
            Actors_SetActorDisplayFlags(id, 0x46);
        }
        Actors_PlayActorAnimation(id, 0, 0, 0);
        Actors_SetActorWalkRunSpeeds(id, actor->stats->walkSpeed << 12, actor->stats->runSpeed << 12);
        if (actor->kind & 7) {
            actor->next = Battle_actors[0]->next;
            Battle_actors[0]->next = actor;
        }
        actor->facing = spawn->facing;
        actor->spawnByte26 = spawn->reserved26;
        actor->roomSpawnIndex = spawn->index;
        actor->killCounted = 0;
        actor->defeated = 0;
        actor->stats->dropItem.id = spawn->itemId;
        actor->stats->dropItem.count = spawn->itemCount;
        actor->stats->secondDropItem.id = spawn->item2Id;
        actor->stats->secondDropItem.count = spawn->item2Count;
        actor->stats->randomDropItem.id = spawn->item3Id;
        actor->stats->randomDropItem.dropRate = spawn->item3Count;
        do {
            *(Actors_SpawnSettings*)((char*)actor + 0x30) =
                *(Actors_SpawnSettings*)spawn->settings;
        } while (0);
        actor->templateWord2C = init->opaque2C;

        if ((State_recentRoomEnemyStates[State_currentRoomStateSlot].zoneId == Field_zoneContext.zndId)
            && (State_recentRoomEnemyStates[State_currentRoomStateSlot].mapId == Field_zoneContext.mapId)
            && ((saved = Actors_FindCachedRoomActorState(spawn->index)) != NULL)) {
            actor->stats->currentHp = saved->currentHp;
            actor->stats->maxHp = saved->maxHp;
            actor->stats->currentMp = saved->currentMp;
            actor->stats->maxMp = saved->maxMp;
            actor->stats->strength = actor->stats->totalStrength = saved->strength;
            actor->stats->intelligence = actor->stats->totalIntelligence =
                saved->intelligence;
            actor->stats->agility = actor->stats->totalAgility = saved->agility;
            for (i = 0; i < 6; ++i) {
                actor->stats->limbs[i].hp = saved->limbHp[i];
            }
            Battle_InflictActorStatuses(saved->statuses, actor->stats);
            i = Battle_Ai_CreateActorAiState((void*)actor, saved->aiFlags);
        } else {
            i = Battle_Ai_CreateActorAiState((void*)actor, -1);
        }
        if (i == 0) {
            Battle_SetEnemyCombatPose(id, spawn->reserved26);
        } else {
            Battle_SetEnemyCombatPose(id, 0);
        }
        return actor;
    }
    return NULL;
}

// decomp: BATTLE.PRG func_8007C8F0 (src/BATTLE/BATTLE.PRG/146C.c:8924)
int Actors_ActorLoadProbeResult(void) { return 0; }

// decomp: BATTLE.PRG func_8007C8F8 (src/BATTLE/BATTLE.PRG/146C.c:8926)
int Actors_QueueAshleyModelLoad(int arg0)
{
    Content_ActorAssetRequest sp10;

    sp10.modelId = arg0;
    sp10.assetKind = 1;
    sp10.targetSlot = 0;
    return Content_QueueActorAssetRequest(&sp10) + 1;
}

// decomp: BATTLE.PRG func_8007C928 (src/BATTLE/BATTLE.PRG/146C.c:8936)
int Actors_QueueScriptedActorSpawn(u_int arg0, int arg1, Field_ActorPlacement* arg2)
{
    int _[6] __attribute__((unused));
    Actors_ActorAllocation* temp_v0;
    Battle_Actor** temp_s1;
    int s0;

    if (arg0 < 16) {
        if (Battle_actors[arg0] == NULL) {
            temp_s1 = &Battle_actors[arg0];
            temp_v0 = Core_Memory_AllocateFromLowEnd(sizeof *temp_v0);
            *temp_s1 = &temp_v0->core.actor.actor;
            temp_v0->core.actor.actor.stats = &temp_v0->core.actor.stats;
            temp_v0->core.actor.actor.allocationKind = 1;
            temp_v0->core.actor.actor.model = &temp_v0->core.model;
            temp_v0->core.actor.actor.equipmentModels[0] = &temp_v0->modelSlots[0];
            temp_v0->core.actor.actor.equipmentModels[1] = &temp_v0->modelSlots[1];
            Actors_InitializeActorDefaults(arg0, arg1, 0, 0, 6);
            s0 = Actors_QueueActorModelLoad(&temp_v0->core.actor.actor, arg0, arg1, arg2, 0);
            Battle_Ai_CreateActorAiState(*temp_s1, -1);
            return s0 + 1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_8007CA20 (src/BATTLE/BATTLE.PRG/146C.c:8962)
void Actors_LoadActorEquipmentModels(int actorId, int bladeWepId, int shieldWepId)
{
    Battle_Actor* temp_s0 = Battle_actors[actorId];
    if (temp_s0 != NULL) {
        Actors_QueueEquipmentModelLoad(temp_s0, actorId, bladeWepId, 0, 0);
        Actors_QueueEquipmentModelLoad(temp_s0, actorId, shieldWepId, 1, 0);
    }
}

// decomp: BATTLE.PRG func_8007CAA4 (src/BATTLE/BATTLE.PRG/146C.c:8971)
void Actors_RefreshActorEquipmentModels(int arg0)
{
    Battle_Actor* temp_s1;
    Battle_ActorStats* temp_s0;

    temp_s1 = Battle_actors[arg0];
    if (temp_s1 != NULL) {
        temp_s0 = temp_s1->stats;
        if (temp_s0 != NULL) {
            if (temp_s0->weapon.blade.id != 0) {
                Actors_QueueEquipmentModelLoad(temp_s1, arg0, temp_s0->weapon.wepId, 0,
                    temp_s0->weapon.blade.material);

            } else {
                Actors_QueueEquipmentModelLoad(temp_s1, arg0, 0, 0, 0);
            }
            if (temp_s0->shield.base.id != 0) {
                Actors_QueueEquipmentModelLoad(temp_s1, arg0, temp_s0->shield.wepId, 1,
                    temp_s0->shield.base.material);
            } else {
                Actors_QueueEquipmentModelLoad(temp_s1, arg0, 0, 0, 0);
            }
        }
    }
}

// decomp: BATTLE.PRG func_8007CCAC (src/BATTLE/BATTLE.PRG/146C.c:9039)
int Actors_FirstFreeAssetLoadSlot(void) { return Content_FindFreeActorAssetRequest(); }

// decomp: BATTLE.PRG func_8007D340 (src/BATTLE/BATTLE.PRG/146C.c:9256)
void Actors_RemoveScriptedActor(int arg0) { Actors_RemoveActorAtSlot(arg0, 0); }

