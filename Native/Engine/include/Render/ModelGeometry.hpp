#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"

// SHP model projection, polygon packets and the actor ground shadow. Every
// read of model data stays inside the byte span the caller supplies, every
// bone index is bounded by the matrices the caller holds, and packets reach
// the frame only through a ModelPacketQueue (#269).
namespace VS::Render {
// Vertices the model render context can hold.
inline constexpr std::size_t kModelVertexCapacity = 768;
// Bones a model header may claim.
inline constexpr std::size_t kModelBoneLimit = 63;
// Bytes in the SHP header before the bone table.
inline constexpr std::size_t kModelHeaderBytes = 0x40;
// Words in the largest polygon packet (a Gouraud quad).
inline constexpr std::size_t kModelPacketWords = 13;
inline constexpr std::uint32_t kNeutralModelTint = 0x808080;

// The eight-byte prefix shared by actor, weapon and gimmick instances.
struct ModelCounts {
  std::uint8_t bones = 0;
  std::uint8_t groups = 0;
  std::uint16_t triangles = 0;
  std::uint16_t quads = 0;
  std::uint16_t other = 0;
};

struct ModelVertex {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
};

// A vertex after perspective: the packed screen position (x in the low
// halfword, y in the high) and the unclamped view-space depth.
struct ProjectedPoint {
  std::int32_t screen = 0;
  std::int32_t depth = 0;
};

// A rotation and translation in the GTE's Q12 convention.
struct ModelTransform {
  Core::Math::Mat3 rotation{};
  Core::Math::Vec3 translation{};
};

class ModelTransformer {
 public:
  virtual ~ModelTransformer() = default;
  // Loads matrix `bone` of the caller's bone matrices; the projection only
  // asks for bones below the matrix count it was given.
  virtual void LoadBone(std::size_t bone) = 0;
  virtual void LoadTransform(const ModelTransform& transform) = 0;
  // Rotates and translates without perspective.
  [[nodiscard]] virtual Core::Math::Vec3 Transform(
      const ModelVertex& vertex) = 0;
  [[nodiscard]] virtual ProjectedPoint Project(const ModelVertex& vertex) = 0;
};

struct ModelProjectionResult {
  std::uint32_t vertex_count = 0;
  // Every vertex depth OR-ed together; zero refuses the draw.
  std::int32_t combined_depth = 0;
};

// BATTLE.PRG func_800B3A68 (4B16C.s:84). Projects every vertex group through
// its bone into `screens` and `depths` (depths clamped to 0..0xFFFF). A model
// whose sections, groups or bones fall outside `model`, `matrix_count` or the
// output spans projects nothing.
[[nodiscard]] ModelProjectionResult ProjectModelVertices(
    const ModelCounts& counts, std::span<const std::byte> model,
    std::size_t matrix_count, ModelTransformer& transformer,
    std::span<std::int32_t> screens, std::span<std::int32_t> depths);

// Whether `model` holds the header, groups, vertices and every polygon the
// counts claim, with each polygon's command matching its group.
[[nodiscard]] bool ValidateModelData(const ModelCounts& counts, bool colored,
                                     std::span<const std::byte> model);

// A model's vertex section: the byte offset of its first vertex and the
// vertices that fit before its polygon section.
struct ModelVertexSection {
  std::size_t offset = 0;
  std::size_t count = 0;

  friend bool operator==(const ModelVertexSection&,
                         const ModelVertexSection&) = default;
};

// The vertex section of `model`, or nothing when its group, vertex and
// polygon offsets are misaligned, out of order or outside `model`.
[[nodiscard]] std::optional<ModelVertexSection> FindModelVertexSection(
    const ModelCounts& counts, std::span<const std::byte> model);

// Adds `delta` to vertex `vertex` of `model`, each component wrapping to 16
// bits. False, with nothing changed, when the vertex lies outside the vertex
// section.
[[nodiscard]] bool MoveModelVertex(const ModelCounts& counts,
                                   std::span<std::byte> model,
                                   std::size_t vertex,
                                   const ModelVertex& delta);

struct ModelProjectionView {
  std::span<const std::int32_t> screens;
  std::span<const std::int32_t> depths;
};

struct ModelPolygonSource {
  ModelCounts counts;
  std::span<const std::byte> model;
  ModelProjectionView projection;
};

// The workspace texture page and palette the packets use.
struct ModelSurface {
  std::uint32_t palette = 0;
  std::uint32_t texture_page = 0;
};

class ModelPacketQueue {
 public:
  virtual ~ModelPacketQueue() = default;
  // Links `packet` (word 0 is its tag) at ordering entry `depth`. False when
  // the packet does not fit in the frame's primitive buffer.
  virtual bool Emit(std::span<const std::uint32_t> packet,
                    std::uint32_t depth) = 0;
};

enum class ModelDrawStatus : std::uint8_t {
  kDrawn,
  kNoVertices,
  kTruncatedModel,
  kInvalidLimbRanges,
  kInvalidPolygon,
  kInvalidVertexIndex,
  kBufferFull,
};

// The page flags an appearance state forces on every polygon (0 for none):
// states below 7 subtract, the rest add.
[[nodiscard]] std::uint32_t ModelAppearanceMaterial(std::uint32_t state);

// BATTLE.PRG func_800B3CF0 (4B16C.s:254): untextured-colour packets with the
// polygon group's own flags.
ModelDrawStatus DrawModelFlatPolygons(const ModelPolygonSource& source,
                                      std::uint32_t tint,
                                      const ModelSurface& surface,
                                      ModelPacketQueue& queue);
// BATTLE.PRG func_800B4594 (4B16C.s:835): per-vertex colour packets.
ModelDrawStatus DrawModelGouraudPolygons(const ModelPolygonSource& source,
                                         std::uint32_t tint,
                                         const ModelSurface& surface,
                                         ModelPacketQueue& queue);

// The appearance overrides a weapon or gimmick draw applies.
struct ModelOverrides {
  // Bit 0x80 >> limb selects the pulsed palette for that limb's polygons.
  std::uint32_t pulse_mask = 0;
  std::uint32_t appearance_state = 0;
};

// BATTLE.PRG func_800B516C (4B16C.s:1646): weapon packets with the pulsed
// limb palettes and the owner's appearance material.
ModelDrawStatus DrawModelFlatPolygonsWithOverrides(
    const ModelPolygonSource& source, std::uint32_t tint,
    const ModelSurface& surface, const ModelOverrides& overrides,
    ModelPacketQueue& queue);
// BATTLE.PRG func_800B5C30 (4B16C.s:2387): gimmick packets with the pulsed
// limb palettes, the motion-state material and a uniform tint.
ModelDrawStatus DrawModelGouraudPolygonsWithOverrides(
    const ModelPolygonSource& source, std::uint32_t tint,
    const ModelSurface& surface, const ModelOverrides& overrides,
    ModelPacketQueue& queue);

// 800B3680..800B38E0: an actor draws with overrides while its palette pulses
// or its appearance state fades; `colored` is the instance's UV layout.
ModelDrawStatus DrawActorModelPolygons(const ModelPolygonSource& source,
                                       bool colored, std::uint32_t tint,
                                       const ModelSurface& surface,
                                       const ModelOverrides& overrides,
                                       ModelPacketQueue& queue);

inline constexpr std::size_t kShadowVertexCount = 18;
inline constexpr std::size_t kShadowFaceCount = 24;

// The actor state the ground shadow reads.
struct ActorShadowSource {
  std::int32_t height = 0;
  std::uint16_t radius = 0;
  std::int16_t rotation_x = 0;
  std::int16_t facing = 0;
  std::int16_t rotation_z = 0;
  std::int16_t scale_x = 0;
  std::int16_t scale_y = 0;
  std::int16_t scale_z = 0;
  std::uint16_t blend_level = 0;
  std::uint16_t radius_shrink_rate = 0;
  std::uint16_t radius_expand_rate = 0;
  // The blend colour word: red, green, then the blue upper halfword.
  std::uint32_t blend_color = 0;
  std::int8_t fade_red = 0;
};

// The battle shadow mesh: packed X/Z vertices and faces of three byte
// offsets into the projected vertices.
struct ShadowMesh {
  std::span<const std::uint32_t> vertices;
  std::span<const std::uint32_t> faces;
};

class ShadowGround {
 public:
  virtual ~ShadowGround() = default;
  // The support height at the actor's position offset by (dx, dz).
  [[nodiscard]] virtual std::int32_t SupportHeight(std::int32_t dx,
                                                   std::int32_t dz) = 0;
  // Whether the last sample stood the actor on a platform.
  [[nodiscard]] virtual bool OnPlatform() const = 0;
};

enum class ActorShadowStatus : std::uint8_t {
  kDrawn,
  kInvalidMesh,
  kBufferFull,
};

// Screen x, screen y and depth.
inline constexpr std::size_t kShadowAnchorComponents = 3;

struct ActorShadowResult {
  ActorShadowStatus status = ActorShadowStatus::kDrawn;
  // The first projected vertex.
  std::array<std::int16_t, kShadowAnchorComponents> anchor{};
};

// BATTLE.PRG func_800B4B14 (4B16C.s:1208): the ground shadow under an actor,
// tilted to the terrain slope and lit from the actor's blend colour.
ActorShadowResult DrawActorGroundShadow(const ActorShadowSource& actor,
                                        const ModelTransform& camera,
                                        const ModelVertex& root_offset,
                                        const ShadowMesh& mesh,
                                        ShadowGround& ground,
                                        ModelTransformer& transformer,
                                        ModelPacketQueue& queue);
}  // namespace VS::Render
