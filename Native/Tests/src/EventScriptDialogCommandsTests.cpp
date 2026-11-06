#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Event/ScriptDialogCommands.hpp"

namespace {
namespace event = VS::Event;
using event::ScriptDialogRefusal;
using event::ScriptStep;
using Calls = std::vector<std::string>;
using Refusal = std::pair<ScriptDialogRefusal, int>;
using Bytes = std::vector<std::uint8_t>;

class RecordingHost final : public event::ScriptDialogHost {
 public:
  Calls calls;
  std::vector<Refusal> refusals;
  bool skip = false;
  event::DialogOrigin origin;
  std::map<int, event::DialogAnchor> anchors;
  std::array<event::TextBoxFrame, event::kTextBoxCount> frames{};
  std::array<bool, event::kTextBoxCount> awaiting{};
  int poll = 0;
  int dismiss = 0;
  bool selection_active = false;
  int choice = 0;
  std::map<int, std::uint8_t> flags;

  template <typename... Values>
  void Log(std::string_view name, const Values&... values) {
    std::string line(name);
    ((line += std::format(" {}", values)), ...);
    calls.push_back(std::move(line));
  }

  void Refuse(ScriptDialogRefusal refusal, int value) override {
    refusals.emplace_back(refusal, value);
  }
  [[nodiscard]] int ResolveActor(std::uint16_t reference) override {
    return reference + 1;
  }
  [[nodiscard]] bool SkipActive() const override { return skip; }
  [[nodiscard]] event::DialogOrigin ActorDialogOrigin(int actor) override {
    Log("Origin", actor);
    return origin;
  }
  void InitTextBox(int box, const event::TextBoxLayout& layout) override {
    Log("Init", box, layout.flags, layout.x, layout.y, layout.chars_per_line,
        layout.line_count, layout.center_x, layout.center_y);
  }
  [[nodiscard]] event::TextBoxFrame Frame(int box) const override {
    return frames.at(static_cast<std::size_t>(box));
  }
  void SetSpeakerTexture(int box, int texture) override {
    Log("Texture", box, texture);
  }
  void SetSpeakerAnimation(int box, int animation) override {
    Log("Animation", box, animation);
  }
  void SetAnchorStyle(int box, int alignment, int frame_part) override {
    Log("Style", box, alignment, frame_part);
  }
  void SetOpenDuration(int box, int ticks) override {
    Log("Duration", box, ticks);
  }
  [[nodiscard]] event::DialogAnchor TextBoxAnchor(int preset, int orientation,
                                                  int columns,
                                                  int lines) override {
    Log("Anchor", preset, orientation, columns, lines);
    return anchors[orientation];
  }
  [[nodiscard]] bool AwaitingClose(int box) const override {
    return awaiting.at(static_cast<std::size_t>(box));
  }
  void SetAwaitingClose(int box, bool value) override {
    awaiting.at(static_cast<std::size_t>(box)) = value;
  }
  void ShowDialogString(int box, int string_id) override {
    Log("Show", box, string_id);
  }
  [[nodiscard]] int PollTextBox(int /*box*/) override { return poll; }
  void SetTextBoxSelection(int selection) override {
    Log("Selection", selection);
  }
  [[nodiscard]] int DismissTextBox(int box) override {
    Log("Dismiss", box);
    return dismiss;
  }
  [[nodiscard]] bool SelectionActive() const override {
    return selection_active;
  }
  void SetSelectionActive(bool active) override { selection_active = active; }
  [[nodiscard]] int SelectTextBoxChoice(std::uint8_t box_mask) override {
    Log("Choice", box_mask);
    return choice;
  }
  [[nodiscard]] std::uint8_t ScriptFlag(int id) override { return flags[id]; }
  void SetScriptFlag(int id, int value) override { Log("Flag", id, value); }
  void SetDialogInteger(int slot, int value) override {
    Log("Integer", slot, value);
  }
};

using Handler = ScriptStep (*)(event::ScriptCommand, event::ScriptDialogHost&);

ScriptStep Run(Handler handler, const Bytes& bytes, RecordingHost& host) {
  return handler(event::ScriptCommand{bytes}, host);
}

struct Command {
  Handler handler;
  std::size_t length;
};

constexpr std::array kCommands{
    Command{event::ScriptInitDialogBox, event::kInitDialogBoxLength},
    Command{event::ScriptShowDialogText, event::kShowDialogTextLength},
    Command{event::ScriptDismissDialogBox, event::kDismissDialogBoxLength},
    Command{event::ScriptInitActorDialogBox, event::kInitActorDialogBoxLength},
    Command{event::ScriptWaitForDialogDismissal,
            event::kWaitForDialogDismissalLength},
    Command{event::ScriptSetDialogInteger, event::kSetDialogIntegerLength},
    Command{event::ScriptSetDialogTransitionDuration,
            event::kSetDialogTransitionDurationLength},
    Command{event::ScriptSelectDialogChoice, event::kSelectDialogChoiceLength},
    Command{event::ScriptSetDialogSpeaker, event::kSetDialogSpeakerLength},
};

// An actor dialog box command: box 1, actor reference 0x0005, flags 0x0302
// whose high byte names anchor preset 3, 10 columns, 2 lines and speaker
// animation 6.
Bytes ActorBoxCommand() { return {0x13, 1, 5, 0, 0x02, 0x03, 10, 2, 6}; }

// A placement's fit, screen position and clamp distance.
std::array<int, 4> Summary(const event::DialogPlacement& placement) {
  return {placement.fits ? 1 : 0, placement.screen_x, placement.screen_y,
          placement.clamp_distance};
}
}  // namespace

TEST_CASE("Event script dialog commands refuse short instructions",
          "[event-script-dialog]") {
  for (const Command& command : kCommands) {
    RecordingHost host;
    const Bytes bytes(command.length - 1, 0x01);
    CHECK(Run(command.handler, bytes, host) == ScriptStep::kAdvance);
    CHECK(host.calls.empty());
    REQUIRE(host.refusals.size() == 1);
    CHECK(host.refusals[0] == Refusal{ScriptDialogRefusal::kShortCommand,
                                      static_cast<int>(command.length - 1)});
  }
}

// #307: the original indexed the text boxes and the string integers with
// the script's bytes.
TEST_CASE("Event script dialog commands refuse boxes past the text boxes",
          "[event-script-dialog]") {
  const std::array<std::pair<Handler, Bytes>, 8> commands{{
      {event::ScriptInitDialogBox, {0x10, 0x09, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
      {event::ScriptInitDialogBox, {0x10, 0x91, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
      {event::ScriptShowDialogText, {0x11, 8, 0, 0}},
      {event::ScriptDismissDialogBox, {0x12, 0xFF}},
      {event::ScriptInitActorDialogBox, {0x13, 8, 0, 0, 0, 0, 0, 0, 0}},
      {event::ScriptWaitForDialogDismissal, {0x14, 12}},
      {event::ScriptSetDialogSpeaker, {0x5B, 0x0F, 0, 0}},
      {event::ScriptSetDialogInteger, {0x1B, 10, 0, 0}},
  }};
  for (const auto& [handler, bytes] : commands) {
    RecordingHost host;
    CHECK(Run(handler, bytes, host) == ScriptStep::kAdvance);
    CHECK(host.calls.empty());
    REQUIRE(host.refusals.size() == 1);
    const bool integer = bytes[0] == 0x1B;
    const int value = bytes[0] == 0x10 && bytes[1] == 0x91   ? 9
                      : bytes[0] == 0x10 || bytes[0] == 0x5B ? bytes[1] & 0xF
                                                             : bytes[1];
    CHECK(host.refusals[0] == Refusal{integer
                                          ? ScriptDialogRefusal::kDialogInteger
                                          : ScriptDialogRefusal::kTextBox,
                                      value});
  }
}

TEST_CASE("Event script dialog boxes centre on themselves or an open box",
          "[event-script-dialog]") {
  RecordingHost host;
  host.frames[2] = {.x = 40, .y = 60, .chars_per_line = 4, .line_count = 3};
  // Box 3 centred on itself at script (60, 52): 16 columns, 2 lines.
  CHECK(Run(event::ScriptInitDialogBox,
            {0x10, 0x33, 0x34, 0x12, 60, 52, 16, 2, 0x00, 0x20, 7},
            host) == ScriptStep::kAdvance);
  // Box 1 centred on box 2, with an actor speaker past the reference range
  // as a signed half, which the original resolved.
  CHECK(Run(event::ScriptInitDialogBox,
            {0x10, 0x21, 0, 0, 20, 40, 8, 1, 0xFF, 0xFF, 1},
            host) == ScriptStep::kAdvance);
  CHECK(Run(event::ScriptInitDialogBox,
            {0x10, 0x00, 0, 0, 20, 40, 8, 1, 0x41, 0x00, 1},
            host) == ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"Init 3 4660 88 20 16 2 188 35", "Animation 3 7",
                            "Init 1 0 8 8 8 1 68 81", "Animation 1 1",
                            "Texture 1 0", "Init 0 0 8 8 8 1 60 16",
                            "Animation 0 1", "Texture 0 2"});
  CHECK(host.refusals.empty());
}

TEST_CASE("Event script dialog speakers name an actor's face or none",
          "[event-script-dialog]") {
  RecordingHost host;
  CHECK(Run(event::ScriptSetDialogSpeaker, {0x5B, 0x52, 0x41, 0x00}, host) ==
        ScriptStep::kAdvance);
  CHECK(Run(event::ScriptSetDialogSpeaker, {0x5B, 0x13, 0x00, 0x20}, host) ==
        ScriptStep::kAdvance);
  CHECK(Run(event::ScriptSetDialogTransitionDuration, {0x1D, 0xE5}, host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"Texture 2 2", "Animation 2 5", "Texture 3 32",
                            "Animation 3 1", "Duration 7 5"});
}

TEST_CASE("Event script dialog text holds until its box closes",
          "[event-script-dialog]") {
  RecordingHost host;
  CHECK(Run(event::ScriptShowDialogText, {0x11, 2, 9, 1}, host) ==
        ScriptStep::kAdvance);
  CHECK_FALSE(host.awaiting[2]);
  CHECK(Run(event::ScriptShowDialogText, {0x11, 2, 9, 0}, host) ==
        ScriptStep::kHold);
  CHECK(host.awaiting[2]);
  host.poll = -1;
  CHECK(Run(event::ScriptShowDialogText, {0x11, 2, 9, 0}, host) ==
        ScriptStep::kHold);
  host.poll = 3;
  CHECK(Run(event::ScriptShowDialogText, {0x11, 2, 9, 0}, host) ==
        ScriptStep::kAdvance);
  CHECK_FALSE(host.awaiting[2]);
  CHECK(host.calls == Calls{"Show 2 9", "Show 2 9", "Selection 3"});
}

TEST_CASE("Event script dialog text closes without a choice on a zero poll",
          "[event-script-dialog]") {
  RecordingHost host;
  host.awaiting[4] = true;
  CHECK(Run(event::ScriptShowDialogText, {0x11, 4, 1, 0}, host) ==
        ScriptStep::kAdvance);
  CHECK_FALSE(host.awaiting[4]);
  CHECK(host.calls.empty());
}

TEST_CASE("Event script dialog dismissal waits hold until the box closes",
          "[event-script-dialog]") {
  RecordingHost host;
  host.dismiss = 1;
  CHECK(Run(event::ScriptWaitForDialogDismissal, {0x14, 4}, host) ==
        ScriptStep::kHold);
  host.dismiss = 0;
  CHECK(Run(event::ScriptWaitForDialogDismissal, {0x14, 4}, host) ==
        ScriptStep::kAdvance);
  CHECK(Run(event::ScriptDismissDialogBox, {0x12, 5}, host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"Dismiss 4", "Dismiss 4", "Dismiss 5"});
}

TEST_CASE("Event script actor dialog boxes take the first side that fits",
          "[event-script-dialog]") {
  RecordingHost host;
  host.origin = {.x = 100, .y = 100, .heading = 0};
  // Heading 0 faces quadrant 3, so the sides run orientations 12, 0, 8, 4.
  host.anchors[12] = {.x = 250, .y = 0, .alignment = 1, .frame_part = 2};
  host.anchors[0] = {.x = 0, .y = -200, .alignment = 2, .frame_part = 3};
  host.anchors[8] = {.x = -50, .y = -50, .alignment = 3, .frame_part = 9};
  CHECK(Run(event::ScriptInitActorDialogBox, ActorBoxCommand(), host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"Origin 6", "Anchor 3 12 10 2", "Anchor 3 0 10 2",
                            "Anchor 3 8 10 2", "Init 1 770 50 50 10 2 114 65",
                            "Texture 1 2", "Style 1 3 9", "Animation 1 6"});
}

TEST_CASE("Event script actor dialog boxes otherwise clamp the least",
          "[event-script-dialog]") {
  RecordingHost host;
  host.origin = {.x = 100, .y = 100, .heading = 0x400};
  // Heading 0x400 faces quadrant 0: orientations 0, 12, 4, 8.
  host.anchors[0] = {.x = 250, .y = 0, .alignment = 1, .frame_part = 1};
  host.anchors[12] = {.x = 0, .y = -200, .alignment = 2, .frame_part = 2};
  host.anchors[4] = {.x = -120, .y = -50, .alignment = 3, .frame_part = 3};
  host.anchors[8] = {.x = -50, .y = -130, .alignment = 0, .frame_part = 4};
  CHECK(Run(event::ScriptInitActorDialogBox, ActorBoxCommand(), host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"Origin 6", "Anchor 3 0 10 2", "Anchor 3 12 10 2",
                            "Anchor 3 4 10 2", "Anchor 3 8 10 2",
                            "Init 1 770 4 50 10 2 68 65", "Texture 1 2",
                            "Style 1 3 3", "Animation 1 6"});
}

TEST_CASE("Event script dialog placement clamps a box onto the screen",
          "[event-script-dialog]") {
  const event::DialogOrigin origin{.x = 300, .y = 210, .heading = 0};
  CHECK(Summary(event::CalcDialogAnchorPlacement(
            {.x = 10, .y = 10, .heading = 0}, {.x = 0, .y = 0}, 4, 1)) ==
        std::array{1, 10, 10, 0});
  // 56 by 17 pixels at (300, 210): clamped to (264, 199).
  CHECK(Summary(event::CalcDialogAnchorPlacement(
            origin, {.x = 0, .y = 0}, 4, 1)) == std::array{0, 264, 199, 37});
  CHECK(Summary(event::CalcDialogAnchorPlacement(
            origin, {.x = -330, .y = -213}, 4, 1)) == std::array{0, 4, 4, 34});
}

TEST_CASE("Event script dialog integers read script flags in four formats",
          "[event-script-dialog]") {
  RecordingHost host;
  host.flags[0x123] = 0xF0;
  host.flags[0x124] = 0xFF;
  CHECK(Run(event::ScriptSetDialogInteger, {0x1B, 0, 0x23, 0x01}, host) ==
        ScriptStep::kAdvance);
  CHECK(Run(event::ScriptSetDialogInteger, {0x1B, 1, 0x23, 0x41}, host) ==
        ScriptStep::kAdvance);
  CHECK(Run(event::ScriptSetDialogInteger, {0x1B, 2, 0x23, 0x81}, host) ==
        ScriptStep::kAdvance);
  CHECK(Run(event::ScriptSetDialogInteger, {0x1B, 9, 0x23, 0xC1}, host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"Integer 0 240", "Integer 1 -16", "Integer 2 65520",
                            "Integer 9 -16"});
}

TEST_CASE("Event script dialog choices hold until the player picks",
          "[event-script-dialog]") {
  RecordingHost host;
  host.skip = true;
  CHECK(Run(event::ScriptSelectDialogChoice, {0x1C, 0x06}, host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls.empty());
  host.skip = false;
  CHECK(Run(event::ScriptSelectDialogChoice, {0x1C, 0x06}, host) ==
        ScriptStep::kHold);
  CHECK(host.selection_active);
  CHECK(Run(event::ScriptSelectDialogChoice, {0x1C, 0x06}, host) ==
        ScriptStep::kHold);
  host.choice = 2;
  CHECK(Run(event::ScriptSelectDialogChoice, {0x1C, 0x06}, host) ==
        ScriptStep::kAdvance);
  CHECK_FALSE(host.selection_active);
  CHECK(host.calls == Calls{"Choice 6", "Choice 6", "Choice 6", "Flag 16 2"});
}
