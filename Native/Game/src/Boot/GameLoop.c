// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/Appearance.h"
#include "Actors/Ashley.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/ModelRendering.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Audio/Sfx.h"
#include "Audio/SoundBanks.h"
#include "Battle/Abilities.h"
#include "Battle/Action.h"
#include "Battle/ActionExecution.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/Hit.h"
#include "Battle/Hud.h"
#include "Battle/Rewards.h"
#include "Battle/Sphere.h"
#include "Battle/StatusEffects.h"
#include "Battle/Targeting.h"
#include "Boot/GameLoop.h"
#include "Boot/GameplayInit.h"
#include "Boot/Lifecycle.h"
#include "Core/Memory/Heap.h"
#include "Core/Timing.h"
#include "DiscQueue.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
#include "Event/Cutscene.h"
#include "Event/ScriptVm.h"
#include "Event/Triggers.h"
#include "Field/Background.h"
#include "Field/Camera.h"
#include "Field/Doors.h"
#include "Field/GameMode.h"
#include "Field/Gimmicks.h"
#include "Field/Input.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Field/RoomLoading.h"
#include "Field/RoomRendering.h"
#include "Field/RoomState.h"
#include "Field/Transitions.h"
#include "Field/Znd.h"
#include "Field/Zone.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "Menu/Framework/ActionSelection.h"
#include "Menu/Framework/Framework.h"
#include "Menu/TextBox.h"
#include "Render/Camera.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/FramebufferEffects.h"
#include "Render/Images.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/Textures.h"
#include "Render/UiPrimitives.h"
#include "State/Location.h"
#include "State/SceneTimer.h"
#include "State/Timers.h"
#include "SystemResources.h"

// decomp: vs_main_startState (vs_main_startState@SLUS_010.40)
int Boot_gameEntryMode;

// decomp: sp2 (sp2@SLUS_010.40)
int Boot_titleReturnStackPointer;

// decomp: _resetEnabled (_resetEnabled@SLUS_010.40)
int Boot_isSoftResetEnabled;

// decomp: sp (sp@SLUS_010.40)
int Boot_battleReturnStackPointer;

// decomp: vs_main_saveGameClearData (vs_main_saveGameClearData@SLUS_010.40)
int Boot_isClearSavePending;

// decomp: _inGame (_inGame@SLUS_010.40)
int Boot_isGameplayActive;

// decomp: BATTLE.PRG vs_battle_exec (src/BATTLE/BATTLE.PRG/146C.c:7558)
void Boot_RunGameLoop(void)
{
    Actors_ActorLocationQuery position;
    int skipCameraUpdate;
    u_long* ot;
    u_int status;
    int ready;
    int i;
    Menu_Framework_ActionSelection* menu;
    uintptr_t result;
    int shortcut;

    Field_isMenuRequestQueued = 0;
    Boot_InitializeGameplayOverlay(Boot_gameEntryMode);
    while (1) {
        Field_FinishZoneLoad(Field_pendingDestinationDoor.destinationZoneId);
        do {
            Field_FinishRoomLoad(Field_pendingDestinationDoor.destinationRoomId);
            Field_isMenuRequestQueued = 0;
            do {
                do {
                    if (Input_UpdateControllers() == -1) {
                        break;
                    }
                    ot = Render_legacyOrderingTables[Render_frameBuf];
                    Render_legacyCurrentOrderingTable = ot;
                    Render_frameScratch.overlayOrderingTable = ot + 2;
                    Render_frameScratch.orderingTable = ot + 18;
                    ClearOTagR(ot, 0x822);
                    Render_frameScratch.primitiveCursor = Render_legacyPrimitiveBuffers[Render_frameBuf];
                    AddPrim(ot, &Render_legacyMaskBitOffPackets[Render_frameBuf]);
                    if (((Boot_gameEntryMode == 3) || (Boot_gameEntryMode == 4))
                        && (Input_buttonsPressed.allButtons & 0x9FF)) {
                        Boot_ResetToTitle();
                    }
                    Field_UpdateMovingPlatforms();
                    status = Actors_ActorStateFlags(0);
                    Actors_ActorPlacement(0, &position);

                    if ((Field_screenTransitionStep != 1)
                        && (Field_screenTransitionStep != 2)
                        && (Field_screenTransitionStep != 4)) {
                        skipCameraUpdate = 0;
                        switch (Field_controlMode) {
                        case 0:
                            break;

                        case 1:
                            if (status & 0x2000) {
                                break;
                            }
                            if (Event_EvaluateRoomEventTriggers(0x20, 0, 0, 0) == 1) {
                                Event_BeginQueuedCutscene();
                                break;
                            }
                            if (Field_CheckAshleyTileTriggers(0, (u_char*)&position)) {
                                break;
                            }
                            if ((Battle_characterState->stats->actionRecoveryTimer == 0)
                                && !(status & 0x4000) && (Field_isMenuRequestQueued == 0)
                                && (Input_previousButtons & 1)) {
                                Field_BeginShortcutMenu();
                                break;
                            }
                            if (Field_isMenuRequestQueued != 0) {
                                if ((Battle_characterState->stats->actionRecoveryTimer == 0)
                                    && !(status & 0x4000) && Menu_Framework_OpenGameMenu(0, 0)) {
                                    Field_BeginBattleActionMenu(0, 0);
                                    Field_isMenuRequestQueued = 0;
                                    break;
                                }
                            } else if ((Input_buttonsPressed.allButtons & PADRup)
                                       && !(Input_previousButtons & 1)) {
                                if ((Battle_characterState->stats->actionRecoveryTimer == 0)
                                    && !(status & 0x4000)) {
                                    if (Menu_Framework_OpenGameMenu(0, 0)) {
                                        Field_BeginBattleActionMenu(0, 0);
                                        break;
                                    }
                                } else {
                                    Field_isMenuRequestQueued = 1;
                                }
                            }
                            if (Input_buttonsPressed.allButtons & PADRdown) {
                                if (Field_FindInteractableGimmickAhead(0) >= 0) {
                                    if (!(Battle_characterState->weaponDrawn & 1)) {
                                        Field_BeginGimmickInteraction(0);
                                        break;
                                    }
                                } else if (!(status & 0x3F4000)) {
                                    i = Actors_FindNearestActorAhead(0, 0xA0, 0x80);
                                    if ((i >= 0) && (Battle_actors[i]->interactionFlags == 4)
                                        && (Event_EvaluateRoomEventTriggers(4, 0, i, 0) == 1)) {
                                        Event_BeginQueuedCutscene();
                                        break;
                                    }
                                    if (Field_TryTriggerFacingTileEvent(0)) {
                                        Event_BeginQueuedCutscene();
                                        break;
                                    }
                                    result = (uintptr_t)Field_FindFacingRoomObject(0);
                                    if (result) {
                                        if (!Field_InteractWithLockedRoomObject((Field_MpdLockTrigger*)result)) {
                                            Field_BeginObjectInteractionWait();
                                            break;
                                        }
                                    } else {
                                        result = (uintptr_t)Field_FindFacingUnlootedChest(0);
                                        if (result) {
                                            Field_BeginChestInteraction((Field_MpdLockTrigger*)result, 0);
                                            break;
                                        }
                                    }
                                }
                            }
                            if (!(status & 0x4000)) {
                                if (!(status & 0x180000)
                                    && (Input_buttonsPressed.allButtons & (PADj | PADstart))) {
                                    Field_BeginFirstPersonView();
                                    break;
                                }
                                if (Input_buttonsPressed.allButtons & PADRright) {
                                    if (Battle_characterState->weaponDrawn & 1) {
                                        if ((Battle_characterState->stats->actionRecoveryTimer == 0)
                                            && !(Battle_characterState->stats->statuses
                                                 & 0x2000)) {
                                            Audio_PlaySfxCentered(0x7E, 9);
                                            Battle_BeginActorActionSelection(0, 1);
                                            skipCameraUpdate = 1;
                                            break;
                                        }
                                        Audio_PlaySfxCentered(0x7E, 0xC);
                                    } else if (!(status & 0x180000)) {
                                        Battle_BeginAshleyWeaponToggle();
                                        break;
                                    }
                                }
                                if ((Input_buttonsPressed.allButtons & PADRdown)
                                    && (Battle_characterState->weaponDrawn & 1)) {
                                    Battle_BeginAshleyWeaponToggle();
                                    break;
                                }
                            }
                            if (Actors_UpdateAshleyMovementInput()) {
                                break;
                            }
                            switch (Actors_ExecuteQueuedActorCommands()) {
                            case 1:
                                Event_BeginQueuedCutscene();
                                break;
                            case 2:
                                Battle_recoveryRewardMode = 0;
                                Items_OpenPendingEnemyLoot();
                                break;
                            }
                            break;

                        case 2:
                            Field_TrackActionCameraTarget(0);
                            break;

                        case 3:
                            if (Battle_actionFlowMode == 1) {
                                Field_UpdateCameraOrbitInput();
                                Actors_UpdateRoomActorMovement();
                                Field_SetCameraPositionFromAngles(
                                    &Field_sphericalCamera.values);
                                if (Field_sphericalCamera.delta.yaw != 0) {
                                    break;
                                }
                                if ((Field_sphericalCamera.delta.mode == 1)
                                    || (Field_sphericalCamera.delta.mode
                                        == 3)) {
                                    ready = 1;
                                    for (i = 0; i < 16; ++i) {
                                        if ((Battle_actors[i] != NULL)
                                            && (Actors_ActorStateFlags(i) & 0x20004000)) {
                                            ready = 0;
                                            break;
                                        }
                                    }
                                    if (ready) {
                                        Event_StartPreparedCutscene();
                                    }
                                }
                            } else {
                                Render_UpdateGimImages();
                                Actors_ExecuteQueuedActorCommands();
                                if (Event_EvaluateRoomEventTriggers(0xFF, 0, 0, 0) == 0) {
                                    if ((Boot_gameEntryMode == 3)
                                        || (Boot_gameEntryMode == 4)) {
                                        Boot_ResetToTitle();
                                    } else {
                                        Event_EndFieldCutscene();
                                    }
                                }
                            }
                            break;

                        case 4:
                            if (Effects_CanStartEffectTrack()) {
                                Effects_StartEffectTrack(0);
                            }
                            if (!(Effects_effectBusyFlags & 3)) {
                                Effects_EndAshleyStandaloneEffect();
                            }
                            break;

                        case 5:
                            if (Battle_actionFlowMode == 1) {
                                Menu_Framework_UpdateMenuOverlay();
                                menu = Menu_Framework_ActionSelectionState();
                                Battle_actionSelection = menu;
                                if (menu->menuStatus == 2) {
                                    Field_DispatchBattleActionMenuResult(menu);
                                }
                            }
                            break;

                        case 6:
                            if (Input_previousButtons & 1) {
                                Battle_ShowShortcutHud();
                                if (Input_buttonsPressed.allButtons & PADRright) {
                                    shortcut = 1;
                                } else if (Input_buttonsPressed.allButtons & PADRup) {
                                    shortcut = 2;
                                } else if (Input_buttonsPressed.allButtons & PADRleft) {
                                    shortcut = 3;
                                } else if (Input_buttonsPressed.allButtons & PADRdown) {
                                    shortcut = 4;
                                } else if (Input_buttonsPressed.allButtons & PADLup) {
                                    shortcut = 5;
                                } else if (Input_buttonsPressed.allButtons & PADLdown) {
                                    shortcut = 6;
                                } else if (Input_buttonsPressed.allButtons & PADLright) {
                                    shortcut = 7;
                                } else if (Input_buttonsPressed.allButtons & PADLleft) {
                                    shortcut = 8;
                                } else {
                                    break;
                                }
                                if (Menu_Framework_OpenShortcutMenu(shortcut)) {
                                    Field_BeginBattleActionMenu(0, 0);
                                }
                            } else {
                                Field_EndShortcutMenu();
                            }
                            break;

                        case 7:
                            switch (Battle_actionFlowMode) {
                            case 0:
                                Field_UpdateFirstPersonViewEntry();
                                break;
                            case 1:
                                Field_CheckFirstPersonViewExit();
                                Field_UpdateFirstPersonCamera();
                                break;
                            case 2:
                                Field_UpdateFirstPersonViewExit();
                                if (Input_buttonsPressed.allButtons & (PADj | PADstart)) {
                                    Field_BeginFirstPersonView();
                                }
                                break;
                            }
                            break;

                        case 8:
                            switch (Battle_actionFlowMode) {
                            case 1:
                                if (!Field_StepTrapCameraFocus()) {
                                    Field_DispatchTrapOrChestInteraction();
                                }
                                break;
                            case 2:
                                if (!Effects_ActionEffectBusyFlags()) {
                                    Field_FinishTrapInteraction();
                                }
                                break;
                            case 4:
                                Field_TrackInteractionCameraToAshley();
                                if (Field_IsCameraLookAtOnAshley()) {
                                    Battle_Ai_RebuildActorActionPlans(&Battle_actionState->queue[0]);
                                    Battle_BeginPostActionRecovery(0);
                                    Field_controlMode = 2;
                                }
                                break;
                            }
                            Field_SetCameraPositionFromAngles(
                                &Field_sphericalCamera.values);
                            break;

                        case 9:
                            switch (Battle_actionFlowMode) {
                            case 1:
                                if (!Field_WaitForChestCameraFocus()) {
                                    Field_DispatchTrapOrChestInteraction();
                                }
                                break;
                            case 2:
                                if (Field_IsChestAnimationReady()) {
                                    Field_OpenChestContents();
                                }
                                break;
                            case 3:
                                if (!Field_UpdateChestContentsSelection()) {
                                    Field_FinishChestInteraction();
                                }
                                break;
                            case 4:
                                ready = Field_TrackInteractionCameraToAshley();
                                if (Field_IsCameraLookAtOnAshley() && ready) {
                                    Menu_ReleaseSceneNotification();
                                    Battle_BeginPostActionRecovery(0);
                                    Field_controlMode = 2;
                                }
                                break;
                            }
                            Field_SetCameraPositionFromAngles(
                                &Field_sphericalCamera.values);
                            break;

                        case 10:
                            Actors_SetDirectMovement(0, 0, 0);
                            if (!((u_int (*)(int))Actors_IsActorActionAnimationBusy)(0)) {
                                Battle_EndAshleyWeaponToggle();
                            }
                            break;

                        case 11:
                            if (!Menu_IsSceneNotificationPending()) {
                                Field_EndObjectInteractionWait();
                            }
                            break;

                        case 12:
                            if (!Field_AreGimmicksMoving()) {
                                Field_EnterFieldExplorationMode();
                            }
                            break;
                        }
                    } else {
                        if (Field_controlMode == 3) {
                            Render_UpdateGimImages();
                        } else if (!(status & 0x2000)) {
                            Actors_UpdateAshleyMovementInput();
                        }
                    }

                    Event_UpdateEventPlayback();
                    switch (Field_controlMode) {
                    case 1:
                    case 10:
                    case 12:
                        Field_UpdateAshleyFollowCamera();
                        break;
                    case 2:
                        switch (Battle_actionFlowMode) {
                        case 7 ... 10:
                            break;
                        default:
                            Field_SetCameraPositionFromAngles(
                                &Field_sphericalCamera.values);
                            break;
                        }
                        break;
                    }

                    Field_UpdateCameraFacingState();
                    Render_BuildCameraViewMatrix();
                    Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
                    Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
                    if (!skipCameraUpdate) {
                        switch (Field_controlMode) {
                        case 0:
                        case 1:
                            if ((Field_screenTransitionStep != 1)
                                && (Field_screenTransitionStep != 2)
                                && (Field_screenTransitionStep != 4)) {
                                switch (Battle_UpdateRoomActorVitals()) {
                                case 1:
                                    Event_BeginQueuedCutscene();
                                    break;
                                case 2:
                                    Battle_recoveryRewardMode = 0;
                                    Items_OpenPendingEnemyLoot();
                                    break;
                                }
                            }
                            break;
                        case 2:
                            Battle_UpdateTargetingRiskPenalty();
                            switch (Battle_actionFlowMode) {
                            case 1:
                                Battle_UpdateInitialTargetSelection();
                                break;
                            case 2:
                                Battle_UpdateActionAreaSelection();
                                break;
                            case 3:
                                Battle_UpdateFinalTargetSelection();
                                break;
                            case 4:
                                Battle_UpdateActionCameraZoom();
                                break;
                            case 5:
                                Battle_WaitForActionEffectReady();
                                break;
                            case 6:
                                Battle_StartReadyActionAnimation();
                                break;
                            case 7:
                                Battle_UpdateBattleAbilityTiming();
                                break;
                            case 8:
                                Battle_StartFollowupActionEffects();
                                break;
                            case 9:
                                Battle_WaitForActionResolution();
                                break;
                            case 10:
                                Battle_WaitForTargetRecovery();
                                break;
                            case 11:
                                Battle_FinishActionSequence();
                                break;
                            case 12:
                                Battle_UpdateLootResults();
                                break;
                            case 13:
                                Battle_UpdateAbilityUnlockDelay();
                                break;
                            case 14:
                                Battle_UpdateAbilityUnlockResults();
                                break;
                            }
                            if (Battle_actionFlowMode < 4) {
                                if (Battle_actionFlowMode != 0) {
                                    Battle_DrawSphere();
                                }
                            }
                            break;
                        }
                    }

                    if (Field_isFieldRenderingEnabled) {
                        Field_DrawRoomBackground(NULL);
                        Field_DrawRoomScene();
                        Field_AdvanceRoomAnimationClock();
                        Core_Gte_SetGeomOffset(160, 112);
                        if (Field_isFieldRenderingEnabled) {
                            Actors_UpdateAndDrawRoomActors(&Render_frameScratch.viewMatrix);
                            Content_UpdateActorAssetQueue();
                            Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
                            Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
                            Battle_UpdateAndDrawFloatingLabels();
                        }
                    }
                    i = 0;
                    Effects_effectBusyFlags = Effects_UpdateEffectRuntime();
                    Battle_DrawBattleHud();
                    Menu_UpdateTextBoxes();
                    for (; i < State_tickSpeed / 2; ++i) {
                        Input_UpdateVibrationPatterns();
                        Render_UpdateBackgroundGradientTransition();
                        Render_UpdateModelColorTransitions();
                        Render_UpdateClutTransitions();
                    }
                    Render_DrawScreenTransition(ot);
                    Render_DrawFramebufferEffect(ot);
                    if (Field_controlMode == 1) {
                        Battle_Ai_RunActorAiUpdateStages();
                    } else if (Field_controlMode == 3) {
                        Battle_Ai_UpdateActorNavigationOverrides();
                    }
                    if ((Render_framebufferEffectState == 1) || (Render_framebufferEffectState == 2)) {
                        AddPrim(ot + 0x820, &Render_legacyMaskBitOnPackets[Render_frameBuf]);
                    } else {
                        AddPrim(ot + 0x820, &Render_legacyMaskBitOnPackets[Render_frameBuf]);
                    }
                    Core_ElapsedFrameCounterTicks();
                    Render_PresentFieldFrame(ot + 0x821);
                    Render_AdvanceTexturePageRetirements();
                } while (!(((Field_screenTransitionStep == 1)
                               || (Field_screenTransitionStep == 2))
                           && (Field_screenTransitionAlpha == 255)));
            } while (!(((Field_screenTransitionStep == 1)
                           || (Field_screenTransitionStep == 2))
                       && (Field_screenTransitionAlpha == 255)));
            Field_UnloadCurrentRoom();
        } while (Field_screenTransitionStep != 2);
        Field_ReleaseZoneData();
    }
}

// decomp: INITBTL.PRG func_800FA35C (src/BATTLE/INITBTL.PRG/18.c:502)
void Boot_InitializeGameplayOverlay(int startState __attribute__((unused)))
{
    int _0[2];
    RECT rect;
    VECTOR vector;
    int _1[6];
    int i;

    rect = (RECT) { 0, 0, 0x400, 0x200 };
    vector = (VECTOR) { 0x71C, 0x71C, 0x71C };

    Audio_ResetSoundSlots();
    Core_Memory_InitializeHeap((Core_HeapBlockHeader*)Core_GameHeapArena(), CORE_GAME_HEAP_SIZE);
    Render_legacyPrimitiveBuffers[1] = 0;
    Render_legacyPrimitiveBuffers[0] = NULL;
    Render_legacyOrderingTables[0] = Core_Memory_AllocateFromLowEnd(0x2088);
    Render_legacyOrderingTables[1] = Core_Memory_AllocateFromLowEnd(0x2088);
    Render_InitializeClutAnimation();
    Field_InitializeRoomTransitionPrimitives();
    Render_InitializeFramebufferEffect();
    Event_InitializeScriptOpcodeTable();
    Effects_InitEffectRuntime();
    Field_InitializeRoomRuntimeState();
    Actors_InitializeActorModelResources();
    Render_InitializeTextureSlotReservations();
    Content_LoadSystemResources();
    Battle_Ai_InitializeActorAiExtensionHook();
    Input_ResetVibrationPatterns();
    Battle_ApplyLearnedActionUnlocks();
    Field_zoneContext.currentSceneId = 0;
    Field_zoneContext.reserved38 = 0;
    Field_zoneContext.mapId = 0;
    Field_zoneContext.zndId = 0;
    Field_zoneContext.zudDiscRequest = 0;
    Field_zoneContext.mpdDiscRequest = NULL;
    Field_zoneContext.zndDiscRequest = NULL;
    Field_zoneContext.mpdData = NULL;
    Field_zoneContext.zndData = NULL;
    Field_zoneContext.musicSlot = 0;
    Field_zoneContext.musicId = 0;
    Field_zoneContext.isMusicFadeEnabled = 1;
    Field_zoneContext.loadedZudIndexPlusOne = 0;
    Field_pendingDestinationDoor = State_savedEntryDoor;
    Field_screenTransitionEffect = 0;
    Field_screenTransitionStep = 4;
    Field_screenTransitionSpeed = 8;
    Field_screenTransitionWipeAngle = 0xC00;
    Field_isInteractionPaused = 0;
    Items_lootListHead = NULL;
    Boot_gameplayResetWordOne = 0;
    (*(int (*))&Field_controlMode) = 0;
    State_tickSpeed = 2;
    Render_projectionDistance = 0x200;
    Render_nearClip = 0x40;
    State_sceneTimer.mode = 0;
    State_sceneTimer.hours = 0;
    State_sceneTimer.minutes = 0;
    State_sceneTimer.seconds = 0;
    State_sceneTimer.frames = 0;
    Field_zoneContext.currentSceneId = State_savedEntryDoor.requiredFlagId;
    Field_cameraTransition = NULL;
    Field_ResetCameraToAshley();
    Render_frameScratch.camera.farClip = 0x1000;
    Field_sphericalCamera.delta.pitch = 0x800;
    Field_sphericalCamera.values.yaw = 0x800;
    Field_sphericalCamera.values.pitch = -0x180;
    Field_sphericalCamera.initialDistance = 0x600;
    Field_sphericalCamera.values.distance = 0x600;
    Field_sphericalCamera.delta.yaw = 0;
    Field_sphericalCamera.delta.distance = 0;
    Field_sphericalCamera.delta.mode = 1;
    Field_sphericalCamera.zoomVelocity = 0;

    Actors_SetRoomActorScale(&vector);

    for (i = 0; i < 16; ++i) {
        Battle_actors[i] = NULL;
    }

    Actors_SpawnAshleyWithEquipment();
    (*(void* (*))&Battle_actionState) = Core_Memory_AllocateFromHighEnd(0x2C10);
    Core_Memory_ZeroAlignedWords((*(void* (*))&Battle_actionState), 0x2C10);
    Field_isFieldRenderingEnabled = 1;
    Boot_gameplayResetWordTwo = 0;
    Render_ConfigureFieldDisplay(0x140, 0xF0, Render_projectionDistance, 0, 0, 0);
}

