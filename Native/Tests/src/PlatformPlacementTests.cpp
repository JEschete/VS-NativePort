#include <algorithm>
#include <array>

#include <catch2/catch_test_macros.hpp>

#include "Field/PlatformPlacement.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
}

namespace {
struct PlatformPlacementFixture {
  Field_RoomRecord saved_room = Field_roomData;
  decltype(std::to_array(Field_platformBounceFrames)) saved_bounce =
      std::to_array(Field_platformBounceFrames);
  std::array<Field_MpdMovingPlatform, 2> platforms{};

  PlatformPlacementFixture() {
    Field_roomData = {};
    Field_roomData.movingPlatforms = platforms.data();
    Field_roomData.header.movingPlatformsLength = sizeof(platforms);
    std::fill_n(Field_platformBounceFrames, saved_bounce.size(), 7);
    for (auto& platform : platforms) {
      platform.positionX = 1216 * ONE;
      platform.positionY = 768 * ONE;
      platform.positionZ = 448 * ONE;
      platform.isHidden = 1;
    }
  }

  ~PlatformPlacementFixture() {
    Field_roomData = saved_room;
    std::copy(saved_bounce.begin(), saved_bounce.end(),
              Field_platformBounceFrames);
  }
};
}  // namespace

namespace {
void CheckAuthoredPlacement(const Field_PlatformContactPoint& contact) {
  CHECK(contact.x == 1216);
  CHECK(contact.height == -768);
  CHECK(contact.z == 448);
  CHECK(contact.platformIndex == 1);
}
}  // namespace

TEST_CASE(
    "Scripted placement reads a hidden authored platform without making "
    "it collide or render",
    "[field][platform-placement]") {
  PlatformPlacementFixture fixture;
  Field_PlatformContactPoint contact{};
  REQUIRE(Field_CopyMovingPlatformPlacement(3, &contact) == 1);
  CheckAuthoredPlacement(contact);
  CHECK(fixture.platforms[0].isHidden == 1);
  CHECK(fixture.platforms[1].isHidden == 1);
  std::array<Field_PlatformContactPoint, 4> visible{};
  CHECK(Field_CollectMovingPlatformPositions(visible.data()) == 0);
  CHECK(Field_FindMovingPlatformById(3) == nullptr);
  fixture.platforms[1].positionX += ONE;
  CHECK(contact.x == 1216);
}

TEST_CASE(
    "Visible platform placement preserves the original contact and bounce",
    "[field][platform-placement]") {
  PlatformPlacementFixture fixture;
  fixture.platforms[1].isHidden = 0;
  for (int frame = 0; frame < 7; ++frame) {
    Field_platformBounceFrames[1] = static_cast<char>(frame);
    std::array<Field_PlatformContactPoint, 4> visible{};
    REQUIRE(Field_CollectMovingPlatformPositions(visible.data()) == 1);
    Field_PlatformContactPoint placement{};
    REQUIRE(Field_CopyMovingPlatformPlacement(3, &placement) == 1);
    CHECK(placement.x == visible[0].x);
    CHECK(placement.height == visible[0].height);
    CHECK(placement.z == visible[0].z);
    CHECK(placement.platformIndex == visible[0].platformIndex);
  }
}

TEST_CASE("Borrowed visible-platform lookup outlives the lookup stack frame",
          "[field][platform-placement]") {
  PlatformPlacementFixture fixture;
  fixture.platforms[1].isHidden = 0;
  const auto* contact = Field_FindMovingPlatformById(3);
  REQUIRE(contact != nullptr);
  Field_PlatformContactPoint independent{};
  REQUIRE(Field_CopyMovingPlatformPlacement(2, &independent) == 1);
  CHECK(contact->x == 1216);
  CHECK(contact->height == -768);
  CHECK(contact->z == 448);
  CHECK(contact->platformIndex == 1);
}

TEST_CASE("Invalid platform placement reports failure and preserves output",
          "[field][platform-placement]") {
  PlatformPlacementFixture fixture;
  Field_PlatformContactPoint contact{1, 2, 3, 4};
  CHECK(Field_CopyMovingPlatformPlacement(1, &contact) == 0);
  CHECK(Field_CopyMovingPlatformPlacement(4, &contact) == 0);
  CHECK(Field_CopyMovingPlatformPlacement(3, nullptr) == 0);
  Field_platformBounceFrames[1] = 8;
  CHECK(Field_CopyMovingPlatformPlacement(3, &contact) == 0);
  CHECK(contact.x == 1);
  CHECK(contact.height == 2);
  CHECK(contact.z == 3);
  CHECK(contact.platformIndex == 4);
}
