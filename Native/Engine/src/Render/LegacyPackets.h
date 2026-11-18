// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_LEGACYPACKETS_H_TYPES
#define VS_RENDER_LEGACYPACKETS_H_TYPES
#include "Legacy/Psx.h"

struct Render_ClosedRingPacket;
struct Render_ClosedRingTexCoord;
struct Render_EffectFlatQuadPacket;
struct Render_EffectGouraudQuadPacket;
struct Render_GouraudQuadPacket;
struct Render_GridPacket;
struct Render_Mesh10Packet;
struct Render_Mesh10TexCoord;
struct Render_MirrorSurfacePacket;
struct Render_PagedGouraudQuadPacket;
struct Render_ParticleLinePacket;
struct Render_ParticleQuadPacket;
struct Render_RingPacket;
struct Render_RotatingSurfacePacket;
struct Render_SpriteTexturePagePacket;
struct Render_SurfacePacket;
struct Render_TilePacket;
struct Render_TileTexturePagePacket;
struct Render_TiledRingPacket;
struct Render_TiledRingTexCoord;
struct Render_Trail9Packet;
struct Render_TrailPacket;
struct Render_TransposedRingPacket;
struct Render_TransposedRingTexCoord;

// decomp: vs_primAddr (src/include/gpu.h:17)
enum Render_LegacyPrimitiveAddress { Render_LegacyPrimitiveAddress_Null = 0, Render_LegacyPrimitiveAddress_End = 0xFFFFFF };

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

// decomp: TrailPacket (src/BATTLE/BATTLE.PRG/5BF94.c:169)
struct Render_TrailPacket {
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
};
static_assert(sizeof(Render_TrailPacket) == 0x3C, "Render_TrailPacket must keep its PS1 layout");

// decomp: VS_POLY_G4 (src/include/gpu.h:81)
struct Render_GouraudQuadPacket {
    u_long colorZeroAndCode;
    u_long positionZero;
    u_long colorOne;
    u_long positionOne;
    u_long colorTwo;
    u_long positionTwo;
    u_long colorThree;
    u_long positionThree;
};
static_assert(sizeof(Render_GouraudQuadPacket) == 0x20, "Render_GouraudQuadPacket must keep its PS1 layout");

// decomp: VS_POLY_G4_TPAGE (src/include/gpu.h:70)
struct Render_PagedGouraudQuadPacket {
    u_long texturePage;
    u_long colorZeroAndCode;
    u_long positionZero;
    u_long colorOne;
    u_long positionOne;
    u_long colorTwo;
    u_long positionTwo;
    u_long colorThree;
    u_long positionThree;
};
static_assert(sizeof(Render_PagedGouraudQuadPacket) == 0x24, "Render_PagedGouraudQuadPacket must keep its PS1 layout");

// decomp: VS_SPRT (src/include/gpu.h:58)
struct Render_SpriteTexturePagePacket {
    u_long texturePageCommand;
    u_long colorCommand;
    u_long position;
    u_long uvPalette;
    u_long size;
};
static_assert(sizeof(Render_SpriteTexturePagePacket) == 0x14, "Render_SpriteTexturePagePacket must keep its PS1 layout");

// decomp: VS_TILE (src/include/gpu.h:50)
struct Render_TilePacket {
    u_long colorAndCode;
    u_long position;
    u_long size;
};
static_assert(sizeof(Render_TilePacket) == 0xC, "Render_TilePacket must keep its PS1 layout");

// decomp: VS_TILE_TPAGE (src/include/gpu.h:44)
struct Render_TileTexturePagePacket {
    u_long texturePageCommand;
    u_long colorCommand;
    u_long position;
    u_long size;
};
static_assert(sizeof(Render_TileTexturePagePacket) == 0x10, "Render_TileTexturePagePacket must keep its PS1 layout");

// decomp: unkPrim (src/EFFECT/PLG005.BIN/18.c:33)
struct Render_EffectFlatQuadPacket {
    u_long tag0;
    POLY_FT4 opaque4;
    int drawMode;
    int reserved30[3];
};
static_assert(sizeof(Render_EffectFlatQuadPacket) == 0x3C, "Render_EffectFlatQuadPacket must keep its PS1 layout");

// decomp: unkPrim (src/EFFECT/trails5.h:23)
struct Render_EffectGouraudQuadPacket {
    u_char tag0;
    u_char tag1;
    u_char tag2;
    u_char wordCount;
    POLY_GT4 opaque4;
    int drawMode;
};
static_assert(sizeof(Render_EffectGouraudQuadPacket) == 0x3C, "Render_EffectGouraudQuadPacket must keep its PS1 layout");

// decomp: vs_closedRingPrim (src/EFFECT/PLG077.BIN/18.c:30)
struct Render_ClosedRingPacket {
    int tag;
    POLY_GT4 poly;
    int drawMode;
};
static_assert(sizeof(Render_ClosedRingPacket) == 0x3C, "Render_ClosedRingPacket must keep its PS1 layout");

// decomp: vs_closedRingUV (src/EFFECT/PLG077.BIN/18.c:34)
struct Render_ClosedRingTexCoord {
    u_char reserved0;
    u_char reserved1;
};
static_assert(sizeof(Render_ClosedRingTexCoord) == 0x2, "Render_ClosedRingTexCoord must keep its PS1 layout");

// decomp: vs_gridPrim (src/EFFECT/grids.h:21)
struct Render_GridPacket {
    int tag;
    POLY_FT4 poly;
    int drawMode;
    /* Effect packets reserve 60 bytes even for the shorter flat-textured quad. */
    int reserved30[3];
};
static_assert(sizeof(Render_GridPacket) == 0x3C, "Render_GridPacket must keep its PS1 layout");

// decomp: vs_mesh10Prim (src/EFFECT/meshes10.h:21)
struct Render_Mesh10Packet {
    int tag;
    POLY_GT3 poly;
    int drawMode;
};
static_assert(sizeof(Render_Mesh10Packet) == 0x30, "Render_Mesh10Packet must keep its PS1 layout");

// decomp: vs_mesh10UV (src/EFFECT/meshes10.h:24)
struct Render_Mesh10TexCoord {
    u_char u, v;
};
static_assert(sizeof(Render_Mesh10TexCoord) == 0x2, "Render_Mesh10TexCoord must keep its PS1 layout");

// decomp: vs_mirrorSurfacePrim (src/EFFECT/mirroredSurfaces.h:22)
struct Render_MirrorSurfacePacket {
    int tag;
    POLY_GT4 poly;
    int drawMode;
};
static_assert(sizeof(Render_MirrorSurfacePacket) == 0x3C, "Render_MirrorSurfacePacket must keep its PS1 layout");

// decomp: vs_particleLine (src/EFFECT/particleLineRender.h:9)
struct Render_ParticleLinePacket {
    u_char tag0, tag1, tag2, wordCount;
    LINE_F2 line;
    int drawMode;
};
static_assert(sizeof(Render_ParticleLinePacket) == 0x18, "Render_ParticleLinePacket must keep its PS1 layout");

// decomp: vs_particlePrim (src/EFFECT/particleRender.h:13)
struct Render_ParticleQuadPacket {
    u_char tag0;
    u_char tag1;
    u_char tag2;
    u_char wordCount;
    POLY_GT4 opaque4;
    int drawMode;
};
static_assert(sizeof(Render_ParticleQuadPacket) == 0x3C, "Render_ParticleQuadPacket must keep its PS1 layout");

// decomp: vs_ringPrim (src/EFFECT/rings.h:22)
struct Render_RingPacket {
    int tag;
    POLY_GT4 poly;
    int drawMode;
};
static_assert(sizeof(Render_RingPacket) == 0x3C, "Render_RingPacket must keep its PS1 layout");

// decomp: vs_rotatingSurfacePrim (src/EFFECT/rotatingSurfaces.h:23)
struct Render_RotatingSurfacePacket {
    int tag;
    POLY_GT4 poly;
    int drawMode;
};
static_assert(sizeof(Render_RotatingSurfacePacket) == 0x3C, "Render_RotatingSurfacePacket must keep its PS1 layout");

// decomp: vs_surfacePrim (src/EFFECT/surfaces.h:22)
struct Render_SurfacePacket {
    int tag;
    POLY_GT4 poly;
    int drawMode;
};
static_assert(sizeof(Render_SurfacePacket) == 0x3C, "Render_SurfacePacket must keep its PS1 layout");

// decomp: vs_tiledRingPrim (src/EFFECT/tiledRings.h:27)
struct Render_TiledRingPacket {
    int tag;
    POLY_GT4 poly;
    int drawMode;
};
static_assert(sizeof(Render_TiledRingPacket) == 0x3C, "Render_TiledRingPacket must keep its PS1 layout");

// decomp: vs_tiledRingUV (src/EFFECT/tiledRings.h:31)
struct Render_TiledRingTexCoord {
    u_char u;
    u_char v;
};
static_assert(sizeof(Render_TiledRingTexCoord) == 0x2, "Render_TiledRingTexCoord must keep its PS1 layout");

// decomp: vs_trail9Prim (src/EFFECT/trails9.h:23)
struct Render_Trail9Packet {
    u_char tag0;
    u_char tag1;
    u_char tag2;
    u_char wordCount;
    POLY_GT4 poly;
    int drawMode;
};
static_assert(sizeof(Render_Trail9Packet) == 0x3C, "Render_Trail9Packet must keep its PS1 layout");

// decomp: vs_transposedRingPrim (src/EFFECT/PLG162.BIN/0.c:25)
struct Render_TransposedRingPacket {
    int tag;
    POLY_GT4 poly;
    int drawMode;
};
static_assert(sizeof(Render_TransposedRingPacket) == 0x3C, "Render_TransposedRingPacket must keep its PS1 layout");

// decomp: vs_transposedRingUV (src/EFFECT/PLG162.BIN/0.c:29)
struct Render_TransposedRingTexCoord {
    u_char reserved0;
    u_char reserved1;
};
static_assert(sizeof(Render_TransposedRingTexCoord) == 0x2, "Render_TransposedRingTexCoord must keep its PS1 layout");

#endif // VS_RENDER_LEGACYPACKETS_H_TYPES
