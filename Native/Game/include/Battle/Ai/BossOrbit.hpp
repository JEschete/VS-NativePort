#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Battle/Ai/ActorCommands.hpp"

// The arena boss that circles the arena centre: closing on or backing away
// from its orbit radius, steering its height, retreating along a fixed path
// and attacking the player's position when the retreat ends. The battle
// runtime owns the actor AI state, the arena orbit, the player's record and
// the movement primitives; these routines see the boss's fields and ask the
// world for the rest, so they reach no frame global or PS1 address.
namespace VS::Battle::Ai {
// The arena centre on x and z, in world units.
inline constexpr int kArenaCentre = 0x7C0;
// The radius the orbit settles on.
inline constexpr int kArenaOrbitRadius = 1024;
// The height the orbit settles on.
inline constexpr int kArenaOrbitHeight = 160;
// Frames between the boss's requested moves.
inline constexpr std::int16_t kArenaMoveCooldown = 900;

// The arena progress the event script and the boss share (script flag 0xA6).
enum class ArenaProgress : std::uint8_t {
  kOrbiting = 0,
  // Set when a requested move starts; the boss's plan is saved for it.
  kMoveRequested = 1,
  kRetreating = 4,
  // Set when a retreat ends while the boss was not orbiting.
  kRetreatEnded = 6,
};

// The boss's stats (+0x5C); only read.
struct BossStats {
  std::int16_t current_hp = 0;  // +0x18
  std::int16_t max_hp = 0;      // +0x1A
  std::int16_t core_hp = 0;     // limb 4's HP, +0x708

  friend bool operator==(const BossStats&, const BossStats&) = default;
};

// The boss's AI state fields these routines use.
struct OrbitingBoss {
  std::int16_t x = 0;  // 0x4C
  std::int16_t y = 0;  // 0x4E
  std::int16_t z = 0;  // 0x50
  // The model's flag at +0x5B0 bit 4 (an actor record's previewPose bit),
  // which the retreat sets while near its start; nothing without a model.
  std::optional<bool> model_flag;
  // Nothing without stats.
  std::optional<BossStats> stats;
  std::uint8_t movement_target = 0;  // 0x15C: an actor ID
  std::int32_t move_x = 0;           // 0x17C
  std::int32_t move_z = 0;           // 0x180
  std::int32_t move_y = 0;           // 0x184
  std::int32_t candidate_count = 0;  // 0x470

  friend bool operator==(const OrbitingBoss&, const OrbitingBoss&) = default;
};

// The arena orbit (D_800F5920) and the orbit's two globals.
struct ArenaOrbit {
  std::uint16_t heading = 0;          // 0x00
  std::uint8_t retreat_phase = 0;     // 0x02: 0 starts a retreat
  std::uint8_t distance_cached = 0;   // 0x03
  std::int32_t distance_squared = 0;  // 0x04: from the centre
  // 0x0A: set when a retreat starts; orbiting and a hit reaction clear it.
  std::uint8_t is_orbiting = 0;
  std::uint8_t path_step = 0;         // 0x0B
  std::int16_t cooldown = 0;          // 0x0C
  std::int16_t move_requested = 0;    // 0x0E
  std::int16_t retreat_angle = 0;     // 0x10
  std::int16_t saved_height = 0;      // 0x12
  std::uint8_t health_phase = 0;      // 0x14
  std::uint8_t plan_saved = 0;        // 0x16
  std::uint8_t saved_plan_count = 0;  // 0x17
  std::int16_t start_x = 0;           // 0x18
  std::int16_t start_z = 0;           // 0x1A
  std::uint8_t started = 0;           // 0x1C
  std::int32_t angular_velocity = 0;  // D_800F5918: 16.16 angle units
  std::int32_t height_offset = 0;     // D_800F591C: above the saved height

  friend bool operator==(const ArenaOrbit&, const ArenaOrbit&) = default;
};

// One retreat path entry: the step's distance along the retreat angle and its
// rise. (0, 0) ends the path.
struct RetreatStep {
  std::int16_t distance = 0;
  std::int16_t height = 0;
};

// The player's actor record.
struct BossPlayer {
  // The model's movement mode is set (byte 0x0A bits 0-2).
  bool moving = false;
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
  std::uint16_t collision_height = 0;  // +0x63E
};

// The boss's battle actor: its core limb's HP and its queued command.
struct BossBattleActor {
  std::int16_t core_hp = 0;  // its stats' limb 4 HP
  ActorCommandPacket command;
};

// The arena orbit's tables, the player, the plan and the movement primitives.
// Each call taking the boss may read and change it.
class BossOrbitWorld {
 public:
  BossOrbitWorld() = default;
  BossOrbitWorld(const BossOrbitWorld&) = delete;
  BossOrbitWorld& operator=(const BossOrbitWorld&) = delete;
  BossOrbitWorld(BossOrbitWorld&&) = delete;
  BossOrbitWorld& operator=(BossOrbitWorld&&) = delete;
  virtual ~BossOrbitWorld() = default;

  // The retreat path's entry `step`; nothing past its loaded entries.
  [[nodiscard]] virtual std::optional<RetreatStep> RetreatPathStep(
      int step) const = 0;
  // The player's record; nothing without one.
  [[nodiscard]] virtual std::optional<BossPlayer> Player() const = 0;
  // func_800DBCB4 for the candidate ranked first; nothing for an empty
  // ranking slot.
  [[nodiscard]] virtual std::optional<bool> IsFirstCandidateCooldownReady(
      OrbitingBoss& boss) = 0;
  // The first-ranked candidate's readiness threshold; nothing for an empty
  // ranking slot.
  [[nodiscard]] virtual std::optional<std::uint16_t> FirstCandidateThreshold()
      const = 0;
  // The room AI state's readiness level; nothing without the room state.
  [[nodiscard]] virtual std::optional<std::uint16_t> ReadinessLevel() const = 0;
  // func_800D826C.
  [[nodiscard]] virtual bool IsEngagementReady(OrbitingBoss& boss) = 0;
  // func_800D836C.
  virtual void TryEngage(OrbitingBoss& boss) = 0;
  // func_800D82CC.
  virtual void Disengage(OrbitingBoss& boss) = 0;
  // func_800DEEFC.
  virtual void IssueCommand(OrbitingBoss& boss, CommandIntent intent) = 0;
  // func_800E2CCC.
  virtual void RecoverMovement(OrbitingBoss& boss) = 0;
  [[nodiscard]] virtual std::uint8_t Progress() const = 0;
  virtual void SetProgress(std::uint8_t progress) = 0;
  // The BIOS rand().
  [[nodiscard]] virtual int Random() = 0;
  // Nothing without a battle actor or its stats.
  [[nodiscard]] virtual std::optional<BossBattleActor> BattleActor() const = 0;
  // Stores the battle actor's command words and opcode and marks the command
  // pending.
  virtual void QueueBattleCommand(const ActorCommandPacket& packet) = 0;
};

// decomp: func_800E7698 (src/BATTLE/BATTLE.PRG/7EE98.c:506).
// One step of the retreat: phase 0 aims away from the centre and starts the
// path; each call moves by the next path entry along that angle and the
// model flag marks the first 14 steps. At the path's end the boss recovers
// while the player moves, and otherwise turns to face the centre, clears the
// flag and returns true. Nothing for a boss without a model or a path step
// past the table; the boss and orbit may then be partly updated.
[[nodiscard]] std::optional<bool> UpdateBossRadialRetreat(BossOrbitWorld& world,
                                                          OrbitingBoss& boss,
                                                          ArenaOrbit& orbit);

// decomp: func_800E78F4 (src/BATTLE/BATTLE.PRG/7EE98.c:553).
// Whether the candidate ranked first is off cooldown and its threshold is
// below the room's readiness level. Nothing for an empty first ranking slot
// or without the room AI state.
[[nodiscard]] std::optional<bool> IsBossActionReady(BossOrbitWorld& world,
                                                    OrbitingBoss& boss);

// decomp: func_800E7960 (src/BATTLE/BATTLE.PRG/7EE98.c:599).
// Circles the centre: eases the radius to 1024 and the saved height to 160,
// accelerates toward facing the player when an action is ready and away
// otherwise, requests a move at 75% and 25% HP or at random while slow and
// low, and steers the height offset. False for a boss without a model or
// stats or a world refusal; the boss and orbit may then be partly updated.
[[nodiscard]] bool UpdateBossOrbitTracking(BossOrbitWorld& world,
                                           OrbitingBoss& boss,
                                           ArenaOrbit& orbit);

// decomp: func_800E7F8C (src/BATTLE/BATTLE.PRG/7EE98.c:753).
// The boss's decision behaviour: engages when ready, counts the move
// cooldown down and follows the arena progress. Orbiting restores a saved
// plan, circles and starts a requested move while its core limb lives;
// retreating disengages and, when the path ends, attacks the player's
// position if the retreat was not interrupted and otherwise marks the retreat
// ended. False for a world refusal; the boss and orbit may then be partly
// updated.
[[nodiscard]] bool UpdateOrbitingBossBehavior(BossOrbitWorld& world,
                                              OrbitingBoss& boss,
                                              ArenaOrbit& orbit);
}  // namespace VS::Battle::Ai
