// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_SPHERE_H_TYPES
#define VS_BATTLE_SPHERE_H_TYPES
#include "Legacy/Psx.h"


// decomp: src/include/gpu.h:110
#define vs_getUV1Tpage(u, v, x, y, tp, abr)                                              \
    (((u) | ((v) << 8)) | (getTPage((tp), (abr), (x), (y)) << 16))

#endif // VS_BATTLE_SPHERE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_SPHERE_H_DECLS)
#define VS_BATTLE_SPHERE_H_DECLS
#include "Battle/BattleTypes.h"
#include "Battle/TargetVolume.h"
#include "Battle/Targeting.h"

extern short Battle_sphereArcBases[502];
extern short Battle_sphereMirroredVertices[430];
extern u_char Battle_sphereShapeCommandStarts[488];
extern u_short Battle_sphereWireframeCommands[218];
extern int Battle_positionCursorSpriteOffsets[126];
extern int Battle_positionCursorSpriteSizes[121];
extern int Battle_positionCursorSpriteTexCoords[116];
extern u_char Battle_positionCursorBlinkFrame;
extern u_char Battle_reuseSphereVolumes;
extern Battle_TargetSphereSetupView* Battle_targetSphere;
#include "Battle/TargetPages.h"

void Battle_InitSphere(void);
void Battle_FreeSphere(void);
void Battle_SetSphereVolumes(Battle_TargetSphereSpec* primary_volume, Battle_TargetSphereSpec* secondary_volume, u_char sphere_mode, Battle_TargetPreview* target_previews);
void Battle_ClearPrimarySphereVolume(void);
void Battle_SetSecondarySphereVolume(Battle_TargetSphereSpec* secondary_volume);
void Battle_ClearSecondarySphereVolume(void);
int Battle_AppendSphereArcVertices(int phase, int segments, int index);
MATRIX* Battle_BuildSphereVolumeTransform(u_char* scale, int angle);
void Battle_DrawSphereWireLine(SVECTOR* start, SVECTOR* end, u_int color, int intensity);
void Battle_DrawSphere(void);
void Battle_BuildTargetVolumeTestTransform(Battle_TargetVolume* volume, Battle_TargetVolumeMatrix* test_transform, MATRIX* rotation_matrix);
int Battle_IsPointInTargetEllipsoid(Battle_TargetVolume* volume, u_short* point_components);
int Battle_IsPointInTargetCylinder(Battle_TargetVolume* volume, u_short* point_components, int shift_height_origin);
int Battle_IsPointInTargetCone(Battle_TargetVolume* volume, u_short* point_components, int shift_height_origin);
int Battle_IsPointInTargetPrism(Battle_TargetVolume* shape, u_short* position, int angled);
int Battle_IsPointInTargetVolume(Battle_TargetVolume* volume, u_short* point_components);
void Battle_ClearSphereTargets(void);
void Battle_DrawRightTargetLabelLink(int x0, int x1, int count);
void Battle_DrawLeftTargetLabelLink(int x0, int x1, int count);
int Battle_AreTargetLabelsSettled(void);
void Battle_DrawSphereTargetLabel(Battle_SphereTargetLabel* label);
void Battle_UpdateSphereTargetLabels(void);
int Battle_AppendSphereCircleVertices(int angle, int index);
int Battle_AppendSphereConeVertices(int step, int radius, int index);
void Battle_BuildSphereWireframe(int type);
void* Battle_BuildSphereCameraBasis(void);
void Battle_DrawSpherePositionCursor(SVECTOR* position, int lock_pitch);
void Battle_CacheSphereTerrainRow(SVECTOR* position, u_int row);
void Battle_UpdateSphereTargetHighlights(Battle_TargetLabelLine* target_lines, int target_count);
int Battle_SphereInteractionMode(void);

#endif // VS_BATTLE_SPHERE_H_DECLS
