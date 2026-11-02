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
}  // namespace VS::Battle::Ai
