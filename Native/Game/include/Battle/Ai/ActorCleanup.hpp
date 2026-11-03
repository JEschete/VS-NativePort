#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// Battle AI actor cleanup: releasing the destination tiles and room flags an
// AI actor reserves, detaching it from the other actors, and tearing down its
// AI state when it leaves the battle. The battle runtime owns the AI state
// table, the room's AI state and the tile flag grid; these routines see one
// actor's reservation fields through ReservationState and everything else
// through the cleanup hosts, so they reach no AI state record, PS1 address
// or heap.
namespace VS::Battle::Ai {
inline constexpr int kCleanupActorSlots = 16;
// The tile flag grid is 32 tiles square, row by row along Z.
inline constexpr int kTileFlagGridSide = 32;
inline constexpr std::size_t kRoomSpecialActorSlots = 4;
// A linked-actor field holding this value links no actor.
inline constexpr int kNoLinkedActor = kCleanupActorSlots;
// The cooldown a newly allocated orbiting boss state starts with.
inline constexpr std::int16_t kOrbitingBossInitialCooldown = 900;

// A battle actor, compared but never dereferenced.
enum class BattleActorHandle : std::uintptr_t {};
inline constexpr BattleActorHandle kNoBattleActor{};

// One actor's reservation fields (offsets in the AI state record).
struct ReservationState {
  std::uint8_t navigation_reset = 0;  // 0x1A
  // The tile the actor is stepping onto: X in byte 0, the contact tile in
  // byte 1 and Z in byte 2 (0x170).
  std::uint32_t next_tile = 0;
  // Byte 0x19B: the actor holds a route gate.
  bool holds_route_gate = false;
};

// The room AI state fields cleanup changes.
struct CleanupRoom {
  // The reservable tile range (room bytes 0-3).
  std::uint8_t max_x = 0;
  std::uint8_t max_z = 0;
  std::uint8_t min_x = 0;
  std::uint8_t min_z = 0;
  std::uint8_t flags = 0;          // 0x0E
  std::uint16_t engaged_mask = 0;  // 0x10
  std::uint8_t engaged_count = 0;  // 0x12
  std::array<BattleActorHandle, kRoomSpecialActorSlots> special_actors{};
};

// The model motion bits that keep a reservation held.
struct ModelMotion {
  std::uint8_t movement = 0;  // model byte 0x0A
  std::uint8_t reaction = 0;  // model byte 0x0B
};

// The AI state fields an actor's save record keeps.
struct PersistenceFields {
  std::uint8_t uses_room_action_delay = 0;  // 0x13
  std::uint32_t relation_to_ashley = 0;     // 0x1A4
  std::uint8_t behavior_mode = 0;           // 0x12D
};

// The battle actor an AI teardown removes.
struct TeardownActor {
  BattleActorHandle handle{};
  int id = 0;                      // actor 0x04
  std::uint8_t command_flags = 0;  // actor 0x0B
  std::uint16_t sub_type = 0;      // actor 0x1A
  std::uint8_t kind = 0;           // actor 0x1C
};

// The AI state table, the room, the battle actors and the heap the cleanup
// routines use. Slot arguments are below kCleanupActorSlots.
class ActorCleanupHost {
 public:
  virtual ~ActorCleanupHost() = default;

  [[nodiscard]] virtual bool HasTileFlagMap() const = 0;
  // The tile flag grid, kTileFlagGridSide halfwords per row; empty without
  // one.
  [[nodiscard]] virtual std::span<std::uint16_t> TileFlags() = 0;
  [[nodiscard]] virtual bool HasAiState(int actor_id) const = 0;
  // Nothing when the slot has no AI state.
  [[nodiscard]] virtual std::optional<ReservationState> Reservations(
      int actor_id) const = 0;
  virtual void StoreReservations(int actor_id,
                                 const ReservationState& state) = 0;
  // Nothing when the room has no AI state.
  [[nodiscard]] virtual std::optional<CleanupRoom> Room() const = 0;
  virtual void StoreRoom(const CleanupRoom& room) = 0;
  // The actor the slot's AI state follows; nothing without an AI state.
  [[nodiscard]] virtual std::optional<int> LinkedActor(int actor_id) const = 0;
  virtual void SetLinkedActor(int actor_id, int linked) = 0;
  // The battle actor's command opcode; nothing without a battle actor.
  [[nodiscard]] virtual std::optional<std::uint8_t> CommandOpcode(
      int actor_id) const = 0;
  // The actor record's model motion; nothing without an actor record.
  [[nodiscard]] virtual std::optional<ModelMotion> RecordMotion(
      int actor_id) const = 0;
  // Model byte 0x0C of the model the AI state holds; nothing without one.
  [[nodiscard]] virtual std::optional<std::uint8_t> StateModelSupport(
      int actor_id) const = 0;
  [[nodiscard]] virtual std::optional<PersistenceFields> Persistence(
      int actor_id) const = 0;
  [[nodiscard]] virtual std::uint32_t AttributeActorMask() const = 0;
  virtual void StoreAttributeActorMask(std::uint32_t mask) = 0;
  // The battle actor table's slot count.
  [[nodiscard]] virtual int BattleActorSlots() const = 0;
  [[nodiscard]] virtual bool HasNavigationOverride(int actor_id) const = 0;

  // Stores the departing battle actor's command flags.
  virtual void StoreCommandFlags(BattleActorHandle actor,
                                 std::uint8_t flags) = 0;
  // Detaches and frees the slot's navigation override.
  virtual void FreeNavigationOverride(int actor_id) = 0;
  virtual void ClearContactOccupancy(int contact) = 0;
  virtual void Disengage(int actor_id) = 0;
  virtual void ResetAwarenessIndicator(int actor_id) = 0;
  virtual void UnlinkEngagedActor(int actor_id) = 0;
  // Empties the slot in the AI state and frame tables and frees its AI
  // state.
  virtual void RetireAiState(int actor_id) = 0;
  // Frees the arena's orbiting boss state, if any.
  virtual void FreeOrbitingBossState() = 0;
  // Allocates a zeroed orbiting boss state with `cooldown`; false when the
  // heap is exhausted.
  virtual bool AllocateOrbitingBossState(std::int16_t cooldown) = 0;
};

// The actor whose reaction is delayed.
struct ReactionDelayState {
  bool has_navigation_override = false;  // 0x18C
  std::uint8_t decision_delay = 0;       // 0x155
};

class ReactionDelayHost {
 public:
  virtual ~ReactionDelayHost() = default;
  // A random value below `range` from the actor random stream.
  virtual int NextRandomMasked(int range) = 0;
  virtual void SetCommandFlags(std::uint8_t flags) = 0;
};

// The actor turning hostile toward a target.
struct HostilityState {
  std::uint8_t alerted = 0;                                   // 0x18
  std::array<std::uint32_t, kCleanupActorSlots> relations{};  // 0x1A4
};

// The effects a newly hostile actor shows. Each sees the state as stored
// before it runs, and its changes are read back afterwards.
class HostilityHost {
 public:
  virtual ~HostilityHost() = default;
  virtual void DrawBiosRandom() = 0;
  virtual void ShowAwarenessIndicator(HostilityState& state, int indicator,
                                      int frames) = 0;
  virtual void RequestReaction(HostilityState& state, int value, int range) = 0;
};

// decomp: func_800E4C28 (src/BATTLE/BATTLE.PRG/7BDD4.s:550).
// Clears tile (x, z)'s reservation bit. Nothing happens without a grid;
// false refuses a tile outside it.
bool ReleaseAiDestinationTile(std::span<std::uint16_t> tile_flags, int x,
                              int z);

// decomp: func_800E678C (src/BATTLE/BATTLE.PRG/7DE94.s:91).
// Clears the navigation reset, releases the next tile when it lies in the
// room's reservable range and, for a route gate holder, the room flag the
// gate reserves. False refuses a room range reaching outside the grid.
bool ReleaseActorAiReservations(ReservationState& state, CleanupRoom& room,
                                std::span<std::uint16_t> tile_flags);

// decomp: func_800E6764 (src/BATTLE/BATTLE.PRG/7DE94.s:73).
// Releases an AI actor's reservations and flags its navigation for a reset.
// Nothing happens without an AI state; false refuses an ID outside the slots
// or a missing room.
bool ForceReleaseActorAiReservations(ActorCleanupHost& host, int actor_id);

// decomp: func_800E6700 (src/BATTLE/BATTLE.PRG/7DE94.s:42).
// Releases an AI actor's reservations when its command is one that frees
// them (opcodes 11, 12, 18, 21 and 23) and its model is neither moving nor
// reacting. False refuses an ID outside the slots, a missing actor record or
// a missing room.
bool RefreshActorAiReservations(ActorCleanupHost& host, int actor_id);

// decomp: func_800E5240 (src/BATTLE/BATTLE.PRG/7C7E0.s:196).
// Frees the actor's navigation override while a tile flag grid exists.
// False refuses an ID outside the slots.
bool FreeActorNavigationOverride(ActorCleanupHost& host, int actor_id);

// decomp: func_800E6EB0 (src/BATTLE/BATTLE.PRG/7E05C.s:510).
// Frees the contact tile an actor holds under command 11 or 14 once its
// model stands on no moving platform. False refuses an ID outside the
// slots, a battle actor without an AI state (or the reverse) or an AI state
// without a model.
bool ReleaseActorAiContactOccupancy(ActorCleanupHost& host, int actor_id);

// decomp: func_800E7608 (src/BATTLE/BATTLE.PRG/7E05C.s:1052).
// Unlinks every AI actor following `actor_id`, then disengages it. False
// refuses an ID outside the slots.
bool ClearActorAiLinksAndPlan(ActorCleanupHost& host, int actor_id);

// decomp: func_800E6694 (src/BATTLE/BATTLE.PRG/7DE94.s:9).
// The actor's AI flags for its save record, or -1 without an AI state.
// Nothing refuses an ID outside the slots.
[[nodiscard]] std::optional<std::int16_t> PackActorAiPersistenceFlags(
    const ActorCleanupHost& host, int actor_id);

// decomp: func_800E6B24 (src/BATTLE/BATTLE.PRG/7E05C.s:241).
// Clears an AI actor's navigation reset. False refuses an ID outside the
// slots.
bool ClearActorAiNavigationReset(ActorCleanupHost& host, int actor_id);

// decomp: func_800E6EAC (src/BATTLE/BATTLE.PRG/7E05C.s:504).
// The AI extension hook the frame loop calls; it does nothing.
void InitializeActorAiExtensionHook();

// decomp: func_800E7660 (src/BATTLE/BATTLE.PRG/7E05C.s:1080).
// Gives the arena a zeroed orbiting boss state with the initial cooldown.
// False refuses an exhausted heap.
bool AllocateOrbitingBossState(ActorCleanupHost& host);

// decomp: func_800E7454 (src/BATTLE/BATTLE.PRG/7E05C.s:924).
// Removes an actor leaving the battle from the attribute mask, the other
// actors' links, the room's special actors and engagement, frees its
// navigation override and AI state, and frees the orbiting boss state with
// an orbiting boss. False refuses an actor outside the battle actor table,
// an AI actor outside the AI slots, a missing room or inconsistent
// engagement.
bool FreeActorAiState(ActorCleanupHost& host, TeardownActor& actor);

// decomp: func_800E48A8 (src/BATTLE/BATTLE.PRG/7BDD4.s:246).
// Draws a random value below `range` and, unless the actor follows a
// navigation override or already waits, makes it wait 30 frames before
// reacting with command flags `value` when the draw is below `range`.
// Returns whether the reaction was set.
bool SetActorAiReactionWithDelay(ReactionDelayState& state,
                                 ReactionDelayHost& host, int value, int range);

// decomp: func_800DC784 (src/BATTLE/BATTLE.PRG/73C84.s:246).
// Classifies `target` as hostile; an actor not already hostile toward it
// is alerted, shows the hostility indicator and reacts after a delay. False
// refuses a target outside the slots.
bool MakeTargetHostile(HostilityState& state, HostilityHost& host, int target);
}  // namespace VS::Battle::Ai
