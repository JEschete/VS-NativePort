#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

#include <catch2/catch_test_macros.hpp>

#include "Battle/FloatingLabelQueue.hpp"
#include "TestSupport/FloatingLabelQueueFixture.hpp"

namespace {
namespace b = VS::Battle;
namespace f = VS::test_support::floating_labels;
}  // namespace

TEST_CASE("F00712 reset clears every producer payload and its five commands",
          "[floating-label-queue][F00712]") {
  f::Queue queue;
  queue.Populate();
  queue.count = 31;
  REQUIRE(b::ResetFloatingLabels(queue, queue.ResetView()));
  CHECK(queue.count == 0);
  const std::array commands{queue.visibility, queue.cue, queue.timer,
                            queue.suppression};
  CHECK(commands == std::array<std::int16_t, 4>{});
  CHECK(std::ranges::all_of(
      queue.records, [](const auto& record) { return record == f::Record{}; }));
  queue.records.back().kind = 7;
  REQUIRE(b::ResetFloatingLabels(queue, queue.ResetView()));
  CHECK(queue.records.back() == f::Record{});
}

TEST_CASE("F00712 missing storage does not roll back preceding scalar resets",
          "[floating-label-queue][F00712]") {
  f::Queue queue;
  queue.owned = false;
  queue.count = -123;
  CHECK_FALSE(b::ResetFloatingLabels(queue, queue.ResetView()));
  CHECK(queue.count == 0);
  CHECK(queue.visibility == 0);
  CHECK(queue.cue == 0);
  CHECK(queue.timer == 0);
  CHECK(queue.suppression == 0);
}

TEST_CASE("F00713 enqueues borrowed world coordinates without clearing payload",
          "[floating-label-queue][F00713]") {
  f::Queue queue;
  queue.Populate();
  queue.count = 4;
  const auto before = queue.records.at(4);
  const std::array<std::int16_t, 3> position{-32768, 32767, -1};
  REQUIRE(b::QueueWorldLockedLabel(queue, position));
  auto expected = before;
  expected.position = position;
  expected.kind = 3;
  expected.elapsed = 0;
  CHECK(queue.records.at(4) == expected);
  CHECK(queue.count == 5);
}

TEST_CASE("F00713 short positions preserve writes preceding the missing axis",
          "[floating-label-queue][F00713]") {
  f::Queue queue;
  queue.Populate();
  const std::array<std::int16_t, 1> position{123};
  CHECK_FALSE(b::QueueWorldLockedLabel(queue, position));
  CHECK(queue.records.front().position ==
        std::array<std::int16_t, 3>{123, -22, 33});
  CHECK(queue.records.front().kind == 3);
  CHECK(queue.records.front().elapsed == 0);
  CHECK(queue.count == 0);
}

TEST_CASE("F00713 full-queue source aliases are read after reserve moves",
          "[floating-label-queue][F00713]") {
  f::Queue queue;
  queue.Populate();
  queue.count = b::kFloatingLabelCapacity;
  queue.records.at(5).position = {101, 202, 303};
  const auto position = std::span(queue.records.at(4).position);
  REQUIRE(b::QueueWorldLockedLabel(queue, position));
  CHECK(queue.records.back().position ==
        std::array<std::int16_t, 3>{101, 202, 303});
  CHECK(queue.records.front().actor == 1);
  CHECK(queue.records.back().kind == 3);
  CHECK(queue.count == b::kFloatingLabelCapacity);
}

TEST_CASE("F00713 saturation does not read a source cleared by reserve early",
          "[floating-label-queue][F00713]") {
  f::Queue queue;
  queue.Populate();
  queue.count = b::kFloatingLabelCapacity;
  const auto source = std::span(queue.records.back().position);
  REQUIRE(b::QueueWorldLockedLabel(queue, source));
  CHECK(queue.records.back().position == std::array<std::int16_t, 3>{});
  CHECK(queue.records.back().actor == 0);
  CHECK(queue.records.back().elapsed == 0);
}

TEST_CASE(
    "F00721 advances a waiting label without touching its producer fields",
    "[floating-label-queue][F00721]") {
  f::Queue queue;
  queue.Populate();
  auto& record = queue.records.front();
  auto expected = record;
  expected.waiting_steps = -4;
  expected.elapsed = 0;
  b::AdvanceFloatingLabelQueue({record.waiting_steps, record.elapsed});
  CHECK(record == expected);
  b::AdvanceFloatingLabelQueue({record.waiting_steps, record.elapsed});
  CHECK(record.waiting_steps == -5);
}

TEST_CASE("F00721 queue-delay decrement wraps as an original unsigned halfword",
          "[floating-label-queue][F00721]") {
  f::Record record;
  record.waiting_steps = std::numeric_limits<std::int16_t>::min();
  record.elapsed = 255;
  b::AdvanceFloatingLabelQueue({record.waiting_steps, record.elapsed});
  CHECK(record.waiting_steps == std::numeric_limits<std::int16_t>::max());
  CHECK(record.elapsed == 0);
  record.waiting_steps = 0;
  b::AdvanceFloatingLabelQueue({record.waiting_steps, record.elapsed});
  CHECK(record.waiting_steps == -1);
}

TEST_CASE("F00734 nonfull reservation neither clears nor publishes its slot",
          "[floating-label-queue][F00734]") {
  f::Queue queue;
  queue.Populate();
  queue.count = 31;
  const auto before = queue.records;
  const auto slot = b::ReserveFloatingLabel(queue);
  REQUIRE(slot.has_value());
  if (!slot) return;
  CHECK(slot.value() == b::FloatingLabelSlot{31});
  CHECK(queue.records == before);
  CHECK(queue.count == 31);
  CHECK(queue.copies == 0);
}

TEST_CASE(
    "F00734 full reservation shifts whole payloads forward without alias loss",
    "[floating-label-queue][F00734]") {
  f::Queue queue;
  queue.Populate();
  queue.count = b::kFloatingLabelCapacity;
  const auto before = queue.records;
  const auto slot = b::ReserveFloatingLabel(queue);
  REQUIRE(slot.has_value());
  if (!slot) return;
  CHECK(slot.value() == b::FloatingLabelSlot{31});
  CHECK(std::equal(queue.records.begin(), queue.records.end() - 1,
                   before.begin() + 1));
  CHECK(queue.records.back() == f::Record{});
  CHECK(queue.count == b::kFloatingLabelCapacity);
  CHECK(queue.copies == 31);
}

TEST_CASE("F00713 repeated full publication retains complete ordered payloads",
          "[floating-label-queue][F00713][F00734]") {
  f::Queue queue;
  queue.Populate();
  queue.count = 32;
  auto expected = queue.records;
  for (std::int16_t value = 0; value < 96; ++value) {
    const std::array<std::int16_t, 3> position{
        value, static_cast<std::int16_t>(-value), 32767};
    std::shift_left(expected.begin(), expected.end(), 1);
    expected.back() = {};
    expected.back().position = position;
    expected.back().kind = 3;
    REQUIRE(b::QueueWorldLockedLabel(queue, position));
    CHECK(queue.count == 32);
    CHECK(queue.records == expected);
  }
}

TEST_CASE("Reserved slot publication preserves a callback-extended prefix",
          "[floating-label-queue][F00734]") {
  f::Queue queue;
  queue.Populate();
  queue.count = 5;
  const auto records = queue.records;
  REQUIRE(b::CommitFloatingLabel(queue, {2}));
  CHECK(queue.count == 5);
  CHECK(queue.records == records);
  queue.count = 0;
  REQUIRE(b::CommitFloatingLabel(queue, {0}));
  CHECK(queue.count == 1);
}

TEST_CASE(
    "Reserved slot publication refuses corrupt extents instead of healing",
    "[floating-label-queue][F00734]") {
  f::Queue queue;
  queue.count = 33;
  CHECK_FALSE(b::CommitFloatingLabel(queue, {31}));
  CHECK(queue.count == 33);
  queue.count = 31;
  CHECK_FALSE(b::CommitFloatingLabel(queue, {32}));
  CHECK(queue.count == 31);
  queue.count = -1;
  CHECK_FALSE(b::CommitFloatingLabel(queue, {0}));
  CHECK(queue.count == -1);
  queue.count = 0;
  CHECK_FALSE(b::CommitFloatingLabel(queue, {31}));
  CHECK(queue.count == 0);
}

TEST_CASE("F00734 repeated full reserve is not an implicit count repair",
          "[floating-label-queue][F00734]") {
  f::Queue queue;
  queue.Populate();
  queue.count = 33;
  REQUIRE(b::ReserveFloatingLabel(queue).has_value());
  REQUIRE(b::ReserveFloatingLabel(queue).has_value());
  CHECK(queue.records.front().actor == 2);
  CHECK(queue.records.at(30) == f::Record{});
  CHECK(queue.records.back() == f::Record{});
  CHECK(queue.count == 33);
}

TEST_CASE("F00734 invalid used slot or absent owner is refused",
          "[floating-label-queue][F00734]") {
  f::Queue queue;
  queue.count = -1;
  CHECK_FALSE(b::ReserveFloatingLabel(queue).has_value());
  queue.count = 0;
  queue.owned = false;
  CHECK_FALSE(b::ReserveFloatingLabel(queue).has_value());
  CHECK(queue.count == 0);
}

TEST_CASE("F00735 compaction rescans the earliest inactive moved slot",
          "[floating-label-queue][F00735]") {
  f::Queue queue;
  queue.Populate();
  queue.count = 5;
  queue.records.at(1).kind = 0;
  queue.records.at(2).kind = 0;
  REQUIRE(b::CompactFloatingLabels(queue, 2));
  CHECK(queue.count == 3);
  CHECK(queue.records.at(0).actor == 0);
  CHECK(queue.records.at(1).actor == 3);
  CHECK(queue.records.at(2).actor == 4);
  CHECK(queue.records.at(3) == f::Record{});
  CHECK(queue.records.at(4) == f::Record{});
}

TEST_CASE(
    "F00735 an active queue and excess removal requests do not remove more",
    "[floating-label-queue][F00735]") {
  f::Queue queue;
  queue.Populate();
  queue.count = 3;
  const auto before = queue.records;
  REQUIRE(b::CompactFloatingLabels(queue, 10));
  CHECK(queue.records == before);
  CHECK(queue.count == 3);
  queue.records.front().kind = 0;
  REQUIRE(b::CompactFloatingLabels(queue, 10));
  CHECK(queue.count == 2);
  CHECK(queue.records.front().actor == 1);
}

TEST_CASE("F00735 empty and unrequested storage stays lazy",
          "[floating-label-queue][F00735]") {
  f::Queue queue;
  queue.owned = false;
  CHECK(b::CompactFloatingLabels(queue, 4));
  queue.count = 20;
  CHECK(b::CompactFloatingLabels(queue, 0));
  CHECK(b::CompactFloatingLabels(queue, -1));
  CHECK_FALSE(b::CompactFloatingLabels(queue, 1));
}

TEST_CASE("F00736 removing first middle or last preserves full payload order",
          "[floating-label-queue][F00736]") {
  for (const int removed : {0, 2, 4}) {
    f::Queue queue;
    queue.Populate();
    queue.count = 5;
    const auto before = queue.records;
    REQUIRE(b::RemoveFloatingLabel(queue, {removed}));
    CHECK(queue.count == 4);
    for (int index = 0; index < 4; ++index) {
      const auto from = index < removed ? index : index + 1;
      CHECK(queue.records.at(static_cast<std::size_t>(index)) ==
            before.at(static_cast<std::size_t>(from)));
    }
    CHECK(queue.records.at(4) == f::Record{});
    CHECK(queue.records.at(5) == before.at(5));
  }
}

TEST_CASE("F00736 an unused high index still discards the original final slot",
          "[floating-label-queue][F00736]") {
  f::Queue queue;
  queue.Populate();
  queue.count = 3;
  const auto before = queue.records;
  REQUIRE(b::RemoveFloatingLabel(queue, {999}));
  CHECK(queue.count == 2);
  CHECK(queue.records.front() == before.front());
  CHECK(queue.records.at(1) == before.at(1));
  CHECK(queue.records.at(2) == f::Record{});
}

TEST_CASE("F00736 repeated removal refuses the first actually invalid access",
          "[floating-label-queue][F00736]") {
  f::Queue queue;
  queue.Populate();
  queue.count = 1;
  REQUIRE(b::RemoveFloatingLabel(queue, {0}));
  CHECK(queue.records.front() == f::Record{});
  CHECK_FALSE(b::RemoveFloatingLabel(queue, {0}));
  CHECK(queue.count == -1);
  queue.count = 4;
  CHECK_FALSE(b::RemoveFloatingLabel(queue, {-1}));
  CHECK(queue.count == 3);
}

TEST_CASE(
    "F00736 corrupt counts retain earlier copies before an invalid source",
    "[floating-label-queue][F00736]") {
  f::Queue queue;
  queue.Populate();
  queue.count = 33;
  const auto tail = queue.records.back();
  CHECK_FALSE(b::RemoveFloatingLabel(queue, {0}));
  CHECK(queue.count == 32);
  CHECK(queue.records.front().actor == 1);
  CHECK(queue.records.at(30) == tail);
  CHECK(queue.records.back() == tail);
  CHECK(queue.copies == 31);
}
