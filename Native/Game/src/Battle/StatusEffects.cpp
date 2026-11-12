// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Battle/StatusHelpersLive.hpp"
#include "Battle/StatusLifecycleLive.hpp"
#include "State/SceneTimerLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/Hit.h"
#include "Battle/StatusEffects.h"
#include "Event/RoomTriggerEntryLive.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Menu/Framework/ActorPreview.h"
#include "Menu/Score/ScoreScreen.h"
#include "Render/Palette.h"
#include "State/Timers.h"

// decomp: D_800E8204 (D_800E8204@BATTLE/BATTLE.PRG)
int Battle_limbStatusFlags[4]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E8490 (D_800E8490@BATTLE/BATTLE.PRG)
int Battle_vitalsTickCounter; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E8494 (D_800E8494@BATTLE/BATTLE.PRG)
int Battle_regeneratingLimb; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E8498 (D_800E8498@BATTLE/BATTLE.PRG)
int Battle_targetingRiskTimer; /* contents: module image (ModuleData.cpp) */

// decomp: vs_battle_sceneBuffer (vs_battle_sceneBuffer@BATTLE/BATTLE.PRG)
void* Battle_unusedSceneBuffer;

// decomp: vs_battle_menu9CursorMemory (vs_battle_menu9CursorMemory@BATTLE/BATTLE.PRG)
Menu_Score_CursorMemory Battle_unusedMenu9CursorMemory;

// decomp: vs_battle_submenuState (vs_battle_submenuState@BATTLE/BATTLE.PRG)
int Battle_unusedSubmenuState;

// decomp: D_800F4590 (D_800F4590@BATTLE/BATTLE.PRG)
Menu_Framework_PreviewPoseState* Battle_unusedGlobal1[1];

// decomp: D_800F4594 (D_800F4594@BATTLE/BATTLE.PRG)
int Battle_unusedGlobal2;

// decomp: vs_main_statusEffectParams (vs_main_statusEffectParams@SLUS_010.40)
Battle_StatusEffectParams Battle_statusEffectParams[32]; /* contents: module image (ModuleData.cpp) */

// decomp: BATTLE.PRG _applyBattleAbilityEffect (src/BATTLE/BATTLE.PRG/146C.c:14018)
void Battle_ApplyActorActionResult(Battle_HitResult* hit_result)
{
    int flag;
    u_int i;
    int targetActorId;
    Battle_ActorStats* actor;

    Battle_CommitEquipmentStatChanges();

    targetActorId = hit_result->target.targetActor;
    actor = Battle_actors[targetActorId]->stats;

    if (hit_result->target.impactKind == 4) {
        VS::Battle::QueueLiveActorStatChangeNumber(targetActorId, hit_result->target.targetLimb, 0, 2);
    }

    switch (hit_result->hpEffect) {
    case 2:

        VS::Battle::QueueLiveActorStatChangeNumber(targetActorId, hit_result->target.targetLimb, hit_result->target.hp, 0);

        actor->currentHp += hit_result->target.hp;

        if (actor->maxHp < actor->currentHp) {
            actor->currentHp = actor->maxHp;
        }

        for (i = 0; i < 6; ++i) {

            actor->limbs[i].hp += hit_result->limbs[i].effects.opaque0;

            if (actor->limbs[i].maxHp < actor->limbs[i].hp) {
                actor->limbs[i].hp = actor->limbs[i].maxHp;
            }
        }

        break;

    case 3:
    case 1:

        VS::Battle::QueueLiveActorStatChangeNumber(
            targetActorId, hit_result->target.targetLimb, hit_result->target.hp | 0x80000000, 0);

        if (actor->currentHp > hit_result->target.hp) {
            actor->currentHp -= hit_result->target.hp;
        } else {
            actor->currentHp = 0;
        }

        for (i = 0; i < 6; ++i) {
            if (actor->limbs[i].hp > hit_result->limbs[i].effects.opaque0) {
                actor->limbs[i].hp -= hit_result->limbs[i].effects.opaque0;
            } else {
                actor->limbs[i].hp = 0;
            }
            if (actor->limbs[i].armor.currentDp > hit_result->limbs[i].effects.opaque2) {
                actor->limbs[i].armor.currentDp -= hit_result->limbs[i].effects.opaque2;
            } else {
                actor->limbs[i].armor.currentDp = 0;
            }
        }

        break;
    }

    switch (hit_result->mpEffect) {
    case 2:
        VS::Battle::QueueLiveActorStatChangeNumber(targetActorId, hit_result->target.targetLimb, hit_result->target.mp, 1);

        actor->currentMp += hit_result->target.mp;

        if (actor->maxMp < actor->currentMp) {
            actor->currentMp = actor->maxMp;
        }

        break;

    case 1:
        VS::Battle::QueueLiveActorStatChangeNumber(
            targetActorId, hit_result->target.targetLimb, hit_result->target.mp | 0x80000000, 1);

        if (actor->currentMp > hit_result->target.mp) {
            actor->currentMp -= hit_result->target.mp;
        } else {
            actor->currentMp = 0;
        }
        break;
    }

    switch (hit_result->riskEffect) {
    case 1:
        break;

    case 2:
        VS::Battle::QueueLiveActorStatChangeNumber(targetActorId, hit_result->target.targetLimb, hit_result->riskChange | 0x80000000, 8);

        if (actor->risk > hit_result->riskChange) {
            actor->risk -= hit_result->riskChange;
        } else {
            actor->risk = 0;
        }
        break;
    }

    switch (hit_result->strengthEffect) {
    case 2:
        VS::Battle::QueueLiveActorStatChangeNumber(targetActorId, hit_result->target.targetLimb, hit_result->strengthChange, 5);

        actor->totalStrength += hit_result->strengthChange;

        if (actor->totalStrength > 999) {
            actor->totalStrength = 999;
        }

        actor->strength += hit_result->strengthChange;

        if (actor->strength > 999) {
            actor->strength = 999;
        }

        break;

    case 1:
        VS::Battle::QueueLiveActorStatChangeNumber(
            targetActorId, hit_result->target.targetLimb, hit_result->strengthChange | 0x80000000, 5);

        if (actor->totalStrength > hit_result->strengthChange) {
            actor->totalStrength -= hit_result->strengthChange;
        } else {
            actor->totalStrength = 0;
        }

        if (actor->strength > hit_result->strengthChange) {
            actor->strength -= hit_result->strengthChange;
        } else {
            actor->strength = 0;
        }

        break;
    }

    switch (hit_result->intelligenceEffect) {
    case 2:
        VS::Battle::QueueLiveActorStatChangeNumber(targetActorId, hit_result->target.targetLimb, hit_result->intelligenceChange, 6);

        actor->totalIntelligence += hit_result->intelligenceChange;

        if (actor->totalIntelligence > 999) {
            actor->totalIntelligence = 999;
        }

        actor->intelligence += hit_result->intelligenceChange;

        if (actor->intelligence > 999) {
            actor->intelligence = 999;
        }

        break;

    case 1:
        VS::Battle::QueueLiveActorStatChangeNumber(
            targetActorId, hit_result->target.targetLimb, hit_result->intelligenceChange | 0x80000000, 6);

        if (actor->totalIntelligence > hit_result->intelligenceChange) {
            actor->totalIntelligence -= hit_result->intelligenceChange;
        } else {
            actor->totalIntelligence = 0;
        }

        if (actor->intelligence > hit_result->intelligenceChange) {
            actor->intelligence -= hit_result->intelligenceChange;
        } else {
            actor->intelligence = 0;
        }

        break;
    }

    switch (hit_result->agilityEffect) {
    case 2:
        VS::Battle::QueueLiveActorStatChangeNumber(targetActorId, hit_result->target.targetLimb, hit_result->agilityChange, 7);

        actor->totalAgility += hit_result->agilityChange;

        if (actor->totalAgility > 999) {
            actor->totalAgility = 999;
        }

        actor->agility += hit_result->agilityChange;

        if (actor->agility > 999) {
            actor->agility = 999;
        }

        break;

    case 1:
        VS::Battle::QueueLiveActorStatChangeNumber(
            targetActorId, hit_result->target.targetLimb, hit_result->agilityChange | 0x80000000, 7);

        if (actor->totalAgility > hit_result->agilityChange) {
            actor->totalAgility -= hit_result->agilityChange;
        } else {
            actor->totalAgility = 0;
        }

        if (actor->agility > hit_result->agilityChange) {
            actor->agility -= hit_result->agilityChange;
        } else {
            actor->agility = 0;
        }
        break;
    }

    switch (hit_result->maxHpEffect) {
    case 1:
        break;
    case 2:
        VS::Battle::QueueLiveActorStatChangeNumber(targetActorId, hit_result->target.targetLimb, hit_result->maxHpChange, 3);

        actor->maxHp += hit_result->maxHpChange;

        if (actor->maxHp > 999) {
            actor->maxHp = 999;
        }
        break;
    }

    switch (hit_result->maxMpEffect) {
    case 1:
        break;
    case 2:
        VS::Battle::QueueLiveActorStatChangeNumber(targetActorId, hit_result->target.targetLimb, hit_result->maxMpChange, 4);

        actor->maxMp += hit_result->maxMpChange;

        if (actor->maxMp > 999) {
            actor->maxMp = 999;
        }
        break;
    }

    switch (hit_result->weaponDpEffect) {
    case 1:
        break;
    case 2:
        actor->weapon.currentDp += hit_result->weaponDpChange;

        if (actor->weapon.maxDp < actor->weapon.currentDp) {
            actor->weapon.currentDp = actor->weapon.maxDp;
        }
        break;
    }

    switch (hit_result->weaponPpEffect) {
    case 2:
        actor->weapon.currentPp += hit_result->weaponPpChange;

        if (actor->weapon.maxPp < actor->weapon.currentPp) {
            actor->weapon.currentPp = actor->weapon.maxPp;
        }

        break;

    case 1:
        if (actor->weapon.currentPp > hit_result->weaponPpChange) {
            actor->weapon.currentPp -= hit_result->weaponPpChange;
        } else {
            actor->weapon.currentPp = 0;
        }

        break;
    }

    switch (hit_result->armorDpEffect) {
    case 1:
        if (actor->shield.currentDp > hit_result->armorDpChange) {
            actor->shield.currentDp -= hit_result->armorDpChange;
        } else {
            actor->shield.currentDp = 0;
        }

        if (actor->limbs[hit_result->target.targetLimb].armor.currentDp > hit_result->armorDpChange) {
            actor->limbs[hit_result->target.targetLimb].armor.currentDp -= hit_result->armorDpChange;
        } else {
            actor->limbs[hit_result->target.targetLimb].armor.currentDp = 0;
        }

        break;
    }

    switch (hit_result->shieldPpEffect) {
    case 1:
        if (actor->shield.currentPp > hit_result->shieldPpChange) {
            actor->shield.currentPp -= hit_result->shieldPpChange;
        } else {
            actor->shield.currentPp = 0;
        }

        break;
    }

    flag = 1;

    if (hit_result->inflictStatuses != 0) {

        for (i = 0; i < 32; ++i) {

            int status = hit_result->inflictStatuses & flag;

            if ((status != 0) && (actor->statuses & flag)) {

                if (targetActorId != 0) {

                    if (actor->currentHp != 0) {
                        ((void (*)(int, int, int, int))Battle_QueueActorStatusLabels)(targetActorId, 0, status, 1);
                    }

                } else {
                    ((void (*)(int, int, int, int))Battle_QueueActorStatusLabels)(0, 0, status, 1);
                }

                VS::Battle::RemoveLiveActorStatuses(hit_result->inflictStatuses & flag, actor);
            }

            flag <<= 1;
        }
    }

    flag = 1;

    if (hit_result->removeStatuses != 0) {

        for (i = 0; i < 32; ++i) {

            int status = hit_result->removeStatuses & flag;

            if ((status != 0) && !(actor->statuses & flag)) {

                if (targetActorId != 0) {

                    if (actor->currentHp != 0) {
                        ((void (*)(int, int, int, int))Battle_QueueActorStatusLabels)(targetActorId, 0, status, 0);
                    }

                } else {
                    ((void (*)(int, int, int, int))Battle_QueueActorStatusLabels)(0, 0, status, 0);
                }

                VS::Battle::InflictLiveActorStatuses(hit_result->removeStatuses & flag, actor);
            }

            flag <<= 1;
        }
    }
}

// decomp: BATTLE.PRG func_80088554 (src/BATTLE/BATTLE.PRG/146C.c:14560)
int Battle_UpdateActorVitals(void)
{
    int update_result;
    int i;
    int pending_event_result = 0;

    for (i = 0; i < 16; ++i) {

        Battle_Actor* battle_actor = Battle_actors[i];
        update_result = 0;

        if ((battle_actor != NULL) && ((battle_actor->roomSpawnIndex != 0x80) || (i == 0))) {

            int actor_state_flags = VS::Actors::LiveActorStateFlags(i);

            if (actor_state_flags & 0x01000000) {

                if (VS::Actors::IsLiveActorActionAnimationBusy(i) != 1) {

                    update_result = Battle_ResolveActorDefeat(0, i, 0);

                    if ((update_result == 0) && (i != 0)) {
                        if (Event_EvaluateRoomEventTriggersEntry(0xC, 0, NULL, 0) == 1) {
                            update_result = 1;
                        } else {
                            update_result = 2;
                        }
                    }
                }

            } else if ((actor_state_flags & 0x01002001) == 1) {

                Battle_ActorStats* actor_stats = battle_actor->stats;

                if (actor_stats != NULL) {

                    if (actor_stats->actionRecoveryTimer > 35) {
                        actor_stats->actionRecoveryTimer -= 35;
                    } else if (actor_stats->actionRecoveryTimer != 0) {
                        actor_stats->actionRecoveryTimer = 0;
                    }

                    if ((Battle_vitalsTickCounter % 30 == 0) && (actor_stats->currentHp != 0)) {

                        if (battle_actor->weaponDrawn & 1) {

                            if (Battle_vitalsTickCounter % 120 == 0) {
                                if (actor_stats->risk != 0) {
                                    --actor_stats->risk;
                                }
                            }

                            if (Battle_vitalsTickCounter % 180 == 0) {

                                if (actor_stats->currentMp < actor_stats->maxMp) {
                                    ++actor_stats->currentMp;
                                }

                                if (battle_actor->weaponDrawn & 1) {

                                    if (actor_stats->weapon.currentPp != 0) {
                                        --actor_stats->weapon.currentPp;
                                    }

                                    if (actor_stats->shield.currentPp != 0) {
                                        --actor_stats->shield.currentPp;
                                    }
                                }
                            }

                            if (Battle_vitalsTickCounter % 240 == 0) {

                                if (actor_stats->currentHp < actor_stats->maxHp) {
                                    if (actor_stats->currentHp != 0) {
                                        ++actor_stats->currentHp;
                                    }
                                }

                                if (actor_stats->limbs[Battle_regeneratingLimb].maxHp != 0) {
                                    if (actor_stats->limbs[Battle_regeneratingLimb].hp
                                        < actor_stats->limbs[Battle_regeneratingLimb].maxHp) {
                                        ++actor_stats->limbs[Battle_regeneratingLimb].hp;
                                    }
                                }
                            }

                        } else {

                            if (!(actor_stats->statuses & 8) || (Battle_vitalsTickCounter % 120 == 0)) {

                                if (actor_stats->risk != 0) {
                                    --actor_stats->risk;
                                }
                            }

                            if (Battle_vitalsTickCounter % 60 == 0) {

                                if (actor_stats->currentMp < actor_stats->maxMp) {
                                    ++actor_stats->currentMp;
                                }

                                if (battle_actor->weaponDrawn & 1) {

                                    if (actor_stats->weapon.currentPp != 0) {
                                        --actor_stats->weapon.currentPp;
                                    }

                                    if (actor_stats->shield.currentPp != 0) {
                                        --actor_stats->shield.currentPp;
                                    }
                                }
                            }

                            if (Battle_vitalsTickCounter % 120 == 0) {

                                if (actor_stats->currentHp < actor_stats->maxHp) {
                                    if (actor_stats->currentHp != 0) {
                                        ++actor_stats->currentHp;
                                    }
                                }

                                if (actor_stats->limbs[Battle_regeneratingLimb].maxHp != 0) {
                                    if (actor_stats->limbs[Battle_regeneratingLimb].hp
                                        < actor_stats->limbs[Battle_regeneratingLimb].maxHp) {
                                        ++actor_stats->limbs[Battle_regeneratingLimb].hp;
                                    }
                                }
                            }
                        }
                    }

                    if (Battle_vitalsTickCounter % 60 == 0) {

                        if (actor_stats->currentHp != 0) {
                            VS::Battle::UpdateLiveActorStatusTimers(actor_stats);
                        }

                        if (actor_stats->currentHp == 0) {

                            if (battle_actor->retainAfterDefeat == 0) {
                                Battle_BeginActorDefeatAnimation(i);
                            } else {

                                update_result = Battle_ResolveActorDefeat(0, i, 0);

                                if ((update_result == 0) && (i != 0)) {
                                    if (Event_EvaluateRoomEventTriggersEntry(0xC, 0, NULL, 0) == 1) {
                                        update_result = 1;
                                    } else {
                                        update_result = 2;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if (update_result != 0) {
            if (pending_event_result == 0) {
                pending_event_result = update_result;
            } else if (update_result == 1) {
                pending_event_result = 1;
            }
        }
    }

    Battle_vitalsTickCounter = (Battle_vitalsTickCounter + State_tickSpeed) % 720;

    if (Battle_vitalsTickCounter % 240 == 0) {
        Battle_regeneratingLimb = (Battle_regeneratingLimb + 1) % 6;
    }

    update_result = VS::State::UpdateLiveSceneTimer();

    if (update_result != 0) {
        pending_event_result = update_result;
    }

    return pending_event_result;
}

// decomp: BATTLE.PRG func_80088B6C (src/BATTLE/BATTLE.PRG/146C.c:14743)
int Battle_UpdateRoomActorVitals(void) { return Battle_UpdateActorVitals(); }

}  // extern "C"
