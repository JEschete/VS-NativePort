#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>

// The battle-actor rules of the actor state code (BATTLE.PRG
// 146C.c:8686-9230): hostility, the HP percentage, the vitals restore, the
// model id and kind scans, post-defeat retention, the AI logic switch and the
// room-list detach. The rules see battle actors as BattleActorState and
// ActorVitals values the host lends them, and reach the roster, AI, markers
// and status effects through the host, so they reach no Battle_Actor layout
// or PS1 address.
namespace VS::Actors {
// Battle_actors: the room actors, then the effect and target placeholders.
inline constexpr int kBattleActorSlots = 42;
// The room actors the scans cover.
inline constexpr int kBattleActorScanSlots = 16;
inline constexpr std::uint16_t kHostileActorKind = 4;
inline constexpr std::uint16_t kPeacefulActorKind = 8;
// The command state and kind bit of an actor whose AI logic is off.
inline constexpr std::uint8_t kLogicDisabledCommand = 0x20;
inline constexpr std::uint16_t kLogicDisabledKind = 0x8000;
// The slot FindActorByModelId reports when no room actor has the model.
inline constexpr int kModelNotFoundSlot = 1;
// The marker flags that clear an actor's marker kind and its pinned mark.
inline constexpr int kClearMarkerKind = 0;
inline constexpr int kClearPinnedMarker = 0x40;
inline constexpr std::size_t kActorLimbs = 6;

// The battle-actor fields the rules use; "read" fields are never stored.
struct BattleActorState {
  std::uint8_t command_state = 0;
  std::uint16_t model_id = 0;  // read
  std::uint16_t kind = 0;
  bool retain_after_defeat = false;  // read

  friend bool operator==(const BattleActorState&,
                         const BattleActorState&) = default;
};

struct LimbVitals {
  std::int16_t hp = 0;
  std::int16_t max_hp = 0;  // read

  friend bool operator==(const LimbVitals&, const LimbVitals&) = default;
};

// The stats the vitals restore and the HP percentage use.
struct ActorVitals {
  std::int16_t current_hp = 0;
  std::int16_t max_hp = 0;  // read
  std::int16_t current_mp = 0;
  std::int16_t max_mp = 0;  // read
  std::uint16_t risk = 0;
  std::uint16_t action_recovery_timer = 0;
  std::array<LimbVitals, kActorLimbs> limbs{};

  friend bool operator==(const ActorVitals&, const ActorVitals&) = default;
};

// The battle actor table and the systems the rules drive. Actors and vitals
// are lent by Actor and Vitals and stay valid for the whole operation; every
// other call may read or write them, so the host stores the lent values
// before it and reloads them after it.
class BattleActorHost {
 public:
  BattleActorHost() = default;
  BattleActorHost(const BattleActorHost&) = delete;
  BattleActorHost& operator=(const BattleActorHost&) = delete;
  BattleActorHost(BattleActorHost&&) = delete;
  BattleActorHost& operator=(BattleActorHost&&) = delete;
  virtual ~BattleActorHost() = default;

  // The battle actor in slot `id`; null for an empty slot or an id outside
  // the table.
  [[nodiscard]] virtual BattleActorState* Actor(int id) = 0;
  // The actor's vitals; null when the slot is empty or has no stats.
  [[nodiscard]] virtual ActorVitals* Vitals(int id) = 0;
  // Ends every status effect on the actor and clears the recovery reset.
  virtual void ClearStatuses(int id) = 0;
  virtual void SetMarker(int id, int flags) = 0;
  // Whether the room actor list changed.
  [[nodiscard]] virtual bool UnlinkFromRoomList(int id) = 0;
  [[nodiscard]] virtual bool LinkAfterAshley(int id) = 0;
  virtual void SetAnimationLocked(int id, bool locked) = 0;
  virtual void ClearAiLinksAndPlan(int id) = 0;
  virtual void ResolveAiLinkedActors() = 0;
  virtual void RebuildActionPlans() = 0;
};

// decomp: BATTLE.PRG func_8007CF64 (src/BATTLE/BATTLE.PRG/146C.c:9139)
// Whether `id` names a battle actor table slot; the original read any index.
[[nodiscard]] bool IsBattleActorSlot(int id);
// decomp: BATTLE.PRG func_8007C1D8 (src/BATTLE/BATTLE.PRG/146C.c:8686)
void SetActorHostile(BattleActorHost& host, int id, bool hostile);
// The HP percentage, at least 1 while any HP remains. A living actor with no
// maximum HP (which the original divided by) is reported at 1.
[[nodiscard]] int HpPercentage(int current_hp, int max_hp);
// decomp: BATTLE.PRG func_8007C218 (src/BATTLE/BATTLE.PRG/146C.c:8697)
// 0 for an actor without stats; the original read an empty slot's stats.
[[nodiscard]] int ActorHpPercentage(BattleActorHost& host, int id);
// decomp: BATTLE.PRG func_8007C28C (src/BATTLE/BATTLE.PRG/146C.c:8714)
void RestoreActorVitals(BattleActorHost& host, int id);
// decomp: BATTLE.PRG func_8007CF18 (src/BATTLE/BATTLE.PRG/146C.c:9127)
// The first room actor with the model, or kModelNotFoundSlot.
[[nodiscard]] int FindActorByModelId(BattleActorHost& host, int model_id);
// decomp: BATTLE.PRG func_8007CF80 (src/BATTLE/BATTLE.PRG/146C.c:9141)
[[nodiscard]] int CountActorsByDisposition(BattleActorHost& host, int kind);
// decomp: BATTLE.PRG func_8007CFCC (src/BATTLE/BATTLE.PRG/146C.c:9155)
[[nodiscard]] bool IsActorRetainedAfterDefeat(BattleActorHost& host, int id);
// decomp: BATTLE.PRG func_8007D08C (src/BATTLE/BATTLE.PRG/146C.c:9179)
// Disabling suspends the actor's logic and clears its markers; enabling
// resumes only an actor whose logic was suspended.
void SetActorLogicEnabled(BattleActorHost& host, int id, bool enabled);
// decomp: BATTLE.PRG func_8007D15C (src/BATTLE/BATTLE.PRG/146C.c:9199)
// Every room actor but Ashley.
void SetOtherActorLogicEnabled(BattleActorHost& host, bool enabled);
// decomp: BATTLE.PRG func_8007D1A8 (src/BATTLE/BATTLE.PRG/146C.c:9209)
// Takes the actor out of (or back into) the room actor list, locking its
// animation while detached, and refreshes the AI when the list changed.
void SetActorDetached(BattleActorHost& host, int id, bool detached);
}  // namespace VS::Actors
