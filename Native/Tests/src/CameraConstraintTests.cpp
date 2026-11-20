#include <array>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/ActorRecord.hpp"
#include "Actors/Location.hpp"
#include "Core/Log.hpp"
#include "Field/CameraLive.hpp"
#include "Field/CameraLiveState.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Mpd.hpp"
#include "Field/Room.hpp"
#include "Legacy/Psx.h"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"

namespace {
struct CameraConstraintFixture {
  VS::Field::RoomDataRecord saved_room = VS::Field::roomData;
  VS::Actors::ActorRecord* saved_actor = VS::Actors::actorRecords[0];
  VS::Core::LogObserver saved_observer = VS::Core::SetLogObserver({});
  VS::Actors::ActorRecord actor{};
  VS::Field::MpdCollisionHeader header{3, 3, 0, 9};
  std::array<VS::Field::MpdTileCell, 9> cells{};
  std::array<int, 17> geometry{};
  std::vector<std::string> warnings;

  CameraConstraintFixture() {
    VS::Field::roomData = {};
    VS::Field::roomData.collision_map = &header;
    VS::Field::roomData.tile_cells = cells.data();
    VS::Field::roomData.header.collision_map_length = sizeof header;
    VS::Field::roomData.header.tile_cells_length = sizeof cells;
    VS::Field::roomData.geometry = geometry.data();
    VS::Field::roomData.header.geometry_length = sizeof geometry;
    geometry[0] = 1;
    geometry[1] = 0x1000;
    VS::Actors::actorRecords[0] = &actor;
    VS::Core::SetLogObserver([this](const VS::Core::LogRecord& record) {
      if (record.channel == "field.camera") {
        CHECK(record.level == VS::Core::LogLevel::Warn);
        warnings.emplace_back(record.message);
      }
    });
  }
  ~CameraConstraintFixture() {
    VS::Field::roomData = saved_room;
    VS::Actors::actorRecords[0] = saved_actor;
    VS::Core::SetLogObserver(std::move(saved_observer));
  }
};
}  // namespace

TEST_CASE("Camera queries outside authored collision have no tile occlusion",
          "[field][camera][camera-constraint]") {
  CameraConstraintFixture fixture;
  const auto coordinate = GENERATE(3U, 31U, 255U);
  VS::Actors::TileTarget target{};
  target.tile_x = 1;
  target.tile_z = coordinate;
  VS::Field::LiveUpdateCameraTileConstraint(0, &target);
  CHECK((fixture.geometry[1] & 0x1000) == 0);
  REQUIRE(fixture.warnings.size() == 1);
  CHECK(fixture.warnings[0].find("collision") != std::string::npos);
  CHECK(target.tile_x == 1);
  CHECK(target.tile_z == coordinate);
}

TEST_CASE("Valid camera cells without authored occlusion retain clearing",
          "[field][camera][camera-constraint]") {
  CameraConstraintFixture fixture;
  VS::Actors::TileTarget target{};
  target.tile_x = 1;
  target.tile_z = 1;
  VS::Field::LiveUpdateCameraTileConstraint(0, &target);
  CHECK((fixture.geometry[1] & 0x1000) == 0);
  CHECK(fixture.warnings.empty());
}

TEST_CASE("Valid authored view triggers still hide the selected geometry group",
          "[field][camera][camera-constraint]") {
  CameraConstraintFixture fixture;
  const auto saved_yaw = VS::Render::FrameScratchpad().camera.angles.vy;
  VS::Render::FrameScratchpad().camera.angles.vy = 0;
  struct ViewTrigger {
    VS::Field::MpdViewTrigger header;
    std::array<char, 4> group_ids{};
  } trigger{};
  trigger.header.tile_x = 1;
  trigger.header.tile_z = 1;
  trigger.header.view_direction = 4;
  trigger.header.effect_count = 1;
  trigger.header.mode = VS::Field::sphericalCamera.preferred_distance == 0x900;
  fixture.cells[4].occlusion_and_floor_flags = 1;
  fixture.geometry[1] = 0;
  VS::Field::roomData.view_triggers = &trigger.header;
  VS::Field::roomData.header.view_triggers_length = sizeof trigger;
  VS::Actors::TileTarget target{};
  target.tile_x = 1;
  target.tile_z = 1;
  VS::Field::LiveUpdateCameraTileConstraint(0, &target);
  CHECK((fixture.geometry[1] & 0x1000) != 0);
  CHECK(fixture.warnings.empty());
  VS::Render::FrameScratchpad().camera.angles.vy = saved_yaw;
}
