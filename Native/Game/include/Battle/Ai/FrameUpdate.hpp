#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

// The per-frame battle AI update: the stage order, the second perception
// target rotation, the room working-state window and the small per-actor
// timer and movement-step rules. The battle runtime owns the actor and room
// tables and reaches the stages through ActorAiFrame, so this policy touches
// no global table or PS1 address.
namespace VS::Battle::Ai {
inline constexpr std::uint32_t kFrameActorSlots = 16;
// Offset of the terrain region bitmap in the room's auxiliary AI section.
inline constexpr std::size_t kTerrainRegionOffset = 0x498;
// Bytes in the room state's collision extent word.
inline constexpr std::size_t kRoomExtentBytes = 4;

// BATTLE.PRG func_800E4358 (7B5DC.s:433): both delays count down to zero.
struct DecisionTimers {
  std::uint8_t decision_delay = 0;
  std::uint8_t min_action_delay = 0;
};
void TickDecisionTimers(DecisionTimers& timers);

// BATTLE.PRG func_800E6828 (7E028.s:8): the movement flags fold the high
// locomotion byte into the low one; bits 0x30 select mode 1, else bit 8
// selects mode 2, else mode 0.
struct LocomotionSelection {
  std::uint16_t movement_flags = 0;
  std::uint8_t mode = 0;
};
[[nodiscard]] LocomotionSelection SelectLocomotion(
    std::uint16_t locomotion_flags);

// BATTLE.PRG func_800E685C (7E05C.s:16): the walk and run steps are stored
// as the two low bytes of walk | run << 8, and the range is the larger step.
struct VerticalMovementSteps {
  std::uint8_t walk_step = 0;
  std::uint8_t run_step = 0;
  std::uint8_t max_range = 0;
};
[[nodiscard]] VerticalMovementSteps MakeVerticalMovementSteps(int walk_step,
                                                              int run_step);

// The second perception target advances to the next actor slot in
// `attribute_mask` after `previous`, wrapping at kFrameActorSlots; it is 0
// when no slot is marked.
[[nodiscard]] std::uint32_t NextPerceptionTarget(std::uint32_t attribute_mask,
                                                 std::uint32_t previous);

// BATTLE.PRG func_800E65DC (7DDDC.s:8): the terrain region bitmap follows
// the auxiliary section's fixed header. Returns the bitmap's offset and byte
// count, or nothing when the section is too short to hold the header.
struct TerrainRegionWindow {
  std::size_t offset = 0;
  std::size_t count = 0;
};
[[nodiscard]] std::optional<TerrainRegionWindow> LocateTerrainRegions(
    std::size_t auxiliary_bytes);

// The room state's collision extent word (width in the low byte, height in
// the next), read little-endian as the original's lw.
[[nodiscard]] std::uint32_t PackRoomBounds(
    const std::array<std::uint8_t, kRoomExtentBytes>& extent);

// The battle runtime's per-frame AI stages.
class ActorAiFrame {
 public:
  virtual ~ActorAiFrame() = default;
  [[nodiscard]] virtual bool HasTileFlags() const = 0;
  [[nodiscard]] virtual bool HasRoomAndActors() const = 0;
  virtual void CacheWorkingState() = 0;
  [[nodiscard]] virtual std::uint32_t AttributeActorMask() const = 0;
  [[nodiscard]] virtual std::uint32_t PerceptionTarget() const = 0;
  virtual void SetPerceptionTarget(std::uint32_t target) = 0;
  virtual void UpdatePerception() = 0;
  virtual void UpdateDecisions() = 0;
  virtual void AnimateEngagement() = 0;
  virtual void SaveRandomState() = 0;
};

enum class FrameUpdateResult : std::uint8_t {
  kNoRoomMap,
  kMissingRoomOrActors,
  kUpdated,
};

// BATTLE.PRG func_800E511C, stages at 0x800F16BC (7C7E0.s:108): with no
// room tile map the frame is skipped; without a room AI state or the linked
// actor list it is refused before any stage runs. Otherwise the working
// state is cached, the second perception target advances, and perception,
// decisions, the engagement animation and the random-state save run in that
// order.
FrameUpdateResult RunActorAiUpdateStages(ActorAiFrame& frame);
}  // namespace VS::Battle::Ai
