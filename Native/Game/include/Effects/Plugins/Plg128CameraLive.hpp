#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Effects_RendererSlot;

// Live binding for PLG128's effect camera slot (Plg128Camera.hpp). It keeps
// the renderer-slot entry shape the EFFPURGE renderer table calls and binds
// the slot state, the particle emitter scratch block, the current effect
// package and instance, the action request's origin, the high-end heap and the
// effect camera globals (CameraLiveState.hpp) to CameraEffectHost. Definitions
// and transforms are read only within the current package's recorded extents
// (PackageExtents.hpp) and the instance's transform table; a camera whose data
// is missing is stopped with a diagnostic instead of indexing past it.
namespace VS::Effects::Plg128 {
// decomp: PLG128.BIN func_800FDEC4 (F8.c:146).
int UpdateLiveEffectCamera(Effects_RendererSlot* slot, unsigned int command,
                           int packed);
}  // namespace VS::Effects::Plg128
