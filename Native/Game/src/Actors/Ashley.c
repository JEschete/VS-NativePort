// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/ActorLoading.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/Animation.h"
#include "Actors/Ashley.h"
#include "Actors/CharacterInit.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Actors/Spawning.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Abilities.h"
#include "Battle/Actor.h"
#include "Battle/Hud.h"
#include "Battle/StatusEffects.h"
#include "Core/Callbacks.h"
#include "Field/Actors.h"
#include "Field/Camera.h"
#include "Items/Equipment.h"
#include "Items/EquipmentInit.h"
#include "Items/EquipmentStats.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"
#include "State/Ashley.h"
#include "State/PlayTime.h"

// decomp: INITBTL.PRG func_800F9AB0 (src/BATTLE/INITBTL.PRG/18.c:304)
void Actors_AshleySpawnCompletionHook(void) { }

// decomp: INITBTL.PRG func_800F9AB8 (src/BATTLE/INITBTL.PRG/18.c:306)
int Actors_AshleyModelLoadQueueSlot(void) { return Content_FindFreeActorAssetRequest(); }

// decomp: INITBTL.PRG func_800F9AD8 (src/BATTLE/INITBTL.PRG/18.c:310)
void Actors_RestoreAshleyRuntimeState(void)
{
    int j;
    int i;
    Battle_ActorStats* temp_s0 = Battle_characterState->stats;
    State_AshleyStatsSnapshot* s1 = &State_ashleySnapshot.stats;

    temp_s0->currentHp = s1->currentHp;
    temp_s0->maxHp = s1->maxHp;
    temp_s0->currentMp = s1->currentMp;
    temp_s0->maxMp = s1->maxMp;
    temp_s0->strength = s1->strength;
    temp_s0->totalStrength = s1->totalStrength;
    temp_s0->intelligence = s1->intelligence;
    temp_s0->totalIntelligence = s1->totalIntelligence;
    temp_s0->agility = s1->agility;
    temp_s0->totalAgility = s1->totalAgility;
    temp_s0->risk = s1->risk;
    temp_s0->actionRecoveryTimer = s1->actorFlags;

    Battle_InflictActorStatuses(s1->statuses, temp_s0);

    for (i = 0; i < 8; ++i) {
        temp_s0->statusTimers[i] = s1->statusTimers[i];
    }

    for (i = 0; i < 6; ++i) {
        temp_s0->limbs[i].hp = s1->limbs[i].hp;
        temp_s0->limbs[i].maxHp = s1->limbs[i].maxHp;

        for (j = 0; j < 4; ++j) {
            temp_s0->limbs[i].types[j] = s1->limbs[i].types[j];
        }

        for (j = 0; j < 8; ++j) {
            temp_s0->limbs[i].affinities[j] = s1->limbs[i].affinities[j];
        }
    }

    Battle_characterState->weaponDrawn = s1->weaponDrawn;

    if (s1->weaponDrawn & 1) {
        Actors_SetActorWeaponDrawn(0, 1, 2);
        Battle_SetHudBattleMode(1);
    }

    State_ashleySnapshot.stats.isPending = 0;
    Field_sphericalCamera.delta.pitch = s1->cameraPitch;
    Field_sphericalCamera.initialDistance = s1->cameraDistance;
}

// decomp: INITBTL.PRG func_800F9CCC (src/BATTLE/INITBTL.PRG/18.c:361)
void Actors_SpawnAshleyWithEquipment(void)
{
    int i;
    int bladeIndex;
    int shieldIndex;
    int j;
    int shieldWepId;
    int bladeWepId;
    int bladeMaterial = 0;
    int shieldMaterial = 0;

    Field_ActorPlacement sp20;

    sp20.tileX = 4;
    sp20.tileZ = 2;
    sp20.direction = 0;
    sp20.height = 128;

    bladeWepId = 0;
    bladeIndex = 0;

    for (i = 0; i < 8; ++i) {
        if (Audio_Seq_unusedInventory.weapons[i].isEquipped != 0) {
            bladeIndex = i + 1;
            bladeWepId =
                Audio_Seq_unusedInventory.blades[Audio_Seq_unusedInventory.weapons[i].blade - 1].wepId;
            bladeMaterial =
                Audio_Seq_unusedInventory.blades[Audio_Seq_unusedInventory.weapons[i].blade - 1].material;
        }
    }

    shieldWepId = 0;
    shieldIndex = 0;

    for (i = 0; i < 8; ++i) {
        if (Audio_Seq_unusedInventory.shields[i].isEquipped != 0) {
            shieldIndex = i + 1;
            shieldWepId = Audio_Seq_unusedInventory.shields[i].base.wepId;
            shieldMaterial = Audio_Seq_unusedInventory.shields[i].base.material;
        }
    }

    Battle_characterState = Actors_CreateActorWithEquipment(
        0, 0, bladeWepId, bladeMaterial, shieldWepId, shieldMaterial, &sp20, 0);

    Actors_InitializeActorFromTemplate(0, &Actors_ashleyInitialLoadout, 0x10, 0, 0x80, 0);
    if (bladeIndex != 0) {
        Items_LoadInventoryWeaponStats(&Battle_characterState->stats->weapon,
            &Audio_Seq_unusedInventory.weapons[bladeIndex - 1]);
    } else {
        Items_LoadInventoryWeaponStats(&Battle_characterState->stats->weapon, NULL);
    }

    Battle_characterState->stats->currentRange =
        Battle_characterState->stats->weapon.range.range
        + Battle_characterState->stats->reach;
    Battle_characterState->stats->currentRangeY =
        Battle_characterState->stats->weapon.range.secondRange
        + Battle_characterState->stats->reach;
    Battle_characterState->stats->currentRangeZ =
        Battle_characterState->stats->weapon.range.thirdRange
        + Battle_characterState->stats->reach;
    Battle_characterState->stats->currentAttackShapeAngle =
        Battle_characterState->stats->weapon.range.attackShapeAngle;
    Battle_characterState->stats->currentAttackShape = 1;

    if (shieldIndex != 0) {
        Items_LoadInventoryShieldStats(&Battle_characterState->stats->shield,
            &Audio_Seq_unusedInventory.shields[shieldIndex - 1]);
    } else {
        Items_LoadInventoryShieldStats(&Battle_characterState->stats->shield, NULL);
    }

    for (i = 0; i < 6; ++i) {
        Items_LoadInventoryArmorStats(&Battle_characterState->stats->limbs[i].armor, NULL);

        for (j = 0; j < 16; ++j) {
            if (Audio_Seq_unusedInventory.armor[j].limb == i + 1) {
                Items_LoadInventoryArmorStats(&Battle_characterState->stats->limbs[i].armor,
                    &Audio_Seq_unusedInventory.armor[j]);
            }
        }
    }

    Items_LoadInventoryAccessoryStats(&Battle_characterState->stats->accessory, NULL);

    for (i = 0; i < 16; ++i) {
        if (Audio_Seq_unusedInventory.armor[i].limb == 7) {
            Items_LoadInventoryAccessoryStats(
                &Battle_characterState->stats->accessory, &Audio_Seq_unusedInventory.armor[i]);
        }
    }

    Core_IgnoreCallback(Battle_characterState->stats);

    if (State_ashleySnapshot.stats.isPending != 0) {
        Actors_RestoreAshleyRuntimeState();
    }

    Actors_AshleySpawnCompletionHook();

    while (Actors_AshleyModelLoadQueueSlot() != 0) {
        Content_UpdateActorAssetQueue();
        State_UpdatePlayTimeAndLoading(0);
    }
}

