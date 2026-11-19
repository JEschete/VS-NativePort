#pragma once
// The states the ring, grid, mesh, surface and trail effect templates keep in
// their renderer slots. RingsLive.cpp, SurfacesLive.cpp, TrailsLive.cpp and
// TrailEmitterLive.cpp bind them to RingState, GridState, Mesh10State,
// SheetState and the trail emitter counters, their typed values.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Effects/Particles.h"
#include "Effects/Templates/SurfaceCore.hpp"
#include "Effects/TrailEmitter.hpp"
#include "Effects/Trails.hpp"
#include "Legacy/Psx.h"

namespace VS::Effects {
// decomp: vs_RingState (src/EFFECT/effectCommon.h:22): the state of the ring
// (rings.h:34), closed ring (closedRings.h:80), tiled ring and transposed
// ring templates; the last two are closedRings.h variants (tiledRings.h,
// VS_CLOSED_RING_TRANSPOSED).
struct LiveRingState {
  // Ticks left, from the create argument's low byte; 0 runs until stopped,
  // and a stop sets 1 so the next tick ends the effect (rings.h:57, 277-288).
  std::uint8_t lifetime;
  // Particle definition, from the create argument's high byte.
  std::uint8_t effect;
  // Never read; the original left it as the heap held it.
  std::int16_t reserved_2;
  // Radius and height, each moved by its speed and the sampled offset; the
  // sampled force accelerates the speeds (rings.h:98-105).
  std::int32_t radius;
  std::int32_t radius_speed;
  std::int32_t height;
  std::int32_t height_speed;
  // Angle of the first segment; advances by the sampled spin each tick.
  std::int32_t angle;
  // Ticks since creation; every curve samples at it.
  std::int32_t age;
  Effects_ParticleTextureAnimation texture;
};

// decomp: vs_GridState (src/EFFECT/grids.h:16): a 9x9 grid of waving
// vertices.
struct LiveGridState {
  // Ticks left and particle definition, as in LiveRingState.
  std::uint8_t lifetime;
  std::uint8_t effect;
  // Texture origin within the page (grids.h:82-83).
  std::uint8_t u;
  std::uint8_t v;
  // Texture scroll, advanced modulo the texture size each tick
  // (grids.h:186-194).
  std::uint8_t u_scroll;
  std::uint8_t v_scroll;
  // Texture page and palette of the grid's quads (grids.h:79-81).
  std::int16_t tpage;
  std::int16_t clut;
  // Cell spacing along x and z, each moved by its speed (grids.h:116-121).
  std::int16_t x_spacing;
  std::int16_t x_speed;
  std::int16_t z_spacing;
  std::int16_t z_speed;
  // Wave phase along x and z (grids.h:122-125).
  std::int16_t x_phase;
  std::int16_t z_phase;
  // Ticks since creation; every curve samples at it.
  std::int32_t age;
};

// decomp: vs_Mesh10State (src/EFFECT/meshes10.h:18): a turning ten-vertex
// mesh.
struct LiveMesh10State {
  // Ticks left and particle definition, as in LiveRingState.
  std::uint8_t lifetime;
  std::uint8_t effect;
  // Never read; the original left it as the heap held it.
  std::int16_t reserved_2;
  // Ticks since creation; every curve samples at it.
  std::int32_t age;
  Effects_ParticleTextureAnimation texture;
  // Accumulated rotation; the sampled spin adds to it each tick
  // (meshes10.h:152-157).
  SVECTOR rotation;
};

// decomp: VS_SURFACE_STATE_TYPE (src/EFFECT/surfaces.h:31): the surface
// (vs_SurfaceState) and mirrored surface (vs_MirrorSurfaceState) states,
// with a colour factor per row.
template <std::size_t Rows>
struct LiveSheetState {
  // Ticks left and particle definition, as in LiveRingState.
  std::uint8_t lifetime;
  std::uint8_t effect;
  // Never read; the original left it as the heap held it.
  std::int16_t reserved_2;
  // Radius and height with their speeds, as in LiveRingState.
  std::int32_t radius;
  std::int32_t radius_speed;
  std::int32_t height;
  std::int32_t height_speed;
  // Ticks since creation; every curve samples at it.
  std::int32_t age;
  // Each row's colour factor: the definition's transparency curve sampled at
  // create and doubled, with 256 stored as 255; 255 without a curve
  // (surfaces.h:113-134).
  std::array<std::int32_t, Rows> factors;
  Effects_ParticleTextureAnimation texture;
};

// decomp: vs_SurfaceState (src/EFFECT/surfaces.h:45)
using LiveSurfaceState = LiveSheetState<Templates::kSurfaceRows>;
// decomp: vs_MirrorSurfaceState (src/EFFECT/surfaces.h:46)
using LiveMirrorSurfaceState = LiveSheetState<Templates::kMirrorSurfaceRows>;

// decomp: vs_RotatingSurfaceState (src/EFFECT/rotatingSurfaces.h:29): the
// surface state with an angle the sampled spin advances
// (rotatingSurfaces.h:143-157).
struct LiveRotatingSurfaceState {
  std::uint8_t lifetime;
  std::uint8_t effect;
  // Never read; the original left it as the heap held it.
  std::int16_t reserved_2;
  std::int32_t radius;
  std::int32_t radius_speed;
  std::int32_t height;
  std::int32_t height_speed;
  std::int32_t angle;
  std::int32_t age;
  std::array<std::int32_t, Templates::kSurfaceRows> factors;
  Effects_ParticleTextureAnimation texture;
};

// decomp: VS_TRAIL_TYPES (src/EFFECT/trailEffect.h:28): the state of an
// emitter of trails with `Samples` spline samples (vs_Trail5State,
// vs_Trail9State).
template <std::size_t Samples>
struct BasicLiveTrailState {
  // Ticks left, from the create argument's low byte; 0 runs until stopped,
  // and a stop sets 1 so the next tick ends the effect
  // (trailEffect.h:729, 825-836).
  std::uint8_t lifetime;
  // Particle definition, from the create argument's high byte.
  std::uint8_t effect;
  // Trails in the buffer, at most the emitter's capacity.
  std::uint8_t trail_count;
  // Set once the first tick has placed and initialized the trails
  // (trailEffect.h:740, 768-773).
  std::uint8_t initialized;
  // The trail buffer, allocated from the high end of the heap.
  LiveTrailRecord<Samples>* trails;
  // Ticks since creation; every curve samples at it.
  std::int32_t curve_sample_distance;
  Effects_ParticleTextureAnimation texture;
  // The definition's transparency curve sampled once per spline sample at
  // create, 128 each without a curve (trailEffect.h:742-757); the original
  // allocated them past the structure.
  std::array<std::uint8_t, Samples> transparency;
};

// LiveTrailState (decomp: vs_Trail5State, src/EFFECT/trailEffect.h:52) and
// LiveTrailNineState (decomp: vs_Trail9State, src/EFFECT/trailEffect.h:53)
// are declared in TrailEmitter.hpp with these sample counts.
static_assert(kTrailTransparencySamples == kLiveTrailSamples);
static_assert(kTrailNineTransparencySamples == kLiveTrailNineSamples);

namespace layout {
inline constexpr std::size_t kGridStateBytes = 0x1C;
static_assert(sizeof(LiveGridState) == kGridStateBytes);
}  // namespace layout
}  // namespace VS::Effects
