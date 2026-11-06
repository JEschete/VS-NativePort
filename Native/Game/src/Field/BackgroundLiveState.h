#pragma once
// Hand-maintained (tools/port/owned.csv).
// The room background globals as the copied room loader, weather effects,
// room state and snapshot read and write them. BackgroundLive.cpp defines
// them and mirrors them into the typed RoomBackgroundState
// (Field/Background.hpp) around each call.

#include <stdint.h>

#include "DiscQueue.h"
#include "Field/Background.h"
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

// decomp: D_800F1CE0 (BATTLE/BATTLE.PRG 146C.h:1113): zero vectors; [0] is
// the sky dome's turn outside a room.
extern VECTOR Field_backgroundReferenceVectors[2];
// decomp: D_800F1D78 (BATTLE/BATTLE.PRG 146C.c:704): the five gradient
// colours last spread over the sky dome.
enum { kFieldBackgroundGradientColorCount = 5 };
extern int Field_backgroundGradientColors[kFieldBackgroundGradientColorCount];
// decomp: D_800F1DAC (BATTLE/BATTLE.PRG 146C.c:711): the tiled background's
// TIM read, or null.
extern Content_DiscQueueSlot* Field_backgroundTextureDiscRequest;
// decomp: D_800F1DB0 (BATTLE/BATTLE.PRG 146C.c:712): its layout read, or
// null.
extern Content_DiscQueueSlot* Field_backgroundLayoutDiscRequest;
// decomp: D_800F1DB4 and D_800F1DB6 (BATTLE/BATTLE.PRG 146C.c:713): the
// tiled background's scroll offset.
extern int16_t Field_backgroundOffsetX;
extern int16_t Field_backgroundOffsetY;
// decomp: D_800F1DB8 and D_800F1DBA (BATTLE/BATTLE.PRG 146C.c:715): the Q12
// parallax of the camera's yaw and pitch.
extern int16_t Field_backgroundParallaxScaleXFixed12;
extern int16_t Field_backgroundParallaxScaleYFixed12;
// decomp: D_800F1DBE (BATTLE/BATTLE.PRG 146C.c:718): how many texture pages
// the tiled background holds.
extern int16_t Field_legacyBackgroundTextureReservationId;
// decomp: D_800F1DC6 (BATTLE/BATTLE.PRG 146C.c:722): 1 when the room's five
// colours match and a solid backdrop replaces the dome.
extern char Field_isSolidBackground;
// decomp: D_800F1DC8 (BATTLE/BATTLE.PRG 146C.c:723): the tiled background's
// first texture page.
extern char Field_legacyBackgroundTexturePage;
// decomp: D_800F1DC9 (BATTLE/BATTLE.PRG 146C.c:724): 1 while the tiled
// background is drawn.
extern u_char Field_isTiledBackgroundEnabled;
// decomp: D_800F1DCC (BATTLE/BATTLE.PRG 146C.c:727): the tiled background's
// layout (high heap), or null.
extern Field_BackgroundLayout* Field_loadedBackgroundLayout;
// decomp: D_800F1DD0 (BATTLE/BATTLE.PRG 146C.c:728): its TIM until the
// upload (high heap), or null.
extern u_long* Field_legacyBackgroundTimBuffer;

#ifdef __cplusplus
}
#endif
