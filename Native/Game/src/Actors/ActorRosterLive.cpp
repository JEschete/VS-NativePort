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

#include "Actors/ActorInstances.hpp"
#include "Actors/ActorRoster.hpp"
#include "Actors/ActorSpawningLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/ActorTableLive.hpp"
#include "Actors/BattleActorState.hpp"
#include "Actors/RoomEnemyMemoryLive.hpp"
#include "Actors/Spawning.hpp"
#include "Battle/AbilityState.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Feedback.hpp"
#include "Battle/HudOrchestrationLive.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Field/GimmickPlacementLive.hpp"
#include "Field/GimmickResourcesLive.hpp"
#include "Field/Gimmicks.hpp"

namespace VS::Actors {
namespace {
constexpr std::uint16_t kLinkableKindMask = 0x7;

static_assert(std::size(VS::Battle::actors) >= kRosterSlots);
static_assert(std::size(VS::Battle::actors) == kBattleActorSlots);
static_assert(sizeof VS::Battle::ActorStats{}.name == kActorNameBytes);

RosterView ViewFrom(SpawnedActor actor) {
  RosterView view{};
  for (std::size_t slot = 0; slot < view.size(); ++slot) {
    const VS::Battle::Actor* occupant = VS::Battle::actors[slot];
    view[slot] = {
        .occupied = occupant != nullptr,
        .links_to_actor = occupant != nullptr && occupant->next == actor};
  }
  return view;
}

RosterCandidate CandidateOf(const VS::Battle::Actor& actor) {
  return {.linkable_kind = (actor.kind & kLinkableKindMask) != 0,
          .leader = actor.id == 0};
}

// The actor table as RemoveActorAtSlot edits it. The actor is read once, as
// the original did, before anything is released; what it owns comes from its
// instance record, not from its rewritable kind.
class LiveRemovalHost final : public ActorRemovalHost {
 public:
  LiveRemovalHost(VS::Battle::Actor* actor, std::size_t slot)
      : actor_(actor),
        identity_(LiveActorIdentity(actor)),
        components_(LiveActorComponents(identity_)),
        slot_(slot) {}

  [[nodiscard]] std::optional<ActorResources> Claim(std::size_t slot) override {
    if (actor_ == nullptr) {
      return std::nullopt;
    }
    return ClaimLiveActorRemoval(slot, actor_);
  }
  void Remember(std::size_t slot, int hidden) override {
    StoreLiveRoomActorState(static_cast<int>(slot), hidden);
  }
  [[nodiscard]] bool CancelAssets(std::size_t slot) override {
    return CancelLiveActorAssets(slot, actor_);
  }
  void FinishAssets(std::size_t slot) override {
    FinishLiveActorAssets(slot, actor_);
  }
  void ReleaseGimmick(std::size_t slot) override {
    if (!components_ || components_->gimmick == nullptr) return;
    auto* expected = components_->gimmick;
    if (VS::Field::gimmickInstances[slot] == expected) {
      static_cast<void>(VS::Field::DropLiveUnsupportedGimmickStack(
          static_cast<std::int32_t>(slot)));
    }
    ReleaseLiveActorRecord(identity_, ActorRecordPart::kGimmick);
  }
  void ReleaseModel(std::size_t model_slot) override {
    if (model_slot == slot_) {
      ReleaseLiveActorRecord(identity_, ActorRecordPart::kBody);
      ReleaseLiveActorRecord(identity_, ActorRecordPart::kWeapon);
      ReleaseLiveActorRecord(identity_, ActorRecordPart::kShield);
    } else if (model_slot == slot_ + kSecondBodySlotOffset) {
      ReleaseLiveActorRecord(identity_, ActorRecordPart::kSecondBody);
    } else {
      WarnActorRefused("ReleaseModel", "model slot does not belong to actor");
    }
  }
  void FreeAiState(std::size_t /*slot*/) override {
    VS::Battle::Ai::FreeLiveActorAiState(actor_);
  }
  [[nodiscard]] RosterView Links(std::size_t /*slot*/) const override {
    return ViewFrom(identity_);
  }
  void Bypass(std::size_t predecessor, std::size_t /*slot*/) override {
    VS::Battle::actors[predecessor]->next = actor_->next;
  }
  void Free(std::size_t /*slot*/) override {
    FinalizeLiveActorRemoval(identity_);
  }

 private:
  VS::Battle::Actor* actor_;
  SpawnedActor identity_;
  std::optional<ActorInstanceComponents> components_;
  std::size_t slot_;
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

// The original 24-byte copy overread the 20-byte placeholder into the next
// table. Its fifth byte terminates the displayed name; native padding is zero.
// The adjacent original bytes are preserved only by the offline learning
// import.
std::array<std::uint8_t, kActorNameBytes> PlaceholderName() {
  constexpr std::size_t kNameBytes = sizeof VS::Actors::placeholderActorName;
  static_assert(kNameBytes < kActorNameBytes);
  std::array<std::uint8_t, kActorNameBytes> name{};
  const std::span<std::uint8_t> bytes(name);
  std::memcpy(bytes.data(), VS::Actors::placeholderActorName.data(),
              kNameBytes);
  return name;
}
}  // namespace

bool LinkLiveActorAfterLeader(SpawnedActor identity) {
  auto* actor = ResolveLiveActorInstance(identity);
  auto* leader = ResolveLiveActorInstance(LiveActorIdentityAt(0));
  if (actor == nullptr || leader == nullptr) {
    WarnActorRefused("LinkLiveActorAfterLeader", "actor or leader is absent");
    return false;
  }
  const auto decision = LinkActorAfterAshley(
      ViewFrom(identity), {.linkable_kind = true, .leader = actor == leader});
  if (decision.edit != RosterEdit::kInsertAfterLeader) return false;
  actor->next = leader->next;
  leader->next = identity;
  return true;
}

int LinkLiveActorAfterAshley(VS::Battle::Actor* actor) {
  constexpr const char* kOperation = "LinkLiveActorAfterAshley";
  if (actor == nullptr) {
    WarnActorRefused(kOperation, "no actor");
    return 0;
  }
  const auto identity = LiveActorIdentity(actor);
  if (identity == SpawnedActor::kNone) {
    WarnActorRefused(kOperation, "actor has no current instance identity");
    return 0;
  }
  const RosterDecision decision =
      LinkActorAfterAshley(ViewFrom(identity), CandidateOf(*actor));
  if (decision.edit == RosterEdit::kRefusedNoLeader) {
    WarnActorRefused(kOperation, "Ashley's actor slot is empty");
    return 0;
  }
  if (decision.edit != RosterEdit::kInsertAfterLeader) {
    return 0;
  }
  return LinkLiveActorAfterLeader(identity) ? 1 : 0;
}

int UnlinkLiveActorFromRoomList(VS::Battle::Actor* actor) {
  if (actor == nullptr) {
    WarnActorRefused("UnlinkLiveActorFromRoomList", "no actor");
    return 0;
  }
  const auto identity = LiveActorIdentity(actor);
  if (identity == SpawnedActor::kNone) {
    WarnActorRefused("UnlinkLiveActorFromRoomList",
                     "actor has no current instance identity");
    return 0;
  }
  const RosterDecision decision =
      UnlinkActorFromRoomList(ViewFrom(identity), CandidateOf(*actor));
  if (decision.edit != RosterEdit::kBypass) {
    return 0;
  }
  VS::Battle::actors[decision.slot]->next = actor->next;
  actor->next = SpawnedActor::kNone;
  return 1;
}

int RemoveLiveActorAtSlot(unsigned index, int hidden) {
  if (CancelLivePendingScriptedActorSpawn(index)) {
    return 1;
  }
  LiveRemovalHost host(
      index < kRosterSlots ? VS::Battle::actors[index] : nullptr, index);
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
    WarnActorRefused(kOperation, "the actor id is outside VS::Battle::actors");
    return;
  }
  VS::Battle::Actor* actor = VS::Battle::actors[actor_id];
  if (actor == nullptr || actor->stats == nullptr) {
    WarnActorRefused(kOperation, "the actor slot has no actor stats");
    return;
  }
  const std::array<std::uint8_t, kActorNameBytes> name = PlaceholderName();
  const PlaceholderActorStats placeholder =
      BuildPlaceholderActorStats(auxiliary, name);
  VS::Battle::ActorStats* stats = actor->stats;
  std::memset(stats, 0, sizeof *stats);
  stats->max_hp = placeholder.max_hp;
  stats->current_hp = placeholder.current_hp;
  std::memcpy(stats->name.data(), placeholder.name.data(),
              placeholder.name.size());
  stats->cancels_hit_results = placeholder.cancels_hit_results ? 1 : 0;
  if (placeholder.marker_flags.has_value()) {
    VS::Battle::SetLiveActorMarker(actor_id, *placeholder.marker_flags);
  }
  actor->room_spawn_index = placeholder.room_spawn_index;
  actor->kill_counted = placeholder.kill_counted ? 1 : 0;
  actor->defeated = placeholder.defeated ? 1 : 0;
  stats->actor_id = placeholder.actor_id;
}
}  // namespace VS::Actors
