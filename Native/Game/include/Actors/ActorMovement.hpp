// Hand-maintained (tools/port/owned.csv).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Actors/ActorMotion.hpp"
#include "Actors/MotionRequests.hpp"

// The per-frame actor movement (BATTLE.PRG 38C1C.c, 3A1A0.c and 40564.c):
// tile jumps, ground offsets, flight and vector moves, teleport and travel
// transitions, the moving-platform contact, the collision-resolved walk and
// run, steering, and the jump and fall arc with its landing. The rules act on
// typed actors; the host supplies the terrain, platforms, gimmicks,
// animation, audio and AI services, so nothing here addresses PS1 memory.
namespace VS::Actors {
// The model's attachment bone while an actor rides its parent.
inline constexpr std::uint8_t kParentAttachBone = 0xFC;
// Moving platforms occupy collision layers from 2 up; platform = layer - 2.
inline constexpr int kFirstPlatformLayer = 2;
// UpdateActorPlatformContact phases.
enum class PlatformContactPhase : std::uint8_t {
  kFollow = 0,  // carry the actor with its platform
  kAnchor = 1,  // record the platform anchor
  kCommit = 2,  // commit the layer and the platform rider
};

// The actor-record fields the movement rules read beyond ActorMotion. Fields
// marked "read" are never written by the rules.
struct MovementActor {
  ActorMotion motion;

  // Model instance.
  bool skipped = false;              // read
  std::uint8_t reaction_state = 0;   // 4 bits: the teleport transition
  bool was_drawn = false;            // read
  bool weapon_drawn = false;         // read
  std::uint8_t support_layer = 0;    // 4 bits
  std::uint8_t placement_layer = 0;  // 4 bits
  std::uint8_t attach_parent = 0;    // read
  std::uint8_t attach_bone = 0;      // read
  std::uint8_t base_color_code = 0;
  std::uint8_t tile_spare = 0;  // the current tile's spare byte; read
  // The grounded history (SaveActorGroundedTransform).
  PackedTile previous_tile{};
  std::array<std::int16_t, kPositionAxes> grounded_position{};
  std::int16_t previous_facing = 0;
  PackedTile saved_placement{};

  // Actor record.
  bool wall_contact = false;  // read
  bool landing_pending = false;
  bool jump_started = false;
  std::uint8_t support_gimmick = 0;  // 4 bits
  std::uint8_t tick_remainder = 0;   // the airborne half-tick carry
  std::uint8_t animation_id = 0;     // read
  std::int16_t collision_state = 0;
  std::int8_t blend_frame = 0;         // read
  std::uint16_t collision_radius = 0;  // read
  std::uint16_t collision_height = 0;  // read
  std::uint8_t queued_animation = 0;
  std::uint8_t animation_set = 0;
  std::uint8_t contact_kind = 0;
  std::int16_t ledge_height = 0;
  std::int8_t platform_offset_x = 0;
  std::int8_t platform_offset_z = 0;
  bool pose_reached_end = false;  // read
  // The platform position last seen; w holds the anchor's platform layer.
  MotionVector platform_anchor{};
  std::uint8_t companion_slot = 0;  // read
  // Refused ledge and step attempts, up to 8.
  std::uint8_t ledge_attempts = 0;
  std::uint16_t move_facing = 0;
  std::int16_t jump_frames = 0;
  std::int16_t momentum_direction = 0;
  std::uint8_t run_turns = 0;
  MotionVector request_step{};  // read
  std::uint16_t pending_target_x = 0;
  std::uint16_t pending_target_z = 0;
  std::uint16_t previous_target_x = 0;
  std::uint16_t previous_target_z = 0;

  friend bool operator==(const MovementActor&, const MovementActor&) = default;
};

// The input of one collision-resolved move: its pad selects the caller (0 the
// player's stick, 1 a navigation step whose x is the facing to steer to).
struct MoveInput {
  std::int16_t x = 0;
  std::int16_t z = 0;
  std::int16_t pad = 0;
};

struct PlatformPoint {
  std::int16_t x = 0;
  std::int16_t height = 0;
  std::int16_t z = 0;
};

// The support the last terrain sample found: a moving-platform layer and a
// gimmick slot (0 for none).
struct SupportContact {
  std::uint16_t layer = 0;
  std::uint8_t gimmick = 0;
};

// The collision probe the last terrain resolve left: the climb or ledge
// point, whose w packs the layer (bits 0-3) and gimmick slot (bits 8-11), its
// facing in eighths of a turn and the probed cell's flag word.
struct MovementProbe {
  MotionVector position{};
  std::uint8_t direction = 0;
  std::uint32_t cell_word = 0;
};

struct MovementGimmick {
  std::int16_t mode = 0;
  std::int16_t position_y = 0;
  std::uint8_t support_index = 0;
  std::uint8_t bone_count = 0;
};

// The module tables the rules index (BATTLE.PRG D_800E90AC-D_800E9300 and
// Field D_800E86C0); jump profiles run 0-6.
inline constexpr std::size_t kJumpArcKinds = 6;
inline constexpr std::size_t kMotionResults = 8;
inline constexpr std::size_t kJumpProfiles = 7;
inline constexpr std::size_t kGimmickSpinSteps = 15;
struct MovementTables {
  std::array<std::uint8_t, kJumpArcKinds> hang_frames{};
  std::array<std::uint8_t, kJumpArcKinds> height_adjustments{};
  std::array<std::uint8_t, kMotionResults> result_animations{};
  std::uint8_t result_one_animation = 0;
  std::uint8_t hang_animation = 0;
  std::array<std::uint8_t, kJumpProfiles> air_timer_end{};
  std::array<std::uint8_t, kJumpProfiles> soft_landing{};
  std::array<std::uint8_t, kJumpProfiles> hard_landing{};
  std::array<std::uint8_t, kJumpProfiles> landing_recovery{};
  std::array<std::int8_t, kGimmickSpinSteps> gimmick_spin_steps{};
};

// The terrain-resolve outcome: how the step was limited (0 free, 1 along x,
// 2 along z, 3 blocked) and the climb class (0 none, 1-3 a step or climb, 4 a
// ledge, 5 or more refused).
struct TerrainResolve {
  int status = 0;
  int climb = 0;
};

class MovementHost {
 public:
  MovementHost() = default;
  MovementHost(const MovementHost&) = delete;
  MovementHost& operator=(const MovementHost&) = delete;
  MovementHost(MovementHost&&) = delete;
  MovementHost& operator=(MovementHost&&) = delete;
  virtual ~MovementHost() = default;

  // The actor in table slot `id`, or null for an empty or invalid slot.
  [[nodiscard]] virtual MovementActor* Actor(int id) = 0;
  [[nodiscard]] virtual const MovementTables& Tables() = 0;
  [[nodiscard]] virtual int TickSpeed() = 0;
  [[nodiscard]] virtual MotionTerrainHost& Terrain() = 0;

  // Animation.
  virtual void PlayAnimation(int slot, int animation, int blend_frames) = 0;
  virtual void QueueAnimation(int slot, int animation, int blend_frames,
                              int start_frame) = 0;
  virtual void UpdateLocomotionAnimation(MovementActor& actor, int distance,
                                         int direction) = 0;

  // Audio and effects.
  virtual void PlayModelSound(const MovementActor& actor, int sound) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
  virtual void PlaySoundPair(const MovementActor& actor, int first,
                             int second) = 0;
  virtual void PlaySurfaceContactSound(int id, const MovementActor& actor,
                                       int sound) = 0;
  virtual void SpawnGroundImpact(int x, int z) = 0;

  // Terrain and platforms.
  // The support height `offset_x`/`offset_z` from `origin`; also records the
  // support contact.
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
  [[nodiscard]] virtual int SampleSupportHeight(const MotionVector& origin,
                                                int offset_x, int offset_z,
                                                int mode) = 0;
  // The floor and gimmick height under a tile centre; records the contact.
  [[nodiscard]] virtual int TileSupportHeight(int x, int z) = 0;
  [[nodiscard]] virtual bool HasCollisionCell(const MotionVector& position,
                                              int layer) = 0;
  // The moving-platform height at a point (0 for none); records the layer.
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
  [[nodiscard]] virtual int PlatformContactHeight(int x, int y, int z,
                                                  int mode) = 0;
  [[nodiscard]] virtual SupportContact CurrentSupport() = 0;
  virtual void ClearSupportLayer() = 0;
  virtual void SetContactSearchDepth(int depth) = 0;
  [[nodiscard]] virtual int CeilingHeight(int x, int z) = 0;
  [[nodiscard]] virtual std::optional<PlatformPoint> MovingPlatform(
      int layer) = 0;
  [[nodiscard]] virtual bool IsPointOnMovingPlatform(
      int layer, const MotionVector& position) = 0;
  [[nodiscard]] virtual int MovingPlatformCount() = 0;
  [[nodiscard]] virtual int PlatformBounceFrame(int platform) = 0;
  // Marks `rider` (-1 for none) on platform `platform`, within the count.
  virtual void SetPlatformRider(int platform, int rider) = 0;
  [[nodiscard]] virtual int TileSurfaceId(int index) = 0;

  // Collision.
  [[nodiscard]] virtual TerrainResolve ResolveTerrainMotion(
      MovementActor& actor, MotionVector& motion, bool probe_only) = 0;
  [[nodiscard]] virtual MovementProbe Probe() = 0;
  [[nodiscard]] virtual bool CanHangFromLedge(MovementActor& actor,
                                              MotionVector& offset,
                                              MotionVector& motion) = 0;
  [[nodiscard]] virtual bool CalcLedgeOffset(MovementActor& actor,
                                             MotionVector& offset) = 0;
  [[nodiscard]] virtual bool HasLedgeSideSupport(MovementActor& actor) = 0;

  // Gimmicks.
  [[nodiscard]] virtual std::optional<MovementGimmick> Gimmick(int index) = 0;
  [[nodiscard]] virtual std::optional<MovementGimmick> GimmickForActorSlot(
      int slot) = 0;
  virtual void UpdateGimmickMotion(int slot, int ticks) = 0;

  // AI reservations and requests.
  virtual void SetAiContactOccupancy(int layer) = 0;
  virtual void ClearAiContactOccupancy(int layer) = 0;
  virtual void ReleaseAiDestinationTile(int x, int z) = 0;
  virtual void ForceReleaseAiReservations(int id) = 0;
  virtual void ClearAiNavigationReset(int slot) = 0;
  virtual void ReleaseAiContactOccupancy(int slot) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
  virtual void SetDirectMovement(int id, int direction, int distance) = 0;
  virtual void SetActorUpdateComplete() = 0;
};

// decomp: BATTLE.PRG func_800A208C (src/BATTLE/BATTLE.PRG/38C1C.c:450)
// The jump arc to `target`'s tile centre or visible platform; refused while
// the actor moves.
void BeginActorJumpToTile(MovementHost& host, MovementActor& actor,
                          PackedTile target, int speed, std::int16_t facing);
// decomp: BATTLE.PRG func_800A25EC (src/BATTLE/BATTLE.PRG/38C1C.c:633)
void ApplyActorGroundOffset(MovementHost& host, MovementActor& actor);
// decomp: BATTLE.PRG func_800A2790 (src/BATTLE/BATTLE.PRG/38C1C.c:677)
void ApplyActorFlightMovement(MovementHost& host, MovementActor& actor);
// decomp: BATTLE.PRG func_800A291C (src/BATTLE/BATTLE.PRG/38C1C.c:712)
void ApplyActorMovementVector(MovementActor& actor);
// decomp: BATTLE.PRG func_800A29A0 (src/BATTLE/BATTLE.PRG/3A1A0.c:86)
// An absent parent or companion counts as settled.
void UpdateActorTeleportTransition(MovementHost& host, MovementActor& actor);
// decomp: BATTLE.PRG func_800A2C48 (src/BATTLE/BATTLE.PRG/3A1A0.c:153)
// Cancels along the companion chain; each actor is cancelled once, so a
// cyclic chain ends.
void CancelActorTeleportTransition(MovementHost& host, MovementActor& actor);
// decomp: BATTLE.PRG func_800A2CD4 (src/BATTLE/BATTLE.PRG/3A1A0.c:164)
void UpdateActorTravelTransition(MovementHost& host, MovementActor& actor);
// decomp: BATTLE.PRG func_800A48CC (src/BATTLE/BATTLE.PRG/3A1A0.c:1048)
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void BeginActorJumpMovement(MovementHost& host, int id, int direction,
                            int distance);
// decomp: BATTLE.PRG func_800A4A88 (src/BATTLE/BATTLE.PRG/3A1A0.c:1107)
void UpdateActorPlatformContact(MovementHost& host, MovementActor& actor,
                                PlatformContactPhase phase);
// decomp: BATTLE.PRG func_800A4D8C (src/BATTLE/BATTLE.PRG/3A1A0.c:1182)
void UpdateRoomActorMovement(MovementHost& host);
// decomp: BATTLE.PRG func_800A4E68 (src/BATTLE/BATTLE.PRG/3A1A0.c:1199)
void UpdateActorMovement(MovementHost& host, int id);
// decomp: BATTLE.PRG func_800A5280 (src/BATTLE/BATTLE.PRG/3A1A0.c:1347)
// The original's status result is never read; a step blocked before any
// probe climbs 0 (#294).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void MoveActorWithCollision(MovementHost& host, int id, int direction,
                            int distance, const MoveInput& input);
// decomp: BATTLE.PRG func_800A6660 (src/BATTLE/BATTLE.PRG/3A1A0.c:1920)
void UpdateActorSteering(MovementActor& actor, int target_facing,
                         bool restart_turn);
// decomp: BATTLE.PRG func_800AC690 (src/BATTLE/BATTLE.PRG/40564.c:2297)
void UpdateActorJumpAndFall(MovementHost& host, int id, MovementActor& actor);
}  // namespace VS::Actors
