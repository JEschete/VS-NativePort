#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Ai/ActorCommands.hpp"
#include "Battle/Ai/BossOrbit.hpp"
#include "Battle/Ai/BossOrbitLive.hpp"
#include "Content/ModuleData.h"
#include "Core/Math/ScalarMath.hpp"

namespace {
namespace ai = VS::Battle::Ai;
using ai::CommandIntent;
using ai::CommandOpcode;

constexpr int kCentre = ai::kArenaCentre;
// Q12 one: the trig values on the axes and the world-to-movement scale.
constexpr std::int32_t kQ12 = 0x1000;
constexpr std::int32_t kCentreFixed = kCentre * kQ12;
constexpr int kRadius = ai::kArenaOrbitRadius;
constexpr std::int32_t kOrbitDistance = kRadius * kRadius;
constexpr std::int16_t kHeight = ai::kArenaOrbitHeight;
// The boss's angle around the centre is 0x400 at this x on the centre's z.
constexpr std::int16_t kOrbitX = kCentre - kRadius;
// One whole angle unit of the 16.16 angular velocity.
constexpr std::int32_t kTurnUnit = 0x10000;

// The retreating boss 100 units along +x from the centre: its retreat angle is
// 0x800 (cosine -1, sine 0).
constexpr std::int16_t kRetreatX = kCentre + 100;
constexpr std::int16_t kRetreatY = 50;
constexpr std::int16_t kRetreatAngle = 0x800;
constexpr ai::RetreatStep kStep{.distance = 64, .height = 8};
constexpr ai::RetreatStep kPathEnd{};

// The player at (centre, centre - 500): its angle around the centre is 0, a
// quarter turn short of the orbiting boss.
constexpr ai::BossPlayer kPlayerAhead{.x = kCentre, .z = kCentre - 500};
// The player just short of the boss's angle (a few units below 0x400).
constexpr ai::BossPlayer kPlayerBeside{.x = kCentre - 1000, .z = kCentre - 10};
// The player across the centre, half a turn from kPlayerBeside.
constexpr ai::BossPlayer kPlayerAcross{.x = kCentre + 1000, .z = kCentre + 10};

enum class Kind {
  kCooldown,
  kEngagement,
  kEngage,
  kDisengage,
  kCommand,
  kRecover,
  kProgress,
  kRandom,
  kQueue
};

// One world call and its arguments.
struct Call {
  Kind kind;
  std::int64_t a = 0;
  std::int64_t b = 0;
  std::int64_t c = 0;
  std::int64_t d = 0;

  friend bool operator==(const Call&, const Call&) = default;
};
using Calls = std::vector<Call>;

Call CooldownQuery() { return {Kind::kCooldown}; }
Call EngagementQuery() { return {Kind::kEngagement}; }
Call Engaged() { return {Kind::kEngage}; }
Call Disengaged() { return {Kind::kDisengage}; }
// A command records the boss's movement vector when it was issued.
Call Moved(std::int32_t x, std::int32_t z, std::int32_t y) {
  return {Kind::kCommand, static_cast<int>(CommandIntent::kMoveVector), x, z,
          y};
}
Call Recovered() { return {Kind::kRecover}; }
Call ProgressSet(int progress) { return {Kind::kProgress, progress}; }
Call Drawn() { return {Kind::kRandom}; }
Call Queued(const ai::ActorCommandPacket& packet) {
  return {Kind::kQueue, static_cast<int>(packet.opcode), packet.arg0,
          packet.arg1, packet.arg2};
}

struct FakeBossWorld final : ai::BossOrbitWorld {
  std::vector<ai::RetreatStep> path;
  std::optional<ai::BossPlayer> player;
  std::optional<bool> cooldown_ready = true;
  std::optional<std::uint16_t> threshold = 0;
  std::optional<std::uint16_t> readiness = 1;
  bool engagement_ready = true;
  std::uint8_t progress = 0;
  // Queued random draws; `otherwise` once they run out.
  std::vector<int> draws;
  std::size_t next_draw = 0;
  int otherwise = 1;
  std::optional<ai::BossBattleActor> actor;
  Calls calls;

  [[nodiscard]] std::optional<ai::RetreatStep> RetreatPathStep(
      int step) const override {
    if (step < 0 || static_cast<std::size_t>(step) >= path.size()) {
      return std::nullopt;
    }
    return path.at(static_cast<std::size_t>(step));
  }
  [[nodiscard]] std::optional<ai::BossPlayer> Player() const override {
    return player;
  }
  [[nodiscard]] std::optional<bool> IsFirstCandidateCooldownReady(
      ai::OrbitingBoss& /*boss*/) override {
    calls.push_back(CooldownQuery());
    return cooldown_ready;
  }
  [[nodiscard]] std::optional<std::uint16_t> FirstCandidateThreshold()
      const override {
    return threshold;
  }
  [[nodiscard]] std::optional<std::uint16_t> ReadinessLevel() const override {
    return readiness;
  }
  [[nodiscard]] bool IsEngagementReady(ai::OrbitingBoss& /*boss*/) override {
    calls.push_back(EngagementQuery());
    return engagement_ready;
  }
  void TryEngage(ai::OrbitingBoss& /*boss*/) override {
    calls.push_back(Engaged());
  }
  void Disengage(ai::OrbitingBoss& /*boss*/) override {
    calls.push_back(Disengaged());
  }
  void IssueCommand(ai::OrbitingBoss& boss, CommandIntent intent) override {
    calls.push_back({Kind::kCommand, static_cast<int>(intent), boss.move_x,
                     boss.move_z, boss.move_y});
  }
  void RecoverMovement(ai::OrbitingBoss& /*boss*/) override {
    calls.push_back(Recovered());
  }
  [[nodiscard]] std::uint8_t Progress() const override { return progress; }
  void SetProgress(std::uint8_t value) override {
    calls.push_back(ProgressSet(value));
    progress = value;
  }
  [[nodiscard]] int Random() override {
    calls.push_back(Drawn());
    if (next_draw >= draws.size()) return otherwise;
    return draws.at(next_draw++);
  }
  [[nodiscard]] std::optional<ai::BossBattleActor> BattleActor()
      const override {
    return actor;
  }
  void QueueBattleCommand(const ai::ActorCommandPacket& packet) override {
    calls.push_back(Queued(packet));
  }
};

// The boss mid-retreat, its model flag clear.
ai::OrbitingBoss RetreatingBoss() {
  return {.x = kRetreatX,
          .y = kRetreatY,
          .z = kCentre,
          .model_flag = false,
          .stats = std::nullopt,
          .candidate_count = 3};
}

// Full HP of 100 and a living core.
ai::BossStats FullHealth() {
  return {.current_hp = 100, .max_hp = 100, .core_hp = 2};
}

// The boss on the orbit at angle 0x400 and the orbit's height. A candidate
// makes it ready to act, so it tracks the player; none keeps it away.
ai::OrbitingBoss OrbitBoss(std::int32_t candidates,
                           ai::BossStats stats = FullHealth()) {
  return {.x = kOrbitX,
          .y = kHeight,
          .z = kCentre,
          .model_flag = true,
          .stats = stats,
          .movement_target = 7,
          .candidate_count = candidates};
}

// A started orbit at the orbit's radius and height, about to turn onto 0x400
// at one unit per call.
ai::ArenaOrbit Orbit(std::int32_t velocity, std::int32_t height_offset = 0) {
  return {.heading = 0x3FF,
          .distance_cached = 1,
          .distance_squared = kOrbitDistance,
          .saved_height = kHeight,
          .started = 1,
          .angular_velocity = velocity,
          .height_offset = height_offset};
}

// The boss's command after Orbit(0) tracking kPlayerAhead: heading 0x400
// (cosine 0, sine 1) at radius 1024, one unit above the orbit's height.
Call AimedAtQuarterTurn() {
  return Moved(kCentreFixed, kCentreFixed + (kRadius * kQ12),
               (kHeight + 1) * kQ12);
}

struct Tracked {
  bool result = false;
  ai::OrbitingBoss boss;
  ai::ArenaOrbit orbit;
  Calls calls;
};

// One orbit update of OrbitBoss with the player at `player`.
Tracked Track(bool tracking, ai::ArenaOrbit orbit,
              ai::BossPlayer player = kPlayerAhead) {
  FakeBossWorld world;
  world.player = player;
  auto boss = OrbitBoss(tracking ? 1 : 0);
  const bool result = ai::UpdateBossOrbitTracking(world, boss, orbit);
  return {result, boss, orbit, world.calls};
}
}  // namespace

// F01846 func_800E7698 (BATTLE/BATTLE.PRG/7EE98.c:506).
TEST_CASE("Boss retreats need a model", "[ai][boss-orbit]") {
  FakeBossWorld world;
  world.path = {kStep};
  auto boss = RetreatingBoss();
  boss.model_flag.reset();
  ai::ArenaOrbit orbit;
  const auto before = orbit;
  CHECK(ai::UpdateBossRadialRetreat(world, boss, orbit) == std::nullopt);
  CHECK(boss.candidate_count == 3);
  CHECK(orbit == before);
  CHECK(world.calls.empty());
}

TEST_CASE("Boss retreats start away from the centre", "[ai][boss-orbit]") {
  FakeBossWorld world;
  world.path = {kStep, kStep};
  auto boss = RetreatingBoss();
  ai::ArenaOrbit orbit{.distance_cached = 1, .path_step = 9};
  REQUIRE(ai::UpdateBossRadialRetreat(world, boss, orbit) ==
          std::optional<bool>(false));
  CHECK(boss.candidate_count == 0);
  // Flagged for the step it had reached, before the path restarts.
  CHECK(boss.model_flag == std::optional<bool>(true));
  // FacingAngle(100, 0): the arctangent 0x400 reflected to 0x800. The first
  // step is taken and the cached distance dropped.
  CHECK(orbit == ai::ArenaOrbit{.retreat_phase = 1,
                                .is_orbiting = 1,
                                .path_step = 1,
                                .retreat_angle = kRetreatAngle,
                                .start_x = kRetreatX,
                                .start_z = kCentre});
  // The first step: 64 units back along 0x800, raised by 8.
  const std::int32_t x = (kRetreatX * kQ12) - (64 * kQ12);
  const std::int32_t z = kCentre * kQ12;
  const std::int32_t y = (kRetreatY + 8) * kQ12;
  CHECK(boss.move_x == x);
  CHECK(boss.move_z == z);
  CHECK(boss.move_y == y);
  CHECK(world.calls == Calls{Moved(x, z, y)});
}

TEST_CASE("Boss retreats flag the model for the first 14 steps",
          "[ai][boss-orbit]") {
  const auto flag_at = [](std::uint8_t step) {
    FakeBossWorld world;
    world.path.assign(16, kStep);
    auto boss = RetreatingBoss();
    ai::ArenaOrbit orbit{
        .retreat_phase = 1, .path_step = step, .retreat_angle = kRetreatAngle};
    REQUIRE(ai::UpdateBossRadialRetreat(world, boss, orbit) ==
            std::optional<bool>(false));
    CHECK(orbit.retreat_phase == 1);
    CHECK(orbit.path_step == step + 1);
    CHECK(world.calls.size() == 1);
    return boss.model_flag;
  };
  CHECK(flag_at(0) == std::optional<bool>(true));
  CHECK(flag_at(13) == std::optional<bool>(true));
  CHECK(flag_at(14) == std::optional<bool>(false));
  CHECK(flag_at(15) == std::optional<bool>(false));
}

TEST_CASE("Boss retreats recover at the path end while the player moves",
          "[ai][boss-orbit]") {
  FakeBossWorld world;
  world.path = {kStep, kPathEnd};
  world.player = ai::BossPlayer{.moving = true};
  auto boss = RetreatingBoss();
  ai::ArenaOrbit orbit{.heading = 0x123, .retreat_phase = 1, .path_step = 1};
  const auto before = orbit;
  CHECK(ai::UpdateBossRadialRetreat(world, boss, orbit) ==
        std::optional<bool>(false));
  CHECK(orbit == before);
  CHECK(boss.candidate_count == 0);
  CHECK(boss.model_flag == std::optional<bool>(true));
  CHECK(boss.move_x == 0);
  CHECK(world.calls == Calls{Recovered()});
}

TEST_CASE("Boss retreats end facing the centre once the player stands",
          "[ai][boss-orbit]") {
  FakeBossWorld world;
  world.path = {kStep, kPathEnd};
  world.player = ai::BossPlayer{};
  // 500 units along -z from the centre.
  ai::OrbitingBoss boss{.x = kCentre,
                        .y = kRetreatY,
                        .z = kCentre - 500,
                        .model_flag = true,
                        .stats = std::nullopt};
  ai::ArenaOrbit orbit{.heading = 0x123, .retreat_phase = 1, .path_step = 1};
  CHECK(ai::UpdateBossRadialRetreat(world, boss, orbit) ==
        std::optional<bool>(true));
  CHECK(orbit.retreat_phase == 0);
  CHECK(orbit.path_step == 1);
  // FacingAngle(0, 500): the arctangent 0 reflected to 0xC00.
  CHECK(orbit.heading == 0xC00);
  CHECK(boss.model_flag == std::optional<bool>(false));
  CHECK(boss.move_x == 0);
  CHECK(boss.move_y == 0);
  CHECK(world.calls.empty());
}

TEST_CASE("Boss retreats past phase 1 keep stepping", "[ai][boss-orbit]") {
  FakeBossWorld world;
  // Outside phase 1 the end entry is a step of nothing.
  world.path = {kPathEnd, {.distance = 32, .height = -4}};
  auto boss = RetreatingBoss();
  ai::ArenaOrbit orbit{.retreat_phase = 2,
                       .distance_cached = 1,
                       .path_step = 0,
                       .retreat_angle = 0x400};
  CHECK(ai::UpdateBossRadialRetreat(world, boss, orbit) ==
        std::optional<bool>(false));
  CHECK(world.calls ==
        Calls{Moved(kRetreatX * kQ12, kCentre * kQ12, kRetreatY * kQ12)});
  CHECK(orbit.path_step == 1);
  CHECK(orbit.distance_cached == 0);
  // 32 units along 0x400 (cosine 0, sine 1), lowered by 4.
  world.calls.clear();
  CHECK(ai::UpdateBossRadialRetreat(world, boss, orbit) ==
        std::optional<bool>(false));
  CHECK(world.calls == Calls{Moved(kRetreatX * kQ12, (kCentre + 32) * kQ12,
                                   (kRetreatY - 4) * kQ12)});
  CHECK(orbit.retreat_phase == 2);
  CHECK(orbit.path_step == 2);
  CHECK(orbit.is_orbiting == 0);
}

TEST_CASE("Boss retreats refuse a step past the path or a missing player",
          "[ai][boss-orbit]") {
  const auto run = [](ai::ArenaOrbit orbit,
                      std::optional<ai::BossPlayer> player) {
    FakeBossWorld world;
    world.path = {kStep, kPathEnd};
    world.player = player;
    auto boss = RetreatingBoss();
    const auto result = ai::UpdateBossRadialRetreat(world, boss, orbit);
    CHECK(world.calls.empty());
    return result;
  };
  CHECK(run({.retreat_phase = 1, .path_step = 2}, ai::BossPlayer{}) ==
        std::nullopt);
  CHECK(run({.retreat_phase = 2, .path_step = 2}, ai::BossPlayer{}) ==
        std::nullopt);
  CHECK(run({.retreat_phase = 1, .path_step = 1}, std::nullopt) ==
        std::nullopt);
  CHECK(run({.retreat_phase = 1, .path_step = 1}, ai::BossPlayer{}) ==
        std::optional<bool>(true));
  // A path without entries refuses even the first step.
  FakeBossWorld world;
  auto boss = RetreatingBoss();
  ai::ArenaOrbit orbit;
  CHECK(ai::UpdateBossRadialRetreat(world, boss, orbit) == std::nullopt);
  CHECK(world.calls.empty());
}

// F01847 func_800E78F4 (BATTLE/BATTLE.PRG/7EE98.c:553).
TEST_CASE("Boss actions need a candidate off cooldown", "[ai][boss-orbit]") {
  FakeBossWorld world;
  ai::OrbitingBoss boss;
  CHECK(ai::IsBossActionReady(world, boss) == std::optional<bool>(false));
  CHECK(world.calls.empty());
  boss.candidate_count = 1;
  world.cooldown_ready.reset();
  CHECK(ai::IsBossActionReady(world, boss) == std::nullopt);
  // A cooling candidate never reads the threshold.
  world.cooldown_ready = false;
  world.threshold.reset();
  CHECK(ai::IsBossActionReady(world, boss) == std::optional<bool>(false));
  CHECK(world.calls == Calls{CooldownQuery(), CooldownQuery()});
}

TEST_CASE("Boss actions are ready below the room's readiness",
          "[ai][boss-orbit]") {
  const auto ready = [](std::optional<std::uint16_t> threshold,
                        std::optional<std::uint16_t> readiness) {
    FakeBossWorld world;
    world.threshold = threshold;
    world.readiness = readiness;
    ai::OrbitingBoss boss;
    boss.candidate_count = 1;
    const auto result = ai::IsBossActionReady(world, boss);
    CHECK(world.calls == Calls{CooldownQuery()});
    return result;
  };
  CHECK(ready(4, 5) == std::optional<bool>(true));
  CHECK(ready(5, 5) == std::optional<bool>(false));
  CHECK(ready(6, 5) == std::optional<bool>(false));
  // Both are unsigned halfwords.
  CHECK(ready(1, 0x8000) == std::optional<bool>(true));
  CHECK(ready(0x8000, 1) == std::optional<bool>(false));
  CHECK(ready(std::nullopt, 5) == std::nullopt);
  CHECK(ready(4, std::nullopt) == std::nullopt);
}

// F01848 func_800E7960 (BATTLE/BATTLE.PRG/7EE98.c:599).
TEST_CASE("Boss orbits refuse a boss without a model, stats or player",
          "[ai][boss-orbit]") {
  FakeBossWorld world;
  world.player = kPlayerAhead;
  auto orbit = Orbit(0);
  const auto before = orbit;
  auto boss = OrbitBoss(1);
  boss.model_flag.reset();
  CHECK_FALSE(ai::UpdateBossOrbitTracking(world, boss, orbit));
  boss = OrbitBoss(1);
  boss.stats.reset();
  CHECK_FALSE(ai::UpdateBossOrbitTracking(world, boss, orbit));
  CHECK(orbit == before);
  CHECK(world.calls.empty());
  boss = OrbitBoss(1);
  world.player.reset();
  CHECK_FALSE(ai::UpdateBossOrbitTracking(world, boss, orbit));
  CHECK(world.calls.empty());
  world.player = kPlayerAhead;
  world.cooldown_ready.reset();
  CHECK_FALSE(ai::UpdateBossOrbitTracking(world, boss, orbit));
  CHECK(world.calls == Calls{CooldownQuery()});
}

TEST_CASE("Boss orbits start from the boss's position", "[ai][boss-orbit]") {
  FakeBossWorld world;
  world.player = kPlayerAhead;
  auto boss = OrbitBoss(1);
  boss.y = 100;
  ai::ArenaOrbit orbit{
      .heading = 0x123, .distance_squared = 5, .saved_height = 300};
  REQUIRE(ai::UpdateBossOrbitTracking(world, boss, orbit));
  // FacingAngle(1024, 0) is 0x800, stepped by the new velocity's whole unit.
  CHECK(orbit.heading == 0x801);
  CHECK(orbit.angular_velocity == kTurnUnit);
  // The boss's height, eased by a step.
  CHECK(orbit.saved_height == 112);
  CHECK(orbit.started == 1);
  CHECK(orbit.distance_cached == 1);
  CHECK(orbit.distance_squared == kOrbitDistance);
  CHECK(boss.model_flag == std::optional<bool>(false));
}

TEST_CASE("Boss orbits keep a started orbit and measure an uncached distance",
          "[ai][boss-orbit]") {
  auto orbit = Orbit(0);
  orbit.distance_cached = 0;
  orbit.distance_squared = 900 * 900;
  orbit.saved_height = 100;
  const auto tracked = Track(true, orbit);
  REQUIRE(tracked.result);
  CHECK(tracked.orbit.heading == 0x400);
  CHECK(tracked.orbit.saved_height == 112);
  CHECK(tracked.orbit.distance_cached == 1);
  CHECK(tracked.orbit.distance_squared == kOrbitDistance);
}

namespace {
// One update of an orbit cached `radius` from the centre; the boss itself sits
// at 1024, so the cached distance is used as stored.
Tracked TrackAtRadius(int radius) {
  auto orbit = Orbit(0);
  orbit.distance_squared = radius * radius;
  auto tracked = Track(true, orbit);
  REQUIRE(tracked.result);
  CHECK(tracked.orbit.distance_cached == 1);
  return tracked;
}
}  // namespace

TEST_CASE("Boss orbits step the table-root radius toward 1024",
          "[ai][boss-orbit]") {
  // The table-sampled root is approximate: it reads 900^2 as 898 and 1100^2 as
  // 1093 before the 32-unit step, but 960^2 exactly.
  REQUIRE(VS::Core::Math::SquareRootInteger(900 * 900) == 898);
  REQUIRE(VS::Core::Math::SquareRootInteger(1100 * 1100) == 1093);
  REQUIRE(VS::Core::Math::SquareRootInteger(960 * 960) == 960);
  CHECK(TrackAtRadius(900).orbit.distance_squared == 930 * 930);
  CHECK(TrackAtRadius(960).orbit.distance_squared == 992 * 992);
  CHECK(TrackAtRadius(1100).orbit.distance_squared == 1061 * 1061);
  // The aim point lies on the eased radius.
  CHECK(TrackAtRadius(900).boss.move_z == kCentreFixed + (930 * kQ12));
}

TEST_CASE("Boss orbits snap the radius within a step", "[ai][boss-orbit]") {
  CHECK(TrackAtRadius(1010).orbit.distance_squared == kOrbitDistance);
  CHECK(TrackAtRadius(1000).orbit.distance_squared == kOrbitDistance);
  CHECK(TrackAtRadius(990).orbit.distance_squared == kOrbitDistance);
  CHECK(TrackAtRadius(kRadius).orbit.distance_squared == kOrbitDistance);
}

TEST_CASE("Boss orbits ease the saved height toward 160", "[ai][boss-orbit]") {
  const auto eased = [](std::int16_t height) {
    auto orbit = Orbit(0);
    orbit.saved_height = height;
    const auto tracked = Track(true, orbit);
    REQUIRE(tracked.result);
    return tracked.orbit.saved_height;
  };
  CHECK(eased(100) == 112);
  CHECK(eased(144) == 156);
  CHECK(eased(150) == kHeight);
  CHECK(eased(170) == kHeight);
  CHECK(eased(176) == 164);
  CHECK(eased(200) == 188);
}

namespace {
std::int32_t VelocityAfter(bool tracking, std::int32_t velocity,
                           ai::BossPlayer player = kPlayerAhead) {
  const auto tracked = Track(tracking, Orbit(velocity), player);
  REQUIRE(tracked.result);
  return tracked.orbit.angular_velocity;
}
}  // namespace

TEST_CASE("Boss orbits accelerate toward the player when tracking",
          "[ai][boss-orbit]") {
  // Aligned: a unit along the velocity, plus an eighth beyond 0x280000.
  CHECK(VelocityAfter(true, 0) == kTurnUnit);
  CHECK(VelocityAfter(true, 0x300000) == 0x370000);
  CHECK(VelocityAfter(true, 0x800000) == 0x820000);
  // Turning away: a unit and a quarter, reversed.
  CHECK(VelocityAfter(true, -kTurnUnit) == 0x4000);
}

TEST_CASE("Boss orbits accelerate away from the player otherwise",
          "[ai][boss-orbit]") {
  // Off its heading and not aligned: reversed.
  CHECK(VelocityAfter(false, 0) == -kTurnUnit);
  CHECK(VelocityAfter(false, 0x100000) == 0xB0000);
  // Aligned and fast.
  CHECK(VelocityAfter(false, -0x800000) == -0x820000);
  // Already facing its heading: not reversed even though not aligned.
  CHECK(VelocityAfter(false, -0x100000, kPlayerAcross) == -0x150000);
}

TEST_CASE("Boss orbits aim at the orbit point raised by the height offset",
          "[ai][boss-orbit]") {
  const auto tracked = Track(true, Orbit(0));
  REQUIRE(tracked.result);
  CHECK(tracked.orbit.heading == 0x400);
  // Closing in: 0x10000 * -80 / 0x400, a fall of one unit.
  CHECK(tracked.orbit.height_offset == -1);
  CHECK(tracked.boss.movement_target == 0);
  const Call aimed = AimedAtQuarterTurn();
  CHECK(tracked.boss.move_x == aimed.b);
  CHECK(tracked.boss.move_z == aimed.c);
  CHECK(tracked.boss.move_y == aimed.d);
  CHECK(tracked.calls ==
        Calls{CooldownQuery(), EngagementQuery(), Drawn(), aimed});
}

TEST_CASE("Boss orbits cap the aim height", "[ai][boss-orbit]") {
  const auto aim_y = [](std::int16_t height) {
    auto orbit = Orbit(0);
    orbit.saved_height = height;
    const auto tracked = Track(true, orbit);
    REQUIRE(tracked.result);
    REQUIRE(tracked.orbit.height_offset == -1);
    return tracked.boss.move_y;
  };
  // Eased to 253 and 388, then one unit up.
  CHECK(aim_y(265) == 254 * kQ12);
  CHECK(aim_y(400) == 0xFF000);
}

namespace {
std::int32_t OffsetAfter(bool tracking, std::int32_t velocity,
                         std::int32_t offset,
                         ai::BossPlayer player = kPlayerAhead) {
  const auto tracked = Track(tracking, Orbit(velocity, offset), player);
  REQUIRE(tracked.result);
  return tracked.orbit.height_offset;
}
}  // namespace

TEST_CASE("Boss orbits steer the height offset", "[ai][boss-orbit]") {
  CHECK(OffsetAfter(true, 0, 0) == -1);
  // Stray: 0xB0000 * 10 / 11.
  CHECK(OffsetAfter(false, 0x100000, 0) == 10);
  // Pulling away: 0x110000 * 300 / 0x400.
  CHECK(OffsetAfter(false, -0x100000, 0) == 4);
  // The product wraps in 32 bits: 0x820000 * 300 / 0x400 falls by 26.
  CHECK(OffsetAfter(false, -0x800000, 0) == -26);
  CHECK(OffsetAfter(false, 0x100000, 600) == 500);
  CHECK(OffsetAfter(false, 0x100000, -200) == -100);
}

TEST_CASE("Boss orbits limit only a falling height correction",
          "[ai][boss-orbit]") {
  // Closing in a few units from the player: the fall is clamped to four
  // times the speed, 0x110000 * 4 >> 16.
  CHECK(VelocityAfter(true, 0x100000, kPlayerBeside) == 0x110000);
  CHECK(OffsetAfter(true, 0x100000, 0, kPlayerBeside) == -68);
  // Pulling away a few units from the opposite heading: the over-limit rise
  // drops to nothing.
  CHECK(VelocityAfter(false, 0x100000, kPlayerAcross) == 0x110000);
  CHECK(OffsetAfter(false, 0x100000, 10, kPlayerAcross) == 10);
}

namespace {
// The orbit after one update of a boss away from the player at `hp` of 100.
ai::ArenaOrbit HealthPhaseAfter(std::int16_t hp, std::uint8_t phase,
                                std::int16_t cooldown = 0) {
  FakeBossWorld world;
  world.player = kPlayerAhead;
  auto stats = FullHealth();
  stats.current_hp = hp;
  auto boss = OrbitBoss(0, stats);
  auto orbit = Orbit(0);
  orbit.health_phase = phase;
  orbit.cooldown = cooldown;
  REQUIRE(ai::UpdateBossOrbitTracking(world, boss, orbit));
  CHECK(orbit.cooldown == cooldown);
  return orbit;
}
}  // namespace

TEST_CASE("Boss orbits request a move at 75% and 25% HP", "[ai][boss-orbit]") {
  const auto first = HealthPhaseAfter(74, 0);
  CHECK(first.health_phase == 1);
  CHECK(first.move_requested == 1);
  const auto second = HealthPhaseAfter(24, 1);
  CHECK(second.health_phase == 2);
  CHECK(second.move_requested == 1);
  // One phase per update.
  CHECK(HealthPhaseAfter(10, 0).health_phase == 1);
}

TEST_CASE("Boss orbits ignore health marks not crossed or while cooling down",
          "[ai][boss-orbit]") {
  // Exactly at a mark, above it, after both, or while cooling down: nothing.
  for (const auto& orbit : {HealthPhaseAfter(75, 0), HealthPhaseAfter(25, 1),
                            HealthPhaseAfter(30, 1), HealthPhaseAfter(10, 2),
                            HealthPhaseAfter(74, 0, 3)}) {
    CHECK(orbit.move_requested == 0);
  }
  CHECK(HealthPhaseAfter(75, 0).health_phase == 0);
  CHECK(HealthPhaseAfter(30, 1).health_phase == 1);
  CHECK(HealthPhaseAfter(10, 2).health_phase == 2);
  CHECK(HealthPhaseAfter(74, 0, 3).health_phase == 0);
}

namespace {
// One update of a tracking boss with a draw of `draw`; whether it drew and
// requested a move.
struct Drew {
  bool drew = false;
  std::int16_t move_requested = 0;

  friend bool operator==(const Drew&, const Drew&) = default;
};

Drew RandomMove(int draw, ai::ArenaOrbit orbit, bool tracking = true) {
  FakeBossWorld world;
  world.player = kPlayerAhead;
  world.draws = {draw};
  auto boss = OrbitBoss(tracking ? 1 : 0);
  REQUIRE(ai::UpdateBossOrbitTracking(world, boss, orbit));
  return {world.next_draw == 1, orbit.move_requested};
}

constexpr int kMoveDraw = 512;
}  // namespace

TEST_CASE("Boss orbits request a move at random while slow and low",
          "[ai][boss-orbit]") {
  CHECK(RandomMove(kMoveDraw, Orbit(0)) == Drew{true, 1});
  CHECK(RandomMove(1, Orbit(0)) == Drew{true, 0});
  CHECK(RandomMove(kMoveDraw, Orbit(0, 29)) == Drew{true, 1});
  CHECK(RandomMove(kMoveDraw, Orbit(0x20000)) == Drew{true, 1});
}

TEST_CASE("Boss orbits draw no move away, high, fast or cooling down",
          "[ai][boss-orbit]") {
  CHECK(RandomMove(kMoveDraw, Orbit(0), false) == Drew{false, 0});
  CHECK(RandomMove(kMoveDraw, Orbit(0, 30)) == Drew{false, 0});
  // 0x30000 reaches 0x40000 after its unit of acceleration.
  CHECK(RandomMove(kMoveDraw, Orbit(0x30000)) == Drew{false, 0});
  auto cooling = Orbit(0);
  cooling.cooldown = 1;
  CHECK(RandomMove(kMoveDraw, cooling) == Drew{false, 0});
}

TEST_CASE("Boss orbit health marks request a move without a draw",
          "[ai][boss-orbit]") {
  auto marked = Orbit(0);
  FakeBossWorld world;
  world.player = kPlayerAhead;
  auto stats = FullHealth();
  stats.current_hp = 50;
  auto boss = OrbitBoss(1, stats);
  REQUIRE(ai::UpdateBossOrbitTracking(world, boss, marked));
  CHECK(marked.move_requested == 1);
  CHECK(world.next_draw == 0);
}

// F01849 func_800E7F8C (BATTLE/BATTLE.PRG/7EE98.c:753).
namespace {
constexpr std::uint8_t kOrbiting = 0;
constexpr std::uint8_t kMoveRequested = 1;
constexpr std::uint8_t kRetreating = 4;
constexpr std::uint8_t kRetreatEnded = 6;
static_assert(kOrbiting ==
              static_cast<std::uint8_t>(ai::ArenaProgress::kOrbiting));
static_assert(kMoveRequested ==
              static_cast<std::uint8_t>(ai::ArenaProgress::kMoveRequested));
static_assert(kRetreating ==
              static_cast<std::uint8_t>(ai::ArenaProgress::kRetreating));
static_assert(kRetreatEnded ==
              static_cast<std::uint8_t>(ai::ArenaProgress::kRetreatEnded));

// The world is neither copyable nor movable, so each test prepares its own.
void PrepareBehaviour(FakeBossWorld& world, std::uint8_t progress) {
  world.player = kPlayerAhead;
  world.progress = progress;
}
}  // namespace

TEST_CASE("Orbiting bosses refuse an unreadable readiness",
          "[ai][boss-orbit]") {
  FakeBossWorld world;
  PrepareBehaviour(world, kMoveRequested);
  world.cooldown_ready.reset();
  auto boss = OrbitBoss(1);
  auto orbit = Orbit(0);
  CHECK_FALSE(ai::UpdateOrbitingBossBehavior(world, boss, orbit));
  CHECK(world.calls == Calls{CooldownQuery()});
}

TEST_CASE("Orbiting bosses engage when an action is ready",
          "[ai][boss-orbit]") {
  const auto run = [](std::int32_t candidates, bool cooled) {
    FakeBossWorld world;
    PrepareBehaviour(world, kMoveRequested);
    world.cooldown_ready = cooled;
    auto boss = OrbitBoss(candidates);
    auto orbit = Orbit(0);
    REQUIRE(ai::UpdateOrbitingBossBehavior(world, boss, orbit));
    return world.calls;
  };
  CHECK(run(1, true) ==
        Calls{CooldownQuery(), Engaged(), ProgressSet(kMoveRequested)});
  CHECK(run(1, false) == Calls{CooldownQuery(), ProgressSet(kMoveRequested)});
  CHECK(run(0, true) == Calls{ProgressSet(kMoveRequested)});
}

TEST_CASE("Orbiting bosses count the move cooldown down", "[ai][boss-orbit]") {
  const auto cooldown_after = [](std::int16_t cooldown) {
    FakeBossWorld world;
    PrepareBehaviour(world, kMoveRequested);
    auto boss = OrbitBoss(0);
    auto orbit = Orbit(0);
    orbit.cooldown = cooldown;
    REQUIRE(ai::UpdateOrbitingBossBehavior(world, boss, orbit));
    return orbit.cooldown;
  };
  CHECK(cooldown_after(5) == 4);
  CHECK(cooldown_after(1) == 0);
  CHECK(cooldown_after(0) == 0);
  CHECK(cooldown_after(-3) == -3);
}

TEST_CASE("Orbiting bosses keep other progress as it is", "[ai][boss-orbit]") {
  for (const std::uint8_t progress :
       {kMoveRequested, std::uint8_t{2}, std::uint8_t{3}, kRetreatEnded}) {
    FakeBossWorld world;
    PrepareBehaviour(world, progress);
    auto boss = OrbitBoss(0);
    auto orbit = Orbit(0);
    orbit.cooldown = 3;
    orbit.plan_saved = 1;
    orbit.is_orbiting = 1;
    const auto boss_before = boss;
    auto orbit_before = orbit;
    orbit_before.cooldown = 2;
    REQUIRE(ai::UpdateOrbitingBossBehavior(world, boss, orbit));
    CHECK(boss == boss_before);
    CHECK(orbit == orbit_before);
    CHECK(world.calls == Calls{ProgressSet(progress)});
  }
}

TEST_CASE("Orbiting bosses restore a saved plan and circle",
          "[ai][boss-orbit]") {
  FakeBossWorld world;
  PrepareBehaviour(world, kOrbiting);
  auto boss = OrbitBoss(0);
  auto orbit = Orbit(0);
  orbit.plan_saved = 1;
  orbit.saved_plan_count = 3;
  orbit.is_orbiting = 1;
  REQUIRE(ai::UpdateOrbitingBossBehavior(world, boss, orbit));
  CHECK(boss.candidate_count == 3);
  CHECK(orbit.plan_saved == 0);
  CHECK(orbit.saved_plan_count == 3);
  CHECK(orbit.is_orbiting == 0);
  CHECK(orbit.heading == 0x400);
  // Readiness counted no candidate; the orbit ran with the restored plan.
  CHECK(world.calls == Calls{CooldownQuery(), EngagementQuery(), Drawn(),
                             AimedAtQuarterTurn(), ProgressSet(kOrbiting)});
}

namespace {
struct Behaved {
  bool result = false;
  ai::OrbitingBoss boss;
  ai::ArenaOrbit orbit;
  Calls calls;
};

// An orbiting boss with two candidates, a requested move and a stale saved
// plan.
Behaved RequestedMove(std::int16_t cooldown, std::int16_t core_hp) {
  FakeBossWorld world;
  PrepareBehaviour(world, kOrbiting);
  auto stats = FullHealth();
  stats.core_hp = core_hp;
  auto boss = OrbitBoss(2, stats);
  auto orbit = Orbit(0);
  orbit.move_requested = 1;
  orbit.cooldown = cooldown;
  orbit.saved_plan_count = 9;
  const bool result = ai::UpdateOrbitingBossBehavior(world, boss, orbit);
  return {result, boss, orbit, world.calls};
}
}  // namespace

TEST_CASE("Orbiting bosses start a requested move and save the plan",
          "[ai][boss-orbit]") {
  const auto moved = RequestedMove(0, 2);
  REQUIRE(moved.result);
  CHECK(moved.orbit.cooldown == ai::kArenaMoveCooldown);
  CHECK(moved.orbit.move_requested == 0);
  CHECK(moved.orbit.saved_plan_count == 2);
  CHECK(moved.boss.candidate_count == 0);
  CHECK(moved.orbit.plan_saved == 1);
  CHECK(moved.calls == Calls{CooldownQuery(), Engaged(), CooldownQuery(),
                             EngagementQuery(), Drawn(), AimedAtQuarterTurn(),
                             ProgressSet(kMoveRequested)});
  // The cooldown counts down before the request is tested.
  CHECK(RequestedMove(1, 2).orbit.cooldown == ai::kArenaMoveCooldown);
}

TEST_CASE("Orbiting bosses hold a requested move while cooling or crippled",
          "[ai][boss-orbit]") {
  for (const auto& held : {RequestedMove(2, 2), RequestedMove(0, 1)}) {
    REQUIRE(held.result);
    CHECK(held.orbit.move_requested == 1);
    CHECK(held.orbit.saved_plan_count == 9);
    CHECK(held.orbit.plan_saved == 0);
    CHECK(held.boss.candidate_count == 2);
    CHECK(held.calls.back() == ProgressSet(kOrbiting));
  }
  CHECK(RequestedMove(2, 2).orbit.cooldown == 1);
  CHECK(RequestedMove(0, 1).orbit.cooldown == 0);
}

TEST_CASE("Orbiting bosses refuse an orbit the tracking refuses",
          "[ai][boss-orbit]") {
  FakeBossWorld world;
  PrepareBehaviour(world, kOrbiting);
  auto boss = OrbitBoss(0);
  boss.model_flag.reset();
  auto orbit = Orbit(0);
  orbit.move_requested = 1;
  CHECK_FALSE(ai::UpdateOrbitingBossBehavior(world, boss, orbit));
  CHECK(world.calls.empty());
  CHECK(orbit.cooldown == 0);
}

namespace {
constexpr std::int16_t kSavedPlanCount = 2;

// A boss retreating along 0x800 from step `step` of a three-step path, after
// orbiting with a saved plan of two candidates.
ai::ArenaOrbit RetreatOrbit(std::uint8_t step) {
  return {.retreat_phase = 1,
          .is_orbiting = 1,
          .path_step = step,
          .move_requested = 1,
          .retreat_angle = kRetreatAngle,
          .saved_height = kHeight,
          .plan_saved = 1,
          .saved_plan_count = kSavedPlanCount,
          .started = 1,
          .angular_velocity = 0x30000,
          .height_offset = 40};
}

// A retreating boss whose plan was saved when its move started.
ai::OrbitingBoss PlanlessBoss() {
  auto boss = RetreatingBoss();
  boss.candidate_count = 0;
  return boss;
}

// The player standing still where the retreat ends.
ai::BossPlayer StandingPlayer() {
  return {.x = 0x700, .y = -100, .z = 0x900, .collision_height = 301};
}

void PrepareRetreat(FakeBossWorld& world, std::int16_t core_hp) {
  PrepareBehaviour(world, kRetreating);
  world.path = {kStep, kStep, kStep, kPathEnd};
  world.player = StandingPlayer();
  world.actor = ai::BossBattleActor{
      .core_hp = core_hp,
      .command = {.opcode = CommandOpcode::kNone,
                  .arg0 = 0x11112222,
                  .arg1 = 0x33334444,
                  .arg2 = static_cast<std::int32_t>(0xABCD5555U)}};
}
}  // namespace

TEST_CASE("Retreating bosses disengage and step along the path",
          "[ai][boss-orbit]") {
  FakeBossWorld world;
  PrepareRetreat(world, 2);
  auto boss = PlanlessBoss();
  auto orbit = RetreatOrbit(1);
  REQUIRE(ai::UpdateOrbitingBossBehavior(world, boss, orbit));
  CHECK(orbit.started == 0);
  CHECK(orbit.path_step == 2);
  CHECK(orbit.angular_velocity == 0x30000);
  CHECK(orbit.height_offset == 40);
  CHECK(orbit.move_requested == 1);
  CHECK(world.calls == Calls{Disengaged(),
                             Moved((kRetreatX - 64) * kQ12, kCentre * kQ12,
                                   (kRetreatY + 8) * kQ12),
                             ProgressSet(kRetreating)});
}

TEST_CASE("Retreating bosses keep retreating while the player moves",
          "[ai][boss-orbit]") {
  FakeBossWorld world;
  PrepareRetreat(world, 2);
  auto player = StandingPlayer();
  player.moving = true;
  world.player = player;
  auto boss = PlanlessBoss();
  auto orbit = RetreatOrbit(3);
  REQUIRE(ai::UpdateOrbitingBossBehavior(world, boss, orbit));
  CHECK(orbit.retreat_phase == 1);
  CHECK(orbit.path_step == 3);
  CHECK(orbit.height_offset == 40);
  CHECK(world.calls ==
        Calls{Disengaged(), Recovered(), ProgressSet(kRetreating)});
}

namespace {
// Ends the retreat and checks what every ending shares.
ai::ArenaOrbit EndRetreat(FakeBossWorld& world, ai::OrbitingBoss& boss,
                          std::uint8_t is_orbiting) {
  auto orbit = RetreatOrbit(3);
  orbit.is_orbiting = is_orbiting;
  REQUIRE(ai::UpdateOrbitingBossBehavior(world, boss, orbit));
  CHECK(orbit.started == 0);
  CHECK(orbit.retreat_phase == 0);
  CHECK(orbit.is_orbiting == is_orbiting);
  CHECK(orbit.height_offset == 0);
  CHECK(orbit.move_requested == 0);
  CHECK(orbit.angular_velocity == 0);
  return orbit;
}
}  // namespace

TEST_CASE("Retreating bosses attack the player's position at the path end",
          "[ai][boss-orbit]") {
  FakeBossWorld world;
  PrepareRetreat(world, 2);
  auto boss = PlanlessBoss();
  const auto orbit = EndRetreat(world, boss, 1);
  // Row 4, column 0; the player's x and its y raised by half of 301; its z
  // under the previous command's high halfword.
  const ai::ActorCommandPacket attack{
      .opcode = CommandOpcode::kAttack,
      .arg0 = 4,
      .arg1 = static_cast<std::int32_t>(0xFF060700U),
      .arg2 = static_cast<std::int32_t>(0xABCD0900U)};
  CHECK(world.calls ==
        Calls{Disengaged(), Queued(attack), ProgressSet(kRetreating)});
  CHECK(orbit.saved_height == kRetreatY);
  CHECK(boss.candidate_count == kSavedPlanCount);
  CHECK(orbit.plan_saved == 0);
}

TEST_CASE("Retreating bosses with a dead core end without attacking",
          "[ai][boss-orbit]") {
  FakeBossWorld world;
  PrepareRetreat(world, 1);
  auto boss = PlanlessBoss();
  const auto orbit = EndRetreat(world, boss, 1);
  CHECK(world.calls == Calls{Disengaged(), ProgressSet(kRetreating)});
  CHECK(orbit.saved_height == kRetreatY);
  CHECK(boss.candidate_count == kSavedPlanCount);
  CHECK(orbit.plan_saved == 0);
}

TEST_CASE("Interrupted retreats mark the retreat ended", "[ai][boss-orbit]") {
  FakeBossWorld world;
  PrepareRetreat(world, 2);
  auto boss = PlanlessBoss();
  const auto orbit = EndRetreat(world, boss, 0);
  CHECK(world.calls == Calls{Disengaged(), ProgressSet(kRetreatEnded)});
  // The plan stays saved and the height as it was.
  CHECK(orbit.saved_height == kHeight);
  CHECK(boss.candidate_count == 0);
  CHECK(orbit.plan_saved == 1);
}

TEST_CASE("Retreating bosses refuse a missing actor or path step",
          "[ai][boss-orbit]") {
  FakeBossWorld world;
  PrepareRetreat(world, 2);
  world.actor.reset();
  auto boss = PlanlessBoss();
  auto orbit = RetreatOrbit(3);
  CHECK_FALSE(ai::UpdateOrbitingBossBehavior(world, boss, orbit));
  CHECK(world.calls == Calls{Disengaged()});
  world.calls.clear();
  orbit = RetreatOrbit(4);
  CHECK_FALSE(ai::UpdateOrbitingBossBehavior(world, boss, orbit));
  CHECK(world.calls == Calls{Disengaged()});
}

TEST_CASE("Orbiting boss keeps its original module entries",
          "[ai][boss-orbit][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [symbols](unsigned address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  CHECK(resolve(0x800E7698U) ==
        reinterpret_cast<void*>(&ai::UpdateLiveBossRadialRetreat));
  CHECK(resolve(0x800E78F4U) ==
        reinterpret_cast<void*>(&ai::IsLiveBossActionReady));
  CHECK(resolve(0x800E7960U) ==
        reinterpret_cast<void*>(&ai::UpdateLiveBossOrbitTracking));
  CHECK(resolve(0x800E7F8CU) ==
        reinterpret_cast<void*>(&ai::UpdateLiveOrbitingBossBehavior));
}
