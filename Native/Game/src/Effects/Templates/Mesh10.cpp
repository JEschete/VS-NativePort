// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/Mesh10.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/Templates/RingCore.hpp"
#include "Effects/Templates/SurfaceCore.hpp"

namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;
using math::Vec3;

// The semi-transparent Gouraud-shaded triangle code each colour word carries.
constexpr std::uint8_t kMesh10Code = 0x36;
constexpr unsigned kLengthShift = 24;
// The tag's length byte: the packet's words after the tag.
constexpr std::uint32_t kMesh10BodyWords{kMesh10PacketWords - 1};
constexpr std::uint32_t kMesh10PacketLength = kMesh10BodyWords << kLengthShift;
constexpr std::size_t kFirstCornerWord = 2;
constexpr std::size_t kCornerWords = 3;
constexpr std::size_t kRestoreWord = kMesh10PacketWords - 1;
constexpr std::int32_t kFaceDepthDivisor = 3;
constexpr std::int32_t kQuarters = 4;
constexpr std::int32_t kThreeQuarters = 3;

// What a mesh tick computes before its vertices.
struct Mesh10Frame {
  SurfaceTickInputs inputs{};
  Mesh10State state{};
  SurfaceScratch scratch{};
  std::array<RingColor, kMesh10Colors> colors{};
};

// What every face of one tick reads.
struct Mesh10Faces {
  std::array<RingProjection, kMesh10Vertices> projected{};
  std::array<std::uint16_t, kMesh10Vertices> vertex_colors{};
  std::array<RingColor, kMesh10Colors> colors{};
  std::array<std::uint8_t, kMesh10UvBytes> uv_bytes{};
  RingTexture texture{};
};

// A triangle and its ordering depth.
struct DepthTriangle {
  Mesh10Triangle triangle{};
  std::int32_t depth{};
};

template <typename Curve>
[[nodiscard]] RingColor CurveColor(
    const std::array<Curve, kColorChannels>& curves, std::int32_t age,
    SurfaceHost& host) {
  return {.r = DoubledCurveChannel(curves[0], age, host),
          .g = DoubledCurveChannel(curves[1], age, host),
          .b = DoubledCurveChannel(curves[2], age, host)};
}

[[nodiscard]] std::int16_t SaturateHalfword(std::int32_t value) noexcept {
  return static_cast<std::int16_t>(
      std::clamp<std::int32_t>(value, std::numeric_limits<std::int16_t>::min(),
                               std::numeric_limits<std::int16_t>::max()));
}

// decomp: meshes10.h:63-144 (the sampling and the two colours).
std::optional<Mesh10Frame> BeginMesh10Tick(Mesh10Host& host) {
  std::optional<Mesh10Frame> opened = OpenSurfaceTick<Mesh10Frame>(host);
  if (!opened.has_value()) {
    return std::nullopt;
  }
  Mesh10Frame& frame = *opened;
  const SurfaceDefinition& definition = frame.inputs.definition;
  const SurfaceCurves& curves = definition.curves;
  const std::int32_t age = frame.state.age;
  frame.scratch.attraction_jitter = SurfaceHalfwords(SampleSurfaceRange(
      definition.attraction_jitter, curves.attraction_jitter, age, host));
  frame.colors = {CurveColor(curves.color, age, host),
                  CurveColor(curves.second_color, age, host)};
  return opened;
}

// decomp: meshes10.h:145-182: the model scaled (the first four vertices by
// the extent, the rest by the attraction jitter), turned by the accumulated
// rotation and moved to the position; each component saturates to a
// halfword as the GTE's IR registers hold it.
std::array<RingPoint, kMesh10Vertices> PlaceMesh10(Mesh10Frame& frame,
                                                   const Mesh10Model& model,
                                                   SurfaceHost& host) {
  const SurfaceDefinition& definition = frame.inputs.definition;
  const SurfaceCurves& curves = definition.curves;
  const std::int32_t age = frame.state.age;
  SurfaceScratch& scratch = frame.scratch;
  const Vec3 jitter_scale = scratch.attraction_jitter.value_or(Vec3{});
  std::array<RingPoint, kMesh10Vertices> scaled{};
  std::size_t next = 0;
  for (const Mesh10Vertex& vertex : model.vertices) {
    const Vec3 scale =
        next < kMesh10ExtentVertices ? scratch.extent : jitter_scale;
    scaled.at(next++) = RingPointAt({RingFixed(vertex.point.x, scale.x),
                                     RingFixed(vertex.point.y, scale.y),
                                     RingFixed(vertex.point.z, scale.z)});
  }
  const Vec3 base =
      SampleSurfaceRange(definition.direction, curves.direction, age, host);
  const Vec3 spin = SampleSurfaceRange(definition.direction_jitter,
                                       curves.direction_jitter, age, host);
  scratch.base_direction = base;
  scratch.direction_jitter = spin;
  RingPoint& rotation = frame.state.rotation;
  rotation = RingPointAt({math::WrappingAdd(rotation.x, spin.x),
                          math::WrappingAdd(rotation.y, spin.y),
                          math::WrappingAdd(rotation.z, spin.z)});
  const RingPoint angles = RingPointAt({math::WrappingAdd(rotation.x, base.x),
                                        math::WrappingAdd(rotation.y, base.y),
                                        math::WrappingAdd(rotation.z, base.z)});
  scratch.aim_angles = angles;
  const math::Transform3 aim{
      .rotation = math::BuildRotationXyz({angles.x, angles.y, angles.z}),
      .translation = scratch.position};
  scratch.aim = aim;
  std::array<RingPoint, kMesh10Vertices> placed{};
  next = 0;
  for (const RingPoint& point : scaled) {
    const Vec3 moved = math::RotateTranslate(aim, {point.x, point.y, point.z});
    placed.at(next++) = {SaturateHalfword(moved.x), SaturateHalfword(moved.y),
                         SaturateHalfword(moved.z)};
  }
  return placed;
}

// decomp: meshes10.h:206-216: the triangle of `face` and its depth; nothing
// for a face whose vertex, colour or texture offset falls outside the
// tables (the original read past them).
std::optional<DepthTriangle> FaceTriangle(const Mesh10Face& face,
                                          const Mesh10Faces& faces) {
  DepthTriangle result{
      .triangle = {.clut = faces.texture.clut, .tpage = faces.texture.tpage}};
  std::int32_t total = 0;
  for (std::size_t corner = 0; corner < kMesh10Corners; ++corner) {
    const std::size_t vertex = face.vertices.at(corner);
    const std::size_t uv = face.uvs.at(corner);
    if (vertex >= kMesh10Vertices || uv + 1 >= kMesh10UvBytes) {
      return std::nullopt;
    }
    const std::size_t color = faces.vertex_colors.at(vertex);
    if (color >= kMesh10Colors) {
      return std::nullopt;
    }
    const RingProjection& at = faces.projected.at(vertex);
    total += at.depth;
    result.triangle.screens.at(corner) = at.screen;
    result.triangle.colors.at(corner) = faces.colors.at(color);
    result.triangle.uvs.at(corner) =
        RingUv{faces.uv_bytes.at(uv), faces.uv_bytes.at(uv + 1)};
  }
  result.depth = total / kFaceDepthDivisor;
  return result;
}

// decomp: meshes10.h:183-237: projection, the texture coordinates and the
// sixteen triangles.
void DrawMesh10(const Mesh10Frame& frame,
                const std::array<RingPoint, kMesh10Vertices>& placed,
                const Mesh10Model& model, SurfaceHost& host) {
  host.BeginProjection((frame.inputs.definition.flags & kRingEffectView) != 0);
  Mesh10Faces faces{.colors = frame.colors};
  std::size_t next = 0;
  for (const RingPoint& point : placed) {
    faces.projected.at(next++) = host.Project(point);
  }
  next = 0;
  for (const Mesh10Vertex& vertex : model.vertices) {
    faces.vertex_colors.at(next++) = vertex.color;
  }
  const std::optional<RingTexture> texture = host.AdvanceTexture();
  if (!texture.has_value()) {
    return;
  }
  faces.texture = *texture;
  faces.uv_bytes = Mesh10UvBytes(*texture);
  for (const Mesh10Face& face : model.faces) {
    const std::optional<DepthTriangle> triangle = FaceTriangle(face, faces);
    if (triangle.has_value() &&
        SurfaceDepthVisible(triangle->depth, frame.inputs.near_clip)) {
      host.Submit(EncodeMesh10Packet(triangle->triangle), triangle->depth);
    }
  }
}
}  // namespace

// decomp: meshes10.h:197-205.
std::array<std::uint8_t, kMesh10UvBytes> Mesh10UvBytes(
    const RingTexture& texture) noexcept {
  const auto byte = [](std::int32_t value) {
    return static_cast<std::uint8_t>(value);
  };
  const std::int32_t u = texture.u;
  const std::int32_t v = texture.v;
  const std::int32_t w = texture.w;
  const std::int32_t h = texture.h;
  const std::uint8_t centre = byte(u + (w / 2));
  const std::uint8_t middle = byte(v + (h / 2));
  const std::uint8_t bottom = byte(v + h);
  return {centre,
          byte(v),
          byte(u + (w / kQuarters)),
          middle,
          byte(u + ((w * kThreeQuarters) / kQuarters)),
          middle,
          byte(u),
          bottom,
          centre,
          bottom,
          byte(u + w),
          bottom};
}

std::array<std::uint32_t, kMesh10PacketWords> EncodeMesh10Packet(
    const Mesh10Triangle& triangle) noexcept {
  std::array<std::uint32_t, kMesh10PacketWords> words{};
  words[0] = kMesh10PacketLength;
  words[1] = kRingDrawModeWord;
  // The first two texture words carry the CLUT and the texture page.
  const std::array<std::uint16_t, kMesh10Corners> high = {triangle.clut,
                                                          triangle.tpage, 0};
  for (std::size_t corner = 0; corner < kMesh10Corners; ++corner) {
    const std::size_t at = kFirstCornerWord + (corner * kCornerWords);
    words.at(at) = SurfaceColorWord(triangle.colors.at(corner), kMesh10Code);
    words.at(at + 1) = triangle.screens.at(corner);
    words.at(at + 2) = SurfaceUvWord(triangle.uvs.at(corner), high.at(corner));
  }
  words[kRestoreWord] = kRingRestoreModeWord;
  return words;
}

// decomp: EFFECT VS_MESH10_FUNCTION (src/EFFECT/meshes10.h:29).
bool Mesh10(SurfaceCommand command, std::int32_t packed, Mesh10Model model,
            Mesh10Host& host) {
  const auto create = [&host](RingPlan plan,
                              const SurfaceDefinition& definition) {
    host.Create(Mesh10State{.lifetime = plan.lifetime,
                            .definition_index = plan.definition_index},
                SurfaceAnimation{.table = definition.texture_index,
                                 .program = definition.animation_id});
  };
  const auto tick = [model, &host] {
    std::optional<Mesh10Frame> frame = BeginMesh10Tick(host);
    if (!frame.has_value()) {
      return false;
    }
    const std::array<RingPoint, kMesh10Vertices> placed =
        PlaceMesh10(*frame, model, host);
    DrawMesh10(*frame, placed, model, host);
    host.PublishScratch(frame->scratch);
    // decomp: meshes10.h:238-255 (the tick event).
    EmitSurfaceTickEvent(frame->inputs.definition, frame->scratch.position,
                         host);
    return FinishSurfaceTick(frame->state, host);
  };
  return RunSurfaceCommand(command, packed, host, create, tick);
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG069.BIN func_800F98E0 (src/EFFECT/PLG069.BIN/E0.c:2).
bool Plg069::Mesh10(Templates::SurfaceCommand command, std::int32_t packed,
                    Templates::Mesh10Model model, Templates::Mesh10Host& host) {
  return Templates::Mesh10(command, packed, model, host);
}

// decomp: PLG077.BIN func_800F9818 (src/EFFECT/PLG077.BIN/18.c:8).
bool Plg077::Mesh10(Templates::SurfaceCommand command, std::int32_t packed,
                    Templates::Mesh10Model model, Templates::Mesh10Host& host) {
  return Templates::Mesh10(command, packed, model, host);
}

// decomp: PLG129.BIN func_800FA424 (src/EFFECT/PLG129.BIN/0.c:7).
bool Plg129::Mesh10(Templates::SurfaceCommand command, std::int32_t packed,
                    Templates::Mesh10Model model, Templates::Mesh10Host& host) {
  return Templates::Mesh10(command, packed, model, host);
}

// decomp: PLG137.BIN func_800F98F8 (src/EFFECT/PLG137.BIN/F8.c:2).
bool Plg137::Mesh10(Templates::SurfaceCommand command, std::int32_t packed,
                    Templates::Mesh10Model model, Templates::Mesh10Host& host) {
  return Templates::Mesh10(command, packed, model, host);
}

// decomp: PLG153.BIN func_800F9910 (src/EFFECT/PLG153.BIN/110.c:2).
bool Plg153::Mesh10(Templates::SurfaceCommand command, std::int32_t packed,
                    Templates::Mesh10Model model, Templates::Mesh10Host& host) {
  return Templates::Mesh10(command, packed, model, host);
}

// decomp: PLG154.BIN func_800F9800 (src/EFFECT/PLG154.BIN/0.c:8).
bool Plg154::Mesh10(Templates::SurfaceCommand command, std::int32_t packed,
                    Templates::Mesh10Model model, Templates::Mesh10Host& host) {
  return Templates::Mesh10(command, packed, model, host);
}

// decomp: PLG260.BIN func_800F9800 (src/EFFECT/PLG260.BIN/0.c:5).
bool Plg260::Mesh10(Templates::SurfaceCommand command, std::int32_t packed,
                    Templates::Mesh10Model model, Templates::Mesh10Host& host) {
  return Templates::Mesh10(command, packed, model, host);
}
}  // namespace VS::Effects
