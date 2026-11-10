#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Battle/Ai/ConditionSnapshot.hpp"

// The battle AI action planner's candidate list: the 20-byte candidate record,
// the ranking the planner sorts, the simulated stat snapshot it plans against,
// and the per-slot usability and targeting checks. The battle runtime owns the
// actor AI state and the stat records; these routines see them only through
// the value records and hosts below.
namespace VS::Battle::Ai {
// Entries in an actor's candidate list and its ranking (AI state 0x2F0/0x430).
inline constexpr std::size_t kCandidateCapacity = 16;
// Actor slots a candidate can target: its 4-bit target ID indexes the relation,
// distance and AI state tables.
inline constexpr int kCandidateTargetSlots = 16;
// The limb action table: six limbs of four action slots each.
inline constexpr int kLimbActionRows = 6;
inline constexpr int kLimbActionColumns = 4;
inline constexpr unsigned kActionDefinitionCount = 256;
// A Battle_ActorStats record (0x964 bytes).
inline constexpr std::size_t kActorStatsRecordBytes = 0x964;
inline constexpr std::uint16_t kTargetMemoryFrames = 900;

struct StatsByteRange {
  std::size_t offset;
  std::size_t size;
};
// The stats record fields a planning snapshot copies: HP and MP, the movement
// bytes, the six limb records and the status word.
inline constexpr StatsByteRange kSnapshotVitals{.offset = 0x18, .size = 8};
inline constexpr StatsByteRange kSnapshotMovement{.offset = 0x30, .size = 4};
inline constexpr StatsByteRange kSnapshotLimbs{.offset = 0x398, .size = 0x528};
inline constexpr StatsByteRange kSnapshotStatuses{.offset = 0x948, .size = 4};
inline constexpr auto kPlanningStatSnapshot = std::to_array(
    {kSnapshotVitals, kSnapshotMovement, kSnapshotLimbs, kSnapshotStatuses});

// The option bits of a candidate's fifth word.
enum class CandidateOption : std::uint8_t {
  // The range is a minimum distance the target must keep.
  kMinimumRange = 1,
  kIgnoresSight = 2,
  kIgnoresObstruction = 3,
  kHarmful = 4,
  kBeneficial = 5,
};

// One planned action, the BATTLE.PRG candidate record (func_800DCAA0_t), kept
// as its five words so a copy carries every bit the planner does not name.
class ActionCandidate {
 public:
  static constexpr std::size_t kWordCount = 5;
  using Words = std::array<std::uint32_t, kWordCount>;

  constexpr ActionCandidate() = default;
  constexpr explicit ActionCandidate(const Words& words) : words_(words) {}

  [[nodiscard]] constexpr const Words& RawWords() const { return words_; }
  [[nodiscard]] std::int32_t Score() const;
  [[nodiscard]] unsigned Row() const;
  [[nodiscard]] unsigned Column() const;
  [[nodiscard]] unsigned Action() const;
  [[nodiscard]] unsigned Threshold() const;
  [[nodiscard]] std::int32_t DistanceSquared() const;
  [[nodiscard]] unsigned TargetId() const;
  [[nodiscard]] unsigned Weight() const;
  [[nodiscard]] bool Option(CandidateOption option) const;
  [[nodiscard]] unsigned Range() const;

  // Each setter keeps only the field's width, as the record's bit-fields do.
  void SetScore(std::int32_t score);
  void SetSlot(unsigned row, unsigned column);
  void SetAction(unsigned action);
  void SetThreshold(unsigned threshold);
  void SetDistanceSquared(std::int32_t distance_squared);
  void SetTargetId(unsigned target_id);
  void SetWeight(unsigned weight);
  void SetOption(CandidateOption option, bool enabled);
  void SetRange(unsigned range);

 private:
  Words words_{};
};

// A ranking entry: the candidate fields the ranking reads and the binding's
// handle for the candidate it refers to.
struct RankedCandidate {
  std::int32_t score = 0;
  unsigned action = 0;
  unsigned weight = 0;
  std::size_t handle = 0;
};

// An actor's candidate storage, ranking and count, and the planner's
// target/action exclusion table.
class CandidateList {
 public:
  virtual ~CandidateList() = default;
  [[nodiscard]] virtual int Count() const = 0;
  // Stores the candidate in storage slot `slot` and ranks it at that position.
  virtual void Store(std::size_t slot, const ActionCandidate& candidate) = 0;
  virtual void SetCount(int count) = 0;
  // False when the exclusion table is not allocated.
  [[nodiscard]] virtual bool ExcludeTargetAction(unsigned target,
                                                 unsigned action) = 0;
};

class CandidateReadinessHost {
 public:
  virtual ~CandidateReadinessHost() = default;
  [[nodiscard]] virtual int Count() const = 0;
  [[nodiscard]] virtual std::optional<std::size_t> RankedHandle(
      std::size_t rank) const = 0;
  [[nodiscard]] virtual std::optional<ActionCandidate> Candidate(
      std::size_t handle) const = 0;
  [[nodiscard]] virtual std::uint16_t ReadinessLevel() const = 0;
  virtual std::optional<bool> CanAffect(std::size_t handle,
                                        unsigned target) = 0;
  virtual std::optional<bool> RefreshUsability(unsigned row,
                                               unsigned column) = 0;
  virtual bool ClearWeight(std::size_t handle) = 0;
  virtual bool SetFirstRank(std::size_t handle) = 0;
  virtual bool ClearCount() = 0;
  virtual bool SelectMovementMode() = 0;
  [[nodiscard]] virtual std::optional<bool> HasHostileActor() const = 0;
  virtual bool StoreActionStatus(std::uint8_t status) = 0;
};

std::optional<bool> RefreshActionCandidateReadiness(
    CandidateReadinessHost& host);

// An actor's target memory fields (AI state 0x38, 0x3C and 0x40).
struct TargetMemory {
  std::uint32_t previous_tile = 0;
  std::uint32_t remembered_target_tile = 0;
  std::uint16_t frames = 0;
};

// The AI state and model placements the target memory reads.
class TargetMemoryHost {
 public:
  virtual ~TargetMemoryHost() = default;
  [[nodiscard]] virtual int CandidateCount() const = 0;
  // The actor's settings disable target memory (AI state byte 0x19).
  [[nodiscard]] virtual bool MemoryDisabled() const = 0;
  // The target of the candidate ranked first; nothing for an empty ranking.
  [[nodiscard]] virtual std::optional<unsigned> FirstRankedTarget() const = 0;
  // The actor's own packed tile (AI state 0x34).
  [[nodiscard]] virtual std::uint32_t OwnTile() const = 0;
  // The target model's packed tile; nothing when it has no placement.
  [[nodiscard]] virtual std::optional<std::uint32_t> TargetTile(
      unsigned target) const = 0;
  virtual void Remember(const TargetMemory& memory) = 0;
};

// An actor's HP, MP, weapon PP and status words as its stats store them.
struct ActorResources {
  std::uint16_t hp = 0;
  std::uint16_t max_hp = 0;
  std::uint16_t mp = 0;
  std::uint16_t max_mp = 0;
  std::uint16_t weapon_pp = 0;
  std::uint32_t statuses = 0;
};

// The resource restrictions an actor's AI state records (bytes 0x10-0x12).
struct ActorResourceConditions {
  bool status_restricted = false;
  bool low_mp = false;
  bool low_hp = false;
};

// The type and cost of an action definition.
struct ActionCost {
  unsigned type = 0;
  unsigned cost = 0;
};

struct SimulatedStatChange {
  std::int16_t delta = 0;
  std::uint8_t operation = 1;
};

struct SimulatedActorEffect {
  std::uint8_t actor_id = 0;
  bool ignored = false;
  SimulatedStatChange hp{};
  SimulatedStatChange mp{};
  SimulatedStatChange weapon_dp{};
  SimulatedStatChange shield_dp{};
  std::uint32_t statuses_set = 0;
  std::uint32_t statuses_clear = 0;
};

bool ApplySimulatedActionResults(std::span<ActorCondition> conditions,
                                 const ActionCost& action,
                                 const SimulatedActorEffect& performer,
                                 std::span<const SimulatedActorEffect> targets);

// The terms of an actor's condition the planner scores a candidate with
// (func_800DCC94, BATTLE.PRG 74088.c:620). The original inlines the four
// helpers (74088.c:565-619) into one run of 99 instructions
// (0x800DCDD4-0x800DCF5C), so they have no symbol or address of their own.
// Ratios are not narrowed here; the scoring narrows them to halfwords.

// decomp: BATTLE.PRG _getStatusScore (74088.c:565), inlined in func_800DCC94.
// The weights of the actor's set status bits (bit 12 only when the actor's AI
// state enables action costs), less 128 at zero HP and a further 84 below
// 44/128 of maximum HP.
[[nodiscard]] std::int32_t ActorStatusConditionScore(
    const ActorCondition& condition, bool action_cost_enabled);

// decomp: BATTLE.PRG _getHpRatio (74088.c:590), inlined in func_800DCC94.
// HP scaled to 128 at the maximum and truncated toward zero; zero without a
// maximum.
[[nodiscard]] std::int32_t ActorHpRatio(const ActorCondition& condition);

// decomp: BATTLE.PRG _getMpRatio (74088.c:598), inlined in func_800DCC94.
// MP scaled like the HP ratio.
[[nodiscard]] std::int32_t ActorMpRatio(const ActorCondition& condition);

// decomp: BATTLE.PRG _getExtraRatio (74088.c:606), inlined in func_800DCC94.
// The weapon's and the shield's DP, each scaled like the HP ratio, summed.
[[nodiscard]] std::int32_t ActorEquipmentDpRatio(
    const ActorCondition& condition);

// The actor stats, action table and AI state the usability refresh reads.
class ActionUsabilityHost {
 public:
  virtual ~ActionUsabilityHost() = default;
  // The action ID a limb action slot holds.
  [[nodiscard]] virtual unsigned SlotAction(int row, int column) const = 0;
  // Called only with an ID below kActionDefinitionCount.
  [[nodiscard]] virtual ActionCost Action(unsigned action_id) const = 0;
  [[nodiscard]] virtual std::int16_t LimbHp(int row) const = 0;
  [[nodiscard]] virtual ActorResources Resources() const = 0;
  virtual void SetSlotUsable(int row, int column, bool usable) = 0;
};

// The deciding actor's relations, its peers' restrictions and the action
// table the target eligibility check reads. IDs are below
// kCandidateTargetSlots; actions are below kActionDefinitionCount.
class ActionTargetingHost {
 public:
  virtual ~ActionTargetingHost() = default;
  [[nodiscard]] virtual int ActorId() const = 0;
  [[nodiscard]] virtual unsigned Classification(int actor_id) const = 0;
  // Nothing when the actor slot holds no AI state.
  [[nodiscard]] virtual std::optional<ActorResourceConditions> PeerConditions(
      int actor_id) const = 0;
  [[nodiscard]] virtual std::uint16_t ActionAiFlags(unsigned action) const = 0;
  // The limb action slot never targets its own actor (slot bit 1).
  [[nodiscard]] virtual bool SlotExcludesSelf(int row, int column) const = 0;
};

// decomp: func_800E4C8C (src/BATTLE/BATTLE.PRG/7BDD4.s:585).
// Remembers the first-ranked target and the actor's tile for 900 frames. False
// when the ranking is empty or memory is disabled; nothing refuses a count
// past the ranking, an empty first entry or a target without a placement.
std::optional<bool> RememberAiActionTarget(TargetMemoryHost& host);

// decomp: func_800DC2E0 (src/BATTLE/BATTLE.PRG/73AE0.s:9).
void CopyActionCandidate(ActionCandidate& destination,
                         const ActionCandidate& source);

// decomp: func_800DC30C (src/BATTLE/BATTLE.PRG/73B0C.s:9).
// Restores (adds) or spends (subtracts) a simulated stat clamped to
// [0, maximum]; a negative maximum is stored when the result is not negative.
// The subtraction is always evaluated, as the original's trapping `sub` is;
// nothing refuses a result outside the signed 32-bit range.
[[nodiscard]] std::optional<std::int16_t> ApplySimulatedStatDelta(
    std::int16_t value, std::int16_t maximum, std::int32_t delta,
    bool restores);

// decomp: func_800DC344 (src/BATTLE/BATTLE.PRG/73B0C.s:30).
// Appends a weighted candidate and excludes its target/action pair from
// further planning. False for a zero weight or a full list; nothing refuses a
// count outside the list or a missing exclusion table.
std::optional<bool> AppendActionCandidate(CandidateList& list,
                                          const ActionCandidate& candidate);

// decomp: func_800DC3CC (src/BATTLE/BATTLE.PRG/73B0C.s:70);
// partition: func_800E6974 / L800E6958 (src/BATTLE/BATTLE.PRG/7E05C.s:114).
// Orders the ranking by descending score with the original partition's tie
// order. False refuses more than kCandidateCapacity entries.
[[nodiscard]] bool SortActionCandidates(std::span<RankedCandidate> ranking);

// decomp: func_800DC3E8 (src/BATTLE/BATTLE.PRG/73B0C.s:81).
// Copies the kPlanningStatSnapshot fields of a stats record.
void CopyPlanningActorStats(
    std::span<const std::byte, kActorStatsRecordBytes> source,
    std::span<std::byte, kActorStatsRecordBytes> destination);

// decomp: func_800E4CF4 (src/BATTLE/BATTLE.PRG/7C4F4.s:10).
// Whether the candidate's action may target the actor: an allied or hostile
// actor with AI state, a beneficial action only on a non-hostile actor whose
// restrictions the action addresses, a harmful one only on a hostile actor,
// and never the actor itself from a self-excluding slot. Nothing refuses an
// actor or slot outside its table.
[[nodiscard]] std::optional<bool> CanAiActionAffectActor(
    const ActionTargetingHost& host, const ActionCandidate& candidate,
    int actor_id);

// decomp: func_800E4DF8 (src/BATTLE/BATTLE.PRG/7C4F4.s:82).
// Records whether a limb action slot is usable: the limb has at least 2 HP,
// statuses allow it, and the actor can pay its MP, HP or weapon PP cost.
// Nothing refuses a slot or action ID outside its table.
std::optional<bool> RefreshAiActionUsability(ActionUsabilityHost& host, int row,
                                             int column);

// decomp: func_800E4F14 (src/BATTLE/BATTLE.PRG/7C4F4.s:162).
// The actor's resource restrictions; a zero threshold disables the MP and
// spell-blocking checks.
[[nodiscard]] ActorResourceConditions RefreshAiResourceRestrictions(
    const ActorResources& resources, std::uint8_t action_cost_threshold);
}  // namespace VS::Battle::Ai
