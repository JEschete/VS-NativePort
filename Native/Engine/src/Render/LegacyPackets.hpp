#pragma once
// GPU packets as the original's effect, menu and battle code built them: an
// ordering-table tag or a word count, the libgpu primitive, and the draw mode
// word that followed it.
#include <cstddef>
#include <cstdint>

#include "Legacy/Render/LibGpu.h"

namespace VS::Render {
// decomp: vs_primAddr (src/include/gpu.h:17)
enum class LegacyPrimitiveAddress : std::uint32_t {
  kNull = 0,
  kEnd = 0xFFFFFF
};

// decomp: vs_primType (src/include/gpu.h:19)
enum class LegacyPrimitiveCode : std::uint8_t {
  kShadeTex = 1,
  kSemiTrans = 2,
  kPolyFt3 = 0x24,
  kPolyFt4 = 0x2C,
  kPolyFt4ShadeTex = kPolyFt4 | kShadeTex,
  kPolyG4 = 0x38,
  kPolyG4SemiTrans = kPolyG4 | kSemiTrans,
  kPolyGt4 = 0x3C,
  kPolyGt4SemiTrans = kPolyGt4 | kSemiTrans,
  kLineF2 = 0x40,
  kLineF2SemiTrans = kLineF2 | kSemiTrans,
  kLineG2 = 0x50,
  kLineG2SemiTrans = kLineG2 | kSemiTrans,
  kTile = 0x60,
  kTileSemiTrans = kTile | kSemiTrans,
  kSprite = 0x64,
  kSpriteSemiTrans = kSprite | kSemiTrans
};

// Effect packets reserve 60 bytes even for the shorter flat-textured quad.
inline constexpr std::size_t kFlatQuadPacketPaddingWords = 3;

// decomp: TrailPacket (src/BATTLE/BATTLE.PRG/5BF94.c:169)
struct TrailPacket {
  std::uint32_t reserved0;
  std::uint32_t mode;
  std::uint8_t red0;
  std::uint8_t green0;
  std::uint8_t blue0;
  std::uint8_t code;
  std::int32_t xy0;
  std::uint16_t uv0;
  std::uint16_t palette;
  std::uint8_t red1;
  std::uint8_t green1;
  std::uint8_t blue1;
  std::uint8_t reserved17;
  std::int32_t xy1;
  std::uint16_t uv1;
  std::uint16_t texture_page;
  std::uint8_t red2;
  std::uint8_t green2;
  std::uint8_t blue2;
  std::uint8_t reserved23;
  std::int32_t xy2;
  std::uint16_t uv2;
  std::uint16_t reserved2a;
  std::uint8_t red3;
  std::uint8_t green3;
  std::uint8_t blue3;
  std::uint8_t reserved2f;
  std::int32_t xy3;
  std::uint16_t uv3;
  std::uint16_t reserved36;
  std::uint32_t end_mode;
};

// decomp: VS_POLY_G4 (src/include/gpu.h:81)
struct GouraudQuadPacket {
  std::uint32_t color_zero_and_code;
  std::uint32_t position_zero;
  std::uint32_t color_one;
  std::uint32_t position_one;
  std::uint32_t color_two;
  std::uint32_t position_two;
  std::uint32_t color_three;
  std::uint32_t position_three;
};

// decomp: VS_POLY_G4_TPAGE (src/include/gpu.h:70)
struct PagedGouraudQuadPacket {
  std::uint32_t texture_page;
  std::uint32_t color_zero_and_code;
  std::uint32_t position_zero;
  std::uint32_t color_one;
  std::uint32_t position_one;
  std::uint32_t color_two;
  std::uint32_t position_two;
  std::uint32_t color_three;
  std::uint32_t position_three;
};

// decomp: VS_SPRT (src/include/gpu.h:58)
struct SpriteTexturePagePacket {
  std::uint32_t texture_page_command;
  std::uint32_t color_command;
  std::uint32_t position;
  std::uint32_t uv_palette;
  std::uint32_t size;
};

// decomp: VS_TILE (src/include/gpu.h:50), without the ordering-table tag that
// UiPackets.hpp's TilePacket carries.
struct UntaggedTilePacket {
  std::uint32_t color_and_code;
  std::uint32_t position;
  std::uint32_t size;
};

// decomp: VS_TILE_TPAGE (src/include/gpu.h:44)
struct TileTexturePagePacket {
  std::uint32_t texture_page_command;
  std::uint32_t color_command;
  std::uint32_t position;
  std::uint32_t size;
};

// decomp: unkPrim (src/EFFECT/PLG005.BIN/18.c:33)
struct EffectFlatQuadPacket {
  std::uint32_t tag0;
  POLY_FT4 opaque4;
  std::int32_t draw_mode;
  std::int32_t reserved30[kFlatQuadPacketPaddingWords];
};

// decomp: unkPrim (src/EFFECT/trails5.h:23)
struct EffectGouraudQuadPacket {
  std::uint8_t tag0;
  std::uint8_t tag1;
  std::uint8_t tag2;
  std::uint8_t word_count;
  POLY_GT4 opaque4;
  std::int32_t draw_mode;
};

// decomp: vs_closedRingPrim (src/EFFECT/PLG077.BIN/18.c:30)
struct ClosedRingPacket {
  std::int32_t tag;
  POLY_GT4 poly;
  std::int32_t draw_mode;
};

// decomp: vs_closedRingUV (src/EFFECT/PLG077.BIN/18.c:34)
struct ClosedRingTexCoord {
  std::uint8_t reserved0;
  std::uint8_t reserved1;
};

// decomp: vs_gridPrim (src/EFFECT/grids.h:21)
struct GridPacket {
  std::int32_t tag;
  POLY_FT4 poly;
  std::int32_t draw_mode;
  std::int32_t reserved30[kFlatQuadPacketPaddingWords];
};

// decomp: vs_mesh10Prim (src/EFFECT/meshes10.h:21)
struct Mesh10Packet {
  std::int32_t tag;
  POLY_GT3 poly;
  std::int32_t draw_mode;
};

// decomp: vs_mesh10UV (src/EFFECT/meshes10.h:24)
struct Mesh10TexCoord {
  std::uint8_t u;
  std::uint8_t v;
};

// decomp: vs_mirrorSurfacePrim (src/EFFECT/mirroredSurfaces.h:22)
struct MirrorSurfacePacket {
  std::int32_t tag;
  POLY_GT4 poly;
  std::int32_t draw_mode;
};

// decomp: vs_particleLine (src/EFFECT/particleLineRender.h:9)
struct ParticleLinePacket {
  std::uint8_t tag0;
  std::uint8_t tag1;
  std::uint8_t tag2;
  std::uint8_t word_count;
  LINE_F2 line;
  std::int32_t draw_mode;
};

// decomp: vs_particlePrim (src/EFFECT/particleRender.h:13)
struct ParticleQuadPacket {
  std::uint8_t tag0;
  std::uint8_t tag1;
  std::uint8_t tag2;
  std::uint8_t word_count;
  POLY_GT4 opaque4;
  std::int32_t draw_mode;
};

// decomp: vs_ringPrim (src/EFFECT/rings.h:22)
struct RingPacket {
  std::int32_t tag;
  POLY_GT4 poly;
  std::int32_t draw_mode;
};

// decomp: vs_rotatingSurfacePrim (src/EFFECT/rotatingSurfaces.h:23)
struct RotatingSurfacePacket {
  std::int32_t tag;
  POLY_GT4 poly;
  std::int32_t draw_mode;
};

// decomp: vs_surfacePrim (src/EFFECT/surfaces.h:22)
struct SurfacePacket {
  std::int32_t tag;
  POLY_GT4 poly;
  std::int32_t draw_mode;
};

// decomp: vs_tiledRingPrim (src/EFFECT/tiledRings.h:27)
struct TiledRingPacket {
  std::int32_t tag;
  POLY_GT4 poly;
  std::int32_t draw_mode;
};

// decomp: vs_tiledRingUV (src/EFFECT/tiledRings.h:31)
struct TiledRingTexCoord {
  std::uint8_t u;
  std::uint8_t v;
};

// decomp: vs_trail9Prim (src/EFFECT/trails9.h:23)
struct Trail9Packet {
  std::uint8_t tag0;
  std::uint8_t tag1;
  std::uint8_t tag2;
  std::uint8_t word_count;
  POLY_GT4 poly;
  std::int32_t draw_mode;
};

// decomp: vs_transposedRingPrim (src/EFFECT/PLG162.BIN/0.c:25)
struct TransposedRingPacket {
  std::int32_t tag;
  POLY_GT4 poly;
  std::int32_t draw_mode;
};

// decomp: vs_transposedRingUV (src/EFFECT/PLG162.BIN/0.c:29)
struct TransposedRingTexCoord {
  std::uint8_t reserved0;
  std::uint8_t reserved1;
};

namespace layout {
inline constexpr std::size_t kTrailPacketBytes = 0x3C;
static_assert(sizeof(TrailPacket) == kTrailPacketBytes);
inline constexpr std::size_t kGouraudQuadPacketBytes = 0x20;
static_assert(sizeof(GouraudQuadPacket) == kGouraudQuadPacketBytes);
inline constexpr std::size_t kPagedGouraudQuadPacketBytes = 0x24;
static_assert(sizeof(PagedGouraudQuadPacket) == kPagedGouraudQuadPacketBytes);
inline constexpr std::size_t kSpriteTexturePagePacketBytes = 0x14;
static_assert(sizeof(SpriteTexturePagePacket) == kSpriteTexturePagePacketBytes);
inline constexpr std::size_t kUntaggedTilePacketBytes = 0xC;
static_assert(sizeof(UntaggedTilePacket) == kUntaggedTilePacketBytes);
inline constexpr std::size_t kTileTexturePagePacketBytes = 0x10;
static_assert(sizeof(TileTexturePagePacket) == kTileTexturePagePacketBytes);
// The effect quads, rings and surfaces all fill one 60-byte packet slot.
inline constexpr std::size_t kEffectPacketBytes = 0x3C;
static_assert(sizeof(EffectFlatQuadPacket) == kEffectPacketBytes);
static_assert(sizeof(EffectGouraudQuadPacket) == kEffectPacketBytes);
static_assert(sizeof(ClosedRingPacket) == kEffectPacketBytes);
static_assert(sizeof(GridPacket) == kEffectPacketBytes);
static_assert(sizeof(MirrorSurfacePacket) == kEffectPacketBytes);
static_assert(sizeof(ParticleQuadPacket) == kEffectPacketBytes);
static_assert(sizeof(RingPacket) == kEffectPacketBytes);
static_assert(sizeof(RotatingSurfacePacket) == kEffectPacketBytes);
static_assert(sizeof(SurfacePacket) == kEffectPacketBytes);
static_assert(sizeof(TiledRingPacket) == kEffectPacketBytes);
static_assert(sizeof(Trail9Packet) == kEffectPacketBytes);
static_assert(sizeof(TransposedRingPacket) == kEffectPacketBytes);
inline constexpr std::size_t kMesh10PacketBytes = 0x30;
static_assert(sizeof(Mesh10Packet) == kMesh10PacketBytes);
inline constexpr std::size_t kParticleLinePacketBytes = 0x18;
static_assert(sizeof(ParticleLinePacket) == kParticleLinePacketBytes);
inline constexpr std::size_t kTexCoordBytes = 2;
static_assert(sizeof(ClosedRingTexCoord) == kTexCoordBytes);
static_assert(sizeof(Mesh10TexCoord) == kTexCoordBytes);
static_assert(sizeof(TiledRingTexCoord) == kTexCoordBytes);
static_assert(sizeof(TransposedRingTexCoord) == kTexCoordBytes);
}  // namespace layout
}  // namespace VS::Render
