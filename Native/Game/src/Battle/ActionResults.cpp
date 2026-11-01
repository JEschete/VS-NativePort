// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionResults.hpp"

#include <cstddef>
#include <cstdint>

namespace VS::Battle {
namespace {
constexpr unsigned kWorldLabelIdMask = 0x1F0;
constexpr unsigned kWorldLabelIdTag = 0x80;

enum class ImpactKind : std::uint8_t {
  kHit = 0,
  kAlternateHit = 1,
  kBlock = 3,
  kAlternateBlock = 4,
  kRecovery = 5,
};

enum class Reaction : std::uint8_t { kHit, kBlock, kRecover, kNone };

Reaction NextReaction(Reaction current, std::uint8_t impact_kind,
                      ActionTargetEffects& effects) {
  switch (static_cast<ImpactKind>(impact_kind)) {
    case ImpactKind::kHit:
    case ImpactKind::kAlternateHit:
      return Reaction::kHit;
    case ImpactKind::kRecovery:
      return Reaction::kRecover;
    case ImpactKind::kBlock:
    case ImpactKind::kAlternateBlock:
      return current == Reaction::kNone && !effects.IsAreaAction()
                 ? Reaction::kBlock
                 : current;
    default:
      return current;
  }
}
}  // namespace

// decomp: BATTLE.PRG func_8006CB04 (146C/func_8006CB04.s); compared with
// rood-boi's PC 146C.c callback.
void ApplyActionTargetEffects(int actor_id, ActionTargetEffects& effects) {
  if (actor_id >= kActionActorSlots) {
    if ((static_cast<unsigned>(actor_id) & kWorldLabelIdMask) ==
        kWorldLabelIdTag) {
      effects.QueueWorldLabel();
    }
    return;
  }
  Reaction reaction = Reaction::kNone;
  for (std::size_t index = 0; index < effects.HitCount(); ++index) {
    const ActionTargetHit hit = effects.Hit(index);
    if (hit.targets_position || hit.target_actor != actor_id) continue;
    reaction = NextReaction(reaction, hit.impact_kind, effects);
    effects.ApplyHit(index);
  }
  switch (reaction) {
    case Reaction::kHit:
      effects.PlayDamageReaction(actor_id);
      break;
    case Reaction::kBlock:
      // Gimmick actors have no actor record. func_8009DDF4 has no NULL check:
      // on PS1 it reads low RAM, on x64 it faults (#251).
      if (effects.HasActorRecord(actor_id)) effects.PlayDefense(actor_id);
      break;
    case Reaction::kRecover:
      if (actor_id == 0) effects.PlayRecoveryPose();
      break;
    case Reaction::kNone:
      break;
  }
}
}  // namespace VS::Battle
