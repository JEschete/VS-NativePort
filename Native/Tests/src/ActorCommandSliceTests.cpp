// Hand-maintained tests for the typed actor command slice (#247).
#include <array>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Ai/ActorCommands.hpp"
#include "Content/ModuleData.h"
#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/TerrainHeight.hpp"

namespace {
using VS::Battle::Ai::ActorCommandPacket;
using VS::Battle::Ai::ActorCommandState;
using VS::Battle::Ai::ActorFacing;
using VS::Battle::Ai::AttackSelection;
using VS::Battle::Ai::CommandIntent;
using VS::Battle::Ai::CommandOpcode;
using VS::Battle::Ai::CommandPosition;
using VS::Field::kTerrainGridSide;
using VS::Field::TerrainGrid;
using VS::Field::TerrainRow;

constexpr int kTileSize = 128;
constexpr int kTileCenter = 64;
constexpr int kFixedOne = 4096;

constexpr std::uint32_t Tile(int x, int z) {
  return static_cast<std::uint32_t>(x) | (static_cast<std::uint32_t>(z) << 16);
}

constexpr std::int32_t Packed(int low, int high) {
  return static_cast<std::int32_t>((static_cast<std::uint32_t>(low) & 0xFFFFu) |
                                   (static_cast<std::uint32_t>(high) << 16));
}

// A room that owns its terrain and records every query the dispatcher makes.
class FakeRoom final : public VS::Battle::Ai::ActorCommandRoom {
 public:
  [[nodiscard]] TerrainGrid Terrain() const override {
    return TerrainGrid{std::span<const TerrainRow, kTerrainGridSide>(rows)};
  }
  [[nodiscard]] bool IsCornerBlocked(std::uint32_t tile, int dx,
                                     int dz) const override {
    corners.push_back({tile, {dx, dz}});
    return blocked;
  }
  [[nodiscard]] ActorFacing Facing() const override { return facing; }
  [[nodiscard]] AttackSelection SelectedAttack() const override {
    return attack;
  }
  [[nodiscard]] CommandPosition TargetPosition(
      unsigned target_id) const override {
    targets.push_back(target_id);
    return target;
  }
  void ReserveTile(int x, int z) override { reserved.emplace_back(x, z); }

  TerrainRow rows[kTerrainGridSide]{};
  bool blocked = false;
  ActorFacing facing{};
  AttackSelection attack{};
  CommandPosition target{};
  mutable std::vector<std::pair<std::uint32_t, std::pair<int, int>>> corners;
  mutable std::vector<unsigned> targets;
  std::vector<std::pair<int, int>> reserved;
};

ActorCommandState Walker() {
  ActorCommandState state{};
  state.actor_id = 1;
  state.tile = Tile(5, 5);
  state.position_x = 5 * kTileSize + kTileCenter;
  state.position_z = 5 * kTileSize + kTileCenter;
  state.radius = 16;
  state.locomotion_flags = 0x02;
  state.walk_step = 8;
  state.run_step = 24;
  state.step_height = 64;
  return state;
}
}  // namespace

TEST_CASE("Actor command slice stores direct packets from the snapshot",
          "[ai][actor-commands][slice]") {
  FakeRoom room;
  auto state = Walker();
  state.heading = 0x1234;
  state.facing_delay = 7;
  state.turn_rate = 0x40;

  ActorCommandPacket packet{};
  VS::Battle::Ai::IssueActorCommand(room, state, packet,
                                    CommandIntent::kSetHeading);
  CHECK(packet.opcode == CommandOpcode::kTurn);
  CHECK(packet.arg0 == Packed(0x234, 7));

  VS::Battle::Ai::IssueActorCommand(room, state, packet,
                                    CommandIntent::kTimedTurn);
  CHECK(packet.opcode == CommandOpcode::kTimedTurn);
  CHECK(packet.arg0 == Packed(0x1234, 0x40));
  CHECK(room.corners.empty());
}

TEST_CASE("Actor command slice stores the move vector unpacked",
          "[ai][actor-commands][slice]") {
  FakeRoom room;
  auto state = Walker();
  state.move_x = 11;
  state.move_y = -22;
  state.move_z = 33;

  ActorCommandPacket packet{};
  VS::Battle::Ai::IssueActorCommand(room, state, packet,
                                    CommandIntent::kMoveVector);
  CHECK(packet.opcode == CommandOpcode::kMoveVector);
  CHECK(packet.arg0 == 11);
  CHECK(packet.arg1 == 33);
  CHECK(packet.arg2 == -22);
  CHECK(room.corners.empty());
  CHECK(room.reserved.empty());
}

TEST_CASE(
    "Actor command slice keeps lunge halves and stores opcode-only commands",
    "[ai][actor-commands][slice]") {
  FakeRoom room;
  auto state = Walker();
  state.move_x = 11;
  state.move_y = -22;
  state.move_z = 33;

  // Lunges replace the low half of the second word and keep its high half.
  ActorCommandPacket packet{};
  packet.arg1 = Packed(0, 0x5A);
  VS::Battle::Ai::IssueActorCommand(room, state, packet, CommandIntent::kLunge);
  CHECK(packet.opcode == CommandOpcode::kLunge);
  CHECK(packet.arg0 == Packed(11, -22));
  CHECK(packet.arg1 == Packed(33, 0x5A));

  for (const auto [intent, opcode] :
       {std::pair{CommandIntent::kStand, CommandOpcode::kStand},
        std::pair{CommandIntent::kStop, CommandOpcode::kStop},
        std::pair{CommandIntent::kHold, CommandOpcode::kHold},
        std::pair{CommandIntent::kCancel, CommandOpcode::kCancel}}) {
    VS::Battle::Ai::IssueActorCommand(room, state, packet, intent);
    CHECK(packet.opcode == opcode);
  }
  CHECK(room.corners.empty());
  CHECK(room.reserved.empty());
}

TEST_CASE("Actor command slice walks freely or stands without a gait",
          "[ai][actor-commands][slice]") {
  FakeRoom room;
  auto state = Walker();
  state.heading = 0;

  ActorCommandPacket packet{CommandOpcode::kStand, Packed(0, 0x77), 0, 0};
  VS::Battle::Ai::IssueActorCommand(room, state, packet,
                                    CommandIntent::kMoveHeading);
  CHECK(packet.opcode == CommandOpcode::kWalk);
  CHECK(packet.arg0 == Packed(0, 0x77));
  CHECK(room.corners.empty());

  // Without a gait the actor stands.
  state.locomotion_flags = 0;
  state.walk_step = 0;
  state.run_step = 0;
  VS::Battle::Ai::IssueActorCommand(room, state, packet,
                                    CommandIntent::kMoveHeading);
  CHECK(packet.opcode == CommandOpcode::kStand);
}

TEST_CASE("Actor command slice slides once per tile at a blocked corner",
          "[ai][actor-commands][slice]") {
  FakeRoom room;
  auto state = Walker();
  ActorCommandPacket packet{};

  // Heading 0 walks toward -z; 20 units from the tile edge with radius 16 the
  // next step overlaps the tile behind it.
  state.position_z = 5 * kTileSize + 20;
  room.blocked = true;
  VS::Battle::Ai::IssueActorCommand(room, state, packet,
                                    CommandIntent::kMoveHeading);
  REQUIRE_FALSE(room.corners.empty());
  CHECK(room.corners.front().first == state.tile);
  CHECK(room.corners.front().second == std::pair{0, -1});
  CHECK(packet.opcode == CommandOpcode::kSlide);
  CHECK(packet.arg0 == 47 * kFixedOne);
  CHECK(packet.arg1 == -4 * kFixedOne);
  CHECK(state.last_slide_tile == Tile(5, 5));

  // The same tile does not slide twice.
  VS::Battle::Ai::IssueActorCommand(room, state, packet,
                                    CommandIntent::kMoveHeading);
  CHECK(packet.opcode == CommandOpcode::kWalk);
}

TEST_CASE("Actor command slice turns the body or the upper body",
          "[ai][actor-commands][slice]") {
  FakeRoom room;
  auto state = Walker();
  state.turn_rate = 0x40;
  room.facing = ActorFacing{0x100, 0};

  ActorCommandPacket packet{};
  state.angle = 0x120;
  VS::Battle::Ai::IssueActorCommand(room, state, packet, CommandIntent::kTurn);
  CHECK(packet.opcode == CommandOpcode::kFace);
  CHECK(packet.arg0 == Packed(0x120, 0x40));

  state.angle = 0x500;
  VS::Battle::Ai::IssueActorCommand(room, state, packet, CommandIntent::kTurn);
  CHECK(packet.opcode == CommandOpcode::kTurn);
  CHECK(packet.arg0 == Packed(0x140, 0x40));

  state.looks_with_upper_body = true;
  VS::Battle::Ai::IssueActorCommand(room, state, packet, CommandIntent::kTurn);
  CHECK(packet.opcode == CommandOpcode::kLook);
  CHECK(packet.arg0 == Packed(0x40, 0x40));

  // Past the upper-body limit the whole body turns, the other way round.
  room.facing = ActorFacing{0x100, -0x320};
  state.angle = 0xC00;
  VS::Battle::Ai::IssueActorCommand(room, state, packet, CommandIntent::kTurn);
  CHECK(packet.opcode == CommandOpcode::kTurn);
  CHECK(packet.arg0 == Packed(0x100 - 0x40, 0x40));
}

TEST_CASE("Actor command slice steps, jumps or walks to the next tile",
          "[ai][actor-commands][slice]") {
  FakeRoom room;
  auto state = Walker();
  state.tile = Tile(3, 4);
  state.next_tile = Tile(4, 4);

  ActorCommandPacket packet{};
  VS::Battle::Ai::IssueActorCommand(room, state, packet,
                                    CommandIntent::kStepToTile);
  constexpr int kNextCentre = 4 * kTileSize + kTileCenter;
  CHECK(packet.opcode == CommandOpcode::kStep);
  CHECK(packet.arg0 == static_cast<std::int32_t>(Tile(4, 4)));
  CHECK(packet.arg1 == 8);
  CHECK(state.next_position ==
        CommandPosition{
            kNextCentre,
            static_cast<std::int16_t>(VS::Field::GroundHeightAtWorldPosition(
                room.Terrain(), kNextCentre, kNextCentre)),
            kNextCentre});
  CHECK(room.reserved == std::vector{std::pair{4, 4}});

  // The player's actor does not reserve its step.
  room.reserved.clear();
  state.actor_id = 0;
  VS::Battle::Ai::IssueActorCommand(room, state, packet,
                                    CommandIntent::kStepToTile);
  CHECK(room.reserved.empty());
}

TEST_CASE("Actor command slice jumps, drops or walks up a high step",
          "[ai][actor-commands][slice]") {
  FakeRoom room;
  auto state = Walker();
  state.tile = Tile(3, 4);
  state.next_tile = Tile(4, 4);
  constexpr int kNextCentre = 4 * kTileSize + kTileCenter;
  ActorCommandPacket packet{};

  room.rows[4][4] = 0x10;
  const int rise = VS::Field::GroundHeightAtTile(room.Terrain(), 4, 4) -
                   VS::Field::GroundHeightAtTile(room.Terrain(), 3, 4);
  REQUIRE((rise < 0 ? -rise : rise) >= state.step_height);
  state.movement_flags = 0x10;
  VS::Battle::Ai::IssueActorCommand(room, state, packet,
                                    CommandIntent::kStepToTile);
  CHECK(packet.opcode == CommandOpcode::kJump);
  state.movement_flags = 0x40;
  VS::Battle::Ai::IssueActorCommand(room, state, packet,
                                    CommandIntent::kStepToTile);
  CHECK(packet.opcode == CommandOpcode::kDrop);

  // Actors that walk instead of stepping take the walk-to-tile command.
  state.movement_flags = 0x08;
  room.reserved.clear();
  VS::Battle::Ai::IssueActorCommand(room, state, packet,
                                    CommandIntent::kStepToTile);
  CHECK(packet.opcode == CommandOpcode::kWalkToTile);
  CHECK(packet.arg0 == static_cast<std::int32_t>(Tile(4, 4)));
  CHECK(packet.arg1 ==
        VS::Core::Math::Arctangent2(state.position_x - kNextCentre,
                                    state.position_z - kNextCentre));
  CHECK(room.reserved == std::vector{std::pair{4, 4}});
}

TEST_CASE("Actor command slice reserves move tiles and hovers over terrain",
          "[ai][actor-commands][slice]") {
  FakeRoom room;
  auto state = Walker();
  state.next_tile = Tile(2, 3);
  state.flight_height = -200;
  room.rows[3][2] = 0x04;

  ActorCommandPacket packet{};
  VS::Battle::Ai::IssueActorCommand(room, state, packet,
                                    CommandIntent::kMoveTile);
  CHECK(packet.opcode == CommandOpcode::kMoveTile);
  CHECK(packet.arg0 == static_cast<std::int32_t>(Tile(2, 3)));
  CHECK(room.reserved == std::vector{std::pair{2, 3}});

  packet.arg1 = Packed(0, 0x33);
  VS::Battle::Ai::IssueActorCommand(room, state, packet, CommandIntent::kHover);
  constexpr int kX = 2 * kTileSize;
  constexpr int kZ = 3 * kTileSize + kTileCenter;
  CHECK(packet.opcode == CommandOpcode::kHover);
  CHECK(packet.arg0 ==
        Packed(kX + kTileCenter,
               VS::Field::GroundHeightAtWorldPosition(room.Terrain(), kX, kZ) -
                   200));
  CHECK(packet.arg1 == Packed(kZ, 0x33));
}

TEST_CASE("Actor command slice aims attacks at limbs or positions",
          "[ai][actor-commands][slice]") {
  FakeRoom room;
  auto state = Walker();
  room.attack = AttackSelection{2, 3, 5, 1, 40, 1};

  ActorCommandPacket packet{CommandOpcode::kStand, 0, 0, Packed(0, 0x66)};
  VS::Battle::Ai::IssueActorCommand(room, state, packet,
                                    CommandIntent::kAttack);
  CHECK(packet.opcode == CommandOpcode::kAttack);
  CHECK(packet.arg0 == Packed(2, 3));
  CHECK(packet.arg1 == Packed(5, 1));
  CHECK(room.targets.empty());

  room.attack.slot_kind = 0;
  room.target = CommandPosition{100, -300, 700};
  VS::Battle::Ai::IssueActorCommand(room, state, packet,
                                    CommandIntent::kAttack);
  CHECK(room.targets == std::vector{5u});
  CHECK(packet.arg1 == Packed(100, -340));
  CHECK(packet.arg2 == Packed(700, 0x66));
}

TEST_CASE("Actor command slice exposes no PS1 module address",
          "[ai][actor-commands][slice]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DEEFCu) == nullptr);
}
