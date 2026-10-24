// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorLoading.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/CharacterInit.h"
#include "Actors/Lifecycle.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/RoomActorCache.h"
#include "Actors/RoomState.h"
#include "Actors/Skeleton.h"
#include "Actors/Spawning.h"
#include "Audio/Music.h"
#include "Audio/Seq/Channel.h"
#include "Audio/SoundBanks.h"
#include "Battle/Abilities.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/RoomState.h"
#include "Battle/Damage.h"
#include "Battle/EncounterHooks.h"
#include "Battle/Feedback.h"
#include "Battle/Hud.h"
#include "Battle/StatusEffects.h"
#include "Boot/GameLoop.h"
#include "Boot/Lifecycle.h"
#include "CdDrive.h"
#include "Core/Compatibility.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "Core/Runtime.h"
#include "DiscQueue.h"
#include "Effects/Runtime.h"
#include "Effects/Weather.h"
#include "Event/Cutscene.h"
#include "Event/ScriptVm.h"
#include "Event/Triggers.h"
#include "Field/Actors.h"
#include "Field/Background.h"
#include "Field/Camera.h"
#include "Field/Collision.h"
#include "Field/Doors.h"
#include "Field/Environment.h"
#include "Field/GameMode.h"
#include "Field/Gimmicks.h"
#include "Field/Locations.h"
#include "Field/Map.h"
#include "Field/MinimapScene.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomEffects.h"
#include "Field/RoomLoading.h"
#include "Field/RoomRendering.h"
#include "Field/RoomState.h"
#include "Field/Transitions.h"
#include "Field/Znd.h"
#include "Field/Zone.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Equipment.h"
#include "Items/EquipmentInit.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "Items/Weapons.h"
#include "Render/Camera.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/FramebufferEffects.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/Textures.h"
#include "Render/UiPrimitives.h"
#include "State/Flags.h"
#include "State/PlayTime.h"
#include "State/RoomCache.h"
#include "State/RoomLoad.h"
#include "State/RoomMemory.h"
#include "State/SceneTimer.h"
#include "State/StateFlags.h"
#include "State/Timers.h"

// decomp: _zoneContext (_zoneContext@BATTLE/BATTLE.PRG)
Field_ZoneContext Field_zoneContext;

// decomp: D_800F18C8 (D_800F18C8@BATTLE/BATTLE.PRG)
Field_RoomLoadResetBlock Field_roomLoadResetBlock;

// decomp: D_800F18E0 (D_800F18E0@BATTLE/BATTLE.PRG)
int Field_isRoomActorSpawnEnabled;

// decomp: D_800F18E4 (D_800F18E4@BATTLE/BATTLE.PRG)
void* Field_roomCleanupBuffer;

// decomp: D_800F1908 (D_800F1908@BATTLE/BATTLE.PRG)
int Field_roomResetWord;

// decomp: D_800F192C (D_800F192C@BATTLE/BATTLE.PRG)
Battle_Actor* Field_auxiliaryActor;

// decomp: vs_battle_screenTransitionAlpha (vs_battle_screenTransitionAlpha@BATTLE/BATTLE.PRG)
int Field_screenTransitionAlpha;

// decomp: D_800F1BBC (D_800F1BBC@BATTLE/BATTLE.PRG)
char Field_reservedResetByte;

// decomp: BATTLE.PRG func_80088CAC (src/BATTLE/BATTLE.PRG/146C.c:14766)
void Field_BeginSceneActorTransition(void)
{
    if (State_sceneTimer.resetScope == 2) {
        State_ResetSceneTimer();
    }
    Actors_ClearRoomActorStateCache();
}

// decomp: BATTLE.PRG _loadMpdLbas (src/BATTLE/BATTLE.PRG/146C.c:14774)
void Field_LoadZoneRoomFileTable(void* arg0, int size)
{
    if (size == 0) {
        Core_IgnoreDiagnostic(0x8B, 0);
    }
    Field_zoneContext.mpdFiles = Core_Memory_AllocateFromLowEnd(size);
    Core_Memory_CopyAlignedWords(Field_zoneContext.mpdFiles, arg0, size);
}

// decomp: BATTLE.PRG _loadZndEnemies (src/BATTLE/BATTLE.PRG/146C.c:14783)
void Field_LoadZoneEnemyTemplates(long* data, int arg1 __attribute__((unused)))
{
    Field_zoneContext.zndEnemyCount = *data;

    if (Field_zoneContext.zndEnemyCount != 0) {
        Field_zoneContext.zudFiles = Core_Memory_AllocateFromLowEnd(Field_zoneContext.zndEnemyCount * 8);
        Core_Memory_CopyAlignedWords(Field_zoneContext.zudFiles, data + 1,
            Field_zoneContext.zndEnemyCount * sizeof *Field_zoneContext.zudFiles);
        Field_zoneContext.zndEnemies = Core_Memory_AllocateFromLowEnd(
            Field_zoneContext.zndEnemyCount * sizeof *Field_zoneContext.zndEnemies);
        Core_Memory_CopyAlignedWords(Field_zoneContext.zndEnemies,
            data + ((Field_zoneContext.zndEnemyCount * 2) + 1),
            Field_zoneContext.zndEnemyCount * sizeof *Field_zoneContext.zndEnemies);
        return;
    }

    Field_zoneContext.zudFiles = NULL;
    Field_zoneContext.zndEnemies = NULL;
}

// decomp: BATTLE.PRG _loadZnd (src/BATTLE/BATTLE.PRG/146C.c:14803)
void Field_QueueZoneLoad(int id)
{
    Content_DiscFile cdFile;

    cdFile.lba = Field_legacyZoneDiscFiles[id].lba;
    cdFile.size = Field_legacyZoneDiscFiles[id].size;
    if (Field_zoneContext.zndDiscRequest != 0) {
        Core_IgnoreDiagnostic(0x87, 0);
    }
    Field_zoneContext.zndDiscRequest = Content_AllocateDiscRequest(&cdFile);
    if (Field_zoneContext.zndData != NULL) {
        Core_IgnoreDiagnostic(0x88, 0);
    }
    Field_zoneContext.zndData = Core_Memory_AllocateFromHighEnd(cdFile.size);
    Content_EnqueuePriorityDiscRequest(Field_zoneContext.zndDiscRequest, Field_zoneContext.zndData);
    if (Field_zoneContext.isMusicFadeEnabled != 0) {
        int temp_s0 = Field_zoneMusicIds[id];
        if (Audio_CurrentMusicId() != temp_s0) {
            Audio_FadeMusicSlotVolume(Audio_soundState.currentMusicSlot, 0, 0x78);
        }
    }
}

// decomp: BATTLE.PRG _finishLoadZnd (src/BATTLE/BATTLE.PRG/146C.c:14826)
void Field_FinishZoneLoad(int id)
{
    int musicId;
    int i;
    Field_ZndHeader* header;

    if (Field_zoneContext.zndDiscRequest == NULL) {
        Field_QueueZoneLoad(id);
    }

    while (Field_zoneContext.zndDiscRequest->state != Content_DiscRequestState_Loaded) {
        State_UpdatePlayTimeAndLoading(0);
    }

    Content_ReleaseDiscRequest(Field_zoneContext.zndDiscRequest);

    Field_zoneContext.zndDiscRequest = NULL;

    if (State_sceneTimer.resetScope == 1) {
        State_ResetSceneTimer();
    }

    header = Field_zoneContext.zndData;

    Field_LoadZoneEnemyTemplates((long*)(header->enemiesOffset + (intptr_t)header), header->enemiesSize);
    Field_UploadZoneTextures((long*)(header->texturesOffset + (intptr_t)header));
    Field_LoadZoneRoomFileTable((long*)(header->mpdListOffset + (intptr_t)header), header->mpdListSize);

    musicId = header->musicId;

    Core_Memory_FreeFromHighEnd(header);
    Field_zoneContext.zndData = NULL;

    if (Boot_gameEntryMode == 1) {
        State_ResetZoneScriptFlags(1);
    } else {
        State_ResetZoneScriptFlags(0);
    }

    if (Audio_CurrentMusicId() != musicId) {
        if (Audio_zoneMusicSwitchEnabled != 0) {
            Audio_StopAndFreeCurrentMusic();

            for (i = 0; i < 4;) {
                Audio_FreeMusicSlot(++i);
            }

            Field_zoneContext.musicSlot = 0;

            if (musicId != 0) {
                Audio_LoadWaveBankBlocking(musicId);
                Field_zoneContext.musicSlot = Audio_LoadMusicSlotBlocking(musicId, 3);
            }
        }
    }
    Field_zoneContext.musicId = musicId;
    Field_zoneContext.isMusicFadeEnabled = 1;
    Field_zoneContext.reserved38 = 0;
}

// decomp: BATTLE.PRG func_80089098 (src/BATTLE/BATTLE.PRG/146C.c:14886)
void Field_ReleaseZoneData(void)
{
    Event_ReleaseZoneEventResources();
    Field_ZoneCleanupHook(0);
    if (Field_zoneContext.zndEnemies != NULL) {
        Core_Memory_FreeFromLowEnd(Field_zoneContext.zndEnemies);
    }
    if (Field_zoneContext.zudFiles != NULL) {
        Core_Memory_FreeFromLowEnd(Field_zoneContext.zudFiles);
    }
    Core_Memory_FreeFromLowEnd(Field_zoneContext.mpdFiles);
}

// decomp: BATTLE.PRG func_80089104 (src/BATTLE/BATTLE.PRG/146C.c:14899)
int Field_ZoneSoundFileId(void) { return Field_zoneSoundFileId; }

// decomp: BATTLE.PRG func_80089114 (src/BATTLE/BATTLE.PRG/146C.c:14901)
void Field_SpawnRoomActors(void)
{
    int _[4] __attribute__((unused));
    int slots[16];
    Field_ActorPlacement data;
    Field_MpdEnemySpawn* enemy;
    Actors_RoomActorRecord* saved;
    Field_ZndEnemyRecord* template;
    int i;
    int visible;
    u_short param;
    int j;
    u_short slot;
    u_short id;

    if (Field_zoneContext.mpdEnemyCount == 0) {
        return;
    }

    for (j = 15; j >= 0; --j) {
        slots[j] = 0;
    }

    for (i = 0; i < Field_zoneContext.mpdEnemyCount; ++i) {
        enemy = &Field_zoneContext.mpdEnemies[i];
        slot = (enemy->kind == 0) ? enemy->slot : ((Field_MpdObjectSpawn*)enemy)->slot;

        if (slot < 16) {
            slots[slot] = 1;
        }
    }

    for (i = 0; i < Field_zoneContext.mpdEnemyCount; ++i) {
        enemy = &Field_zoneContext.mpdEnemies[i];

        if (enemy->kind == 0) {
            visible = enemy->isHidden == 0;

            switch (enemy->sceneTest) {
            case 1:
                if (State_ScriptFlag(1) != 0) {
                    visible = 0;
                }
                break;

            case 2:
                if (State_ScriptFlag(1) != 1) {
                    visible = 0;
                }
                break;

            case 3:
                if (State_ScriptFlag(2) != 1) {
                    visible = 0;
                }
                break;
            }

            switch (enemy->flagTest) {
            case 1:
                if (State_ScriptFlag(0x82) != enemy->flagValue) {
                    visible = 0;
                }
                break;

            case 2:
                if (State_ScriptFlag(0x82) > enemy->flagValue) {
                    visible = 0;
                }
                break;

            case 3:
                if (State_ScriptFlag(0x82) < enemy->flagValue) {
                    visible = 0;
                }
                break;
            }

            switch (enemy->statTest) {
            case 1:
                if ((enemy->statValue & 0x3FF)
                    < Battle_characterState->stats->currentHp) {
                    visible = 0;
                }
                break;

            case 2:
                if (Battle_characterState->stats->currentHp
                    < (enemy->statValue & 0x3FF)) {
                    visible = 0;
                }
                break;

            case 3:
                if (State_ScriptFlag(enemy->statValue & 0x3FF)
                    != (enemy->statValue >> 10)) {
                    visible = 0;
                }
                break;

            case 4:
                if (State_ScriptFlag(enemy->statValue & 0x3FF)
                    > (enemy->statValue >> 10)) {
                    visible = 0;
                }
                break;

            case 5:
                if (State_ScriptFlag(enemy->statValue & 0x3FF)
                    < (enemy->statValue >> 10)) {
                    visible = 0;
                }
                break;
            }

            id = enemy->enemyId;

            switch (enemy->facing) {
            case 0:
            case 3:
                param = 6;
                break;

            case 1:
                param = 0;
                break;

            case 2:
                param = 1;
                break;
            }
            if (enemy->actorStateByte) {
                param |= 0x80;
            }

            template = &Field_zoneContext.zndEnemies[id];
            slot = enemy->slot;

            if (slot == 0) {
                for (j = 2; j < 10; ++j) {
                    if ((Battle_actors[j] == NULL) && (slots[j] == 0)) {
                        if (Actors_usesSecondaryBodyModel[template->recordBytes[2]] != 0) {
                            if ((Battle_actors[j + 4] != NULL)
                                || (slots[j + 4] != 0)) {
                                continue;
                            }

                            slots[j + 4] = 1;
                        }

                        slot = j;
                        break;
                    }
                }
                if (j >= 10) {
                    Core_IgnoreDiagnostic(0x86, 0);

                    if (Field_zoneContext.zudData != NULL) {
                        Core_Memory_FreeFromHighEnd(Field_zoneContext.zudData);
                    }

                    Field_zoneContext.zudData = NULL;
                    Field_zoneContext.loadedZudIndexPlusOne = 0;
                    return;
                }
            } else if (Actors_usesSecondaryBodyModel[template->recordBytes[2]] != 0) {
                if ((Battle_actors[slot + 4] == NULL) && (slots[slot + 4] == 0)) {
                    slots[slot + 4] = 1;
                } else {
                    Core_IgnoreDiagnostic(0x86, 0);

                    if (Field_zoneContext.zudData != NULL) {
                        Core_Memory_FreeFromHighEnd(Field_zoneContext.zudData);
                    }

                    Field_zoneContext.zudData = NULL;
                    Field_zoneContext.loadedZudIndexPlusOne = 0;
                    return;
                }
            }

            if (visible) {
                if (id == 0x7F) {
                    data.tileX = enemy->tileX;
                    data.tileZ = enemy->tileZ;
                    data.direction = enemy->direction;
                    data.height = enemy->height * 64;
                    Actors_CreateActorWithEquipment(slot, 0x7F, 0, 0, 0, 0, &data, param);
                } else {
                    Actors_SpawnRoomEnemy(slot, enemy, (Actors_CharacterInit*)template, param);
                }
            }
        } else {
            u_int flag = State_ScriptFlag(((Field_MpdObjectSpawn*)enemy)->flag & 0x3FF);
            u_int model;
            u_int max = ((Field_MpdObjectSpawn*)enemy)->flagMax;

            if ((flag >= ((Field_MpdObjectSpawn*)enemy)->flagMin) && (flag <= max)) {
                data.tileX = ((Field_MpdObjectSpawn*)enemy)->tileX;
                data.tileZ = ((Field_MpdObjectSpawn*)enemy)->tileZ;
                data.direction = ((Field_MpdObjectSpawn*)enemy)->height;
                data.height = ((Field_MpdObjectSpawn*)enemy)->direction * 64;
                slot = ((Field_MpdObjectSpawn*)enemy)->slot;

                if (slot == 0xFF) {
                    for (j = 2; j < 16; ++j) {
                        if ((Battle_actors[j] == NULL) && (slots[j] == 0)) {
                            slot = j;
                            break;
                        }
                    }
                    if (j >= 16) {
                        Core_IgnoreDiagnostic(0x86, 0);
                        return;
                    }
                }

                param = ((Field_MpdObjectSpawn*)enemy)->model;
                model = param;
                Actors_CreateAuxiliaryActor(slot, (model >> 12) | (model & 0xF00), &data,
                    ((model >> 3) & 0x1C) | (data.height / 64), 0);
            }
        }
    }

    for (i = 0; i < 32; ++i) {
        enemy = &Field_zoneContext.mpdEnemies[i];
        saved = Actors_FindCachedRoomActorState(i);

        if (saved != NULL) {
            saved->isHidden = enemy->isHidden;
        }
    }
    if (Field_zoneContext.zudData != NULL) {
        Core_Memory_FreeFromHighEnd(Field_zoneContext.zudData);
    }

    Field_zoneContext.zudData = NULL;
    Field_zoneContext.loadedZudIndexPlusOne = 0;
}

// decomp: BATTLE.PRG func_80089888 (src/BATTLE/BATTLE.PRG/146C.c:15142)
void Field_PlaceAshleyAtRoomEntrance(void)
{
    SVECTOR sp10;
    Actors_ActorLocationQuery sp18;
    Field_ActorPlacement sp28;

    Actors_FinishActorPlacement();
    sp28.tileX = Field_pendingDestinationDoor.entryTileX;
    sp28.tileZ = Field_pendingDestinationDoor.entryTileZ;
    sp28.direction = Field_pendingDestinationDoor.tileLocation.entryDirection;
    sp28.height = Field_pendingDestinationDoor.entryElevation * 64;
    Actors_PlaceActorInRoom(0, &sp28, -0x800, 2);

    if (Field_pendingDestinationDoor.hasEntryStep) {
        switch (Field_pendingDestinationDoor.entryElevation) {
        case 0:
            sp10.vx = 0x40;
            sp10.vz = 0;
            sp10.vy = 0;
            break;
        case 1:
            sp10.vx = 0;
            sp10.vz = 0x40;
            sp10.vy = 0;
            break;
        case 2:
            sp10.vx = 0x40;
            sp10.vz = 0;
            sp10.vy = 0;
            break;
        case 3:
            sp10.vx = 0;
            sp10.vz = 0x40;
            sp10.vy = 0;
            break;
        default:
            sp10.vy = 0;
            break;
        }
        Actors_TranslateActorByAtRate(0, &sp10, 0);
    }
    Actors_ActorPlacement(0, &sp18);
    Actors_RestoreActorIdlePose(0);
    Actors_BuildActorPoseAtMatrix(0, &Render_frameScratch.viewMatrix);
    Field_SetActorCollisionBypass(0, 0);
    Battle_Ai_InitializeRoomNavigation();
}

// decomp: BATTLE.PRG func_80089A00 (src/BATTLE/BATTLE.PRG/146C.c:15190)
void Field_RestoreRoomEnemyCache(void)
{
    int j;
    int i;
    int var_s4;
    Field_MpdEnemySpawn* temp_s2;
    u_char* temp_v0;

    var_s4 = 0;

    for (j = 0; j < 4; ++j) {
        if ((State_recentRoomEnemyStates[j].recency != 0) && (State_recentRoomEnemyStates[j].zoneId == Field_zoneContext.zndId)
            && (State_recentRoomEnemyStates[j].mapId == Field_zoneContext.mapId)) {
            var_s4 = 1;
            State_currentRoomStateSlot = j;
            for (i = 0; i < Field_zoneContext.mpdEnemyCount; ++i) {
                temp_s2 = &Field_zoneContext.mpdEnemies[i];
                temp_v0 = Actors_FindCachedRoomActorState(i);
                if (temp_v0 != NULL) {
                    temp_s2->isHidden = temp_v0[1];
                }
            }
            break;
        }
    }

    if (var_s4 == 0) {
        i = 0;
        for (j = 0; j < 4; ++j) {
            if (State_recentRoomEnemyStates[j].recency < State_recentRoomEnemyStates[i].recency) {
                i = j;
            }
        }
        State_currentRoomStateSlot = i;
        Core_Memory_ZeroAlignedWords(&State_recentRoomEnemyStates[i], sizeof State_recentRoomEnemyStates[i]);
    }

    for (i = 0; i < 4; ++i) {
        if (State_recentRoomEnemyStates[State_currentRoomStateSlot].recency < State_recentRoomEnemyStates[i].recency) {
            --State_recentRoomEnemyStates[i].recency;
        }
    }

    State_recentRoomEnemyStates[State_currentRoomStateSlot].recency = 4;
    State_recentRoomEnemyStates[State_currentRoomStateSlot].zoneId = Field_zoneContext.zndId;
    State_recentRoomEnemyStates[State_currentRoomStateSlot].mapId = Field_zoneContext.mapId;
}

// decomp: BATTLE.PRG _loadMpdEnemySection (src/BATTLE/BATTLE.PRG/146C.c:15238)
void Field_LoadRoomEnemyPlacements(void* data, u_int dataLen)
{
    Boot_EncounterRoomPlacements(&data, &dataLen);
    if (dataLen != 0) {
        Field_zoneContext.mpdEnemies = Core_Memory_AllocateFromLowEnd(dataLen);
        Core_Memory_CopyAlignedWords(Field_zoneContext.mpdEnemies, data, dataLen);
        Field_zoneContext.mpdEnemyCount = dataLen / sizeof(Field_MpdEnemySpawn);
        return;
    }
    Field_zoneContext.mpdEnemies = NULL;
    Field_zoneContext.mpdEnemyCount = 0;
}

// decomp: BATTLE.PRG _loadMpd (src/BATTLE/BATTLE.PRG/146C.c:15254)
void Field_QueueRoomLoad(int id)
{
    Content_DiscFile file;

    file.lba = Field_zoneContext.mpdFiles[id].lba;
    file.size = Field_zoneContext.mpdFiles[id].size;

    if (Field_zoneContext.mpdDiscRequest != NULL) {
        Core_IgnoreDiagnostic(0x89, 0);
    }
    Field_zoneContext.mpdDiscRequest = Content_AllocateDiscRequest(&file);
    if (Field_zoneContext.mpdData != NULL) {
        Core_IgnoreDiagnostic(0x8A, 0);
    }
    Field_zoneContext.mpdData = Core_Memory_AllocateFromHighEnd(file.size);
    Content_EnqueuePriorityDiscRequest(Field_zoneContext.mpdDiscRequest, Field_zoneContext.mpdData);
}

// decomp: BATTLE.PRG func_80089DC0 (src/BATTLE/BATTLE.PRG/146C.c:15272)
void Field_FinishRoomLoad(int arg0)
{
    Boot_ResetGameplayInspection();
    VECTOR sp20;
    VECTOR lookAt;
    Actors_ActorLocationQuery sp40;
    int temp_s4;
    int var_a0;
    char* mpdHeader;
    int v0;

    if (Field_zoneContext.mpdDiscRequest == NULL) {
        Field_QueueRoomLoad(arg0);
    }

    while (Field_zoneContext.mpdDiscRequest->state != Content_DiscRequestState_Loaded) {
        State_UpdatePlayTimeAndLoading(0);
    }

    Content_ReleaseDiscRequest(Field_zoneContext.mpdDiscRequest);
    Field_zoneContext.mpdDiscRequest = NULL;

    Render_legacyPrimitiveBuffers[0] = Core_Memory_AllocateFromLowEnd(0x20000);
    Render_legacyPrimitiveBuffers[1] = Core_Memory_AllocateFromLowEnd(0x20000);

    Render_InitializeClutAnimation();

    mpdHeader = Field_zoneContext.mpdData;
    v0 = ((Field_MpdHeader*)mpdHeader)->roomSectionOffset;

    Field_LoadRoomSections(0, (void*)(v0 + (intptr_t)mpdHeader));

    Field_zoneContext.zndId = Field_pendingDestinationDoor.destinationZoneId;
    Field_zoneContext.mapId = Field_pendingDestinationDoor.destinationRoomId;

    var_a0 = Field_CurrentSceneId();

    if (var_a0 != Field_zoneContext.currentSceneId) {
        Field_zoneContext.currentSceneId = var_a0;
        Field_BeginSceneActorTransition();
    }

    if (State_sceneTimer.resetScope == 0) {
        State_ResetSceneTimer();
    }

    Field_isRoomVisitPending = 1;
    var_a0 = Field_CollisionMapDimensions(0);
    Field_zoneContext.tileColumns = (var_a0 & 0xFFFF) / 16;

    if (var_a0 < 0) {
        var_a0 += 0xFFFFF;
    }
    Field_zoneContext.tileRows = var_a0 >> 0x14;

    if (Field_RoomCameraBounds(0) != NULL) {
        Field_zoneContext.bounds = *Field_RoomCameraBounds(0);
    } else {
        Field_zoneContext.bounds.minZ = 0;
        Field_zoneContext.bounds.minX = 0;
        Field_zoneContext.bounds.maxX = Field_zoneContext.tileColumns;
        Field_zoneContext.bounds.maxZ = Field_zoneContext.tileRows;
    }

    {
        short* v0;
        Field_roomLoadResetBlock.reserved00 = 0;
        v0 = &Field_roomLoadResetBlock.reserved08;
        Field_roomLoadResetBlock.reserved02 = 0;
        Field_roomLoadResetBlock.reserved04 = 0;
        Field_roomLoadResetBlock.reserved08 = 0;
        v0[1] = 0;
        v0[2] = 0;
    }

    Render_CaptureRoomBackgroundColors();

    if (((Field_MpdHeader*)mpdHeader)->lootSectionSize != 0) {
        v0 = ((Field_MpdHeader*)mpdHeader)->lootSectionOffset;
        Items_LoadRoomChestLoot((void*)(v0 + (intptr_t)mpdHeader));
    }

    {
        u_short arg5;
        intptr_t arg2;
        int new_var;
        intptr_t arg4;
        intptr_t arg0 = ((Field_MpdHeader*)mpdHeader)->clearedSectionOffset;
        arg0 += (intptr_t)mpdHeader;
        arg2 = ((Field_MpdHeader*)mpdHeader)->scriptSectionOffset;
        arg2 += ((intptr_t)mpdHeader);
        arg4 = ((Field_MpdHeader*)mpdHeader)->doorSectionOffset;
        arg4 += ((intptr_t)mpdHeader);
        arg5 = ((Field_MpdHeader*)mpdHeader)->doorSectionSize;
        new_var = 1;
        if (!Boot_EncounterUsesRoomEvents()) {
            temp_s4 = Boot_EncounterLoadRoomEvents();
        } else {
        temp_s4 = Event_LoadRoomEventSections((void*)(arg0),
            ((Field_MpdHeader*)mpdHeader)->clearedSectionSize, (void*)(arg2),
            ((Field_MpdHeader*)mpdHeader)->scriptSectionSize, (void*)(arg4), arg5,
            Boot_gameEntryMode == new_var);
        }
    }

    if (Boot_gameEntryMode == 1) {
        Boot_gameEntryMode = 5;
    }

    v0 = ((Field_MpdHeader*)mpdHeader)->enemySectionOffset;
    Field_LoadRoomEnemyPlacements(
        (void*)(v0 + (intptr_t)mpdHeader), ((Field_MpdHeader*)mpdHeader)->enemySectionSize);

    Core_Memory_FreeFromHighEnd(mpdHeader);

    Field_roomLoadResetBlock.reserved14 = 0;

    Field_RestoreRoomEnemyCache();
    Field_PlaceAshleyAtRoomEntrance();
    Field_SpawnRoomActors();
    Field_SnapRoomActorsToSupport();
    ((void (*)(int, int, int, int))Battle_QueueActorStatusLabels)(0, 0, Battle_characterState->stats->statuses & 0x21000, 1);
    Battle_RemoveActorStatuses(0x21000, Battle_characterState->stats);

    Field_roomGeometryDrawMode = 0;
    Field_isFirstPersonViewEnabled = 0;
    Actors_ashleyMoveInput[0] = 1;
    Effects_effectBusyFlags = 0;
    Actors_SetOtherActorLogicEnabled(1);
    State_roomLoadResetWordTwo = 0;
    State_roomLoadResetWordThree = 0;
    State_roomLoadResetWordOne = 0;
    Input_buttonRepeat = 0;
    Input_releasedButtons = 0;
    Input_buttonsPressed.allButtons = 0;
    Input_previousButtons = 0;
    Input_buttonsState = 0;
    Render_frameScratch.camera.farClip = ONE;

    if (Field_screenTransitionEffect == 3) {
        Field_screenTransitionStep = 0;
        Field_screenTransitionAlpha = 0;
    } else {
        Field_screenTransitionStep = 3;
    }

    Render_SetCameraNearClip(ONE / 64);
    Render_SetCameraProjectionDistance(ONE / 8);

    Field_sphericalCamera.delta.yaw = 0;
    Field_sphericalCamera.delta.distance = 0;
    Field_sphericalCamera.values.yaw =
        Field_sphericalCamera.delta.pitch;
    Field_sphericalCamera.values.distance =
        Field_sphericalCamera.initialDistance;
    if (Field_sphericalCamera.initialDistance == (3 * ONE / 8)) {
        Field_sphericalCamera.delta.mode = 1;
    } else {
        Field_sphericalCamera.delta.mode = 3;
    }

    Field_sphericalCamera.values.pitch = -384;
    Field_sphericalCamera.zoomVelocity = 0;

    if ((Field_zoneContext.zndId == 27) && (Field_zoneContext.mapId == 1)) {
        Field_sphericalCamera.values.pitch = -128;
    }

    Field_roomResetWord = 0;

    Actors_ActorPlacement(0, &sp40);

    sp20.vx = sp40.location.position.vx * ONE;
    sp20.vy = sp40.location.position.vy * ONE - 90 * ONE;
    sp20.vz = sp40.location.position.vz * ONE;

    Field_ClampCameraLookAtToZone(&sp20, &lookAt);

    Render_frameScratch.camera.lookAt = lookAt;
    Render_frameScratch.camera.angles.vy = Field_sphericalCamera.values.yaw;

    Field_SetCameraPositionFromAngles(&Field_sphericalCamera.values);
    Actors_ActorPlacement(0, &sp40);
    Field_UpdateCameraTileConstraint(0, &sp40.location.packedTile.target);
    Render_ResetFramebufferEffect();
    Render_CommitClutBuffer();
    Render_ConfigureFieldDisplay(0x140, 0xF0, Render_projectionDistance, 0, 0, 0);

    if ((temp_s4 << 0x10) != 0) {
        Effects_InitializeRoomWeather(1);
        Event_PrepareCutsceneActors();
    } else {
        if (Event_EvaluateRoomEventTriggers(1, 0, NULL, 0) == 1) {
            Event_RequestEventStart();
            Effects_InitializeRoomWeather(1);
            Event_PrepareCutsceneActors();
        } else {
            Field_ShowPrimaryRoomGeometry();
            Actors_EnableAllActorModels();
            Effects_InitializeRoomWeather(0);
            Field_RestoreRoomEffectSoundVolume();
            Field_EnterFieldExplorationMode();
            Battle_SetHudHidden(0);
            if (Field_roomMusicSlot != 0) {
                Audio_PlayMusicSlot(Field_roomMusicSlot, 0x7F, 0);
            }
            Field_screenTransitionStep = 4;
            Render_ShowDisplay();
        }
    }
    Input_InvalidateControllerConnections();
}

// decomp: BATTLE.PRG func_8008A364 (src/BATTLE/BATTLE.PRG/146C.c:15480)
void Field_RemoveRoomActors(void)
{
    int i;

    for (i = 2; i < 16; ++i) {
        Actors_RemoveActorAtSlot(i, 0);
    }
}

// decomp: BATTLE.PRG func_8008A3A0 (src/BATTLE/BATTLE.PRG/146C.c:15489)
void Field_UnloadCurrentRoom(void)
{
    Boot_ResetGameplayInspection();
    int i;

    for (i = 0; i < 2; ++i) {
        Render_frameScratch.orderingTable = Render_legacyOrderingTables[Render_frameBuf] + 0x10;
        ClearOTagR(Render_frameScratch.orderingTable, 0x800);
        Core_NoOperation();
        Render_DrawScreenTransition(Render_frameScratch.orderingTable);
        Render_PresentFieldFrame(Render_frameScratch.orderingTable + 0x1FFC);
    }

    State_UpdatePlayTimeAndLoading(0);
    Event_FreeRoomEventSections();
    Field_MarkCurrentRoomVisited();
    Field_ReleaseOwnedRoomSections(0);
    if (Field_roomCleanupBuffer != NULL) {
        Core_Memory_FreeFromLowEnd(Field_roomCleanupBuffer);
    }
    if (Field_auxiliaryActor != NULL) {
        Battle_Ai_FreeActorAiState(Field_auxiliaryActor);
    }
    Field_RemoveRoomActors();
    Battle_Ai_FreeRoomAiState();
    Battle_SetActorMarker(0, 0);
    Actors_ResetAllActorMovement();
    Items_ClearRoomLootList();
    Core_Memory_FreeFromLowEnd(Render_legacyPrimitiveBuffers[1]);
    Core_Memory_FreeFromLowEnd(Render_legacyPrimitiveBuffers[0]);
}

// decomp: BATTLE.PRG _loadZndTims (src/BATTLE/BATTLE.PRG/146C.c:15608)
void Field_UploadZoneTextures(void* data)
{
    TIM_IMAGE tim;
    u_long* timPtrs[4];
    int numTims;
    int i;
    u_long* dataPtr = data;
    int byteOffset = 0;
    int* sizePtr = (int*)dataPtr;
    u_long** ptrTableEntry = timPtrs;

    do {
        if (*sizePtr > 0) {
            *ptrTableEntry = dataPtr + ((byteOffset / 4) + 4);
        } else {
            *ptrTableEntry = NULL;
        }
        byteOffset += *sizePtr;
        ++sizePtr;
        ++ptrTableEntry;
    } while ((intptr_t)sizePtr < ((intptr_t)data + 0x10));

    dataPtr = timPtrs[0];
    numTims = *dataPtr++;

    for (i = 0; i < numTims; ++i) {
        byteOffset = *dataPtr++;

        Render_ParseTextureImage(dataPtr, &tim);

        if (tim.paddr != NULL) {
            if (tim.prect->x >= 768) {
                int line = tim.prect->y;
                u_short* pixels = (u_short*)tim.paddr;

                while (line < (tim.prect->y + tim.prect->h)) {
                    u_int clutIndex = line - 224;
                    if (clutIndex < 4) {
                        Field_CopyZonePaletteRow(
                            pixels, clutIndex, tim.prect->x - 768, tim.prect->w);
                    }
                    ++line;
                    pixels += tim.prect->w;
                }
            } else {
                tim.prect->x -= 320;
                tim.prect->y += 256;
                LoadImage(tim.prect, tim.paddr);
            }
        }
        dataPtr += byteOffset >> 2;
    }
}

// decomp: BATTLE.PRG _loadMpdRoomSection (src/BATTLE/BATTLE.PRG/146C.c:15663)
int Field_LoadRoomSections(int arg0, void* data)
{
    int i;
    void* temp_v0;
    Field_RoomRecord* var_s0;
    Field_MpdSectionE* sectionE;
    u_int dataSz;

    Field_isRoomTintActive = 0;

    for (i = 0, dataSz = 0; i < 24; ++i) {
        dataSz += ((int*)data)[i];
    }

    if (dataSz == 0) {
        return 0;
    }

    Core_Memory_CopyAlignedWords(&Field_roomData.header, data, 0x60);
    temp_v0 = Core_Memory_AllocateFromLowEnd(dataSz);
    Core_Memory_CopyAlignedWords(temp_v0, data + 0x60, dataSz);

    dataSz = 0;
    var_s0 = &Field_roomData;

    for (i = 0; i < 24; i++) {
        if (((int*)data)[i] > 0) {
            (&var_s0->geometry)[i] = (void*)(temp_v0 + dataSz);
        } else {
            (&var_s0->geometry)[i] = NULL;
        }
        dataSz += ((int*)data)[i];
    }

    Field_roomData.isLoaded = 1;
    Field_roomData.displayFlag = 0;
    Field_roomData.roomIndex = arg0;

    if (Field_roomData.geometry != NULL) {
        Field_OffsetRoomTextureIds(11, Field_roomData.geometry);
    }

    sectionE = Field_roomData.sectionE;
    if (sectionE != NULL) {
        sectionE->firstArray = sizeof(*sectionE);
        if (sectionE->hasSecondArray > 0) {
            sectionE->secondArray = sizeof(*sectionE);
            sectionE->secondArray += (sectionE->secondArrayOffset >> 2) * sizeof(int);
        } else {
            sectionE->secondArray = 0;
        }
    }

    if (Field_roomData.collisionMap != NULL) {
        Field_roomData.collisionMap->cellCount =
            Field_roomData.collisionMap->width
            * Field_roomData.collisionMap->height;
    }

    if (Field_roomData.minimapScene != NULL) {
        Field_ResolveSceneRoomData(Field_roomData.minimapScene, Field_roomData.header.minimapSceneLength);
        Field_RestoreSceneRoomVisits(Field_roomData.minimapScene);
        Field_roomData.minimapScene->rooms[0].visited = 1;
        if (Audio_Seq_unusedStateFlags.doorEntered == 0) {
            Field_MarkEntranceDoorOnSceneMap(Field_enteredDoorId);
        } else {
            Field_MarkEntranceDoorOnSceneMap(Audio_Seq_unusedStateFlags.doorEntered - 1);
            Audio_Seq_unusedStateFlags.doorEntered = 0;
        }
    }

    Field_ResetGimmickAnimationQueue();
    Field_InitializeRoomEffects();
    Items_RestoreRoomChestStates();
    Field_InitializeMovingPlatforms();
    Battle_ResetFloatingLabels();
    Effects_AllocateWeatherParticles();
    Field_InitializeRoomBackgroundColors();
    Field_UploadRoomPalettes();

    Field_reservedResetByte = 0;

    if (Field_roomData.sectionE != NULL) {
        Boot_WaitForOverlayReady();
    }
    return 1;
}

// decomp: BATTLE.PRG func_8008ABF0 (src/BATTLE/BATTLE.PRG/146C.c:15774)
int Field_ReleaseOwnedRoomSections(int arg0)
{
    if (Field_roomData.isLoaded == 1) {
        if (Field_roomData.roomIndex == arg0) {
            Field_ReleaseTiledBackground();
            Field_ReleaseWeatherParticles();
            ((void (*)(void))Field_ReleaseRoomEffects)();
            if (Field_roomData.geometry != 0) {
                Core_Memory_FreeFromLowEnd((void*)Field_roomData.geometry);
            }
            Core_Memory_ZeroAlignedWords(&Field_roomData, sizeof(Field_RoomRecord));
            return 1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG _vsBattleNop2 (src/BATTLE/BATTLE.PRG/146C.c:16031)
void Field_ZoneCleanupHook(int arg0 __attribute__((unused))) { }

// decomp: BATTLE.PRG func_8008B6FC (src/BATTLE/BATTLE.PRG/146C.c:16061)
intptr_t Field_RoomAuxiliaryData(void) { return Field_roomData.section6; }

// decomp: BATTLE.PRG func_8008C478 (src/BATTLE/BATTLE.PRG/146C.c:16467)
intptr_t Field_RoomSupplementalRecord(int arg0)
{
    if (Field_roomData.section7 != 0) {
        return Field_roomData.section7 + (arg0 * 8);
    }
    return 0;
}

// decomp: BATTLE.PRG vs_battle_inWorkshop (src/BATTLE/BATTLE.PRG/146C.c:16798)
int Field_IsCurrentRoomWorkshop(void) { return Field_roomData.environment->isWorkshop; }

// decomp: BATTLE.PRG func_8008D3E8 (src/BATTLE/BATTLE.PRG/146C.c:16800)
int Field_RoomEnvironmentFlags(void) { return Field_roomData.environment->environmentFlags; }

// decomp: BATTLE.PRG vs_battle_getCurrentSceneId (src/BATTLE/BATTLE.PRG/146C.c:16802)
int Field_CurrentSceneId(void) { return Field_roomData.environment->sceneId; }

// decomp: BATTLE.PRG func_8008D418 (src/BATTLE/BATTLE.PRG/146C.c:16804)
int Field_RoomFallDestination(void)
{
    return (Field_roomData.environment->fallDestinationZone << 0x10)
         | Field_roomData.environment->fallDestinationRoom;
}

// decomp: BATTLE.PRG func_8008E470 (src/BATTLE/BATTLE.PRG/146C.c:17321)
int Field_IsRoomAudioSequenceAbsent(void) { return (uintptr_t)Field_roomData.soundData < 1; }

// decomp: BATTLE.PRG func_8008E4AC (src/BATTLE/BATTLE.PRG/146C.c:17329)
short Field_RoomEnvironmentParameter(void) { return Field_roomData.environment->opaque56; }

// decomp: BATTLE.PRG _getRoomTrapLevel (src/BATTLE/BATTLE.PRG/146C.c:17331)
int Field_RoomTrapLevel(void) { return Field_roomData.environment->trapLevel; }

