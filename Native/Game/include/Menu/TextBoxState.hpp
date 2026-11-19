#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// The menu text boxes' state, selection, anchors and lifecycle (BATTLE.PRG
// 5BF94.c): the text-box consumer ABI. Eight boxes are drawn by
// UpdateTextBoxes; these functions open, fill, select, anchor and
// dismiss them without reaching global state (TextBoxStateLive.hpp binds
// them to textBoxes and textBoxSelector).
namespace VS::Menu {
inline constexpr std::size_t kTextBoxCount = 8;
// The information box (kind 4) the menus share.
inline constexpr std::size_t kInformationTextBox = 7;
// Speaker texture bit 5 set: the box has no speaking actor to animate.
inline constexpr std::uint8_t kNoTextBoxSpeaker = 0x20;
// The speed a finished page leaves; 0 is a box that never printed.
inline constexpr std::uint16_t kTextBoxFinishedSpeed = 0xFFFF;
// Brightness of the selected and the other choices of a selection.
inline constexpr std::uint8_t kSelectedChoiceBrightness = 0x80;
inline constexpr std::uint8_t kOtherChoiceBrightness = 0x40;
// Anchor presets addressable by the script's 4-bit preset field.
inline constexpr std::size_t kTextAnchorPresetCount = 16;

// decomp: informationTextBoxHeader (src/BATTLE/BATTLE.PRG/battleUi.h:188):
// the header an information box (kInformationTextBox) draws above its text.
enum class InformationHeader : std::uint8_t {
  kNone = 0,
  kCaution = 1,
  kInformation = 2,
  kCheck = 3,
};

// The box kinds UpdateTextBoxes draws (5BF94.c:3936) and the two
// aliases InitTextBox rewrites to a shaped frame.
enum class TextBoxKind : std::uint8_t {
  kPlain = 0,
  kShapedFrameAlias = 1,
  kShapedFrameLegacy = 2,
  kHidden = 3,
  kInformation = 4,
  kSecondShapedFrameAlias = 5,
  kBlendOnly = 6,
  kShapedFrame = 7,
};

// vs_battle_textBox_fields (5BF94.h:93), one field per bit-field. Values
// wider than their bit-field are truncated when packed.
struct TextBoxFlags {
  std::uint8_t kind = 0;             // bits 0-3
  std::uint8_t open_style = 0;       // bits 4-5: index of the open duration
  std::uint8_t alignment = 0;        // bits 6-7
  std::uint8_t frame_shape = 0;      // bits 8-11 (information header + 1)
  std::uint8_t frame_part = 0;       // bits 12-15
  std::uint8_t arrow_frame = 0;      // bits 16-23
  std::uint8_t speaker_texture = 0;  // bits 24-29
  bool done = false;                 // bit 30
  bool frozen = false;               // bit 31: UpdateTextBoxes skips it

  [[nodiscard]] static TextBoxFlags FromWord(std::uint32_t word);
  [[nodiscard]] std::uint32_t Word() const;
  [[nodiscard]] bool HasSpeaker() const;
  friend bool operator==(const TextBoxFlags&, const TextBoxFlags&) = default;
};

// vs_battle_textBox (5BF94.h:121) with native field types. `text` borrows
// the encoded string the box prints; the box never owns or writes it.
struct TextBox {
  TextBoxFlags flags{};
  std::int16_t opaque4 = 0;
  std::int16_t opaque6 = 0;
  const char* text = nullptr;
  std::uint16_t speed = 0;
  std::uint16_t previous_chunks = 0;
  std::int16_t x_indent = 0;
  std::int16_t y_indent = 0;
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t center_x = 0;
  std::int16_t center_y = 0;
  // 0 closed; 1..open_duration + 1 opening then open; negative closing.
  std::int16_t state = 0;
  std::uint8_t brightness = 0;
  std::uint8_t reserved1f = 0;
  std::int8_t chars_per_line = 0;
  std::int8_t line_count = 0;
  std::int8_t open_duration = 0;
  std::int8_t reserved23 = 0;
  std::int16_t frame_x = 0;
  std::int16_t frame_y = 0;
  std::int16_t frame_width = 0;
  std::int16_t frame_height = 0;
  // The speaker's mouth animation counter and its texture timeline.
  std::int16_t speaker_pause = 0;
  std::int16_t speaker_animation = 0;

  friend bool operator==(const TextBox&, const TextBox&) = default;
};

using TextBoxes = std::span<TextBox, kTextBoxCount>;

// _textBoxSelector_t (5BF94.c:186): the selectable boxes in screen order and
// the highlighted position. count 0 means the next call starts a selection.
struct TextBoxSelector {
  std::array<std::uint8_t, kTextBoxCount> order{};
  std::uint8_t selected = 0;
  std::uint8_t count = 0;

  friend bool operator==(const TextBoxSelector&,
                         const TextBoxSelector&) = default;
};

// What a selection reads from the frame.
struct TextBoxSelectionInput {
  // Buttons newly pressed this frame (vs_main_buttonsPressed.all).
  std::uint32_t pressed = 0;
  // One-based choice to highlight first (vs_main_stateFlags byte 0x11); 0
  // or past the choices highlights the first.
  std::uint8_t default_choice = 0;
};

// vs_battle_initTextBox's arguments, as the original's words.
struct TextBoxLayout {
  std::int32_t flags = 0;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t chars_per_line = 0;
  std::int32_t line_count = 0;
  std::int32_t center_x = 0;
  std::int32_t center_y = 0;
};

// func_800CCE10_t (5BF94.c:307): a packed anchor, 12-bit signed x and y.
struct TextAnchor {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::uint8_t alignment = 0;   // 2 bits
  std::uint8_t frame_part = 0;  // 4 bits

  [[nodiscard]] static TextAnchor FromWord(std::uint32_t word);
  // The low 30 bits; the original leaves the top two bits as the caller's
  // stale register bits, which no reader uses.
  [[nodiscard]] std::uint32_t Word() const;
  friend bool operator==(const TextAnchor&, const TextAnchor&) = default;
};

// D_800EC284: per preset, the anchor for the primary and secondary facing.
struct TextAnchorPreset {
  TextAnchor primary{};
  TextAnchor secondary{};
};

// func_800CCE10's arguments: the preset, the speaker's orientation flags and
// the text size in characters and lines.
struct TextAnchorRequest {
  std::int32_t preset = 0;
  std::int32_t orientation = 0;
  std::int32_t columns = 0;
  std::int32_t lines = 0;
};

class TextBoxHost {
 public:
  virtual ~TextBoxHost() = default;
  // func_800AAD4C(speaker, 0, timeline, command): the speaking actor's
  // texture animation channel 0.
  virtual void AnimateSpeaker(std::uint8_t speaker, std::int32_t timeline,
                              std::int32_t command) = 0;
  virtual void PlaySelectSound() = 0;
  virtual void PlayChangeSound() = 0;
};

// Starts printing `text` (non-null) from its first page and starts the
// speaker's talking animation (timeline speaker_animation + 1) unless the box
// has no speaker. decomp: F01329 vs_battle_setTextBox
// (src/BATTLE/BATTLE.PRG/5BF94.c:1536, 0x800C6BF0-0x800C6C8C).
void StartTextBoxText(TextBox& box, TextBoxHost& host, const char* text);
// The text is still printing: speed is neither 0 nor finished.
[[nodiscard]] bool TextBoxTextPending(const TextBox& box);
// The original dispatch: a non-null text starts printing and returns 0; null
// polls, returning -1 while printing and 0 otherwise. decomp: F01329.
int SetTextBoxText(TextBox& box, TextBoxHost& host, const char* text);

// One frame of a choice between the boxes in `box_mask`. The first call
// orders the boxes by y (ties and boxes at y 0x7FFF resolve to the lowest
// index, as in the original) and highlights default_choice; later calls move
// with up/down (wrapping) and return the one-based position on any face or
// shoulder button. Each frame the highlighted box's contents move to the
// highest box index of the selection (drawn last) and the boxes' brightness
// is set. Returns 0 while choosing. An empty mask never confirms: the
// selector stays at count 0 and every call restarts it and returns 0.
// A confirm still returns from any started selector. A move refuses (nullopt,
// nothing changed) a selector whose count exceeds the eight boxes or whose
// used order entries name no box; the original read past them.
// decomp: F01330 vs_battle_selectTextBox
// (src/BATTLE/BATTLE.PRG/5BF94.c:1559, 0x800C6C8C-0x800C7010).
[[nodiscard]] std::optional<int> SelectTextBoxChoice(
    TextBoxes boxes, TextBoxSelector& selector, TextBoxHost& host,
    std::uint8_t box_mask, const TextBoxSelectionInput& input);

// The layout vs_battle_initInformationTextBox passes: box 7, kind 4 with
// the header in frame_shape, x 0, y 1, four lines.
[[nodiscard]] TextBoxLayout InformationTextBoxLayout(std::int32_t header);
// decomp: F01343 vs_battle_initInformationTextBox
// (src/BATTLE/BATTLE.PRG/5BF94.c:2653, 0x800C8E04-0x800C8E48).
void InitInformationTextBox(TextBoxes boxes, std::int32_t header);

// Starts closing every box in index order; true when all eight were already
// closed. decomp: F01378 _dismissAllTextBoxes
// (src/BATTLE/BATTLE.PRG/5BF94.c:3757, 0x800CB268-0x800CB2B8).
[[nodiscard]] bool DismissAllTextBoxes(TextBoxes boxes, TextBoxHost& host);

// The box index for a box id, refusing ids past the eight boxes (the
// original returned an address past the array). decomp: F01409
// vs_battle_getTextBox (src/BATTLE/BATTLE.PRG/5BF94.c:4544,
// 0x800CCDF4-0x800CCE10).
[[nodiscard]] std::optional<std::size_t> TextBoxIndex(std::int32_t id);

// The anchor of a text box of `columns` x `lines` around its speaker: the
// preset's secondary anchor for facings 1 and 2 (orientation bits 2-3),
// mirrored in x with alignment bit 1 flipped for facings 1 and 3, less half
// the box size, wrapped to 12 bits. Refuses a preset past `presets`.
// decomp: F01410 func_800CCE10 (src/BATTLE/BATTLE.PRG/5BF94.c:4546,
// 0x800CCE10-0x800CCF08).
[[nodiscard]] std::optional<TextAnchor> TextBoxAnchor(
    std::span<const TextAnchorPreset> presets,
    const TextAnchorRequest& request);

// Opens (or reopens) a box: flags keep their low 16 bits with no speaker,
// the open duration follows open_style (10, 20, 10, 8), a closed or closing
// box restarts at state 1 while an opening/open box keeps its state, text and
// frame. Kinds 1 and 5 become shaped frames 0 and 2; kind 4 takes the
// information box's fixed bottom layout. decomp: F01411 vs_battle_initTextBox
// (src/BATTLE/BATTLE.PRG/5BF94.c:4567, 0x800CCF08-0x800CD064).
void InitTextBox(TextBox& box, const TextBoxLayout& layout);

// Stops the speaker's animation and starts closing an open box over 8 ticks
// (open style 0). Returns true while the box is still visible (opening, open
// or closing), false once closed. Closing is asynchronous:
// UpdateTextBoxes counts the state down to 0. decomp: F01412
// vs_battle_dismissTextBox (src/BATTLE/BATTLE.PRG/5BF94.c:4613,
// 0x800CD064-0x800CD0FC).
[[nodiscard]] bool DismissTextBox(TextBox& box, TextBoxHost& host);
}  // namespace VS::Menu
