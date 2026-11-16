#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/FloatingLabelPlacement.hpp"
#include "Battle/FloatingLabelProducers.hpp"
#include "Battle/FloatingLabelRendering.hpp"

namespace {
namespace b = VS::Battle;
namespace r = VS::Render;
constexpr b::FloatingLabelScreenPoint kOrigin{100, 100};
constexpr std::size_t kCapacity = 32;
constexpr std::uint8_t kActorValue = 1;
constexpr std::uint8_t kActorStatus = 2;
constexpr std::uint8_t kWorldLocked = 3;
constexpr std::uint8_t kTiming = 4;
constexpr std::uint8_t kEquipment = 6;
constexpr std::uint8_t kPendingEquipment = 7;
constexpr std::int16_t kUnit = 0x1000;
constexpr std::uint16_t kTextPage = 0x0C;
constexpr std::uint16_t kDigitClut = 0x37F4;
constexpr std::uint16_t kWordClut = 0x37F5;

std::array<std::uint8_t, b::kLabelTextBytes> Text(std::string_view digits) {
  std::array<std::uint8_t, b::kLabelTextBytes> text{};
  std::ranges::copy(digits, text.begin());
  return text;
}

b::FloatingLabelRecord Digits(std::string_view digits, std::uint16_t value) {
  return {.value = value,
          .text_length = static_cast<std::uint8_t>(digits.size()),
          .text = Text(digits)};
}

class Records : public b::EquipmentStatChangeQueue {
 public:
  std::array<b::FloatingLabelRecord, kCapacity> labels{};
  std::int16_t count = 0;
  bool refuse_reserve = false;
  std::int16_t LabelCount() override { return count; }
  std::optional<b::FloatingLabelRecord> Label(std::int32_t index) override {
    if (index < 0 || static_cast<std::size_t>(index) >= kCapacity) {
      return std::nullopt;
    }
    return labels[static_cast<std::size_t>(index)];
  }
  bool WriteLabel(std::int32_t index,
                  const b::FloatingLabelRecord& record) override {
    if (index < 0 || static_cast<std::size_t>(index) >= kCapacity) return false;
    labels[static_cast<std::size_t>(index)] = record;
    return true;
  }
  std::optional<std::int32_t> ReserveLabel() override {
    if (refuse_reserve) return std::nullopt;
    return count;
  }
  bool CommitLabel(std::int32_t index) override {
    if (index != count) return false;
    ++count;
    return true;
  }
};

class PlacedTable final : public b::PlacedLabelPositions {
 public:
  std::array<b::LabelScreenPoint, kCapacity> points{};
  std::int16_t count = 0;
  std::int16_t Count() override { return count; }
  void SetCount(std::int16_t value) override { count = value; }
  std::optional<b::LabelScreenPoint> Position(std::int32_t index) override {
    if (index < 0 || static_cast<std::size_t>(index) >= kCapacity) {
      return std::nullopt;
    }
    return points[static_cast<std::size_t>(index)];
  }
  bool SetPosition(std::int32_t index, b::LabelScreenPoint point) override {
    if (index < 0 || static_cast<std::size_t>(index) >= kCapacity) return false;
    points[static_cast<std::size_t>(index)] = point;
    return true;
  }
};

class Feedback final : public b::FloatingLabelFeedbackWorld {
 public:
  static constexpr std::array<b::FloatingLabelAtlasRect, 2> kCaptions{
      {{0, 64, 36, 12}, {0, 76, 24, 12}}};
  static constexpr std::array<r::QuadColor, 2> kColors{
      {{208, 208, 208}, {64, 192, 255}}};
  static constexpr b::FloatingLabelAtlasRect kFirst{76, 168, 28, 8};
  static constexpr b::FloatingLabelAtlasRect kSecond{104, 176, 20, 8};
  static constexpr std::uint8_t kFirstWords = 7;
  static constexpr std::uint8_t kSecondWords = 13;
  b::StatusLabelLayout layout{2, {0, 1}, 1};
  std::vector<b::FloatingLabelQuad> quads;
  std::vector<b::FloatingLabelQuad> equipment;
  std::optional<b::FloatingLabelAtlasRect> ValueCaption(std::uint8_t) override {
    return b::FloatingLabelAtlasRect{150, 64, 26, 12};
  }
  std::optional<std::int16_t> LockedHalfHeight(std::int32_t) override {
    return std::nullopt;
  }
  std::optional<b::FloatingLabelAtlasRect> LockedPiece(std::size_t) override {
    return std::nullopt;
  }
  bool DrawQuad(const b::FloatingLabelQuad& quad) override {
    quads.push_back(quad);
    return true;
  }
  bool DrawText(std::string_view, b::FloatingLabelScreenPoint,
                std::uint32_t) override {
    return false;
  }
  std::optional<b::StatusLabelLayout> StatusLayout(
      std::uint16_t status) override {
    if (status != 0) return std::nullopt;
    return layout;
  }
  std::optional<b::FloatingLabelAtlasRect> CaptionRect(
      std::uint8_t caption) override {
    if (caption >= kCaptions.size()) return std::nullopt;
    return kCaptions[caption];
  }
  std::optional<r::QuadColor> StatusColor(std::uint8_t color) override {
    if (color >= kColors.size()) return std::nullopt;
    return kColors[color];
  }
  std::optional<b::FloatingLabelAtlasRect> StatChangeFirstWord(
      std::uint8_t word) override {
    if (word >= kFirstWords) return std::nullopt;
    return kFirst;
  }
  std::optional<b::FloatingLabelAtlasRect> StatChangeSecondWord(
      std::uint8_t word) override {
    if (word >= kSecondWords) return std::nullopt;
    return kSecond;
  }
  bool DrawEquipmentQuad(const b::FloatingLabelQuad& quad) override {
    equipment.push_back(quad);
    return true;
  }
};

struct LeafDraw {
  b::FloatingLabelLeaf leaf{};
  std::int32_t index = 0;
  b::FloatingLabelScreenPoint origin{};
  b::FloatingLabelRecord record{};
};

// Projects a point onto its own x and y; a non-positive z is behind the
// camera. Missing bones are actors without an entry.
class Update final : public b::FloatingLabelUpdateWorld {
 public:
  Records records;
  PlacedTable placed;
  std::map<std::int32_t, b::LabelPoint> bones;
  std::vector<LeafDraw> draws;
  std::vector<std::int32_t> compactions;
  std::int32_t cues = 0;
  std::int16_t LabelCount() override { return records.count; }
  std::optional<b::FloatingLabelRecord> Label(std::int32_t index) override {
    return records.Label(index);
  }
  bool WriteLabel(std::int32_t index,
                  const b::FloatingLabelRecord& record) override {
    return records.WriteLabel(index, record);
  }
  b::PlacedLabelPositions& Placed() override { return placed; }
  std::int32_t TaggedBonePosition(std::int32_t actor, std::int32_t tag,
                                  std::int32_t endpoint,
                                  b::LabelPoint& point) override {
    const auto found = bones.find(actor);
    if (tag != b::kLabelAnchorTag || endpoint != 0 || found == bones.end()) {
      return -1;
    }
    point = found->second;
    return 0;
  }
  std::int32_t Project(const b::LabelPoint& point,
                       b::LabelScreenPoint& screen) override {
    screen = {point.x, point.y};
    return point.z > 0 ? 1 : 0;
  }
  bool DrawAbilityTimingCue() override {
    ++cues;
    return true;
  }
  bool DrawLeaf(b::FloatingLabelLeaf leaf, std::int32_t index,
                b::FloatingLabelScreenPoint origin) override {
    draws.push_back(
        {leaf, index, origin, records.labels[static_cast<std::size_t>(index)]});
    return true;
  }
  bool CompactLabels(std::int32_t removal_count) override {
    compactions.push_back(removal_count);
    return true;
  }
};

struct Timers {
  std::int16_t cue = 0;
  std::int16_t step = 0;
  b::FloatingLabelUpdateState State(std::int32_t tick, bool paused = false,
                                    bool hidden = false) {
    return {cue, step, tick, paused, hidden};
  }
};

b::FloatingLabelRecord Label(std::uint8_t kind, std::uint8_t actor,
                             b::LabelPoint position, std::uint8_t frame) {
  return {.position = position, .kind = kind, .actor = actor, .frame = frame};
}

void CheckQuad(const b::FloatingLabelQuad& quad, r::ScreenVertex top_left,
               r::ScreenVertex bottom_right, std::uint8_t u, std::uint8_t v) {
  CHECK(quad.top_left == top_left);
  CHECK(quad.bottom_right == bottom_right);
  CHECK(quad.texture.u == u);
  CHECK(quad.texture.v == v);
  CHECK_FALSE(quad.raw_texture);
}

constexpr b::LabelScreenPoint kShown{105, 48};

void CheckValueLabelsPlaced(const Update& world) {
  constexpr b::LabelScreenPoint kBeside{129, 48};
  REQUIRE(world.draws.size() == 1);
  const auto& draw = world.draws.front();
  CHECK(draw.leaf == b::FloatingLabelLeaf::kActorValue);
  CHECK(draw.origin.x == kShown.x);
  CHECK(draw.origin.y == kShown.y);
  CHECK(draw.record.color_flags == 1);
  CHECK(world.placed.count == 2);
  CHECK(world.placed.points[0] == kShown);
  CHECK(world.placed.points[1] == kBeside);
}

void CheckValueLabelsAged(const Update& world) {
  constexpr std::int16_t kBesideOffset = 24;
  constexpr std::uint8_t kSettled = 10;
  const auto& shown = world.records.labels[0];
  CHECK(shown.frame == kSettled);
  CHECK(shown.position.w == -1);
  const auto& waiting = world.records.labels[1];
  CHECK(waiting.scale_x == kBesideOffset);
  CHECK(waiting.scale_y == 0);
  CHECK(waiting.color_flags == 0);
  CHECK(waiting.frame == 0);
  CHECK(waiting.position.w == 1);
}
void CheckTimingDrawn(const Update& world) {
  CHECK(world.cues == 1);
  REQUIRE(world.draws.size() == 1);
  CHECK(world.draws.front().leaf == b::FloatingLabelLeaf::kAbilityTimingResult);
  CHECK(world.draws.front().origin.x == 44);
  CHECK(world.draws.front().origin.y == 48);
}

void CheckEquipmentStepped(const Update& world, std::uint8_t last) {
  constexpr std::int16_t kLift = 10;
  CHECK(world.records.labels[0].position.w == 2);
  CHECK(world.records.labels[0].scale_y == kLift);
  CHECK(world.records.labels[1].position.w == -1);
  REQUIRE(world.draws.size() == 1);
  CHECK(world.draws.front().leaf == b::FloatingLabelLeaf::kEquipmentStatChange);
  CHECK(world.draws.front().origin.y == 172);
  CHECK(world.records.labels[1].frame == last);
}

void CheckBouncedDigits(const Feedback& world) {
  REQUIRE(world.quads.size() == 2);
  CheckQuad(world.quads[0], {100, 82}, {108, 94}, 104, 0);
  CheckQuad(world.quads[1], {94, 88}, {102, 100}, 80, 0);
  CHECK(world.quads[0].tint == r::QuadColor{0, 128, 0});
  CHECK(world.quads[0].texture.texture_page == kTextPage);
  CHECK(world.quads[0].texture.clut == kDigitClut);
  CHECK_FALSE(world.quads[0].translucent);
}

void CheckOpeningStatus(const Feedback& world) {
  constexpr std::uint16_t kFadingPage = 0x37;
  REQUIRE(world.quads.size() == 2);
  CheckQuad(world.quads[0], {76, 94}, {112, 106}, 0, 64);
  CheckQuad(world.quads[1], {112, 94}, {136, 106}, 0, 76);
  CHECK(world.quads[0].tint == r::QuadColor{26, 78, 103});
  CHECK(world.quads[0].translucent);
  CHECK(world.quads[0].texture.texture_page == kFadingPage);
}

b::FloatingLabelQuad FirstStatusQuad(std::uint8_t frame) {
  Feedback world;
  CHECK(b::DrawActorStatusLabel(world, kOrigin, {.frame = frame}));
  REQUIRE_FALSE(world.quads.empty());
  return world.quads.front();
}

void CheckStatusRefusals(Feedback& world) {
  constexpr std::uint8_t kPastEnd = 31;
  constexpr std::uint8_t kTooMany = 3;
  CHECK_FALSE(b::DrawActorStatusLabel(world, kOrigin, {.frame = kPastEnd}));
  world.layout.count = kTooMany;
  CHECK_FALSE(b::DrawActorStatusLabel(world, kOrigin, {}));
  CHECK_FALSE(b::DrawActorStatusLabel(world, kOrigin, {.value = 1}));
  CHECK(world.quads.empty());
}

void CheckQueuedLoss(const b::FloatingLabelRecord& label) {
  constexpr std::uint8_t kArmorWord = 4;
  constexpr std::uint16_t kShownLoss = 0x8000 | 999;
  CHECK(label.kind == kPendingEquipment);
  CHECK(label.actor == b::kEquipmentStatChangeActor);
  CHECK(label.position == b::LabelPoint{7, 8, 9, 2});
  CHECK(label.scale_x == kUnit);
  CHECK(label.color_flags == kArmorWord);
  CHECK(label.limb == 0xFF);
  CHECK(label.value == kShownLoss);
  CHECK(label.text == Text("999"));
  CHECK(label.text_length == 3);
}
}  // namespace

TEST_CASE("F00719 anchors places draws and ages actor value labels",
          "[floating-placement][F00719][F00720]") {
  constexpr std::uint8_t kActor = 3;
  constexpr std::uint8_t kShownMode = 0x81;
  constexpr std::uint8_t kRepeatedLimb = 0x80;
  constexpr std::int32_t kTick = 3;
  constexpr b::LabelPoint kOffset{5, -4, 10, 0};
  Update world;
  world.bones[kActor] = {100, 20, 1, 0};
  auto shown = Label(kActorValue, kActor, kOffset, 9);
  shown.color_flags = kShownMode;
  shown.scale_x = kUnit;
  shown.scale_y = kUnit;
  auto waiting = shown;
  waiting.position.w = 2;
  waiting.color_flags = kRepeatedLimb;
  world.records.labels[0] = shown;
  world.records.labels[1] = waiting;
  world.records.count = 2;
  Timers timers;
  CHECK(b::UpdateAndDrawFloatingLabels(world, timers.State(kTick)));
  CheckValueLabelsPlaced(world);
  CheckValueLabelsAged(world);
  CHECK(world.compactions.empty());
  CHECK(timers.step == 1);
}

TEST_CASE("F00719 removes hidden labels and compacts once",
          "[floating-placement][F00719]") {
  constexpr std::uint8_t kMissingActor = 4;
  constexpr std::uint8_t kTimingLast = 45;
  constexpr std::int32_t kRemoved = 3;
  Update world;
  world.records.labels[0] = Label(kActorStatus, kMissingActor, {}, 0);
  world.records.labels[1] = Label(kWorldLocked, 0, {1, 2, 0, 0}, 0);
  world.records.labels[2] = Label(kTiming, 0, {}, kTimingLast);
  world.records.count = 3;
  Timers timers{.cue = 2};
  CHECK(b::UpdateAndDrawFloatingLabels(world, timers.State(2)));
  CHECK(timers.cue == 1);
  CheckTimingDrawn(world);
  CHECK(world.compactions == std::vector<std::int32_t>{kRemoved});
  for (const auto& label : std::span(world.records.labels).first(3)) {
    CHECK(label.kind == 0);
  }
}

TEST_CASE("F00719 hides Ashley status labels but keeps aging them",
          "[floating-placement][F00719]") {
  constexpr std::uint8_t kLength = 6;
  constexpr std::uint8_t kLockedMode = 2;
  Update world;
  world.bones[0] = {50, -10, 1, 0};
  world.bones[1] = {60, -10, 1, 0};
  auto ashley = Label(kActorStatus, 0, {}, 0);
  ashley.text_length = kLength;
  auto enemy = ashley;
  enemy.actor = 1;
  auto locked = enemy;
  locked.color_flags = kLockedMode;
  world.records.labels = {ashley, enemy, locked};
  world.records.count = 3;
  Timers timers;
  CHECK(b::UpdateAndDrawFloatingLabels(world, timers.State(2, false, true)));
  REQUIRE(world.draws.size() == 2);
  CHECK(world.draws[0].leaf == b::FloatingLabelLeaf::kActorStatus);
  CHECK(world.draws[0].origin.x == 60);
  CHECK(world.draws[0].origin.y == 40 - kLength);
  CHECK(world.draws[1].leaf == b::FloatingLabelLeaf::kLocked);
  CHECK(world.records.labels[0].frame == 1);
  CHECK(world.placed.count == 0);
}

TEST_CASE("F00719 steps and lifts the equipment stack on the status timer",
          "[floating-placement][F00719]") {
  constexpr std::uint8_t kLast = 60;
  constexpr std::int16_t kDue = 21;
  Update world;
  world.records.labels[0] = Label(kEquipment, 0, {0, 0, 0, 3}, 0);
  world.records.labels[1] = Label(kEquipment, 0, {0, 0, 0, 0}, kLast);
  world.records.labels[2] = Label(kActorValue, 0, {0, 0, 0, 0}, 0);
  world.records.labels[2].kind = 0;
  world.records.count = 3;
  Timers timers{.step = kDue};
  CHECK(b::UpdateAndDrawFloatingLabels(world, timers.State(2, true)));
  CHECK(timers.step == 0);
  CheckEquipmentStepped(world, kLast);
  CHECK(b::UpdateAndDrawFloatingLabels(world, timers.State(2)));
  CHECK(world.records.labels[1].kind == 0);
  CHECK(world.compactions == std::vector<std::int32_t>{1});
}

TEST_CASE("F00720 searches downward and outward for a free spot",
          "[floating-placement][F00720]") {
  constexpr b::LabelScreenPoint kAnchor{100, 80};
  constexpr std::int16_t kSpread = 16;
  constexpr b::LabelScreenPoint kLower{100, 96};
  PlacedTable placed;
  placed.points[0] = {static_cast<std::int16_t>(kAnchor.x + kSpread),
                      kAnchor.y};
  placed.count = 1;
  b::FloatingLabelRecord label{.scale_x = kUnit, .scale_y = kUnit};
  auto position = kAnchor;
  CHECK(b::PlaceActorValueLabel(position, label, placed));
  CHECK(label.scale_x == -8);
  CHECK(position.x == kAnchor.x - 8);
  placed.points[1] = kAnchor;
  placed.points[2] = {static_cast<std::int16_t>(kAnchor.x - kSpread),
                      kAnchor.y};
  placed.count = 3;
  label = {.scale_x = kUnit, .scale_y = kUnit};
  position = kAnchor;
  CHECK(b::PlaceActorValueLabel(position, label, placed));
  CHECK(label.scale_x == 0);
  CHECK(label.scale_y == kSpread);
  CHECK(placed.points[3] == kLower);
  CHECK(placed.count == 4);
}

TEST_CASE("F00720 keeps an unplaceable label and clamps after recording",
          "[floating-placement][F00720]") {
  constexpr b::LabelScreenPoint kAnchor{10, 250};
  constexpr std::array<std::int16_t, 3> kColumns{-16, 0, 16};
  constexpr std::int32_t kRows = 6;
  constexpr std::int16_t kRowStep = 8;
  PlacedTable placed;
  for (std::int32_t row = 0; row < kRows; ++row) {
    for (const auto column : kColumns) {
      placed.points[static_cast<std::size_t>(placed.count++)] = {
          static_cast<std::int16_t>(kAnchor.x + column),
          static_cast<std::int16_t>(kAnchor.y + row * kRowStep)};
    }
  }
  const auto filled = placed.count;
  b::FloatingLabelRecord label{.scale_x = kUnit, .scale_y = kUnit};
  auto position = kAnchor;
  CHECK(b::PlaceActorValueLabel(position, label, placed));
  CHECK(label.scale_x == 0);
  CHECK(label.scale_y == 0);
  CHECK(placed.points[static_cast<std::size_t>(filled)] == kAnchor);
  CHECK(position == b::LabelScreenPoint{32, 200});
  placed.count = static_cast<std::int16_t>(kCapacity);
  CHECK_FALSE(b::PlaceActorValueLabel(position, label, placed));
  placed.count = -1;
  CHECK_FALSE(b::PlaceActorValueLabel(position, label, placed));
}

TEST_CASE("F00722 bounces digits right to left with halving offsets",
          "[floating-placement][F00722]") {
  constexpr std::uint8_t kFrame = 2;
  Feedback world;
  auto label = Digits("25", 25);
  label.frame = kFrame;
  CHECK(b::DrawActorValueLabel(world, kOrigin, label));
  CheckBouncedDigits(world);
  world.quads.clear();
  label.value = 0x8000 | 25;
  label.color_flags = 1;
  CHECK(b::DrawActorValueLabel(world, kOrigin, label));
  REQUIRE(world.quads.size() == 2);
  CHECK(world.quads[0].top_left.y == 89);
  CHECK(world.quads[0].tint == r::QuadColor{128, 0, 128});
}

TEST_CASE("F00722 clips digits and lays captioned values out",
          "[floating-placement][F00722]") {
  constexpr std::uint8_t kCaptionMode = 3;
  constexpr std::uint8_t kFrame = 4;
  constexpr b::FloatingLabelScreenPoint kEdge{320, 100};
  Feedback world;
  auto label = Digits("12", 12);
  CHECK(b::DrawActorValueLabel(world, kEdge, label));
  REQUIRE(world.quads.size() == 1);
  CHECK(world.quads[0].top_left.x == kEdge.x - 6);
  world.quads.clear();
  label.color_flags = kCaptionMode;
  label.frame = kFrame;
  CHECK(b::DrawActorValueLabel(world, kOrigin, label));
  REQUIRE(world.quads.size() == 4);
  CHECK(world.quads[2].top_left.y == 92);
  CHECK(world.quads[2].tint == r::QuadColor{128, 128, 128});
}

TEST_CASE("F00722 refuses overlong digit text",
          "[floating-placement][F00722]") {
  Feedback world;
  auto label = Digits("12", 12);
  label.text_length = b::kLabelTextBytes + 1;
  CHECK_FALSE(b::DrawActorValueLabel(world, kOrigin, label));
  CHECK(world.quads.empty());
}

TEST_CASE("F00725 slides fades and centres status captions",
          "[floating-placement][F00725]") {
  constexpr std::uint8_t kOpening = 2;
  constexpr std::uint8_t kHeld = 10;
  constexpr std::uint8_t kBright = 5;
  constexpr std::uint8_t kClosing = 28;
  constexpr std::uint16_t kHeldPage = 0x17;
  Feedback world;
  CHECK(b::DrawActorStatusLabel(world, kOrigin, {.frame = kOpening}));
  CheckOpeningStatus(world);
  const auto held = FirstStatusQuad(kHeld);
  CHECK(held.top_left.x == 70);
  CHECK(held.tint == r::QuadColor{32, 96, 127});
  CHECK(held.texture.texture_page == kHeldPage);
  CHECK(FirstStatusQuad(kBright).tint == r::QuadColor{128, 192, 223});
  CHECK(FirstStatusQuad(kClosing).top_left.x == 64);
}

TEST_CASE("F00725 marks removals and refuses before drawing",
          "[floating-placement][F00725]") {
  constexpr std::uint8_t kRemoval = 1;
  constexpr std::uint8_t kMarked = 16;
  Feedback world;
  CHECK(b::DrawActorStatusLabel(world, kOrigin,
                                {.frame = kMarked, .color_flags = kRemoval}));
  Feedback marker;
  CHECK(b::DrawStatusRemovalMarker(marker, kOrigin, 1));
  REQUIRE(world.quads.size() == 3);
  CHECK(world.quads[0].top_left == marker.quads[0].top_left);
  CHECK(world.quads[1].tint == r::QuadColor{104, 104, 104});
  world.quads.clear();
  CheckStatusRefusals(world);
  world.layout.count = 0;
  CHECK(b::DrawActorStatusLabel(world, kOrigin, {}));
  CHECK(world.quads.empty());
}

TEST_CASE("F00738 queues a pending stat change behind the stack",
          "[floating-placement][F00738]") {
  constexpr std::int32_t kArmor = 1;
  constexpr std::int32_t kStat = 0x1FF;
  constexpr std::uint32_t kLoss = 0x80000000U | 1234U;
  constexpr b::LabelPoint kKept{7, 8, 9, 0};
  Records queue;
  queue.labels[0].kind = kEquipment;
  queue.labels[1].kind = kPendingEquipment;
  queue.labels[2].kind = kActorValue;
  queue.labels[3].position = kKept;
  queue.labels[3].scale_x = kUnit;
  queue.labels[3].text = Text("ZZZZ");
  queue.count = 3;
  CHECK(b::QueueEquipmentStatChange(kArmor, kStat, kLoss, {false, true},
                                    queue) == 0);
  CHECK(queue.count == 4);
  CheckQueuedLoss(queue.labels[3]);
}

TEST_CASE("F00738 honours the stat-change settings and maps equipment",
          "[floating-placement][F00738]") {
  constexpr std::array<std::pair<std::int32_t, std::uint8_t>, 3> kMapped{
      {{241, 1}, {6, 5}, {5, 5}}};
  Records queue;
  queue.labels[0].text = Text("ZZZZ");
  CHECK(b::QueueEquipmentStatChange(b::kWeaponStatChange, 0, 42, {false, true},
                                    queue) == 0);
  CHECK(b::QueueEquipmentStatChange(1, 0, 42, {true, false}, queue) == 0);
  CHECK(queue.count == 0);
  CHECK(b::QueueEquipmentStatChange(b::kWeaponStatChange, 0, 42, {true, false},
                                    queue) == 0);
  CHECK(queue.labels[0].color_flags == 0);
  CHECK(queue.labels[0].text == std::array<std::uint8_t, 4>{'4', '2', 0, 'Z'});
  for (const auto& [kind, word] : kMapped) {
    CHECK(b::QueueEquipmentStatChange(kind, 0, 1, {false, true}, queue) == 0);
    CHECK(queue.labels[static_cast<std::size_t>(queue.count - 1)].color_flags ==
          word);
  }
  queue.refuse_reserve = true;
  CHECK_FALSE(
      b::QueueEquipmentStatChange(1, 0, 1, {false, true}, queue).has_value());
}

TEST_CASE("F00739 draws both words the sign and the digits",
          "[floating-placement][F00739]") {
  constexpr std::int16_t kStack = 10;
  constexpr b::FloatingLabelScreenPoint kStackOrigin{8, 172};
  Feedback world;
  auto label = Digits("5", 5);
  label.color_flags = 1;
  label.limb = 2;
  label.scale_y = kStack;
  CHECK(b::DrawEquipmentStatChange(world, kStackOrigin, label));
  REQUIRE(world.equipment.size() == 4);
  CHECK(world.quads.empty());
  CheckQuad(world.equipment[0], {8, 162}, {36, 170}, 76, 168);
  CheckQuad(world.equipment[1], {38, 162}, {58, 170}, 104, 176);
  CheckQuad(world.equipment[2], {60, 161}, {68, 169}, 128, 208);
  CheckQuad(world.equipment[3], {67, 161}, {73, 171}, 30, 0);
  CHECK(world.equipment[0].tint == r::QuadColor{0, 64, 128});
  CHECK(world.equipment[2].texture.clut == kWordClut);
  CHECK(world.equipment[3].texture.clut == kDigitClut);
  CHECK(world.equipment[3].texture.height == 10);
}

TEST_CASE("F00739 colours losses and refuses foreign captions",
          "[floating-placement][F00739]") {
  constexpr std::uint8_t kFirstPastEnd = 7;
  constexpr std::uint8_t kSecondPastEnd = 13;
  Feedback world;
  auto label = Digits("5", 0x8005);
  CHECK(b::DrawEquipmentStatChange(world, kOrigin, label));
  REQUIRE(world.equipment.size() == 4);
  CHECK(world.equipment[2].texture.u == 120);
  CHECK(world.equipment[2].tint == r::QuadColor{128, 64, 0});
  world.equipment.clear();
  label.color_flags = kFirstPastEnd;
  CHECK_FALSE(b::DrawEquipmentStatChange(world, kOrigin, label));
  label.color_flags = 0;
  label.limb = kSecondPastEnd;
  CHECK_FALSE(b::DrawEquipmentStatChange(world, kOrigin, label));
  label.limb = 0;
  label.text_length = b::kLabelTextBytes + 1;
  CHECK_FALSE(b::DrawEquipmentStatChange(world, kOrigin, label));
  CHECK(world.equipment.empty());
}

TEST_CASE("F00742 publishes pending stat changes inside the count",
          "[floating-placement][F00742]") {
  Records records;
  records.labels[0].kind = kPendingEquipment;
  records.labels[1].kind = kEquipment;
  records.labels[2].kind = kActorValue;
  records.labels[3].kind = kPendingEquipment;
  records.count = 3;
  CHECK(b::CommitEquipmentStatChanges(records));
  CHECK(records.labels[0].kind == kEquipment);
  CHECK(records.labels[1].kind == kEquipment);
  CHECK(records.labels[2].kind == kActorValue);
  CHECK(records.labels[3].kind == kPendingEquipment);
}

TEST_CASE("F00742 refuses an overcount after publishing earlier records",
          "[floating-placement][F00742]") {
  constexpr std::size_t kLast = kCapacity - 1;
  Records records;
  records.labels[0].kind = kPendingEquipment;
  CHECK(b::CommitEquipmentStatChanges(records));
  CHECK(records.labels[0].kind == kPendingEquipment);
  records.labels[kLast].kind = kPendingEquipment;
  records.count = static_cast<std::int16_t>(kCapacity + 1);
  CHECK_FALSE(b::CommitEquipmentStatChanges(records));
  CHECK(records.labels[0].kind == kEquipment);
  CHECK(records.labels[kLast].kind == kEquipment);
}
