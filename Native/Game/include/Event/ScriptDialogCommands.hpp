#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event script's dialog opcodes: text box layout and speakers, dialog
// text and choices, and the placement of a box beside a speaking actor. Each
// handler checks its instruction's length and the box it names, then drives
// the text boxes only through ScriptDialogHost, so a mod can show the same
// dialog through any text renderer.
#include <array>
#include <cstddef>
#include <cstdint>

#include "Event/ScriptCommand.hpp"
#include "Event/ScriptStep.hpp"

namespace VS::Event {

// Instruction lengths in bytes, opcode included, from the event VM's opcode
// size table (BATTLE.PRG D_800E9C30).
inline constexpr std::size_t kInitDialogBoxLength = 11;
inline constexpr std::size_t kShowDialogTextLength = 4;
inline constexpr std::size_t kDismissDialogBoxLength = 2;
inline constexpr std::size_t kInitActorDialogBoxLength = 9;
inline constexpr std::size_t kWaitForDialogDismissalLength = 2;
inline constexpr std::size_t kSetDialogIntegerLength = 4;
inline constexpr std::size_t kSetDialogTransitionDurationLength = 2;
inline constexpr std::size_t kSelectDialogChoiceLength = 2;
inline constexpr std::size_t kSetDialogSpeakerLength = 4;

// The text boxes a script can address (Menu_textBoxes) and the integers a
// dialog string can print (vs_battle_stringContext_t integers).
inline constexpr int kTextBoxCount = 8;
inline constexpr int kDialogIntegerCount = 10;
// A speaker texture with this bit set animates no actor's face.
inline constexpr int kNoSpeakerTexture = 0x20;
inline constexpr int kSpeakerTextureMask = 0x3F;
// The speaker texture an actor dialog box shows. The original stored an
// uninitialized local here; BATTLE.PRG 0x800B6F08 masks the callee-saved
// $s6, which vs_battle_exec sets to 2 before every event VM run (#306).
inline constexpr int kActorDialogSpeakerTexture = 2;
// The script flag a dialog choice lands in.
inline constexpr int kDialogChoiceFlag = 0x10;
// The order an actor dialog box tries the four sides around the actor
// (BATTLE.PRG D_800E9BEC).
inline constexpr std::array<std::uint8_t, 4> kDialogPlacementOrder{0, 3, 1, 2};

// A box's outer size in pixels for its columns of 12-pixel glyphs and its
// 13-pixel lines.
[[nodiscard]] constexpr int DialogBoxWidth(int columns) {
  constexpr int kGlyphWidth = 12;
  constexpr int kFrameWidth = 8;
  return (columns * kGlyphWidth) + kFrameWidth;
}
[[nodiscard]] constexpr int DialogBoxHeight(int lines) {
  constexpr int kLineHeight = 13;
  constexpr int kFrameHeight = 4;
  return (lines * kLineHeight) + kFrameHeight;
}

// Script requests the commands refuse rather than overrun.
enum class ScriptDialogRefusal : std::uint8_t {
  kTextBox,        // a box id past the text boxes (#307)
  kDialogInteger,  // a string integer past the context's integers (#307)
  kShortCommand,   // an instruction shorter than its operands
};

// A text box's layout as the init commands set it.
struct TextBoxLayout {
  std::uint16_t flags = 0;
  int x = 0;
  int y = 0;
  int chars_per_line = 0;
  int line_count = 0;
  int center_x = 0;
  int center_y = 0;
};

// The part of an open text box another box can be centred on.
struct TextBoxFrame {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::uint8_t chars_per_line = 0;
  std::uint8_t line_count = 0;
};

// A box's offset from its speaker for one anchor preset and side, with the
// tail alignment and frame part that side draws.
struct DialogAnchor {
  int x = 0;
  int y = 0;
  int alignment = 0;
  int frame_part = 0;
};

// A speaking actor's dialog point on screen and its heading.
struct DialogOrigin {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t heading = 0;
};

// One candidate position for an actor's dialog box: whether it fits on
// screen as anchored, and if not, the clamped position and how far it moved.
struct DialogPlacement {
  DialogAnchor anchor;
  std::int16_t screen_x = 0;
  std::int16_t screen_y = 0;
  int clamp_distance = 0;
  bool fits = false;
};

// decomp: func_800B6B98 (src/BATTLE/BATTLE.PRG/4A0A8.c:741): places a box of
// `columns` by `lines` at `anchor` from `origin`, clamped to the screen.
[[nodiscard]] DialogPlacement CalcDialogAnchorPlacement(
    DialogOrigin origin, const DialogAnchor& anchor, int columns, int lines);

// The event state and text boxes the dialog opcodes read and change.
class ScriptDialogHost {
 public:
  ScriptDialogHost() = default;
  ScriptDialogHost(const ScriptDialogHost&) = delete;
  ScriptDialogHost& operator=(const ScriptDialogHost&) = delete;
  ScriptDialogHost(ScriptDialogHost&&) = delete;
  ScriptDialogHost& operator=(ScriptDialogHost&&) = delete;
  virtual ~ScriptDialogHost() = default;

  virtual void Refuse(ScriptDialogRefusal refusal, int value) = 0;

  // Actor references and the event VM.
  [[nodiscard]] virtual int ResolveActor(std::uint16_t reference) = 0;
  [[nodiscard]] virtual bool SkipActive() const = 0;
  [[nodiscard]] virtual DialogOrigin ActorDialogOrigin(int actor) = 0;

  // Text box layout and appearance; `box` is below kTextBoxCount.
  virtual void InitTextBox(int box, const TextBoxLayout& layout) = 0;
  [[nodiscard]] virtual TextBoxFrame Frame(int box) const = 0;
  virtual void SetSpeakerTexture(int box, int texture) = 0;
  virtual void SetSpeakerAnimation(int box, int animation) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the box fields.
  virtual void SetAnchorStyle(int box, int alignment, int frame_part) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the box fields.
  virtual void SetOpenDuration(int box, int ticks) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the box fields.
  [[nodiscard]] virtual DialogAnchor TextBoxAnchor(int preset, int orientation,
                                                   int columns, int lines) = 0;

  // Dialog text, dismissal and choices.
  [[nodiscard]] virtual bool AwaitingClose(int box) const = 0;
  virtual void SetAwaitingClose(int box, bool awaiting) = 0;
  virtual void ShowDialogString(int box, int string_id) = 0;
  [[nodiscard]] virtual int PollTextBox(int box) = 0;
  virtual void SetTextBoxSelection(int selection) = 0;
  [[nodiscard]] virtual int DismissTextBox(int box) = 0;
  [[nodiscard]] virtual bool SelectionActive() const = 0;
  virtual void SetSelectionActive(bool active) = 0;
  [[nodiscard]] virtual int SelectTextBoxChoice(std::uint8_t box_mask) = 0;

  // Script flags and the integers dialog strings print.
  [[nodiscard]] virtual std::uint8_t ScriptFlag(int id) = 0;
  virtual void SetScriptFlag(int id, int value) = 0;
  virtual void SetDialogInteger(int slot, int value) = 0;
};

// decomp: vs_battle_script_initTextBox (src/BATTLE/BATTLE.PRG/4A0A8.c:672),
// opcode 0x10: lays out a box, centred on itself or on another open box.
ScriptStep ScriptInitDialogBox(ScriptCommand command, ScriptDialogHost& host);
// decomp: vs_battle_script_showTextBox (src/BATTLE/BATTLE.PRG/4A0A8.c:707),
// opcode 0x11: shows a string, optionally holding until the box closes.
ScriptStep ScriptShowDialogText(ScriptCommand command, ScriptDialogHost& host);
// decomp: vs_battle_script_dismissTextBox
// (src/BATTLE/BATTLE.PRG/4A0A8.c:733), opcode 0x12
ScriptStep ScriptDismissDialogBox(ScriptCommand command,
                                  ScriptDialogHost& host);
// decomp: func_800B6D48 (src/BATTLE/BATTLE.PRG/4A0A8.c:794), opcode 0x13:
// opens a box on the first side of an actor that fits on screen, or on the
// side that needs the least clamping.
ScriptStep ScriptInitActorDialogBox(ScriptCommand command,
                                    ScriptDialogHost& host);
// decomp: func_800B6F8C (src/BATTLE/BATTLE.PRG/4A0A8.c:838), opcode 0x14
ScriptStep ScriptWaitForDialogDismissal(ScriptCommand command,
                                        ScriptDialogHost& host);
// decomp: vs_battle_script_setStringContextInt
// (src/BATTLE/BATTLE.PRG/4A0A8.c:916), opcode 0x1B: copies one or two script
// flags into a string integer, as a signed or unsigned byte or half.
ScriptStep ScriptSetDialogInteger(ScriptCommand command,
                                  ScriptDialogHost& host);
// decomp: func_800B7358 (src/BATTLE/BATTLE.PRG/4A0A8.c:941), opcode 0x1D
ScriptStep ScriptSetDialogTransitionDuration(ScriptCommand command,
                                             ScriptDialogHost& host);
// decomp: vs_battle_script_selectTextBox (src/BATTLE/BATTLE.PRG/4A0A8.c:947),
// opcode 0x1C: holds until the player picks a box and stores the choice.
ScriptStep ScriptSelectDialogChoice(ScriptCommand command,
                                    ScriptDialogHost& host);
// decomp: func_800B9564 (src/BATTLE/BATTLE.PRG/4A0A8.c:1897), opcode 0x5B
ScriptStep ScriptSetDialogSpeaker(ScriptCommand command,
                                  ScriptDialogHost& host);

}  // namespace VS::Event
