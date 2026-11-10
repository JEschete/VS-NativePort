#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>
#include <span>

// How battle AI actors classify each other, and an actor's temporary alliance
// with Ashley. The battle runtime owns the actor AI table; these policies only
// see actors through the hosts below, so they reach no global table, PS1
// address or callback pointer.
namespace VS::Battle::Ai {
inline constexpr int kRelationActorSlots = 16;
inline constexpr int kRelationCategoryCount = 4;

// The actor AI relation words the classification walk reads and writes.
class ActorRelationTable {
 public:
  virtual ~ActorRelationTable() = default;
  // Actor IDs (0 to kRelationActorSlots - 1) present in the room.
  [[nodiscard]] virtual std::span<const std::uint8_t> RoomActors() const = 0;
  // The actor's relation category (settings bits 21-22).
  [[nodiscard]] virtual std::uint8_t Category(int actor) const = 0;
  [[nodiscard]] virtual unsigned Classification(int source,
                                                int target) const = 0;
  virtual void SetClassification(int source, int target, unsigned value) = 0;
};

// Classifies every room actor from `actor`'s point of view and spreads the
// walk to each actor that has not classified the source yet; an actor
// classifies itself as 2. Returns false, leaving the walk part-way, when a
// source actor has no valid category (func_800E7370).
[[nodiscard]] bool InitializeActorAiRelations(ActorRelationTable& actors,
                                              int actor);

// One actor's temporary alliance with Ashley.
class AshleyAllianceActor {
 public:
  virtual ~AshleyAllianceActor() = default;
  [[nodiscard]] virtual std::uint8_t ReactionCount() const = 0;
  virtual void SetReactionCount(std::uint8_t count) = 0;
  [[nodiscard]] virtual bool AllianceActive() const = 0;
  virtual void SetAllianceActive(bool active) = 0;
  virtual void SetAwarenessIndicator(int marker) = 0;
  // Reclassifies the room from this actor's point of view.
  virtual void RecomputeRelations() = 0;
  // Saves the decision behaviour and switches to the engagement approach.
  virtual void SaveDecisionAndApproach() = 0;
  // Restores the saved decision behaviour, if any, and clears the save.
  virtual void RestoreSavedDecision() = 0;
  [[nodiscard]] virtual bool RecoveryDisabled() const = 0;
  virtual void SetTemporaryAwareness(int marker, int frames) = 0;
  virtual void QueueReaction(int command, int range) = 0;
  // A draw from the BIOS random generator.
  [[nodiscard]] virtual int Roll() = 0;
};

// Ends the alliance: clears the reaction count and, when allied, clears the
// marker, reclassifies the room, restores the saved behaviour and (unless
// recovery is disabled) queues the recovery reaction (func_800DC19C).
void EndAshleyAlliance(AshleyAllianceActor& actor);

// Counts a favourable reaction and allies the actor with Ashley on the second
// one; `forced` allies at once without drawing (func_800DC210).
void TryBeginAshleyAlliance(AshleyAllianceActor& actor, bool forced);

// The Ashley relation fields packed in an actor's saved relation options.
struct SavedAshleyRelation {
  std::uint8_t previous_classification = 0;
  std::uint8_t classification = 0;
  std::uint8_t classification_timer = 0;
  bool allied = false;
};

// Decodes the packed options word (bit 0 allied, bits 1-2 classification,
// bits 3-4 previous classification, bits 6-12 timer).
[[nodiscard]] SavedAshleyRelation DecodeSavedAshleyRelation(
    std::uint16_t options);

// The actor AI states whose saved relation options are restored.
class SavedRelationActors {
 public:
  virtual ~SavedRelationActors() = default;
  // Actor IDs present in the room, in list order.
  [[nodiscard]] virtual std::span<const std::uint8_t> RoomActors() const = 0;
  // The saved options; negative when the actor has none.
  [[nodiscard]] virtual std::int16_t SavedOptions(int actor) const = 0;
  // Writes the Ashley relation, clearing its active flag and every reserved
  // flag but the preserved one.
  virtual void RestoreAshleyRelation(int actor,
                                     const SavedAshleyRelation& relation) = 0;
  virtual void BeginForcedAlliance(int actor) = 0;
};

// Restores each room actor's Ashley relation from its saved options and
// re-enters a saved alliance (func_800E72D0).
void ApplySavedActorAiRelations(SavedRelationActors& actors);

// A listed actor's AI group fields (battle actor word 0x30).
struct LinkGroupActor {
  std::uint8_t id = 0;
  // Bits 0-3: the group the actor belongs to.
  std::uint8_t group = 0;
  // Bits 9-13: the group whose first listed member the actor follows.
  std::uint8_t linked_group = 0;
};

// The listed actors and the linked-actor field of their AI states.
class LinkedActorTable {
 public:
  virtual ~LinkedActorTable() = default;
  // The actors after the list head (Ashley), in battle actor list order, with
  // IDs 0 to kRelationActorSlots - 1.
  [[nodiscard]] virtual std::span<const LinkGroupActor> FollowingActors()
      const = 0;
  // The actor the AI state follows (kNoLinkedActor, 16, while unresolved);
  // nothing without an AI state.
  [[nodiscard]] virtual std::optional<int> LinkedActor(int actor_id) const = 0;
  virtual void SetLinkedActor(int actor_id, int linked) = 0;
};

// Links every listed actor whose linked actor is unresolved to the first
// listed actor, itself included, whose group equals its linked group; the list
// head is never examined and a linked group of 16 or more matches no actor.
// Returns false, writing nothing, when a listed actor has no AI state (#337)
// (func_800E5998).
[[nodiscard]] bool ResolveAiLinkedActors(LinkedActorTable& actors);
}  // namespace VS::Battle::Ai
