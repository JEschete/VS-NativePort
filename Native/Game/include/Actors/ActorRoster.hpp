#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// The room actor roster (BATTLE.PRG 146C.c:884-946, 1842-1864, 6028 and
// 74088.c:1001): the list of room actors chained after Ashley (slot 0),
// removing an actor from its slot, freezing every actor's animation and the
// stats of a placeholder actor. ActorRosterLive binds Battle_actors and the
// actor records.
namespace VS::Actors {
// The actor slots the roster walks: Ashley, the room actors and gimmicks.
inline constexpr std::size_t kRosterSlots = 16;

// One roster slot as seen from the actor being linked, unlinked or removed.
struct RosterSlotView {
  bool occupied = false;
  // The slot's actor chains to that actor next.
  bool links_to_actor = false;
};
using RosterView = std::array<RosterSlotView, kRosterSlots>;

struct RosterCandidate {
  // The kind is an actor that joins the list (kind & 7: Ashley, enemies and
  // non-player characters).
  bool linkable_kind = false;
  // The actor is Ashley (actor id 0), the head of the list.
  bool leader = false;
};

enum class RosterEdit : std::uint8_t {
  kNone,
  // Chain the actor directly after the leader.
  kInsertAfterLeader,
  // Point slot's actor at the actor's successor, dropping the actor.
  kBypass,
  // The actor would join, but slot 0 is empty; the original dereferenced it.
  kRefusedNoLeader,
};

struct RosterDecision {
  RosterEdit edit = RosterEdit::kNone;
  std::size_t slot = 0;

  friend bool operator==(const RosterDecision&,
                         const RosterDecision&) = default;
};

// _insertActor (146C.c:884): a linkable non-leader actor that no slot chains
// to yet is inserted after the leader.
[[nodiscard]] RosterDecision LinkActorAfterAshley(const RosterView& view,
                                                  RosterCandidate candidate);
// _removeActor (146C.c:909): the first slot chaining to a non-leader actor
// bypasses it.
[[nodiscard]] RosterDecision UnlinkActorFromRoomList(const RosterView& view,
                                                     RosterCandidate candidate);

struct RosterOccupant {
  // A gimmick (kind 0x10) rather than a modelled actor.
  bool gimmick = false;
  // The actor owns a second model in slot + 4 (allocation kind 2).
  bool second_model = false;
};

// The live actor table RemoveActorAtSlot edits.
class ActorRemovalHost {
 public:
  ActorRemovalHost() = default;
  ActorRemovalHost(const ActorRemovalHost&) = delete;
  ActorRemovalHost& operator=(const ActorRemovalHost&) = delete;
  ActorRemovalHost(ActorRemovalHost&&) = delete;
  ActorRemovalHost& operator=(ActorRemovalHost&&) = delete;
  virtual ~ActorRemovalHost() = default;

  // The actor in `slot`, or nothing for an empty slot.
  [[nodiscard]] virtual std::optional<RosterOccupant> Occupant(
      std::size_t slot) const = 0;
  // Records the room enemy in `slot` (func_80069FC4).
  virtual void Remember(std::size_t slot, int hidden) = 0;
  virtual void ReleaseGimmick(std::size_t slot) = 0;
  virtual void ReleaseModel(std::size_t model_slot) = 0;
  virtual void FreeAiState(std::size_t slot) = 0;
  // The roster as seen from the actor in `slot`.
  [[nodiscard]] virtual RosterView Links(std::size_t slot) const = 0;
  // Points `predecessor`'s actor at the successor of `slot`'s actor.
  virtual void Bypass(std::size_t predecessor, std::size_t slot) = 0;
  // Frees `slot`'s actor and empties the slot.
  virtual void Free(std::size_t slot) = 0;
};

enum class RemovalResult : std::uint8_t { kOutOfRange, kEmpty, kRemoved };

// _removeActorAtIndex (146C.c:926): remembers the actor, releases its
// gimmick or models and AI state, unchains it and frees it.
RemovalResult RemoveActorAtSlot(ActorRemovalHost& host, std::uint32_t index,
                                int hidden);

// The live actor display flags (VS::Actors::LiveActorStateFlags and
// VS::Actors::SetLiveActorDisplayFlags).
class DisplayFlagsHost {
 public:
  DisplayFlagsHost() = default;
  DisplayFlagsHost(const DisplayFlagsHost&) = delete;
  DisplayFlagsHost& operator=(const DisplayFlagsHost&) = delete;
  DisplayFlagsHost(DisplayFlagsHost&&) = delete;
  DisplayFlagsHost& operator=(DisplayFlagsHost&&) = delete;
  virtual ~DisplayFlagsHost() = default;

  [[nodiscard]] virtual std::uint32_t StateFlags(std::size_t slot) const = 0;
  virtual void SetDisplayFlags(std::size_t slot, std::uint32_t flags) = 0;
};

inline constexpr std::uint32_t kAnimationFrozenFlag = 0x4;

// func_8006C250 (146C.c:1842): freezes each roster slot's animation.
void FreezeAllActorAnimations(DisplayFlagsHost& host);
// func_8006C2A0 (146C.c:1854): resumes each roster slot's animation.
void UnfreezeAllActorAnimations(DisplayFlagsHost& host);

inline constexpr std::size_t kActorNameBytes = 24;
// Battle_Actor::roomSpawnIndex and Battle_ActorStats::actorId of an actor no
// room spawn placed.
inline constexpr std::uint8_t kNoRoomSpawn = 0x80;
// The Battle_SetActorMarker flags a target-only placeholder receives.
inline constexpr int kPlaceholderMarkerFlags = 0x8;

// The stats of a placeholder actor; every other stat is zero.
struct PlaceholderActorStats {
  std::array<std::uint8_t, kActorNameBytes> name{};
  std::int16_t current_hp = 1;
  std::int16_t max_hp = 1;
  bool cancels_hit_results = false;
  // Battle_SetActorMarker flags for the slot, when it is marked.
  std::optional<int> marker_flags;
  std::uint8_t room_spawn_index = kNoRoomSpawn;
  std::uint8_t actor_id = kNoRoomSpawn;
  bool kill_counted = false;
  bool defeated = false;

  friend bool operator==(const PlaceholderActorStats&,
                         const PlaceholderActorStats&) = default;
};

// func_8007647C (146C.c:6028): a placeholder named `name`. The low byte of
// `auxiliary` selects whether it cancels hit results (all but 2 and 4) and
// whether its slot is marked (1).
[[nodiscard]] PlaceholderActorStats BuildPlaceholderActorStats(
    int auxiliary, std::span<const std::uint8_t, kActorNameBytes> name);

inline constexpr std::size_t kCombatLimbs = 6;

// actorLimb (74088.c:1001): the index of combat limb `limb`, or nothing when
// it is outside the six limbs (the original returned an unchecked address).
[[nodiscard]] std::optional<std::size_t> CombatLimbIndex(int limb);
}  // namespace VS::Actors
