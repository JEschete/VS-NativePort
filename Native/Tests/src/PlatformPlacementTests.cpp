#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Log.hpp"
#include "Field/PlatformPlacement.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Field/Collision.h"
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

// #257: callers of the moving-platform lookup must not read through the null
// result for a hidden or absent platform (the original reads PS1 address 0).
namespace {
namespace Core = VS::Core;

constexpr int kLayer = 3;

struct PlatformErrorLog {
  std::vector<std::string> errors;
  Core::LogLevel console = Core::SetConsoleLogLevel(Core::LogLevel::Off);
  Core::LogObserver saved =
      Core::SetLogObserver([this](const Core::LogRecord& record) {
        if (record.level == Core::LogLevel::Error &&
            record.channel == "field.platform") {
          errors.emplace_back(record.message);
        }
      });

  PlatformErrorLog() = default;
  PlatformErrorLog(const PlatformErrorLog&) = delete;
  PlatformErrorLog& operator=(const PlatformErrorLog&) = delete;
  PlatformErrorLog(PlatformErrorLog&&) = delete;
  PlatformErrorLog& operator=(PlatformErrorLog&&) = delete;
  ~PlatformErrorLog() {
    Core::SetLogObserver(std::move(saved));
    Core::SetConsoleLogLevel(console);
  }
};

Actors_ActorRecord OnPlatform() {
  Actors_ActorRecord actor{};
  actor.instance.supportLayer = kLayer;
  actor.instance.placementLayer = kLayer;
  actor.instance.position = {1200, -768, 440, 0};
  actor.runSpeed = 64;
  actor.collisionRadius = 20;
  return actor;
}

bool Same(const Actors_ActorRecord& a, const Actors_ActorRecord& b) {
  return std::memcmp(&a, &b, sizeof(a)) == 0;
}
}  // namespace

TEST_CASE("Jump to a hidden platform is refused before any actor change",
          "[field][platform-placement][issue-257]") {
  PlatformPlacementFixture fixture;
  PlatformErrorLog log;
  auto actor = OnPlatform();
  const auto before = actor;
  std::array<u_char, 4> target{5, kLayer, 7, 0};
  Actors_BeginActorJumpToTile(&actor, target.data(), 32, 0);
  CHECK(Same(actor, before));
  REQUIRE(log.errors.size() == 1);
  CHECK(log.errors[0].find("layer 3") != std::string::npos);
}

TEST_CASE("Jump to a visible platform targets its contact position",
          "[field][platform-placement][issue-257]") {
  PlatformPlacementFixture fixture;
  PlatformErrorLog log;
  fixture.platforms[1].isHidden = 0;
  auto actor = OnPlatform();
  actor.instance.position.vy = -600;  // the platform top is 168 above
  auto* saved = Actors_actorRecords[0];
  Actors_actorRecords[0] = &actor;
  std::array<u_char, 4> target{5, kLayer, 7, 0};
  Actors_BeginActorJumpToTile(&actor, target.data(), 32, 0);
  Actors_actorRecords[0] = saved;
  CHECK(log.errors.empty());
  CHECK(actor.destinationX == 1216);
  CHECK(actor.destinationZ == 448);
}

TEST_CASE("Platform contact update keeps the actor when its platform is hidden",
          "[field][platform-placement][issue-257]") {
  PlatformPlacementFixture fixture;
  PlatformErrorLog log;
  for (const int mode : {0, 1}) {
    auto actor = OnPlatform();
    const auto before = actor;
    Actors_UpdateActorPlatformContact(&actor, mode);
    CHECK(Same(actor, before));
  }
  CHECK(log.errors.size() == 2);
}

TEST_CASE("Platform contact update follows and anchors a visible platform",
          "[field][platform-placement][issue-257]") {
  PlatformPlacementFixture fixture;
  PlatformErrorLog log;
  fixture.platforms[1].isHidden = 0;
  auto actor = OnPlatform();
  actor.platformAnchor = {1210, -768, 440, 0};
  Actors_UpdateActorPlatformContact(&actor, 0);
  CHECK(actor.platformOffsetX == 6);
  CHECK(actor.platformOffsetZ == 8);
  CHECK(actor.instance.position.vx == 1206);
  CHECK(actor.instance.position.vz == 448);
  auto anchored = OnPlatform();
  Actors_UpdateActorPlatformContact(&anchored, 1);
  CHECK(anchored.platformAnchor.vx == 1216);
  CHECK(anchored.platformAnchor.vy == -768);
  CHECK(anchored.platformAnchor.vz == 448);
  CHECK(log.errors.empty());
}

TEST_CASE("Ledge probe on a hidden platform reports no ledge unchanged",
          "[field][platform-placement][issue-257]") {
  PlatformPlacementFixture fixture;
  PlatformErrorLog log;
  auto actor = OnPlatform();
  actor.moveFacing = 123;
  const auto before = actor;
  SVECTOR offset{1, 2, 3, 4};
  CHECK(Field_CalcActorLedgeOffset(&actor, &offset) == 0);
  CHECK(Same(actor, before));
  CHECK(offset.vx == 1);
  CHECK(offset.vy == 2);
  CHECK(offset.vz == 3);
  CHECK(log.errors.size() == 1);
}
