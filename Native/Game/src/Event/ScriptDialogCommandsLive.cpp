// Hand-maintained (tools/port/owned.csv).
// Binds the typed event script dialog commands (ScriptDialogCommands.cpp) to
// the event VM's opcode slots, the menu text boxes, the script's dialog
// strings, the script flags and the string context integers.
#include "Event/ScriptDialogCommandsLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <string>

#include "Battle/TargetingState.hpp"
#include "Battle/TextBoxes.hpp"
#include "Core/Diagnostics.hpp"
#include "Event/ScriptActorCommandsLive.hpp"
#include "Event/ScriptActors.hpp"
#include "Event/ScriptDialogCommands.hpp"
#include "Event/ScriptDialogOpcodes.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Event/ScriptVmLiveState.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Framework/TextBoxes.hpp"
#include "Menu/TextBox.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "State/ScriptFlagsLive.hpp"
#include "State/StateFlags.hpp"

using VS::Core::Gte::SVECTOR;

namespace {
namespace event = VS::Event;

static_assert(std::size(VS::Menu::textBoxes) ==
              static_cast<std::size_t>(event::kTextBoxCount));
static_assert(std::size(event::isTextBoxAwaitingClose) >=
              static_cast<std::size_t>(event::kTextBoxCount));
static_assert(std::size(VS::Battle::stringContext.integers) ==
              static_cast<std::size_t>(event::kDialogIntegerCount));

constexpr char kSkipActive = 2;
constexpr unsigned kAlignmentMask = 0x3;
constexpr unsigned kFramePartMask = 0xF;
std::array<event::ScriptDataLease, event::kTextBoxCount + 1> dialog_owners{};

bool ContainsDialogText(const event::ScriptDataLease& lease,
                        const char* text) noexcept {
  const auto bytes = lease.source.Bytes();
  const auto base = reinterpret_cast<std::uintptr_t>(bytes.data());
  const auto pointer = reinterpret_cast<std::uintptr_t>(text);
  return base != 0 && pointer >= base && pointer - base < bytes.size();
}

bool RetainedDialogText(const char* text) noexcept {
  return std::ranges::any_of(dialog_owners, [text](const auto& owner) {
    return ContainsDialogText(owner, text);
  });
}

void ClearRetainedDialogText(VS::Menu::TextBoxState& box) noexcept {
  if (RetainedDialogText(box.text)) {
    box.text = nullptr;
  }
}

void PruneDialogOwners() noexcept {
  for (auto& box : VS::Menu::textBoxes) {
    if (box.state == 0) {
      ClearRetainedDialogText(box);
    }
  }
  for (auto& owner : dialog_owners) {
    const bool borrowed =
        std::ranges::any_of(VS::Menu::textBoxes, [&owner](const auto& box) {
          return ContainsDialogText(owner, box.text);
        });
    if (!borrowed) {
      owner = {};
    }
  }
}

bool RetainDialogOwner(const event::ScriptDataLease& lease) {
  PruneDialogOwners();
  const auto bytes = lease.source.Bytes();
  for (auto& owner : dialog_owners) {
    const auto retained = owner.source.Bytes();
    if (retained.data() == bytes.data() && retained.size() == bytes.size()) {
      return true;
    }
  }
  for (auto& owner : dialog_owners) {
    if (owner.source.Bytes().empty()) {
      owner = lease;
      return true;
    }
  }
  return false;
}

void RefuseNativeDialog(int box, int string_id) {
  VS::Core::Warnf("event", "Native dialog string refused: box=%d string=%d",
                  box, string_id);
}

void Require(bool valid, const char* message) {
  if (!valid) {
    VS::Core::Errorf("event", "%s", message);
    std::abort();
  }
}

const char* RefusalText(event::ScriptDialogRefusal refusal) {
  switch (refusal) {
    case event::ScriptDialogRefusal::kTextBox:
      return "a text box past the menu's boxes";
    case event::ScriptDialogRefusal::kDialogInteger:
      return "a string integer past the string context";
    case event::ScriptDialogRefusal::kShortCommand:
      return "an instruction shorter than its operands";
  }
  return "an unknown request";
}

// The core checks every box against kTextBoxCount before it reaches the host.
VS::Menu::TextBoxState& Box(int box) {
  VS::Menu::TextBoxState* state = VS::Menu::LiveTextBox(box);
  Require(state != nullptr, "Script dialog text box is missing");
  return *state;
}

class LiveScriptDialogHost final : public event::ScriptDialogHost {
 public:
  void Refuse(event::ScriptDialogRefusal refusal, int value) override {
    VS::Core::Warnf("event", "Script dialog command refused %s (%d)",
                    RefusalText(refusal), value);
  }

  [[nodiscard]] int ResolveActor(std::uint16_t reference) override {
    return VS::Event::ResolveLiveScriptActorReference(reference);
  }
  [[nodiscard]] bool SkipActive() const override {
    return event::skipState == kSkipActive;
  }
  [[nodiscard]] event::DialogOrigin ActorDialogOrigin(int actor) override {
    SVECTOR anchor{};
    static_cast<void>(event::LiveActorDialogAnchor(actor, &anchor));
    return {.x = anchor.vx, .y = anchor.vy, .heading = anchor.vz};
  }

  void InitTextBox(int box, const event::TextBoxLayout& layout) override {
    ClearRetainedDialogText(Box(box));
    PruneDialogOwners();
    VS::Menu::InitLiveTextBox(box, layout.flags, layout.x, layout.y,
                              layout.chars_per_line, layout.line_count,
                              layout.center_x, layout.center_y);
  }
  [[nodiscard]] event::TextBoxFrame Frame(int box) const override {
    const VS::Menu::TextBoxState& state = Box(box);
    return {.x = state.x,
            .y = state.y,
            .chars_per_line = static_cast<std::uint8_t>(state.chars_per_line),
            .line_count = static_cast<std::uint8_t>(state.line_count)};
  }
  void SetSpeakerTexture(int box, int texture) override {
    Box(box).flags.speaker_texture =
        static_cast<unsigned>(texture) & event::kSpeakerTextureMask;
  }
  void SetSpeakerAnimation(int box, int animation) override {
    Box(box).speaker_animation = static_cast<std::int16_t>(animation);
  }
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the box fields.
  void SetAnchorStyle(int box, int alignment, int frame_part) override {
    VS::Menu::TextBoxState& state = Box(box);
    state.flags.alignment = static_cast<unsigned>(alignment) & kAlignmentMask;
    state.flags.frame_part = static_cast<unsigned>(frame_part) & kFramePartMask;
  }
  void SetOpenDuration(int box, int ticks) override {
    Box(box).open_duration = static_cast<std::int8_t>(ticks);
  }
  [[nodiscard]] event::DialogAnchor TextBoxAnchor(int preset, int orientation,
                                                  int columns,
                                                  int lines) override {
    const VS::Menu::TextAnchor anchor =
        VS::Menu::LiveTextBoxAnchor(preset, orientation, columns, lines);
    return {.x = anchor.x,
            .y = anchor.y,
            .alignment = anchor.alignment,
            .frame_part = anchor.frame_part};
  }

  [[nodiscard]] bool AwaitingClose(int box) const override {
    return event::isTextBoxAwaitingClose[box] != 0;
  }
  void SetAwaitingClose(int box, bool awaiting) override {
    event::isTextBoxAwaitingClose[box] = awaiting ? 1 : 0;
  }
  // The script's dialog strings open with a table of half-word offsets, from
  // the start of the strings, of each string.
  void ShowDialogString(int box, int string_id) override {
    const auto lease = event::LeaseLiveDialogStrings();
    if (!lease) {
      RefuseNativeDialog(box, string_id);
      return;
    }
    const auto bytes = lease->Bytes();
    if (!bytes) {
      RefuseNativeDialog(box, string_id);
      return;
    }
    const auto text = event::ResolveDialogString(*bytes, string_id);
    if (!text || !RetainDialogOwner(*lease)) {
      RefuseNativeDialog(box, string_id);
      return;
    }
    static_cast<void>(VS::Menu::SetLiveTextBoxText(
        box, const_cast<char*>(reinterpret_cast<const char*>(text->data()))));
    PruneDialogOwners();
  }
  [[nodiscard]] int PollTextBox(int box) override {
    return VS::Menu::SetLiveTextBoxText(box, nullptr);
  }
  void SetTextBoxSelection(int selection) override {
    VS::State::stateFlags.text_box_selection = static_cast<char>(selection);
  }
  [[nodiscard]] int DismissTextBox(int box) override {
    const int result = VS::Menu::DismissLiveTextBox(box);
    PruneDialogOwners();
    return result;
  }
  [[nodiscard]] bool SelectionActive() const override {
    return event::isTextSelectionActive != 0;
  }
  void SetSelectionActive(bool active) override {
    event::isTextSelectionActive = active ? 1 : 0;
  }
  [[nodiscard]] int SelectTextBoxChoice(std::uint8_t box_mask) override {
    return VS::Menu::SelectLiveTextBoxChoice(box_mask);
  }

  [[nodiscard]] std::uint8_t ScriptFlag(int id) override {
    return VS::State::LiveScriptFlag(id);
  }
  void SetScriptFlag(int id, int value) override {
    VS::State::SetLiveScriptFlag(id, value);
  }
  void SetDialogInteger(int slot, int value) override {
    VS::Battle::stringContext.integers[slot] = value;
  }
};

using Handler = event::ScriptStep (*)(event::ScriptCommand,
                                      event::ScriptDialogHost&);

intptr_t RunCommand(Handler handler, const std::uint8_t* command,
                    std::size_t length) {
  Require(command != nullptr, "Script dialog command is missing");
  LiveScriptDialogHost host;
  return static_cast<intptr_t>(
      handler(event::ScriptCommand{command, length}, host));
}
}  // namespace

namespace VS::Event {

void PruneLiveScriptDialogText() noexcept { PruneDialogOwners(); }

void ReleaseLiveScriptDialogText() noexcept {
  for (auto& box : VS::Menu::textBoxes) {
    ClearRetainedDialogText(box);
  }
  std::ranges::fill(dialog_owners, ScriptDataLease{});
}

}  // namespace VS::Event

// decomp: vs_battle_script_initTextBox (src/BATTLE/BATTLE.PRG/4A0A8.c:672),
// opcode 0x10
std::intptr_t VS::Event::ScriptInitDialogBoxOpcode(std::uint8_t* command,
                                                   std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptInitDialogBox, command,
                    event::kInitDialogBoxLength);
}

// decomp: vs_battle_script_showTextBox (src/BATTLE/BATTLE.PRG/4A0A8.c:707),
// opcode 0x11
std::intptr_t VS::Event::ScriptShowDialogTextOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptShowDialogText, command,
                    event::kShowDialogTextLength);
}

// decomp: vs_battle_script_dismissTextBox (src/BATTLE/BATTLE.PRG/4A0A8.c:733),
// opcode 0x12
std::intptr_t VS::Event::ScriptDismissDialogBoxOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptDismissDialogBox, command,
                    event::kDismissDialogBoxLength);
}

// decomp: func_800B6D48 (src/BATTLE/BATTLE.PRG/4A0A8.c:794), opcode 0x13
std::intptr_t VS::Event::ScriptInitActorDialogBoxOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptInitActorDialogBox, command,
                    event::kInitActorDialogBoxLength);
}

// decomp: func_800B6F8C (src/BATTLE/BATTLE.PRG/4A0A8.c:838), opcode 0x14
std::intptr_t VS::Event::ScriptWaitForDialogDismissalOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptWaitForDialogDismissal, command,
                    event::kWaitForDialogDismissalLength);
}

// decomp: vs_battle_script_setStringContextInt
// (src/BATTLE/BATTLE.PRG/4A0A8.c:916), opcode 0x1B
std::intptr_t VS::Event::ScriptSetDialogIntegerOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetDialogInteger, command,
                    event::kSetDialogIntegerLength);
}

// decomp: func_800B7358 (src/BATTLE/BATTLE.PRG/4A0A8.c:941), opcode 0x1D
std::intptr_t VS::Event::ScriptSetDialogTransitionDurationOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetDialogTransitionDuration, command,
                    event::kSetDialogTransitionDurationLength);
}

// decomp: vs_battle_script_selectTextBox (src/BATTLE/BATTLE.PRG/4A0A8.c:947),
// opcode 0x1C
std::intptr_t VS::Event::ScriptSelectDialogChoiceOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSelectDialogChoice, command,
                    event::kSelectDialogChoiceLength);
}

// decomp: func_800B9564 (src/BATTLE/BATTLE.PRG/4A0A8.c:1897), opcode 0x5B
std::intptr_t VS::Event::ScriptSetDialogSpeakerOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetDialogSpeaker, command,
                    event::kSetDialogSpeakerLength);
}
