#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Effects {
struct LiveRendererSlot;
}  // namespace VS::Effects

// Live bindings for the particle-set dispatch instances (ParticleDispatch.cpp).
// Each keeps the renderer-slot entry shape the generated renderer tables and
// plugin updates call and binds the slot state and its control points, the
// current effect package and instance, the emitter scratch block, the
// overlay's emission cursor, the GTE, the BIOS generator, the high-end heap,
// the texture animation and the frame's ordering table and primitive buffer to
// ParticleSetHost. Definitions, transforms and curves are read only within the
// current package's recorded extents (PackageExtents.hpp); a set whose data
// the package lacks is refused with a diagnostic instead of indexing past it.
namespace VS::Effects::Templates {
int UpdateLivePlg045ParticleDispatch(LiveRendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg057ParticleDispatch(LiveRendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg065ParticleDispatch(LiveRendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg082ParticleDispatch(LiveRendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg087ParticleDispatch(LiveRendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg114ParticleDispatch(LiveRendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg114ParticleDispatch2(LiveRendererSlot* slot,
                                      unsigned int command, int packed);
int UpdateLivePlg146ParticleDispatch(LiveRendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg147ParticleDispatch(LiveRendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg152ParticleDispatch(LiveRendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg153ParticleDispatch(LiveRendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg156ParticleDispatch(LiveRendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg168ParticleDispatch(LiveRendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg224ParticleDispatch(LiveRendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg255ParticleDispatch(LiveRendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg263ParticleDispatch(LiveRendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg265ParticleDispatch(LiveRendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg265ParticleDispatch2(LiveRendererSlot* slot,
                                      unsigned int command, int packed);
int UpdateLivePlg266ParticleDispatch(LiveRendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg266ParticleDispatch2(LiveRendererSlot* slot,
                                      unsigned int command, int packed);
int UpdateLivePlg300ParticleDispatch(LiveRendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg301ParticleDispatch(LiveRendererSlot* slot,
                                     unsigned int command, int packed);
}  // namespace VS::Effects::Templates
