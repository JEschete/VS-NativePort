#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Effects_RendererSlot;

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
int UpdateLivePlg045ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg057ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg065ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg082ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg087ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg114ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg114ParticleDispatch2(Effects_RendererSlot* slot,
                                      unsigned int command, int packed);
int UpdateLivePlg146ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg147ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg152ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg153ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg156ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg168ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg224ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg255ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg263ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg265ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg265ParticleDispatch2(Effects_RendererSlot* slot,
                                      unsigned int command, int packed);
int UpdateLivePlg266ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg266ParticleDispatch2(Effects_RendererSlot* slot,
                                      unsigned int command, int packed);
int UpdateLivePlg300ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed);
int UpdateLivePlg301ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed);
}  // namespace VS::Effects::Templates
