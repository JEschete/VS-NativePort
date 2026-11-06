#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/Distance.hpp"
#include "Core/Math/Interpolation.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/Curves.hpp"

// Effect transforms (BATTLE.PRG 5BF94.c:5940-6310): each of an effect
// instance's transforms follows its package definition (block 10). A
// transform animates a local rotation, translation and scale from the curve
// samples, attaches to an actor bone, the camera or the action origin, moves
// towards a target along a line or a spline, and may face its target. The
// cores work on TransformDefinition and TransformState values and reach the
// actors, the camera and the GTE through a TransformHost; TransformsLive.hpp
// binds them to the effect instance records.
namespace VS::Effects {
// Effects_EffectInstance.transforms (5BF94.h:490).
inline constexpr std::size_t kEffectTransformSlots = 16;
// Actor ids past the actor table name the camera, the camera offset towards
// the eye and the action origin (5BF94.c:6213).
inline constexpr std::uint8_t kCameraAttachment = 0x10;
inline constexpr std::uint8_t kCameraOffsetAttachment = 0x11;
inline constexpr std::uint8_t kOriginAttachment = 0x12;
inline constexpr std::uint8_t kNoAttachment = 0xFF;

// func_800D0B30_t (5BF94.c:200): a block-10 transform definition. The range
// tables hold (start, end) pairs for x, y and z.
struct TransformDefinition {
  std::uint16_t flags{};
  std::uint8_t attach_actor{};
  std::uint8_t attach_bone{};
  std::uint8_t translation_curve{};
  std::uint8_t curve{};
  std::uint8_t rotation_curve{};
  std::uint8_t scale_curve{};
  std::uint16_t rotation_x{};
  std::uint16_t mode{};
  std::uint16_t rotation_y{};
  std::uint8_t limit{};
  std::uint8_t radius{};
  std::uint8_t radius_jitter{};
  std::uint16_t rotation_z{};
  std::uint16_t shape{};
  std::array<Core::Math::ScalarSpan, Core::Math::kMatrixOrder> rotation_range{};
  std::array<Core::Math::ScalarSpan, Core::Math::kMatrixOrder>
      translation_range{};
  std::array<Core::Math::ScalarSpan, Core::Math::kMatrixOrder> scale_range{};

  friend bool operator==(const TransformDefinition&,
                         const TransformDefinition&) = default;
};

// The bytes of one block-10 definition (Effects_TransformDefinition).
inline constexpr std::size_t kTransformDefinitionBytes = 0x38;

// Decodes the whole little-endian definitions at the start of a block-10
// payload into `out`, at most out.size(); returns how many. A trailing
// partial record is not decoded.
std::size_t ReadTransformDefinitions(
    std::span<const std::byte> block,
    std::span<TransformDefinition> out) noexcept;

// A halfword point with its fourth halfword (an SVECTOR); the saved target
// keeps the attached actor's facing angle there.
struct TransformPoint {
  std::int16_t x{};
  std::int16_t y{};
  std::int16_t z{};
  std::int16_t pad{};

  friend bool operator==(const TransformPoint&,
                         const TransformPoint&) = default;
};

// func_800D0548_arg1 (5BF94.c:6076): one transform's animation state.
struct TransformState {
  std::array<std::int16_t, Core::Math::kMatrixOrder> rotation{};
  std::uint16_t flags{};
  std::int16_t frame{};
  Core::Math::Vec3 position{};
  std::uint8_t actor{};
  std::uint8_t bone{};
  std::uint8_t target_actor{};
  std::uint8_t target_bone{};
  Core::Math::Transform3 world{};
  Core::Math::Transform3 view{};
  Core::Math::Transform3 attachment{};
  Core::Math::Transform3 facing{};
  TransformPoint saved{};
  TransformPoint control0{};
  TransformPoint control1{};

  friend bool operator==(const TransformState&,
                         const TransformState&) = default;
};

// An action effect endpoint (Effects_ActionEffectTarget): an actor part when
// `type` is kActorPartEndpoint, otherwise a point.
inline constexpr std::uint16_t kActorPartEndpoint = 4;
struct TransformEndpoint {
  std::uint16_t type{};
  std::uint8_t actor{};
  std::uint8_t part{};
  TransformPoint position{};
};

// The instance's emitter and receiver endpoints.
struct TransformEndpoints {
  TransformEndpoint emit_from{};
  TransformEndpoint emit_to{};
};

// The render camera's eye and look-at point (Q12 world units).
struct TransformCameraView {
  Core::Math::Vec3 position{};
  Core::Math::Vec3 look_at{};
};

// What the transforms need from the rest of the game.
class TransformHost {
 public:
  virtual ~TransformHost() = default;
  // LiveActorAnchorMatrix: the bone's camera-relative matrix; a refused
  // actor leaves `attachment` as it was.
  virtual void ActorAnchorMatrix(std::uint8_t actor, std::uint8_t bone,
                                 Core::Math::Transform3& attachment) = 0;
  // Actors_ActorPlacement's facing angle, or nullopt when the actor has no
  // placement.
  [[nodiscard]] virtual std::optional<std::int16_t> ActorFacing(
      std::uint8_t actor) = 0;
  // LiveActorBonePosition: the bone end's camera-relative point.
  virtual void ActorBonePosition(std::uint8_t actor, std::uint8_t bone,
                                 TransformPoint& position) = 0;
  [[nodiscard]] virtual TransformCameraView FrameCamera() = 0;
  [[nodiscard]] virtual Core::Math::Transform3 ViewMatrix() = 0;
  // The action request's target point (Effects_effectOrigin).
  [[nodiscard]] virtual TransformPoint EffectOrigin() = 0;
  // The curve samples of the current frame, by curve id.
  [[nodiscard]] virtual std::span<const std::int32_t> CurveSamples() = 0;
  // SampleEffectCurve over the current package.
  [[nodiscard]] virtual CurveSample SampleCurve(std::int32_t curve_id,
                                                std::int32_t distance) = 0;
  // The BIOS rand() draw (0..0x7FFF).
  [[nodiscard]] virtual std::int32_t Random() = 0;
  // The GTE RTV0TR of `point` (halfwords) through `transform`; the live host
  // leaves the GTE registers as the original's RotTrans does.
  [[nodiscard]] virtual Core::Math::Vec3 RotateTranslate(
      const Core::Math::Transform3& transform, Core::Math::Vec3 point) = 0;

 protected:
  TransformHost() = default;
  TransformHost(const TransformHost&) = default;
  TransformHost& operator=(const TransformHost&) = default;
  TransformHost(TransformHost&&) = default;
  TransformHost& operator=(TransformHost&&) = default;
};

// How a transform step ended.
enum class TransformStatus : std::uint8_t {
  kApplied,
  // The definition's easing (mode bits 5-6) is 3, which the original leaves
  // uninitialized (refused; the position is kept).
  kUnknownEasing,
  // A curve id indexes past the curve samples (refused; the transform is
  // skipped this frame).
  kCurveOutOfRange,
  // The attached actor has no placement, so its facing is kept (refused).
  kNoPlacement,
};

// Transforms an update pass applied and refused.
struct TransformPass {
  std::int32_t applied{};
  std::int32_t refused{};
};

// func_800CFEF0 (5BF94.c:5940): resets the first min(definitions, states)
// transforms and resolves their attachments and targets from the
// definitions and the instance's endpoints.
std::size_t InitializeTransformStates(
    std::span<const TransformDefinition> definitions,
    const TransformEndpoints& endpoints, std::span<TransformState> states);
// func_800D01E4 (5BF94.c:6020): moves the position along the line between
// the world translation and the saved target.
TransformStatus InterpolateTransformPosition(
    const TransformDefinition& definition, TransformState& state,
    TransformHost& host);
// func_800D037C (5BF94.c:6047): moves the position along the Lagrange curve
// through the line's ends and the two spline controls.
TransformStatus InterpolateTransformSpline(
    const TransformDefinition& definition, TransformState& state,
    TransformHost& host);
// func_800D0548 (5BF94.c:6076): one motion step, then the frame advances
// while the definition's limit allows.
TransformStatus AdvanceTransformMotion(const TransformDefinition& definition,
                                       TransformState& state,
                                       TransformHost& host);
// func_800D05F4 (5BF94.c:6093): the spline controls, offset by the
// definition's radius and angle (with random jitter) around the line.
void BuildTransformSplineControls(const TransformDefinition& definition,
                                  TransformState& state, TransformHost& host);
// func_800D0984 (5BF94.c:6152): attaches to the camera, looking from the
// look-at point to the eye, optionally pushed towards the eye.
void BuildCameraAttachment(TransformState& state, TransformHost& host,
                           bool offset_toward_camera);
// func_800D0B08 (5BF94.c:6182): an identity attachment at the action origin.
[[nodiscard]] Core::Math::Transform3 BuildEffectOriginAttachment(
    TransformHost& host);
// func_800D0B30 (5BF94.c:6184): the animated local transform; the sampled
// rotation offset accumulates into `rotation`. nullopt (and `rotation` kept)
// when a curve id indexes past `curve_samples`.
[[nodiscard]] std::optional<Core::Math::Transform3> BuildAnimatedLocalTransform(
    const TransformDefinition& definition,
    std::array<std::int16_t, Core::Math::kMatrixOrder>& rotation,
    std::span<const std::int32_t> curve_samples);
// func_800D0C60 (5BF94.c:6204): rebuilds the attachment of the transform's
// actor, the camera or the origin.
TransformStatus ResolveTransformAttachment(TransformState& state,
                                           TransformHost& host);
// func_800D0D08 (5BF94.c:6227): one frame of the first min(definitions,
// states) transforms: local animation, target, attachment, motion, facing,
// world and view matrices.
TransformPass UpdateEffectTransforms(
    std::span<const TransformDefinition> definitions,
    std::span<TransformState> states, TransformHost& host);
}  // namespace VS::Effects
