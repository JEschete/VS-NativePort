#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstdint>
#include <optional>

#include "Battle/Ai/ActorCommands.hpp"

// An actor approaching its target: holding the range it was given, closing
// in, chasing to where a lost target was last seen, and turning to face a
// target that moved. The battle runtime owns the actor AI state, the actor
// models and the movement primitives; these routines see the actor's approach
// fields and ask the world for the rest, so they reach no frame global or PS1
// address.
namespace VS::Battle::Ai {
inline constexpr int kTargetApproachActorSlots = 16;

// Bits of an actor's relation word for one other actor.
inline constexpr std::uint32_t kApproachRelationActive = 1U << 0;
inline constexpr std::uint32_t kApproachRelationWasActive = 1U << 1;
// Set by perception when the other actor is within its height line.
inline constexpr std::uint32_t kApproachRelationReachable = 1U << 2;
inline constexpr std::uint32_t kApproachRelationVisible = 1U << 5;

// The approach and facing fields of an actor AI state.
struct TargetApproacher {
  std::uint8_t close_approach = 0;      // 0x00
  std::uint8_t gait = 0;                // 0x01: 1 runs
  std::uint8_t height_filter = 0;       // 0x03
  std::uint8_t near_target_detour = 0;  // 0x04
  std::uint8_t toward_point = 0;        // 0x09
  std::uint8_t facing_locked = 0;       // 0x0C
  std::uint8_t approach_directly = 0;   // 0x0D
  // 0x24: chase to where a lost target was last seen (settings bit 13).
  std::uint8_t chases_lost_target = 0;
  // 0x33: close in on the target's point rather than the target (settings
  // byte 0 bit 7).
  std::uint8_t approaches_target_point = 0;
  std::uint32_t tile = 0;                // 0x34
  std::uint32_t saved_destination = 0;   // 0x48
  std::int16_t x = 0;                    // 0x4C
  std::int16_t z = 0;                    // 0x50
  bool flies = false;                    // the model's flight flag
  std::int16_t model_look_yaw = 0;       // the model's +0x14
  std::int16_t model_facing = 0;         // the model's +0x26
  std::uint8_t actor_id = 0;             // 0x88
  std::uint16_t facing_delay = 0;        // 0xBA
  std::int32_t range = 0;                // 0xDC
  std::int32_t previous_range = 0;       // 0xE0
  std::uint8_t chasing_lost_target = 0;  // 0xE4
  std::uint8_t target_id = 0;            // 0xE5
  std::uint8_t chase_timeout = 0;        // 0xE6
  // 0xE7: recover whenever the target is not out of range. No known code
  // sets it; the approach only clears it.
  std::uint8_t recover_in_range = 0;
  // 0xE8: set when closing in issued a walk; recover until the target is
  // close again.
  std::uint8_t walked_close = 0;
  std::uint8_t range_mode = 0;             // 0xE9: 0 far, 1 in range, 2 close
  std::uint8_t previous_option = 0;        // 0xEA
  std::uint8_t previous_target_id = 0;     // 0xEB
  std::uint32_t chase_destination = 0;     // 0xEC
  std::uint8_t target_half_tile_x = 0;     // 0xF4: in 64-unit steps
  std::uint8_t target_half_tile_z = 0;     // 0xF6
  std::uint16_t retarget_timer = 0;        // 0xF8
  std::uint32_t previous_facing_tile = 0;  // 0x108
  std::uint8_t facing_pending = 0;         // 0x10C
  std::uint16_t turn_timer = 0;            // 0x13A
  std::int16_t target_x = 0;               // 0x158
  std::int16_t target_z = 0;               // 0x15A
  std::uint8_t movement_target = 0;        // 0x15C: an actor ID
  std::uint16_t movement_flags = 0;        // 0x15E
  std::uint32_t destination = 0;           // 0x168
  std::int32_t move_x = 0;                 // 0x17C
  std::int32_t move_z = 0;                 // 0x180
  std::int16_t angle = 0;                  // 0x18A
  std::array<std::uint32_t, kTargetApproachActorSlots> relations{};  // 0x1A4
  std::array<std::uint32_t, kTargetApproachActorSlots>
      distances_squared{};            // 0x1E4
  std::uint8_t reaction_pending = 0;  // 0x481

  friend bool operator==(const TargetApproacher&,
                         const TargetApproacher&) = default;
};

// The parts of another actor's model the approach reads.
struct ApproachTargetModel {
  // The model's movement mode or locomotion state is set (byte 0x0A bits
  // 0-4).
  bool moving = false;
  std::int16_t x = 0;
  std::int16_t z = 0;
};

// The actor models, the battle actor and the movement primitives the approach
// uses. Each call taking the approacher may read and change it.
class TargetApproachWorld {
 public:
  virtual ~TargetApproachWorld() = default;
  // Nothing for an ID outside the actor slots or an empty slot.
  [[nodiscard]] virtual std::optional<ApproachTargetModel> TargetModel(
      int actor_id) const = 0;
  // The actor's battle command opcode (+0x09); nothing without a battle
  // actor.
  [[nodiscard]] virtual std::optional<std::uint8_t> CommandOpcode() const = 0;
  // func_800DEEFC.
  virtual void IssueCommand(TargetApproacher& approacher,
                            CommandIntent intent) = 0;
  // func_800E1388.
  virtual void ProjectClearRadialDestination(TargetApproacher& approacher,
                                             std::uint32_t source, int angle,
                                             std::uint32_t distance) = 0;
  // func_800E50A0.
  virtual void DispatchLocomotion(TargetApproacher& approacher, int kind,
                                  int angle) = 0;
  // func_800E2CCC.
  virtual void RecoverMovement(TargetApproacher& approacher) = 0;
  // func_800D8260.
  virtual void SetTemporaryAwareness(TargetApproacher& approacher, int marker,
                                     int frames) = 0;
  // func_800E4180: nonzero while the actor keeps hesitating.
  [[nodiscard]] virtual int HesitationTurn(TargetApproacher& approacher,
                                           int chance, int kind, int slot) = 0;
  // Clears the room frame's walk-test cache (its first direction byte).
  virtual void ClearWalkTests() = 0;
  // func_800E4904.
  virtual void RefreshTargetTile(TargetApproacher& approacher) = 0;
};

// decomp: func_800E24EC (src/BATTLE/BATTLE.PRG/771A8.c:1973).
// Moves toward the target (target_id) and keeps it at `range`. A moved,
// changed or 181-call-old target restarts the approach. An active, reachable
// target sets the range mode by distance with hysteresis: far walks or runs
// in (projecting the destination back to the range when the actor holds its
// distance), in range recovers and close steps onto the target's position.
// A lost target is chased to its last tile for up to 255 calls when the
// actor's settings ask for it, and otherwise is looked for. `option` 1 asks
// for a close approach; a flying actor always does. Returns whether
// the actor is still approaching; nothing for a target outside the actor
// slots or without a model, or a closing in without a battle actor, and the
// approacher may then be partly updated.
[[nodiscard]] std::optional<bool> UpdateTargetApproachMovement(
    TargetApproachWorld& world, TargetApproacher& approacher, int option);

// decomp: func_800E2B2C (src/BATTLE/BATTLE.PRG/771A8.c:2185).
// Turns toward a visible target whose tile changed, after `facing_delay`
// frames; a pending reaction or a lost target hesitates instead. Returns
// whether the actor turned or is still hesitating; targeting itself stands.
// Nothing for a target outside the actor slots once the relation is read.
[[nodiscard]] std::optional<bool> UpdateTargetFacing(
    TargetApproachWorld& world, TargetApproacher& approacher, int target);
}  // namespace VS::Battle::Ai
