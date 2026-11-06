#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Event/RoomTriggers.hpp"

namespace {
namespace event = VS::Event;
using Result = event::RoomTriggerResult;
using Request = event::RoomTriggerRequest;
using Bytes = std::vector<std::uint8_t>;

class TriggerHost final : public event::RoomTriggerHost {
 public:
  event::PlaybackPhase phase = event::PlaybackPhase::kIdle;
  bool direct = false;
  bool entry = false;
  std::int16_t event_id = 123;
  std::array<std::uint8_t, 1024> flags{};
  std::array<std::optional<event::TriggerPlacement>, 32> actors{};
  std::optional<event::TriggerPlacement> gimmick;
  std::optional<int> model = 2;
  std::optional<int> resolved = 2;
  int count = 0;
  int hp = 0;
  int items = 0;
  int height = 0;
  std::uint32_t actor_flags = 0;
  bool retained = false;
  std::vector<std::pair<event::RoomTriggerRefusal, std::size_t>> refusals;
  std::vector<std::tuple<event::EventSource, std::int16_t, bool>> queued;
  std::vector<std::pair<int, std::int16_t>> writes;
  std::vector<int> model_queries;
  std::vector<std::uint16_t> references;
  int item_query = 0;

  void Refuse(event::RoomTriggerRefusal reason, std::size_t location) override {
    refusals.emplace_back(reason, location);
  }
  event::PlaybackPhase Phase() override { return phase; }
  bool DirectRequest() override { return direct; }
  bool RoomEntryPending() override { return entry; }
  std::int16_t CurrentEventId() override { return event_id; }
  void ConsumeRoomEntry() override { entry = false; }
  void QueueEvent(event::EventSource source, std::int16_t id,
                  bool is_direct) override {
    queued.emplace_back(source, id, is_direct);
    phase = event::PlaybackPhase::kRequested;
    event_id = id;
    direct = is_direct;
  }
  std::uint8_t Flag(int flag) override { return flags.at(flag); }
  void SetFlag(int flag, std::int16_t value) override {
    writes.emplace_back(flag, value);
  }
  bool ActorPresent(int actor) override { return actors.at(actor).has_value(); }
  std::optional<int> FindActorByModel(int id) override {
    model_queries.push_back(id);
    return model;
  }
  std::optional<int> ResolveActor(std::uint16_t reference) override {
    references.push_back(reference);
    return resolved;
  }
  std::optional<event::TriggerPlacement> ActorPlacement(int actor) override {
    return actors.at(actor);
  }
  int CountActorsByDisposition(int) override { return count; }
  int ActorHpPercentage(int) override { return hp; }
  std::uint32_t ActorStateFlags(int) override { return actor_flags; }
  bool ActorRetainedAfterDefeat(int) override { return retained; }
  int CountInventoryItem(int item) override {
    item_query = item;
    return items;
  }
  std::optional<event::TriggerPlacement> GimmickPlacement(int) override {
    return gimmick;
  }
  int GimmickSupportHeight(event::TriggerTile) override { return height; }
};

Bytes Section(Bytes conditions, std::uint16_t event_id = 37,
              std::uint8_t mask = 8) {
  Bytes bytes(32, 0xFF);
  bytes[0] = 32;
  bytes[1] = 0;
  bytes.push_back(mask);
  bytes.push_back(static_cast<std::uint8_t>(event_id));
  bytes.push_back(static_cast<std::uint8_t>(event_id >> 8));
  bytes.insert(bytes.end(), conditions.begin(), conditions.end());
  return bytes;
}

Result Run(TriggerHost& host, const Bytes& bytes,
           event::RoomTriggerContext context = event::TriggerActorPair{}) {
  return event::EvaluateRoomEventTriggers(
      {Request::kActorState, 0, std::move(context)}, bytes, host);
}
}  // namespace

TEST_CASE("Room trigger control requests do not require a section",
          "[room-triggers]") {
  TriggerHost host;
  CHECK(event::EvaluateRoomEventTriggers({Request::kPlaybackStatus}, {},
                                         host) == Result::kNoMatch);
  host.phase = event::PlaybackPhase::kRunning;
  CHECK(event::EvaluateRoomEventTriggers({Request::kPlaybackStatus}, {},
                                         host) == Result::kBusy);
  CHECK(event::EvaluateRoomEventTriggers({Request::kDirectEvent, -1}, {},
                                         host) == Result::kStarted);
  CHECK(host.queued.back() ==
        std::tuple{event::EventSource::kEventFile, std::int16_t{-1}, true});
  CHECK(host.refusals.empty());
}

TEST_CASE("Room entry requests consume the pending entry and preserve its id",
          "[room-triggers]") {
  TriggerHost host;
  host.entry = true;
  host.direct = true;
  host.phase = event::PlaybackPhase::kRunning;
  CHECK(event::EvaluateRoomEventTriggers({Request::kRoomEntry}, {}, host) ==
        Result::kStarted);
  CHECK_FALSE(host.entry);
  CHECK(host.queued.back() == std::tuple{event::EventSource::kRoomEntryScript,
                                         std::int16_t{123}, true});
}

TEST_CASE("Direct playback blocks ordinary triggers before section access",
          "[room-triggers]") {
  TriggerHost host;
  host.phase = event::PlaybackPhase::kLoading;
  host.direct = true;
  CHECK(Run(host, {}) == Result::kBusy);
  CHECK(host.refusals.empty());
}

TEST_CASE(
    "Room trigger tables allow an early sentinel and reject short offsets",
    "[room-triggers]") {
  TriggerHost host;
  CHECK(Run(host, {0xFF, 0xFF}) == Result::kNoMatch);
  CHECK(host.refusals.empty());
  CHECK(Run(host, {0}) == Result::kRefused);
  REQUIRE(host.refusals.size() == 1);
  CHECK(host.refusals.back().first == event::RoomTriggerRefusal::kOffsetTable);
}

TEST_CASE(
    "Room trigger offsets and matching headers remain inside their section",
    "[room-triggers]") {
  TriggerHost host;
  CHECK(Run(host, {0xFE, 0xFF}) == Result::kRefused);
  CHECK(host.refusals.back().first == event::RoomTriggerRefusal::kRecordOffset);
  CHECK(Run(host, {2, 0, 8}) == Result::kRefused);
  CHECK(host.refusals.back().first == event::RoomTriggerRefusal::kRecordHeader);
}

TEST_CASE("Room trigger match markers select event files or masked door ids",
          "[room-triggers]") {
  TriggerHost host;
  const auto id = GENERATE(std::uint16_t{123}, std::uint16_t{0xABCD});
  REQUIRE(Run(host, Section({0xFF}, id)) == Result::kStarted);
  CHECK(host.event_id == (id & 0x8000 ? id & 15 : id));
  CHECK(std::get<0>(host.queued.front()) ==
        (id & 0x8000 ? event::EventSource::kDoorScript
                     : event::EventSource::kEventFile));
  CHECK_FALSE(host.direct);
}

TEST_CASE("Unmatched trigger masks do not read condition or event operands",
          "[room-triggers]") {
  TriggerHost host;
  CHECK(Run(host, {4, 0, 0xFF, 0xFF, 1}) == Result::kNoMatch);
  CHECK(host.refusals.empty());
}

TEST_CASE("Room trigger scans finish after sixteen nonmatching records",
          "[room-triggers]") {
  TriggerHost host;
  Bytes bytes(33, 0);
  for (std::size_t index = 0; index < 32; index += 2) {
    bytes[index] = 32;
  }
  bytes.back() = 1;
  CHECK(Run(host, bytes) == Result::kNoMatch);
  CHECK(host.refusals.empty());
}

TEST_CASE(
    "Malformed and negated unknown conditions are refused without looping",
    "[room-triggers]") {
  TriggerHost host;
  const auto bytes = GENERATE(Bytes{0}, Bytes{0xFE, 0}, Bytes{0xFE},
                              Bytes{0xFE, 0xFE}, Bytes{7, 0});
  CHECK(Run(host, Section(bytes)) == Result::kRefused);
  REQUIRE(host.refusals.size() == 1);
  CHECK(host.queued.empty());
}

TEST_CASE("Negation applies once and repeated negate bytes do not toggle",
          "[room-triggers]") {
  TriggerHost host;
  host.actor_flags = 0x104000;
  CHECK(Run(host, Section({0xFE, 0xFE, 9, 0xFF})) == Result::kStarted);
  host.phase = event::PlaybackPhase::kIdle;
  CHECK(Run(host, Section({0xFE, 9, 9, 0xFF})) == Result::kNoMatch);
}

TEST_CASE("Actor and tile trigger contexts do not reinterpret one another",
          "[room-triggers]") {
  TriggerHost host;
  const auto pair = Section({6, 2, 0x10, 2, 0x10, 0xFF});
  CHECK(Run(host, pair, event::TriggerActorPair{2, 2}) == Result::kStarted);
  CHECK(Run(host, pair, event::TriggerTile{2, 2}) == Result::kNoMatch);
  CHECK(Run(host, pair, std::monostate{}) == Result::kNoMatch);
  const auto tile = Section({8, 2, 3, 0xFF});
  CHECK(Run(host, tile, event::TriggerTile{2, 3}) == Result::kStarted);
  CHECK(Run(host, tile, event::TriggerActorPair{2, 3}) == Result::kNoMatch);
}

TEST_CASE("Unresolved actors cannot satisfy a negated actor predicate",
          "[room-triggers]") {
  TriggerHost host;
  host.resolved.reset();
  const auto predicate = GENERATE(Bytes{7, 0, 0, 0, 0}, Bytes{13, 0, 0, 0, 0},
                                  Bytes{14, 0, 0, 0, 0, 1, 1}, Bytes{16, 0, 0});
  Bytes conditions{0xFE};
  conditions.insert(conditions.end(), predicate.begin(), predicate.end());
  conditions.push_back(0xFF);
  CHECK(Run(host, Section(conditions)) == Result::kNoMatch);
  CHECK(host.queued.empty());
  CHECK(host.refusals.empty());
}

TEST_CASE("Player tile conditions use exclusive rectangle edges",
          "[room-triggers]") {
  TriggerHost host;
  host.actors[0] = event::TriggerPlacement{{3, 4}, 0, 0};
  CHECK(Run(host, Section({1, 3, 4, 0xFF})) == Result::kStarted);
  CHECK(Run(host, Section({2, 3, 4, 1, 1, 0xFF})) == Result::kStarted);
  CHECK(Run(host, Section({2, 2, 4, 1, 1, 0xFF})) == Result::kNoMatch);
  CHECK(Run(host, Section({2, 3, 3, 1, 1, 0xFF})) == Result::kNoMatch);
}

TEST_CASE("Missing placement refuses even a negated position predicate",
          "[room-triggers]") {
  TriggerHost host;
  CHECK(Run(host, Section({0xFE, 1, 0, 0, 0xFF})) == Result::kRefused);
  REQUIRE(host.refusals.size() == 1);
  CHECK(host.refusals.front().first ==
        event::RoomTriggerRefusal::kMissingPlacement);
  CHECK(host.queued.empty());
}

TEST_CASE("Trigger counters write narrowed values before negation",
          "[room-triggers]") {
  TriggerHost host;
  host.count = 0xFFFF;
  CHECK(Run(host, Section({0xFE, 5, 4, 7, 0, 0xFF})) == Result::kNoMatch);
  CHECK(host.writes == std::vector<std::pair<int, std::int16_t>>{{7, -1}});
}

TEST_CASE("Trigger support height handles the lowest native integer",
          "[room-triggers]") {
  TriggerHost host;
  host.height = std::numeric_limits<int>::min();
  CHECK(Run(host, Section({12, 1, 2, 7, 0, 0xFF})) == Result::kStarted);
  CHECK(host.writes == std::vector<std::pair<int, std::int16_t>>{{7, 0}});
}

TEST_CASE("Trigger flag comparisons keep values above the signed-byte boundary",
          "[room-triggers]") {
  TriggerHost host;
  host.flags[0] = 255;
  CHECK(Run(host, Section({3, 0x40, 0, 128, 0xFF})) == Result::kNoMatch);
  CHECK(Run(host, Section({3, 0x50, 0, 128, 0xFF})) == Result::kStarted);
  host.flags[3] = 255;
  CHECK(Run(host, Section({15, 0x20, 0, 3, 0xFF})) == Result::kStarted);
}

TEST_CASE("Actor presence distinguishes direct slots from signed model ids",
          "[room-triggers]") {
  TriggerHost host;
  host.actors[31] = event::TriggerPlacement{};
  CHECK(Run(host, Section({4, 31, 0x10, 0xFF})) == Result::kStarted);
  CHECK(host.model_queries.empty());
  CHECK(Run(host, Section({4, 0, 0x80, 0xFF})) == Result::kStarted);
  CHECK(host.model_queries == std::vector<int>{-32768});
  host.model.reset();
  CHECK(Run(host, Section({4, 7, 0, 0xFF})) == Result::kNoMatch);
}

TEST_CASE("Default trigger context preserves the legacy player pair",
          "[room-triggers]") {
  TriggerHost host;
  host.resolved = 0;
  CHECK(Run(host, Section({6, 0, 0x10, 0, 0x10, 0xFF})) == Result::kStarted);
  CHECK(host.references == std::vector<std::uint16_t>{0x1000, 0x1000});
}

TEST_CASE("Resolved actor predicates evaluate health placement and retention",
          "[room-triggers]") {
  TriggerHost host;
  host.hp = 75;
  host.actors[2] = event::TriggerPlacement{{8, 9}, 0, 0};
  host.retained = true;
  CHECK(Run(host, Section({7, 2, 0x10, 0, 0, 0xFF})) == Result::kStarted);
  CHECK(Run(host, Section({13, 2, 0x10, 8, 9, 0xFF})) == Result::kStarted);
  CHECK(Run(host, Section({14, 2, 0x10, 8, 9, 1, 1, 0xFF})) ==
        Result::kStarted);
  CHECK(Run(host, Section({16, 2, 0x10, 0xFF})) == Result::kStarted);
  host.retained = false;
  CHECK(Run(host, Section({16, 2, 0x10, 0xFF})) == Result::kNoMatch);
}

TEST_CASE(
    "Inventory trigger operands and stored counts retain signed halfwords",
    "[room-triggers]") {
  TriggerHost host;
  host.items = 65535;
  CHECK(Run(host, Section({10, 0xFF, 0xFF, 32, 0, 0xFF})) == Result::kStarted);
  CHECK(host.item_query == -1);
  CHECK(host.writes == std::vector<std::pair<int, std::int16_t>>{{32, -1}});
  host.items = 65536;
  CHECK(Run(host, Section({10, 1, 0, 0, 0, 0xFF})) == Result::kNoMatch);
}

TEST_CASE("Gimmick trigger position height and facing use source conventions",
          "[room-triggers]") {
  TriggerHost host;
  host.gimmick = event::TriggerPlacement{{3, 4}, -255, 12};
  CHECK(Run(host, Section({11, 0, 3, 4, 2, 4, 0xFF})) == Result::kStarted);
  CHECK(Run(host, Section({11, 0, 3, 4, 0, 0, 0xFF})) == Result::kStarted);
  CHECK(Run(host, Section({11, 0, 3, 4, 3, 4, 0xFF})) == Result::kNoMatch);
  CHECK(Run(host, Section({11, 0, 3, 4, 2, 5, 0xFF})) == Result::kNoMatch);
  host.gimmick.reset();
  CHECK(Run(host, Section({11, 0, 3, 4, 0, 0, 0xFF})) == Result::kNoMatch);
}

TEST_CASE("Trigger comparisons decode all packed flag index bits",
          "[room-triggers]") {
  TriggerHost host;
  const unsigned left = GENERATE(0u, 63u, 64u, 255u, 256u, 1023u);
  const unsigned right = left ^ 512u;
  host.flags[left] = 201;
  host.flags[right] = 201;
  const Bytes condition{
      15, static_cast<std::uint8_t>(0x20 | (left >> 6)),
      static_cast<std::uint8_t>(((left & 63) << 2) | (right >> 8)),
      static_cast<std::uint8_t>(right), 0xFF};
  CHECK(Run(host, Section(condition)) == Result::kStarted);
  host.flags[right] = 200;
  CHECK(Run(host, Section(condition)) == Result::kNoMatch);
}

TEST_CASE("Trigger comparison operators preserve equal and unequal boundaries",
          "[room-triggers]") {
  TriggerHost host;
  const auto left = GENERATE(std::uint8_t{0}, std::uint8_t{127},
                             std::uint8_t{128}, std::uint8_t{255});
  const auto right = GENERATE(std::uint8_t{0}, std::uint8_t{127},
                              std::uint8_t{128}, std::uint8_t{255});
  host.flags[0] = left;
  const std::array<bool, 6> expected{left <= right, left >= right,
                                     left == right, left != right,
                                     left<right, left> right};
  for (std::size_t op = 0; op < expected.size(); ++op) {
    const auto result = Run(
        host, Section({3, static_cast<std::uint8_t>(op << 4), 0, right, 0xFF}));
    CHECK((result == Result::kStarted) == expected[op]);
  }
}

TEST_CASE("Each trigger condition refuses every incomplete operand prefix",
          "[room-triggers]") {
  constexpr std::array<std::size_t, 17> kSizes{0, 3, 5, 4, 3, 4, 5, 5, 3,
                                               1, 5, 6, 5, 5, 7, 4, 3};
  for (std::size_t opcode = 1; opcode < kSizes.size(); ++opcode) {
    for (std::size_t size = 1; size < kSizes[opcode]; ++size) {
      TriggerHost host;
      Bytes command(size, 0);
      command[0] = static_cast<std::uint8_t>(opcode);
      CHECK(Run(host, Section(command)) == Result::kRefused);
      CHECK(host.queued.empty());
    }
  }
}

TEST_CASE("A failed trigger record can fall through to the next record",
          "[room-triggers]") {
  TriggerHost host;
  host.actor_flags = 0x104000;
  auto bytes = Section({9, 0xFF});
  const auto second = bytes.size();
  bytes[2] = static_cast<std::uint8_t>(second);
  bytes[3] = 0;
  bytes.insert(bytes.end(), {8, 99, 0, 0xFF});
  CHECK(Run(host, bytes) == Result::kStarted);
  CHECK(host.event_id == 99);
}

TEST_CASE("Trigger masks keep their full halfword before special dispatch",
          "[room-triggers]") {
  TriggerHost host;
  // A fixed uint16_t enum preserves imported masks outside the named requests.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const event::RoomTriggerQuery query{static_cast<Request>(0x1FF), 0,
                                      std::monostate{}};
  CHECK(event::EvaluateRoomEventTriggers(query, Section({0xFF}), host) ==
        Result::kStarted);
}
