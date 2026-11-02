#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"

// MPD room geometry: group views, polygon decoding and projection,
// subdivision, depth linking, the sky dome and the room tint post-pass. Every
// read stays inside the byte span the caller supplies, clip settings are
// passed explicitly, and packets reach the frame only through a
// RoomPacketQueue.
namespace VS::Render {
// Bytes in one _mpdGeometryHeader group record (146C.h:31).
inline constexpr std::size_t kRoomGroupRecordBytes = 64;
inline constexpr std::size_t kRoomGroupLimit = 4096;
inline constexpr std::size_t kRoomTriangleRecordBytes = 32;
inline constexpr std::size_t kRoomQuadRecordBytes = 40;
// The packed base position and two delta triples every polygon starts with.
inline constexpr std::size_t kRoomCoordinateBytes = 12;
inline constexpr std::size_t kRoomTrianglePacketWords = 10;
inline constexpr std::size_t kRoomQuadPacketWords = 13;
inline constexpr int kRoomMaximumSubdivisionLevels = 3;
inline constexpr std::uint32_t kRoomLastOrderingSlot = 2047;
inline constexpr std::size_t kSkyDomeVertexCount = 34;
inline constexpr std::size_t kSkyDomeFaceCount = 64;
inline constexpr std::size_t kSkyDomeVertexBytes = 12;
inline constexpr std::size_t kSkyDomeFaceBytes = 4;
inline constexpr std::size_t kSkyDomeBytes = 664;
static_assert(kSkyDomeBytes == kSkyDomeVertexCount * kSkyDomeVertexBytes +
                                   kSkyDomeFaceCount * kSkyDomeFaceBytes);
inline constexpr std::size_t kSkyDomePacketWords = 7;
inline constexpr std::size_t kRoomAxes = 3;
inline constexpr std::size_t kRoomTriangleCorners = 3;
inline constexpr std::size_t kRoomQuadCorners = 4;
inline constexpr std::int32_t kRoomDefaultNearDepth = 8;
inline constexpr std::uint32_t kRoomDefaultLodDepth = 512;
inline constexpr std::int32_t kRoomViewportWidth = 320;
inline constexpr std::int32_t kRoomViewportHeight = 224;
// One in the GTE's Q12 fixed point.
inline constexpr std::int32_t kRoomUnitScale = 4096;

struct RoomPosition {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;

  friend bool operator==(const RoomPosition&, const RoomPosition&) = default;
};

// A subdivision vertex: the 0xBBGGRR colour and the texture coordinate with
// u in the low byte and v in the high byte.
struct RoomVertex {
  RoomPosition position;
  std::uint32_t color = 0;
  std::uint16_t uv = 0;

  friend bool operator==(const RoomVertex&, const RoomVertex&) = default;
};

// Projected screen extrema, or a clip rectangle whose left and top edges are
// inclusive and whose right and bottom edges are exclusive.
struct RoomScreenBounds {
  std::int32_t left = 0;
  std::int32_t top = 0;
  std::int32_t right = 0;
  std::int32_t bottom = 0;

  friend bool operator==(const RoomScreenBounds&,
                         const RoomScreenBounds&) = default;
};

// The group flags, command byte, palette and texture page a packet carries.
struct RoomPolygonMaterial {
  std::uint32_t flags = 0;
  std::uint32_t command = 0;
  std::uint32_t clut = 0;
  std::uint32_t page = 0;
};

// The clip and texture level-of-detail settings room polygons draw with.
struct RoomClipState {
  std::int32_t near_depth = kRoomDefaultNearDepth;
  std::int32_t far_depth = static_cast<std::int32_t>(kRoomLastOrderingSlot);
  std::uint32_t lod_depth = kRoomDefaultLodDepth;
  RoomScreenBounds screen{0, 0, kRoomViewportWidth, kRoomViewportHeight};
};

// A rotation and translation in the GTE's Q12 convention.
struct RoomView {
  Core::Math::Mat3 rotation{};
  Core::Math::Vec3 translation{};

  friend bool operator==(const RoomView&, const RoomView&) = default;
};

// The frame camera a room view is built from: its view, its zoom scale (the
// camera far clip, Q12) and its roll angle.
struct RoomCamera {
  RoomView view;
  std::int32_t scale = kRoomUnitScale;
  std::int32_t roll = 0;
};

// A vertex after perspective: the packed screen position (x in the low
// halfword, y in the high) and the quarter depth (SZ3 >> 2).
struct RoomProjectedPoint {
  std::int32_t screen = 0;
  std::int32_t depth = 0;
};

struct RoomScreenOffset {
  std::int32_t x = 0;
  std::int32_t y = 0;

  friend bool operator==(const RoomScreenOffset&,
                         const RoomScreenOffset&) = default;
};

class RoomTransformer {
 public:
  virtual ~RoomTransformer() = default;
  virtual void LoadView(const RoomView& view) = 0;
  [[nodiscard]] virtual RoomProjectedPoint Project(
      const RoomPosition& position) = 0;
  // Rotates and translates without perspective.
  [[nodiscard]] virtual Core::Math::Vec3 Transform(
      const RoomPosition& position) = 0;
  [[nodiscard]] virtual RoomScreenOffset ScreenOffset() const = 0;
  virtual void SetScreenOffset(const RoomScreenOffset& offset) = 0;
};

class RoomPacketQueue {
 public:
  virtual ~RoomPacketQueue() = default;
  // Links `packet` (word 0 is its tag) at ordering entry `depth`. False when
  // the packet does not fit in the space the queue was given.
  virtual bool Emit(std::span<const std::uint32_t> packet,
                    std::uint32_t depth) = 0;
};

enum class RoomDrawStatus : std::uint8_t {
  kDrawn,
  kInvalidGeometry,
  kInvalidLevels,
  kBufferFull,
};

struct RoomPolygonResult {
  RoomDrawStatus status = RoomDrawStatus::kDrawn;
  std::uint32_t emitted = 0;
};

// One _mpdGeometryHeader group (146C.h:31).
struct RoomGroup {
  // 0x100 keeps the stored view and hides the group, as does 0x1000; 0x400
  // faces the camera and bit 15 marks the alternate pass.
  std::uint32_t flags = 0;
  // Minimum and maximum per axis; the minima are the group origin.
  std::array<std::array<std::int16_t, 2>, kRoomAxes> bounds{};
  Core::Math::Angles3 angles{};
  // Byte offset of the group's polygon counts from the geometry start.
  std::uint32_t polygon_offset = 0;
  RoomView view;
};

// The group count, group records and polygon spans of `geometry` all fit.
[[nodiscard]] bool ValidateRoomGeometry(std::span<const std::byte> geometry);
// Group `index`, or nothing when it is past the stored count or the span.
[[nodiscard]] std::optional<RoomGroup> ReadRoomGroup(
    std::span<const std::byte> geometry, std::size_t index);
// Stores every field of `group` as record `index`; false when the record is
// outside the span.
bool WriteRoomGroup(std::span<std::byte> geometry, std::size_t index,
                    const RoomGroup& group);
bool WriteRoomGroupView(std::span<std::byte> geometry, std::size_t index,
                        const RoomView& view);

// BATTLE.PRG func_800985AC (2EA3C.s:1383): the group rotation applied after
// the camera, translated by the camera-rotated origin.
[[nodiscard]] RoomView BuildRoomGeometryViewMatrix(
    const RoomCamera& camera, const RoomPosition& position,
    const Core::Math::Angles3& angles);
// BATTLE.PRG func_80098648 (2EA3C.s:1429): as above with the camera rotation
// divided by its zoom scale; a non-positive scale yields a zero view.
[[nodiscard]] RoomView BuildScaleCompensatedRoomView(
    const RoomCamera& camera, const RoomPosition& position,
    const Core::Math::Angles3& angles);
// BATTLE.PRG func_800983F8 (2EA3C.s:1265): rebuilds every group view not
// flagged 0x100, facing camera groups toward the viewer. False when the group
// records do not fit in `geometry`.
bool UpdateRoomGroupTransforms(std::span<std::byte> geometry,
                               const RoomCamera& camera);

// BATTLE.PRG func_8009797C (2EA3C.s:506): the three positions of the packed
// base and signed byte deltas scaled by 2^(shift & 31).
[[nodiscard]] std::array<RoomPosition, kRoomTriangleCorners>
DecodeRoomPolygonVertices(
    std::span<const std::byte, kRoomCoordinateBytes> coordinates,
    std::uint32_t coordinate_shift);

struct RoomTriangleProjection {
  std::array<RoomPosition, kRoomTriangleCorners> positions{};
  std::array<RoomProjectedPoint, kRoomTriangleCorners> points{};
  std::int32_t winding = 0;
};

// The GTE NCLIP cross product of three packed screen positions, truncated to
// a word: positive for clockwise screen winding.
[[nodiscard]] std::int32_t RoomPolygonWinding(
    const std::array<std::int32_t, kRoomTriangleCorners>& screens);
// BATTLE.PRG func_80097A10 (2EA3C.s:548): decodes, projects and winds the
// first three corners.
[[nodiscard]] RoomTriangleProjection ProjectAndDecodeRoomPolygon(
    std::span<const std::byte, kRoomCoordinateBytes> coordinates,
    std::uint32_t coordinate_shift, RoomTransformer& transformer);

// BATTLE.PRG func_80097F70 (2EA3C.s:921): floor midpoints of the position,
// each colour channel and each texture coordinate byte.
[[nodiscard]] RoomVertex InterpolateRoomVertexMidpoint(
    const RoomVertex& first, const RoomVertex& second);
// BATTLE.PRG func_80098014 (2EA3C.s:969). Inverted projected bounds are
// never on screen.
[[nodiscard]] bool IsRoomPolygonOnScreen(const RoomScreenBounds& projected,
                                         const RoomScreenBounds& clip);
// The ordering entry a projected depth plus the signed bias in the flags'
// upper halfword links at, or nothing when it is clipped.
[[nodiscard]] std::optional<std::uint32_t> ResolveRoomPolygonDepth(
    std::int32_t projected_depth, std::uint32_t flags,
    const RoomClipState& clip);

enum class RoomLinkStatus : std::uint8_t {
  kLinked,
  kRejected,
  kInvalidPacket,
  kBufferFull,
};

// BATTLE.PRG func_800980F8 (2EA3C.s:1036): links a ten- or thirteen-word
// packet at its resolved depth.
RoomLinkStatus LinkRoomPolygonByDepth(std::span<const std::uint32_t> packet,
                                      std::int32_t projected_depth,
                                      std::uint32_t flags,
                                      const RoomClipState& clip,
                                      RoomPacketQueue& queue);

// BATTLE.PRG func_80097AEC (2EA3C.s:612): splits into seven children per
// level, recursing into the first four.
RoomPolygonResult SubdivideRoomTriangle(
    std::span<const RoomVertex, kRoomTriangleCorners> vertices, int levels,
    const RoomPolygonMaterial& material, const RoomClipState& clip,
    RoomTransformer& transformer, RoomPacketQueue& queue);
// BATTLE.PRG func_80097CF0 (2EA3C.s:752): four child quads per level plus
// four edge triangles.
RoomPolygonResult SubdivideRoomQuad(
    std::span<const RoomVertex, kRoomQuadCorners> vertices, int levels,
    const RoomPolygonMaterial& material, const RoomClipState& clip,
    RoomTransformer& transformer, RoomPacketQueue& queue);

// BATTLE.PRG func_80097388 (2EA3C.s:104): one group's triangle and quad
// records after their two counts. Bit 0 of `mode` enables subdivision.
RoomPolygonResult DrawRoomGeometryPolygons(std::span<const std::byte> polygons,
                                           std::uint32_t flags,
                                           std::int32_t mode,
                                           const RoomClipState& clip,
                                           RoomTransformer& transformer,
                                           RoomPacketQueue& queue);

struct RoomDrawPass {
  // Bit 0 enables subdivision and skips alternate-pass groups.
  std::int32_t mode = 0;
  // The first-person facing mask (Render_FrameScratch.firstPersonViewFacing).
  std::uint32_t view_facing = 0;
};

// BATTLE.PRG func_8009723C (2EA3C.s:10): every visible group through its
// stored view.
RoomDrawStatus DrawRoomGeometry(std::span<const std::byte> geometry,
                                const RoomDrawPass& pass,
                                const RoomClipState& clip,
                                RoomTransformer& transformer,
                                RoomPacketQueue& queue);

// BATTLE.PRG func_80098160: the near depth, as a signed halfword.
void SetRoomPolygonNearClip(RoomClipState& clip, std::int32_t depth);
// BATTLE.PRG func_8009817C (2EA3C.s:1078): the far depth, as a signed
// halfword.
void SetRoomPolygonFarClip(RoomClipState& clip, std::int32_t depth);
// BATTLE.PRG func_80098194: the depth past which packets use the
// half-resolution page, sign-extended from a halfword.
void SetRoomTextureLodDepth(RoomClipState& clip, std::int32_t depth);
// BATTLE.PRG func_800981B8 (2EA3C.s:1101): each edge as a signed halfword.
void SetRoomPolygonScreenBounds(RoomClipState& clip,
                                const RoomScreenBounds& bounds);

// BATTLE.PRG func_8009820C: the 34-vertex, 64-face Gouraud sky behind the
// room, rotated without perspective and centred on the 320x240 frame.
RoomDrawStatus DrawSkyDome(std::span<const std::byte> sky,
                           RoomTransformer& transformer,
                           RoomPacketQueue& queue);

// The tint parameters (D_800F1BB0_t): signed channel offsets and the Q12
// strength of the fade toward grey.
struct RoomTint {
  std::int8_t red = 0;
  std::int8_t green = 0;
  std::int8_t blue = 0;
  std::int16_t strength = 0;
};

// The fade strength and the channel offsets (already scaled by four) one
// colour is tinted with.
struct RoomColorAdjustment {
  std::int16_t strength = 0;
  std::int16_t red = 0;
  std::int16_t green = 0;
  std::int16_t blue = 0;
};

// BATTLE.PRG func_80098E50 (2EA3C.s:2018): fades a 0xBBGGRR colour toward
// its 5:9:2 luminance by `strength` / 4096, then adds the offsets, each
// channel clamped to 0..255. The high byte passes through.
[[nodiscard]] std::uint32_t TintRoomPrimitiveColor(
    std::uint32_t color, const RoomColorAdjustment& adjustment);

enum class RoomTintStatus : std::uint8_t {
  kTinted,
  // A packet that is not a room triangle or quad ended the walk.
  kStoppedAtForeignPacket,
  // A packet ran past the span; nothing was tinted.
  kTruncated,
};

// BATTLE.PRG func_80098D6C (2EA3C.s:1944): tints every vertex colour of the
// room packets in `packets`. Flag 2 means each packet carries two trailing
// words.
RoomTintStatus TintRoomGeometryPrimitives(std::span<std::uint32_t> packets,
                                          const RoomTint& tint,
                                          std::uint32_t flags);

// BATTLE.PRG vs_battle_getGeomOffset (2EA3C.s:2470): each offset as an
// unsigned halfword.
[[nodiscard]] RoomScreenOffset GetProjectionScreenOffset(
    const RoomTransformer& transformer);
// BATTLE.PRG vs_battle_setGeomOffset (2EA3C.s:2482).
void SetProjectionScreenOffset(RoomTransformer& transformer,
                               const RoomScreenOffset& offset);
}  // namespace VS::Render
