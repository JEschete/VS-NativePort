#pragma once
// Hand-maintained (tools/port/owned.csv).

// Live binding for texture animations (ParticleTextureAnimation.hpp): the
// LiveParticleTextureAnimation records of particles and of the ring,
// surface and trail states, the current package's texture tables (block 1)
// and its animation table (block 2), whose extent PackageExtents.hpp records.
// The entries keep the original signatures for the renderers and plugins. A
// missing animation is refused with a diagnostic, as are a program index the
// table lacks and a program the original would have looped on or read past.
// A frame of an unbound texture table is reported and leaves the frame as it
// was; nothing records a table's frame count, so the frame index is not
// bounded.
namespace VS::Effects {
struct LiveParticleTextureAnimation;

// func_800D6CCC (5BF94.c:8501): clears the whole record, pointers included.
void LiveClearParticleTextureState(LiveParticleTextureAnimation* animation);
// func_800D6CF0 (5BF94.c:8513): a program index outside the package's
// animation table leaves the animation without a program.
void LiveInitializeParticleTextureAnimation(
    LiveParticleTextureAnimation* animation, int texture_index,
    int program_index);
// func_800D6D24 (5BF94.c:8521).
void LiveAdvanceParticleTextureAnimation(
    LiveParticleTextureAnimation* animation);
}  // namespace VS::Effects
