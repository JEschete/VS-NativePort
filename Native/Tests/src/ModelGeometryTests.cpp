#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Render/ModelGeometry.hpp"
#include "Render/ModelGeometryLive.hpp"

extern "C" {
#include "Actors/ModelRendering.h"
#include "Actors/WeaponModel.h"
#include "Field/Gimmicks.h"
}

namespace {
namespace Render = VS::Render;

constexpr std::size_t kGroupsAt = 0x50;
constexpr std::size_t kVerticesAt = 0x54;
constexpr std::size_t kPolygonsAt = 0x6C;
constexpr std::size_t kModelBytes = kPolygonsAt + 16;
constexpr std::int32_t kDepth = 100;

template <typename T>
void Put(std::vector<std::byte>& bytes, std::size_t offset, T value) {
  std::memcpy(&bytes.at(offset), &value, sizeof value);
}

// One bone, one vertex group of three vertices and one flat triangle that
// faces the camera inside the viewport.
std::vector<std::byte> FlatTriangleModel(std::uint16_t group_bone = 0) {
  std::vector<std::byte> model(kModelBytes);
  Put<std::uint32_t>(model, 0x34, kGroupsAt);
  Put<std::uint32_t>(model, 0x38, kVerticesAt);
  Put<std::uint32_t>(model, 0x3C, kPolygonsAt);
  Put<std::uint16_t>(model, kGroupsAt, group_bone);
  Put<std::uint16_t>(model, kGroupsAt + 2, 3);
  const std::array<std::array<std::int16_t, 2>, 3> points{
      {{10, 10}, {50, 10}, {10, 50}}};
  for (std::size_t vertex = 0; vertex < points.size(); ++vertex) {
    const std::size_t at = kVerticesAt + vertex * 8;
    Put<std::int16_t>(model, at, points.at(vertex)[0]);
    Put<std::int16_t>(model, at + 2, points.at(vertex)[1]);
    Put<std::int16_t>(model, at + 4, static_cast<std::int16_t>(kDepth));
  }
  model.at(kPolygonsAt) = std::byte{0x24};
  model.at(kPolygonsAt + 1) = std::byte{16};
  Put<std::uint16_t>(model, kPolygonsAt + 4, 0);
  Put<std::uint16_t>(model, kPolygonsAt + 6, 4);
  Put<std::uint16_t>(model, kPolygonsAt + 8, 8);
  return model;
}

using Counts = Render::ModelCounts;
constexpr Counts kTriangleCounts{.bones = 1, .groups = 1, .triangles = 1};

std::int32_t Pack(std::int32_t x, std::int32_t y) {
  return static_cast<std::int32_t>((static_cast<std::uint32_t>(y) << 16) |
                                   (static_cast<std::uint32_t>(x) & 0xFFFFU));
}

// Projects x/y straight to the screen and z to the depth.
class FakeTransformer final : public Render::ModelTransformer {
 public:
  void LoadBone(std::size_t bone) override { bones.push_back(bone); }
  void LoadTransform(const Render::ModelTransform& /*transform*/) override {
    ++transforms;
  }
  VS::Core::Math::Vec3 Transform(const Render::ModelVertex& vertex) override {
    return {vertex.x, vertex.y, vertex.z};
  }
  Render::ProjectedPoint Project(const Render::ModelVertex& vertex) override {
    return {Pack(vertex.x, vertex.y), vertex_depth};
  }

  std::vector<std::size_t> bones;
  int transforms = 0;
  std::int32_t vertex_depth = kDepth;
};

class FakeQueue final : public Render::ModelPacketQueue {
 public:
  bool Emit(std::span<const std::uint32_t> packet,
            std::uint32_t depth) override {
    if (!accept) {
      return false;
    }
    packets.emplace_back(packet.begin(), packet.end());
    depths.push_back(depth);
    return true;
  }

  bool accept = true;
  std::vector<std::vector<std::uint32_t>> packets;
  std::vector<std::uint32_t> depths;
};

class FlatGround final : public Render::ShadowGround {
 public:
  std::int32_t SupportHeight(std::int32_t /*dx*/,
                             std::int32_t /*dz*/) override {
    return 0;
  }
  [[nodiscard]] bool OnPlatform() const override { return false; }
};

struct Projected {
  std::array<std::int32_t, Render::kModelVertexCapacity> screens{};
  std::array<std::int32_t, Render::kModelVertexCapacity> depths{};
  Render::ModelProjectionResult result;

  [[nodiscard]] Render::ModelProjectionView View() const {
    return {std::span(screens).first(result.vertex_count),
            std::span(depths).first(result.vertex_count)};
  }
};

Projected ProjectTriangle(const std::vector<std::byte>& model,
                          std::size_t matrix_count,
                          FakeTransformer& transformer) {
  Projected projected;
  projected.result = Render::ProjectModelVertices(
      kTriangleCounts, model, matrix_count, transformer, projected.screens,
      projected.depths);
  return projected;
}
}  // namespace

TEST_CASE("Model projection loads each group bone and clamps depths",
          "[render][model-geometry]") {
  const auto model = FlatTriangleModel();
  FakeTransformer transformer;
  const Projected projected = ProjectTriangle(model, 1, transformer);
  CHECK(projected.result.vertex_count == 3);
  CHECK(projected.result.combined_depth == kDepth);
  CHECK(transformer.bones == std::vector<std::size_t>{0});
  CHECK(projected.screens[1] == Pack(50, 10));

  FakeTransformer far;
  far.vertex_depth = 0x12345;
  CHECK(ProjectTriangle(model, 1, far).depths[0] == 0xFFFF);
}

TEST_CASE("Model projection refuses bones beyond the caller's matrices",
          "[render][model-geometry]") {
  FakeTransformer none;
  CHECK(ProjectTriangle(FlatTriangleModel(), 0, none).result.vertex_count == 0);
  CHECK(none.bones.empty());

  // The header claims two bones but the group names one past the matrices.
  FakeTransformer one;
  Projected projected;
  auto model = FlatTriangleModel(1);
  Put<std::uint32_t>(model, 0x34, kGroupsAt);
  projected.result = Render::ProjectModelVertices(
      {.bones = 2, .groups = 1, .triangles = 1}, model, 1, one,
      projected.screens, projected.depths);
  CHECK(projected.result.vertex_count == 0);
  CHECK(one.bones.empty());
}

TEST_CASE("Model projection and validation refuse truncated sections",
          "[render][model-geometry]") {
  const auto model = FlatTriangleModel();
  REQUIRE(Render::ValidateModelData(kTriangleCounts, false, model));

  FakeTransformer transformer;
  Projected projected;
  const std::span<const std::byte> short_model =
      std::span(model).first(kPolygonsAt - 4);
  projected.result =
      Render::ProjectModelVertices(kTriangleCounts, short_model, 1, transformer,
                                   projected.screens, projected.depths);
  CHECK(projected.result.vertex_count == 0);
  CHECK(transformer.bones.empty());

  CHECK_FALSE(Render::ValidateModelData(
      kTriangleCounts, false, std::span(model).first(kModelBytes - 4)));
  CHECK_FALSE(Render::ValidateModelData(kTriangleCounts, true, model));
  CHECK_FALSE(Render::ValidateModelData(
      {.bones = 1, .groups = 1, .triangles = 2}, false, model));

  // A vertex group that ends past the vertex section is refused.
  auto overlong = model;
  Put<std::uint16_t>(overlong, kGroupsAt + 2, 4);
  CHECK(ProjectTriangle(overlong, 1, transformer).result.vertex_count == 0);
}

TEST_CASE("Model flat polygons queue one packet at the averaged depth",
          "[render][model-geometry]") {
  const auto model = FlatTriangleModel();
  FakeTransformer transformer;
  const Projected projected = ProjectTriangle(model, 1, transformer);
  FakeQueue queue;
  const Render::ModelSurface surface{.palette = 0x1234, .texture_page = 0x56};
  REQUIRE(
      Render::DrawModelFlatPolygons({kTriangleCounts, model, projected.View()},
                                    Render::kNeutralModelTint, surface,
                                    queue) == Render::ModelDrawStatus::kDrawn);
  REQUIRE(queue.packets.size() == 1);
  // (3 * 100 * 341) >> 12, less the normal group's one-step bias.
  CHECK(queue.depths[0] == 23);
  CHECK(queue.packets[0].size() <= Render::kModelPacketWords);

  FakeQueue full;
  full.accept = false;
  CHECK(Render::DrawModelFlatPolygons(
            {kTriangleCounts, model, projected.View()},
            Render::kNeutralModelTint, surface,
            full) == Render::ModelDrawStatus::kBufferFull);
}

TEST_CASE("Model polygons refuse invalid data instead of reading past it",
          "[render][model-geometry]") {
  auto model = FlatTriangleModel();
  FakeTransformer transformer;
  const Projected projected = ProjectTriangle(model, 1, transformer);
  const Render::ModelSurface surface;
  FakeQueue queue;
  const auto draw = [&](const std::vector<std::byte>& data,
                        Render::ModelProjectionView view,
                        Render::ModelCounts counts = kTriangleCounts) {
    return Render::DrawModelFlatPolygons(
        {counts, data, view}, Render::kNeutralModelTint, surface, queue);
  };

  CHECK(draw(model, {}) == Render::ModelDrawStatus::kNoVertices);
  CHECK(draw(model, projected.View(),
             {.bones = 1, .groups = 1, .triangles = 2}) ==
        Render::ModelDrawStatus::kTruncatedModel);

  auto wrong_command = model;
  wrong_command.at(kPolygonsAt) = std::byte{0x2C};
  CHECK(draw(wrong_command, projected.View()) ==
        Render::ModelDrawStatus::kInvalidPolygon);

  auto far_index = model;
  Put<std::uint16_t>(far_index, kPolygonsAt + 8, 12);
  CHECK(draw(far_index, projected.View()) ==
        Render::ModelDrawStatus::kInvalidVertexIndex);
  auto odd_index = model;
  Put<std::uint16_t>(odd_index, kPolygonsAt + 8, 2);
  CHECK(draw(odd_index, projected.View()) ==
        Render::ModelDrawStatus::kInvalidVertexIndex);

  // Pulsed limb palettes need limb ranges that fit the polygon counts.
  auto ranges = model;
  Put<std::uint16_t>(ranges, 0, 2);
  CHECK(Render::DrawModelFlatPolygonsWithOverrides(
            {kTriangleCounts, ranges, projected.View()},
            Render::kNeutralModelTint, surface, {.pulse_mask = 0x80},
            queue) == Render::ModelDrawStatus::kInvalidLimbRanges);
  // Only the truncated model's first, complete triangle was queued.
  CHECK(queue.packets.size() == 1);
}

namespace {
struct ShadowFixture {
  ShadowFixture() {
    vertices[0] = static_cast<std::uint32_t>(Pack(10, 10));
    vertices[1] = static_cast<std::uint32_t>(Pack(10, 50));
    vertices[2] = static_cast<std::uint32_t>(Pack(50, 10));
    faces.fill(0x080400);
  }

  Render::ActorShadowStatus Draw(
      Render::ModelPacketQueue& queue,
      std::span<const std::uint32_t> mesh_faces,
      std::size_t vertex_count = Render::kShadowVertexCount) {
    const Render::ActorShadowResult result = Render::DrawActorGroundShadow(
        actor, {}, {}, {std::span(vertices).first(vertex_count), mesh_faces},
        ground, transformer, queue);
    anchor = result.anchor;
    return result.status;
  }

  std::array<std::uint32_t, Render::kShadowVertexCount> vertices{};
  std::array<std::uint32_t, Render::kShadowFaceCount> faces{};
  Render::ActorShadowSource actor{
      .radius = 64, .scale_x = 0x1000, .scale_y = 0x1000, .scale_z = 0x1000};
  FlatGround ground;
  FakeTransformer transformer;
  std::array<std::int16_t, 3> anchor{};
};
}  // namespace

TEST_CASE("Actor ground shadow queues every face and reports the anchor",
          "[render][model-geometry][shadow]") {
  ShadowFixture shadow;
  FakeQueue queue;
  CHECK(shadow.Draw(queue, shadow.faces) == Render::ActorShadowStatus::kDrawn);
  // The fake projection drops the shadow's zero height into screen y.
  CHECK(shadow.anchor == std::array<std::int16_t, 3>{10, 0, kDepth});
  CHECK(queue.packets.size() == Render::kShadowFaceCount);
  // The camera places the anchor, then the shadow transform projects.
  CHECK(shadow.transformer.transforms == 2);

  FakeQueue full;
  full.accept = false;
  CHECK(shadow.Draw(full, shadow.faces) ==
        Render::ActorShadowStatus::kBufferFull);
}

TEST_CASE("Actor ground shadow refuses meshes it would read past",
          "[render][model-geometry][shadow]") {
  ShadowFixture shadow;
  FakeQueue untouched;
  auto bad_faces = shadow.faces;
  bad_faces[5] = 0x480400;  // Corner 2 names vertex 18 of 18.
  CHECK(shadow.Draw(untouched, bad_faces) ==
        Render::ActorShadowStatus::kInvalidMesh);
  bad_faces[5] = 0x080402;  // Corner 0 is not word aligned.
  CHECK(shadow.Draw(untouched, bad_faces) ==
        Render::ActorShadowStatus::kInvalidMesh);
  CHECK(shadow.Draw(untouched, shadow.faces, Render::kShadowVertexCount - 1) ==
        Render::ActorShadowStatus::kInvalidMesh);
  CHECK(untouched.packets.empty());
}

TEST_CASE("Live model extents follow loads, copies and releases",
          "[render][model-geometry][live]") {
  std::array<std::byte, 0x100> loaded{};
  std::array<std::byte, 0x40> copy{};
  CHECK(Render::LiveModelBytes(loaded.data()).empty());

  Render::RecordLiveModelExtent(loaded.data(), 0x80);
  CHECK(Render::LiveModelBytes(loaded.data()).size() == 0x80);
  // Only the exact start of a recorded model resolves.
  CHECK(Render::LiveModelBytes(loaded.data() + 4).empty());

  Render::RecordLiveModelCopy(copy.data(), loaded.data(), copy.size());
  CHECK(Render::LiveModelBytes(copy.data()).size() == copy.size());

  // A new extent that overlaps an old one replaces it.
  Render::RecordLiveModelExtent(loaded.data() + 0x40, 0x40);
  CHECK(Render::LiveModelBytes(loaded.data()).empty());
  CHECK(Render::LiveModelBytes(loaded.data() + 0x40).size() == 0x40);

  Render::ForgetLiveModelExtent(loaded.data() + 0x40);
  CHECK(Render::LiveModelBytes(loaded.data() + 0x40).empty());
  // A copy of an unrecorded model loses its own extent.
  Render::RecordLiveModelCopy(copy.data(), loaded.data(), copy.size());
  CHECK(Render::LiveModelBytes(copy.data()).empty());
}

TEST_CASE("Live gimmick and weapon projection refuse unrecorded models",
          "[render][model-geometry][live]") {
  auto model = FlatTriangleModel();
  Field_GimmickInstance object{};
  std::memcpy(&object, &kTriangleCounts, sizeof kTriangleCounts);
  object.model = reinterpret_cast<Actors_ShpModel*>(model.data());
  Actors_modelRenderContext.projectedVertexCount = 7;
  CHECK_FALSE(Render::ProjectLiveGimmickModel(object));
  CHECK(Actors_modelRenderContext.projectedVertexCount == 0);

  Actors_WeaponModel weapon{};
  std::memcpy(&weapon, &kTriangleCounts, sizeof kTriangleCounts);
  weapon.sectionOffsets =
      reinterpret_cast<Actors_WeaponModelData*>(model.data());
  Actors_modelRenderContext.projectedVertexCount = 7;
  CHECK_FALSE(Render::ProjectLiveWeaponModel(weapon));
  CHECK(Actors_modelRenderContext.projectedVertexCount == 0);
}
