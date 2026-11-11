#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "Battle/Ai/BlockedMovement.hpp"
#include "Core/Math/Distance.hpp"

// AI perception: how an actor sees each battle actor (line of sight, view
// cone, range, occlusion and hostility decay), and the per-frame pass that
// refreshes every listed actor's perception, tile tracking and alert state.
// The battle runtime owns the AI states, models, frame context and room
// state; these routines see them only through the records and worlds below.
namespace VS::Battle::Ai {
// Relation and distance slots in an AI state (0x1A4 and 0x1E4).
inline constexpr int kPerceptionSlots = 16;
// Entries in the actor record table (BATTLE.PRG D_800F4538).
inline constexpr int kPerceptionRecordSlots = 17;

// decomp: actorRelation (src/BATTLE/BATTLE.PRG/6FC00.c:608).
// One relation word: how the observing actor perceives one target.
struct PerceivedRelation {
  bool active = false;                       // bit 0
  bool was_active = false;                   // bit 1
  bool clear_at_point_height = false;        // bit 2
  bool in_view = false;                      // bit 3
  bool reserved4 = false;                    // bit 4, kept
  bool visible = false;                      // bit 5
  std::uint8_t classification = 0;           // bits 6-7
  std::uint8_t counter = 0;                  // bits 8-14
  std::uint8_t timer = 0;                    // bits 15-19
  std::uint8_t classification_timer = 0;     // bits 20-26
  bool blocked = false;                      // bit 27
  std::uint8_t previous_classification = 0;  // bits 28-29
  bool clear_at_body_height = false;         // bit 30
  bool marked = false;                       // bit 31

  friend bool operator==(const PerceivedRelation&,
                         const PerceivedRelation&) = default;
};

[[nodiscard]] PerceivedRelation DecodePerceivedRelation(std::uint32_t word);
[[nodiscard]] std::uint32_t EncodePerceivedRelation(
    const PerceivedRelation& relation);

// decomp: actorRelationState (src/BATTLE/BATTLE.PRG/6FC00.c:638).
// The observing actor's AI state fields the target update reads and writes.
struct PerceivingObserver {
  bool decays_hostility = false;            // 0x22
  bool ignores_body_line = false;           // 0x25
  std::uint32_t tile = 0;                   // 0x34
  Core::Math::Position3 position;           // 0x4C
  std::int16_t point_height = 0;            // 0x82
  std::uint8_t actor_id = 0;                // 0x88
  std::uint32_t near_distance_squared = 0;  // 0xA4
  std::uint32_t view_cosine = 0;            // 0xA8, compared signed
  std::uint32_t far_distance_squared = 0;   // 0xAC
  std::int16_t body_height = 0;             // 0xB8
  std::uint8_t phase = 0;                   // 0x12B
  std::array<PerceivedRelation, kPerceptionSlots> relations{};      // 0x1A4
  std::array<std::uint32_t, kPerceptionSlots> distances_squared{};  // 0x1E4

  friend bool operator==(const PerceivingObserver&,
                         const PerceivingObserver&) = default;
};

// A target's model (or gimmick) as perception reads it.
struct PerceivedModel {
  Core::Math::Position3 position;  // 0x1C
  std::uint32_t tile = 0;          // 0x5C
  std::uint32_t last_tile = 0;     // 0x60
  // The model is in a locomotion state (word 8 bits 19-20).
  bool locomoting = false;
};

// The observing actor's own model: where it stands and where it looks.
struct ObserverPose {
  Core::Math::Position3 position;  // 0x1C
  std::int16_t facing = 0;         // 0x26
  std::int16_t look_yaw = 0;       // 0x14
};

// Another actor's AI state sizes perception uses.
struct PerceptionShape {
  std::int16_t body_height = 0;   // 0xB8
  std::int16_t point_height = 0;  // 0x82
  std::uint16_t radius = 0;       // 0x92
};

// A battle actor list entry and its model's position, when it has a model.
struct ListedPerceptionActor {
  int id = 0;
  std::optional<Core::Math::Position3> position;
};

// The actor tables, the frame context's tile flags and the terrain. Target
// IDs are already checked against kPerceptionSlots.
class TargetPerceptionWorld {
 public:
  TargetPerceptionWorld() = default;
  TargetPerceptionWorld(const TargetPerceptionWorld&) = delete;
  TargetPerceptionWorld& operator=(const TargetPerceptionWorld&) = delete;
  TargetPerceptionWorld(TargetPerceptionWorld&&) = delete;
  TargetPerceptionWorld& operator=(TargetPerceptionWorld&&) = delete;
  virtual ~TargetPerceptionWorld() = default;

  // The battle actor's model; nothing without an actor or a model.
  [[nodiscard]] virtual std::optional<PerceivedModel> Model(
      int target) const = 0;
  // The observer's model; nothing when it has none.
  [[nodiscard]] virtual std::optional<ObserverPose> ObserverModel() const = 0;
  // The AI state table's entry (BATTLE.PRG D_800F5878).
  [[nodiscard]] virtual std::optional<PerceptionShape> AiStateShape(
      int target) const = 0;
  // The frame context's entry (scratch 0x1F80037C).
  [[nodiscard]] virtual std::optional<PerceptionShape> FrameShape(
      int target) const = 0;
  [[nodiscard]] virtual bool HasGimmick(int target) const = 0;
  // Whether the tile carries the 0x2000 flag; nothing without the frame's
  // tile flags or for a tile outside them.
  [[nodiscard]] virtual std::optional<bool> IsMarkedTile(
      Core::Math::TileCoordinate tile) const = 0;
  // func_800E44E8.
  [[nodiscard]] virtual bool IsTerrainSegmentBlocked(Core::Math::Position3 from,
                                                     Core::Math::Position3 to,
                                                     int from_height,
                                                     int to_height) const = 0;
  // The battle actor list in order.
  [[nodiscard]] virtual std::vector<ListedPerceptionActor> ListedActors()
      const = 0;
};

// decomp: checkExtent (src/BATTLE/BATTLE.PRG/6FC00.c:646), inlined in
// func_800D87E8.
// Whether terrain blocks the line from the target to the observer at body
// height (the target's frame shape, 128 without one).
[[nodiscard]] bool IsTargetOccludedAtBodyHeight(
    const TargetPerceptionWorld& world, const PerceivingObserver& observer,
    int target, Core::Math::Position3 target_position);

// decomp: checkHeight (src/BATTLE/BATTLE.PRG/6FC00.c:660), inlined in
// func_800D87E8.
// Whether terrain blocks the line at both actors' point heights, taken as
// magnitudes (64 for a target without a frame shape).
[[nodiscard]] bool IsTargetOccludedAtPointHeight(
    const TargetPerceptionWorld& world, const PerceivingObserver& observer,
    int target, Core::Math::Position3 target_position);

// decomp: func_800D87E8 (src/BATTLE/BATTLE.PRG/6FC00.c:679).
// Refreshes the observer's distance to and relation with one target: line of
// sight (one height per call, alternating), view cone, activity, the marked
// tile, occlusion by nearer actors, and hostility decay. False refuses a
// target outside the slots, a target or observer without a model, a tile the
// frame cannot flag, or a listed actor outside the slots or without a model
// where its position is wanted; the original read past a table or through a
// null pointer there.
[[nodiscard]] bool UpdateTargetPerception(TargetPerceptionWorld& world,
                                          PerceivingObserver& observer,
                                          int target);

// The room AI state and the engagement flag the pass resets.
struct PerceptionRoom {
  std::uint16_t readiness_level = 0;   // room 0x18
  std::int32_t approach_moves = 0;     // room 0x24
  std::int32_t candidate_engaged = 0;  // BATTLE.PRG D_800F58E0

  friend bool operator==(const PerceptionRoom&,
                         const PerceptionRoom&) = default;
};

// decomp: func_800D8EF4 prologue (src/BATTLE/BATTLE.PRG/6FC00.c:1003).
// Clears the engagement flag and the approach moves and raises the room's
// readiness level, saturating at 0xFFFE.
void BeginActorPerception(PerceptionRoom& room);

// The listed battle actor fields the pass reads and clears.
struct PerceivingBattleActor {
  std::uint8_t id = 0;
  std::uint8_t command_state = 0;           // 0x08
  std::uint8_t command_opcode = 0;          // 0x09
  std::uint8_t command_flags = 0;           // 0x0B bits 0-1
  std::uint16_t action_recovery_timer = 0;  // stats 0x954

  friend bool operator==(const PerceivingBattleActor&,
                         const PerceivingBattleActor&) = default;
};

// decomp: actorUpdateState (src/BATTLE/BATTLE.PRG/6FC00.c:885).
// The AI state fields the pass reads and writes for one listed actor.
struct PerceivingActor {
  PerceivingBattleActor battle;
  std::uint8_t stayed_on_tile = 0;              // 0x08
  std::uint8_t alert = 0;                       // 0x0E
  std::uint8_t perception_skipped = 0;          // 0x14
  std::uint8_t alerted = 0;                     // 0x18
  std::uint8_t holds_stall_tile = 0;            // 0x1C
  std::uint8_t partial_perception = 0;          // 0x1D
  std::uint8_t alert_rule = 0;                  // 0x2F
  std::uint32_t tile = 0;                       // 0x34
  std::int32_t previous_tile = 0;               // 0x38, byte 3 = held target
  std::uint32_t remembered_target_tile = 0;     // 0x3C
  std::uint16_t target_memory_frames = 0;       // 0x40
  std::uint8_t detour_step_frames = 0;          // 0x45
  std::uint16_t stall_frames = 0;               // 0x46
  Core::Math::Position3 position;               // 0x4C
  std::int16_t position_pad = 0;                // 0x52
  MovementHistory history;                      // 0x70-0x7F, 0x81
  std::uint8_t history_index = 0;               // 0x80
  std::uint16_t action_delay = 0;               // 0x90
  std::uint8_t detour_step = 0;                 // 0xFA
  std::uint8_t detour_target = 0;               // 0xFB
  std::uint16_t flee_timer = 0;                 // 0x114
  std::uint32_t fine_tile = 0;                  // 0x120
  std::uint8_t observed_command = 0;            // 0x124
  std::uint8_t awareness_frames_remaining = 0;  // 0x12A
  std::uint8_t action_status = 0;               // 0x131
  std::uint16_t awareness_marker = 0;           // 0x138
  std::uint16_t movement_wait = 0;              // 0x156
  bool has_navigation_override = false;         // 0x18C, read only
  std::uint8_t navigation_counter = 0;          // 0x190 bits 5-11
  std::uint8_t recovery_frames = 0;             // 0x19A
  // Read only here; the target update writes them through the world.
  std::array<PerceivedRelation, kPerceptionSlots> relations{};  // 0x1A4

  friend bool operator==(const PerceivingActor&,
                         const PerceivingActor&) = default;
};

// The perceiving model's state the pass gates on.
struct PerceiverModel {
  // A movement mode or locomotion state (word 8 bits 16-20).
  bool moving = false;
  bool running = false;            // record 0x5AC bit 15
  bool forces_perception = false;  // word 8 bit 31
  std::uint32_t tile = 0;          // 0x5C
};

// The AI state's linked model position (0x60): the two words copied to 0x4C.
struct LinkedPoint {
  Core::Math::Position3 position;
  std::int16_t pad = 0;
};

struct PerceivedHp {
  std::uint16_t current = 0;
  std::uint16_t max = 0;
};

// One listed actor's world: its model and links, the frame budget, the
// battle actor list, the actor records and the AI routines the pass calls.
// Each routine taking the actor stores it first and reloads it after.
class ActorPerceptionWorld {
 public:
  ActorPerceptionWorld() = default;
  ActorPerceptionWorld(const ActorPerceptionWorld&) = delete;
  ActorPerceptionWorld& operator=(const ActorPerceptionWorld&) = delete;
  ActorPerceptionWorld(ActorPerceptionWorld&&) = delete;
  ActorPerceptionWorld& operator=(ActorPerceptionWorld&&) = delete;
  virtual ~ActorPerceptionWorld() = default;

  [[nodiscard]] virtual std::optional<PerceiverModel> Model() const = 0;
  // The tile word behind the AI state's 0x64 link.
  [[nodiscard]] virtual std::optional<std::uint32_t> LinkedTile() const = 0;
  [[nodiscard]] virtual std::optional<LinkedPoint> LinkedPosition() const = 0;
  // The AI state's battle actor's HP.
  [[nodiscard]] virtual std::optional<PerceivedHp> Hp() const = 0;
  [[nodiscard]] virtual std::vector<int> ListedActorIds() const = 0;
  // The actor record's current tile word; nothing without a record. `id`
  // is below kPerceptionRecordSlots.
  [[nodiscard]] virtual std::optional<std::uint32_t> RecordTile(
      int id) const = 0;
  [[nodiscard]] virtual std::int32_t FrameTickBudget() const = 0;
  [[nodiscard]] virtual std::int32_t SecondPerceptionTarget() const = 0;
  // func_800E45B4.
  [[nodiscard]] virtual std::int32_t ElapsedFrameTicks() = 0;
  virtual void SetLastBudgetTick(std::int32_t tick) = 0;

  // func_800D82CC.
  virtual void Disengage(PerceivingActor& actor) = 0;
  // func_800D821C.
  virtual void ResetAwarenessIndicator(PerceivingActor& actor) = 0;
  // func_800DBF00.
  virtual void UnlinkEngagedActor(PerceivingActor& actor) = 0;
  // func_800D8280.
  virtual void UpdateTemporaryAwareness(PerceivingActor& actor) = 0;
  // func_800E4358.
  virtual void TickDecisionTimers(PerceivingActor& actor) = 0;
  // func_800DBFE4.
  virtual void UpdateEngagementBuildup(PerceivingActor& actor) = 0;
  // func_800D87E8; false when it refused.
  [[nodiscard]] virtual bool UpdateTargetPerception(PerceivingActor& actor,
                                                    int target) = 0;
  // func_800E48F8.
  virtual void RefreshTargetTileWhenTracking(PerceivingActor& actor) = 0;
  // func_800E2CCC.
  virtual void RecoverMovement(PerceivingActor& actor) = 0;
  // func_800E4CE8.
  virtual void ForgetActionTarget(PerceivingActor& actor) = 0;
  // func_800E48A8.
  virtual void SetReactionWithDelay(PerceivingActor& actor, int reaction,
                                    int delay) = 0;
  // func_800E4F14.
  virtual void RefreshResourceRestrictions(PerceivingActor& actor) = 0;
  // func_800D85D8; true when the actor has a ready candidate.
  [[nodiscard]] virtual bool RefreshCandidateReadiness(
      PerceivingActor& actor) = 0;
};

// decomp: func_800D8EF4 loop body (src/BATTLE/BATTLE.PRG/6FC00.c:1009).
// One listed actor's frame: command-state gates, the frame-budget check,
// movement recovery, stall and detour timers on an unchanged tile, the
// command-change reaction, position and movement history, target
// perception, candidate readiness, the alert rule and target memory decay.
// False refuses a missing model, link, HP or a listed or remembered ID
// outside its table, a history index past its eight samples, or a refused
// target update; the original read through a null pointer or past a table
// there.
[[nodiscard]] bool UpdatePerceivingActor(ActorPerceptionWorld& world,
                                         PerceivingActor& actor);
}  // namespace VS::Battle::Ai
