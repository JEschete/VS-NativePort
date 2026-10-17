// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Music.h"
#include "Battle/Hud.h"
#include "Battle/Rewards.h"
#include "Core/Memory/Copy.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptAudio.h"
#include "Event/ScriptField.h"
#include "Event/ScriptVm.h"
#include "Event/Weather.h"
#include "Field/Background.h"
#include "Field/Camera.h"
#include "Field/Collision.h"
#include "Field/Doors.h"
#include "Field/Environment.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/RoomEffects.h"
#include "Field/RoomLoading.h"
#include "Field/RoomState.h"
#include "Field/Transitions.h"
#include "Menu/Framework/ActionSelection.h"
#include "Menu/Framework/Framework.h"
#include "Render/RoomGeometry.h"
#include "State/Flags.h"

// decomp: D_800E9BF0 (D_800E9BF0@BATTLE/BATTLE.PRG)
u_char Event_kMenuTransitionModes[320]; /* contents: module image (ModuleData.c) */

// decomp: D_800F4B88 (D_800F4B88@BATTLE/BATTLE.PRG)
Event_WeatherTween Event_weatherTween;

// decomp: BATTLE.PRG func_800B91C4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1780)
int Event_ScriptPreloadRoom(u_char* arg0, short arg1)
{
    Field_ClearSavedRoomEnemies(arg0[2], arg0[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800B92D0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1813)
int Event_ScriptSetHudVisible(u_char* arg0, short arg1)
{
    Battle_SetGameplayHudHidden(arg0[1] ^ 1);
    return 0;
}

// decomp: BATTLE.PRG func_800B92F4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1819)
int Event_ScriptRunMenuTransition(u_char* script, short arg1)
{
    switch (script[1]) {
    case 10:
    case 11:
    case 12:
        if (Event_isMenuTransitionActive == 0) {
            State_SetScriptFlag(0xB6, Event_kMenuTransitionModes[script[1] - 10]);
            Battle_BeginEventRewardResults();
            Event_isMenuTransitionActive = 1;
            return 1;
        }
        goto finishState;
    case 14:
        if (Event_isMenuTransitionActive == 0) {
            if (State_ScriptFlag(0xB6) == 1) {
                State_SetScriptFlag(0xB6, 2);
            }
            Battle_BeginEventRewardResults();
            Event_musicSlot = 2;
            Event_isMenuTransitionActive = 1;
            return 1;
        }
    finishState:
        if (Battle_UpdateRewardSequence() != 0) {
            return 1;
        }
        State_SetScriptFlag(0xB6, 0);
        Event_isMenuTransitionActive = 0;
        return 4;
    case 13:
        if (Event_isMenuTransitionActive == 0) {
            State_SetScriptFlag(0xB6, 1);
            Battle_BeginEventRewardResults();
            Event_isMenuTransitionActive = 1;
            return 1;
        }
        goto finishTransition;
    case 15:
        if (Event_isMenuTransitionActive == 0) {
            Battle_BeginPostActionRewards();
            Event_isMenuTransitionActive = 1;
            return 1;
        }
    finishTransition:
        if (Battle_UpdateRewardSequence() == 0) {
            Event_isMenuTransitionActive = 0;
            return 4;
        }
        return 1;
    default:
        if (Event_isMenuTransitionActive == 0) {
            Menu_Framework_OpenGameMenu(script[1], 2);
            Event_isMenuTransitionActive = 1;
            return 1;
        }
        Menu_Framework_UpdateMenuOverlay();
        if (Menu_Framework_ActionSelectionState()->menuStatus == 2) {
            Menu_Framework_ReleaseMenuRows();
            Event_isMenuTransitionActive = 0;
            break;
        }
        return 1;
    }
    return 4;
}

// decomp: BATTLE.PRG func_800B9770 (src/BATTLE/BATTLE.PRG/4A0A8.c:1966)
int Event_ScriptSetGimmickVisibility(u_char* arg0, short arg1)
{
    Field_SetMovingPlatformVisibility(arg0[1], arg0[2]);
    return 0;
}

// decomp: BATTLE.PRG func_800B979C (src/BATTLE/BATTLE.PRG/4A0A8.c:1972)
int Event_ScriptOverrideDepthCueDistance(u_char* arg0, short arg1)
{
    short temp_v0 = Core_Memory_ReadUnalignedShort(arg0 + 1);
    Event_isScriptBackdropEnabled = (temp_v0 << 0x10) >> 0x1F;
    if (Event_isScriptBackdropEnabled) {
        Render_SetRoomTextureLodDepth(temp_v0 & 0x7FF);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B97E4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1982)
int Event_ScriptSetRoomLightingMode(u_char* arg0, short arg1)
{
    Field_roomGeometryDrawMode = arg0[1];
    return 0;
}

// decomp: BATTLE.PRG func_800B9818 (src/BATTLE/BATTLE.PRG/4A0A8.c:1994)
int Event_ScriptShowGeometryGroup(u_char* arg0, short arg1)
{
    Field_ShowRoomGeometryGroup(arg0[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800B983C (src/BATTLE/BATTLE.PRG/4A0A8.c:2000)
int Event_ScriptHideGeometryGroup(u_char* arg0, short arg1)
{
    Field_HideRoomGeometryGroup(arg0[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800B9860 (src/BATTLE/BATTLE.PRG/4A0A8.c:2006)
int Event_ScriptSwapGeometryGroupFlags(u_char* arg0, short arg1)
{
    Field_SwapRoomGeometryGroupFlags(arg0[1], arg0[2]);
    return 0;
}

// decomp: BATTLE.PRG func_800B988C (src/BATTLE/BATTLE.PRG/4A0A8.c:2012)
int Event_ScriptSetCollisionTileMode(u_char* arg0, short arg1)
{
    Field_SetCollisionCellBlocked(arg0[1], arg0[3], arg0[2], arg0[4]);
    return 0;
}

// decomp: BATTLE.PRG func_800B98C0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2018)
int Event_ScriptChangeRoom(u_char* arg0, short arg1)
{
    Field_MpdDoorRecord sp10;

    if (Event_roomChangeState == 0) {

        sp10.destinationZoneId = arg0[1];
        sp10.destinationRoomId = arg0[2];
        sp10.entryTileX = arg0[3];
        sp10.entryTileZ = arg0[5];
        sp10.tileLocation.entryDirection = arg0[4];
        sp10.entryElevation = arg0[6];
        sp10.hasEntryStep = arg0[7];
        sp10.reserved06Bit15 = arg0[7] >> 1;

        if (arg0[1] == 0) {
            Field_BeginScriptedRoomTransition(&sp10, arg0[8], arg0[9]);
        } else {
            Audio_SetZoneMusicSwitchEnabled((arg0[7] >> 3) & 1);
            Field_BeginScriptedZoneTransition(&sp10, arg0[8], arg0[9]);
        }
        Event_roomChangeState = arg0[7] & 4 ? 1 : 3;
    }

    if (Event_roomChangeState == 2) {
        Event_roomChangeState = 0;
        return 0;
    }
    return 1;
}

// decomp: BATTLE.PRG func_800B9F28 (src/BATTLE/BATTLE.PRG/4A0A8.c:2160)
int Event_ScriptSetRoomBackgroundVisible(u_char* arg0, short arg1)
{
    Field_SetTiledBackgroundEnabled(arg0[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800B9F4C (src/BATTLE/BATTLE.PRG/4A0A8.c:2166)
int Event_ScriptLoadRoomBackground(u_char* arg0, short arg1)
{
    Field_QueueTiledBackgroundLoad(arg0[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800B9F70 (src/BATTLE/BATTLE.PRG/4A0A8.c:2172)
int Event_ScriptWaitForRoomBackground(u_char* arg0, short arg1) { return Field_FinishTiledBackgroundLoad() == 0; }

// decomp: BATTLE.PRG func_800B9F90 (src/BATTLE/BATTLE.PRG/4A0A8.c:2174)
int Event_ScriptFreeRoomBackground(u_char* arg0, short arg1)
{
    Field_ReleaseTiledBackground();
    return (Event_skipState == 2) * 4;
}

// decomp: BATTLE.PRG func_800B9FC0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2180)
int Event_ScriptSetEnvironmentParticles(u_char* arg0, short arg1)
{
    ((int (*)(int))Field_SetRoomWeather)(arg0[1] >> 4);
    if (arg0[4] == 0) {
        Event_weatherTween.intensity = arg0[1] & 0xF;
        Field_SetWeatherIntensity(Event_weatherTween.intensity);
        Event_weatherTween.windDirection = arg0[2] * 16;
        Event_weatherTween.windStrength = arg0[3] * 4 + ONE / 8;
        Field_SetWeatherWindAngles(Event_weatherTween.windDirection, Event_weatherTween.windStrength);
    } else {
        int v = arg0[1] & 0xF;
        short base;
        int delta;

        Event_weatherTween.startIntensity = Event_weatherTween.intensity;
        Event_weatherTween.deltaIntensity = v - Event_weatherTween.intensity;
        Event_weatherTween.deltaWindStrength =
            arg0[3] * 4 - (base = (Event_weatherTween.startWindStrength = Event_weatherTween.windStrength) - ONE / 8);
        delta = arg0[2] * 16 - (Event_weatherTween.startWindDirection = Event_weatherTween.windDirection);
        if (delta < -ONE / 2) {
            delta += ONE;
        } else if (delta > ONE / 2) {
            delta -= ONE;
        }
        Event_weatherTween.deltaWindDirection = delta;
        Event_weatherTween.durationFrames = arg0[4];
        Event_weatherTween.elapsedFrames = 0;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800BA0E4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2211)
int Event_ScriptSetParticleVariationEnabled(u_char* arg0, short arg1)
{
    Field_SetWeatherVariationEnabled(arg0[1] ^ 1);
    return 0;
}

// decomp: BATTLE.PRG func_800BA108 (src/BATTLE/BATTLE.PRG/4A0A8.c:2217)
int Event_ScriptMuteEnvironmentSound(u_char* arg0, short arg1)
{
    if (arg0[1] != 0) {
        if (Event_isAmbientSoundSuspended != 0) {
            return 0;
        }

        Event_isAmbientSoundSuspended = Field_IsRoomAudioSequenceAbsent();

        if (Event_isAmbientSoundSuspended) {
            Event_suspendedAmbientSoundId = Field_SetWeatherAmbientSound(-1);
        }

    } else if (Event_isAmbientSoundSuspended != 0) {
        Field_SetWeatherAmbientSound(Event_suspendedAmbientSoundId);
        Event_isAmbientSoundSuspended = 0;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800BA194 (src/BATTLE/BATTLE.PRG/4A0A8.c:2238)
int Event_ScriptControlRoomAnimation(u_char* arg0, short arg1)
{
    int temp_s0 = arg0[2];

    switch (arg0[1]) {
    case 0:
        if ((Event_skipState == 2) && (Field_IsRoomEffectActive(temp_s0) != 0)) {
            Field_FinishRoomDoorRotation(temp_s0);
        }
        Field_StartRoomEffect(temp_s0);
        break;
    case 1:
        Field_ToggleRoomEffectUpdates(temp_s0);
        break;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800BA218 (src/BATTLE/BATTLE.PRG/4A0A8.c:2256)
int Event_ScriptSetBackgroundOffset(u_char* arg0, short arg1)
{
    Field_SetRoomBackgroundOffset(Core_Memory_ReadUnalignedShort(arg0 + 1), Core_Memory_ReadUnalignedShort(arg0 + 3));
    return 0;
}

// decomp: BATTLE.PRG func_800BA264 (src/BATTLE/BATTLE.PRG/4A0A8.c:2262)
int Event_ScriptSetFirstPersonView(u_char* arg0, short arg1)
{
    Field_SetFirstPersonViewDisabled(arg0[1] != 0);
    return 0;
}

// decomp: BATTLE.PRG func_800BA288 (src/BATTLE/BATTLE.PRG/4A0A8.c:2268)
int Event_ScriptAdjustGeometryDepthBias(u_char* arg0, short arg1)
{
    Field_AddRoomGeometryDepthBias(arg0[1], (signed char)arg0[2]);
    return 0;
}

// decomp: BATTLE.PRG func_800BA2B4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2274)
int Event_ScriptSetBackgroundParallax(u_char* arg0, short arg1)
{
    Field_SetBackgroundParallaxScale((signed char)arg0[1] << 7, (signed char)arg0[2] << 7);
    return 0;
}

