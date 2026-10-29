#include <array>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Log.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Location.h"
#include "Actors/Movement.h"
#include "Field/Camera.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace {
struct CameraConstraintFixture {
  Field_RoomRecord saved_room = Field_roomData;
  Actors_ActorRecord* saved_actor = Actors_actorRecords[0];
  VS::Core::LogObserver saved_observer = VS::Core::SetLogObserver({});
  Actors_ActorRecord actor{};
  Field_MpdCollisionHeader header{3, 3, 0, 9};
  std::array<Field_MpdTileCell, 9> cells{};
  std::array<int, 17> geometry{};
  std::vector<std::string> warnings;

  CameraConstraintFixture() {
    Field_roomData = {};
    Field_roomData.collisionMap = &header;
    Field_roomData.tileCells = cells.data();
    Field_roomData.geometry = geometry.data();
    geometry[0] = 1;
    geometry[1] = 0x1000;
    Actors_actorRecords[0] = &actor;
    VS::Core::SetLogObserver([this](const VS::Core::LogRecord& record) {
      if (record.channel == "field.camera") {
        CHECK(record.level == VS::Core::LogLevel::Warn);
        warnings.emplace_back(record.message);
      }
    });
  }
  ~CameraConstraintFixture() {
    Field_roomData = saved_room;
    Actors_actorRecords[0] = saved_actor;
    VS::Core::SetLogObserver(std::move(saved_observer));
  }
};
}  // namespace

TEST_CASE("Camera queries outside authored collision have no tile occlusion",
          "[field][camera][camera-constraint]") {
  CameraConstraintFixture fixture;
  const auto coordinate = GENERATE(3U, 31U, 255U);
  Actors_TileTarget target{};
  target.tileX = 1;
  target.tileZ = coordinate;
  Field_UpdateCameraTileConstraint(0, &target);
  CHECK((fixture.geometry[1] & 0x1000) == 0);
  REQUIRE(fixture.warnings.size() == 1);
  CHECK(fixture.warnings[0].find("collision") != std::string::npos);
  CHECK(target.tileX == 1);
  CHECK(target.tileZ == coordinate);
}

TEST_CASE("Valid camera cells without authored occlusion retain clearing",
          "[field][camera][camera-constraint]") {
  CameraConstraintFixture fixture;
  Actors_TileTarget target{};
  target.tileX = 1;
  target.tileZ = 1;
  Field_UpdateCameraTileConstraint(0, &target);
  CHECK((fixture.geometry[1] & 0x1000) == 0);
  CHECK(fixture.warnings.empty());
}

TEST_CASE("Valid authored view triggers still hide the selected geometry group",
          "[field][camera][camera-constraint]") {
  CameraConstraintFixture fixture;
  const auto saved_yaw = Render_frameScratch.camera.angles.vy;
  Render_frameScratch.camera.angles.vy = 0;
  struct ViewTrigger {
    Field_MpdViewTrigger header;
    std::array<char, 4> group_ids{};
  } trigger{};
  trigger.header.tileX = 1;
  trigger.header.tileZ = 1;
  trigger.header.viewDirection = 4;
  trigger.header.effectCount = 1;
  trigger.header.mode = Field_sphericalCamera.initialDistance == 0x900;
  fixture.cells[4].occlusionAndFloorFlags = 1;
  fixture.geometry[1] = 0;
  Field_roomData.viewTriggers = &trigger.header;
  Field_roomData.header.viewTriggersLength = sizeof trigger;
  Actors_TileTarget target{};
  target.tileX = 1;
  target.tileZ = 1;
  Field_UpdateCameraTileConstraint(0, &target);
  CHECK((fixture.geometry[1] & 0x1000) != 0);
  CHECK(fixture.warnings.empty());
  Render_frameScratch.camera.angles.vy = saved_yaw;
}
