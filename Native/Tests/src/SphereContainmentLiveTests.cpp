#include <array>
#include <cstdint>
#include <cstring>

#include <catch2/catch_test_macros.hpp>

#include "Battle/SphereContainmentLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "TestSupport/SphereContainmentLiveFixture.hpp"
extern "C" {
#include "Core/Gte.h"
}

namespace {
namespace b = VS::Battle;
namespace f = VS::test_support::sphere_containment;
namespace math = VS::Core::Math;

struct Projection {
  std::array<std::int16_t, 3> ir{};
  std::array<Core_GteWord, 3> mac{};
  Core_GteWord winding{};
  Core_GteWord screen{};

  friend bool operator==(const Projection&, const Projection&) = default;
};

Projection CurrentProjection() {
  SVECTOR ir{};
  VECTOR mac{};
  Projection projection;
  Core_Gte_StoreIr(&ir);
  Core_Gte_StoreLvnl(&mac);
  Core_Gte_StoreOpz(&projection.winding);
  Core_Gte_StoreSxy2(&projection.screen);
  projection.ir = {ir.vx, ir.vy, ir.vz};
  projection.mac = {mac.vx, mac.vy, mac.vz};
  return projection;
}
}  // namespace

TEST_CASE(
    "F01290 Live transform reads the current supplied volume without writes",
    "[sphere-containment-live][F01290]") {
  auto volume = f::Volume(1, {0, 1, 255, 255}, {-32768, 32767, -1});
  const auto saved = f::Bytes(volume);
  const auto first = b::BuildLiveTargetVolumeTestTransform(&volume);
  CHECK(first.origin == math::Position3{-32768, 32767, -1});
  CHECK(first.reciprocal_extents ==
        std::array<std::int16_t, 3>{-1, -32768, 128});
  volume.position[0] = 123;
  CHECK(b::BuildLiveTargetVolumeTestTransform(&volume).origin.x == 123);
  volume.position[0] = -32768;
  CHECK(f::Bytes(volume) == saved);
}

TEST_CASE(
    "F01291 typed local points agree with the actual production SDK helper",
    "[sphere-containment-live][F01291]") {
  auto volume = f::Volume(1, {255, 255, 255, 0});
  volume.angle = -512;
  const auto transform = b::BuildLiveTargetVolumeTestTransform(&volume);
  MATRIX matrix{};
  math::StoreMatrix(matrix, transform.rotation);
  SVECTOR input{32767, 0, 32767, 0};
  SVECTOR output{};
  CHECK(Core_Gte_ApplyMatrixSV(&matrix, &input, &output) == &output);
  CHECK(transform.LocalPoint({input.vx, input.vy, input.vz}) ==
        math::Position3{output.vx, output.vy, output.vz});
  CHECK(output.vx == 32767);
  input = {-32768, 0, -32768, 0};
  Core_Gte_ApplyMatrixSV(&matrix, &input, &output);
  CHECK(transform.LocalPoint({input.vx, input.vy, input.vz}) ==
        math::Position3{output.vx, output.vy, output.vz});
  CHECK(output.vx == -32768);
  Core_Gte_Reset();
}

TEST_CASE("F01291 Live point reads ignore a fourth supplied component",
          "[sphere-containment-live][F01291]") {
  const auto volume = f::Volume(1, {8, 8, 8, 0});
  const std::array<std::uint16_t, 4> point{255, 0, 0, 65535};
  CHECK(b::IsLivePointInTargetEllipsoid(&volume, point) == 1);
}

TEST_CASE("F01292 Live cylinder keeps nonzero mode and zero height semantics",
          "[sphere-containment-live][F01292]") {
  auto volume = f::Volume(3);
  CHECK(b::IsLivePointInTargetCylinder(&volume, f::Point{0, 64, 0}, -1) == 1);
  CHECK(b::IsLivePointInTargetCylinder(&volume, f::Point{0, 65, 0}, -1) == 0);
  volume = f::Volume(3, {0, 0, 0, 0});
  CHECK(b::IsLivePointInTargetCylinder(&volume, f::Point{32767, 0, 32768}, 0) ==
        1);
}

TEST_CASE("F01293 Live zero-height cone retains the defined radius257 edge",
          "[sphere-containment-live][F01293]") {
  const auto volume = f::Volume(4, {8, 0, 8, 0});
  CHECK(b::IsLivePointInTargetCone(&volume, f::Point{256, 0, 0}, 0) == 1);
  CHECK(b::IsLivePointInTargetCone(&volume, f::Point{257, 0, 0}, 0) == 0);
}

TEST_CASE("F01294 Live prism preserves degenerate and nonzero rectangle modes",
          "[sphere-containment-live][F01294]") {
  auto volume = f::Volume(6);
  CHECK(b::IsLivePointInTargetPrism(&volume, f::Point{256, 0, 256}, -9) == 1);
  CHECK(b::IsLivePointInTargetPrism(&volume, f::Point{256, 0, 256}, 0) == 0);
  volume = f::Volume(6, {0, 0, 0, 0});
  CHECK(b::IsLivePointInTargetPrism(&volume, f::Point{32767, 0, 32768}, 0) ==
        1);
}

TEST_CASE("F01295 shape aliases observe the temporary wrapped height byte",
          "[sphere-containment-live][F01295]") {
  auto volume = f::Volume(2, {255, 128, 255, 0}, {0, 255, 0});
  const auto saved = f::Bytes(volume);
  f::Point earlier{};
  std::memcpy(earlier.data(), f::FlagsPoint(volume).data(), sizeof earlier);
  CHECK(b::IsLivePointInTargetVolume(&volume, earlier) == 0);
  CHECK(b::IsLivePointInTargetVolume(&volume, f::FlagsPoint(volume)) == 1);
  CHECK(f::Bytes(volume) == saved);
}

TEST_CASE("F01295 shape aliases observe the temporary wrapped pitch byte",
          "[sphere-containment-live][F01295]") {
  auto volume = f::Volume(5, {255, 8, 255, 128}, {0, 127, 0});
  const auto saved = f::Bytes(volume);
  f::Point earlier{};
  std::memcpy(earlier.data(), f::FlagsPoint(volume).data(), sizeof earlier);
  CHECK(b::IsLivePointInTargetVolume(&volume, earlier) == 0);
  CHECK(b::IsLivePointInTargetVolume(&volume, f::FlagsPoint(volume)) == 1);
  CHECK(f::Bytes(volume) == saved);
}

TEST_CASE("F01295 unused point and sphere owners remain lazy",
          "[sphere-containment-live][F01295]") {
  f::State state;
  Battle_targetSphere = nullptr;
  auto volume = f::Volume(129);
  const auto saved = f::Bytes(volume);
  CHECK(b::IsLivePointInTargetVolume(&volume, {}) == 0);
  CHECK(f::Bytes(volume) == saved);
  volume.kind = 1;
  CHECK(b::IsLivePointInTargetVolume(&volume, f::Point{}) == 1);
}

TEST_CASE("Current-sphere containment resolves a rebound owner on every query",
          "[sphere-containment-live][F01295]") {
  f::State state;
  state.spheres[0].volumes[1] = f::Volume(1);
  state.spheres[1].volumes[1] = f::Volume(0);
  CHECK(b::IsLivePointInCurrentSphereVolume(1, f::Point{}) == 1);
  state.Select(1);
  CHECK(b::IsLivePointInCurrentSphereVolume(1, {}) == 0);
  state.Select(0);
  CHECK(b::IsLivePointInCurrentSphereVolume(1, f::Point{}) == 1);
}

TEST_CASE("Containment no longer publishes unconsumed projection scratch state",
          "[sphere-containment-live][F01291][F01294]") {
  Core_Gte_Reset();
  SVECTOR position{17, -31, 1000, 0};
  Core_Gte_LoadV0(&position);
  Core_Gte_Rtps();
  const auto before = CurrentProjection();
  auto volume = f::Volume(6);
  CHECK(b::IsLivePointInTargetVolume(&volume, f::Point{}) == 1);
  volume.kind = 1;
  CHECK(b::IsLivePointInTargetVolume(&volume, f::Point{}) == 1);
  CHECK(CurrentProjection() == before);
  Core_Gte_Reset();
}
