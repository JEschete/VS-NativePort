#pragma once

#ifdef __cplusplus
extern "C" {
#endif
struct Field_MpdFloorTrigger;
struct SVECTOR;
struct Field_MpdFloorTrigger* Field_RoomTriggerAtTileEntry(int x, int z,
                                                           int layer);
void Field_SetRoomTriggerEnabledEntry(struct Field_MpdFloorTrigger* selected,
                                      int activation);
void Field_DisableTrapAtPositionEntry(struct SVECTOR* position);
#ifdef __cplusplus
}
#endif
