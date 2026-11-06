#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Effects_RendererSlot;

// Live bindings for the ring, closed-ring and tiled-ring template instances
// (Ring.cpp, ClosedRing.cpp, TiledRing.cpp). Each keeps the renderer-slot entry
// shape the generated renderer tables and plugin updates call (EFFPURGE.BIN
// entry points): it passes its overlay's colour palette to the typed instance
// and binds the slot state, the current effect package and instance, the
// emitter scratch block, the GTE, the BIOS generator, the high-end heap and the
// frame's ordering table and primitive buffer to RingHost. Definitions,
// transforms and particle events are read only within the current package's
// recorded extents (PackageExtents.hpp); a ring whose data the package lacks is
// refused with a diagnostic instead of indexing past it.
namespace VS::Effects::Templates {
int UpdateLivePlg049Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg050Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg061Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg065Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg073Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg081Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg129Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg129Ring2(Effects_RendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg138Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg142Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg146Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg147Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg149Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg150Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg153Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg160Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg161Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg161Ring2(Effects_RendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg162Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg166Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg169Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg170Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg198Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg236Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg263Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg297Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg047ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg051ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg053ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg061ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg065ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg081ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg082ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg086ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg087ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg128ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg134ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg135ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg142ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg143ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg152ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg154ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg158ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg162ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg165ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg166ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg167ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg169ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg170ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg236ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg250ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg272ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg291ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg297ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg301ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg044TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg049TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg069TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg073TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg085TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg137TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg138TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg139TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg143TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg146TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg148TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg150TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg160TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg242TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg265TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
}  // namespace VS::Effects::Templates
