#pragma once

#include <cstdint>

#include "Field/TerrainHeight.hpp"

// Builds the command an AI actor queues for its battle actor: standing,
// turning, walking, stepping between tiles, hovering or attacking. The AI
// state is passed as a typed snapshot and the room supplies terrain, corner
// tests, the selected attack and tile reservations, so the dispatcher reads no
// frame global, actor table or PS1 address.
namespace VS::Battle::Ai {
// The AI's movement request (func_800DEEFC's jump-table index).
enum class CommandIntent : std::uint8_t {
  kStand,
  kStop,
  kHold,
  kSetHeading,
  kTurn,
  kMoveDirection,
  kMoveDestination,
  kMoveHeading,
  kMoveVector,
  kLunge,
  kDash,
  kMoveTile,
  kHover,
  kWalkToTile,
  kStepToTile,
  kAttack,
  kTimedTurn,
  kCancel,
};
inline constexpr int kCommandIntentCount = 18;

// The battle actor's command opcode at +0x09; 0 is an actor with no command.
enum class CommandOpcode : std::uint8_t {
  kNone = 0,
  kStop = 1,
  kStand = 2,
  kTurn = 3,
  kFace = 4,
  kTimedTurn = 6,
  kLook = 7,
  kCancel = 8,
  kWalk = 9,
  kRun = 10,
  kStep = 11,
  kSlide = 13,
  kDrop = 14,
  kLunge = 16,
  kDash = 17,
  kJump = 18,
  kMoveVector = 19,
  kHold = 20,
  kMoveTile = 21,
  kHover = 22,
  kWalkToTile = 23,
  kAttack = 24,
};

// The actor's queued command words. Several commands replace one halfword and
// keep the other from the previous command, so the packet is read and written.
struct ActorCommandPacket {
  CommandOpcode opcode{};
  std::int32_t arg0{};
  std::int32_t arg1{};
  std::int32_t arg2{};
};

struct CommandPosition {
  std::int16_t x{};
  std::int16_t y{};
  std::int16_t z{};

  friend bool operator==(const CommandPosition&,
                         const CommandPosition&) = default;
};

// The AI state fields the dispatcher reads; `last_slide_tile` and
// `next_position` are written back. Tiles are packed AI tile words (x in bits
// 0-7, layer in 8-15, z in 16-23).
struct ActorCommandState {
  int actor_id{};
  std::uint32_t tile{};
  std::uint32_t next_tile{};
  std::uint32_t destination{};
  std::int16_t position_x{};
  std::int16_t position_z{};
  std::uint16_t radius{};
  std::int16_t heading{};
  std::int16_t angle{};
  std::uint16_t facing_delay{};
  std::uint16_t turn_rate{};
  bool looks_with_upper_body{};
  std::int32_t move_x{};
  std::int32_t move_y{};
  std::int32_t move_z{};
  bool may_run{};
  std::uint16_t locomotion_flags{};
  std::uint8_t walk_step{};
  std::uint8_t run_step{};
  std::uint16_t movement_flags{};
  std::uint16_t step_height{};
  std::int16_t flight_height{};
  std::uint32_t last_slide_tile{};
  CommandPosition next_position{};
};

// The battle model's body heading and upper-body yaw.
struct ActorFacing {
  std::int16_t facing{};
  std::int16_t look_yaw{};
};

// The planned action: its limb row and action column, the target and limb it
// aims at, the height subtracted from a positional target, and the slot kind
// (1 and 2 aim at the target's limb; others aim at its position).
struct AttackSelection {
  unsigned row{};
  unsigned column{};
  unsigned target_id{};
  unsigned target_limb{};
  int height_offset{};
  unsigned slot_kind{};
};

class ActorCommandRoom {
 public:
  ActorCommandRoom() = default;
  ActorCommandRoom(const ActorCommandRoom&) = delete;
  ActorCommandRoom& operator=(const ActorCommandRoom&) = delete;
  ActorCommandRoom(ActorCommandRoom&&) = delete;
  ActorCommandRoom& operator=(ActorCommandRoom&&) = delete;
  virtual ~ActorCommandRoom() = default;

  [[nodiscard]] virtual Field::TerrainGrid Terrain() const = 0;
  // Whether moving from `tile` by one tile in (dx, dz), each -1..1 and not
  // both 0, is blocked by a wall, an edge or a higher slope layer.
  [[nodiscard]] virtual bool IsCornerBlocked(std::uint32_t tile, int dx,
                                             int dz) const = 0;
  [[nodiscard]] virtual ActorFacing Facing() const = 0;
  [[nodiscard]] virtual AttackSelection SelectedAttack() const = 0;
  [[nodiscard]] virtual CommandPosition TargetPosition(
      unsigned target_id) const = 0;
  virtual void ReserveTile(int x, int z) = 0;
};

// Rewrites `packet` for `intent` (func_800DEEFC). The caller marks the
// command pending and records the movement target.
void IssueActorCommand(ActorCommandRoom& room, ActorCommandState& state,
                       ActorCommandPacket& packet, CommandIntent intent);
}  // namespace VS::Battle::Ai
