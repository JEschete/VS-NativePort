#pragma once
// The particle records of the effect runtime as the live bindings read and
// write them: the pooled particles (ParticlePoolLive.cpp), their motion
// (ParticleMotionLive.cpp) and texture animation
// (ParticleTextureAnimationLive.cpp), the package's particle definitions and
// the emitter's scratch (ParticleEmitterLive.cpp). The typed values are in
// Effects/ParticleEmitter.hpp, Effects/ParticleMotion.hpp and
// Effects/ParticleTextureAnimation.hpp.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Effects/EffectScript.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Effects {
struct LivePackageTextureFrame;

inline constexpr std::size_t kParticleAxes = 3;
inline constexpr std::size_t kParticleColorCurves = 3;
// A (start, end) range of one value.
inline constexpr std::size_t kParticleScalarRangeHalfwords = 2;
// Three (start, end) ranges: x, y and z.
inline constexpr std::size_t kParticleVectorRangeHalfwords = 6;
// Two (start, end) ranges: x and y.
inline constexpr std::size_t kParticlePairRangeHalfwords = 4;
inline constexpr std::size_t kParticleVectorRanges = 8;
inline constexpr std::size_t kParticlePairRanges = 5;
inline constexpr std::size_t kParticleScratchMatrixGapBytes = 16;
inline constexpr std::size_t kParticleScratchTailGapBytes = 20;
inline constexpr std::size_t kParticleScratchColorGapBytes = 16;
// A colour preset entry: the red, green and blue curves, then padding in the
// paired presets or the definition index in the preset table.
inline constexpr std::size_t kColorPresetEntryBytes = 4;

// N on-disc halfword bounds.
template <std::size_t N>
using LiveHalfwords = std::array<std::int16_t, N>;
using LiveParticleScalarRange = LiveHalfwords<kParticleScalarRangeHalfwords>;
using LiveParticleVectorRange = LiveHalfwords<kParticleVectorRangeHalfwords>;
using LiveParticlePairRange = LiveHalfwords<kParticlePairRangeHalfwords>;
using LiveColorPresetEntry = std::array<std::uint8_t, kColorPresetEntryBytes>;
// One colour preset: an entry per paired definition or preset table entry.
template <std::size_t N>
using LiveColorPreset = std::array<LiveColorPresetEntry, N>;
// The colour presets, each of N entries.
template <std::size_t N>
using LiveColorPresets = std::array<LiveColorPreset<N>, kParticleColorPresets>;

// decomp: func_800D52A4_t2 (src/BATTLE/BATTLE.PRG/battleEffect.h:242)
struct LiveFixedVector {
  std::int32_t vx;
  std::int32_t vy;
  std::int32_t vz;
};

// decomp: func_800D52A4_t (src/BATTLE/BATTLE.PRG/battleEffect.h:248)
struct LiveParticleMotion {
  // Divides each velocity step (the dragged velocity plus the force); a zero
  // divides as the R3000 does.
  std::int16_t mass;
  // Scales the shared gravity (particleGravity).
  std::int16_t gravity_scale;
  LiveFixedVector position;
  LiveFixedVector velocity;
  LiveFixedVector force;
  LiveFixedVector acceleration;
};

// decomp: func_800D6CF_t (src/BATTLE/BATTLE.PRG/battleEffect.h:89): a
// texture animation program's cursor and current frame (TextureAnimationState
// in Effects/ParticleTextureAnimation.hpp).
struct LiveParticleTextureAnimation {
  std::int32_t reserved_0;
  std::int16_t reserved_4;
  // Ticks the frame is still held, counted down by two.
  std::uint8_t frame_timer;
  // Bits 24-29 of the frame word, stored with the timer; the renderer's
  // fallback depth mode.
  std::uint8_t frame_flags;
  std::int16_t frame_offset_x;
  std::int16_t frame_offset_y;
  std::int16_t reserved_c;
  // Byte offset of the program's next word.
  std::uint16_t cursor;
  std::int32_t reserved_10;
  // The sprite rotation, 4096 per turn.
  std::int16_t rotation;
  std::uint8_t texture_index;
  std::uint8_t reserved_17;
  std::uint8_t* program;
  LivePackageTextureFrame* texture_info;
};

// decomp: func_800D2904_t (src/BATTLE/BATTLE.PRG/battleEffect.h:257): one
// pooled particle, linked into its effect instance's list.
struct LiveParticle {
  LiveParticle* previous;
  LiveParticle* next;
  LiveParticleMotion motion;
  LiveParticleTextureAnimation texture_animation;
  // The definition's emission flags.
  std::int32_t emit_flags;
  std::uint8_t transparency;
  // The particle variant; non-zero selects the renderer's depth mode.
  std::uint8_t variant : 4;
  std::uint8_t reserved_61_high : 4;
  // The point the attraction pulls toward.
  std::array<std::int16_t, kParticleAxes> attraction_target;
  // Ticks left; the particle is released at zero.
  std::int16_t lifetime;
  std::uint8_t transform_index;
  std::uint8_t attraction_transform;
  // Definitions emitted when the particle ends and on each tick.
  std::uint8_t end_event;
  std::uint8_t tick_event;
  std::array<char, 2> reserved_6e;
  std::int32_t attraction;
  // The red, green and blue curves, sampled at the age.
  std::array<std::uint8_t, kParticleColorCurves> color_curves;
  std::uint8_t age;
};

// decomp: func_800FA098_arg0 (src/BATTLE/BATTLE.PRG/battleEffect.h:112): one
// particle definition of a package's block 5, in its on-disc layout. Each
// curve indexes the package's curves (0 is none) and weights the matching
// range.
struct LiveParticleDefinition {
  std::uint8_t animation_id;
  std::uint8_t texture_index;
  std::uint8_t end_event;
  std::uint8_t tick_event;
  std::uint32_t flags : 26;
  std::uint32_t transparency_curve : 6;
  std::uint8_t position_curve;
  std::uint8_t extent_curve;
  std::uint8_t direction_curve;
  std::uint8_t direction_jitter_curve;
  std::uint8_t speed_curve;
  std::uint8_t force_curve;
  std::uint8_t acceleration_curve;
  std::uint8_t attraction_offset_curve;
  std::uint8_t attraction_jitter_curve;
  std::uint8_t attraction_curve;
  std::uint8_t mass_curve;
  std::uint8_t gravity_scale_curve;
  std::uint8_t lifetime_curve;
  std::uint8_t count_curve;
  std::uint8_t interval_curve;
  std::uint8_t inner_extent_curve;
  LiveParticleVectorRange position_range;
  LiveParticleVectorRange extent_range;
  LiveParticleScalarRange inner_extent_range;
  std::array<LiveParticleVectorRange, kParticleVectorRanges> vector_ranges;
  std::array<LiveParticlePairRange, kParticlePairRanges> pair_ranges;
  LiveParticleScalarRange count_range;
  LiveParticleScalarRange interval_range;
  std::uint8_t r_curve;
  std::uint8_t g_curve;
  std::uint8_t b_curve;
  std::uint8_t reserved_c7;
  std::uint8_t transform_index;
  std::uint8_t attraction_transform;
  std::uint8_t ring_segments;
  std::uint8_t ring_positions;
};

// The emitter scratch's colour curves and the age the colours are sampled at.
struct LiveParticleColorCurves {
  std::uint8_t red_curve;
  std::uint8_t green_curve;
  std::uint8_t blue_curve;
  std::uint8_t age;
};

// decomp: func_800FA098_arg1 (src/BATTLE/BATTLE.PRG/battleEffect.h:152): the
// emitter's scratch at scratchpad offset 0x1D0, carried between emissions.
struct LiveParticleEmitterScratch {
  // RotMatrix_gte overlays the first eighteen bytes with the direction matrix.
  VS::Core::Gte::VECTOR reserved_0;
  VS::Core::Gte::VECTOR reserved_10;
  std::int32_t reserved_20;
  std::uint32_t flags;
  // The definition flags of the previous emission.
  std::uint16_t saved_flags;
  std::uint16_t reserved_2a;
  // The spawn offset from the aim point.
  VS::Core::Gte::SVECTOR vector;
  VS::Core::Gte::VECTOR aim_point;
  VS::Core::Gte::VECTOR spawn_point;
  std::array<std::uint8_t, kParticleScratchMatrixGapBytes> reserved_54;
  VS::Core::Gte::MATRIX aim_matrix;
  std::array<std::uint8_t, kParticleScratchTailGapBytes> reserved_84;
  VS::Core::Gte::VECTOR position;
  VS::Core::Gte::VECTOR extent;
  VS::Core::Gte::VECTOR direction;
  VS::Core::Gte::VECTOR direction_jitter;
  VS::Core::Gte::VECTOR inner_extent;
  VS::Core::Gte::VECTOR base_direction;
  VS::Core::Gte::SVECTOR force_min;
  VS::Core::Gte::SVECTOR force_max;
  VS::Core::Gte::SVECTOR acceleration_min;
  VS::Core::Gte::SVECTOR acceleration_max;
  std::array<std::int32_t, kParticleScalarRangeHalfwords> speed_range;
  std::int32_t speed;
  std::array<std::int32_t, kParticleScalarRangeHalfwords> mass_range;
  std::array<std::int32_t, kParticleScalarRangeHalfwords> gravity_scale_range;
  std::array<std::int32_t, kParticleScalarRangeHalfwords> lifetime_range;
  VS::Core::Gte::SVECTOR attraction_offset;
  VS::Core::Gte::SVECTOR attraction_jitter;
  std::array<std::int32_t, kParticleScalarRangeHalfwords> attraction_range;
  LiveParticleColorCurves color_curves;
  std::array<std::uint8_t, kParticleScratchColorGapBytes> reserved_158;
  std::int32_t transform_index;
  std::int32_t attraction_transform;
  std::int32_t sin_angle0;
  std::int32_t sin_angle1;
  std::int32_t cos_angle0;
  std::int32_t cos_angle1;
  std::int32_t transparency;
  std::int32_t ring_segments;
  std::int32_t ring_positions;
};

// decomp: func_800FA098_arg3 (src/EFFECT/particleSetup.h:19): one particle
// set of the particle templates (ParticleSetState in
// Effects/Templates/ParticleCore.hpp).
struct LiveParticleQuadSet {
  // Ticks the set lives.
  std::uint8_t lifetime;
  std::uint8_t definition_index;
  std::int16_t count;
  VS::Core::Gte::SVECTOR* control_points;
  // Ticks until the set emits again.
  std::int32_t cooldown;
  std::int32_t age;
  LiveParticleTextureAnimation texture_animation;
};

// decomp: func_800CFAAC_t (src/BATTLE/BATTLE.PRG/battleEffect.h:106): a
// linked line packet. func_800CFAAC steps the line's second point (x1, y1)
// toward its first (x0, y0).
struct LiveParticleLinePacket {
  std::uint32_t tag;
  VS::Legacy::LINE_F2 line;
  std::uint32_t draw_mode;
};

namespace layout {
inline constexpr std::size_t kFixedVectorBytes = 0xC;
static_assert(sizeof(LiveFixedVector) == kFixedVectorBytes);
inline constexpr std::size_t kParticleMotionBytes = 0x34;
static_assert(sizeof(LiveParticleMotion) == kParticleMotionBytes);
inline constexpr std::size_t kTextureAnimationProgramOffset = 0x18;
static_assert(offsetof(LiveParticleTextureAnimation, program) ==
              kTextureAnimationProgramOffset);
inline constexpr std::size_t kParticleDefinitionBytes = 0xCC;
static_assert(sizeof(LiveParticleDefinition) == kParticleDefinitionBytes);
inline constexpr std::size_t kParticleColorCurvesBytes = 4;
static_assert(sizeof(LiveParticleColorCurves) == kParticleColorCurvesBytes);
inline constexpr std::size_t kEmitterScratchAimMatrixOffset = 0x64;
static_assert(offsetof(LiveParticleEmitterScratch, aim_matrix) ==
              kEmitterScratchAimMatrixOffset);
inline constexpr std::size_t kEmitterScratchBytes = 0x18C;
static_assert(sizeof(LiveParticleEmitterScratch) == kEmitterScratchBytes);
inline constexpr std::size_t kParticleLinePacketBytes = 0x18;
static_assert(sizeof(LiveParticleLinePacket) == kParticleLinePacketBytes);
}  // namespace layout

// decomp: D_800EC328 (BATTLE.PRG): the emission counter func_800D2ADC
// advances.
extern std::int32_t emissionCounter;
// decomp: D_800EC330 (src/BATTLE/BATTLE.PRG/5BF94.c:402): the colour curves
// of the paired definitions, by preset.
extern LiveColorPresets<kPairedDefinitions> pairedParticleColorPresets;
// decomp: D_800EC368 (src/BATTLE/BATTLE.PRG/5BF94.c:403): the colour curves
// and definition of each preset table entry, by preset.
extern LiveColorPresets<kColorPresetEntries> particleColorPresetTable;
// decomp: D_800F55F0 (src/BATTLE/BATTLE.PRG/5BF94.c:446)
extern std::int32_t particleCapacity;
// decomp: D_800F55F4 (src/BATTLE/BATTLE.PRG/5BF94.c:447)
extern LiveParticle* freeParticles;
// decomp: D_800F55F8 (src/BATTLE/BATTLE.PRG/5BF94.c:448)
extern std::int32_t usedParticleCount;
// decomp: D_800F55FC (src/BATTLE/BATTLE.PRG/5BF94.c:453)
extern LiveParticle* particlePool;
// decomp: D_800F5600 (src/BATTLE/BATTLE.PRG/5BF94.c:449): the pool's users.
extern std::int32_t particlePoolReferences;
// decomp: D_800F5618 (src/BATTLE/BATTLE.PRG/5BF94.c:451)
extern std::int32_t particleDrag;
// decomp: D_800F5620 (src/BATTLE/BATTLE.PRG/5BF94.c:452): the gravity on
// every particle; pad is carried but unused.
extern VS::Core::Gte::VECTOR particleGravity;
}  // namespace VS::Effects
