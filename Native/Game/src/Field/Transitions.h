// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_TRANSITIONS_H_TYPES
#define VS_FIELD_TRANSITIONS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_FIELD_TRANSITIONS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_TRANSITIONS_H_DECLS)
#define VS_FIELD_TRANSITIONS_H_DECLS
#include "Field/Doors.h"
#include "Field/Mpd.h"

extern u_int Field_screenTransitionStep;
extern int Field_screenTransitionEffect;
extern int Field_screenTransitionWipeAngle;
extern int Field_screenTransitionSpeed;
int Field_ScreenTransitionPhase(void);
void Field_ConfigureScreenTransition(int transition_effect, int transition_speed, int wipe_angle);
void Field_SetTransitionWipeAngle(int wipe_angle);
void Field_BeginScreenFadeOut(int effect, int speed, int angle);
void Field_BeginScreenFadeIn(int effect, int transition_speed, int wipe_angle);
int Field_BeginRoomTransition(Field_MpdDoorRecord* destination_door, int effect, int speed);
void Field_BeginScriptedRoomTransition(Field_MpdDoorRecord* destination_door, int transition_effect, int transition_speed);
int Field_BeginZoneTransition(Field_MpdDoorRecord* destination_door, int effect, int speed);
void Field_BeginScriptedZoneTransition(Field_MpdDoorRecord* destination_door, int transition_effect, int transition_speed);
int Field_TeleportToLocation(int location_id, int destination_tile_x, int destination_tile_z, int effect, int speed);
void Field_InitializeRoomTransitionPrimitives(void);

#endif // VS_FIELD_TRANSITIONS_H_DECLS
