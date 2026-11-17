#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Menu/TextBoxUpdate.hpp"

namespace {
namespace menu = VS::Menu;
enum class Draw {
  kBlend,
  kInformation,
  kFrame,
  kProgressive,
  kScaled,
  kSubmit,
  kBegin,
  kTimer,
  kOverlay,
  kRandom,
};
struct Call {
  Draw kind;
  std::size_t box = 0;
  std::int32_t value = 0;
  friend bool operator==(const Call&, const Call&) = default;
};
struct UpdateWorld final : menu::TextBoxUpdateHost {
  std::array<menu::TextBox, menu::kTextBoxCount> boxes{};
  std::vector<Call> calls;
  std::int32_t tick_speed = 2;
  std::uint8_t buffer = 0;
  bool timer = false;

  menu::TextBox ReadBox(std::size_t index) const override {
    return boxes.at(index);
  }
  void WriteBox(std::size_t index, const menu::TextBox& box) override {
    boxes.at(index) = box;
  }
  std::int32_t TickSpeed() const override { return tick_speed; }
  void BlendText(std::size_t index, std::int32_t scale) override {
    calls.push_back({Draw::kBlend, index, scale});
  }
  void DrawInformationBackdrop(std::size_t index) override {
    calls.push_back({Draw::kInformation, index});
  }
  void DrawFrame(std::size_t index) override {
    calls.push_back({Draw::kFrame, index});
  }
  void DrawProgressiveText(std::size_t index) override {
    calls.push_back({Draw::kProgressive, index});
  }
  void DrawScaledText(std::size_t index, std::int32_t scale) override {
    calls.push_back({Draw::kScaled, index, scale});
  }
  void SubmitOrdering() override { calls.push_back({Draw::kSubmit}); }
  std::uint8_t OrderingBufferIndex() const override { return buffer; }
  void BeginOrderingBuffer(std::uint8_t index) override {
    buffer = index;
    calls.push_back({Draw::kBegin, index});
  }
  bool TimerVisible() const override { return timer; }
  void DrawTimer() override { calls.push_back({Draw::kTimer}); }
  void DrawOverlay() override { calls.push_back({Draw::kOverlay}); }
  void AdvanceRandomBits() override { calls.push_back({Draw::kRandom}); }
};

menu::TextBox OpeningBox(std::uint8_t kind = 6) {
  menu::TextBox box;
  box.flags.kind = kind;
  box.flags.open_style = 2;
  box.state = 2;
  box.open_duration = 4;
  box.x = 40;
  box.y = 60;
  box.center_x = 20;
  box.center_y = 30;
  box.chars_per_line = 10;
  box.line_count = 2;
  return box;
}
}  // namespace

TEST_CASE("F01392 skips closed and frozen boxes but preserves frame tail order",
          "[text-box-update][F01392]") {
  UpdateWorld world;
  world.boxes[7] = OpeningBox();
  world.boxes[7].flags.frozen = true;
  const auto original = world.boxes;
  world.timer = true;
  world.buffer = 2;
  REQUIRE(menu::UpdateTextBoxes(world) == menu::TextBoxUpdateStatus::kComplete);
  CHECK(world.boxes == original);
  CHECK(world.buffer == 0);
  CHECK(world.calls == std::vector<Call>{{Draw::kSubmit},
                                         {Draw::kBegin},
                                         {Draw::kTimer},
                                         {Draw::kOverlay},
                                         {Draw::kRandom}});
}

TEST_CASE("F01392 visits active boxes in descending order",
          "[text-box-update][F01392]") {
  UpdateWorld world;
  world.boxes[1] = OpeningBox();
  world.boxes[4] = OpeningBox();
  world.boxes[7] = OpeningBox();
  REQUIRE(menu::UpdateTextBoxes(world) == menu::TextBoxUpdateStatus::kComplete);
  REQUIRE(world.calls.size() == 7);
  CHECK(world.calls[0] == Call{Draw::kBlend, 7, 0x10000});
  CHECK(world.calls[1] == Call{Draw::kBlend, 4, 0x10000});
  CHECK(world.calls[2] == Call{Draw::kBlend, 1, 0x10000});
  CHECK(world.calls[3].kind == Draw::kSubmit);
}

TEST_CASE("F01392 opening styles retain original scale and geometry",
          "[text-box-update][F01392]") {
  UpdateWorld world;
  const auto style = GENERATE(std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{2},
                              std::uint8_t{3});
  constexpr std::array<std::int32_t, 4> kScale{46336, 46336, 65536, 73728};
  constexpr std::array<std::int16_t, 4> kWidth{91, 91, 130, 146};
  constexpr std::array<std::int16_t, 4> kX{34, 34, 40, 42};
  world.boxes[0] = OpeningBox();
  world.boxes[0].flags.open_style = style;
  REQUIRE(menu::UpdateTextBoxes(world) == menu::TextBoxUpdateStatus::kComplete);
  CHECK(world.boxes[0].state == 3);
  CHECK(world.calls.front() == Call{Draw::kBlend, 0, kScale[style]});
  CHECK(world.boxes[0].frame_width == kWidth[style]);
  CHECK(world.boxes[0].frame_x == kX[style]);
}

TEST_CASE("F01392 frame overshoot completes before progressive text is drawn",
          "[text-box-update][F01392]") {
  UpdateWorld world;
  world.boxes[3] = OpeningBox(7);
  world.boxes[3].state = 3;
  REQUIRE(menu::UpdateTextBoxes(world) == menu::TextBoxUpdateStatus::kComplete);
  CHECK(world.boxes[3].state == 4);
  CHECK(world.boxes[3].frame_width == 138);
  REQUIRE(world.calls.size() == 5);
  CHECK(world.calls.front() == Call{Draw::kFrame, 3});
  world.calls.clear();
  REQUIRE(menu::UpdateTextBoxes(world) == menu::TextBoxUpdateStatus::kComplete);
  CHECK(world.boxes[3].frame_width == 130);
  CHECK(world.calls[1] == Call{Draw::kProgressive, 3});
}

TEST_CASE("F01392 closing dispatch distinguishes blend information and frame",
          "[text-box-update][F01392]") {
  UpdateWorld world;
  const auto kind =
      GENERATE(std::uint8_t{0}, std::uint8_t{3}, std::uint8_t{4},
               std::uint8_t{6}, std::uint8_t{7}, std::uint8_t{15});
  world.boxes[2] = OpeningBox(kind);
  world.boxes[2].state = -1;
  REQUIRE(menu::UpdateTextBoxes(world) == menu::TextBoxUpdateStatus::kComplete);
  CHECK(world.boxes[2].state == -2);
  const std::vector<Call> drawing(world.calls.begin(), world.calls.end() - 4);
  if (kind == 0 || kind == 6) {
    CHECK(drawing == std::vector<Call>{{Draw::kBlend, 2, 0xC000}});
  } else if (kind == 3) {
    CHECK(drawing.empty());
  } else if (kind == 4) {
    CHECK(drawing == std::vector<Call>{{Draw::kInformation, 2}});
  } else if (kind == 7) {
    CHECK(drawing ==
          std::vector<Call>{{Draw::kFrame, 2}, {Draw::kScaled, 2, 0xC000}});
  } else {
    CHECK(drawing == std::vector<Call>{{Draw::kScaled, 2, 0xC000}});
  }
}

TEST_CASE("F01392 closing beyond duration only clears state",
          "[text-box-update][F01392]") {
  UpdateWorld world;
  world.boxes[0] = OpeningBox();
  world.boxes[0].state = -4;
  world.boxes[0].frame_width = 77;
  auto expected = world.boxes[0];
  expected.state = 0;
  REQUIRE(menu::UpdateTextBoxes(world) == menu::TextBoxUpdateStatus::kComplete);
  CHECK(world.boxes[0] == expected);
  CHECK(world.calls.front().kind == Draw::kSubmit);
}

TEST_CASE("F01392 byte dimensions and duration are zero extended",
          "[text-box-update][F01392]") {
  UpdateWorld world;
  world.boxes[0] = OpeningBox();
  world.boxes[0].state = 1;
  world.boxes[0].open_duration = -1;
  world.boxes[0].chars_per_line = -1;
  world.boxes[0].line_count = -1;
  REQUIRE(menu::UpdateTextBoxes(world) == menu::TextBoxUpdateStatus::kComplete);
  CHECK(world.boxes[0].state == 2);
  CHECK(world.boxes[0].frame_width == 12);
  CHECK(world.boxes[0].frame_height == 13);
  CHECK(world.calls.front() == Call{Draw::kBlend, 0, 259});
}

TEST_CASE("F01392 drawing observes the register before narrowing stored state",
          "[text-box-update][F01392]") {
  UpdateWorld world;
  world.boxes[0] = OpeningBox(0);
  world.boxes[0].state = 1;
  world.boxes[0].open_duration = -1;
  world.boxes[0].flags.open_style = 0;
  world.tick_speed = (std::numeric_limits<std::int32_t>::min)();
  REQUIRE(menu::UpdateTextBoxes(world) == menu::TextBoxUpdateStatus::kComplete);
  CHECK(world.boxes[0].state == 1);
  CHECK(world.calls.front() == Call{Draw::kBlend, 0, 0});
}

TEST_CASE("F01392 invalid ordering index is reported without indexing storage",
          "[text-box-update][F01392]") {
  UpdateWorld world;
  world.buffer = 255;
  REQUIRE(menu::UpdateTextBoxes(world) ==
          menu::TextBoxUpdateStatus::kInvalidOrderingBuffer);
  CHECK(world.buffer == 255);
  CHECK(world.calls ==
        std::vector<Call>{{Draw::kSubmit}, {Draw::kOverlay}, {Draw::kRandom}});
}
