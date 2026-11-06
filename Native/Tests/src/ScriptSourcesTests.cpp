#include <algorithm>
#include <array>
#include <csetjmp>
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

template <typename T, typename E>
T Required(const std::expected<T, E>& value) {
  REQUIRE(value);
  return value.value();
}

template <typename T, typename E>
void CheckError(const std::expected<T, E>& actual, E error) {
  REQUIRE_FALSE(actual);
  CHECK(actual.error() == error);
}

event::ScriptCommand StartMountedUse(
    event::ScriptSourceMounts& mounts, event::ScriptRegion region,
    const std::shared_ptr<const Buffer>& buffer) {
  REQUIRE(mounts.MountBuffer(region, buffer));
  REQUIRE(mounts.BeginUse());
  return Required(mounts.Borrow(region));
}

[[noreturn]] void TransferAfterSourceUse(event::ScriptSourceMounts& mounts,
                                         std::jmp_buf& boundary) {
  if (!mounts.BeginUse() || !mounts.Borrow(event::ScriptRegion::kEventScript)) {
    throw std::logic_error("Could not begin the source use");
  }
  mounts.Reset();
  mounts.AbandonUses();
  // No automatic owning objects cross this transition, as in SessionLive.
  // NOLINTNEXTLINE(cert-err52-cpp): exercises the explicit abandonment
  // boundary.
  std::longjmp(boundary, 1);
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

TEST_CASE("Script mounts keep every exposed generation through nested uses",
          "[event-vm][script-sources]") {
  const auto region = GENERATE(event::ScriptRegion::kEventScript,
                               event::ScriptRegion::kDoorEvents,
                               event::ScriptRegion::kSubroutine);
  event::ScriptSourceMounts mounts;
  auto first = MakeBuffer();
  const std::weak_ptr<const Buffer> first_owner = first;
  const auto outer = StartMountedUse(mounts, region, first);
  first.reset();
  auto second = MakeBuffer();
  const std::weak_ptr<const Buffer> second_owner = second;
  const auto inner = StartMountedUse(mounts, region, second);
  second.reset();
  mounts.Reset();
  REQUIRE(mounts.EndUse());
  REQUIRE_FALSE(first_owner.expired());
  REQUIRE_FALSE(second_owner.expired());
  CheckBytes(outer, inner);
  REQUIRE(mounts.EndUse());
  CHECK(first_owner.expired());
  CHECK(second_owner.expired());
}

TEST_CASE("Script mount reset never reuses a same-address generation",
          "[event-vm][script-sources]") {
  event::ScriptSourceMounts mounts;
  const auto region = event::ScriptRegion::kEventScript;
  const auto buffer = MakeBuffer();
  REQUIRE(mounts.MountBuffer(region, buffer));
  const auto first = Required(mounts.Lease(region));
  const auto cursor = Required(first.At(0));
  mounts.Reset();
  REQUIRE(mounts.MountBuffer(region, buffer));
  const auto second = Required(mounts.Lease(region));
  CheckFreshMount(first, second, cursor);
  CHECK(second.Generation() == first.Generation() + 1);
}

TEST_CASE("Script mount borrowing requires an active use and a real source",
          "[event-vm][script-sources]") {
  event::ScriptSourceMounts mounts;
  const auto region = event::ScriptRegion::kEventScript;
  CheckError(mounts.Borrow(region), event::ScriptSourceError::kNoActiveUse);
  CheckError(mounts.EndUse(), event::ScriptSourceError::kNoActiveUse);
  REQUIRE(mounts.BeginUse());
  CheckError(mounts.Borrow(region), event::ScriptSourceError::kMissingOwner);
  CheckError(mounts.Borrow(event::ScriptRegion::kNone),
             event::ScriptSourceError::kRegion);
  REQUIRE(mounts.EndUse());
}

TEST_CASE("Repeated script borrows retain each generation only once",
          "[event-vm][script-sources]") {
  event::ScriptSourceMounts mounts;
  const auto region = event::ScriptRegion::kEventScript;
  auto buffer = MakeBuffer();
  const std::weak_ptr<const Buffer> owner = buffer;
  const auto first = StartMountedUse(mounts, region, buffer);
  REQUIRE(owner.use_count() == 3);
  for (int repeat = 0; repeat < 20; ++repeat) {
    CheckBytes(Required(mounts.Borrow(region)), first);
  }
  CHECK(owner.use_count() == 3);
  buffer.reset();
  REQUIRE(mounts.Clear(region));
  CHECK_FALSE(owner.expired());
  REQUIRE(mounts.EndUse());
  CHECK(owner.expired());
}

TEST_CASE("Rejected script mounts preserve the current owner and generation",
          "[event-vm][script-sources]") {
  event::ScriptSourceMounts mounts;
  const auto region = event::ScriptRegion::kEventScript;
  REQUIRE(mounts.MountBuffer(region, MakeBuffer()));
  const auto first = Required(mounts.Lease(region));
  CheckError(mounts.MountBuffer(region, {}),
             event::ScriptSourceError::kMissingOwner);
  CheckError(mounts.MountBuffer(event::ScriptRegion::kNone, MakeBuffer()),
             event::ScriptSourceError::kRegion);
  CheckError(mounts.MountRoom(event::ScriptRegion::kSubroutine, {}),
             event::ScriptSourceError::kRegion);
  CHECK(Required(mounts.Lease(region)).Generation() == first.Generation());
  REQUIRE(mounts.MountBuffer(region, MakeBuffer()));
  CHECK(Required(mounts.Lease(region)).Generation() == first.Generation() + 1);
}

TEST_CASE("Script mount abandonment releases uses without resetting serials",
          "[event-vm][script-sources]") {
  event::ScriptSourceMounts mounts;
  const auto region = event::ScriptRegion::kEventScript;
  auto buffer = MakeBuffer();
  const std::weak_ptr<const Buffer> owner = buffer;
  static_cast<void>(StartMountedUse(mounts, region, buffer));
  const auto generation = Required(mounts.Lease(region)).Generation();
  buffer.reset();
  REQUIRE(mounts.BeginUse());
  mounts.Reset();
  CHECK_FALSE(owner.expired());
  mounts.AbandonUses();
  CHECK(owner.expired());
  CheckError(mounts.EndUse(), event::ScriptSourceError::kNoActiveUse);
  REQUIRE(mounts.MountBuffer(region, MakeBuffer()));
  CHECK(Required(mounts.Lease(region)).Generation() == generation + 1);
}

TEST_CASE("Mounted room sources keep independent entry and door leases",
          "[event-vm][script-sources]") {
  const std::array<std::uint8_t, 2> entry{0x11, 0x22};
  const std::array<std::uint8_t, 3> doors{0x33, 0x44, 0x55};
  auto room = Required(
      event::RoomEventSections::Copy({.entry_script = entry, .doors = doors}));
  const std::weak_ptr<const event::RoomEventSections> owner = room;
  event::ScriptSourceMounts mounts;
  REQUIRE(mounts.MountRoom(event::ScriptRegion::kEventScript, room));
  REQUIRE(mounts.MountRoom(event::ScriptRegion::kDoorEvents, room));
  room.reset();
  auto door = Required(mounts.Lease(event::ScriptRegion::kDoorEvents));
  REQUIRE(mounts.Clear(event::ScriptRegion::kEventScript));
  CheckBytes(door.Bytes(), doors);
  mounts.Reset();
  CHECK_FALSE(owner.expired());
  door = {};
  CHECK(owner.expired());
}

TEST_CASE("Empty mounted sources differ from absent and invalid regions",
          "[event-vm][script-sources]") {
  event::ScriptSourceMounts mounts;
  const auto region = event::ScriptRegion::kEventScript;
  REQUIRE(mounts.MountBuffer(region, std::make_shared<const Buffer>()));
  REQUIRE(mounts.BeginUse());
  CHECK(Required(mounts.Borrow(region)).empty());
  CHECK(Required(mounts.Lease(region)).Generation() != 0);
  CheckError(mounts.Clear(event::ScriptRegion::kNone),
             event::ScriptSourceError::kRegion);
  CheckError(mounts.Lease(event::ScriptRegion::kNone),
             event::ScriptSourceError::kRegion);
  REQUIRE(mounts.Clear(region));
  CheckError(mounts.Lease(region), event::ScriptSourceError::kMissingOwner);
  REQUIRE(mounts.EndUse());
}

TEST_CASE("Script source uses release before an actual nonreturning transfer",
          "[event-vm][script-sources]") {
  const auto mounts = std::make_unique<event::ScriptSourceMounts>();
  const std::weak_ptr<const Buffer> owner = [&] {
    const auto buffer = MakeBuffer();
    REQUIRE(mounts->MountBuffer(event::ScriptRegion::kEventScript, buffer));
    return std::weak_ptr<const Buffer>(buffer);
  }();
  std::jmp_buf boundary;
  // The pointer and weak owner are unchanged across setjmp; only heap state
  // is mutated by the called function.
  if (setjmp(boundary) == 0) {
    TransferAfterSourceUse(*mounts, boundary);
  }
  CHECK(owner.expired());
  CheckError(mounts->EndUse(), event::ScriptSourceError::kNoActiveUse);
  REQUIRE(mounts->MountBuffer(event::ScriptRegion::kEventScript, MakeBuffer()));
  CHECK(
      Required(mounts->Lease(event::ScriptRegion::kEventScript)).Generation() ==
      2);
}

TEST_CASE("Script source preparation does not replace the published mount",
          "[event-vm][script-sources]") {
  event::ScriptSourceMounts mounts;
  const auto region = event::ScriptRegion::kEventScript;
  REQUIRE(mounts.MountBuffer(region, MakeBuffer()));
  const auto previous = Required(mounts.Lease(region)).Generation();
  auto prepared = mounts.PrepareBuffer(region, MakeBuffer());
  REQUIRE(prepared);
  CHECK(Required(mounts.Lease(region)).Generation() == previous);
  REQUIRE(mounts.Publish(std::move(*prepared)));
  CHECK(Required(mounts.Lease(region)).Generation() == previous + 1);
  CheckError(mounts.Publish(std::move(*prepared)),
             event::ScriptSourceError::kPreparedSource);
  CHECK(Required(mounts.Lease(region)).Generation() == previous + 1);
}

TEST_CASE("Discarded script preparation releases its unpublished owner",
          "[event-vm][script-sources]") {
  event::ScriptSourceMounts mounts;
  auto buffer = MakeBuffer();
  const std::weak_ptr<const Buffer> owner = buffer;
  auto prepared =
      mounts.PrepareBuffer(event::ScriptRegion::kEventScript, buffer);
  REQUIRE(prepared);
  buffer.reset();
  CHECK_FALSE(owner.expired());
  prepared = event::PreparedScriptSource{};
  CHECK(owner.expired());
  CheckError(mounts.Lease(event::ScriptRegion::kEventScript),
             event::ScriptSourceError::kMissingOwner);
}

TEST_CASE("Script source reset refuses a previously prepared publication",
          "[event-vm][script-sources]") {
  event::ScriptSourceMounts mounts;
  const auto region = event::ScriptRegion::kEventScript;
  auto prepared = mounts.PrepareBuffer(region, MakeBuffer());
  REQUIRE(prepared);
  mounts.Reset();
  CheckError(mounts.Publish(std::move(*prepared)),
             event::ScriptSourceError::kPreparedSource);
  CheckError(mounts.Lease(region), event::ScriptSourceError::kMissingOwner);
  REQUIRE(mounts.MountBuffer(region, MakeBuffer()));
  CHECK(Required(mounts.Lease(region)).Generation() == 2);
}

TEST_CASE("Prepared script publication cannot target a different mount set",
          "[event-vm][script-sources]") {
  event::ScriptSourceMounts origin;
  event::ScriptSourceMounts other;
  const auto region = event::ScriptRegion::kEventScript;
  auto prepared = origin.PrepareBuffer(region, MakeBuffer());
  REQUIRE(prepared);
  CheckError(other.Publish(std::move(*prepared)),
             event::ScriptSourceError::kPreparedSource);
  CheckError(other.Lease(region), event::ScriptSourceError::kMissingOwner);
  REQUIRE(origin.Publish(std::move(*prepared)));
  CHECK(Required(origin.Lease(region)).Generation() == 1);
}

TEST_CASE("Prepared script moves transfer the single publication right",
          "[event-vm][script-sources]") {
  event::ScriptSourceMounts mounts;
  const auto region = event::ScriptRegion::kEventScript;
  auto prepared = mounts.PrepareBuffer(region, MakeBuffer());
  REQUIRE(prepared);
  event::PreparedScriptSource moved(std::move(*prepared));
  CheckError(mounts.Publish(std::move(*prepared)),
             event::ScriptSourceError::kPreparedSource);
  event::PreparedScriptSource assigned;
  assigned = std::move(moved);
  REQUIRE(mounts.Publish(std::move(assigned)));
  // Publication leaves an invalid token whose reuse must be refused.
  // NOLINTNEXTLINE(bugprone-use-after-move)
  CheckError(mounts.Publish(std::move(assigned)),
             event::ScriptSourceError::kPreparedSource);
  CHECK(Required(mounts.Lease(region)).Generation() == 1);
}

TEST_CASE("Room source preparation can precede nonallocating publication",
          "[event-vm][script-sources]") {
  const std::array<std::uint8_t, 2> entry{0x11, 0x22};
  auto room = Required(event::RoomEventSections::Copy({.entry_script = entry}));
  const std::weak_ptr<const event::RoomEventSections> owner = room;
  event::ScriptSourceMounts mounts;
  const auto region = event::ScriptRegion::kEventScript;
  auto prepared = mounts.PrepareRoom(region, room);
  REQUIRE(prepared);
  room.reset();
  CHECK_FALSE(owner.expired());
  CheckError(mounts.Lease(region), event::ScriptSourceError::kMissingOwner);
  REQUIRE(mounts.Publish(std::move(*prepared)));
  CheckBytes(Required(mounts.Lease(region)).Bytes(), entry);
  mounts.Reset();
  CHECK(owner.expired());
}
