#pragma once
// The live text box records BATTLE.PRG keeps (src/BATTLE/BATTLE.PRG/battleUi.h
// and 5BF94.c) and its anchor and information header tables. Menu/TextBox.hpp
// holds the eight boxes; TextBoxStateLive.cpp converts a record to and from
// the TextBox value (Menu/TextBoxState.hpp) the menus work on.
#include <cstddef>
#include <cstdint>

#include "Menu/TextBoxState.hpp"

namespace VS::Menu {
// Two anchors per preset: the primary and the secondary facing.
inline constexpr std::size_t kTextAnchorFacings = 2;
// The caution, information and check headers.
inline constexpr std::size_t kInformationHeaderCount = 3;
// The integers and strings a formatted text can substitute.
inline constexpr std::size_t kStringContextSlots = 10;
// The original TextBoxSelector's size, which the module symbol map relocates.
inline constexpr std::size_t kTextBoxSelectorBytes = 13;

// decomp: TextBox_fields (src/BATTLE/BATTLE.PRG/battleUi.h:91): TextBoxFlags
// as the record packs it. Values wider than a field are truncated.
struct PackedTextBoxFlags {
  std::uint32_t kind : 4;
  std::uint32_t open_style : 2;
  std::uint32_t alignment : 2;
  std::uint32_t frame_shape : 4;
  std::uint32_t frame_part : 4;
  std::uint32_t arrow_frame : 8;
  std::uint32_t speaker_texture : 6;
  std::uint32_t done : 1;
  std::uint32_t frozen : 1;
};
static_assert(sizeof(PackedTextBoxFlags) == sizeof(std::uint32_t),
              "PackedTextBoxFlags must keep the original word");

// decomp: TextBoxSelector (src/BATTLE/BATTLE.PRG/5BF94.c:195): the selection
// TextBoxSelector reads and writes, in the original's 13 bytes, which the
// module symbol map relocates byte for byte.
struct TextBoxSelectorState {
  std::uint8_t order[kTextBoxCount];
  std::uint8_t reserved8;
  std::uint8_t reserved9;
  std::uint8_t selected_index;
  std::uint8_t count;
  std::uint8_t reserved_c;
};
static_assert(sizeof(TextBoxSelectorState) == kTextBoxSelectorBytes,
              "TextBoxSelectorState must keep the original layout");

// decomp: PrintfArgs (src/BATTLE/BATTLE.PRG/battleUi.h:164): the values a
// formatted text substitutes, by slot.
struct StringContext {
  std::int32_t integers[kStringContextSlots];
  char* strings[kStringContextSlots];
};

// decomp: vs_TextBox (src/BATTLE/BATTLE.PRG/battleUi.h:103): one live text
// box, the stored form of TextBox. `text` borrows the encoded string the box
// prints.
struct TextBoxState {
  PackedTextBoxFlags flags;
  std::int16_t opaque4;
  std::int16_t opaque6;
  char* text;
  std::uint16_t speed;
  std::uint16_t previous_chunks;
  std::int16_t x_indent;
  std::int16_t y_indent;
  std::int16_t x;
  std::int16_t y;
  std::int16_t center_x;
  std::int16_t center_y;
  std::int16_t state;
  std::uint8_t brightness;
  std::uint8_t reserved1f;
  std::int8_t chars_per_line;
  std::int8_t line_count;
  std::int8_t open_duration;
  std::int8_t reserved23;
  std::int16_t frame_x;
  std::int16_t frame_y;
  std::int16_t frame_width;
  std::int16_t frame_height;
  std::int16_t speaker_pause;
  std::int16_t speaker_animation;
};

// decomp: D_800EC284 (src/BATTLE/BATTLE.PRG/5BF94.c:394): each preset's
// packed TextAnchor words. The original holds nine presets; the script's
// 4-bit preset reads the following module bytes as presets 9-15, which
// ModuleData.cpp copies here so those reads match.
extern std::uint32_t textBoxAnchors[kTextAnchorPresetCount][kTextAnchorFacings];
// decomp: D_800EC2CC (src/BATTLE/BATTLE.PRG/5BF94.c:395): each information
// header sprite's packed position.
extern std::int32_t informationHeaderPositions[kInformationHeaderCount];
}  // namespace VS::Menu
