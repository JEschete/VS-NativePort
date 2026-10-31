// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ActorLoading.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/CharacterInit.h"
#include "Actors/Lifecycle.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/RoomActorCache.h"
#include "Actors/RoomState.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/Spawning.h"
#include "Actors/TextureAnimation.h"
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
#include "Core/Memory/WordCopy.h"
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
    int index, int actorId, Field_ActorPlacement* placement, int material, int arg4)
{
    Content_ActorAssetRequest objData;
    int i;
    int texture_slot_dividend;
    Actors_ActorCore* actor_core;

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
        actor_core = (Actors_ActorCore *)(Core_Memory_AllocateFromLowEnd(sizeof *actor_core));
        Core_Memory_ZeroAlignedWords(actor_core, sizeof actor_core->actor);
        actor_core->actor.actor.stats = &actor_core->actor.stats;
        actor_core->actor.actor.model = &actor_core->model;
        actor_core->actor.actor.modelId = 0xFE;
        actor_core->actor.actor.kind = 0x10;
        actor_core->actor.actor.allocationKind = 0;
        actor_core->actor.actor.id = index;
        actor_core->actor.actor.next = NULL;
        actor_core->actor.actor.retainAfterDefeat = 0;
        objData.assetKind = 6;
        objData.targetSlot = index;
        objData.placement = *placement;
        objData.placement.direction &= 1;
        texture_slot_dividend = actorId;
        objData.destination = actor_core->actor.actor.model;
        objData.subKind = actorId;
        if (actorId < 0) {
            texture_slot_dividend = actorId + 0xFF;
        }
        objData.textureSlot = texture_slot_dividend >> 8;
        objData.material = material;
        objData.variant = placement->direction >> 4;

        Content_QueueActorAssetRequest(&objData);

        while (Content_FindFreeActorAssetRequest() != 0) {
            Content_UpdateActorAssetQueue();
            State_UpdatePlayTimeAndLoading(0);
        }

        Battle_actors[index] = &actor_core->actor.actor;

        Actors_InitializePlaceholderActorStats(index, actorId);
        Battle_Ai_CreateActorAiState((Battle_Ai_AiActorView *)(&actor_core->actor.actor), -1);
        Actors_SetActorDisplayFlags(index, 0x1846);

        return &actor_core->actor.actor;
    }
    return NULL;
}

// decomp: BATTLE.PRG func_80076784 (src/BATTLE/BATTLE.PRG/146C.c:6128)
void Actors_ApplyActorInitialStats(
    int actor_id, Battle_ActorStats* actor, Actors_CharacterInit* init, int randomize_stats)
{
    int j;
    int i;
    short initial_stat_value;

    actor->actorId = actor_id;

    for (i = 0; i < 24; ++i) {
        actor->name[i] = init->name[i];
    }

    initial_stat_value = init->hp;
    if (randomize_stats != 0) {
        initial_stat_value += -10 + Core_Random_NextAveraged(21);
    }
    if (initial_stat_value < 0) {
        initial_stat_value = 0;
    } else if (initial_stat_value > 999) {
        initial_stat_value = 999;
    }

    actor->maxHp = initial_stat_value;
    actor->currentHp = initial_stat_value;
    actor->risk = 0;

    initial_stat_value = init->mp;
    if (initial_stat_value != 0) {
        if (randomize_stats != 0) {
            initial_stat_value += -10 + Core_Random_NextAveraged(21);
        }
        if (initial_stat_value < 0) {
            initial_stat_value = 0;
        } else if (initial_stat_value > 999) {
            initial_stat_value = 999;
        }
    }

    actor->maxMp = initial_stat_value;
    actor->currentMp = initial_stat_value;

    initial_stat_value = init->strength;
    if (randomize_stats != 0) {
        initial_stat_value += -10 + Core_Random_NextAveraged(21);
    }
    if (initial_stat_value < 0) {
        initial_stat_value = 0;
    } else if (initial_stat_value > 999) {
        initial_stat_value = 999;
    }

    actor->totalStrength = initial_stat_value;
    actor->strength = initial_stat_value;

    initial_stat_value = init->intelligence;
    if (randomize_stats != 0) {
        initial_stat_value += -10 + Core_Random_NextAveraged(21);
    }
    if (initial_stat_value < 0) {
        initial_stat_value = 0;
    } else if (initial_stat_value > 999) {
        initial_stat_value = 999;
    }

    actor->totalIntelligence = initial_stat_value;
    actor->intelligence = initial_stat_value;

    initial_stat_value = init->agility;
    if (randomize_stats != 0) {
        initial_stat_value += -10 + Core_Random_NextAveraged(21);
    }
    if (initial_stat_value < 0) {
        initial_stat_value = 0;
    } else if (initial_stat_value > 999) {
        initial_stat_value = 999;
    }

    actor->totalAgility = initial_stat_value;
    actor->agility = initial_stat_value;
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

        j = Actors_FindActorBoneTag(actor_id, i, 2);

        if (j >= 0) {
            actor->limbs[i].anchorBone = j;
            actor->limbs[i].nameIndex = Actors_ActorBoneTag(actor_id, j, 0);
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
void Actors_InitializeActorDefaults(u_int id, int model_id, int blade_id, int shield_id, int spawn_flags)
{
    Battle_Actor* actor = Battle_actors[id];
    Battle_ActorStats* stats = actor->stats;

    actor->id = id;
    actor->modelId = model_id;
    actor->subType = 0;
    actor->bladeId = blade_id;
    actor->shieldId = shield_id;
    actor->weaponDrawn = 0;

    switch (id) {
    case 0:
        actor->kind = 1;
        break;
    case 1:
        actor->kind = 2;
        break;
    default:
        switch (spawn_flags & 3) {
        case 1:
            actor->kind = 4;
            actor->weaponDrawn |= 1;
            Actors_SetActorWeaponDrawn(id, 1, 0);
            break;
        case 0:
            actor->kind = 2;
            break;
        case 2:
        case 3:
            actor->kind = 8;
            break;
        }
    }

    actor->interactionFlags = spawn_flags & 0x1C;
    actor->lastTriggerTile.bytes[3] = 0;
    actor->lastTriggerTile.bytes[2] = 0;
    actor->lastTriggerTile.bytes[1] = 0;
    actor->lastTriggerTile.bytes[0] = 0;
    actor->facing = 0;
    actor->commandState = 0;
    stats->maxHp = 100;
    stats->currentHp = 100;
    stats->maxMp = 100;
    stats->currentMp = 100;
    stats->risk = 0;
    stats->actionRecoveryTimer = 0;

    if (model_id == 41) {
        stats->movementClass = 16;
        stats->gaitClass = 32;
    } else if (model_id == 48) {
        stats->movementClass = 2;
        stats->gaitClass = 8;
    } else {
        stats->movementClass = 2;
        stats->gaitClass = 4;
    }

    stats->walkSpeed = 9;
    stats->runSpeed = 18;
    stats->weapon.recoveryDelay = 20;
    stats->weapon.range.range = 5;
    stats->weapon.range.secondRange = 4;
    stats->weapon.range.thirdRange = 5;
    stats->cancelsHitResults = 0;
    stats->weapon.range.attackShapeAngle = 0;
    actor->roomSpawnIndex = 128;
    actor->killCounted = 0;
    actor->defeated = 0;
    stats->actorId = 128;
}

// decomp: BATTLE.PRG func_80076F24 (src/BATTLE/BATTLE.PRG/146C.c:6367)
void Actors_InitializeActorFromTemplate(
    int id, Actors_CharacterInit* character_init, int blade_id, int shield_id, int spawn_flags, int randomize_stats)
{
    int actor_kind;
    Battle_Actor* actor;
    Battle_ActorStats* stats;

    actor = Battle_actors[id];
    actor->id = id;
    stats = actor->stats;
    actor->modelId = character_init->characterKind;
    actor->subType = character_init->subType;
    actor->bladeId = blade_id;
    actor->shieldId = shield_id;
    actor->weaponDrawn = 0;

    switch (id) {
    case 0:
        actor->kind = 1;
        break;
    case 1:
        actor_kind = 2;
        actor->kind = actor_kind;
        break;
    default:
        switch (spawn_flags & 3) {
        case 1:
            actor->kind = 4;
            actor->weaponDrawn |= 1;
            Actors_SetActorWeaponDrawn(id, 1, 0);
            break;
        case 2:
        case 3:
            actor->kind = 8;
            break;
        case 0:
            actor->kind = 2;
            break;
        }
    }

    if (spawn_flags & 0x80) {
        actor->retainAfterDefeat = 1;
    } else {
        actor->retainAfterDefeat = 0;
    }

    actor->interactionFlags = spawn_flags & 0x1C;
    actor->commandState = 0;
    actor->lastTriggerTile.bytes[3] = 0;
    actor->lastTriggerTile.bytes[2] = 0;
    actor->lastTriggerTile.bytes[1] = 0;
    actor->lastTriggerTile.bytes[0] = 0;
    Actors_ApplyActorInitialStats(id, stats, character_init, randomize_stats);
}

// decomp: BATTLE.PRG func_80077078 (src/BATTLE/BATTLE.PRG/146C.c:6423)
int Actors_QueueActorModelLoad(Battle_Actor* actor, int index, int wepId,
    Field_ActorPlacement* placement, int load_flags)
{
    Content_ActorAssetRequest asset_request;

    asset_request.assetKind = 1;
    asset_request.targetSlot = index;
    asset_request.modelId = wepId;
    asset_request.placement = *placement;
    asset_request.destination = actor->model;
    if (load_flags & 0x80) {
        asset_request.subKind = (load_flags & 0xF00) >> 8;
        asset_request.textureSlot = (load_flags & 0xF000) >> 0xC;
    } else {
        asset_request.subKind = 0xFF;
    }
    asset_request.variant = 0;
    asset_request.material = 2;
    return Content_QueueActorAssetRequest(&asset_request);
}

// decomp: BATTLE.PRG func_80077130 (src/BATTLE/BATTLE.PRG/146C.c:6455)
void Actors_QueueEquipmentModelLoad(
    Battle_Actor* actor, int actorId, int wepId, int isShield, int material)
{
    Content_ActorAssetRequest asset_request;

    if (wepId != 0xFF) {
        int equipment_slot = (actorId * 2) + isShield;
        Content_ReleaseWeaponModel(equipment_slot);
        if (wepId != 0) {
            asset_request.assetKind = 3;
            asset_request.targetSlot = equipment_slot;
            asset_request.modelId = wepId;
            asset_request.destination = actor->equipmentModels[isShield];
            asset_request.subKind = actorId;
            if (isShield == 0) {
                asset_request.textureSlot = 0xF0;
            } else {
                asset_request.textureSlot = 0xF1;
            }
            asset_request.material = material;
            Content_QueueActorAssetRequest(&asset_request);
        }
    }
}

// decomp: BATTLE.PRG func_80077240 (src/BATTLE/BATTLE.PRG/146C.c:6493)
Actors_ActorAllocation* Actors_AllocateActorWithEquipment(int actor_id, int model_id, int bladeWepId, int bladeMaterial,
    int shieldWepId, int shieldMaterial, Field_ActorPlacement* placement, int load_flags)
{
    int i;
    Actors_ActorAllocation* allocation;
    int _[6] __attribute__((unused));

    if (Battle_actors[actor_id] != NULL) {
        return NULL;
    }
    if (actor_id < 0xA) {
        allocation = (Actors_ActorAllocation *)(Core_Memory_AllocateFromLowEnd(sizeof *allocation));
        Core_Memory_ZeroAlignedWords(allocation, sizeof allocation->core.actor);
        i = 0xA;
        allocation->core.actor.actor.stats = &allocation->core.actor.stats;
        allocation->core.actor.actor.allocationKind = 1;
        allocation->core.actor.actor.model = &allocation->core.model;
        allocation->core.actor.actor.equipmentModels[0] = &allocation->modelSlots[0];
        allocation->core.actor.actor.equipmentModels[1] = &allocation->modelSlots[1];

        if (bladeWepId != 0) {
            for (i = 1; i < 11; ++i) {
                if (Items_weaponCategoryUpperIds[i] >= bladeWepId) {
                    break;
                }
            }
        }

        allocation->core.actor.actor.equippedWeaponCategory = i;

        Actors_QueueActorModelLoad(&allocation->core.actor.actor, actor_id, model_id, placement, load_flags);
        if (model_id != 0x7F) {
            Actors_QueueActorBaseSequenceLoad(&allocation->core.actor.actor, actor_id);
            Actors_QueueEquipmentModelLoad(&allocation->core.actor.actor, actor_id, bladeWepId, 0, bladeMaterial);
            Actors_QueueEquipmentModelLoad(&allocation->core.actor.actor, actor_id, shieldWepId, 1, shieldMaterial);
            Actors_QueueActorWeaponSequenceLoad((char*)&allocation->core, actor_id,
                allocation->core.actor.actor.equippedWeaponCategory, load_flags);
        }
        return allocation;
    }
    return NULL;
}

// decomp: BATTLE.PRG func_800773BC (src/BATTLE/BATTLE.PRG/146C.c:6536)
void Actors_RegisterLoadedActor(
    Battle_Actor* actor, int actor_id, int model_id, int blade_id, int shield_id, int spawn_flags)
{
    Actors_PlayActorAnimation(actor_id, 0, 0, 0);
    Actors_SetActorDisplayFlags(actor_id, 0x1847);
    Battle_actors[actor_id] = actor;
    Actors_InitializeActorDefaults(actor_id, model_id, blade_id, shield_id, spawn_flags);
    Actors_SetActorWalkRunSpeeds(actor_id, 0x9000, 0x12000);
    Battle_SetEnemyCombatPose(actor_id, 0);
    if (actor->kind & 7) {
        if (actor_id != 0) {
            actor->next = Battle_actors[0]->next;
            Battle_actors[0]->next = actor;
        } else {
            actor->next = NULL;
        }
    }
    if (actor->weaponDrawn & 1) {
        Actors_SetActorDisplayFlags(actor_id, 0x1846);
    } else {
        Actors_SetActorDisplayFlags(actor_id, 0x46);
    }
    actor->templateWord2C = 0;
    Battle_Ai_CreateActorAiState((Battle_Ai_AiActorView *)(actor), -1);
}

// decomp: BATTLE.PRG func_800774FC (src/BATTLE/BATTLE.PRG/146C.c:6562)
Battle_Actor* Actors_CreateActorWithEquipment(int actor_id, int model_id, int bladeWepId, int bladeMaterial,
    int shieldWepId, int shieldMaterial, Field_ActorPlacement* placement, int spawn_flags)
{
    Battle_Actor* actor = (Battle_Actor*)Actors_AllocateActorWithEquipment(
        actor_id, model_id, bladeWepId, bladeMaterial, shieldWepId, shieldMaterial, placement, spawn_flags);
    if (actor != NULL) {
        while (Content_FindFreeActorAssetRequest() != 0) {
            Content_UpdateActorAssetQueue();
            State_UpdatePlayTimeAndLoading(0);
        }
        Actors_RegisterLoadedActor(actor, actor_id, model_id, bladeWepId, shieldWepId, spawn_flags);
        return actor;
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
            actor = (Battle_Actor *)(Core_Memory_AllocateFromLowEnd(offsetof(Actors_ActorAllocation, modelSlots) + sizeof(Actors_ActorRecord)));
            actor->stats = (Battle_ActorStats*)(actor + 1);
            actor->allocationKind = 2;
            actor->model = (Actors_ModelInstance *)((void*)&((Actors_ActorAllocation*)actor)->core.model);
            actor->equipmentModels[0] = (Actors_ActorModelSlot *)((void*)((Actors_ActorAllocation*)actor)->modelSlots);
        } else {
            actor = (Battle_Actor *)(Core_Memory_AllocateFromLowEnd(sizeof(Actors_ActorAllocation)));
            actor->stats = (Battle_ActorStats*)(actor + 1);
            actor->allocationKind = 1;
            actor->model = (Actors_ModelInstance *)((void*)&((Actors_ActorAllocation*)actor)->core.model);
            actor->equipmentModels[0] = (Actors_ActorModelSlot *)((void*)((Actors_ActorAllocation*)actor)->modelSlots);
            actor->equipmentModels[1] = (Actors_ActorModelSlot *)((void*)&((Actors_ActorAllocation*)actor)->modelSlots[1]);
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

        zud = (Actors_ZudHeader *)(Field_zoneContext.zudData);
        object.assetKind = 2;
        object.targetSlot = id;
        object.dataAddress = zud->characterShapeOffset + (uintptr_t)zud;
        object.modelId = (u_char)Field_zoneContext.zndEnemies[zudIndex].recordBytes[2];
        if ((State_recentRoomEnemyStates[State_currentRoomStateSlot].zoneId == Field_zoneContext.zndId)
            && (State_recentRoomEnemyStates[State_currentRoomStateSlot].mapId == Field_zoneContext.mapId)
            && ((saved = (Actors_RoomActorRecord *)(Actors_FindCachedRoomActorState(spawn->index))) != NULL)) {
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
                object.modelId = (u_char)Field_zoneContext.zndEnemies[zudIndex].recordBytes[2] + 1;
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
            *(Actors_SpawnSettings*)((char*)&actor->reserved34 - 4) =
                *(Actors_SpawnSettings*)spawn->settings;
        } while (0);
        actor->templateWord2C = init->opaque2C;

        if ((State_recentRoomEnemyStates[State_currentRoomStateSlot].zoneId == Field_zoneContext.zndId)
            && (State_recentRoomEnemyStates[State_currentRoomStateSlot].mapId == Field_zoneContext.mapId)
            && ((saved = (Actors_RoomActorRecord *)(Actors_FindCachedRoomActorState(spawn->index))) != NULL)) {
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
            i = Battle_Ai_CreateActorAiState((Battle_Ai_AiActorView *)((void*)actor), saved->aiFlags);
        } else {
            i = Battle_Ai_CreateActorAiState((Battle_Ai_AiActorView *)((void*)actor), -1);
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
int Actors_QueueAshleyModelLoad(int model_id)
{
    Content_ActorAssetRequest asset_request;

    asset_request.modelId = model_id;
    asset_request.assetKind = 1;
    asset_request.targetSlot = 0;
    return Content_QueueActorAssetRequest(&asset_request) + 1;
}

// decomp: BATTLE.PRG func_8007C928 (src/BATTLE/BATTLE.PRG/146C.c:8936)
int Actors_QueueScriptedActorSpawn(u_int actor_id, int model_id, Field_ActorPlacement* placement)
{
    int _[6] __attribute__((unused));
    Actors_ActorAllocation* allocation;
    Battle_Actor** actor_slot;
    int load_request_index;

    Core_Debugf("actors", "script spawn actor=%u model=%d existing=%p", actor_id, model_id,
        actor_id < 16 ? (void*)Battle_actors[actor_id] : NULL);
    if (actor_id < 16) {
        if (Battle_actors[actor_id] == NULL) {
            actor_slot = &Battle_actors[actor_id];
            allocation = (Actors_ActorAllocation *)(Core_Memory_AllocateFromLowEnd(sizeof *allocation));
            *actor_slot = &allocation->core.actor.actor;
            allocation->core.actor.actor.stats = &allocation->core.actor.stats;
            allocation->core.actor.actor.allocationKind = 1;
            allocation->core.actor.actor.model = &allocation->core.model;
            allocation->core.actor.actor.equipmentModels[0] = &allocation->modelSlots[0];
            allocation->core.actor.actor.equipmentModels[1] = &allocation->modelSlots[1];
            Actors_InitializeActorDefaults(actor_id, model_id, 0, 0, 6);
            load_request_index = Actors_QueueActorModelLoad(&allocation->core.actor.actor, actor_id, model_id, placement, 0);
            Battle_Ai_CreateActorAiState((Battle_Ai_AiActorView *)(*actor_slot), -1);
            return load_request_index + 1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_8007CA20 (src/BATTLE/BATTLE.PRG/146C.c:8962)
void Actors_LoadActorEquipmentModels(int actorId, int bladeWepId, int shieldWepId)
{
    Battle_Actor* actor = Battle_actors[actorId];
    if (actor != NULL) {
        Actors_QueueEquipmentModelLoad(actor, actorId, bladeWepId, 0, 0);
        Actors_QueueEquipmentModelLoad(actor, actorId, shieldWepId, 1, 0);
    }
}

// decomp: BATTLE.PRG func_8007CAA4 (src/BATTLE/BATTLE.PRG/146C.c:8971)
void Actors_RefreshActorEquipmentModels(int actor_id)
{
    Battle_Actor* actor;
    Battle_ActorStats* stats;

    actor = Battle_actors[actor_id];
    if (actor != NULL) {
        stats = actor->stats;
        if (stats != NULL) {
            if (stats->weapon.blade.id != 0) {
                Actors_QueueEquipmentModelLoad(actor, actor_id, stats->weapon.wepId, 0,
                    stats->weapon.blade.material);

            } else {
                Actors_QueueEquipmentModelLoad(actor, actor_id, 0, 0, 0);
            }
            if (stats->shield.base.id != 0) {
                Actors_QueueEquipmentModelLoad(actor, actor_id, stats->shield.wepId, 1,
                    stats->shield.base.material);
            } else {
                Actors_QueueEquipmentModelLoad(actor, actor_id, 0, 0, 0);
            }
        }
    }
}

// decomp: BATTLE.PRG func_8007CCAC (src/BATTLE/BATTLE.PRG/146C.c:9039)
int Actors_FirstFreeAssetLoadSlot(void) { return Content_FindFreeActorAssetRequest(); }

// decomp: BATTLE.PRG func_8007D340 (src/BATTLE/BATTLE.PRG/146C.c:9256)
void Actors_RemoveScriptedActor(int actor_id) { Actors_RemoveActorAtSlot(actor_id, 0); }

}  // extern "C"
