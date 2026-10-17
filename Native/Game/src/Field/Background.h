// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_BACKGROUND_H_TYPES
#define VS_FIELD_BACKGROUND_H_TYPES
#include "Legacy/Psx.h"

typedef struct Field_BackgroundLayout Field_BackgroundLayout;
typedef struct Field_BackgroundTile Field_BackgroundTile;

// decomp: vs_battle_backgroundTile (src/BATTLE/BATTLE.PRG/2842C.h:9)
typedef struct Field_BackgroundTile {
    RECT textureRect;
    CVECTOR cornerColors[4];
} Field_BackgroundTile;
_Static_assert(sizeof(Field_BackgroundTile) == 0x18, "Field_BackgroundTile must keep its PS1 layout");

// decomp: vs_battle_backgroundLayout (src/BATTLE/BATTLE.PRG/2842C.h:16)
typedef struct Field_BackgroundLayout {
    int width;
    int height;
    int reserved08;
    Field_BackgroundTile tiles[0];
} Field_BackgroundLayout;
_Static_assert(sizeof(Field_BackgroundLayout) == 0xC, "Field_BackgroundLayout must keep its PS1 layout");

#endif // VS_FIELD_BACKGROUND_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_BACKGROUND_H_DECLS)
#define VS_FIELD_BACKGROUND_H_DECLS
#include "DiscQueue.h"

extern int Field_legacyBackgroundGradientPackets[1344];
extern VECTOR Field_backgroundReferenceVectors[2];
extern int Field_backgroundGradientColors[5];
extern Content_DiscQueueSlot* Field_backgroundTextureDiscRequest;
extern Content_DiscQueueSlot* Field_backgroundLayoutDiscRequest;
extern short Field_backgroundOffsetX;
extern short Field_backgroundOffsetY;
extern short Field_backgroundParallaxScaleXFixed12;
extern short Field_backgroundParallaxScaleYFixed12;
extern short Field_legacyBackgroundTextureReservationId;
extern char Field_isSolidBackground;
extern char Field_legacyBackgroundTexturePage;
extern u_char Field_isTiledBackgroundEnabled;
extern Field_BackgroundLayout* Field_loadedBackgroundLayout;
extern u_long* Field_legacyBackgroundTimBuffer;
void Field_InitializeRoomBackground(void);
void Field_InitializeRoomBackgroundColors(void);
void Field_SetBackgroundParallaxScale(int arg0, int arg1);
void Field_SetTiledBackgroundEnabled(int arg0);
int* Field_RoomBackgroundColors(void);
void Field_SetRoomBackgroundColors(int* arg0);
void Field_DrawRoomBackground(VECTOR* arg0);
int Field_QueueTiledBackgroundLoad(int arg0);
int Field_FinishTiledBackgroundLoad(void);
int Field_ReleaseTiledBackground(void);

#endif // VS_FIELD_BACKGROUND_H_DECLS
