#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "Battle/Ai/ActorCleanup.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/Navigation.h"
}

namespace {
namespace ai = VS::Battle::Ai;
using Events = std::vector<std::string>;
constexpr std::size_t kGridCells = std::size_t{32} * 32;

constexpr std::uint32_t Tile(unsigned x, unsigned contact, unsigned z) {
  return x | (contact << 8) | (z << 16) | (0x99u << 24);
}

std::size_t Cell(unsigned x, unsigned z) {
  return std::size_t{z} * 32 + std::size_t{x};
}

std::string Event(const char* name, int actor_id) {
  return std::string(name) + " " + std::to_string(actor_id);
}

ai::CleanupRoom FullRoom() {
  return ai::CleanupRoom{.max_x = 31, .max_z = 31, .min_x = 0, .min_z = 0};
}

class FakeCleanupHost final : public ai::ActorCleanupHost {
 public:
  FakeCleanupHost() { links.fill(ai::kNoLinkedActor); }

  [[nodiscard]] bool HasTileFlagMap() const override { return has_grid; }
  [[nodiscard]] std::span<std::uint16_t> TileFlags() override {
    if (!has_grid) return {};
    return grid;
  }
  [[nodiscard]] bool HasAiState(int actor_id) const override {
    return states.at(Slot(actor_id)).has_value();
  }
  [[nodiscard]] std::optional<ai::ReservationState> Reservations(
      int actor_id) const override {
    return states.at(Slot(actor_id));
  }
  void StoreReservations(int actor_id,
                         const ai::ReservationState& state) override {
    states.at(Slot(actor_id)) = state;
    events.push_back(Event("store-reservations", actor_id));
  }
  [[nodiscard]] std::optional<ai::CleanupRoom> Room() const override {
    return room;
  }
  void StoreRoom(const ai::CleanupRoom& value) override { room = value; }
  [[nodiscard]] std::optional<int> LinkedActor(int actor_id) const override {
    if (!HasAiState(actor_id)) return std::nullopt;
    return links.at(Slot(actor_id));
  }
  void SetLinkedActor(int actor_id, int linked) override {
    links.at(Slot(actor_id)) = linked;
  }
  [[nodiscard]] std::optional<std::uint8_t> CommandOpcode(
      int actor_id) const override {
    return opcodes.at(Slot(actor_id));
  }
  [[nodiscard]] std::optional<ai::ModelMotion> RecordMotion(
      int actor_id) const override {
    return motions.at(Slot(actor_id));
  }
  [[nodiscard]] std::optional<std::uint8_t> StateModelSupport(
      int actor_id) const override {
    ++support_reads;
    return supports.at(Slot(actor_id));
  }
  [[nodiscard]] std::optional<ai::PersistenceFields> Persistence(
      int actor_id) const override {
    if (!HasAiState(actor_id)) return std::nullopt;
    return persistence;
  }
  [[nodiscard]] std::uint32_t AttributeActorMask() const override {
    return attribute_mask;
  }
  void StoreAttributeActorMask(std::uint32_t mask) override {
    attribute_mask = mask;
  }
  [[nodiscard]] int BattleActorSlots() const override { return battle_slots; }
  [[nodiscard]] bool HasNavigationOverride(int actor_id) const override {
    return overrides.at(Slot(actor_id));
  }
  void StoreCommandFlags(ai::BattleActorHandle actor,
                         std::uint8_t flags) override {
    events.emplace_back("flags");
    stored_flags.emplace_back(actor, flags);
  }
  void FreeNavigationOverride(int actor_id) override {
    overrides.at(Slot(actor_id)) = false;
    events.push_back(Event("free-override", actor_id));
  }
  void ClearContactOccupancy(int contact) override {
    contacts.push_back(contact);
  }
  void Disengage(int actor_id) override {
    events.push_back(Event("disengage", actor_id));
  }
  void ResetAwarenessIndicator(int actor_id) override {
    events.push_back(Event("reset-awareness", actor_id));
  }
  void UnlinkEngagedActor(int actor_id) override {
    events.push_back(Event("unlink", actor_id));
  }
  void RetireAiState(int actor_id) override {
    states.at(Slot(actor_id)).reset();
    events.push_back(Event("retire", actor_id));
  }
  void FreeOrbitingBossState() override { events.emplace_back("free-orbit"); }
  bool AllocateOrbitingBossState(std::int16_t cooldown) override {
    cooldowns.push_back(cooldown);
    return allocates;
  }

  bool has_grid = true;
  std::vector<std::uint16_t> grid =
      std::vector<std::uint16_t>(kGridCells, 0xFFFF);
  std::array<std::optional<ai::ReservationState>, ai::kCleanupActorSlots>
      states{};
  std::optional<ai::CleanupRoom> room = FullRoom();
  std::array<int, ai::kCleanupActorSlots> links{};
  std::array<std::optional<std::uint8_t>, ai::kCleanupActorSlots> opcodes{};
  std::array<std::optional<ai::ModelMotion>, ai::kCleanupActorSlots> motions{};
  std::array<std::optional<std::uint8_t>, ai::kCleanupActorSlots> supports{};
  ai::PersistenceFields persistence{};
  std::uint32_t attribute_mask = 0;
  int battle_slots = 24;
  std::array<bool, ai::kCleanupActorSlots> overrides{};
  bool allocates = true;
  mutable int support_reads = 0;
  Events events;
  std::vector<std::pair<ai::BattleActorHandle, std::uint8_t>> stored_flags;
  std::vector<int> contacts;
  std::vector<std::int16_t> cooldowns;

 private:
  static std::size_t Slot(int actor_id) {
    return static_cast<std::size_t>(actor_id);
  }
};

class FakeReactionDelay final : public ai::ReactionDelayHost {
 public:
  int NextRandomMasked(int range) override {
    ranges.push_back(range);
    return roll;
  }
  void SetCommandFlags(std::uint8_t value) override { flags.push_back(value); }

  int roll = 0;
  std::vector<int> ranges;
  std::vector<std::uint8_t> flags;
};

class FakeHostility final : public ai::HostilityHost {
 public:
  void DrawBiosRandom() override { calls.emplace_back("draw"); }
  void ShowAwarenessIndicator(ai::HostilityState& state, int indicator,
                              int frames) override {
    calls.push_back("show " + std::to_string(indicator) + " " +
                    std::to_string(frames) + " " +
                    std::to_string(state.alerted));
  }
  void RequestReaction(ai::HostilityState& state, int value,
                       int range) override {
    calls.push_back("react " + std::to_string(value) + " " +
                    std::to_string(range));
    state.relations.at(3) |= 0x10000u;
  }

  Events calls;
};

ai::ReservationState Reserving(unsigned x, unsigned z) {
  return ai::ReservationState{.navigation_reset = 1,
                              .next_tile = Tile(x, 0, z)};
}

bool Reserved(const FakeCleanupHost& host, unsigned x, unsigned z) {
  return (host.grid.at(Cell(x, z)) & 0x80) != 0;
}

// The value of an optional a test has established must be present.
template <class Optional>
auto& Engaged(Optional& optional) {
  if (!optional.has_value()) throw std::runtime_error("expected a value");
  return *optional;
}
}  // namespace

TEST_CASE("Destination tile release clears only the reservation bit",
          "[ai][cleanup]") {
  const auto [x, z, valid] = GENERATE(table<int, int, bool>({{0, 0, true},
                                                             {31, 0, true},
                                                             {0, 31, true},
                                                             {31, 31, true},
                                                             {-1, 0, false},
                                                             {0, -1, false},
                                                             {32, 0, false},
                                                             {0, 32, false}}));
  std::vector<std::uint16_t> grid(kGridCells, 0xFFFF);
  CHECK(ai::ReleaseAiDestinationTile(grid, x, z) == valid);
  for (std::size_t index = 0; index < grid.size(); ++index) {
    const bool released = valid && index == Cell(static_cast<unsigned>(x),
                                                 static_cast<unsigned>(z));
    CHECK(grid[index] == (released ? 0xFF7F : 0xFFFF));
  }
  // Without a grid nothing is released and nothing refuses.
  CHECK(ai::ReleaseAiDestinationTile({}, x, z));
}

TEST_CASE("Destination tile release refuses a cell past a short grid",
          "[ai][cleanup]") {
  std::vector<std::uint16_t> grid(32, 0xFFFF);
  CHECK(ai::ReleaseAiDestinationTile(grid, 31, 0));
  CHECK(grid[31] == 0xFF7F);
  CHECK_FALSE(ai::ReleaseAiDestinationTile(grid, 0, 1));
}

TEST_CASE("Reservation release honours the room's inclusive tile range",
          "[ai][cleanup]") {
  // Room bytes 0-3: X up to 10, Z up to 12, X from 4 and Z from 6.
  const auto [x, z, released] =
      GENERATE(table<unsigned, unsigned, bool>({{3, 8, false},
                                                {4, 8, true},
                                                {10, 8, true},
                                                {11, 8, false},
                                                {7, 5, false},
                                                {7, 6, true},
                                                {7, 12, true},
                                                {7, 13, false}}));
  ai::ReservationState state = Reserving(x, z);
  ai::CleanupRoom room{.max_x = 10, .max_z = 12, .min_x = 4, .min_z = 6};
  std::vector<std::uint16_t> grid(kGridCells, 0xFFFF);
  REQUIRE(ai::ReleaseActorAiReservations(state, room, grid));
  CHECK(state.navigation_reset == 0);
  CHECK(grid.at(Cell(x, z)) == (released ? 0xFF7F : 0xFFFF));
}

TEST_CASE("Route gate holders release the room flag the gate reserves",
          "[ai][cleanup]") {
  const auto [flags, kept] = GENERATE(table<std::uint8_t, std::uint8_t>(
      {{0xE3, 0xE0}, {0x45, 0x40}, {0x00, 0x00}, {0x1F, 0x00}, {0xFF, 0x00}}));
  const bool holds = GENERATE(true, false);
  ai::ReservationState state = Reserving(40, 40);
  state.holds_route_gate = holds;
  ai::CleanupRoom room = FullRoom();
  room.flags = flags;
  REQUIRE(ai::ReleaseActorAiReservations(state, room, {}));
  CHECK(room.flags == (holds ? kept : flags));
  CHECK(state.navigation_reset == 0);
}

TEST_CASE("Reservation release refuses a room range outside the grid",
          "[ai][cleanup]") {
  ai::ReservationState state = Reserving(35, 2);
  state.holds_route_gate = true;
  ai::CleanupRoom room{.max_x = 40, .max_z = 40, .min_x = 0, .min_z = 0};
  room.flags = 0xE3;
  std::vector<std::uint16_t> grid(kGridCells, 0xFFFF);
  CHECK_FALSE(ai::ReleaseActorAiReservations(state, room, grid));
  CHECK(room.flags == 0xE3);
  // Without a grid the same range releases nothing and does not refuse.
  CHECK(ai::ReleaseActorAiReservations(state, room, {}));
  CHECK(room.flags == 0xE0);
}

TEST_CASE("Forced reservation release flags the navigation for a reset",
          "[ai][cleanup]") {
  FakeCleanupHost host;
  const int invalid = GENERATE(-1, 16);
  CHECK_FALSE(ai::ForceReleaseActorAiReservations(host, invalid));
  CHECK(ai::ForceReleaseActorAiReservations(host, 3));
  CHECK(host.events.empty());

  host.states.at(3) = Reserving(5, 7);
  Engaged(host.states.at(3)).navigation_reset = 0;
  host.room.reset();
  CHECK_FALSE(ai::ForceReleaseActorAiReservations(host, 3));
  CHECK(Reserved(host, 5, 7));

  host.room = FullRoom();
  REQUIRE(ai::ForceReleaseActorAiReservations(host, 3));
  CHECK(Engaged(host.states.at(3)).navigation_reset == 1);
  CHECK_FALSE(Reserved(host, 5, 7));
  CHECK(host.events == Events{"store-reservations 3"});
}

TEST_CASE("Reservation refresh releases only after finishing commands",
          "[ai][cleanup]") {
  const int opcode = GENERATE(range(0, 32), 43, 255);
  FakeCleanupHost host;
  host.states.at(4) = Reserving(5, 7);
  host.opcodes.at(4) = static_cast<std::uint8_t>(opcode);
  host.motions.at(4) = ai::ModelMotion{.movement = 0x20, .reaction = 0xF4};
  const int command = opcode & 31;
  const bool releases = command == 11 || command == 12 || command == 18 ||
                        command == 21 || command == 23;
  REQUIRE(ai::RefreshActorAiReservations(host, 4));
  CHECK(Reserved(host, 5, 7) == !releases);
  CHECK(Engaged(host.states.at(4)).navigation_reset == (releases ? 0 : 1));
}

TEST_CASE("Reservation refresh keeps a moving or reacting actor's tile",
          "[ai][cleanup]") {
  const auto [movement, reaction, keeps] =
      GENERATE(table<std::uint8_t, std::uint8_t, bool>({{0x01, 0, true},
                                                        {0x10, 0, true},
                                                        {0x1F, 0, true},
                                                        {0xE0, 0, false},
                                                        {0, 0x01, true},
                                                        {0, 0x02, true},
                                                        {0, 0x08, true},
                                                        {0, 0xF4, false}}));
  FakeCleanupHost host;
  host.states.at(4) = Reserving(5, 7);
  host.opcodes.at(4) = 11;
  host.motions.at(4) =
      ai::ModelMotion{.movement = movement, .reaction = reaction};
  REQUIRE(ai::RefreshActorAiReservations(host, 4));
  CHECK(Reserved(host, 5, 7) == keeps);
}

TEST_CASE("Reservation refresh needs the actor's record and the room",
          "[ai][cleanup]") {
  FakeCleanupHost host;
  CHECK_FALSE(ai::RefreshActorAiReservations(host, 16));
  // No battle actor: nothing to refresh.
  CHECK(ai::RefreshActorAiReservations(host, 4));

  host.opcodes.at(4) = 12;
  CHECK_FALSE(ai::RefreshActorAiReservations(host, 4));

  host.motions.at(4) = ai::ModelMotion{};
  CHECK(ai::RefreshActorAiReservations(host, 4));
  CHECK(host.events.empty());

  host.states.at(4) = Reserving(5, 7);
  host.room.reset();
  CHECK_FALSE(ai::RefreshActorAiReservations(host, 4));
  CHECK(Reserved(host, 5, 7));
}

TEST_CASE("Navigation override release refuses actors outside the slots",
          "[ai][cleanup]") {
  FakeCleanupHost host;
  const int invalid = GENERATE(-1, 16);
  CHECK_FALSE(ai::FreeActorNavigationOverride(host, invalid));
  CHECK(host.events.empty());
}

TEST_CASE("Navigation override release needs a grid and an AI state",
          "[ai][cleanup]") {
  FakeCleanupHost host;
  host.overrides.at(6) = true;
  CHECK(ai::FreeActorNavigationOverride(host, 6));
  CHECK(host.overrides.at(6));

  host.states.at(6) = Reserving(1, 1);
  host.has_grid = false;
  CHECK(ai::FreeActorNavigationOverride(host, 6));
  CHECK(host.overrides.at(6));
  CHECK(host.events.empty());
}

TEST_CASE("Navigation override release frees an existing override once",
          "[ai][cleanup]") {
  FakeCleanupHost host;
  host.overrides.at(6) = true;
  host.states.at(6) = Reserving(1, 1);
  CHECK(ai::FreeActorNavigationOverride(host, 6));
  CHECK_FALSE(host.overrides.at(6));
  CHECK(ai::FreeActorNavigationOverride(host, 6));
  CHECK(host.events == Events{"free-override 6"});
}

TEST_CASE("Contact release needs both a battle actor and an AI state",
          "[ai][cleanup]") {
  FakeCleanupHost host;
  CHECK_FALSE(ai::ReleaseActorAiContactOccupancy(host, 16));
  CHECK(ai::ReleaseActorAiContactOccupancy(host, 2));

  host.opcodes.at(2) = 11;
  CHECK_FALSE(ai::ReleaseActorAiContactOccupancy(host, 2));

  host.opcodes.at(2).reset();
  host.states.at(2) = ai::ReservationState{.next_tile = Tile(1, 9, 1)};
  CHECK_FALSE(ai::ReleaseActorAiContactOccupancy(host, 2));
  CHECK(host.contacts.empty());
}

TEST_CASE("Contact release frees a grounded actor's contact tile",
          "[ai][cleanup]") {
  const auto [opcode, support, cleared] =
      GENERATE(table<std::uint8_t, std::uint8_t, bool>({{11, 0x00, true},
                                                        {14, 0xF0, true},
                                                        {43, 0x00, true},
                                                        {11, 0x01, false},
                                                        {14, 0x0F, false},
                                                        {12, 0x00, false},
                                                        {18, 0x00, false}}));
  FakeCleanupHost host;
  host.opcodes.at(2) = opcode;
  host.states.at(2) = ai::ReservationState{.next_tile = Tile(1, 9, 1)};
  host.supports.at(2) = support;
  REQUIRE(ai::ReleaseActorAiContactOccupancy(host, 2));
  CHECK(host.contacts == (cleared ? std::vector<int>{9} : std::vector<int>{}));
}

TEST_CASE("Contact release tests the contact tile before the model",
          "[ai][cleanup]") {
  FakeCleanupHost host;
  host.opcodes.at(2) = 14;
  host.states.at(2) = ai::ReservationState{.next_tile = Tile(1, 0, 1)};
  CHECK(ai::ReleaseActorAiContactOccupancy(host, 2));
  CHECK(host.support_reads == 0);

  Engaged(host.states.at(2)).next_tile = Tile(1, 9, 1);
  CHECK_FALSE(ai::ReleaseActorAiContactOccupancy(host, 2));
  CHECK(host.support_reads == 1);
  CHECK(host.contacts.empty());
}

TEST_CASE("Link clearing refuses actors outside the slots", "[ai][cleanup]") {
  FakeCleanupHost host;
  const int invalid = GENERATE(-1, 16);
  CHECK_FALSE(ai::ClearActorAiLinksAndPlan(host, invalid));
  CHECK(host.events.empty());
}

TEST_CASE("Link clearing unlinks followers before disengaging",
          "[ai][cleanup]") {
  FakeCleanupHost host;
  for (const std::size_t slot : {1u, 2u, 3u, 5u}) {
    host.states.at(slot) = ai::ReservationState{};
  }
  host.links.at(1) = 5;
  host.links.at(2) = 6;
  host.links.at(3) = 5;
  host.links.at(4) = 5;  // no AI state: not a follower
  REQUIRE(ai::ClearActorAiLinksAndPlan(host, 5));
  const std::vector<int> expected{ai::kNoLinkedActor, ai::kNoLinkedActor, 6,
                                  ai::kNoLinkedActor, 5};
  CHECK(std::vector<int>(host.links.begin(), host.links.begin() + 5) ==
        expected);
  CHECK(host.events == Events{"disengage 5"});
}

TEST_CASE("Link clearing for an actor without AI only unlinks followers",
          "[ai][cleanup]") {
  FakeCleanupHost host;
  host.states.at(2) = ai::ReservationState{};
  host.links.at(2) = 6;
  REQUIRE(ai::ClearActorAiLinksAndPlan(host, 6));
  CHECK(host.links.at(2) == ai::kNoLinkedActor);
  CHECK(host.events.empty());
}

TEST_CASE("Persistence flags pack the AI state's save fields",
          "[ai][cleanup]") {
  FakeCleanupHost host;
  CHECK_FALSE(ai::PackActorAiPersistenceFlags(host, -1).has_value());
  CHECK_FALSE(ai::PackActorAiPersistenceFlags(host, 16).has_value());
  CHECK(ai::PackActorAiPersistenceFlags(host, 7) == -1);

  const auto [delay, relation, mode, packed] =
      GENERATE(table<std::uint8_t, std::uint32_t, std::uint8_t, std::int16_t>(
          {{1, 0x37F00080u, 0, 0x1FFD},
           {0, 0xFFFFFFFFu, 1, 0x1FDE},
           {0, 0x4000003Fu, 2, 0},
           {0x80, 0, 0, 0xA0}}));
  host.states.at(7) = ai::ReservationState{};
  host.persistence = ai::PersistenceFields{.uses_room_action_delay = delay,
                                           .relation_to_ashley = relation,
                                           .behavior_mode = mode};
  CHECK(ai::PackActorAiPersistenceFlags(host, 7) == packed);
}

TEST_CASE("Navigation reset clearing needs an AI state", "[ai][cleanup]") {
  FakeCleanupHost host;
  CHECK_FALSE(ai::ClearActorAiNavigationReset(host, 16));
  CHECK(ai::ClearActorAiNavigationReset(host, 8));
  CHECK(host.events.empty());
  host.states.at(8) = Reserving(1, 1);
  REQUIRE(ai::ClearActorAiNavigationReset(host, 8));
  CHECK(Engaged(host.states.at(8)).navigation_reset == 0);
  CHECK(Engaged(host.states.at(8)).next_tile == Tile(1, 0, 1));
}

TEST_CASE("AI extension hooks do nothing", "[ai][cleanup]") {
  ai::InitializeActorAiExtensionHook();
  ai::InitializeLiveActorAiExtensionHook();
  SUCCEED();
}

TEST_CASE("Orbiting boss allocation starts with its cooldown",
          "[ai][cleanup]") {
  FakeCleanupHost host;
  CHECK(ai::AllocateOrbitingBossState(host));
  host.allocates = false;
  CHECK_FALSE(ai::AllocateOrbitingBossState(host));
  CHECK(host.cooldowns == std::vector<std::int16_t>{900, 900});
}

TEST_CASE("Teardown needs a tile grid and a battle actor slot",
          "[ai][cleanup]") {
  FakeCleanupHost host;
  host.attribute_mask = 0xFFFF;
  host.states.at(2) = Reserving(5, 7);
  ai::TeardownActor actor{.id = 2, .command_flags = 0xFF, .kind = 4};
  host.has_grid = false;
  CHECK(ai::FreeActorAiState(host, actor));
  CHECK(host.events.empty());
  CHECK(host.attribute_mask == 0xFFFF);

  host.has_grid = true;
  actor.id = GENERATE(-1, 24, 100);
  CHECK_FALSE(ai::FreeActorAiState(host, actor));
  CHECK(host.events.empty());
  CHECK(host.attribute_mask == 0xFFFF);
  CHECK(actor.command_flags == 0xFF);
}

namespace {
constexpr ai::BattleActorHandle kDepartingHandle{0x1234};
constexpr ai::BattleActorHandle kOtherHandle{0x5678};

// Actor 2 departs holding tile (5, 7), a navigation override, an engagement
// and two special actor slots; actors 1 and 3 follow actors 2 and 4.
void PrepareDeparture(FakeCleanupHost& host) {
  host.attribute_mask = 0xFFFF;
  host.states.at(1) = ai::ReservationState{};
  host.states.at(2) = Reserving(5, 7);
  host.states.at(3) = ai::ReservationState{};
  host.links.at(1) = 2;
  host.links.at(3) = 4;
  host.overrides.at(2) = true;
  ai::CleanupRoom& room = Engaged(host.room);
  room.engaged_mask = 0x0005;
  room.engaged_count = 2;
  room.special_actors = {kDepartingHandle, kOtherHandle, kDepartingHandle,
                         ai::kNoBattleActor};
}

ai::TeardownActor DepartingActor() {
  return ai::TeardownActor{.handle = kDepartingHandle,
                           .id = 2,
                           .command_flags = 0xFF,
                           .sub_type = 0xAC,
                           .kind = 4};
}
}  // namespace

TEST_CASE("Teardown retires a departing AI actor in the original order",
          "[ai][cleanup]") {
  FakeCleanupHost host;
  PrepareDeparture(host);
  ai::TeardownActor actor = DepartingActor();
  REQUIRE(ai::FreeActorAiState(host, actor));
  CHECK(host.events == Events{"free-override 2", "store-reservations 2",
                              "flags", "reset-awareness 2", "unlink 2",
                              "retire 2", "free-orbit"});
  CHECK(host.attribute_mask == 0xFFFB);
  CHECK(actor.command_flags == 0xFC);
  const std::vector<std::pair<ai::BattleActorHandle, std::uint8_t>> flags{
      {kDepartingHandle, 0xFC}};
  CHECK(host.stored_flags == flags);
  CHECK_FALSE(host.states.at(2).has_value());
}

TEST_CASE("Teardown detaches a departing AI actor from links and the room",
          "[ai][cleanup]") {
  FakeCleanupHost host;
  PrepareDeparture(host);
  ai::TeardownActor actor = DepartingActor();
  REQUIRE(ai::FreeActorAiState(host, actor));
  const std::vector<int> links{ai::kNoLinkedActor, ai::kNoLinkedActor,
                               ai::kNoLinkedActor, 4};
  CHECK(std::vector<int>(host.links.begin(), host.links.begin() + 4) == links);
  CHECK_FALSE(Reserved(host, 5, 7));
  const ai::CleanupRoom& room = Engaged(host.room);
  CHECK(room.engaged_mask == 0x0001);
  CHECK(room.engaged_count == 1);
  const decltype(room.special_actors) specials{
      ai::kNoBattleActor, kOtherHandle, ai::kNoBattleActor, ai::kNoBattleActor};
  CHECK(room.special_actors == specials);
}

TEST_CASE("Teardown keeps Ashley's AI state after releasing her tile",
          "[ai][cleanup]") {
  FakeCleanupHost host;
  host.attribute_mask = 0x0003;
  host.states.at(0) = Reserving(5, 7);
  ai::TeardownActor actor{.id = 0, .command_flags = 0xFF, .kind = 4};
  REQUIRE(ai::FreeActorAiState(host, actor));
  CHECK(host.events == Events{"store-reservations 0"});
  CHECK(host.attribute_mask == 0x0002);
  CHECK_FALSE(Reserved(host, 5, 7));
  CHECK(host.states.at(0).has_value());
  CHECK(actor.command_flags == 0xFF);
}

TEST_CASE("Teardown refuses inconsistent engagement membership",
          "[ai][cleanup]") {
  const auto [has_state, count] =
      GENERATE(table<bool, std::uint8_t>({{true, 0}, {false, 1}}));
  FakeCleanupHost host;
  if (has_state) host.states.at(2) = Reserving(5, 7);
  Engaged(host.room).engaged_mask = 0x0004;
  Engaged(host.room).engaged_count = count;
  ai::TeardownActor actor{.id = 2, .command_flags = 0xFF, .kind = 1};
  CHECK_FALSE(ai::FreeActorAiState(host, actor));
  CHECK(Engaged(host.room).engaged_mask == 0x0004);
  CHECK(actor.command_flags == 0xFF);
  CHECK(host.stored_flags.empty());
  CHECK(host.states.at(2).has_value() == has_state);
}

TEST_CASE("Teardown of an actor without AI only detaches references",
          "[ai][cleanup]") {
  FakeCleanupHost host;
  host.attribute_mask = 0xFFFF;
  host.states.at(1) = ai::ReservationState{};
  host.links.at(1) = 3;
  ai::TeardownActor actor{.id = 3, .command_flags = 0xFF, .kind = 0x10};
  REQUIRE(ai::FreeActorAiState(host, actor));
  CHECK(host.attribute_mask == 0xFFF7);
  CHECK(host.links.at(1) == ai::kNoLinkedActor);
  CHECK(host.events.empty());
  CHECK(actor.command_flags == 0xFF);

  // Actor 0 leaves the attribute mask only as an attribute kind.
  host.attribute_mask = 0xFFFF;
  actor = ai::TeardownActor{.id = 0, .kind = 0x08};
  REQUIRE(ai::FreeActorAiState(host, actor));
  CHECK(host.attribute_mask == 0xFFFF);
  actor.kind = 0x10;
  REQUIRE(ai::FreeActorAiState(host, actor));
  CHECK(host.attribute_mask == 0xFFFE);
}

TEST_CASE("Teardown refuses AI kinds outside the AI slots", "[ai][cleanup]") {
  const auto [kind, accepted] = GENERATE(table<std::uint8_t, bool>(
      {{0x08, true}, {0x10, false}, {0x04, false}, {0x01, false}}));
  FakeCleanupHost host;
  ai::TeardownActor actor{.id = 20, .command_flags = 0xFF, .kind = kind};
  CHECK(ai::FreeActorAiState(host, actor) == accepted);
  CHECK(host.events.empty());

  host.room.reset();
  actor.kind = 0x08;
  CHECK_FALSE(ai::FreeActorAiState(host, actor));
}

TEST_CASE("Teardown of a kind-only AI actor still retires its slot",
          "[ai][cleanup]") {
  const auto [sub_type, frees_orbit] = GENERATE(table<std::uint16_t, bool>(
      {{0xAC, true}, {0xAD, true}, {0xAE, false}, {0x2AC, false}}));
  FakeCleanupHost host;
  ai::TeardownActor actor{
      .id = 3, .command_flags = 0x07, .sub_type = sub_type, .kind = 1};
  REQUIRE(ai::FreeActorAiState(host, actor));
  Events expected{"flags", "retire 3"};
  if (frees_orbit) expected.emplace_back("free-orbit");
  CHECK(host.events == expected);
  CHECK(actor.command_flags == 0x04);
}

TEST_CASE("Delayed reactions draw before checking whether one may start",
          "[ai][cleanup]") {
  const auto [overridden, delay, roll, range, starts] = GENERATE(
      table<bool, std::uint8_t, int, int, bool>({{false, 0, 4, 5, true},
                                                 {false, 0, 5, 5, false},
                                                 {false, 0, 0, 0, false},
                                                 {true, 0, 0, 5, false},
                                                 {false, 1, 0, 5, false}}));
  ai::ReactionDelayState state{.has_navigation_override = overridden,
                               .decision_delay = delay};
  FakeReactionDelay host;
  host.roll = roll;
  CHECK(ai::SetActorAiReactionWithDelay(state, host, 0x1FF, range) == starts);
  CHECK(host.ranges == std::vector<int>{range});
  CHECK(state.decision_delay == (starts ? 30 : delay));
  CHECK(host.flags == (starts ? std::vector<std::uint8_t>{0xFF}
                              : std::vector<std::uint8_t>{}));
}

TEST_CASE("Hostility refuses targets outside the actor slots",
          "[ai][cleanup]") {
  ai::HostilityState state;
  FakeHostility host;
  const int invalid = GENERATE(-1, 16);
  CHECK_FALSE(ai::MakeTargetHostile(state, host, invalid));
  CHECK(host.calls.empty());
}

TEST_CASE("Hostility alerts an actor that was not yet hostile",
          "[ai][cleanup]") {
  ai::HostilityState state;
  FakeHostility host;
  state.relations.at(3) = 0x12345678u;
  REQUIRE(ai::MakeTargetHostile(state, host, 3));
  CHECK(host.calls == Events{"draw", "show 6 40 1", "react 2 128"});
  CHECK(state.alerted == 1);
  // The classification is written over the reaction's changes.
  CHECK(state.relations.at(3) == 0x123500F8u);
}

TEST_CASE("Hostility toward a hostile target only rewrites its relation",
          "[ai][cleanup]") {
  ai::HostilityState state;
  FakeHostility host;
  state.relations.at(3) = 0xFFFFFFFFu;
  REQUIRE(ai::MakeTargetHostile(state, host, 3));
  CHECK(host.calls.empty());
  CHECK(state.alerted == 0);
  CHECK(state.relations.at(3) == 0xFFFF80FFu);
}

TEST_CASE("Live destination release uses the live tile grid", "[ai][cleanup]") {
  const auto saved = Battle_Ai_tileFlagMap;
  u_short flags[32][32]{};
  for (auto& row : flags) {
    for (u_short& cell : row) cell = 0xFFFF;
  }
  Battle_Ai_tileFlagMap = flags;
  ai::ReleaseLiveAiDestinationTile(3, 5);
  CHECK(flags[5][3] == 0xFF7F);
  CHECK(flags[3][5] == 0xFFFF);
  Battle_Ai_tileFlagMap = nullptr;
  ai::ReleaseLiveAiDestinationTile(3, 6);
  CHECK(flags[6][3] == 0xFFFF);
  Battle_Ai_tileFlagMap = saved;
}

TEST_CASE("Live reservation release reads the room's tile range bytes",
          "[ai][cleanup]") {
  const auto [x, z, released] =
      GENERATE(table<unsigned, unsigned, bool>({{3, 8, false},
                                                {4, 8, true},
                                                {10, 8, true},
                                                {11, 8, false},
                                                {7, 5, false},
                                                {7, 6, true},
                                                {7, 12, true},
                                                {7, 13, false}}));
  const auto saved_flags = Battle_Ai_tileFlagMap;
  Battle_Ai_RoomAiState* const saved_room = Battle_Ai_roomAiState;
  u_short flags[32][32]{};
  for (auto& row : flags) {
    for (u_short& cell : row) cell = 0xFFFF;
  }
  Battle_Ai_RoomAiState room{};
  room.maxTileX = 10;
  room.maxTileZ = 12;
  room.minTileX = 4;
  room.minTileZ = 6;
  auto state = std::make_unique<Battle_Ai_ActorAiState>();
  state->opaque1A = 1;
  state->nextTile.word = static_cast<u_int>(x | (z << 16));
  Battle_Ai_tileFlagMap = flags;
  Battle_Ai_roomAiState = &room;
  ai::ReleaseLiveActorAiReservations(state.get());
  Battle_Ai_tileFlagMap = saved_flags;
  Battle_Ai_roomAiState = saved_room;
  CHECK(flags[z][x] == (released ? 0xFF7F : 0xFFFF));
  CHECK(state->opaque1A == 0);
}

TEST_CASE("Live persistence flags read the AI state table", "[ai][cleanup]") {
  Battle_Ai_ActorAiState* const saved = Battle_Ai_actorAiStates[4];
  auto state = std::make_unique<Battle_Ai_ActorAiState>();
  state->usesRoomActionDelay = 1;
  state->behaviorMode = 0;
  const std::uint32_t relation = 0x37F00080u;
  std::memcpy(&state->relations[0], &relation, sizeof(relation));
  Battle_Ai_actorAiStates[4] = nullptr;
  CHECK(ai::PackLiveActorAiPersistenceFlags(4) == -1);
  Battle_Ai_actorAiStates[4] = state.get();
  CHECK(ai::PackLiveActorAiPersistenceFlags(4) == 0x1FFD);
  Battle_Ai_actorAiStates[4] = saved;
}
