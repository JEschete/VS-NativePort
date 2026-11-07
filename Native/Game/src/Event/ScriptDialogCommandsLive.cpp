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

#include "Event/ScriptActorCommandsLive.hpp"
#include "Event/ScriptDialogCommands.hpp"
#include "Event/ScriptDialogOpcodes.h"
#include "Event/ScriptVmLive.hpp"
#include "State/ScriptFlagsLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/TargetingState.h"
#include "Battle/TextBoxes.h"
#include "Core/Diagnostics.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptVm.h"
#include "Legacy/Psx.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/TextBox.h"
#include "State/StateFlags.h"
}

namespace {
namespace event = VS::Event;

static_assert(std::size(Menu_textBoxes) ==
              static_cast<std::size_t>(event::kTextBoxCount));
static_assert(std::size(Event_isTextBoxAwaitingClose) >=
              static_cast<std::size_t>(event::kTextBoxCount));
static_assert(std::size(Battle_stringContext.integers) ==
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

void ClearRetainedDialogText(Menu_TextBoxState& box) noexcept {
  if (RetainedDialogText(box.string)) {
    box.string = nullptr;
  }
}

void PruneDialogOwners() noexcept {
  for (auto& box : Menu_textBoxes) {
    if (box.state == 0) {
      ClearRetainedDialogText(box);
    }
  }
  for (auto& owner : dialog_owners) {
    const bool borrowed =
        std::ranges::any_of(Menu_textBoxes, [&owner](const auto& box) {
          return ContainsDialogText(owner, box.string);
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
  Core_Warnf("event", "Native dialog string refused: box=%d string=%d", box,
             string_id);
}

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Debugf("event", "%s", message);
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
Menu_TextBoxState& Box(int box) {
  Menu_TextBoxState* state = Menu_TextBox(box);
  Require(state != nullptr, "Script dialog text box is missing");
  return *state;
}

class LiveScriptDialogHost final : public event::ScriptDialogHost {
 public:
  void Refuse(event::ScriptDialogRefusal refusal, int value) override {
    Core_Warnf("event", "Script dialog command refused %s (%d)",
               RefusalText(refusal), value);
  }

  [[nodiscard]] int ResolveActor(std::uint16_t reference) override {
    return VS::Event::ResolveLiveScriptActorReference(reference);
  }
  [[nodiscard]] bool SkipActive() const override {
    return Event_skipState == kSkipActive;
  }
  [[nodiscard]] event::DialogOrigin ActorDialogOrigin(int actor) override {
    SVECTOR anchor{};
    static_cast<void>(event::LiveActorDialogAnchor(actor, &anchor));
    return {.x = anchor.vx, .y = anchor.vy, .heading = anchor.vz};
  }

  void InitTextBox(int box, const event::TextBoxLayout& layout) override {
    ClearRetainedDialogText(Box(box));
    PruneDialogOwners();
    Menu_InitTextBox(box, layout.flags, layout.x, layout.y,
                     layout.chars_per_line, layout.line_count, layout.center_x,
                     layout.center_y);
  }
  [[nodiscard]] event::TextBoxFrame Frame(int box) const override {
    const Menu_TextBoxState& state = Box(box);
    return {.x = state.x,
            .y = state.y,
            .chars_per_line = static_cast<std::uint8_t>(state.charsPerLine),
            .line_count = static_cast<std::uint8_t>(state.lineCount)};
  }
  void SetSpeakerTexture(int box, int texture) override {
    Box(box).flags.speakerTexture =
        static_cast<unsigned>(texture) & event::kSpeakerTextureMask;
  }
  void SetSpeakerAnimation(int box, int animation) override {
    Box(box).opaque2E = static_cast<std::int16_t>(animation);
  }
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the box fields.
  void SetAnchorStyle(int box, int alignment, int frame_part) override {
    Menu_TextBoxState& state = Box(box);
    state.flags.alignment = static_cast<unsigned>(alignment) & kAlignmentMask;
    state.flags.framePart = static_cast<unsigned>(frame_part) & kFramePartMask;
  }
  void SetOpenDuration(int box, int ticks) override {
    Box(box).openDuration = static_cast<char>(ticks);
  }
  [[nodiscard]] event::DialogAnchor TextBoxAnchor(int preset, int orientation,
                                                  int columns,
                                                  int lines) override {
    const Menu_TextAnchorWord word =
        Menu_TextBoxAnchor(preset, orientation, columns, lines);
    return {.x = word.x,
            .y = word.y,
            .alignment = static_cast<int>(word.alignment),
            .frame_part = static_cast<int>(word.opaque3Bit2)};
  }

  [[nodiscard]] bool AwaitingClose(int box) const override {
    return Event_isTextBoxAwaitingClose[box] != 0;
  }
  void SetAwaitingClose(int box, bool awaiting) override {
    Event_isTextBoxAwaitingClose[box] = awaiting ? 1 : 0;
  }
  // The script's dialog strings open with a table of half-word offsets, from
  // the start of the strings, of each string.
  void ShowDialogString(int box, int string_id) override {
    const auto lease = event::LeaseLiveDialogStrings();
    if (lease) {
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
      static_cast<void>(Menu_SetTextBoxText(
          box, const_cast<char*>(reinterpret_cast<const char*>(text->data()))));
      PruneDialogOwners();
      return;
    }
    if (lease.error() != event::ScriptSourceError::kMissingOwner) {
      RefuseNativeDialog(box, string_id);
      return;
    }
    Require(Event_dialogStrings != nullptr,
            "Script dialog strings are missing");
    const std::uint16_t offset = Event_dialogStrings[string_id];
    static_cast<void>(Menu_SetTextBoxText(
        box, reinterpret_cast<char*>(&Event_dialogStrings[offset])));
  }
  [[nodiscard]] int PollTextBox(int box) override {
    return Menu_SetTextBoxText(box, nullptr);
  }
  void SetTextBoxSelection(int selection) override {
    Audio_Seq_unusedStateFlags.textBoxSelection = static_cast<char>(selection);
  }
  [[nodiscard]] int DismissTextBox(int box) override {
    const int result = Menu_DismissTextBox(box);
    PruneDialogOwners();
    return result;
  }
  [[nodiscard]] bool SelectionActive() const override {
    return Event_isTextSelectionActive != 0;
  }
  void SetSelectionActive(bool active) override {
    Event_isTextSelectionActive = active ? 1 : 0;
  }
  [[nodiscard]] int SelectTextBoxChoice(std::uint8_t box_mask) override {
    return Menu_SelectTextBoxChoice(box_mask);
  }

  [[nodiscard]] std::uint8_t ScriptFlag(int id) override {
    return VS::State::LiveScriptFlag(id);
  }
  void SetScriptFlag(int id, int value) override {
    VS::State::SetLiveScriptFlag(id, value);
  }
  void SetDialogInteger(int slot, int value) override {
    Battle_stringContext.integers[slot] = value;
  }
};

using Handler = event::ScriptStep (*)(event::ScriptCommand,
                                      event::ScriptDialogHost&);

intptr_t RunCommand(Handler handler, const u_char* command,
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
  for (auto& box : Menu_textBoxes) {
    ClearRetainedDialogText(box);
  }
  std::ranges::fill(dialog_owners, ScriptDataLease{});
}

}  // namespace VS::Event

// decomp: vs_battle_script_initTextBox (src/BATTLE/BATTLE.PRG/4A0A8.c:672),
// opcode 0x10
intptr_t Event_ScriptInitDialogBoxOpcode(u_char* command,
                                         int16_t /*thread_id*/) {
  return RunCommand(event::ScriptInitDialogBox, command,
                    event::kInitDialogBoxLength);
}

// decomp: vs_battle_script_showTextBox (src/BATTLE/BATTLE.PRG/4A0A8.c:707),
// opcode 0x11
intptr_t Event_ScriptShowDialogTextOpcode(u_char* command,
                                          int16_t /*thread_id*/) {
  return RunCommand(event::ScriptShowDialogText, command,
                    event::kShowDialogTextLength);
}

// decomp: vs_battle_script_dismissTextBox (src/BATTLE/BATTLE.PRG/4A0A8.c:733),
// opcode 0x12
intptr_t Event_ScriptDismissDialogBoxOpcode(u_char* command,
                                            int16_t /*thread_id*/) {
  return RunCommand(event::ScriptDismissDialogBox, command,
                    event::kDismissDialogBoxLength);
}

// decomp: func_800B6D48 (src/BATTLE/BATTLE.PRG/4A0A8.c:794), opcode 0x13
intptr_t Event_ScriptInitActorDialogBoxOpcode(u_char* command,
                                              int16_t /*thread_id*/) {
  return RunCommand(event::ScriptInitActorDialogBox, command,
                    event::kInitActorDialogBoxLength);
}

// decomp: func_800B6F8C (src/BATTLE/BATTLE.PRG/4A0A8.c:838), opcode 0x14
intptr_t Event_ScriptWaitForDialogDismissalOpcode(u_char* command,
                                                  int16_t /*thread_id*/) {
  return RunCommand(event::ScriptWaitForDialogDismissal, command,
                    event::kWaitForDialogDismissalLength);
}

// decomp: vs_battle_script_setStringContextInt
// (src/BATTLE/BATTLE.PRG/4A0A8.c:916), opcode 0x1B
intptr_t Event_ScriptSetDialogIntegerOpcode(u_char* command,
                                            int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetDialogInteger, command,
                    event::kSetDialogIntegerLength);
}

// decomp: func_800B7358 (src/BATTLE/BATTLE.PRG/4A0A8.c:941), opcode 0x1D
intptr_t Event_ScriptSetDialogTransitionDurationOpcode(u_char* command,
                                                       int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetDialogTransitionDuration, command,
                    event::kSetDialogTransitionDurationLength);
}

// decomp: vs_battle_script_selectTextBox (src/BATTLE/BATTLE.PRG/4A0A8.c:947),
// opcode 0x1C
intptr_t Event_ScriptSelectDialogChoiceOpcode(u_char* command,
                                              int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSelectDialogChoice, command,
                    event::kSelectDialogChoiceLength);
}

// decomp: func_800B9564 (src/BATTLE/BATTLE.PRG/4A0A8.c:1897), opcode 0x5B
intptr_t Event_ScriptSetDialogSpeakerOpcode(u_char* command,
                                            int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetDialogSpeaker, command,
                    event::kSetDialogSpeakerLength);
}
