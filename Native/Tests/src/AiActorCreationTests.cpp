#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "Battle/Ai/ActorCreation.hpp"
#include "Battle/Ai/ActorCreationLive.hpp"
#include "Battle/Ai/ActorSettings.hpp"
#include "Content/ModuleData.h"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Render/ModelGeometryLive.hpp"

extern "C" {
#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Actors/SkeletonLiveState.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Damage.h"
}

// F01818 func_800E6178 (BATTLE/BATTLE.PRG/7CD68.c:934).
namespace {
using VS::Battle::Ai::ActorCreationHost;
using VS::Battle::Ai::BattleActorHandle;
using VS::Battle::Ai::CommandIntent;
using VS::Battle::Ai::CreateActorAiState;
using VS::Battle::Ai::CreatedAiState;
using VS::Battle::Ai::CreationActor;
using VS::Battle::Ai::CreationModel;
using VS::Battle::Ai::CreationStats;
using VS::Battle::Ai::kNoBattleActor;
using VS::Battle::Ai::RoomSpecialActors;

constexpr auto kActor = BattleActorHandle{0x1234};
constexpr auto kOther = BattleActorHandle{0x5678};
constexpr std::size_t kGridSide = VS::Battle::Ai::kTileFlagGridSide;
// Tile X 7, layer 0x0A, Z 5 and status 0x3C.
constexpr std::uint32_t kTile = 0x3C050A07u;
constexpr std::size_t kTileCell = (5 * kGridSide) + 7;

template <typename T>
T& Required(std::optional<T>& value) {
  REQUIRE(value);
  if (!value) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *value;
}

// Records each step creation takes and the state each step sees.
class FakeCreationHost final : public ActorCreationHost {
 public:
  std::uint32_t attribute_mask = 0x80000001u;
  std::optional<RoomSpecialActors> special_actors = RoomSpecialActors{};
  std::vector<std::uint16_t> tile_flags =
      std::vector<std::uint16_t>(kGridSide * kGridSide);
  std::optional<std::uint32_t> object_word = 0;
  std::optional<CreationModel> model = CreationModel{
      .position_y = -100, .tile = kTile, .radius = 50, .body_height = 0x90};
  std::optional<CreationStats> stats =
      CreationStats{.walk_speed = 6, .run_speed = 9};
  bool heap_available = true;
  std::optional<std::int16_t> anchor_y = -260;
  bool auxiliary_tag_bone = true;
  // The far perception range the settings decode sets.
  std::uint32_t far_range = 0xFFFFFFFFu;
  int rebuild_passes = 0;

  std::vector<std::string> steps;
  std::optional<std::uint32_t> decoded_settings;
  int allocated_id = -1;
  CreatedAiState decoded;
  CreatedAiState commanded;
  CreatedAiState finished;

  [[nodiscard]] std::uint32_t AttributeActorMask() const override {
    return attribute_mask;
  }
  void StoreAttributeActorMask(std::uint32_t mask) override {
    steps.emplace_back("store-mask");
    attribute_mask = mask;
  }
  [[nodiscard]] std::optional<RoomSpecialActors> SpecialActors()
      const override {
    return special_actors;
  }
  void StoreSpecialActor(std::size_t slot, BattleActorHandle actor) override {
    steps.push_back("store-special " + std::to_string(slot));
    Required(special_actors).at(slot) = actor;
  }
  [[nodiscard]] std::span<const std::uint16_t> TileFlags() const override {
    return tile_flags;
  }
  [[nodiscard]] std::optional<std::uint32_t> ModelObjectWord(
      BattleActorHandle actor) const override {
    CHECK(actor == kActor);
    return object_word;
  }
  [[nodiscard]] std::optional<CreationModel> Model(
      BattleActorHandle actor) const override {
    CHECK(actor == kActor);
    return model;
  }
  [[nodiscard]] std::optional<CreationStats> Stats(
      BattleActorHandle actor) const override {
    CHECK(actor == kActor);
    return stats;
  }
  [[nodiscard]] bool AllocateAiState(int actor_id,
                                     BattleActorHandle actor) override {
    CHECK(actor == kActor);
    steps.emplace_back("allocate");
    allocated_id = actor_id;
    return heap_available;
  }
  [[nodiscard]] std::optional<std::int16_t> AnchorHeightY(
      int actor_id) const override {
    CHECK(actor_id == allocated_id);
    return anchor_y;
  }
  [[nodiscard]] bool HasAuxiliaryBoneTag(int actor_id, int tag) const override {
    CHECK(actor_id == allocated_id);
    CHECK(tag == 2);
    return auxiliary_tag_bone;
  }
  void ClearActorCommand(BattleActorHandle actor) override {
    CHECK(actor == kActor);
    steps.emplace_back("clear-command");
  }
  void ApplySettings(CreatedAiState& state, std::uint32_t settings) override {
    steps.emplace_back("settings");
    decoded_settings = settings;
    decoded = state;
    state.far_distance_squared = far_range;
  }
  void InitializeRelations(CreatedAiState& state) override {
    steps.emplace_back("relations");
    CHECK(state.far_distance_squared == far_range);
  }
  void IssueCommand(CreatedAiState& state, CommandIntent intent) override {
    steps.push_back("command " + std::to_string(static_cast<int>(intent)));
    commanded = state;
  }
  void ApplySavedRelations(CreatedAiState& state) override {
    steps.emplace_back("saved-relations");
    finished = state;
  }
  [[nodiscard]] bool RebuildActionPlans(CreatedAiState& state) override {
    steps.emplace_back("rebuild");
    CHECK(state.previous_tile == -1);
    return rebuild_passes-- > 0;
  }
};

CreationActor AiActor(int id = 3) {
  return CreationActor{.handle = kActor,
                       .id = id,
                       .kind = 0x05,
                       .settings = 0x12345678u,
                       .allocation_kind = 1};
}

bool Allocated(const FakeCreationHost& host) {
  return std::ranges::find(host.steps, "allocate") != host.steps.end();
}

std::vector<std::string> Commands(const FakeCreationHost& host) {
  std::vector<std::string> commands;
  std::ranges::copy_if(
      host.steps, std::back_inserter(commands),
      [](const std::string& step) { return step.starts_with("command"); });
  return commands;
}
}  // namespace

TEST_CASE("AI creation marks attribute kinds and creates only AI kinds",
          "[ai][actor-creation]") {
  // Kinds 0x15 mark the attribute bit; kinds 0x05 create the AI state.
  const std::uint16_t kind =
      GENERATE(0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x0A, 0xFFFF);
  FakeCreationHost host;
  CreationActor actor = AiActor(4);
  actor.kind = kind;
  const std::optional<bool> idle = CreateActorAiState(host, actor, 0x20);
  REQUIRE(idle.has_value());
  CHECK(host.attribute_mask ==
        ((kind & 0x15) != 0 ? 0x80000011u : 0x80000001u));
  CHECK(Allocated(host) == ((kind & 0x05) != 0));
  CHECK(*idle == ((kind & 0x05) != 0));
}

TEST_CASE("AI creation marks the attribute bit of the actor ID modulo 32",
          "[ai][actor-creation]") {
  FakeCreationHost host;
  host.attribute_mask = 0;
  CreationActor actor = AiActor(37);
  actor.kind = 0x10;
  CHECK(CreateActorAiState(host, actor, -1) == false);
  CHECK(host.attribute_mask == (1u << 5));
}

TEST_CASE("AI creation gives an auxiliary actor of object 2 or 4 a free slot",
          "[ai][actor-creation][special-actors]") {
  const std::uint32_t object_id = GENERATE(range(0u, 8u));
  FakeCreationHost host;
  host.object_word = 0xFFFFFFF8u | object_id;
  host.special_actors =
      RoomSpecialActors{kOther, kNoBattleActor, kOther, kNoBattleActor};
  CreationActor actor = AiActor();
  actor.allocation_kind = VS::Battle::Ai::kAuxiliaryAllocationKind;
  host.model.reset();  // an auxiliary core holds no actor record
  actor.kind = 0x10;
  CHECK(CreateActorAiState(host, actor, -1) == false);
  const bool special = object_id == 2 || object_id == 4;
  CHECK(host.special_actors ==
        RoomSpecialActors{kOther, special ? kActor : kNoBattleActor, kOther,
                          kNoBattleActor});
  CHECK(host.steps ==
        (special ? std::vector<std::string>{"store-mask", "store-special 1"}
                 : std::vector<std::string>{"store-mask"}));
}

TEST_CASE("AI creation leaves full special slots and non-auxiliary actors",
          "[ai][actor-creation][special-actors]") {
  FakeCreationHost host;
  host.object_word = 2;
  SECTION("every slot is taken") {
    Required(host.special_actors).fill(kOther);
    CreationActor actor = AiActor();
    actor.allocation_kind = VS::Battle::Ai::kAuxiliaryAllocationKind;
    CHECK(CreateActorAiState(host, actor, -1).has_value());
    CHECK(std::ranges::count(Required(host.special_actors), kOther) == 4);
  }
  SECTION("an actor with a body allocation is never special") {
    host.object_word.reset();
    host.special_actors.reset();
    CHECK(CreateActorAiState(host, AiActor(), -1).has_value());
  }
  CHECK(std::ranges::count(host.steps, std::string{"store-special 0"}) == 0);
}

TEST_CASE("AI creation refuses a special actor without its model or room",
          "[ai][actor-creation][special-actors]") {
  FakeCreationHost host;
  CreationActor actor = AiActor();
  actor.allocation_kind = VS::Battle::Ai::kAuxiliaryAllocationKind;
  SECTION("no model") { host.object_word.reset(); }
  SECTION("no room AI state for object 4") {
    host.object_word = 4;
    host.special_actors.reset();
  }
  CHECK_FALSE(CreateActorAiState(host, actor, -1).has_value());
  CHECK(host.steps == std::vector<std::string>{"store-mask"});
}

TEST_CASE("AI creation needs no room AI state for other auxiliary objects",
          "[ai][actor-creation][special-actors]") {
  FakeCreationHost host;
  host.object_word = 3;
  host.special_actors.reset();
  CreationActor actor = AiActor();
  actor.allocation_kind = VS::Battle::Ai::kAuxiliaryAllocationKind;
  CHECK(CreateActorAiState(host, actor, -1).has_value());
}

TEST_CASE("AI creation refuses an AI actor without its state inputs",
          "[ai][actor-creation][refusals]") {
  FakeCreationHost host;
  CreationActor actor = AiActor();
  bool allocates = false;
  SECTION("an ID before the AI state table") { actor.id = -1; }
  SECTION("an ID past the AI state table") { actor.id = 16; }
  SECTION("no model") { host.model.reset(); }
  SECTION("no stats") { host.stats.reset(); }
  SECTION("an exhausted heap") {
    host.heap_available = false;
    allocates = true;
  }
  SECTION("no anchor height") {
    host.anchor_y.reset();
    allocates = true;
  }
  CHECK_FALSE(CreateActorAiState(host, actor, -1).has_value());
  CHECK(Allocated(host) == allocates);
  CHECK(host.steps.front() == "store-mask");
  CHECK_FALSE(host.decoded_settings.has_value());
}

TEST_CASE("AI creation sizes the body from the model's collision radius",
          "[ai][actor-creation][body]") {
  struct Case {
    int id;
    std::uint16_t model_radius;
    std::uint16_t radius;
    std::uint16_t step_height;
    std::uint16_t collision_radius;
  };
  const Case sample =
      GENERATE(Case{0, 200, 40, 32, 200}, Case{0, 10, 40, 128, 40},
               Case{3, 0, 0, 128, 0}, Case{3, 62, 62, 128, 62},
               Case{3, 63, 62, 128, 62}, Case{3, 128, 62, 128, 62},
               Case{3, 129, 62, 32, 129}, Case{5, 0xFFFF, 62, 32, 0xFFFF});
  FakeCreationHost host;
  Required(host.model).radius = sample.model_radius;
  REQUIRE(CreateActorAiState(host, AiActor(sample.id), -1).has_value());
  CHECK(host.finished.radius == sample.radius);
  CHECK(host.finished.step_height == sample.step_height);
  CHECK(host.finished.collision_radius == sample.collision_radius);
}

TEST_CASE("AI creation starts the state from the actor, anchor and skeleton",
          "[ai][actor-creation][body]") {
  FakeCreationHost host;
  host.auxiliary_tag_bone = GENERATE(false, true);
  REQUIRE(CreateActorAiState(host, AiActor(9), -1).has_value());
  CHECK(host.allocated_id == 9);
  const CreatedAiState& state = host.decoded;
  CHECK(state.actor_id == 9);
  CHECK(state.point_height == -160);
  CHECK(state.requested_movement_mode == 0);
  CHECK(state.current_movement_mode == 0);
  CHECK(state.patrol_direction == 1);
  CHECK(state.has_auxiliary_tag_bone == host.auxiliary_tag_bone);
}

TEST_CASE("AI creation places the state only after decoding its settings",
          "[ai][actor-creation][body]") {
  FakeCreationHost host;
  REQUIRE(CreateActorAiState(host, AiActor(), -1).has_value());
  CHECK(host.decoded.tile == 0);
  CHECK(host.decoded.ground_clearance == 0);
  CHECK(host.finished.tile == kTile);
}

TEST_CASE("AI creation derives locomotion from the stats",
          "[ai][actor-creation][locomotion]") {
  struct Case {
    CreationStats stats;
    std::uint16_t locomotion_flags;
    std::uint16_t movement_flags;
    std::uint8_t mode;
    std::int32_t move_distance;
    std::uint8_t max_range;
  };
  // Movement class bit 3 fixes the distance to the walk step even when the
  // gait class also has it; otherwise gait bit 3 fixes it to the run step.
  const Case sample =
      GENERATE(Case{{0x18, 6, 0x48, 9}, 0x4818, 0x4858, 1, 6 << 7, 9},
               Case{{0x00, 200, 0x08, 7}, 0x0800, 0x0808, 2, 7 << 7, 200},
               Case{{0x20, 3, 0x00, 3}, 0x0020, 0x0020, 1, 0, 3},
               Case{{0x00, 0, 0x00, 0}, 0x0000, 0x0000, 0, 0, 0},
               Case{{0xFF, 255, 0xFF, 255}, 0xFFFF, 0xFFFF, 1, 255 << 7, 255});
  FakeCreationHost host;
  host.stats = sample.stats;
  REQUIRE(CreateActorAiState(host, AiActor(), 0x20).has_value());
  const CreatedAiState& state = host.decoded;
  CHECK(state.locomotion_flags == sample.locomotion_flags);
  CHECK(state.walk_step == sample.stats.walk_speed);
  CHECK(state.run_step == sample.stats.run_speed);
  CHECK(state.movement_flags == sample.movement_flags);
  CHECK(state.locomotion_mode == sample.mode);
  CHECK(state.move_distance == sample.move_distance);
  CHECK(state.max_range == sample.max_range);
}

TEST_CASE("AI creation decodes the saved settings flags when enabled",
          "[ai][actor-creation][settings]") {
  const std::uint16_t saved = GENERATE(0x0000, 0x0ACE, 0x0ACF, 0xFFFF);
  FakeCreationHost host;
  CreationActor actor = AiActor();
  actor.saved_settings_flags = saved;
  REQUIRE(CreateActorAiState(host, actor, -1).has_value());
  REQUIRE(host.decoded_settings.has_value());
  CHECK(*host.decoded_settings ==
        ((saved & 1u) != 0
             ? VS::Battle::Ai::ApplySavedAiSettingsFlags(0x12345678u, saved)
             : 0x12345678u));
  CHECK(host.steps[1] == "allocate");
  CHECK(host.steps[2] == "settings");
  CHECK(host.steps[3] == "relations");
}

TEST_CASE("AI creation keeps the move distance within the far range",
          "[ai][actor-creation][reach]") {
  // The walk step 6 fixes the distance at 768, so the reach is 704 and its
  // square 495616.
  const std::uint32_t far =
      GENERATE(0xFFFFFFFFu, 495617u, 495616u, 495615u, 40000u, 1u, 0u);
  FakeCreationHost host;
  host.stats = CreationStats{.movement_class = 8, .walk_speed = 6};
  host.far_range = far;
  REQUIRE(CreateActorAiState(host, AiActor(), -1).has_value());
  CHECK(host.finished.far_distance_squared == far);
  CHECK(host.finished.move_distance ==
        (far < 495616u ? VS::Core::Math::SquareRootInteger(
                             static_cast<std::int32_t>(far)) -
                             64
                       : 768));
}

TEST_CASE("AI creation clamps a move distance below the reach margin too",
          "[ai][actor-creation][reach]") {
  // Without a fixed distance the reach is -64, whose square is 4096.
  const std::uint32_t far = GENERATE(4096u, 4095u, 3600u, 0u);
  FakeCreationHost host;
  host.far_range = far;
  REQUIRE(CreateActorAiState(host, AiActor(), -1).has_value());
  CHECK(host.finished.move_distance ==
        (far < 4096u ? VS::Core::Math::SquareRootInteger(
                           static_cast<std::int32_t>(far)) -
                           64
                     : 0));
}

TEST_CASE("AI creation places the state's heights from the model",
          "[ai][actor-creation][placement]") {
  FakeCreationHost host;
  CreationModel& model = Required(host.model);
  model.body_height = 0x8001;
  model.position_y =
      GENERATE(std::int16_t{-100}, std::int16_t{-32768}, std::int16_t{32767});
  REQUIRE(CreateActorAiState(host, AiActor(), -1).has_value());
  const CreatedAiState& state = host.finished;
  CHECK(state.ground_clearance == -256);
  CHECK(state.flight_height == -128);
  CHECK(state.body_height == -32767);
  CHECK(state.move_y ==
        static_cast<std::int32_t>(static_cast<std::uint32_t>(model.position_y)
                                  << 12));
}

TEST_CASE("AI creation places the state on the model's tile",
          "[ai][actor-creation][placement]") {
  FakeCreationHost host;
  REQUIRE(CreateActorAiState(host, AiActor(), -1).has_value());
  const CreatedAiState& state = host.finished;
  CHECK(state.tile == kTile);
  CHECK(state.flee_tile == kTile);
  CHECK(state.detour_tile == 0xFFFFFFFFu);
  CHECK(state.last_slide_tile == 0xFFFFFFFFu);
  CHECK(state.next_tile == 0);
}

TEST_CASE("AI creation starts Ashley with no command and no plan rebuild",
          "[ai][actor-creation][start]") {
  FakeCreationHost host;
  host.stats = CreationStats{.movement_class = 0x10, .gait_class = 0x40};
  host.tile_flags[kTileCell] = 0x10;
  host.rebuild_passes = 3;
  CreationActor actor = AiActor(0);
  actor.combat_pose = 1;
  REQUIRE(CreateActorAiState(host, actor, -1) == false);
  CHECK(host.steps == std::vector<std::string>{
                          "store-mask", "allocate", "settings", "relations",
                          "clear-command", "saved-relations"});
  CHECK(host.finished.behavior_mode == 0);
}

TEST_CASE("AI creation hovers an actor over a flagged tile",
          "[ai][actor-creation][start]") {
  // Movement class 0x10 selects mode 1; gait class 0x40 is a hover flag.
  FakeCreationHost host;
  host.stats = CreationStats{.movement_class = 0x10, .gait_class = 0x40};
  host.tile_flags[kTileCell] = 0x10;
  CreationActor actor = AiActor();
  actor.combat_pose = 1;
  REQUIRE(CreateActorAiState(host, actor, -1) == false);
  CHECK(Commands(host) == std::vector<std::string>{"command 12"});
  CHECK(host.commanded.next_tile == kTile);
  CHECK(host.commanded.tile == kTile);
  CHECK(host.finished.behavior_mode == 0);
}

TEST_CASE("AI creation stops an actor that cannot hover",
          "[ai][actor-creation][start]") {
  FakeCreationHost host;
  host.stats = CreationStats{.movement_class = 0x10, .gait_class = 0x40};
  host.tile_flags[kTileCell] = 0x10;
  SECTION("a motion-locked model") {
    Required(host.model).motion_locked = true;
  }
  SECTION("an unflagged tile") { host.tile_flags[kTileCell] = 0xFFEF; }
  SECTION("no mode-one flag, so no grid is needed") {
    Required(host.stats).movement_class = 0;
    host.tile_flags.clear();
  }
  SECTION("no hover flag, so no grid is needed") {
    Required(host.stats).gait_class = 0;
    host.tile_flags.clear();
  }
  CreationActor actor = AiActor();
  actor.combat_pose = 1;
  REQUIRE(CreateActorAiState(host, actor, -1) == false);
  CHECK(Commands(host) == std::vector<std::string>{"command 1"});
  CHECK(host.commanded.next_tile == 0);
  CHECK(host.commanded.behavior_mode == 0);
  CHECK(host.finished.behavior_mode == 1);
}

TEST_CASE("AI creation refuses a hover start without the tile in the grid",
          "[ai][actor-creation][start][refusals]") {
  FakeCreationHost host;
  host.stats = CreationStats{.movement_class = 0x10, .gait_class = 0x40};
  SECTION("no tile flag grid") { host.tile_flags.clear(); }
  SECTION("a short tile flag grid") { host.tile_flags.resize(kTileCell); }
  SECTION("a tile X outside the grid") {
    Required(host.model).tile = 0x00050020u;
  }
  SECTION("a tile Z outside the grid") {
    Required(host.model).tile = 0x00200007u;
  }
  CHECK_FALSE(CreateActorAiState(host, AiActor(), -1).has_value());
  CHECK(Commands(host).empty());
  CHECK(std::ranges::count(host.steps, std::string{"clear-command"}) == 0);
}

TEST_CASE("AI creation stops a fresh or non-idle actor in its combat pose",
          "[ai][actor-creation][start]") {
  const std::int16_t saved =
      GENERATE(std::int16_t{-1}, std::int16_t{-32768}, std::int16_t{0},
               std::int16_t{0x1F}, std::int16_t{0x20}, std::int16_t{0x7FFF},
               std::int16_t{0x7FDF});
  const std::uint8_t pose =
      GENERATE(std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{0xFF});
  FakeCreationHost host;
  CreationActor actor = AiActor();
  actor.combat_pose = pose;
  const bool idle = saved >= 0 && (saved & 0x20) != 0;
  REQUIRE(CreateActorAiState(host, actor, saved) == idle);
  const bool stops = pose != 0 && !idle;
  CHECK(Commands(host) == (stops ? std::vector<std::string>{"command 1"}
                                 : std::vector<std::string>{}));
  CHECK(host.finished.behavior_mode == (stops ? 1 : 0));
  CHECK(host.finished.saved_relation_options == saved);
}

TEST_CASE("AI creation clears the command, then applies relations and plans",
          "[ai][actor-creation][finish]") {
  const int passes = GENERATE(0, 1, 3);
  FakeCreationHost host;
  host.rebuild_passes = passes;
  CreationActor actor = AiActor();
  actor.combat_pose = 1;
  REQUIRE(CreateActorAiState(host, actor, 0x1F) == false);
  std::vector<std::string> expected{
      "store-mask", "allocate",      "settings",       "relations",
      "command 1",  "clear-command", "saved-relations"};
  expected.insert(expected.end(), static_cast<std::size_t>(passes) + 1,
                  "rebuild");
  CHECK(host.steps == expected);
  // The saved relations see the finished state.
  CHECK(host.finished.observed_command_opcode == 0);
  CHECK(host.finished.previous_tile == -1);
  CHECK(host.finished.saved_relation_options == 0x1F);
  CHECK(host.finished.behavior_mode == 1);
  CHECK(host.finished.tile == kTile);
}

namespace {
constexpr std::int16_t kOne = 4096;

// An SHP model header and two bones; bone 1 carries auxiliary tag 2.
struct CreationTestModel {
  Actors_ShpModel header{};
  std::array<Actors_ShpBone, 2> bones{};
};
static_assert(offsetof(CreationTestModel, bones) == sizeof(Actors_ShpModel));

// The battle actor, actor record and AI tables a live creation uses, saved
// and restored around one test, with a private heap.
struct LiveCreationFixture {
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  decltype(std::to_array(Battle_Ai_actorAiStates)) saved_states =
      std::to_array(Battle_Ai_actorAiStates);
  decltype(std::to_array(Actors_actorRecords)) saved_records =
      std::to_array(Actors_actorRecords);
  Battle_Ai_RoomAiState* saved_room = Battle_Ai_roomAiState;
  decltype(Battle_Ai_tileFlagMap) saved_flags = Battle_Ai_tileFlagMap;
  void* saved_has_waypoints = Battle_Ai_roomHasWaypoints;
  Battle_Ai_FrameContext saved_frame = *Battle_Ai_FrameState();
  int saved_attribute = Battle_Ai_attributeActorMask;
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 3072> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  Battle_Ai_RoomAiState room{};
  u_short flags[32][32]{};
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  Actors_ActorRecord record{};
  CreationTestModel model{};

  LiveCreationFixture() {
    std::fill_n(Battle_actors, saved_actors.size(), nullptr);
    std::fill_n(Battle_Ai_actorAiStates, saved_states.size(), nullptr);
    std::fill(std::begin(Actors_actorRecords), std::end(Actors_actorRecords),
              nullptr);
    Battle_Ai_roomAiState = &room;
    Battle_Ai_tileFlagMap = flags;
    Battle_Ai_roomHasWaypoints = nullptr;
    Battle_Ai_attributeActorMask = 0;
    model.bones[1].auxiliaryTag = 2;
    VS::Render::RecordLiveModelExtent(&model, sizeof model);
  }
  LiveCreationFixture(const LiveCreationFixture&) = delete;
  LiveCreationFixture& operator=(const LiveCreationFixture&) = delete;
  LiveCreationFixture(LiveCreationFixture&&) = delete;
  LiveCreationFixture& operator=(LiveCreationFixture&&) = delete;
  ~LiveCreationFixture() {
    VS::Render::ForgetLiveModelExtent(&model);
    std::ranges::copy(saved_actors, std::begin(Battle_actors));
    std::ranges::copy(saved_states, std::begin(Battle_Ai_actorAiStates));
    std::ranges::copy(saved_records, std::begin(Actors_actorRecords));
    Battle_Ai_roomAiState = saved_room;
    Battle_Ai_tileFlagMap = saved_flags;
    Battle_Ai_roomHasWaypoints = saved_has_waypoints;
    *Battle_Ai_FrameState() = saved_frame;
    Battle_Ai_attributeActorMask = saved_attribute;
  }

  // Ashley: an AI kind with a body allocation, her actor record and stats.
  void PrepareAshley() {
    actor.id = 0;
    actor.kind = 0x04;
    actor.allocationKind = 1;
    actor.spawnByte26 = 1;
    actor.commandOpcode = 7;
    actor.commandFlags = 0xAB;
    actor.stats = &stats;
    actor.model = &record.instance;
    stats.movementClass = 0x18;
    stats.walkSpeed = 6;
    stats.gaitClass = 0x40;
    stats.runSpeed = 9;
    record.instance.slot = 0;
    record.instance.model = &model.header;
    record.instance.boneCount = static_cast<u_char>(model.bones.size());
    record.instance.position = {100, -50, 300, 0};
    record.instance.currentTileX = 7;
    record.instance.currentTileLayer = 0x0A;
    record.instance.currentTileZ = 5;
    record.instance.reserved5F = 0x3C;
    record.modelDataSize = static_cast<unsigned>(sizeof model);
    record.scale = 2 * kOne;
    record.anchorHeight = 0x100;
    record.collisionRadius = 20;
    record.collisionHeight = 0x90;
    Actors_actorRecords[0] = &record;
    Battle_actors[0] = &actor;
  }
};

// Ashley's state points at her actor, model, stats and position in the heap.
void CheckLiveStateLinks(const Battle_Ai_ActorAiState& state,
                         const LiveCreationFixture& fixture) {
  const auto* address = reinterpret_cast<const std::byte*>(&state);
  CHECK(address >= fixture.arena.data());
  CHECK(address < fixture.arena.data() + fixture.arena.size());
  CHECK(state.actorId == 0);
  CHECK(state.actor ==
        reinterpret_cast<const Battle_Ai_EngagementData*>(&fixture.actor));
  CHECK(state.model == reinterpret_cast<const Battle_Ai_AiModelView*>(
                           &fixture.record.instance));
  CHECK(state.stats == &fixture.stats);
  CHECK(state.nativePointer60 == &fixture.record.instance.position);
  CHECK(state.nativePointer64 == &fixture.record.instance.currentTileX);
}

void CheckLiveStateBody(const Battle_Ai_ActorAiState& state) {
  // The anchor rises 0x100 at scale 2 above the position.
  CHECK(state.pointHeight == 512);
  CHECK(state.hasTaggedBone3 == 1);
  CHECK(state.radius == 40);
  CHECK(state.collision_radius == 40);
  CHECK(state.opaque13C == 128);
  CHECK(state.bodyHeight == 0x90);
  CHECK(state.flightHeight == -128);
}

void CheckLiveStateLocomotion(const Battle_Ai_ActorAiState& state) {
  CHECK(state.locomotionFlags == 0x4018);
  CHECK(state.walkStep == 6);
  CHECK(state.runStep == 9);
  CHECK(state.movementFlags == 0x4058);
  CHECK(state.locomotionMode == 1);
  CHECK(state.maxRange == 9);
}

void CheckLiveStatePlacement(const Battle_Ai_ActorAiState& state) {
  CHECK(state.tile.word == 0x3C050A07u);
  CHECK(state.flee_tile.word == 0x3C050A07u);
  CHECK(state.moveY == -50 * kOne);
  CHECK(state.previousTile == -1);
  CHECK(state.patrolDirection == 1);
  CHECK(state.reserved124 == 0);
}
}  // namespace

TEST_CASE("Live AI creation builds Ashley's state from her actor record",
          "[ai][actor-creation][creation-live]") {
  const int options = GENERATE(-1, 0x20, 0x1F);
  LiveCreationFixture fixture;
  fixture.PrepareAshley();

  const int idle =
      VS::Battle::Ai::CreateLiveActorAiState(&fixture.actor, options);
  CHECK(idle == (options == 0x20 ? 1 : 0));
  const Battle_Ai_ActorAiState* state = Battle_Ai_actorAiStates[0];
  REQUIRE(state != nullptr);
  CHECK(Battle_Ai_attributeActorMask == 1);
  CheckLiveStateLinks(*state, fixture);
  CheckLiveStateBody(*state);
  CheckLiveStateLocomotion(*state);
  CheckLiveStatePlacement(*state);
  CHECK(state->savedRelationOptions == static_cast<std::int16_t>(options));
  // Ashley starts no command; only her command is cleared.
  CHECK(state->behaviorMode == 0);
  CHECK(fixture.actor.commandOpcode == 0);
  CHECK(fixture.actor.commandFlags == 0xA0);
}

TEST_CASE("Live AI creation registers an auxiliary special actor",
          "[ai][actor-creation][creation-live]") {
  LiveCreationFixture fixture;
  Actors_ActorCore core{};
  core.reserved16C = static_cast<int>(0xFFFFFF04u);
  Battle_Actor holder{};
  fixture.room.specialActors[0] = &holder;
  fixture.actor.id = 6;
  fixture.actor.kind = 0x10;
  fixture.actor.allocationKind = VS::Battle::Ai::kAuxiliaryAllocationKind;
  fixture.actor.model = &core.model;

  CHECK(VS::Battle::Ai::CreateLiveActorAiState(&fixture.actor, -1) == 0);
  CHECK(fixture.room.specialActors[0] == &holder);
  CHECK(fixture.room.specialActors[1] == &fixture.actor);
  CHECK(fixture.room.specialActors[2] == nullptr);
  CHECK(Battle_Ai_attributeActorMask == (1 << 6));
  CHECK(Battle_Ai_actorAiStates[6] == nullptr);
}

TEST_CASE("AI creation retains its original module address",
          "[ai][actor-creation][creation-live]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800E6178) ==
        reinterpret_cast<const void*>(&VS::Battle::Ai::CreateLiveActorAiState));
}
