// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/Relations.hpp"

#include <array>
#include <cstdint>

namespace VS::Battle::Ai {
namespace {
constexpr unsigned kSelfClassification = 2;
constexpr std::uint8_t kNoncombatCategory = 3;
// decomp: BATTLE.PRG categories at 0x80069C0C (7E05C.s:857), indexed by the
// source category and whether the target is a combatant other than Ashley.
using CategoryRow = std::array<std::uint8_t, 2>;
using CategoryTable = std::array<CategoryRow, kRelationCategoryCount>;
constexpr CategoryTable kClassByCategory{{{3, 2}, {1, 1}, {1, 2}, {2, 3}}};

constexpr int kClearAllianceMarker = 0x40;
constexpr int kAllianceMarker = 0x80;
constexpr int kRecoveryMarker = 6;
constexpr int kRecoveryFrames = 0x28;
constexpr int kRecoveryCommand = 2;
constexpr int kRecoveryRange = 0x80;
constexpr int kRollMask = 0x7F;
constexpr int kFavorableThreshold = 0x4C;
constexpr unsigned kRequiredReactions = 2;

constexpr unsigned kClassificationMask = 3;
constexpr unsigned kPreviousClassShift = 3;
constexpr unsigned kTimerShift = 6;
constexpr unsigned kTimerMask = 0x7F;

constexpr std::uint32_t Bit(int actor) { return std::uint32_t{1} << actor; }
}  // namespace

// decomp: BATTLE.PRG func_800E7370 (src/BATTLE/BATTLE.PRG/7E05C.s:857).
bool InitializeActorAiRelations(ActorRelationTable& actors, int actor) {
  std::uint32_t pending = Bit(actor);
  std::uint32_t visited = 0;
  while (pending != 0) {
    int source = 0;
    while ((pending & Bit(source)) == 0) ++source;
    pending &= ~Bit(source);
    visited |= Bit(source);
    const std::uint8_t category = actors.Category(source);
    if (category >= kRelationCategoryCount) return false;
    for (const std::uint8_t target : actors.RoomActors()) {
      const bool combatant =
          target != 0 && actors.Category(target) < kNoncombatCategory;
      actors.SetClassification(
          source, target,
          source == target
              ? kSelfClassification
              : kClassByCategory.at(category).at(combatant ? 1 : 0));
      if (actors.Classification(target, source) == 0 &&
          (visited & Bit(target)) == 0) {
        pending |= Bit(target);
      }
    }
  }
  return true;
}

// decomp: BATTLE.PRG func_800DC19C (src/BATTLE/BATTLE.PRG/7399C.s:11).
void EndAshleyAlliance(AshleyAllianceActor& actor) {
  actor.SetReactionCount(0);
  if (!actor.AllianceActive()) return;
  actor.SetAllianceActive(false);
  actor.SetAwarenessIndicator(kClearAllianceMarker);
  actor.RecomputeRelations();
  actor.RestoreSavedDecision();
  if (actor.RecoveryDisabled()) return;
  actor.SetTemporaryAwareness(kRecoveryMarker, kRecoveryFrames);
  actor.QueueReaction(kRecoveryCommand, kRecoveryRange);
}

// decomp: BATTLE.PRG func_800DC210 (src/BATTLE/BATTLE.PRG/7399C.s:43).
void TryBeginAshleyAlliance(AshleyAllianceActor& actor, bool forced) {
  if (!forced) {
    if ((actor.Roll() & kRollMask) >= kFavorableThreshold) return;
    // 7399C.s stores the byte but tests the untruncated register value.
    const unsigned reactions = unsigned{actor.ReactionCount()} + 1;
    actor.SetReactionCount(static_cast<std::uint8_t>(reactions));
    if (reactions < kRequiredReactions) return;
  }
  actor.SetAllianceActive(true);
  actor.SaveDecisionAndApproach();
  actor.SetAwarenessIndicator(kAllianceMarker);
}

SavedAshleyRelation DecodeSavedAshleyRelation(std::uint16_t options) {
  return {
      .previous_classification = static_cast<std::uint8_t>(
          (options >> kPreviousClassShift) & kClassificationMask),
      .classification =
          static_cast<std::uint8_t>((options >> 1) & kClassificationMask),
      .classification_timer =
          static_cast<std::uint8_t>((options >> kTimerShift) & kTimerMask),
      .allied = (options & 1) != 0,
  };
}

// decomp: BATTLE.PRG func_800E72D0 (src/BATTLE/BATTLE.PRG/7E05C.s:810).
void ApplySavedActorAiRelations(SavedRelationActors& actors) {
  for (const std::uint8_t actor : actors.RoomActors()) {
    const std::int16_t options = actors.SavedOptions(actor);
    if (options < 0) continue;
    const SavedAshleyRelation relation =
        DecodeSavedAshleyRelation(static_cast<std::uint16_t>(options));
    actors.RestoreAshleyRelation(actor, relation);
    if (relation.allied) actors.BeginForcedAlliance(actor);
  }
}
}  // namespace VS::Battle::Ai
