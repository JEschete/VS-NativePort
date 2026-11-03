#pragma once
// Hand-maintained (tools/port/owned.csv).
// C counterparts: rood-boi pc/src/battle_motion.c:64-85,112-123,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

// The BATTLE.PRG motion requests a movement script issues for one actor:
// tile jumps, teleports and travel, queued movement vectors, turns and the
// grounded-history snapshot. The actor table owns the actor records; these
// requests see an actor only through the host below, so they reach no actor
// record, PS1 address or bitfield layout.
namespace VS::Actors {
inline constexpr int kPlainJumpProfile = 1;
inline constexpr int kNavigationJumpAnimation = 31;
inline constexpr int kNavigationJumpBlendFrames = 8;
// Actors at least this wide turn at the large rate.
inline constexpr unsigned kLargeActorRadius = 128;
inline constexpr int kSmallTurnRate = 32;
inline constexpr int kLargeTurnRate = 128;
// x, y and z of an actor position.
inline constexpr std::size_t kPositionAxes = 3;

// The four bytes of a packed tile: X, layer, Z and a spare byte the original
// copies verbatim.
struct PackedTile {
  std::uint8_t x = 0;
  std::uint8_t layer = 0;
  std::uint8_t z = 0;
  std::uint8_t spare = 0;
  bool operator==(const PackedTile&) const = default;
};

// Three halfword axes and a fourth halfword (a route's facing, a step's pad).
struct MotionVector {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
  std::int16_t w = 0;
  bool operator==(const MotionVector&) const = default;
};

// Which entry started a tile jump; the arc consumer is shared.
enum class JumpEntry : std::uint8_t { kPlain, kAirborne, kNavigation };

// Queued movement request modes (actor request byte 0x1848).
enum class MotionMode : std::uint8_t { kGround = 2, kFlight = 3, kVector = 4 };

// Travel pose states (actor byte 0x0B bits 4-6).
enum class TravelPose : std::uint8_t { kTile = 1, kTimed = 4 };

// A queued flight or vector movement.
struct QueuedMovement {
  MotionMode mode = MotionMode::kFlight;
  MotionVector step{};
  std::uint32_t distance = 0;
  std::int16_t facing = 0;
  bool operator==(const QueuedMovement&) const = default;
};

// The actor's current placement a grounded snapshot records.
struct GroundedPose {
  PackedTile tile{};
  std::array<std::int16_t, kPositionAxes> position{};
  std::int16_t facing = 0;
  // Nonzero while the actor stands on a moving support.
  std::uint8_t support_layer = 0;
};

// The actor's grounded history after a snapshot. The saved placement is only
// refreshed on static ground; nothing means it keeps its previous anchor.
struct GroundedHistory {
  PackedTile previous_tile{};
  std::array<std::int16_t, kPositionAxes> position{};
  std::int16_t previous_facing = 0;
  std::optional<PackedTile> saved_placement;
  bool operator==(const GroundedHistory&) const = default;
};

// One actor's motion state and the consumers a request hands it to.
class MotionActor {
 public:
  virtual ~MotionActor() = default;
  // Jump flags (actor byte 0x0A bits 6 and 5).
  [[nodiscard]] virtual bool Jumping() const = 0;
  [[nodiscard]] virtual bool MotionLocked() const = 0;
  virtual void SetJumpFlags(bool motion_locked, bool jumping) = 0;
  virtual void SetJumpProfile(int profile) = 0;
  virtual void PlayAnimation(int animation, int blend_frames) = 0;
  // The shared jump arc consumer (func_800A208C).
  virtual void JumpToTile(JumpEntry entry, PackedTile target, int speed,
                          std::int16_t facing) = 0;

  // A nonzero reaction state (actor byte 0x0B bits 0-3) refuses teleports.
  [[nodiscard]] virtual bool Reacting() const = 0;
  // Clears the turn frames and pose/drawn bits, stores the route and marks
  // the actor reacting.
  virtual void StartTeleport(const MotionVector& route) = 0;
  // The companion a teleport propagates to, or null below slot 2.
  [[nodiscard]] virtual MotionActor* TeleportCompanion() = 0;
  // Stores the route and the travel pose; the reaction and drawn bits stay.
  virtual void StartTravel(const MotionVector& route, TravelPose pose) = 0;

  virtual void QueueGroundOffset(const MotionVector& step) = 0;
  // Also locks the actor's motion.
  virtual void QueueMovement(const QueuedMovement& movement) = 0;

  [[nodiscard]] virtual unsigned CollisionRadius() const = 0;
  virtual void TurnAtRate(std::int16_t facing, int rate) = 0;
  virtual void TurnAtAngularSpeed(std::int16_t facing, int speed) = 0;
  virtual void PlayTurnAnimation(int facing) = 0;
};

// The low halfword of a facing, as the original stores it.
[[nodiscard]] std::int16_t NarrowFacing(int facing);

// decomp: BATTLE.PRG func_800ACF54 (src/BATTLE/BATTLE.PRG/40564.c:2548)
[[nodiscard]] GroundedHistory SaveActorGroundedTransform(
    const GroundedPose& pose);

// decomp: BATTLE.PRG func_800B63AC (src/BATTLE/BATTLE.PRG/4A0A8.c:340)
// A jumping actor takes the airborne entry and a locked one the navigation
// entry; otherwise the jump uses the plain profile.
void BeginTileJump(MotionActor& actor, PackedTile target, int speed,
                   int facing);
// decomp: BATTLE.PRG func_800B6404 (src/BATTLE/BATTLE.PRG/4A0A8.c:368)
void BeginAirborneTileJump(MotionActor& actor, PackedTile target, int speed,
                           int facing);
// decomp: BATTLE.PRG func_800B6438 (src/BATTLE/BATTLE.PRG/4A0A8.c:383)
void BeginNavigationJump(MotionActor& actor, PackedTile target, int speed,
                         int facing);

// decomp: BATTLE.PRG func_800B6580 (src/BATTLE/BATTLE.PRG/4A0A8.c:490)
// Starts every idle actor along the companion chain; the chain ends at the
// first busy actor, so cyclic links end once each actor is reacting.
void BeginTeleport(MotionActor& actor, PackedTile target, int facing);
// decomp: BATTLE.PRG func_800B65E8 (src/BATTLE/BATTLE.PRG/4A0A8.c:522)
void BeginTileTravel(MotionActor& actor, const MotionVector& target,
                     int facing);
// decomp: BATTLE.PRG func_800B65F4 (src/BATTLE/BATTLE.PRG/4A0A8.c:531)
void BeginTimedTravel(MotionActor& actor, const MotionVector& target,
                      int facing);

// decomp: BATTLE.PRG func_800B6500 (src/BATTLE/BATTLE.PRG/4A0A8.c:445)
void QueueGroundOffset(MotionActor& actor, const MotionVector& step);
// decomp: BATTLE.PRG func_800B6530 (src/BATTLE/BATTLE.PRG/4A0A8.c:459)
// The facing doubles as the request distance.
void QueueMovementVector(MotionActor& actor, const MotionVector& step,
                         int facing);
// decomp: BATTLE.PRG func_800B653C (src/BATTLE/BATTLE.PRG/4A0A8.c:467)
void QueueFlightMovement(MotionActor& actor, const MotionVector& step,
                         int speed, int facing);

// decomp: BATTLE.PRG func_800B6634 (src/BATTLE/BATTLE.PRG/4A0A8.c:551)
void TurnNavigationActor(MotionActor& actor, int facing);
// decomp: BATTLE.PRG func_800B668C (src/BATTLE/BATTLE.PRG/4A0A8.c:578)
// Refuses (returns false, changing nothing) a zero angular speed, which the
// turn consumer divides by.
[[nodiscard]] bool TurnActorAtSpeed(MotionActor& actor, std::int16_t facing,
                                    int speed);
}  // namespace VS::Actors
