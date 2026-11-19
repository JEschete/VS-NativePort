#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Effects {
struct LiveRendererSlot;
}  // namespace VS::Effects

// Live bindings for the grid, ten-vertex mesh, surface, mirrored surface and
// rotating surface template instances (Grid.cpp, Mesh10.cpp, Surface.cpp,
// MirrorSurface.cpp, RotatingSurface.cpp). Each keeps the renderer-slot entry
// shape the generated renderer tables and plugin updates call (EFFPURGE.BIN
// entry points): it passes its overlay's profile or model to the typed
// instance and binds the slot state, the current effect package and instance,
// the emitter scratch block, the GTE, the BIOS generator, the high-end heap
// and the frame's ordering table and primitive buffer to SurfaceHost.
// Definitions, curves, transforms and particle events are read only within
// the current package's recorded extents (PackageExtents.hpp); a surface
// whose data the package lacks is refused with a diagnostic instead of
// indexing past it.
namespace VS::Effects::Templates {
int UpdateLivePlg049RotatingSurface(LiveRendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg053Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg061Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg061Surface(LiveRendererSlot* slot, unsigned int command,
                            int packed);
int UpdateLivePlg065RotatingSurface(LiveRendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg069Mesh10(LiveRendererSlot* slot, unsigned int command,
                           int packed);
int UpdateLivePlg073Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg073RotatingSurface(LiveRendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg077Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg077Surface(LiveRendererSlot* slot, unsigned int command,
                            int packed);
int UpdateLivePlg077Mesh10(LiveRendererSlot* slot, unsigned int command,
                           int packed);
int UpdateLivePlg081RotatingSurface(LiveRendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg082Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg085Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg086Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg086RotatingSurface(LiveRendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg087Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg087Surface(LiveRendererSlot* slot, unsigned int command,
                            int packed);
int UpdateLivePlg128Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg128Surface(LiveRendererSlot* slot, unsigned int command,
                            int packed);
int UpdateLivePlg128Surface2(LiveRendererSlot* slot, unsigned int command,
                             int packed);
int UpdateLivePlg128RotatingSurface(LiveRendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg129Surface(LiveRendererSlot* slot, unsigned int command,
                            int packed);
int UpdateLivePlg129RotatingSurface(LiveRendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg129Mesh10(LiveRendererSlot* slot, unsigned int command,
                           int packed);
int UpdateLivePlg137Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg137Mesh10(LiveRendererSlot* slot, unsigned int command,
                           int packed);
int UpdateLivePlg138Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg138RotatingSurface(LiveRendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg139Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg142Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg142RotatingSurface(LiveRendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg146MirrorSurface(LiveRendererSlot* slot, unsigned int command,
                                  int packed);
int UpdateLivePlg150Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg150Surface(LiveRendererSlot* slot, unsigned int command,
                            int packed);
int UpdateLivePlg152Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg153Mesh10(LiveRendererSlot* slot, unsigned int command,
                           int packed);
int UpdateLivePlg154Mesh10(LiveRendererSlot* slot, unsigned int command,
                           int packed);
int UpdateLivePlg156Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg158Surface(LiveRendererSlot* slot, unsigned int command,
                            int packed);
int UpdateLivePlg158MirrorSurface(LiveRendererSlot* slot, unsigned int command,
                                  int packed);
int UpdateLivePlg158RotatingSurface(LiveRendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg161Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg161RotatingSurface(LiveRendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg162RotatingSurface(LiveRendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg165Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg165RotatingSurface(LiveRendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg166MirrorSurface(LiveRendererSlot* slot, unsigned int command,
                                  int packed);
int UpdateLivePlg166RotatingSurface(LiveRendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg167RotatingSurface(LiveRendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg168Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg170Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg170Surface(LiveRendererSlot* slot, unsigned int command,
                            int packed);
int UpdateLivePlg170MirrorSurface(LiveRendererSlot* slot, unsigned int command,
                                  int packed);
int UpdateLivePlg170RotatingSurface(LiveRendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg236RotatingSurface(LiveRendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg242Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg246RotatingSurface(LiveRendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg250RotatingSurface(LiveRendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg260Mesh10(LiveRendererSlot* slot, unsigned int command,
                           int packed);
int UpdateLivePlg263Surface(LiveRendererSlot* slot, unsigned int command,
                            int packed);
int UpdateLivePlg272Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg272Surface(LiveRendererSlot* slot, unsigned int command,
                            int packed);
int UpdateLivePlg273MirrorSurface(LiveRendererSlot* slot, unsigned int command,
                                  int packed);
int UpdateLivePlg291Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg297Grid(LiveRendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg297MirrorSurface(LiveRendererSlot* slot, unsigned int command,
                                  int packed);
int UpdateLivePlg300MirrorSurface(LiveRendererSlot* slot, unsigned int command,
                                  int packed);
}  // namespace VS::Effects::Templates
