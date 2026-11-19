#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstdint>
#include <optional>

#include "Core/Math/Distance.hpp"
#include "Core/Math/Interpolation.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/Camera.hpp"

// PLG128's effect camera (EFFECT/PLG128.BIN F8.c:66-249): a renderer slot that
// eases the battle's effect camera from the pose it captured at creation to an
// end pose resolved every frame from the effect definition's curves. The cores
// work on typed values and reach the package, the effect instance, the look-at
// orientation and the effect camera globals through a CameraEffectHost;
// Plg128CameraLive.hpp binds them to the slot, the scratch pad and the battle.
namespace VS::Effects::Plg128 {
// The slot commands func_800FDEC4 handles; other values return 1 untouched.
enum class CameraCommand : std::uint32_t {
  kCreate = 1,
  kTick = 2,
  kRelease = 3,
  kStop = 4,
};

// func_800FD9A4_t (src/EFFECT/PLG128.BIN/F8.c:10): the slot's state. Upstream
// names `position` unk88 and `target` unk98; func_800FDE78 stores the first as
// the end pose's position and the second as its look-at.
struct CameraEffectState {
  std::uint8_t lifetime{};
  std::uint8_t definition{};
  std::int16_t mode{};
  std::int32_t age{};
  EffectCameraPose start_camera{};
  EffectCameraPose end_camera{};
  Core::Math::Vec3 position{};
  Core::Math::Vec3 target{};
  Core::Math::Transform3 view{};
  std::int32_t near_clip{};

  friend bool operator==(const CameraEffectState&,
                         const CameraEffectState&) = default;
};

// The particle emitter scratch fields (0x1F8001D0) the camera reads and
// writes. The vector, offset and jitter are halfwords there.
struct CameraScratch {
  std::uint32_t flags{};
  Core::Math::Position3 vector{};
  Core::Math::Vec3 position{};
  Core::Math::Vec3 extent{};
  Core::Math::Vec3 base_direction{};
  std::array<std::int32_t, 2> speed{};
  Core::Math::Position3 attraction_offset{};
  Core::Math::Position3 attraction_jitter{};
  std::int32_t sin_angle0{};
  std::int32_t sin_angle1{};
  std::int32_t cos_angle0{};
  std::int32_t cos_angle1{};

  friend bool operator==(const CameraScratch&, const CameraScratch&) = default;
};

using ScalarSpan = Core::Math::ScalarSpan;
using VectorRange = std::array<ScalarSpan, Core::Math::kMatrixOrder>;

// The particle definition fields (D_800F569C block 5) the camera samples.
struct CameraDefinition {
  // The definition word's low 26 bits.
  std::uint32_t flags{};
  std::uint8_t position_curve{};
  std::uint8_t direction_curve{};
  std::uint8_t speed_curve{};
  std::uint8_t attraction_offset_curve{};
  std::uint8_t attraction_jitter_curve{};
  std::uint8_t inner_extent_curve{};
  VectorRange position_range{};
  VectorRange extent_range{};
  VectorRange direction_range{};
  std::array<Core::Math::ScalarSpan, 2> speed_range{};
  VectorRange attraction_offset_range{};
  VectorRange attraction_jitter_range{};
  Core::Math::ScalarSpan inner_extent_range{};
  std::uint8_t transform_index{};
  std::uint8_t attraction_transform{};
};

// What the resolvers read from the effect instance and the action request.
// Fields the flags and mode do not select stay zero.
struct CameraAnchors {
  Core::Math::Position3 effect_origin{};
  Core::Math::Vec3 transform_translation{};
  Core::Math::Vec3 attraction_translation{};
  std::int16_t facing{};
};

// The flag fields func_800FDA04, func_800FDC80 and func_800FDEC4 switch on.
inline constexpr std::uint32_t kPositionSourceMask = 0x7;
inline constexpr std::uint32_t kPositionFromOrigin = 0x1;
inline constexpr std::uint32_t kPositionFromTransform = 0x2;
inline constexpr std::uint32_t kOffsetShapeMask = 0x38;
inline constexpr std::uint32_t kOffsetShapeSphere = 0x0;
inline constexpr std::uint32_t kOffsetShapeDisc = 0x20;
inline constexpr std::uint32_t kTargetSourceMask = 0x1C0;
inline constexpr std::uint32_t kTargetFromOrigin = 0x80;
inline constexpr std::uint32_t kTargetFromTransform = 0xC0;
inline constexpr std::uint32_t kDefinitionFlagsMask = 0x3FFFFFF;

// The modes func_800FDEC4 resolves endpoints for: 0 ResolveCameraEndpoints,
// 1 ResolveActorFacingCamera. Other modes keep the last endpoints.
inline constexpr std::int16_t kEndpointMode = 0;
inline constexpr std::int16_t kActorFacingMode = 1;
// The transition (cosine) a camera created in an endpoint mode selects.
inline constexpr std::int32_t kCameraTransition = 2;

[[nodiscard]] constexpr bool ResolvesEndpoints(std::int16_t mode) noexcept {
  return mode == kEndpointMode || mode == kActorFacingMode;
}

// What the camera needs from the rest of the game. Queries return nullopt to
// refuse an index the live package or instance does not hold.
class CameraEffectHost {
 public:
  virtual ~CameraEffectHost() = default;
  [[nodiscard]] virtual std::optional<CameraDefinition> Definition(
      std::uint8_t index) = 0;
  // vs_battle_sampleCurve(curve, age).
  [[nodiscard]] virtual std::int32_t SampleCurve(std::uint8_t curve,
                                                 std::int32_t age) = 0;
  // D_800F5310: the action request's effect origin.
  [[nodiscard]] virtual Core::Math::Position3 EffectOrigin() = 0;
  // The current instance's transform world translation.
  [[nodiscard]] virtual std::optional<Core::Math::Vec3> TransformTranslation(
      std::uint8_t index) = 0;
  // The current instance's transform facing angle.
  [[nodiscard]] virtual std::optional<std::int16_t> TransformFacing(
      std::uint8_t index) = 0;
  // vs_battle_lookAt(from, to); the live host builds it with
  // Core::Math::BuildLookAtOrientation.
  [[nodiscard]] virtual Core::Math::Mat3 LookAt(Core::Math::Vec3 from,
                                                Core::Math::Vec3 to) = 0;
  // func_800D1B18: the field camera into `pose`, keeping its shake offsets.
  virtual void CaptureCamera(EffectCameraPose& pose) = 0;
  // D_800F54D0 = 0 and D_800F55A0 = duration.
  virtual void BeginCameraMotion(std::int16_t duration) = 0;
  // func_800D1E20.
  virtual void SetTransitionMode(std::int32_t mode) = 0;
  // D_800F5520[0] and [1].
  virtual void PublishCameraMotionEnds(const EffectCameraPose& start,
                                       const EffectCameraPose& end) = 0;

 protected:
  CameraEffectHost() = default;
  CameraEffectHost(const CameraEffectHost&) = default;
  CameraEffectHost& operator=(const CameraEffectHost&) = default;
  CameraEffectHost(CameraEffectHost&&) = default;
  CameraEffectHost& operator=(CameraEffectHost&&) = default;
};

// The anchors `mode` and `flags` select, read before anything is written:
// nullopt when the host refuses a transform the original read unchecked.
[[nodiscard]] std::optional<CameraAnchors> GatherCameraAnchors(
    CameraEffectHost& host, const CameraDefinition& definition,
    std::uint32_t flags, std::int16_t mode);

// func_800FD9A4: the view looks from the target towards the position and is
// translated to `translation`.
void BuildCameraOffsetTransform(CameraEffectHost& host,
                                CameraEffectState& state,
                                Core::Math::Vec3 translation);

// func_800FDA04: the position is the scratch position, moved by the effect
// origin or a transform; the target is the attraction offset, moved in
// halfwords by a transform or the effect origin.
void ResolveCameraEndpoints(CameraEffectState& state, CameraScratch& scratch,
                            const CameraAnchors& anchors) noexcept;

// func_800FDC80: the target is the resolved scratch position, and the position
// stands the jitter's x behind it along the transform's facing.
void ResolveActorFacingCamera(CameraEffectState& state, CameraScratch& scratch,
                              const CameraAnchors& anchors) noexcept;

// func_800FDE78: the end pose's position and look-at in 12 fractional bits.
void StoreCameraEndpoints(CameraEffectState& state) noexcept;

// func_800FDEC4: one slot command; 0 ends the slot. A tick whose definition or
// anchors the host refuses ends the slot without writing anything.
[[nodiscard]] std::int32_t UpdateEffectCamera(CameraEffectHost& host,
                                              CameraEffectState& state,
                                              CameraScratch& scratch,
                                              std::uint32_t command,
                                              std::int32_t packed);
}  // namespace VS::Effects::Plg128
