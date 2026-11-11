#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
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
// The classification an actor gives itself and the actors on its side.
inline constexpr unsigned kAllyClassification = 2;

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

// The settings bytes of an AI state that steer its reaction to actions and
// its hostility toward other actors (actionReactionState, 74088.c:681). Each
// flag is the whole byte the original tests.
struct ReactionSettings {
  // Byte 0x13: the actor is temporarily allied with Ashley.
  bool allied_with_ashley = false;
  // Settings bit 25 (byte 0x30): harm makes the acting actor hostile at once
  // (only Ashley while bit 26 is also set).
  bool settings_bit25 = false;
  // Settings bit 26 (byte 0x20): a grievance makes the acting actor hostile.
  bool settings_bit26 = false;
  // Settings bit 27 (byte 0x27): Ashley's help can begin, and her harm end, a
  // temporary alliance.
  bool settings_bit27 = false;
  // Settings byte 3 bit 0 (byte 0x21): the actor takes up its allies'
  // hostility.
  bool shares_ally_hostility = false;
  // Settings bits 21-22 (byte 0x2C): the relation category.
  std::uint8_t category = 0;
};

// The AI state reacting to an action or sharing hostility, and the other AI
// states it reads. A relation word packs bit 0 active, bits 6-7
// classification, bits 15-19 reaction timer, bits 20-26 grievance counter and
// bits 28-29 previous classification.
class ActorReactionHost {
 public:
  virtual ~ActorReactionHost() = default;
  [[nodiscard]] virtual int ActorId() const = 0;
  [[nodiscard]] virtual std::uint8_t BehaviorMode() const = 0;
  virtual void SetBehaviorMode(std::uint8_t mode) = 0;
  [[nodiscard]] virtual ReactionSettings Settings() const = 0;
  // The relation word toward `target` (0 to kRelationActorSlots - 1).
  [[nodiscard]] virtual std::uint32_t Relation(int target) const = 0;
  virtual void SetRelation(int target, std::uint32_t word) = 0;
  // Another actor's AI state settings and relation words; nothing when
  // `actor` has no AI state.
  [[nodiscard]] virtual std::optional<ReactionSettings> PeerSettings(
      int actor) const = 0;
  [[nodiscard]] virtual std::optional<std::uint32_t> PeerRelation(
      int actor, int target) const = 0;
  // Writes a relation word of a peer whose state PeerRelation returned.
  virtual void SetPeerRelation(int actor, int target, std::uint32_t word) = 0;
  // Whether `action` helps or harms this actor (func_800DC48C,
  // func_800DC484).
  [[nodiscard]] virtual bool IsBeneficial(int action) const = 0;
  [[nodiscard]] virtual bool IsHarmful(int action) const = 0;
  // TryBeginAshleyAlliance without forcing, and EndAshleyAlliance.
  virtual void TryBeginAlliance() = 0;
  virtual void EndAlliance() = 0;
  // MakeTargetHostile for `target`.
  virtual void MakeHostile(int target) = 0;
  // Stops the arena's orbiting boss circling, if the arena has one.
  virtual void StopArenaOrbit() = 0;
};

// One hit of an applied action (actionStatDelta, 74088.c:446).
struct ReactionHit {
  int actor = 0;
  std::int16_t hp_change = 0;
  // Bits 0-1 of the effect word: 1 damages, 2 restores, 3 damages both ways.
  unsigned hp_effect = 0;
  // The nonzero word at 0x40 that excludes the hit.
  bool skipped = false;
};

// An applied action (actionStatApplication, 74088.c:467).
struct AppliedAction {
  int action = 0;
  // Each hit definition's prerequisite function (effect bits 7-12); nothing
  // for an action outside the action table.
  std::optional<std::array<std::uint8_t, 2>> hit_prerequisites;
  // The acting actor's own entry.
  ReactionHit acting;
  std::span<const ReactionHit> targets;
};

// decomp: func_800DD000 (src/BATTLE/BATTLE.PRG/74088.c:716).
// Reacts to each of the action's hits on this actor by another actor. A
// holding actor (behaviour mode 1) is released (2). Help from Ashley may begin
// an alliance with her, while help from an actor hostile to Ashley ends one.
// The acting actor's reaction timer restarts at 28. Harm stops the orbiting
// boss when Ashley damages it, ends an alliance when she causes it, and makes
// the acting actor hostile, forgives a grievance it holds against this actor
// or records one against it. Nothing happens for a skipped acting entry, for
// Ashley's own state or for an action whose hit uses prerequisite 3. False
// refuses, possibly part-way, an action outside the table, an acting actor
// outside the AI slots or the acting actor's missing state where it is read.
[[nodiscard]] bool ApplyActorActionReaction(ActorReactionHost& host,
                                            const AppliedAction& applied);

// decomp: func_800DD344 (src/BATTLE/BATTLE.PRG/74088.c:782).
// While allied with Ashley, reclassifies each room actor: itself, Ashley and
// noncombatants as allies (2), an actor hostile to Ashley outside such an
// alliance as hostile (3, clearing its grievance counter) and the rest as
// neutral (1). Otherwise, when the actor shares its allies' hostility, it
// turns hostile toward each active neutral actor a mutual ally is hostile to,
// unless some mutual ally counts that actor as an ally or holds a grievance
// against it over a former alliance. `room_actors` lists the battle actors in
// list order. False refuses, possibly part-way, an ID outside the AI slots or
// a missing AI state where the original reads it.
[[nodiscard]] bool PropagateActorRelationships(
    ActorReactionHost& host, std::span<const std::uint8_t> room_actors);

// The room effects that follow a completed action.
class ActionNotificationHost {
 public:
  virtual ~ActionNotificationHost() = default;
  // Clears the actor's reengagement grace; nothing without an AI state.
  virtual void ClearReengagementGrace(int actor) = 0;
  // Every AI actor reacts to the action (func_800DC810).
  virtual void ApplyRelationshipChanges() = 0;
};

// decomp: func_800DEEA4 (src/BATTLE/BATTLE.PRG/74088.c:1596).
// Unless the acting entry is skipped, lets the acting actor engage again at
// once, then has every AI actor react to the action. False refuses, doing
// nothing, an unskipped acting actor outside the AI slots.
[[nodiscard]] bool NotifyActionRelationshipChanges(ActionNotificationHost& host,
                                                   const ReactionHit& acting);
}  // namespace VS::Battle::Ai
