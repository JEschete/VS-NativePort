#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ActorMotion.hpp"
#include "Actors/ActorMovement.hpp"
#include "Actors/ActorMovementLive.hpp"
#include "Content/ModuleData.h"
#include "TestSupport/ChannelWarnings.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Field/Gimmicks.h"
}

namespace actors = VS::Actors;
using actors::MotionVector;
using actors::MoveInput;
using actors::MovementActor;
using actors::PackedTile;
using actors::SupportContact;
using actors::TerrainResolve;
using VS::test_support::ChannelWarnings;
using VS::test_support::Refused;

namespace {
constexpr int kAshley = 0;
constexpr int kActor = 3;
constexpr int kActorSlots = 17;
// movement_mode, locomotion_state and pose_state values the rules use.
constexpr std::uint8_t kModeTakeoff = 2;
constexpr std::uint8_t kModeAirborne = 3;
constexpr std::uint8_t kModeLanding = 4;
constexpr std::uint8_t kLocomotionHanging = 2;
constexpr std::uint8_t kTravelLeaping = 2;
constexpr std::uint8_t kTravelGliding = 5;
constexpr std::int16_t kDestinationJump = 2;
constexpr std::int16_t kFullCollision = 4;
constexpr int kStickDistance = -1;
constexpr int kStatusBlocked = 3;
constexpr int kHangDropFrame = 7;
constexpr std::int16_t kHangDropLift = 3;
constexpr int kWalkSpeed = 4;
constexpr int kRunSpeed = 8;
constexpr int kFixedShift = 12;
constexpr int kWalkDistance = kWalkSpeed << kFixedShift;
constexpr std::uint8_t kGimmickSlot = 5;
constexpr std::uint16_t kQuarterTurn = 0x400;

// Records the host calls the rules make and answers with the configured
// terrain, platforms and probe.
class FakeMovementHost final : public actors::MovementHost {
 public:
  struct FakeTerrain final : actors::MotionTerrainHost {
    int SupportHeight(const MotionVector& /*origin*/,
                      actors::SupportProbe /*probe*/) override {
      return 0;
    }
    int InterpolatedFloorHeight(int /*x*/, int /*z*/) override { return 0; }
    int GimmickSupportHeight(int /*tile_x*/, int /*tile_z*/) override {
      return 0;
    }
  };

  std::array<std::optional<MovementActor>, kActorSlots> table{};
  actors::MovementTables tables{};
  SupportContact support{};
  int support_height = 0;
  int platform_count = 2;
  int bounce_frame = 0;
  TerrainResolve resolve{};
  actors::MovementProbe probe{};
  std::vector<std::string> calls;
  std::vector<std::pair<int, int>> riders;
  std::vector<int> bounce_queries;

  MovementActor& Add(int id) {
    auto& slot = table.at(static_cast<std::size_t>(id));
    slot.emplace();
    slot->motion.slot = static_cast<std::uint8_t>(id);
    slot->motion.walk_speed = kWalkSpeed;
    slot->motion.run_speed = kRunSpeed;
    slot->collision_state = kFullCollision;
    return *slot;
  }

  [[nodiscard]] bool Called(const std::string& call) const {
    return std::ranges::find(calls, call) != calls.end();
  }

  MovementActor* Actor(int id) override {
    if (id < 0 || id >= kActorSlots) return nullptr;
    auto& slot = table.at(static_cast<std::size_t>(id));
    return slot ? &*slot : nullptr;
  }
  const actors::MovementTables& Tables() override { return tables; }
  int TickSpeed() override { return 2; }
  actors::MotionTerrainHost& Terrain() override { return terrain_; }

  void PlayAnimation(int slot, int animation, int blend_frames) override {
    calls.push_back("animation " + std::to_string(slot) + " " +
                    std::to_string(animation) + " " +
                    std::to_string(blend_frames));
  }
  void QueueAnimation(int /*slot*/, int /*animation*/, int /*blend_frames*/,
                      int /*start_frame*/) override {
    calls.emplace_back("queue animation");
  }
  void UpdateLocomotionAnimation(MovementActor& /*actor*/, int distance,
                                 int /*direction*/) override {
    calls.push_back("locomotion " + std::to_string(distance));
  }
  void PlayModelSound(const MovementActor& /*actor*/, int sound) override {
    calls.push_back("model sound " + std::to_string(sound));
  }
  void PlaySoundPair(const MovementActor& /*actor*/, int first,
                     int second) override {
    calls.push_back("sound pair " + std::to_string(first) + " " +
                    std::to_string(second));
  }
  void PlaySurfaceContactSound(int /*id*/, const MovementActor& /*actor*/,
                               int sound) override {
    calls.push_back("surface sound " + std::to_string(sound));
  }
  void SpawnGroundImpact(int /*x*/, int /*z*/) override {
    calls.emplace_back("ground impact");
  }
  int SampleSupportHeight(const MotionVector& /*origin*/, int /*offset_x*/,
                          int /*offset_z*/, int /*mode*/) override {
    return support_height;
  }
  int TileSupportHeight(int /*x*/, int /*z*/) override {
    return support_height;
  }
  bool HasCollisionCell(const MotionVector& /*position*/,
                        int /*layer*/) override {
    return true;
  }
  int PlatformContactHeight(int /*x*/, int /*y*/, int /*z*/,
                            int /*mode*/) override {
    return support_height;
  }
  SupportContact CurrentSupport() override { return support; }
  void ClearSupportLayer() override {
    support.layer = 0;
    calls.emplace_back("clear support layer");
  }
  void SetContactSearchDepth(int /*depth*/) override {}
  int CeilingHeight(int /*x*/, int /*z*/) override { return 0; }
  std::optional<actors::PlatformPoint> MovingPlatform(int /*layer*/) override {
    return std::nullopt;
  }
  bool IsPointOnMovingPlatform(int /*layer*/,
                               const MotionVector& /*position*/) override {
    return false;
  }
  int MovingPlatformCount() override { return platform_count; }
  int PlatformBounceFrame(int platform) override {
    bounce_queries.push_back(platform);
    return bounce_frame;
  }
  void SetPlatformRider(int platform, int rider) override {
    riders.emplace_back(platform, rider);
  }
  int TileSurfaceId(int /*index*/) override { return 0; }
  TerrainResolve ResolveTerrainMotion(MovementActor& /*actor*/,
                                      MotionVector& /*motion*/,
                                      bool /*probe_only*/) override {
    return resolve;
  }
  actors::MovementProbe Probe() override { return probe; }
  bool CanHangFromLedge(MovementActor& /*actor*/, MotionVector& /*offset*/,
                        MotionVector& /*motion*/) override {
    calls.emplace_back("can hang");
    return false;
  }
  bool CalcLedgeOffset(MovementActor& /*actor*/,
                       MotionVector& /*offset*/) override {
    calls.emplace_back("ledge offset");
    return false;
  }
  bool HasLedgeSideSupport(MovementActor& /*actor*/) override {
    calls.emplace_back("ledge side support");
    return false;
  }
  std::optional<actors::MovementGimmick> Gimmick(int /*index*/) override {
    return actors::MovementGimmick{};
  }
  std::optional<actors::MovementGimmick> GimmickForActorSlot(
      int /*slot*/) override {
    return actors::MovementGimmick{};
  }
  void UpdateGimmickMotion(int /*slot*/, int /*ticks*/) override {}
  void SetAiContactOccupancy(int /*layer*/) override {}
  void ClearAiContactOccupancy(int /*layer*/) override {}
  void ReleaseAiDestinationTile(int /*x*/, int /*z*/) override {}
  void ForceReleaseAiReservations(int /*id*/) override {
    calls.emplace_back("force release");
  }
  void ClearAiNavigationReset(int /*slot*/) override {}
  void ReleaseAiContactOccupancy(int /*slot*/) override {}
  void SetDirectMovement(int /*id*/, int /*direction*/,
                         int /*distance*/) override {
    calls.emplace_back("direct movement");
  }
  void SetActorUpdateComplete() override {}

 private:
  FakeTerrain terrain_;
};

[[nodiscard]] bool NoLedgeCalls(const FakeMovementHost& host) {
  return !host.Called("can hang") && !host.Called("ledge offset") &&
         !host.Called("ledge side support");
}

// A travel leap or glide with frames left, over support `support`.
[[nodiscard]] std::uint8_t TravelPoseAfterStep(std::uint8_t pose,
                                               SupportContact support) {
  FakeMovementHost host;
  MovementActor& actor = host.Add(kActor);
  actor.motion.pose_state = pose;
  actor.motion.transition_frames = 2;
  host.support = support;
  actors::UpdateActorTravelTransition(host, actor);
  return actor.motion.pose_state;
}

// An airborne actor five frames into a tile jump.
MovementActor& StartTileJump(FakeMovementHost& host) {
  MovementActor& actor = host.Add(kActor);
  actor.motion.movement_mode = kModeAirborne;
  actor.motion.destination_mode = kDestinationJump;
  actor.jump_frames = 5;
  actor.motion.position = {.x = 640, .y = -200, .z = 640};
  return actor;
}

// Ashley hanging from the ledge of platform layer 2 at bounce frame `frame`.
MovementActor& StartHang(FakeMovementHost& host, int frame) {
  MovementActor& actor = host.Add(kAshley);
  actor.motion.locomotion_state = kLocomotionHanging;
  actor.support_layer = 2;
  actor.support_gimmick = kGimmickSlot;
  host.bounce_frame = frame;
  return actor;
}

// A step refused before the terrain probe leaves the actor where it stood.
void CheckBlockedStep(const FakeMovementHost& host, const MovementActor& actor,
                      const MotionVector& start) {
  CHECK(NoLedgeCalls(host));
  CHECK(actor.motion.position == start);
  CHECK(actor.motion.move_frames == 0);
  CHECK(actor.motion.locomotion_state == 0);
  CHECK(actor.ledge_attempts == 0);
  CHECK(host.calls.back() == "locomotion 0");
}
}  // namespace

TEST_CASE("Travel transitions stop on a platform layer, not a gimmick slot",
          "[actors][actors-movement]") {
  // #292: the original read the halfword layer as a word, so the gimmick slot
  // after it also ended the leap and the glide.
  const SupportContact gimmick_only{.layer = 0, .gimmick = kGimmickSlot};
  const SupportContact platform{.layer = 2, .gimmick = 0};
  CHECK(TravelPoseAfterStep(kTravelLeaping, gimmick_only) == kTravelLeaping);
  CHECK(TravelPoseAfterStep(kTravelGliding, gimmick_only) == kTravelGliding);
  CHECK(TravelPoseAfterStep(kTravelLeaping, platform) == 0);
  CHECK(TravelPoseAfterStep(kTravelGliding, platform) == 0);
}

TEST_CASE("Tile jumps keep going over a gimmick slot with no platform layer",
          "[actors][actors-movement]") {
  // #292: the word read saw the gimmick slot as a platform layer and ended the
  // jump early.
  FakeMovementHost host;
  MovementActor& actor = StartTileJump(host);
  host.support = {.layer = 0, .gimmick = kGimmickSlot};
  actors::UpdateActorJumpAndFall(host, kActor, actor);
  CHECK(actor.motion.movement_mode == kModeAirborne);
  CHECK(actor.contact_kind == 0);
  CHECK(host.riders.empty());
  CHECK_FALSE(host.Called("clear support layer"));
  CHECK_FALSE(host.Called("force release"));
}

TEST_CASE("Tile jumps land at once on another platform layer",
          "[actors][actors-movement]") {
  FakeMovementHost host;
  MovementActor& actor = StartTileJump(host);
  host.support = {.layer = 3, .gimmick = 0};
  actors::UpdateActorJumpAndFall(host, kActor, actor);
  CHECK(actor.motion.movement_mode == kModeLanding);
  CHECK(actor.contact_kind == 3);
  CHECK(host.riders == std::vector<std::pair<int, int>>{{1, kActor}, {1, -1}});
  CHECK(host.Called("clear support layer"));
  CHECK(host.Called("force release"));
}

TEST_CASE("Falls land on a gimmick slot with no platform layer",
          "[actors][actors-movement]") {
  FakeMovementHost host;
  MovementActor& actor = host.Add(kActor);
  actor.motion.movement_mode = kModeAirborne;
  actor.motion.position = {.x = 640, .y = 200, .z = 640};
  actor.motion.root_motion = {.x = 6, .y = 0, .z = 6};
  // The fall's next step passes the support by more than 128.
  host.support_height = 0;

  SECTION("a gimmick slot alone lands the actor") {
    // #292: the word read saw the gimmick slot as a platform layer and held
    // the actor in the air.
    host.support = {.layer = 0, .gimmick = kGimmickSlot};
    actors::UpdateActorJumpAndFall(host, kActor, actor);
    CHECK(actor.motion.movement_mode == kModeLanding);
    CHECK(actor.support_layer == 0);
    CHECK(actor.support_gimmick == kGimmickSlot);
  }
  SECTION("a platform layer far below holds the horizontal motion") {
    host.support = {.layer = 2, .gimmick = 0};
    actors::UpdateActorJumpAndFall(host, kActor, actor);
    CHECK(actor.motion.movement_mode == kModeAirborne);
    CHECK(actor.motion.root_motion.x == 0);
    CHECK(actor.motion.root_motion.z == 0);
  }
}

TEST_CASE("Hanging actors drop when their platform's bounce frame is 7",
          "[actors][actors-movement]") {
  // #293: the original indexed two bytes before the platform bounce frames.
  FakeMovementHost host;
  MovementActor& actor = StartHang(host, kHangDropFrame);
  actors::MoveActorWithCollision(host, kAshley, 0, 0, MoveInput{});
  CHECK(host.bounce_queries == std::vector<int>{0});
  CHECK(actor.motion.movement_mode == kModeAirborne);
  CHECK(actor.motion.root_motion.y == kHangDropLift);
  CHECK(actor.motion.locomotion_state == 0);
  CHECK(actor.support_layer == 0);
  CHECK(actor.support_gimmick == 0);
}

TEST_CASE("Hanging actors stay on other platform bounce frames",
          "[actors][actors-movement]") {
  FakeMovementHost host;
  MovementActor& actor = StartHang(host, 3);
  actors::MoveActorWithCollision(host, kAshley, 0, 0, MoveInput{});
  CHECK(host.bounce_queries == std::vector<int>{0});
  CHECK(actor.motion.movement_mode == 0);
  CHECK(actor.motion.locomotion_state == kLocomotionHanging);
  CHECK(actor.support_layer == 2);
}

TEST_CASE("Hanging below the platform layers reads no bounce frame",
          "[actors][actors-movement]") {
  FakeMovementHost host;
  MovementActor& actor = StartHang(host, kHangDropFrame);
  actor.support_layer = 1;
  actors::MoveActorWithCollision(host, kAshley, 0, 0, MoveInput{});
  CHECK(host.bounce_queries.empty());
  CHECK(actor.motion.locomotion_state == kLocomotionHanging);
}

TEST_CASE("Steps blocked before any probe climb nothing for both pads",
          "[actors][actors-movement]") {
  // #294: the original read an unset climb class when the overlap check
  // refused the step before the terrain probe.
  FakeMovementHost host;
  host.resolve = {.status = kStatusBlocked, .climb = 0};
  const MotionVector start{.x = 1000, .y = 0, .z = 1000};

  SECTION("the player's stick") {
    MovementActor& actor = host.Add(kAshley);
    actor.motion.position = start;
    actors::MoveActorWithCollision(host, kAshley, 0, kWalkDistance,
                                   MoveInput{.x = 0, .z = 0, .pad = 0});
    CheckBlockedStep(host, actor, start);
  }
  SECTION("a navigation step") {
    MovementActor& actor = host.Add(kActor);
    actor.motion.position = start;
    actor.ledge_attempts = 3;
    actors::MoveActorWithCollision(
        host, kActor, 0, kWalkDistance,
        MoveInput{.x = static_cast<std::int16_t>(kQuarterTurn), .pad = 1});
    CheckBlockedStep(host, actor, start);
  }
}

TEST_CASE("Airborne rejects keep the support gimmick the move began with",
          "[actors][actors-movement]") {
  // #294: the original restored an unset previous gimmick when an airborne
  // step was rejected.
  FakeMovementHost host;
  MovementActor& actor = host.Add(kAshley);
  actor.motion.movement_mode = kModeAirborne;
  actor.motion.position = {.x = 1000, .y = -100, .z = 1000};
  actor.support_gimmick = 6;
  actor.support_layer = 3;
  actor.placement_layer = 1;
  // A drawn weapon refuses the low climb the resolve reports.
  actor.weapon_drawn = true;
  host.support = {.layer = 0, .gimmick = 9};
  host.support_height = -100;
  host.resolve = {.status = 0, .climb = 2};
  actors::MoveActorWithCollision(host, kAshley, 0, kWalkDistance, MoveInput{});
  CHECK(actor.support_gimmick == 6);
  CHECK(actor.support_layer == 1);
  CHECK(actor.motion.movement_mode == kModeAirborne);
  CHECK(actor.motion.root_motion.x == 0);
  CHECK(actor.motion.root_motion.z == 0);
  CHECK(NoLedgeCalls(host));
}

TEST_CASE("Hang steering turns towards a defined facing on both pads",
          "[actors][actors-movement]") {
  // #294: the original left the facing unset on the stick-distance path.
  FakeMovementHost host;
  MovementActor& actor = host.Add(kAshley);
  actor.motion.locomotion_state = kLocomotionHanging;
  actor.move_facing = kQuarterTurn;
  actor.ledge_attempts = 8;
  host.resolve = {.status = 0, .climb = 3};
  host.tables.hang_animation = 0x44;

  const auto expected_turn = [&](std::uint16_t target) {
    actors::ActorMotion turned = actor.motion;
    actors::TurnActorToOverTime(turned, target, 12);
    return std::make_pair(turned.facing_delta, turned.turn_frames);
  };

  SECTION("the stick steers to the hang's move facing") {
    const auto expected = expected_turn(kQuarterTurn);
    actors::MoveActorWithCollision(host, kAshley, kQuarterTurn, kStickDistance,
                                   MoveInput{.x = 8, .z = 0, .pad = 0});
    CHECK(host.Called("animation 0 68 4"));
    CHECK(host.Called("sound pair 42 43"));
    CHECK(std::make_pair(actor.motion.facing_delta, actor.motion.turn_frames) ==
          expected);
  }
  SECTION("a navigation step restarts the turn to its input facing") {
    // The input's high bits fall outside the facing.
    constexpr std::int16_t kInput = 0x1200;
    constexpr std::int16_t kTarget = 0x200;
    actor.motion.turn_frames = 3;
    actor.motion.facing_delta = 0x100;
    actors::MoveActorWithCollision(host, kAshley, kQuarterTurn, kStickDistance,
                                   MoveInput{.x = kInput, .pad = 1});
    CHECK(host.Called("animation 0 68 4"));
    CHECK(actor.motion.facing_delta == kTarget);
    CHECK(actor.motion.turn_frames == 12);
  }
}

TEST_CASE("Takeoffs release only riders of an existing platform",
          "[actors][actors-movement]") {
  FakeMovementHost host;
  MovementActor& actor = host.Add(kActor);
  const auto takeoff_from = [&](std::uint8_t layer) {
    host.riders.clear();
    actor.motion.movement_mode = kModeTakeoff;
    actor.support_layer = layer;
    actors::UpdateActorJumpAndFall(host, kActor, actor);
    CHECK(actor.motion.movement_mode == kModeAirborne);
    CHECK(actor.support_layer == 0);
    return host.riders;
  };
  CHECK(takeoff_from(3) == std::vector<std::pair<int, int>>{{1, -1}});
  // #294 guard: layers below the platforms or past the room's count name no
  // platform; the original wrote outside the platform array.
  CHECK(takeoff_from(1).empty());
  CHECK(takeoff_from(4).empty());
}

TEST_CASE("Jump movement starts a standing jump or a run-up",
          "[actors][actors-movement]") {
  FakeMovementHost host;
  MovementActor& actor = host.Add(kActor);
  actors::BeginActorJumpMovement(host, kActor, 0x100, 0);
  CHECK(actor.motion.movement_mode == kModeTakeoff);
  CHECK(actor.motion.jump_countdown == 6);
  CHECK(actor.motion.request_direction == 0x100);
  CHECK(host.Called("animation 3 47 4"));

  actor.motion.movement_mode = 0;
  actors::BeginActorJumpMovement(host, kActor, 0x200, kRunSpeed << kFixedShift);
  CHECK(actor.motion.movement_mode == 1);
  CHECK(actor.motion.jump_profile == 2);
  CHECK(actor.jump_started);

  actors::BeginActorJumpMovement(host, kActor, 0x300, 0);
  CHECK(host.Called("direct movement"));
  // An empty slot is skipped with a warning.
  ChannelWarnings warnings{"actors"};
  actors::BeginActorJumpMovement(host, 9, 0, 0);
  CHECK(warnings.messages ==
        std::vector<std::string>{"jump movement skips absent actor 9"});
}

namespace {
[[nodiscard]] MovementActor SampleMovementActor() {
  MovementActor actor;
  actor.motion.slot = kActor;
  actor.motion.position = {.x = 100, .y = -20, .z = 300};
  actor.motion.movement_mode = kModeAirborne;
  actor.skipped = true;
  actor.reaction_state = 5;
  actor.was_drawn = true;
  actor.weapon_drawn = true;
  actor.support_layer = 3;
  actor.placement_layer = 2;
  actor.attach_parent = 4;
  actor.attach_bone = 0xFC;
  actor.base_color_code = 0x2C;
  actor.tile_spare = 7;
  actor.previous_tile = {.x = 1, .layer = 2, .z = 3, .spare = 4};
  actor.grounded_position = {-5, 6, -7};
  actor.previous_facing = 0x123;
  actor.saved_placement = {.x = 8, .layer = 9, .z = 10, .spare = 11};
  actor.wall_contact = true;
  actor.landing_pending = true;
  actor.jump_started = true;
  actor.support_gimmick = 12;
  actor.tick_remainder = 13;
  actor.animation_id = 14;
  actor.collision_state = -15;
  actor.blend_frame = -16;
  actor.collision_radius = 17;
  actor.collision_height = 18;
  actor.queued_animation = 19;
  actor.animation_set = 20;
  actor.contact_kind = 21;
  actor.ledge_height = -22;
  actor.platform_offset_x = -23;
  actor.platform_offset_z = 24;
  actor.pose_reached_end = true;
  actor.platform_anchor = {.x = 25, .y = -26, .z = 27, .w = 2};
  actor.companion_slot = 28;
  actor.ledge_attempts = 6;
  actor.move_facing = 0x456;
  actor.jump_frames = -29;
  actor.momentum_direction = 0x789;
  actor.run_turns = 30;
  actor.request_step = {.x = 31, .y = 32, .z = -33, .w = 34};
  actor.pending_target_x = 35;
  actor.pending_target_z = 36;
  actor.previous_target_x = 37;
  actor.previous_target_z = 38;
  return actor;
}

// The fields the rules only read, which a store leaves to the record.
void SetReadFields(const MovementActor& actor, Actors_ActorRecord& record) {
  record.instance.isSkipped = static_cast<char>(actor.skipped ? -1 : 0);
  record.instance.wasDrawn = actor.was_drawn ? 1 : 0;
  record.instance.weaponDrawn = actor.weapon_drawn ? 1 : 0;
  record.instance.attachParent = actor.attach_parent;
  record.instance.attachBone = actor.attach_bone;
  record.instance.reserved5F = actor.tile_spare;
  record.wallContact = actor.wall_contact ? 1 : 0;
  record.animationId = actor.animation_id;
  record.blendFrame = static_cast<char>(actor.blend_frame);
  record.collisionRadius = actor.collision_radius;
  record.collisionHeight = actor.collision_height;
  record.pose.reachedEnd = actor.pose_reached_end ? -1 : 0;
  record.companionSlot = actor.companion_slot;
  record.turnState.step = {.vx = actor.request_step.x,
                           .vy = actor.request_step.y,
                           .vz = actor.request_step.z,
                           .pad = actor.request_step.w};
}

// The actor table, emptied for one test and then restored.
struct LiveMovementState {
  decltype(std::to_array(Actors_actorRecords)) saved_records =
      std::to_array(Actors_actorRecords);
  std::vector<std::unique_ptr<Actors_ActorRecord>> records;

  LiveMovementState() {
    std::fill(std::begin(Actors_actorRecords), std::end(Actors_actorRecords),
              nullptr);
  }
  LiveMovementState(const LiveMovementState&) = delete;
  LiveMovementState& operator=(const LiveMovementState&) = delete;
  LiveMovementState(LiveMovementState&&) = delete;
  LiveMovementState& operator=(LiveMovementState&&) = delete;
  ~LiveMovementState() {
    std::ranges::copy(saved_records, std::begin(Actors_actorRecords));
  }

  Actors_ActorRecord& AddActor(int actor_id) {
    records.push_back(std::make_unique<Actors_ActorRecord>());
    Actors_ActorRecord& actor = *records.back();
    actor.instance.slot = static_cast<u_char>(actor_id);
    Actors_actorRecords[actor_id] = &actor;
    return actor;
  }
};
}  // namespace

TEST_CASE("Live actor movement stores and loads every record field",
          "[actors][actors-movement]") {
  const MovementActor sample = SampleMovementActor();
  auto record = std::make_unique<Actors_ActorRecord>();
  actors::StoreLiveMovementActor(sample, *record);
  SetReadFields(sample, *record);
  CHECK(actors::LoadLiveMovementActor(*record) == sample);
  CHECK(record->instance.supportLayer == 3);
  CHECK(record->supportGimmick == 12);
  CHECK(record->opaque17FF == 6);
  CHECK(record->instance.reserved50 == -7);

  // The rules never store the fields they only read.
  auto other = std::make_unique<Actors_ActorRecord>();
  other->animationId = 99;
  other->companionSlot = 3;
  actors::StoreLiveMovementActor(sample, *other);
  CHECK(other->animationId == 99);
  CHECK(other->companionSlot == 3);
}

TEST_CASE("Live movement tables copy the module tables",
          "[actors][actors-movement]") {
  const actors::MovementTables tables = actors::LiveMovementTables();
  CHECK(tables.hang_frames.front() == Actors_kJumpHangFrames[0]);
  CHECK(tables.result_animations.back() ==
        Actors_kMotionResultAnimations[actors::kMotionResults - 1]);
  CHECK(tables.hang_animation == Actors_kHangMotionAnimationId);
  CHECK(tables.landing_recovery.back() ==
        Actors_kLandingRecoveryAnimations[actors::kJumpProfiles - 1]);
  for (std::size_t step = 0; step < actors::kGimmickSpinSteps; ++step) {
    CAPTURE(step);
    CHECK(tables.gimmick_spin_steps.at(step) ==
          static_cast<std::int8_t>(Field_kGimmickSpinHeightSteps[step]));
  }
}

TEST_CASE("Live movement adapters run the rules on the actor table",
          "[actors][actors-movement]") {
  LiveMovementState state;
  Actors_ActorRecord& actor = state.AddActor(kActor);
  // A locked animation keeps the jump's animation request off the bare
  // record's sequences.
  actor.animationLocked = 1;
  actors::BeginLiveActorJumpMovement(kActor, 0x100, 0);
  CHECK(actor.instance.movementMode == kModeTakeoff);
  CHECK(actor.instance.jumpCountdown == 6);
  CHECK(actor.turnState.mode == 7);
  CHECK(actor.turnState.direction == 0x100);

  actor.instance.jumpCountdown = 0;
  actor.destinationMode = 0;
  actors::UpdateLiveActorJumpAndFall(kActor, &actor);
  CHECK(actor.instance.movementMode == kModeAirborne);
  CHECK(actor.instance.rootMotion.vy == -30);
}

TEST_CASE("Live movement adapters refuse a missing record or a bad mode",
          "[actors][actors-movement]") {
  ChannelWarnings warnings{"actors"};
  actors::BeginLiveActorJumpToTile(nullptr, PackedTile{}, 0, 0);
  actors::CancelLiveActorTeleportTransition(nullptr);
  actors::UpdateLiveActorPlatformContact(nullptr, 0);
  actors::UpdateLiveActorJumpAndFall(kActor, nullptr);
  auto record = std::make_unique<Actors_ActorRecord>();
  actors::UpdateLiveActorPlatformContact(record.get(), 3);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused("Actor tile jump", "no actor record"),
            Refused("Actor teleport cancel", "no actor record"),
            Refused("Platform contact", "no actor record"),
            Refused("Actor jump and fall", "no actor record"),
            Refused("Platform contact", "mode 3 is not 0 to 2")});
}

TEST_CASE("Promoted actor movement leaves no module address binding",
          "[actors][actors-movement][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  constexpr std::array<std::uint32_t, 14> kRetired{
      0x800A208Cu, 0x800A25ECu, 0x800A2790u, 0x800A291Cu, 0x800A29A0u,
      0x800A2C48u, 0x800A2CD4u, 0x800A48CCu, 0x800A4A88u, 0x800A4D8Cu,
      0x800A4E68u, 0x800A5280u, 0x800A6660u, 0x800AC690u};
  for (const std::uint32_t address : kRetired) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            address) == nullptr);
  }
}
