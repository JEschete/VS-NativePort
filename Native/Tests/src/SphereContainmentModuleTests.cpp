#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Battle/SphereContainmentLive.hpp"
#include "Content/ModuleData.h"
#include "TestSupport/SphereContainmentLiveFixture.hpp"

namespace {
namespace b = VS::Battle;
namespace f = VS::test_support::sphere_containment;
using Point = std::span<const std::uint16_t>;

template <typename Function>
Function Entry(unsigned address, Function expected) {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  void* const pointer =
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG", address);
  const bool matches =
      pointer != nullptr && pointer == reinterpret_cast<void*>(expected);
  CHECK(matches);
  if (!matches) return nullptr;
  return reinterpret_cast<Function>(pointer);
}

static_assert(std::is_same_v<
              decltype(&b::BuildLiveTargetVolumeTestTransform),
              b::SphereContainmentTransform (*)(const Battle_TargetVolume*)>);
static_assert(std::is_same_v<decltype(&b::IsLivePointInTargetEllipsoid),
                             int (*)(const Battle_TargetVolume*, Point)>);
static_assert(std::is_same_v<decltype(&b::IsLivePointInTargetCylinder),
                             int (*)(const Battle_TargetVolume*, Point, int)>);
static_assert(std::is_same_v<decltype(&b::IsLivePointInTargetCone),
                             int (*)(const Battle_TargetVolume*, Point, int)>);
static_assert(std::is_same_v<decltype(&b::IsLivePointInTargetPrism),
                             int (*)(const Battle_TargetVolume*, Point, int)>);
static_assert(std::is_same_v<decltype(&b::IsLivePointInTargetVolume),
                             int (*)(Battle_TargetVolume*, Point)>);
}  // namespace

TEST_CASE("F01290 actual production module entry returns a semantic transform",
          "[sphere-containment-module][F01290]") {
  const auto build = Entry(0x800C0FA8, &b::BuildLiveTargetVolumeTestTransform);
  if (build == nullptr) return;
  const auto volume = f::Volume(1, {0, 1, 255, 0});
  CHECK(build(&volume).reciprocal_extents ==
        std::array<std::int16_t, 3>{-1, -32768, 128});
}

TEST_CASE("F01291 actual production module entry excludes the ellipsoid edge",
          "[sphere-containment-module][F01291]") {
  const auto query = Entry(0x800C1034, &b::IsLivePointInTargetEllipsoid);
  if (query == nullptr) return;
  const auto volume = f::Volume(1);
  CHECK(query(&volume, f::Point{256, 0, 0}) == 0);
}

TEST_CASE(
    "F01292 actual production module entry retains any nonzero height mode",
    "[sphere-containment-module][F01292]") {
  const auto query = Entry(0x800C110C, &b::IsLivePointInTargetCylinder);
  if (query == nullptr) return;
  const auto volume = f::Volume(3);
  CHECK(query(&volume, f::Point{0, 64, 0}, -1) == 1);
}

TEST_CASE(
    "F01293 actual production module entry preserves zero-height division",
    "[sphere-containment-module][F01293]") {
  const auto query = Entry(0x800C123C, &b::IsLivePointInTargetCone);
  if (query == nullptr) return;
  const auto volume = f::Volume(4, {8, 0, 8, 0});
  CHECK(query(&volume, f::Point{256, 0, 0}, 0) == 1);
}

TEST_CASE(
    "F01294 actual production module entry preserves inclusive prism edges",
    "[sphere-containment-module][F01294]") {
  const auto query = Entry(0x800C1384, &b::IsLivePointInTargetPrism);
  if (query == nullptr) return;
  const auto volume = f::Volume(6);
  CHECK(query(&volume, f::Point{256, 0, 256}, 1) == 1);
}

TEST_CASE(
    "F01295 actual production module entry observes aliased temporary flags",
    "[sphere-containment-module][F01295]") {
  const auto query = Entry(0x800C1564, &b::IsLivePointInTargetVolume);
  if (query == nullptr) return;
  auto volume = f::Volume(2, {255, 128, 255, 0}, {0, 255, 0});
  const auto saved = f::Bytes(volume);
  CHECK(query(&volume, f::FlagsPoint(volume)) == 1);
  CHECK(f::Bytes(volume) == saved);
}

TEST_CASE(
    "Actual production registry does not expose a false overlay or interior",
    "[sphere-containment-module][F01295]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "TITLE/TITLE.PRG",
                                          0x800C1564) == nullptr);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800C1568) == nullptr);
}

TEST_CASE(
    "Actual production pointer relocation resets and rebinds the sphere owner",
    "[sphere-containment-module][F01295]") {
  f::State state;
  state.spheres[0].volumes[1] = f::Volume(1);
  state.spheres[1].volumes[1] = f::Volume(0);
  const std::array<Content_ModuleSymbol, 2> owners{{
      {"BATTLE/BATTLE.PRG", 0x80200000, 1, &state.spheres[0],
       sizeof(Battle_TargetSphere), 1, 1},
      {"BATTLE/BATTLE.PRG", 0x80210000, 1, &state.spheres[1],
       sizeof(Battle_TargetSphere), 1, 1},
  }};
  const Content_ModulePointerTable table{
      "BATTLE/BATTLE.PRG",
      0,
      1,
      reinterpret_cast<void**>(&Battle_targetSphere),
      1,
      nullptr};
  const std::array<unsigned char, 4> cleared{};
  REQUIRE(VS::Content::RelocateModulePointerTable(table, cleared, owners));
  CHECK(Battle_targetSphere == nullptr);
  const auto owner_b =
      std::bit_cast<std::array<unsigned char, 4>>(std::uint32_t{0x80210000});
  REQUIRE(VS::Content::RelocateModulePointerTable(table, owner_b, owners));
  CHECK(b::IsLivePointInCurrentSphereVolume(1, {}) == 0);
  const auto owner_a =
      std::bit_cast<std::array<unsigned char, 4>>(std::uint32_t{0x80200000});
  REQUIRE(VS::Content::RelocateModulePointerTable(table, owner_a, owners));
  CHECK(b::IsLivePointInCurrentSphereVolume(1, f::Point{}) == 1);
}

TEST_CASE(
    "Actual module data copying leaves no stale containment geometry cache",
    "[sphere-containment-module][F01290][F01295]") {
  auto volume = f::Volume(3);
  CHECK(b::IsLivePointInTargetVolume(&volume, f::Point{}) == 1);
  const auto replacement = f::Volume(0);
  const auto bytes = f::Bytes(replacement);
  const Content_ModuleDataEntry entry{"BATTLE/BATTLE.PRG", 0, sizeof volume,
                                      &volume, sizeof volume};
  REQUIRE(VS::Content::CopyModuleDataEntry(entry, bytes));
  CHECK(b::IsLivePointInTargetVolume(&volume, {}) == 0);
}
