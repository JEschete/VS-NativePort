#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/SphereContainment.hpp"

namespace {
namespace b = VS::Battle;
namespace math = VS::Core::Math;

b::SphereVolumeGeometry Geometry(b::TargetVolumeExtents extents) {
  return {.extents = extents};
}

class Query final : public b::SphereContainmentQuery {
 public:
  std::uint8_t kind = 1;
  b::TargetVolumeExtents extents{11, 200, 33, 250};
  b::TargetVolumeExtents observed{};
  int result = -91;
  int selected = 0;
  int mode = -1;
  int calls = 0;
  int stores = 0;

  std::uint8_t Kind() const override { return kind; }
  b::TargetVolumeExtents Extents() const override { return extents; }
  void SetExtents(const b::TargetVolumeExtents& value) override {
    extents = value;
    ++stores;
  }
  int Ellipsoid() override { return Selected(1, 0); }
  int Cylinder(int shift_height_origin) override {
    return Selected(2, shift_height_origin);
  }
  int Cone(int shift_height_origin) override {
    return Selected(3, shift_height_origin);
  }
  int Prism(int rectangular) override { return Selected(4, rectangular); }

 private:
  int Selected(int shape, int origin_mode) {
    ++calls;
    selected = shape;
    mode = origin_mode;
    observed = extents;
    extents = {99, 98, 97, 96};
    return result;
  }
};
}  // namespace

TEST_CASE("F01290 retains zero one and maximum unsigned axis reciprocals",
          "[sphere-containment][F01290]") {
  auto volume = Geometry({0, 1, 255, 0});
  volume.origin = {-32768, 32767, -1};
  const auto transform = b::BuildTargetVolumeTestTransform(volume);
  CHECK(transform.origin == volume.origin);
  CHECK(transform.reciprocal_extents ==
        std::array<std::int16_t, 3>{-1, -32768, 128});
  CHECK(transform.rotation.m[0] == std::array<std::int16_t, 3>{4096, 0, 0});
  CHECK(transform.rotation.m[1] == std::array<std::int16_t, 3>{0, 4096, 0});
}

TEST_CASE("F01290 uses byte tilt and negated halfword yaw in XYZ order",
          "[sphere-containment][F01290]") {
  auto volume = Geometry({8, 8, 8, 64});
  const auto tilted = b::BuildTargetVolumeTestTransform(volume);
  CHECK(tilted.rotation.m[1] == std::array<std::int16_t, 3>{0, 0, -4096});
  CHECK(tilted.rotation.m[2] == std::array<std::int16_t, 3>{0, 4096, 0});
  volume.extents[3] = 0;
  volume.yaw = 1024;
  const auto turned = b::BuildTargetVolumeTestTransform(volume);
  CHECK(turned.LocalPoint({17, 0, 31}) == math::Position3{-31, 0, 17});
  volume.yaw = std::numeric_limits<std::int16_t>::min();
  CHECK(b::BuildTargetVolumeTestTransform(volume).LocalPoint({17, 0, 31}) ==
        math::Position3{17, 0, 31});
}

TEST_CASE("F01291 local points wrap deltas then saturate rotated halfwords",
          "[sphere-containment][F01291]") {
  auto volume = Geometry({255, 255, 255, 0});
  volume.origin = {-32768, 32767, 0};
  CHECK(b::BuildTargetVolumeTestTransform(volume).LocalPoint(
            {32767, -32768, 0}) == math::Position3{-1, 1, 0});
  volume.origin = {};
  volume.yaw = -512;
  const auto transform = b::BuildTargetVolumeTestTransform(volume);
  CHECK(transform.LocalPoint({32767, 0, 32767}).x == 32767);
  CHECK(transform.LocalPoint({-32768, 0, -32768}).x == -32768);
}

TEST_CASE("F01291 ellipsoid uses all axes and excludes the exact radius",
          "[sphere-containment][F01291]") {
  const auto volume = Geometry({8, 8, 8, 0});
  CHECK(b::IsPointInTargetEllipsoid(volume, {255, 0, 0}) == 1);
  CHECK(b::IsPointInTargetEllipsoid(volume, {256, 0, 0}) == 0);
  CHECK(b::IsPointInTargetEllipsoid(volume, {0, 256, 0}) == 0);
  CHECK(b::IsPointInTargetEllipsoid(volume, {0, 0, -256}) == 0);
  CHECK(b::IsPointInTargetEllipsoid(volume, {128, 128, 128}) == 1);
}

TEST_CASE("F01291 defines zero extents and unsigned wrapped squared sums",
          "[sphere-containment][F01291]") {
  CHECK(b::IsPointInTargetEllipsoid(Geometry({0, 0, 0, 0}),
                                    {32767, -32768, 32767}) == 1);
  const auto volume = Geometry({1, 1, 1, 0});
  CHECK(b::IsPointInTargetEllipsoid(volume, {31, 0, 0}) == 1);
  CHECK(b::IsPointInTargetEllipsoid(volume, {32, 0, 0}) == 0);
  CHECK(b::IsPointInTargetEllipsoid(volume, {8192, 0, 0}) == 1);
  CHECK(b::IsPointInTargetEllipsoid(volume, {5000, 5000, 0}) == 0);
}

TEST_CASE(
    "F01292 cylinder height includes both ends and uses unsigned negation",
    "[sphere-containment][F01292]") {
  const auto volume = Geometry({8, 4, 8, 0});
  CHECK(b::IsPointInTargetCylinder(volume, {0, 0, 0}, 0) == 1);
  CHECK(b::IsPointInTargetCylinder(volume, {0, -128, 0}, 0) == 1);
  CHECK(b::IsPointInTargetCylinder(volume, {0, -129, 0}, 0) == 0);
  CHECK(b::IsPointInTargetCylinder(volume, {0, 1, 0}, 0) == 0);
  CHECK(b::IsPointInTargetCylinder(volume, {256, 0, 0}, 0) == 0);
}

TEST_CASE("F01292 every nonzero mode shifts height before the gate",
          "[sphere-containment][F01292]") {
  const int mode = GENERATE(1, -1, std::numeric_limits<int>::min());
  const auto volume = Geometry({8, 4, 8, 0});
  CHECK(b::IsPointInTargetCylinder(volume, {0, 64, 0}, mode) == 1);
  CHECK(b::IsPointInTargetCylinder(volume, {0, -64, 0}, mode) == 1);
  CHECK(b::IsPointInTargetCylinder(volume, {0, 65, 0}, mode) == 0);
}

TEST_CASE("F01292 zero height and maximum extents remain defined",
          "[sphere-containment][F01292]") {
  CHECK(b::IsPointInTargetCylinder(Geometry({0, 0, 0, 0}), {32767, 0, -32768},
                                   0) == 1);
  CHECK(b::IsPointInTargetCylinder(Geometry({1, 0, 1, 0}), {5000, 0, 5000},
                                   0) == 0);
  const auto maximum = Geometry({255, 255, 255, 0});
  CHECK(b::IsPointInTargetCylinder(maximum, {0, -8160, 0}, 0) == 1);
  CHECK(b::IsPointInTargetCylinder(maximum, {0, -8161, 0}, 0) == 0);
}

TEST_CASE("F01293 cone narrows toward its strictly excluded apex",
          "[sphere-containment][F01293]") {
  const auto volume = Geometry({8, 4, 8, 0});
  CHECK(b::IsPointInTargetCone(volume, {255, 0, 0}, 0) == 1);
  CHECK(b::IsPointInTargetCone(volume, {256, 0, 0}, 0) == 0);
  CHECK(b::IsPointInTargetCone(volume, {127, -64, 0}, 0) == 1);
  CHECK(b::IsPointInTargetCone(volume, {128, -64, 0}, 0) == 0);
  CHECK(b::IsPointInTargetCone(volume, {0, -128, 0}, 0) == 0);
}

TEST_CASE("F01293 cone has a defined zero-height quotient and signed sum",
          "[sphere-containment][F01293]") {
  const auto flat = Geometry({8, 0, 8, 0});
  CHECK(b::IsPointInTargetCone(flat, {256, 0, 0}, 0) == 1);
  CHECK(b::IsPointInTargetCone(flat, {257, 0, 0}, 0) == 0);
  CHECK(b::IsPointInTargetCone(flat, {0, 1, 0}, 0) == 0);
  CHECK(b::IsPointInTargetCone(Geometry({1, 0, 1, 0}), {5000, 0, 5000}, 0) ==
        1);
}

TEST_CASE("F01293 nonzero cone modes shift by the full height",
          "[sphere-containment][F01293]") {
  const int mode = GENERATE(1, -7, std::numeric_limits<int>::max());
  const auto volume = Geometry({8, 4, 8, 0});
  CHECK(b::IsPointInTargetCone(volume, {0, 0, 0}, mode) == 0);
  CHECK(b::IsPointInTargetCone(volume, {0, 128, 0}, mode) == 1);
  CHECK(b::IsPointInTargetCone(volume, {128, 64, 0}, mode) == 0);
}

TEST_CASE("F01294 prism retains rectangle diamond and inclusive winding edges",
          "[sphere-containment][F01294]") {
  const auto volume = Geometry({8, 4, 8, 0});
  CHECK(b::IsPointInTargetPrism(volume, {256, 0, 256}, 1) == 1);
  CHECK(b::IsPointInTargetPrism(volume, {256, 0, 256}, 0) == 0);
  CHECK(b::IsPointInTargetPrism(volume, {256, 0, 0}, 0) == 1);
  CHECK(b::IsPointInTargetPrism(volume, {128, 0, 128}, 0) == 1);
  CHECK(b::IsPointInTargetPrism(volume, {129, 0, 128}, 0) == 0);
}

TEST_CASE("F01294 prism defines degenerate corners maximum extents and heights",
          "[sphere-containment][F01294]") {
  CHECK(b::IsPointInTargetPrism(Geometry({0, 0, 0, 255}), {0, 0, 0}, 0) == 1);
  CHECK(b::IsPointInTargetPrism(Geometry({0, 0, 0, 0}), {32767, 0, -32768},
                                1) == 1);
  const auto maximum = Geometry({255, 255, 255, 0});
  CHECK(b::IsPointInTargetPrism(maximum, {8160, -8160, 8160}, -1) == 1);
  CHECK(b::IsPointInTargetPrism(maximum, {8161, 0, 0}, 1) == 0);
  CHECK(b::IsPointInTargetPrism(maximum, {0, -8161, 0}, 1) == 0);
}

TEST_CASE("F01294 prism rotates the point before testing horizontal edges",
          "[sphere-containment][F01294]") {
  auto volume = Geometry({2, 4, 4, 0});
  CHECK(b::IsPointInTargetPrism(volume, {128, 0, 0}, 1) == 0);
  volume.yaw = 1024;
  CHECK(b::IsPointInTargetPrism(volume, {128, 0, 0}, 1) == 1);
  CHECK(b::IsPointInTargetPrism(volume, {0, 1, 0}, 1) == 0);
}

TEST_CASE(
    "F01295 selects every original callee and retains full signed results",
    "[sphere-containment][F01295]") {
  const int kind = GENERATE(1, 2, 3, 4, 5, 6, 7);
  Query query;
  query.kind = static_cast<std::uint8_t>(kind);
  const auto saved = query.extents;
  constexpr std::array<int, 7> kShapes{1, 2, 2, 3, 3, 4, 4};
  constexpr std::array<int, 7> kModes{0, 1, 0, 0, 1, 1, 0};
  CHECK(b::IsPointInTargetVolume(query) == -91);
  CHECK(query.selected == kShapes.at(static_cast<std::size_t>(kind - 1)));
  CHECK(query.mode == kModes.at(static_cast<std::size_t>(kind - 1)));
  CHECK(query.calls == 1);
  CHECK(query.extents == saved);
}

TEST_CASE(
    "F01295 height and pitch edits wrap bytes then restore the entire word",
    "[sphere-containment][F01295]") {
  Query query;
  query.kind = 2;
  CHECK(b::IsPointInTargetVolume(query) == query.result);
  CHECK(query.observed == b::TargetVolumeExtents{11, 144, 33, 250});
  CHECK(query.extents == b::TargetVolumeExtents{11, 200, 33, 250});
  query.kind = 5;
  CHECK(b::IsPointInTargetVolume(query) == query.result);
  CHECK(query.observed == b::TargetVolumeExtents{11, 200, 33, 122});
  CHECK(query.extents == b::TargetVolumeExtents{11, 200, 33, 250});
}

TEST_CASE("F01295 unknown kinds need no point or selected-query owner",
          "[sphere-containment][F01295]") {
  Query query;
  query.kind = GENERATE(0, 8, 129, 255);
  const auto saved = query.extents;
  CHECK(b::IsPointInTargetVolume(query) == 0);
  CHECK(query.calls == 0);
  CHECK(query.stores == 1);
  CHECK(query.extents == saved);
}
