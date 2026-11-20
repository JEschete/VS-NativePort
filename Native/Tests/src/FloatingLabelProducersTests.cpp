#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/FloatingLabelProducers.hpp"
#include "Battle/FloatingLabelProducersLive.hpp"
#include "TestSupport/FloatingLabelProducersFixture.hpp"
#include "TestSupport/FloatingLabelQueueLiveFixture.hpp"

namespace {
namespace b = VS::Battle;
namespace fixture = VS::test_support::floating_label_producers;
namespace live = VS::test_support::live_floating_labels;
using fixture::Call;
using fixture::Service;

constexpr std::int32_t kActor = 3;
using Kind = b::FloatingLabelKind;
constexpr std::uint8_t KindByte(Kind kind) {
  return static_cast<std::uint8_t>(kind);
}
constexpr auto kValueKind = KindByte(Kind::kActorValue);
constexpr auto kStatusKind = KindByte(Kind::kActorStatus);
constexpr auto kTimingKind = KindByte(Kind::kAbilityTimingResult);
constexpr auto kChainKind = KindByte(Kind::kChainCount);

b::FloatingLabelRecord Label(std::uint8_t kind, std::uint8_t actor,
                             std::int16_t order = 0) {
  b::FloatingLabelRecord record{};
  record.kind = kind;
  record.actor = actor;
  record.position.w = order;
  return record;
}

void Fill(fixture::World& world, std::size_t count) {
  for (std::size_t index = 0; index < count; ++index) {
    auto record = Label(1, 9);
    record.value = static_cast<std::uint16_t>(index);
    world.SetRecord(index, record);
  }
  world.queue.count = static_cast<std::int16_t>(count);
}

TEST_CASE("Chain count replaces the first shared chain label",
          "[battle][floating-label-producers]") {
  fixture::World world;
  world.SetRecord(0, Label(kChainKind, 1));
  world.SetRecord(1, Label(kChainKind, 0));
  world.SetRecord(2, Label(kChainKind, 0));
  world.queue.count = 3;
  const auto result = b::QueueChainCount(4321, world);
  REQUIRE(result.has_value());
  CHECK(result == 0);
  CHECK(world.queue.count == 3);
  CHECK(world.Record(0).actor == 1);
  CHECK(world.Record(1).kind == kChainKind);
  CHECK(world.Record(2).kind == kChainKind);
  CHECK(world.Record(2).value == b::kMaxLabelValue);
  CHECK(world.Record(2).actor == 0);
}

TEST_CASE("Chain count zero only removes and a 16-bit count wraps",
          "[battle][floating-label-producers]") {
  fixture::World world;
  world.SetRecord(0, Label(kChainKind, 0));
  world.queue.count = 1;
  CHECK(b::QueueChainCount(0, world) == 0);
  CHECK(world.queue.count == 0);
  CHECK(world.Record(0).kind == 0);
  CHECK(b::QueueChainCount(0x10005, world) == 0);
  CHECK(world.queue.count == 1);
  CHECK(world.Record(0).value == 5);
  CHECK(b::QueueChainCount(-1, world) == 0);
  CHECK(world.queue.count == 1);
  CHECK(world.Record(0).value == b::kMaxLabelValue);
}

TEST_CASE("Timing result replaces its label and keeps the random jitter bits",
          "[battle][floating-label-producers]") {
  fixture::World world;
  world.SetRecord(0, Label(kTimingKind, 2));
  world.SetRecord(1, Label(kTimingKind, 0));
  world.queue.count = 2;
  world.random = 0x7FFD;
  CHECK(b::QueueAbilityTimingResult(0x12345, world) == 0);
  CHECK(world.queue.count == 2);
  CHECK(world.Record(0).actor == 2);
  const auto record = world.Record(1);
  CHECK(record.kind == kTimingKind);
  CHECK(record.value == 0x2345);
  CHECK(record.text_length == 5);
  CHECK(world.calls == std::vector<Call>{{Service::kRandom, {}}});
}

TEST_CASE("A full queue evicts the oldest label for a new chain count",
          "[battle][floating-label-producers]") {
  fixture::World world;
  Fill(world, b::kFloatingLabelCapacity);
  CHECK(b::QueueChainCount(12, world) == 0);
  CHECK(world.queue.count == b::kFloatingLabelCapacity);
  CHECK(world.Record(0).value == 1);
  CHECK(world.Record(30).value == 31);
  CHECK(world.Record(31).kind == kChainKind);
  CHECK(world.Record(31).value == 12);
}

TEST_CASE("A full queue reserve stays cleared when the anchor is off screen",
          "[battle][floating-label-producers]") {
  fixture::World world;
  Fill(world, b::kFloatingLabelCapacity);
  world.projections = {{0, {321, 10, 0, 0}}};
  CHECK(b::QueueActorStatusLabel(kActor, b::kLabelAnchorTag, 4,
                                 b::kRequireOnScreenFlag, world) == 0);
  CHECK(world.queue.count == b::kFloatingLabelCapacity);
  CHECK(world.Record(0).value == 1);
  CHECK(world.Record(31) == b::FloatingLabelRecord{});
  CHECK(world.calls ==
        std::vector<Call>{{Service::kProjectTaggedBone, {kActor, 251, 0}}});
}

TEST_CASE("A corrupt queue count refuses the reserve",
          "[battle][floating-label-producers]") {
  fixture::World world;
  world.queue.count = -1;
  CHECK_FALSE(b::QueueChainCount(3, world).has_value());
  CHECK_FALSE(b::QueueAbilityTimingResult(3, world).has_value());
  CHECK_FALSE(b::QueueActorStatusLabels(kActor, 1, 0, world).has_value());
  CHECK(world.queue.count == -1);
  CHECK(world.Record(0) == b::FloatingLabelRecord{});
  CHECK(world.calls.empty());
}

// A value label anchored on the target point, 16-bit wrapped and capped.
b::FloatingLabelRecord SignedTargetLabel(fixture::World& world) {
  world.tagged_positions = {{0, {-30000, 5, -7, 99}}, {0, {10000, -20, 7, 0}}};
  const auto result =
      b::QueueActorValueLabel(kActor, b::kNoLimbTag, 0x80001234U, 0x7F, world);
  REQUIRE(result.has_value());
  CHECK(result == 0);
  return world.Record(0);
}

TEST_CASE("Value label positions are signed offsets from the anchor",
          "[battle][floating-label-producers]") {
  fixture::World world;
  const auto record = SignedTargetLabel(world);
  CHECK(record.position == b::LabelPoint{25536, 25, -14, 0});
  CHECK(record.value == (b::kLabelNegativeBit | b::kMaxLabelValue));
  CHECK(world.queue.count == 1);
}

TEST_CASE("Value label text caps the shown amount on the target point",
          "[battle][floating-label-producers]") {
  fixture::World world;
  const auto record = SignedTargetLabel(world);
  CHECK(record.text == std::array<std::uint8_t, 4>{'9', '9', '9', 0});
  CHECK(record.text_length == 3);
  CHECK(record.limb == b::kTargetPointSelector);
  CHECK(record.color_flags == 0x7F);
  CHECK(record.scale_x == b::kLabelUnitScale);
  CHECK(world.queue.count == 1);
}

TEST_CASE("Value label stacks over the actor and marks a repeated limb",
          "[battle][floating-label-producers]") {
  fixture::World world;
  auto first = Label(kValueKind, kActor, 0);
  first.limb = 6;
  first.frame = 4;
  auto second = Label(kValueKind, kActor, 1);
  second.limb = 6;
  second.frame = 9;
  world.SetRecord(0, first);
  world.SetRecord(1, second);
  world.SetRecord(2, Label(kValueKind, kActor, -1));
  world.SetRecord(3, Label(kValueKind, kActor + 1, 0));
  world.queue.count = 4;
  world.found_bone = 6;
  CHECK(b::QueueActorValueLabel(kActor, 17, 42, 0x83, world) == 0);
  const auto record = world.Record(4);
  CHECK(record.position.w == 2);
  CHECK(record.frame == 9);
  CHECK(record.limb == 6);
  CHECK(record.color_flags == (0x03 | b::kRepeatedLimbFlag));
  CHECK(record.text_length == 2);
  CHECK(world.calls ==
        std::vector<Call>{{Service::kFindBoneTag, {kActor, 17, 2}},
                          {Service::kBonePosition, {kActor, 6, 2}},
                          {Service::kProjectTaggedBone, {kActor, 251, 0}},
                          {Service::kTaggedBonePosition, {kActor, 251, 0}}});
}

TEST_CASE("Value label missing bones return before or after the reserve",
          "[battle][floating-label-producers]") {
  fixture::World world;
  world.found_bone = -1;
  CHECK(b::QueueActorValueLabel(kActor, 4, 5, 0, world) ==
        b::kLabelBoneMissing);
  CHECK(world.calls.size() == 1);
  world.found_bone = 2;
  world.bone_positions = {{1, {7, 8, 9, 10}}};
  CHECK(b::QueueActorValueLabel(kActor, 4, 5, 0, world) ==
        b::kLabelBoneMissing);
  CHECK(world.queue.count == 0);
  CHECK(world.Record(0).position == b::LabelPoint{7, 8, 9, 10});
  world.tagged_positions = {{0, {}}, {1, {}}};
  CHECK(b::QueueActorValueLabel(kActor, b::kNoLimbTag, 5, 0, world) ==
        b::kLabelBoneMissing);
  CHECK(world.queue.count == 0);
}

TEST_CASE("On-screen checks accept the last pixel and refuse beyond it",
          "[battle][floating-label-producers]") {
  fixture::World world;
  world.projections = {{0, {320, 240, 0, 0}},
                       {0, {0, 241, 0, 0}},
                       {0, {-1, 0, 0, 0}},
                       {1, {0, 0, 0, 0}}};
  CHECK(b::QueueActorStatusLabel(kActor, 7, 1, 0x80, world) == 0);
  CHECK(world.queue.count == 1);
  CHECK(b::QueueActorStatusLabel(kActor, 7, 1, 0x80, world) == 0);
  CHECK(b::QueueActorStatusLabel(kActor, 7, 1, 0x80, world) == 0);
  CHECK(b::QueueActorValueLabel(kActor, b::kNoLimbTag, 1, 0x80, world) == 0);
  CHECK(world.queue.count == 1);
  CHECK(world.Record(0).limb == 7);
}

TEST_CASE("Status labels follow the mask from the lowest bit",
          "[battle][floating-label-producers]") {
  fixture::World world;
  constexpr std::uint32_t kMask =
      (1U << 0) | (1U << 3) | (1U << 28) | (1U << 29) | (1U << 31);
  CHECK(b::QueueActorStatusLabels(kActor, static_cast<std::int32_t>(kMask),
                                  0x85, world) == 0);
  REQUIRE(world.queue.count == 3);
  const std::array<std::uint16_t, 3> values{0, 3, 28};
  for (std::size_t index = 0; index < values.size(); ++index) {
    const auto record = world.Record(index);
    CHECK(record.kind == kStatusKind);
    CHECK(record.value == values.at(index));
    CHECK(record.position.w == static_cast<std::int16_t>(index));
    CHECK(record.limb == b::kLabelAnchorTag);
    CHECK(record.color_flags == 0x05);
  }
  CHECK(world.Record(1).text_length == 12);
  CHECK(world.Record(2).text_length == 24);
}

TEST_CASE("Status label restarts its frame lead past the limit",
          "[battle][floating-label-producers]") {
  fixture::World world;
  auto earlier = Label(kStatusKind, kActor, -1);
  earlier.text_length = 25;
  earlier.frame = 6;
  world.SetRecord(0, earlier);
  auto shown = Label(kStatusKind, kActor, 0);
  shown.text_length = 24;
  shown.frame = 8;
  world.SetRecord(1, shown);
  world.queue.count = 2;
  CHECK(b::QueueActorStatusLabel(kActor, 9, 2, 0, world) == 0);
  CHECK(world.Record(2).text_length == 36);
  CHECK(world.Record(2).frame == 8);
  CHECK(world.Record(2).position.w == 1);
  CHECK(b::QueueActorStatusLabel(kActor, 9, 2, 0, world) == 0);
  CHECK(world.Record(3).text_length == 0);
  CHECK(world.Record(3).position.w == 2);
}

// Chain 1500, then timing 3 and 4, over a 30-label queue whose slot 4 holds
// the shared chain label and slot 7 the shared timing label. Returns the
// labels before the producers ran.
std::array<live::LabelWords, 32> ReplaceSharedLabels(live::State& state) {
  state.Populate();
  VS::Battle::floatingLabelCount = 30;
  state.labels[0][4].key.bytes[0] = static_cast<char>(kChainKind);
  state.labels[0][4].key.bytes[1] = 0;
  state.labels[0][7].key.bytes[0] = static_cast<char>(kTimingKind);
  state.labels[0][7].key.bytes[1] = 0;
  const auto before = live::Snapshot(state);
  VS::Core::SeedBiosRand(77);
  CHECK(b::LiveQueueChainCount(1500) == 0);
  CHECK(b::LiveQueueAbilityTimingResult(3) == 0);
  CHECK(b::LiveQueueAbilityTimingResult(4) == 0);
  CHECK(VS::Battle::floatingLabelCount == 30);
  return before;
}

TEST_CASE("Live chain and timing removals shift the later labels down",
          "[battle][floating-label-producers]") {
  live::State state;
  const auto before = ReplaceSharedLabels(state);
  const auto after = live::Snapshot(state);
  CHECK(std::equal(after.begin(), after.begin() + 4, before.begin()));
  CHECK(std::equal(after.begin() + 4, after.begin() + 6, before.begin() + 5));
  CHECK(std::equal(after.begin() + 6, after.begin() + 28, before.begin() + 8));
  CHECK(std::equal(after.begin() + 30, after.end(), before.begin() + 30));
}

TEST_CASE("Live chain and timing producers publish into the cleared tail",
          "[battle][floating-label-producers]") {
  live::State state;
  static_cast<void>(ReplaceSharedLabels(state));
  // Each removal clears the vacated tail record, which the replacement then
  // reserves, so only the producer's own fields are set.
  VS::Battle::FloatingLabel chain{};
  chain.key.bytes[0] = static_cast<char>(kChainKind);
  chain.value = b::kMaxLabelValue;
  CHECK(live::Words(state.labels[0][28]) == live::Words(chain));
  const auto& timing_label = state.labels[0][29];
  CHECK(timing_label.text_length <= b::kTimingJitterMask);
  VS::Battle::FloatingLabel timing{};
  timing.key.bytes[0] = static_cast<char>(kTimingKind);
  timing.value = 4;
  timing.text_length = timing_label.text_length;
  CHECK(live::Words(timing_label) == live::Words(timing));
}

// A value label for actor 1, its status labels 0 and 4, then status label 6,
// over a queue of two empty labels.
void QueueActorLabels() {
  VS::Battle::floatingLabelCount = 2;
  CHECK(b::LiveQueueActorValueLabel(1, -1, 0x80000505U, 3) == 0);
  CHECK(b::LiveQueueActorStatusLabels(1, 0, 0x11, 2) == 0);
  CHECK(b::LiveQueueActorStatusLabel(1, 251, 6, 1) == 0);
  CHECK(VS::Battle::floatingLabelCount == 6);
}

TEST_CASE("Live actor value producer clamps and prints its amount",
          "[battle][floating-label-producers]") {
  live::State state;
  live::Producers producers;
  QueueActorLabels();
  const auto& value = state.labels[0][2];
  VS::Battle::FloatingLabel expected{};
  // The bone-relative position is covered by the F00714 differential.
  expected.position = value.position;
  expected.position.pad = 0;
  expected.key.bytes[0] = static_cast<char>(kValueKind);
  expected.key.bytes[1] = 1;
  expected.limb = static_cast<std::uint8_t>(b::kTargetPointSelector);
  expected.value =
      static_cast<std::uint16_t>(b::kMaxLabelValue | b::kLabelNegativeBit);
  expected.color_flags = 3;
  expected.text_length = 3;
  expected.scale_x = b::kLabelUnitScale;
  expected.scale_y = b::kLabelUnitScale;
  constexpr std::array<signed char, 4> kText{'9', '9', '9', 0};
  std::ranges::copy(kText, std::begin(expected.text));
  CHECK(live::Words(value) == live::Words(expected));
}

struct StatusRow {
  std::int16_t order;
  std::uint8_t follow;
  std::uint8_t color;
  std::uint16_t status;
};

VS::Battle::FloatingLabel StatusLabel(const StatusRow& row) {
  VS::Battle::FloatingLabel label{};
  label.position.pad = row.order;
  label.key.bytes[0] = static_cast<char>(kStatusKind);
  label.key.bytes[1] = 1;
  label.limb = static_cast<std::uint8_t>(b::kLabelAnchorTag);
  label.color_flags = row.color;
  label.text_length = row.follow;
  label.value = row.status;
  return label;
}

TEST_CASE("Live actor status producers stack after the previous label",
          "[battle][floating-label-producers]") {
  live::State state;
  live::Producers producers;
  QueueActorLabels();
  // Each status label follows the actor's previous one by 12 frames.
  CHECK(live::Words(state.labels[0][3]) ==
        live::Words(
            StatusLabel({.order = 0, .follow = 0, .color = 2, .status = 0})));
  CHECK(live::Words(state.labels[0][4]) ==
        live::Words(
            StatusLabel({.order = 1, .follow = 12, .color = 2, .status = 4})));
  CHECK(live::Words(state.labels[0][5]) ==
        live::Words(
            StatusLabel({.order = 2, .follow = 24, .color = 1, .status = 6})));
}
}  // namespace
