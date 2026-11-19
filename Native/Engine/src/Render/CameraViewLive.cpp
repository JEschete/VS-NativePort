// Hand-maintained (tools/port/owned.csv).
// Live binding for the camera view (CameraView.cpp).
#include "Render/CameraViewLive.hpp"

#include <cstdint>

#include "Actors/SkeletonLiveState.hpp"
#include "Core/Gte.h"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Render/Camera.hpp"
#include "Render/ModelDraw.hpp"
#include "Render/ModelWorkspace.hpp"
#include "Render/Render.hpp"
#include "Render/RoomDraw.hpp"
#include "Render/RoomDrawLive.hpp"
#include "Render/UiPrimitives.hpp"

namespace VS::Render {
namespace {
class LiveCameraClip final : public CameraClipTarget {
 public:
  void RecordNearClip(std::int32_t near_clip) override { nearClip = near_clip; }
  void ClipRoomPolygonsNear(std::int32_t near_clip) override {
    SetRoomPolygonNearClip(LiveRoomClipState(), near_clip);
  }
  void RecordProjectionDistance(std::int32_t distance) override {
    projectionDistance = distance;
  }
  void SetScreenDistance(std::int32_t distance) override {
    Core_Gte_SetGeomScreen(distance);
  }
  void RecordFarClip(std::int32_t far_clip) override {
    FrameScratchpad().camera.far_clip = far_clip;
  }
  void SetRoomTextureLodDepth(std::int32_t depth) override {
    VS::Render::SetRoomTextureLodDepth(LiveRoomClipState(), depth);
  }
};
}  // namespace

void BuildLiveCameraViewMatrix() {
  FrameScratch& scratch = FrameScratchpad();
  const CameraPose pose{
      .position = Core::Math::VectorOf(scratch.camera.position),
      .look_at = Core::Math::VectorOf(scratch.camera.look_at),
      .roll = static_cast<std::int32_t>(scratch.camera.angles.vz),
      .far_clip = scratch.camera.far_clip,
  };
  const CameraView view = BuildCameraViewMatrix(pose);
  Core::Math::StoreTransform(scratch.view_matrix, view.view);
  Core::Math::StoreMatrix(cameraBasisMatrix, view.basis);
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
  const FrameScratch& scratch = FrameScratchpad();
  const ActorCameraMatrices matrices = PrepareActorCameraMatrices(
      Core::Math::MatrixOf(cameraBasisMatrix),
      static_cast<std::int32_t>(scratch.camera.angles.vz),
      scratch.camera.far_clip);
  ModelScratchpad* workspace = ModelWorkspace();
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
      VS::Actors::boneMatrixScratch,
      Core::Math::Transform3{matrices.rotation, translation});
}
}  // namespace VS::Render
