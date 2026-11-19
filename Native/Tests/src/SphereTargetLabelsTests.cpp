#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Sphere.h"
#include "Battle/SphereTargetLabels.hpp"
#include "Battle/SphereTargetLabelsLive.hpp"
#include "Battle/TargetVolume.h"
#include "Battle/Targeting.hpp"
#include "Menu/Framework/Menus.hpp"

namespace {
namespace battle = VS::Battle;

using battle::LabelScreenPoint;
using battle::SphereTargetLabel;
using battle::TargetLabelLinkLine;

enum class CanvasCall : std::uint8_t {
  kLine,
  kDrawMode,
  kGradient,
  kText,
  kBox,
  kBeginProjection,
  kProject,
};

struct GradientRequest {
  int gradient = 0;
  int color_index = 0;
  int brightness = 0;

  friend bool operator==(const GradientRequest&,
                         const GradientRequest&) = default;
};

class RecordingLabelCanvas final : public battle::SphereTargetLabelCanvas {
 public:
  void DrawLinkLine(const TargetLabelLinkLine& line) override {
    calls.push_back(CanvasCall::kLine);
    lines.push_back(line);
  }
  void SetLinkDrawMode(std::uint32_t draw_mode) override {
    calls.push_back(CanvasCall::kDrawMode);
    draw_modes.push_back(draw_mode);
  }
  std::uint32_t GradientStopColor(int gradient, int color_index,
                                  int brightness) override {
    calls.push_back(CanvasCall::kGradient);
    gradients.push_back({gradient, color_index, brightness});
    return GradientColor(gradient, color_index);
  }
  void DrawText(const battle::TargetLabelTextDraw& text) override {
    calls.push_back(CanvasCall::kText);
    texts.push_back(text);
  }
  void DrawBox(const battle::TargetLabelBox& box) override {
    calls.push_back(CanvasCall::kBox);
    boxes.push_back(box);
  }
  void BeginProjection() override {
    calls.push_back(CanvasCall::kBeginProjection);
  }
  LabelScreenPoint Project(const battle::TargetWorldPoint& world) override {
    calls.push_back(CanvasCall::kProject);
    projected.push_back(world);
    return projection;
  }

  static std::uint32_t GradientColor(int gradient, int color_index) {
    constexpr int kGradientShift = 16;
    constexpr int kIndexShift = 8;
    return static_cast<std::uint32_t>((gradient << kGradientShift) |
                                      (color_index << kIndexShift) | 1);
  }

  LabelScreenPoint projection{};
  std::vector<CanvasCall> calls;
  std::vector<TargetLabelLinkLine> lines;
  std::vector<std::uint32_t> draw_modes;
  std::vector<GradientRequest> gradients;
  std::vector<battle::TargetLabelTextDraw> texts;
  std::vector<battle::TargetLabelBox> boxes;
  std::vector<battle::TargetWorldPoint> projected;
};

constexpr std::uint32_t kHeadColor = 0x42FFC0C0;
constexpr std::uint32_t kTailColor = 0x422D0C0C;

TargetLabelLinkLine Line(std::uint32_t color, LabelScreenPoint start,
                         LabelScreenPoint end) {
  return {.command_color = color, .start = start, .end = end};
}

std::vector<TargetLabelLinkLine> RightLink(LabelScreenPoint anchor,
                                           LabelScreenPoint ring,
                                           int pulse_head) {
  RecordingLabelCanvas canvas;
  battle::DrawRightTargetLabelLink(anchor, ring, pulse_head, canvas);
  return canvas.lines;
}

std::vector<TargetLabelLinkLine> LeftLink(LabelScreenPoint anchor,
                                          LabelScreenPoint ring,
                                          int pulse_head) {
  RecordingLabelCanvas canvas;
  battle::DrawLeftTargetLabelLink(anchor, ring, pulse_head, canvas);
  return canvas.lines;
}

constexpr battle::LabelSlideSteps SlideSteps() {
  return {0, 1, 2, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 160, 200};
}

// Distance from the rest position on each final slide step.
constexpr battle::LabelSlideSteps kSlideSteps = SlideSteps();

class RecordingLabelDrawer final : public battle::SphereTargetLabelDrawer {
 public:
  void DrawLabel(std::size_t index, SphereTargetLabel& label) override {
    drawn.push_back(index);
    label.gradient = static_cast<std::uint8_t>(label.gradient + 1);
  }

  std::vector<std::size_t> drawn;
};

using battle::kSphereTargetLabelCount;
using LabelArray = std::array<SphereTargetLabel, kSphereTargetLabelCount>;

LabelArray SlidingLabels(std::uint8_t state, std::int16_t screen_x) {
  LabelArray labels{};
  labels[0] = {.state = state, .screen_x = screen_x};
  return labels;
}

struct HighlightCall {
  bool enable = false;
  int actor_id = 0;
  int pulse_slot = 0;

  friend bool operator==(const HighlightCall&, const HighlightCall&) = default;
};

class RecordingHighlightHost final : public battle::SphereTargetHighlightHost {
 public:
  bool IsInSecondaryVolume(const battle::TargetPoint& point) override {
    tested.push_back(point);
    return point.x > 0;
  }
  void EnablePalettePulse(int actor_id, int pulse_slot) override {
    calls.push_back({true, actor_id, pulse_slot});
  }
  void DisablePalettePulse(int actor_id, int pulse_slot) override {
    calls.push_back({false, actor_id, pulse_slot});
  }

  std::vector<battle::TargetPoint> tested;
  std::vector<HighlightCall> calls;
};

battle::ActionTargetPreview Target(std::int16_t x, std::uint8_t actor_id,
                                   std::int8_t limb) {
  return {
      .position = {.x = x, .y = 2, .z = 3}, .actor_id = actor_id, .limb = limb};
}

// A selected label in the left column with both pulses running.
SphereTargetLabel SelectedLeftLabel() {
  return {.state = battle::kTargetLabelSettled,
          .depth = 1,
          .gradient = 2,
          .ring_size = 20,
          .screen_x = -4,
          .offset_y = 10,
          .fade = 1,
          .world_position = {1, 2, 3}};
}

struct SpherePointerRestorer {
  SpherePointerRestorer() = default;
  SpherePointerRestorer(const SpherePointerRestorer&) = delete;
  SpherePointerRestorer& operator=(const SpherePointerRestorer&) = delete;
  SpherePointerRestorer(SpherePointerRestorer&&) = delete;
  SpherePointerRestorer& operator=(SpherePointerRestorer&&) = delete;
  ~SpherePointerRestorer() {
    Battle_targetSphere = saved;
    std::ranges::copy(steps,
                      std::begin(VS::Menu::Framework::rowAnimationSteps));
  }

  Battle_TargetSphereSetupView* saved = Battle_targetSphere;
  std::array<std::uint8_t, std::size(VS::Menu::Framework::rowAnimationSteps)>
      steps = std::to_array(VS::Menu::Framework::rowAnimationSteps);
};
}  // namespace

TEST_CASE("Right target label link draws only the head segment at pulse zero",
          "[battle][sphere][labels]") {
  RecordingLabelCanvas canvas;
  battle::DrawRightTargetLabelLink({100, 50}, {200, 50}, 0, canvas);
  const std::vector<TargetLabelLinkLine> expected = {
      Line(kHeadColor, {200, 50}, {195, 50})};
  CHECK(canvas.lines == expected);
  CHECK(canvas.draw_modes ==
        std::vector<std::uint32_t>{battle::kTargetLabelLinkDrawMode});
  CHECK(canvas.calls.back() == CanvasCall::kDrawMode);
}

TEST_CASE("Right target label link runs horizontally from the ring",
          "[battle][sphere][labels]") {
  // Rise 40 and run 30 make a 70-pixel link: six horizontal segments, the
  // elbow segment and nine diagonal segments.
  const std::vector<TargetLabelLinkLine> lines =
      RightLink({100, 100}, {150, 60}, 15);
  REQUIRE(lines.size() == 17);
  CHECK(lines[0] == Line(kTailColor, {150, 60}, {147, 60}));
  CHECK(lines[5].start == LabelScreenPoint{129, 60});
  CHECK(lines[5].end == LabelScreenPoint{125, 60});
  CHECK(lines[6].start == LabelScreenPoint{124, 60});
  CHECK(lines[6].end == LabelScreenPoint{121, 60});
}

TEST_CASE("Right target label link turns diagonally at its elbow",
          "[battle][sphere][labels]") {
  const std::vector<TargetLabelLinkLine> lines =
      RightLink({100, 100}, {150, 60}, 15);
  REQUIRE(lines.size() == 17);
  CHECK(lines[7].start == LabelScreenPoint{120, 60});
  CHECK(lines[7].end == LabelScreenPoint{119, 60});
  CHECK(lines[8].start == LabelScreenPoint{119, 61});
  CHECK(lines[8].end == LabelScreenPoint{117, 64});
  CHECK(lines[16] == Line(kHeadColor, {102, 96}, {100, 99}));
}

TEST_CASE("Right target label link skips the elbow join on the ring row",
          "[battle][sphere][labels]") {
  // A flat link reaches the anchor on the ring row, so its elbow segment has
  // no diagonal join.
  const std::vector<TargetLabelLinkLine> lines =
      RightLink({100, 50}, {116, 50}, 15);
  REQUIRE(lines.size() == 16);
  CHECK(lines[15].start == LabelScreenPoint{101, 50});
  CHECK(lines[15].end == LabelScreenPoint{101, 50});
}

TEST_CASE("Right target label link follows a ring left of its elbow",
          "[battle][sphere][labels]") {
  const std::vector<TargetLabelLinkLine> lines =
      RightLink({100, 100}, {110, 60}, 0);
  REQUIRE(lines.size() == 1);
  CHECK(lines[0].start == LabelScreenPoint{110, 60});
  CHECK(lines[0].end == LabelScreenPoint{112, 60});
}

TEST_CASE("Right target label link moves an odd rise to the ring side",
          "[battle][sphere][labels]") {
  // A ring below the anchor flips the vertical direction; the odd rise moves
  // the anchor one row towards the ring.
  const std::vector<TargetLabelLinkLine> lines =
      RightLink({100, 50}, {150, 61}, 15);
  REQUIRE_FALSE(lines.empty());
  CHECK(lines.back() == Line(kHeadColor, {101, 54}, {100, 52}));
}

TEST_CASE("Target label link pulse heads past the link fade out",
          "[battle][sphere][labels]") {
  const LabelScreenPoint anchor{100, 100};
  const LabelScreenPoint ring{150, 60};
  CHECK(RightLink(anchor, ring, 20).size() == 13);
  const std::vector<TargetLabelLinkLine> last = RightLink(anchor, ring, 31);
  REQUIRE(last.size() == 1);
  CHECK(last[0].command_color == 0x42200000);
  CHECK(last[0].end == LabelScreenPoint{100, 99});

  for (const int pulse_head : {32, -1}) {
    RecordingLabelCanvas canvas;
    battle::DrawRightTargetLabelLink(anchor, ring, pulse_head, canvas);
    CHECK(canvas.lines.empty());
    CHECK(canvas.calls == std::vector<CanvasCall>{CanvasCall::kDrawMode});
  }
}

TEST_CASE("Left target label link mirrors the right link",
          "[battle][sphere][labels]") {
  RecordingLabelCanvas canvas;
  battle::DrawLeftTargetLabelLink({200, 50}, {100, 50}, 0, canvas);
  const std::vector<TargetLabelLinkLine> expected = {
      Line(kHeadColor, {100, 50}, {105, 50})};
  CHECK(canvas.lines == expected);
  CHECK(canvas.draw_modes ==
        std::vector<std::uint32_t>{battle::kTargetLabelLinkDrawMode});

  const std::vector<TargetLabelLinkLine> lines =
      LeftLink({200, 100}, {150, 60}, 15);
  REQUIRE(lines.size() == 17);
  CHECK(lines[0] == Line(kTailColor, {150, 60}, {153, 60}));
  CHECK(lines[6].end == LabelScreenPoint{179, 60});
  CHECK(lines[7].end == LabelScreenPoint{181, 60});
  CHECK(lines[16] == Line(kHeadColor, {198, 96}, {200, 99}));
}

TEST_CASE("Target labels are settled only when none is sliding",
          "[battle][sphere][labels]") {
  LabelArray labels{};
  CHECK(battle::AreTargetLabelsSettled(labels));
  labels[3].state = battle::kTargetLabelSettled;
  CHECK(battle::AreTargetLabelsSettled(labels));
  labels[battle::kSphereTargetLabelCount - 1].state =
      battle::kTargetLabelSlidingLeft;
  CHECK_FALSE(battle::AreTargetLabelsSettled(labels));
  labels[battle::kSphereTargetLabelCount - 1].state = 0xFF;
  CHECK_FALSE(battle::AreTargetLabelsSettled(labels));
}

TEST_CASE("Target label box packet links a shadow and a gradient quad",
          "[battle][sphere][labels]") {
  const battle::TargetLabelBox box{.shadow_position = {250, 52},
                                   .shadow_width = 0x48,
                                   .position = {248, 50},
                                   .left_color = 0x00123456,
                                   .right_color = 0x00654321};
  const battle::LabelBoxPacket expected = {
      0x0D123456, 0x60000000, 0x003400FA, 0x00080048, 0xE1000200,
      0x38123456, 0x003200F8, 0x00654321, 0x00320140, 0x00123456,
      0x003A00F8, 0x00654321, 0x003A0140, 0xE1000000};
  CHECK(battle::BuildTargetLabelBoxPacket(box, 0xAB123456) == expected);

  const battle::TargetLabelBox left{.position = {-4, 40}};
  const battle::LabelBoxPacket packet =
      battle::BuildTargetLabelBoxPacket(left, 0);
  CHECK(packet[6] == 0x0028FFFC);
  CHECK(packet[8] == 0x00280044);
}

TEST_CASE("Unselected right target label draws its text and box",
          "[battle][sphere][labels]") {
  SphereTargetLabel label{.state = battle::kTargetLabelSettled,
                          .gradient = 5,
                          .screen_x = 0xF8,
                          .offset_y = 20,
                          .fade = 3};
  RecordingLabelCanvas canvas;
  battle::DrawSphereTargetLabel(label, 30, canvas);

  CHECK(canvas.calls ==
        std::vector<CanvasCall>{CanvasCall::kGradient, CanvasCall::kGradient,
                                CanvasCall::kText, CanvasCall::kBox});
  CHECK(canvas.gradients ==
        std::vector<GradientRequest>{{3, 24, 0x80}, {5, 24, 0x80}});
  REQUIRE(canvas.texts.size() == 1);
  CHECK(canvas.texts[0] == battle::TargetLabelTextDraw{.position = {254, 49},
                                                       .color = 0x404040,
                                                       .overlay_slot = 2});
  REQUIRE(canvas.boxes.size() == 1);
  CHECK(canvas.boxes[0] ==
        battle::TargetLabelBox{
            .shadow_position = {250, 52},
            .shadow_width = 0x48,
            .position = {248, 50},
            .left_color = RecordingLabelCanvas::GradientColor(3, 24),
            .right_color = RecordingLabelCanvas::GradientColor(5, 24),
            .overlay_slot = 2});
  CHECK(label.gradient == 4);
}

TEST_CASE("Target label highlight fade stops at zero",
          "[battle][sphere][labels]") {
  SphereTargetLabel label{.state = battle::kTargetLabelSettled,
                          .screen_x = 0xF8};
  RecordingLabelCanvas canvas;
  battle::DrawSphereTargetLabel(label, 0, canvas);
  CHECK(label.gradient == 0);
  CHECK(canvas.gradients ==
        std::vector<GradientRequest>{{8, 0, 0x80}, {0, 0, 0x80}});
}

TEST_CASE("Selected left target label swaps its gradient colours",
          "[battle][sphere][labels]") {
  SphereTargetLabel label = SelectedLeftLabel();
  RecordingLabelCanvas canvas;
  canvas.projection = {160, 120};
  battle::DrawSphereTargetLabel(label, 30, canvas);

  CHECK(canvas.gradients ==
        std::vector<GradientRequest>{{0, 8, 0x80}, {8, 8, 0x80}});
  REQUIRE(canvas.texts.size() == 1);
  CHECK(canvas.texts[0] == battle::TargetLabelTextDraw{.position = {62, 39},
                                                       .color = 0x808080,
                                                       .overlay_slot = 1});
  REQUIRE(canvas.boxes.size() == 1);
  CHECK(canvas.boxes[0] ==
        battle::TargetLabelBox{
            .shadow_position = {-4, 42},
            .shadow_width = 0x4A,
            .position = {-4, 40},
            .left_color = RecordingLabelCanvas::GradientColor(8, 8),
            .right_color = RecordingLabelCanvas::GradientColor(0, 8),
            .overlay_slot = 1});
  CHECK(label.gradient == 7);
}

TEST_CASE("Selected left target label draws two pulses after its box",
          "[battle][sphere][labels]") {
  SphereTargetLabel label = SelectedLeftLabel();
  RecordingLabelCanvas canvas;
  canvas.projection = {160, 120};
  battle::DrawSphereTargetLabel(label, 30, canvas);

  CHECK(canvas.projected == std::vector<battle::TargetWorldPoint>{{1, 2, 3}});
  CHECK(canvas.draw_modes.size() == 2);
  std::vector<TargetLabelLinkLine> expected =
      LeftLink({160, 120}, {67, 44}, 18);
  const std::vector<TargetLabelLinkLine> second =
      LeftLink({160, 120}, {67, 44}, 2);
  expected.insert(expected.end(), second.begin(), second.end());
  CHECK(canvas.lines == expected);
  const auto projection = std::ranges::find(canvas.calls, CanvasCall::kProject);
  REQUIRE(projection != canvas.calls.begin());
  CHECK(*std::prev(projection) == CanvasCall::kBeginProjection);
  CHECK(std::ranges::find(canvas.calls, CanvasCall::kBox) <
        std::prev(projection));
}

TEST_CASE("Selected right target label links once before its second pulse",
          "[battle][sphere][labels]") {
  SphereTargetLabel label{.state = battle::kTargetLabelSettled,
                          .depth = 1,
                          .ring_size = 17,
                          .screen_x = 0xF8,
                          .offset_y = 4};
  RecordingLabelCanvas canvas;
  canvas.projection = {100, 150};
  battle::DrawSphereTargetLabel(label, 0, canvas);
  CHECK(canvas.draw_modes.size() == 1);
  CHECK(canvas.lines == RightLink({100, 150}, {0xF8, 8}, 15));
  CHECK(canvas.texts.at(0).position == LabelScreenPoint{0xFE, 3});
}

TEST_CASE("Target labels slide into the right column and settle",
          "[battle][sphere][labels]") {
  LabelArray labels{};
  labels[0] = {.state = battle::kTargetLabelSlidingLeft, .screen_x = 288};
  labels[1] = {.state = battle::kTargetLabelSlidingLeft, .screen_x = 249};
  labels[2] = {.state = battle::kTargetLabelSlidingLeft, .screen_x = 248};
  RecordingLabelDrawer drawer;
  battle::UpdateSphereTargetLabels(labels, kSlideSteps, drawer);

  CHECK(labels[0].screen_x == 280);
  CHECK(labels[0].state == battle::kTargetLabelSlidingLeft);
  CHECK(labels[1].screen_x == 248);
  CHECK(labels[1].state == battle::kTargetLabelSlidingLeft);
  CHECK(labels[2].screen_x == 248);
  CHECK(labels[2].state == battle::kTargetLabelSettled);
  CHECK(drawer.drawn == std::vector<std::size_t>{0, 1, 2});
  CHECK(labels[0].gradient == 1);
}

TEST_CASE("Target labels past every slide step leave the screen",
          "[battle][sphere][labels]") {
  LabelArray labels{};
  labels[3] = {.state = battle::kTargetLabelSlidingLeft, .screen_x = 498};
  RecordingLabelDrawer drawer;
  battle::UpdateSphereTargetLabels(labels, kSlideSteps, drawer);

  // Past every step the label moves to the last one, off screen.
  CHECK(labels[3].screen_x == 448);
  CHECK(labels[3].state == battle::kTargetLabelHidden);
  CHECK(labels[3].gradient == 0);
  CHECK(drawer.drawn.empty());
}

TEST_CASE("Target labels slide into the left column and settle",
          "[battle][sphere][labels]") {
  LabelArray labels{};
  labels[0] = {.state = battle::kTargetLabelSlidingRight, .screen_x = -44};
  labels[1] = {.state = battle::kTargetLabelSlidingRight, .screen_x = -3};
  labels[2] = {.state = battle::kTargetLabelSlidingRight, .screen_x = -5};
  RecordingLabelDrawer drawer;
  battle::UpdateSphereTargetLabels(labels, kSlideSteps, drawer);

  CHECK(labels[0].screen_x == -36);
  CHECK(labels[0].state == battle::kTargetLabelSlidingRight);
  CHECK(labels[1].screen_x == -4);
  CHECK(labels[1].state == battle::kTargetLabelSettled);
  CHECK(labels[2].screen_x == -4);
  CHECK(labels[2].state == battle::kTargetLabelSlidingRight);
  CHECK(drawer.drawn == std::vector<std::size_t>{0, 1, 2});
}

TEST_CASE("Target labels leave their columns and hide off screen",
          "[battle][sphere][labels]") {
  LabelArray labels{};
  labels[0] = {.state = battle::kTargetLabelSlidingLeft, .screen_x = 100};
  labels[1] = {.state = battle::kTargetLabelSlidingLeft, .screen_x = -100};
  labels[2] = {.state = battle::kTargetLabelSlidingRight, .screen_x = 200};
  labels[3] = {.state = battle::kTargetLabelSlidingRight, .screen_x = 300};
  RecordingLabelDrawer drawer;
  battle::UpdateSphereTargetLabels(labels, kSlideSteps, drawer);

  CHECK(labels[0].screen_x == 68);
  CHECK(labels[1].screen_x == -132);
  CHECK(labels[1].state == battle::kTargetLabelHidden);
  CHECK(labels[2].screen_x == 232);
  CHECK(labels[3].screen_x == 332);
  CHECK(labels[3].state == battle::kTargetLabelHidden);
  CHECK(drawer.drawn == std::vector<std::size_t>{0, 2});
}

TEST_CASE("Target label updates keep resting labels and skip hidden ones",
          "[battle][sphere][labels]") {
  LabelArray labels{};
  labels[0] = {.state = battle::kTargetLabelHidden, .screen_x = 100};
  labels[1] = {.state = battle::kTargetLabelSettled, .screen_x = -126};
  labels[2] = {.state = battle::kTargetLabelSettled, .screen_x = -127};
  labels[3] = {.state = 4, .screen_x = 319};
  labels[4] = {.state = battle::kTargetLabelSettled, .screen_x = 320};
  RecordingLabelDrawer drawer;
  battle::UpdateSphereTargetLabels(labels, kSlideSteps, drawer);

  CHECK(drawer.drawn == std::vector<std::size_t>{1, 3});
  CHECK(labels[0].state == battle::kTargetLabelHidden);
  CHECK(labels[0].gradient == 0);
  CHECK(labels[1].screen_x == -126);
  CHECK(labels[2].state == battle::kTargetLabelHidden);
  CHECK(labels[3].state == 4);
  CHECK(labels[3].screen_x == 319);
  CHECK(labels[4].state == battle::kTargetLabelHidden);
}

TEST_CASE("Target label slides wrap their halfword screen x",
          "[battle][sphere][labels]") {
  LabelArray labels = SlidingLabels(battle::kTargetLabelSlidingRight, 32767);
  RecordingLabelDrawer drawer;
  battle::UpdateSphereTargetLabels(labels, kSlideSteps, drawer);
  CHECK(labels[0].screen_x == -32737);
  CHECK(labels[0].state == battle::kTargetLabelHidden);

  labels = SlidingLabels(battle::kTargetLabelSlidingLeft, -32768);
  battle::UpdateSphereTargetLabels(labels, kSlideSteps, drawer);
  CHECK(labels[0].screen_x == 32736);
  CHECK(labels[0].state == battle::kTargetLabelHidden);
  CHECK(drawer.drawn.empty());
}

TEST_CASE("Sphere target highlights pulse actors inside the secondary volume",
          "[battle][sphere][labels]") {
  const std::array targets = {Target(5, 3, -1), Target(-5, 15, 2),
                              Target(5, 16, 1), Target(5, 0x25, 1),
                              Target(7, 0, 4)};
  RecordingHighlightHost host;
  battle::UpdateSphereTargetHighlights(targets, host);

  CHECK(host.tested.size() == 3);
  CHECK(host.calls == std::vector<HighlightCall>{
                          {true, 3, -1}, {false, 15, 2}, {true, 0, 4}});
  CHECK(host.tested[1] == battle::TargetPoint{.x = -5, .y = 2, .z = 3});
}

TEST_CASE("Sphere target highlights do nothing without targets",
          "[battle][sphere][labels]") {
  RecordingHighlightHost host;
  battle::UpdateSphereTargetHighlights({}, host);
  CHECK(host.tested.empty());
  CHECK(host.calls.empty());
}

TEST_CASE("Live target label settled entry reads the sphere labels",
          "[battle][sphere][labels]") {
  const SpherePointerRestorer restore;
  Battle_TargetSphereSetupView storage{};
  auto& sphere = reinterpret_cast<Battle_TargetSphere&>(storage);
  Battle_targetSphere = &storage;
  CHECK(battle::AreTargetLabelsSettledEntry() == 1);
  sphere.labels[23].state = battle::kTargetLabelSlidingRight;
  CHECK(battle::AreTargetLabelsSettledEntry() == 0);
  sphere.labels[23].state = battle::kTargetLabelSettled;
  CHECK(battle::AreTargetLabelsSettledEntry() == 1);
}

TEST_CASE("Live target label update reads unsigned slide steps",
          "[battle][sphere][labels]") {
  const SpherePointerRestorer restore;
  Battle_TargetSphereSetupView storage{};
  auto& sphere = reinterpret_cast<Battle_TargetSphere&>(storage);
  Battle_targetSphere = &storage;

  // Steps above 0x7F are unsigned; read signed, this label would land on
  // screen instead of moving past it.
  std::fill_n(std::begin(VS::Menu::Framework::rowAnimationSteps),
              battle::kTargetLabelSlideStepCount, static_cast<char>(0xC8));
  sphere.labels[0].state = battle::kTargetLabelSlidingLeft;
  sphere.labels[0].screenX = 32752;
  sphere.labels[0].gradient = 5;
  sphere.labels[1].state = battle::kTargetLabelSlidingRight;
  sphere.labels[1].screenX = 300;
  sphere.labels[2].state = battle::kTargetLabelSlidingLeft;
  sphere.labels[2].screenX = -100;
  sphere.labels[3].screenX = 77;

  battle::UpdateSphereTargetLabelsEntry();
  CHECK(sphere.labels[0].screenX == 448);
  CHECK(sphere.labels[0].state == battle::kTargetLabelHidden);
  CHECK(sphere.labels[0].gradient == 5);
  CHECK(sphere.labels[1].screenX == 332);
  CHECK(sphere.labels[2].screenX == -132);
  CHECK(sphere.labels[3].screenX == 77);
  CHECK(battle::AreTargetLabelsSettledEntry() == 1);
}

TEST_CASE("Live sphere target highlights ignore an empty or missing list",
          "[battle][sphere][labels]") {
  const SpherePointerRestorer restore;
  Battle_targetSphere = nullptr;
  battle::UpdateSphereTargetHighlightsEntry(nullptr, 0);
  battle::UpdateSphereTargetHighlightsEntry(nullptr, 3);
  std::array<Battle_TargetLabelLine, 2> lines{};
  lines[0].opaque9 = 16;
  lines[1].opaque9 = 0x30;
  battle::UpdateSphereTargetHighlightsEntry(lines.data(),
                                            static_cast<int>(lines.size()));
  battle::UpdateSphereTargetHighlightsEntry(lines.data(), -1);
  CHECK(Battle_targetSphere == nullptr);
}
