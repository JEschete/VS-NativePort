// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
#include "Actors/Animation.h"
#include "Actors/Appearance.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Audio/Music.h"
#include "Audio/SoundBanks.h"
#include "Battle/Abilities.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/Hud.h"
#include "DiscQueue.h"
#include "Event/Cutscene.h"
#include "Event/ScriptVm.h"
#include "Event/Triggers.h"
#include "Field/Camera.h"
#include "Field/GameMode.h"
#include "Field/Gimmicks.h"
#include "Field/Input.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Field/RoomEffects.h"
#include "Field/RoomLoading.h"
#include "Field/Znd.h"
#include "Field/Zone.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Camera.h"
#include "Render/Palette.h"
#include "State/Flags.h"
#include "State/SceneTimer.h"
#include "State/Timers.h"

// decomp: BATTLE.PRG func_80073718 (src/BATTLE/BATTLE.PRG/146C.c:4720)
void Event_PrepareCutsceneActors(void)
{
    int i;

    Field_controlMode = 3;
    Battle_actionFlowMode = 2;
    State_SetScriptFlag(0xAC, Battle_characterState->weaponDrawn & 1);
    State_SetScriptFlag(
        0xAF, (Field_sphericalCamera.delta.pitch / 512) & 7);
    if (Field_sphericalCamera.initialDistance == 0x600) {
        State_SetScriptFlag(0xB0, 0);
    } else {
        State_SetScriptFlag(0xB0, 1);
    }

    for (i = 0; i < 16; ++i) {
        if (Battle_actors[i] != NULL) {
            Actors_SetActorHeadTracking(i, -2, NULL, -1);
            switch (Battle_actors[i]->commandState) {
            case 0x10:
                break;
            case 0x20:
                break;
            case 0x40:
                Battle_actors[i]->commandState = 0x10;
                break;
            default:
                if (Battle_actors[i]->commandState == 0x80) {
                    Battle_Ai_RefreshActorAiReservations(i);
                }
                Battle_actors[i]->commandState = 0;
                break;
            }
        }
    }

    Battle_SetActorMarkersHidden(1);
    Field_isMenuRequestQueued = 0;
    Field_SetMovingPlatformsPaused(1);
}

// decomp: BATTLE.PRG func_80073870 (src/BATTLE/BATTLE.PRG/146C.c:4761)
void Event_StartPreparedCutscene(void)
{
    Event_RequestEventStart();
    Event_PrepareCutsceneActors();
}

// decomp: BATTLE.PRG func_80073898 (src/BATTLE/BATTLE.PRG/146C.c:4767)
void Event_BeginQueuedCutscene(void)
{
    if ((Event_PendingEventSelector() << 16) != 0) {
        Field_controlMode = 3;
        Battle_actionFlowMode = 1;
        return;
    }
    Event_StartPreparedCutscene();
}

// decomp: BATTLE.PRG func_800738E4 (src/BATTLE/BATTLE.PRG/146C.c:4777)
void Event_EndFieldCutscene(void)
{
    Actors_ModelEffectSlot sp10;
    int i;

    Field_SetMovingPlatformsPaused(0);
    Battle_SetActorMarkersHidden(0);
    Field_isFirstPersonViewEnabled = 0;

    for (i = 0; i < 16; ++i) {
        if ((Battle_actors[i] != NULL) && (Battle_actors[i]->commandState == 0x10)) {
            Battle_actors[i]->commandState = 0x40;
        }
    }

    Battle_Ai_RebuildActorActionPlans(NULL);
    Battle_Ai_UpdateRouteGateAvailability();
    if ((Event_EvaluateRoomEventTriggers(0xA, 0, NULL, 0) == 1)
        || ((State_sceneTimer.mode != 0) && (Event_EvaluateRoomEventTriggers(0x10, 0, NULL, 0) == 1))) {
        Event_StartPreparedCutscene();
        return;
    }

    sp10.kind = 0;
    Actors_SetActorSoundOverrides(-1, &sp10);

    for (i = 0; i < 16; ++i) {
        Actors_SetActorPaletteFade(i, 0);
        Actors_SetActorTint(i, 0);
    }

    Field_RestoreRoomEffectSoundVolume();
    Render_SetCameraFarClip(0x1000, 1);
    Field_SetCameraRoll(0);
    Battle_SetGameplayHudHidden(0);
    if (Audio_soundState.playingMusicHandle == 0xFFFF) {
        if (Field_roomMusicSlot != 0) {
            Audio_PlayMusicSlot(Field_roomMusicSlot, 0x7F, 0);
        }
    } else if ((Field_roomMusicSlot != 0)
               && (Audio_soundState.playingMusicHandle == Audio_soundState.musicHandles[Field_roomMusicSlot - 1])) {
        Audio_FadeMusicSlotVolume(Field_roomMusicSlot, 0x7F, 0x3C);
    }
    Field_EnterFieldExplorationMode();
}

