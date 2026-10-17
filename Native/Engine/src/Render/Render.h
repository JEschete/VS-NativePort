// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_RENDER_H_TYPES
#define VS_RENDER_RENDER_H_TYPES
#include "Legacy/Psx.h"

typedef struct Render_Camera Render_Camera;
typedef struct Render_FrameScratch Render_FrameScratch;
typedef struct Render_ScreenOffset Render_ScreenOffset;

// decomp: vs_dithering (src/include/gpu.h:15)
enum Render_Dithering { Render_Dithering_Off = 0, Render_Dithering_On = 1 };
typedef enum Render_Dithering Render_Dithering;

// decomp: vs_semiTransparencyRate (src/include/gpu.h:8)
enum Render_BlendMode {
    Render_BlendMode_Average = 0,
    Render_BlendMode_Add = 1,
    Render_BlendMode_Subtract = 2,
    Render_BlendMode_AddQuarter = 3
};
typedef enum Render_BlendMode Render_BlendMode;

// decomp: vs_texMode (src/include/gpu.h:6)
enum Render_TextureFormat { Render_TextureFormat_Palette4Bit = 0, Render_TextureFormat_Palette8Bit = 1, Render_TextureFormat_Direct16Bit = 2 };
typedef enum Render_TextureFormat Render_TextureFormat;

// decomp: vs_battle_geomOffset (src/BATTLE/BATTLE.PRG/2EA3C.h:7)
typedef struct Render_ScreenOffset {
    int x;
    int y;
} Render_ScreenOffset;
_Static_assert(sizeof(Render_ScreenOffset) == 0x8, "Render_ScreenOffset must keep its PS1 layout");

// decomp: vs_camera_t (src/include/scratch.h:10)
typedef struct Render_Camera {
    /* 0x00 */ VECTOR position;
    /* 0x10 */ VECTOR lookAt;
    /* 0x20 */ VECTOR angles;
    /* 0x30 */ int farClip;
} Render_Camera;
_Static_assert(sizeof(Render_Camera) == 0x34, "Render_Camera must keep its PS1 layout");

// decomp: vs_scratch_t (src/include/scratch.h:20)
typedef struct Render_FrameScratch {
    /* 0x00 */ void* primitiveCursor;
    /* 0x04 */ void* orderingTable;
    /* 0x08 */ void* overlayOrderingTable;
    /* 0x0C */ void* reservedC;
    /* 0x10 */ u_int firstPersonViewFacing;
    /* 0x14 */ MATRIX viewMatrix;
    /* 0x34 */ Render_Camera camera;
} Render_FrameScratch;

#endif // VS_RENDER_RENDER_H_TYPES
