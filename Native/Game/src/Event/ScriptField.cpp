// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Actors/RoomEnemyMemoryLive.hpp"
#include "Audio/MusicLive.hpp"
#include "Battle/RewardFlowLive.hpp"
#include "Core/Memory/CopyLive.hpp"
#include "Field/BackgroundLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/EnvironmentLive.hpp"
#include "Field/RoomEffectsLive.hpp"
#include "Field/RoomLoadingLive.hpp"
#include "Field/TransitionsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Render/RoomDraw.hpp"
#include "Render/RoomDrawLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/Hud.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptAudio.h"
#include "Event/ScriptField.h"
#include "Event/ScriptVm.h"
#include "Event/Weather.h"
#include "Field/CameraLiveState.h"
#include "Field/Doors.h"
#include "Field/Mpd.h"
#include "Field/PlatformPlacementLive.h"

// decomp: D_800E9BF0 (D_800E9BF0@BATTLE/BATTLE.PRG)
u_char Event_kMenuTransitionModes[320]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F4B88 (D_800F4B88@BATTLE/BATTLE.PRG)
Event_WeatherTween Event_weatherTween;

// decomp: BATTLE.PRG func_800B91C4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1780)
intptr_t Event_ScriptClearRoomActorCache(u_char* instruction, short thread_index)
{
    VS::Actors::ForgetLiveRoomEnemies(instruction[2], instruction[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800B92D0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1813)
intptr_t Event_ScriptSetHudVisible(u_char* instruction, short thread_index)
{
    Battle_SetGameplayHudHidden(instruction[1] ^ 1);
    return 0;
}

// decomp: BATTLE.PRG func_800B92F4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1819)
intptr_t Event_ScriptRunMenuTransition(u_char* script, short thread_index)
{
    switch (script[1]) {
    case 10:
    case 11:
    case 12:
        if (Event_isMenuTransitionActive == 0) {
            VS::State::SetLiveScriptFlag(0xB6, Event_kMenuTransitionModes[script[1] - 10]);
            VS::Battle::BeginLiveEventRewardResults();
            Event_isMenuTransitionActive = 1;
            return 1;
        }
        goto finishState;
    case 14:
        if (Event_isMenuTransitionActive == 0) {
            if (VS::State::LiveScriptFlag(0xB6) == 1) {
                VS::State::SetLiveScriptFlag(0xB6, 2);
            }
            VS::Battle::BeginLiveEventRewardResults();
            Event_musicSlot = 2;
            Event_isMenuTransitionActive = 1;
            return 1;
        }
    finishState:
        if (VS::Battle::UpdateLiveRewardSequence() != 0) {
            return 1;
        }
        VS::State::SetLiveScriptFlag(0xB6, 0);
        Event_isMenuTransitionActive = 0;
        return 4;
    case 13:
        if (Event_isMenuTransitionActive == 0) {
            VS::State::SetLiveScriptFlag(0xB6, 1);
            VS::Battle::BeginLiveEventRewardResults();
            Event_isMenuTransitionActive = 1;
            return 1;
        }
        goto finishTransition;
    case 15:
        if (Event_isMenuTransitionActive == 0) {
            VS::Battle::BeginLivePostActionRewards();
            Event_isMenuTransitionActive = 1;
            return 1;
        }
    finishTransition:
        if (VS::Battle::UpdateLiveRewardSequence() == 0) {
            Event_isMenuTransitionActive = 0;
            return 4;
        }
        return 1;
    default:
        if (Event_isMenuTransitionActive == 0) {
            VS::Menu::Framework::OpenLiveGameMenu(script[1], 2);
            Event_isMenuTransitionActive = 1;
            return 1;
        }
        VS::Menu::Framework::UpdateLiveMenuOverlay();
        if (VS::Menu::Framework::LiveActionSelectionState()->menuStatus == 2) {
            VS::Menu::Framework::ReleaseLiveMenuRows();
            Event_isMenuTransitionActive = 0;
            break;
        }
        return 1;
    }
    return 4;
}

// decomp: BATTLE.PRG func_800B9770 (src/BATTLE/BATTLE.PRG/4A0A8.c:1966)
intptr_t Event_ScriptSetGimmickVisibility(u_char* instruction, short thread_index)
{
    Field_SetMovingPlatformVisibilityEntry(instruction[1], instruction[2]);
    return 0;
}

// decomp: BATTLE.PRG func_800B979C (src/BATTLE/BATTLE.PRG/4A0A8.c:1972)
intptr_t Event_ScriptOverrideDepthCueDistance(u_char* instruction, short thread_index)
{
    short depth_cue_operand = VS::Core::Memory::ReadLiveUnalignedShort(instruction + 1);
    Event_isScriptBackdropEnabled = (depth_cue_operand << 0x10) >> 0x1F;
    if (Event_isScriptBackdropEnabled) {
        VS::Render::SetRoomTextureLodDepth(VS::Render::LiveRoomClipState(), depth_cue_operand & 0x7FF);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B97E4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1982)
intptr_t Event_ScriptSetRoomLightingMode(u_char* instruction, short thread_index)
{
    Field_roomGeometryDrawMode = instruction[1];
    return 0;
}

// decomp: BATTLE.PRG func_800B9818 (src/BATTLE/BATTLE.PRG/4A0A8.c:1994)
intptr_t Event_ScriptShowGeometryGroup(u_char* instruction, short thread_index)
{
    VS::Field::LiveShowRoomGeometryGroup(instruction[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800B983C (src/BATTLE/BATTLE.PRG/4A0A8.c:2000)
intptr_t Event_ScriptHideGeometryGroup(u_char* instruction, short thread_index)
{
    VS::Field::LiveHideRoomGeometryGroup(instruction[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800B9860 (src/BATTLE/BATTLE.PRG/4A0A8.c:2006)
intptr_t Event_ScriptSwapGeometryGroupFlags(u_char* instruction, short thread_index)
{
    VS::Field::LiveSwapRoomGeometryGroupFlags(instruction[1], instruction[2]);
    return 0;
}

// decomp: BATTLE.PRG func_800B988C (src/BATTLE/BATTLE.PRG/4A0A8.c:2012)
intptr_t Event_ScriptSetCollisionTileMode(u_char* instruction, short thread_index)
{
    VS::Field::SetLiveCollisionCellBlocked(instruction[1], instruction[3], instruction[2], instruction[4]);
    return 0;
}

// decomp: BATTLE.PRG func_800B98C0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2018)
intptr_t Event_ScriptChangeRoom(u_char* instruction, short thread_index)
{
    Field_MpdDoorRecord door_record;

    if (Event_roomChangeState == 0) {

        door_record.destinationZoneId = instruction[1];
        door_record.destinationRoomId = instruction[2];
        door_record.entryTileX = instruction[3];
        door_record.entryTileZ = instruction[5];
        door_record.tileLocation.entryDirection = instruction[4];
        door_record.entryElevation = instruction[6];
        door_record.hasEntryStep = instruction[7];
        door_record.reserved06Bit15 = instruction[7] >> 1;

        if (instruction[1] == 0) {
            VS::Field::LiveBeginScriptedRoomTransition(&door_record, instruction[8], instruction[9]);
        } else {
            VS::Audio::LiveSetZoneMusicSwitchEnabled((instruction[7] >> 3) & 1);
            VS::Field::LiveBeginScriptedZoneTransition(&door_record, instruction[8], instruction[9]);
        }
        Event_roomChangeState = instruction[7] & 4 ? 1 : 3;
    }

    if (Event_roomChangeState == 2) {
        Event_roomChangeState = 0;
        return 0;
    }
    return 1;
}

// decomp: BATTLE.PRG func_800B9F28 (src/BATTLE/BATTLE.PRG/4A0A8.c:2160)
intptr_t Event_ScriptSetRoomBackgroundVisible(u_char* instruction, short thread_index)
{
    VS::Field::LiveSetTiledBackgroundEnabled(instruction[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800B9F4C (src/BATTLE/BATTLE.PRG/4A0A8.c:2166)
intptr_t Event_ScriptLoadRoomBackground(u_char* instruction, short thread_index)
{
    VS::Field::LiveQueueTiledBackgroundLoad(instruction[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800B9F70 (src/BATTLE/BATTLE.PRG/4A0A8.c:2172)
intptr_t Event_ScriptWaitForRoomBackground(u_char* instruction, short thread_index) { return VS::Field::LiveFinishTiledBackgroundLoad() == 0; }

// decomp: BATTLE.PRG func_800B9F90 (src/BATTLE/BATTLE.PRG/4A0A8.c:2174)
intptr_t Event_ScriptFreeRoomBackground(u_char* instruction, short thread_index)
{
    VS::Field::LiveReleaseTiledBackground();
    return (Event_skipState == 2) * 4;
}

// decomp: BATTLE.PRG func_800B9FC0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2180)
intptr_t Event_ScriptSetEnvironmentParticles(u_char* instruction, short thread_index)
{
    VS::Field::LiveSetRoomWeather(instruction[1] >> 4);
    if (instruction[4] == 0) {
        Event_weatherTween.intensity = instruction[1] & 0xF;
        VS::Field::LiveSetWeatherIntensity(Event_weatherTween.intensity);
        Event_weatherTween.windDirection = instruction[2] * 16;
        Event_weatherTween.windStrength = instruction[3] * 4 + ONE / 8;
        VS::Field::LiveSetWeatherWindAngles(Event_weatherTween.windDirection, Event_weatherTween.windStrength);
    } else {
        int v = instruction[1] & 0xF;
        short base;
        int delta;

        Event_weatherTween.startIntensity = Event_weatherTween.intensity;
        Event_weatherTween.deltaIntensity = v - Event_weatherTween.intensity;
        Event_weatherTween.deltaWindStrength =
            instruction[3] * 4 - (base = (Event_weatherTween.startWindStrength = Event_weatherTween.windStrength) - ONE / 8);
        delta = instruction[2] * 16 - (Event_weatherTween.startWindDirection = Event_weatherTween.windDirection);
        if (delta < -ONE / 2) {
            delta += ONE;
        } else if (delta > ONE / 2) {
            delta -= ONE;
        }
        Event_weatherTween.deltaWindDirection = delta;
        Event_weatherTween.durationFrames = instruction[4];
        Event_weatherTween.elapsedFrames = 0;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800BA0E4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2211)
intptr_t Event_ScriptSetParticleVariationEnabled(u_char* instruction, short thread_index)
{
    VS::Field::LiveSetWeatherVariationEnabled(instruction[1] ^ 1);
    return 0;
}

// decomp: BATTLE.PRG func_800BA108 (src/BATTLE/BATTLE.PRG/4A0A8.c:2217)
intptr_t Event_ScriptMuteEnvironmentSound(u_char* instruction, short thread_index)
{
    if (instruction[1] != 0) {
        if (Event_isAmbientSoundSuspended != 0) {
            return 0;
        }

        Event_isAmbientSoundSuspended = VS::Field::LiveIsRoomAudioSequenceAbsent();

        if (Event_isAmbientSoundSuspended) {
            Event_suspendedAmbientSoundId = VS::Field::LiveSetWeatherAmbientSound(-1);
        }

    } else if (Event_isAmbientSoundSuspended != 0) {
        VS::Field::LiveSetWeatherAmbientSound(Event_suspendedAmbientSoundId);
        Event_isAmbientSoundSuspended = 0;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800BA194 (src/BATTLE/BATTLE.PRG/4A0A8.c:2238)
intptr_t Event_ScriptControlRoomAnimation(u_char* instruction, short thread_index)
{
    int room_effect_id = instruction[2];

    switch (instruction[1]) {
    case 0:
        if ((Event_skipState == 2) && (VS::Field::LiveIsRoomEffectActive(room_effect_id) != 0)) {
            VS::Field::LiveFinishRoomDoorRotation(room_effect_id);
        }
        VS::Field::LiveStartRoomEffect(room_effect_id);
        break;
    case 1:
        VS::Field::LiveToggleRoomEffectUpdates(room_effect_id);
        break;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800BA218 (src/BATTLE/BATTLE.PRG/4A0A8.c:2256)
intptr_t Event_ScriptSetBackgroundOffset(u_char* instruction, short thread_index)
{
    VS::Field::LiveSetRoomBackgroundOffset(VS::Core::Memory::ReadLiveUnalignedShort(instruction + 1), VS::Core::Memory::ReadLiveUnalignedShort(instruction + 3));
    return 0;
}

// decomp: BATTLE.PRG func_800BA264 (src/BATTLE/BATTLE.PRG/4A0A8.c:2262)
intptr_t Event_ScriptSetFirstPersonView(u_char* instruction, short thread_index)
{
    VS::Field::LiveSetFirstPersonViewDisabled(instruction[1] != 0);
    return 0;
}

// decomp: BATTLE.PRG func_800BA288 (src/BATTLE/BATTLE.PRG/4A0A8.c:2268)
intptr_t Event_ScriptAdjustGeometryDepthBias(u_char* instruction, short thread_index)
{
    VS::Field::LiveAddRoomGeometryDepthBias(instruction[1], (signed char)instruction[2]);
    return 0;
}

// decomp: BATTLE.PRG func_800BA2B4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2274)
intptr_t Event_ScriptSetBackgroundParallax(u_char* instruction, short thread_index)
{
    VS::Field::LiveSetBackgroundParallaxScale((signed char)instruction[1] << 7, (signed char)instruction[2] << 7);
    return 0;
}

}  // extern "C"
