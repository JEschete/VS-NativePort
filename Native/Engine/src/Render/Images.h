// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_IMAGES_H_TYPES
#define VS_RENDER_IMAGES_H_TYPES
#include "Legacy/Psx.h"

typedef struct Content_DiscQueueSlot Content_DiscQueueSlot;
typedef struct Render_GimLayer Render_GimLayer;
typedef struct Render_TextureRegion Render_TextureRegion;

// decomp: src/include/gpu.h:112
#define vs_getUV(u, v) ((u) | ((v) << 8))

// decomp: src/include/lbaMacros.h:3
#define squashLba(file) squashLbaVal((file##_LBA), (file##_SIZE))

// decomp: _texture_t (src/include/texture_t.h:12)
typedef struct Render_TextureRegion {
    char x;
    char y;
    char w;
    char h;
    u_short tpage;
    u_short clut;
} Render_TextureRegion;
_Static_assert(sizeof(Render_TextureRegion) == 0x8, "Render_TextureRegion must keep its PS1 layout");

// decomp: gim_t (src/BATTLE/BATTLE.PRG/5BF94.c:82)
typedef struct Render_GimLayer {
    u_int drawEnabled : 1;
    u_int loading : 1;
    u_int textureMode : 1;
    u_int drawRoutine : 3;
    u_int loadPhase : 3;
    u_int texturePageCount : 3;
    u_int ownsPalette : 1;
    u_int reservedBits13 : 3;
    char vramPage;
    char brightness;
    u_char columns;
    u_char rows;
    short imageId;
    short offsetX;
    short offsetY;
    short scaleAndOrder;
    u_short backdropColor;
    u_short* fileData;
    Content_DiscQueueSlot* discRequest;
    u_short scanlineWipeWidths[240];
    u_short tileTable[254];
    int reserved3F4;
} Render_GimLayer;

#endif // VS_RENDER_IMAGES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_RENDER_IMAGES_H_DECLS)
#define VS_RENDER_IMAGES_H_DECLS
#include "Event/Splash.h"

extern Event_SplashLayerView* Render_gimLayers;
extern void (*Render_kGimLayerDrawRoutines[4])(Render_GimLayer*, int, u_long*);
extern char Render_gimBlendSelect;
extern int Render_legacyGimDiscFiles[];
void Render_QueueGimImage(int id, int arg1);
void Render_ActivateGimLayer(u_int arg0, void* arg1, u_char* arg2);
void Render_ReleaseGimImages(void);
void Render_UpdateGimImages(void);
void Render_DrawGimTiles(Render_GimLayer* image, int clut, u_long* ot);
void Render_DrawGimFramebufferFade(Render_GimLayer* arg0, int arg1, u_long* arg2);
void Render_DrawGimScanlineWipe(Render_GimLayer* image, int clut, u_long* ot);
void Render_SetGimWorkspaceDrawArea(u_long* arg0, int arg1);
void Render_SetGimFramebufferDrawArea(u_long* arg0, int arg1);
void Render_DrawGimCrossfade(Render_GimLayer* image, int clut, u_long* ot);
void Render_DrawGimLayer(int arg0);

#endif // VS_RENDER_IMAGES_H_DECLS
