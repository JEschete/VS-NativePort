// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/Lifecycle.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Audio/Ambience.h"
#include "Battle/Abilities.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Feedback.h"
#include "Battle/Hud.h"
#include "Battle/Targeting.h"
#include "Effects/ActionEffects.h"
#include "Field/GameMode.h"
#include "Field/Gimmicks.h"
#include "Field/Interactions.h"
#include "Field/Map.h"
#include "Field/RoomRendering.h"
#include "Field/RoomState.h"
#include "Field/Transitions.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Menu/Framework/ActionSelection.h"
#include "Menu/Framework/Framework.h"
#include "Render/Palette.h"
#include "State/Flags.h"

// decomp: D_800F1968 (D_800F1968@BATTLE/BATTLE.PRG)
int Field_isInteractionPaused;

// decomp: BATTLE.PRG func_8006C2FC (src/BATTLE/BATTLE.PRG/146C.c:1866)
void Field_PauseFieldForInteractionDialog(void)
{
    int i;

    for (i = 1; i < 16; ++i) {
        Actors_RestoreActorIdlePose(i);
    }

    Field_SetMinimapVisible(0);
    Battle_SetActorMarkersHidden(1);

    Field_isInteractionPaused = 1;
}

// decomp: BATTLE.PRG func_8006C350 (src/BATTLE/BATTLE.PRG/146C.c:1880)
void Field_PauseFieldForObjectInteraction(void)
{
    int i;

    for (i = 1; i < 16; ++i) {
        Actors_RestoreActorIdlePose(i);
    }

    Battle_SetActorMarkersHidden(1);

    Field_isInteractionPaused = 1;
}

// decomp: BATTLE.PRG func_8006C39C (src/BATTLE/BATTLE.PRG/146C.c:1893)
void Field_PauseFieldForMenu(void)
{
    Render_TransformRoomPalettes(13, 4, -4, -4, 0);
    Render_SetClutAnimationEnabled(0);
    Actors_UnfreezeAllActorAnimations();
    Field_SetRoomAnimationsPaused(1);
    Field_SetMovingPlatformsPaused(1);
    Field_SetMinimapVisible(0);
    Audio_SetRoomAmbienceDucked(1);
    Battle_SetActorMarkersHidden(1);

    Field_isInteractionPaused = 1;
}

// decomp: BATTLE.PRG func_8006C40C (src/BATTLE/BATTLE.PRG/146C.c:1907)
void Field_ResumeFieldAfterMenu(void)
{
    Render_SetClutAnimationEnabled(1);
    Render_TransformSceneColors(8, 4, 0, 0, 0);
    Actors_FreezeAllActorAnimations();
    Field_SetRoomAnimationsPaused(0);
    Field_SetMovingPlatformsPaused(0);
    Field_SetMinimapVisible(1);
    Audio_SetRoomAmbienceDucked(0);
    Battle_SetActorMarkersHidden(0);

    Field_isInteractionPaused = 0;
}

// decomp: BATTLE.PRG func_8006F5CC (src/BATTLE/BATTLE.PRG/146C.c:3221)
void Field_EnterFieldExplorationMode(void)
{
    Field_controlMode = 1;
    Battle_actionFlowMode = 0;
    Field_MarkCurrentRoomVisited();
}

// decomp: BATTLE.PRG func_8007352C (src/BATTLE/BATTLE.PRG/146C.c:4659)
void Field_BeginShortcutMenu(void)
{
    Field_controlMode = 6;
    Field_PauseFieldForMenu();
}

// decomp: BATTLE.PRG func_80073554 (src/BATTLE/BATTLE.PRG/146C.c:4665)
void Field_EndShortcutMenu(void)
{
    Field_controlMode = 1;
    Field_ResumeFieldAfterMenu();
}

// decomp: BATTLE.PRG func_8007357C (src/BATTLE/BATTLE.PRG/146C.c:4671)
void Field_BeginBattleActionMenu(int arg0 __attribute__((unused)), int arg1 __attribute__((unused)))
{
    Field_controlMode = 5;
    Battle_actionFlowMode = 1;
    Battle_SetHudBattleMode((Battle_characterState->weaponDrawn & 1) | 2);
    Field_PauseFieldForMenu();
}

// decomp: BATTLE.PRG func_800735CC (src/BATTLE/BATTLE.PRG/146C.c:4679)
void Field_CloseBattleActionMenu(Menu_Framework_ActionSelection* arg0)
{
    Menu_Framework_ReleaseMenuRows();
    arg0->menuStatus = 0;
    Field_ResumeFieldAfterMenu();
}

// decomp: BATTLE.PRG func_800735F8 (src/BATTLE/BATTLE.PRG/146C.c:4686)
void Field_DispatchBattleActionMenuResult(Menu_Framework_ActionSelection* arg0)
{
    Actors_ActorLocationQuery sp18;

    Field_SetMinimapVisible(1);
    Battle_SetHudBattleMode(Battle_characterState->weaponDrawn & 1);

    switch (arg0->selectionKind) {
    case 5:
        break;
    case 6:
    case 7:
    case 8:
        Battle_BeginActorActionSelection(0, arg0->executeAbility.word);
        return;
    case 10:
        Actors_ActorPlacement(0, &sp18);
        Effects_SpawnTrapTileEffect(&sp18.location);
        State_SetScriptFlag(0xB7, 1);
        Field_TeleportToLocation(arg0->executeAbility.bits.location,
            arg0->executeAbility.bits.tileX, arg0->executeAbility.bits.tileZ, 0,
            8);
        if (Battle_characterState->stats->currentMp
            >= arg0->executeAbility.bits.mpCost) {
            Battle_characterState->stats->currentMp -=
                arg0->executeAbility.bits.mpCost;
        }
        ((void (*)(int, short*, int))Actors_PlayActorActionPose)(0, NULL, 7);
        break;
    }
    Field_EnterFieldExplorationMode();
    Field_CloseBattleActionMenu(arg0);
}

