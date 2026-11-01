#pragma once

#include <cstdint>

// The pause an AI actor takes before acting again after being targeted. The
// battle runtime owns the actor AI table; the reset policy only sees the slots
// through ActorReactionDelays, so it reaches no global table or PS1 address.
namespace VS::Battle::Ai {
inline constexpr int kAiActorSlotCount = 16;
inline constexpr std::uint8_t kReactionDelayFrames = 30;

// The actor AI slots a reaction delay is written to.
class ActorReactionDelays {
 public:
  virtual ~ActorReactionDelays() = default;
  // Sets the minimum action delay of an actor slot (0 to kAiActorSlotCount -
  // 1); returns false when the slot has no AI state.
  virtual bool SetMinimumActionDelay(int actor_id, std::uint8_t frames) = 0;
};

// The battle runtime's actor AI table.
[[nodiscard]] ActorReactionDelays& BattleActorReactionDelays();

// Gives an AI actor a fresh 30-frame reaction delay. Ashley (slot 0) and empty
// slots are left alone; an ID outside the table aborts (func_800E4C64).
void ResetActorAiReactionDelay(ActorReactionDelays& actors, int actor_id);
}  // namespace VS::Battle::Ai
