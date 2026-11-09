#pragma once

#ifdef __cplusplus
extern "C" {
#endif
struct Field_GimmickSnapshot;
struct Field_GimmickInstance;
struct Actors_ActorRecord;
int Field_GimmickPlacementEntry(int gimmick_slot,
                                struct Field_GimmickSnapshot* output_snapshot);
int Field_FindGimmickBelowEntry(const struct Field_GimmickInstance* reference);
int Field_FindTopGimmickAtTileEntry(int tile_x, int tile_z,
                                    int require_idle_mode);
int Field_GimmickSubtypeEntry(int slot);
int Field_AreGimmicksMovingEntry(void);
int Field_SetGimmickOrientationEntry(int slot, int orientation);
int Field_CanPlaceGimmickAtPointEntry(int x, int z, int minimum_height);
void Field_DropUnsupportedGimmickStackEntry(int slot);
int Field_FindInteractableGimmickAheadEntry(int actor_slot);
int Field_BeginGimmickInteractionEntry(int actor_slot);
void Field_ApplyGimmickInteractionCommandEntry(struct Actors_ActorRecord* actor,
                                               int mode);
int Field_MeasureTileStackEntry(int tile_x, int tile_z, int* height_offset);
int Field_IsGimmickWithinHeightBandEntry(int tile_x, int tile_z,
                                         int minimum_height);
#ifdef __cplusplus
}
#endif
