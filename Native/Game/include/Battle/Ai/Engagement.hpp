#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

// Battle AI engagement: the room roster of actors closing in on the player,
// each actor's engagement timers and the awareness marker shown over an
// actor. The battle runtime owns the room and actor AI state; these routines
// see them only through the value records and hosts below.
namespace VS::Battle::Ai {
// Actor slots the engaged mask and the AI state table cover.
inline constexpr int kEngagementActorSlots = 16;
inline constexpr std::uint16_t kEngagementReadyFrames = 0x20;
inline constexpr std::uint16_t kEngagementLeaveCooldownFrames = 0x30;
inline constexpr std::uint16_t kReengagementGraceFrames = 90;
inline constexpr int kAwarenessMarkerReset = 0x40;
// Buildup frames after which an engaged actor gives up and leaves.
inline constexpr std::uint16_t kEngagementBuildupLimitFrames = 120;
// The buildup marker advances every eight frames, up to its fourth step.
inline constexpr std::uint16_t kBuildupMarkerStepFrames = 8;
inline constexpr int kBuildupMarkerSteps = 4;
inline constexpr int kGraceAwarenessMarker = 4;
// The engagement phase turns back at 16 and forward again at 0.
inline constexpr std::int8_t kEngagementPhaseTurn = 16;
// An engaged actor's pose (battle actor command flags bits 2-3): rising while
// its phase climbs, falling while it drops and after it leaves the list.
inline constexpr unsigned kEngagementPoseRising = 1;
inline constexpr unsigned kEngagementPoseFalling = 2;

// An actor's engagement timers (AI state 0x8E, 0x98 and 0x9A).
struct EngagementTimers {
  std::uint16_t engaged_frames = 0;
  std::uint16_t cooldown_frames = 0;
  std::uint16_t grace_frames = 0;
};

// The room's engagement roster (room AI state 0x0D, 0x10, 0x12 and 0x24).
struct EngagementRoster {
  std::uint16_t engaged_mask = 0;
  std::uint8_t engaged_count = 0;
  std::uint8_t limit = 0;
  std::int32_t approach_moves = 0;
};

// A temporary awareness marker and its remaining frames (AI state 0x12A and
// 0x138).
struct TemporaryAwareness {
  std::uint8_t frames_remaining = 0;
  std::uint16_t marker = 0;
};

class AwarenessMarkers {
 public:
  virtual ~AwarenessMarkers() = default;
  virtual void SetMarker(int actor_id, int marker) = 0;
};

// The actor's place in the room's engaged list and the battle actor list.
class EngagementHost : public AwarenessMarkers {
 public:
  // Another actor engaged during this AI update.
  [[nodiscard]] virtual bool CandidateEngagedThisUpdate() const = 0;
  virtual void ClearEngagementRequest() = 0;
  // Pushes the actor onto the front of the engaged list.
  virtual void LinkEngagedActor() = 0;
  virtual void UnlinkEngagedActor() = 0;
  // Whether the battle actor list holds exactly two actors; nothing when the
  // list is empty.
  [[nodiscard]] virtual std::optional<bool> HasSingleOpponent() const = 0;
};

// An engaged actor that remembers its target before it leaves.
class EngagementBuildupHost : public EngagementHost {
 public:
  // Remembers the actor's first-ranked target (func_800E4C8C).
  virtual void RememberActionTarget() = 0;
};

// An engaged actor's phase and the step added each animation frame (AI state
// 0x12F and 0x130).
struct EngagementPhase {
  std::int8_t phase = 0;
  std::int8_t step = 0;
};

// The room's engaged list as actor IDs below kEngagementActorSlots: its head
// and tail (room AI state 0x1C and 0x20) and each member's successor (AI state
// 0x1A0), with nothing for the null link. Each member's phase, pose and
// classification of Ashley are read through the same IDs.
class EngagedList {
 public:
  virtual ~EngagedList() = default;
  [[nodiscard]] virtual std::optional<int> Head() const = 0;
  virtual void SetHead(std::optional<int> actor_id) = 0;
  [[nodiscard]] virtual std::optional<int> Tail() const = 0;
  virtual void SetTail(std::optional<int> actor_id) = 0;
  [[nodiscard]] virtual std::optional<int> Next(int actor_id) const = 0;
  virtual void SetNext(int actor_id, std::optional<int> next) = 0;
  [[nodiscard]] virtual EngagementPhase Phase(int actor_id) const = 0;
  virtual void SetPhase(int actor_id, EngagementPhase phase) = 0;
  virtual void SetPose(int actor_id, unsigned pose) = 0;
  // The actor's classification of Ashley (relation word 0 bits 6-7).
  [[nodiscard]] virtual unsigned AshleyClassification(int actor_id) const = 0;
};

// The AI state table, indexed by actor IDs below kEngagementActorSlots.
class EngagementTable {
 public:
  virtual ~EngagementTable() = default;
  [[nodiscard]] virtual bool HasActorState(int actor_id) const = 0;
  virtual void DisengageActorState(int actor_id) = 0;
};

// decomp: func_800D820C (src/BATTLE/BATTLE.PRG/6FA0C.s:9).
// Shows a marker over the actor; Ashley (ID 0) has none. False refuses an ID
// outside the AI state table.
bool SetActorAwarenessIndicator(AwarenessMarkers& markers, int actor_id,
                                int marker);

// decomp: func_800D821C (src/BATTLE/BATTLE.PRG/6FA0C.s:18).
// Clears the marker, then shows the reset marker.
bool ResetActorAwarenessIndicator(AwarenessMarkers& markers, int actor_id);

// decomp: func_800D8260 (src/BATTLE/BATTLE.PRG/6FA60.c:407).
// Starts a temporary marker for `frames` frames. The AI state keeps the
// marker's low 16 bits and the count's low 8 bits, as the original's halfword
// and byte stores did.
void SetTemporaryAwarenessIndicator(TemporaryAwareness& awareness, int marker,
                                    int frames);

// decomp: func_800D826C (src/BATTLE/BATTLE.PRG/6FA6C.s:10).
[[nodiscard]] bool IsEngagementReady(std::uint16_t engaged_frames);

// decomp: func_800D8280 (src/BATTLE/BATTLE.PRG/6FA6C.s:22).
// Counts a temporary marker down, showing it until its last frame clears it.
// False refuses an ID outside the AI state table.
bool UpdateTemporaryAwarenessIndicator(AwarenessMarkers& markers, int actor_id,
                                       TemporaryAwareness& awareness);

// decomp: func_800D82A8 (src/BATTLE/BATTLE.PRG/6FA6C.s:37).
// Disengages the actor with AI state `actor_id`; Ashley and empty slots are
// ignored. Nothing refuses an ID outside the AI state table.
std::optional<bool> DisengageActorById(EngagementTable& table, int actor_id);

// decomp: func_800D82CC (src/BATTLE/BATTLE.PRG/6FA6C.s:53).
// Removes an engaged actor from the roster, starts its rejoin timers and
// clears its marker. False when it was not engaged; nothing refuses an ID
// outside the mask, an empty roster or an empty battle actor list.
std::optional<bool> DisengageActor(EngagementHost& host, int actor_id,
                                   EngagementTimers& timers,
                                   EngagementRoster& roster);

// decomp: func_800D836C (src/BATTLE/BATTLE.PRG/6FA6C.s:99).
// Whether the actor is engaged, adding it to the roster when the room allows
// another engagement. The original reads the room only for an actor that is
// not yet engaged, so `roster` may be null for an engaged actor. Nothing
// refuses an ID outside the mask, a full roster or a missing roster that is
// needed.
std::optional<bool> TryEngageActor(EngagementHost& host, int actor_id,
                                   EngagementTimers& timers,
                                   EngagementRoster* roster);

// decomp: func_800DBF00 (src/BATTLE/BATTLE.PRG/73700.c:414).
// Resets the actor's phase, shows the falling pose and removes the actor from
// the engaged list. A leaving tail moves to its successor or wraps to the
// head, and the tail is cleared when the list empties. An actor outside the
// list changes only its phase and pose. False refuses, changing nothing, an
// ID outside the slots or a walk that revisits an actor.
bool UnlinkEngagedActor(EngagedList& list, int actor_id);

// decomp: func_800DBFE4 (src/BATTLE/BATTLE.PRG/73700.c:463).
// Advances an engaged actor's buildup and shows its marker step. After
// kEngagementBuildupLimitFrames the actor remembers its target and leaves;
// the grace and cooldown timers then count down. The 16-bit buildup counter
// wraps to zero without leaving. `roster` is read only when the actor leaves.
// False refuses an ID outside the AI state table, or a leave that
// DisengageActor refuses or that has no roster.
bool UpdateEngagementBuildup(EngagementBuildupHost& host, int actor_id,
                             EngagementTimers& timers,
                             EngagementRoster* roster);

// decomp: func_800DC0D0 (src/BATTLE/BATTLE.PRG/73700.c:499).
// Steps the phase of each engaged actor that does not classify Ashley as an
// ally (kAllyClassification), until one turns at 0 or kEngagementPhaseTurn. A
// turning actor takes the matching pose and the walk stops there. Phases wrap
// as signed bytes. False refuses, changing nothing, a list that revisits an
// actor or holds an ID outside the slots.
bool AnimateEngagementPhase(EngagedList& list);
}  // namespace VS::Battle::Ai
