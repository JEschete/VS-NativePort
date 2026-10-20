#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_FIELD_TERRAINHEIGHT_H
#define VS_OWNED_FIELD_TERRAINHEIGHT_H
#ifdef __cplusplus
extern "C" {
#endif

int Field_GroundHeightAtWorldPosition(int, int);

int Field_GroundHeightAtPackedTile(unsigned int);

int Field_GroundHeightAtTile(int, int);

int Field_GroundHeightFlagsAtPackedTile(unsigned int);

int Field_GroundHeightFlagsAtWorldPosition(int, int);

int Field_GroundHeightFlagsAtTile(int, int);

#ifdef __cplusplus
}
#endif
#endif  // VS_OWNED_FIELD_TERRAINHEIGHT_H
