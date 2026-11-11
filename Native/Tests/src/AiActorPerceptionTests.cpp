#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Ai/ActorPerception.hpp"
#include "Battle/Ai/ActorPerceptionLive.hpp"
#include "Content/ModuleData.h"
#include "Core/Math/Distance.hpp"

extern "C" {
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
}

namespace {
namespace ai = VS::Battle::Ai;
using VS::Core::Math::Position3;

constexpr int kObserver = 1;
constexpr int kTarget = 2;
constexpr int kBlocker = 3;
// A view cosine every direction meets, and one none does (compared signed).
constexpr auto kAlwaysInView = static_cast<std::uint32_t>(-4096);
constexpr std::uint32_t kNeverInView = 4097;
constexpr std::uint32_t kFar = 1U << 30;

constexpr std::uint32_t Tile(std::uint32_t x, std::uint32_t z,
                             std::uint32_t layer = 0) {
  return x | (layer << 8) | (z << 16);
}

struct TerrainCall {
  Position3 from;
  Position3 to;
  int from_height = 0;
  int to_height = 0;

  friend bool operator==(const TerrainCall&, const TerrainCall&) = default;
};

// The fake's state, kept apart so tests can reset it: the interface cannot be
// assigned.
struct TargetWorldState {
  std::array<std::optional<ai::PerceivedModel>, ai::kPerceptionSlots> models{};
  std::optional<ai::ObserverPose> pose = ai::ObserverPose{};
  std::array<std::optional<ai::PerceptionShape>, ai::kPerceptionSlots>
      ai_shapes{};
  std::array<std::optional<ai::PerceptionShape>, ai::kPerceptionSlots>
      frame_shapes{};
  std::array<bool, ai::kPerceptionSlots> gimmicks{};
  bool has_tile_flags = true;
  std::vector<std::pair<int, int>> marked_tiles;
  bool terrain_blocks = false;
  mutable std::vector<TerrainCall> terrain;
  std::vector<ai::ListedPerceptionActor> listed;

  TargetWorldState() {
    models.at(kTarget) =
        ai::PerceivedModel{.position = {.x = 0, .y = 0, .z = 64},
                           .tile = Tile(4, 5),
                           .last_tile = Tile(9, 9)};
  }
};

struct FakeTargetWorld final : ai::TargetPerceptionWorld, TargetWorldState {
  void Reset() { static_cast<TargetWorldState&>(*this) = TargetWorldState{}; }

  [[nodiscard]] std::optional<ai::PerceivedModel> Model(
      int target) const override {
    return models.at(target);
  }
  [[nodiscard]] std::optional<ai::ObserverPose> ObserverModel() const override {
    return pose;
  }
  [[nodiscard]] std::optional<ai::PerceptionShape> AiStateShape(
      int target) const override {
    return ai_shapes.at(target);
  }
  [[nodiscard]] std::optional<ai::PerceptionShape> FrameShape(
      int target) const override {
    return frame_shapes.at(target);
  }
  [[nodiscard]] bool HasGimmick(int target) const override {
    return gimmicks.at(target);
  }
  [[nodiscard]] std::optional<bool> IsMarkedTile(
      VS::Core::Math::TileCoordinate tile) const override {
    if (!has_tile_flags) return std::nullopt;
    return std::ranges::find(marked_tiles,
                             std::pair<int, int>{tile.x, tile.z}) !=
           marked_tiles.end();
  }
  [[nodiscard]] bool IsTerrainSegmentBlocked(Position3 from, Position3 to,
                                             int from_height,
                                             int to_height) const override {
    terrain.push_back({from, to, from_height, to_height});
    return terrain_blocks;
  }
  [[nodiscard]] std::vector<ai::ListedPerceptionActor> ListedActors()
      const override {
    return listed;
  }
};

ai::PerceivingObserver Observer() {
  ai::PerceivingObserver observer;
  observer.actor_id = kObserver;
  observer.tile = Tile(4, 4);
  observer.view_cosine = kAlwaysInView;
  observer.far_distance_squared = kFar;
  return observer;
}

ai::PerceivedRelation& RelationOf(ai::PerceivingObserver& observer) {
  return observer.relations.at(kTarget);
}

// Unwraps a fake's optional field, throwing as value() would; the explicit
// check keeps the access visibly guarded.
template <typename T>
T& Present(std::optional<T>& field) {
  if (!field) throw std::bad_optional_access();
  return *field;
}

ai::PerceivedModel& TargetModel(FakeTargetWorld& world) {
  return Present(world.models.at(kTarget));
}

bool Update(FakeTargetWorld& world, ai::PerceivingObserver& observer,
            int target = kTarget) {
  return ai::UpdateTargetPerception(world, observer, target);
}
}  // namespace

TEST_CASE("Actor perception relation words keep every bit",
          "[ai][actor-perception]") {
  for (const std::uint32_t word :
       {0U, 0xFFFFFFFFU, 0x5A5A5A5AU, 0xA5A5A5A5U, 0x12345678U}) {
    CHECK(ai::EncodePerceivedRelation(ai::DecodePerceivedRelation(word)) ==
          word);
  }
  const auto relation = ai::DecodePerceivedRelation(
      (1U << 2) | (2U << 6) | (0x55U << 8) | (0x11U << 15) | (0x41U << 20) |
      (1U << 27) | (3U << 28) | (1U << 30));
  const ai::PerceivedRelation expected{.clear_at_point_height = true,
                                       .classification = 2,
                                       .counter = 0x55,
                                       .timer = 0x11,
                                       .classification_timer = 0x41,
                                       .blocked = true,
                                       .previous_classification = 3,
                                       .clear_at_body_height = true};
  CHECK(expected == relation);
  CHECK(ai::DecodePerceivedRelation(1U << 31).marked);
  CHECK(ai::DecodePerceivedRelation(1U << 4).reserved4);
}

TEST_CASE("Actor perception tests terrain at body and point heights",
          "[ai][actor-perception]") {
  FakeTargetWorld world;
  auto observer = Observer();
  observer.position = {.x = 5, .y = -10, .z = 7};
  observer.body_height = 90;
  observer.point_height = -24;
  const Position3 target{.x = 40, .y = 3, .z = 80};

  CHECK_FALSE(
      ai::IsTargetOccludedAtBodyHeight(world, observer, kTarget, target));
  CHECK_FALSE(
      ai::IsTargetOccludedAtPointHeight(world, observer, kTarget, target));
  world.frame_shapes.at(kTarget) =
      ai::PerceptionShape{.body_height = 100, .point_height = -40, .radius = 0};
  world.terrain_blocks = true;
  CHECK(ai::IsTargetOccludedAtBodyHeight(world, observer, kTarget, target));
  CHECK(ai::IsTargetOccludedAtPointHeight(world, observer, kTarget, target));
  CHECK(world.terrain ==
        std::vector<TerrainCall>{{target, observer.position, 128, 90},
                                 {target, observer.position, 64, 24},
                                 {target, observer.position, 100, 90},
                                 {target, observer.position, 40, 24}});
}

TEST_CASE("Actor perception refuses targets and models it cannot read",
          "[ai][actor-perception]") {
  for (const int target : {-1, ai::kPerceptionSlots}) {
    FakeTargetWorld world;
    auto observer = Observer();
    CHECK_FALSE(Update(world, observer, target));
    CHECK(observer == Observer());
  }
  FakeTargetWorld world;
  auto observer = Observer();
  world.models.at(kTarget).reset();
  CHECK_FALSE(Update(world, observer));
  world.Reset();
  world.pose.reset();
  CHECK_FALSE(Update(world, observer));
  world.Reset();
  world.has_tile_flags = false;
  CHECK_FALSE(Update(world, observer));
  world.Reset();
  CHECK(Update(world, observer));
}

TEST_CASE("Actor perception measures targets at their original heights",
          "[ai][actor-perception]") {
  FakeTargetWorld world;
  auto observer = Observer();
  REQUIRE(Update(world, observer));
  // Without an AI state the target counts 64 units up.
  CHECK(observer.distances_squared.at(kTarget) == 64 * 64 + 64 * 64);

  auto& model = TargetModel(world);
  model.position.y = 10;
  world.ai_shapes.at(kTarget) =
      ai::PerceptionShape{.body_height = 50, .point_height = -5, .radius = 0};
  REQUIRE(Update(world, observer));
  CHECK(observer.distances_squared.at(kTarget) == 64 * 64 + 5 * 5);
  // A locomoting target below the observer is measured at its body height.
  model.locomoting = true;
  REQUIRE(Update(world, observer));
  CHECK(observer.distances_squared.at(kTarget) == 64 * 64 + 40 * 40);
  model.position.y = 0;
  REQUIRE(Update(world, observer));
  CHECK(observer.distances_squared.at(kTarget) == 64 * 64 + 5 * 5);
}

TEST_CASE("Actor perception skips line of sight on a shared tile",
          "[ai][actor-perception]") {
  const auto shared = [](FakeTargetWorld& world) {
    auto observer = Observer();
    REQUIRE(Update(world, observer));
    return RelationOf(observer).clear_at_body_height &&
           RelationOf(observer).clear_at_point_height && observer.phase == 0 &&
           world.terrain.empty();
  };
  FakeTargetWorld world;
  world.gimmicks.at(kTarget) = true;
  CHECK(shared(world));
  world.Reset();
  // Tiles compare x and z only.
  TargetModel(world).tile = Tile(4, 4, 7);
  CHECK(shared(world));
  world.Reset();
  TargetModel(world).last_tile = Tile(4, 4);
  CHECK_FALSE(shared(world));
  world.Reset();
  TargetModel(world).last_tile = Tile(4, 4);
  TargetModel(world).locomoting = true;
  CHECK(shared(world));
}

namespace {
// An observer whose line-of-sight tests alternate between its body and point
// heights.
struct SightSequence {
  FakeTargetWorld world;
  ai::PerceivingObserver observer = Observer();

  SightSequence() {
    observer.body_height = 70;
    observer.point_height = -20;
  }

  // Replays the sequence's first updates without checking them: one clear,
  // two blocked, then clear again.
  void Replay(int frames) {
    for (int frame = 0; frame < frames; ++frame) {
      world.terrain_blocks = frame == 1 || frame == 2;
      REQUIRE(Update(world, observer));
    }
  }
};
}  // namespace

TEST_CASE("Actor perception alternates its line-of-sight height",
          "[ai][actor-perception]") {
  SightSequence sequence;
  auto& world = sequence.world;
  auto& observer = sequence.observer;
  REQUIRE(Update(world, observer));
  CHECK(observer.phase == 1);
  CHECK(RelationOf(observer).clear_at_body_height);
  CHECK_FALSE(RelationOf(observer).clear_at_point_height);
  world.terrain_blocks = true;
  REQUIRE(Update(world, observer));
  CHECK(observer.phase == 2);
  CHECK(RelationOf(observer).clear_at_body_height);
  CHECK_FALSE(RelationOf(observer).clear_at_point_height);
}

TEST_CASE("Actor perception retests its body height after a blocked point",
          "[ai][actor-perception]") {
  SightSequence sequence;
  auto& world = sequence.world;
  auto& observer = sequence.observer;
  sequence.Replay(2);
  REQUIRE(Update(world, observer));
  CHECK(observer.phase == 3);
  CHECK_FALSE(RelationOf(observer).clear_at_body_height);
  const Position3 target = TargetModel(world).position;
  CHECK(world.terrain ==
        std::vector<TerrainCall>{{target, observer.position, 128, 70},
                                 {target, observer.position, 64, 20},
                                 {target, observer.position, 128, 70}});
}

TEST_CASE("Actor perception clears its body height with a clear point line",
          "[ai][actor-perception]") {
  SightSequence sequence;
  auto& world = sequence.world;
  auto& observer = sequence.observer;
  sequence.Replay(3);
  // A clear point-height line clears the body-height line too.
  world.terrain_blocks = false;
  REQUIRE(Update(world, observer));
  CHECK(observer.phase == 4);
  CHECK(RelationOf(observer).clear_at_point_height);
  CHECK(RelationOf(observer).clear_at_body_height);
}

TEST_CASE("Actor perception wraps its line-of-sight phase as a byte",
          "[ai][actor-perception]") {
  SightSequence sequence;
  auto& world = sequence.world;
  auto& observer = sequence.observer;
  sequence.Replay(4);
  observer.phase = 0xFF;
  REQUIRE(Update(world, observer));
  CHECK(observer.phase == 0);
  CHECK(world.terrain.back().from_height == 64);
}

TEST_CASE("Actor perception adds the look yaw to the facing",
          "[ai][actor-perception]") {
  // The target is straight down +z: facing 0x800 looks at it.
  const auto in_view = [](std::int16_t facing, std::int16_t look_yaw) {
    FakeTargetWorld world;
    world.pose = ai::ObserverPose{
        .position = Position3{}, .facing = facing, .look_yaw = look_yaw};
    auto observer = Observer();
    observer.view_cosine = 4096;
    REQUIRE(Update(world, observer));
    return RelationOf(observer).in_view;
  };
  CHECK(in_view(0x700, 0x100));
  CHECK(in_view(0x900, -0x100));
  CHECK_FALSE(in_view(0x800, 0x100));
  FakeTargetWorld world;
  auto observer = Observer();
  observer.view_cosine = kNeverInView;
  REQUIRE(Update(world, observer));
  CHECK_FALSE(RelationOf(observer).in_view);
}

namespace {
constexpr std::uint32_t kActivityDistance = 64 * 64 + 64 * 64;

// A target exactly at the observer's far range.
struct ActivitySequence {
  FakeTargetWorld world;
  ai::PerceivingObserver observer = Observer();

  ActivitySequence() { observer.far_distance_squared = kActivityDistance; }

  // Replays the sequence's first updates without checking them: in range,
  // out of range twice, then inside the near range.
  void Replay(int frames) {
    for (int frame = 0; frame < frames; ++frame) {
      if (frame == 1) observer.far_distance_squared = kActivityDistance - 1;
      if (frame == 3) observer.near_distance_squared = kActivityDistance + 1;
      REQUIRE(Update(world, observer));
    }
  }
};
}  // namespace

TEST_CASE("Actor perception tracks activity, its timer and visibility",
          "[ai][actor-perception]") {
  ActivitySequence sequence;
  auto& world = sequence.world;
  auto& observer = sequence.observer;
  REQUIRE(Update(world, observer));
  CHECK(RelationOf(observer).active);
  CHECK(RelationOf(observer).visible);
  CHECK_FALSE(RelationOf(observer).was_active);

  // Leaving range drops activity and remembers it for one frame.
  observer.far_distance_squared = kActivityDistance - 1;
  REQUIRE(Update(world, observer));
  CHECK_FALSE(RelationOf(observer).active);
  CHECK(RelationOf(observer).was_active);
  CHECK_FALSE(RelationOf(observer).visible);
}

TEST_CASE("Actor perception forgets lost activity after one frame",
          "[ai][actor-perception]") {
  ActivitySequence sequence;
  sequence.Replay(2);
  REQUIRE(Update(sequence.world, sequence.observer));
  CHECK_FALSE(RelationOf(sequence.observer).was_active);
}

TEST_CASE("Actor perception sees near targets with a clear body line",
          "[ai][actor-perception]") {
  ActivitySequence sequence;
  auto& observer = sequence.observer;
  sequence.Replay(3);
  observer.near_distance_squared = kActivityDistance + 1;
  REQUIRE(Update(sequence.world, observer));
  CHECK_FALSE(RelationOf(observer).active);
  CHECK(RelationOf(observer).visible);
}

TEST_CASE("Actor perception holds activity while its timer counts down",
          "[ai][actor-perception]") {
  ActivitySequence sequence;
  auto& observer = sequence.observer;
  sequence.Replay(4);
  RelationOf(observer).timer = 2;
  REQUIRE(Update(sequence.world, observer));
  CHECK(RelationOf(observer).active);
  CHECK(RelationOf(observer).timer == 1);
}

TEST_CASE("Actor perception deactivates a target behind a blocked body line",
          "[ai][actor-perception]") {
  // Unless the observer ignores the body line.
  FakeTargetWorld world;
  world.terrain_blocks = true;
  auto observer = Observer();
  REQUIRE(Update(world, observer));
  CHECK_FALSE(RelationOf(observer).active);
  observer = Observer();
  observer.ignores_body_line = true;
  REQUIRE(Update(world, observer));
  CHECK(RelationOf(observer).active);
}

TEST_CASE("Actor perception always sees itself", "[ai][actor-perception]") {
  FakeTargetWorld world;
  world.models.at(kObserver) = world.models.at(kTarget);
  world.frame_shapes.at(kObserver) = ai::PerceptionShape{};
  auto observer = Observer();
  observer.view_cosine = kNeverInView;
  observer.far_distance_squared = 0;
  observer.decays_hostility = true;
  auto& self = observer.relations.at(kObserver);
  self.was_active = true;
  self.classification = 3;
  self.counter = 120;
  self.classification_timer = 1;
  REQUIRE(Update(world, observer, kObserver));
  CHECK(self.active);
  CHECK(self.visible);
  CHECK_FALSE(self.was_active);
  CHECK(self.counter == 120);
  CHECK(self.classification_timer == 1);
}

TEST_CASE("Actor perception marks flagged tiles or active targets",
          "[ai][actor-perception]") {
  FakeTargetWorld world;
  auto observer = Observer();
  observer.view_cosine = kNeverInView;
  REQUIRE(Update(world, observer));
  CHECK_FALSE(RelationOf(observer).marked);
  world.marked_tiles = {{4, 5}};
  REQUIRE(Update(world, observer));
  CHECK(RelationOf(observer).marked);
  world.marked_tiles.clear();
  observer.view_cosine = kAlwaysInView;
  REQUIRE(Update(world, observer));
  CHECK(RelationOf(observer).marked);
}

TEST_CASE("Actor perception finds nearer actors across the line",
          "[ai][actor-perception]") {
  const auto run = [](Position3 blocker, std::uint32_t distance,
                      std::optional<std::uint16_t> radius) {
    FakeTargetWorld world;
    TargetModel(world).position = {.x = 0, .y = 0, .z = 1000};
    world.ai_shapes.at(kTarget) = ai::PerceptionShape{};
    if (radius) {
      world.frame_shapes.at(kTarget) = ai::PerceptionShape{
          .body_height = 0, .point_height = 0, .radius = *radius};
    }
    world.listed = {{.id = kObserver, .position = Position3{}},
                    {.id = kTarget, .position = Position3{.z = 1000}},
                    {.id = kBlocker, .position = blocker}};
    auto observer = Observer();
    observer.distances_squared.at(kBlocker) = distance;
    RelationOf(observer).blocked = true;
    REQUIRE(Update(world, observer));
    REQUIRE(observer.distances_squared.at(kTarget) == 1000 * 1000);
    return RelationOf(observer).blocked;
  };
  // About 29 units off the line.
  CHECK(run({.x = 30, .y = 0, .z = 500}, 500 * 500 + 30 * 30, std::nullopt));
  // About 100 units off the line: outside the default radius of 63.
  CHECK_FALSE(
      run({.x = 100, .y = 0, .z = 500}, 500 * 500 + 100 * 100, std::nullopt));
  CHECK(run({.x = 100, .y = 0, .z = 500}, 500 * 500 + 100 * 100, 120));
  // Behind the observer.
  CHECK_FALSE(run({.x = 0, .y = 0, .z = -500}, 500 * 500, 1000));
  // Farther than the target.
  CHECK_FALSE(run({.x = 30, .y = 0, .z = 500}, 1000 * 1000 + 1, 1000));
  CHECK(run({.x = 30, .y = 0, .z = 500}, 1000 * 1000, 1000));
}

TEST_CASE("Actor perception refuses listed actors it cannot place",
          "[ai][actor-perception]") {
  FakeTargetWorld world;
  auto observer = Observer();
  for (const int id : {-1, ai::kPerceptionSlots}) {
    world.listed = {{.id = id, .position = Position3{}}};
    CHECK_FALSE(Update(world, observer));
  }
  // The observer and the target are skipped before their slots are read.
  observer.actor_id = 20;
  world.listed = {{.id = 20, .position = std::nullopt},
                  {.id = kTarget, .position = std::nullopt}};
  CHECK(Update(world, observer));
  // Farther actors are skipped before their positions are read.
  observer = Observer();
  world.listed = {{.id = kBlocker, .position = std::nullopt}};
  observer.distances_squared.at(kBlocker) = 0xFFFFFFFFU;
  CHECK(Update(world, observer));
  observer.distances_squared.at(kBlocker) = 0;
  CHECK_FALSE(Update(world, observer));
}

namespace {
struct ClassificationOutcome {
  std::uint8_t classification = 0;
  std::uint8_t counter = 0;
  std::uint8_t classification_timer = 0;

  friend bool operator==(const ClassificationOutcome&,
                         const ClassificationOutcome&) = default;
};

// By default the observer decays hostility, the target has a frame shape and
// the target is inactive.
struct ClassificationRun {
  ai::PerceivedRelation relation;
  bool decays = true;
  bool frame_shape = true;
  bool active = false;
};

ClassificationOutcome Classify(const ClassificationRun& run) {
  FakeTargetWorld world;
  if (run.frame_shape) world.frame_shapes.at(kTarget) = ai::PerceptionShape{};
  auto observer = Observer();
  observer.decays_hostility = run.decays;
  if (!run.active) observer.view_cosine = kNeverInView;
  RelationOf(observer) = run.relation;
  REQUIRE(Update(world, observer));
  const auto& updated = RelationOf(observer);
  return {.classification = updated.classification,
          .counter = updated.counter,
          .classification_timer = updated.classification_timer};
}

constexpr ai::PerceivedRelation kHostile{.classification = 3, .counter = 120};
}  // namespace

TEST_CASE("Actor perception decays hostility", "[ai][actor-perception]") {
  CHECK(Classify({.relation = kHostile}) == ClassificationOutcome{1, 121, 0});
  CHECK(Classify({.relation = kHostile, .active = true}) ==
        ClassificationOutcome{3, 0, 0});
  CHECK(Classify({.relation = kHostile, .decays = false}) ==
        ClassificationOutcome{3, 120, 0});
  CHECK(Classify({.relation = kHostile, .frame_shape = false}) ==
        ClassificationOutcome{3, 120, 0});
  CHECK(Classify({.relation = {.classification = 3, .counter = 127}}) ==
        ClassificationOutcome{3, 0, 0});
}

TEST_CASE("Actor perception expires classifications",
          "[ai][actor-perception]") {
  CHECK(Classify({.relation = {.classification = 2,
                               .classification_timer = 1,
                               .previous_classification = 3}}) ==
        ClassificationOutcome{3, 1, 0});
  CHECK(Classify({.relation = {.classification = 2,
                               .classification_timer = 3,
                               .previous_classification = 3}}) ==
        ClassificationOutcome{2, 0, 2});
}

TEST_CASE("Actor perception begins a frame by resetting the room",
          "[ai][actor-perception]") {
  ai::PerceptionRoom room{
      .readiness_level = 5, .approach_moves = 9, .candidate_engaged = 1};
  ai::BeginActorPerception(room);
  CHECK(room == ai::PerceptionRoom{.readiness_level = 6});
  room.readiness_level = 0xFFFE;
  ai::BeginActorPerception(room);
  CHECK(room.readiness_level == 0xFFFE);
}

namespace {
constexpr std::uint8_t kNoCommand = 8;
constexpr std::uint32_t kStandingTile = Tile(4, 6);
constexpr std::int32_t kElapsed = 10;

enum class Op {
  kDisengage,
  kResetAwareness,
  kUnlink,
  kTemporaryAwareness,
  kTickTimers,
  kBuildup,
  kPerceive,
  kRefreshTile,
  kRecover,
  kForget,
  kReaction,
  kRestrictions,
  kReadiness,
  kBudgetTick
};

struct Event {
  Op op;
  int a = 0;
  int b = 0;

  friend bool operator==(const Event&, const Event&) = default;
};
using Events = std::vector<Event>;

struct ActorWorldState {
  std::optional<ai::PerceiverModel> model =
      ai::PerceiverModel{.tile = kStandingTile};
  std::optional<std::uint32_t> linked_tile = kStandingTile;
  std::optional<ai::LinkedPoint> linked_position =
      ai::LinkedPoint{.position = {.x = 0x140, .y = -8, .z = 0x2C0}, .pad = 7};
  std::optional<ai::PerceivedHp> hp =
      ai::PerceivedHp{.current = 100, .max = 100};
  std::vector<int> listed{kObserver, kTarget};
  std::array<std::optional<std::uint32_t>, ai::kPerceptionRecordSlots>
      record_tiles{};
  std::int32_t budget = 100;
  std::int32_t second = 0;
  bool perceives = true;
  bool ready = false;
  Events events;
};

struct FakeActorWorld final : ai::ActorPerceptionWorld, ActorWorldState {
  void Reset() { static_cast<ActorWorldState&>(*this) = ActorWorldState{}; }

  [[nodiscard]] std::optional<ai::PerceiverModel> Model() const override {
    return model;
  }
  [[nodiscard]] std::optional<std::uint32_t> LinkedTile() const override {
    return linked_tile;
  }
  [[nodiscard]] std::optional<ai::LinkedPoint> LinkedPosition() const override {
    return linked_position;
  }
  [[nodiscard]] std::optional<ai::PerceivedHp> Hp() const override {
    return hp;
  }
  [[nodiscard]] std::vector<int> ListedActorIds() const override {
    return listed;
  }
  [[nodiscard]] std::optional<std::uint32_t> RecordTile(int id) const override {
    return record_tiles.at(id);
  }
  [[nodiscard]] std::int32_t FrameTickBudget() const override { return budget; }
  [[nodiscard]] std::int32_t SecondPerceptionTarget() const override {
    return second;
  }
  [[nodiscard]] std::int32_t ElapsedFrameTicks() override { return kElapsed; }
  void SetLastBudgetTick(std::int32_t tick) override {
    events.push_back({Op::kBudgetTick, tick});
  }
  void Disengage(ai::PerceivingActor& /*actor*/) override {
    events.push_back({Op::kDisengage});
  }
  void ResetAwarenessIndicator(ai::PerceivingActor& /*actor*/) override {
    events.push_back({Op::kResetAwareness});
  }
  void UnlinkEngagedActor(ai::PerceivingActor& /*actor*/) override {
    events.push_back({Op::kUnlink});
  }
  void UpdateTemporaryAwareness(ai::PerceivingActor& /*actor*/) override {
    events.push_back({Op::kTemporaryAwareness});
  }
  void TickDecisionTimers(ai::PerceivingActor& /*actor*/) override {
    events.push_back({Op::kTickTimers});
  }
  void UpdateEngagementBuildup(ai::PerceivingActor& /*actor*/) override {
    events.push_back({Op::kBuildup});
  }
  [[nodiscard]] bool UpdateTargetPerception(ai::PerceivingActor& actor,
                                            int target) override {
    events.push_back({Op::kPerceive, target, actor.partial_perception});
    return perceives;
  }
  void RefreshTargetTileWhenTracking(ai::PerceivingActor& /*actor*/) override {
    events.push_back({Op::kRefreshTile});
  }
  void RecoverMovement(ai::PerceivingActor& /*actor*/) override {
    events.push_back({Op::kRecover});
  }
  void ForgetActionTarget(ai::PerceivingActor& /*actor*/) override {
    events.push_back({Op::kForget});
  }
  void SetReactionWithDelay(ai::PerceivingActor& /*actor*/, int reaction,
                            int delay) override {
    events.push_back({Op::kReaction, reaction, delay});
  }
  void RefreshResourceRestrictions(ai::PerceivingActor& /*actor*/) override {
    events.push_back({Op::kRestrictions});
  }
  [[nodiscard]] bool RefreshCandidateReadiness(
      ai::PerceivingActor& /*actor*/) override {
    events.push_back({Op::kReadiness});
    return ready;
  }
};

ai::PerceivingActor Actor() {
  ai::PerceivingActor actor;
  actor.battle =
      ai::PerceivingBattleActor{.id = kTarget, .command_opcode = kNoCommand};
  actor.tile = kStandingTile;
  actor.previous_tile = -1;
  actor.observed_command = kNoCommand;
  return actor;
}

ai::PerceiverModel& ModelOf(FakeActorWorld& world) {
  return Present(world.model);
}

ai::LinkedPoint& LinkedPointOf(FakeActorWorld& world) {
  return Present(world.linked_position);
}

Events Gates() {
  return {{Op::kTemporaryAwareness}, {Op::kTickTimers}, {Op::kBuildup}};
}

Events Then(Events events, std::initializer_list<Event> more) {
  events.insert(events.end(), more);
  return events;
}

// The gates, the budget and a clean perception of target 0.
Events Perceived() {
  return Then(Gates(), {{Op::kBudgetTick, kElapsed},
                        {Op::kPerceive, 0, 0},
                        {Op::kRestrictions},
                        {Op::kReadiness}});
}
}  // namespace

TEST_CASE("Actor perception pass clears the actor's frame flags",
          "[ai][actor-perception]") {
  FakeActorWorld world;
  auto actor = Actor();
  actor.battle.command_state = 0x20;
  actor.battle.command_flags = 3;
  actor.battle.action_recovery_timer = 77;
  actor.partial_perception = 1;
  REQUIRE(ai::UpdatePerceivingActor(world, actor));
  CHECK(actor.battle.command_flags == 0);
  CHECK(actor.partial_perception == 0);
  CHECK(actor.action_delay == 77);
  CHECK(actor.perception_skipped == 1);
  CHECK(world.events ==
        Events{{Op::kDisengage}, {Op::kResetAwareness}, {Op::kUnlink}});
}

namespace {
// The events of a pass that stops early for an actor `prepare` adjusts.
template <typename Prepare>
Events StoppedEvents(Prepare prepare) {
  FakeActorWorld world;
  auto actor = Actor();
  prepare(world, actor);
  REQUIRE(ai::UpdatePerceivingActor(world, actor));
  CHECK(actor.perception_skipped == 1);
  return world.events;
}
}  // namespace

TEST_CASE("Actor perception pass stops at held, scripted and busy actors",
          "[ai][actor-perception]") {
  CHECK(StoppedEvents([](FakeActorWorld&, ai::PerceivingActor& actor) {
          actor.battle.command_state = 0x80;
        }) == Events{{Op::kTemporaryAwareness}, {Op::kTickTimers}});
  CHECK(StoppedEvents([](FakeActorWorld&, ai::PerceivingActor& actor) {
          actor.battle.command_state = 0x40;
        }) == Gates());
  CHECK(StoppedEvents([](FakeActorWorld&, ai::PerceivingActor& actor) {
          actor.has_navigation_override = true;
        }) == Gates());
  CHECK(StoppedEvents([](FakeActorWorld& world, ai::PerceivingActor&) {
          ModelOf(world).moving = true;
        }) == Gates());
  CHECK(StoppedEvents([](FakeActorWorld& world, ai::PerceivingActor&) {
          ModelOf(world).running = true;
        }) == Gates());
}

TEST_CASE("Actor perception pass stops at an actor without an id",
          "[ai][actor-perception]") {
  CHECK(StoppedEvents([](FakeActorWorld&, ai::PerceivingActor& actor) {
          actor.battle.id = 0;
        }) == Gates());
}

TEST_CASE("Actor perception pass perceives only a drawn model's first target",
          "[ai][actor-perception]") {
  CHECK(StoppedEvents([](FakeActorWorld& world, ai::PerceivingActor&) {
          ModelOf(world).forces_perception = true;
        }) == Then(Gates(), {{Op::kPerceive, 0, 1}}));
}

TEST_CASE("Actor perception pass refuses a missing model or a refused update",
          "[ai][actor-perception]") {
  FakeActorWorld world;
  world.model.reset();
  auto actor = Actor();
  CHECK_FALSE(ai::UpdatePerceivingActor(world, actor));
  CHECK(world.events == Gates());
  world.Reset();
  ModelOf(world).forces_perception = true;
  world.perceives = false;
  actor = Actor();
  CHECK_FALSE(ai::UpdatePerceivingActor(world, actor));
  world.Reset();
  world.perceives = false;
  actor = Actor();
  CHECK_FALSE(ai::UpdatePerceivingActor(world, actor));
}

TEST_CASE("Actor perception pass idles past the frame budget",
          "[ai][actor-perception]") {
  FakeActorWorld world;
  world.budget = kElapsed - 1;
  auto actor = Actor();
  REQUIRE(ai::UpdatePerceivingActor(world, actor));
  CHECK(world.events == Then(Gates(), {{Op::kRefreshTile}}));
  CHECK(actor.perception_skipped == 1);
}

TEST_CASE("Actor perception pass idles while an alert's awareness lasts",
          "[ai][actor-perception]") {
  FakeActorWorld world;
  world.budget = kElapsed;
  auto actor = Actor();
  actor.alerted = 1;
  actor.awareness_frames_remaining = 1;
  actor.awareness_marker = 6;
  REQUIRE(ai::UpdatePerceivingActor(world, actor));
  CHECK(world.events ==
        Then(Gates(), {{Op::kBudgetTick, kElapsed}, {Op::kRefreshTile}}));
  CHECK(actor.alerted == 1);
}

TEST_CASE("Actor perception pass clears an alert once its awareness ends",
          "[ai][actor-perception]") {
  for (const auto [frames, marker] :
       {std::pair<std::uint8_t, std::uint16_t>{0, 6}, {1, 5}}) {
    FakeActorWorld world;
    auto actor = Actor();
    actor.alerted = 1;
    actor.awareness_frames_remaining = frames;
    actor.awareness_marker = marker;
    REQUIRE(ai::UpdatePerceivingActor(world, actor));
    CHECK(actor.alerted == 0);
    CHECK(world.events == Perceived());
  }
}

TEST_CASE("Actor perception pass recovers movement before perceiving",
          "[ai][actor-perception]") {
  FakeActorWorld world;
  auto actor = Actor();
  actor.recovery_frames = 2;
  actor.stall_frames = 3;
  REQUIRE(ai::UpdatePerceivingActor(world, actor));
  CHECK(world.events == Then(Gates(), {{Op::kBudgetTick, kElapsed},
                                       {Op::kRecover},
                                       {Op::kPerceive, 0, 0},
                                       {Op::kRefreshTile}}));
  CHECK(actor.recovery_frames == 1);
  CHECK(actor.partial_perception == 1);
  CHECK(actor.stall_frames == 2);
  CHECK(actor.perception_skipped == 1);
  actor.stall_frames = 1;
  REQUIRE(ai::UpdatePerceivingActor(world, actor));
  CHECK(actor.stall_frames == 1);
  CHECK(actor.recovery_frames == 0);
}

namespace {
// An actor whose model stands on layer 2 of a new tile, linked to a point
// west of it.
struct TileCopy {
  FakeActorWorld world;
  ai::PerceivingActor actor = Actor();

  TileCopy() {
    ModelOf(world).tile = Tile(4, 7, 2);
    LinkedPointOf(world).position = {.x = -64, .y = -8, .z = 0x2C0};
    actor.perception_skipped = 1;
    actor.movement_wait = 3;
    actor.history.mask = 0x0F;
  }
};
}  // namespace

TEST_CASE("Actor perception pass copies the model tile and linked position",
          "[ai][actor-perception]") {
  TileCopy copy;
  auto& actor = copy.actor;
  REQUIRE(ai::UpdatePerceivingActor(copy.world, actor));
  CHECK(copy.world.events == Perceived());
  CHECK(actor.tile == Tile(4, 7, 2));
  CHECK(actor.position == Position3{.x = -64, .y = -8, .z = 0x2C0});
  CHECK(actor.position_pad == 7);
  CHECK(actor.fine_tile == ((0x2C0U >> 6) << 16 | (0xFFC0U >> 6)));
}

TEST_CASE("Actor perception pass updates its counters with the copied tile",
          "[ai][actor-perception]") {
  TileCopy copy;
  auto& actor = copy.actor;
  REQUIRE(ai::UpdatePerceivingActor(copy.world, actor));
  CHECK(actor.perception_skipped == 0);
  CHECK(actor.stayed_on_tile == 1);
  // Command 8 keeps no history.
  CHECK(actor.history.mask == 0);
  CHECK(actor.alert == 0);
  CHECK(actor.movement_wait == 2);
}

TEST_CASE("Actor perception pass refuses a missing linked tile or position",
          "[ai][actor-perception]") {
  FakeActorWorld world;
  world.linked_tile.reset();
  auto actor = Actor();
  CHECK_FALSE(ai::UpdatePerceivingActor(world, actor));
  world.Reset();
  world.linked_position.reset();
  actor = Actor();
  CHECK_FALSE(ai::UpdatePerceivingActor(world, actor));
}

namespace {
// An actor whose stall timer runs out on layer 3 of its tile.
struct StallSequence {
  FakeActorWorld world;
  ai::PerceivingActor actor = Actor();

  StallSequence() {
    world.linked_tile = Tile(4, 6, 3);
    actor.stall_frames = 1;
    actor.detour_target = 2;
  }

  // Replays the stall and both detour steps without checking them.
  void ReplayDetour() {
    REQUIRE(ai::UpdatePerceivingActor(world, actor));
    actor.detour_step_frames = 1;
    actor.detour_step = 4;
    REQUIRE(ai::UpdatePerceivingActor(world, actor));
    actor.detour_step_frames = 1;
    REQUIRE(ai::UpdatePerceivingActor(world, actor));
  }
};
}  // namespace

TEST_CASE("Actor perception pass times stalls and detours on its tile",
          "[ai][actor-perception]") {
  StallSequence sequence;
  auto& actor = sequence.actor;
  REQUIRE(ai::UpdatePerceivingActor(sequence.world, actor));
  CHECK(actor.stall_frames == 0);
  CHECK(actor.previous_tile == static_cast<std::int32_t>(Tile(4, 6, 3)));
  CHECK(actor.detour_target == 16);
  CHECK(actor.holds_stall_tile == 1);
  // The detour timer starts and counts its first frame at once.
  CHECK(actor.detour_step_frames == 39);
  CHECK(actor.stayed_on_tile == 1);
}

TEST_CASE("Actor perception pass steps through a detour",
          "[ai][actor-perception]") {
  StallSequence sequence;
  auto& actor = sequence.actor;
  REQUIRE(ai::UpdatePerceivingActor(sequence.world, actor));
  actor.detour_step_frames = 1;
  actor.detour_step = 4;
  REQUIRE(ai::UpdatePerceivingActor(sequence.world, actor));
  CHECK(actor.detour_step == 1);
  CHECK(actor.detour_step_frames == 40);
  actor.detour_step_frames = 1;
  REQUIRE(ai::UpdatePerceivingActor(sequence.world, actor));
  CHECK(actor.detour_step == 2);
}

TEST_CASE("Actor perception pass restarts its stall timer off its tile",
          "[ai][actor-perception]") {
  // Leaving the tile also forgets a held target.
  StallSequence sequence;
  auto& world = sequence.world;
  auto& actor = sequence.actor;
  sequence.ReplayDetour();
  world.Reset();
  world.linked_tile = Tile(5, 6);
  world.events.clear();
  actor.holds_stall_tile = 1;
  actor.tile = kStandingTile;
  REQUIRE(ai::UpdatePerceivingActor(world, actor));
  CHECK(actor.stall_frames == 0xA8C);
  CHECK(actor.stayed_on_tile == 0);
  CHECK(actor.holds_stall_tile == 0);
  CHECK(world.events.at(4) == Event{Op::kForget});
}

TEST_CASE("Actor perception pass reacts to a changed command",
          "[ai][actor-perception]") {
  const auto reacted = [](std::uint8_t observed, std::uint8_t opcode) {
    FakeActorWorld world;
    auto actor = Actor();
    actor.observed_command = observed;
    actor.battle.command_opcode = opcode;
    REQUIRE(ai::UpdatePerceivingActor(world, actor));
    CHECK(actor.observed_command == opcode);
    return std::ranges::count(world.events, Event{Op::kReaction, 1, 64}) == 1;
  };
  CHECK(reacted(9, 10));
  CHECK_FALSE(reacted(9, 9));
  CHECK_FALSE(reacted(kNoCommand, 10));
  CHECK_FALSE(reacted(9, kNoCommand));
}

namespace {
// An actor moving under command 9 with two history words to fill.
struct HistorySequence {
  FakeActorWorld world;
  ai::PerceivingActor actor = Actor();

  HistorySequence() {
    LinkedPointOf(world).position = {.x = 0x1234, .y = 0, .z = -2};
    actor.battle.command_opcode = 9;
    actor.observed_command = 9;
    actor.history_index = 2;
    actor.history.x_words = {0x11111111U, 0x22222222U};
    actor.history.z_words = {0x33333333U, 0x44444444U};
    actor.history.mask = 1;
  }

  void Record() { REQUIRE(ai::UpdatePerceivingActor(world, actor)); }

  // Replays the first record and the record at the last index without
  // checking them.
  void ReplayFirstRecords() {
    Record();
    actor.history_index = 7;
    Record();
  }

  void RecordCommand(std::uint8_t command) {
    world.events.clear();
    actor.battle.command_opcode = command;
    actor.observed_command = command;
    actor.history.mask = 1;
    actor.history_index = 0;
    Record();
  }
};

// Commands 10, 13 and 16 also record; the others clear the mask.
constexpr std::array<std::uint8_t, 5> kHistoryCommands{10, 13, 16, 11, 17};
}  // namespace

TEST_CASE("Actor perception pass records movement history",
          "[ai][actor-perception]") {
  HistorySequence sequence;
  auto& actor = sequence.actor;
  sequence.Record();
  CHECK(actor.history.x_words ==
        std::array<std::uint32_t, 2>{0x11341111U, 0x22222222U});
  CHECK(actor.history.z_words ==
        std::array<std::uint32_t, 2>{0x33FE3333U, 0x44444444U});
  CHECK(actor.history.mask == 5);
  CHECK(actor.history_index == 3);
}

TEST_CASE("Actor perception pass wraps its movement history index",
          "[ai][actor-perception]") {
  HistorySequence sequence;
  auto& actor = sequence.actor;
  sequence.Record();
  actor.history_index = 7;
  sequence.Record();
  CHECK(actor.history.x_words.at(1) == 0x34222222U);
  CHECK(actor.history.mask == 0x85);
  CHECK(actor.history_index == 0);
}

TEST_CASE("Actor perception pass records history for movement commands",
          "[ai][actor-perception]") {
  HistorySequence sequence;
  sequence.ReplayFirstRecords();
  for (const std::uint8_t command : kHistoryCommands) {
    sequence.RecordCommand(command);
    const bool records = command == 10 || command == 13 || command == 16;
    CHECK(sequence.actor.history.mask == (records ? 1 : 0));
  }
}

TEST_CASE("Actor perception pass refuses a history index past its end",
          "[ai][actor-perception]") {
  HistorySequence sequence;
  auto& actor = sequence.actor;
  sequence.ReplayFirstRecords();
  for (const std::uint8_t command : kHistoryCommands) {
    sequence.RecordCommand(command);
  }
  actor.battle.command_opcode = 9;
  actor.observed_command = 9;
  actor.history_index = 8;
  CHECK_FALSE(ai::UpdatePerceivingActor(sequence.world, actor));
}

TEST_CASE("Actor perception pass recovers a stalled movement history",
          "[ai][actor-perception]") {
  FakeActorWorld world;
  auto actor = Actor();
  actor.battle.command_opcode = 13;
  actor.observed_command = 13;
  actor.history.x_words = {0x01020304U, 0x01020304U};
  actor.history.z_words = {0x05060708U, 0x05060708U};
  actor.history.mask = 0xFF;
  REQUIRE(ai::UpdatePerceivingActor(world, actor));
  CHECK(actor.history.mask == 0);
  CHECK(actor.recovery_frames == 30);
  CHECK(actor.perception_skipped == 1);
  CHECK(world.events ==
        Then(Gates(), {{Op::kBudgetTick, kElapsed}, {Op::kRefreshTile}}));
}

TEST_CASE("Actor perception pass perceives the second target and readiness",
          "[ai][actor-perception]") {
  FakeActorWorld world;
  world.second = 5;
  auto actor = Actor();
  actor.navigation_counter = 2;
  REQUIRE(ai::UpdatePerceivingActor(world, actor));
  CHECK(actor.navigation_counter == 1);
  CHECK(world.events == Then(Gates(), {{Op::kBudgetTick, kElapsed},
                                       {Op::kPerceive, 0, 0},
                                       {Op::kPerceive, 5, 0},
                                       {Op::kRestrictions},
                                       {Op::kReadiness}}));

  // A ready candidate idles before the alert.
  world.Reset();
  world.ready = true;
  actor = Actor();
  actor.alert = 1;
  actor.alert_rule = 3;
  actor.flee_timer = 0;
  REQUIRE(ai::UpdatePerceivingActor(world, actor));
  CHECK(actor.alert == 1);
  CHECK(actor.perception_skipped == 1);
  CHECK(world.events.back() == Event{Op::kRefreshTile});
}

namespace {
// The actor's alert after a pass under `rule`, or nothing if the pass fails.
template <typename Prepare>
std::optional<int> AlertUnder(std::uint8_t rule, Prepare prepare) {
  FakeActorWorld world;
  auto actor = Actor();
  actor.alert = 7;
  actor.alert_rule = rule;
  prepare(world, actor);
  if (!ai::UpdatePerceivingActor(world, actor)) return std::nullopt;
  return actor.alert;
}

auto WithHp(std::uint16_t current, std::uint16_t max) {
  return [=](FakeActorWorld& world, ai::PerceivingActor& /*actor*/) {
    world.hp = ai::PerceivedHp{.current = current, .max = max};
  };
}

void Unchanged(FakeActorWorld& /*world*/, ai::PerceivingActor& /*actor*/) {}

void WithoutHp(FakeActorWorld& world, ai::PerceivingActor& /*actor*/) {
  world.hp.reset();
}

void Fleeing(FakeActorWorld& /*world*/, ai::PerceivingActor& actor) {
  actor.action_status = 2;
}
}  // namespace

TEST_CASE("Actor perception pass applies the actor's alert rule",
          "[ai][actor-perception]") {
  CHECK(AlertUnder(0, Unchanged) == 0);
  CHECK(AlertUnder(1, WithHp(24, 100)) == 1);
  CHECK(AlertUnder(1, WithHp(25, 100)) == 0);
  CHECK(AlertUnder(2, WithHp(49, 100)) == 1);
  CHECK(AlertUnder(2, WithHp(50, 100)) == 0);
}

TEST_CASE("Actor perception pass reads HP only for the HP alert rules",
          "[ai][actor-perception]") {
  CHECK(AlertUnder(1, WithoutHp) == std::nullopt);
  CHECK(AlertUnder(0, WithoutHp) == 0);
  CHECK(AlertUnder(3, WithoutHp) == 0);
}

TEST_CASE("Actor perception pass alerts on a flee timer or a visible hostile",
          "[ai][actor-perception]") {
  CHECK(AlertUnder(3, [](FakeActorWorld&, ai::PerceivingActor& actor) {
          actor.flee_timer = 1;
        }) == 1);
  CHECK(AlertUnder(3, [](FakeActorWorld&, ai::PerceivingActor& actor) {
          actor.relations.at(kTarget) = {.visible = true, .classification = 3};
        }) == 1);
  CHECK(AlertUnder(3, [](FakeActorWorld&, ai::PerceivingActor& actor) {
          actor.relations.at(kTarget) = {.visible = true, .classification = 2};
        }) == 0);
  CHECK(AlertUnder(3, [](FakeActorWorld&, ai::PerceivingActor& actor) {
          actor.relations.at(5) = {.visible = true, .classification = 3};
        }) == 0);
  CHECK(AlertUnder(3, [](FakeActorWorld& world, ai::PerceivingActor&) {
          world.listed = {ai::kPerceptionSlots};
        }) == std::nullopt);
}

TEST_CASE("Actor perception pass alerts a fleeing actor under any rule but 0",
          "[ai][actor-perception]") {
  // A fleeing actor is alert under any rule but none.
  CHECK(AlertUnder(3, Fleeing) == 1);
  CHECK(AlertUnder(0, Fleeing) == 0);
}

namespace {
constexpr std::uint32_t kHeldTile = Tile(8, 9);

// Whether a pass forgets a target remembered on record 3's tile, or nothing
// if the pass fails.
template <typename Prepare>
std::optional<bool> ForgetsUnder(Prepare prepare) {
  FakeActorWorld world;
  world.record_tiles.at(3) = kHeldTile | (5U << 8);
  auto actor = Actor();
  actor.previous_tile = static_cast<std::int32_t>(3U << 24);
  actor.remembered_target_tile = kHeldTile;
  actor.target_memory_frames = 5;
  prepare(world, actor);
  if (!ai::UpdatePerceivingActor(world, actor)) return std::nullopt;
  return std::ranges::count(world.events, Event{Op::kForget}) == 1;
}
}  // namespace

TEST_CASE("Actor perception pass forgets a remembered target that moved",
          "[ai][actor-perception]") {
  CHECK(ForgetsUnder([](FakeActorWorld&, ai::PerceivingActor&) {}) == false);
  CHECK(ForgetsUnder([](FakeActorWorld&, ai::PerceivingActor& actor) {
          actor.target_memory_frames = 1;
        }) == true);
  CHECK(ForgetsUnder([](FakeActorWorld&, ai::PerceivingActor& actor) {
          actor.target_memory_frames = 0;
          actor.previous_tile = static_cast<std::int32_t>(40U << 24);
        }) == true);
  CHECK(ForgetsUnder([](FakeActorWorld& world, ai::PerceivingActor&) {
          world.record_tiles.at(3) = Tile(8, 10);
        }) == true);
  CHECK(ForgetsUnder([](FakeActorWorld& world, ai::PerceivingActor&) {
          world.record_tiles.at(3).reset();
        }) == true);
}

TEST_CASE("Actor perception pass keeps a remembered target it cannot recheck",
          "[ai][actor-perception]") {
  CHECK(ForgetsUnder([](FakeActorWorld&, ai::PerceivingActor& actor) {
          actor.holds_stall_tile = 1;
          actor.target_memory_frames = 0;
        }) == false);
  CHECK(ForgetsUnder([](FakeActorWorld&, ai::PerceivingActor& actor) {
          actor.previous_tile = -1;
          actor.target_memory_frames = 0;
        }) == false);
}

TEST_CASE("Actor perception pass refuses a remembered record past its end",
          "[ai][actor-perception]") {
  CHECK(ForgetsUnder([](FakeActorWorld&, ai::PerceivingActor& actor) {
          actor.previous_tile =
              static_cast<std::int32_t>(ai::kPerceptionRecordSlots << 24);
        }) == std::nullopt);
}

namespace {
struct LivePerceptionFixture {
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  Battle_Ai_FrameContext saved_frame = *Battle_Ai_FrameState();
  decltype(Battle_Ai_roomAiState) saved_room = Battle_Ai_roomAiState;
  int saved_engaged = Battle_Ai_candidateEngagedThisUpdate;
  Battle_Ai_RoomAiState room{};
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  Battle_Ai_ActorAiState state{};

  LivePerceptionFixture() {
    std::fill_n(Battle_actors, saved_actors.size(), nullptr);
    auto& frame = *Battle_Ai_FrameState();
    std::fill_n(frame.actors, kAiFrameActorSlots, nullptr);
    Battle_Ai_roomAiState = &room;
  }

  ~LivePerceptionFixture() {
    std::copy(saved_actors.begin(), saved_actors.end(), Battle_actors);
    *Battle_Ai_FrameState() = saved_frame;
    Battle_Ai_roomAiState = saved_room;
    Battle_Ai_candidateEngagedThisUpdate = saved_engaged;
  }
};
}  // namespace

TEST_CASE("Actor perception Live pass resets the room without actors",
          "[ai][actor-perception]") {
  LivePerceptionFixture fixture;
  fixture.room.readinessLevel = 5;
  fixture.room.approachMoves = 9;
  Battle_Ai_candidateEngagedThisUpdate = 1;
  ai::UpdateLiveActorPerception();
  CHECK(fixture.room.readinessLevel == 6);
  CHECK(fixture.room.approachMoves == 0);
  CHECK(Battle_Ai_candidateEngagedThisUpdate == 0);
}

TEST_CASE("Actor perception Live pass keeps the AI state's other fields",
          "[ai][actor-perception]") {
  LivePerceptionFixture fixture;
  auto& state = fixture.state;
  std::memset(&state, 0x5A, sizeof(state));
  state.actorId = kTarget;
  state.awarenessFramesRemaining = 0;
  state.decisionDelay = 0;
  state.minActionDelay = 0;
  fixture.actor.id = kTarget;
  fixture.actor.commandState = 0x80;
  fixture.actor.commandFlags = 0xFF;
  fixture.actor.stats = &fixture.stats;
  fixture.stats.actionRecoveryTimer = 77;
  Battle_actors[0] = &fixture.actor;
  Battle_Ai_FrameState()->actors[kTarget] = &state;
  const Battle_Ai_ActorAiState before = state;

  ai::UpdateLiveActorPerception();
  CHECK(fixture.actor.commandFlags == 0xFC);
  CHECK(state.actionDelay == 77);
  const auto& view =
      *reinterpret_cast<const Battle_Ai_PerceptionUpdateView*>(&state);
  CHECK(view.opaque14 == 1);
  CHECK(view.opaque1D == 0);

  // Every other byte keeps its value through the typed round trip.
  Battle_Ai_ActorAiState expected = before;
  expected.actionDelay = 77;
  auto& expected_view =
      *reinterpret_cast<Battle_Ai_PerceptionUpdateView*>(&expected);
  expected_view.opaque14 = 1;
  expected_view.opaque1D = 0;
  using StateBytes = std::array<std::byte, sizeof(Battle_Ai_ActorAiState)>;
  CHECK(std::bit_cast<StateBytes>(state) ==
        std::bit_cast<StateBytes>(expected));
}

TEST_CASE("Actor perception keeps its original module entries",
          "[ai][actor-perception][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [symbols](unsigned address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  CHECK(resolve(0x800D87E8U) ==
        reinterpret_cast<void*>(&ai::UpdateLiveTargetPerception));
  CHECK(resolve(0x800D8EF4U) ==
        reinterpret_cast<void*>(&ai::UpdateLiveActorPerception));
}
