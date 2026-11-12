// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Audio/SfxLive.hpp"
#include "Battle/ActionResultsLive.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Items/LootLive.hpp"
#include "State/KillScoreLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Action.h"
#include "Battle/ActionAnimation.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/Hit.h"
#include "Battle/TargetingState.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Menu/Framework/ActionSelection.h"
#include "Render/Palette.h"
#include "State/Progress.h"

// decomp: D_800F18EC (D_800F18EC@BATTLE/BATTLE.PRG)
Menu_Framework_ActionSelection* Battle_actionSelection;

// decomp: _cameraMode (_cameraMode@BATTLE/BATTLE.PRG)
u_int Battle_actionFlowMode;

// decomp: D_800F18FC (D_800F18FC@BATTLE/BATTLE.PRG)
int Battle_recoveryRewardMode;

// decomp: D_800F19CC (D_800F19CC@BATTLE/BATTLE.PRG)
Battle_ActionState* Battle_actionState;

// decomp: D_8005FFAF (D_8005FFAF@SLUS_010.40)
char Battle_spellCommitFlag;

// decomp: BATTLE.PRG _enemyDefeated (src/BATTLE/BATTLE.PRG/146C.c:1824)
void Battle_ResolveEnemyDefeat(int source_actor_slot, int actorId, int count_kill)
{
    Battle_Actor* enemy = Battle_actors[actorId];

    if ((enemy->roomSpawnIndex == 0x80) || (enemy->defeated != 0)) {
        return;
    }

    if ((count_kill != 0) && (source_actor_slot == 0)) {
        VS::State::RecordLiveEnemyKillScore(enemy);
        VS::State::UpdateLiveWeaponKillStreak(0);
    }

    VS::Items::QueueLiveEnemyLoot(enemy);

    enemy->defeated = 1;
}

// decomp: BATTLE.PRG func_8006C5AC (src/BATTLE/BATTLE.PRG/146C.c:1955)
int Battle_PlayWeaponHitSounds(int source_actor_slot)
{
    Actors_ActorLocationQuery target_placement;
    SVECTOR hit_position;
    Battle_ActionRequest* queued_action;
    int i;

    queued_action = &Battle_actionState->queue[Battle_actionState->queueIndex & 3];

    for (i = 0; i < queued_action->targetCount; ++i) {
        if (queued_action->targets[i].targetsPosition == 0) {

            VS::Actors::LiveActorPlacement(queued_action->targets[i].target.targetActor, &target_placement);

            hit_position.vx = target_placement.location.position.vx;
            hit_position.vz = target_placement.location.position.vz;
            hit_position.vy = target_placement.location.position.vy;

            switch (queued_action->targets[i].target.impactKind) {
            case 0:
            case 1:
            case 2:
            case 4:
                switch (Battle_actors[source_actor_slot]->equippedWeaponCategory) {
                case 1:
                    VS::Audio::LivePlayWorldSfx(0x7E, 0x47, &hit_position);
                    VS::Audio::LivePlayWorldSfx(0x7E, 0x48, &hit_position);
                    break;

                case 2:
                    VS::Audio::LivePlayWorldSfx(0x7E, 0x49, &hit_position);
                    VS::Audio::LivePlayWorldSfx(0x7E, 0x4A, &hit_position);
                    break;

                case 3:
                    VS::Audio::LivePlayWorldSfx(0x7E, 0x4B, &hit_position);
                    VS::Audio::LivePlayWorldSfx(0x7E, 0x4C, &hit_position);
                    break;

                case 4:
                    VS::Audio::LivePlayWorldSfx(0x7E, 0x50, &hit_position);
                    VS::Audio::LivePlayWorldSfx(0x7E, 0x51, &hit_position);
                    break;

                case 5:
                    VS::Audio::LivePlayWorldSfx(0x7E, 0x52, &hit_position);
                    VS::Audio::LivePlayWorldSfx(0x7E, 0x53, &hit_position);
                    break;

                case 6:
                    VS::Audio::LivePlayWorldSfx(0x7E, 0x4D, &hit_position);
                    break;

                case 7:
                    VS::Audio::LivePlayWorldSfx(0x7E, 0x4E, &hit_position);
                    VS::Audio::LivePlayWorldSfx(0x7E, 0x4F, &hit_position);
                    break;

                case 8:
                    VS::Audio::LivePlayWorldSfx(0x7E, 0x54, &hit_position);
                    VS::Audio::LivePlayWorldSfx(0x7E, 0x55, &hit_position);
                    break;

                case 10:
                    VS::Audio::LivePlayWorldSfx(0x7E, 0x5B, &hit_position);
                    VS::Audio::LivePlayWorldSfx(0x7E, 0x5C, &hit_position);
                    break;

                case 9:
                    VS::Audio::LivePlayWorldSfx(0x7E, 0x69, &hit_position);
                    VS::Audio::LivePlayWorldSfx(0x7E, 0x6A, &hit_position);
                    break;
                }

                break;

            case 3:
                VS::Audio::LivePlayWorldSfx(0x7E, 0x33, &hit_position);
                break;

            case 5:
                break;
            }
        }
    }

    return 1;
}

// decomp: BATTLE.PRG func_8006CA20 (src/BATTLE/BATTLE.PRG/146C.c:2106)
void Battle_ApplyActionSourceEffects(int source_actor_slot, SVECTOR* arg1)
{
    Battle_ActionRequest* queued_action;

    if (source_actor_slot >= 16) {
        return;
    }

    queued_action = &Battle_actionState->queue[Battle_actionState->queueIndex & 3];

    if ((queued_action->primaryTarget.target.targetActor == source_actor_slot) && (queued_action->primaryTarget.targetsPosition == 0)) {

        switch (queued_action->primaryTarget.target.impactKind) {
        case 2:
        case 3:
        case 4:
            break;

        case 0:
        case 1:
            Battle_PlayActorDamageReaction(source_actor_slot, &queued_action->focusPosition);
            break;

        case 5:
            if (source_actor_slot == 0) {
                VS::Actors::PlayLiveActorActionPose(0, NULL, 6);
            }

            break;
        }

        VS::Battle::ApplyLiveActorActionResult(&queued_action->primaryTarget);
    }
}

// decomp: BATTLE.PRG func_80074A20 (src/BATTLE/BATTLE.PRG/146C.c:5315)
int Battle_BeginActorDefeatAnimation(int actor_slot)
{
    int ret;

    VS::Battle::Ai::RefreshLiveActorAiReservations(actor_slot);
    ret = 0;

    if (Battle_actors[actor_slot] != NULL) {
        Battle_actors[actor_slot]->commandState = 0;
        if (Battle_actors[actor_slot]->stats->currentHp == 0) {
            Battle_SetActorMarker(actor_slot, 0);
            Battle_actors[actor_slot]->commandState = 0x20;
            if (Battle_actors[actor_slot]->retainAfterDefeat == 0) {
                VS::Actors::BeginLiveActorDeath(actor_slot, &Battle_actionState->rangeOrigin, 1);
                VS::Actors::SetLiveActorAnimationLocked(actor_slot, 1);
                if ((Battle_actors[actor_slot]->roomSpawnIndex != 0x80) || (actor_slot == 0)) {
                    ret = 1;
                }
            }
        }
    }
    return ret;
}

// decomp: INITBTL.PRG func_800F9818 (src/BATTLE/INITBTL.PRG/18.c:240)
void Battle_ApplyLearnedActionUnlocks(void)
{
    int i;
    int learned_flag_index;

    for (i = 0; i < 256; ++i) {
        learned_flag_index = i;
        if (i < 0) {
            learned_flag_index = i + 7;
        }
        if (State_learnedActionFlags[learned_flag_index >> 3] & (0x80 >> (i - ((learned_flag_index >> 3) * 8)))) {
            Battle_actionDefinitions[i].unlocked = 1;
        } else {
            Battle_actionDefinitions[i].unlocked = 0;
        }
    }
}

}  // extern "C"
