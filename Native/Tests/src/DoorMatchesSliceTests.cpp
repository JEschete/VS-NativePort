#include <array>
#include <cstddef>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Field/DoorMatches.hpp"

namespace {
namespace field = VS::Field;

using Indices = std::vector<std::size_t>;

constexpr std::array<field::DoorTile, 6> kDoors{{
    {.x = 4, .z = 7, .layer = 0},
    {.x = 4, .z = 7, .layer = 1},
    {.x = 8, .z = 9, .layer = 0},
    {.x = 4, .z = 7, .layer = 0},
    {.x = 1, .z = 2, .layer = 2},
    {.x = 4, .z = 7, .layer = 3},
}};
}  // namespace

TEST_CASE("Typed door lookup matches tile and layer in record order",
          "[field][doors][typed]") {
  Indices matches{99};
  field::TransitionDoorsAtTile(kDoors, 4, 7, 0, matches);
  CHECK(matches == Indices{0, 3});
  field::TransitionDoorsAtTile(kDoors, 4, 7, 1, matches);
  CHECK(matches == Indices{1});
  field::TransitionDoorsAtTile(kDoors, 8, 9, 1, matches);
  CHECK(matches.empty());
}

TEST_CASE("Typed door lookup lists every door on a platform layer",
          "[field][doors][typed]") {
  Indices matches;
  field::TransitionDoorsAtTile(kDoors, -1, -1, field::kFirstPlatformLayer,
                               matches);
  CHECK(matches == Indices{4});
  field::TransitionDoorsAtTile(kDoors, 0, 0, 3, matches);
  CHECK(matches == Indices{5});
  field::TransitionDoorsAtTile({}, 4, 7, 0, matches);
  CHECK(matches.empty());
}
