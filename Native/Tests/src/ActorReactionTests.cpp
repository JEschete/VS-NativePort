#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/Ai/Relations.hpp"

namespace {
namespace ai = VS::Battle::Ai;
using Calls = std::vector<std::string>;
using Hits = std::vector<ai::ReactionHit>;
using Room = std::vector<std::uint8_t>;

constexpr int kOwn = 4;
constexpr unsigned kNeutral = 1;
constexpr unsigned kAlly = 2;
constexpr unsigned kHostile = 3;
// Bits outside every relation-word field, which reactions must preserve.
constexpr std::uint32_t kNoise = 0xC8007F3E;

constexpr std::uint32_t Word(unsigned classification, unsigned counter = 0,
                             unsigned previous = 0, bool active = false) {
  return kNoise | (active ? 1u : 0u) | (classification << 6) | (counter << 20) |
         (previous << 28);
}

constexpr unsigned ClassOf(std::uint32_t word) { return (word >> 6) & 3; }
constexpr unsigned TimerOf(std::uint32_t word) { return (word >> 15) & 0x1F; }
constexpr unsigned CounterOf(std::uint32_t word) { return (word >> 20) & 0x7F; }
constexpr unsigned PreviousOf(std::uint32_t word) { return (word >> 28) & 3; }
// The word without its reaction timer.
constexpr std::uint32_t Untimed(std::uint32_t word) {
  return word & ~(std::uint32_t{0x1F} << 15);
}

std::size_t Slot(int actor) { return static_cast<std::size_t>(actor); }

struct FakeState {
  ai::ReactionSettings settings;
  std::array<std::uint32_t, ai::kRelationActorSlots> relations{};
};

class FakeReactionHost final : public ai::ActorReactionHost {
 public:
  [[nodiscard]] int ActorId() const override { return id; }
  [[nodiscard]] std::uint8_t BehaviorMode() const override { return mode; }
  void SetBehaviorMode(std::uint8_t value) override { mode = value; }
  [[nodiscard]] ai::ReactionSettings Settings() const override {
    return own.settings;
  }
  [[nodiscard]] std::uint32_t Relation(int target) const override {
    return own.relations.at(Slot(target));
  }
  void SetRelation(int target, std::uint32_t word) override {
    own.relations.at(Slot(target)) = word;
  }
  [[nodiscard]] std::optional<ai::ReactionSettings> PeerSettings(
      int actor) const override {
    const FakeState* peer = Peer(actor);
    if (peer == nullptr) return std::nullopt;
    return peer->settings;
  }
  [[nodiscard]] std::optional<std::uint32_t> PeerRelation(
      int actor, int target) const override {
    const FakeState* peer = Peer(actor);
    if (peer == nullptr) return std::nullopt;
    return peer->relations.at(Slot(target));
  }
  void SetPeerRelation(int actor, int target, std::uint32_t word) override {
    if (actor == id) {
      own.relations.at(Slot(target)) = word;
      return;
    }
    std::optional<FakeState>& peer = peers.at(Slot(actor));
    if (!peer.has_value()) {
      calls.emplace_back("missing peer write");
      return;
    }
    peer->relations.at(Slot(target)) = word;
  }
  [[nodiscard]] bool IsBeneficial(int /*action*/) const override {
    return beneficial;
  }
  [[nodiscard]] bool IsHarmful(int /*action*/) const override {
    return harmful;
  }
  void TryBeginAlliance() override { calls.emplace_back("begin alliance"); }
  void EndAlliance() override { calls.emplace_back("end alliance"); }
  void MakeHostile(int target) override {
    calls.push_back("hostile " + std::to_string(target));
    // MakeTargetHostile's classification, where a test relies on it.
    if (hostility_reclassifies) {
      std::uint32_t& word = own.relations.at(Slot(target));
      word = (word & ~std::uint32_t{0xC0}) | (kHostile << 6);
    }
  }
  void StopArenaOrbit() override { calls.emplace_back("stop orbit"); }

  // The peer state as the AI state table holds it, including this actor's.
  [[nodiscard]] const FakeState* Peer(int actor) const {
    if (actor == id) return &own;
    const std::optional<FakeState>& peer = peers.at(Slot(actor));
    return peer.has_value() ? &*peer : nullptr;
  }
  FakeState& AddPeer(int actor) { return peers.at(Slot(actor)).emplace(); }

  int id = kOwn;
  FakeState own;
  std::array<std::optional<FakeState>, ai::kRelationActorSlots> peers{};
  std::uint8_t mode = 0;
  bool beneficial = false;
  bool harmful = false;
  bool hostility_reclassifies = false;
  Calls calls;
};

// An action by `source` hitting each of `targets`.
ai::AppliedAction Action(int source, const Hits& targets,
                         std::array<std::uint8_t, 2> prerequisites = {0, 0}) {
  return {.action = 42,
          .hit_prerequisites = prerequisites,
          .acting = {.actor = source},
          .targets = std::span(targets)};
}

class FakeNotificationHost final : public ai::ActionNotificationHost {
 public:
  void ClearReengagementGrace(int actor) override {
    calls.push_back("grace " + std::to_string(actor));
  }
  void ApplyRelationshipChanges() override { calls.emplace_back("apply"); }

  Calls calls;
};
}  // namespace

TEST_CASE(
    "Action reaction ignores a skipped acting entry, Ashley and prerequisite "
    "3 actions",
    "[ai][relations][reaction]") {
  const int ignored = GENERATE(0, 1, 2, 3);
  FakeReactionHost host;
  host.mode = 1;
  host.beneficial = host.harmful = true;
  host.own.settings.settings_bit25 = true;
  const Hits hits{{.actor = kOwn}, {.actor = 0}};
  ai::AppliedAction applied = Action(2, hits);
  if (ignored == 0) applied.acting.skipped = true;
  if (ignored == 1) host.id = 0;
  if (ignored == 2) {
    applied.hit_prerequisites = std::array<std::uint8_t, 2>{3, 0};
  }
  if (ignored == 3) {
    applied.hit_prerequisites = std::array<std::uint8_t, 2>{0, 3};
  }
  CHECK(ai::ApplyActorActionReaction(host, applied));
  CHECK(host.mode == 1);
  CHECK(host.own.relations.at(2) == 0);
  CHECK(host.calls.empty());
}

TEST_CASE(
    "Action reaction refuses an action outside the table and an acting actor "
    "outside the slots",
    "[ai][relations][reaction]") {
  FakeReactionHost host;
  host.mode = 1;
  const Hits hits{{.actor = kOwn}};
  ai::AppliedAction outside_table = Action(2, hits);
  outside_table.hit_prerequisites.reset();
  CHECK_FALSE(ai::ApplyActorActionReaction(host, outside_table));
  CHECK_FALSE(ai::ApplyActorActionReaction(host, Action(16, hits)));
  CHECK(host.mode == 1);
  // A hit on another actor never reads the acting actor.
  const Hits elsewhere{{.actor = 5}};
  CHECK(ai::ApplyActorActionReaction(host, Action(16, elsewhere)));
}

TEST_CASE(
    "Action reaction releases a holding actor and restarts the acting "
    "actor's timer for each hit on itself",
    "[ai][relations][reaction]") {
  const auto [mode, released] =
      GENERATE(table<std::uint8_t, std::uint8_t>({{0, 0}, {1, 2}, {3, 3}}));
  FakeReactionHost host;
  host.mode = mode;
  host.own.relations.at(2) = Word(kNeutral, 5, 1, true);
  const Hits hits{{.actor = kOwn, .skipped = true}, {.actor = 5}};
  CHECK(ai::ApplyActorActionReaction(host, Action(2, hits)));
  CHECK(host.mode == mode);
  CHECK(TimerOf(host.own.relations.at(2)) == 0);
  const Hits on_self{{.actor = 5}, {.actor = kOwn}, {.actor = kOwn}};
  CHECK(ai::ApplyActorActionReaction(host, Action(2, on_self)));
  CHECK(host.mode == released);
  CHECK(TimerOf(host.own.relations.at(2)) == 28);
  CHECK(Untimed(host.own.relations.at(2)) == Word(kNeutral, 5, 1, true));
  // The actor never reacts to its own action.
  CHECK(ai::ApplyActorActionReaction(host, Action(kOwn, on_self)));
  CHECK(host.calls.empty());
}

TEST_CASE("Ashley's help begins an alliance the actor allows",
          "[ai][relations][reaction]") {
  const auto [allied, allows, begins] = GENERATE(table<bool, bool, bool>(
      {{false, true, true}, {true, true, false}, {false, false, false}}));
  FakeReactionHost host;
  host.beneficial = true;
  host.own.settings.allied_with_ashley = allied;
  host.own.settings.settings_bit27 = allows;
  host.AddPeer(0).relations.at(0) = Word(kAlly);
  const Hits hits{{.actor = kOwn}};
  CHECK(ai::ApplyActorActionReaction(host, Action(0, hits)));
  CHECK(host.calls == (begins ? Calls{"begin alliance"} : Calls{}));
  CHECK(TimerOf(host.own.relations.at(0)) == 28);
}

TEST_CASE(
    "Help from an actor hostile to Ashley ends the alliance and needs that "
    "actor's state",
    "[ai][relations][reaction]") {
  const unsigned toward_ashley = GENERATE(kNeutral, kAlly, kHostile);
  FakeReactionHost host;
  host.beneficial = true;
  host.AddPeer(2).relations.at(0) = Word(toward_ashley);
  const Hits hits{{.actor = kOwn}};
  CHECK(ai::ApplyActorActionReaction(host, Action(2, hits)));
  CHECK(host.calls ==
        (toward_ashley == kHostile ? Calls{"end alliance"} : Calls{}));
  host.peers.at(2).reset();
  CHECK_FALSE(ai::ApplyActorActionReaction(host, Action(2, hits)));
}

TEST_CASE("Ashley's damage stops the arena orbit unless it restores HP",
          "[ai][relations][reaction]") {
  const auto [effect, hp, stops] =
      GENERATE(table<unsigned, int, bool>({{1, 5, true},
                                           {3, 5, true},
                                           {0, 1, true},
                                           {2, 5, false},
                                           {1, 0, false},
                                           {1, -3, false}}));
  FakeReactionHost host;
  host.harmful = true;
  host.own.settings.settings_bit25 = true;
  const Hits hits{{.actor = kOwn,
                   .hp_change = static_cast<std::int16_t>(hp),
                   .hp_effect = effect}};
  CHECK(ai::ApplyActorActionReaction(host, Action(0, hits)));
  CHECK(host.calls ==
        (stops ? Calls{"stop orbit", "hostile 0"} : Calls{"hostile 0"}));
}

TEST_CASE(
    "Ashley's harm ends an alliance the actor allows before it turns hostile",
    "[ai][relations][reaction]") {
  const bool allows = GENERATE(false, true);
  FakeReactionHost host;
  host.harmful = true;
  host.own.settings.settings_bit25 = true;
  host.own.settings.settings_bit27 = allows;
  const Hits hits{{.actor = kOwn}};
  CHECK(ai::ApplyActorActionReaction(host, Action(0, hits)));
  CHECK(host.calls ==
        (allows ? Calls{"end alliance", "hostile 0"} : Calls{"hostile 0"}));
}

TEST_CASE(
    "Settings bit 25 makes the attacker hostile at once, only Ashley with bit "
    "26",
    "[ai][relations][reaction]") {
  const auto [bit26, source, at_once] = GENERATE(table<bool, int, bool>(
      {{false, 2, true}, {true, 0, true}, {true, 2, false}}));
  FakeReactionHost host;
  host.harmful = true;
  host.own.settings.settings_bit25 = true;
  host.own.settings.settings_bit26 = bit26;
  host.own.relations.at(Slot(source)) = Word(kNeutral);
  host.AddPeer(source);
  const Hits hits{{.actor = kOwn}};
  CHECK(ai::ApplyActorActionReaction(host, Action(source, hits)));
  CHECK(host.calls == Calls{"hostile " + std::to_string(source)});
  CHECK(CounterOf(host.own.relations.at(Slot(source))) ==
        (at_once ? 0u : 0x7Fu));
}

TEST_CASE("Harm settles a grievance the attacker holds against the actor",
          "[ai][relations][reaction]") {
  FakeReactionHost host;
  host.harmful = true;
  host.own.settings.settings_bit26 = true;
  host.own.relations.at(2) = Word(kNeutral);
  host.AddPeer(2).relations.at(kOwn) = Word(kHostile, 5, kAlly, true);
  const Hits hits{{.actor = kOwn}};
  CHECK(ai::ApplyActorActionReaction(host, Action(2, hits)));
  const FakeState* attacker = host.Peer(2);
  REQUIRE(attacker != nullptr);
  CHECK(attacker->relations.at(kOwn) == Word(kAlly, 0, kAlly, true));
  CHECK(Untimed(host.own.relations.at(2)) == Word(kNeutral));
  CHECK(host.calls.empty());
}

TEST_CASE("Harm records a grievance against the attacker",
          "[ai][relations][reaction]") {
  const bool bit26 = GENERATE(false, true);
  FakeReactionHost host;
  host.harmful = true;
  host.own.settings.settings_bit26 = bit26;
  host.own.relations.at(2) = Word(kNeutral, 0, 0, true);
  host.AddPeer(2).relations.at(kOwn) = Word(kAlly, 0, kHostile);
  const Hits hits{{.actor = kOwn}};
  CHECK(ai::ApplyActorActionReaction(host, Action(2, hits)));
  const std::uint32_t grievance = host.own.relations.at(2);
  CHECK(ClassOf(grievance) == kNeutral);
  CHECK(CounterOf(grievance) == 0x7F);
  CHECK(PreviousOf(grievance) == kNeutral);
  const FakeState* attacker = host.Peer(2);
  REQUIRE(attacker != nullptr);
  CHECK(attacker->relations.at(kOwn) == Word(kAlly, 0, kHostile));
  CHECK(host.calls == (bit26 ? Calls{"hostile 2"} : Calls{}));
}

TEST_CASE(
    "Harm from a hostile attacker reads nothing more, otherwise it needs the "
    "attacker's state",
    "[ai][relations][reaction]") {
  FakeReactionHost host;
  host.harmful = true;
  host.own.relations.at(2) = Word(kHostile, 3);
  const Hits hits{{.actor = kOwn}};
  CHECK(ai::ApplyActorActionReaction(host, Action(2, hits)));
  CHECK(Untimed(host.own.relations.at(2)) == Word(kHostile, 3));
  host.own.relations.at(2) = Word(kNeutral);
  CHECK_FALSE(ai::ApplyActorActionReaction(host, Action(2, hits)));
  CHECK(Untimed(host.own.relations.at(2)) == Word(kNeutral));
  CHECK(host.calls.empty());
}

TEST_CASE(
    "Relationship propagation does nothing without an alliance or shared "
    "hostility",
    "[ai][relations][propagation]") {
  FakeReactionHost host;
  const Room room{0, 2, 99};
  CHECK(ai::PropagateActorRelationships(host, room));
  CHECK(host.own.relations == FakeState{}.relations);
  CHECK(host.calls.empty());
}

TEST_CASE("An Ashley ally reclassifies every room actor",
          "[ai][relations][propagation]") {
  FakeReactionHost host;
  host.own.settings.allied_with_ashley = true;
  host.own.settings.shares_ally_hostility = true;
  host.AddPeer(2).settings.category = 3;
  host.AddPeer(3).relations.at(0) = Word(kHostile);
  FakeState& ally = host.AddPeer(5);
  ally.settings.allied_with_ashley = true;
  ally.relations.at(0) = Word(kHostile);
  host.AddPeer(6).relations.at(0) = Word(kNeutral);
  host.own.relations.at(3) = Word(kAlly, 9, kNeutral);
  host.own.relations.at(5) = Word(kHostile, 9);
  const Room room{0, kOwn, 2, 3, 5, 6};
  CHECK(ai::PropagateActorRelationships(host, room));
  const auto& relations = host.own.relations;
  CHECK(ClassOf(relations.at(0)) == kAlly);
  CHECK(ClassOf(relations.at(kOwn)) == kAlly);
  CHECK(ClassOf(relations.at(2)) == kAlly);
  CHECK(relations.at(3) == Word(kHostile, 0, kNeutral));
  CHECK(relations.at(5) == Word(kNeutral, 9));
  CHECK(ClassOf(relations.at(6)) == kNeutral);
  CHECK(host.calls.empty());
}

TEST_CASE(
    "Alliance reclassification refuses IDs outside the slots and a missing "
    "room actor state",
    "[ai][relations][propagation]") {
  FakeReactionHost host;
  host.own.settings.allied_with_ashley = true;
  host.AddPeer(6);
  host.own.relations.at(6) = Word(kAlly);
  const Room outside{6, 16};
  CHECK_FALSE(ai::PropagateActorRelationships(host, outside));
  CHECK(host.own.relations.at(6) == Word(kAlly));
  // The original reclassifies in list order, so a later gap stops part-way.
  const Room missing{6, 2};
  CHECK_FALSE(ai::PropagateActorRelationships(host, missing));
  CHECK(ClassOf(host.own.relations.at(6)) == kNeutral);
}

TEST_CASE(
    "Shared hostility turns on an active neutral actor a mutual ally is "
    "hostile to, unless another ally defends it",
    "[ai][relations][propagation]") {
  const auto [defender_view, defended] =
      GENERATE(table<std::uint32_t, bool>({{Word(kNeutral), false},
                                           {Word(kAlly), true},
                                           {Word(kHostile, 4, kAlly), true},
                                           {Word(kHostile, 4, kNeutral), false},
                                           {Word(kHostile, 0, kAlly), false}}));
  FakeReactionHost host;
  host.hostility_reclassifies = true;
  host.own.settings.shares_ally_hostility = true;
  host.own.relations.at(1) = Word(kAlly);
  host.own.relations.at(5) = Word(kAlly);
  host.own.relations.at(2) = Word(kNeutral, 0, 0, true);
  host.own.relations.at(3) = Word(kNeutral, 0, 0, false);
  FakeState& mutual = host.AddPeer(1);
  mutual.relations.at(kOwn) = Word(kAlly);
  mutual.relations.at(2) = Word(kHostile);
  mutual.relations.at(3) = Word(kHostile);
  FakeState& defender = host.AddPeer(5);
  defender.relations.at(kOwn) = Word(kNeutral);
  defender.relations.at(2) = Word(kAlly);
  host.AddPeer(2);
  host.AddPeer(3);
  const Room room{kOwn, 1, 2, 3, 5};
  CHECK(ai::PropagateActorRelationships(host, room));
  // A defender that is not a mutual ally does not count.
  CHECK(host.calls == Calls{"hostile 2"});
  host.calls.clear();
  host.own.relations.at(2) = Word(kNeutral, 0, 0, true);
  defender.relations.at(kOwn) = Word(kAlly);
  defender.relations.at(2) = defender_view;
  CHECK(ai::PropagateActorRelationships(host, room));
  CHECK(host.calls == (defended ? Calls{} : Calls{"hostile 2"}));
}

TEST_CASE("Shared hostility refuses a missing ally state",
          "[ai][relations][propagation]") {
  FakeReactionHost host;
  host.own.settings.shares_ally_hostility = true;
  host.own.relations.at(1) = Word(kAlly);
  const Room room{1};
  CHECK_FALSE(ai::PropagateActorRelationships(host, room));
  const Room outside{1, 16};
  host.AddPeer(1);
  CHECK_FALSE(ai::PropagateActorRelationships(host, outside));
  CHECK(host.calls.empty());
}

TEST_CASE(
    "Action notification lets the acting actor engage again, then applies "
    "the relationship changes",
    "[ai][relations][notification]") {
  FakeNotificationHost host;
  CHECK(ai::NotifyActionRelationshipChanges(host, {.actor = 3}));
  CHECK(host.calls == Calls{"grace 3", "apply"});
  host.calls.clear();
  CHECK(ai::NotifyActionRelationshipChanges(host,
                                            {.actor = 16, .skipped = true}));
  CHECK(host.calls == Calls{"apply"});
  host.calls.clear();
  CHECK_FALSE(ai::NotifyActionRelationshipChanges(host, {.actor = 16}));
  CHECK_FALSE(ai::NotifyActionRelationshipChanges(host, {.actor = -1}));
  CHECK(host.calls.empty());
}
