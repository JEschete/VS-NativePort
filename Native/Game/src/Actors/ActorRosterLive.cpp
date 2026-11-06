// Hand-maintained (tools/port/owned.csv).
// Live binding for the room actor roster (ActorRoster.cpp).
#include "Actors/ActorRosterLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>
#include <span>

#include "Actors/ActorRoster.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/ActorTableLive.hpp"
#include "Actors/BattleActorState.hpp"
#include "Actors/RoomEnemyMemoryLive.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Core/Memory/HeapLive.hpp"
extern "C" {
#include "Actors/Spawning.h"
#include "Battle/AbilityState.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Field/Gimmicks.h"
}

namespace VS::Actors {
namespace {
constexpr u_short kLinkableKindMask = 0x7;
constexpr u_short kGimmickKind = 0x10;
constexpr int kSecondModelAllocation = 2;

static_assert(std::size(Battle_actors) >= kRosterSlots);
static_assert(std::size(Battle_actors) == kBattleActorSlots);
static_assert(sizeof Battle_ActorStats{}.name == kActorNameBytes);

RosterView ViewFrom(const Battle_Actor* actor) {
  RosterView view{};
  for (std::size_t slot = 0; slot < view.size(); ++slot) {
    const Battle_Actor* occupant = Battle_actors[slot];
    view[slot] = {
        .occupied = occupant != nullptr,
        .links_to_actor = occupant != nullptr && occupant->next == actor};
  }
  return view;
}

RosterCandidate CandidateOf(const Battle_Actor& actor) {
  return {.linkable_kind = (actor.kind & kLinkableKindMask) != 0,
          .leader = actor.id == 0};
}

// The actor table as RemoveActorAtSlot edits it. The actor is read once, as
// the original did, before anything is released.
class LiveRemovalHost final : public ActorRemovalHost {
 public:
  explicit LiveRemovalHost(Battle_Actor* actor) : actor_(actor) {}

  [[nodiscard]] std::optional<RosterOccupant> Occupant(
      std::size_t /*slot*/) const override {
    if (actor_ == nullptr) {
      return std::nullopt;
    }
    return RosterOccupant{
        .gimmick = actor_->kind == kGimmickKind,
        .second_model = actor_->allocationKind == kSecondModelAllocation};
  }
  void Remember(std::size_t slot, int hidden) override {
    StoreLiveRoomActorState(static_cast<int>(slot), hidden);
  }
  void ReleaseGimmick(std::size_t slot) override {
    Field_DropUnsupportedGimmickStack(static_cast<int>(slot));
    Field_ReleaseGimmickInstance(static_cast<int>(slot));
  }
  void ReleaseModel(std::size_t model_slot) override {
    VS::Content::ReleaseLiveActorModel(static_cast<int>(model_slot));
  }
  void FreeAiState(std::size_t /*slot*/) override {
    VS::Battle::Ai::FreeLiveActorAiState(actor_);
  }
  [[nodiscard]] RosterView Links(std::size_t /*slot*/) const override {
    return ViewFrom(actor_);
  }
  void Bypass(std::size_t predecessor, std::size_t /*slot*/) override {
    Battle_actors[predecessor]->next = actor_->next;
  }
  void Free(std::size_t slot) override {
    VS::Core::Memory::FreeLiveFromLowEnd(actor_);
    Battle_actors[slot] = nullptr;
  }

 private:
  Battle_Actor* actor_;
};

class LiveDisplayFlagsHost final : public DisplayFlagsHost {
 public:
  [[nodiscard]] std::uint32_t StateFlags(std::size_t slot) const override {
    return LiveActorStateFlags(static_cast<int>(slot));
  }
  void SetDisplayFlags(std::size_t slot, std::uint32_t flags) override {
    SetLiveActorDisplayFlags(static_cast<int>(slot), std::bit_cast<int>(flags));
  }
};

// The original copied 24 name bytes from the 20-byte placeholder name
// (D_8004FDD0), reading the first four bytes of the table after it,
// Battle_breakArtPointRequirements (D_8004FDE4). The name ends at its fifth
// byte (0xE7), so the copied tail is never shown; it is kept byte for byte.
std::array<std::uint8_t, kActorNameBytes> PlaceholderName() {
  constexpr std::size_t kNameBytes = sizeof Actors_placeholderActorName;
  static_assert(kNameBytes < kActorNameBytes);
  static_assert(sizeof Battle_breakArtPointRequirements >=
                kActorNameBytes - kNameBytes);
  std::array<std::uint8_t, kActorNameBytes> name{};
  const std::span<std::uint8_t> bytes(name);
  std::memcpy(bytes.data(), Actors_placeholderActorName, kNameBytes);
  std::memcpy(bytes.subspan(kNameBytes).data(),
              Battle_breakArtPointRequirements, kActorNameBytes - kNameBytes);
  return name;
}
}  // namespace

int LinkLiveActorAfterAshley(Battle_Actor* actor) {
  constexpr const char* kOperation = "LinkLiveActorAfterAshley";
  if (actor == nullptr) {
    WarnActorRefused(kOperation, "no actor");
    return 0;
  }
  const RosterDecision decision =
      LinkActorAfterAshley(ViewFrom(actor), CandidateOf(*actor));
  if (decision.edit == RosterEdit::kRefusedNoLeader) {
    WarnActorRefused(kOperation, "Ashley's actor slot is empty");
    return 0;
  }
  if (decision.edit != RosterEdit::kInsertAfterLeader) {
    return 0;
  }
  Battle_Actor* leader = Battle_actors[decision.slot];
  actor->next = leader->next;
  leader->next = actor;
  return 1;
}

int UnlinkLiveActorFromRoomList(Battle_Actor* actor) {
  if (actor == nullptr) {
    WarnActorRefused("UnlinkLiveActorFromRoomList", "no actor");
    return 0;
  }
  const RosterDecision decision =
      UnlinkActorFromRoomList(ViewFrom(actor), CandidateOf(*actor));
  if (decision.edit != RosterEdit::kBypass) {
    return 0;
  }
  Battle_actors[decision.slot]->next = actor->next;
  actor->next = nullptr;
  return 1;
}

int RemoveLiveActorAtSlot(unsigned index, int hidden) {
  LiveRemovalHost host(index < kRosterSlots ? Battle_actors[index] : nullptr);
  return RemoveActorAtSlot(host, index, hidden) == RemovalResult::kOutOfRange
             ? 0
             : 1;
}

void FreezeLiveActorAnimations() {
  LiveDisplayFlagsHost host;
  FreezeAllActorAnimations(host);
}

void UnfreezeLiveActorAnimations() {
  LiveDisplayFlagsHost host;
  UnfreezeAllActorAnimations(host);
}

void InitializeLivePlaceholderActor(int actor_id, int auxiliary) {
  constexpr const char* kOperation = "InitializeLivePlaceholderActor";
  if (!IsBattleActorSlot(actor_id)) {
    WarnActorRefused(kOperation, "the actor id is outside Battle_actors");
    return;
  }
  Battle_Actor* actor = Battle_actors[actor_id];
  if (actor == nullptr || actor->stats == nullptr) {
    WarnActorRefused(kOperation, "the actor slot has no actor stats");
    return;
  }
  const std::array<std::uint8_t, kActorNameBytes> name = PlaceholderName();
  const PlaceholderActorStats placeholder =
      BuildPlaceholderActorStats(auxiliary, name);
  Battle_ActorStats* stats = actor->stats;
  std::memset(stats, 0, sizeof *stats);
  stats->maxHp = placeholder.max_hp;
  stats->currentHp = placeholder.current_hp;
  std::memcpy(stats->name, placeholder.name.data(), placeholder.name.size());
  stats->cancelsHitResults = placeholder.cancels_hit_results ? 1 : 0;
  if (placeholder.marker_flags.has_value()) {
    Battle_SetActorMarker(actor_id, *placeholder.marker_flags);
  }
  actor->roomSpawnIndex = placeholder.room_spawn_index;
  actor->killCounted = placeholder.kill_counted ? 1 : 0;
  actor->defeated = placeholder.defeated ? 1 : 0;
  stats->actorId = placeholder.actor_id;
}
}  // namespace VS::Actors
