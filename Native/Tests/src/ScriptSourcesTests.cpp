#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Event/ScriptSources.hpp"

namespace {
namespace event = VS::Event;
using Buffer = std::vector<std::uint8_t>;

std::shared_ptr<const Buffer> MakeBuffer() {
  return std::make_shared<const Buffer>(Buffer{0x10, 0x20, 0x30});
}

template <typename T>
T Required(const std::optional<T>& value) {
  REQUIRE(value);
  if (!value) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *value;
}

void CheckBytes(event::ScriptCommand actual, event::ScriptCommand expected) {
  CHECK(actual.size() == expected.size());
  CHECK(std::ranges::equal(actual, expected));
}

void CheckFreshMount(const event::ScriptSource& original,
                     const event::ScriptSource& replacement,
                     event::ScriptLocation cursor) {
  CHECK(replacement.Bytes().data() == original.Bytes().data());
  CHECK(replacement.Generation() != original.Generation());
  CHECK_FALSE(replacement.View(cursor));
}

void CheckOutsideOffsets(const event::ScriptSource& source,
                         event::ScriptLocation cursor) {
  CHECK_FALSE(source.At(3));
  CHECK_FALSE(source.At(std::numeric_limits<std::uint32_t>::max()));
  cursor.offset = 3;
  CHECK_FALSE(source.View(cursor));
}
}  // namespace

TEST_CASE("Script leases retain replaced buffers and reject stale locations",
          "[event-vm][script-sources]") {
  std::uint64_t serial = 0;
  auto buffer = MakeBuffer();
  const std::weak_ptr<const Buffer> old_owner = buffer;
  auto first = event::ScriptSource::FromBuffer(
      event::ScriptRegion::kEventScript, buffer, serial);
  REQUIRE(first);
  const auto cursor = Required(first->At(1));
  const event::ScriptSource executing = *first;
  auto replacement = event::ScriptSource::FromBuffer(
      event::ScriptRegion::kEventScript, buffer, serial);
  REQUIRE(replacement);
  CheckFreshMount(*first, *replacement, cursor);
  first = std::move(replacement);
  buffer.reset();
  first = event::ScriptSource{};
  CHECK_FALSE(old_owner.expired());
  const auto bytes = Required(executing.View(cursor));
  const std::array<std::uint8_t, 2> expected{0x20, 0x30};
  CheckBytes(bytes, expected);
}

TEST_CASE("Script source moves revoke the moved-from view",
          "[event-vm][script-sources]") {
  std::uint64_t serial = 0;
  auto prepared = event::ScriptSource::FromBuffer(
      event::ScriptRegion::kEventScript, MakeBuffer(), serial);
  REQUIRE(prepared);
  const auto cursor = Required(prepared->At(0));
  event::ScriptSource moved(std::move(*prepared));
  CHECK(prepared->Bytes().empty());
  CHECK(prepared->Generation() == 0);
  CHECK_FALSE(prepared->View(cursor));
  CHECK(moved.View(cursor).has_value());
  event::ScriptSource assigned;
  assigned = std::move(moved);
  // The custom move guarantees an empty source, not an unspecified state.
  // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
  CHECK(moved.Bytes().empty());
  // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
  CHECK_FALSE(moved.View(cursor));
  CHECK(assigned.View(cursor).has_value());
}

TEST_CASE("Replacing a script mount releases only its unleased owner",
          "[event-vm][script-sources]") {
  std::uint64_t serial = 0;
  auto buffer = MakeBuffer();
  const std::weak_ptr<const Buffer> old_owner = buffer;
  auto original = event::ScriptSource::FromBuffer(
      event::ScriptRegion::kEventScript, buffer, serial);
  REQUIRE(original);
  buffer.reset();
  auto mounted = std::move(original.value());
  auto replacement = event::ScriptSource::FromBuffer(
      event::ScriptRegion::kEventScript, MakeBuffer(), serial);
  REQUIRE(replacement);
  mounted = std::move(replacement.value());
  CHECK(old_owner.expired());
  CHECK(replacement->Bytes().empty());
  const auto cursor = Required(mounted.At(0));
  event::ScriptSource pinned;
  pinned = mounted;
  mounted = {};
  CHECK(pinned.View(cursor).has_value());
  CHECK_FALSE(mounted.View(cursor));
}

TEST_CASE("Script source offsets and region identities are bounded",
          "[event-vm][script-sources]") {
  const auto region = GENERATE(event::ScriptRegion::kEventScript,
                               event::ScriptRegion::kDoorEvents,
                               event::ScriptRegion::kSubroutine);
  std::uint64_t serial = 0;
  const auto source =
      event::ScriptSource::FromBuffer(region, MakeBuffer(), serial);
  REQUIRE(source);
  const auto cursor = Required(source->At(2));
  CHECK(cursor.region == region);
  CHECK(Required(source->View(cursor)).size() == 1);
  CheckOutsideOffsets(*source, cursor);
  auto outside = cursor;
  outside.region = event::ScriptRegion::kNone;
  CHECK_FALSE(source->View(outside));
}

TEST_CASE("Room script leases retain their original door and entry bytes",
          "[event-vm][script-sources]") {
  const std::array<std::uint8_t, 2> entry{0x11, 0x22};
  const std::array<std::uint8_t, 3> doors{0x33, 0x44, 0x55};
  auto room =
      event::RoomEventSections::Copy({.entry_script = entry, .doors = doors});
  REQUIRE(room);
  const std::weak_ptr<const event::RoomEventSections> owner = *room;
  std::uint64_t serial = 0;
  auto script = event::ScriptSource::FromRoom(event::ScriptRegion::kEventScript,
                                              *room, serial);
  auto door = event::ScriptSource::FromRoom(event::ScriptRegion::kDoorEvents,
                                            *room, serial);
  REQUIRE(script);
  REQUIRE(door);
  room->reset();
  CHECK_FALSE(owner.expired());
  CheckBytes(script->Bytes(), entry);
  CheckBytes(door->Bytes(), doors);
  script = event::ScriptSource{};
  CHECK_FALSE(owner.expired());
  door = event::ScriptSource{};
  CHECK(owner.expired());
}

TEST_CASE("Empty script sources own no readable position",
          "[event-vm][script-sources]") {
  std::uint64_t serial = 0;
  const auto source =
      event::ScriptSource::FromBuffer(event::ScriptRegion::kEventScript,
                                      std::make_shared<const Buffer>(), serial);
  REQUIRE(source);
  CHECK(source->Bytes().empty());
  CHECK_FALSE(source->At(0));
  CHECK_FALSE(source->View(
      {.region = event::ScriptRegion::kEventScript, .generation = serial}));
  const event::ScriptSource absent;
  CHECK_FALSE(absent.At(0));
  CHECK_FALSE(absent.View({}));
}

TEST_CASE("Invalid script source preparation does not consume a generation",
          "[event-vm][script-sources]") {
  std::uint64_t serial = 7;
  const auto bad_region = event::ScriptSource::FromBuffer(
      event::ScriptRegion::kNone, MakeBuffer(), serial);
  REQUIRE_FALSE(bad_region);
  CHECK(bad_region.error() == event::ScriptSourceError::kRegion);
  const auto no_buffer = event::ScriptSource::FromBuffer(
      event::ScriptRegion::kEventScript, {}, serial);
  REQUIRE_FALSE(no_buffer);
  CHECK(no_buffer.error() == event::ScriptSourceError::kMissingOwner);
  const auto no_room = event::ScriptSource::FromRoom(
      event::ScriptRegion::kDoorEvents, {}, serial);
  REQUIRE_FALSE(no_room);
  CHECK(no_room.error() == event::ScriptSourceError::kMissingOwner);
  const auto not_a_room_region = event::ScriptSource::FromRoom(
      event::ScriptRegion::kSubroutine, {}, serial);
  REQUIRE_FALSE(not_a_room_region);
  CHECK(not_a_room_region.error() == event::ScriptSourceError::kRegion);
  CHECK(serial == 7);
}

TEST_CASE("Script source generation exhaustion never wraps or publishes",
          "[event-vm][script-sources]") {
  std::uint64_t serial = std::numeric_limits<std::uint64_t>::max() - 1;
  const auto last = event::ScriptSource::FromBuffer(
      event::ScriptRegion::kEventScript, MakeBuffer(), serial);
  REQUIRE(last);
  CHECK(last->Generation() == std::numeric_limits<std::uint64_t>::max());
  const auto refused = event::ScriptSource::FromBuffer(
      event::ScriptRegion::kEventScript, MakeBuffer(), serial);
  REQUIRE_FALSE(refused);
  CHECK(refused.error() == event::ScriptSourceError::kGenerationExhausted);
  CHECK(serial == std::numeric_limits<std::uint64_t>::max());
  const auto retry = event::NextScriptSourceGeneration(serial);
  REQUIRE_FALSE(retry);
  CHECK(retry.error() == event::ScriptSourceError::kGenerationExhausted);
  CHECK(last->Bytes().front() == 0x10);
}
