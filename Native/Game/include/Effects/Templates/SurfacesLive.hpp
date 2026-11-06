#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Effects_RendererSlot;

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
int UpdateLivePlg049RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg053Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg061Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg061Surface(Effects_RendererSlot* slot, unsigned int command,
                            int packed);
int UpdateLivePlg065RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg069Mesh10(Effects_RendererSlot* slot, unsigned int command,
                           int packed);
int UpdateLivePlg073Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg073RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg077Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg077Surface(Effects_RendererSlot* slot, unsigned int command,
                            int packed);
int UpdateLivePlg077Mesh10(Effects_RendererSlot* slot, unsigned int command,
                           int packed);
int UpdateLivePlg081RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg082Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg085Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg086Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg086RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg087Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg087Surface(Effects_RendererSlot* slot, unsigned int command,
                            int packed);
int UpdateLivePlg128Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg128Surface(Effects_RendererSlot* slot, unsigned int command,
                            int packed);
int UpdateLivePlg128Surface2(Effects_RendererSlot* slot, unsigned int command,
                             int packed);
int UpdateLivePlg128RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg129Surface(Effects_RendererSlot* slot, unsigned int command,
                            int packed);
int UpdateLivePlg129RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg129Mesh10(Effects_RendererSlot* slot, unsigned int command,
                           int packed);
int UpdateLivePlg137Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg137Mesh10(Effects_RendererSlot* slot, unsigned int command,
                           int packed);
int UpdateLivePlg138Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg138RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg139Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg142Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg142RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg146MirrorSurface(Effects_RendererSlot* slot,
                                  unsigned int command, int packed);
int UpdateLivePlg150Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg150Surface(Effects_RendererSlot* slot, unsigned int command,
                            int packed);
int UpdateLivePlg152Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg153Mesh10(Effects_RendererSlot* slot, unsigned int command,
                           int packed);
int UpdateLivePlg154Mesh10(Effects_RendererSlot* slot, unsigned int command,
                           int packed);
int UpdateLivePlg156Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg158Surface(Effects_RendererSlot* slot, unsigned int command,
                            int packed);
int UpdateLivePlg158MirrorSurface(Effects_RendererSlot* slot,
                                  unsigned int command, int packed);
int UpdateLivePlg158RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg161Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg161RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg162RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg165Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg165RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg166MirrorSurface(Effects_RendererSlot* slot,
                                  unsigned int command, int packed);
int UpdateLivePlg166RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg167RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg168Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg170Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg170Surface(Effects_RendererSlot* slot, unsigned int command,
                            int packed);
int UpdateLivePlg170MirrorSurface(Effects_RendererSlot* slot,
                                  unsigned int command, int packed);
int UpdateLivePlg170RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg236RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg242Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg246RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg250RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed);
int UpdateLivePlg260Mesh10(Effects_RendererSlot* slot, unsigned int command,
                           int packed);
int UpdateLivePlg263Surface(Effects_RendererSlot* slot, unsigned int command,
                            int packed);
int UpdateLivePlg272Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg272Surface(Effects_RendererSlot* slot, unsigned int command,
                            int packed);
int UpdateLivePlg273MirrorSurface(Effects_RendererSlot* slot,
                                  unsigned int command, int packed);
int UpdateLivePlg291Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg297Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed);
int UpdateLivePlg297MirrorSurface(Effects_RendererSlot* slot,
                                  unsigned int command, int packed);
int UpdateLivePlg300MirrorSurface(Effects_RendererSlot* slot,
                                  unsigned int command, int packed);
}  // namespace VS::Effects::Templates
