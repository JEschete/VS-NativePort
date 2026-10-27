// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_TEXTUREANIMATION_H_TYPES
#define VS_ACTORS_TEXTUREANIMATION_H_TYPES
#include "Legacy/Psx.h"

typedef struct Actors_TextureAnimationFrame Actors_TextureAnimationFrame;
typedef struct Actors_TextureAnimationKey Actors_TextureAnimationKey;
typedef struct Actors_TextureAnimationSlot Actors_TextureAnimationSlot;
typedef struct Actors_TextureCellOrigin Actors_TextureCellOrigin;
typedef struct Actors_TextureOverlayRect Actors_TextureOverlayRect;
typedef struct Actors_TextureOverlayUpload Actors_TextureOverlayUpload;
typedef struct Actors_TextureScrollBounds Actors_TextureScrollBounds;
typedef struct Actors_TextureScrollState Actors_TextureScrollState;

// decomp: D_800F4538_unk180C (src/BATTLE/BATTLE.PRG/3A1A0.h:198)
typedef struct Actors_TextureAnimationSlot {
    u_char timeline : 4;
    u_char mode : 4;
    u_char currentFrame;
    u_char keyIndex;
    u_char ticks;
} Actors_TextureAnimationSlot;
_Static_assert(sizeof(Actors_TextureAnimationSlot) == 0x4, "Actors_TextureAnimationSlot must keep its PS1 layout");

// decomp: D_800F4538_unk181C (src/BATTLE/BATTLE.PRG/3A1A0.h:242)
typedef struct Actors_TextureScrollState {
    u_char x;
    u_char y;
    signed char stepX;
    signed char stepY;
    u_char interval;
    u_char timer;
    u_char pad6;
    u_char pad7;
} Actors_TextureScrollState;
_Static_assert(sizeof(Actors_TextureScrollState) == 0x8, "Actors_TextureScrollState must keep its PS1 layout");

// decomp: D_800F4538_unk614 (src/BATTLE/BATTLE.PRG/3A1A0.h:212)
typedef struct Actors_TextureCellOrigin {
    u_char x;
    u_char y;
    u_char columns;
    u_char reserved3;
} Actors_TextureCellOrigin;
_Static_assert(sizeof(Actors_TextureCellOrigin) == 0x4, "Actors_TextureCellOrigin must keep its PS1 layout");

// decomp: D_800F4538_unk61C (src/BATTLE/BATTLE.PRG/3A1A0.h:231)
typedef struct Actors_TextureScrollBounds {
    u_char x;
    u_char y;
    u_char width;
    u_char height;
    u_char initialStepX;
    u_char initialStepY;
    u_char bounces : 1;
    u_char reservedBits1 : 7;
    u_char interval;
} Actors_TextureScrollBounds;
_Static_assert(sizeof(Actors_TextureScrollBounds) == 0x8, "Actors_TextureScrollBounds must keep its PS1 layout");

// decomp: func_800AAE9C_t (src/BATTLE/BATTLE.PRG/40564.c:12)
typedef struct Actors_TextureAnimationKey {
    u_char frame;
    u_char duration;
} Actors_TextureAnimationKey;
_Static_assert(sizeof(Actors_TextureAnimationKey) == 0x2, "Actors_TextureAnimationKey must keep its PS1 layout");

// decomp: func_800AB358_t (src/BATTLE/BATTLE.PRG/40564.c:32)
typedef struct Actors_TextureOverlayUpload {
    u_char reserved0;
    u_char overlayIndex;
    u_char sourceX;
    u_char sourceY;
} Actors_TextureOverlayUpload;
_Static_assert(sizeof(Actors_TextureOverlayUpload) == 0x4, "Actors_TextureOverlayUpload must keep its PS1 layout");

// decomp: texMapAnimation_t (src/BATTLE/BATTLE.PRG/3A1A0.h:219)
typedef struct Actors_TextureAnimationFrame {
    u_char overlayIndex;
    u_char sourceX;
    u_char sourceY;
    u_char columns;
} Actors_TextureAnimationFrame;
_Static_assert(sizeof(Actors_TextureAnimationFrame) == 0x4, "Actors_TextureAnimationFrame must keep its PS1 layout");

// decomp: texMapOverlay_t (src/BATTLE/BATTLE.PRG/3A1A0.h:205)
typedef struct Actors_TextureOverlayRect {
    u_char x;
    u_char y;
    u_char width;
    u_char height;
} Actors_TextureOverlayRect;
_Static_assert(sizeof(Actors_TextureOverlayRect) == 0x4, "Actors_TextureOverlayRect must keep its PS1 layout");

#endif // VS_ACTORS_TEXTUREANIMATION_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ACTORS_TEXTUREANIMATION_H_DECLS)
#define VS_ACTORS_TEXTUREANIMATION_H_DECLS
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Render/Palette.h"

int Actors_SetActorTextureAnimation(int actorId, int index, int value, int mode);
int Actors_UpdateActorTextureAnimations(Actors_ActorRecord* actor);
void Actors_UploadActorTextureFrame(Actors_ActorRecord* actor, int index, int frame);
void Actors_ResetActorTextureFrames(int bank_index);
void Actors_UploadActorTextureOverlay(int index, Actors_TextureOverlayUpload* arg);
void Actors_UpdateActorTextureScrolling(Actors_ActorRecord* actor);

#endif // VS_ACTORS_TEXTUREANIMATION_H_DECLS
