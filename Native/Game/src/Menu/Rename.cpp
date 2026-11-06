// Hand-maintained (tools/port/owned.csv).
#include "Menu/Rename.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>

#include "Render/EncodedText.hpp"
#include "Render/Sprites.hpp"

namespace VS::Menu::Rename {
namespace {
using Framework::Layer;
using Framework::LayerBase;
using Framework::PackUvClut;
using Framework::PackXy;

// libpad's button bits.
constexpr std::uint32_t kPadL1 = 0x4;
constexpr std::uint32_t kPadR1 = 0x8;
constexpr std::uint32_t kPadTriangle = 0x10;
constexpr std::uint32_t kPadCircle = 0x20;
constexpr std::uint32_t kPadCross = 0x40;
constexpr std::uint32_t kPadSquare = 0x80;
constexpr std::uint32_t kPadStart = 0x800;
constexpr std::uint32_t kPadUp = 0x1000;
constexpr std::uint32_t kPadRight = 0x2000;
constexpr std::uint32_t kPadDown = 0x4000;
constexpr std::uint32_t kPadLeft = 0x8000;

constexpr std::uint8_t kSpace = Render::kTextSpaceGlyph;
constexpr std::uint8_t kTerminator = Render::kTextTerminator;
constexpr int kLastColumn = static_cast<int>(kNameColumns) - 1;

// _renderRenameMenuStrings (Text_Menu8_rename_*): the index entries of the
// first confirmation option and of the insert command, and the halfword
// offsets of the prompt and the character grid.
constexpr std::size_t kOptionYesIndex = 6;
constexpr std::size_t kInsertIndex = 12;
constexpr std::size_t kConfirmPromptOffset = 19;
constexpr std::size_t kGridOffset = 54;
// The grid: nine rows of fourteen glyphs, fifteen bytes a row.
constexpr int kGridColumns = 14;
constexpr int kGridStride = 15;
constexpr int kLastGridRow = 8;
constexpr int kLastGridColumn = kGridColumns - 1;

// The scene table's entry before its head, the command glyphs' text layers
// (0x14 bytes before the head), the overlay head and the name and grid
// glyphs' text layers (8 bytes before it).
constexpr Layer kSceneBack{.base = LayerBase::kScene, .slot = -1};
constexpr Layer kCommandText{.base = LayerBase::kScene, .slot = -5};
constexpr Layer kOverlayFront{.base = LayerBase::kOverlay, .slot = 0};
constexpr Layer kOverlayText{.base = LayerBase::kOverlay, .slot = -2};

// vs_getTpage(0, 0, 0, abr, dtd) and the primitive codes in a colour word.
constexpr std::uint32_t kDitheredDrawMode = 0xE1000200;
constexpr std::uint32_t kPlainDrawMode = 0xE1000000;
constexpr std::uint32_t kAdditiveDrawMode = 0xE1000220;
constexpr std::uint32_t kLineG2 = 0x50000000;
constexpr int kLinePacketWords = 7;
constexpr int kQuadPacketWords = 10;

// The command column: the button icons (Menu_Framework_ButtonId), the Start
// icon above them and the highlight bars, twelve lines a command that fade
// over eight frames after the command runs.
constexpr int kIconX = 16;
constexpr int kCrossIcon = 3;
constexpr int kSquareIcon = 1;
constexpr int kTriangleIcon = 0;
constexpr std::array<int, 3> kIconRows{50, 66, 82};
constexpr int kStartIconY = 34;
constexpr std::uint32_t kIconSize = PackXy(16, 16);
constexpr std::uint32_t kStartIconTexture = PackUvClut(176, 112, 944, 223);
constexpr int kFirstBarCommand = 1;
constexpr int kBarCommandEnd = 5;
constexpr int kBarLines = 12;
constexpr int kBarTop = 20;
constexpr int kBarPitch = 16;
constexpr int kBarWidth = 96;
constexpr std::uint8_t kFlashFrames = 8;
constexpr std::uint32_t kBarBrightness = 128;
// The command texts, from the insert command upwards.
constexpr int kCommandFontStyle = 4;
constexpr int kCommandCount = 4;
constexpr int kCommandTextX = 40;
constexpr int kCommandTextY = 84;
constexpr int kCommandPitch = 16;
constexpr int kGlyphAdvance = 6;
// The name line: the placeholder row behind it and its glyphs.
constexpr Framework::RowPlacement kNameRow{
    .row = 29,
    .x = 156,
    .y = 48,
    .w = 164,
    .background_width = 0,
};
constexpr std::array<std::uint8_t, 4> kNamePlaceholder{
    kTerminator,
    kTerminator,
    kTerminator,
    kTerminator,
};
constexpr int kNameX = 0xA2;
constexpr int kNameY = 0x30;
// The L1 and R1 hints beside the name line, pulsing while the cursor moves.
constexpr std::uint8_t kNavRestFrame = 11;
constexpr int kNavNudgeShift = 2;
constexpr std::uint32_t kNavDimColor = 0x404040;
constexpr std::uint32_t kNavLitColor = 0x808080;
constexpr std::uint32_t kNavRestClut = 0x37F9;
constexpr int kNavIconY = 48;
constexpr int kLeftNavIconX = 136;
constexpr int kRightNavIconX = 296;
constexpr std::uint32_t kLeftNavUv = Framework::PackUv(0, 48);
constexpr std::uint32_t kRightNavUv = Framework::PackUv(16, 48);
constexpr std::uint32_t kLeftNavOne = PackXy(152, 51);
constexpr std::uint32_t kLeftNavLetter = PackXy(146, 51);
constexpr std::uint32_t kRightNavOne = PackXy(292, 51);
constexpr std::uint32_t kRightNavLetter = PackXy(286, 51);
constexpr std::uint8_t kFrameCycle = 12;
// The gradient quad drawn with the hints: the words after its tag.
constexpr std::array<std::uint32_t, kQuadPacketWords> kNavQuad{
    0, kDitheredDrawMode, 0x38000000, PackXy(208, 30),
    0, PackXy(232, 32),   0xC0A080,   PackXy(232, 46),
    0, PackXy(256, 30),
};
// The character grid.
constexpr int kGridX = 0x82;
constexpr int kGridY = 78;
constexpr int kGridColumnPitch = 12;
constexpr int kGridRowPitch = 16;
// The four backdrop quads (_renderRenameMenu's hexagon coordinates).
constexpr std::uint32_t kBackdropColor = 0x386B4100;
constexpr std::uint32_t kBackdropShade = 0x330500;
constexpr std::size_t kBackdropCorners = 4;
constexpr std::size_t kBackdropQuads = 4;
using BackdropQuad = std::array<std::uint32_t, kBackdropCorners>;
constexpr std::array<BackdropQuad, kBackdropQuads> kBackdrop{{
    {PackXy(216, 72), PackXy(142, 180), PackXy(112, 72), PackXy(112, 224)},
    {PackXy(216, 72), PackXy(216, 224), PackXy(142, 180), PackXy(112, 224)},
    {PackXy(216, 72), PackXy(290, 180), PackXy(216, 224), PackXy(320, 224)},
    {PackXy(216, 72), PackXy(320, 72), PackXy(290, 180), PackXy(320, 224)},
}};
// The selection box: vs_getRGB0(PolyG4SemiTrans, 128, 160, 192).
constexpr std::uint32_t kBoxColor = 0x3AC0A080;
constexpr std::uint32_t kBoxShade = 0xC0A080;
constexpr Box kGridBox{.x = 128, .y = 76, .w = 10, .h = 18};
constexpr int kGridCursorX = 112;
constexpr int kGridCursorY = 66;
constexpr Box kNameBox{.x = 161, .y = 48, .w = 8, .h = 12};
constexpr int kCommandCursorY = 28;
// The confirmation: three rows sliding in from the right.
constexpr int kConfirmationRows = 3;
constexpr int kConfirmationFirstRow = 20;
constexpr int kConfirmationX = 320;
constexpr int kConfirmationY = 128;
constexpr int kConfirmationPitch = 16;
constexpr int kConfirmationWidth = 0x7E;
constexpr std::int16_t kConfirmationTarget = 194;
constexpr int kConfirmationCursorBase = 7;
constexpr std::uint8_t kConfirmationIdle = 4;
// UpdateConfirmation's results.
constexpr int kChoseYes = 1;
constexpr int kChoseNo = 2;
// vs_menu8_execRename: the frame slides over ten steps of 32 pixels.
constexpr std::uint8_t kHiddenStep = 10;
constexpr int kSlideShift = 5;
constexpr int kInformationRow = 10;
constexpr int kTitleRow = 0;
constexpr int kRenamedEffect = 0xD;
constexpr std::uint8_t kEditorDelay = 8;

enum Step : std::uint8_t { kOpen, kStartEditor, kEdit, kClose };

[[nodiscard]] std::uint32_t Word(int value) {
  return static_cast<std::uint32_t>(value);
}

[[nodiscard]] std::uint32_t TagWord(std::uint32_t link, std::size_t words) {
  return (link & Render::kPacketLinkMask) |
         (static_cast<std::uint32_t>(words - 1) << Render::kPacketLengthShift);
}

template <std::size_t N>
void EmitPacket(Render::PacketQueue& queue,
                std::array<std::uint32_t, N>& packet) {
  packet[0] = TagWord(queue.ChainLink(), N);
  queue.Emit(packet);
}

// vs_battle_setSprite with the default texture page; the caller's texture
// word replaces the template's.
void EmitSprite(Context& ctx, std::uint32_t position, std::uint32_t texture) {
  Render::PacketQueue& queue = ctx.host.Queue(kOverlayFront);
  Render::SpritePacket packet = Render::BuildSpritePacket(
      {.draw_mode = Render::kDefaultSpriteDrawMode,
       .brightness_transparency = Render::kDefaultSpriteBrightness,
       .packed_position = static_cast<int>(position),
       .packed_size = static_cast<int>(kIconSize)},
      queue.ChainLink());
  packet[Render::kSpriteTextureWord] = texture;
  queue.Emit(packet);
}

// The table's halfword `index`, or nothing past its end.
[[nodiscard]] std::optional<std::size_t> Halfword(
    std::span<const std::uint8_t> strings, std::size_t index) {
  const std::size_t byte = index * 2;
  if (byte + 1 >= strings.size()) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(strings[byte]) |
         (static_cast<std::size_t>(strings[byte + 1]) << Framework::kByteShift);
}

// The text at halfword `offset` of the table, to its end (which reads as the
// terminator); refused past the end.
[[nodiscard]] std::span<const std::uint8_t> TextAt(Context& ctx,
                                                   std::size_t offset) {
  const std::span<const std::uint8_t> strings = ctx.host.MenuStrings();
  if (offset * 2 >= strings.size()) {
    ctx.host.Refuse(std::format(
        "rename text offset {} lies outside the {}-byte string table", offset,
        strings.size()));
    return {};
  }
  return strings.subspan(offset * 2);
}

// The text the table's index entry `index` points at.
[[nodiscard]] std::span<const std::uint8_t> IndexedText(Context& ctx,
                                                        std::size_t index) {
  const std::optional<std::size_t> offset =
      Halfword(ctx.host.MenuStrings(), index);
  if (!offset) {
    ctx.host.Refuse(std::format(
        "rename text index {} lies outside the string table", index));
    return {};
  }
  return TextAt(ctx, *offset);
}

// The grid glyph at the cell, or nothing for a cell past the table.
[[nodiscard]] std::optional<std::uint8_t> GridGlyph(Context& ctx,
                                                    GridCell cell) {
  const std::span<const std::uint8_t> grid = TextAt(ctx, kGridOffset);
  const int index = cell.column + (cell.row * kGridStride);
  if (index < 0 || static_cast<std::size_t>(index) >= grid.size()) {
    ctx.host.Refuse(
        std::format("rename grid cell ({}, {}) lies outside the string table",
                    cell.column, cell.row));
    return std::nullopt;
  }
  return grid[static_cast<std::size_t>(index)];
}

// Shifts the name right from `position` and blanks it; a column past the
// name is refused (the original wrote past _stringBuf).
void InsertSpace(Context& ctx, int position) {
  if (position < 0 || position > kLastColumn) {
    ctx.host.Refuse(std::format(
        "rename insert at column {} lies outside the {}-column name", position,
        kNameColumns));
    return;
  }
  std::array<std::uint8_t, kNameColumns>& name = ctx.state.name;
  for (int column = kLastColumn; position < column; --column) {
    name[static_cast<std::size_t>(column)] =
        name[static_cast<std::size_t>(column - 1)];
  }
  name[static_cast<std::size_t>(position)] = kSpace;
}

// Shifts the name left onto `position` and blanks the last column.
void RemoveAt(Context& ctx, int position) {
  if (position < 0) {
    ctx.host.Refuse(std::format(
        "rename delete at column {} lies before the name", position));
    return;
  }
  std::array<std::uint8_t, kNameColumns>& name = ctx.state.name;
  for (auto column = static_cast<std::size_t>(position);
       column + 1 < kNameColumns; ++column) {
    name[column] = name[column + 1];
  }
  name[static_cast<std::size_t>(kLastColumn)] = kSpace;
}

// Moves the cursor back and removes the column there.
void Backspace(Context& ctx) {
  State& state = ctx.state;
  if (state.position != 0) {
    --state.position;
    RemoveAt(ctx, state.position);
  }
}

// The weapon's blade's name (itemNames[blades[blade - 1].id]); empty and
// refused for a missing blade, slot or name.
[[nodiscard]] std::span<const std::uint8_t> BladeName(Context& ctx,
                                                      std::uint8_t blade) {
  if (blade == 0) {
    // The original read the slot before vs_main_inventory.blades.
    ctx.host.Refuse("renamed weapon has no blade");
    return {};
  }
  const std::optional<std::uint16_t> item = ctx.host.BladeItem(blade - 1);
  if (!item) {
    ctx.host.Refuse(
        std::format("renamed weapon's blade slot {} lies outside the inventory",
                    blade - 1));
    return {};
  }
  const std::span<const std::uint8_t> name = ctx.host.ItemName(*item);
  if (name.size() < kNameBytes) {
    ctx.host.Refuse(std::format("item name {} is not loaded", *item));
    return {};
  }
  return name.first(kNameBytes);
}

// vs_menu8_execRename's opening: the weapon's name without its spaces and
// spacing codes, one glyph a column.
void LoadName(Context& ctx) {
  State& state = ctx.state;
  const std::optional<Weapon> weapon = ctx.host.LoadWeapon(state.weapon);
  if (!weapon) {
    ctx.host.Refuse(std::format("renamed weapon {} lies outside the inventory",
                                state.weapon));
    return;
  }
  std::size_t source = 0;
  for (std::size_t column = 0; column < kNameColumns; ++column) {
    if (source >= kNameBytes) {
      // The original read on into the next inventory record.
      ctx.host.Refuse(std::format("weapon {} name runs past its {} bytes",
                                  state.weapon, kNameBytes));
      return;
    }
    const std::uint8_t glyph = weapon->name[source++];
    if (glyph == kTerminator) {
      return;
    }
    if (glyph == Render::kTextSpacing) {
      ++source;
    } else if (glyph != kSpace) {
      state.name[column] = glyph;
    }
  }
}

void DrawCommandIcons(Context& ctx, int x_offset) {
  const int x = kIconX - x_offset;
  ctx.host.DrawButtonIcon(kCrossIcon, x, kIconRows[0]);
  ctx.host.DrawButtonIcon(kSquareIcon, x, kIconRows[1]);
  ctx.host.DrawButtonIcon(kTriangleIcon, x, kIconRows[2]);
  EmitSprite(ctx, PackXy(x, kStartIconY), kStartIconTexture);
}

void DrawCommandBars(Context& ctx, int x_offset) {
  State& state = ctx.state;
  if (state.selected_action < kActionCounters) {
    state.action_counters[state.selected_action] = kFlashFrames;
  } else {
    // The original wrote past _frameControllerActionAnimationCounters.
    ctx.host.Refuse(
        std::format("rename command flash {} lies outside its {} counters",
                    state.selected_action, kActionCounters));
  }
  state.selected_action = 0;
  for (int command = kFirstBarCommand; command < kBarCommandEnd; ++command) {
    std::uint8_t& counter =
        state.action_counters[static_cast<std::size_t>(command)];
    const std::uint32_t frames = counter;
    if (counter != 0) {
      --counter;
    }
    const std::uint32_t start =
        ctx.host.GradientStopColor(kFlashFrames - frames, 0, kBarBrightness) |
        kLineG2;
    const std::uint32_t end =
        ctx.host.GradientStopColor(frames, 0, kBarBrightness);
    for (int line = 0; line < kBarLines; ++line) {
      const int y = line + kBarTop + (command * kBarPitch);
      std::array<std::uint32_t, kLinePacketWords> packet{
          0,
          kDitheredDrawMode,
          start,
          PackXy(-x_offset, y),
          end,
          PackXy(-x_offset - line + kBarWidth, y),
          kPlainDrawMode};
      EmitPacket(ctx.host.Queue(kSceneBack), packet);
    }
  }
}

void DrawCommandTexts(Context& ctx, int x_offset) {
  ctx.host.SetFontStyle(kCommandFontStyle);
  for (int command = 0; command < kCommandCount; ++command) {
    const std::span<const std::uint8_t> text =
        IndexedText(ctx, kInsertIndex - static_cast<std::size_t>(command));
    int x = kCommandTextX;
    for (const std::uint8_t glyph : text) {
      if (glyph == kTerminator) {
        break;
      }
      ctx.host.DrawGlyph(glyph, x - x_offset,
                         kCommandTextY - (command * kCommandPitch),
                         kCommandText);
      x += kGlyphAdvance;
    }
  }
}

void DrawName(Context& ctx, int x_offset) {
  Framework::RowPlacement placement = kNameRow;
  placement.x += x_offset;
  ctx.host.SetMenuItem(placement, kNamePlaceholder).selected = 1;
  for (std::size_t column = 0; column < kNameColumns; ++column) {
    const std::uint8_t glyph = ctx.state.name[column];
    if (glyph != kSpace) {
      ctx.host.DrawGlyph(
          glyph, x_offset + (static_cast<int>(column) * kGlyphAdvance) + kNameX,
          kNameY, kOverlayText);
    }
  }
}

void DrawNameNavigation(Context& ctx) {
  State& state = ctx.state;
  std::uint32_t color = kNavDimColor;
  std::uint32_t clut = kNavRestClut;
  if (state.nav_icon_pulse || state.nav_icon_animation != kNavRestFrame) {
    color = kNavLitColor;
    ++state.nav_icon_animation;
    --clut;
    if (state.nav_icon_animation > kNavRestFrame) {
      state.nav_icon_animation = 0;
    }
  }
  state.nav_icon_pulse = false;
  const int nudge = state.nav_icon_animation >> kNavNudgeShift;
  const std::uint32_t texture = clut << Framework::kHalfwordShift;
  ctx.host.DrawCompactText("1", kLeftNavOne, color);
  ctx.host.DrawCompactText("L", kLeftNavLetter, color);
  EmitSprite(ctx, PackXy(kLeftNavIconX - nudge, kNavIconY),
             texture | kLeftNavUv);
  ctx.host.DrawCompactText("1", kRightNavOne, color);
  ctx.host.DrawCompactText("R", kRightNavLetter, color);
  EmitSprite(ctx, PackXy(nudge + kRightNavIconX, kNavIconY),
             texture | kRightNavUv);
  std::array<std::uint32_t, kQuadPacketWords> quad = kNavQuad;
  EmitPacket(ctx.host.Queue(kOverlayFront), quad);
  if (++state.unused_frame_cycle >= kFrameCycle) {
    state.unused_frame_cycle = 0;
  }
}

void DrawGrid(Context& ctx, int x_offset) {
  for (int row = 0; row <= kLastGridRow; ++row) {
    for (int column = 0; column < kGridColumns; ++column) {
      const std::optional<std::uint8_t> glyph =
          GridGlyph(ctx, {.column = column, .row = row});
      if (!glyph) {
        return;
      }
      ctx.host.DrawGlyph(*glyph,
                         x_offset + kGridX + (column * kGridColumnPitch),
                         (row * kGridRowPitch) + kGridY, kOverlayText);
    }
  }
}

void DrawBackdrop(Context& ctx, int x_offset) {
  const std::uint32_t shift = Word(x_offset);
  for (const auto& [first, second, third, fourth] : kBackdrop) {
    std::array<std::uint32_t, kQuadPacketWords> packet{
        0,
        kDitheredDrawMode,
        kBackdropColor,
        shift + first,
        kBackdropShade,
        shift + second,
        kBackdropShade,
        shift + third,
        kBackdropShade,
        shift + fourth,
    };
    EmitPacket(ctx.host.Queue(kSceneBack), packet);
  }
}

// _renameMenuInput's command column (state 1).
void UpdateCommands(Context& ctx) {
  State& state = ctx.state;
  Host& host = ctx.host;
  state.position =
      static_cast<std::uint8_t>(EditNameAtCursor(ctx, state.position));
  const Framework::Buttons buttons = host.Input();
  if ((buttons.pressed & kPadStart) != 0) {
    host.PlaySound(MenuSound::kSelect);
    SetCommandHighlight(ctx, kNoCommand);
    state.editor_step = EditorStep::kLeave;
    return;
  }
  if ((buttons.repeat & kPadCross) != 0) {
    host.PlaySound(MenuSound::kChange);
    Backspace(ctx);
  }
  int command = state.row;
  if ((buttons.pressed & kPadRight) != 0) {
    host.PlaySound(MenuSound::kChange);
    SetCommandHighlight(ctx, kNoCommand);
    state.row = command == static_cast<int>(Command::kInsert) ? 1 : 0;
    state.editor_step = EditorStep::kGrid;
    return;
  }
  if ((buttons.pressed & kPadCircle) != 0) {
    switch (static_cast<Command>(command)) {
      case Command::kOk:
        host.PlaySound(MenuSound::kSelect);
        SetCommandHighlight(ctx, kNoCommand);
        state.editor_step = EditorStep::kLeave;
        return;
      case Command::kBack:
        Backspace(ctx);
        break;
      case Command::kDelete:
        RemoveAt(ctx, state.position);
        break;
      case Command::kInsert:
        InsertSpace(ctx, state.position);
        break;
    }
    host.PlaySound(MenuSound::kChange);
  }
  if ((buttons.repeat & kPadUp) != 0) {
    if (command == 0) {
      if ((buttons.pressed & kPadUp) != 0) {
        command = static_cast<int>(Command::kInsert);
      }
    } else {
      --command;
    }
  }
  if ((buttons.repeat & kPadDown) != 0) {
    if (command == static_cast<int>(Command::kInsert)) {
      if ((buttons.pressed & kPadDown) != 0) {
        command = 0;
      }
    } else {
      ++command;
    }
  }
  if (command != state.row) {
    host.PlaySound(MenuSound::kChange);
    state.row = static_cast<std::uint8_t>(command);
  }
  SetCommandHighlight(ctx, command);
  state.cursor_animation = static_cast<std::uint8_t>(
      host.DrawCursor(state.cursor_animation,
                      PackXy(0, (command * kCommandPitch) + kCommandCursorY)));
}

// Moves the grid cell; true when Left at the first column moved to the
// command column.
bool MoveGridCell(Context& ctx, const Framework::Buttons& buttons) {
  State& state = ctx.state;
  if ((buttons.repeat & kPadUp) != 0) {
    if (state.row != 0) {
      --state.row;
    } else if ((buttons.pressed & kPadUp) != 0) {
      state.row = kLastGridRow;
    }
  }
  if ((buttons.repeat & kPadDown) != 0) {
    if (state.row != kLastGridRow) {
      ++state.row;
    } else if ((buttons.pressed & kPadDown) != 0) {
      // The original stored nameEditorState != 2, always 0 here.
      state.row = 0;
    }
  }
  if ((buttons.repeat & kPadLeft) != 0) {
    if (state.column != 0) {
      --state.column;
    } else if ((buttons.pressed & kPadLeft) != 0) {
      ctx.host.PlaySound(MenuSound::kChange);
      state.row = static_cast<std::uint8_t>(
          state.row == 0 ? static_cast<int>(Command::kDelete)
                         : static_cast<int>(Command::kInsert));
      state.editor_step = EditorStep::kCommands;
      return true;
    }
  }
  if ((buttons.repeat & kPadRight) != 0) {
    if (state.column != kLastGridColumn) {
      ++state.column;
    } else if ((buttons.pressed & kPadRight) != 0) {
      state.column = 0;
    }
  }
  return false;
}

// _renameMenuInput's character grid (state 2).
void UpdateGrid(Context& ctx) {
  State& state = ctx.state;
  Host& host = ctx.host;
  state.position =
      static_cast<std::uint8_t>(EditNameAtCursor(ctx, state.position));
  const Framework::Buttons buttons = host.Input();
  if ((buttons.pressed & kPadStart) != 0) {
    host.PlaySound(MenuSound::kSelect);
    state.editor_step = EditorStep::kLeave;
    return;
  }
  if ((buttons.pressed & kPadCircle) != 0) {
    host.PlaySound(MenuSound::kChange);
    const std::optional<std::uint8_t> glyph =
        GridGlyph(ctx, {.column = state.column, .row = state.row});
    if (glyph && state.position < kNameColumns) {
      state.name[state.position] = *glyph;
    } else if (glyph) {
      host.Refuse(
          std::format("rename cursor column {} lies outside the {}-column name",
                      state.position, kNameColumns));
    }
    if (state.position < kLastColumn) {
      ++state.position;
    }
  }
  if ((buttons.repeat & kPadCross) != 0) {
    host.PlaySound(MenuSound::kChange);
    Backspace(ctx);
  }
  const int before = state.column + (state.row * kGridColumns);
  if (MoveGridCell(ctx, buttons)) {
    return;
  }
  if (before != state.column + (state.row * kGridColumns)) {
    host.PlaySound(MenuSound::kChange);
  }
}

// _renameMenuInput state 4: the caution box once the frame has slid away.
void OpenConfirmation(Context& ctx) {
  State& state = ctx.state;
  if (state.animation_step != kHiddenStep) {
    return;
  }
  if (CopyTrimmedName(ctx, state.confirmed_name)) {
    const std::optional<Weapon> weapon = ctx.host.LoadWeapon(state.weapon);
    if (!weapon) {
      ctx.host.Refuse(std::format(
          "renamed weapon {} lies outside the inventory", state.weapon));
    } else if (const std::span<const std::uint8_t> name =
                   BladeName(ctx, weapon->blade);
               !name.empty()) {
      std::ranges::copy(name, state.confirmed_name.begin());
    }
  }
  ctx.host.ShowConfirmPrompt(state.confirmed_name,
                             TextAt(ctx, kConfirmPromptOffset));
  UpdateConfirmation(ctx, true);
  state.editor_step = EditorStep::kConfirmation;
}

// _renameMenuInput state 5: 1 once the name is stored or dropped.
int Confirm(Context& ctx) {
  State& state = ctx.state;
  Host& host = ctx.host;
  const int choice = UpdateConfirmation(ctx, false);
  if (choice == 0) {
    return 0;
  }
  host.DismissConfirmPrompt();
  if (choice == kChoseNo) {
    host.PlaySound(MenuSound::kLeave);
    state.show_interface = true;
    state.editor_step = EditorStep::kReturnToGrid;
    return 0;
  }
  const std::optional<Weapon> weapon = host.LoadWeapon(state.weapon);
  if (!weapon) {
    host.Refuse(std::format("renamed weapon {} lies outside the inventory",
                            state.weapon));
    return 1;
  }
  if (choice == kChoseYes) {
    host.PlaySoundEffect(kRenamedEffect);
    host.StoreWeaponName(state.weapon, state.confirmed_name);
    if (weapon->equipped) {
      host.EquipWeapon(state.weapon);
    }
    return 1;
  }
  host.PlaySound(MenuSound::kLeave);
  if (weapon->name[0] == kTerminator) {
    const std::span<const std::uint8_t> name = BladeName(ctx, weapon->blade);
    if (!name.empty()) {
      host.StoreWeaponName(state.weapon, name);
    }
  }
  return 1;
}

// vs_menu8_execRename state 0: the request, the blank name and the weapon's.
void OpenScreen(Context& ctx) {
  State& state = ctx.state;
  const std::optional<Request> request = ctx.host.RenameRequest();
  if (!request) {
    ctx.host.Refuse("rename opened without the menu's request");
  }
  state.weapon = request ? request->weapon : 0;
  ctx.host.DismissInformationBox();
  state.name.fill(kSpace);
  if (request && request->edit_existing) {
    LoadName(ctx);
  }
  state.show_interface = false;
  state.animation_step = kHiddenStep;
}

void DrawSlide(Context& ctx) {
  State& state = ctx.state;
  if (state.show_interface) {
    if (state.animation_step != 0) {
      --state.animation_step;
    }
    const std::span<const std::uint8_t> steps = ctx.host.RowAnimationSteps();
    if (state.animation_step >= steps.size()) {
      ctx.host.Refuse(std::format(
          "rename slide step {} lies outside the {} row animation steps",
          state.animation_step, steps.size()));
      return;
    }
    DrawFrame(ctx, steps[state.animation_step]);
  } else if (state.animation_step < kHiddenStep) {
    ++state.animation_step;
    DrawFrame(ctx, state.animation_step << kSlideShift);
  }
}
}  // namespace

// decomp: MENU8.PRG _insertDeleteHandler (src/MENU/MENU8.PRG/21A0.c:29)
int EditNameAtCursor(Context& ctx, int position) {
  const Framework::Buttons buttons = ctx.host.Input();
  ctx.state.nav_icon_pulse = true;
  const int previous = position;
  constexpr std::uint32_t kBothShoulders = kPadL1 | kPadR1;
  if ((buttons.repeat & kPadTriangle) != 0) {
    ctx.host.PlaySound(MenuSound::kChange);
    InsertSpace(ctx, position);
  } else if ((buttons.repeat & kPadSquare) != 0) {
    ctx.host.PlaySound(MenuSound::kChange);
    RemoveAt(ctx, position);
  } else if ((buttons.held & kBothShoulders) != kBothShoulders) {
    if ((buttons.repeat & kPadL1) != 0) {
      if (position != 0) {
        --position;
      } else if ((buttons.pressed & kPadL1) != 0) {
        position = kLastColumn;
      }
    }
    if ((buttons.repeat & kPadR1) != 0) {
      if (position != kLastColumn) {
        ++position;
      } else if ((buttons.pressed & kPadR1) != 0) {
        position = 0;
      }
    }
  }
  if (position != previous) {
    ctx.host.PlaySound(MenuSound::kChange);
  }
  return position;
}

// decomp: MENU8.PRG _copyNormalizedString (src/MENU/MENU8.PRG/21A0.c:92)
bool CopyTrimmedName(Context& ctx, std::span<std::uint8_t, kNameBytes> out) {
  const std::array<std::uint8_t, kNameColumns>& name = ctx.state.name;
  std::size_t end = 0;
  while (end < kNameColumns && name[end] != kTerminator) {
    out[end] = name[end];
    ++end;
  }
  out[end] = kTerminator;
  if (end == 0) {
    // The original then read the byte before the buffer.
    ctx.host.Refuse("rename name starts with a terminator");
    return true;
  }
  std::size_t last = end - 1;
  while (out[last] == kSpace) {
    out[last] = kTerminator;
    if (last == 0) {
      return true;
    }
    --last;
  }
  for (std::size_t pos = last + 2; pos < kNameBytes; ++pos) {
    out[pos] = 0;
  }
  return false;
}

// decomp: MENU8.PRG _renderRenameMenu (src/MENU/MENU8.PRG/21A0.c:133)
void DrawFrame(Context& ctx, int x_offset) {
  DrawCommandIcons(ctx, x_offset);
  DrawCommandBars(ctx, x_offset);
  DrawCommandTexts(ctx, x_offset);
  DrawName(ctx, x_offset);
  if (x_offset == 0) {
    DrawNameNavigation(ctx);
  }
  DrawGrid(ctx, x_offset);
  DrawBackdrop(ctx, x_offset);
}

// decomp: MENU8.PRG _highlightSelection (src/MENU/MENU8.PRG/21A0.c:318)
void DrawSelectionBox(Context& ctx, const Box& box) {
  const auto corner = [](int x, int y) {
    return Word(x) | (Word(y) << Framework::kHalfwordShift);
  };
  std::array<std::uint32_t, kQuadPacketWords> packet{
      0,         kAdditiveDrawMode,
      kBoxColor, corner(box.x, box.y),
      0,         corner(box.x + box.w, box.y),
      0,         corner(box.x, box.y + box.h),
      kBoxShade, corner(box.x + box.w, box.y + box.h)};
  EmitPacket(ctx.host.Queue(kOverlayFront), packet);
}

// decomp: MENU8.PRG _highlightCharSelection (src/MENU/MENU8.PRG/21A0.c:346)
int HighlightGridCharacter(Context& ctx, int cursor_step, GridCell cell) {
  const int x = cell.column * kGridColumnPitch;
  const int y = cell.row * kGridRowPitch;
  DrawSelectionBox(ctx, {.x = x + kGridBox.x,
                         .y = y + kGridBox.y,
                         .w = kGridBox.w,
                         .h = kGridBox.h});
  return ctx.host.DrawCursor(
      cursor_step, Word(x + kGridCursorX) |
                       (Word(y + kGridCursorY) << Framework::kHalfwordShift));
}

// decomp: MENU8.PRG _confirmationScreen (src/MENU/MENU8.PRG/21A0.c:364)
int UpdateConfirmation(Context& ctx, bool initialize) {
  State& state = ctx.state;
  Host& host = ctx.host;
  if (initialize) {
    state.confirmation_row = 0;
    state.confirmation_step = 0;
  }
  if (state.confirmation_step < kConfirmationRows) {
    const int option = state.confirmation_step;
    Framework::MenuRow& row = host.SetMenuItem(
        {.row = option + kConfirmationFirstRow,
         .x = kConfirmationX,
         .y = (option * kConfirmationPitch) + kConfirmationY,
         .w = kConfirmationWidth,
         .background_width = 0},
        IndexedText(ctx, kOptionYesIndex + static_cast<std::size_t>(option)));
    row.state = Framework::RowState::kSlideX;
    row.target0 = kConfirmationTarget;
    ++state.confirmation_step;
    return 0;
  }
  if (state.confirmation_step == kConfirmationRows) {
    state.confirmation_step += host.RowsIdle() ? 1 : 0;
    return 0;
  }
  if (state.confirmation_step != kConfirmationIdle) {
    return 0;
  }
  for (int option = 0; option < kConfirmationRows; ++option) {
    host.MenuItem(option + kConfirmationFirstRow).selected =
        state.confirmation_row == option ? 1 : 0;
  }
  const Framework::Buttons buttons = host.Input();
  if ((buttons.pressed & (kPadCircle | kPadCross)) != 0) {
    for (int option = 0; option < kConfirmationRows; ++option) {
      host.FlyOutRowRight(option + kConfirmationFirstRow);
    }
    if ((buttons.pressed & kPadCross) != 0) {
      state.confirmation_row = 1;
    }
    return state.confirmation_row + 1;
  }
  int next = state.confirmation_row;
  if ((buttons.repeat & kPadUp) != 0) {
    next += 2;
  }
  if ((buttons.repeat & kPadDown) != 0) {
    ++next;
  }
  if (next >= kConfirmationRows) {
    next -= kConfirmationRows;
  }
  if (next != state.confirmation_row) {
    host.PlaySound(MenuSound::kChange);
    state.confirmation_row = static_cast<std::uint8_t>(next);
  }
  state.confirmation_cursor = static_cast<std::uint8_t>(host.DrawMainMenuCursor(
      state.confirmation_cursor, next + kConfirmationCursorBase));
  return 0;
}

// decomp: MENU8.PRG _updateMenuSelection (src/MENU/MENU8.PRG/21A0.c:445)
void SetCommandHighlight(Context& ctx, int row) {
  // Rows 33 to 37 follow the name row; none of them matches a command.
  constexpr int kFirstHighlightRow = 4;
  constexpr int kHighlightRowEnd = 9;
  constexpr int kHighlightRowBase = 29;
  if (row < kCommandCount) {
    ctx.state.selected_action = static_cast<std::uint8_t>(row + 1);
  }
  for (int i = kFirstHighlightRow; i < kHighlightRowEnd; ++i) {
    ctx.host.MenuItem(i + kHighlightRowBase).selected = i == row ? 1 : 0;
  }
}

// decomp: MENU8.PRG _renameMenuInput (src/MENU/MENU8.PRG/21A0.c:463)
int UpdateNameEditor(Context& ctx, bool initialize) {
  State& state = ctx.state;
  if (initialize) {
    state.column = 0;
    state.row = 0;
    state.position = 0;
    state.cursor_animation = kEditorDelay;
    state.editor_step = EditorStep::kDelay;
    return 0;
  }
  switch (state.editor_step) {
    case EditorStep::kDelay:
      // The cursor's step doubles as the opening delay.
      if (state.cursor_animation != 0) {
        --state.cursor_animation;
      } else {
        state.editor_step = EditorStep::kGrid;
      }
      break;
    case EditorStep::kCommands:
      UpdateCommands(ctx);
      break;
    case EditorStep::kGrid:
      UpdateGrid(ctx);
      break;
    case EditorStep::kLeave:
      SetCommandHighlight(ctx, 0);
      state.show_interface = false;
      state.editor_step = EditorStep::kInitConfirmation;
      break;
    case EditorStep::kInitConfirmation:
      OpenConfirmation(ctx);
      break;
    case EditorStep::kConfirmation:
      if (Confirm(ctx) != 0) {
        return 1;
      }
      break;
    case EditorStep::kReturnToGrid:
      if (state.animation_step == 0) {
        state.column = 0;
        state.row = 0;
        state.editor_step = EditorStep::kGrid;
      }
      break;
  }
  if (state.editor_step == EditorStep::kGrid) {
    state.cursor_animation = static_cast<std::uint8_t>(
        HighlightGridCharacter(ctx, state.cursor_animation,
                               {.column = state.column, .row = state.row}));
  }
  if (state.editor_step == EditorStep::kCommands ||
      state.editor_step == EditorStep::kGrid) {
    DrawSelectionBox(ctx, {.x = (state.position * kGlyphAdvance) + kNameBox.x,
                           .y = kNameBox.y,
                           .w = kNameBox.w,
                           .h = kNameBox.h});
  }
  return 0;
}

// decomp: MENU8.PRG vs_menu8_execRename (src/MENU/MENU8.PRG/21A0.c:820)
int Update(Context& ctx, std::uint8_t& step) {
  State& state = ctx.state;
  switch (step) {
    case kOpen:
      OpenScreen(ctx);
      step = kStartEditor;
      break;
    case kStartEditor:
      UpdateNameEditor(ctx, true);
      state.show_interface = true;
      step = kEdit;
      break;
    case kEdit:
      if (UpdateNameEditor(ctx, false) != 0) {
        ctx.host.FlyOutRowRight(kInformationRow);
        ctx.host.FlyOutRowLeft(kTitleRow);
        ctx.host.InitializeInformationBox();
        step = kClose;
      }
      break;
    case kClose:
      if (state.animation_step == kHiddenStep) {
        step = kOpen;
        ctx.host.ReturnToMenu();
      }
      break;
    default:
      break;
  }
  DrawSlide(ctx);
  return 0;
}
}  // namespace VS::Menu::Rename
