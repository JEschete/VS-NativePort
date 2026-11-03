#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Legacy/Psx.h"
#include "Render/CameraView.hpp"

// Live binding for the camera view (CameraView.cpp): the frame scratch camera
// and view matrix, the camera basis, the GTE screen distance and the room
// renderer's clip state.
namespace VS::Render {
void BuildLiveCameraViewMatrix();
void SetLiveCameraNearClip(int near_clip);
void SetLiveCameraProjectionDistance(int distance);
void SetLiveCameraFarClip(int far_clip, int preserve_texture_lod);
// The actor model workspace and bone scratch matrices; the bone scratch keeps
// `source`'s translation.
void PrepareLiveActorCameraMatrices(const MATRIX* source);
}  // namespace VS::Render
