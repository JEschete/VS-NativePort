#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Battle/ActionResults.hpp"
#include "Battle/StatusQueue.hpp"

namespace VS::Battle {
enum class SourceActionOutcome : std::uint8_t {
  kUnclassified0 = 0,
  kUnclassified1 = 1,
  kSuccess = 2,
  kUnclassified3 = 3,
};

class InstantDeathQueue {
 public:
  virtual ~InstantDeathQueue() = default;
  virtual void RemoveStatuses(StatusRuleId rule) = 0;
  [[nodiscard]] virtual std::uint16_t CurrentHp() = 0;
  virtual void QueueHp(ActionResultChange change) = 0;
};

enum class ActionScriptFlag : int { kFlag64 = 64, kFlag32 = 32 };
class ActionScriptFlags {
 public:
  virtual ~ActionScriptFlags() = default;
  virtual void Set(ActionScriptFlag flag, std::uint8_t value) = 0;
};

// The live queue owns a cached target identity. Reads follow the random
// service, which may change the pending amounts or their mode bits.
class RandomStatQueue {
 public:
  virtual ~RandomStatQueue() = default;
  [[nodiscard]] virtual std::int32_t DrawBounded(std::int32_t bound) = 0;
  [[nodiscard]] virtual ActionResultChange ReadChange(
      ActionResultField field) = 0;
  virtual void WriteChange(ActionResultField field,
                           ActionResultChange change) = 0;
};

// decomp: _increaseStrengthRand (src/BATTLE/BATTLE.PRG/146C.c:12717).
void QueueRandomStrengthIncrease(RandomStatQueue& queue);
// decomp: _increaseIntelligenceRand (src/BATTLE/BATTLE.PRG/146C.c:12726).
void QueueRandomIntelligenceIncrease(RandomStatQueue& queue);
// decomp: _increaseAgilityRand (src/BATTLE/BATTLE.PRG/146C.c:12735).
void QueueRandomAgilityIncrease(RandomStatQueue& queue);
// decomp: _increaseMaxHpRand (src/BATTLE/BATTLE.PRG/146C.c:12744).
void QueueRandomMaxHpIncrease(RandomStatQueue& queue);
// decomp: _increaseMaxMpRand (src/BATTLE/BATTLE.PRG/146C.c:12753).
void QueueRandomMaxMpIncrease(RandomStatQueue& queue);
// Decreases also accumulate a positive amount; only their mode differs.
// decomp: _decreaseStrengthRand (src/BATTLE/BATTLE.PRG/146C.c:12762).
void QueueRandomStrengthDecrease(RandomStatQueue& queue);
// decomp: _decreaseIntelligenceRand (src/BATTLE/BATTLE.PRG/146C.c:12771).
void QueueRandomIntelligenceDecrease(RandomStatQueue& queue);
// decomp: _decreaseAglityRand (src/BATTLE/BATTLE.PRG/146C.c:12780).
void QueueRandomAgilityDecrease(RandomStatQueue& queue);

// These three originals are byte-identical aliases of one policy.
// decomp: func_80084228 (src/BATTLE/BATTLE.PRG/146C.c:12547).
// decomp: func_800842F0 (src/BATTLE/BATTLE.PRG/146C.c:12572).
// decomp: func_80084370 (src/BATTLE/BATTLE.PRG/146C.c:12599).
void MarkSourceActionSuccess(SourceActionOutcome& outcome);
// Current HP is read after the status-removal service, not before it.
// decomp: func_80084248 (src/BATTLE/BATTLE.PRG/146C.c:12554).
void QueueInstantDeath(InstantDeathQueue& queue);
// Refuses an invalid signed limb without changing the queued result.
// decomp: func_800842AC (src/BATTLE/BATTLE.PRG/146C.c:12563).
bool QueueDamageBasedArmorWear(ActorActionResult& target);
// decomp: func_80084310 (src/BATTLE/BATTLE.PRG/146C.c:12579).
void SetActionFlag64(bool execute, ActionScriptFlags& flags);
// decomp: func_80084340 (src/BATTLE/BATTLE.PRG/146C.c:12589).
void SetActionFlag32(bool execute, ActionScriptFlags& flags);
}  // namespace VS::Battle
