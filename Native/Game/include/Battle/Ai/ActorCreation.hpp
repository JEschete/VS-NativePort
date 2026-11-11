#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Battle/Ai/ActorCleanup.hpp"
#include "Battle/Ai/ActorCommands.hpp"

// Creating a battle actor's AI state: registering the actor's attribute bit
// and room special actor slot, allocating its AI state and deriving the
// state's size, locomotion, reach and placement from its model and stats,
// then applying its settings, relations and start command. The battle runtime
// owns the AI state table, the room AI state, the tile flag grid and the
// actor records; this policy reaches them only through ActorCreationHost, so
// it touches no AI state record, PS1 address or heap.
namespace VS::Battle::Ai {
// Actor word 0x40 of an actor allocated as an auxiliary core; only those
// actors can take a room special actor slot.
inline constexpr std::int32_t kAuxiliaryAllocationKind = 0;
// Actor kinds that take an AI state.
inline constexpr std::uint16_t kAiStateActorKinds = 0x05;

// The battle actor whose AI state is created.
struct CreationActor {
  BattleActorHandle handle{};
  int id = 0;              // actor 0x04
  std::uint16_t kind = 0;  // actor 0x1C
  // Actor byte 0x1F, the spawn's combat pose; a pose starts command 1.
  std::uint8_t combat_pose = 0;
  // The low halfword of actor word 0x34: bit 0 enables the saved settings
  // flags in bits 1-11.
  std::uint16_t saved_settings_flags = 0;
  std::uint32_t settings = 0;        // actor 0x38, the template settings
  std::int32_t allocation_kind = 0;  // actor 0x40
};

// The actor record fields creation reads through the actor's model (0x44).
struct CreationModel {
  bool motion_locked = false;   // model word 0x08, bit 21
  std::int16_t position_y = 0;  // 0x1E
  // The current tile word (0x5C): X in byte 0 and Z in byte 2.
  std::uint32_t tile = 0;
  std::uint16_t radius = 0;       // 0x63C, the collision radius
  std::uint16_t body_height = 0;  // 0x63E, the collision height
};

// The actor stats bytes 0x30-0x33.
struct CreationStats {
  std::uint8_t movement_class = 0;
  std::uint8_t walk_speed = 0;
  std::uint8_t gait_class = 0;
  std::uint8_t run_speed = 0;
};

// The AI state fields creation writes or reads, by PS1 AI state offset. The
// rest of the new state starts zeroed.
struct CreatedAiState {
  // A bone carries auxiliary tag 2 (0x2A).
  bool has_auxiliary_tag_bone = false;
  std::uint32_t tile = 0;                    // 0x34
  std::int32_t previous_tile = 0;            // 0x38
  std::int16_t saved_relation_options = 0;   // 0x42
  std::int16_t point_height = 0;             // 0x82
  std::uint8_t actor_id = 0;                 // 0x88
  std::uint8_t requested_movement_mode = 0;  // 0x89
  std::uint8_t current_movement_mode = 0;    // 0x8A
  std::uint16_t radius = 0;                  // 0x92
  std::uint16_t collision_radius = 0;        // 0x94
  std::uint32_t last_slide_tile = 0;         // 0x9C
  // The far perception range (0xAC), set by the settings decode.
  std::uint32_t far_distance_squared = 0;
  std::uint8_t max_range = 0;         // 0xB3
  std::int32_t move_distance = 0;     // 0xB4
  std::int16_t body_height = 0;       // 0xB8
  std::int16_t ground_clearance = 0;  // 0xBC
  std::int16_t flight_height = 0;     // 0xBE
  std::int8_t patrol_direction = 0;   // 0x103
  std::uint32_t flee_tile = 0;        // 0x110
  // The battle actor's command opcode as perception last saw it (0x124).
  std::uint8_t observed_command_opcode = 0;
  std::uint8_t locomotion_mode = 0;  // 0x125
  std::uint8_t behavior_mode = 0;    // 0x12D
  std::uint16_t step_height = 0;     // 0x13C
  std::uint16_t movement_flags = 0;  // 0x15E
  std::uint32_t next_tile = 0;       // 0x170
  std::int32_t move_y = 0;           // 0x184
  std::uint32_t detour_tile = 0;     // 0x194
  // Stats bytes 0x30 (low) and 0x32 (high).
  std::uint16_t locomotion_flags = 0;  // 0x224
  std::uint8_t walk_step = 0;          // 0x226
  std::uint8_t run_step = 0;           // 0x227
};

using RoomSpecialActors = std::array<BattleActorHandle, kRoomSpecialActorSlots>;

// The AI state table, room AI state, tile flag grid, battle actors and
// skeletons creation uses. The steps that take a CreatedAiState see the
// state as stored before they run, and their changes are read back
// afterwards.
class ActorCreationHost {
 public:
  virtual ~ActorCreationHost() = default;

  [[nodiscard]] virtual std::uint32_t AttributeActorMask() const = 0;
  virtual void StoreAttributeActorMask(std::uint32_t mask) = 0;
  // Nothing when the room has no AI state.
  [[nodiscard]] virtual std::optional<RoomSpecialActors> SpecialActors()
      const = 0;
  virtual void StoreSpecialActor(std::size_t slot, BattleActorHandle actor) = 0;
  // The tile flag grid, kTileFlagGridSide halfwords per row; empty without
  // one.
  [[nodiscard]] virtual std::span<const std::uint16_t> TileFlags() const = 0;
  // Model word 0x16C, which follows an auxiliary actor core's eight
  // matrices; nothing without a model.
  [[nodiscard]] virtual std::optional<std::uint32_t> ModelObjectWord(
      BattleActorHandle actor) const = 0;
  // Nothing without the model's actor record (an auxiliary actor's core
  // holds none).
  [[nodiscard]] virtual std::optional<CreationModel> Model(
      BattleActorHandle actor) const = 0;
  // Nothing without stats.
  [[nodiscard]] virtual std::optional<CreationStats> Stats(
      BattleActorHandle actor) const = 0;
  // Allocates a zeroed AI state into slot `actor_id`, linked to the actor,
  // its model and stats and the model's position and tile; false when the
  // heap is exhausted.
  [[nodiscard]] virtual bool AllocateAiState(int actor_id,
                                             BattleActorHandle actor) = 0;
  // The Y of the actor's position raised by its anchor height (func_800A190C
  // selector 0xFA); nothing without an actor record.
  [[nodiscard]] virtual std::optional<std::int16_t> AnchorHeightY(
      int actor_id) const = 0;
  // Whether one of the actor's bones carries auxiliary tag `tag`
  // (func_800A152C kind 3).
  [[nodiscard]] virtual bool HasAuxiliaryBoneTag(int actor_id,
                                                 int tag) const = 0;
  // Clears the battle actor's command opcode and the low four bits of its
  // command flags.
  virtual void ClearActorCommand(BattleActorHandle actor) = 0;

  // Decodes the packed settings into the state (func_800E5A9C).
  virtual void ApplySettings(CreatedAiState& state, std::uint32_t settings) = 0;
  // func_800E7370.
  virtual void InitializeRelations(CreatedAiState& state) = 0;
  // func_800DEEFC.
  virtual void IssueCommand(CreatedAiState& state, CommandIntent intent) = 0;
  // Applies every actor's saved relation options (func_800E72D0).
  virtual void ApplySavedRelations(CreatedAiState& state) = 0;
  // One action plan rebuild pass; true while another pass is needed
  // (func_800DEC88).
  [[nodiscard]] virtual bool RebuildActionPlans(CreatedAiState& state) = 0;
};

// decomp: func_800E6178 (src/BATTLE/BATTLE.PRG/7CD68.c:934).
// Marks the actor's attribute bit (kinds 0x15), gives an auxiliary actor
// whose model object ID is 2 or 4 the room's first free special actor slot,
// and, for AI kinds (0x05), creates its AI state. `saved_flags` are the
// actor's remembered AI flags (PackActorAiPersistenceFlags) or negative for
// a fresh state. An actor other than Ashley whose movement flags hold 0x30
// and 0xC6 bits, whose current tile is flagged 0x10 and whose model is not
// motion locked hovers over that tile; otherwise a fresh actor, or one not
// remembered as idle, with a combat pose stops.
// Returns whether the actor was remembered as idle (false without an AI
// state), or nothing when it refuses an AI actor outside the AI slots, a
// missing room AI state, model, stats or skeleton, a missing tile flag grid
// or a tile outside it, or an exhausted heap.
[[nodiscard]] std::optional<bool> CreateActorAiState(ActorCreationHost& host,
                                                     const CreationActor& actor,
                                                     std::int16_t saved_flags);
}  // namespace VS::Battle::Ai
