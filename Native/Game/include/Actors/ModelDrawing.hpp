#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <span>

// Actor model drawing order and bone endpoint projection. The model runtime
// owns poses, geometry, texture slots, the GTE and the ordering table; these
// policies reach them only through the hosts below.
namespace VS::Actors {
// Actors at or below this height are hidden unless they are the leader or an
// event is playing.
inline constexpr int kHiddenActorHeight = 256;
inline constexpr int kNeutralModelColor = 128;
inline constexpr int kColorAdjustmentScale = 4;

// How the companion argument of the original draw selects geometry: 0 draws
// the actor itself, 1 overlays the actor's geometry in its companion color,
// and a larger value draws the geometry of that actor slot.
enum class ModelDrawRole : std::uint8_t {
  kSelf = 0,
  kCompanionOverlay = 1,
  kCompanionGeometry = 2,
};

[[nodiscard]] ModelDrawRole ClassifyModelDraw(int companion);

enum class ModelPolygons : std::uint8_t {
  // The geometry's own textured polygons in its base color.
  kTextured,
  // The companion overlay in the companion color, Gouraud for UV-laid-out
  // geometry and flat otherwise.
  kCompanionGouraud,
  kCompanionFlat,
};

enum class ModelDrawResult : std::uint8_t {
  kNoPose,
  kHidden,
  kMissingGeometry,
  kInvalidGeometry,
  // Projection failed; only the shadow can still be drawn.
  kCulled,
  kDrawn,
};

class ActorModelDrawHost {
 public:
  virtual ~ActorModelDrawHost() = default;
  // Builds the bone matrices; false when the pose cannot be built.
  [[nodiscard]] virtual bool BuildPose() = 0;
  [[nodiscard]] virtual bool IsVisible() const = 0;
  [[nodiscard]] virtual int Height() const = 0;
  [[nodiscard]] virtual bool IsLeader() const = 0;
  [[nodiscard]] virtual bool IsEventPlaying() const = 0;
  // Selects the actor itself (slot 0) or the actor in `slot` as the geometry
  // source; false when that slot holds no actor.
  [[nodiscard]] virtual bool SelectGeometry(int slot) = 0;
  // Checks the selected geometry against the pose and binds its model, texture
  // page and palettes; false when the geometry cannot be drawn.
  [[nodiscard]] virtual bool BindGeometry() = 0;
  [[nodiscard]] virtual bool ProjectVertices() = 0;
  // Records whether the vertex projection failed.
  virtual void SetCulled(bool culled) = 0;
  // Refreshes the geometry's base and shadow colors for `role`.
  virtual void UpdateColors(ModelDrawRole role) = 0;
  [[nodiscard]] virtual bool GeometryHasUvLayout() const = 0;
  virtual void DrawPolygons(ModelPolygons polygons) = 0;
  [[nodiscard]] virtual bool CastsShadow() const = 0;
  virtual void DrawShadow() = 0;
};

// Draws one actor model and, for the actor's own draw, its ground shadow
// (func_800B28A8 material dispatch).
ModelDrawResult DrawActorModel(ActorModelDrawHost& host, int companion);

// The base channel shifted by four times the signed byte adjustment, clamped to
// a byte.
[[nodiscard]] std::uint8_t AdjustModelColorChannel(std::uint8_t base,
                                                   std::uint8_t adjustment);
// A ground color channel halved toward neutral gray for the shadow tint.
[[nodiscard]] std::uint8_t GroundShadowChannel(std::uint8_t ground);

inline constexpr int kBoneProjectionMissing = -1;
inline constexpr int kBoneProjectionInvalidBone = -4;

enum class BoneEndpoint : std::uint8_t {
  kBase = 0,
  kTip = 1,
  kMiddle = 2,
};

// The original endpoint argument: 0 is the bone base, 1 its tip and any other
// value its midpoint.
[[nodiscard]] BoneEndpoint ClassifyBoneEndpoint(int endpoint);
// The bone-local x offset of the endpoint along a bone of `length`.
[[nodiscard]] std::int16_t BoneEndpointOffset(std::int32_t length,
                                              BoneEndpoint endpoint);
// The projected depth clamped to the 16-bit depth range.
[[nodiscard]] std::int32_t ClampProjectedDepth(std::int32_t depth);

enum class BoneLookup : std::uint8_t { kFound, kMissing, kInvalidBone };

struct BoneSelection {
  BoneLookup status = BoneLookup::kMissing;
  std::int32_t length = 0;
};

struct ProjectedBonePoint {
  // Packed screen x (low half) and y (high half).
  std::int32_t screen = 0;
  std::int32_t depth = 0;
};

class BoneProjectionHost {
 public:
  virtual ~BoneProjectionHost() = default;
  // Selects the bone of the actor or gimmick in `slot` for projection.
  [[nodiscard]] virtual BoneSelection SelectBone(int slot, int bone) = 0;
  // Projects the bone-local point (offset_x, 0, 0) through the selected bone.
  [[nodiscard]] virtual ProjectedBonePoint Project(std::int16_t offset_x) = 0;
};

// Projects a bone endpoint into `output` as packed screen coordinates and
// clamped depth. Returns 0, kBoneProjectionMissing or
// kBoneProjectionInvalidBone (func_800A141C).
int ProjectActorBoneEndpoint(BoneProjectionHost& host, int slot, int bone,
                             int endpoint, std::span<std::int32_t, 2> output);
}  // namespace VS::Actors
