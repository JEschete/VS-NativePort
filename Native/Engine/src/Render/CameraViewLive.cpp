// Hand-maintained (tools/port/owned.csv).
// Live binding for the camera view (CameraView.cpp).
#include "Render/CameraViewLive.hpp"

#include <cstdint>

#include "Core/Math/MatrixLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Render/RoomDraw.hpp"
#include "Render/RoomDrawLive.hpp"

extern "C" {
#include "Actors/SkeletonLiveState.h"
#include "Core/Gte.h"
#include "Render/Camera.h"
#include "Render/ModelDraw.h"
#include "Render/ModelWorkspace.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace VS::Render {
namespace {
class LiveCameraClip final : public CameraClipTarget {
 public:
  void RecordNearClip(std::int32_t near_clip) override {
    Render_nearClip = near_clip;
  }
  void ClipRoomPolygonsNear(std::int32_t near_clip) override {
    SetRoomPolygonNearClip(LiveRoomClipState(), near_clip);
  }
  void RecordProjectionDistance(std::int32_t distance) override {
    Render_projectionDistance = distance;
  }
  void SetScreenDistance(std::int32_t distance) override {
    Core_Gte_SetGeomScreen(distance);
  }
  void RecordFarClip(std::int32_t far_clip) override {
    Render_frameScratch.camera.farClip = far_clip;
  }
  void SetRoomTextureLodDepth(std::int32_t depth) override {
    VS::Render::SetRoomTextureLodDepth(LiveRoomClipState(), depth);
  }
};
}  // namespace

void BuildLiveCameraViewMatrix() {
  Render_FrameScratch& scratch = Render_frameScratch;
  const CameraPose pose{
      .position = Core::Math::VectorOf(scratch.camera.position),
      .look_at = Core::Math::VectorOf(scratch.camera.lookAt),
      .roll = static_cast<std::int32_t>(scratch.camera.angles.vz),
      .far_clip = scratch.camera.farClip,
  };
  const CameraView view = BuildCameraViewMatrix(pose);
  Core::Math::StoreTransform(scratch.viewMatrix, view.view);
  Core::Math::StoreMatrix(Render_cameraBasisMatrix, view.basis);
}

void SetLiveCameraNearClip(int near_clip) {
  LiveCameraClip target;
  SetCameraNearClip(target, near_clip);
}

void SetLiveCameraProjectionDistance(int distance) {
  LiveCameraClip target;
  SetCameraProjectionDistance(target, distance);
}

void SetLiveCameraFarClip(int far_clip, int preserve_texture_lod) {
  LiveCameraClip target;
  SetCameraFarClip(target, far_clip, preserve_texture_lod != 0);
}

void PrepareLiveActorCameraMatrices(const MATRIX* source) {
  const Render_FrameScratch& scratch = Render_frameScratch;
  const ActorCameraMatrices matrices = PrepareActorCameraMatrices(
      Core::Math::MatrixOf(Render_cameraBasisMatrix),
      static_cast<std::int32_t>(scratch.camera.angles.vz),
      scratch.camera.farClip);
  Render_ModelScratchpad* workspace = Render_ModelWorkspace();
  // The original copied an uninitialised stack translation along with the
  // rotation; the workspace translation is cleared instead.
  Core::Math::StoreTransform(workspace->matrix,
                             Core::Math::Transform3{matrices.rotation, {}});
  Core::Math::StoreVector(
      workspace->scale,
      {matrices.far_scale, matrices.far_scale, matrices.far_scale});
  // A missing source (a crash in the original) leaves no translation.
  const Core::Math::Vec3 translation =
      source != nullptr
          ? Core::Math::Vec3{static_cast<std::int32_t>(source->t[0]),
                             static_cast<std::int32_t>(source->t[1]),
                             static_cast<std::int32_t>(source->t[2])}
          : Core::Math::Vec3{};
  Core::Math::StoreTransform(
      Actors_boneMatrixScratch,
      Core::Math::Transform3{matrices.rotation, translation});
}
}  // namespace VS::Render
