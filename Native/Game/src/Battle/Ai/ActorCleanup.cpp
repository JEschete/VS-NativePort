// Hand-maintained (tools/port/owned.csv).
// C counterparts: rood-boi pc/src/actor_cleanup.c:12-83,
// actor_update.c:44 and battle_movement.c:1688, revision
// 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Battle/Ai/ActorCleanup.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Battle/Ai/ReactionDelay.hpp"
#include "Core/Diagnostics.h"

namespace VS::Battle::Ai {
namespace {
constexpr std::uint16_t kReservationBit = 0x80;
constexpr unsigned kByteMask = 0xFF;
constexpr unsigned kByteBits = 8;
constexpr unsigned kShiftMask = 31;
// Commands whose end releases the actor's reservations (opcodes 11, 12, 18,
// 21 and 23) or its contact tile (opcodes 11 and 14).
constexpr std::uint32_t kReservationCommands = 0xA41800;
constexpr std::uint32_t kContactCommands = 0x4800;
constexpr unsigned kMovementBits = 0x1F;
constexpr unsigned kReactionBits = 0xB;
constexpr unsigned kSupportLayerBits = 0xF;
constexpr unsigned kAttributeKinds = 0x14;
constexpr unsigned kAiKinds = 5;
constexpr std::uint8_t kCommandModeMask = 0xFC;
constexpr std::uint16_t kOrbitingBossFirst = 0xAC;
constexpr std::uint16_t kOrbitingBossSecond = 0xAD;
// Relationship word fields (Battle_Ai_TargetRelation).
constexpr unsigned kClassificationShift = 6;
constexpr unsigned kClassificationMask = 3;
constexpr unsigned kClassificationTimerShift = 20;
constexpr unsigned kClassificationTimerMask = 0x7F;
constexpr unsigned kPreviousClassificationShift = 28;
constexpr std::uint32_t kHostilityClearMask = 0xFFFF803Fu;
constexpr std::uint32_t kHostileClassification = 0xC0;
constexpr unsigned kHostile = 3;
constexpr int kHostilityIndicator = 6;
constexpr int kHostilityIndicatorFrames = 40;
constexpr int kHostilityReaction = 2;
constexpr int kHostilityReactionRange = 128;
// Packed persistence flag positions.
constexpr unsigned kPackedClassShift = 1;
constexpr unsigned kPackedPreviousClassShift = 3;
constexpr std::int32_t kPackedIdleFlag = 0x20;
constexpr unsigned kPackedTimerShift = 6;

bool Refuse(const char* message) {
  Core_Debugf("ai", "%s", message);
  return false;
}

bool IsCleanupSlot(int actor_id) {
  return actor_id >= 0 && actor_id < kCleanupActorSlots;
}

std::uint32_t ActorBit(int actor_id) {
  return 1u << static_cast<unsigned>(actor_id);
}

bool CommandIn(std::uint8_t opcode, std::uint32_t commands) {
  return ((1u << (opcode & kShiftMask)) & commands) != 0;
}

unsigned TileByte(std::uint32_t tile, unsigned index) {
  return (tile >> (index * kByteBits)) & kByteMask;
}

// Releases a present AI actor's reservations through the host.
bool ReleaseHostReservations(ActorCleanupHost& host, int actor_id,
                             std::uint8_t navigation_reset) {
  std::optional<ReservationState> state = host.Reservations(actor_id);
  if (!state.has_value()) return true;
  std::optional<CleanupRoom> room = host.Room();
  if (!room.has_value()) {
    return Refuse("AI reservation release requires the room's AI state");
  }
  if (!ReleaseActorAiReservations(*state, *room, host.TileFlags())) {
    return false;
  }
  state->navigation_reset = navigation_reset;
  host.StoreReservations(actor_id, *state);
  host.StoreRoom(*room);
  return true;
}

void ClearLinkedActorReferences(ActorCleanupHost& host, int actor_id) {
  for (int slot = 0; slot < kCleanupActorSlots; ++slot) {
    if (host.LinkedActor(slot) == actor_id) {
      host.SetLinkedActor(slot, kNoLinkedActor);
    }
  }
}

// Drops the departing actor from the attribute mask, the other actors' links
// and the room's special actors.
bool DetachActorReferences(ActorCleanupHost& host, const TeardownActor& actor) {
  if ((actor.id > 0 && actor.id < kCleanupActorSlots) ||
      (actor.kind & kAttributeKinds) != 0) {
    if (!IsCleanupSlot(actor.id)) {
      return Refuse("AI teardown attribute actor is outside the AI slots");
    }
    // The original subtracts the bit for attribute kinds only; any AI actor
    // leaving the battle clears it here so a stale bit cannot underflow.
    host.StoreAttributeActorMask(host.AttributeActorMask() &
                                 ~ActorBit(actor.id));
  }
  ClearLinkedActorReferences(host, actor.id);
  std::optional<CleanupRoom> room = host.Room();
  if (!room.has_value()) return Refuse("AI teardown requires the room state");
  for (BattleActorHandle& special : room->special_actors) {
    if (special == actor.handle) special = kNoBattleActor;
  }
  host.StoreRoom(*room);
  return true;
}

bool LeaveEngagement(ActorCleanupHost& host, int actor_id, bool has_state) {
  std::optional<CleanupRoom> room = host.Room();
  if (!room.has_value()) return Refuse("AI teardown requires the room state");
  const std::uint32_t bit = ActorBit(actor_id);
  if ((room->engaged_mask & bit) == 0) return true;
  if (room->engaged_count == 0 || !has_state) {
    return Refuse("AI teardown found inconsistent engagement membership");
  }
  room->engaged_mask = static_cast<std::uint16_t>(room->engaged_mask & ~bit);
  --room->engaged_count;
  host.StoreRoom(*room);
  return true;
}
}  // namespace

// decomp: func_800E4C28 (src/BATTLE/BATTLE.PRG/7BDD4.s:550).
bool ReleaseAiDestinationTile(std::span<std::uint16_t> tile_flags, int x,
                              int z) {
  if (tile_flags.empty()) return true;
  if (x < 0 || z < 0 || x >= kTileFlagGridSide || z >= kTileFlagGridSide) {
    return Refuse("AI reservation release tile is outside the tile grid");
  }
  const auto index = static_cast<std::size_t>(z) * kTileFlagGridSide +
                     static_cast<std::size_t>(x);
  if (index >= tile_flags.size()) {
    return Refuse("AI reservation release tile is outside the tile grid");
  }
  tile_flags[index] =
      static_cast<std::uint16_t>(tile_flags[index] & ~kReservationBit);
  return true;
}

// decomp: func_800E678C (src/BATTLE/BATTLE.PRG/7DE94.s:91).
bool ReleaseActorAiReservations(ReservationState& state, CleanupRoom& room,
                                std::span<std::uint16_t> tile_flags) {
  state.navigation_reset = 0;
  const unsigned x = TileByte(state.next_tile, 0);
  const unsigned z = TileByte(state.next_tile, 2);
  // The range check compares X with room bytes 2-0 and Z with bytes 3-1.
  if (x >= room.min_x && x <= room.max_x && z >= room.min_z &&
      z <= room.max_z &&
      !ReleaseAiDestinationTile(tile_flags, static_cast<int>(x),
                                static_cast<int>(z))) {
    return false;
  }
  if (state.holds_route_gate) {
    const unsigned flags = room.flags;
    room.flags =
        static_cast<std::uint8_t>(flags & (0u - (1u << (flags & kShiftMask))));
  }
  return true;
}

// decomp: func_800E6764 (src/BATTLE/BATTLE.PRG/7DE94.s:73).
bool ForceReleaseActorAiReservations(ActorCleanupHost& host, int actor_id) {
  if (!IsCleanupSlot(actor_id)) {
    return Refuse("AI reservation release actor ID is outside the AI slots");
  }
  return ReleaseHostReservations(host, actor_id, 1);
}

// decomp: func_800E6700 (src/BATTLE/BATTLE.PRG/7DE94.s:42).
bool RefreshActorAiReservations(ActorCleanupHost& host, int actor_id) {
  if (!IsCleanupSlot(actor_id)) {
    return Refuse("AI reservation refresh actor ID is outside the AI slots");
  }
  const std::optional<std::uint8_t> opcode = host.CommandOpcode(actor_id);
  if (!opcode.has_value() || !CommandIn(*opcode, kReservationCommands)) {
    return true;
  }
  const std::optional<ModelMotion> motion = host.RecordMotion(actor_id);
  if (!motion.has_value()) {
    return Refuse("AI reservation refresh requires the actor's record");
  }
  if (((motion->movement & kMovementBits) |
       (motion->reaction & kReactionBits)) != 0) {
    return true;
  }
  return ReleaseHostReservations(host, actor_id, 0);
}

// decomp: func_800E5240 (src/BATTLE/BATTLE.PRG/7C7E0.s:196).
bool FreeActorNavigationOverride(ActorCleanupHost& host, int actor_id) {
  if (!IsCleanupSlot(actor_id)) {
    return Refuse("AI navigation override actor ID is outside the AI slots");
  }
  if (!host.HasTileFlagMap() || !host.HasAiState(actor_id)) return true;
  if (host.HasNavigationOverride(actor_id)) {
    host.FreeNavigationOverride(actor_id);
  }
  return true;
}

// decomp: func_800E6EB0 (src/BATTLE/BATTLE.PRG/7E05C.s:510).
bool ReleaseActorAiContactOccupancy(ActorCleanupHost& host, int actor_id) {
  if (!IsCleanupSlot(actor_id)) {
    return Refuse("AI contact release actor ID is outside the AI slots");
  }
  const std::optional<std::uint8_t> opcode = host.CommandOpcode(actor_id);
  const std::optional<ReservationState> state = host.Reservations(actor_id);
  if (!opcode.has_value() && !state.has_value()) return true;
  if (!opcode.has_value() || !state.has_value()) {
    return Refuse(
        "AI contact release requires both a battle actor and an AI "
        "state");
  }
  if (!CommandIn(*opcode, kContactCommands)) return true;
  const unsigned contact = TileByte(state->next_tile, 1);
  if (contact == 0) return true;
  // The original reads the model before testing the contact tile; without
  // one it would read low memory.
  const std::optional<std::uint8_t> support = host.StateModelSupport(actor_id);
  if (!support.has_value()) {
    return Refuse("AI contact release requires the AI state's model");
  }
  if ((*support & kSupportLayerBits) != 0) return true;
  host.ClearContactOccupancy(static_cast<int>(contact));
  return true;
}

// decomp: func_800E7608 (src/BATTLE/BATTLE.PRG/7E05C.s:1052).
bool ClearActorAiLinksAndPlan(ActorCleanupHost& host, int actor_id) {
  if (!IsCleanupSlot(actor_id)) {
    return Refuse("AI link clearing actor ID is outside the AI slots");
  }
  ClearLinkedActorReferences(host, actor_id);
  if (host.HasAiState(actor_id)) host.Disengage(actor_id);
  return true;
}

// decomp: func_800E6694 (src/BATTLE/BATTLE.PRG/7DE94.s:9).
std::optional<std::int16_t> PackActorAiPersistenceFlags(
    const ActorCleanupHost& host, int actor_id) {
  if (!IsCleanupSlot(actor_id)) {
    Refuse("AI persistence actor ID is outside the AI slots");
    return std::nullopt;
  }
  const std::optional<PersistenceFields> fields = host.Persistence(actor_id);
  if (!fields.has_value()) return -1;
  const std::uint32_t relation = fields->relation_to_ashley;
  const std::uint32_t classification =
      (relation >> kClassificationShift) & kClassificationMask;
  const std::uint32_t previous =
      (relation >> kPreviousClassificationShift) & kClassificationMask;
  const std::uint32_t timer =
      (relation >> kClassificationTimerShift) & kClassificationTimerMask;
  const std::uint32_t idle = fields->behavior_mode == 0 ? kPackedIdleFlag : 0;
  return static_cast<std::int16_t>(fields->uses_room_action_delay |
                                   (classification << kPackedClassShift) |
                                   (previous << kPackedPreviousClassShift) |
                                   idle | (timer << kPackedTimerShift));
}

// decomp: func_800E6B24 (src/BATTLE/BATTLE.PRG/7E05C.s:241).
bool ClearActorAiNavigationReset(ActorCleanupHost& host, int actor_id) {
  if (!IsCleanupSlot(actor_id)) {
    return Refuse("AI navigation reset actor ID is outside the AI slots");
  }
  std::optional<ReservationState> state = host.Reservations(actor_id);
  if (!state.has_value()) return true;
  state->navigation_reset = 0;
  host.StoreReservations(actor_id, *state);
  return true;
}

// decomp: func_800E6EAC (src/BATTLE/BATTLE.PRG/7E05C.s:504).
// The original's return delay slot only changes caller-saved t0.
void InitializeActorAiExtensionHook() {}

// decomp: func_800E7660 (src/BATTLE/BATTLE.PRG/7E05C.s:1080).
bool AllocateOrbitingBossState(ActorCleanupHost& host) {
  if (!host.AllocateOrbitingBossState(kOrbitingBossInitialCooldown)) {
    return Refuse("AI orbiting boss state allocation failed");
  }
  return true;
}

// decomp: func_800E7454 (src/BATTLE/BATTLE.PRG/7E05C.s:924).
bool FreeActorAiState(ActorCleanupHost& host, TeardownActor& actor) {
  if (!host.HasTileFlagMap()) return true;
  if (actor.id < 0 || actor.id >= host.BattleActorSlots()) {
    return Refuse("AI teardown actor is outside the battle actor table");
  }
  if (!DetachActorReferences(host, actor)) return false;
  // A script may reclassify an actor without retiring its AI state, so a
  // present state is torn down whatever the actor's kind.
  const bool has_state =
      actor.id < kCleanupActorSlots && host.HasAiState(actor.id);
  if (!has_state && (actor.kind & kAiKinds) == 0) return true;
  if (!IsCleanupSlot(actor.id)) {
    return Refuse("AI teardown actor is outside the AI slots");
  }
  if (!FreeActorNavigationOverride(host, actor.id) ||
      !ReleaseHostReservations(host, actor.id, 0)) {
    return false;
  }
  if (actor.id == 0) return true;
  if (!LeaveEngagement(host, actor.id, has_state)) return false;
  actor.command_flags &= kCommandModeMask;
  host.StoreCommandFlags(actor.handle, actor.command_flags);
  if (has_state) {
    host.ResetAwarenessIndicator(actor.id);
    host.UnlinkEngagedActor(actor.id);
  }
  host.RetireAiState(actor.id);
  if (actor.sub_type == kOrbitingBossFirst ||
      actor.sub_type == kOrbitingBossSecond) {
    host.FreeOrbitingBossState();
  }
  return true;
}

// decomp: func_800E48A8 (src/BATTLE/BATTLE.PRG/7BDD4.s:246).
bool SetActorAiReactionWithDelay(ReactionDelayState& state,
                                 ReactionDelayHost& host, int value,
                                 int range) {
  // The original draws before testing whether the actor may react.
  const int roll = host.NextRandomMasked(range);
  if (state.has_navigation_override || state.decision_delay != 0 ||
      roll >= range) {
    return false;
  }
  state.decision_delay = kReactionDelayFrames;
  host.SetCommandFlags(static_cast<std::uint8_t>(value));
  return true;
}

// decomp: func_800DC784 (src/BATTLE/BATTLE.PRG/73C84.s:246).
bool MakeTargetHostile(HostilityState& state, HostilityHost& host, int target) {
  if (!IsCleanupSlot(target)) {
    return Refuse("AI hostility target is outside the AI slots");
  }
  const auto slot = static_cast<std::size_t>(target);
  if (((state.relations.at(slot) >> kClassificationShift) &
       kClassificationMask) != kHostile) {
    // The original tests (rand() & 0x7F) < 0x80, which always holds.
    host.DrawBiosRandom();
    state.alerted = 1;
    host.ShowAwarenessIndicator(state, kHostilityIndicator,
                                kHostilityIndicatorFrames);
    host.RequestReaction(state, kHostilityReaction, kHostilityReactionRange);
  }
  state.relations.at(slot) =
      (state.relations.at(slot) & kHostilityClearMask) | kHostileClassification;
  return true;
}
}  // namespace VS::Battle::Ai
