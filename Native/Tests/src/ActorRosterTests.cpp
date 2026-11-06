#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ActorRoster.hpp"
#include "Actors/ActorRosterLive.hpp"
#include "Actors/RoomEnemyMemory.hpp"
#include "Actors/RoomEnemyMemoryLive.hpp"
#include "Legacy/Psx.h"
#include "TestSupport/ChannelWarnings.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Actors/RoomActorCache.h"
#include "Actors/SkeletonLiveState.h"
#include "Actors/Spawning.h"
#include "Battle/AbilityState.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "State/RoomCache.h"
#include "State/RoomMemory.h"
}

namespace {
namespace actors = VS::Actors;
using VS::test_support::ChannelWarnings;
using VS::test_support::Refused;

template <typename T>
const T& Required(const std::optional<T>& value) {
  REQUIRE(value);
  if (!value) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *value;
}

actors::RememberedEnemy Tagged(std::uint8_t tag) {
  actors::RememberedEnemy enemy;
  enemy.spawn_tag = tag;
  return enemy;
}

actors::EnemyStanding Standing(std::uint8_t spawn_index) {
  actors::EnemyStanding standing;
  standing.spawn_index = spawn_index;
  standing.ai_flags = 0x0123;
  standing.current_hp = 40;
  standing.max_hp = 90;
  standing.current_mp = 11;
  standing.max_mp = 20;
  standing.strength = 33;
  standing.intelligence = 44;
  standing.agility = 55;
  standing.limb_hp = {10, 11, 12, 13, 14, 15};
  standing.statuses = 0x12345;
  standing.placement = {
      .tile_x = 7, .tile_layer = 10, .tile_z = 3, .facing = 0x34};
  return standing;
}

// The record remembering Standing() writes over a record whose reserved
// half-word was 0x1234.
actors::RememberedEnemy RememberedStanding(std::uint8_t spawn_tag,
                                           std::uint8_t hidden) {
  actors::RememberedEnemy record;
  record.spawn_tag = spawn_tag;
  record.hidden = hidden;
  record.ai_flags = 0x0123;
  record.current_hp = 40;
  record.max_hp = 90;
  record.current_mp = 11;
  record.max_mp = 20;
  record.strength = 33;
  record.intelligence = 44;
  record.agility = 55;
  record.limb_hp = {10, 11, 12, 13, 14, 15};
  record.reserved = 0x1234;
  record.statuses = 0x12345;
  record.placement = {
      .tile_x = 7, .tile_layer = 10, .tile_z = 3, .facing = 0x34};
  return record;
}

struct Acquisition {
  int spawn_index = 0;
  std::size_t record = 0;
  std::uint32_t next_slot = 0;
};

// Acquires a record of `room` (the current room) and checks which record it
// gave and where the room's next slot moved.
void CheckAcquired(actors::RoomEnemyMemory& memory,
                   const actors::RememberedRoom& room,
                   const Acquisition& acquisition) {
  CHECK(actors::AcquireRememberedEnemy(memory, acquisition.spawn_index) ==
        &room.enemies.at(acquisition.record));
  CHECK(room.next_slot == acquisition.next_slot);
}

// A forgotten room keeps its recency, place and next slot but no enemies.
void CheckForgotten(const actors::RememberedRoom& room, std::size_t recency) {
  CHECK(room.recency == recency);
  CHECK(room.zone == 3);
  CHECK(room.map == 12);
  CHECK(room.next_slot == 5);
  CHECK(std::ranges::all_of(room.enemies, [](const auto& enemy) {
    return enemy == actors::RememberedEnemy{};
  }));
}
}  // namespace

TEST_CASE("Room enemy memory finds the last record of a spawn",
          "[actors-room-memory]") {
  actors::RoomEnemyMemory memory;
  memory.current_room = 1;
  auto& enemies = memory.rooms[1].enemies;
  enemies[2] = Tagged(6);
  enemies[5] = Tagged(6);
  memory.rooms[0].enemies[0] = Tagged(5);

  CHECK(actors::FindRememberedEnemy(memory, 5) == &enemies[5]);
  CHECK(actors::FindRememberedEnemy(memory, 4) == nullptr);

  memory.current_room = 4;
  CHECK(actors::FindRememberedEnemy(memory, 5) == nullptr);
  memory.current_room = -1;
  CHECK(actors::FindRememberedEnemy(memory, 5) == nullptr);
}

TEST_CASE("Room enemy memory acquires the first match or the next record",
          "[actors-room-memory]") {
  actors::RoomEnemyMemory memory;
  memory.current_room = 2;
  actors::RememberedRoom& room = memory.rooms[2];
  room.enemies[3] = Tagged(10);
  room.enemies[6] = Tagged(10);

  CheckAcquired(memory, room, {.spawn_index = 9, .record = 3, .next_slot = 0});
  CheckAcquired(memory, room, {.spawn_index = 1, .record = 0, .next_slot = 1});

  room.next_slot = 7;
  CheckAcquired(memory, room, {.spawn_index = 2, .record = 7, .next_slot = 0});

  // The original wrote past the eight records here.
  room.next_slot = 8;
  const actors::RoomEnemyMemory before = memory;
  CHECK(actors::AcquireRememberedEnemy(memory, 2) == nullptr);
  CHECK(memory == before);

  memory.current_room = 4;
  CHECK(actors::AcquireRememberedEnemy(memory, 2) == nullptr);
}

TEST_CASE("Remembering an enemy records its standing", "[actors-room-memory]") {
  actors::RoomEnemyMemory memory;
  memory.current_room = 0;
  actors::RememberedRoom& room = memory.rooms[0];
  room.enemies[0].reserved = 0x1234;

  REQUIRE(actors::RememberEnemy(memory, Standing(5), 1));
  CHECK(room.enemies[0] == RememberedStanding(6, 1));
  CHECK(room.next_slot == 1);

  // A fading enemy is remembered shown, at 1 HP.
  actors::EnemyStanding fading = Standing(5);
  fading.fading = true;
  fading.current_hp = 0;
  REQUIRE(actors::RememberEnemy(memory, fading, 0));
  actors::RememberedEnemy faded = RememberedStanding(6, 0);
  faded.current_hp = 1;
  CHECK(room.enemies[0] == faded);
  CHECK(room.next_slot == 1);
}

TEST_CASE("Remembering wraps spawn 255 and stops at a full room",
          "[actors-room-memory]") {
  actors::RoomEnemyMemory memory;
  memory.current_room = 0;
  actors::RememberedRoom& room = memory.rooms[0];
  // Spawn index 255 wraps to the unused tag, as the original's byte store did.
  REQUIRE(actors::RememberEnemy(memory, Standing(255), 0));
  CHECK(room.enemies[0].spawn_tag == 0);

  room.next_slot = 8;
  const actors::RoomEnemyMemory before = memory;
  CHECK_FALSE(actors::RememberEnemy(memory, Standing(9), 0));
  CHECK(memory == before);
}

TEST_CASE("Forgetting room enemies keeps each room's place",
          "[actors-room-memory]") {
  actors::RoomEnemyMemory memory;
  for (std::size_t index = 0; index < memory.rooms.size(); ++index) {
    actors::RememberedRoom& room = memory.rooms[index];
    room.recency = static_cast<std::uint8_t>(index + 1);
    room.zone = 3;
    room.map = 12;
    room.next_slot = 5;
    room.enemies.fill(Tagged(4));
  }
  memory.rooms[1].zone = 4;
  memory.rooms[3].map = 13;

  actors::ForgetRoomEnemies(memory, 12, 3);
  for (const std::size_t index : {std::size_t{0}, std::size_t{2}}) {
    CheckForgotten(memory.rooms[index], index + 1);
  }
  CHECK(memory.rooms[1].enemies[0] == Tagged(4));
  CHECK(memory.rooms[3].enemies[7] == Tagged(4));

  memory.current_room = 3;
  actors::ClearRoomEnemyMemory(memory);
  CHECK(memory == actors::RoomEnemyMemory{});
}

TEST_CASE("The current remembered room is matched by zone and map",
          "[actors-room-memory]") {
  actors::RoomEnemyMemory memory;
  memory.current_room = 1;
  memory.rooms[1].zone = 3;
  memory.rooms[1].map = 12;
  CHECK(actors::IsCurrentRememberedRoom(memory, 3, 12));
  CHECK_FALSE(actors::IsCurrentRememberedRoom(memory, 3, 13));
  CHECK_FALSE(actors::IsCurrentRememberedRoom(memory, 4, 12));
  memory.current_room = 4;
  CHECK_FALSE(actors::IsCurrentRememberedRoom(memory, 3, 12));
  CHECK(actors::CurrentRememberedRoom(std::as_const(memory)) == nullptr);
}

TEST_CASE("Linking an actor after Ashley", "[actors-room-memory]") {
  actors::RosterView view{};
  view[0].occupied = true;
  view[4].occupied = true;
  const actors::RosterCandidate enemy{.linkable_kind = true, .leader = false};

  CHECK(actors::LinkActorAfterAshley(view, enemy) ==
        actors::RosterDecision{.edit = actors::RosterEdit::kInsertAfterLeader,
                               .slot = 0});
  CHECK(actors::LinkActorAfterAshley(view, {.linkable_kind = false}) ==
        actors::RosterDecision{});
  CHECK(actors::LinkActorAfterAshley(view,
                                     {.linkable_kind = true, .leader = true}) ==
        actors::RosterDecision{});

  view[4].links_to_actor = true;
  CHECK(actors::LinkActorAfterAshley(view, enemy) == actors::RosterDecision{});

  view[4].links_to_actor = false;
  view[0].occupied = false;
  CHECK(actors::LinkActorAfterAshley(view, enemy) ==
        actors::RosterDecision{.edit = actors::RosterEdit::kRefusedNoLeader,
                               .slot = 0});
}

TEST_CASE("Unlinking an actor bypasses its first predecessor",
          "[actors-room-memory]") {
  actors::RosterView view{};
  view[2].links_to_actor = true;  // An empty slot never links.
  view[5] = {.occupied = true, .links_to_actor = true};
  view[9] = {.occupied = true, .links_to_actor = true};
  const actors::RosterCandidate enemy{.linkable_kind = true, .leader = false};

  CHECK(actors::UnlinkActorFromRoomList(view, enemy) ==
        actors::RosterDecision{.edit = actors::RosterEdit::kBypass, .slot = 5});
  CHECK(actors::UnlinkActorFromRoomList(view, {.leader = true}) ==
        actors::RosterDecision{});
  CHECK(actors::UnlinkActorFromRoomList(actors::RosterView{}, enemy) ==
        actors::RosterDecision{});
}

namespace {
class RecordingRemovalHost final : public actors::ActorRemovalHost {
 public:
  std::array<std::optional<actors::RosterOccupant>, actors::kRosterSlots>
      occupants{};
  actors::RosterView links{};
  std::vector<std::string> calls;

  [[nodiscard]] std::optional<actors::RosterOccupant> Occupant(
      std::size_t slot) const override {
    return occupants.at(slot);
  }
  void Remember(std::size_t slot, int hidden) override {
    calls.push_back("remember " + std::to_string(slot) + " " +
                    std::to_string(hidden));
  }
  void ReleaseGimmick(std::size_t slot) override {
    calls.push_back("gimmick " + std::to_string(slot));
  }
  void ReleaseModel(std::size_t model_slot) override {
    calls.push_back("model " + std::to_string(model_slot));
  }
  void FreeAiState(std::size_t slot) override {
    calls.push_back("ai " + std::to_string(slot));
  }
  [[nodiscard]] actors::RosterView Links(std::size_t /*slot*/) const override {
    return links;
  }
  void Bypass(std::size_t predecessor, std::size_t slot) override {
    calls.push_back("bypass " + std::to_string(predecessor) + " " +
                    std::to_string(slot));
  }
  void Free(std::size_t slot) override {
    calls.push_back("free " + std::to_string(slot));
  }
  // The calls since the last take.
  std::vector<std::string> TakeCalls() { return std::exchange(calls, {}); }
};
}  // namespace

TEST_CASE("Removing an actor releases it in the original order",
          "[actors-room-memory]") {
  RecordingRemovalHost host;
  host.occupants[3] = actors::RosterOccupant{.second_model = true};
  host.occupants[7] = actors::RosterOccupant{.gimmick = true};
  host.occupants[9] = actors::RosterOccupant{};
  host.links[0] = {.occupied = true, .links_to_actor = true};

  CHECK(actors::RemoveActorAtSlot(host, 3, 1) ==
        actors::RemovalResult::kRemoved);
  CHECK(host.TakeCalls() == std::vector<std::string>{"remember 3 1", "model 3",
                                                     "model 7", "ai 3",
                                                     "bypass 0 3", "free 3"});

  CHECK(actors::RemoveActorAtSlot(host, 7, 0) ==
        actors::RemovalResult::kRemoved);
  CHECK(host.TakeCalls() == std::vector<std::string>{"remember 7 0",
                                                     "gimmick 7", "ai 7",
                                                     "bypass 0 7", "free 7"});

  host.links = {};
  CHECK(actors::RemoveActorAtSlot(host, 9, 0) ==
        actors::RemovalResult::kRemoved);
  CHECK(host.TakeCalls() ==
        std::vector<std::string>{"remember 9 0", "model 9", "ai 9", "free 9"});

  CHECK(actors::RemoveActorAtSlot(host, 4, 0) == actors::RemovalResult::kEmpty);
  CHECK(actors::RemoveActorAtSlot(host, 16, 0) ==
        actors::RemovalResult::kOutOfRange);
  CHECK(host.calls.empty());
}

namespace {
class RecordingDisplayFlagsHost final : public actors::DisplayFlagsHost {
 public:
  std::array<std::uint32_t, actors::kRosterSlots> flags{};
  std::vector<std::size_t> written;

  [[nodiscard]] std::uint32_t StateFlags(std::size_t slot) const override {
    return flags.at(slot);
  }
  void SetDisplayFlags(std::size_t slot, std::uint32_t value) override {
    written.push_back(slot);
    flags.at(slot) = value;
  }
};
}  // namespace

TEST_CASE("Freezing and resuming every actor's animation",
          "[actors-room-memory]") {
  RecordingDisplayFlagsHost host;
  host.flags[1] = 0x8001;
  host.flags[2] = 0x8005;
  host.flags[15] = 0x40;

  actors::FreezeAllActorAnimations(host);
  CHECK(std::to_array({host.flags[1], host.flags[2], host.flags[15]}) ==
        std::array<std::uint32_t, 3>{0x8005, 0x8005, 0x44});
  CHECK(host.written.size() == actors::kRosterSlots - 1);
  CHECK(std::ranges::find(host.written, std::size_t{2}) == host.written.end());

  host.written.clear();
  host.flags[6] = 0;
  actors::UnfreezeAllActorAnimations(host);
  CHECK(std::to_array(
            {host.flags[1], host.flags[2], host.flags[15], host.flags[6]}) ==
        std::array<std::uint32_t, 4>{0x8001, 0x8001, 0x40, 0});
  CHECK(host.written.size() == actors::kRosterSlots - 1);
}

namespace {
// The marks BuildPlaceholderActorStats gives auxiliary id 0x101.
void CheckPlaceholderMarks(const actors::PlaceholderActorStats& marked) {
  CHECK(marked.cancels_hit_results);
  CHECK(Required(marked.marker_flags) == actors::kPlaceholderMarkerFlags);
  CHECK(marked.room_spawn_index == actors::kNoRoomSpawn);
  CHECK(marked.actor_id == actors::kNoRoomSpawn);
  CHECK_FALSE(marked.kill_counted);
  CHECK_FALSE(marked.defeated);
}

// Auxiliary ids without the marker flags; only some cancel hit results.
void CheckUnmarked(
    int auxiliary, bool cancels,
    const std::array<std::uint8_t, actors::kActorNameBytes>& name) {
  const actors::PlaceholderActorStats stats =
      actors::BuildPlaceholderActorStats(auxiliary, name);
  CHECK(stats.cancels_hit_results == cancels);
  CHECK_FALSE(stats.marker_flags.has_value());
}
}  // namespace

TEST_CASE("A placeholder actor's stats follow its auxiliary id",
          "[actors-room-memory]") {
  std::array<std::uint8_t, actors::kActorNameBytes> name{};
  for (std::size_t index = 0; index < name.size(); ++index) {
    name[index] = static_cast<std::uint8_t>(0xA0 + index);
  }

  const actors::PlaceholderActorStats marked =
      actors::BuildPlaceholderActorStats(0x101, name);
  CHECK(marked.name == name);
  CHECK(marked.current_hp == 1);
  CHECK(marked.max_hp == 1);
  CheckPlaceholderMarks(marked);

  for (const int auxiliary : {2, 4, 0x302}) {
    CheckUnmarked(auxiliary, false, name);
  }
  for (const int auxiliary : {0, 3, 5, -1}) {
    CheckUnmarked(auxiliary, true, name);
  }
}

TEST_CASE("Combat limb indices are range checked", "[actors-room-memory]") {
  CHECK(actors::CombatLimbIndex(-1) == std::nullopt);
  CHECK(Required(actors::CombatLimbIndex(0)) == 0);
  CHECK(Required(actors::CombatLimbIndex(5)) == 5);
  CHECK(actors::CombatLimbIndex(6) == std::nullopt);
}

namespace {
// Saves the live tables the roster and room enemy memory bindings touch and
// restores them afterwards.
struct LiveRosterFixture {
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  decltype(std::to_array(Actors_actorRecords)) saved_records =
      std::to_array(Actors_actorRecords);
  decltype(std::to_array(Battle_Ai_actorAiStates)) saved_ai =
      std::to_array(Battle_Ai_actorAiStates);
  decltype(std::to_array(Actors_weaponModels)) saved_weapons =
      std::to_array(Actors_weaponModels);
  decltype(std::to_array(Battle_actorMarkers)) saved_markers =
      std::to_array(Battle_actorMarkers);
  decltype(std::to_array(State_recentRoomEnemyStates)) saved_rooms =
      std::to_array(State_recentRoomEnemyStates);
  int saved_room_slot = State_currentRoomStateSlot;

  LiveRosterFixture() {
    std::fill_n(Battle_actors, saved_actors.size(), nullptr);
    std::fill_n(Actors_actorRecords, saved_records.size(), nullptr);
    std::fill_n(Battle_Ai_actorAiStates, saved_ai.size(), nullptr);
    std::fill_n(Actors_weaponModels, saved_weapons.size(), nullptr);
    std::fill_n(Battle_actorMarkers, saved_markers.size(), 0);
    std::fill_n(State_recentRoomEnemyStates, saved_rooms.size(),
                State_RoomEnemyStates{});
    State_currentRoomStateSlot = 0;
  }
  LiveRosterFixture(const LiveRosterFixture&) = delete;
  LiveRosterFixture& operator=(const LiveRosterFixture&) = delete;
  LiveRosterFixture(LiveRosterFixture&&) = delete;
  LiveRosterFixture& operator=(LiveRosterFixture&&) = delete;
  ~LiveRosterFixture() {
    std::ranges::copy(saved_actors, Battle_actors);
    std::ranges::copy(saved_records, Actors_actorRecords);
    std::ranges::copy(saved_ai, Battle_Ai_actorAiStates);
    std::ranges::copy(saved_weapons, Actors_weaponModels);
    std::ranges::copy(saved_markers, Battle_actorMarkers);
    std::ranges::copy(saved_rooms, State_recentRoomEnemyStates);
    State_currentRoomStateSlot = saved_room_slot;
  }
};

using RoomBytes = std::array<unsigned char, sizeof State_recentRoomEnemyStates>;

RoomBytes SavedRoomBytes() {
  RoomBytes bytes{};
  std::memcpy(bytes.data(), State_recentRoomEnemyStates, bytes.size());
  return bytes;
}

Actors_RoomActorRecord SavedRecord(std::size_t room, std::size_t slot) {
  Actors_RoomActorRecord record{};
  std::memcpy(&record, State_recentRoomEnemyStates[room].actorStates[slot],
              sizeof record);
  return record;
}

// An enemy in actor slot 3 from room spawn 5, standing on tile (7, 3) of
// layer 10 and facing 0x340.
struct LiveEnemy {
  std::unique_ptr<Battle_ActorStats> stats =
      std::make_unique<Battle_ActorStats>();
  std::unique_ptr<Actors_ActorRecord> record =
      std::make_unique<Actors_ActorRecord>();
  Battle_Actor actor{};

  LiveEnemy() {
    actor.id = 3;
    actor.kind = 1;
    actor.roomSpawnIndex = 5;
    actor.stats = stats.get();
    stats->currentHp = 40;
    stats->maxHp = 90;
    stats->currentMp = 11;
    stats->maxMp = 20;
    stats->totalStrength = 33;
    stats->totalIntelligence = 44;
    stats->totalAgility = 55;
    for (std::size_t limb = 0; limb < 6; ++limb) {
      stats->limbs[limb].hp = static_cast<std::int16_t>(10 + limb);
    }
    stats->statuses = 0x12345;
    record->instance.slot = 3;
    record->instance.attachParent = 0xFF;
    record->instance.facing = 0x340;
    record->savedPlacement = 0x00030A07;
    Battle_actors[3] = &actor;
    Actors_actorRecords[3] = record.get();
  }
};

// The saved record StoreLiveRoomActorState writes for a hidden LiveEnemy.
Actors_RoomActorRecord StoredLiveEnemy() {
  Actors_RoomActorRecord record{};
  record.spawnSlot = 6;
  record.isHidden = 1;
  record.aiFlags = -1;  // No AI state.
  record.currentHp = 40;
  record.maxHp = 90;
  record.currentMp = 11;
  record.maxMp = 20;
  record.strength = 33;
  record.intelligence = 44;
  record.agility = 55;
  for (std::size_t limb = 0; limb < 6; ++limb) {
    record.limbHp[limb] = static_cast<std::int16_t>(10 + limb);
  }
  record.statuses = 0x12345;
  record.tileX = 7;
  record.direction = 10;
  record.tileZ = 3;
  record.height = 0x34;
  return record;
}

bool SameRecord(const Actors_RoomActorRecord& first,
                const Actors_RoomActorRecord& second) {
  return std::memcmp(&first, &second, sizeof first) == 0;
}

// Forgetting map 12 in zone 3 cleared saved room 1's enemies and kept its
// recency and next slot; room 0 kept the pattern.
void CheckLiveRoomForgotten(const State_RoomEnemyStates& before,
                            const RoomBytes& pattern) {
  CHECK(State_recentRoomEnemyStates[1].recency == before.recency);
  CHECK(State_recentRoomEnemyStates[1].nextSlot == before.nextSlot);
  CHECK(std::ranges::all_of(
      std::as_bytes(std::span(State_recentRoomEnemyStates[1].actorStates)),
      [](std::byte value) { return value == std::byte{0}; }));
  CHECK(std::memcmp(&State_recentRoomEnemyStates[0], pattern.data(),
                    sizeof(State_RoomEnemyStates)) == 0);
}

// Copying the remembered spawn 5 gives its saved record; spawn 4 has none.
void CheckLiveRememberedCopy(const Actors_RoomActorRecord& saved) {
  Actors_RoomActorRecord copy{};
  CHECK(VS::Actors::CopyLiveRememberedEnemy(5, &copy) == &copy);
  CHECK(SameRecord(copy, saved));
  CHECK(VS::Actors::CopyLiveRememberedEnemy(4, &copy) == nullptr);
}

// Spawn 5's hidden flag round-trips; spawn 4 answers the fallback and setting
// it remembers nothing.
void CheckLiveRememberedHidden() {
  CHECK(VS::Actors::LiveRememberedEnemyHidden(5, 9) == 0);
  VS::Actors::SetLiveRememberedEnemyHidden(5, 1);
  CHECK(VS::Actors::LiveRememberedEnemyHidden(5, 9) == 1);
  CHECK(VS::Actors::LiveRememberedEnemyHidden(4, 9) == 9);
  VS::Actors::SetLiveRememberedEnemyHidden(4, 1);
  CHECK(SavedRecord(1, 1).spawnSlot == 0);
}
}  // namespace

TEST_CASE("The live room enemy memory round-trips the saved rooms",
          "[actors-room-memory]") {
  const LiveRosterFixture fixture;
  RoomBytes pattern{};
  for (std::size_t index = 0; index < pattern.size(); ++index) {
    pattern[index] = static_cast<unsigned char>(index * 7 + 3);
  }
  std::memcpy(State_recentRoomEnemyStates, pattern.data(), pattern.size());
  State_currentRoomStateSlot = 2;

  ChannelWarnings warnings("actors");
  VS::Actors::ForgetLiveRoomEnemies(-1, -1);
  CHECK(SavedRoomBytes() == pattern);
  CHECK(State_currentRoomStateSlot == 2);
  CHECK(warnings.messages.empty());

  State_recentRoomEnemyStates[1].zoneId = 3;
  State_recentRoomEnemyStates[1].mapId = 12;
  const State_RoomEnemyStates before = State_recentRoomEnemyStates[1];
  VS::Actors::ForgetLiveRoomEnemies(12, 3);
  CheckLiveRoomForgotten(before, pattern);

  VS::Actors::ClearLiveRoomEnemyMemory();
  CHECK(SavedRoomBytes() == RoomBytes{});
  CHECK(State_currentRoomStateSlot == 0);
}

TEST_CASE("Storing a live enemy remembers it in the current saved room",
          "[actors-room-memory]") {
  const LiveRosterFixture fixture;
  LiveEnemy enemy;
  State_currentRoomStateSlot = 1;
  State_recentRoomEnemyStates[1].zoneId = 3;
  State_recentRoomEnemyStates[1].mapId = 12;
  ChannelWarnings warnings("actors");

  VS::Actors::StoreLiveRoomActorState(3, 1);
  CHECK(SameRecord(SavedRecord(1, 0), StoredLiveEnemy()));
  CHECK(State_recentRoomEnemyStates[1].nextSlot == 1);

  // A fading enemy keeps 1 HP and reuses its record.
  enemy.record->instance.actionAnimationState = 2;
  VS::Actors::StoreLiveRoomActorState(3, 0);
  Actors_RoomActorRecord faded = StoredLiveEnemy();
  faded.isHidden = 0;
  faded.currentHp = 1;
  CHECK(SameRecord(SavedRecord(1, 0), faded));
  CHECK(State_recentRoomEnemyStates[1].nextSlot == 1);

  CheckLiveRememberedCopy(faded);
  CheckLiveRememberedHidden();

  CHECK(VS::Actors::IsLiveRememberedRoom(3, 12));
  CHECK_FALSE(VS::Actors::IsLiveRememberedRoom(3, 13));
  CHECK(warnings.messages.empty());
}

TEST_CASE("Storing a live enemy refuses what the original indexed past",
          "[actors-room-memory]") {
  const LiveRosterFixture fixture;
  LiveEnemy enemy;
  ChannelWarnings warnings("actors");
  const RoomBytes empty = SavedRoomBytes();

  VS::Actors::StoreLiveRoomActorState(17, 0);
  enemy.actor.roomSpawnIndex = actors::kNoRoomSpawn;
  VS::Actors::StoreLiveRoomActorState(3, 0);
  VS::Actors::StoreLiveRoomActorState(4, 0);
  enemy.actor.roomSpawnIndex = 5;
  enemy.actor.stats = nullptr;
  VS::Actors::StoreLiveRoomActorState(3, 0);
  enemy.actor.stats = enemy.stats.get();
  State_currentRoomStateSlot = 4;
  VS::Actors::StoreLiveRoomActorState(3, 0);
  CHECK_FALSE(VS::Actors::IsLiveRememberedRoom(0, 0));
  Actors_RoomActorRecord copy{};
  CHECK(VS::Actors::CopyLiveRememberedEnemy(5, &copy) == nullptr);
  CHECK(VS::Actors::CopyLiveRememberedEnemy(5, nullptr) == nullptr);
  State_currentRoomStateSlot = 0;
  State_recentRoomEnemyStates[0].nextSlot = 9;
  VS::Actors::StoreLiveRoomActorState(3, 0);
  State_recentRoomEnemyStates[0].nextSlot = 0;

  CHECK(SavedRoomBytes() == empty);
  const std::string store = "StoreLiveRoomActorState";
  const std::string outside = "the current saved room is outside the four";
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused(store, "the actor id is outside the actor record table"),
            Refused(store, "the actor has no stats"), Refused(store, outside),
            Refused("IsLiveRememberedRoom", outside),
            Refused("CopyLiveRememberedEnemy", outside),
            Refused("CopyLiveRememberedEnemy", "no output record"),
            Refused(store, "the saved room's next record is outside it")});
}

namespace {
using LiveRoster = std::array<Battle_Actor, 4>;

// Actors 1 and 2 join after Ashley, the newest first; a member, Ashley and a
// gimmick do not.
void CheckLiveLinks(LiveRoster& roster) {
  CHECK(VS::Actors::LinkLiveActorAfterAshley(&roster[1]) == 1);
  CHECK(VS::Actors::LinkLiveActorAfterAshley(&roster[2]) == 1);
  CHECK(std::to_array({roster[0].next, roster[2].next, roster[1].next}) ==
        std::array<Battle_Actor*, 3>{&roster[2], &roster[1], nullptr});
  CHECK(VS::Actors::LinkLiveActorAfterAshley(&roster[2]) == 0);
  CHECK(VS::Actors::LinkLiveActorAfterAshley(&roster[0]) == 0);
  CHECK(VS::Actors::LinkLiveActorAfterAshley(&roster[3]) == 0);
  CHECK(roster[0].next == &roster[2]);
}

// Leaving bypasses the actor's predecessor; a non-member and Ashley stay.
void CheckLiveUnlinks(LiveRoster& roster) {
  CHECK(VS::Actors::UnlinkLiveActorFromRoomList(&roster[1]) == 1);
  CHECK(roster[2].next == nullptr);
  CHECK(VS::Actors::UnlinkLiveActorFromRoomList(&roster[1]) == 0);
  CHECK(VS::Actors::UnlinkLiveActorFromRoomList(&roster[0]) == 0);
  roster[2].next = &roster[3];
  CHECK(VS::Actors::UnlinkLiveActorFromRoomList(&roster[2]) == 1);
  CHECK(roster[0].next == &roster[3]);
  CHECK(roster[2].next == nullptr);
}
}  // namespace

TEST_CASE("Live actors join and leave the list after Ashley",
          "[actors-room-memory]") {
  const LiveRosterFixture fixture;
  LiveRoster roster{};
  for (std::size_t index = 0; index < roster.size(); ++index) {
    roster[index].id = static_cast<int>(index);
    roster[index].kind = 1;
    Battle_actors[index] = &roster[index];
  }
  roster[3].kind = 0x10;  // A gimmick never joins.
  ChannelWarnings warnings("actors");

  CheckLiveLinks(roster);
  CheckLiveUnlinks(roster);
  CHECK(warnings.messages.empty());

  Battle_actors[0] = nullptr;
  CHECK(VS::Actors::LinkLiveActorAfterAshley(&roster[1]) == 0);
  CHECK(VS::Actors::LinkLiveActorAfterAshley(nullptr) == 0);
  CHECK(VS::Actors::UnlinkLiveActorFromRoomList(nullptr) == 0);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused("LinkLiveActorAfterAshley", "Ashley's actor slot is empty"),
            Refused("LinkLiveActorAfterAshley", "no actor"),
            Refused("UnlinkLiveActorFromRoomList", "no actor")});
}

namespace {
// The stats InitializeLivePlaceholderActor writes for auxiliary id 0x101.
void CheckLivePlaceholderStats(const Battle_ActorStats& stats) {
  CHECK(stats.currentHp == 1);
  CHECK(stats.maxHp == 1);
  CHECK(stats.statuses == 0);
  CHECK(stats.cancelsHitResults == 1);
  CHECK(stats.actorId == actors::kNoRoomSpawn);
  constexpr std::size_t kNameBytes = sizeof Actors_placeholderActorName;
  CHECK(std::memcmp(stats.name, Actors_placeholderActorName, kNameBytes) == 0);
  CHECK(std::memcmp(stats.name + kNameBytes, Battle_breakArtPointRequirements,
                    actors::kActorNameBytes - kNameBytes) == 0);
}
}  // namespace

TEST_CASE("A live placeholder actor gets the placeholder stats",
          "[actors-room-memory]") {
  const LiveRosterFixture fixture;
  auto stats = std::make_unique<Battle_ActorStats>();
  std::memset(stats.get(), 0xFF, sizeof *stats);
  Battle_Actor actor{};
  actor.stats = stats.get();
  actor.roomSpawnIndex = 2;
  actor.killCounted = 1;
  actor.defeated = 1;
  Battle_actors[5] = &actor;
  Battle_actorMarkers[5] = static_cast<char>(0x83);
  ChannelWarnings warnings("actors");

  VS::Actors::InitializeLivePlaceholderActor(5, 0x101);
  CheckLivePlaceholderStats(*stats);
  // The actor forgets its spawn, kill count and defeat.
  CHECK(std::to_array<int>(
            {actor.roomSpawnIndex, actor.killCounted, actor.defeated}) ==
        std::array<int, 3>{actors::kNoRoomSpawn, 0, 0});
  CHECK(static_cast<unsigned char>(Battle_actorMarkers[5]) == 0x88);

  Battle_actorMarkers[5] = 0;
  VS::Actors::InitializeLivePlaceholderActor(5, 2);
  CHECK(stats->cancelsHitResults == 0);
  CHECK(Battle_actorMarkers[5] == 0);
  CHECK(warnings.messages.empty());

  VS::Actors::InitializeLivePlaceholderActor(42, 1);
  VS::Actors::InitializeLivePlaceholderActor(6, 1);
  actor.stats = nullptr;
  VS::Actors::InitializeLivePlaceholderActor(5, 1);
  const std::string operation = "InitializeLivePlaceholderActor";
  const std::string no_stats = "the actor slot has no actor stats";
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused(operation, "the actor id is outside Battle_actors"),
            Refused(operation, no_stats), Refused(operation, no_stats)});
}
