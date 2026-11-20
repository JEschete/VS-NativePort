#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ActorMovementLive.hpp"
#include "Actors/ActorRecord.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Content/ModuleData.hpp"
#include "Core/Log.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Field/ActorCollisionLive.hpp"
#include "Field/Doors.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Mpd.hpp"
#include "Field/PlatformPlacement.hpp"
#include "Field/PlatformPlacementLive.hpp"
#include "Field/Room.hpp"
#include "Field/RoomEffects.hpp"
#include "Field/RoomRenderingLiveState.hpp"
#include "Legacy/Psx.h"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/Render.hpp"
#include "Render/RoomDraw.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.hpp"
#include "State/Timers.hpp"

namespace {
constexpr auto kGroupBytes = VS::Render::kRoomGroupRecordBytes;
constexpr auto kGeometryWordCount = 1 + kGroupBytes / sizeof(std::int32_t);
using GeometryWords = std::array<std::int32_t, kGeometryWordCount>;

void CheckWaypointResetFlags(
    const VS::Field::MovingPlatformWaypointState& state) {
  CHECK_FALSE(state.hidden);
  CHECK_FALSE(state.waiting);
  CHECK(state.control_state == 0);
  CHECK(state.motion_ticks == 0);
}

void CheckWaypointResetPosition(
    const VS::Field::MovingPlatformWaypointState& state,
    const VS::Field::MovingPlatformWaypoint& point) {
  CHECK(state.x == point.x);
  CHECK(state.y == point.y);
  CHECK(state.z == point.z);
}

void CheckWaypointResetRoute(
    const VS::Field::MovingPlatformWaypointState& state, std::int32_t selected,
    std::uint8_t end) {
  const auto index = static_cast<std::size_t>(selected & 3);
  CHECK(state.start_waypoint == index);
  CHECK(state.current_segment == (selected & 7));
  const bool forward = index != end;
  CHECK(state.moving_forward == forward);
  CHECK(state.current_waypoint == ((selected + (forward ? 1 : -1)) & 3));
}

void CheckWaypointResetState(
    const VS::Field::MovingPlatformWaypointState& state,
    const VS::Field::MovingPlatformWaypoint& point, std::int32_t selected,
    std::uint8_t end) {
  CheckWaypointResetFlags(state);
  CheckWaypointResetPosition(state, point);
  CheckWaypointResetRoute(state, selected, end);
}

struct RecordingFreezeHost final : VS::Field::MovingPlatformFreezeHost {
  VS::Field::MovingPlatformRuntimeState* runtime = nullptr;
  bool has_platforms = true;
  std::optional<bool> appearance;
  std::optional<std::int16_t> duration_during_appearance;
  bool HasPlatforms() const override { return has_platforms; }
  void SetFreezeAppearance(bool enabled) override {
    appearance = enabled;
    if (runtime != nullptr) duration_during_appearance = runtime->freeze_frames;
  }
};

struct RecordingPlatformVisibilityHost final
    : VS::Field::MovingPlatformVisibilityHost {
  bool present = true;
  bool valid = true;
  bool geometry = true;
  bool hidden = false;
  bool geometry_hidden = false;
  bool invalidate_on_lookup = false;
  std::optional<bool> callback_hidden;
  std::optional<bool> stored_hidden;
  std::vector<int> calls;
  bool StoreHidden(bool value) override {
    if (!present || !valid) return false;
    calls.push_back(1);
    stored_hidden = value;
    hidden = value;
    return true;
  }
  bool FindGeometryGroup() override {
    calls.push_back(2);
    if (callback_hidden) hidden = *callback_hidden;
    if (invalidate_on_lookup) valid = false;
    return geometry;
  }
  std::optional<bool> Hidden() const override {
    return valid ? std::optional(hidden) : std::nullopt;
  }
  bool StoreGeometryHidden(bool value) override {
    calls.push_back(3);
    geometry_hidden = value;
    return true;
  }
};

struct RecordingPlatformAppearanceHost final
    : VS::Field::MovingPlatformAppearanceHost {
  std::vector<std::string> calls;
  bool accept = true;
  std::optional<std::uint8_t> callback_tint;
  bool LoadPalette(bool frozen) override {
    calls.push_back(frozen ? "frozen" : "original");
    return accept;
  }
  bool SwapGroupColors(std::uint8_t group) override {
    calls.push_back("group" + std::to_string(group));
    return accept;
  }
  bool ToggleEffect(std::int32_t effect) override {
    calls.push_back("effect" + std::to_string(effect));
    return accept;
  }
  bool RefreshTint(VS::Field::MovingPlatformAppearanceState& state) override {
    if (callback_tint) state.tint = *callback_tint;
    return accept;
  }
  bool StoreTint(std::uint8_t tint) override {
    calls.push_back("tint" + std::to_string(tint));
    return accept;
  }
};

struct RecordingPlatformShadowHost final : VS::Field::MovingPlatformShadowHost {
  int begin_count = 0;
  int surface_count = 0;
  std::int32_t depth = 5;
  std::vector<VS::Field::MovingPlatformShadow> draws;
  void BeginView() override { ++begin_count; }
  VS::Field::MovingPlatformShadowSurface SurfaceFor(
      std::size_t, const VS::Field::MovingPlatformPose&) override {
    ++surface_count;
    return {.geometry_present = true, .floor_height = 256};
  }
  std::int32_t Project(const VS::Field::MovingPlatformShadow&) override {
    return depth;
  }
  void Draw(const VS::Field::MovingPlatformShadow& shadow,
            std::int32_t) override {
    draws.push_back(shadow);
  }
};

struct RecordingPlatformMotionHost final : VS::Field::MovingPlatformMotionHost {
  std::optional<bool> blocked = false;
  std::optional<std::uint8_t> callback_segment;
  std::optional<VS::Field::PlatformContact> candidate;
  std::optional<bool> IsObstructed(
      const VS::Field::PlatformContact& point,
      VS::Field::MovingPlatformMotionState& state) override {
    candidate = point;
    if (callback_segment) state.waypoint.current_segment = *callback_segment;
    return blocked;
  }
};

struct RecordingPlatformUpdateHost final : VS::Field::MovingPlatformUpdateHost {
  RecordingPlatformMotionHost motion;
  VS::Field::MovingPlatformRuntimeState runtime{};
  std::vector<bool> appearances;
  std::vector<VS::Field::MovingPlatformGeometryPlacement> geometry;
  std::optional<std::int32_t> next_tick_speed;
  std::optional<bool> IsObstructed(
      const VS::Field::PlatformContact& candidate,
      VS::Field::MovingPlatformMotionState& state) override {
    return motion.IsObstructed(candidate, state);
  }
  bool StoreRuntime(
      const VS::Field::MovingPlatformRuntimeState& state) override {
    runtime = state;
    return true;
  }
  bool RefreshFrame(VS::Field::MovingPlatformUpdateState& state) override {
    if (next_tick_speed) state.tick_speed = *next_tick_speed;
    return true;
  }
  bool SetFreezeAppearance(
      bool enabled, VS::Field::MovingPlatformUpdateState& state) override {
    appearances.push_back(enabled);
    state.freeze_tint_active = enabled;
    return true;
  }
  bool StorePlatform(std::size_t,
                     const VS::Field::MovingPlatformMotionState&) override {
    return true;
  }
  bool WriteGeometry(
      const VS::Field::MovingPlatformGeometryPlacement& placement) override {
    geometry.push_back(placement);
    return true;
  }
};

struct PlatformPlacementFixture {
  VS::Field::RoomDataRecord saved_room = VS::Field::roomData;
  decltype(VS::Field::platformFreezeFrames) saved_freeze =
      VS::Field::platformFreezeFrames;
  decltype(VS::Field::areMovingPlatformsPaused) saved_pause =
      VS::Field::areMovingPlatformsPaused;
  decltype(VS::Field::isPlatformFreezeTintActive) saved_tint =
      VS::Field::isPlatformFreezeTintActive;
  VS::Field::DoorTileLocation saved_entrance =
      VS::Field::entranceDoorRecord.tile_location;
  decltype(VS::Field::platformBounceFrames) saved_bounce =
      VS::Field::platformBounceFrames;
  std::array<VS::Field::MpdMovingPlatform, 2> platforms{};

  PlatformPlacementFixture() {
    VS::Field::roomData = {};
    VS::Field::roomData.moving_platforms = platforms.data();
    VS::Field::roomData.header.moving_platforms_length = sizeof(platforms);
    std::fill_n(VS::Field::platformBounceFrames.begin(), saved_bounce.size(),
                7);
    for (auto& platform : platforms) {
      platform.position_x = 1216 * ONE;
      platform.position_y = 768 * ONE;
      platform.position_z = 448 * ONE;
      platform.is_hidden = 1;
    }
  }

  ~PlatformPlacementFixture() {
    VS::Field::roomData = saved_room;
    VS::Field::platformFreezeFrames = saved_freeze;
    VS::Field::areMovingPlatformsPaused = saved_pause;
    VS::Field::isPlatformFreezeTintActive = saved_tint;
    VS::Field::entranceDoorRecord.tile_location = saved_entrance;
    std::copy(saved_bounce.begin(), saved_bounce.end(),
              VS::Field::platformBounceFrames.begin());
  }
};

struct PlatformMotionLiveFixture : PlatformPlacementFixture {
  decltype(VS::Battle::actors) saved_actors = VS::Battle::actors;
  decltype(VS::Actors::actorRecords) saved_models = VS::Actors::actorRecords;
  decltype(VS::State::tickSpeed) saved_tick_speed = VS::State::tickSpeed;
  decltype(VS::Field::areRoomUpdatesPaused) saved_room_pause =
      VS::Field::areRoomUpdatesPaused;
  decltype(VS::Render::frameBuf) saved_frame_buffer = VS::Render::frameBuf;
  PlatformMotionLiveFixture() {
    std::fill_n(VS::Battle::actors.begin(), saved_actors.size(), nullptr);
    std::fill_n(VS::Actors::actorRecords.begin(), saved_models.size(), nullptr);
    VS::Field::areRoomUpdatesPaused = 0;
    VS::Field::areMovingPlatformsPaused = 0;
    VS::Field::platformFreezeFrames = 0;
    VS::Field::isPlatformFreezeTintActive = 0;
    VS::State::tickSpeed = 2;
    VS::Render::frameBuf = 0;
    platforms[1].scene_object_id = 99;
  }
  ~PlatformMotionLiveFixture() {
    std::copy(saved_actors.begin(), saved_actors.end(),
              VS::Battle::actors.begin());
    std::copy(saved_models.begin(), saved_models.end(),
              VS::Actors::actorRecords.begin());
    VS::State::tickSpeed = saved_tick_speed;
    VS::Field::areRoomUpdatesPaused = saved_room_pause;
    VS::Render::frameBuf = saved_frame_buffer;
  }
};

struct PlatformShadowLiveFixture : PlatformPlacementFixture {
  VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  void* saved_buffer = VS::Render::legacyPrimitiveBuffers[0];
  void* saved_table = VS::Render::legacyOrderingTables[0];
  void* saved_overlay = VS::Render::legacyCurrentOrderingTable;
  int saved_frame_buffer = VS::Render::frameBuf;
  std::vector<u_long> storage = std::vector<u_long>(0x20000 / sizeof(u_long));
  std::vector<u_long> ordering = std::vector<u_long>(0x2088 / sizeof(u_long));
  GeometryWords geometry{1};
  PlatformShadowLiveFixture() {
    // Project with the GTE's own screen distance, not one a prior test left.
    Core_Gte_Reset();
    VS::Render::frameBuf = 0;
    VS::Render::legacyPrimitiveBuffers[0] = storage.data();
    VS::Render::legacyOrderingTables[0] = ordering.data();
    VS::Render::legacyCurrentOrderingTable = ordering.data();
    VS::Render::FrameScratchpad().primitive_cursor = storage.data();
    VS::Render::FrameScratchpad().ordering_table = ordering.data() + 18;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    VS::Render::FrameScratchpad().view_matrix = {};
    const SVECTOR angles{512, 0, 0, 0};
    VS::Core::Math::BuildLiveRotationXyz(
        &angles, &VS::Render::FrameScratchpad().view_matrix);
    VS::Render::FrameScratchpad().view_matrix.t[2] = 512;
    platforms[0].is_hidden = 0;
    platforms[0].position_x = platforms[0].position_z = 0;
    platforms[0].position_y = 100 * ONE;
    platforms[0].scene_object_id = 0;
    platforms[1].scene_object_id = 99;
    VS::Field::roomData.geometry = geometry.data();
    VS::Field::roomData.header.geometry_length = sizeof geometry;
  }
  std::int64_t ProjectedArea() const {
    const auto x = [](u_long word) {
      return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(word));
    };
    const auto y = [](u_long word) {
      return std::bit_cast<std::int16_t>(
          static_cast<std::uint16_t>(word >> 16));
    };
    return static_cast<std::int64_t>(x(storage[3]) - x(storage[2])) *
               (y(storage[4]) - y(storage[2])) -
           static_cast<std::int64_t>(y(storage[3]) - y(storage[2])) *
               (x(storage[4]) - x(storage[2]));
  }
  ~PlatformShadowLiveFixture() {
    VS::Render::FrameScratchpad() = saved_frame;
    VS::Render::legacyPrimitiveBuffers[0] = saved_buffer;
    VS::Render::legacyOrderingTables[0] = saved_table;
    VS::Render::legacyCurrentOrderingTable = saved_overlay;
    VS::Render::frameBuf = saved_frame_buffer;
    Core_Gte_SetRotMatrix(&saved_frame.view_matrix);
    Core_Gte_SetTransMatrix(&saved_frame.view_matrix);
  }
};

struct PlatformAppearanceGeometry {
  std::uint32_t groups = 1;
  std::array<std::byte, VS::Render::kRoomGroupRecordBytes> group{};
  std::uint32_t triangles = 0;
  std::uint32_t quads = 1;
  VS::Field::MpdQuad quad{};
};

struct PlatformAppearanceLiveFixture : PlatformPlacementFixture {
  VS::Render::PaletteAnimation saved_palette =
      VS::Render::LivePaletteAnimation();
  u_short* saved_room_palettes = VS::Field::roomPalettes;
  std::array<std::uint16_t, 0x240> palettes{};
  PlatformAppearanceGeometry geometry;
  PlatformAppearanceLiveFixture() {
    constexpr std::size_t kPolygonOffsetWord = 7;
    const auto block = static_cast<std::int32_t>(
        offsetof(PlatformAppearanceGeometry, triangles));
    std::memcpy(geometry.group.data() + kPolygonOffsetWord * sizeof(block),
                &block, sizeof(block));
    geometry.quad.red1 = 0x12;
    geometry.quad.green1 = 0x34;
    geometry.quad.blue1 = 0x56;
    palettes.fill(0x1234);
    VS::Field::roomPalettes = palettes.data();
    VS::Field::roomData.geometry = reinterpret_cast<int*>(&geometry);
    VS::Field::roomData.header.geometry_length = sizeof geometry;
    platforms[0].scene_object_id = 0;
    platforms[0].swaps_colors_when_frozen = 3;
    platforms[1].scene_object_id = 99;
    VS::Field::isPlatformFreezeTintActive = 0;
  }
  ~PlatformAppearanceLiveFixture() {
    const_cast<VS::Render::PaletteAnimation&>(
        VS::Render::LivePaletteAnimation()) = saved_palette;
    VS::Field::roomPalettes = saved_room_palettes;
  }
};
}  // namespace

namespace {
void CheckAuthoredPlacement(const VS::Field::PlatformContact& contact) {
  CHECK(contact.x == 1216);
  CHECK(contact.height == -768);
  CHECK(contact.z == 448);
  CHECK(contact.platform_index == 1);
}
}  // namespace

TEST_CASE("Platform initialization resets runtime for an absent room",
          "[platform-initialization-core][field]") {
  const auto state = VS::Field::InitializeMovingPlatforms({}, 0);
  CHECK(state.runtime.paused == 0);
  CHECK(state.runtime.freeze_frames == 0);
  CHECK_FALSE(state.freeze_tint_active);
  CHECK(std::ranges::all_of(state.bounce_frames,
                            [](auto frame) { return frame == 7; }));
}

TEST_CASE("Platform initialization converts hidden geometry toward zero",
          "[platform-initialization-core][field]") {
  const std::array<VS::Field::MovingPlatformPose, 1> platforms{
      VS::Field::MovingPlatformPose{
          .x = -4097, .y = -8191, .z = 8193, .hidden = true}};
  const auto state = VS::Field::InitializeMovingPlatforms(
      {.platforms = platforms, .bounce_offsets = {}}, 0);
  REQUIRE(state.geometry.size() == platforms.size());
  CHECK(state.geometry[0].platform_index == 0);
  CHECK(state.geometry[0].hide);
  CHECK(state.geometry[0].x == -1);
  CHECK(state.geometry[0].y == 1);
  CHECK(state.geometry[0].z == 2);
}

TEST_CASE("Platform initialization converts extreme visible positions",
          "[platform-initialization-core][field]") {
  const std::array<VS::Field::MovingPlatformPose, 1> platforms{
      VS::Field::MovingPlatformPose{
          .x = std::numeric_limits<std::int32_t>::min(),
          .y = std::numeric_limits<std::int32_t>::max(),
          .z = -4095}};
  const auto state = VS::Field::InitializeMovingPlatforms(
      {.platforms = platforms, .bounce_offsets = {}}, 0);
  REQUIRE(state.geometry.size() == platforms.size());
  CHECK_FALSE(state.geometry[0].hide);
  CHECK(state.geometry[0].x == -524288);
  CHECK(state.geometry[0].y == -524287);
  CHECK(state.geometry[0].z == 0);
}

TEST_CASE("Platform initialization retains every entrance layer flag",
          "[platform-initialization-core][field]") {
  for (std::uint32_t flags = 0; flags <= 65535; ++flags) {
    const auto state = VS::Field::InitializeMovingPlatforms(
        {}, static_cast<std::uint16_t>(flags));
    const auto layer = static_cast<int>(flags >> 13);
    CHECK(state.entrance_platform ==
          (layer >= 2 ? std::optional<int>(layer - 2) : std::nullopt));
    CHECK(state.geometry.empty());
  }
}

TEST_CASE("Native platform initialization resets only its four bounce slots",
          "[platform-initialization-live][field]") {
  PlatformPlacementFixture fixture;
  VS::Field::entranceDoorRecord.tile_location = {};
  VS::Field::areMovingPlatformsPaused = 255;
  VS::Field::platformFreezeFrames = -1;
  VS::Field::isPlatformFreezeTintActive = 1;
  std::fill_n(VS::Field::platformBounceFrames.begin(),
              VS::Field::kMovingPlatformCapacity, 3);
  VS::Field::platformBounceFrames[VS::Field::kMovingPlatformCapacity] = 19;
  VS::Field::roomData.moving_platforms = nullptr;
  VS::Field::InitializeMovingPlatformsEntry();
  CHECK(VS::Field::areMovingPlatformsPaused == 0);
  CHECK(VS::Field::platformFreezeFrames == 0);
  CHECK(VS::Field::isPlatformFreezeTintActive == 0);
  CHECK(std::ranges::all_of(std::span(VS::Field::platformBounceFrames)
                                .first(VS::Field::kMovingPlatformCapacity),
                            [](auto frame) { return frame == 7; }));
  CHECK(VS::Field::platformBounceFrames[VS::Field::kMovingPlatformCapacity] ==
        19);
}

TEST_CASE("Native platform initialization preserves packed records and flags",
          "[platform-initialization-live][field]") {
  PlatformPlacementFixture fixture;
  VS::Field::entranceDoorRecord.tile_location = {};
  fixture.platforms[0].scene_object_id = 0;
  fixture.platforms[1].scene_object_id = 99;
  GeometryWords geometry{1, 0x12345678, 0, 0, 0, 7, 8, 9, 10};
  VS::Field::roomData.geometry = geometry.data();
  VS::Field::roomData.header.geometry_length = sizeof geometry;
  const auto saved = fixture.platforms;
  VS::Field::InitializeMovingPlatformsEntry();
  using Bytes = std::array<std::byte, sizeof(saved)>;
  CHECK(std::bit_cast<Bytes>(fixture.platforms) == std::bit_cast<Bytes>(saved));
  CHECK(geometry == GeometryWords{1, 0x12345778, 1216, -768, 448, 7, 8, 9, 10});
}

TEST_CASE("Native platform initialization resets its entrance waypoint last",
          "[platform-initialization-live][field]") {
  PlatformPlacementFixture fixture;
  VS::Field::entranceDoorRecord.tile_location = {};
  VS::Field::entranceDoorRecord.tile_location.entry_direction = 2;
  auto& platform = fixture.platforms[0];
  platform.terminal_waypoint = 2;
  platform.waypoints[2] =
      VECTOR{.vx = -4097, .vy = -8191, .vz = 8193, .pad = 0};
  GeometryWords geometry{1, 0x100, 0, 0, 0};
  VS::Field::roomData.geometry = geometry.data();
  VS::Field::roomData.header.geometry_length = sizeof geometry;
  VS::Field::InitializeMovingPlatformsEntry();
  CHECK(platform.position_x == -4097);
  CHECK(platform.position_y == -8191);
  CHECK(platform.position_z == 8193);
  CHECK(platform.is_hidden == 0);
  CHECK(geometry == GeometryWords{1, 0x100, -1, 1, 2});
}

TEST_CASE("Platform initialization retains its original module address",
          "[platform-initialization-live][field]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008C538) ==
        reinterpret_cast<const void*>(
            &VS::Field::InitializeMovingPlatformsEntry));
}

TEST_CASE("Platform collection preserves visible order and original indices",
          "[platform-collection-core][field]") {
  const std::array<std::int16_t, 2> bounce{-2, 5};
  const std::array<VS::Field::MovingPlatformPose, 3> platforms{
      VS::Field::MovingPlatformPose{.bounce_frame = 255, .hidden = true},
      {.x = -4097, .y = -8191, .z = 8193, .bounce_frame = 1},
      {.x = 4095, .y = 4096, .z = -4095, .bounce_frame = 2}};
  const auto contacts = VS::Field::CollectMovingPlatformPositions(
      {.platforms = platforms, .bounce_offsets = bounce});
  CHECK(contacts ==
        std::vector<VS::Field::PlatformContact>{{-1, 6, 2, 1}, {0, -1, 0, 2}});
}

TEST_CASE("Platform collection retains every wrapped coordinate halfword",
          "[platform-collection-core][field]") {
  std::array<VS::Field::MovingPlatformPose, 1> platforms{};
  for (std::uint32_t bits = 0; bits <= 65535; ++bits) {
    platforms[0].x = static_cast<std::int32_t>(bits) * 4096;
    const auto contacts = VS::Field::CollectMovingPlatformPositions(
        {.platforms = platforms, .bounce_offsets = {}});
    REQUIRE(contacts.size() == 1);
    CHECK(contacts[0].x ==
          std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(bits)));
  }
}

TEST_CASE("Platform collection refuses bad bounce and retains five records",
          "[platform-collection-core][field]") {
  std::array<VS::Field::MovingPlatformPose, 5> platforms{};
  const std::array<std::int16_t, 1> bounce{3};
  platforms[0].bounce_frame = 2;
  CHECK(VS::Field::CollectMovingPlatformPositions(
            {.platforms = std::span(platforms).first(1),
             .bounce_offsets = bounce})
            .empty());
  platforms[0].bounce_frame = 0;
  const auto contacts = VS::Field::CollectMovingPlatformPositions(
      {.platforms = platforms, .bounce_offsets = bounce});
  REQUIRE(contacts.size() == platforms.size());
  CHECK(contacts.back().platform_index == 4);
}

TEST_CASE("Native platform collection preserves unused output and records",
          "[platform-collection-live][field]") {
  PlatformPlacementFixture fixture;
  fixture.platforms[1].is_hidden = 0;
  const auto saved = fixture.platforms;
  std::array<VS::Field::PlatformContact, 3> contacts{
      VS::Field::PlatformContact{7, 8, 9, 10}, {7, 8, 9, 10}, {7, 8, 9, 10}};
  CHECK(VS::Field::CollectMovingPlatformPositionsEntry(contacts.data()) == 1);
  using ContactBytes = std::array<std::byte, sizeof(contacts)>;
  const decltype(contacts) expected{
      VS::Field::PlatformContact{1216, -768, 448, 1},
      {7, 8, 9, 10},
      {7, 8, 9, 10}};
  CHECK(std::bit_cast<ContactBytes>(contacts) ==
        std::bit_cast<ContactBytes>(expected));
  using RecordBytes = std::array<std::byte, sizeof(saved)>;
  CHECK(std::bit_cast<RecordBytes>(fixture.platforms) ==
        std::bit_cast<RecordBytes>(saved));
  CHECK(VS::Field::CollectMovingPlatformPositionsEntry(nullptr) == 0);
}

TEST_CASE("Native platform collection refuses partial and misaligned records",
          "[platform-collection-live][field]") {
  PlatformPlacementFixture fixture;
  VS::Field::roomData.header.moving_platforms_length =
      sizeof(fixture.platforms) - 1;
  CHECK(VS::Field::CollectLiveMovingPlatformPositions().empty());
  VS::Field::roomData.header.moving_platforms_length =
      sizeof(fixture.platforms);
  auto* bytes = reinterpret_cast<std::byte*>(fixture.platforms.data());
  VS::Field::roomData.moving_platforms =
      reinterpret_cast<VS::Field::MpdMovingPlatform*>(bytes + 1);
  CHECK(VS::Field::CollectLiveMovingPlatformPositions().empty());
}

TEST_CASE("Platform collection retains its original module address",
          "[platform-collection-live][field]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008D2C0) ==
        reinterpret_cast<const void*>(
            &VS::Field::CollectMovingPlatformPositionsEntry));
}

TEST_CASE("Typed collection supports five records without legacy buffer writes",
          "[platform-collection-live][field]") {
  PlatformPlacementFixture fixture;
  std::array<VS::Field::MpdMovingPlatform, 5> platforms{};
  VS::Field::roomData.moving_platforms = platforms.data();
  VS::Field::roomData.header.moving_platforms_length = sizeof platforms;
  CHECK(VS::Field::CollectLiveMovingPlatformPositions().size() == 5);
  std::array<VS::Field::PlatformContact, 4> output{};
  const auto saved = output;
  CHECK(VS::Field::CollectMovingPlatformPositionsEntry(output.data()) == 0);
  using Bytes = std::array<std::byte, sizeof(output)>;
  CHECK(std::bit_cast<Bytes>(output) == std::bit_cast<Bytes>(saved));
}

TEST_CASE(
    "Initialization positions a fifth platform without clearing its bounce",
    "[platform-initialization-live][field]") {
  PlatformPlacementFixture fixture;
  std::array<VS::Field::MpdMovingPlatform, 5> platforms{};
  for (auto& platform : platforms) platform.scene_object_id = 99;
  platforms.back().scene_object_id = 0;
  platforms.back().position_x = 8193;
  VS::Field::platformBounceFrames[4] = 19;
  VS::Field::entranceDoorRecord.tile_location = {};
  VS::Field::roomData.moving_platforms = platforms.data();
  VS::Field::roomData.header.moving_platforms_length = sizeof platforms;
  GeometryWords geometry{1};
  VS::Field::roomData.geometry = geometry.data();
  VS::Field::roomData.header.geometry_length = sizeof geometry;
  VS::Field::InitializeMovingPlatformsEntry();
  CHECK(geometry == GeometryWords{1, 0, 2});
  CHECK(VS::Field::platformBounceFrames[4] == 19);
}

TEST_CASE("Entrance initialization resets the sixth platform terminal waypoint",
          "[platform-initialization-live][field]") {
  PlatformPlacementFixture fixture;
  std::array<VS::Field::MpdMovingPlatform, 6> platforms{};
  VS::Field::entranceDoorRecord.tile_location = {};
  VS::Field::entranceDoorRecord.tile_location.entry_direction = 7;
  auto& platform = platforms.back();
  platform.is_hidden = 1;
  platform.terminal_waypoint = 2;
  platform.waypoints[2] = VECTOR{.vx = -4097, .vy = 0, .vz = 0, .pad = 0};
  VS::Field::roomData.moving_platforms = platforms.data();
  VS::Field::roomData.header.moving_platforms_length = sizeof platforms;
  GeometryWords geometry{1};
  VS::Field::roomData.geometry = geometry.data();
  VS::Field::roomData.header.geometry_length = sizeof geometry;
  VS::Field::InitializeMovingPlatformsEntry();
  CHECK(platform.position_x == -4097);
  CHECK(platform.is_hidden == 0);
  CHECK(geometry == GeometryWords{1, 0x100, -1});
}

TEST_CASE("Platform motion reaches and docks at its exact terminal waypoint",
          "[platform-motion-core][field]") {
  RecordingPlatformMotionHost host;
  VS::Field::MovingPlatformMotionState state{
      .waypoint = {.current_waypoint = 1, .terminal_waypoint = 1},
      .bounce_frame = 6};
  const std::array<VS::Field::MovingPlatformWaypoint, 2> points{
      VS::Field::MovingPlatformWaypoint{}, {13, 17, 19, 7}};
  const std::array<VS::Field::MovingPlatformWaypoint, 1> velocities{
      VS::Field::MovingPlatformWaypoint{4096, -8192, 12288, 1}};
  REQUIRE(VS::Field::AdvanceMovingPlatformMotion(state, points, velocities, 2,
                                                 host));
  CHECK(state.bounce_frame == 7);
  CHECK(state.waypoint.waiting);
  CHECK(state.waypoint.motion_ticks == 0);
  CHECK(state.docked);
  CHECK(host.candidate == VS::Field::PlatformContact{1, 2, 3, 0});
  CHECK(state.waypoint.x == 13);
}

TEST_CASE("Platform obstruction rolls back with the callback-selected segment",
          "[platform-motion-core][field]") {
  RecordingPlatformMotionHost host;
  host.blocked = true;
  host.callback_segment = 1;
  VS::Field::MovingPlatformMotionState state{.bounce_frame = 0};
  const std::array<VS::Field::MovingPlatformWaypoint, 1> points{};
  const std::array<VS::Field::MovingPlatformWaypoint, 2> velocities{
      VS::Field::MovingPlatformWaypoint{4096, 0, 0, 4}, {8192, 0, 0, 4}};
  REQUIRE(VS::Field::AdvanceMovingPlatformMotion(state, points, velocities, 1,
                                                 host));
  CHECK(state.waypoint.x == -4096);
  CHECK(state.waypoint.motion_ticks == 0);
  CHECK(state.bounce_frame == 1);
}

TEST_CASE(
    "Platform dwell departure preserves packed endpoint and segment wraps",
    "[platform-motion-core][field]") {
  RecordingPlatformMotionHost host;
  VS::Field::MovingPlatformMotionState state{
      .waypoint = {.waiting = true,
                   .moving_forward = true,
                   .current_waypoint = 3,
                   .current_segment = 7,
                   .end_waypoint = 3,
                   .motion_ticks = 2},
      .bounce_frame = 7,
      .docked = true};
  const std::array<VS::Field::MovingPlatformWaypoint, 4> points{};
  REQUIRE(VS::Field::AdvanceMovingPlatformMotion(state, points, {}, 1, host));
  CHECK_FALSE(state.waypoint.waiting);
  CHECK_FALSE(state.waypoint.moving_forward);
  CHECK(state.waypoint.current_segment == 0);
  CHECK(state.waypoint.current_waypoint == 2);
  CHECK_FALSE(state.docked);
}

TEST_CASE("Platform motion wraps position and unsigned halfword counters",
          "[platform-motion-core][field]") {
  RecordingPlatformMotionHost host;
  VS::Field::MovingPlatformMotionState state{
      .waypoint = {.motion_ticks = 65535,
                   .x = std::numeric_limits<std::int32_t>::max()},
      .bounce_frame = 6};
  const std::array<VS::Field::MovingPlatformWaypoint, 1> points{};
  const std::array<VS::Field::MovingPlatformWaypoint, 1> velocities{
      VS::Field::MovingPlatformWaypoint{1, 0, 0, 1}};
  REQUIRE(VS::Field::AdvanceMovingPlatformMotion(state, points, velocities, 250,
                                                 host));
  CHECK(state.waypoint.x == std::numeric_limits<std::int32_t>::min());
  CHECK(state.waypoint.motion_ticks == 0);
  CHECK_FALSE(state.waypoint.waiting);
  CHECK(state.bounce_frame == 0);
}

TEST_CASE("Platform motion refuses missing motion data without mutation",
          "[platform-motion-core][field]") {
  RecordingPlatformMotionHost host;
  VS::Field::MovingPlatformMotionState state{};
  const auto saved = state;
  CHECK_FALSE(VS::Field::AdvanceMovingPlatformMotion(state, {}, {}, 1, host));
  CHECK(state == saved);
}

TEST_CASE("Frozen platform update flashes before clearing expired duration",
          "[platform-motion-core][field]") {
  RecordingPlatformUpdateHost host;
  VS::Field::MovingPlatformUpdateState state{
      .runtime = {.paused = 0, .freeze_frames = 1},
      .tick_speed = 4,
      .platform_section_present = true,
      .frame_buffer_nonzero = true};
  REQUIRE(VS::Field::UpdateMovingPlatforms(state, {}, {}, host));
  CHECK(host.appearances == std::vector<bool>{true, false});
  CHECK(state.runtime.freeze_frames == 0);
  CHECK(host.runtime.freeze_frames == 0);
  CHECK(host.geometry.empty());
}

TEST_CASE("Paused room platform update does not advance freeze or geometry",
          "[platform-motion-core][field]") {
  RecordingPlatformUpdateHost host;
  VS::Field::MovingPlatformUpdateState state{
      .runtime = {.paused = 0, .freeze_frames = 31},
      .tick_speed = 4,
      .room_updates_paused = true,
      .platform_section_present = true};
  REQUIRE(VS::Field::UpdateMovingPlatforms(state, {}, {}, host));
  CHECK(state.runtime.freeze_frames == 31);
  CHECK(host.appearances.empty());
  CHECK(host.geometry.empty());
}

TEST_CASE("Platform update applies full tick increment on every substep",
          "[platform-motion-core][field]") {
  RecordingPlatformUpdateHost host;
  const std::array<VS::Field::MovingPlatformWaypoint, 1> points{};
  const std::array<VS::Field::MovingPlatformWaypoint, 1> velocities{
      VS::Field::MovingPlatformWaypoint{4096, 0, 0, 10}};
  std::array<VS::Field::MovingPlatformMotionRecord, 1> platforms{
      VS::Field::MovingPlatformMotionRecord{
          .state = {}, .waypoints = points, .velocities = velocities}};
  const std::array<std::int16_t, 7> bounce{0, 0, 0, 0, 11, 0, 0};
  VS::Field::MovingPlatformUpdateState state{.tick_speed = 4,
                                             .platform_section_present = true};
  REQUIRE(VS::Field::UpdateMovingPlatforms(state, platforms, bounce, host));
  CHECK(platforms[0].state.bounce_frame == 4);
  CHECK(platforms[0].state.waypoint.motion_ticks == 2);
  CHECK(platforms[0].state.waypoint.x == 8192);
  REQUIRE(host.geometry.size() == 1);
  CHECK(host.geometry[0].y == 11);
}

TEST_CASE("Looping detached platforms skip motion but still place geometry",
          "[platform-motion-core][field]") {
  RecordingPlatformUpdateHost host;
  std::array<VS::Field::MovingPlatformMotionRecord, 1> platforms{
      VS::Field::MovingPlatformMotionRecord{
          .state = {.waypoint = {.control_state = 127, .x = 4096},
                    .bounce_frame = 7,
                    .looping = true},
          .waypoints = {},
          .velocities = {}}};
  VS::Field::MovingPlatformUpdateState state{.tick_speed = 4,
                                             .platform_section_present = true};
  const std::array<std::int16_t, 7> bounce{};
  REQUIRE(VS::Field::UpdateMovingPlatforms(state, platforms, bounce, host));
  REQUIRE(host.geometry.size() == 1);
  CHECK(host.geometry[0].x == 1);
  CHECK_FALSE(host.motion.candidate.has_value());
}

TEST_CASE("Platform update rereads tick speed after the obstruction callback",
          "[platform-motion-core][field]") {
  RecordingPlatformUpdateHost host;
  host.next_tick_speed = 0;
  const std::array<VS::Field::MovingPlatformWaypoint, 1> points{};
  const std::array<VS::Field::MovingPlatformWaypoint, 1> velocities{
      VS::Field::MovingPlatformWaypoint{4096, 0, 0, 10}};
  std::array<VS::Field::MovingPlatformMotionRecord, 1> platforms{
      VS::Field::MovingPlatformMotionRecord{
          .state = {}, .waypoints = points, .velocities = velocities}};
  VS::Field::MovingPlatformUpdateState state{.tick_speed = 4,
                                             .platform_section_present = true};
  const std::array<std::int16_t, 7> bounce{};
  REQUIRE(VS::Field::UpdateMovingPlatforms(state, platforms, bounce, host));
  CHECK(platforms[0].state.waypoint.motion_ticks == 1);
  CHECK(platforms[0].state.waypoint.x == 4096);
}

TEST_CASE("Native platform motion preserves packed data and bounce geometry",
          "[platform-motion-live][field]") {
  PlatformMotionLiveFixture fixture;
  VS::State::tickSpeed = 4;
  VS::Field::platformBounceFrames[0] = 0;
  auto& platform = fixture.platforms[0];
  platform.is_hidden = 0;
  platform.segment_velocities[0] =
      VECTOR{.vx = 4096, .vy = 0, .vz = 0, .pad = 10};
  auto expected = platform;
  expected.position_x += 8192;
  expected.motion_ticks = 2;
  GeometryWords geometry{1, 0x100, 0, 0, 0, 7, 8, 9, 10};
  VS::Field::roomData.geometry = geometry.data();
  VS::Field::roomData.header.geometry_length = sizeof geometry;
  REQUIRE(VS::Field::UpdateLiveMovingPlatforms());
  using Bytes = std::array<std::byte, sizeof(expected)>;
  CHECK(std::bit_cast<Bytes>(platform) == std::bit_cast<Bytes>(expected));
  CHECK(VS::Field::platformBounceFrames[0] == 4);
  CHECK(geometry == GeometryWords{1, 0, 1218,
                                  VS::Field::kPlatformBounceOffsets[4] - 768,
                                  448, 7, 8, 9, 10});
}

TEST_CASE("Native platform freeze expires before any motion substep",
          "[platform-motion-live][field]") {
  PlatformMotionLiveFixture fixture;
  fixture.platforms[0].is_hidden = 0;
  VS::Field::platformFreezeFrames = 1;
  const auto saved = fixture.platforms;
  REQUIRE(VS::Field::UpdateLiveMovingPlatforms());
  CHECK(VS::Field::platformFreezeFrames == 0);
  using Bytes = std::array<std::byte, sizeof(saved)>;
  CHECK(std::bit_cast<Bytes>(fixture.platforms) == std::bit_cast<Bytes>(saved));
}

TEST_CASE("Native platform motion refuses partial tables without state writes",
          "[platform-motion-live][field]") {
  PlatformMotionLiveFixture fixture;
  const auto saved = fixture.platforms;
  VS::Field::roomData.header.moving_platforms_length =
      sizeof(fixture.platforms) - 1;
  CHECK_FALSE(VS::Field::UpdateLiveMovingPlatforms());
  using Bytes = std::array<std::byte, sizeof(saved)>;
  CHECK(std::bit_cast<Bytes>(fixture.platforms) == std::bit_cast<Bytes>(saved));
}

TEST_CASE("Platform motion retains its original module address",
          "[platform-motion-live][field]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008C8A8) ==
        reinterpret_cast<const void*>(&VS::Field::UpdateMovingPlatformsEntry));
}

TEST_CASE("Platform shadow preserves all original pit surface choices",
          "[platform-shadow-core][field]") {
  const VS::Field::MovingPlatformPose platform{.y = 4096};
  for (std::uint8_t kind = 0; kind < 32; ++kind) {
    const auto shadow = VS::Field::BuildMovingPlatformShadow(
        platform, {.geometry_present = true,
                   .pit = true,
                   .surface_kind = kind,
                   .floor_height = 0});
    CHECK(shadow.has_value() == (kind >= 15 && kind < 19));
  }
}

TEST_CASE("Platform shadow converts every signed fifteen-bit floor word",
          "[platform-shadow-core][field]") {
  const VS::Field::MovingPlatformPose platform{.y = 16385 * 4096};
  for (std::uint32_t word = 0; word <= 65535; ++word) {
    const auto shadow = VS::Field::BuildMovingPlatformShadow(
        platform, {.geometry_present = true,
                   .floor_height = static_cast<std::uint16_t>(word)});
    REQUIRE(shadow.has_value());
    if (!shadow) return;
    int expected = static_cast<int>(word & 0x7FFF);
    if (expected >= 0x4000) expected -= 0x8000;
    CHECK(shadow->vertices[0].y == expected);
  }
}

TEST_CASE("Platform shadow shade and corners follow original altitude fade",
          "[platform-shadow-core][field]") {
  const auto shadow = VS::Field::BuildMovingPlatformShadow(
      {.x = 4096, .y = -4095, .z = -4097},
      {.geometry_present = true, .floor_height = 256});
  REQUIRE(shadow.has_value());
  if (!shadow) return;
  CHECK(shadow->shade == 79);
  CHECK(shadow->vertices ==
        std::array<VS::Field::MovingPlatformShadowVertex, 4>{
            VS::Field::MovingPlatformShadowVertex{-46, 256, -48},
            {48, 256, -48},
            {-46, 256, 46},
            {48, 256, 46}});
}

TEST_CASE("Platform shadow suppresses hidden absent and floor-equal platforms",
          "[platform-shadow-core][field]") {
  CHECK_FALSE(VS::Field::BuildMovingPlatformShadow({}, {}).has_value());
  CHECK_FALSE(
      VS::Field::BuildMovingPlatformShadow(
          {.hidden = true}, {.geometry_present = true, .floor_height = 1})
          .has_value());
  CHECK_FALSE(VS::Field::BuildMovingPlatformShadow(
                  {}, {.geometry_present = true, .floor_height = 0})
                  .has_value());
}

TEST_CASE(
    "Platform shadow drawing retains view setup and exclusive depth bounds",
    "[platform-shadow-core][field]") {
  for (const auto depth : {-1, 0, 4, 5, 2047, 2048}) {
    RecordingPlatformShadowHost host;
    host.depth = depth;
    const std::array<VS::Field::MovingPlatformPose, 2> platforms{
        VS::Field::MovingPlatformPose{.hidden = true}, {}};
    VS::Field::DrawMovingPlatformShadows(platforms, host);
    CHECK(host.begin_count == 1);
    CHECK(host.surface_count == 1);
    CHECK(host.draws.size() == (depth > 4 && depth < 2048 ? 1 : 0));
  }
}

TEST_CASE("Native platform shadows emit translucent quad then subtractive mode",
          "[platform-shadow-live][field]") {
  PlatformShadowLiveFixture fixture;
  const auto saved = fixture.platforms;
  const auto geometry = fixture.geometry;
  VS::Field::DrawMovingPlatformShadowsEntry();
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        fixture.storage.data() + 8);
  CHECK((fixture.storage[0] >> 24) == 5);
  CHECK((fixture.storage[1] >> 24) == 0x2A);
  CHECK(fixture.storage[7] == 0xE1000240);
  using Bytes = std::array<std::byte, sizeof(saved)>;
  CHECK(std::bit_cast<Bytes>(fixture.platforms) == std::bit_cast<Bytes>(saved));
  CHECK(fixture.geometry == geometry);
}

TEST_CASE("Native platform shadows skip hidden records without packet writes",
          "[platform-shadow-live][field]") {
  PlatformShadowLiveFixture fixture;
  fixture.platforms[0].is_hidden = 1;
  VS::Field::DrawMovingPlatformShadowsEntry();
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        fixture.storage.data());
}

TEST_CASE("Native platform shadows project a nondegenerate shaded footprint",
          "[platform-shadow-live][field]") {
  PlatformShadowLiveFixture fixture;
  VS::Field::DrawMovingPlatformShadowsEntry();
  CHECK(fixture.ProjectedArea() != 0);
  CHECK(fixture.storage[1] == 0x2A767676);
}

TEST_CASE("Native platform shadows refuse a full primitive buffer",
          "[platform-shadow-live][field]") {
  PlatformShadowLiveFixture fixture;
  VS::Render::FrameScratchpad().primitive_cursor =
      fixture.storage.data() + fixture.storage.size();
  const auto dropped = VS::Render::DroppedLiveUiPacketCount();
  VS::Field::DrawMovingPlatformShadowsEntry();
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        fixture.storage.data() + fixture.storage.size());
  CHECK(VS::Render::DroppedLiveUiPacketCount() == dropped + 1);
}

TEST_CASE("Native platform shadows refuse when their draw mode cannot fit",
          "[platform-shadow-capacity][platform-shadow-live][field]") {
  PlatformShadowLiveFixture fixture;
  auto* cursor = fixture.storage.data() + fixture.storage.size() - 6;
  VS::Render::FrameScratchpad().primitive_cursor = cursor;
  const auto saved_storage = fixture.storage;
  const auto saved_ordering = fixture.ordering;
  const auto dropped = VS::Render::DroppedLiveUiPacketCount();
  VS::Field::DrawMovingPlatformShadowsEntry();
  CHECK(VS::Render::FrameScratchpad().primitive_cursor == cursor);
  CHECK(VS::Render::DroppedLiveUiPacketCount() == dropped + 1);
  CHECK(std::ranges::equal(fixture.storage, saved_storage));
  CHECK(std::ranges::equal(fixture.ordering, saved_ordering));
}

TEST_CASE("Platform shadows retain their original module address",
          "[platform-shadow-live][field]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008CEB0) ==
        reinterpret_cast<const void*>(
            &VS::Field::DrawMovingPlatformShadowsEntry));
}

TEST_CASE(
    "Freeze appearance preserves palette group effect and final tint order",
    "[platform-appearance-core][field]") {
  RecordingPlatformAppearanceHost host;
  VS::Field::MovingPlatformAppearanceState state{};
  const std::array<VS::Field::MovingPlatformAppearanceTarget, 3> targets{
      VS::Field::MovingPlatformAppearanceTarget{7, true},
      {8, false},
      {9, true}};
  REQUIRE(VS::Field::SetPlatformFreezeAppearance(state, 1, targets, 4, host));
  CHECK(host.calls == std::vector<std::string>{"frozen", "group7", "group9",
                                               "effect4", "tint1"});
  CHECK(state.tint == 1);
}

TEST_CASE(
    "Freeze appearance compares the full request before boolean palette "
    "selection",
    "[platform-appearance-core][field]") {
  for (const auto request : {-1, 0, 1, 2, 255, 256, 65536}) {
    RecordingPlatformAppearanceHost host;
    VS::Field::MovingPlatformAppearanceState state{1};
    REQUIRE(VS::Field::SetPlatformFreezeAppearance(state, request, {},
                                                   std::nullopt, host));
    if (request == 1) {
      CHECK(host.calls.empty());
      CHECK(state.tint == 1);
    } else {
      CHECK(host.calls.front() == (request == 0 ? "original" : "frozen"));
      CHECK(state.tint == 0);
    }
  }
}

TEST_CASE("Freeze appearance flips the byte reread after effect callbacks",
          "[platform-appearance-core][field]") {
  for (std::uint32_t tint = 0; tint <= 255; ++tint) {
    RecordingPlatformAppearanceHost host;
    host.callback_tint = static_cast<std::uint8_t>(tint);
    VS::Field::MovingPlatformAppearanceState state{};
    REQUIRE(VS::Field::SetPlatformFreezeAppearance(state, 1, {}, 0, host));
    CHECK(state.tint == (tint == 0 ? 1 : 0));
  }
}

TEST_CASE("Freeze appearance skips negative effects and stops on host refusal",
          "[platform-appearance-core][field]") {
  RecordingPlatformAppearanceHost host;
  VS::Field::MovingPlatformAppearanceState state{};
  REQUIRE(VS::Field::SetPlatformFreezeAppearance(state, 1, {}, -1, host));
  CHECK(host.calls == std::vector<std::string>{"frozen", "tint1"});
  host.calls.clear();
  host.accept = false;
  CHECK_FALSE(VS::Field::SetPlatformFreezeAppearance(state, 0, {}, 3, host));
  CHECK(state.tint == 1);
  CHECK(host.calls == std::vector<std::string>{"original"});
}

TEST_CASE(
    "Native freeze appearance loads original frozen palette and swaps colors",
    "[platform-appearance-live][field]") {
  PlatformAppearanceLiveFixture fixture;
  const auto saved = fixture.platforms;
  const std::array<std::uint16_t, 16> expected{
      0x0010, 0x0001, 0x0865, 0x0C66, 0x0869, 0x0CA9, 0x088D, 0x10AD,
      0x0CB1, 0x14D1, 0x0CD5, 0x14F5, 0x0CF9, 0x113A, 0x199B, 0x21FC};
  REQUIRE(VS::Field::SetLivePlatformFreezeAppearance(1));
  CHECK(
      std::equal(expected.begin(), expected.end(),
                 VS::Render::LivePaletteAnimation().colors[2].begin() + 0x30));
  CHECK(fixture.geometry.quad.red1 == 0x34);
  CHECK(fixture.geometry.quad.green1 == 0x12);
  CHECK(fixture.geometry.quad.blue1 == 0x56);
  CHECK(VS::Field::isPlatformFreezeTintActive == 1);
  using Bytes = std::array<std::byte, sizeof(saved)>;
  CHECK(std::bit_cast<Bytes>(fixture.platforms) == std::bit_cast<Bytes>(saved));
}

TEST_CASE("Native freeze appearance restores room palette and group colors",
          "[platform-appearance-live][field]") {
  PlatformAppearanceLiveFixture fixture;
  REQUIRE(VS::Field::SetLivePlatformFreezeAppearance(1));
  REQUIRE(VS::Field::SetLivePlatformFreezeAppearance(0));
  CHECK(VS::Render::LivePaletteAnimation().colors[2][0x30] == 0x1234);
  CHECK(fixture.geometry.quad.red1 == 0x12);
  CHECK(fixture.geometry.quad.green1 == 0x34);
  CHECK(VS::Field::isPlatformFreezeTintActive == 0);
}

TEST_CASE("Native freeze appearance equality skips all malformed room data",
          "[platform-appearance-live][field]") {
  PlatformAppearanceLiveFixture fixture;
  VS::Field::roomData.header.moving_platforms_length =
      sizeof(fixture.platforms) - 1;
  VS::Field::roomPalettes = nullptr;
  REQUIRE(VS::Field::SetLivePlatformFreezeAppearance(0));
  CHECK(VS::Field::isPlatformFreezeTintActive == 0);
  CHECK(fixture.geometry.quad.red1 == 0x12);
}

TEST_CASE(
    "Native freeze appearance refuses malformed table and missing palette",
    "[platform-appearance-live][field]") {
  PlatformAppearanceLiveFixture fixture;
  VS::Field::roomData.header.moving_platforms_length =
      sizeof(fixture.platforms) - 1;
  CHECK_FALSE(VS::Field::SetLivePlatformFreezeAppearance(1));
  CHECK(VS::Field::isPlatformFreezeTintActive == 0);
  VS::Field::roomData.header.moving_platforms_length =
      sizeof(fixture.platforms);
  VS::Field::isPlatformFreezeTintActive = 1;
  VS::Field::roomPalettes = nullptr;
  CHECK_FALSE(VS::Field::SetLivePlatformFreezeAppearance(0));
  CHECK(VS::Field::isPlatformFreezeTintActive == 1);
}

TEST_CASE("Native freeze appearance refuses a truncated environment field",
          "[platform-appearance-live][field]") {
  PlatformAppearanceLiveFixture fixture;
  VS::Field::MpdEnvironment environment{};
  environment.freeze_effect_id = -1;
  VS::Field::roomData.environment = &environment;
  VS::Field::roomData.header.environment_length =
      offsetof(VS::Field::MpdEnvironment, freeze_effect_id);
  CHECK_FALSE(VS::Field::SetLivePlatformFreezeAppearance(1));
  CHECK(VS::Field::isPlatformFreezeTintActive == 0);
}

TEST_CASE("Freeze appearance retains its original module address",
          "[platform-appearance-live][field]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008E4DC) ==
        reinterpret_cast<const void*>(
            &VS::Field::SetPlatformFreezeAppearanceEntry));
}

TEST_CASE("Platform target waypoint returns every masked index and exact value",
          "[platform-target-waypoint-core][field]") {
  const std::array<VS::Field::MovingPlatformWaypoint, 4> points{
      VS::Field::MovingPlatformWaypoint{-1, 2, -3, 7},
      {11, 13, 17, 19},
      {23, 29, 31, 37},
      {41, 43, 47, 53}};
  for (std::uint32_t current = 0; current <= 255; ++current) {
    const auto selected = VS::Field::MovingPlatformTargetWaypoint(
        static_cast<std::uint8_t>(current), points);
    REQUIRE(selected.has_value());
    if (!selected) return;
    CHECK(selected->index == (current & 3));
    CHECK(selected->point == points[current & 3]);
  }
}

TEST_CASE("Platform target waypoint refuses missing selected points",
          "[platform-target-waypoint-core][field]") {
  const std::array<VS::Field::MovingPlatformWaypoint, 1> points{};
  CHECK_FALSE(VS::Field::MovingPlatformTargetWaypoint(0, {}).has_value());
  CHECK_FALSE(VS::Field::MovingPlatformTargetWaypoint(1, points).has_value());
}

TEST_CASE(
    "Native target waypoint returns hidden platform data by detached value",
    "[platform-target-waypoint-live][field]") {
  PlatformPlacementFixture fixture;
  fixture.platforms[0].current_waypoint = 2;
  fixture.platforms[0].waypoints[2] =
      VECTOR{.vx = -4097, .vy = 8193, .vz = 12288, .pad = 17};
  const auto point = VS::Field::LiveMovingPlatformTargetWaypoint(0);
  REQUIRE(point.has_value());
  if (!point) return;
  CHECK(*point == VS::Field::MovingPlatformWaypoint{-4097, 8193, 12288, 17});
  CHECK(VS::Field::MovingPlatformTargetWaypointEntry(0) ==
        &fixture.platforms[0].waypoints[2]);
  fixture.platforms[0].waypoints[2].vx = 44;
  CHECK(point->x == -4097);
}

TEST_CASE("Native target waypoint refuses invalid indices and partial tables",
          "[platform-target-waypoint-live][field]") {
  PlatformPlacementFixture fixture;
  CHECK_FALSE(VS::Field::LiveMovingPlatformTargetWaypoint(-1).has_value());
  CHECK(VS::Field::MovingPlatformTargetWaypointEntry(2) == nullptr);
  VS::Field::roomData.header.moving_platforms_length =
      sizeof(fixture.platforms) - 1;
  CHECK_FALSE(VS::Field::LiveMovingPlatformTargetWaypoint(0).has_value());
  CHECK(VS::Field::MovingPlatformTargetWaypointEntry(0) == nullptr);
}

TEST_CASE("Target waypoint preserves each original C record address",
          "[platform-target-waypoint-live][field]") {
  PlatformPlacementFixture fixture;
  for (std::uint8_t current = 0; current < 4; ++current) {
    fixture.platforms[1].current_waypoint = current;
    CHECK(VS::Field::MovingPlatformTargetWaypointEntry(1) ==
          &fixture.platforms[1].waypoints[current]);
  }
}

TEST_CASE("Target waypoint retains its original module address",
          "[platform-target-waypoint-live][field]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008E2D4) ==
        reinterpret_cast<const void*>(
            &VS::Field::MovingPlatformTargetWaypointEntry));
}

TEST_CASE("Platform control retains seven bits and conditional bounce reset",
          "[platform-control-core][field]") {
  for (std::uint32_t bits = 0; bits <= UINT16_MAX; ++bits) {
    const auto control =
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(bits));
    VS::Field::MovingPlatformControlState state{.control_state = 9,
                                                .bounce_frame = 7,
                                                .waiting = true,
                                                .motion_ticks = 123};
    VS::Field::SetMovingPlatformActor(state, control, 0);
    CHECK(state.control_state == (bits & 127));
    CHECK(state.bounce_frame == (control >= 0 ? 0 : 7));
    CHECK(state.waiting);
    CHECK(state.motion_ticks == 123);
    state.bounce_frame = 7;
    VS::Field::SetMovingPlatformActor(state, control, -1);
    CHECK(state.bounce_frame == 7);
  }
}

TEST_CASE("Platform pause preserves every input low byte",
          "[platform-control-core][field]") {
  VS::Field::MovingPlatformRuntimeState state{.freeze_frames = 47};
  for (std::uint32_t bits = 0; bits <= UINT16_MAX; ++bits) {
    VS::Field::SetMovingPlatformsPaused(state, static_cast<std::int32_t>(bits));
    CHECK(state.paused == static_cast<std::uint8_t>(bits));
    CHECK(state.freeze_frames == 47);
  }
}

TEST_CASE(
    "Platform freeze appearance uses full input before halfword duration store",
    "[platform-control-core][field]") {
  VS::Field::MovingPlatformRuntimeState state{.paused = 128,
                                              .freeze_frames = 47};
  RecordingFreezeHost host;
  host.runtime = &state;
  for (const auto value : std::array<std::int32_t, 7>{
           0, 1, -1, 65536, -65536, std::numeric_limits<std::int32_t>::min(),
           std::numeric_limits<std::int32_t>::max()}) {
    state.freeze_frames = 47;
    REQUIRE(VS::Field::FreezeMovingPlatforms(state, value, host));
    CHECK(host.appearance == (value != 0));
    CHECK(host.duration_during_appearance == 47);
    CHECK(state.freeze_frames ==
          std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value)));
    CHECK(state.paused == 128);
  }
  host.has_platforms = false;
  host.appearance.reset();
  state.freeze_frames = 47;
  CHECK_FALSE(VS::Field::FreezeMovingPlatforms(state, 180, host));
  CHECK_FALSE(host.appearance);
  CHECK(state.freeze_frames == 47);
}

TEST_CASE("Temporary room freeze uses exactly 180 ticks after appearance",
          "[platform-temporary-freeze-core][field]") {
  VS::Field::MovingPlatformRuntimeState state{.paused = 128,
                                              .freeze_frames = 47};
  RecordingFreezeHost host;
  host.runtime = &state;
  REQUIRE(VS::Field::FreezeRoomGimmicksTemporarily(state, host));
  CHECK(host.appearance == true);
  CHECK(host.duration_during_appearance == 47);
  CHECK(state.freeze_frames == 180);
  CHECK(state.paused == 128);
  host.has_platforms = false;
  host.appearance.reset();
  state.freeze_frames = 47;
  CHECK_FALSE(VS::Field::FreezeRoomGimmicksTemporarily(state, host));
  CHECK_FALSE(host.appearance);
  CHECK(state.freeze_frames == 47);
  CHECK(state.paused == 128);
}

TEST_CASE("Platform visibility wraps subtraction into exactly one hidden bit",
          "[platform-visibility-core][field]") {
  RecordingPlatformVisibilityHost host;
  for (std::uint32_t value = 0; value <= UINT16_MAX; ++value) {
    host.calls.clear();
    REQUIRE(VS::Field::SetMovingPlatformVisibility(
        static_cast<std::int32_t>(value), host));
    CHECK(host.hidden == ((value & 1) == 0));
    CHECK(host.geometry_hidden == host.hidden);
  }
  for (const auto value : {INT32_MIN, INT32_MAX, -1, 0, 1, 2, 65536}) {
    REQUIRE(VS::Field::SetMovingPlatformVisibility(value, host));
    CHECK(host.hidden == ((static_cast<std::uint32_t>(value) & 1) == 0));
  }
}

TEST_CASE("Platform visibility rereads hidden state after geometry lookup",
          "[platform-visibility-core][field]") {
  RecordingPlatformVisibilityHost host;
  host.callback_hidden = false;
  REQUIRE(VS::Field::SetMovingPlatformVisibility(0, host));
  CHECK(host.stored_hidden == true);
  CHECK_FALSE(host.geometry_hidden);
  CHECK(host.calls == std::vector<int>{1, 2, 3});
}

TEST_CASE("Platform visibility keeps hidden state when geometry is absent",
          "[platform-visibility-core][field]") {
  RecordingPlatformVisibilityHost host;
  host.geometry = false;
  REQUIRE(VS::Field::SetMovingPlatformVisibility(0, host));
  CHECK(host.hidden);
  CHECK(host.calls == std::vector<int>{1, 2});
  host.present = false;
  host.calls.clear();
  CHECK_FALSE(VS::Field::SetMovingPlatformVisibility(1, host));
  CHECK(host.hidden);
  CHECK(host.calls.empty());
}

TEST_CASE("Platform visibility refuses geometry writes after invalidation",
          "[platform-visibility-core][field]") {
  RecordingPlatformVisibilityHost host;
  host.invalidate_on_lookup = true;
  CHECK_FALSE(VS::Field::SetMovingPlatformVisibility(0, host));
  CHECK(host.hidden);
  CHECK_FALSE(host.geometry_hidden);
  CHECK(host.calls == std::vector<int>{1, 2});
}

TEST_CASE("Platform dwell reports unsigned halfword only while waiting",
          "[platform-control-core][field]") {
  CHECK_FALSE(VS::Field::MovingPlatformDwellTicks(std::nullopt));
  for (std::uint32_t ticks = 0; ticks <= UINT16_MAX; ++ticks) {
    const VS::Field::MovingPlatformControlState state{
        .waiting = true, .motion_ticks = static_cast<std::uint16_t>(ticks)};
    CHECK(VS::Field::MovingPlatformDwellTicks(state) == ticks);
    auto moving = state;
    moving.waiting = false;
    CHECK_FALSE(VS::Field::MovingPlatformDwellTicks(moving));
  }
}

TEST_CASE(
    "Native platform control changes only its packed association and bounce "
    "byte",
    "[platform-control-live][field]") {
  PlatformPlacementFixture fixture;
  fixture.platforms[0].motion_ticks = 65535;
  fixture.platforms[0].is_waiting = 1;
  fixture.platforms[0].terminal_waypoint = 3;
  VS::Field::platformFreezeFrames = 0;
  for (const auto control : std::array<std::int32_t, 8>{
           -1, 0, 1, 127, 128, 65535, std::numeric_limits<std::int32_t>::min(),
           std::numeric_limits<std::int32_t>::max()}) {
    VS::Field::platformBounceFrames[0] = 7;
    auto expected = fixture.platforms[0];
    expected.control_state = static_cast<std::uint32_t>(control) & 127;
    REQUIRE(VS::Field::SetMovingPlatformActorEntry(0, control) == 1);
    using Bytes = std::array<std::byte, sizeof(expected)>;
    CHECK(std::bit_cast<Bytes>(fixture.platforms[0]) ==
          std::bit_cast<Bytes>(expected));
    CHECK(VS::Field::platformBounceFrames[0] == (control >= 0 ? 0 : 7));
  }
  VS::Field::platformFreezeFrames = -1;
  VS::Field::platformBounceFrames[0] = 7;
  REQUIRE(VS::Field::SetMovingPlatformActorEntry(0, 1) == 1);
  CHECK(VS::Field::platformBounceFrames[0] == 7);
}

TEST_CASE("Native platform control and dwell refuse invalid record spans",
          "[platform-control-live][field]") {
  PlatformPlacementFixture fixture;
  CHECK(VS::Field::SetMovingPlatformActorEntry(-1, 1) == 0);
  CHECK(VS::Field::SetMovingPlatformActorEntry(2, 1) == 0);
  CHECK(VS::Field::MovingPlatformDwellTicksEntry(-1) == UINT32_MAX);
  VS::Field::roomData.header.moving_platforms_length =
      sizeof fixture.platforms - 1;
  CHECK(VS::Field::SetMovingPlatformActorEntry(0, 1) == 0);
  CHECK(VS::Field::MovingPlatformDwellTicksEntry(0) == UINT32_MAX);
  VS::Field::roomData.header.moving_platforms_length = sizeof fixture.platforms;
  VS::Field::roomData.moving_platforms =
      reinterpret_cast<VS::Field::MpdMovingPlatform*>(
          reinterpret_cast<std::byte*>(fixture.platforms.data()) + 1);
  CHECK(VS::Field::SetMovingPlatformActorEntry(0, 1) == 0);
  VS::Field::roomData.moving_platforms = nullptr;
  CHECK(VS::Field::SetMovingPlatformActorEntry(0, 1) == 0);
}

TEST_CASE(
    "Native platform pause and dwell retain original byte and unsigned "
    "halfword",
    "[platform-control-live][field]") {
  PlatformPlacementFixture fixture;
  VS::Field::platformFreezeFrames = 47;
  VS::Field::SetMovingPlatformsPausedEntry(65535);
  CHECK(VS::Field::areMovingPlatformsPaused == 255);
  CHECK(VS::Field::platformFreezeFrames == 47);
  fixture.platforms[0].motion_ticks = 65535;
  fixture.platforms[0].is_waiting = 1;
  CHECK(VS::Field::MovingPlatformDwellTicksEntry(0) == 65535);
  fixture.platforms[0].is_waiting = 0;
  CHECK(VS::Field::MovingPlatformDwellTicksEntry(0) == UINT32_MAX);
}

TEST_CASE(
    "Native platform freeze uses full nonzero input then halfword duration",
    "[platform-control-live][field]") {
  PlatformPlacementFixture fixture;
  VS::Field::isPlatformFreezeTintActive = 1;
  VS::Field::platformFreezeFrames = 47;
  REQUIRE(VS::Field::FreezeLiveMovingPlatforms(65536));
  CHECK(VS::Field::platformFreezeFrames == 0);
  CHECK(VS::Field::isPlatformFreezeTintActive == 1);
  REQUIRE(VS::Field::FreezeLiveMovingPlatforms(-1));
  CHECK(VS::Field::platformFreezeFrames == -1);
  VS::Field::roomData.header.moving_platforms_length = 0;
  VS::Field::FreezeMovingPlatformsEntry(180);
  CHECK(VS::Field::platformFreezeFrames == -1);
}

TEST_CASE("Temporary room freeze native entry preserves packed platform data",
          "[platform-temporary-freeze-live][field]") {
  PlatformPlacementFixture fixture;
  VS::Field::isPlatformFreezeTintActive = 1;
  VS::Field::areMovingPlatformsPaused = 128;
  VS::Field::platformFreezeFrames = 47;
  const auto before = fixture.platforms;
  VS::Field::FreezeRoomGimmicksTemporarilyEntry();
  CHECK(VS::Field::platformFreezeFrames == 180);
  CHECK(VS::Field::isPlatformFreezeTintActive == 1);
  CHECK(VS::Field::areMovingPlatformsPaused == 128);
  using Bytes = std::array<std::byte, sizeof(fixture.platforms)>;
  CHECK(std::bit_cast<Bytes>(fixture.platforms) ==
        std::bit_cast<Bytes>(before));
  VS::Field::roomData.header.moving_platforms_length = 0;
  VS::Field::platformFreezeFrames = 47;
  CHECK_FALSE(VS::Field::FreezeLiveRoomGimmicksTemporarily());
  VS::Field::FreezeRoomGimmicksTemporarilyEntry();
  CHECK(VS::Field::platformFreezeFrames == 47);
}

TEST_CASE("Temporary room freeze retains its original module address",
          "[platform-temporary-freeze-live][field]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8006CE50) ==
        reinterpret_cast<const void*>(
            &VS::Field::FreezeRoomGimmicksTemporarilyEntry));
}

TEST_CASE("Native platform visibility changes only record and geometry bits",
          "[platform-visibility-live][field]") {
  PlatformPlacementFixture fixture;
  GeometryWords geometry{1, 0x12345678, 7, 8, 9};
  VS::Field::roomData.geometry = geometry.data();
  VS::Field::roomData.header.geometry_length = sizeof geometry;
  using Bytes = std::array<std::byte, sizeof(fixture.platforms)>;
  for (const auto value : {INT32_MIN, INT32_MAX, -1, 0, 1, 2, 65536}) {
    auto expected = fixture.platforms;
    const bool hidden = (static_cast<std::uint32_t>(value) & 1) == 0;
    expected[0].is_hidden = hidden;
    auto expected_geometry = geometry;
    expected_geometry[1] = hidden ? geometry[1] | 0x100 : geometry[1] & ~0x100;
    REQUIRE(VS::Field::SetLiveMovingPlatformVisibility(0, value));
    CHECK(std::bit_cast<Bytes>(fixture.platforms) ==
          std::bit_cast<Bytes>(expected));
    CHECK(geometry == expected_geometry);
  }
}

TEST_CASE("Native platform visibility retains missing geometry and bounds",
          "[platform-visibility-live][field]") {
  PlatformPlacementFixture fixture;
  REQUIRE(VS::Field::SetLiveMovingPlatformVisibility(0, 1));
  CHECK(fixture.platforms[0].is_hidden == 0);
  CHECK_FALSE(VS::Field::SetLiveMovingPlatformVisibility(-1, 0));
  CHECK_FALSE(VS::Field::SetLiveMovingPlatformVisibility(2, 0));
  VS::Field::roomData.header.moving_platforms_length =
      sizeof fixture.platforms - 1;
  CHECK_FALSE(VS::Field::SetLiveMovingPlatformVisibility(0, 0));
  CHECK(fixture.platforms[0].is_hidden == 0);
  VS::Field::roomData.moving_platforms = nullptr;
  VS::Field::SetMovingPlatformVisibilityEntry(0, 0);
  CHECK(fixture.platforms[0].is_hidden == 0);
}

TEST_CASE("Native platform visibility supports the fifth bounded record",
          "[platform-visibility-live][field]") {
  PlatformPlacementFixture fixture;
  std::array<VS::Field::MpdMovingPlatform, 5> platforms{};
  VS::Field::roomData.moving_platforms = platforms.data();
  VS::Field::roomData.header.moving_platforms_length = sizeof platforms;
  REQUIRE(VS::Field::SetLiveMovingPlatformVisibility(4, 0));
  CHECK(platforms[4].is_hidden == 1);
  CHECK(platforms[0].is_hidden == 0);
}

TEST_CASE("Platform visibility retains its original module address",
          "[platform-visibility-live][field]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008E224) ==
        reinterpret_cast<const void*>(
            &VS::Field::SetMovingPlatformVisibilityEntry));
}

TEST_CASE("Platform control entries retain original module addresses",
          "[platform-control-live][field]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008C49C) ==
        reinterpret_cast<const void*>(&VS::Field::SetMovingPlatformActorEntry));
  CHECK(
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                        0x8008D594) ==
      reinterpret_cast<const void*>(&VS::Field::SetMovingPlatformsPausedEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008D5A0) ==
        reinterpret_cast<const void*>(&VS::Field::FreezeMovingPlatformsEntry));
  CHECK(
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                        0x8008E320) ==
      reinterpret_cast<const void*>(&VS::Field::MovingPlatformDwellTicksEntry));
}

TEST_CASE(
    "Platform waypoint reset preserves original masked states and exact "
    "positions",
    "[platform-waypoint-core][field]") {
  const std::array<VS::Field::MovingPlatformWaypoint, 4> points{
      VS::Field::MovingPlatformWaypoint{-4097, -8191, 4095, 7},
      {8192, 4096, -4097, 8},
      {3, -5, 7, 9},
      {11, 13, -17, 10}};
  for (std::int32_t selected = 0; selected < 16; ++selected) {
    for (std::uint8_t end = 0; end < 4; ++end) {
      VS::Field::MovingPlatformWaypointState state{.hidden = true,
                                                   .waiting = true,
                                                   .control_state = 127,
                                                   .end_waypoint = end,
                                                   .motion_ticks = 65535};
      REQUIRE(VS::Field::ResetMovingPlatformWaypoint(state, points, selected));
      const auto index = static_cast<std::size_t>(selected & 3);
      CheckWaypointResetState(state, points[index], selected, end);
    }
  }
}

TEST_CASE(
    "Platform waypoint reset uses terminal fallback and refuses missing data "
    "unchanged",
    "[platform-waypoint-core][field]") {
  const std::array<VS::Field::MovingPlatformWaypoint, 4> points{};
  for (std::uint8_t terminal = 0; terminal < 4; ++terminal) {
    VS::Field::MovingPlatformWaypointState state{.end_waypoint = 3,
                                                 .terminal_waypoint = terminal};
    REQUIRE(VS::Field::ResetMovingPlatformWaypoint(state, points, -1));
    CHECK(state.start_waypoint == terminal);
  }
  VS::Field::MovingPlatformWaypointState state{
      .hidden = true, .control_state = 127, .motion_ticks = 123};
  const auto saved = state;
  CHECK_FALSE(VS::Field::ResetMovingPlatformWaypoint(state, points, -2));
  CHECK(state == saved);
  CHECK_FALSE(VS::Field::ResetMovingPlatformWaypoint(state, {}, 0));
  CHECK(state == saved);
}

TEST_CASE("Platform waypoint reset advances only the already masked waypoint",
          "[platform-waypoint-core][field]") {
  const std::array<VS::Field::MovingPlatformWaypoint, 4> points{};
  VS::Field::MovingPlatformWaypointState state{};
  REQUIRE(VS::Field::ResetMovingPlatformWaypoint(
      state, points, std::numeric_limits<std::int32_t>::max()));
  CHECK(state.start_waypoint == 3);
  CHECK(state.current_waypoint == 0);
  CHECK(state.current_segment == 7);
}

TEST_CASE(
    "Native waypoint reset preserves other packed fields and updates bounded "
    "geometry",
    "[platform-waypoint-live][field]") {
  PlatformPlacementFixture fixture;
  auto& platform = fixture.platforms[0];
  platform.scene_object_id = 0;
  platform.control_state = 127;
  platform.motion_ticks = 65535;
  platform.is_waiting = 1;
  platform.end_waypoint = 1;
  platform.terminal_waypoint = 2;
  platform.waypoints[2] =
      VECTOR{.vx = -4097, .vy = -8191, .vz = 8193, .pad = 17};
  GeometryWords geometry{1, 0x12345678, 0, 0, 0, 7, 8, 9, 10};
  VS::Field::roomData.geometry = geometry.data();
  VS::Field::roomData.header.geometry_length = sizeof geometry;
  auto expected = platform;
  expected.is_hidden = 0;
  expected.is_waiting = 0;
  expected.is_moving_forward = 1;
  expected.control_state = 0;
  expected.start_waypoint = 2;
  expected.current_waypoint = 3;
  expected.current_segment = 2;
  expected.motion_ticks = 0;
  expected.position_x = -4097;
  expected.position_y = -8191;
  expected.position_z = 8193;
  REQUIRE(VS::Field::ResetLiveMovingPlatformWaypoint(0, -1));
  using Bytes = std::array<std::byte, sizeof(expected)>;
  CHECK(std::bit_cast<Bytes>(platform) == std::bit_cast<Bytes>(expected));
  CHECK(geometry == GeometryWords{1, 0x12345678, -1, 1, 2, 7, 8, 9, 10});
}

TEST_CASE(
    "Native waypoint reset refuses invalid targets and negative selectors "
    "unchanged",
    "[platform-waypoint-live][field]") {
  PlatformPlacementFixture fixture;
  const auto saved = fixture.platforms;
  CHECK_FALSE(VS::Field::ResetLiveMovingPlatformWaypoint(-1, 0));
  CHECK_FALSE(VS::Field::ResetLiveMovingPlatformWaypoint(2, 0));
  CHECK_FALSE(VS::Field::ResetLiveMovingPlatformWaypoint(0, -2));
  using Bytes = std::array<std::byte, sizeof(saved)>;
  CHECK(std::bit_cast<Bytes>(fixture.platforms) == std::bit_cast<Bytes>(saved));
}

TEST_CASE("Geometry position setter rejects truncated groups without mutation",
          "[platform-waypoint-live][field]") {
  GeometryWords geometry{1};
  const auto saved = geometry;
  auto bytes = std::as_writable_bytes(std::span(geometry));
  CHECK_FALSE(VS::Field::SetRoomGeometryGroupPosition(
      bytes.first(bytes.size() - 1), 0, {1, 2, 3}));
  CHECK_FALSE(VS::Field::SetRoomGeometryGroupPosition(bytes, 1, {1, 2, 3}));
  CHECK(geometry == saved);
}

TEST_CASE("Waypoint reset retains its original module address",
          "[platform-waypoint-live][field]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008C6B4) ==
        reinterpret_cast<const void*>(
            &VS::Field::ResetMovingPlatformWaypointEntry));
}

TEST_CASE(
    "Scripted placement reads a hidden authored platform without making "
    "it collide or render",
    "[field][platform-placement]") {
  PlatformPlacementFixture fixture;
  const auto contact = VS::Field::LiveMovingPlatformPlacement(3);
  REQUIRE(contact.has_value());
  CheckAuthoredPlacement(*contact);
  CHECK(fixture.platforms[0].is_hidden == 1);
  CHECK(fixture.platforms[1].is_hidden == 1);
  std::array<VS::Field::PlatformContact, 4> visible{};
  CHECK(VS::Field::CollectMovingPlatformPositionsEntry(visible.data()) == 0);
  CHECK_FALSE(VS::Field::FindLiveMovingPlatformById(3).has_value());
}

TEST_CASE(
    "Visible platform placement preserves the original contact and bounce",
    "[field][platform-placement]") {
  PlatformPlacementFixture fixture;
  fixture.platforms[1].is_hidden = 0;
  for (int frame = 0; frame < 7; ++frame) {
    VS::Field::platformBounceFrames[1] = static_cast<char>(frame);
    std::array<VS::Field::PlatformContact, 4> visible{};
    REQUIRE(VS::Field::CollectMovingPlatformPositionsEntry(visible.data()) ==
            1);
    const auto placement = VS::Field::LiveMovingPlatformPlacement(3);
    const auto found = VS::Field::FindLiveMovingPlatformById(3);
    REQUIRE(placement.has_value());
    REQUIRE(found.has_value());
    for (const auto& contact : {*placement, *found}) {
      CHECK(contact.x == visible[0].x);
      CHECK(contact.height == visible[0].height);
      CHECK(contact.z == visible[0].z);
      CHECK(contact.platform_index == visible[0].platform_index);
    }
  }
}

TEST_CASE("Visible-platform lookup returns its contact by value",
          "[field][platform-placement]") {
  PlatformPlacementFixture fixture;
  fixture.platforms[1].is_hidden = 0;
  const auto contact = VS::Field::FindLiveMovingPlatformById(3);
  REQUIRE(contact.has_value());
  REQUIRE(VS::Field::LiveMovingPlatformPlacement(2).has_value());
  fixture.platforms[1].position_x += 64 * ONE;
  CheckAuthoredPlacement(*contact);
}

TEST_CASE("Invalid platform placement reports no contact",
          "[field][platform-placement]") {
  PlatformPlacementFixture fixture;
  CHECK_FALSE(VS::Field::LiveMovingPlatformPlacement(1).has_value());
  CHECK_FALSE(VS::Field::LiveMovingPlatformPlacement(4).has_value());
  VS::Field::roomData.header.moving_platforms_length = 1;
  CHECK_FALSE(VS::Field::LiveMovingPlatformPlacement(3).has_value());
  VS::Field::roomData.header.moving_platforms_length =
      sizeof(fixture.platforms);
  VS::Field::platformBounceFrames[1] = 8;
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

VS::Actors::ActorRecord OnPlatform() {
  VS::Actors::ActorRecord actor{};
  actor.instance.support_layer = kLayer;
  actor.instance.placement_layer = kLayer;
  actor.instance.position = {1200, -768, 440, 0};
  actor.run_speed = 64;
  actor.collision_radius = 20;
  return actor;
}

// The fields the jump, contact and ledge paths write.
auto Observed(const VS::Actors::ActorRecord& actor) {
  const auto& position = actor.instance.position;
  const auto& anchor = actor.platform_anchor;
  return std::tuple(position.vx, position.vy, position.vz, anchor.vx, anchor.vy,
                    anchor.vz, actor.platform_offset_x, actor.platform_offset_z,
                    actor.move_facing, actor.instance.jump_countdown,
                    actor.landing_pending, actor.destination_x,
                    actor.destination_z);
}

bool Same(const VS::Actors::ActorRecord& a, const VS::Actors::ActorRecord& b) {
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
  fixture.platforms[1].is_hidden = 0;
  auto actor = OnPlatform();
  actor.instance.position.vy = -600;  // the platform top is 168 above
  auto* saved = VS::Actors::actorRecords[0];
  VS::Actors::actorRecords[0] = &actor;
  const VS::Actors::PackedTile target{.x = 5, .layer = kLayer, .z = 7};
  VS::Actors::BeginLiveActorJumpToTile(&actor, target, 32, 0);
  VS::Actors::actorRecords[0] = saved;
  CHECK(log.errors.empty());
  CHECK(actor.destination_x == 1216);
  CHECK(actor.destination_z == 448);
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
  fixture.platforms[1].is_hidden = 0;
  auto actor = OnPlatform();
  actor.platform_anchor = {1210, -768, 440, 0};
  VS::Actors::UpdateLiveActorPlatformContact(&actor, 0);
  CHECK(actor.platform_offset_x == 6);
  CHECK(actor.platform_offset_z == 8);
  CHECK(actor.instance.position.vx == 1206);
  CHECK(actor.instance.position.vz == 448);
  auto anchored = OnPlatform();
  VS::Actors::UpdateLiveActorPlatformContact(&anchored, 1);
  CHECK(anchored.platform_anchor.vx == 1216);
  CHECK(anchored.platform_anchor.vy == -768);
  CHECK(anchored.platform_anchor.vz == 448);
  CHECK(log.errors.empty());
}

TEST_CASE("Ledge probe on a hidden platform reports no ledge unchanged",
          "[field][platform-placement][issue-257]") {
  PlatformPlacementFixture fixture;
  PlatformErrorLog log;
  auto actor = OnPlatform();
  actor.move_facing = 123;
  const auto before = actor;
  SVECTOR offset{1, 2, 3, 4};
  CHECK(VS::Field::LiveCalcActorLedgeOffset(&actor, &offset) == 0);
  CHECK(Same(actor, before));
  CHECK(offset.vx == 1);
  CHECK(offset.vy == 2);
  CHECK(offset.vz == 3);
  CHECK(log.errors.size() == 1);
}
