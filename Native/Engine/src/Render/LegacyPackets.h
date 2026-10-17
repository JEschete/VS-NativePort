// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_LEGACYPACKETS_H_TYPES
#define VS_RENDER_LEGACYPACKETS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Render_ClosedRingPacket Render_ClosedRingPacket;
typedef struct Render_ClosedRingTexCoord Render_ClosedRingTexCoord;
typedef struct Render_EffectFlatQuadPacket Render_EffectFlatQuadPacket;
typedef struct Render_EffectGouraudQuadPacket Render_EffectGouraudQuadPacket;
typedef struct Render_GouraudQuadPacket Render_GouraudQuadPacket;
typedef struct Render_GridPacket Render_GridPacket;
typedef struct Render_Mesh10Packet Render_Mesh10Packet;
typedef struct Render_Mesh10TexCoord Render_Mesh10TexCoord;
typedef struct Render_MirrorSurfacePacket Render_MirrorSurfacePacket;
typedef struct Render_PagedGouraudQuadPacket Render_PagedGouraudQuadPacket;
typedef struct Render_ParticleLinePacket Render_ParticleLinePacket;
typedef struct Render_ParticleQuadPacket Render_ParticleQuadPacket;
typedef struct Render_RingPacket Render_RingPacket;
typedef struct Render_RotatingSurfacePacket Render_RotatingSurfacePacket;
typedef struct Render_SpriteTexturePagePacket Render_SpriteTexturePagePacket;
typedef struct Render_SurfacePacket Render_SurfacePacket;
typedef struct Render_TilePacket Render_TilePacket;
typedef struct Render_TileTexturePagePacket Render_TileTexturePagePacket;
typedef struct Render_TiledRingPacket Render_TiledRingPacket;
typedef struct Render_TiledRingTexCoord Render_TiledRingTexCoord;
typedef struct Render_Trail9Packet Render_Trail9Packet;
typedef struct Render_TrailPacket Render_TrailPacket;
typedef struct Render_TransposedRingPacket Render_TransposedRingPacket;
typedef struct Render_TransposedRingTexCoord Render_TransposedRingTexCoord;

// decomp: vs_primAddr (src/include/gpu.h:17)
enum Render_LegacyPrimitiveAddress { Render_LegacyPrimitiveAddress_Null = 0, Render_LegacyPrimitiveAddress_End = 0xFFFFFF };
typedef enum Render_LegacyPrimitiveAddress Render_LegacyPrimitiveAddress;

// decomp: vs_primType (src/include/gpu.h:19)
enum Render_LegacyPrimitiveCode {
    Render_LegacyPrimitiveCode_ShadeTex = 1,
    Render_LegacyPrimitiveCode_SemiTrans = 2,
    Render_LegacyPrimitiveCode_PolyFt3 = 0x24,
    Render_LegacyPrimitiveCode_PolyFt4 = 0x2C,
    Render_LegacyPrimitiveCode_PolyFt4ShadeTex = Render_LegacyPrimitiveCode_PolyFt4 | Render_LegacyPrimitiveCode_ShadeTex,
    Render_LegacyPrimitiveCode_PolyG4 = 0x38,
    Render_LegacyPrimitiveCode_PolyG4SemiTrans = Render_LegacyPrimitiveCode_PolyG4 | Render_LegacyPrimitiveCode_SemiTrans,
    Render_LegacyPrimitiveCode_PolyGt4 = 0x3C,
    Render_LegacyPrimitiveCode_PolyGt4SemiTrans = Render_LegacyPrimitiveCode_PolyGt4 | Render_LegacyPrimitiveCode_SemiTrans,
    Render_LegacyPrimitiveCode_LineF2 = 0x40,
    Render_LegacyPrimitiveCode_LineF2SemiTrans = Render_LegacyPrimitiveCode_LineF2 | Render_LegacyPrimitiveCode_SemiTrans,
    Render_LegacyPrimitiveCode_LineG2 = 0x50,
    Render_LegacyPrimitiveCode_LineG2SemiTrans = Render_LegacyPrimitiveCode_LineG2 | Render_LegacyPrimitiveCode_SemiTrans,
    Render_LegacyPrimitiveCode_Tile = 0x60,
    Render_LegacyPrimitiveCode_TileSemiTrans = Render_LegacyPrimitiveCode_Tile | Render_LegacyPrimitiveCode_SemiTrans,
    Render_LegacyPrimitiveCode_Sprite = 0x64,
    Render_LegacyPrimitiveCode_SpriteSemiTrans = Render_LegacyPrimitiveCode_Sprite | Render_LegacyPrimitiveCode_SemiTrans
};
typedef enum Render_LegacyPrimitiveCode Render_LegacyPrimitiveCode;

// decomp: TrailPacket (src/BATTLE/BATTLE.PRG/5BF94.c:169)
typedef struct Render_TrailPacket {
    u_long reserved0;
    u_long mode;
    u_char r0;
    u_char g0;
    u_char blue0;
    u_char code;
    long xy0;
    u_short uv0;
    u_short clut;
    u_char r1;
    u_char g1;
    u_char blue1;
    u_char reserved17;
    long xy1;
    u_short uv1;
    u_short tpage;
    u_char r2;
    u_char g2;
    u_char blue2;
    u_char reserved23;
    long xy2;
    u_short uv2;
    u_short reserved2A;
    u_char r3;
    u_char g3;
    u_char blue3;
    u_char reserved2F;
    long xy3;
    u_short uv3;
    u_short reserved36;
    u_long endMode;
} Render_TrailPacket;
_Static_assert(sizeof(Render_TrailPacket) == 0x3C, "Render_TrailPacket must keep its PS1 layout");

// decomp: VS_POLY_G4 (src/include/gpu.h:81)
typedef struct Render_GouraudQuadPacket {
    u_long colorZeroAndCode;
    u_long positionZero;
    u_long colorOne;
    u_long positionOne;
    u_long colorTwo;
    u_long positionTwo;
    u_long colorThree;
    u_long positionThree;
} Render_GouraudQuadPacket;
_Static_assert(sizeof(Render_GouraudQuadPacket) == 0x20, "Render_GouraudQuadPacket must keep its PS1 layout");

// decomp: VS_POLY_G4_TPAGE (src/include/gpu.h:70)
typedef struct Render_PagedGouraudQuadPacket {
    u_long texturePage;
    u_long colorZeroAndCode;
    u_long positionZero;
    u_long colorOne;
    u_long positionOne;
    u_long colorTwo;
    u_long positionTwo;
    u_long colorThree;
    u_long positionThree;
} Render_PagedGouraudQuadPacket;
_Static_assert(sizeof(Render_PagedGouraudQuadPacket) == 0x24, "Render_PagedGouraudQuadPacket must keep its PS1 layout");

// decomp: VS_SPRT (src/include/gpu.h:58)
typedef struct Render_SpriteTexturePagePacket {
    u_long texturePageCommand;
    u_long colorCommand;
    u_long position;
    u_long uvPalette;
    u_long size;
} Render_SpriteTexturePagePacket;
_Static_assert(sizeof(Render_SpriteTexturePagePacket) == 0x14, "Render_SpriteTexturePagePacket must keep its PS1 layout");

// decomp: VS_TILE (src/include/gpu.h:50)
typedef struct Render_TilePacket {
    u_long colorAndCode;
    u_long position;
    u_long size;
} Render_TilePacket;
_Static_assert(sizeof(Render_TilePacket) == 0xC, "Render_TilePacket must keep its PS1 layout");

// decomp: VS_TILE_TPAGE (src/include/gpu.h:44)
typedef struct Render_TileTexturePagePacket {
    u_long texturePageCommand;
    u_long colorCommand;
    u_long position;
    u_long size;
} Render_TileTexturePagePacket;
_Static_assert(sizeof(Render_TileTexturePagePacket) == 0x10, "Render_TileTexturePagePacket must keep its PS1 layout");

// decomp: unkPrim (src/EFFECT/PLG005.BIN/18.c:33)
typedef struct Render_EffectFlatQuadPacket {
    u_long tag0;
    POLY_FT4 opaque4;
    int drawMode;
    int reserved30[3];
} Render_EffectFlatQuadPacket;
_Static_assert(sizeof(Render_EffectFlatQuadPacket) == 0x3C, "Render_EffectFlatQuadPacket must keep its PS1 layout");

// decomp: unkPrim (src/EFFECT/trails5.h:23)
typedef struct Render_EffectGouraudQuadPacket {
    u_char tag0;
    u_char tag1;
    u_char tag2;
    u_char wordCount;
    POLY_GT4 opaque4;
    int drawMode;
} Render_EffectGouraudQuadPacket;
_Static_assert(sizeof(Render_EffectGouraudQuadPacket) == 0x3C, "Render_EffectGouraudQuadPacket must keep its PS1 layout");

// decomp: vs_closedRingPrim (src/EFFECT/PLG077.BIN/18.c:30)
typedef struct Render_ClosedRingPacket {
    int tag;
    POLY_GT4 poly;
    int drawMode;
} Render_ClosedRingPacket;
_Static_assert(sizeof(Render_ClosedRingPacket) == 0x3C, "Render_ClosedRingPacket must keep its PS1 layout");

// decomp: vs_closedRingUV (src/EFFECT/PLG077.BIN/18.c:34)
typedef struct Render_ClosedRingTexCoord {
    u_char reserved0;
    u_char reserved1;
} Render_ClosedRingTexCoord;
_Static_assert(sizeof(Render_ClosedRingTexCoord) == 0x2, "Render_ClosedRingTexCoord must keep its PS1 layout");

// layout: ledger (upstream merge) for vs_gridPrim
typedef struct Render_GridPacket {
    /* 0x00 */ int tag;
    /* 0x04 */ POLY_FT4 poly;
    /* 0x2C */ int drawMode;
    /* 0x30 */ int reserved30[3];
} Render_GridPacket;
_Static_assert(sizeof(Render_GridPacket) == 0x3C, "Render_GridPacket must keep its PS1 layout");

// decomp: vs_mesh10Prim (src/EFFECT/meshes10.h:21)
typedef struct Render_Mesh10Packet {
    int tag;
    POLY_GT3 poly;
    int drawMode;
} Render_Mesh10Packet;
_Static_assert(sizeof(Render_Mesh10Packet) == 0x30, "Render_Mesh10Packet must keep its PS1 layout");

// decomp: vs_mesh10UV (src/EFFECT/meshes10.h:24)
typedef struct Render_Mesh10TexCoord {
    u_char u, v;
} Render_Mesh10TexCoord;
_Static_assert(sizeof(Render_Mesh10TexCoord) == 0x2, "Render_Mesh10TexCoord must keep its PS1 layout");

// decomp: vs_mirrorSurfacePrim (src/EFFECT/mirroredSurfaces.h:22)
typedef struct Render_MirrorSurfacePacket {
    int tag;
    POLY_GT4 poly;
    int drawMode;
} Render_MirrorSurfacePacket;
_Static_assert(sizeof(Render_MirrorSurfacePacket) == 0x3C, "Render_MirrorSurfacePacket must keep its PS1 layout");

// decomp: vs_particleLine (src/EFFECT/particleLineRender.h:9)
typedef struct Render_ParticleLinePacket {
    u_char tag0, tag1, tag2, wordCount;
    LINE_F2 line;
    int drawMode;
} Render_ParticleLinePacket;
_Static_assert(sizeof(Render_ParticleLinePacket) == 0x18, "Render_ParticleLinePacket must keep its PS1 layout");

// decomp: vs_particlePrim (src/EFFECT/particleRender.h:13)
typedef struct Render_ParticleQuadPacket {
    u_char tag0;
    u_char tag1;
    u_char tag2;
    u_char wordCount;
    POLY_GT4 opaque4;
    int drawMode;
} Render_ParticleQuadPacket;
_Static_assert(sizeof(Render_ParticleQuadPacket) == 0x3C, "Render_ParticleQuadPacket must keep its PS1 layout");

// decomp: vs_ringPrim (src/EFFECT/rings.h:22)
typedef struct Render_RingPacket {
    int tag;
    POLY_GT4 poly;
    int drawMode;
} Render_RingPacket;
_Static_assert(sizeof(Render_RingPacket) == 0x3C, "Render_RingPacket must keep its PS1 layout");

// decomp: vs_rotatingSurfacePrim (src/EFFECT/rotatingSurfaces.h:23)
typedef struct Render_RotatingSurfacePacket {
    int tag;
    POLY_GT4 poly;
    int drawMode;
} Render_RotatingSurfacePacket;
_Static_assert(sizeof(Render_RotatingSurfacePacket) == 0x3C, "Render_RotatingSurfacePacket must keep its PS1 layout");

// decomp: vs_surfacePrim (src/EFFECT/surfaces.h:22)
typedef struct Render_SurfacePacket {
    int tag;
    POLY_GT4 poly;
    int drawMode;
} Render_SurfacePacket;
_Static_assert(sizeof(Render_SurfacePacket) == 0x3C, "Render_SurfacePacket must keep its PS1 layout");

// decomp: vs_tiledRingPrim (src/EFFECT/tiledRings.h:27)
typedef struct Render_TiledRingPacket {
    int tag;
    POLY_GT4 poly;
    int drawMode;
} Render_TiledRingPacket;
_Static_assert(sizeof(Render_TiledRingPacket) == 0x3C, "Render_TiledRingPacket must keep its PS1 layout");

// decomp: vs_tiledRingUV (src/EFFECT/tiledRings.h:31)
typedef struct Render_TiledRingTexCoord {
    u_char u;
    u_char v;
} Render_TiledRingTexCoord;
_Static_assert(sizeof(Render_TiledRingTexCoord) == 0x2, "Render_TiledRingTexCoord must keep its PS1 layout");

// decomp: vs_trail9Prim (src/EFFECT/trails9.h:23)
typedef struct Render_Trail9Packet {
    u_char tag0;
    u_char tag1;
    u_char tag2;
    u_char wordCount;
    POLY_GT4 poly;
    int drawMode;
} Render_Trail9Packet;
_Static_assert(sizeof(Render_Trail9Packet) == 0x3C, "Render_Trail9Packet must keep its PS1 layout");

// decomp: vs_transposedRingPrim (src/EFFECT/PLG162.BIN/0.c:25)
typedef struct Render_TransposedRingPacket {
    int tag;
    POLY_GT4 poly;
    int drawMode;
} Render_TransposedRingPacket;
_Static_assert(sizeof(Render_TransposedRingPacket) == 0x3C, "Render_TransposedRingPacket must keep its PS1 layout");

// decomp: vs_transposedRingUV (src/EFFECT/PLG162.BIN/0.c:29)
typedef struct Render_TransposedRingTexCoord {
    u_char reserved0;
    u_char reserved1;
} Render_TransposedRingTexCoord;
_Static_assert(sizeof(Render_TransposedRingTexCoord) == 0x2, "Render_TransposedRingTexCoord must keep its PS1 layout");

#endif // VS_RENDER_LEGACYPACKETS_H_TYPES
