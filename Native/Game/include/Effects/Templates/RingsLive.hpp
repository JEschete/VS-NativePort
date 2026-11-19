#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Effects {
struct LiveRendererSlot;
}  // namespace VS::Effects

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
int UpdateLivePlg049Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg050Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg061Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg065Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg073Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg081Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg129Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg129Ring2(LiveRendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg138Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg142Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg146Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg147Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg149Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg150Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg153Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg160Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg161Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg161Ring2(LiveRendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg162Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg166Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg169Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg170Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg198Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg236Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg263Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg297Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg047ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg051ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg053ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg061ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg065ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg081ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg082ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg086ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg087ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg128ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg134ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg135ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg142ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg143ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg152ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg154ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg158ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg162ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg165ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg166ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg167ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg169ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg170ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg236ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg250ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg272ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg291ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg297ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg301ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed);
int UpdateLivePlg044TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg049TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg069TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg073TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg085TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg137TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg138TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg139TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg143TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg146TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg148TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg150TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg160TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg242TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg265TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed);
}  // namespace VS::Effects::Templates
