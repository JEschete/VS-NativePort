#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"

// Camera view matrices and clip settings. The camera pose comes in as values
// and the matrices go out as values; clip settings reach the projection and
// the room renderer through a CameraClipTarget. Nothing here reads the frame
// scratch, GTE registers or module addresses.
namespace VS::Render {
// The camera as the frame scratch records it (Render_Camera).
struct CameraPose {
  Core::Math::Vec3 position;
  Core::Math::Vec3 look_at;
  // Rotation about the view axis, 4096 per turn (angles.vz).
  std::int32_t roll = 0;
  std::int32_t far_clip = 0;
};

struct CameraView {
  // World-to-view transform scaled by the far clip.
  Core::Math::Transform3 view;
  // Rows -right.x/right.y/right.z, -up.x/up.y/up.z and
  // -forward.x/forward.y/forward.z, before roll and scaling.
  Core::Math::Mat3 basis;
};

// BATTLE.PRG func_8007ACB0 (146C.c:8157).
[[nodiscard]] CameraView BuildCameraViewMatrix(const CameraPose& pose) noexcept;

// BATTLE.PRG func_800D197C (5BF94.c:6487): a view matrix looking from
// `position` at `target` with -y up.
[[nodiscard]] Core::Math::Transform3 BuildLookAtViewMatrix(
    Core::Math::Vec3 position, Core::Math::Vec3 target) noexcept;

struct ActorCameraMatrices {
  // The inverse camera basis with the actor aspect, roll and far-clip scale.
  Core::Math::Mat3 rotation;
  // 4096 * 4096 / far clip, the scale applied to every column.
  std::int32_t far_scale = 0;
};

// BATTLE.PRG func_800AECA0 (44F14.c:853). A zero far clip gives the R3000
// division result (-1) instead of a trap.
[[nodiscard]] ActorCameraMatrices PrepareActorCameraMatrices(
    const Core::Math::Mat3& basis, std::int32_t roll,
    std::int32_t far_clip) noexcept;

// Where the camera clip settings are recorded and applied.
class CameraClipTarget {
 public:
  virtual ~CameraClipTarget() = default;
  virtual void RecordNearClip(std::int32_t near_clip) = 0;
  virtual void ClipRoomPolygonsNear(std::int32_t near_clip) = 0;
  virtual void RecordProjectionDistance(std::int32_t distance) = 0;
  virtual void SetScreenDistance(std::int32_t distance) = 0;
  virtual void RecordFarClip(std::int32_t far_clip) = 0;
  virtual void SetRoomTextureLodDepth(std::int32_t depth) = 0;
};

inline constexpr std::int32_t kMaxRoomTextureLodDepth = 0x800;

// far_clip / 8 rounded toward zero, clamped to 0..kMaxRoomTextureLodDepth.
[[nodiscard]] std::int32_t TextureLodDepthForFarClip(
    std::int32_t far_clip) noexcept;

// BATTLE.PRG vs_battle_setNearClip (146C.c:9041).
void SetCameraNearClip(CameraClipTarget& target, std::int32_t near_clip);
// BATTLE.PRG vs_battle_setProjectionDistance (146C.c:9047).
void SetCameraProjectionDistance(CameraClipTarget& target,
                                 std::int32_t distance);
// BATTLE.PRG vs_battle_setFarClip (146C.c:9053): the room texture LOD depth
// follows the far clip unless `preserve_texture_lod` is set.
void SetCameraFarClip(CameraClipTarget& target, std::int32_t far_clip,
                      bool preserve_texture_lod);
}  // namespace VS::Render
