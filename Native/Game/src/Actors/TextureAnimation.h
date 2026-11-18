// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_TEXTUREANIMATION_H_TYPES
#define VS_ACTORS_TEXTUREANIMATION_H_TYPES
#include "Legacy/Psx.h"

struct Actors_TextureAnimationFrame;
struct Actors_TextureAnimationKey;
struct Actors_TextureAnimationSlot;
struct Actors_TextureCellOrigin;
struct Actors_TextureOverlayRect;
struct Actors_TextureOverlayUpload;
struct Actors_TextureScrollBounds;
struct Actors_TextureScrollState;

// decomp: D_800F4538_unk180C (src/BATTLE/BATTLE.PRG/3A1A0.h:198)
struct Actors_TextureAnimationSlot {
    u_char timeline : 4;
    u_char mode : 4;
    u_char currentFrame;
    u_char keyIndex;
    u_char ticks;
};
static_assert(sizeof(Actors_TextureAnimationSlot) == 0x4, "Actors_TextureAnimationSlot must keep its PS1 layout");

// decomp: D_800F4538_unk181C (src/BATTLE/BATTLE.PRG/3A1A0.h:242)
struct Actors_TextureScrollState {
    u_char x;
    u_char y;
    signed char stepX;
    signed char stepY;
    u_char interval;
    u_char timer;
    u_char pad6;
    u_char pad7;
};
static_assert(sizeof(Actors_TextureScrollState) == 0x8, "Actors_TextureScrollState must keep its PS1 layout");

// decomp: D_800F4538_unk614 (src/BATTLE/BATTLE.PRG/3A1A0.h:212)
struct Actors_TextureCellOrigin {
    u_char x;
    u_char y;
    u_char columns;
    u_char reserved3;
};
static_assert(sizeof(Actors_TextureCellOrigin) == 0x4, "Actors_TextureCellOrigin must keep its PS1 layout");

// decomp: D_800F4538_unk61C (src/BATTLE/BATTLE.PRG/3A1A0.h:231)
struct Actors_TextureScrollBounds {
    u_char x;
    u_char y;
    u_char width;
    u_char height;
    u_char initialStepX;
    u_char initialStepY;
    u_char bounces : 1;
    u_char reservedBits1 : 7;
    u_char interval;
};
static_assert(sizeof(Actors_TextureScrollBounds) == 0x8, "Actors_TextureScrollBounds must keep its PS1 layout");

// decomp: func_800AAE9C_t (src/BATTLE/BATTLE.PRG/40564.c:12)
struct Actors_TextureAnimationKey {
    u_char frame;
    u_char duration;
};
static_assert(sizeof(Actors_TextureAnimationKey) == 0x2, "Actors_TextureAnimationKey must keep its PS1 layout");

// decomp: func_800AB358_t (src/BATTLE/BATTLE.PRG/40564.c:32)
struct Actors_TextureOverlayUpload {
    u_char reserved0;
    u_char overlayIndex;
    u_char sourceX;
    u_char sourceY;
};
static_assert(sizeof(Actors_TextureOverlayUpload) == 0x4, "Actors_TextureOverlayUpload must keep its PS1 layout");

// decomp: texMapAnimation_t (src/BATTLE/BATTLE.PRG/3A1A0.h:219)
struct Actors_TextureAnimationFrame {
    u_char overlayIndex;
    u_char sourceX;
    u_char sourceY;
    u_char columns;
};
static_assert(sizeof(Actors_TextureAnimationFrame) == 0x4, "Actors_TextureAnimationFrame must keep its PS1 layout");

// decomp: texMapOverlay_t (src/BATTLE/BATTLE.PRG/3A1A0.h:205)
struct Actors_TextureOverlayRect {
    u_char x;
    u_char y;
    u_char width;
    u_char height;
};
static_assert(sizeof(Actors_TextureOverlayRect) == 0x4, "Actors_TextureOverlayRect must keep its PS1 layout");

#endif // VS_ACTORS_TEXTUREANIMATION_H_TYPES
