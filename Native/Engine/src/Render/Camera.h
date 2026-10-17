// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_CAMERA_H_TYPES
#define VS_RENDER_CAMERA_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_RENDER_CAMERA_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_RENDER_CAMERA_H_DECLS)
#define VS_RENDER_CAMERA_H_DECLS

extern MATRIX Render_actorCameraMatrix;
extern int Render_nearClip;
extern MATRIX Render_cameraBasisMatrix;
extern int Render_projectionDistance;
void Render_BuildCameraViewMatrix(void);
void Render_SetCameraNearClip(int arg0);
void Render_SetCameraProjectionDistance(int projectionDistance);
void Render_SetCameraFarClip(int farClip, int arg1);
void Render_BuildLookAtViewMatrix(VECTOR* position, VECTOR* target, MATRIX* view);

#endif // VS_RENDER_CAMERA_H_DECLS
