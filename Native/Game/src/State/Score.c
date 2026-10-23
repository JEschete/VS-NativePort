// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Abilities.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Hit.h"
#include "Battle/Targeting.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"
#include "State/Progress.h"
#include "State/Score.h"

// decomp: BATTLE.PRG _updateEnemyKills (src/BATTLE/BATTLE.PRG/146C.c:1772)
void State_RecordEnemyKillScore(Battle_Actor* enemy)
{
    u_int subType = enemy->subType;

    if ((enemy->stats != NULL) && (enemy->kind == 4)) {

        Battle_ActorStats* actor = enemy->stats;

        if (Audio_Seq_unusedScoredata.enemyKills[actor->enemyClass] < USHRT_MAX) {
            ++Audio_Seq_unusedScoredata.enemyKills[actor->enemyClass];
        }

        if (Audio_Seq_unusedScoredata.killStreakEnemyClass != (enemy->stats->enemyClass)) {
            Audio_Seq_unusedScoredata.killStreakEnemyClass = enemy->stats->enemyClass;
            Audio_Seq_unusedScoredata.killStreakEnemyCount = 0;
        } else if (Audio_Seq_unusedScoredata.killStreakEnemyCount < 100) {
            ++Audio_Seq_unusedScoredata.killStreakEnemyCount;
        }

        Audio_Seq_unusedScoredata.streakScore +=
            actor->maxHp + ((actor->maxHp * Audio_Seq_unusedScoredata.killStreakEnemyCount) / 2)
            + ((actor->maxHp * Audio_Seq_unusedScoredata.weaponKillStreak) / 2)
            + ((actor->maxHp * Audio_Seq_unusedScoredata.chainStreak) / 2);

        if (Audio_Seq_unusedScoredata.streakScore > 999999999) {
            Audio_Seq_unusedScoredata.streakScore = 999999999;
        }
    }

    if (subType < 256) {
        Audio_Seq_unusedScoredata.enemyKillFlags[subType / 32] |= 1 << (subType % 32);
    }
}

// decomp: BATTLE.PRG _updateWeaponKillStreak (src/BATTLE/BATTLE.PRG/146C.c:1806)
void State_UpdateWeaponKillStreak(int arg0 __attribute__((unused)))
{
    Battle_ActorStats* actor = Battle_characterState->stats;

    if (Battle_actionState->selection.actionId >= 40) {
        return;
    }

    if (Audio_Seq_unusedScoredata.weaponKillStreakBladeCategory != actor->weapon.blade.category) {

        Audio_Seq_unusedScoredata.weaponKillStreakBladeCategory = actor->weapon.blade.category;
        Audio_Seq_unusedScoredata.weaponKillStreak = 0;

    } else if (Audio_Seq_unusedScoredata.weaponKillStreak < 100) {
        ++Audio_Seq_unusedScoredata.weaponKillStreak;
    }
}

