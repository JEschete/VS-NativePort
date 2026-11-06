// Hand-maintained (tools/port/owned.csv).
#include "Menu/Framework/Framework.hpp"  // NOLINT(build/include): native C++ interface.

#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "Core/Math/ScalarMath.hpp"
#include "Render/GameFont.hpp"
#include "Render/Sprites.hpp"
#include "Render/UiPackets.hpp"

namespace VS::Menu::Framework {
namespace {
// Core_TextCode and the row text's end.
constexpr std::uint8_t kTextNonPrinting = 0xE5;
constexpr std::uint8_t kTextTerminator = 0xE7;
constexpr std::uint8_t kTextChunkSize = 0xF8;
constexpr std::uint8_t kTextSpacing = 0xFA;
constexpr std::uint8_t kRowTextEnd = 0xFF;
// A text chunk opens with its code and a zero byte.
constexpr std::size_t kChunkHeaderBytes = 2;
constexpr auto kDescriptionEnd = kChunkHeaderBytes + kRowDescriptionCopyBytes;
constexpr auto kMessageEnd = kChunkHeaderBytes + kInformationMessageCopyBytes;

// The pad bits the menus read (PADLup, PADLdown, PADRup, PADRright,
// PADRdown).
constexpr std::uint32_t kPadLUp = 0x1000;
constexpr std::uint32_t kPadLDown = 0x4000;
constexpr std::uint32_t kPadRUp = 0x10;
constexpr std::uint32_t kPadRRight = 0x20;
constexpr std::uint32_t kPadRDown = 0x40;

// GPU packet words: primitive codes and the draw modes the menus set
// (vs_getTpage over texture page, depth, blend and dithering).
constexpr int kCommandShift = 24;
constexpr std::uint32_t kPolyG4 = 0x38;
constexpr std::uint32_t kLineG2 = 0x50;
constexpr std::uint32_t kLineG2SemiTransparent = 0x52;
constexpr std::uint32_t kTileCommand = 0x60000000;
constexpr std::uint32_t kDrawModeDithered = 0xE1000200;
constexpr std::uint32_t kDrawModePlain = 0xE1000000;
constexpr std::uint32_t kDrawModeAddDithered = 0xE1000220;
constexpr std::uint32_t kDrawModeAdd = 0xE1000020;
// Page (768, 0) and (832, 0), 4-bit, additive.
constexpr std::uint32_t kCommandLabelDrawMode = 0xE100002C;
constexpr std::uint32_t kRowGlyphDrawMode = 0xE100002D;
// Pages (768, 256) and (640, 256), 8-bit, additive.
constexpr std::uint32_t kBackgroundRightDrawMode = 0xE10000BC;
constexpr std::uint32_t kBackgroundLeftDrawMode = 0xE10000BA;
// A 15-bit page with additive blending; its column is x >> 6.
constexpr std::uint32_t kFramePageDrawMode = 0xE1000120;
constexpr int kPageColumnShift = 6;
constexpr std::size_t kGradientQuadWords = 11;
constexpr std::size_t kGradientLineWords = 7;

// The ordering-table slots the framework links into.
constexpr Layer kOverlayFront{.base = LayerBase::kOverlay, .slot = 0};
constexpr Layer kRowIconLayer{.base = LayerBase::kOverlay, .slot = 1};
constexpr Layer kRowLayer{.base = LayerBase::kOverlay, .slot = 2};
constexpr Layer kRowShadowLayer{.base = LayerBase::kOverlay, .slot = 3};
constexpr Layer kActionCostLayer{.base = LayerBase::kScene, .slot = -1};
constexpr Layer kButtonPromptLayer{.base = LayerBase::kScene, .slot = -3};

// Row geometry and colours.
constexpr int kScreenWidth = 320;
constexpr int kRowHeight = 12;
constexpr int kRowSpacing = 16;
constexpr int kFirstRowY = 18;
constexpr int kRowTextIndent = 6;
constexpr int kIconRowTextIndent = 22;
constexpr unsigned kTextIndentingIcons = 23;
constexpr int kRowFontPaletteBase = 4;
constexpr int kUnselectablePalette = 3;
constexpr int kGradientStops = 8;
constexpr int kFullBrightness = 128;
constexpr int kShadowOffset = 2;
constexpr int kSubTextIndent = 10;
constexpr std::uint8_t kBackgroundWidthStepMask = 7;
constexpr std::uint8_t kBackgroundWidthMask = 0xF;
constexpr int kIconSize = 16;
constexpr std::uint32_t kStepMask = 0xF;

// _getMenuItemGradientColor: the first nine stops blend these colours.
constexpr std::uint32_t kBlendedGradientStops = 9;
constexpr std::uint32_t kGradientTopFrom = 0x6B4100;
constexpr std::uint32_t kGradientTopTo = 0x6C8219;
constexpr std::uint32_t kGradientBottomFrom = 0x330500;
constexpr std::uint32_t kGradientBottomTo = 0x262801;
constexpr int kGradientTableShift = 3;
constexpr std::int64_t kGradientTableBias = 2;

// The rows' slide steps (rowAnimationSteps) are searched over 1..15.
constexpr int kSlideSteps = 16;
constexpr int kSlideJump = 32;

// A row type's icon (bits 26..31) and material (bits 16..18).
constexpr int kRowIconShift = 26;
constexpr int kMaterialShift = 16;
constexpr std::uint32_t kMaterialMask = 7;

[[nodiscard]] std::uint32_t TagWord(std::uint32_t link, std::size_t words) {
  return (link & Render::kPacketLinkMask) |
         (static_cast<std::uint32_t>(words - 1) << Render::kPacketLengthShift);
}

[[nodiscard]] std::uint32_t Command(std::uint32_t code, std::uint32_t rgb) {
  return (code << kCommandShift) | rgb;
}

// vs_battle_setSprite and its texture-page variant with the words the
// callers overwrite.
struct Sprite {
  std::uint32_t draw_mode = Render::kDefaultSpriteDrawMode;
  int brightness = Render::kDefaultSpriteBrightness;
  std::uint32_t position = 0;
  std::uint32_t size = 0;
  std::uint32_t texture = 0;
};

void EmitSprite(Context& ctx, Layer layer, const Sprite& sprite) {
  Render::PacketQueue& queue = ctx.host.Queue(layer);
  Render::SpritePacket packet = Render::BuildSpritePacket(
      {.draw_mode = sprite.draw_mode,
       .brightness_transparency = sprite.brightness,
       .packed_position = static_cast<int>(sprite.position),
       .packed_size = static_cast<int>(sprite.size)},
      queue.ChainLink());
  packet[Render::kSpriteTextureWord] = sprite.texture;
  queue.Emit(packet);
}

template <std::size_t N>
void EmitPacket(Render::PacketQueue& queue,
                std::array<std::uint32_t, N>& packet) {
  packet[0] = TagWord(queue.ChainLink(), N);
  queue.Emit(packet);
}

void DrawTile(Context& ctx, Layer layer, std::uint32_t position,
              std::uint32_t size) {
  Render::InsertThreeWordPrimitive(
      ctx.host.Queue(layer),
      {.color_command = kTileCommand, .position = position, .size = size});
}

[[nodiscard]] std::uint32_t Word(int value) {
  return static_cast<std::uint32_t>(value);
}

[[nodiscard]] int AnimationStep(Context& ctx, int index) {
  const auto steps = ctx.tables.row_animation_steps;
  if (index < 0 || static_cast<std::size_t>(index) >= steps.size()) {
    ctx.host.Refuse(
        std::format("row animation step {} outside the table", index));
    return 0;
  }
  return steps[static_cast<std::size_t>(index)];
}

// _renderMenuItems: a row above its target lands on the step before the
// first one (1..15) at which it would reach the target.
[[nodiscard]] int StepDownTo(Context& ctx, int target, int origin) {
  int step = 1;
  while (step < kSlideSteps && target + AnimationStep(ctx, step) < origin) {
    ++step;
  }
  return target + AnimationStep(ctx, step - 1);
}

[[nodiscard]] int StepUpTo(Context& ctx, int target, int origin) {
  int step = 1;
  while (step < kSlideSteps && origin < target - AnimationStep(ctx, step)) {
    ++step;
  }
  return target - AnimationStep(ctx, step - 1);
}

// Menu_menuStrings: halfword offsets (in halfwords) to the strings.
[[nodiscard]] std::span<const std::uint8_t> MenuString(Context& ctx,
                                                       int index) {
  const auto strings = ctx.tables.menu_strings;
  const auto at = static_cast<std::size_t>(index) * 2;
  if (index < 0 || at + 1 >= strings.size()) {
    ctx.host.Refuse(std::format("menu string {} outside the table", index));
    return {};
  }
  const std::size_t offset =
      static_cast<std::size_t>(strings[at] | (strings[at + 1] << CHAR_BIT)) * 2;
  if (offset >= strings.size()) {
    ctx.host.Refuse(std::format("menu string {} past the table", index));
    return {};
  }
  return strings.subspan(offset);
}

// The cursor memory of a menu: the row and page a list reopens at.
struct CursorMemory {
  int row = 0;
  int page = 0;
};

[[nodiscard]] std::optional<std::size_t> MemoryIndex(Context& ctx, int menu) {
  const auto at = static_cast<std::size_t>(menu) * 2;
  if (menu < 0 || at + 1 >= ctx.host.MenuMemory().size()) {
    ctx.host.Refuse(std::format("cursor memory {} outside the record", menu));
    return std::nullopt;
  }
  return at;
}

[[nodiscard]] CursorMemory RecalledCursor(Context& ctx, int menu) {
  const auto at = MemoryIndex(ctx, menu);
  if (!at) {
    return {};
  }
  const auto memory = ctx.host.MenuMemory();
  return {.row = memory[*at], .page = memory[*at + 1]};
}

void RememberCursor(Context& ctx, int menu, const CursorMemory& cursor) {
  const auto at = MemoryIndex(ctx, menu);
  if (!at) {
    return;
  }
  const auto memory = ctx.host.MenuMemory();
  memory[*at] = static_cast<std::uint8_t>(cursor.row);
  memory[*at + 1] = static_cast<std::uint8_t>(cursor.page);
}

// vs_mainMenu_addMenuActions and vs_mainmenu_setMenuRows: each entry keeps
// 26 name bytes and 92 description bytes, each closed by two terminators.
void FillItems(std::span<ItemText> items, std::span<const RowText> texts,
               std::span<const std::int32_t> row_types) {
  for (std::size_t i = 0; i < items.size(); ++i) {
    ItemText& item = items[i];
    item.flags = row_types[i];
    const RowText& text = texts[i];
    if (!text.name.empty()) {
      const std::size_t count = std::min(text.name.size(), kRowNameCopyBytes);
      std::copy_n(text.name.begin(), count, item.name.begin());
      item.name[kRowNameCopyBytes] = kTextTerminator;
      item.name[kRowNameCopyBytes + 1] = kTextTerminator;
    } else {
      item.name[0] = kTextTerminator;
      item.name[1] = kTextTerminator;
    }
    if (!text.description.empty()) {
      item.description[0] = kTextChunkSize;
      item.description[1] = 0;
      const std::size_t count =
          std::min(text.description.size(), kRowDescriptionCopyBytes);
      std::copy_n(text.description.begin(), count,
                  item.description.begin() + kChunkHeaderBytes);
      item.description[kDescriptionEnd] = kTextTerminator;
      item.description[kDescriptionEnd + 1] = kTextTerminator;
    } else {
      item.description[0] = kTextTerminator;
      item.description[1] = kTextTerminator;
    }
  }
}

// The entry `index` of a list, or null when the list is shorter.
[[nodiscard]] ItemText* ListItem(Context& ctx, ItemList list, int index) {
  const auto items = ctx.host.Items(list);
  if (index < 0 || static_cast<std::size_t>(index) >= items.size()) {
    ctx.host.Refuse(std::format("item {} outside the list", index));
    return nullptr;
  }
  return &items[static_cast<std::size_t>(index)];
}

[[nodiscard]] std::span<const std::uint8_t> ItemName(Context& ctx,
                                                     ItemList list, int index) {
  const ItemText* item = ListItem(ctx, list, index);
  return item != nullptr ? std::span<const std::uint8_t>(item->name)
                         : std::span<const std::uint8_t>();
}

[[nodiscard]] std::int32_t ItemFlags(Context& ctx, ItemList list, int index) {
  const ItemText* item = ListItem(ctx, list, index);
  return item != nullptr ? item->flags : 0;
}

void ShowItemDescription(Context& ctx, ItemList list, int index) {
  const ItemText* item = ListItem(ctx, list, index);
  if (item != nullptr) {
    SetInformationMessage(ctx, item->description);
  }
}

// The glyphs of a row's text up to its end; 0xFA moves the pen.
void DrawRowGlyphs(Context& ctx, const MenuRow& row, int pen) {
  std::size_t i = 0;
  while (true) {
    if (i >= row.text.size()) {
      ctx.host.Refuse("menu row text without an end");
      return;
    }
    const std::uint8_t c = row.text[i++];
    if (c == kRowTextEnd) {
      return;
    }
    if (c == kTextSpacing) {
      if (i >= row.text.size()) {
        ctx.host.Refuse("menu row text without an end");
        return;
      }
      pen += row.text[i++];
    } else {
      pen = Render::DrawVariableWidthGlyph(
          ctx.host.Queue(kRowLayer), ctx.host.FontStyle(),
          ctx.tables.glyph_widths, c, pen, row.y);
    }
  }
}

// The row loops refuse a missing table once rather than per row.
[[nodiscard]] bool HasRowTable(Context& ctx) {
  if (ctx.host.Rows().empty()) {
    ctx.host.Refuse("menu rows without the menu table");
    return false;
  }
  return true;
}
}  // namespace

// decomp: BATTLE.PRG vs_battle_getMenuItem (src/BATTLE/BATTLE.PRG/5BF94.c:2658)
MenuRow& MenuItem(Context& ctx, int row) {
  const auto rows = ctx.host.Rows();
  if (row < 0 || static_cast<std::size_t>(row) >= rows.size()) {
    ctx.host.Refuse(std::format("menu row {} outside the table", row));
    ctx.state.discard_row = MenuRow{};
    return ctx.state.discard_row;
  }
  return rows[static_cast<std::size_t>(row)];
}

// decomp: BATTLE.PRG vs_battle_setMenuItem (src/BATTLE/BATTLE.PRG/5BF94.c:2660)
MenuRow& SetMenuItem(Context& ctx, const RowPlacement& placement,
                     std::span<const std::uint8_t> text) {
  MenuRow& row = MenuItem(ctx, placement.row);
  const MenuRow cleared;
  row.state = RowState::kIdle;
  row.w = static_cast<std::uint8_t>(placement.w);
  row.background_width = static_cast<std::uint8_t>(placement.background_width);
  // The original zeroed the record from gradientState through subText.
  row.gradient_state = cleared.gradient_state;
  row.is_scrollable = cleared.is_scrollable;
  row.selected = cleared.selected;
  row.unselectable = cleared.unselectable;
  row.button_id = cleared.button_id;
  row.row_icon = cleared.row_icon;
  row.font_color = cleared.font_color;
  row.invert_gradient = cleared.invert_gradient;
  row.material = cleared.material;
  row.outset_icon = cleared.outset_icon;
  row.item_page = cleared.item_page;
  row.count = cleared.count;
  row.max = cleared.max;
  row.target0 = cleared.target0;
  row.target1 = cleared.target1;
  row.text = cleared.text;
  row.sub_text = cleared.sub_text;
  row.x = static_cast<std::int16_t>(placement.x);
  row.y = static_cast<std::int16_t>(placement.y);

  std::size_t source = 0;
  const auto next = [&]() -> std::optional<std::uint8_t> {
    if (source >= text.size()) {
      return std::nullopt;
    }
    return text[source++];
  };
  for (std::size_t i = 0; i < kRowTextCharacters;) {
    std::optional<std::uint8_t> c = next();
    if (!c) {
      ctx.host.Refuse("menu row source text without an end");
      row.text[i] = kRowTextEnd;
      break;
    }
    if (*c == kTextSpacing) {
      row.text[i++] = *c;
      c = next();
      if (!c) {
        ctx.host.Refuse("menu row source text without an end");
        row.text[i] = kRowTextEnd;
        break;
      }
    } else if (*c == kTextTerminator) {
      row.text[i] = kRowTextEnd;
      break;
    } else if (*c >= kTextNonPrinting) {
      continue;
    }
    row.text[i++] = *c;
  }
  return row;
}

// decomp: BATTLE.PRG vs_battle_uiGradientStop
// (src/BATTLE/BATTLE.PRG/5BF94.c:2721)
std::uint32_t GradientStopColor(Context& ctx, std::uint32_t gradient,
                                std::uint32_t color_index, int brightness) {
  std::uint32_t color0 = 0;
  std::uint32_t color1 = 0;
  if (color_index < kBlendedGradientStops) {
    const auto blend = static_cast<std::int32_t>(color_index);
    color0 = Render::BlendRgbWithBrightness(kGradientTopFrom, kGradientTopTo,
                                            kFullBrightness, blend);
    color1 = Render::BlendRgbWithBrightness(
        kGradientBottomFrom, kGradientBottomTo, kFullBrightness, blend);
  } else {
    const std::int64_t table =
        static_cast<std::int64_t>(color_index >> kGradientTableShift) -
        kGradientTableBias;
    const auto window = ctx.tables.gradient_colors;
    const std::int64_t top =
        table + static_cast<std::int64_t>(kGradientWindowTop);
    const std::int64_t bottom =
        table + static_cast<std::int64_t>(kGradientWindowBottom);
    if (bottom >= static_cast<std::int64_t>(window.size())) {
      ctx.host.Refuse(
          std::format("gradient colour {} outside the tables", color_index));
      return 0;
    }
    color0 = window[static_cast<std::size_t>(top)];
    color1 = window[static_cast<std::size_t>(bottom)];
  }
  return Render::BlendRgbWithBrightness(color0, color1, brightness,
                                        static_cast<std::int32_t>(gradient));
}

// decomp: BATTLE.PRG vs_battle_renderMenuItem
// (src/BATTLE/BATTLE.PRG/5BF94.c:2737)
void DrawMenuItem(Context& ctx, MenuRow& row) {
  int y = row.gradient_state;
  const int w = row.w;
  const int background_width = row.background_width;
  if (row.state == RowState::kInactive) {
    return;
  }
  if ((row.selected | row.invert_gradient) != 0 && row.unselectable == 0) {
    y = kGradientStops;
  }
  int color = row.font_color;
  if (color == 0) {
    color = row.unselectable * kUnselectablePalette;
  }
  Render::SetMenuFontStyle(
      ctx.host.FontStyle(),
      static_cast<std::uint8_t>(color + kRowFontPaletteBase));
  int pen = row.x + kRowTextIndent;
  if (static_cast<unsigned>(row.row_icon - 1) < kTextIndentingIcons) {
    pen = row.x + kIconRowTextIndent;
  }
  DrawRowGlyphs(ctx, row, pen);

  const auto bg = static_cast<std::uint32_t>(background_width);
  const std::uint32_t color0 =
      GradientStopColor(ctx, Word(kGradientStops - y), bg, kFullBrightness);
  const std::uint32_t color1 =
      GradientStopColor(ctx, Word(y), bg, kFullBrightness);
  if ((background_width & kBackgroundWidthStepMask) != 0) {
    row.background_width = static_cast<std::uint8_t>((background_width + 1) &
                                                     kBackgroundWidthMask);
  }
  if (y != 0) {
    row.gradient_state = static_cast<std::uint8_t>(y - 1);
  }

  const int x = static_cast<std::uint16_t>(row.x);
  const int top = row.y;
  DrawTile(ctx, kRowShadowLayer, PackXy(x + kShadowOffset, top + kShadowOffset),
           PackXy(w, kRowHeight));
  Render::PacketQueue& queue = ctx.host.Queue(kRowLayer);
  std::array<std::uint32_t, kGradientQuadWords> packet{
      0,
      kDrawModeDithered,
      Command(kPolyG4, color0),
      PackXy(x, top),
      color1,
      PackXy(x + w, top),
      color0,
      PackXy(x, top + kRowHeight),
      color1,
      PackXy(x + w, top + kRowHeight),
      kDrawModePlain};
  EmitPacket(queue, packet);

  if (row.sub_text != nullptr) {
    ctx.host.DrawCompactText(
        kRowLayer, row.sub_text,
        Word(x - kSubTextIndent) | (Word(top + 1) << kHalfwordShift),
        Render::kCompactTextGray);
  }
}

// decomp: MAINMENU.PRG vs_mainMenu_deactivateMenuItem
// (src/MENU/MAINMENU.PRG/58EC.c:458)
void DeactivateRow(Context& ctx, int row) {
  MenuItem(ctx, row).state = RowState::kInactive;
}

// decomp: MAINMENU.PRG vs_mainMenu_menuItemFlyoutLeft
// (src/MENU/MAINMENU.PRG/C48.c:226)
void FlyOutRowLeft(Context& ctx, int row) {
  MenuRow& item = MenuItem(ctx, row + kActionRowBase);
  if (item.state != RowState::kInactive) {
    item.state = RowState::kDismiss;
    item.target0 = static_cast<std::int16_t>(-item.w);
  }
}

// decomp: MAINMENU.PRG vs_mainMenu_flyoutLeftExcept
// (src/MENU/MAINMENU.PRG/C48.c:236)
void FlyOutRowsLeftExcept(Context& ctx, int row) {
  constexpr int kActionRows = 8;
  if (!HasRowTable(ctx)) {
    return;
  }
  for (int i = 0; i < kActionRows; ++i) {
    if (i != row) {
      FlyOutRowLeft(ctx, i);
    }
  }
}

// decomp: MAINMENU.PRG vs_mainMenu_menuItemFlyoutRight
// (src/MENU/MAINMENU.PRG/C48.c:247)
void FlyOutRowRight(Context& ctx, int row) {
  MenuRow& item = MenuItem(ctx, row);
  if (item.state != RowState::kInactive) {
    item.state = RowState::kSlideX;
    item.target0 = kScreenWidth;
  }
}

// decomp: MAINMENU.PRG vs_mainMenu_clearMenuExcept
// (src/MENU/MAINMENU.PRG/C48.c:257)
void ClearMenuExcept(Context& ctx, int row) {
  if (!HasRowTable(ctx)) {
    return;
  }
  for (int i = 0; i < kActionRowBase; ++i) {
    if (i != row) {
      FlyOutRowRight(ctx, i);
    }
  }
}

// decomp: MENU5.PRG _clearMenu (src/MENU/MENU5.PRG/4D8.c:201)
void ClearMenuExceptTitle(Context& ctx) {
  constexpr int kTitleRow = 5;
  ClearMenuExcept(ctx, kTitleRow);
}

// decomp: MAINMENU.PRG vs_mainMenu_flyoutMenuRightAndHoistSelection
// (src/MENU/MAINMENU.PRG/C48.c:268)
void FlyOutRowsAndHoistSelection(Context& ctx, int row, int offset) {
  constexpr int kRowsPerPanel = 10;
  constexpr int kPanelRows = 30;
  constexpr int kHoistX = 180;
  if (!HasRowTable(ctx)) {
    return;
  }
  for (int i = offset * kRowsPerPanel; i < kPanelRows; ++i) {
    FlyOutRowRight(ctx, i);
  }
  MenuRow& item = MenuItem(ctx, row + (offset * kRowsPerPanel));
  item.state = RowState::kSlideXy;
  item.target0 = kHoistX;
  item.target1 = static_cast<std::int16_t>((offset * kRowSpacing) + kFirstRowY);
  if (item.background_width == 0) {
    item.background_width = 1;
  }
}

// decomp: MAINMENU.PRG vs_mainmenu_ready (src/MENU/MAINMENU.PRG/C48.c:287)
bool AreMenuRowsIdle(Context& ctx) {
  if (!HasRowTable(ctx)) {
    return true;
  }
  for (int i = 0; i < static_cast<int>(kRowCount); ++i) {
    if (MenuItem(ctx, i).state > RowState::kIdle) {
      return false;
    }
  }
  return true;
}

// decomp: MENU9.PRG _setMenuRow10 (src/MENU/MENU9.PRG/238.c:244)
void ShowSecondaryTitle(Context& ctx, std::span<const std::uint8_t> text) {
  constexpr int kSecondaryTitleRow = 10;
  constexpr int kSecondaryTitleY = 34;
  constexpr int kTitleWidth = 140;
  constexpr int kSecondaryTitleBackground = 9;
  constexpr int kTitleX = 180;
  MenuRow& row = SetMenuItem(ctx,
                             {.row = kSecondaryTitleRow,
                              .x = kScreenWidth,
                              .y = kSecondaryTitleY,
                              .w = kTitleWidth,
                              .background_width = kSecondaryTitleBackground},
                             text);
  row.state = RowState::kSlideX;
  row.target0 = kTitleX;
  row.selected = 1;
  row.font_color = 1;
}

// decomp: MENU _continueMenuTransition (src/MENU/items0.h:107)
void ContinueItemDetailsTransition(Context& ctx, int row) {
  constexpr int kDetailsRow = 31;
  constexpr int kDetailsX = 16;
  constexpr int kDetailsWidth = 164;
  MenuRow& details = MenuItem(ctx, kDetailsRow);
  details.state = RowState::kSlideX;
  details.target0 = kDetailsX;
  details.w = kDetailsWidth;
  MenuRow& item = MenuItem(ctx, row);
  item.state = RowState::kSlideY;
  item.target0 = kFirstRowY;
}

// decomp: MENU _switchItemStatusPage (src/MENU/items1.h:35)
void UpdateItemDetailsSelection(Context& ctx, int row, const RowText& text,
                                std::uint32_t row_type, int page) {
  constexpr int kDetailsRow = 31;
  constexpr int kDetailsX = 155;
  constexpr int kDetailsWidth = 165;
  ctx.host.PlaySound(MenuSound::kChange);
  MenuRow& item = SetMenuItem(
      ctx, {.row = row, .x = kDetailsX, .y = kFirstRowY, .w = kDetailsWidth},
      text.name);
  item.selected = 1;
  item.row_icon = static_cast<std::uint8_t>(row_type >> kRowIconShift);
  item.material =
      static_cast<std::uint8_t>((row_type >> kMaterialShift) & kMaterialMask);
  SetInformationMessage(ctx, text.description);
  MenuItem(ctx, kDetailsRow).item_page = static_cast<std::uint16_t>(page + 1);
}
namespace {
// vs_battle_menuState_t's currentMenu (Menu_Framework_MenuState_*): the low
// five bits name the overlay and 0x40 marks a screen the main menu opened.
constexpr int kMenuClosing = 0x1F;
constexpr int kMenuClosed = 0x3F;
constexpr int kMenuReturning = 0x7F;
constexpr int kOpenedFromMainMenu = 0x40;
constexpr int kSubMenuMask = 0x1F;
constexpr int kMenuMask = 0x3F;
constexpr int kStateIndexMask = 0xF;
constexpr int kQuitMenu = 31;
// The screens opened without the main menu.
constexpr int kItemsMenu = 4;
constexpr int kDataMenu = 7;
constexpr int kLootMenu = 12;
constexpr int kChallengeMenu = 15;
constexpr int kUnlockMenu = 3;
// Menu_Framework_actionSelection.menuStatus.
constexpr std::int16_t kMenuOpen = 1;
constexpr std::int16_t kMenuDone = 2;
// The item shortcut runs MAINMENU.PRG's item screen alone.
constexpr std::uint8_t kItemShortcut = 5;

// The main menu's ten entries and their enabled bits.
constexpr int kMainMenuEntries = 10;
constexpr std::uint32_t kEntryCount = 10;
constexpr std::uint32_t kMainMenuEntryBits = 0x3FF;
constexpr std::uint32_t kShiftCountMask = 31;
constexpr int kOpenMainMenu = 2;
constexpr int kFirstSlideStep = 9;
constexpr int kMainMenuX = 194;
constexpr int kMainMenuWidth = 126;
constexpr int kRowSpacingShift = 4;
constexpr int kEntryTitleX = 180;
constexpr int kEntryTitleWidth = 140;
constexpr int kEntryTitleBackground = 8;
// The restrictions that disable an entry and the strings naming them.
constexpr std::int32_t kMagicRestrictions = 0xB7;
constexpr std::int32_t kBreakArtRestrictions = 0x15F;
constexpr std::int32_t kInventoryRestrictions = 7;
constexpr int kRestrictionReasons = 9;
constexpr int kRestrictionStrings = 12;
// The action restrictions: opened by an event, and the container save.
constexpr std::int32_t kOpenedByEvent = 0x200;
constexpr std::int32_t kContainerSave = 0x400;
// Menu_textBoxes[7].state.
constexpr std::uint8_t kInformationBoxShown = 11;
constexpr std::uint8_t kInformationBoxHidden = 0;
// _renderMenuCommand's labels (Menu_Framework_MenuCommand).
constexpr int kLabelNone = 0;
constexpr int kLabelCommand = 1;
constexpr int kLabelMenu = 2;
constexpr int kFadeLimit = 16;
constexpr int kFadeStep = 2;
constexpr int kStatusBackgroundOt = -2;
constexpr int kMenuBackgroundOt = -4;
// vs_battle_drawCursor's cursor beside the main menu.
constexpr int kMainCursorX = 180;
constexpr int kMainCursorY = 10;
constexpr int kClutRow = 223;
constexpr int kCursorUvU = 32;
constexpr int kCursorUvV = 48;
constexpr int kCursorClut = 896;
constexpr int kAlternateCursorClut = 928;

[[nodiscard]] constexpr int Entry(Submenu submenu) {
  return static_cast<int>(submenu);
}

[[nodiscard]] int CursorBrightness(Context& ctx, int step) {
  const auto table = ctx.tables.cursor_brightness;
  if (step < 0 || static_cast<std::size_t>(step) >= table.size()) {
    ctx.host.Refuse(std::format("cursor step {} outside the table", step));
    return 0;
  }
  return table[static_cast<std::size_t>(step)];
}

// The MIPS shift takes the low five bits of its count.
[[nodiscard]] bool IsEntryEnabled(std::uint32_t enabled, std::uint32_t entry) {
  return ((enabled >> (entry & kShiftCountMask)) & 1U) != 0;
}

// The first enabled entry from `entry` on, wrapping over the ten entries.
[[nodiscard]] std::optional<std::uint32_t> NextEnabledEntry(
    Context& ctx, std::uint32_t enabled, std::uint32_t entry) {
  if (!IsEntryEnabled(enabled, entry) && (enabled & kMainMenuEntryBits) == 0) {
    ctx.host.Refuse("main menu without an enabled entry");
    return std::nullopt;
  }
  while (!IsEntryEnabled(enabled, entry)) {
    entry = (entry + 1) % kEntryCount;
  }
  return entry;
}

[[nodiscard]] std::optional<std::uint32_t> PreviousEnabledEntry(
    Context& ctx, std::uint32_t enabled, std::uint32_t entry) {
  if ((enabled & kMainMenuEntryBits) == 0) {
    ctx.host.Refuse("main menu without an enabled entry");
    return std::nullopt;
  }
  do {
    entry = (entry + kEntryCount - 1) % kEntryCount;
  } while (!IsEntryEnabled(enabled, entry));
  return entry;
}

[[nodiscard]] std::int32_t EntryRestrictions(int entry) {
  if (entry == Entry(Submenu::kMagic)) {
    return kMagicRestrictions;
  }
  if (entry == Entry(Submenu::kBreakArts)) {
    return kBreakArtRestrictions;
  }
  if (entry == Entry(Submenu::kInventory)) {
    return kInventoryRestrictions;
  }
  return 0;
}

void MarkRestrictedEntries(Context& ctx) {
  const std::int32_t restrictions = ctx.state.action_restrictions;
  for (const Submenu entry :
       {Submenu::kMagic, Submenu::kBreakArts, Submenu::kInventory}) {
    MenuItem(ctx, Entry(entry)).unselectable = static_cast<std::uint8_t>(
        (restrictions & EntryRestrictions(Entry(entry))) != 0);
  }
  MenuItem(ctx, Entry(Submenu::kMap)).unselectable =
      static_cast<std::uint8_t>(ctx.host.CurrentSceneId() == 0);
}

// UpdateMainMenuSelection(1 or 2): the menu table cleared and the enabled
// entries' slide steps set.
void PrepareMainMenu(Context& ctx, int mode, const Settings& settings) {
  std::uint32_t selected = ctx.state.submenu_states[0];
  if (mode == kOpenMainMenu && settings.cursor_memory == 0) {
    selected = 0;
  }
  const auto first = NextEnabledEntry(ctx, settings.menu_flags, selected);
  if (!first) {
    return;
  }
  ctx.state.submenu_states[0] = static_cast<std::uint8_t>(*first);
  std::ranges::fill(ctx.host.Rows(), MenuRow{});
  std::ranges::fill(ctx.host.Strings(), std::uint8_t{0});
  std::ranges::fill(ctx.host.RowTypes(), 0);
  int step = kFirstSlideStep;
  for (int entry = 0; entry < kMainMenuEntries; ++entry) {
    if (IsEntryEnabled(settings.menu_flags, Word(entry))) {
      MenuItem(ctx, entry).animation_step = static_cast<std::uint8_t>(step++);
    }
  }
  ctx.state.main_menu_cursor_animation = -step;
}

void SlideInMainMenu(Context& ctx, std::uint32_t enabled) {
  ++ctx.state.main_menu_cursor_animation;
  int shown = 0;
  for (int entry = 0; entry < kMainMenuEntries; ++entry) {
    if (!IsEntryEnabled(enabled, Word(entry))) {
      continue;
    }
    MenuRow& row = MenuItem(ctx, entry);
    int step = row.animation_step;
    if (step != 0) {
      --step;
      row.animation_step = static_cast<std::uint8_t>(step);
    }
    SetMenuItem(ctx,
                {.row = entry,
                 .x = AnimationStep(ctx, step) + kMainMenuX,
                 .y = (shown * kRowSpacing) + kFirstRowY,
                 .w = kMainMenuWidth},
                MenuString(ctx, entry));
    ++shown;
    MarkRestrictedEntries(ctx);
  }
}

// The caution for a disabled entry: its first restriction names the reason.
void ExplainRestriction(Context& ctx, std::uint32_t entry, std::uint32_t held) {
  if ((held & (kPadLUp | kPadLDown)) != 0) {
    return;
  }
  ctx.host.InitInformationBox(InformationHeader::kCaution);
  ctx.host.SetInformationBoxState(kInformationBoxShown);
  ctx.host.SetInformationText(ctx.tables.main_menu_help_text);
  const std::int32_t conditions = ctx.state.action_restrictions &
                                  EntryRestrictions(static_cast<int>(entry));
  int reason = 0;
  while (reason < kRestrictionReasons && (conditions & (1 << reason)) == 0) {
    ++reason;
  }
  ctx.host.SetStringArgument(MenuString(ctx, reason + kRestrictionStrings));
}

int SelectMainMenuEntry(Context& ctx, std::uint32_t enabled) {
  const Buttons buttons = ctx.host.Input();
  std::uint32_t selected = ctx.state.submenu_states[0];
  MenuRow& previous_row = MenuItem(ctx, static_cast<int>(selected));
  if ((buttons.pressed & kPadRRight) != 0) {
    ctx.host.PlaySound(MenuSound::kSelect);
    previous_row.selected = 1;
    previous_row.background_width = 1;
    ctx.host.SetInformationBoxState(kInformationBoxHidden);
    return static_cast<int>(selected) + 1;
  }
  previous_row.selected = 0;
  if ((buttons.pressed & (kPadRUp | kPadRDown)) != 0) {
    ctx.host.PlaySound(MenuSound::kLeave);
    ctx.host.SetInformationBoxState(kInformationBoxHidden);
    return -1;
  }
  if ((buttons.repeat & kPadLUp) != 0) {
    const auto previous = PreviousEnabledEntry(ctx, enabled, selected);
    if (!previous) {
      return 0;
    }
    selected = *previous;
  }
  if ((buttons.repeat & kPadLDown) != 0) {
    selected = (selected + 1) % kEntryCount;
  }
  const auto next = NextEnabledEntry(ctx, enabled, selected);
  if (!next) {
    return 0;
  }
  selected = *next;
  if (selected != ctx.state.submenu_states[0]) {
    ctx.host.PlaySound(MenuSound::kChange);
    ctx.state.submenu_states[0] = static_cast<std::uint8_t>(selected);
  }
  MenuRow& row = MenuItem(ctx, static_cast<int>(selected));
  row.selected = 1;
  ctx.state.main_menu_cursor_animation =
      DrawMainMenuCursor(ctx, ctx.state.main_menu_cursor_animation,
                         (row.y >> kRowSpacingShift) - 1);
  if (row.unselectable != 0) {
    ExplainRestriction(ctx, selected, buttons.held);
  } else {
    ctx.host.SetInformationBoxState(kInformationBoxHidden);
  }
  return 0;
}

// The screens a menu opened without the main menu runs (restriction 0x200).
struct EventScreen {
  int menu = 0;
  Submenu submenu = Submenu::kResults;
};

constexpr std::array<EventScreen, 6> kEventScreens{{
    {.menu = kChallengeMenu, .submenu = Submenu::kResults},
    {.menu = kUnlockMenu, .submenu = Submenu::kUnlockNotifications},
    {.menu = 14, .submenu = Submenu::kContainer},
    {.menu = 8, .submenu = Submenu::kRename},
    {.menu = 13, .submenu = Submenu::kSetup},
    {.menu = kLootMenu, .submenu = Submenu::kLoot},
}};

[[nodiscard]] std::optional<Submenu> EventSubmenu(Context& ctx, int menu) {
  for (const EventScreen& screen : kEventScreens) {
    if (screen.menu == menu) {
      return screen.submenu;
    }
  }
  if (menu == kDataMenu) {
    if ((ctx.state.action_restrictions & kContainerSave) != 0) {
      return Submenu::kContainerSave;
    }
    if (ctx.host.Story().game_over == 1) {
      return Submenu::kGameOver;
    }
  }
  return std::nullopt;
}

[[nodiscard]] int RunEventScreen(Context& ctx, int menu, int state_index,
                                 bool loaded) {
  if (!loaded) {
    return 0;
  }
  const auto submenu = EventSubmenu(ctx, menu);
  if (!submenu) {
    ctx.state.action_restrictions = 0;
    return 0;
  }
  return ctx.host.RunSubmenu(*submenu, state_index);
}

// vs_mainMenu_exec's first step: the entry's title slides to the top right.
void OpenMainMenuEntry(Context& ctx, int packed_menu_state, int row) {
  const int entry = (packed_menu_state & kMenuMask) - 1;
  if ((packed_menu_state & kOpenedFromMainMenu) == 0) {
    int unselectable = 0;
    if (entry == Entry(Submenu::kMagic) ||
        entry == Entry(Submenu::kBreakArts)) {
      unselectable = static_cast<int>(
          (ctx.state.action_restrictions & EntryRestrictions(entry)) != 0);
    }
    SetMenuItem(ctx,
                {.row = entry,
                 .x = kScreenWidth,
                 .y = kFirstRowY,
                 .w = kEntryTitleWidth,
                 .background_width = kEntryTitleBackground},
                MenuString(ctx, entry))
        .unselectable = static_cast<std::uint8_t>(unselectable);
  }
  if (entry == Entry(Submenu::kInventory) || entry == Entry(Submenu::kStatus) ||
      entry == Entry(Submenu::kData) || entry == Entry(Submenu::kScore)) {
    SetBackgroundFade(ctx, 1);
  }
  SetCommandLabel(ctx, kLabelMenu);
  ClearMenuExcept(ctx, entry);
  MenuRow& title = MenuItem(ctx, row);
  title.state = RowState::kSlideX;
  title.target0 = kEntryTitleX;
  title.selected = 1;
  if (entry == Entry(Submenu::kStatus)) {
    SetBackgroundPriority(ctx, kStatusBackgroundOt, kFullBrightness);
    title.target0 = kScreenWidth;
  }
}

// The second step: the title rises once idle; the status screen also
// slides in Ashley's name.
[[nodiscard]] bool RaiseMainMenuEntry(Context& ctx, int packed_menu_state,
                                      int row) {
  const int entry = (packed_menu_state & kMenuMask) - 1;
  MenuRow& title = MenuItem(ctx, row);
  if (title.state != RowState::kIdle) {
    return false;
  }
  title.state = RowState::kSlideY;
  title.target0 = kFirstRowY;
  if (entry == Entry(Submenu::kStatus)) {
    MenuRow& name = SetMenuItem(ctx,
                                {.row = Entry(Submenu::kStatus),
                                 .x = kScreenWidth,
                                 .y = kFirstRowY,
                                 .w = kEntryTitleWidth,
                                 .background_width = kEntryTitleBackground},
                                ctx.host.AshleyName());
    name.state = RowState::kSlideX;
    name.target0 = kEntryTitleX;
    name.selected = 1;
  }
  return true;
}

[[nodiscard]] int RunMainMenuEntry(Context& ctx, int packed_menu_state,
                                   bool loaded) {
  const int menu = packed_menu_state & kMenuMask;
  const int state_index = packed_menu_state & kStateIndexMask;
  const int row = (packed_menu_state - 1) & kStateIndexMask;
  std::uint8_t& submenu_state =
      ctx.state.submenu_states[static_cast<std::size_t>(state_index)];
  if (submenu_state == 0) {
    OpenMainMenuEntry(ctx, packed_menu_state, row);
    submenu_state = 1;
    return 0;
  }
  if (submenu_state == 1) {
    if (!RaiseMainMenuEntry(ctx, packed_menu_state, row)) {
      return 0;
    }
    constexpr std::uint8_t kEntryRunning = 3;
    submenu_state = kEntryRunning;
  }
  if (!loaded) {
    return 0;
  }
  if (menu > kMainMenuEntries) {
    ctx.host.Refuse(std::format("main menu entry {} has no screen", menu));
    return 0;
  }
  return ctx.host.RunSubmenu(static_cast<Submenu>(menu - 1), state_index);
}

void SelectFromMainMenu(Context& ctx) {
  SetCommandLabel(ctx, kLabelCommand);
  int state = UpdateMainMenuSelection(ctx, 0);
  if (state == 0) {
    return;
  }
  if (state > 0) {
    state |= kOpenedFromMainMenu;
  } else {
    state = kMenuClosing;
    SetCommandLabel(ctx, kLabelNone);
  }
  ctx.state.control.current_menu = static_cast<std::uint8_t>(state);
}
}  // namespace

// decomp: BATTLE.PRG vs_battle_loadMenuPrg (src/BATTLE/BATTLE.PRG/5BF94.c:2576)
int LoadMenuOverlay(Context& ctx, int packed_menu_state) {
  const int sub_menu = packed_menu_state & kSubMenuMask;
  if (sub_menu == kQuitMenu) {
    return 1;
  }
  if (sub_menu == 0 && ctx.state.loaded_sub_menu > 0) {
    return 1;
  }
  if (ctx.state.loaded_sub_menu != sub_menu) {
    ctx.state.loaded_sub_menu = static_cast<std::int8_t>(sub_menu);
    ctx.state.control.loading = 1;
    ctx.host.BeginOverlayLoad(sub_menu,
                              ctx.state.shortcut_invoked == kItemShortcut);
  }
  if (ctx.state.control.loading == 0) {
    if (ctx.host.ReleaseBackgroundBuffer()) {
      ctx.host.StoreEquippedStats();
    }
    return 1;
  }
  if (!ctx.host.IsOverlayLoaded()) {
    return 0;
  }
  ctx.host.FinishOverlayLoad();
  if (sub_menu == 0) {
    ctx.state.main_menu = MainMenuStatics{};
    ctx.host.ResetMainMenuOverlay();
  }
  ctx.state.control.loading = 0;
  if (ctx.state.opened_without_main_menu != 0) {
    if (ctx.state.shortcut_invoked == kItemShortcut) {
      ctx.host.UpdateMiscItemShortcut(1);
      return 0;
    }
    return 1;
  }
  if (sub_menu == 0) {
    LoadBackgroundTexture(ctx, ctx.host.AllocateBackgroundBuffer());
  }
  return 0;
}

// decomp: BATTLE.PRG func_800C930C (src/BATTLE/BATTLE.PRG/5BF94.c:2829)
int UpdateMainMenuSelection(Context& ctx, int mode) {
  const Settings settings = ctx.host.MenuSettings();
  if (mode != 0) {
    PrepareMainMenu(ctx, mode, settings);
    return 0;
  }
  if (ctx.state.main_menu_cursor_animation < 0) {
    SlideInMainMenu(ctx, settings.menu_flags);
    return 0;
  }
  return SelectMainMenuEntry(ctx, settings.menu_flags);
}

// decomp: BATTLE.PRG func_800C97BC (src/BATTLE/BATTLE.PRG/5BF94.c:2966)
void UpdateMenuOverlay(Context& ctx) {
  int packed_menu_state = ctx.state.control.current_menu;
  if (packed_menu_state == kMenuReturning) {
    ctx.state.action_selection.menu_status = kMenuDone;
    ctx.state.control.current_menu = kMenuClosed;
  }
  const int selected = packed_menu_state & kMenuMask;
  if (selected == kMenuClosed) {
    return;
  }
  if (LoadMenuOverlay(ctx, 0) != 0) {
    if (ctx.state.shortcut_invoked == kItemShortcut) {
      ctx.host.UpdateMiscItemShortcut(0);
      return;
    }
    UpdateMainMenu(ctx, packed_menu_state);
    return;
  }
  if (selected == 0) {
    const int result = UpdateMainMenuSelection(ctx, 0);
    if (result != 0) {
      packed_menu_state =
          result > 0 ? (result | kOpenedFromMainMenu) : kMenuClosing;
      ctx.state.control.current_menu =
          static_cast<std::uint8_t>(packed_menu_state);
    }
  }
  if (packed_menu_state >= kMenuClosing || selected == 0) {
    for (int i = 0; i < kMainMenuEntries; ++i) {
      DrawMenuItem(ctx, MenuItem(ctx, i));
    }
  }
}

// decomp: BATTLE.PRG vs_battle_drawCursor (src/BATTLE/BATTLE.PRG/5BF94.c:4523)
int DrawMainMenuCursor(Context& ctx, int step, int position) {
  EmitSprite(
      ctx, kOverlayFront,
      {.brightness = CursorBrightness(ctx, step),
       .position =
           (Word((position * kRowSpacing) + kMainCursorY) << kHalfwordShift) |
           Word(kMainCursorX),
       .size = PackXy(kIconSize, kIconSize),
       .texture = PackUvClut(kCursorUvU, kCursorUvV, kCursorClut, kClutRow)});
  return (step + 1) & static_cast<int>(kStepMask);
}

// decomp: MAINMENU.PRG _quitDelay (src/MENU/MAINMENU.PRG/C48.c:305)
int QuitDelay(Context& ctx) {
  constexpr int kQuitFrames = 5;
  const int frames = ++ctx.state.main_menu.quit_delay;
  ClearMenuExcept(ctx, kNoRow);
  return static_cast<int>(frames == kQuitFrames);
}

// decomp: MAINMENU.PRG func_800FAA5C (src/MENU/MAINMENU.PRG/C48.c:316)
int ReturnFromSubmenu(Context& ctx, int packed_menu_state) {
  if ((packed_menu_state & kOpenedFromMainMenu) != 0) {
    ctx.state.control.current_menu = 0;
    UpdateMainMenuSelection(ctx, 1);
    return 0;
  }
  if (ctx.state.opened_without_main_menu == 0) {
    ctx.host.RetireMenuTexturePages();
  }
  ctx.state.control.current_menu = kMenuReturning;
  return 1;
}

// decomp: MAINMENU.PRG vs_mainMenu_exec (src/MENU/MAINMENU.PRG/C48.c:334)
void UpdateMainMenu(Context& ctx, int packed_menu_state) {
  ctx.host.BindInventoryViews();
  const int state_index = packed_menu_state & kStateIndexMask;
  const bool loaded = LoadMenuOverlay(ctx, packed_menu_state) != 0;
  const int selected = packed_menu_state & kMenuMask;
  int submenu_return = 0;
  if (selected == 0) {
    SelectFromMainMenu(ctx);
  } else if (selected == kQuitMenu) {
    submenu_return = QuitDelay(ctx);
  } else if ((ctx.state.action_restrictions & kOpenedByEvent) != 0) {
    submenu_return = RunEventScreen(ctx, selected, state_index, loaded);
  } else {
    submenu_return = RunMainMenuEntry(ctx, packed_menu_state, loaded);
  }
  if (submenu_return == 0 ||
      ReturnFromSubmenu(ctx, ctx.state.control.current_menu) == 0) {
    UpdateScreen(ctx);
  }
}

// decomp: MAINMENU.PRG vs_mainMenu_renderScreen
// (src/MENU/MAINMENU.PRG/58EC.c:1709)
void UpdateScreen(Context& ctx) {
  int fade = ctx.state.main_menu.background_fade_step;
  if (ctx.host.HasItems(ItemList::kRows)) {
    UpdateRowSelection(ctx);
  }
  if (ctx.state.main_menu.background_fade_state != 0) {
    fade = std::min(fade + kFadeStep, kFadeLimit);
  } else {
    fade = std::max(fade - kFadeStep, 0);
  }
  ctx.state.main_menu.background_fade_step = fade;
  if (fade != 0) {
    DrawBackground(ctx, fade);
  }
  if (ctx.state.shortcut_invoked == 0) {
    DrawCommandLabel(ctx);
  }
  if (!ctx.host.Rows().empty()) {
    UpdateAndDrawRows(ctx);
  }
  DrawActionCostPanel(ctx);
  ctx.host.UpdateStatPanels();
}

// decomp: MENU5.PRG _setNextAction (src/MENU/MENU5.PRG/4D8.c:90)
void SetMenuVisible(Context& ctx, int display) {
  if (display != 0) {
    SetCommandLabel(ctx, kLabelMenu);
    ctx.state.main_menu.hide_menu = 0;
    return;
  }
  SetCommandLabel(ctx, kLabelNone);
  ctx.state.main_menu.hide_menu = 1;
}
namespace {
// InitGameInterface's three images: position, source offset and size.
struct InterfaceImage {
  std::uint32_t position = 0;
  std::size_t offset = 0;
  std::uint32_t size = 0;
};

constexpr std::array<InterfaceImage, 3> kInterfaceImages{{
    {.position = PackXy(832, 0), .offset = 0, .size = PackXy(64, 224)},
    {.position = PackXy(896, 0), .offset = 0x7000, .size = PackXy(64, 224)},
    {.position = PackXy(960, 66), .offset = 0xE000, .size = PackXy(64, 158)},
}};
constexpr std::size_t kBytesPerPixel = 2;

[[nodiscard]] constexpr std::size_t ImageBytes(std::uint32_t size) {
  return static_cast<std::size_t>(size & kHalfwordMask) *
         (size >> kHalfwordShift) * kBytesPerPixel;
}

// VS::Actors::LiveActorStateFlags(0) and Ashley's statuses
// (Battle_StatusFlag_*).
constexpr std::uint32_t kStateLocked = 0x80000;
constexpr std::uint32_t kStateLimitMask = 0x40100000;
constexpr std::uint32_t kStateLimited = 0x100000;
constexpr std::uint32_t kStatusStrDown = 1U << 0;
constexpr std::uint32_t kStatusRegen = 1U << 12;
constexpr std::uint32_t kStatusWard = 1U << 13;
// The restriction bits ActionRestrictionFlags returns.
constexpr std::int32_t kRestrictionLocked = 2;
constexpr std::int32_t kRestrictionLimited = 4;
constexpr std::int32_t kRestrictionSheathed = 8;
constexpr std::int32_t kRestrictionNoMp = 0x20;
constexpr std::int32_t kRestrictionNoHp = 0x40;
constexpr std::int32_t kRestrictionRegenOrStrDown = 0x80;
constexpr std::int32_t kRestrictionWard = 0x100;
constexpr int kEventMode = 2;

// The ability groups the main menu enables (magic, break arts, battle
// abilities) and the story's extra entries.
constexpr int kAbilityGroups = 3;
constexpr int kIntroDone = 3;
constexpr std::uint32_t kStoryEntries = 0x120;
// The shortcuts: spell classes 1..4, the items, break arts, abilities 7..8.
constexpr int kLastSpellShortcut = 4;
constexpr int kBreakArtShortcut = 6;
constexpr int kFirstAbilityShortcut = 7;
constexpr int kLastAbilityShortcut = 8;
constexpr int kChestLocationBase = 0x40;

void PrepareWorkspace(Context& ctx) {
  ctx.state.loaded_sub_menu = -1;
  if (!ctx.host.AllocateWorkspace()) {
    ctx.host.Refuse("menu workspace allocation failed");
    return;
  }
  auto flags = static_cast<std::uint16_t>(ctx.host.MenuSettings().menu_flags);
  const auto ranges = ctx.tables.ability_group_action_ranges;
  for (int group = 0; group < kAbilityGroups; ++group) {
    const auto at = static_cast<std::size_t>(group) * 2;
    if (at + 1 >= ranges.size()) {
      ctx.host.Refuse("ability group outside the table");
      break;
    }
    for (int action = ranges[at]; action < ranges[at + 1]; ++action) {
      if (ctx.host.IsActionUnlocked(action)) {
        flags = static_cast<std::uint16_t>(flags | (1U << group));
        break;
      }
    }
  }
  const StoryFlags story = ctx.host.Story();
  if (story.opaque_b5 != 0) {
    flags = static_cast<std::uint16_t>(flags | 1U);
  }
  if (story.intro_state >= kIntroDone) {
    flags = static_cast<std::uint16_t>(flags | kStoryEntries);
  }
  ctx.host.SetMenuFlags(flags);
  UpdateMainMenuSelection(ctx, kOpenMainMenu);
}
}  // namespace

// decomp: BATTLE.PRG func_800CA9C0 (src/BATTLE/BATTLE.PRG/5BF94.c:3484)
void InitGameInterface(Context& ctx,
                       std::span<const std::byte> interface_texture) {
  ctx.host.ResetBattleInterface();
  ctx.state.loaded_sub_menu = 0;
  ctx.state.unused_interface_flag = 0;
  ctx.host.ForgetWorkspace();
  ctx.host.ForgetBackgroundBuffer();
  ctx.state.control = MenuControl{};
  ctx.host.ForgetOverlayLoad();
  ctx.state.control.current_menu = kMenuClosed;
  ctx.state.action_selection = ActionSelection{};
  ctx.state.submenu_states = {};
  std::ranges::fill(ctx.host.MenuMemory(), std::uint8_t{0});
  if (interface_texture.size() < kInterfaceTextureBytes) {
    ctx.host.Refuse("interface texture shorter than its three images");
    return;
  }
  for (const InterfaceImage& image : kInterfaceImages) {
    ctx.host.UploadImage(
        image.position,
        interface_texture.subspan(image.offset, ImageBytes(image.size)),
        image.size);
  }
}

// decomp: BATTLE.PRG func_800CAB40 (src/BATTLE/BATTLE.PRG/5BF94.c:3518)
void ResumeAfterActionSelection(Context& ctx) {
  ctx.state.action_selection.selection_kind = 0;
  ctx.state.action_selection.menu_status = kMenuOpen;
  if (ctx.state.shortcut_invoked == kItemShortcut) {
    ctx.state.control.current_menu = kItemsMenu;
    ctx.host.UpdateMiscItemShortcut(2);
  } else {
    const std::uint8_t return_state = ctx.state.control.return_state;
    ctx.state.control.current_menu = return_state;
    ctx.state.submenu_states[static_cast<std::size_t>(return_state &
                                                      kStateIndexMask)] =
        ctx.state.control.execute_ability_type;
  }
  if (ctx.state.opened_without_main_menu == 0) {
    ctx.host.ReserveMenuTexturePages();
  }
}

// decomp: BATTLE.PRG func_800CABE0 (src/BATTLE/BATTLE.PRG/5BF94.c:3535)
std::int32_t RestrictionFlagsOf(const RestrictionInputs& inputs, int mode) {
  std::int32_t flags = 0;
  if ((inputs.state_flags & kStateLocked) != 0) {
    flags = kRestrictionLocked;
  }
  if ((inputs.state_flags & kStateLimitMask) == kStateLimited) {
    flags |= kRestrictionLimited;
  }
  if (!inputs.weapon_drawn) {
    flags |= kRestrictionSheathed;
  }
  if (inputs.mp == 0) {
    flags |= kRestrictionNoMp;
  }
  if (inputs.hp == 0) {
    flags |= kRestrictionNoHp;
  }
  if ((inputs.statuses & (kStatusRegen | kStatusStrDown)) != 0) {
    flags |= kRestrictionRegenOrStrDown;
  }
  if ((inputs.statuses & kStatusWard) != 0) {
    flags |= kRestrictionWard;
  }
  if (mode == kEventMode) {
    flags |= kOpenedByEvent;
  }
  return flags;
}

// decomp: BATTLE.PRG func_800CABE0 (src/BATTLE/BATTLE.PRG/5BF94.c:3535)
std::int32_t ActionRestrictionFlags(Context& ctx, int mode) {
  const auto inputs = ctx.host.Restrictions();
  if (!inputs) {
    ctx.host.Refuse("action restrictions without Ashley");
    return mode == kEventMode ? kOpenedByEvent : 0;
  }
  return RestrictionFlagsOf(*inputs, mode);
}

// decomp: BATTLE.PRG func_800CACD0 (src/BATTLE/BATTLE.PRG/5BF94.c:3575)
int OpenGameMenu(Context& ctx, int menu_state, int mode) {
  if (ctx.state.control.current_menu != kMenuClosed) {
    ctx.host.PlaySound(MenuSound::kInvalid);
    return 0;
  }
  ctx.state.shortcut_invoked = 0;
  ctx.state.opened_without_main_menu = 0;
  ctx.state.action_restrictions = ActionRestrictionFlags(ctx, mode);
  ctx.state.action_selection.selection_kind = 0;
  ctx.state.control.current_menu = static_cast<std::uint8_t>(menu_state);
  ctx.state.action_selection.menu_status = kMenuOpen;
  if (mode == 0) {
    ctx.host.PlaySound(MenuSound::kOpen);
  }
  if (ctx.host.Rows().empty()) {
    PrepareWorkspace(ctx);
  }
  return 1;
}

// decomp: BATTLE.PRG vs_battle_validateShortcutSelection
// (src/BATTLE/BATTLE.PRG/5BF94.c:3673)
int OpenShortcutMenu(Context& ctx, int shortcut) {
  bool none_unlocked = false;
  int menu_state = 0;
  if (shortcut >= 1 && shortcut <= kLastSpellShortcut) {
    none_unlocked = !ctx.host.HasUnlockedSpellClass(shortcut - 1);
    menu_state = Entry(Submenu::kMagic) + 1;
  } else if (shortcut == kItemShortcut) {
    menu_state = kItemsMenu;
  } else if (shortcut == kBreakArtShortcut) {
    none_unlocked = !ctx.host.HasUnlockedWeaponBreakArt();
    menu_state = Entry(Submenu::kBreakArts) + 1;
  } else if (shortcut >= kFirstAbilityShortcut &&
             shortcut <= kLastAbilityShortcut) {
    none_unlocked =
        !ctx.host.HasUnlockedBattleAbility(shortcut - kFirstAbilityShortcut);
    menu_state = Entry(Submenu::kBattleAbilities) + 1;
  }
  if (none_unlocked) {
    ctx.host.PlaySound(MenuSound::kInvalid);
    ctx.state.action_selection.selection_kind = 0;
    ctx.state.action_selection.menu_status = kMenuDone;
    return 0;
  }
  const int opened = OpenGameMenu(ctx, menu_state, 0);
  ctx.state.shortcut_invoked = static_cast<std::uint8_t>(shortcut);
  ctx.state.opened_without_main_menu = 1;
  return opened;
}

// decomp: BATTLE.PRG func_800CB114 (src/BATTLE/BATTLE.PRG/5BF94.c:3714)
void ReleaseMenuRows(Context& ctx) {
  if (!ctx.host.Rows().empty()) {
    ctx.host.ReleaseWorkspace();
  }
}

// decomp: BATTLE.PRG func_800CB158 (src/BATTLE/BATTLE.PRG/5BF94.c:3723)
void OpenLootListMenu(Context& ctx, LootListId lists) {
  ctx.state.loot_lists = lists;
  ctx.state.loot_entry = LootEntryId{};
  OpenGameMenu(ctx, kLootMenu, kEventMode);
}

// decomp: BATTLE.PRG func_800CB18C (src/BATTLE/BATTLE.PRG/5BF94.c:3730)
void OpenLootEntryMenu(Context& ctx, LootEntryId entry) {
  ctx.state.loot_entry = entry;
  ctx.state.loot_lists = LootListId{};
  OpenGameMenu(ctx, kLootMenu, kEventMode);
}

// decomp: BATTLE.PRG func_800CB1C0 (src/BATTLE/BATTLE.PRG/5BF94.c:3737)
void OpenChestStorageMenu(Context& ctx, int location) {
  OpenGameMenu(ctx, kDataMenu, kEventMode);
  ctx.state.save_location_id =
      static_cast<std::uint8_t>(location + kChestLocationBase);
  ctx.state.action_restrictions |= kContainerSave;
}

// decomp: BATTLE.PRG _setArtAndAbilityToUnlock
// (src/BATTLE/BATTLE.PRG/5BF94.c:3744)
void OpenAbilityUnlockMenu(Context& ctx, int art, int ability) {
  ctx.state.unlocked_break_art = static_cast<std::uint8_t>(art);
  ctx.state.unlocked_battle_ability = static_cast<std::uint8_t>(ability);
  OpenGameMenu(ctx, kUnlockMenu, kEventMode);
}

// decomp: BATTLE.PRG func_800CB23C (src/BATTLE/BATTLE.PRG/5BF94.c:3751)
void OpenChallengeMenu(Context& ctx) {
  OpenGameMenu(ctx, kChallengeMenu, kEventMode);
  ctx.state.opened_without_main_menu = 1;
}

// decomp: BATTLE.PRG func_800CB66C (src/BATTLE/BATTLE.PRG/5BF94.c:3871)
ActionSelection& ActionSelectionState(Context& ctx) {
  return ctx.state.action_selection;
}

namespace {
// The action menu's seven visible rows and its page offset below the menu.
constexpr int kVisibleActions = 7;
constexpr int kListRows = 10;
constexpr int kLongActionListOffset = 3;
constexpr int kCountMask = 0xFF;
constexpr int kDiscardActions = 4;
constexpr int kActionsWithoutMemory = 3;
constexpr int kActionSlideX = -kMainMenuX;
constexpr int kCursorRaise = 8;
// vs_battle_menuItem_t's isScrollable (Menu_Framework_MenuItemScroll_*).
constexpr std::uint8_t kScrollUp = 1;
constexpr std::uint8_t kScrollDown = 2;
// The list states after the rows are built: sliding in, then ready.
constexpr std::uint8_t kListBuilt = 16;
constexpr std::uint8_t kListReady = 17;
// The results of a closed list: Rup (back) and Rdown (cancel).
constexpr int kListBack = -3;
constexpr int kListCancelled = -2;

// The cursor of a list: the selected row, its page offset, the item count
// and the visible rows.
struct ListCursor {
  std::int32_t& selected;
  std::uint8_t& page;
  int count = 0;
  int visible = 0;
};

void MoveCursorUp(const ListCursor& cursor, std::uint32_t pressed) {
  if (cursor.count <= cursor.visible) {
    cursor.selected =
        cursor.selected == 0 ? cursor.count - 1 : cursor.selected - 1;
    return;
  }
  if (cursor.page == 0) {
    if (cursor.selected != 0) {
      --cursor.selected;
    } else if ((pressed & kPadLUp) != 0) {
      cursor.selected = cursor.visible - 1;
      cursor.page = static_cast<std::uint8_t>(cursor.count - cursor.visible);
    }
  } else if (cursor.selected == 1) {
    --cursor.page;
  } else {
    --cursor.selected;
  }
}

void MoveCursorDown(const ListCursor& cursor, std::uint32_t pressed) {
  if (cursor.count <= cursor.visible) {
    cursor.selected =
        cursor.selected == cursor.count - 1 ? 0 : cursor.selected + 1;
    return;
  }
  if (cursor.page == cursor.count - cursor.visible) {
    if (cursor.selected != cursor.visible - 1) {
      ++cursor.selected;
    } else if ((pressed & kPadLDown) != 0) {
      cursor.selected = 0;
      cursor.page = 0;
    }
  } else if (cursor.selected == cursor.visible - 2) {
    ++cursor.page;
  } else {
    ++cursor.selected;
  }
}

// A scrolled page: the rows are rebuilt and keep their items' gradients.
struct Relayout {
  int count = 0;
  int visible = 0;
  int row_base = 0;
  int previous_page = 0;
  int page = 0;
};

template <typename CreateRow>
void RelayoutRows(Context& ctx, const Relayout& layout,
                  const CreateRow& create_row) {
  std::vector<std::uint8_t> gradients(static_cast<std::size_t>(layout.count));
  const auto gradient = [&](int item) -> std::uint8_t* {
    if (item < 0 || item >= layout.count) {
      ctx.host.Refuse(
          std::format("gradient of item {} outside the list", item));
      return nullptr;
    }
    return &gradients[static_cast<std::size_t>(item)];
  };
  const int shown = std::min(layout.count, layout.visible);
  for (int i = 0; i < shown; ++i) {
    if (std::uint8_t* state = gradient(i + layout.previous_page)) {
      *state = MenuItem(ctx, i + layout.row_base).gradient_state;
    }
  }
  for (int i = 0;;) {
    MenuRow& row = create_row(i);
    const std::uint8_t* state = gradient(i + layout.page);
    row.gradient_state = state != nullptr ? *state : 0;
    ++i;
    if (i == layout.count) {
      break;
    }
    if (i == layout.visible) {
      if (layout.page + layout.visible < layout.count) {
        row.is_scrollable = kScrollDown;
      }
      break;
    }
  }
}

[[nodiscard]] CursorMemory PlaceActionCursor(CursorMemory cursor, int count) {
  const int scrollable = count - kVisibleActions;
  if (scrollable < 0) {
    cursor.row += cursor.page;
    cursor.page = 0;
    if (cursor.row >= count) {
      cursor.row = 0;
    }
  } else if (scrollable < cursor.page) {
    if (cursor.row + cursor.page >= count) {
      cursor = {};
    } else {
      cursor.row += cursor.page - scrollable;
      cursor.page = scrollable;
    }
  }
  if (cursor.page > 0 && cursor.row == 0) {
    cursor.row = 1;
    --cursor.page;
  }
  if (cursor.page < scrollable && cursor.row == kVisibleActions - 1) {
    cursor.row = kVisibleActions - 2;
    ++cursor.page;
  }
  return cursor;
}

// The texts and row types of a new list, or a refusal when they are short.
void FillList(Context& ctx, ItemList list, int count,
              std::span<const RowText> texts,
              std::span<const std::int32_t> row_types) {
  const auto needed = static_cast<std::size_t>(std::max(count, 0));
  if (texts.size() < needed || row_types.size() < needed) {
    ctx.host.Refuse("menu list shorter than its row count");
    return;
  }
  const std::span<ItemText> items = ctx.host.AllocateItems(list, count);
  const std::size_t filled = std::min(items.size(), needed);
  FillItems(items.first(filled), texts.first(filled), row_types.first(filled));
}

void CloseActions(Context& ctx, int result) {
  MainMenuStatics& menu = ctx.state.main_menu;
  ctx.host.FreeItems(ItemList::kActions);
  RememberCursor(
      ctx, menu.actions_sub_menu_id,
      {.row = menu.selected_item_action, .page = menu.item_action_list_offset});
  menu.selected_item_action = result;
}

void BuildNextActionRow(Context& ctx) {
  MainMenuStatics& menu = ctx.state.main_menu;
  MenuRow& row = CreateActionRow(ctx, menu.action_menu_state, kActionSlideX);
  row.state = RowState::kDismiss;
  row.target0 = 0;
  ++menu.action_menu_state;
  if (menu.action_menu_state == menu.item_action_count) {
    menu.action_menu_state = kListBuilt;
  }
  if (menu.action_menu_state == kVisibleActions) {
    if (menu.action_menu_state + menu.item_action_list_offset <
        menu.item_action_count) {
      row.is_scrollable = kScrollDown;
    }
    menu.action_menu_state = kListBuilt;
  }
}

void NavigateActions(Context& ctx, MenuRow& row) {
  MainMenuStatics& menu = ctx.state.main_menu;
  const Buttons buttons = ctx.host.Input();
  const int highlighted =
      menu.selected_item_action + menu.item_action_list_offset;
  const int previous_page = menu.item_action_list_offset;
  if ((buttons.pressed & kPadRUp) != 0) {
    CloseActions(ctx, kListBack);
    return;
  }
  if ((buttons.pressed & kPadRRight) != 0) {
    if (row.unselectable == 0) {
      row.selected = 1;
      CloseActions(ctx, highlighted);
      return;
    }
    ctx.host.PlaySound(MenuSound::kInvalid);
  }
  row.selected = 0;
  if ((buttons.pressed & kPadRDown) != 0) {
    CloseActions(ctx, kListCancelled);
    return;
  }
  const ListCursor cursor{.selected = menu.selected_item_action,
                          .page = menu.item_action_list_offset,
                          .count = menu.item_action_count,
                          .visible = kVisibleActions};
  if ((buttons.repeat & kPadLUp) != 0) {
    MoveCursorUp(cursor, buttons.pressed);
  }
  if ((buttons.repeat & kPadLDown) != 0) {
    MoveCursorDown(cursor, buttons.pressed);
  }
  if (highlighted != menu.selected_item_action + menu.item_action_list_offset) {
    ctx.host.PlaySound(MenuSound::kChange);
    if (menu.item_action_list_offset != previous_page) {
      RelayoutRows(
          ctx,
          {.count = menu.item_action_count,
           .visible = kVisibleActions,
           .row_base = kActionRowBase,
           .previous_page = previous_page,
           .page = menu.item_action_list_offset},
          [&](int i) -> MenuRow& { return CreateActionRow(ctx, i, 0); });
    }
  }
  MenuRow& current = MenuItem(ctx, menu.selected_item_action + kActionRowBase);
  current.selected = 1;
  menu.action_menu_cursor_step = static_cast<std::uint8_t>(DrawCursor(
      ctx, menu.action_menu_cursor_step, PackXy(0, current.y - kCursorRaise)));
}
}  // namespace

// decomp: MAINMENU.PRG vs_mainMenu_addMenuActions
// (src/MENU/MAINMENU.PRG/58EC.c:51)
void CreateActionRows(Context& ctx, int row_count, int sub_menu_id,
                      std::span<const RowText> texts,
                      std::span<const std::int32_t> row_types) {
  MainMenuStatics& menu = ctx.state.main_menu;
  ctx.state.menu_actions_page_offset =
      static_cast<std::uint8_t>(row_count >> kByteShift);
  const int count = row_count & kCountMask;
  if (ctx.state.menu_actions_page_offset == 0) {
    ctx.state.menu_actions_page_offset = static_cast<std::uint8_t>(
        count < kVisibleActions ? kListRows - count : kLongActionListOffset);
  }
  menu.actions_sub_menu_id = static_cast<std::uint8_t>(sub_menu_id);
  CursorMemory cursor;
  if (sub_menu_id == kDiscardActions) {
    cursor.row = 1;
  } else if (ctx.host.MenuSettings().cursor_memory != 0 &&
             sub_menu_id != kActionsWithoutMemory) {
    cursor = RecalledCursor(ctx, sub_menu_id);
  }
  cursor = PlaceActionCursor(cursor, count);
  menu.selected_item_action = cursor.row;
  menu.item_action_list_offset = static_cast<std::uint8_t>(cursor.page);
  menu.item_action_count = static_cast<std::uint8_t>(count);
  menu.action_menu_state = 0;
  FillList(ctx, ItemList::kActions, count, texts, row_types);
}

// decomp: MAINMENU.PRG vs_mainMenu_getSelectedAction
// (src/MENU/MAINMENU.PRG/58EC.c:147)
int HighlightedActionRow(Context& ctx) {
  return ctx.state.main_menu.selected_item_action +
         ctx.state.main_menu.item_action_list_offset;
}

// decomp: MAINMENU.PRG vs_mainMenu_actionInputProcessed
// (src/MENU/MAINMENU.PRG/58EC.c:152)
int ActionSelectionResult(Context& ctx) {
  return ctx.host.HasItems(ItemList::kActions)
             ? -1
             : ctx.state.main_menu.selected_item_action;
}

// decomp: MAINMENU.PRG _renderActionRow (src/MENU/MAINMENU.PRG/58EC.c:162)
MenuRow& CreateActionRow(Context& ctx, int row, int x) {
  const MainMenuStatics& menu = ctx.state.main_menu;
  const int item = menu.item_action_list_offset + row;
  MenuRow& action = SetMenuItem(
      ctx,
      {.row = row + kActionRowBase,
       .x = x,
       .y = ((row + ctx.state.menu_actions_page_offset) * kRowSpacing) +
            kFirstRowY,
       .w = kMainMenuWidth},
      ItemName(ctx, ItemList::kActions, item));
  action.unselectable =
      static_cast<std::uint8_t>(ItemFlags(ctx, ItemList::kActions, item) & 1);
  if (row == 0 && menu.item_action_list_offset != 0) {
    action.is_scrollable = kScrollUp;
  }
  return action;
}

// decomp: MAINMENU.PRG vs_mainMenu_renderItemActionMenu
// (src/MENU/MAINMENU.PRG/58EC.c:180)
void UpdateActionMenu(Context& ctx) {
  MainMenuStatics& menu = ctx.state.main_menu;
  if (menu.action_menu_state < kListRows) {
    BuildNextActionRow(ctx);
    return;
  }
  MenuRow& row = MenuItem(ctx, menu.selected_item_action + kActionRowBase);
  ShowItemDescription(ctx, ItemList::kActions,
                      menu.selected_item_action + menu.item_action_list_offset);
  if (menu.action_menu_state == kListBuilt) {
    if (AreMenuRowsIdle(ctx)) {
      menu.action_menu_state = kListReady;
    }
  } else if (menu.action_menu_state == kListReady) {
    NavigateActions(ctx, row);
  }
}

namespace {
// vs_mainmenu_setMenuRows' row info: the menu id, the rows above the list
// (bits 8..11) and the width added to its rows (bits 12..).
constexpr int kRowYOffsetShift = 12;
// The list row flags.
constexpr std::int32_t kWideRowFlag = 2;
constexpr int kWideRowBackground = 24;
constexpr int kFontColorShift = 2;
constexpr int kInvertShift = 3;
constexpr int kRowValueShift = 9;
constexpr std::int32_t kRowValueMask = 0x7F;
constexpr int kRowMaxShift = 19;
constexpr int kMaxRowCount = 100;
constexpr int kLastOutsetIcon = 102;
constexpr int kFirstLimbName = 103;
constexpr int kListCursorX = 180;
constexpr int kListCursorY = 10;

[[nodiscard]] CursorMemory PlaceListCursor(CursorMemory cursor, int count,
                                           int info) {
  const int max_page = count - kListRows + info;
  if (max_page < 0) {
    cursor.row += cursor.page;
    cursor.page = 0;
    if (cursor.row >= count) {
      cursor.row = 0;
    }
    return cursor;
  }
  if (max_page < cursor.page) {
    if (cursor.row + cursor.page >= count) {
      cursor = {};
    } else {
      cursor.row += cursor.page - max_page;
      cursor.page = max_page;
    }
  }
  bool following = cursor.page < max_page;
  if (cursor.page > 0 && cursor.row == 0) {
    cursor.row = 1;
    --cursor.page;
    following = cursor.page < max_page;
  }
  if (following && cursor.row == kListRows - 1 - info) {
    --cursor.row;
    ++cursor.page;
  }
  return cursor;
}

void CloseRows(Context& ctx, int result) {
  MainMenuStatics& menu = ctx.state.main_menu;
  ctx.host.FreeItems(ItemList::kRows);
  RememberCursor(ctx, menu.sub_menu_id,
                 {.row = menu.selected_row, .page = menu.sub_menu_page});
  menu.selected_row = result;
}

void BuildNextListRow(Context& ctx) {
  MainMenuStatics& menu = ctx.state.main_menu;
  MenuRow& row = CreateListRow(ctx, menu.items_list_row, kScreenWidth);
  row.state = RowState::kSlideX;
  ++menu.items_list_row;
  row.target0 = static_cast<std::int16_t>(kMainMenuX - menu.menu_row_y_offset);
  if (menu.items_list_row == menu.sub_menu_row_count) {
    menu.items_list_row = kListBuilt;
  }
  if (menu.items_list_row == kListRows - menu.sub_menu_row_info) {
    if (menu.items_list_row + menu.sub_menu_page < menu.sub_menu_row_count) {
      row.is_scrollable = kScrollDown;
    }
    menu.items_list_row = kListBuilt;
  }
}

void NavigateRows(Context& ctx, MenuRow& row) {
  MainMenuStatics& menu = ctx.state.main_menu;
  const Buttons buttons = ctx.host.Input();
  const int highlighted = menu.selected_row + menu.sub_menu_page;
  const int previous_page = menu.sub_menu_page;
  if ((buttons.pressed & kPadRUp) != 0) {
    ctx.host.PlaySound(MenuSound::kLeave);
    CloseRows(ctx, kListBack);
    return;
  }
  if ((buttons.pressed & kPadRRight) != 0) {
    if (row.unselectable == 0) {
      row.selected = 1;
      ctx.host.PlaySound(MenuSound::kSelect);
      CloseRows(ctx, highlighted);
      return;
    }
    ctx.host.PlaySound(MenuSound::kInvalid);
  }
  row.selected = 0;
  if ((buttons.pressed & kPadRDown) != 0) {
    ctx.host.PlaySound(MenuSound::kLeave);
    CloseRows(ctx, kListCancelled);
    return;
  }
  const int info = menu.sub_menu_row_info;
  const ListCursor cursor{.selected = menu.selected_row,
                          .page = menu.sub_menu_page,
                          .count = menu.sub_menu_row_count,
                          .visible = kListRows - info};
  if ((buttons.repeat & kPadLUp) != 0) {
    MoveCursorUp(cursor, buttons.pressed);
  }
  if ((buttons.repeat & kPadLDown) != 0) {
    MoveCursorDown(cursor, buttons.pressed);
  }
  if (highlighted != menu.selected_row + menu.sub_menu_page) {
    ctx.host.PlaySound(MenuSound::kChange);
    if (menu.sub_menu_page != previous_page) {
      const int x = kMainMenuX - menu.menu_row_y_offset;
      RelayoutRows(ctx,
                   {.count = menu.sub_menu_row_count,
                    .visible = kListRows - info,
                    .row_base = info * kListRows,
                    .previous_page = previous_page,
                    .page = menu.sub_menu_page},
                   [&](int i) -> MenuRow& { return CreateListRow(ctx, i, x); });
    }
  }
  MenuItem(ctx, menu.selected_row + (info * kListRows)).selected = 1;
  menu.row_selection_cursor_step = static_cast<std::uint8_t>(DrawCursor(
      ctx, menu.row_selection_cursor_step,
      Word(kListCursorX - menu.menu_row_y_offset) |
          (Word(((menu.selected_row + info) * kRowSpacing) + kListCursorY)
           << kHalfwordShift)));
}
}  // namespace

// decomp: MAINMENU.PRG vs_mainmenu_setMenuRows
// (src/MENU/MAINMENU.PRG/58EC.c:798)
void CreateMenuRows(Context& ctx, int row_count, int row_info,
                    std::span<const RowText> texts,
                    std::span<const std::int32_t> row_types) {
  MainMenuStatics& menu = ctx.state.main_menu;
  const int sub_menu_id = row_info & kCountMask;
  menu.menu_row_y_offset =
      static_cast<std::uint8_t>(row_info >> kRowYOffsetShift);
  menu.sub_menu_id = static_cast<std::uint8_t>(sub_menu_id);
  const int info = (row_info >> kByteShift) & kStateIndexMask;
  CursorMemory cursor;
  if (ctx.host.MenuSettings().cursor_memory != 0) {
    cursor = RecalledCursor(ctx, sub_menu_id);
  }
  cursor = PlaceListCursor(cursor, row_count, info);
  menu.selected_row = cursor.row;
  menu.sub_menu_page = static_cast<std::uint8_t>(cursor.page);
  menu.sub_menu_row_count = static_cast<std::uint16_t>(row_count);
  menu.sub_menu_row_info = static_cast<std::uint8_t>(info);
  menu.items_list_row = 0;
  FillList(ctx, ItemList::kRows, row_count, texts, row_types);
}

// decomp: MAINMENU.PRG vs_mainMenu_storeCursor
// (src/MENU/MAINMENU.PRG/58EC.c:892)
int CloseRowsAndRememberCursor(Context& ctx) {
  const MainMenuStatics& menu = ctx.state.main_menu;
  ctx.host.FreeItems(ItemList::kRows);
  RememberCursor(ctx, menu.sub_menu_id,
                 {.row = menu.selected_row, .page = menu.sub_menu_page});
  return (menu.selected_row + menu.sub_menu_page) |
         ((menu.selected_row + (menu.sub_menu_row_info * kListRows))
          << kByteShift);
}

// decomp: MAINMENU.PRG vs_mainMenu_getConfirmedRow
// (src/MENU/MAINMENU.PRG/58EC.c:903)
int HighlightedRow(Context& ctx) {
  return ctx.state.main_menu.selected_row + ctx.state.main_menu.sub_menu_page;
}

// decomp: MAINMENU.PRG vs_mainmenu_getSelectedRow
// (src/MENU/MAINMENU.PRG/58EC.c:905)
int GetSelectedRow(Context& ctx) {
  return ctx.host.HasItems(ItemList::kRows) ? -1
                                            : ctx.state.main_menu.selected_row;
}

// decomp: MAINMENU.PRG _initItemRow (src/MENU/MAINMENU.PRG/58EC.c:910)
MenuRow& CreateListRow(Context& ctx, int row, int x) {
  const MainMenuStatics& menu = ctx.state.main_menu;
  const int info = menu.sub_menu_row_info;
  const int item = menu.sub_menu_page + row;
  MenuRow& entry = SetMenuItem(ctx,
                               {.row = row + (info * kListRows),
                                .x = x,
                                .y = ((row + info) * kRowSpacing) + kFirstRowY,
                                .w = menu.menu_row_y_offset + kMainMenuWidth},
                               ItemName(ctx, ItemList::kRows, item));
  const std::int32_t flags = ItemFlags(ctx, ItemList::kRows, item);
  entry.unselectable = static_cast<std::uint8_t>(flags & 1);
  if ((flags & kWideRowFlag) != 0) {
    entry.background_width =
        static_cast<std::uint8_t>(entry.background_width + kWideRowBackground);
  }
  const int invert = (flags >> kInvertShift) & 1;
  entry.invert_gradient = static_cast<std::uint8_t>(invert);
  entry.font_color =
      static_cast<std::uint8_t>(invert | ((flags >> kFontColorShift) & 1));
  entry.row_icon = static_cast<std::uint8_t>(flags >> kRowIconShift);
  if (row == 0 && menu.sub_menu_page != 0) {
    entry.is_scrollable = kScrollUp;
  }
  const int value = (flags >> kRowValueShift) & kRowValueMask;
  if (value <= kMaxRowCount) {
    entry.count = static_cast<std::uint16_t>(value);
  } else if (value <= kLastOutsetIcon) {
    entry.outset_icon = static_cast<std::uint8_t>(value - kMaxRowCount);
  } else {
    entry.sub_text = ctx.host.LimbName(value - kFirstLimbName);
  }
  entry.material = static_cast<std::uint8_t>(
      (static_cast<std::uint32_t>(flags) >> kMaterialShift) & kMaterialMask);
  entry.max =
      static_cast<std::uint16_t>((flags >> kRowMaxShift) & kRowValueMask);
  return entry;
}

// decomp: MAINMENU.PRG vs_mainMenu_renderItemsList
// (src/MENU/MAINMENU.PRG/58EC.c:947)
void UpdateRowSelection(Context& ctx) {
  MainMenuStatics& menu = ctx.state.main_menu;
  if (menu.sub_menu_row_count == 0) {
    if (ctx.host.HasItems(ItemList::kRows)) {
      ctx.host.FreeItems(ItemList::kRows);
    }
    menu.selected_row = kListCancelled;
    return;
  }
  if (menu.items_list_row < kListRows) {
    BuildNextListRow(ctx);
    return;
  }
  MenuRow& row =
      MenuItem(ctx, menu.selected_row + (menu.sub_menu_row_info * kListRows));
  ShowItemDescription(ctx, ItemList::kRows,
                      menu.selected_row + menu.sub_menu_page);
  if (menu.items_list_row == kListBuilt) {
    if (AreMenuRowsIdle(ctx)) {
      menu.items_list_row = kListReady;
    }
  } else if (menu.items_list_row == kListReady) {
    NavigateRows(ctx, row);
  }
}
namespace {
// vs_mainMenu_printInformation: a row type's caution bit and the text box
// state that shows the box.
constexpr std::int32_t kCautionRow = 1;
constexpr int kInformationRowState = 16;

// _renderMenuCommand: the label slides 2 pixels a frame to 12.
constexpr int kLabelStep = 2;
constexpr int kLabelHeight = 12;
constexpr int kLabelRight = 312;
constexpr int kLabelRaise = 4;
constexpr std::uint32_t kLabelWidthMask = 0xFF;
constexpr int kLabelBrightness = 0x180;
// The labels' sizes (their widths also place them) and texture positions.
constexpr std::array<std::uint32_t, 8> kLabelSizes{
    PackXy(0, 0),   PackXy(62, 10), PackXy(36, 10), PackXy(30, 10),
    PackXy(56, 10), PackXy(30, 10), PackXy(32, 10), PackXy(0, 0),
};
constexpr std::array<std::uint32_t, 7> kLabelUvs{
    PackUv(0, 0),    PackUv(0, 64),   PackUv(64, 64), PackUv(100, 64),
    PackUv(132, 64), PackUv(188, 64), PackUv(0, 74),
};
constexpr int kLabelClutX = 848;

// The action cost panel: the two headers over a gradient bar.
constexpr std::uint8_t kCostPanelShown = 5;
constexpr int kCostPanelY = 160;
constexpr int kCostPanelBottom = 171;
constexpr int kCostPanelWidth = 80;
constexpr int kCostTileY = 162;
constexpr int kCostTileWidth = 82;
constexpr int kCostTileHeight = 11;
constexpr std::uint32_t kCostPanelDark = 0x200808;
constexpr std::uint32_t kCostPanelLight = 0x663040;
constexpr std::uint32_t kGray = 0x808080;
constexpr std::uint32_t kDisabledShiftMask = 31;

// The compact digits: "#" selects the digit glyphs.
constexpr char kDigitPrefix = '#';
constexpr int kDigitAdvance = 8;
constexpr std::int32_t kDigitMask = 0xF;
constexpr int kDigitShift = 4;
// SplitDecimalDigit's shifted quotient wraps from here, and the original
// never terminated.
constexpr std::int32_t kLargestDrawnInteger = 1342177280;
constexpr std::uint32_t kGreaterColor = 0x804020;
constexpr std::uint32_t kLesserColor = 0x204080;

// The button icons, the cursor and the row icons.
constexpr std::uint32_t kButtonColumnMask = 3;
constexpr std::uint32_t kButtonRowMask = 4;
constexpr int kButtonRowStride = 4;
constexpr int kButtonV = 128;
constexpr int kButtonClutX = 944;
constexpr int kCursorColorShift = 8;
constexpr std::uint32_t kFeatherSteps = 16;
constexpr int kFeatherBrightnessStep = 8;
constexpr int kSemiTransparent = 0x100;
constexpr int kFeatherFlipRows = 15;
constexpr int kMaterialSize = 7;
constexpr int kMaterialsPerColumn = 3;
constexpr int kMaterialCell = 8;
constexpr int kMaterialU = 216;
constexpr int kMaterialV = 200;
constexpr int kGoldMaterial = 2;
constexpr int kMaterialClutX = 912;
constexpr int kGoldMaterialClutX = 928;
constexpr int kOutsetIconU = 96;
constexpr int kOutsetIconV = 128;
constexpr int kOutsetIconClutX = 976;
constexpr int kRowIconClutX = 992;
constexpr int kRowIconFadeShift = 16;
constexpr int kRowIconBrightShift = 8;
constexpr std::uint32_t kRowIconMask = 0xFF;
constexpr int kRowIconBrightness = 64;
constexpr std::size_t kFirstWideIcon = 23;
constexpr std::uint32_t kWideIconShift = 12;
constexpr int kFadeUp = 2;
// vs_mainMenu_renderMenuRowIcon's icon texture positions.
constexpr std::array<std::uint16_t, 33> kRowIconUvs{
    PackUv(80, 96),   PackUv(144, 96),  PackUv(160, 96),  PackUv(112, 96),
    PackUv(128, 96),  PackUv(176, 96),  PackUv(208, 96),  PackUv(96, 96),
    PackUv(48, 112),  PackUv(128, 112), PackUv(0, 112),   PackUv(16, 112),
    PackUv(32, 112),  PackUv(224, 96),  PackUv(64, 112),  PackUv(80, 112),
    PackUv(96, 112),  PackUv(112, 112), PackUv(128, 112), PackUv(144, 112),
    PackUv(160, 112), PackUv(32, 96),   PackUv(176, 112), PackUv(120, 128),
    PackUv(136, 128), PackUv(152, 128), PackUv(168, 128), PackUv(184, 128),
    PackUv(200, 128), PackUv(216, 128), PackUv(232, 128), PackUv(96, 128),
    PackUv(80, 128),
};

// The button prompt background's lines.
constexpr std::uint32_t kPromptLineColor = 0x203840;
constexpr std::uint32_t kPromptLineFade = 0x081010;

// vs_mainMenu_renderTabNavigation.
constexpr std::uint8_t kTabArrowSteps = 12;
constexpr int kAnimationShift = 2;
constexpr std::uint32_t kDimColor = 0x404040;
constexpr int kTabClutX = 896;
constexpr int kTabIdleClutX = 912;
constexpr std::array<std::string_view, 4> kTabControls{"1", "L", "1", "R"};
constexpr std::array<std::uint32_t, 4> kTabControlPositions{
    PackXy(216, 8),
    PackXy(210, 8),
    PackXy(288, 8),
    PackXy(282, 8),
};
// The arrows beside the menu tabs, and beside the tab controls (mode 1).
constexpr std::uint32_t kTabArrowLeft = PackXy(16, 16);
constexpr std::uint32_t kTabArrowRight = PackXy(162, 17);
constexpr std::uint32_t kControlArrowLeft = PackXy(200, 5);
constexpr std::uint32_t kControlArrowRight = PackXy(292, 6);

// vs_mainMenu_renderRowLabel: a glyph drawn as twelve fading lines.
constexpr std::uint32_t kTextSpace = 0x20;
constexpr std::uint32_t kGlyphIndexMask = 0x1FF;
constexpr std::uint32_t kGlyphCell = 12;
constexpr std::uint32_t kGlyphRowStep = 12U << kByteShift;
constexpr int kGlyphLines = 12;
constexpr int kGlyphClutX = 896;
constexpr int kGlyphClutY = 222;
constexpr int kPaletteStride = 16;
constexpr int kFeatherInBrightness = 264;
constexpr int kFeatherOutBrightness = 352;
constexpr std::uint32_t kLineStep = 1U << kByteShift;
constexpr int kReversedShift = 31;

// _renderScrollableRow.
constexpr std::uint8_t kScrollArrowSteps = 12;
constexpr int kScrollArrowRise = 5;
constexpr int kScrollArrowX = 126;
constexpr int kScrollArrowIndent = 14;
constexpr int kArrowClutX = 912;
constexpr int kScrollLines = 12;
constexpr int kScrollFadeBase = 32;
constexpr int kScrollFadeTop = 120;
constexpr std::uint32_t kNextLine = 1U << kHalfwordShift;
constexpr std::uint32_t kShadowGap = 2;
constexpr std::uint32_t kShineHalf = 128;
constexpr std::uint32_t kTextureCoordinateMask = 0xFF;

[[nodiscard]] std::uint32_t LowHalf(std::uint32_t word) {
  return word & kHalfwordMask;
}

[[nodiscard]] std::uint32_t HighHalf(std::uint32_t word) {
  return (word >> kHalfwordShift) << kHalfwordShift;
}

// Whether any of the rows [first, last) is still moving.
[[nodiscard]] bool AnyRowMoving(Context& ctx, int first, int last) {
  for (int i = first; i < last; ++i) {
    if (MenuItem(ctx, i).state > RowState::kIdle) {
      return true;
    }
  }
  return false;
}

void DrawScrollArrow(Context& ctx, const MenuRow& row, int scroll, int bob) {
  std::uint32_t position = 0;
  if (row.x == 0) {
    if (AnyRowMoving(ctx, kActionRowBase, static_cast<int>(kRowCount))) {
      return;
    }
    position = PackXy(kScrollArrowX, row.y + bob);
  } else {
    if (AnyRowMoving(ctx, 0, kActionRowBase)) {
      return;
    }
    position = PackXy(row.x - kScrollArrowIndent, row.y + bob);
  }
  EmitSprite(ctx, kOverlayFront,
             {.brightness = kFullBrightness,
              .position = position,
              .size = PackXy(kIconSize, kIconSize),
              .texture = PackUvClut(scroll * kIconSize, kCursorUvV, kArrowClutX,
                                    kClutRow)});
}

void DrawScrollableGlyphs(Context& ctx, const MenuRow& row, int scroll) {
  int pen = row.x + (row.row_icon != 0 ? kIconRowTextIndent : kRowTextIndent);
  const std::uint32_t reversed = Word(scroll) << kReversedShift;
  std::size_t i = 0;
  while (true) {
    if (i >= row.text.size()) {
      ctx.host.Refuse("menu row text without an end");
      return;
    }
    const std::uint8_t c = row.text[i++];
    if (c == kRowTextEnd) {
      return;
    }
    if (c == kTextSpacing) {
      if (i >= row.text.size()) {
        ctx.host.Refuse("menu row text without an end");
        return;
      }
      pen += row.text[i++];
    } else {
      pen = DrawFeatheredRowCharacter(ctx, c | reversed, pen, row, kRowLayer);
    }
  }
}

// The gradient lines, the shadow and the reflection under a scrolling row.
void DrawScrollableBackground(Context& ctx, MenuRow& row, int scroll,
                              std::uint32_t xy) {
  const int w = row.w;
  const int gradient = row.gradient_state;
  const auto background = static_cast<std::uint32_t>(row.background_width);
  for (int i = 0; i < kScrollLines; ++i) {
    const int level = scroll == 0
                          ? (i * kFeatherBrightnessStep) + kScrollFadeBase
                          : kScrollFadeTop - (i * kFeatherBrightnessStep);
    std::array<std::uint32_t, kGradientLineWords> packet{
        0,
        kDrawModeAddDithered,
        Command(kLineG2SemiTransparent,
                GradientStopColor(ctx, Word(kGradientStops - gradient),
                                  background, level)),
        xy,
        GradientStopColor(ctx, Word(gradient), background, level),
        LowHalf(xy + Word(w) - 1) | HighHalf(xy),
        kDrawModeAdd};
    EmitPacket(ctx.host.Queue(kRowLayer), packet);
    xy += kNextLine;
  }
  if (scroll == 0) {
    DrawTile(ctx, kRowLayer, LowHalf(xy + kShadowGap) | HighHalf(xy),
             Word(w) | (kShadowGap << kHalfwordShift));
  }
  if (gradient != 0) {
    row.gradient_state = static_cast<std::uint8_t>(gradient - 1);
  }
  xy -= Word(kScrollLines) << kHalfwordShift;
  const std::uint32_t bank =
      ctx.host.FrameBufferIndex() == 0 ? kScreenWidth : 0;
  const std::uint32_t shine =
      row.x < static_cast<int>(kShineHalf) ? 0 : kShineHalf;
  for (int i = 0; i < kScrollLines; ++i) {
    const int brightness = scroll == 0
                               ? kScrollFadeTop - (i * kFeatherBrightnessStep)
                               : (i * kFeatherBrightnessStep) + kScrollFadeBase;
    EmitSprite(
        ctx, kRowLayer,
        {.draw_mode = kFramePageDrawMode | ((bank + shine) >> kPageColumnShift),
         .brightness = brightness,
         .position = xy,
         .size = Word(w) | kNextLine,
         .texture = ((xy - shine) & kTextureCoordinateMask) |
                    ((xy >> kHalfwordShift) << kByteShift)});
    xy += kNextLine;
  }
}
}  // namespace

// decomp: MAINMENU.PRG vs_mainMenu_printInformation
// (src/MENU/MAINMENU.PRG/58EC.c:367)
void UpdateInformationVisibility(Context& ctx, int source_row,
                                 int action_menu_state) {
  if (action_menu_state < kInformationRowState) {
    return;
  }
  const auto row_types = ctx.host.RowTypes();
  if (source_row < 0 ||
      static_cast<std::size_t>(source_row) >= row_types.size()) {
    ctx.host.Refuse(std::format("row type {} outside the table", source_row));
    return;
  }
  const Settings settings = ctx.host.MenuSettings();
  if ((row_types[static_cast<std::size_t>(source_row)] & kCautionRow) != 0) {
    if (settings.information == 0 &&
        (ctx.host.Input().held & (kPadLUp | kPadLDown)) != 0) {
      return;
    }
    ctx.host.InitInformationBox(InformationHeader::kCaution);
    ctx.host.SetInformationBoxState(kInformationBoxShown);
  } else if (settings.information != 0) {
    ctx.host.InitInformationBox(InformationHeader::kInformation);
    ctx.host.SetInformationBoxState(kInformationBoxShown);
  } else {
    ctx.host.SetInformationBoxState(kInformationBoxHidden);
  }
}

// decomp: MAINMENU.PRG vs_mainMenu_setMenuCommand
// (src/MENU/MAINMENU.PRG/58EC.c:405)
void SetCommandLabel(Context& ctx, int action) {
  ctx.state.main_menu.menu_action_next = static_cast<std::uint8_t>(action);
}

// decomp: MAINMENU.PRG _renderMenuCommand (src/MENU/MAINMENU.PRG/58EC.c:413)
void DrawCommandLabel(Context& ctx) {
  MainMenuStatics& menu = ctx.state.main_menu;
  const int action = menu.menu_action_current;
  int y = menu.command_label_y;
  if (action != menu.menu_action_next) {
    if (y == 0) {
      menu.menu_action_current = menu.menu_action_next;
      return;
    }
    y -= kLabelStep;
  } else if (y < kLabelHeight) {
    y += kLabelStep;
  }
  menu.command_label_y = y;
  if (action == 0) {
    return;
  }
  if (static_cast<std::size_t>(action) >= kLabelUvs.size()) {
    ctx.host.Refuse(std::format("command label {} outside the table", action));
    return;
  }
  const std::uint32_t size = kLabelSizes[static_cast<std::size_t>(action)];
  EmitSprite(ctx, kOverlayFront,
             {.draw_mode = kCommandLabelDrawMode,
              .brightness = kLabelBrightness,
              .position =
                  Word(kLabelRight - static_cast<int>(size & kLabelWidthMask)) |
                  (Word(y - kLabelRaise) << kHalfwordShift),
              .size = size,
              .texture = kLabelUvs[static_cast<std::size_t>(action)] |
                         (PackClut(kLabelClutX, kClutRow) << kHalfwordShift)});
}

// decomp: MAINMENU.PRG vs_mainMenu_dismissInformationBox
// (src/MENU/MAINMENU.PRG/58EC.c:464)
void DismissInformationBox(Context& ctx) { ctx.host.DismissInformationBox(); }

// decomp: MAINMENU.PRG vs_mainMenu_initInformationBox
// (src/MENU/MAINMENU.PRG/58EC.c:466)
void InitializeInformationBox(Context& ctx) {
  if (ctx.host.MenuSettings().information != 0) {
    ctx.host.InitInformationBox(InformationHeader::kInformation);
  } else {
    DismissInformationBox(ctx);
  }
}

// decomp: MAINMENU.PRG vs_mainmenu_setInformationMessage
// (src/MENU/MAINMENU.PRG/58EC.c:475)
void SetInformationMessage(Context& ctx,
                           std::span<const std::uint8_t> message) {
  auto& buffer = ctx.state.information_message;
  buffer[0] = kTextChunkSize;
  buffer[1] = 0;
  std::copy_n(message.begin(),
              std::min(message.size(), kInformationMessageCopyBytes),
              buffer.begin() + kChunkHeaderBytes);
  buffer[kMessageEnd] = kTextTerminator;
  buffer[kMessageEnd + 1] = kTextTerminator;
  buffer[kMessageEnd + 2] = 0;
  ctx.host.ShowInformationMessage(buffer);
}

// decomp: MAINMENU.PRG vs_mainmenu_setActionCost
// (src/MENU/MAINMENU.PRG/58EC.c:652)
void SetActionCostText(Context& ctx, int index,
                       std::span<const std::uint8_t> text, int x_offset,
                       int disabled) {
  ctx.state.main_menu.display_action_cost = 1;
  if (index < 0 || static_cast<std::size_t>(index) >= kTextHeaderCount) {
    ctx.host.Refuse(
        std::format("action cost header {} outside the table", index));
    return;
  }
  TextHeader& header = ctx.state.text_headers[static_cast<std::size_t>(index)];
  std::copy_n(text.begin(), std::min(text.size(), kTextHeaderBytes),
              header.text.begin());
  header.disabled = static_cast<std::uint8_t>(disabled);
  header.x = static_cast<std::uint8_t>(x_offset);
}

// decomp: MAINMENU.PRG _renderAbilityCost (src/MENU/MAINMENU.PRG/58EC.c:665)
void DrawActionCostPanel(Context& ctx) {
  std::uint8_t& step = ctx.state.main_menu.action_cost_panel_step;
  if (ctx.state.main_menu.display_action_cost != 0) {
    if (step != 0) {
      --step;
    }
  } else if (step < kCostPanelShown) {
    step = kCostPanelShown;
  } else if (step < kActionCostPanelHidden) {
    ++step;
  }
  if (step >= kActionCostPanelHidden) {
    return;
  }
  const int x1 = -AnimationStep(ctx, step);
  for (const TextHeader& header : ctx.state.text_headers) {
    const auto* begin = reinterpret_cast<const char*>(header.text.data());
    const std::string_view text(
        begin,
        std::ranges::find(header.text, std::uint8_t{0}) - header.text.begin());
    ctx.host.DrawCompactText(
        kOverlayFront, text,
        LowHalf(Word(x1 + header.x)) | (Word(kCostPanelY) << kHalfwordShift),
        kGray >> (header.disabled & kDisabledShiftMask));
  }
  const std::uint32_t x0 = LowHalf(Word(x1));
  const std::uint32_t x_end = Word(x1 + kCostPanelWidth);
  std::array<std::uint32_t, kGradientQuadWords> packet{
      0,
      kDrawModeDithered,
      Command(kPolyG4, kCostPanelDark),
      x0 | (Word(kCostPanelY) << kHalfwordShift),
      kCostPanelLight,
      x_end | (Word(kCostPanelY) << kHalfwordShift),
      kCostPanelDark,
      x0 | (Word(kCostPanelBottom) << kHalfwordShift),
      kCostPanelLight,
      x_end | (Word(kCostPanelBottom) << kHalfwordShift),
      kDrawModePlain};
  EmitPacket(ctx.host.Queue(kActionCostLayer), packet);
  DrawTile(ctx, kActionCostLayer, x0 | (Word(kCostTileY) << kHalfwordShift),
           PackXy(kCostTileWidth, kCostTileHeight));
}

// decomp: MAINMENU.PRG vs_mainmenu_renderButton
// (src/MENU/MAINMENU.PRG/58EC.c:487)
void DrawButtonIcon(Context& ctx, int button_id, int x, int y, Layer layer) {
  const auto id = static_cast<std::uint32_t>(button_id);
  EmitSprite(ctx, layer,
             {.brightness = kFullBrightness,
              .position = PackXy(x, y),
              .size = PackXy(kIconSize, kIconSize),
              .texture = PackUvClut(
                  static_cast<int>((id & kButtonColumnMask) * kIconSize),
                  static_cast<int>((id & kButtonRowMask) * kButtonRowStride) +
                      kButtonV,
                  kButtonClutX, kClutRow)});
}

// decomp: MAINMENU.PRG vs_mainMenu_renderCursor
// (src/MENU/MAINMENU.PRG/58EC.c:499)
int DrawCursor(Context& ctx, std::uint32_t step, std::uint32_t position) {
  std::uint8_t& color = ctx.state.main_menu.cursor_color;
  // The MIPS shift takes the low five bits of its count.
  const auto cursor_step = static_cast<int>(
      step >>
      ((static_cast<unsigned>(color) * kCursorColorShift) & kShiftCountMask));
  EmitSprite(ctx, kOverlayFront,
             {.brightness = CursorBrightness(ctx, cursor_step),
              .position = position,
              .size = PackXy(kIconSize, kIconSize),
              .texture = PackUvClut(
                  kCursorUvU, kCursorUvV,
                  color == 0 ? kCursorClut : kAlternateCursorClut, kClutRow)});
  color = 0;
  return (cursor_step + 1) & static_cast<int>(kStepMask);
}

// decomp: MAINMENU.PRG vs_mainMenu_renderIntColor
// (src/MENU/MAINMENU.PRG/58EC.c:520)
std::uint32_t DrawColoredInteger(Context& ctx, const NumberText& number,
                                 Layer layer) {
  std::uint32_t position = number.position;
  if (number.value == std::numeric_limits<std::int32_t>::min() ||
      std::abs(number.value) >= kLargestDrawnInteger) {
    ctx.host.Refuse(std::format("integer {} too large to draw", number.value));
    return position;
  }
  std::int32_t digits = number.value < 0 ? -number.value : number.value;
  do {
    digits = Core::Math::SplitDecimalDigit(digits);
    const std::array<char, 2> glyph{
        kDigitPrefix, static_cast<char>('0' + (digits & kDigitMask))};
    ctx.host.DrawCompactText(layer,
                             std::string_view(glyph.data(), glyph.size()),
                             position, number.color);
    digits >>= kDigitShift;
    position -= kDigitAdvance;
  } while (digits != 0);
  if (number.value < 0) {
    ctx.host.DrawCompactText(layer, "#-", position + 1, number.color);
  }
  return position;
}

// decomp: MAINMENU.PRG vs_mainMenu_renderIntWithThreshold
// (src/MENU/MAINMENU.PRG/58EC.c:547)
std::uint32_t DrawComparedInteger(Context& ctx, std::int32_t value,
                                  std::uint32_t position,
                                  std::int32_t threshold, Layer layer) {
  std::uint32_t color = kGray;
  if (threshold < value) {
    color = kGreaterColor;
  }
  if (value < threshold) {
    color = kLesserColor;
  }
  return DrawColoredInteger(
      ctx, {.value = value, .position = position, .color = color}, layer);
}

// decomp: MAINMENU.PRG vs_mainMenu_renderIntColorDefault
// (src/MENU/MAINMENU.PRG/58EC.c:562)
void DrawNeutralInteger(Context& ctx, std::int32_t value,
                        std::uint32_t position, Layer layer) {
  DrawColoredInteger(
      ctx, {.value = value, .position = position, .color = kGray}, layer);
}

// decomp: MAINMENU.PRG _renderFeatheredIcon (src/MENU/MAINMENU.PRG/58EC.c:571)
void DrawFeatheredRowIcon(Context& ctx, int direction, std::uint32_t position,
                          std::uint32_t uv_clut, Layer layer) {
  if (direction < 0) {
    position += Word(kFeatherFlipRows) << kHalfwordShift;
    uv_clut += Word(kFeatherFlipRows) << kByteShift;
  }
  for (std::uint32_t i = 1; i <= kFeatherSteps; ++i) {
    EmitSprite(ctx, layer,
               {.brightness = static_cast<int>(i * kFeatherBrightnessStep) |
                              kSemiTransparent,
                .position = position,
                .size = PackXy(kIconSize, 1),
                .texture = uv_clut});
    position += Word(direction) << kHalfwordShift;
    uv_clut += Word(direction) << kByteShift;
  }
}

// decomp: MAINMENU.PRG _renderItemMaterial (src/MENU/MAINMENU.PRG/58EC.c:594)
void DrawMaterialIndicator(Context& ctx, int material, std::uint32_t position) {
  const std::uint32_t uv =
      Word(((material / kMaterialsPerColumn) * kMaterialCell) + kMaterialU) |
      (Word(((material % kMaterialsPerColumn) * kMaterialCell) + kMaterialV)
       << kByteShift);
  const int clut_x =
      material == kGoldMaterial ? kGoldMaterialClutX : kMaterialClutX;
  EmitSprite(ctx, kOverlayFront,
             {.position = position,
              .size = PackXy(kMaterialSize, kMaterialSize),
              .texture = uv | (PackClut(clut_x, kClutRow) << kHalfwordShift)});
}

// decomp: MAINMENU.PRG _renderOutsetIcon (src/MENU/MAINMENU.PRG/58EC.c:609)
void DrawEquipmentStateIcon(Context& ctx, int icon, std::uint32_t position) {
  EmitSprite(ctx, kOverlayFront,
             {.position = position,
              .size = PackXy(kIconSize, kIconSize),
              .texture = PackUvClut(kOutsetIconU - (icon * kIconSize),
                                    kOutsetIconV, kOutsetIconClutX, kClutRow)});
}

// decomp: MAINMENU.PRG vs_mainMenu_renderMenuRowIcon
// (src/MENU/MAINMENU.PRG/58EC.c:615)
void DrawRowIcon(Context& ctx, int icon, int x, int y) {
  int fade = icon >> kRowIconFadeShift;
  const int bright = (icon >> kRowIconBrightShift) & 1;
  const std::size_t index = static_cast<std::uint32_t>(icon - 1) & kRowIconMask;
  if (index >= kRowIconUvs.size()) {
    ctx.host.Refuse(std::format("row icon {} outside the table", index + 1));
    return;
  }
  const std::uint32_t uv = kRowIconUvs[index];
  if (fade != 0) {
    if (fade == kFadeUp) {
      fade = -1;
    }
    DrawFeatheredRowIcon(
        ctx, fade, PackXy(x, y),
        uv | (PackClut(kRowIconClutX, kClutRow) << kHalfwordShift),
        kRowIconLayer);
    return;
  }
  const bool wide = index >= kFirstWideIcon;
  EmitSprite(
      ctx, kRowIconLayer,
      {.brightness = kRowIconBrightness << bright,
       .position = PackXy(x, y) - (wide ? kWideIconShift : 0),
       .size = PackXy(kIconSize, kIconSize),
       .texture =
           uv | (PackClut(wide ? kOutsetIconClutX : kRowIconClutX, kClutRow)
                 << kHalfwordShift)});
}

// decomp: MAINMENU.PRG vs_mainmenu_renderButtonBackground
// (src/MENU/MAINMENU.PRG/58EC.c:718)
void DrawButtonPromptBackground(Context& ctx, const Box& box) {
  for (int i = 0; i < box.h; ++i) {
    std::array<std::uint32_t, kGradientLineWords> packet{
        0,
        kDrawModeDithered,
        Command(kLineG2, kPromptLineColor),
        LowHalf(Word(box.x)) | (Word(box.y + i) << kHalfwordShift),
        kPromptLineFade,
        PackXy(box.x + box.w - i, box.y + i),
        kDrawModePlain};
    EmitPacket(ctx.host.Queue(kButtonPromptLayer), packet);
  }
}

// decomp: MAINMENU.PRG vs_mainMenu_renderTabNavigation
// (src/MENU/MAINMENU.PRG/58EC.c:1259)
void DrawTabNavigation(Context& ctx, int mode) {
  std::uint8_t& step = ctx.state.main_menu.tab_arrow_step;
  std::uint32_t clut = PackClut(kTabIdleClutX, kClutRow) << kHalfwordShift;
  std::uint32_t color = kDimColor;
  if (ctx.host.TabArrowsFrozen() || step != kTabArrowAnimationStart) {
    clut = PackClut(kTabClutX, kClutRow) << kHalfwordShift;
    ++step;
    color = kGray;
    if (step >= kTabArrowSteps) {
      step = 0;
    }
  }
  const std::uint32_t bob = Word(step) >> kAnimationShift;
  std::uint32_t left = kTabArrowLeft;
  std::uint32_t right = kTabArrowRight;
  if (mode != 0) {
    for (std::size_t i = 0; i < kTabControls.size(); ++i) {
      ctx.host.DrawCompactText(kOverlayFront, kTabControls[i],
                               kTabControlPositions[i], color);
    }
    left = kControlArrowLeft;
    right = kControlArrowRight;
  }
  const std::uint32_t size = PackXy(kIconSize, kIconSize);
  EmitSprite(ctx, kOverlayFront,
             {.position = left - bob,
              .size = size,
              .texture = clut | PackUv(0, kCursorUvV)});
  EmitSprite(ctx, kOverlayFront,
             {.position = right + bob,
              .size = size,
              .texture = clut | PackUv(kIconSize, kCursorUvV)});
}

// decomp: MAINMENU.PRG vs_mainMenu_renderRowLabel
// (src/MENU/MAINMENU.PRG/58EC.c:1215)
int DrawFeatheredRowCharacter(Context& ctx, std::uint32_t character, int x,
                              const MenuRow& row, Layer layer) {
  const bool reversed = (character >> kReversedShift) != 0;
  character &= kHalfwordMask;
  if (character == kTextSpace) {
    return x + Render::kVariableFontSpaceAdvance;
  }
  int palette = row.font_color * kPaletteStride;
  if (row.font_color == 0) {
    palette = row.unselectable * kUnselectablePalette * kPaletteStride;
  }
  const std::uint32_t glyph = character & kGlyphIndexMask;
  std::uint32_t uv_clut =
      ((glyph % Render::kVariableFontGlyphsPerRow) * kGlyphCell) |
      ((glyph / Render::kVariableFontGlyphsPerRow) * kGlyphRowStep) |
      (PackClut(palette + kGlyphClutX, kGlyphClutY) << kHalfwordShift);
  for (int i = 0; i < kGlyphLines; ++i) {
    EmitSprite(
        ctx, layer,
        {.draw_mode = kRowGlyphDrawMode,
         .brightness =
             reversed ? kFeatherOutBrightness - (i * kFeatherBrightnessStep)
                      : (i * kFeatherBrightnessStep) + kFeatherInBrightness,
         .position = PackXy(x, row.y + i),
         .size = PackXy(static_cast<int>(kGlyphCell), 1),
         .texture = uv_clut});
    uv_clut += kLineStep;
  }
  const auto widths = ctx.tables.glyph_widths;
  if (character >= widths.size()) {
    ctx.host.Refuse(std::format("glyph {} outside the width table", character));
    return x;
  }
  return x + widths[character];
}

// decomp: MAINMENU.PRG _renderScrollableRow (src/MENU/MAINMENU.PRG/58EC.c:1309)
void DrawScrollableRow(Context& ctx, MenuRow& row) {
  MainMenuStatics& menu = ctx.state.main_menu;
  const int scroll = row.is_scrollable - 1;
  const int frame = ctx.host.FrameBufferIndex();
  if (frame != menu.scrollable_row_frame) {
    ++menu.scrollable_row_step;
    menu.scrollable_row_frame = static_cast<std::uint8_t>(frame);
    if (menu.scrollable_row_step >= kScrollArrowSteps) {
      menu.scrollable_row_step = 0;
    }
  }
  const int phase = menu.scrollable_row_step >> kAnimationShift;
  DrawScrollArrow(ctx, row, scroll,
                  scroll == 0 ? phase - kScrollArrowRise : 1 - phase);
  DrawScrollableGlyphs(ctx, row, scroll);
  if (row.w != 0) {
    DrawScrollableBackground(ctx, row, scroll,
                             Word(static_cast<std::uint16_t>(row.x)) |
                                 (Word(row.y) << kHalfwordShift));
  }
}
namespace {
// _renderMenuItems.
constexpr int kWideSlideTarget = 155;
// The width byte keeps 320 - x modulo 256.
constexpr int kWidthWrap = 64;
constexpr std::uint8_t kMaxSlideWidth = 165;
constexpr int kCounterIndent = 116;
constexpr int kCounterScale = 19;
constexpr int kCounterScaleShift = 4;
constexpr int kCounterBase = 96;
constexpr std::uint32_t kCounterSeparator = 30;
constexpr std::uint32_t kPageNumberOffset = 32;
constexpr int kPagedRowMaxX = 96;
constexpr int kCountIndent = 16;
constexpr int kCountDrop = 3;
constexpr int kOutsetIconIndent = 24;
constexpr int kIconRaise = 3;
constexpr int kButtonIndent = 5;
constexpr int kRowIconRaise = 2;
constexpr int kMaterialIndent = 13;
constexpr int kScrollFlagShift = 16;
constexpr int kSelectedShift = 8;

// _renderBackground: the left 256 and right 64 columns of the picture.
constexpr int kBackgroundBrightnessScale = 8;
constexpr int kBackgroundSplit = 256;
constexpr int kBackgroundRightWidth = 64;
constexpr int kBackgroundHeight = 240;
constexpr int kBackgroundClutX = 768;
constexpr int kBackgroundClutY = 227;

// vs_mainMenu_unpackMenubg: a 256-colour CLUT, then run-length words.
constexpr std::size_t kBackgroundClutBytes = 0x200;
constexpr std::size_t kBackgroundEncodedWords = 0x1780;
constexpr std::size_t kWordBytes = sizeof(std::uint32_t);
constexpr std::array kBackgroundClearRects{
    ImageRect{.x = 640, .y = 256, .w = 32, .h = 240},
    ImageRect{.x = 768, .y = 256, .w = 32, .h = 240},
};
constexpr std::uint32_t kGameOverClutPosition = PackXy(640, 511);
constexpr std::uint32_t kGameOverClutSize = PackXy(256, 1);
constexpr std::uint32_t kBackgroundPosition = PackXy(672, 256);
constexpr std::uint32_t kBackgroundSize = PackXy(96, 240);
constexpr int kStoryGameOver = 1;

void SlideRowX(Context& ctx, MenuRow& row) {
  int origin = row.x;
  const int target = row.target0;
  if (origin < target) {
    origin = std::min(origin + kSlideJump, target);
  } else if (target < origin) {
    origin = StepDownTo(ctx, target, origin);
    if (target >= kWideSlideTarget && row.w < kScreenWidth - origin) {
      row.w = static_cast<std::uint8_t>(kWidthWrap - origin);
      if (kScreenWidth - origin > kMaxSlideWidth) {
        row.w = kMaxSlideWidth;
      }
    }
  }
  row.x = static_cast<std::int16_t>(origin);
  if (origin == target) {
    row.state = RowState::kIdle;
  }
}

void SlideRowY(Context& ctx, MenuRow& row) {
  int origin = row.y;
  const int target = row.target0;
  if (origin < target) {
    origin = StepUpTo(ctx, target, origin);
  } else if (target < origin) {
    origin = StepDownTo(ctx, target, origin);
  }
  row.y = static_cast<std::int16_t>(origin);
  if (origin == target) {
    row.state = RowState::kIdle;
  }
}

void SlideRowXy(Context& ctx, MenuRow& row) {
  if (row.x != row.target0) {
    row.x = static_cast<std::int16_t>(StepDownTo(ctx, row.target0, row.x));
    if (row.w < kScreenWidth - row.x) {
      row.w = static_cast<std::uint8_t>(kWidthWrap - row.x);
    }
    return;
  }
  if (row.y != row.target1) {
    row.y = static_cast<std::int16_t>(StepDownTo(ctx, row.target1, row.y));
  }
  if (row.y == row.target1) {
    row.state = RowState::kIdle;
  }
}

void DismissRow(Context& ctx, MenuRow& row) {
  int origin = row.x;
  const int target = row.target0;
  if (target < origin) {
    origin = std::max(origin - kSlideJump, target);
    if (-row.w >= origin) {
      row.state = RowState::kInactive;
    }
  } else if (origin < target) {
    origin = StepUpTo(ctx, target, origin);
  }
  row.x = static_cast<std::int16_t>(origin);
  if (origin == target) {
    row.state = origin < 0 ? RowState::kInactive : RowState::kIdle;
  }
}

void MoveRow(Context& ctx, MenuRow& row) {
  switch (row.state) {
    case RowState::kSlideX:
      SlideRowX(ctx, row);
      break;
    case RowState::kSlideY:
      SlideRowY(ctx, row);
      break;
    case RowState::kSlideXy:
      SlideRowXy(ctx, row);
      break;
    case RowState::kDismiss:
      DismissRow(ctx, row);
      break;
    default:
      break;
  }
}

// A list row's "count/max" (with the page on the first row) or its count.
void DrawRowCounters(Context& ctx, const MenuRow& row) {
  if (row.max != 0) {
    const std::uint32_t x =
        row.w == kMainMenuWidth
            ? Word(row.x + kCounterIndent)
            : Word(((row.x * kCounterScale) >> kCounterScaleShift) +
                   kCounterBase) &
                  kHalfwordMask;
    const std::uint32_t origin = x | (Word(row.y + 1) << kHalfwordShift);
    DrawNeutralInteger(ctx, row.max, origin, kRowLayer);
    const std::uint32_t separator = origin - kCounterSeparator;
    ctx.host.DrawCompactText(kRowLayer, "/", separator,
                             Render::kCompactTextGray);
    DrawNeutralInteger(ctx, row.count, separator, kRowLayer);
    if (row.y == kFirstRowY && row.x < kPagedRowMaxX) {
      DrawNeutralInteger(ctx, row.item_page, origin + kPageNumberOffset,
                         kRowLayer);
      ctx.host.DrawCompactText(kRowLayer, "-", origin,
                               Render::kCompactTextGray);
    }
  } else if (row.count != 0) {
    Render::DrawIntegerRightAligned(
        ctx.host.Queue(kRowLayer),
        {.font = 0,
         .position = static_cast<std::int32_t>(
             Word(row.x + row.w - kCountIndent) |
             (Word(row.y + kCountDrop) << kHalfwordShift)),
         .value = row.count});
  }
}

void DrawRowParts(Context& ctx, MenuRow& row) {
  if (row.x < kScreenWidth) {
    if (row.is_scrollable != 0) {
      DrawScrollableRow(ctx, row);
    } else {
      DrawMenuItem(ctx, row);
    }
    if (row.outset_icon != 0) {
      DrawEquipmentStateIcon(ctx, row.outset_icon - 1,
                             Word(row.x - kOutsetIconIndent) |
                                 (Word(row.y - kIconRaise) << kHalfwordShift));
    }
  }
  if (row.button_id != 0) {
    DrawButtonIcon(ctx, row.button_id - 1, row.x + kButtonIndent,
                   row.y - kRowIconRaise, kOverlayFront);
  }
  if (row.row_icon != 0) {
    DrawRowIcon(ctx,
                (row.is_scrollable << kScrollFlagShift) |
                    (row.selected << kSelectedShift) | row.row_icon,
                row.x, row.y - kRowIconRaise);
  }
  if (row.material != 0) {
    DrawMaterialIndicator(ctx, row.material - 1,
                          Word(row.x + kMaterialIndent) |
                              (Word(row.y - kIconRaise) << kHalfwordShift));
  }
}

// The background's run-length words: zeros, then copied words.
[[nodiscard]] bool UnpackBackground(Context& ctx,
                                    std::span<const std::uint8_t> encoded,
                                    std::span<std::uint32_t> buffer) {
  const std::size_t available = encoded.size() / kWordBytes;
  auto read = [&](std::size_t index, std::uint32_t& word) {
    if (index >= available) {
      ctx.host.Refuse("menu background shorter than its runs");
      return false;
    }
    std::memcpy(&word, encoded.data() + (index * kWordBytes), kWordBytes);
    return true;
  };
  std::size_t out = 0;
  for (std::size_t i = 0; i < kBackgroundEncodedWords;) {
    std::uint32_t runs = 0;
    if (!read(i++, runs)) {
      return false;
    }
    const std::size_t zeros = runs & kHalfwordMask;
    const std::size_t copies = runs >> kHalfwordShift;
    if (zeros + copies > buffer.size() - out) {
      ctx.host.Refuse("menu background runs past its buffer");
      return false;
    }
    std::fill_n(buffer.begin() + static_cast<std::ptrdiff_t>(out), zeros, 0U);
    out += zeros;
    for (std::size_t n = 0; n < copies; ++n) {
      if (!read(i++, buffer[out++])) {
        return false;
      }
    }
  }
  return true;
}
}  // namespace

// decomp: MAINMENU.PRG vs_mainMenu_toggleBackgroundFade
// (src/MENU/MAINMENU.PRG/58EC.c:448)
void SetBackgroundFade(Context& ctx, int fade_state) {
  MainMenuStatics& menu = ctx.state.main_menu;
  if (fade_state != 0) {
    menu.background_ot_offset = kMenuBackgroundOt;
    menu.background_brightness = kFullBrightness;
  }
  menu.background_fade_state = static_cast<std::uint8_t>(fade_state);
}

// decomp: MAINMENU.PRG vs_mainMenu_setBackgroundRenderPriority
// (src/MENU/MAINMENU.PRG/58EC.c:739)
void SetBackgroundPriority(Context& ctx, int ot_offset, int brightness) {
  ctx.host.SetBackgroundRaised(ot_offset > 0);
  ctx.state.main_menu.background_ot_offset =
      static_cast<std::int16_t>(ot_offset);
  ctx.state.main_menu.background_brightness =
      static_cast<std::int16_t>(brightness);
}

// decomp: MAINMENU.PRG vs_mainMenu_unpackMenubg
// (src/MENU/MAINMENU.PRG/58EC.c:751)
void LoadBackgroundTexture(Context& ctx, std::span<std::uint32_t> buffer) {
  const std::span<const std::uint8_t> source = ctx.host.BackgroundSource();
  if (source.size() < kBackgroundClutBytes ||
      buffer.size() < kBackgroundWords) {
    ctx.host.Refuse("menu background without its palette or buffer");
    return;
  }
  if (!UnpackBackground(ctx, source.subspan(kBackgroundClutBytes), buffer)) {
    return;
  }
  ctx.host.ReserveMenuTexturePages();
  for (const ImageRect& rect : kBackgroundClearRects) {
    ctx.host.ClearImage(rect);
  }
  const std::span<const std::uint8_t> palette =
      source.first(kBackgroundClutBytes);
  if ((ctx.state.control.current_menu & kMenuMask) == kDataMenu &&
      (ctx.state.action_restrictions & kOpenedByEvent) != 0 &&
      ctx.host.Story().game_over == kStoryGameOver) {
    ctx.host.UploadImage(kGameOverClutPosition, std::as_bytes(palette),
                         kGameOverClutSize);
  } else {
    ctx.host.LoadBackgroundClut(palette);
  }
  ctx.host.UploadImage(kBackgroundPosition,
                       std::as_bytes(buffer.first(kBackgroundWords)),
                       kBackgroundSize);
  SetBackgroundPriority(ctx, kMenuBackgroundOt, kFullBrightness);
}

// decomp: MAINMENU.PRG _renderBackground (src/MENU/MAINMENU.PRG/58EC.c:1172)
void DrawBackground(Context& ctx, int brightness) {
  const Layer layer{.base = LayerBase::kScene,
                    .slot = ctx.state.main_menu.background_ot_offset};
  const std::uint32_t bank =
      ctx.host.FrameBufferIndex() == 0 ? kScreenWidth : 0;
  brightness *= kBackgroundBrightnessScale;
  const bool opaque = brightness == kFullBrightness;
  if (opaque) {
    brightness = ctx.state.main_menu.background_brightness;
  } else {
    brightness |= kSemiTransparent;
  }
  const std::uint32_t texture = PackClut(kBackgroundClutX, kBackgroundClutY)
                                << kHalfwordShift;
  const std::uint32_t right_size =
      PackXy(kBackgroundRightWidth, kBackgroundHeight);
  const std::uint32_t left_size = PackXy(kBackgroundSplit, kBackgroundHeight);
  EmitSprite(ctx, layer,
             {.draw_mode = kBackgroundRightDrawMode,
              .brightness = brightness,
              .position = kBackgroundSplit,
              .size = right_size,
              .texture = texture});
  EmitSprite(ctx, layer,
             {.draw_mode = kBackgroundLeftDrawMode,
              .brightness = brightness,
              .position = 0,
              .size = left_size,
              .texture = texture});
  if (opaque) {
    return;
  }
  brightness = (kSemiTransparent | kFullBrightness) - brightness;
  EmitSprite(ctx, layer,
             {.draw_mode = kFramePageDrawMode | (bank >> kPageColumnShift),
              .brightness = brightness,
              .position = 0,
              .size = left_size,
              .texture = 0});
  EmitSprite(ctx, layer,
             {.draw_mode = kFramePageDrawMode |
                           ((bank + kBackgroundSplit) >> kPageColumnShift),
              .brightness = brightness,
              .position = kBackgroundSplit,
              .size = right_size,
              .texture = 0});
}

// decomp: MAINMENU.PRG _renderMenuItems (src/MENU/MAINMENU.PRG/58EC.c:1454)
void UpdateAndDrawRows(Context& ctx) {
  for (int i = 0; i < static_cast<int>(kRowCount); ++i) {
    MenuRow& row = MenuItem(ctx, i);
    if (row.state == RowState::kInactive) {
      continue;
    }
    MoveRow(ctx, row);
    if (ctx.state.main_menu.hide_menu != 0) {
      continue;
    }
    DrawRowCounters(ctx, row);
    DrawRowParts(ctx, row);
  }
}
namespace {
// Menu_Framework_AshleyRenderMode.
constexpr int kAshleyRender = 0;
constexpr int kAshleyInit = 1;
constexpr int kAshleyRestore = 2;

// Menu_Inventory_ashleyRenderState: the preview slides in (2) and then
// holds (0), or slides out (1) or drops away (3).
constexpr std::int32_t kPreviewShown = 0;
constexpr std::int32_t kPreviewLeaving = 1;
constexpr std::int32_t kPreviewEntering = 2;
constexpr std::int32_t kPreviewDropping = 3;
constexpr std::int32_t kPreviewSteps = 10;
constexpr std::int32_t kPreviewRaised = 128;
constexpr std::int32_t kPreviewDropBase = 608;
constexpr std::int32_t kPreviewDropStep = 48;
constexpr std::int32_t kPreviewCameraAngle = 0xB00;
constexpr int kPreviewScaleShift = 8;
constexpr int kCollisionHeightShift = 11;
constexpr std::int64_t kPreviewDistanceScale = 4;
constexpr int kPreviewBackgroundOt = 2046;

constexpr std::int16_t kMatrixOne = 0x1000;

// The original's 32-bit arithmetic.
[[nodiscard]] std::int32_t Wrap(std::int64_t value) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

// The preview's slide offset for this frame.
[[nodiscard]] std::int32_t AdvanceAshleyPreview(Context& ctx) {
  AshleyPreviewState preview = ctx.host.AshleyPreview();
  std::int32_t y = preview.y_offset;
  switch (preview.render_state) {
    case kPreviewShown:
      if (y < kPreviewSteps) {
        preview.y_offset = ++y;
      }
      y = kPreviewRaised - AnimationStep(ctx, kPreviewSteps - y);
      break;
    case kPreviewLeaving:
      if (y > 0) {
        preview.y_offset = --y;
      }
      y = AnimationStep(ctx, y);
      break;
    case kPreviewEntering:
      if (y < kPreviewSteps) {
        preview.y_offset = ++y;
      } else {
        preview.render_state = kPreviewShown;
      }
      y = AnimationStep(ctx, kPreviewSteps - y) + kPreviewRaised;
      break;
    case kPreviewDropping:
      if (y > 0) {
        preview.y_offset = --y;
      }
      y = kPreviewDropBase - (y * kPreviewDropStep);
      break;
    default:
      break;
  }
  ctx.host.SetAshleyPreview(preview);
  return y;
}

// The camera looks at Ashley from the menu's angle, offset by the slide.
void DrawAshley(Context& ctx, std::int32_t y_offset) {
  const std::optional<AshleyMetrics> ashley = ctx.host.Ashley();
  if (!ashley) {
    ctx.host.Refuse("the Ashley preview without Ashley's actor");
    return;
  }
  const std::int64_t distance = ashley->camera_distance;
  const std::int64_t sine = Core::Math::Sine(kPreviewCameraAngle);
  const std::int64_t cosine = Core::Math::Cosine(kPreviewCameraAngle);
  const Core::Math::Vec3 look_at{
      .x = Wrap(static_cast<std::int64_t>(Wrap(-sine * y_offset) >>
                                          kPreviewScaleShift) *
                distance),
      .y = Wrap(-static_cast<std::int64_t>(
          Wrap(static_cast<std::int64_t>(ashley->collision_height)
               << kCollisionHeightShift))),
      .z = Wrap(static_cast<std::int64_t>(Wrap(cosine * y_offset) >>
                                          kPreviewScaleShift) *
                distance)};
  const std::int64_t scaled = Wrap(distance * kPreviewDistanceScale);
  const Core::Math::Vec3 position{
      .x = Wrap(look_at.x + static_cast<std::int64_t>(Wrap(cosine * scaled))),
      .y = look_at.y,
      .z = Wrap(look_at.z + static_cast<std::int64_t>(Wrap(sine * scaled)))};
  ctx.host.DrawAshleyPreview(look_at, position);
}
}  // namespace

// decomp: MENU _renderAshley (src/MENU/_renderAshley.h:27)
void RenderAshley(Context& ctx, int mode) {
  AshleyPreviewStatics& ashley = ctx.state.ashley;
  switch (mode) {
    case kAshleyRender: {
      const std::int32_t y_offset = AdvanceAshleyPreview(ctx);
      if (ashley.skip_frame != 0) {
        ashley.skip_frame = 0;
        return;
      }
      if (ashley.render != 0) {
        DrawAshley(ctx, y_offset);
      }
      break;
    }
    case kAshleyInit:
      if (ashley.render == 0) {
        ctx.host.SetAshleyPreview(
            {.y_offset = 0, .render_state = kPreviewEntering});
        ashley.skip_frame = 1;
        ashley.render = 1;
        ctx.host.BeginAshleyPreview();
        SetBackgroundPriority(ctx, kPreviewBackgroundOt, kFullBrightness);
        ctx.host.LoadActorPreview();
      }
      break;
    case kAshleyRestore:
      if (ashley.render != 0) {
        ctx.host.ReleaseActorPreview();
        SetBackgroundPriority(ctx, kMenuBackgroundOt, kFullBrightness);
        ctx.host.EndAshleyPreview();
        ashley.render = 0;
      }
      break;
    default:
      break;
  }
}

// decomp: MENU5.PRG _buildYawMatrix (src/MENU/MENU5.PRG/4D8.c:1756)
Core::Math::Mat3 BuildYawMatrix(std::int16_t angle) {
  const auto sine = static_cast<std::int16_t>(Core::Math::Sine(-angle));
  const auto cosine = static_cast<std::int16_t>(Core::Math::Cosine(-angle));
  const auto negative_sine = static_cast<std::int16_t>(-sine);
  return {
      .m = {
          {{cosine, 0, negative_sine}, {0, kMatrixOne, 0}, {sine, 0, cosine}}}};
}

// decomp: MENU5.PRG _buildPitchMatrix (src/MENU/MENU5.PRG/4D8.c:1772)
Core::Math::Mat3 BuildPitchMatrix(std::int16_t angle) {
  const auto sine = static_cast<std::int16_t>(Core::Math::Sine(angle));
  const auto cosine = static_cast<std::int16_t>(Core::Math::Cosine(angle));
  const auto negative_sine = static_cast<std::int16_t>(-sine);
  return {
      .m = {
          {{kMatrixOne, 0, 0}, {0, cosine, negative_sine}, {0, sine, cosine}}}};
}

// decomp: MAINMENU.PRG _getNextEmptyObjectDataSlot
// (src/MENU/MAINMENU.PRG/C48.c:81)
int NextFreeActorAssetRequest(Context& ctx) {
  ctx.host.UpdateActorAssetQueue();
  return ctx.host.FindFreeActorAssetRequest();
}

// decomp: MAINMENU.PRG _nop (src/MENU/MAINMENU.PRG/C48.c:73)
void NoOperation() {}

namespace {
// The sort keys' stat numbers.
constexpr int kSortCategory = 0;
constexpr int kSortMaterial = 1;
constexpr int kSortRange = 2;
constexpr int kSortDamageType = 3;
constexpr int kSortCurrentDp = 4;
constexpr int kSortMaxDp = 5;
constexpr int kSortCurrentPp = 6;
constexpr int kSortMaxPp = 7;
constexpr int kSortStrength = 8;
constexpr int kSortIntelligence = 9;
constexpr int kSortAgility = 10;
constexpr int kSortFirstClass = 11;
constexpr int kSortFirstAffinity = 17;
constexpr int kSortLastAffinity = 23;
constexpr int kShieldFirstClass = 27;
constexpr int kShieldFirstAffinity = 33;
constexpr int kShieldLastAffinity = 39;
// Equipment stats past the shield's class numbers fold onto the weapon's.
constexpr int kEquipmentFoldedStats = 16;

template <std::size_t N>
[[nodiscard]] std::int32_t SortElement(
    Context& ctx, const std::array<std::int32_t, N>& values, int index) {
  if (index < 0 || static_cast<std::size_t>(index) >= N) {
    ctx.host.Refuse(
        std::format("sort key element {} outside its array", index));
    return 0;
  }
  return values[static_cast<std::size_t>(index)];
}

// The stats every equipment sort key reads the same way.
[[nodiscard]] std::optional<std::int32_t> CommonSortValue(
    int stat, const SortStats& stats) {
  switch (stat) {
    case kSortCurrentDp:
      return stats.current_dp;
    case kSortMaxDp:
      return stats.max_dp;
    case kSortCurrentPp:
      return stats.current_pp;
    case kSortMaxPp:
      return stats.max_pp;
    case kSortStrength:
      return stats.strength;
    case kSortIntelligence:
      return stats.intelligence;
    case kSortAgility:
      return stats.agility;
    default:
      return std::nullopt;
  }
}

// The weapon and part keys' identity stats, negated to sort ascending.
[[nodiscard]] std::optional<std::int32_t> IdentitySortValue(
    int stat, const SortStats& stats) {
  switch (stat) {
    case kSortCategory:
      return -stats.category;
    case kSortMaterial:
      return -stats.material;
    case kSortRange:
      return stats.range;
    case kSortDamageType:
      return -stats.damage_type;
    default:
      return CommonSortValue(stat, stats);
  }
}
}  // namespace

// decomp: MENU _getWeaponStatValue (src/MENU/items1.h:695)
std::int32_t WeaponSortValue(Context& ctx, int stat, const SortStats& weapon) {
  if (const auto value = IdentitySortValue(stat, weapon)) {
    return *value;
  }
  if (stat >= kSortFirstAffinity) {
    return SortElement(ctx, weapon.affinities, stat - kSortFirstAffinity);
  }
  return SortElement(ctx, weapon.classes, stat - kSortFirstClass);
}

// decomp: MENU _getShieldStatValue (src/MENU/items1.h:792)
std::int32_t ShieldSortValue(Context& ctx, int stat, const SortStats& shield) {
  if (stat == kSortMaterial) {
    return -shield.material;
  }
  if (const auto value = CommonSortValue(stat, shield)) {
    return *value;
  }
  if (stat < kShieldFirstAffinity) {
    return SortElement(ctx, shield.classes, stat - kShieldFirstClass);
  }
  if (stat > kShieldLastAffinity) {
    return SortElement(ctx, shield.types, stat - kShieldLastAffinity);
  }
  return SortElement(ctx, shield.affinities, stat - kShieldFirstAffinity);
}

// decomp: MENU _getMiscStatValue (src/MENU/items1.h:888)
std::int32_t MiscSortValue(int stat, const MiscSortStats& misc) {
  if (stat == kSortCategory) {
    return -misc.id;
  }
  return misc.count;
}

// decomp: MENU _getEquipmentStatValue (src/MENU/items1.h:949)
std::int32_t EquipmentSortValue(Context& ctx, int stat,
                                const SortStats& equipment) {
  if (const auto value = IdentitySortValue(stat, equipment)) {
    return *value;
  }
  if (stat >= kShieldFirstClass) {
    stat -= kEquipmentFoldedStats;
  }
  if (stat < kSortFirstAffinity) {
    return SortElement(ctx, equipment.classes, stat - kSortFirstClass);
  }
  if (stat > kSortLastAffinity) {
    return SortElement(ctx, equipment.types, stat - kSortLastAffinity);
  }
  return SortElement(ctx, equipment.affinities, stat - kSortFirstAffinity);
}
}  // namespace VS::Menu::Framework
