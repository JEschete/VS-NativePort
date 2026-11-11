// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/Relations.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>

#include "Battle/Ai/ActorCleanup.hpp"
#include "Core/Diagnostics.h"

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

// Relation word fields (Battle_Ai_RelationWord).
constexpr std::uint32_t kRelationActiveBit = 1;
constexpr unsigned kRelationClassShift = 6;
constexpr unsigned kRelationTimerShift = 15;
constexpr std::uint32_t kRelationTimerMask = 0x1F;
constexpr unsigned kRelationCounterShift = 20;
constexpr std::uint32_t kRelationCounterMask = 0x7F;
constexpr unsigned kRelationPreviousShift = 28;

constexpr int kAshley = 0;
constexpr unsigned kNeutralClassification = 1;
constexpr unsigned kHostileClassification = 3;
constexpr unsigned kReactionTimerFrames = 28;
constexpr unsigned kGrievanceCounter = 0x7F;
constexpr std::uint8_t kNoReactionPrerequisite = 3;
constexpr std::uint8_t kHoldingBehaviorMode = 1;
constexpr std::uint8_t kReleasedBehaviorMode = 2;
constexpr unsigned kHpRestoreEffect = 2;

bool Refuse(const char* message) {
  Core_Debugf("ai", "%s", message);
  return false;
}

bool IsRelationSlot(int actor) {
  return actor >= 0 && actor < kRelationActorSlots;
}

unsigned Field(std::uint32_t word, unsigned shift, std::uint32_t mask) {
  return (word >> shift) & mask;
}

std::uint32_t WithField(std::uint32_t word, unsigned shift, std::uint32_t mask,
                        unsigned value) {
  return (word & ~(mask << shift)) | ((value & mask) << shift);
}

unsigned ClassOf(std::uint32_t word) {
  return Field(word, kRelationClassShift, kClassificationMask);
}

unsigned CounterOf(std::uint32_t word) {
  return Field(word, kRelationCounterShift, kRelationCounterMask);
}

unsigned PreviousOf(std::uint32_t word) {
  return Field(word, kRelationPreviousShift, kClassificationMask);
}

// The reaction to harm from `source` (74088.c:755-776).
bool ReactToHarm(ActorReactionHost& host, int source, const ReactionHit& hit) {
  const int own = host.ActorId();
  if (source == kAshley && hit.hp_effect != kHpRestoreEffect &&
      hit.hp_change > 0) {
    host.StopArenaOrbit();
  }
  if (source == kAshley && host.Settings().settings_bit27) host.EndAlliance();
  const ReactionSettings settings = host.Settings();
  if (settings.settings_bit25 &&
      (!settings.settings_bit26 || source == kAshley)) {
    host.MakeHostile(source);
    return true;
  }
  const std::uint32_t relation = host.Relation(source);
  if (ClassOf(relation) == kHostileClassification) return true;
  const std::optional<std::uint32_t> held = host.PeerRelation(source, own);
  if (!held.has_value()) {
    return Refuse("AI action reaction requires the acting actor's state");
  }
  const std::uint32_t held_word = *held;
  if (CounterOf(held_word) != 0) {
    // Harm in return settles the acting actor's grievance.
    const std::uint32_t settled = WithField(
        WithField(held_word, kRelationCounterShift, kRelationCounterMask, 0),
        kRelationClassShift, kClassificationMask, PreviousOf(held_word));
    host.SetPeerRelation(source, own, settled);
    return true;
  }
  host.SetRelation(source,
                   WithField(WithField(relation, kRelationCounterShift,
                                       kRelationCounterMask, kGrievanceCounter),
                             kRelationPreviousShift, kClassificationMask,
                             ClassOf(relation)));
  if (host.Settings().settings_bit26) host.MakeHostile(source);
  return true;
}

// The reaction to one hit on this actor by `source` (74088.c:741-776).
bool ReactToHit(ActorReactionHost& host, int action, int source,
                const ReactionHit& hit) {
  if (host.BehaviorMode() == kHoldingBehaviorMode) {
    host.SetBehaviorMode(kReleasedBehaviorMode);
  }
  if (host.IsBeneficial(action)) {
    const ReactionSettings settings = host.Settings();
    if (source == kAshley && !settings.allied_with_ashley &&
        settings.settings_bit27) {
      host.TryBeginAlliance();
    } else {
      const std::optional<std::uint32_t> toward_ashley =
          host.PeerRelation(source, kAshley);
      if (!toward_ashley.has_value()) {
        return Refuse("AI action reaction requires the acting actor's state");
      }
      if (ClassOf(*toward_ashley) == kHostileClassification) host.EndAlliance();
    }
  }
  host.SetRelation(source, WithField(host.Relation(source), kRelationTimerShift,
                                     kRelationTimerMask, kReactionTimerFrames));
  if (!host.IsHarmful(action)) return true;
  return ReactToHarm(host, source, hit);
}

bool HasOnlyRelationSlots(std::span<const std::uint8_t> actors) {
  return std::ranges::all_of(
      actors, [](std::uint8_t actor) { return IsRelationSlot(actor); });
}

// The alliance reclassification (74088.c:793-811).
bool ClassifyForAshleyAlliance(ActorReactionHost& host,
                               std::span<const std::uint8_t> room_actors) {
  const int own = host.ActorId();
  for (const std::uint8_t id : room_actors) {
    unsigned classification = kNeutralClassification;
    if (id == own || id == kAshley) {
      classification = kAllyClassification;
    } else {
      const std::optional<ReactionSettings> peer = host.PeerSettings(id);
      if (!peer.has_value()) {
        return Refuse("AI alliance reclassification requires every state");
      }
      const ReactionSettings peer_settings = *peer;
      if (peer_settings.category == kNoncombatCategory) {
        classification = kAllyClassification;
      } else if (!peer_settings.allied_with_ashley) {
        const std::optional<std::uint32_t> toward_ashley =
            host.PeerRelation(id, kAshley);
        if (!toward_ashley.has_value()) {
          return Refuse("AI alliance reclassification requires every state");
        }
        if (ClassOf(*toward_ashley) == kHostileClassification) {
          classification = kHostileClassification;
          host.SetRelation(id,
                           WithField(host.Relation(id), kRelationCounterShift,
                                     kRelationCounterMask, 0));
        }
      }
    }
    host.SetRelation(id, WithField(host.Relation(id), kRelationClassShift,
                                   kClassificationMask, classification));
  }
  return true;
}

// Whether a mutual ally counts `candidate` as an ally or holds a grievance
// against it over a former alliance (74088.c:822-831); nothing refuses a
// missing ally state.
std::optional<bool> IsDefendedByAlly(const ActorReactionHost& host,
                                     std::span<const std::uint8_t> room_actors,
                                     int candidate) {
  const int own = host.ActorId();
  for (const std::uint8_t ally : room_actors) {
    if (ClassOf(host.Relation(ally)) != kAllyClassification) continue;
    const std::optional<std::uint32_t> toward_own =
        host.PeerRelation(ally, own);
    const std::optional<std::uint32_t> toward_candidate =
        host.PeerRelation(ally, candidate);
    if (!toward_own.has_value() || !toward_candidate.has_value()) {
      Refuse("AI hostility sharing requires every ally's state");
      return std::nullopt;
    }
    if (ClassOf(*toward_own) != kAllyClassification) continue;
    const std::uint32_t word = *toward_candidate;
    if (ClassOf(word) == kAllyClassification ||
        (CounterOf(word) != 0 && PreviousOf(word) == kAllyClassification)) {
      return true;
    }
  }
  return false;
}

// Takes up each mutual ally's hostility (74088.c:812-836).
bool ShareAllyHostility(ActorReactionHost& host,
                        std::span<const std::uint8_t> room_actors) {
  const int own = host.ActorId();
  for (const std::uint8_t mutual : room_actors) {
    if (ClassOf(host.Relation(mutual)) != kAllyClassification) continue;
    const std::optional<std::uint32_t> toward_own =
        host.PeerRelation(mutual, own);
    if (!toward_own.has_value()) {
      return Refuse("AI hostility sharing requires every ally's state");
    }
    if (ClassOf(*toward_own) != kAllyClassification) continue;
    for (const std::uint8_t candidate : room_actors) {
      const std::uint32_t relation = host.Relation(candidate);
      if (ClassOf(relation) != kNeutralClassification ||
          (relation & kRelationActiveBit) == 0) {
        continue;
      }
      const std::optional<std::uint32_t> mutual_view =
          host.PeerRelation(mutual, candidate);
      if (!mutual_view.has_value()) {
        return Refuse("AI hostility sharing requires every ally's state");
      }
      if (ClassOf(*mutual_view) != kHostileClassification) continue;
      const std::optional<bool> defended =
          IsDefendedByAlly(host, room_actors, candidate);
      if (!defended.has_value()) return false;
      if (!*defended) host.MakeHostile(candidate);
    }
  }
  return true;
}
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

// decomp: BATTLE.PRG func_800E5998 (src/BATTLE/BATTLE.PRG/7CD68.c:573).
bool ResolveAiLinkedActors(LinkedActorTable& actors) {
  const std::span<const LinkGroupActor> listed = actors.FollowingActors();
  // The original reads a missing state through a null table entry (#337).
  const bool all_have_states =
      std::ranges::all_of(listed, [&actors](const LinkGroupActor& actor) {
        return actors.LinkedActor(actor.id).has_value();
      });
  if (!all_have_states) return false;
  for (const LinkGroupActor& actor : listed) {
    // Reread per entry, as the original does: a repeated ID sees an earlier
    // entry's link.
    if (actors.LinkedActor(actor.id) != kNoLinkedActor) continue;
    const auto leader =
        std::ranges::find(listed, actor.linked_group, &LinkGroupActor::group);
    if (leader != listed.end()) actors.SetLinkedActor(actor.id, leader->id);
  }
  return true;
}

// decomp: BATTLE.PRG func_800DD000 (src/BATTLE/BATTLE.PRG/74088.c:716).
bool ApplyActorActionReaction(ActorReactionHost& host,
                              const AppliedAction& applied) {
  if (applied.acting.skipped) return true;
  const int own = host.ActorId();
  if (own == kAshley) return true;
  if (!applied.hit_prerequisites.has_value()) {
    return Refuse("AI action reaction action is outside the action table");
  }
  const std::array<std::uint8_t, 2> prerequisites = *applied.hit_prerequisites;
  if (std::ranges::find(prerequisites, kNoReactionPrerequisite) !=
      prerequisites.end()) {
    return true;
  }
  const int source = applied.acting.actor;
  for (const ReactionHit& hit : applied.targets) {
    if (hit.skipped || hit.actor != own || source == own) continue;
    if (!IsRelationSlot(source) || !IsRelationSlot(own)) {
      return Refuse("AI action reaction actor is outside the AI slots");
    }
    if (!ReactToHit(host, applied.action, source, hit)) return false;
  }
  return true;
}

// decomp: BATTLE.PRG func_800DD344 (src/BATTLE/BATTLE.PRG/74088.c:782).
bool PropagateActorRelationships(ActorReactionHost& host,
                                 std::span<const std::uint8_t> room_actors) {
  const ReactionSettings settings = host.Settings();
  if (!settings.allied_with_ashley && !settings.shares_ally_hostility) {
    return true;
  }
  if (!HasOnlyRelationSlots(room_actors)) {
    return Refuse("AI relationship propagation actor is outside the AI slots");
  }
  if (settings.allied_with_ashley) {
    return ClassifyForAshleyAlliance(host, room_actors);
  }
  if (!IsRelationSlot(host.ActorId())) {
    return Refuse("AI relationship propagation actor is outside the AI slots");
  }
  return ShareAllyHostility(host, room_actors);
}

// decomp: BATTLE.PRG func_800DEEA4 (src/BATTLE/BATTLE.PRG/74088.c:1596).
bool NotifyActionRelationshipChanges(ActionNotificationHost& host,
                                     const ReactionHit& acting) {
  if (!acting.skipped) {
    if (!IsRelationSlot(acting.actor)) {
      return Refuse("AI action notification actor is outside the AI slots");
    }
    host.ClearReengagementGrace(acting.actor);
  }
  host.ApplyRelationshipChanges();
  return true;
}
}  // namespace VS::Battle::Ai
