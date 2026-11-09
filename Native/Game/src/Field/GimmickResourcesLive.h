#pragma once

struct Content_ActorAssetRequest;

#ifdef __cplusplus
extern "C" {
#endif
int Field_FindSharedGimmickModelEntry(
    struct Content_ActorAssetRequest* request);
void Field_SetGimmickPaletteTableEntry(const void* palette_data);
int Field_ReleaseGimmickInstanceEntry(int slot);
int Field_CreateGimmickInstanceEntry(struct Content_ActorAssetRequest* request);
#ifdef __cplusplus
}
#endif
