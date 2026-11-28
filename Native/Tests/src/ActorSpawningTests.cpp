#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ActorInstances.hpp"
#include "Actors/ActorLoads.hpp"
#include "Actors/ActorSpawning.hpp"
#include "Actors/ActorSpawningLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/CharacterInit.hpp"
#include "Actors/RoomActorCache.hpp"
#include "Battle/AbilityState.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Content/ActorAssets.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Content/CdDrive.hpp"
#include "Field/Mpd.hpp"
#include "Legacy/Psx.hpp"
#include "TestSupport/ChannelWarnings.hpp"

namespace {
namespace actors = VS::Actors;
namespace content = VS::Content;
using actors::ActorDefinition;
using actors::ActorResources;
using actors::ActorShape;
using actors::EquipmentHand;
using actors::SpawnedActor;
using actors::SpawnRefusal;
using actors::ZudReadState;
using content::ActorAssetKind;
using content::ActorAssetRequest;
using content::AssetAddress;
using VS::test_support::ChannelWarnings;
using VS::test_support::Refused;

constexpr std::uintptr_t kFirstHandle = 0x1000;
constexpr std::uintptr_t kHandleStride = 0x100;
constexpr std::uintptr_t kZudBase = 0x80000;

SpawnedActor Handle(std::uintptr_t value) {
  // A handle names any allocation, not only kNone.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<SpawnedActor>(value);
}

AssetAddress At(std::uintptr_t value) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<AssetAddress>(value);
}

std::uintptr_t Raw(SpawnedActor actor) {
  return static_cast<std::uintptr_t>(actor);
}

std::string Opt(std::optional<std::size_t> value) {
  return value ? std::to_string(*value) : std::string("-");
}

std::uint8_t Kind(ActorAssetKind kind) {
  return static_cast<std::uint8_t>(kind);
}

// Catch2's REQUIRE throws, but clang-tidy cannot see that; the explicit throw
// lets bugprone-unchecked-optional-access prove the accesses through it.
template <typename T>
T& Required(std::optional<T>& value) {
  REQUIRE(value.has_value());
  if (!value) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *value;
}

template <typename T>
const T& Required(const std::optional<T>& value) {
  REQUIRE(value.has_value());
  if (!value) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *value;
}

constexpr ActorDefinition kEquippedActor{.shape = ActorShape::kEquipped};
constexpr ActorDefinition kGimmickActor{.shape = ActorShape::kAuxiliary};
constexpr ActorDefinition kTwoBodyActor{.shape = ActorShape::kSecondaryBody};

TEST_CASE("Actor handles validate and retire published identities",
          "[actors][actors-spawning]") {
  actors::ActorInstanceTable handles;
  const SpawnedActor actor = handles.Begin(3, 0x1000, kEquippedActor);

  CHECK(actor != SpawnedActor::kNone);
  CHECK(handles.Resolve(actor) == 0x1000);
  CHECK(handles.Publish(3, actor));
  CHECK(handles.Observe(3, 0x1000) == actor);
  CHECK(handles.Remove(3, actor));
  CHECK(handles.Resolve(actor) == 0);
}

TEST_CASE("Reused actor slots issue a new generation for the same address",
          "[actors][actors-spawning]") {
  actors::ActorInstanceTable handles;
  const SpawnedActor first = handles.Begin(3, 0x1000, kEquippedActor);
  REQUIRE(handles.Publish(3, first));
  REQUIRE(handles.Remove(3, first));

  const SpawnedActor replacement = handles.Begin(3, 0x1000, kEquippedActor);
  CHECK(replacement != SpawnedActor::kNone);
  CHECK(replacement != first);
  CHECK(handles.Resolve(first) == 0);
  CHECK(handles.Resolve(replacement) == 0x1000);
  CHECK(handles.Publish(3, replacement));
  CHECK_FALSE(handles.Remove(3, first));
}

TEST_CASE("Discarding an unpublished actor invalidates its handle",
          "[actors][actors-spawning]") {
  actors::ActorInstanceTable handles;
  const SpawnedActor actor = handles.Begin(7, 0x2000, kEquippedActor);

  CHECK(actor != SpawnedActor::kNone);
  CHECK_FALSE(handles.Publish(6, actor));
  CHECK_FALSE(handles.Remove(7, actor));
  CHECK(handles.Discard(actor));
  CHECK(handles.Resolve(actor) == 0);
  CHECK_FALSE(handles.Definition(actor).has_value());
  CHECK_FALSE(handles.Discard(actor));
}

TEST_CASE("Resetting the actor table invalidates published handles",
          "[actors][actors-spawning]") {
  actors::ActorInstanceTable handles;
  const SpawnedActor actor = handles.Begin(4, 0x3000, kEquippedActor);
  REQUIRE(handles.Publish(4, actor));

  handles.Reset();

  CHECK(handles.Resolve(actor) == 0);
  const SpawnedActor replacement = handles.Observe(4, 0x3000);
  CHECK(replacement != SpawnedActor::kNone);
  CHECK(replacement != actor);
  CHECK(handles.Resolve(replacement) == 0x3000);
  // Reset also forgets the definition: an address seen again afterwards is an
  // instance the table did not allocate.
  CHECK_FALSE(handles.Definition(replacement).has_value());
}

TEST_CASE("Actor definitions fix the slot resources an actor owns",
          "[actors][actors-spawning]") {
  CHECK(kGimmickActor.Resources() == ActorResources{.gimmick = true});
  CHECK(kEquippedActor.Resources() == ActorResources{.model = true});
  CHECK(kTwoBodyActor.Resources() ==
        ActorResources{.model = true, .second_model = true});
}

TEST_CASE("An actor keeps its definition until another address takes its slot",
          "[actors][actors-spawning]") {
  actors::ActorInstanceTable handles;
  const SpawnedActor gimmick = handles.Begin(5, 0x4000, kGimmickActor);
  CHECK(handles.Definition(gimmick) == kGimmickActor);
  REQUIRE(handles.Publish(5, gimmick));
  CHECK(handles.Observe(5, 0x4000) == gimmick);
  CHECK(handles.Definition(gimmick) == kGimmickActor);
  // An address the table did not allocate has no definition.
  const SpawnedActor stranger = handles.Observe(5, 0x5000);
  CHECK(stranger != gimmick);
  CHECK_FALSE(handles.Definition(stranger).has_value());
  CHECK_FALSE(handles.Definition(gimmick).has_value());
}

TEST_CASE("Removal claims an actor's resources exactly once",
          "[actors][actors-spawning]") {
  actors::ActorInstanceTable handles;
  const SpawnedActor actor = handles.Begin(6, 0x6000, kTwoBodyActor);

  // An unpublished allocation (a failed creation) has nothing to remove.
  CHECK_FALSE(handles.ClaimRemoval(6, actor).has_value());
  REQUIRE(handles.Publish(6, actor));
  CHECK_FALSE(handles.ClaimRemoval(5, actor).has_value());
  CHECK(handles.ClaimRemoval(6, actor) ==
        ActorResources{.model = true, .second_model = true});
  // A second removal of the same instance, even before it is retired, claims
  // nothing, so nothing is released twice.
  CHECK_FALSE(handles.ClaimRemoval(6, actor).has_value());
  CHECK(handles.Remove(6, actor));
  CHECK_FALSE(handles.ClaimRemoval(6, actor).has_value());
}

TEST_CASE("A slot's replacement actor claims only its own resources",
          "[actors][actors-spawning]") {
  actors::ActorInstanceTable handles;
  const SpawnedActor first = handles.Begin(6, 0x6000, kTwoBodyActor);
  REQUIRE(handles.Publish(6, first));
  REQUIRE(handles.Remove(6, first));

  const SpawnedActor replacement = handles.Begin(6, 0x6000, kGimmickActor);
  REQUIRE(handles.Publish(6, replacement));
  CHECK_FALSE(handles.ClaimRemoval(6, first).has_value());
  CHECK(handles.ClaimRemoval(6, replacement) ==
        ActorResources{.gimmick = true});
}

TEST_CASE("An actor without a definition claims no resources, once",
          "[actors][actors-spawning]") {
  actors::ActorInstanceTable handles;
  const SpawnedActor stranger = handles.Observe(6, 0x7000);
  CHECK(handles.ClaimRemoval(6, stranger) == ActorResources{});
  CHECK_FALSE(handles.ClaimRemoval(6, stranger).has_value());
}

TEST_CASE("A pending allocation is found by its address until published",
          "[actors][actors-spawning]") {
  actors::ActorInstanceTable handles;
  CHECK(handles.Pending(0x1000) == SpawnedActor::kNone);
  const SpawnedActor actor = handles.Begin(3, 0x1000, kEquippedActor);
  CHECK(handles.Pending(0) == SpawnedActor::kNone);
  CHECK(handles.Pending(0x1000) == actor);
  REQUIRE(handles.Publish(3, actor));
  CHECK(handles.Pending(0x1000) == SpawnedActor::kNone);
  // The lookup did not install or re-key anything.
  CHECK(handles.Observe(3, 0x1000) == actor);
}

using actors::ActorRecordPart;
using content::ActorRecordTable;

TEST_CASE("An equipped actor's owned records bind only into its own parts",
          "[actors][actors-spawning][asset-ownership]") {
  // #48: an owned request names no memory; the actor its owner names
  // provides it, so a record of another slot binds nowhere.
  actors::ActorInstanceTable handles;
  const SpawnedActor actor = handles.Begin(3, 0x1000, kEquippedActor);
  CHECK(actors::OwnedActorRecordPart(handles, actor, ActorRecordTable::kActor,
                                     3) == ActorRecordPart::kBody);
  CHECK(actors::OwnedActorRecordPart(handles, actor, ActorRecordTable::kWeapon,
                                     6) == ActorRecordPart::kWeapon);
  CHECK(actors::OwnedActorRecordPart(handles, actor, ActorRecordTable::kWeapon,
                                     7) == ActorRecordPart::kShield);
  // Slot 7 is a second body's record, which an equipped actor lacks.
  CHECK_FALSE(
      actors::OwnedActorRecordPart(handles, actor, ActorRecordTable::kActor, 7)
          .has_value());
  CHECK_FALSE(
      actors::OwnedActorRecordPart(handles, actor, ActorRecordTable::kWeapon, 8)
          .has_value());
  CHECK_FALSE(
      actors::OwnedActorRecordPart(handles, actor, ActorRecordTable::kActor, -1)
          .has_value());
  REQUIRE(handles.Publish(3, actor));
  REQUIRE(handles.Remove(3, actor));
  CHECK_FALSE(
      actors::OwnedActorRecordPart(handles, actor, ActorRecordTable::kActor, 3)
          .has_value());
}

TEST_CASE("Second bodies and gimmicks bind only their own record tables",
          "[actors][actors-spawning][asset-ownership]") {
  actors::ActorInstanceTable handles;
  const SpawnedActor two_body = handles.Begin(2, 0x2000, kTwoBodyActor);
  CHECK(actors::OwnedActorRecordPart(handles, two_body,
                                     ActorRecordTable::kActor,
                                     2) == ActorRecordPart::kBody);
  CHECK(actors::OwnedActorRecordPart(handles, two_body,
                                     ActorRecordTable::kActor,
                                     6) == ActorRecordPart::kSecondBody);
  CHECK_FALSE(actors::OwnedActorRecordPart(handles, two_body,
                                           ActorRecordTable::kWeapon, 4)
                  .has_value());
  const SpawnedActor gimmick = handles.Begin(5, 0x3000, kGimmickActor);
  CHECK(actors::OwnedActorRecordPart(handles, gimmick,
                                     ActorRecordTable::kGimmick,
                                     5) == ActorRecordPart::kGimmick);
  CHECK_FALSE(actors::OwnedActorRecordPart(handles, gimmick,
                                           ActorRecordTable::kActor, 5)
                  .has_value());
  const SpawnedActor stranger = handles.Observe(6, 0x4000);
  CHECK_FALSE(actors::OwnedActorRecordPart(handles, stranger,
                                           ActorRecordTable::kActor, 6)
                  .has_value());
  CHECK_FALSE(actors::OwnedActorRecordPart(handles, SpawnedActor::kNone,
                                           ActorRecordTable::kActor, 0)
                  .has_value());
}

using content::ActorAssetOutcome;
using content::ActorAssetTicket;
using SettledLoad = std::pair<SpawnedActor, ActorAssetOutcome>;

// An actor load host over a real ticket table, recording each settled load.
struct FakeLoadHost final : actors::ActorLoadHost {
  content::ActorAssetCompletions tickets;
  std::vector<SettledLoad> settled;

  content::ActorAssetCompletion Completion(ActorAssetTicket ticket) override {
    return tickets.Find(ticket);
  }
  void Forget(ActorAssetTicket ticket) override { tickets.Forget(ticket); }
  void Settled(SpawnedActor actor,
               const content::ActorAssetCompletion& completion) override {
    settled.emplace_back(actor, completion.outcome);
  }
  ActorAssetTicket Issue(SpawnedActor actor) {
    const std::optional<ActorAssetTicket> ticket =
        tickets.Issue({.value = Raw(actor)});
    return Required(ticket);
  }
};

// Tracks a load of `actor` that has ended with `outcome`, or is pending.
ActorAssetTicket TrackLoad(FakeLoadHost& host, actors::ActorLoadTable& loads,
                           SpawnedActor actor, ActorAssetOutcome outcome) {
  const ActorAssetTicket ticket = host.Issue(actor);
  REQUIRE(loads.Track(actor, ticket));
  if (outcome != ActorAssetOutcome::kPending) {
    REQUIRE(host.tickets.Complete(ticket, outcome, 0));
  }
  return ticket;
}

TEST_CASE("An actor load table reports each ended load once",
          "[actors][actors-spawning][asset-ownership]") {
  FakeLoadHost host;
  actors::ActorLoadTable loads;
  const SpawnedActor actor = Handle(kFirstHandle);
  TrackLoad(host, loads, actor, ActorAssetOutcome::kLoaded);
  TrackLoad(host, loads, actor, ActorAssetOutcome::kFailed);
  TrackLoad(host, loads, actor, ActorAssetOutcome::kPending);

  loads.Settle(host);
  CHECK(host.settled ==
        std::vector<SettledLoad>{{actor, ActorAssetOutcome::kLoaded},
                                 {actor, ActorAssetOutcome::kFailed}});
  CHECK(loads.Tracked(actor) == 1);
  loads.Settle(host);
  CHECK(host.settled.size() == 2);
}

TEST_CASE("Settled actor loads forget their tickets, pending ones do not",
          "[actors][actors-spawning][asset-ownership]") {
  FakeLoadHost host;
  actors::ActorLoadTable loads;
  const SpawnedActor actor = Handle(kFirstHandle);
  const ActorAssetTicket ended =
      TrackLoad(host, loads, actor, ActorAssetOutcome::kFailed);
  const ActorAssetTicket pending =
      TrackLoad(host, loads, actor, ActorAssetOutcome::kPending);
  loads.Settle(host);
  CHECK(host.tickets.Find(ended).outcome == ActorAssetOutcome::kUnknown);
  CHECK(host.tickets.Find(pending).outcome == ActorAssetOutcome::kPending);
}

TEST_CASE("Releasing an actor's loads settles only that actor's tickets",
          "[actors][actors-spawning][asset-ownership]") {
  FakeLoadHost host;
  actors::ActorLoadTable loads;
  const SpawnedActor first = Handle(kFirstHandle);
  const SpawnedActor second = Handle(kFirstHandle + kHandleStride);
  const ActorAssetTicket cancelled = host.Issue(first);
  const ActorAssetTicket other = host.Issue(second);
  REQUIRE(loads.Track(first, cancelled));
  REQUIRE(loads.Track(second, other));
  REQUIRE(host.tickets.Complete(cancelled, ActorAssetOutcome::kCancelled, -2));

  loads.Release(host, first);
  CHECK(host.settled ==
        std::vector<SettledLoad>{{first, ActorAssetOutcome::kCancelled}});
  CHECK(loads.Tracked(first) == 0);
  CHECK(host.tickets.Find(other).outcome == ActorAssetOutcome::kPending);
  // A load released before it ends is reported pending and forgotten.
  loads.Release(host, second);
  CHECK(host.settled.back() ==
        SettledLoad{second, ActorAssetOutcome::kPending});
  CHECK(host.tickets.Find(other).outcome == ActorAssetOutcome::kUnknown);
  CHECK(loads.Tracked(second) == 0);
}

TEST_CASE("An actor load table refuses no actor, no ticket and overflow",
          "[actors][actors-spawning][asset-ownership]") {
  FakeLoadHost host;
  actors::ActorLoadTable loads;
  const SpawnedActor actor = Handle(kFirstHandle);
  const ActorAssetTicket ticket = host.Issue(actor);
  CHECK_FALSE(loads.Track(SpawnedActor::kNone, ticket));
  CHECK_FALSE(loads.Track(actor, ActorAssetTicket::kNone));
  for (std::size_t i = 0; i < actors::ActorLoadTable::kCapacity; ++i) {
    REQUIRE(loads.Track(actor, host.Issue(actor)));
  }
  CHECK_FALSE(loads.Track(actor, ticket));
  CHECK(loads.Tracked(actor) == actors::ActorLoadTable::kCapacity);
}

// A recording host: the actor table, the asset queue, the zone's ZUD cache
// and Ashley's state, with every edit logged in call order.
struct FakeSpawnHost final : actors::ActorSpawnHost {
  struct OwnedRequest {
    SpawnedActor actor = SpawnedActor::kNone;
    content::ActorAssetTicket ticket = content::ActorAssetTicket::kNone;
  };

  std::vector<std::string> log;
  std::vector<SpawnRefusal> refusals;
  std::array<SpawnedActor, actors::kBattleActorSlots> table{};
  std::array<actors::SpawnedState, actors::kBattleActorSlots> states{};
  std::vector<ActorAssetRequest> requests;
  std::vector<OwnedRequest> owned_requests;
  std::vector<SpawnedActor> queued_owners;
  std::vector<content::ActorAssetTicket> forgotten_assets;
  std::vector<content::ActorAssetOutcome> forgotten_outcomes;
  std::vector<content::ActorAssetTicket> checked_assets;
  std::vector<SpawnedActor> discarded;
  std::vector<SpawnedActor> cancelled_owners;
  std::vector<int> released_actor_models;
  std::vector<int> released_weapon_models;
  bool refuse_allocation = false;
  bool refuse_install = false;
  bool allow_owned_cancel = true;
  std::uintptr_t next_handle = kFirstHandle;
  int queue_index = 4;
  int owned_submit_attempts = 0;
  int refuse_owned_submit_at = -1;
  int fail_owned_completion_at = -1;
  int pending_owned_completion_at = -1;
  content::ActorAssetCompletions owned_completions;
  int ai_result = 0;
  int roll = 10;
  int rolls = 0;
  int bone = -1;
  std::optional<actors::AuxiliaryIdentity> auxiliary;
  std::optional<actors::ActorDefaults> defaults;
  std::optional<actors::TemplateIdentity> identity;
  std::optional<actors::InitialStats> initial;
  std::optional<actors::AttackReach> reach;
  std::optional<actors::SpawnDetails> details;
  std::optional<actors::RememberedSpawn> restored;
  actors::WeaponRange weapon_range{
      .range = 3, .range_y = 4, .range_z = 5, .attack_shape_angle = 0x21};
  std::optional<actors::EquippedModels> equipped;
  int zone_enemies = 4;
  std::uint8_t enemy_model = 0x21;
  bool secondary_body = false;
  std::optional<actors::RememberedSpawn> remembered;
  std::optional<int> cached_zud;
  std::vector<std::byte> zud;
  bool refuse_zud = false;
  bool read_pending = false;
  std::vector<ZudReadState> reads{ZudReadState::kLoaded};
  mutable std::size_t polls = 0;
  bool busy = false;
  actors::AshleySnapshot snapshot;
  SpawnedActor character = SpawnedActor::kNone;
  actors::AshleyInventory inventory;
  actors::CharacterTemplate ashley;
  actors::WeaponRange ashley_range{
      .range = 6, .range_y = 7, .range_z = 8, .attack_shape_angle = 2};

  void Refuse(SpawnRefusal refusal) override { refusals.push_back(refusal); }
  void Diagnostic(int code) override {
    Log(std::format("Diagnostic {:#x}", code));
  }
  int RandomAveraged(int range) override {
    CHECK(range == actors::kStatRollRange);
    ++rolls;
    return roll;
  }

  SpawnedActor ActorAt(std::size_t slot) const override {
    return slot < table.size() ? table.at(slot) : SpawnedActor::kNone;
  }
  bool SlotIsEmpty(std::size_t slot) const override {
    return ActorAt(slot) == SpawnedActor::kNone;
  }
  SpawnedActor Allocate(ActorShape shape, std::size_t /*slot*/) override {
    Log(std::format("Allocate {}", static_cast<int>(shape)));
    if (refuse_allocation) {
      return SpawnedActor::kNone;
    }
    const SpawnedActor actor = Handle(next_handle);
    next_handle += kHandleStride;
    return actor;
  }
  void DiscardUninstalled(SpawnedActor actor) override {
    discarded.push_back(actor);
    Log("DiscardUninstalled");
  }
  void Install(std::size_t slot, SpawnedActor actor) override {
    Log(std::format("Install {}", slot));
    if (!refuse_install) {
      table.at(slot) = actor;
    }
  }
  bool InstallIfEmpty(std::size_t slot, SpawnedActor actor) override {
    Log(std::format("InstallIfEmpty {}", slot));
    if (refuse_install || table.at(slot) != SpawnedActor::kNone) {
      return false;
    }
    table.at(slot) = actor;
    return true;
  }
  void SetEquippedWeaponCategory(SpawnedActor /*actor*/,
                                 std::uint8_t category) override {
    Log(std::format("Category {}", category));
  }
  void WriteAuxiliaryIdentity(
      SpawnedActor /*actor*/,
      const actors::AuxiliaryIdentity& written) override {
    auxiliary = written;
    Log("AuxiliaryIdentity");
  }
  std::array<std::uint8_t, actors::kWeaponCategoryBounds> WeaponCategoryBounds()
      const override {
    return {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
  }

  void WriteDefaults(std::size_t slot,
                     const actors::ActorDefaults& written) override {
    defaults = written;
    states.at(slot) = {.kind = written.disposition.kind,
                       .weapon_drawn = written.disposition.weapon_drawn ? 1 : 0,
                       .walk_speed = actors::kDefaultWalkSpeed,
                       .run_speed = actors::kDefaultRunSpeed};
    Log(std::format("Defaults {}", slot));
  }
  void WriteTemplateIdentity(std::size_t slot,
                             const actors::TemplateIdentity& written) override {
    identity = written;
    states.at(slot).kind = written.disposition.kind;
    states.at(slot).weapon_drawn = written.disposition.weapon_drawn ? 1 : 0;
    Log(std::format("TemplateIdentity {}", slot));
  }
  void WriteInitialStats(std::size_t slot,
                         const actors::InitialStats& written) override {
    initial = written;
    states.at(slot).walk_speed = written.walk_speed;
    states.at(slot).run_speed = written.run_speed;
    Log(std::format("InitialStats {}", slot));
  }
  actors::WeaponRange BuildTemplateEquipment(
      std::size_t slot, actors::TemplateEquipment /*equipment*/) override {
    Log(std::format("BuildEquipment {}", slot));
    return weapon_range;
  }
  void WriteAttackReach(std::size_t slot,
                        const actors::AttackReach& written) override {
    reach = written;
    Log(std::format("AttackReach {}", slot));
  }
  actors::SpawnedState State(std::size_t slot) const override {
    return states.at(slot);
  }
  void WriteSpawnDetails(std::size_t slot,
                         const actors::SpawnDetails& written) override {
    details = written;
    Log(std::format("SpawnDetails {}", slot));
  }
  void SetTemplateWord(std::size_t slot, std::int32_t word) override {
    Log(std::format("TemplateWord {} {}", slot, word));
  }
  void RestoreRemembered(std::size_t slot,
                         const actors::RememberedSpawn& written) override {
    restored = written;
    Log(std::format("RestoreRemembered {}", slot));
  }
  void InflictStatuses(std::size_t slot, std::int32_t statuses) override {
    Log(std::format("InflictStatuses {} {:#x}", slot, statuses));
  }
  void LinkAfterLeader(std::size_t slot) override {
    Log(std::format("LinkAfterLeader {}", slot));
  }
  void ClearNext(std::size_t slot) override {
    Log(std::format("ClearNext {}", slot));
  }
  int FindBoneTag(std::size_t /*slot*/, int tag, int /*kind*/) const override {
    return bone < 0 ? -1 : bone + tag;
  }
  int BoneTag(std::size_t /*slot*/, int tagged, int /*kind*/) const override {
    return tagged + 0x40;
  }
  std::optional<actors::EquippedModels> Equipped(
      std::size_t /*slot*/) const override {
    return equipped;
  }

  void DrawWeapon(std::size_t slot, int mode) override {
    Log(std::format("DrawWeapon {} {}", slot, mode));
  }
  void SetDisplayFlags(std::size_t slot, std::uint32_t flags) override {
    Log(std::format("Display {} {:#x}", slot, flags));
  }
  void PlayAnimation(std::size_t slot, int animation) override {
    Log(std::format("Animation {} {}", slot, animation));
  }
  void SetWalkRunSpeeds(std::size_t slot, int walk, int run) override {
    Log(std::format("Speeds {} {:#x} {:#x}", slot, walk, run));
  }
  void SetCombatPose(std::size_t slot, int pose) override {
    Log(std::format("CombatPose {} {}", slot, pose));
  }
  int CreateAiState(std::size_t slot, int flags) override {
    Log(std::format("AiState {} {}", slot, flags));
    return ai_result;
  }
  void InitializePlaceholder(std::size_t slot, int actor_id) override {
    Log(std::format("Placeholder {} {}", slot, actor_id));
  }
  void QueueBaseSequence(SpawnedActor /*actor*/, std::size_t slot) override {
    Log(std::format("BaseSequence {}", slot));
  }
  void QueueWeaponSequence(SpawnedActor /*actor*/, std::size_t slot,
                           std::uint8_t category, int flags) override {
    Log(std::format("WeaponSequence {} {} {}", slot, category, flags));
  }

  int QueueAsset(const ActorAssetRequest& request) override {
    requests.push_back(request);
    busy = true;
    Log("QueueAsset");
    return queue_index;
  }
  std::optional<content::ActorAssetTicket> SubmitOwnedAsset(
      const ActorAssetRequest& request, SpawnedActor actor) override {
    const int attempt = owned_submit_attempts++;
    if (attempt == refuse_owned_submit_at) {
      return std::nullopt;
    }
    const content::ActorAssetOwner owner{.value =
                                             static_cast<std::uint64_t>(actor)};
    const std::optional<content::ActorAssetTicket> ticket =
        owned_completions.Issue(owner);
    if (!ticket) {
      return std::nullopt;
    }
    if (QueueAsset(request) < 0) {
      owned_completions.Forget(*ticket);
      return std::nullopt;
    }
    owned_requests.push_back({.actor = actor, .ticket = *ticket});
    return ticket;
  }
  int QueueOwnedAsset(const ActorAssetRequest& request,
                      SpawnedActor actor) override {
    Log("QueueOwnedAsset");
    if (queue_index < 0) {
      return queue_index;
    }
    const content::ActorAssetOwner owner{.value =
                                             static_cast<std::uint64_t>(actor)};
    const std::optional<content::ActorAssetTicket> ticket =
        owned_completions.Issue(owner);
    if (!ticket) {
      return -1;
    }
    requests.push_back(request);
    queued_owners.push_back(actor);
    owned_requests.push_back({.actor = actor, .ticket = *ticket});
    busy = true;
    return queue_index;
  }
  content::ActorAssetCompletion AssetCompletion(
      content::ActorAssetTicket ticket) override {
    checked_assets.push_back(ticket);
    return owned_completions.Find(ticket);
  }
  void ForgetAsset(content::ActorAssetTicket ticket) override {
    forgotten_assets.push_back(ticket);
    forgotten_outcomes.push_back(owned_completions.Find(ticket).outcome);
    owned_completions.Forget(ticket);
  }
  bool CancelOwnedAssets(SpawnedActor actor) override {
    if (!allow_owned_cancel) {
      return false;
    }
    cancelled_owners.push_back(actor);
    for (const OwnedRequest& request : owned_requests) {
      if (request.actor == actor) {
        static_cast<void>(owned_completions.Complete(
            request.ticket, content::ActorAssetOutcome::kCancelled,
            content::kAssetRefused));
      }
    }
    busy = false;
    return true;
  }
  bool AssetQueueIdle() const override { return !busy; }
  void UpdateAssetQueue() override {
    busy = false;
    for (std::size_t i = 0; i < owned_requests.size(); ++i) {
      if (static_cast<int>(i) == pending_owned_completion_at) continue;
      const content::ActorAssetOutcome outcome =
          static_cast<int>(i) == fail_owned_completion_at
              ? content::ActorAssetOutcome::kFailed
              : content::ActorAssetOutcome::kLoaded;
      static_cast<void>(owned_completions.Complete(
          owned_requests.at(i).ticket, outcome,
          outcome == content::ActorAssetOutcome::kLoaded
              ? content::kAssetDone
              : content::kAssetRefused));
    }
    Log("UpdateQueue");
  }
  void PumpLoading() override { Log("Pump"); }
  void ReleaseActorModel(int slot) override {
    released_actor_models.push_back(slot);
    Log(std::format("ReleaseActorModel {}", slot));
  }
  void ReleaseWeaponModel(int slot) override {
    released_weapon_models.push_back(slot);
    Log(std::format("Release {}", slot));
  }

  bool UsesSecondaryBodyModel(std::uint8_t /*model_id*/) const override {
    return secondary_body;
  }
  int ZoneEnemyCount() const override { return zone_enemies; }
  std::uint8_t ZoneEnemyModel(int /*zud*/) const override {
    return enemy_model;
  }
  std::optional<actors::RememberedSpawn> Remembered(
      std::uint8_t /*spawn_index*/) const override {
    return remembered;
  }
  std::optional<int> CachedZud() const override { return cached_zud; }
  actors::ZudBlock CachedZudBlock() const override {
    return {.address = At(kZudBase), .bytes = zud};
  }
  void FreeZud() override {
    cached_zud.reset();
    Log("FreeZud");
  }
  content::DiscFileExtent ZudFile(int index) const override {
    return {.sector = 100 + index, .bytes = static_cast<int>(zud.size())};
  }
  bool ZudReadPending() const override { return read_pending; }
  bool AllocateZud(content::DiscFileExtent file) override {
    Log(std::format("AllocateZud {}", file.bytes));
    return !refuse_zud;
  }
  void BeginZudRead(content::DiscFileExtent file) override {
    Log(std::format("BeginZudRead {}", file.sector));
  }
  ZudReadState PollZudRead() const override {
    const ZudReadState state = reads.at(std::min(polls, reads.size() - 1));
    ++polls;
    return state;
  }
  void FinishZudRead(int index, bool loaded) override {
    Log(std::format("FinishZudRead {} {}", index, loaded));
    if (loaded) {
      cached_zud = index;
    } else {
      cached_zud.reset();
    }
  }

  actors::AshleySnapshot ReadAshleySnapshot() const override {
    return snapshot;
  }
  SpawnedActor Character() const override { return character; }
  void SetCharacter(SpawnedActor actor) override {
    character = actor;
    Log("SetCharacter");
  }
  void RestoreAshleyVitals(const actors::AshleySnapshot& /*saved*/) override {
    Log("RestoreVitals");
  }
  void InflictCharacterStatuses(std::int32_t statuses) override {
    Log(std::format("CharacterStatuses {:#x}", statuses));
  }
  void RestoreAshleyConditions(
      const actors::AshleySnapshot& /*saved*/) override {
    Log("RestoreConditions");
  }
  void SetCharacterWeaponDrawn(std::int32_t weapon_drawn) override {
    Log(std::format("CharacterDrawn {}", weapon_drawn));
  }
  void SetHudBattleMode(int mode) override {
    Log(std::format("HudMode {}", mode));
  }
  void ClearAshleySnapshotPending() override {
    snapshot.pending = false;
    Log("ClearPending");
  }
  void RestoreCamera(std::int32_t pitch, std::int32_t distance) override {
    Log(std::format("Camera {} {}", pitch, distance));
  }
  actors::AshleyInventory ReadAshleyInventory() const override {
    return inventory;
  }
  actors::CharacterTemplate AshleyTemplate() const override { return ashley; }
  actors::WeaponRange LoadAshleyWeapon(
      std::optional<std::size_t> weapon) override {
    Log("AshleyWeapon " + Opt(weapon));
    return ashley_range;
  }
  void LoadAshleyShield(std::optional<std::size_t> shield) override {
    Log("AshleyShield " + Opt(shield));
  }
  void LoadAshleyArmor(std::size_t limb,
                       std::optional<std::size_t> armor) override {
    Log(std::format("AshleyArmor {} ", limb) + Opt(armor));
  }
  void LoadAshleyAccessory(std::optional<std::size_t> armor) override {
    Log("AshleyAccessory " + Opt(armor));
  }

  void Log(std::string entry) { log.push_back(std::move(entry)); }
};

void CompleteOwnedScriptedActorLoad(FakeSpawnHost& host,
                                    actors::ScriptedActorSpawnTable& pending) {
  host.UpdateAssetQueue();
  const content::ActorAssetTicket ticket = host.owned_requests.at(0).ticket;
  actors::CompleteScriptedActorSpawn(host, pending, Handle(kFirstHandle),
                                     host.AssetCompletion(ticket).outcome);
}

void PutWord(std::vector<std::byte>& file, std::size_t at, std::int32_t word) {
  const auto bits = static_cast<std::uint32_t>(word);
  for (std::size_t i = 0; i < sizeof bits; ++i) {
    file.at(at + i) = static_cast<std::byte>((bits >> (8 * i)) & 0xFF);
  }
}

// A ZUD file of `bytes` bytes with the shape, weapon, shield, common and
// battle animation sections.
std::vector<std::byte> MakeZud(std::size_t bytes,
                               const std::array<actors::ZudSection, 5>& parts) {
  std::vector<std::byte> file(bytes);
  std::size_t at = 8;
  for (const actors::ZudSection& part : parts) {
    PutWord(file, at, part.offset);
    PutWord(file, at + 4, part.size);
    at += 8;
  }
  return file;
}

std::vector<std::byte> WellFormedZud() {
  return MakeZud(
      0x80,
      {{{0x30, 0x10}, {0x40, 0x10}, {0x50, 0x10}, {0x60, 0x10}, {0x70, 0x10}}});
}

actors::CharacterTemplate EnemyTemplate() {
  actors::CharacterTemplate character;
  character.sub_type = 0x0102;
  character.model_id = 0x30;
  character.reach = 0x23;
  character.hp = 120;
  character.strength = 30;
  character.intelligence = 31;
  character.agility = 32;
  character.material = 3;
  character.walk_speed = 10;
  character.run_speed = 20;
  character.blade_model = 5;
  character.blade_category = 4;
  character.weapon_material = 2;
  character.shield_model = 6;
  character.shield_material = 1;
  character.zud_index = 2;
  character.ai_settings = 0x1234;
  return character;
}

actors::RoomSpawn EnemySpawn() {
  actors::RoomSpawn spawn;
  spawn.index = 7;
  spawn.placement = {.tile_x = 5, .surface = 3, .tile_z = 6, .facing = 2};
  spawn.settings = {0xAB, 0xCD, 0xEF, 0x12, 0x78, 0x56, 0x34, 0x92};
  spawn.drop = {.id = 0x101, .count = 1};
  spawn.second_drop = {.id = 0x102, .count = 2};
  spawn.random_drop = {.id = 0x103, .count = 30};
  spawn.facing = 9;
  spawn.variant = 3;
  spawn.combat_pose = 4;
  return spawn;
}

// ZUD file 2 is cached and Ashley's actor is in slot 0.
void PrepareRoom(FakeSpawnHost& host) {
  host.table.at(actors::kAshleySlot) = Handle(0x10);
  host.cached_zud = 2;
  host.zud = WellFormedZud();
}

// The table is as before the call and nothing was queued or installed.
void CheckNothingSpawned(
    const FakeSpawnHost& host,
    const std::array<SpawnedActor, actors::kBattleActorSlots>& table) {
  CHECK(host.table == table);
  CHECK(host.requests.empty());
  CHECK(std::ranges::none_of(host.log, [](const std::string& entry) {
    return entry.starts_with("Install");
  }));
}
}  // namespace

TEST_CASE("Spawn dispositions, gaits and weapon categories follow the original",
          "[actors][actors-spawning]") {
  using actors::SpawnDisposition;
  struct DispositionCase {
    std::size_t slot;
    int flags;
    SpawnDisposition expected;
  };
  const std::array<DispositionCase, 7> dispositions{{
      {.slot = 0, .flags = 1, .expected = {.kind = 1}},
      {.slot = 1, .flags = 1, .expected = {.kind = 2}},
      {.slot = 5, .flags = 1, .expected = {.kind = 4, .weapon_drawn = true}},
      {.slot = 5, .flags = 5, .expected = {.kind = 4, .weapon_drawn = true}},
      {.slot = 5, .flags = 2, .expected = {.kind = 8}},
      {.slot = 5, .flags = 3, .expected = {.kind = 8}},
      {.slot = 5, .flags = 0x1C, .expected = {.kind = 2}},
  }};
  for (const DispositionCase& entry : dispositions) {
    CAPTURE(entry.slot, entry.flags);
    CHECK(actors::DispositionFor(entry.slot, entry.flags) == entry.expected);
  }

  struct GaitCase {
    int model;
    actors::MovementGait expected;
  };
  const std::array<GaitCase, 3> gaits{{
      {.model = 41, .expected = {.movement_class = 16, .gait_class = 32}},
      {.model = 48, .expected = {.movement_class = 2, .gait_class = 8}},
      {.model = 7, .expected = {.movement_class = 2, .gait_class = 4}},
  }};
  for (const GaitCase& entry : gaits) {
    CAPTURE(entry.model);
    CHECK(actors::DefaultMovementGait(entry.model) == entry.expected);
  }

  const std::array<std::uint8_t, actors::kWeaponCategoryBounds> bounds{
      0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
  const std::array<std::pair<int, std::uint8_t>, 5> categories{{
      {0, 10},
      {5, 1},
      {10, 1},
      {11, 2},
      {101, 11},
  }};
  for (const auto& [blade, category] : categories) {
    CAPTURE(blade);
    CHECK(actors::EquippedWeaponCategory(blade, bounds) == category);
  }
}

TEST_CASE("Spawn stat, speed and reach rules follow the original",
          "[actors][actors-spawning]") {
  // The original's short sum wrapped; the int sum clamps.
  const std::array<std::pair<int, std::int16_t>, 4> clamps{{
      {-5, 0},
      {500, 500},
      {1200, 999},
      {0x7FFF + 10, 999},
  }};
  for (const auto& [value, clamped] : clamps) {
    CAPTURE(value);
    CHECK(actors::ClampInitialStat(value) == clamped);
  }
  CHECK(actors::ScaledSpeed(18) == 16);
  CHECK(actors::ScaledSpeed(255) == 229);
  CHECK(actors::InitialStatuses(-1) == static_cast<std::int32_t>(0x80000000U));
  CHECK(actors::InitialStatuses(5) == 0);

  CHECK(
      actors::AttackReachFor(
          {.range = 3, .range_y = 4, .range_z = 5, .attack_shape_angle = 0x3F},
          0xE2) ==
      actors::AttackReach{
          .range = 5, .range_y = 6, .range_z = 7, .shape_angle = 0x1F});
}

TEST_CASE("Gimmick texture slots and room enemy placements follow the original",
          "[actors][actors-spawning]") {
  const std::array<std::pair<int, std::uint8_t>, 3> slots{{
      {0x1234, 0x12},
      {-0x100, 0xFF},
      {-0xFF, 0},
  }};
  for (const auto& [actor_id, slot] : slots) {
    CAPTURE(actor_id);
    CHECK(actors::AuxiliaryTextureSlot(actor_id) == slot);
  }

  actors::RoomSpawn spawn;
  spawn.placement = {.tile_x = 1, .surface = 3, .tile_z = 2, .facing = 5};
  CHECK(actors::RoomEnemyPlacement(spawn, std::nullopt) ==
        actors::SpawnPlacement{
            .tile_x = 1, .surface = 1, .tile_z = 2, .facing = 0x40});
  actors::RememberedSpawn remembered;
  remembered.placement = {
      .tile_x = 9, .surface = 0xFE, .tile_z = 8, .facing = 77};
  CHECK(actors::RoomEnemyPlacement(spawn, remembered) ==
        actors::SpawnPlacement{
            .tile_x = 9, .surface = 0, .tile_z = 8, .facing = 77});
  CHECK(actors::PackPlacement(
            {.tile_x = 1, .surface = 2, .tile_z = 3, .facing = 4}) ==
        0x04030201U);
}

TEST_CASE("Spawn settings and ZUD headers decode within their bytes",
          "[actors][actors-spawning]") {
  const std::array<std::uint8_t, actors::kSpawnSettingsBytes> bytes{
      0xAB, 0xCD, 0xEF, 0x12, 0x78, 0x56, 0x34, 0x92};
  CHECK(actors::DecodeSpawnSettings(bytes) ==
        actors::SpawnSettings{.ai_group = 0xB,
                              .group_reserved = 0x1A,
                              .ai_linked_group = 0x6,
                              .linked_reserved = 0x4BBF,
                              .extra = -1842063752});

  const std::vector<std::byte> short_file(actors::kZudHeaderBytes - 1);
  CHECK_FALSE(actors::ParseZudHeader(short_file));
  const std::vector<std::byte> file = WellFormedZud();
  const std::optional<actors::ZudLayout> parsed = actors::ParseZudHeader(file);
  CHECK(Required(parsed) ==
        actors::ZudLayout{.shape = {0x30, 0x10},
                          .weapon = {0x40, 0x10},
                          .shield = {0x50, 0x10},
                          .common_animation = {0x60, 0x10},
                          .battle_animation = {0x70, 0x10}});

  struct FitCase {
    actors::ZudSection section;
    bool fits;
  };
  const std::array<FitCase, 6> fits{{
      {.section = {0, 0x30}, .fits = true},
      {.section = {0x30, 0}, .fits = true},
      {.section = {0x10, 0x21}, .fits = false},
      {.section = {0x31, 0}, .fits = false},
      {.section = {-1, 0}, .fits = false},
      {.section = {0, -1}, .fits = false},
  }};
  for (const FitCase& entry : fits) {
    CAPTURE(entry.section.offset, entry.section.size);
    CHECK(actors::ZudSectionFits(entry.section, 0x30) == entry.fits);
  }
}

TEST_CASE("Ashley spawns with her last equipped weapon and shield",
          "[actors][actors-spawning]") {
  actors::AshleyInventory inventory;
  inventory.weapons.at(2) = {.equipped = true, .blade = 3};
  inventory.weapons.at(5) = {.equipped = true, .blade = 1};
  inventory.blades.at(0) = {.model = 9, .material = 0x103};
  inventory.shields.at(1) = {.equipped = true, .model = 4, .material = 2};
  inventory.shields.at(4) = {.equipped = true, .model = 5, .material = 0x104};
  CHECK(actors::ChooseAshleyEquipment(inventory) ==
        actors::AshleyEquipment{.weapon = 5,
                                .blade_model = 9,
                                .blade_material = 0x103,
                                .shield = 4,
                                .shield_model = 5,
                                .shield_material = 0x104});

  inventory.weapons.at(5).blade = 16;
  inventory.blades.at(15) = {.model = 11, .material = 1};
  const std::optional<actors::AshleyEquipment> last_blade =
      actors::ChooseAshleyEquipment(inventory);
  CHECK(Required(last_blade).blade_model == 11);
  // #302: blade 0 read blades[-1]; past the blades read beyond them.
  inventory.weapons.at(5).blade = 0;
  CHECK_FALSE(actors::ChooseAshleyEquipment(inventory));
  inventory.weapons.at(5).blade = 17;
  CHECK_FALSE(actors::ChooseAshleyEquipment(inventory));

  actors::AshleyInventory unarmed;
  CHECK(actors::ChooseAshleyEquipment(unarmed) == actors::AshleyEquipment{});
}

TEST_CASE("A gimmick actor takes the first free slot and queues its model",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  host.table.at(2) = Handle(0x20);
  const SpawnedActor actor = actors::CreateAuxiliaryActor(
      host, actors::kAnyAuxiliarySlot, 0x1234,
      {.tile_x = 1, .surface = 0x53, .tile_z = 2, .facing = 3}, 7);
  REQUIRE(actor == Handle(kFirstHandle));
  CHECK(host.table.at(3) == actor);
  CHECK(host.auxiliary == actors::AuxiliaryIdentity{.id = 3});
  ActorAssetRequest expected{};
  expected.kind = Kind(ActorAssetKind::kGimmick);
  expected.target_slot = 3;
  expected.placement = actors::PackPlacement(
      {.tile_x = 1, .surface = 1, .tile_z = 2, .facing = 3});
  expected.sub_kind = 0x34;
  expected.texture_slot = 0x12;
  expected.material = 7;
  expected.variant = 5;
  CHECK(host.requests == std::vector<ActorAssetRequest>{expected});
  CHECK(host.log ==
        std::vector<std::string>{"Allocate 0", "AuxiliaryIdentity",
                                 "QueueAsset", "UpdateQueue", "Pump",
                                 "Install 3", "Placeholder 3 4660",
                                 "AiState 3 -1", "Display 3 0x1846"});
  CHECK(host.refusals.empty());
  CHECK(host.owned_requests.size() == 1);
  CHECK(host.forgotten_outcomes ==
        std::vector{content::ActorAssetOutcome::kLoaded});
}

TEST_CASE("A gimmick actor is refused without a slot or an allocation",
          "[actors][actors-spawning]") {
  FakeSpawnHost full;
  for (std::size_t slot = actors::kFirstAuxiliarySlot;
       slot < actors::kSpawnActorSlots; ++slot) {
    full.table.at(slot) = Handle(0x20 + slot);
  }
  const auto full_table = full.table;
  CHECK(actors::CreateAuxiliaryActor(full, actors::kAnyAuxiliarySlot, 1, {},
                                     0) == SpawnedActor::kNone);
  CHECK(full.log == std::vector<std::string>{"Diagnostic 0x86"});
  CheckNothingSpawned(full, full_table);

  FakeSpawnHost refused;
  refused.refuse_allocation = true;
  const auto empty_table = refused.table;
  CHECK(actors::CreateAuxiliaryActor(refused, 5, 1, {}, 0) ==
        SpawnedActor::kNone);
  CHECK(refused.refusals == std::vector{SpawnRefusal::kAllocation});
  CHECK(refused.log == std::vector<std::string>{"Allocate 0"});
  CheckNothingSpawned(refused, empty_table);
}

TEST_CASE("A gimmick actor is refused in an occupied slot or outside the table",
          "[actors][actors-spawning]") {
  FakeSpawnHost occupied;
  occupied.table.at(4) = Handle(0x20);
  CHECK(actors::CreateAuxiliaryActor(occupied, 4, 1, {}, 0) ==
        SpawnedActor::kNone);
  CHECK(occupied.refusals.empty());
  CHECK(occupied.log.empty());

  FakeSpawnHost outside;
  CHECK(actors::CreateAuxiliaryActor(outside, 42, 1, {}, 0) ==
        SpawnedActor::kNone);
  CHECK(outside.refusals == std::vector{SpawnRefusal::kActorSlot});
  CHECK(outside.log.empty());
}

TEST_CASE("Auxiliary actors require a loaded request before publication",
          "[actors][actors-spawning][auxiliary-ownership]") {
  for (const bool submission : {true, false}) {
    FakeSpawnHost host;
    if (submission) {
      host.refuse_owned_submit_at = 0;
    } else {
      host.fail_owned_completion_at = 0;
    }
    CHECK(actors::CreateAuxiliaryActor(host, 2, 1, {}, 0) ==
          SpawnedActor::kNone);
    CHECK(host.table.at(2) == SpawnedActor::kNone);
    CHECK(host.discarded == std::vector{Handle(kFirstHandle)});
    CHECK(host.cancelled_owners == std::vector{Handle(kFirstHandle)});
    CHECK(host.refusals == std::vector{SpawnRefusal::kAssetQueue});
    CHECK(std::ranges::none_of(host.log, [](const std::string& entry) {
      return entry.starts_with("Placeholder ") || entry.starts_with("AiState ");
    }));
  }
}

TEST_CASE("Auxiliary actors discard a loaded model if publication is refused",
          "[actors][actors-spawning][auxiliary-ownership]") {
  FakeSpawnHost host;
  host.refuse_install = true;
  CHECK(actors::CreateAuxiliaryActor(host, 2, 1, {}, 0) == SpawnedActor::kNone);
  CHECK(host.table.at(2) == SpawnedActor::kNone);
  CHECK(host.discarded == std::vector{Handle(kFirstHandle)});
  CHECK(host.forgotten_assets.size() == 1);
  CHECK(std::ranges::none_of(host.log, [](const std::string& entry) {
    return entry.starts_with("Placeholder ") || entry.starts_with("AiState ");
  }));
}

TEST_CASE("Auxiliary actors cancel pending tickets before forgetting them",
          "[actors][actors-spawning][auxiliary-ownership]") {
  FakeSpawnHost host;
  host.pending_owned_completion_at = 0;
  CHECK(actors::CreateAuxiliaryActor(host, 2, 1, {}, 0) == SpawnedActor::kNone);
  CHECK(host.table.at(2) == SpawnedActor::kNone);
  CHECK(host.discarded == std::vector{Handle(kFirstHandle)});
  CHECK(host.forgotten_outcomes ==
        std::vector{content::ActorAssetOutcome::kCancelled});
}

namespace {
constexpr actors::SpawnPlacement EquippedPlacement() {
  return {.tile_x = 4, .surface = 1, .tile_z = 5, .facing = 6};
}

// Actor 3 with model 0x22, blade 15 (material 2) and shield 6 (material 1).
SpawnedActor CreateEquippedActor(FakeSpawnHost& host) {
  host.table.at(actors::kAshleySlot) = Handle(0x10);
  return actors::CreateActorWithEquipment(host, 3, 0x22, 15, 2, 6, 1,
                                          EquippedPlacement(), 1);
}
}  // namespace

TEST_CASE("An equipped actor queues its models and joins the list after Ashley",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  const SpawnedActor actor = CreateEquippedActor(host);
  REQUIRE(actor == Handle(kFirstHandle));
  CHECK(host.table.at(3) == actor);
  CHECK(host.log == std::vector<std::string>{"Allocate 1",
                                             "Category 2",
                                             "QueueAsset",
                                             "BaseSequence 3",
                                             "Release 6",
                                             "QueueAsset",
                                             "Release 7",
                                             "QueueAsset",
                                             "WeaponSequence 3 2 1",
                                             "UpdateQueue",
                                             "Pump",
                                             "Animation 3 0",
                                             "Display 3 0x1847",
                                             "Install 3",
                                             "Defaults 3",
                                             "DrawWeapon 3 0",
                                             "Speeds 3 0x9000 0x12000",
                                             "CombatPose 3 0",
                                             "LinkAfterLeader 3",
                                             "Display 3 0x1846",
                                             "TemplateWord 3 0",
                                             "AiState 3 -1"});
  CHECK(host.defaults ==
        actors::ActorDefaults{.id = 3,
                              .model_id = 0x22,
                              .blade_id = 15,
                              .shield_id = 6,
                              .disposition = {.kind = 4, .weapon_drawn = true},
                              .interaction_flags = 0,
                              .gait = {.movement_class = 2, .gait_class = 4}});
}

TEST_CASE("An equipped actor's requests load its slots and it owns its parts",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  const SpawnedActor actor = CreateEquippedActor(host);
  REQUIRE(host.requests.size() == 3);
  ActorAssetRequest model{};
  model.kind = Kind(ActorAssetKind::kActorModelLoad);
  model.target_slot = 3;
  model.model_id = 0x22;
  model.placement = actors::PackPlacement(EquippedPlacement());
  model.sub_kind = actors::kDefaultSubKind;
  model.material = actors::kModelLoadMaterial;
  CHECK(host.requests.at(0) == model);
  ActorAssetRequest shield{};
  shield.kind = Kind(ActorAssetKind::kWeaponModelLoad);
  shield.target_slot = 7;
  shield.model_id = 6;
  shield.sub_kind = 3;
  shield.texture_slot = actors::kShieldTextureSlot;
  shield.material = 1;
  CHECK(host.requests.at(2) == shield);
  // Each initial model request is submitted under the allocated actor owner.
  REQUIRE(host.owned_requests.size() == 3);
  CHECK(std::ranges::all_of(
      host.owned_requests, [actor](const FakeSpawnHost::OwnedRequest& request) {
        return request.actor == actor;
      }));
}

TEST_CASE("Equipped actor stays unpublished when a model load fails",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  host.table.at(actors::kAshleySlot) = Handle(0x10);
  host.fail_owned_completion_at = 1;

  CHECK(CreateEquippedActor(host) == SpawnedActor::kNone);
  CHECK(host.table.at(3) == SpawnedActor::kNone);
  CHECK(host.discarded == std::vector{Handle(kFirstHandle)});
  CHECK(host.refusals == std::vector{SpawnRefusal::kAssetQueue});
  CHECK(host.forgotten_outcomes ==
        std::vector{content::ActorAssetOutcome::kLoaded,
                    content::ActorAssetOutcome::kFailed,
                    content::ActorAssetOutcome::kLoaded});
  CHECK(host.released_actor_models == std::vector{3});
  CHECK(host.released_weapon_models == std::vector{6, 7, 6, 7});
}

TEST_CASE("Equipped actor cleans up when a model request is refused",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  host.table.at(actors::kAshleySlot) = Handle(0x10);
  host.refuse_owned_submit_at = 1;

  CHECK(CreateEquippedActor(host) == SpawnedActor::kNone);
  CHECK(host.table.at(3) == SpawnedActor::kNone);
  CHECK(host.discarded == std::vector{Handle(kFirstHandle)});
  CHECK(host.refusals == std::vector{SpawnRefusal::kAssetQueue});
  CHECK(host.forgotten_outcomes ==
        std::vector{content::ActorAssetOutcome::kCancelled});
  CHECK(host.released_actor_models == std::vector{3});
  CHECK(host.released_weapon_models == std::vector{6, 6, 7});
}

TEST_CASE("Ashley's slot ends the list and a bare model queues no parts",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  const SpawnedActor actor = actors::CreateActorWithEquipment(
      host, 0, actors::kBareModel, 15, 2, 6, 1, {}, 0);
  REQUIRE(actor != SpawnedActor::kNone);
  CHECK(host.log == std::vector<std::string>{
                        "Allocate 1", "Category 2", "QueueAsset", "UpdateQueue",
                        "Pump", "Animation 0 0", "Display 0 0x1847",
                        "Install 0", "Defaults 0", "Speeds 0 0x9000 0x12000",
                        "CombatPose 0 0", "ClearNext 0", "Display 0 0x46",
                        "TemplateWord 0 0", "AiState 0 -1"});
  CHECK(host.requests.size() == 1);
  REQUIRE(host.owned_requests.size() == 1);
  CHECK(host.owned_requests.front().actor == actor);
}

TEST_CASE("An equipped actor is refused before anything is allocated",
          "[actors][actors-spawning]") {
  // #302: a listed actor linked after an empty slot 0.
  FakeSpawnHost leaderless;
  const auto table = leaderless.table;
  CHECK(actors::CreateActorWithEquipment(leaderless, 3, 1, 0, 0, 0, 0, {}, 1) ==
        SpawnedActor::kNone);
  CHECK(leaderless.refusals == std::vector{SpawnRefusal::kNoLeader});
  CHECK(leaderless.log.empty());
  CheckNothingSpawned(leaderless, table);

  FakeSpawnHost refused;
  refused.table.at(actors::kAshleySlot) = Handle(0x10);
  refused.refuse_allocation = true;
  const auto led_table = refused.table;
  CHECK(actors::CreateActorWithEquipment(refused, 4, 1, 0, 0, 0, 0, {}, 0) ==
        SpawnedActor::kNone);
  CHECK(refused.refusals == std::vector{SpawnRefusal::kAllocation});
  CHECK(refused.log == std::vector<std::string>{"Allocate 1"});
  CheckNothingSpawned(refused, led_table);
}

TEST_CASE("An equipped actor is refused outside the equipped slots",
          "[actors][actors-spawning]") {
  FakeSpawnHost beyond;
  CHECK(actors::CreateActorWithEquipment(beyond, 10, 1, 0, 0, 0, 0, {}, 0) ==
        SpawnedActor::kNone);
  CHECK(beyond.refusals.empty());
  CHECK(beyond.log.empty());

  FakeSpawnHost outside;
  CHECK(actors::CreateActorWithEquipment(outside, -1, 1, 0, 0, 0, 0, {}, 0) ==
        SpawnedActor::kNone);
  CHECK(outside.refusals == std::vector{SpawnRefusal::kActorSlot});
}

TEST_CASE("Registering a loaded actor checks its slot, actor and leader",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  actors::RegisterLoadedActor(host, Handle(0x20), 42, 1, 0, 0, 0);
  actors::RegisterLoadedActor(host, SpawnedActor::kNone, 3, 1, 0, 0, 0);
  actors::RegisterLoadedActor(host, Handle(0x20), 3, 1, 0, 0, 1);
  CHECK(host.refusals == std::vector{SpawnRefusal::kActorSlot,
                                     SpawnRefusal::kNoActor,
                                     SpawnRefusal::kNoLeader});
  CHECK(host.log.empty());

  actors::RegisterLoadedActor(host, Handle(0x20), 3, 1, 0, 0, 2);
  CHECK(host.table.at(3) == Handle(0x20));
  CHECK(host.states.at(3).kind == 8);
  CHECK(std::ranges::find(host.log, "LinkAfterLeader 3") == host.log.end());
}

TEST_CASE("Equipment model loads stay inside the weapon model slots",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  const SpawnedActor actor = Handle(0x2000);
  // #302: actor 10's weapon slot is 20, past the 20 weapon model slots.
  actors::QueueEquipmentModelLoad(host, actor, 10, 5, EquipmentHand::kWeapon,
                                  0);
  actors::QueueEquipmentModelLoad(host, actor, -1, 5, EquipmentHand::kShield,
                                  0);
  CHECK(host.refusals == std::vector{SpawnRefusal::kEquipmentSlot,
                                     SpawnRefusal::kEquipmentSlot});
  CHECK(host.log.empty());

  actors::QueueEquipmentModelLoad(host, actor, 4, actors::kUnchangedEquipment,
                                  EquipmentHand::kWeapon, 0);
  CHECK(host.log.empty());
  actors::QueueEquipmentModelLoad(host, actor, 4, 0, EquipmentHand::kWeapon, 0);
  CHECK(host.log == std::vector<std::string>{"Release 8"});
  CHECK(host.requests.empty());

  actors::QueueEquipmentModelLoad(host, actor, 9, 12, EquipmentHand::kShield,
                                  3);
  ActorAssetRequest expected{};
  expected.kind = Kind(ActorAssetKind::kWeaponModelLoad);
  expected.target_slot = 19;
  expected.model_id = 12;
  expected.sub_kind = 9;
  expected.texture_slot = actors::kShieldTextureSlot;
  expected.material = 3;
  CHECK(host.requests == std::vector<ActorAssetRequest>{expected});

  actors::QueueEquipmentModelLoad(host, SpawnedActor::kNone, 1, 2,
                                  EquipmentHand::kWeapon, 0);
  CHECK(host.refusals.back() == SpawnRefusal::kNoActor);
}

TEST_CASE("Model loads take the texture override from the spawn flags",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  const SpawnedActor actor = Handle(0x2000);
  CHECK(actors::QueueActorModelLoad(host, actor, 5, 0x31, {},
                                    0x80 | (3 << 8) | (0xB << 12)) ==
        host.queue_index);
  REQUIRE(host.requests.size() == 1);
  CHECK(host.requests.at(0).sub_kind == 3);
  CHECK(host.requests.at(0).texture_slot == 0xB);

  CHECK(actors::QueueActorModelLoad(host, SpawnedActor::kNone, 5, 1, {}, 0) ==
        -1);
  CHECK(host.refusals == std::vector{SpawnRefusal::kNoActor});
}

TEST_CASE(
    "Model loads without an override keep the default texture and material",
    "[actors][actors-spawning]") {
  FakeSpawnHost host;
  CHECK(actors::QueueActorModelLoad(host, Handle(0x2000), 5, 0x31, {}, 0x300) ==
        host.queue_index);
  REQUIRE(host.requests.size() == 1);
  CHECK(host.requests.at(0).sub_kind == actors::kDefaultSubKind);
  CHECK(host.requests.at(0).texture_slot == 0);
  CHECK(host.requests.at(0).material == actors::kModelLoadMaterial);
  CHECK(host.requests.at(0).variant == 0);
}

namespace {
// The five ZUD part requests EnemySpawn and EnemyTemplate queue into slot 3.
// Each request carries the fields of the one before it.
std::vector<ActorAssetRequest> ExpectedRoomEnemyRequests() {
  ActorAssetRequest body{};
  body.kind = Kind(ActorAssetKind::kActorModelParse);
  body.target_slot = 3;
  body.data = At(kZudBase + 0x30);
  body.model_id = 0x21;
  body.placement = actors::PackPlacement(
      {.tile_x = 5, .surface = 1, .tile_z = 6, .facing = 0x80});
  body.sub_kind = actors::kDefaultSubKind;
  body.variant = 1;
  body.material = 3;
  ActorAssetRequest weapon = body;
  weapon.kind = Kind(ActorAssetKind::kWeaponModelParse);
  weapon.target_slot = 6;
  weapon.data = At(kZudBase + 0x40);
  weapon.model_id = 5;
  weapon.sub_kind = 3;
  weapon.texture_slot = actors::kWeaponTextureSlot;
  weapon.material = 2;
  ActorAssetRequest shield = weapon;
  shield.target_slot = 7;
  shield.data = At(kZudBase + 0x50);
  shield.model_id = 6;
  shield.texture_slot = actors::kShieldTextureSlot;
  shield.material = 1;
  ActorAssetRequest battle = shield;
  battle.kind = Kind(ActorAssetKind::kSequenceParse);
  battle.target_slot = 3;
  battle.data = At(kZudBase + 0x70);
  battle.sub_kind = 0;
  battle.model_id = 4;
  ActorAssetRequest common = battle;
  common.data = At(kZudBase + 0x60);
  common.model_id = 0;
  return {body, weapon, shield, battle, common};
}
}  // namespace

TEST_CASE("A room enemy queues its ZUD parts in the original's order",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  PrepareRoom(host);
  const actors::CharacterTemplate character = EnemyTemplate();
  const actors::RoomSpawn spawn = EnemySpawn();
  const SpawnedActor actor =
      actors::SpawnRoomEnemy(host, 3, spawn, character, 1);
  REQUIRE(actor == Handle(kFirstHandle));
  CHECK(host.refusals.empty());
  const std::vector<ActorAssetRequest> expected = ExpectedRoomEnemyRequests();
  REQUIRE(host.requests.size() == expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    CAPTURE(i);
    CHECK(host.requests.at(i) == expected.at(i));
  }
  CHECK(host.checked_assets.size() == expected.size());
  CHECK(host.forgotten_assets.size() == expected.size());
}

TEST_CASE("A room enemy is discarded when an owned model part fails",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  PrepareRoom(host);
  host.fail_owned_completion_at = 1;

  CHECK(actors::SpawnRoomEnemy(host, 3, EnemySpawn(), EnemyTemplate(), 1) ==
        SpawnedActor::kNone);
  CHECK(host.table.at(3) == SpawnedActor::kNone);
  CHECK(host.discarded == std::vector{Handle(kFirstHandle)});
  CHECK(host.released_actor_models == std::vector{3});
  CHECK(host.released_weapon_models == std::vector{6, 7});
  CHECK(host.forgotten_assets.size() == 5);
  CHECK(host.refusals == std::vector{SpawnRefusal::kAssetQueue});
  CHECK(std::ranges::find(host.log, "Install 3") == host.log.end());
  CHECK(std::ranges::none_of(host.log, [](const std::string& entry) {
    return entry.starts_with("AiState ");
  }));
}

TEST_CASE("A room enemy cancels queued owned parts when submission fails",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  PrepareRoom(host);
  host.refuse_owned_submit_at = 2;
  const SpawnedActor actor = Handle(kFirstHandle);

  CHECK(actors::SpawnRoomEnemy(host, 3, EnemySpawn(), EnemyTemplate(), 1) ==
        SpawnedActor::kNone);
  CHECK(host.cancelled_owners == std::vector{actor});
  CHECK(host.forgotten_assets.size() == 2);
  CHECK(host.discarded == std::vector{actor});
  CHECK(host.table.at(3) == SpawnedActor::kNone);
  CHECK(host.refusals == std::vector{SpawnRefusal::kAssetQueue});
  CHECK(std::ranges::find(host.log, "Install 3") == host.log.end());
}

TEST_CASE("A room enemy drains its requests if cancellation is refused",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  PrepareRoom(host);
  host.refuse_owned_submit_at = 2;
  host.allow_owned_cancel = false;
  const SpawnedActor actor = Handle(kFirstHandle);

  CHECK((actors::SpawnRoomEnemy(host, 3, EnemySpawn(), EnemyTemplate(), 1) ==
             SpawnedActor::kNone &&
         host.table.at(3) == SpawnedActor::kNone));
  CHECK((host.checked_assets.size() == 2 && host.forgotten_assets.size() == 2));
  CHECK(host.discarded == std::vector{actor});
  const std::vector<int> actor_models = {3};
  const std::vector<int> weapon_models = {6, 7};
  CHECK((host.released_actor_models == actor_models &&
         host.released_weapon_models == weapon_models));
  const auto installed = std::ranges::find(host.log, "Install 3");
  const auto updated = std::ranges::find(host.log, "UpdateQueue");
  const auto released = std::ranges::find(host.log, "ReleaseActorModel 3");
  CHECK(installed == host.log.end());
  CHECK(updated < released);
}

TEST_CASE("A room enemy is discarded if its handle cannot be published",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  PrepareRoom(host);
  host.refuse_install = true;

  CHECK(actors::SpawnRoomEnemy(host, 3, EnemySpawn(), EnemyTemplate(), 1) ==
        SpawnedActor::kNone);
  CHECK(host.table.at(3) == SpawnedActor::kNone);
  CHECK(host.discarded == std::vector{Handle(kFirstHandle)});
  CHECK(host.released_actor_models == std::vector{3});
  CHECK(host.released_weapon_models == std::vector{6, 7});
  CHECK(host.forgotten_assets.size() == 5);
  CHECK(std::ranges::none_of(host.log, [](const std::string& entry) {
    return entry.starts_with("TemplateIdentity ");
  }));
}

TEST_CASE("A room enemy settles from its template and spawn record",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  PrepareRoom(host);
  const actors::RoomSpawn spawn = EnemySpawn();
  REQUIRE(actors::SpawnRoomEnemy(host, 3, spawn, EnemyTemplate(), 1) !=
          SpawnedActor::kNone);
  CHECK(
      host.log ==
      std::vector<std::string>{
          "Allocate 1",        "Category 4",         "QueueAsset",
          "QueueAsset",        "QueueAsset",         "QueueAsset",
          "QueueAsset",        "UpdateQueue",        "Pump",
          "Install 3",         "TemplateIdentity 3", "DrawWeapon 3 0",
          "BuildEquipment 3",  "InitialStats 3",     "AttackReach 3",
          "Display 3 0x1846",  "Animation 3 0",      "Speeds 3 0x9000 0x12000",
          "LinkAfterLeader 3", "SpawnDetails 3",     "AiState 3 -1",
          "CombatPose 3 4"});
  // The template's stats are rolled; MP 0 draws no roll.
  CHECK(host.rolls == 4);
  CHECK(host.details ==
        actors::SpawnDetails{
            .facing = 9,
            .spawn_byte = 4,
            .room_spawn_index = 7,
            .drop = spawn.drop,
            .second_drop = spawn.second_drop,
            .random_drop = spawn.random_drop,
            .settings = actors::DecodeSpawnSettings(spawn.settings),
            .ai_settings = 0x1234});
  const actors::TemplateIdentity& identity = Required(host.identity);
  CHECK_FALSE(identity.retain_after_defeat);
  CHECK(identity.blade_id == 5);
  CHECK(identity.shield_id == 6);
}

TEST_CASE("A remembered room enemy restores its state and AI flags",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  PrepareRoom(host);
  actors::RememberedSpawn remembered;
  remembered.ai_flags = 0x21;
  remembered.statuses = 0x400;
  remembered.placement = {.tile_x = 8, .surface = 3, .tile_z = 9, .facing = 10};
  host.remembered = remembered;
  host.ai_result = 1;
  REQUIRE(actors::SpawnRoomEnemy(host, 3, EnemySpawn(), EnemyTemplate(), 2) !=
          SpawnedActor::kNone);
  CHECK(host.restored == remembered);
  CHECK(host.requests.at(0).placement ==
        actors::PackPlacement(
            {.tile_x = 8, .surface = 1, .tile_z = 9, .facing = 10}));
  const std::vector<std::string> tail(host.log.end() - 4, host.log.end());
  CHECK(tail == std::vector<std::string>{"RestoreRemembered 3",
                                         "InflictStatuses 3 0x400",
                                         "AiState 3 33", "CombatPose 3 0"});
  // Flags 2 spawn a peaceful actor without its battle animation.
  CHECK(host.requests.size() == 4);
}

TEST_CASE("A room enemy with a second body loads it in the weapon slot",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  PrepareRoom(host);
  host.secondary_body = true;
  REQUIRE(actors::SpawnRoomEnemy(host, 3, EnemySpawn(), EnemyTemplate(), 1) !=
          SpawnedActor::kNone);
  CHECK(host.log.at(0) == "Allocate 2");
  REQUIRE(host.requests.size() == 4);
  ActorAssetRequest second = host.requests.at(0);
  second.kind = Kind(ActorAssetKind::kActorModelLoad);
  second.target_slot = 7;
  second.model_id = 0x22;
  second.sub_kind = 3;
  second.texture_slot = actors::kSecondBodyTextureSlot;
  second.variant = 0;
  CHECK(host.requests.at(1) == second);
}

TEST_CASE("A room enemy reads its ZUD file in place of the cached one",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  PrepareRoom(host);
  host.cached_zud = 1;
  host.read_pending = true;
  host.reads = {ZudReadState::kPending, ZudReadState::kPending,
                ZudReadState::kLoaded};
  REQUIRE(actors::SpawnRoomEnemy(host, 3, EnemySpawn(), EnemyTemplate(), 1) !=
          SpawnedActor::kNone);
  const std::vector<std::string> head(host.log.begin(), host.log.begin() + 8);
  CHECK(head == std::vector<std::string>{"FreeZud", "Diagnostic 0xa7",
                                         "AllocateZud 128", "BeginZudRead 102",
                                         "Pump", "Pump", "FinishZudRead 2 true",
                                         "Allocate 1"});
  CHECK(host.cached_zud == 2);
}

namespace {
// The room enemy in slot 3 is refused for `refusal` and nothing spawned.
void CheckZudRefused(FakeSpawnHost& host,
                     const actors::CharacterTemplate& character,
                     SpawnRefusal refusal) {
  const auto table = host.table;
  CHECK(actors::SpawnRoomEnemy(host, 3, EnemySpawn(), character, 1) ==
        SpawnedActor::kNone);
  CHECK(host.refusals == std::vector{refusal});
  CheckNothingSpawned(host, table);
}
}  // namespace

TEST_CASE("A room enemy is refused when its ZUD file cannot be used",
          "[actors][actors-spawning]") {
  actors::CharacterTemplate character = EnemyTemplate();

  FakeSpawnHost index;
  PrepareRoom(index);
  index.zone_enemies = 2;
  CheckZudRefused(index, character, SpawnRefusal::kZudIndex);
  CHECK(index.log.empty());

  FakeSpawnHost allocation;
  PrepareRoom(allocation);
  allocation.cached_zud.reset();
  allocation.refuse_zud = true;
  CheckZudRefused(allocation, character, SpawnRefusal::kZudAllocation);

  // #302: the original waited for a failed read forever.
  FakeSpawnHost read;
  PrepareRoom(read);
  read.cached_zud.reset();
  read.reads = {ZudReadState::kPending, ZudReadState::kFailed};
  CheckZudRefused(read, character, SpawnRefusal::kZudRead);
  CHECK(read.log.back() == "FinishZudRead 2 false");
  CHECK_FALSE(read.cached_zud);

  FakeSpawnHost header;
  PrepareRoom(header);
  header.zud.resize(actors::kZudHeaderBytes - 1);
  CheckZudRefused(header, character, SpawnRefusal::kZudSection);

  FakeSpawnHost section;
  PrepareRoom(section);
  section.zud = MakeZud(
      0x80,
      {{{0x30, 0x10}, {0x40, 0x10}, {0x50, 0x31}, {0x60, 0x10}, {0x70, 0x10}}});
  CheckZudRefused(section, character, SpawnRefusal::kZudSection);

  // An unused section is not checked.
  FakeSpawnHost unused;
  PrepareRoom(unused);
  unused.zud = section.zud;
  character.shield_model = 0;
  CHECK(actors::SpawnRoomEnemy(unused, 3, EnemySpawn(), character, 1) !=
        SpawnedActor::kNone);

  FakeSpawnHost actor;
  PrepareRoom(actor);
  actor.refuse_allocation = true;
  CheckZudRefused(actor, EnemyTemplate(), SpawnRefusal::kAllocation);
}

TEST_CASE("A room enemy is refused without a leader to follow",
          "[actors][actors-spawning]") {
  FakeSpawnHost leaderless;
  PrepareRoom(leaderless);
  leaderless.table.at(actors::kAshleySlot) = SpawnedActor::kNone;
  CHECK(actors::SpawnRoomEnemy(leaderless, 3, EnemySpawn(), EnemyTemplate(),
                               1) == SpawnedActor::kNone);
  CHECK(leaderless.refusals == std::vector{SpawnRefusal::kNoLeader});
  CHECK(leaderless.log.empty());

  // #302: a room enemy in slot 0 was linked after itself.
  FakeSpawnHost self;
  PrepareRoom(self);
  self.table.at(actors::kAshleySlot) = SpawnedActor::kNone;
  CHECK(actors::SpawnRoomEnemy(self, 0, EnemySpawn(), EnemyTemplate(), 0) ==
        SpawnedActor::kNone);
  CHECK(self.refusals == std::vector{SpawnRefusal::kSelfLeader});
  CHECK(self.log.empty());
}

TEST_CASE("A room enemy is refused outside the table or in an occupied slot",
          "[actors][actors-spawning]") {
  FakeSpawnHost outside;
  PrepareRoom(outside);
  CHECK(actors::SpawnRoomEnemy(outside, 42, EnemySpawn(), EnemyTemplate(), 0) ==
        SpawnedActor::kNone);
  CHECK(outside.refusals == std::vector{SpawnRefusal::kActorSlot});

  FakeSpawnHost occupied;
  PrepareRoom(occupied);
  occupied.table.at(3) = Handle(0x30);
  CHECK(actors::SpawnRoomEnemy(occupied, 3, EnemySpawn(), EnemyTemplate(), 0) ==
        SpawnedActor::kNone);
  CHECK(occupied.refusals.empty());
  CHECK(occupied.log.empty());
}

namespace {
// A template whose stats saturate and whose limb 1 carries eight affinities
// and a masked action.
actors::CharacterTemplate StatTemplate() {
  actors::CharacterTemplate character = EnemyTemplate();
  character.hp = 995;
  character.mp = 40;
  character.recovery_delay_bonus = 0xC5;
  character.status_immunities = -2;
  character.limbs.at(1).affinities = {1, 2, 3, 4, 5, 6, 7, 8};
  character.limbs.at(1).actions.at(2) = {
      .action_id = 0x55, .effect_model = 0x9A, .index = 0x1B};
  return character;
}

// Applies StatTemplate's stats, rolled, to the actor in slot 4 with a roll
// of 15 and an anchor bone of 3.
void ApplyRolledStats(FakeSpawnHost& host) {
  host.table.at(4) = Handle(0x40);
  host.roll = 15;
  host.bone = 3;
  actors::ApplyActorInitialStats(host, 4, StatTemplate(), true);
}
}  // namespace

TEST_CASE("Initial stats roll and clamp the template's stats",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  ApplyRolledStats(host);
  const actors::InitialStats& stats = Required(host.initial);
  CHECK(host.rolls == 5);
  CHECK(stats.hp == 999);
  CHECK(stats.mp == 45);
  CHECK(stats.strength == 35);
  CHECK(stats.walk_speed == 9);
  CHECK(stats.reach == 3);
  CHECK(stats.recovery_delay_bonus == 5);
  CHECK(stats.statuses == static_cast<std::int32_t>(0x80000000U));
}

TEST_CASE("Initial stats copy seven affinities and mask the limb actions",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  ApplyRolledStats(host);
  const actors::InitialStats& stats = Required(host.initial);
  CHECK(stats.limbs.at(1).affinities ==
        std::array<std::int16_t, actors::kInitialLimbAffinities>{1, 2, 3, 4, 5,
                                                                 6, 7});
  CHECK(stats.limbs.at(1).anchor_bone == 4);
  CHECK(stats.limbs.at(1).name_index == 0x44);
  CHECK(stats.limb_actions.at(1).at(2) ==
        actors::LimbActionStats{
            .action_id = 0x55, .index = 0xB, .effect_model = 0xA});
  CHECK(host.reach == actors::AttackReachFor(host.weapon_range, 3));
}

TEST_CASE("Initial stats without a roll or a bone keep the template's values",
          "[actors][actors-spawning]") {
  FakeSpawnHost boneless;
  boneless.table.at(4) = Handle(0x40);
  actors::ApplyActorInitialStats(boneless, 4, StatTemplate(), false);
  const actors::InitialStats& stats = Required(boneless.initial);
  CHECK(boneless.rolls == 0);
  CHECK(stats.limbs.at(1).anchor_bone == 0);
  CHECK(stats.limbs.at(1).name_index == 0);
  CHECK(stats.hp == 995);
}

TEST_CASE("Rolled HP and MP saturate at 0-999 instead of wrapping at 16 bits",
          "[actors][actors-spawning]") {
  actors::CharacterTemplate character = EnemyTemplate();
  character.hp = -32765;
  character.mp = 32765;

  // With a roll of 0 the original's short wraps -32765 - 10 to +32761 (999);
  // the int sum stays negative. With a roll of 20 it wraps 32765 + 10 to
  // -32761 (0).
  struct RollCase {
    int roll;
    bool rolled;
  };
  const std::array<RollCase, 3> rolls{{
      {.roll = 0, .rolled = true},
      {.roll = 20, .rolled = true},
      {.roll = 0, .rolled = false},
  }};
  for (const RollCase& entry : rolls) {
    CAPTURE(entry.roll, entry.rolled);
    FakeSpawnHost host;
    host.table.at(4) = Handle(0x40);
    host.roll = entry.roll;
    actors::ApplyActorInitialStats(host, 4, character, entry.rolled);
    const actors::InitialStats& stats = Required(host.initial);
    CHECK(stats.hp == 0);
    CHECK(stats.mp == 999);
  }
}

TEST_CASE("Actor initializers refuse an id outside the table or no actor",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  const actors::CharacterTemplate character = EnemyTemplate();
  actors::ApplyActorInitialStats(host, 42, character, false);
  actors::InitializeActorDefaults(host, 42, 1, 0, 0, 0);
  actors::InitializeActorFromTemplate(host, 42, character, 0, 0, 0, false);
  actors::InitializeActorDefaults(host, 5, 1, 0, 0, 0);
  CHECK(host.refusals ==
        std::vector{SpawnRefusal::kActorSlot, SpawnRefusal::kActorSlot,
                    SpawnRefusal::kActorSlot, SpawnRefusal::kNoActor});
  CHECK(host.log.empty());

  host.table.at(5) = Handle(0x50);
  actors::InitializeActorDefaults(host, 5, 41, 7, 8, 0x1D);
  CHECK(host.defaults == actors::ActorDefaults{
                             .id = 5,
                             .model_id = 41,
                             .blade_id = 7,
                             .shield_id = 8,
                             .disposition = {.kind = 4, .weapon_drawn = true},
                             .interaction_flags = 0x1C,
                             .gait = {.movement_class = 16, .gait_class = 32}});
  CHECK(host.log == std::vector<std::string>{"Defaults 5", "DrawWeapon 5 0"});
}

TEST_CASE("Ashley's model request is value-initialized",
          "[actors][actors-spawning]") {
  // #302: the original queued a stack request with only three fields set.
  FakeSpawnHost host;
  host.queue_index = 6;
  CHECK(actors::QueueAshleyModelLoad(host, 0x12) == 7);
  ActorAssetRequest expected{};
  expected.kind = Kind(ActorAssetKind::kActorModelLoad);
  expected.target_slot = 0;
  expected.model_id = 0x12;
  CHECK(host.requests == std::vector<ActorAssetRequest>{expected});
  // Without Ashley the load stays unowned, as the original queued it.
  CHECK(host.queued_owners.empty());
}

TEST_CASE("An installed Ashley owns her model load",
          "[actors][actors-spawning]") {
  // #48: removing Ashley cancels the load into her record.
  FakeSpawnHost host;
  host.table.at(actors::kAshleySlot) = Handle(0x10);
  host.queue_index = 2;
  CHECK(actors::QueueAshleyModelLoad(host, 0x12) == 3);
  CHECK(host.queued_owners == std::vector{Handle(0x10)});
  CHECK(host.log == std::vector<std::string>{"QueueOwnedAsset"});
  REQUIRE(host.requests.size() == 1);
  CHECK(host.requests.at(0).target_slot == 0);
  CHECK(host.requests.at(0).model_id == 0x12);
}

TEST_CASE("A scripted actor stays unpublished until its model is ready",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  actors::ScriptedActorSpawnTable pending;
  const actors::SpawnPlacement placement{
      .tile_x = 2, .surface = 1, .tile_z = 3, .facing = 4};
  CHECK(actors::QueueScriptedActorSpawn(host, pending, 9, 0x33, placement) ==
        host.queue_index + 1);
  CHECK(host.table.at(9) == SpawnedActor::kNone);
  CHECK(pending.ContainsSlot(9));
  CHECK(host.log == std::vector<std::string>{"Allocate 1", "QueueOwnedAsset"});
  CHECK(actors::QueueScriptedActorSpawn(host, pending, 9, 0x34, placement) ==
        0);
  CHECK(host.log == std::vector<std::string>{"Allocate 1", "QueueOwnedAsset"});
  CHECK(host.defaults == std::nullopt);
  REQUIRE(host.owned_requests.size() == 1);
}

TEST_CASE("A scripted actor spawns into a free slot below 16",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  actors::ScriptedActorSpawnTable pending;
  CHECK(actors::QueueScriptedActorSpawn(host, pending, 9, 0x33, {}) ==
        host.queue_index + 1);
  REQUIRE(host.owned_requests.size() == 1);
  CompleteOwnedScriptedActorLoad(host, pending);
  CHECK(host.table.at(9) == Handle(kFirstHandle));
  CHECK_FALSE(pending.ContainsSlot(9));
  CHECK(host.queued_owners == std::vector{Handle(kFirstHandle)});
}

TEST_CASE("A loaded scripted actor receives its placement and spawn defaults",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  actors::ScriptedActorSpawnTable pending;
  const actors::SpawnPlacement placement{
      .tile_x = 2, .surface = 1, .tile_z = 3, .facing = 4};
  CHECK(actors::QueueScriptedActorSpawn(host, pending, 9, 0x33, placement) ==
        host.queue_index + 1);
  REQUIRE(host.owned_requests.size() == 1);
  CompleteOwnedScriptedActorLoad(host, pending);

  const actors::ActorDefaults& defaults = Required(host.defaults);
  CHECK(defaults.disposition == actors::SpawnDisposition{.kind = 8});
  CHECK(defaults.interaction_flags == 4);
  CHECK(host.requests.at(0).placement == actors::PackPlacement(placement));
  CHECK(host.requests.at(0).target_slot == 9);
}

TEST_CASE("A failed scripted actor model load discards its unpublished actor",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  actors::ScriptedActorSpawnTable pending;
  host.fail_owned_completion_at = 0;

  CHECK(actors::QueueScriptedActorSpawn(host, pending, 9, 0x33, {}) ==
        host.queue_index + 1);
  REQUIRE(host.owned_requests.size() == 1);
  CompleteOwnedScriptedActorLoad(host, pending);

  CHECK(host.table.at(9) == SpawnedActor::kNone);
  CHECK_FALSE(pending.ContainsSlot(9));
  CHECK(host.discarded == std::vector{Handle(kFirstHandle)});
  CHECK(host.released_actor_models == std::vector{9});
  CHECK(host.defaults == std::nullopt);
  CHECK(host.refusals == std::vector{SpawnRefusal::kAssetQueue});
}

TEST_CASE("A pending scripted actor is cancelled before discard",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  actors::ScriptedActorSpawnTable pending;

  CHECK(actors::QueueScriptedActorSpawn(host, pending, 9, 0x33, {}) ==
        host.queue_index + 1);
  REQUIRE(host.owned_requests.size() == 1);
  const content::ActorAssetTicket ticket = host.owned_requests.at(0).ticket;
  CHECK(actors::CancelScriptedActorSpawn(host, pending, 9));

  CHECK_FALSE(pending.ContainsSlot(9));
  CHECK(host.table.at(9) == SpawnedActor::kNone);
  CHECK(host.discarded == std::vector{Handle(kFirstHandle)});
  CHECK(host.released_actor_models == std::vector{9});
  CHECK(host.owned_completions.Find(ticket).outcome ==
        content::ActorAssetOutcome::kCancelled);
  CHECK(host.cancelled_owners == std::vector{Handle(kFirstHandle)});
}

TEST_CASE("A pending scripted actor drains when cancellation is refused",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  actors::ScriptedActorSpawnTable pending;
  host.allow_owned_cancel = false;

  CHECK(actors::QueueScriptedActorSpawn(host, pending, 9, 0x33, {}) ==
        host.queue_index + 1);
  REQUIRE(host.owned_requests.size() == 1);
  const content::ActorAssetTicket ticket = host.owned_requests.at(0).ticket;
  CHECK(actors::CancelScriptedActorSpawn(host, pending, 9));

  CHECK_FALSE(pending.ContainsSlot(9));
  CHECK(host.table.at(9) == SpawnedActor::kNone);
  CHECK(host.discarded == std::vector{Handle(kFirstHandle)});
  CHECK(host.released_actor_models == std::vector{9});
  CHECK(host.owned_completions.Find(ticket).outcome ==
        content::ActorAssetOutcome::kLoaded);
  CHECK(host.cancelled_owners.empty());
}

TEST_CASE("A scripted actor does not replace a slot occupied before completion",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  actors::ScriptedActorSpawnTable pending;
  CHECK(actors::QueueScriptedActorSpawn(host, pending, 9, 0x33, {}) ==
        host.queue_index + 1);
  host.table.at(9) = Handle(0x99);
  REQUIRE(host.owned_requests.size() == 1);
  CompleteOwnedScriptedActorLoad(host, pending);

  CHECK(host.table.at(9) == Handle(0x99));
  CHECK_FALSE(pending.ContainsSlot(9));
  CHECK(host.discarded == std::vector{Handle(kFirstHandle)});
  CHECK(host.released_actor_models.empty());
  CHECK(host.defaults == std::nullopt);
  CHECK(host.refusals == std::vector{SpawnRefusal::kAllocation});
}

TEST_CASE("A scripted actor is discarded when its model request cannot queue",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  actors::ScriptedActorSpawnTable pending;
  host.queue_index = -1;

  CHECK(actors::QueueScriptedActorSpawn(host, pending, 9, 0x33, {}) == 0);
  CHECK(host.table.at(9) == SpawnedActor::kNone);
  CHECK_FALSE(pending.ContainsSlot(9));
  CHECK(host.discarded == std::vector{Handle(kFirstHandle)});
  CHECK(host.refusals == std::vector{SpawnRefusal::kAssetQueue});
  CHECK(host.defaults == std::nullopt);
  CHECK(host.log == std::vector<std::string>{"Allocate 1", "QueueOwnedAsset",
                                             "DiscardUninstalled"});
}

TEST_CASE(
    "A scripted actor is refused at slot 16, in an occupied slot or without an "
    "allocation",
    "[actors][actors-spawning]") {
  FakeSpawnHost beyond;
  actors::ScriptedActorSpawnTable beyond_pending;
  CHECK(actors::QueueScriptedActorSpawn(beyond, beyond_pending, 16, 1, {}) ==
        0);
  beyond.table.at(4) = Handle(0x40);
  CHECK(actors::QueueScriptedActorSpawn(beyond, beyond_pending, 4, 1, {}) == 0);
  CHECK(beyond.refusals.empty());
  CHECK(beyond.log.empty());

  FakeSpawnHost refused;
  actors::ScriptedActorSpawnTable refused_pending;
  refused.refuse_allocation = true;
  CHECK(actors::QueueScriptedActorSpawn(refused, refused_pending, 4, 1, {}) ==
        0);
  CHECK(refused.refusals == std::vector{SpawnRefusal::kAllocation});
  CHECK(refused.table.at(4) == SpawnedActor::kNone);
}

TEST_CASE("Equipment refreshes keep the original's blade form without a shield",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  actors::LoadActorEquipmentModels(host, -1, 1, 1);
  actors::RefreshActorEquipmentModels(host, 42);
  CHECK(host.refusals ==
        std::vector{SpawnRefusal::kActorSlot, SpawnRefusal::kActorSlot});
  actors::LoadActorEquipmentModels(host, 3, 1, 1);
  actors::RefreshActorEquipmentModels(host, 3);
  CHECK(host.log.empty());

  host.table.at(3) = Handle(0x30);
  host.equipped = actors::EquippedModels{};
  actors::RefreshActorEquipmentModels(host, 3);
  // #302: the shield form was meant; the weapon slot is released twice.
  CHECK(host.log == std::vector<std::string>{"Release 6", "Release 6"});

  host.log.clear();
  host.equipped = actors::EquippedModels{.has_blade = true,
                                         .weapon_model = 11,
                                         .blade_material = 2,
                                         .has_shield = true,
                                         .shield_model = 12,
                                         .shield_material = 3};
  actors::RefreshActorEquipmentModels(host, 3);
  CHECK(host.log == std::vector<std::string>{"Release 6", "QueueOwnedAsset",
                                             "Release 7", "QueueOwnedAsset"});
  CHECK(host.queued_owners == std::vector{Handle(0x30), Handle(0x30)});
  CHECK(host.requests.at(0).material == 2);
  CHECK(host.requests.at(1).model_id == 12);

  host.log.clear();
  actors::LoadActorEquipmentModels(host, 3, actors::kUnchangedEquipment, 0);
  CHECK(host.log == std::vector<std::string>{"Release 7"});
}

TEST_CASE("Ashley's runtime state is restored only onto her actor",
          "[actors][actors-spawning]") {
  FakeSpawnHost absent;
  actors::RestoreAshleyRuntimeState(absent);
  CHECK(absent.refusals == std::vector{SpawnRefusal::kNoCharacter});
  CHECK(absent.log.empty());

  FakeSpawnHost host;
  host.character = Handle(0x10);
  host.snapshot.pending = true;
  host.snapshot.statuses = 0x40;
  host.snapshot.weapon_drawn = 3;
  host.snapshot.camera_pitch = 0x100;
  host.snapshot.camera_distance = 0x2000;
  actors::RestoreAshleyRuntimeState(host);
  CHECK(host.log ==
        std::vector<std::string>{"RestoreVitals", "CharacterStatuses 0x40",
                                 "RestoreConditions", "CharacterDrawn 3",
                                 "DrawWeapon 0 2", "HudMode 1", "ClearPending",
                                 "Camera 256 8192"});
  CHECK_FALSE(host.snapshot.pending);

  host.log.clear();
  host.snapshot.weapon_drawn = 2;
  actors::RestoreAshleyRuntimeState(host);
  CHECK(std::ranges::find(host.log, "HudMode 1") == host.log.end());
}

namespace {
// Weapon 1 with blade 2, shield 0, limb 3 armor in slots 1 and 9 and an
// accessory in slot 4.
actors::AshleyInventory EquippedInventory() {
  actors::AshleyInventory inventory;
  inventory.weapons.at(1) = {.equipped = true, .blade = 2};
  inventory.blades.at(1) = {.model = 9, .material = 3};
  inventory.shields.at(0) = {.equipped = true, .model = 4, .material = 2};
  inventory.armor_limbs.at(1) = 3;
  inventory.armor_limbs.at(4) = actors::kAccessoryLimb;
  inventory.armor_limbs.at(9) = 3;
  return inventory;
}
}  // namespace

TEST_CASE("Ashley spawns with her inventory's equipment and armor",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  host.inventory = EquippedInventory();
  host.ashley.reach = 2;
  actors::SpawnAshleyWithEquipment(host);
  CHECK(host.refusals.empty());
  const auto set = std::ranges::find(host.log, "SetCharacter");
  REQUIRE(set != host.log.end());
  const std::vector<std::string> tail(set, host.log.end());
  CHECK(tail == std::vector<std::string>{
                    "SetCharacter", "TemplateIdentity 0", "BuildEquipment 0",
                    "InitialStats 0", "AttackReach 0", "AshleyWeapon 1",
                    "AttackReach 0", "AshleyShield 0", "AshleyArmor 0 -",
                    "AshleyArmor 1 -", "AshleyArmor 2 -", "AshleyArmor 2 1",
                    "AshleyArmor 2 9", "AshleyArmor 3 -", "AshleyArmor 4 -",
                    "AshleyArmor 5 -", "AshleyAccessory -",
                    "AshleyAccessory 4"});

  FakeSpawnHost pending;
  pending.inventory = host.inventory;
  pending.snapshot.pending = true;
  actors::SpawnAshleyWithEquipment(pending);
  CHECK(pending.log.back() == "Camera 0 0");
}

TEST_CASE("Ashley spawns from her template at her placement",
          "[actors][actors-spawning]") {
  FakeSpawnHost host;
  host.inventory = EquippedInventory();
  host.ashley.reach = 2;
  actors::SpawnAshleyWithEquipment(host);
  CHECK(host.character == Handle(kFirstHandle));
  CHECK(host.rolls == 0);
  const actors::TemplateIdentity& identity = Required(host.identity);
  CHECK(identity.retain_after_defeat);
  CHECK(identity.blade_id == actors::kAshleyTemplateBlade);
  CHECK(host.reach == actors::AttackReachFor(host.ashley_range, 2));
  CHECK(host.requests.at(0).placement ==
        actors::PackPlacement(actors::kAshleyPlacement));
}

TEST_CASE("Ashley's spawn is refused without a blade or her slot",
          "[actors][actors-spawning]") {
  // #302: blade 0 read blades[-1].
  FakeSpawnHost bladeless;
  bladeless.inventory.weapons.at(0) = {.equipped = true, .blade = 0};
  actors::SpawnAshleyWithEquipment(bladeless);
  CHECK(bladeless.refusals == std::vector{SpawnRefusal::kAshleyBlade});
  CHECK(bladeless.log.empty());

  // #302: the original dereferenced the null actor of an occupied slot 0.
  FakeSpawnHost occupied;
  occupied.table.at(actors::kAshleySlot) = Handle(0x10);
  actors::SpawnAshleyWithEquipment(occupied);
  CHECK(occupied.refusals == std::vector{SpawnRefusal::kAshleyActor});
  CHECK(occupied.log.empty());
  CHECK(occupied.character == SpawnedActor::kNone);
}

namespace {
// A character record whose byte fields hold values above 0x7F.
VS::Actors::CharacterInit LegacyCharacterInit() {
  VS::Actors::CharacterInit init{};
  init.sub_type = 0x1234;
  init.model_id = 0xF3;
  init.reach = 0x1F;
  init.enemy_class = 5;
  init.name[0] = 'A';
  init.hp = 300;
  init.mp = 40;
  init.strength = 0xC8;
  init.recovery_delay_bonus = 0x85;
  init.material = 4;
  init.walk_speed = 0x90;
  init.gait_class = 7;
  init.run_speed = 0xA0;
  init.status_immunities = -1;
  init.ai_settings = 0x01020304;
  init.mobility_limbs = {1, 0xFE};
  init.default_target_limb = 3;
  init.weapon.blade.weapon_model_id = -2;
  init.weapon.blade.category = 6;
  init.weapon.material = static_cast<char>(0x81);
  init.shield.base.weapon_model_id = 12;
  init.shield.material = 2;
  init.limbs[2].hp = 77;
  init.limbs[2].types[1] = -4;
  init.limbs[2].affinities[7] = 9;
  init.limbs[2].actions[1].action_id = 0x55;
  init.limbs[2].actions[1].effect_model = 0x9A;
  init.limbs[2].actions[1].index = 0x0B;
  init.limbs[2].dp_splash[5] = 0xEE;
  init.zud_index = 9;
  return init;
}

void CheckDecodedIdentity(const actors::CharacterTemplate& decoded) {
  CHECK(decoded.sub_type == 0x1234);
  CHECK(decoded.model_id == 0xF3);
  CHECK(decoded.reach == 0x1F);
  CHECK(decoded.enemy_class == 5);
  CHECK(decoded.name.at(0) == 'A');
  CHECK(decoded.hp == 300);
  CHECK(decoded.strength == 0xC8);
  CHECK(decoded.recovery_delay_bonus == 0x85);
  CHECK(decoded.walk_speed == 0x90);
}

void CheckDecodedTraitsAndWeapon(const actors::CharacterTemplate& decoded) {
  CHECK(decoded.gait_class == 7);
  CHECK(decoded.run_speed == 0xA0);
  CHECK(decoded.status_immunities == -1);
  CHECK(decoded.ai_settings == 0x01020304);
  CHECK(decoded.mobility_limbs == std::array<std::uint8_t, 2>{1, 0xFE});
  CHECK(decoded.default_target_limb == 3);
  CHECK(decoded.blade_model == 0xFE);
  CHECK(decoded.blade_category == 6);
  CHECK(decoded.weapon_material == 0x81);
}

void CheckDecodedShieldAndLimbs(const actors::CharacterTemplate& decoded) {
  CHECK(decoded.shield_model == 12);
  CHECK(decoded.shield_material == 2);
  CHECK(decoded.limbs.at(2).hp == 77);
  CHECK(decoded.limbs.at(2).types.at(1) == -4);
  CHECK(decoded.limbs.at(2).affinities.at(7) == 9);
  CHECK(decoded.limbs.at(2).actions.at(1) ==
        actors::LimbActionTemplate{
            .action_id = 0x55, .effect_model = 0x9A, .index = 0x0B});
  CHECK(decoded.limbs.at(2).dp_splash.at(5) == 0xEE);
  CHECK(decoded.zud_index == 9);
  CHECK(decoded.equipment != actors::TemplateEquipment::kNone);
}
}  // namespace

TEST_CASE("Live spawn codecs read the legacy records as unsigned bytes",
          "[actors][actors-spawning]") {
  const actors::CharacterTemplate decoded =
      actors::DecodeCharacterTemplate(LegacyCharacterInit());
  CheckDecodedIdentity(decoded);
  CheckDecodedTraitsAndWeapon(decoded);
  CheckDecodedShieldAndLimbs(decoded);
}

namespace {
VS::Field::MpdEnemySpawn LegacyRoomSpawn() {
  VS::Field::MpdEnemySpawn spawn{};
  spawn.index = 6;
  spawn.tile_x = 1;
  spawn.surface = 0x81;
  spawn.tile_z = 3;
  spawn.facing_quadrant = 4;
  spawn.settings[0] = 0xAB;
  spawn.settings[7] = 0x92;
  spawn.item_id = 0x101;
  spawn.item_count = 2;
  spawn.item2_id = 0x102;
  spawn.item2_count = 3;
  spawn.item3_id = 0x103;
  spawn.item3_count = 40;
  spawn.facing = 5;
  spawn.variant = 7;
  spawn.combat_pose = 9;
  return spawn;
}

void CheckDecodedRoomSpawn(const actors::RoomSpawn& room) {
  CHECK(room.index == 6);
  CHECK(room.placement ==
        actors::SpawnPlacement{
            .tile_x = 1, .surface = 0x81, .tile_z = 3, .facing = 4});
  CHECK(room.settings == std::array<std::uint8_t, actors::kSpawnSettingsBytes>{
                             0xAB, 0, 0, 0, 0, 0, 0, 0x92});
  CHECK(room.drop == actors::ItemDrop{.id = 0x101, .count = 2});
  CHECK(room.second_drop == actors::ItemDrop{.id = 0x102, .count = 3});
  CHECK(room.random_drop == actors::ItemDrop{.id = 0x103, .count = 40});
  CHECK(room.facing == 5);
  CHECK(room.variant == 7);
  CHECK(room.combat_pose == 9);
}

VS::Actors::RoomActorRecord LegacyRoomRecord() {
  VS::Actors::RoomActorRecord record{};
  record.ai_flags = -2;
  record.current_hp = 10;
  record.max_hp = 20;
  record.current_mp = 3;
  record.max_mp = 4;
  record.strength = 5;
  record.intelligence = 6;
  record.agility = 7;
  record.limb_hp[5] = 66;
  record.statuses = 0x1000;
  record.tile_x = 8;
  record.surface = 1;
  record.tile_z = 9;
  record.facing = 10;
  return record;
}

void CheckDecodedRememberedStats(const actors::RememberedSpawn& remembered) {
  CHECK(remembered.ai_flags == -2);
  CHECK(remembered.current_hp == 10);
  CHECK(remembered.max_hp == 20);
  CHECK(remembered.current_mp == 3);
  CHECK(remembered.max_mp == 4);
  CHECK(remembered.strength == 5);
  CHECK(remembered.intelligence == 6);
  CHECK(remembered.agility == 7);
}
}  // namespace

TEST_CASE("Live spawn codecs decode room spawns, placements and memories",
          "[actors][actors-spawning]") {
  CheckDecodedRoomSpawn(actors::DecodeRoomSpawn(LegacyRoomSpawn()));

  constexpr std::uint32_t kPlacementWord = 0x80F302F1;
  constexpr actors::SpawnPlacement kPlacement{
      .tile_x = 0xF1, .surface = 2, .tile_z = 0xF3, .facing = 0x80};
  CHECK(actors::UnpackPlacement(kPlacementWord) == kPlacement);
  CHECK(actors::PackPlacement(kPlacement) == kPlacementWord);

  const actors::RememberedSpawn remembered =
      actors::DecodeRemembered(LegacyRoomRecord());
  CheckDecodedRememberedStats(remembered);
  CHECK(remembered.limb_hp.at(5) == 66);
  CHECK(remembered.statuses == 0x1000);
  CHECK(remembered.placement ==
        actors::SpawnPlacement{
            .tile_x = 8, .surface = 1, .tile_z = 9, .facing = 10});
}

TEST_CASE("Live spawn adapters report their refusals on the actors channel",
          "[actors][actors-spawning]") {
  VS::Battle::Actor* const saved_actor = VS::Battle::actors[10];
  VS::Battle::Actor* const saved_character = VS::Battle::characterState;
  VS::Battle::Actor actor{};
  {
    ChannelWarnings warnings("actors");
    CHECK(actors::CreateLiveAuxiliaryActor(2, 1, nullptr, 0, 0) == nullptr);
    CHECK(actors::CreateLiveActorWithEquipment(3, 1, 0, 0, 0, 0, nullptr, 0) ==
          nullptr);
    CHECK(actors::SpawnLiveRoomEnemy(3, nullptr, nullptr, 0) == nullptr);
    actors::LoadLiveActorEquipmentModels(-1, 1, 1);
    actors::RefreshLiveActorEquipmentModels(42);
    VS::Battle::characterState = nullptr;
    actors::RestoreLiveAshleyRuntimeState();
    VS::Battle::actors[10] = &actor;
    actors::LoadLiveActorEquipmentModels(10, actors::kUnchangedEquipment, 5);
    CHECK(warnings.messages ==
          std::vector<std::string>{
              Refused("CreateAuxiliaryActor", "no placement"),
              Refused("CreateActorWithEquipment", "no placement"),
              Refused("SpawnRoomEnemy", "no spawn record or template"),
              Refused("LoadActorEquipmentModels",
                      "actor id outside the actor table"),
              Refused("RefreshActorEquipmentModels",
                      "actor id outside the actor table"),
              Refused("RestoreAshleyRuntimeState",
                      "no character actor to restore"),
              Refused("LoadActorEquipmentModels",
                      "equipment model slot outside the weapon model "
                      "table")});
  }
  VS::Battle::actors[10] = saved_actor;
  VS::Battle::characterState = saved_character;
}

TEST_CASE("Removing a live actor cancels only its own queued load",
          "[actors][actors-spawning]") {
  // #48: the equipment menu queues Ashley's reloads through this binding.
  constexpr std::size_t kSlot = 10;
  VS::Battle::Actor* const saved_actor = VS::Battle::actors[kSlot];
  content::ClearLiveActorAssetQueue();
  actors::ResetLiveActorHandles();
  VS::Battle::Actor installed{};
  VS::Battle::Actor stranger{};
  VS::Battle::actors[kSlot] = &installed;
  ActorAssetRequest request{};
  request.kind = Kind(ActorAssetKind::kWeaponModelLoad);
  request.target_slot = 0;
  request.model_id = 3;
  CHECK(actors::QueueLiveActorOwnedAsset(&stranger, request) == 0);
  CHECK(actors::QueueLiveActorOwnedAsset(&installed, request) == 1);
  CHECK(content::FindLiveFreeActorAssetRequest() == 2);
  CHECK(actors::CancelLiveActorAssets(kSlot, &installed));
  CHECK(content::FindLiveFreeActorAssetRequest() == 1);
  content::ClearLiveActorAssetQueue();
  actors::ResetLiveActorHandles();
  VS::Battle::actors[kSlot] = saved_actor;
}

TEST_CASE("A live slot asset ticket survives until its consumer forgets it",
          "[actors][actors-spawning][asset-ownership]") {
  constexpr std::size_t kSlot = 10;
  VS::Battle::Actor* const saved_actor = VS::Battle::actors[kSlot];
  content::ClearLiveActorAssetQueue();
  actors::ResetLiveActorHandles();
  VS::Battle::Actor installed{};
  VS::Battle::actors[kSlot] = &installed;
  ActorAssetRequest request{};
  request.kind = Kind(ActorAssetKind::kWeaponModelLoad);
  request.target_slot = 0;
  request.model_id = 3;
  const auto ticket = actors::SubmitLiveSlotActorAsset(kSlot, request);
  CHECK(ticket.has_value());
  if (!ticket) {
    content::ClearLiveActorAssetQueue();
    actors::ResetLiveActorHandles();
    VS::Battle::actors[kSlot] = saved_actor;
    return;
  }
  CHECK(content::FindLiveFreeActorAssetRequest() == 1);
  CHECK(actors::CancelLiveActorAssets(kSlot, &installed));
  CHECK(content::LiveActorAssetCompletion(*ticket).outcome ==
        content::ActorAssetOutcome::kCancelled);
  content::ForgetLiveActorAsset(*ticket);
  content::ClearLiveActorAssetQueue();
  actors::ResetLiveActorHandles();
  VS::Battle::actors[kSlot] = saved_actor;
}

TEST_CASE("A live actor owns the sequence loads its equipped spawn queues",
          "[actors][actors-spawning][asset-ownership]") {
  // #48: removing the actor cancels its own sequences; a shared sequence,
  // which no actor owns, stays queued.
  constexpr int kSlot = 1;
  VS::Battle::Actor* const saved_actor = VS::Battle::actors[kSlot];
  content::ClearLiveActorAssetQueue();
  actors::ResetLiveActorHandles();
  VS::Battle::Actor installed{};
  installed.equipped_weapon_category = 2;
  VS::Battle::actors[kSlot] = &installed;
  actors::QueueLiveSharedSequenceLoad(0, 1);
  actors::QueueLiveActorBaseSequenceLoad(&installed, kSlot);
  actors::QueueLiveActorWeaponSequenceLoad(reinterpret_cast<char*>(&installed),
                                           kSlot, 1, 0);
  CHECK(content::FindLiveFreeActorAssetRequest() == 3);
  CHECK(actors::CancelLiveActorAssets(kSlot, &installed));
  CHECK(content::FindLiveFreeActorAssetRequest() == 1);
  content::ClearLiveActorAssetQueue();
  actors::ResetLiveActorHandles();
  VS::Battle::actors[kSlot] = saved_actor;
}

TEST_CASE("Live actor loads report how they end and forget their tickets",
          "[actors][actors-spawning][asset-ownership]") {
  // #48: each load an actor queues is ticketed and settled; more loads than
  // the ticket table holds still queue because each settled ticket is gone.
  constexpr std::size_t kSlot = 10;
  constexpr std::size_t kLoads = 2 * content::ActorAssetCompletions::kCapacity;
  VS::Battle::Actor* const saved_actor = VS::Battle::actors[kSlot];
  content::ClearLiveActorAssetQueue();
  actors::ResetLiveActorHandles();
  actors::SettleLiveActorLoads();
  VS::Battle::Actor installed{};
  VS::Battle::actors[kSlot] = &installed;
  ActorAssetRequest request{};
  // The weapon parser refuses a target below the weapon slots at once.
  request.kind = Kind(ActorAssetKind::kWeaponModelParse);
  request.target_slot = -1;
  bool queued = true;
  {
    ChannelWarnings warnings("actors");
    for (std::size_t load = 0; load < kLoads; ++load) {
      queued = queued &&
               actors::QueueLiveActorOwnedAsset(&installed, request) == 0 &&
               content::UpdateLiveActorAssetQueue() == -1;
      actors::SettleLiveActorLoads();
    }
    CHECK(queued);
    CHECK(warnings.messages.size() == kLoads);
    CHECK(std::ranges::all_of(warnings.messages, [](const std::string& line) {
      return line.ends_with("asset load failed (result -2)");
    }));
  }
  content::ClearLiveActorAssetQueue();
  actors::ResetLiveActorHandles();
  VS::Battle::actors[kSlot] = saved_actor;
}

TEST_CASE("A live removal claims an undefined actor once and releases nothing",
          "[actors][actors-spawning]") {
  // #55: an actor the spawn host did not allocate has no definition, so its
  // removal names no gimmick or model, whatever its kind says.
  constexpr std::size_t kSlot = 11;
  VS::Battle::Actor* const saved_actor = VS::Battle::actors[kSlot];
  actors::ResetLiveActorHandles();
  VS::Battle::Actor foreign{};
  VS::Battle::Actor other{};
  VS::Battle::actors[kSlot] = &foreign;
  {
    ChannelWarnings warnings("actors");
    CHECK_FALSE(actors::ClaimLiveActorRemoval(kSlot, nullptr).has_value());
    CHECK_FALSE(actors::ClaimLiveActorRemoval(kSlot, &other).has_value());
    CHECK(actors::ClaimLiveActorRemoval(kSlot, &foreign) == ActorResources{});
    CHECK_FALSE(actors::ClaimLiveActorRemoval(kSlot, &foreign).has_value());
    CHECK(warnings.messages ==
          std::vector<std::string>{
              Refused("RemoveActorAtSlot",
                      "the actor has no spawn definition; no gimmick or "
                      "model is released")});
  }
  actors::ResetLiveActorHandles();
  VS::Battle::actors[kSlot] = saved_actor;
}

TEST_CASE("Live actor shapes come from the spawn definitions",
          "[actors][actors-spawning][actor-shape]") {
  // #55: native code reads an actor's shape from the definition recorded
  // with its allocation, not from its allocation kind word (0x40).
  constexpr std::size_t kSlot = 12;
  VS::Battle::Actor* const saved_actor = VS::Battle::actors[kSlot];
  actors::ResetLiveActorHandles();
  VS::Battle::actors[kSlot] = nullptr;
  VS::Battle::Actor body{};
  VS::Battle::Actor other{};
  CHECK_FALSE(actors::LiveActorShape(nullptr).has_value());
  CHECK(actors::InstallLiveActorInstance(
            kSlot, &body, ActorShape::kSecondaryBody) != SpawnedActor::kNone);
  CHECK(VS::Battle::actors[kSlot] == &body);
  CHECK(actors::LiveActorShape(&body) == ActorShape::kSecondaryBody);
  CHECK_FALSE(actors::LiveActorShape(&other).has_value());
  // A removed actor's storage left in its slot is observed without one.
  actors::InvalidateLiveActorHandle(kSlot, &body);
  CHECK_FALSE(actors::LiveActorShape(&body).has_value());
  actors::ResetLiveActorHandles();
  VS::Battle::actors[kSlot] = saved_actor;
}

TEST_CASE("A live actor instance installs only into a free spawn slot",
          "[actors][actors-spawning][actor-shape]") {
  constexpr std::size_t kSlot = 12;
  VS::Battle::Actor* const saved_actor = VS::Battle::actors[kSlot];
  actors::ResetLiveActorHandles();
  VS::Battle::actors[kSlot] = nullptr;
  VS::Battle::Actor body{};
  VS::Battle::Actor other{};
  CHECK(actors::InstallLiveActorInstance(kSlot, &body, ActorShape::kEquipped) !=
        SpawnedActor::kNone);
  CHECK(actors::InstallLiveActorInstance(
            kSlot, &other, ActorShape::kEquipped) == SpawnedActor::kNone);
  CHECK(actors::InstallLiveActorInstance(actors::kSpawnActorSlots, &other,
                                         ActorShape::kEquipped) ==
        SpawnedActor::kNone);
  CHECK(VS::Battle::actors[kSlot] == &body);
  CHECK_FALSE(actors::LiveActorShape(&other).has_value());
  actors::ResetLiveActorHandles();
  VS::Battle::actors[kSlot] = saved_actor;
}
