#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Menu/TimeAttackTerminalFixture.hpp"

namespace {
namespace r = VS::Menu::Results;
namespace fixture = VS::tests::time_attack_terminal;
using fixture::Tag;
using Step = r::GameClearStep;

std::vector<std::int64_t> Tags(std::initializer_list<Tag> tags) {
  std::vector<std::int64_t> result;
  for (const auto tag : tags) result.push_back(static_cast<std::int64_t>(tag));
  return result;
}

r::TimeAttackState SentinelState() {
  return {.init_phase = 2,
          .timer = 199,
          .phase = 3,
          .animation = 2047,
          .new_slot = 2,
          .time = 123456,
          .shown_map = 57,
          .shown_score = 89,
          .selection = 0xFEDCBA98};
}
}  // namespace

TEST_CASE("F03011 reserves and enqueues once before polling",
          "[time-attack-terminal][F03011]") {
  fixture::Load host;
  r::TimeAttackState state;
  CHECK(r::InitializeTimeAttackResults(state, host) == Step::kPending);
  CHECK(state.init_phase == 1);
  CHECK(host.calls.Tags() ==
        Tags({Tag::kReserve, Tag::kAllocate, Tag::kRequest, Tag::kEnqueue}));
  CHECK(host.calls.rows.front().at(2) == 1);
  host.loaded = false;
  CHECK(r::InitializeTimeAttackResults(state, host) == Step::kPending);
  CHECK(host.calls.Count(Tag::kAllocate) == 1);
  CHECK(state.init_phase == 1);
}

TEST_CASE(
    "F03011 uploads then saves all palette colors after clearing one word",
    "[time-attack-terminal][F03011]") {
  fixture::Load host;
  host.palette.fill(0x7FFF);
  host.rectangles.front().width = 61;
  r::TimeAttackState state{.init_phase = 1};
  CHECK(r::InitializeTimeAttackResults(state, host) == Step::kPending);
  CHECK(state.init_phase == 2);
  CHECK(host.calls.Tags() ==
        Tags({Tag::kParse, Tag::kPixels, Tag::kPalette, Tag::kCopyPalette,
              Tag::kRelease, Tag::kQueueMusic}));
  CHECK(host.rectangles.front() == VS::Render::ScreenRect{832, 256, 61, 255});
  CHECK(host.rectangles.at(1) == VS::Render::ScreenRect{768, 511, 160, 1});
  CHECK(host.saved_palette.at(0) == 0);
  CHECK(host.saved_palette.at(1) == 0);
  CHECK(host.saved_palette.at(2) == 0x7FFF);
  CHECK(host.saved_palette.back() == 0x7FFF);
}

TEST_CASE("F03011 inserts an equal time ahead and shifts complete record words",
          "[time-attack-terminal][F03011]") {
  fixture::Load host;
  host.run.course = 3;
  host.run.seconds = 30;
  host.run.round = 254;
  host.run.difficulty = 3;
  host.scores.at(3) = {0x81000F00, 0xF2001E00, 0x00003C00};
  host.scores.at(2) = {77, 88, 99};
  auto state = SentinelState();
  CHECK(r::InitializeTimeAttackResults(state, host) == Step::kComplete);
  CHECK(host.scores.at(3) ==
        r::TrialScoreRow{0x81000F00, 0xFE001E00, 0xF2001E00});
  CHECK(host.scores.at(2) == r::TrialScoreRow{77, 88, 99});
  CHECK(state.new_slot == 1);
  CHECK(state.time == 0x1E00);
  CHECK(host.at_free == state);
  CHECK(state.shown_map == 57);
  CHECK(state.shown_score == 89);
  CHECK(state.selection == 0xFEDCBA98);
}

TEST_CASE("F03011 zero clock uses original maximum and respects no-record rank",
          "[time-attack-terminal][F03011]") {
  fixture::Load host;
  auto state = SentinelState();
  const auto before = host.scores;
  CHECK(r::InitializeTimeAttackResults(state, host) == Step::kComplete);
  CHECK(state.time == 0x3B3B63);
  CHECK(state.new_slot == 3);
  CHECK(host.scores == before);
  CHECK(state.timer == 0);
  CHECK(state.phase == 0);
  CHECK(state.animation == 0);
  CHECK(state.init_phase == 0);
}

TEST_CASE(
    "F03011 reads the clock after playing music and waits without mutation",
    "[time-attack-terminal][F03011]") {
  fixture::Load host;
  auto state = SentinelState();
  const auto before = state;
  host.music_busy = true;
  CHECK(r::InitializeTimeAttackResults(state, host) == Step::kPending);
  CHECK(state == before);
  CHECK(host.calls.Tags() == Tags({Tag::kPollMusic}));
  host.music_busy = false;
  host.minutes_on_play = 255;
  CHECK(r::InitializeTimeAttackResults(state, host) == Step::kComplete);
  CHECK(state.time == 0xFF0000);
  CHECK(host.calls.Tags() ==
        Tags({Tag::kPollMusic, Tag::kPollMusic, Tag::kPlayMusic, Tag::kFree}));
}

TEST_CASE("F03011 refuses bounded record views without inserting or freeing",
          "[time-attack-terminal][F03011]") {
  for (const auto course : {1U, 8U, 255U}) {
    fixture::Load host;
    host.run.course = static_cast<std::uint8_t>(course);
    host.run.records = std::span(host.scores).first(1);
    auto state = SentinelState();
    const auto before = state;
    CHECK(r::InitializeTimeAttackResults(state, host) == Step::kRefused);
    CHECK(state == before);
    CHECK(host.commits == 0);
    CHECK(host.calls.Count(Tag::kFree) == 0);
  }
}

TEST_CASE("F03012 loads both images but only the first palette and no music",
          "[time-attack-terminal][F03012]") {
  fixture::Load host;
  host.palette.fill(0x1234);
  host.saved_palette.fill(0x5678);
  r::TimeAttackState state{.init_phase = 1};
  const auto second_palette = host.rectangles.at(3);
  CHECK(r::InitializeTimeAttackStart(state, host) == Step::kPending);
  CHECK(host.calls.Tags() == Tags({Tag::kParse, Tag::kPixels, Tag::kPalette,
                                   Tag::kParse, Tag::kPixels, Tag::kRelease}));
  CHECK(host.rectangles.at(2) == VS::Render::ScreenRect{896, 256, 64, 255});
  CHECK(host.rectangles.at(3) == second_palette);
  CHECK(host.saved_palette.front() == 0x5678);
  CHECK(state.init_phase == 2);
}

TEST_CASE("F03012 frees before explicit resets and retains result-only fields",
          "[time-attack-terminal][F03012]") {
  fixture::Load host;
  auto state = SentinelState();
  const auto before = state;
  CHECK(r::InitializeTimeAttackStart(state, host) == Step::kComplete);
  CHECK(host.at_free == before);
  CHECK(state.new_slot == before.new_slot);
  CHECK(state.time == before.time);
  CHECK(state.shown_map == 0);
  CHECK(state.shown_score == 0);
  CHECK(state.selection == 0);
  CHECK(state.init_phase == 0);
  CHECK(host.calls.Tags() == Tags({Tag::kFree}));
}

TEST_CASE("F03011 and F03012 refuse malformed uploads without releasing owners",
          "[time-attack-terminal][F03011][F03012]") {
  using Initialize = Step (*)(r::TimeAttackState&, r::TimeAttackLoadHost&);
  for (const Initialize initialize :
       {r::InitializeTimeAttackResults, r::InitializeTimeAttackStart}) {
    fixture::Load host;
    r::TimeAttackState state{.init_phase = 1};
    host.parse_ok = false;
    CHECK(initialize(state, host) == Step::kRefused);
    CHECK(state.init_phase == 1);
    CHECK(host.calls.Count(Tag::kRelease) == 0);
    host.parse_ok = true;
    host.upload_ok = false;
    CHECK(initialize(state, host) == Step::kRefused);
    CHECK(host.calls.Count(Tag::kQueueMusic) == 0);
  }
}

TEST_CASE("F03011 and F03012 preflight phases allocation and completion owners",
          "[time-attack-terminal][F03011][F03012]") {
  using Initialize = Step (*)(r::TimeAttackState&, r::TimeAttackLoadHost&);
  for (const Initialize initialize :
       {r::InitializeTimeAttackResults, r::InitializeTimeAttackStart}) {
    fixture::Load host;
    r::TimeAttackState state{.init_phase = -1};
    CHECK(initialize(state, host) == Step::kRefused);
    CHECK(host.calls.rows.empty());
    state.init_phase = 0;
    host.reserve_ok = false;
    CHECK(initialize(state, host) == Step::kRefused);
    CHECK(host.calls.Count(Tag::kAllocate) == 0);
    state.init_phase = 2;
    host.ready = false;
    CHECK(initialize(state, host) == Step::kRefused);
    CHECK(host.calls.Count(Tag::kFree) == 0);
    CHECK(state.init_phase == 2);
  }
}

TEST_CASE("F03016 reveal sound order and timers follow the original",
          "[time-attack-terminal][F03016]") {
  fixture::Screen host;
  r::TimeAttackState state{.timer = 96, .animation = 1024, .new_slot = 1};
  CHECK(r::UpdateTimeAttackResults(state, false, host) == Step::kPending);
  CHECK(host.calls.Tags() ==
        Tags({Tag::kPulse, Tag::kStop, Tag::kSound, Tag::kDraw, Tag::kDraw,
              Tag::kDraw, Tag::kDraw, Tag::kDraw, Tag::kDraw}));
  CHECK(host.calls.rows.front().at(1) == 16);
  CHECK(host.calls.rows.at(1).at(2) == 0x72);
  CHECK(host.calls.rows.at(2).at(2) == 0x73);
  CHECK(host.calls.rows.at(4).at(4) == 64);
  CHECK(host.calls.rows.at(5).at(4) == 0);
  CHECK(state.animation == 1088);
  CHECK(state.timer == 97);
}

TEST_CASE(
    "F03016 new-record sound follows its draw and precedes stored records",
    "[time-attack-terminal][F03016]") {
  fixture::Screen host;
  r::TimeAttackState state{.timer = 128};
  CHECK(r::UpdateTimeAttackResults(state, false, host) == Step::kPending);
  CHECK(host.calls.Tags() ==
        Tags({Tag::kPulse, Tag::kDraw, Tag::kDraw, Tag::kDraw, Tag::kDraw,
              Tag::kSound, Tag::kDraw, Tag::kDraw, Tag::kDraw}));
  CHECK(host.calls.rows.at(4).at(1) ==
        static_cast<std::int64_t>(r::TimeAttackLine::kNewRecord));
  CHECK(host.calls.rows.at(5).at(2) == 0x76);
  CHECK(host.calls.rows.back().at(4) == -48);
  CHECK(host.calls.rows.back().at(5) == 2);
}

TEST_CASE("F03016 reveal and hold gates compare the incremented timer",
          "[time-attack-terminal][F03016]") {
  fixture::Screen host;
  r::TimeAttackState state{.timer = 191, .new_slot = 3};
  CHECK(r::UpdateTimeAttackResults(state, false, host) == Step::kPending);
  CHECK(state.phase == 1);
  CHECK(state.timer == 0);
  state.timer = 449;
  CHECK(r::UpdateTimeAttackResults(state, false, host) == Step::kPending);
  CHECK(state.phase == 2);
  CHECK(state.timer == 7);
  CHECK(host.calls.Count(Tag::kSound) == 0);
}

TEST_CASE(
    "F03016 direct completion repeats pulse and retirement without latching",
    "[time-attack-terminal][F03016]") {
  fixture::Screen host;
  r::TimeAttackState state{.timer = 0, .phase = 2, .new_slot = 3};
  CHECK(r::UpdateTimeAttackResults(state, false, host) == Step::kPending);
  CHECK(state.phase == 3);
  CHECK(r::UpdateTimeAttackResults(state, false, host) == Step::kComplete);
  CHECK(r::UpdateTimeAttackResults(state, false, host) == Step::kComplete);
  CHECK(host.calls.Count(Tag::kRetire) == 2);
  CHECK(host.calls.Count(Tag::kPulse) == 3);
  CHECK(state.timer == 0);
  CHECK(state.phase == 3);
  CHECK(state.animation == 192);
}

TEST_CASE(
    "F03016 wraps counters and refuses invalid native state before drawing",
    "[time-attack-terminal][F03016]") {
  fixture::Screen host;
  r::TimeAttackState state{.timer = std::numeric_limits<std::int32_t>::max(),
                           .animation = 0xFFFFFFFF};
  CHECK(r::UpdateTimeAttackResults(state, false, host) == Step::kPending);
  CHECK(state.timer == std::numeric_limits<std::int32_t>::min());
  CHECK(state.animation == 63);
  CHECK(state.phase == 0);
  host.calls.rows.clear();
  state.new_slot = 4;
  const auto before = state;
  CHECK(r::UpdateTimeAttackResults(state, true, host) == Step::kRefused);
  CHECK(state == before);
  CHECK(host.calls.rows.empty());
}

TEST_CASE(
    "F03017 confirm bypasses the introduction fade and countdown ignores it",
    "[time-attack-terminal][F03017]") {
  fixture::Screen host;
  r::TimeAttackState state{.timer = 89};
  CHECK(r::UpdateTimeAttackStart(state, true, host) == Step::kPending);
  CHECK(state.phase == 2);
  CHECK(state.timer == 0);
  CHECK(r::UpdateTimeAttackStart(state, true, host) == Step::kPending);
  CHECK(state.phase == 3);
  CHECK(state.timer == 0);
  CHECK(r::UpdateTimeAttackStart(state, true, host) == Step::kPending);
  CHECK(state.phase == 3);
  CHECK(state.timer == 1);
}

TEST_CASE(
    "F03017 decrements the fade before drawing and configures effects in order",
    "[time-attack-terminal][F03017]") {
  fixture::Screen host;
  r::TimeAttackState state{.timer = 1, .phase = 1};
  CHECK(r::UpdateTimeAttackStart(state, false, host) == Step::kPending);
  CHECK(host.calls.rows.front().at(4) == 0);
  CHECK(state.phase == 2);
  host.calls.rows.clear();
  CHECK(r::UpdateTimeAttackStart(state, false, host) == Step::kPending);
  CHECK(host.calls.Tags() ==
        Tags({Tag::kTickSpeed, Tag::kRotation, Tag::kScale, Tag::kTint,
              Tag::kOffset, Tag::kBlend, Tag::kDepth, Tag::kPrevious,
              Tag::kEnabled}));
  CHECK(host.calls.rows.at(2).at(1) == 0x1029);
  CHECK(host.calls.rows.at(3).at(1) == 64);
  CHECK(host.calls.rows.at(3).at(3) == 220);
}

TEST_CASE(
    "F03017 countdown slides settle and GO changes effects at exact frames",
    "[time-attack-terminal][F03017]") {
  fixture::Screen host;
  r::TimeAttackState state{.timer = 7, .phase = 3};
  CHECK(r::UpdateTimeAttackStart(state, false, host) == Step::kPending);
  CHECK(host.calls.rows.back().at(2) == 160);
  state.timer = 45;
  host.calls.rows.clear();
  CHECK(r::UpdateTimeAttackStart(state, false, host) == Step::kPending);
  CHECK(host.calls.Tags() ==
        Tags({Tag::kSound, Tag::kSound, Tag::kSound, Tag::kBlend, Tag::kScale,
              Tag::kTint, Tag::kDraw}));
  CHECK(host.calls.rows.at(0).at(2) == 0x80);
  CHECK(host.calls.rows.at(2).at(2) == 0x82);
  CHECK(host.calls.rows.at(4).at(1) == 0x107A);
  CHECK(host.calls.rows.back().at(4) == 59);
}

TEST_CASE(
    "F03017 countdown end and repeated completion keep caller-owned timers",
    "[time-attack-terminal][F03017]") {
  fixture::Screen host;
  r::TimeAttackState state{.timer = 61, .phase = 3, .selection = 77};
  CHECK(r::UpdateTimeAttackStart(state, false, host) == Step::kPending);
  CHECK(state.phase == 4);
  CHECK(state.timer == 62);
  host.calls.rows.clear();
  CHECK(r::UpdateTimeAttackStart(state, false, host) == Step::kComplete);
  CHECK(host.calls.Tags() ==
        Tags({Tag::kTickSpeed, Tag::kScale, Tag::kEnabled, Tag::kRetire}));
  CHECK(host.calls.rows.at(1).at(1) == 0);
  CHECK(r::UpdateTimeAttackStart(state, false, host) == Step::kComplete);
  CHECK(state.selection == 77);
  CHECK(state.timer == 62);
}

TEST_CASE("F03017 refuses negative table indices and failed effect acquisition",
          "[time-attack-terminal][F03017]") {
  fixture::Screen host;
  r::TimeAttackState state{.timer = -1, .phase = 3};
  CHECK(r::UpdateTimeAttackStart(state, false, host) == Step::kRefused);
  CHECK(host.calls.rows.empty());
  CHECK(state.timer == -1);
  state.phase = 2;
  host.effect_ok = false;
  CHECK(r::UpdateTimeAttackStart(state, false, host) == Step::kRefused);
  CHECK(state.phase == 2);
  CHECK(host.calls.rows.back().front() ==
        static_cast<std::int64_t>(Tag::kEnabled));
}

TEST_CASE("F03009 preserves child returns and score-only call ordering",
          "[time-attack-terminal][F03009]") {
  fixture::Presentations host;
  host.result = -17;
  for (std::uint8_t selector = 0; selector != 7; ++selector) {
    host.calls.rows.clear();
    CHECK(r::InitializeResultsScreen(selector, host) ==
          (selector == 3 ? 1 : -17));
    if (selector == 3) {
      CHECK(host.calls.Tags() == Tags({Tag::kHeal, Tag::kFinish}));
    } else {
      CHECK(host.calls.Tags() == Tags({Tag::kInitialize}));
      CHECK(host.calls.rows.front().at(1) == selector);
    }
  }
}

TEST_CASE(
    "F03014 preserves child returns and score-only completion has no callbacks",
    "[time-attack-terminal][F03014]") {
  fixture::Presentations host;
  host.result = std::numeric_limits<std::int32_t>::min();
  for (std::uint8_t selector = 0; selector != 7; ++selector) {
    host.calls.rows.clear();
    CHECK(r::UpdateResultsPresentation(selector, host) ==
          (selector == 3 ? 1 : host.result));
    CHECK(host.calls.rows.size() == (selector == 3 ? 0 : 1));
    if (selector != 3) {
      CHECK(host.calls.rows.front().at(1) == selector);
    }
  }
}

TEST_CASE("F03009 and F03014 refuse unsigned out-of-range selectors",
          "[time-attack-terminal][F03009][F03014]") {
  fixture::Presentations host;
  for (const auto selector : {7, 127, 128, 255}) {
    CHECK_FALSE(
        r::InitializeResultsScreen(static_cast<std::uint8_t>(selector), host)
            .has_value());
    CHECK_FALSE(
        r::UpdateResultsPresentation(static_cast<std::uint8_t>(selector), host)
            .has_value());
    CHECK(host.calls.rows.empty());
  }
}

TEST_CASE(
    "F03008 init callback sees one but cannot replace the saved phase byte",
    "[time-attack-terminal][F03008]") {
  std::array<std::uint8_t, 2> bytes{0, 0xAA};
  fixture::Presentations host;
  host.caller = bytes.data();
  host.callback_write = 255;
  host.result = -5;
  CHECK(r::UpdateResultsScreen(bytes, 1, host) == 0);
  CHECK(host.calls.rows.front().at(2) == 1);
  CHECK(bytes.front() == 2);
  CHECK(bytes.back() == 0xAA);
  host.result = 0;
  CHECK(r::UpdateResultsScreen(bytes, 1, host) == 0);
  CHECK(bytes.front() == 255);
}

TEST_CASE(
    "F03008 completion clears callback writes and does not repeat child update",
    "[time-attack-terminal][F03008]") {
  std::array<std::uint8_t, 1> bytes{2};
  fixture::Presentations host;
  host.caller = bytes.data();
  host.callback_write = 19;
  host.result = 37;
  CHECK(r::UpdateResultsScreen(bytes, 2, host) == 1);
  CHECK(bytes.front() == 0);
  CHECK(r::UpdateResultsScreen(bytes, 2, host) == 0);
  CHECK(bytes.front() == 2);
  CHECK(host.calls.Tags() == Tags({Tag::kUpdate, Tag::kInitialize}));
}

TEST_CASE(
    "F03008 refuses missing state and preserves unknown outer-state no-op",
    "[time-attack-terminal][F03008]") {
  fixture::Presentations host;
  CHECK_FALSE(r::UpdateResultsScreen({}, 0, host).has_value());
  std::array<std::uint8_t, 1> bytes{255};
  CHECK(r::UpdateResultsScreen(bytes, 255, host) == 0);
  CHECK(bytes.front() == 255);
  CHECK(host.calls.rows.empty());
  bytes.front() = 0;
  CHECK_FALSE(r::UpdateResultsScreen(bytes, 255, host).has_value());
  CHECK(bytes.front() == 1);
  CHECK(host.calls.rows.empty());
}
