#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ActorMovementLive.hpp"
#include "Core/Log.hpp"
#include "Field/ActorCollisionLive.hpp"
#include "Field/PlatformPlacement.hpp"
#include "Field/PlatformPlacementLive.hpp"
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
  const auto contact = VS::Field::LiveMovingPlatformPlacement(3);
  REQUIRE(contact.has_value());
  CheckAuthoredPlacement(*contact);
  CHECK(fixture.platforms[0].isHidden == 1);
  CHECK(fixture.platforms[1].isHidden == 1);
  std::array<Field_PlatformContactPoint, 4> visible{};
  CHECK(Field_CollectMovingPlatformPositions(visible.data()) == 0);
  CHECK_FALSE(VS::Field::FindLiveMovingPlatformById(3).has_value());
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
    const auto placement = VS::Field::LiveMovingPlatformPlacement(3);
    const auto found = VS::Field::FindLiveMovingPlatformById(3);
    REQUIRE(placement.has_value());
    REQUIRE(found.has_value());
    for (const auto& contact : {*placement, *found}) {
      CHECK(contact.x == visible[0].x);
      CHECK(contact.height == visible[0].height);
      CHECK(contact.z == visible[0].z);
      CHECK(contact.platformIndex == visible[0].platformIndex);
    }
  }
}

TEST_CASE("Visible-platform lookup returns its contact by value",
          "[field][platform-placement]") {
  PlatformPlacementFixture fixture;
  fixture.platforms[1].isHidden = 0;
  const auto contact = VS::Field::FindLiveMovingPlatformById(3);
  REQUIRE(contact.has_value());
  REQUIRE(VS::Field::LiveMovingPlatformPlacement(2).has_value());
  fixture.platforms[1].positionX += 64 * ONE;
  CheckAuthoredPlacement(*contact);
}

TEST_CASE("Invalid platform placement reports no contact",
          "[field][platform-placement]") {
  PlatformPlacementFixture fixture;
  CHECK_FALSE(VS::Field::LiveMovingPlatformPlacement(1).has_value());
  CHECK_FALSE(VS::Field::LiveMovingPlatformPlacement(4).has_value());
  Field_roomData.header.movingPlatformsLength = 1;
  CHECK_FALSE(VS::Field::LiveMovingPlatformPlacement(3).has_value());
  Field_roomData.header.movingPlatformsLength = sizeof(fixture.platforms);
  Field_platformBounceFrames[1] = 8;
  CHECK_FALSE(VS::Field::LiveMovingPlatformPlacement(3).has_value());
}

namespace {
using VS::Field::MovingPlatformPose;

constexpr std::array<std::int16_t, 7> kBounce{8, 14, 16, 11, 7, 3, 1};

VS::Field::MovingPlatformRoom Room(std::span<const MovingPlatformPose> poses) {
  return {.platforms = poses, .bounce_offsets = kBounce};
}
}  // namespace

TEST_CASE("Typed platform lookup finds a visible platform by support layer",
          "[field][platform-placement]") {
  const std::array<MovingPlatformPose, 1> poses{MovingPlatformPose{
      .x = 64 * ONE, .y = 32 * ONE, .z = 16 * ONE, .bounce_frame = 7}};
  CHECK(VS::Field::FindMovingPlatformById(Room(poses), 2) ==
        VS::Field::PlatformContact{
            .x = 64, .height = -32, .z = 16, .platform_index = 0});
}

TEST_CASE("Typed platform placement reads a platform the lookup hides",
          "[field][platform-placement]") {
  const std::array<MovingPlatformPose, 2> poses{
      MovingPlatformPose{},
      MovingPlatformPose{
          .x = -4097, .y = -ONE, .bounce_frame = 2, .hidden = true}};
  CHECK_FALSE(VS::Field::FindMovingPlatformById(Room(poses), 3).has_value());
  // C division truncates toward zero: -4097 / 4096 is -1.
  CHECK(VS::Field::MovingPlatformPlacement(Room(poses), 3) ==
        VS::Field::PlatformContact{
            .x = -1, .height = 17, .z = 0, .platform_index = 1});
}

TEST_CASE("Typed platform lookup rejects absent layers and bad frames",
          "[field][platform-placement]") {
  std::array<MovingPlatformPose, 1> poses{};
  const auto room = Room(poses);
  for (const int layer : {0, 1, 3, 6}) {
    CHECK_FALSE(VS::Field::FindMovingPlatformById(room, layer).has_value());
  }
  poses[0].bounce_frame = 8;
  CHECK_FALSE(VS::Field::MovingPlatformPlacement(room, 2).has_value());
  poses[0].bounce_frame = -1;
  CHECK_FALSE(VS::Field::MovingPlatformPlacement(room, 2).has_value());
  poses[0] = {.x = 40000 * ONE, .bounce_frame = 7};
  CHECK_FALSE(VS::Field::FindMovingPlatformById(room, 2).has_value());
  const std::array<MovingPlatformPose, 5> crowded{};
  CHECK_FALSE(VS::Field::FindMovingPlatformById(Room(crowded), 2).has_value());
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

// The fields the jump, contact and ledge paths write.
auto Observed(const Actors_ActorRecord& actor) {
  const auto& position = actor.instance.position;
  const auto& anchor = actor.platformAnchor;
  return std::tuple(position.vx, position.vy, position.vz, anchor.vx, anchor.vy,
                    anchor.vz, actor.platformOffsetX, actor.platformOffsetZ,
                    actor.moveFacing, actor.instance.jumpCountdown,
                    actor.landingPending, actor.destinationX,
                    actor.destinationZ);
}

bool Same(const Actors_ActorRecord& a, const Actors_ActorRecord& b) {
  return Observed(a) == Observed(b);
}
}  // namespace

TEST_CASE("Jump to a hidden platform is refused before any actor change",
          "[field][platform-placement][issue-257]") {
  PlatformPlacementFixture fixture;
  PlatformErrorLog log;
  auto actor = OnPlatform();
  const auto before = actor;
  const VS::Actors::PackedTile target{.x = 5, .layer = kLayer, .z = 7};
  VS::Actors::BeginLiveActorJumpToTile(&actor, target, 32, 0);
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
  const VS::Actors::PackedTile target{.x = 5, .layer = kLayer, .z = 7};
  VS::Actors::BeginLiveActorJumpToTile(&actor, target, 32, 0);
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
    VS::Actors::UpdateLiveActorPlatformContact(&actor, mode);
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
  VS::Actors::UpdateLiveActorPlatformContact(&actor, 0);
  CHECK(actor.platformOffsetX == 6);
  CHECK(actor.platformOffsetZ == 8);
  CHECK(actor.instance.position.vx == 1206);
  CHECK(actor.instance.position.vz == 448);
  auto anchored = OnPlatform();
  VS::Actors::UpdateLiveActorPlatformContact(&anchored, 1);
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
  CHECK(VS::Field::LiveCalcActorLedgeOffset(&actor, &offset) == 0);
  CHECK(Same(actor, before));
  CHECK(offset.vx == 1);
  CHECK(offset.vy == 2);
  CHECK(offset.vz == 3);
  CHECK(log.errors.size() == 1);
}
