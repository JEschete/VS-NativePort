#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Menu/Framework/Framework.hpp"
#include "Menu/Sounds.hpp"
#include "Render/UiPackets.hpp"

// MENU8.PRG's weapon naming screen (21A0.c): the name line, the command
// column, the character grid and the confirmation. Every function works on a
// Context: the screen's State and a Host (RenameLive.hpp binds it to the menu
// framework, the frame's ordering tables, the pad, the inventory and the item
// names).
namespace VS::Menu::Rename {
// _stringBuf: the edited name, one glyph a column.
inline constexpr std::size_t kNameColumns = 20;
// vs_main_inventoryWeapon.name and _nameEditorNameBuffer.
inline constexpr std::size_t kNameBytes = 24;
// _renderRenameMenuStrings (21A0.c): halfword offsets, the strings, then the
// character grid.
inline constexpr std::size_t kMenuStringHalfwords = 208;
// _frameControllerActionAnimationCounters.
inline constexpr std::size_t kActionCounters = 8;
// The command SetCommandHighlight takes to clear the command column.
inline constexpr int kNoCommand = 255;

// _renameMenuInput's states (21A0.c:463).
enum class EditorStep : std::uint8_t {
  kDelay,
  kCommands,
  kGrid,
  kLeave,
  kInitConfirmation,
  kConfirmation,
  kReturnToGrid,
};

// The command column from the top: accept the name, delete before the
// cursor, delete at the cursor and insert a space.
enum class Command : std::uint8_t { kOk, kBack, kDelete, kInsert };

struct State {
  // _stringBuf.
  std::array<std::uint8_t, kNameColumns> name{};
  // _renderRenameMenu's statics and the command it flashes next.
  std::array<std::uint8_t, kActionCounters> action_counters{};
  std::uint8_t selected_action = 0;
  bool nav_icon_pulse = false;
  std::uint8_t nav_icon_animation = 0;
  std::uint8_t unused_frame_cycle = 0;
  // _confirmationScreen's statics.
  std::uint8_t confirmation_step = 0;
  std::uint8_t confirmation_row = 0;
  std::uint8_t confirmation_cursor = 0;
  // _renameMenuInput's statics: the grid cell (or the command row), the name
  // column and the confirmed name.
  EditorStep editor_step = EditorStep::kDelay;
  std::uint8_t cursor_animation = 0;
  std::uint8_t column = 0;
  std::uint8_t row = 0;
  std::uint8_t position = 0;
  std::array<std::uint8_t, kNameBytes> confirmed_name{};
  // vs_menu8_execRename's: the weapon, the frame's visibility and slide.
  std::uint8_t weapon = 0;
  bool show_interface = false;
  std::uint8_t animation_step = 0;
};

// The menu framework's request (vs_battle_stringBuf[0..1]): whether to start
// from the weapon's name, and the weapon.
struct Request {
  bool edit_existing = false;
  std::uint8_t weapon = 0;
};

// The vs_main_inventoryWeapon fields the screen reads.
struct Weapon {
  std::uint8_t blade = 0;
  bool equipped = false;
  std::array<std::uint8_t, kNameBytes> name{};
};

// A selection box: DrawSelectionBox's rectangle.
struct Box {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

// A character grid cell.
struct GridCell {
  int column = 0;
  int row = 0;
};

class Host {
 public:
  virtual ~Host() = default;
  virtual void Refuse(std::string_view what) = 0;
  // _renderRenameMenuStrings as bytes.
  [[nodiscard]] virtual std::span<const std::uint8_t> MenuStrings() = 0;
  [[nodiscard]] virtual Framework::Buttons Input() = 0;
  virtual void PlaySound(MenuSound sound) = 0;
  // An effect of the menu sound group (the accepted name's).
  virtual void PlaySoundEffect(int effect) = 0;

  [[nodiscard]] virtual Render::PacketQueue& Queue(Framework::Layer layer) = 0;
  // vs_battle_printVariableWidthFontChar into the four text layers at
  // `layers`.
  virtual void DrawGlyph(std::uint8_t glyph, int x, int y,
                         Framework::Layer layers) = 0;
  virtual void SetFontStyle(int style) = 0;
  // In front of the frame's overlay ordering table.
  virtual void DrawCompactText(std::string_view text, std::uint32_t position,
                               std::uint32_t color) = 0;
  // In front of the frame's overlay ordering table.
  virtual void DrawButtonIcon(int button, int x, int y) = 0;
  [[nodiscard]] virtual std::uint32_t GradientStopColor(
      std::uint32_t gradient, std::uint32_t color_index, int brightness) = 0;
  [[nodiscard]] virtual int DrawCursor(int step, std::uint32_t position) = 0;
  [[nodiscard]] virtual int DrawMainMenuCursor(int step, int position) = 0;

  virtual Framework::MenuRow& SetMenuItem(
      const Framework::RowPlacement& placement,
      std::span<const std::uint8_t> text) = 0;
  [[nodiscard]] virtual Framework::MenuRow& MenuItem(int row) = 0;
  [[nodiscard]] virtual bool RowsIdle() = 0;
  virtual void FlyOutRowRight(int row) = 0;
  virtual void FlyOutRowLeft(int row) = 0;
  // vs_mainMenu_rowAnimationSteps.
  [[nodiscard]] virtual std::span<const std::uint8_t> RowAnimationSteps() = 0;

  virtual void DismissInformationBox() = 0;
  virtual void InitializeInformationBox() = 0;
  // The caution box with the prompt formatted around the name.
  virtual void ShowConfirmPrompt(std::span<const std::uint8_t> name,
                                 std::span<const std::uint8_t> prompt) = 0;
  virtual void DismissConfirmPrompt() = 0;
  // vs_battle_menuState.currentMenu = 13: back to the weapon's menu.
  virtual void ReturnToMenu() = 0;

  [[nodiscard]] virtual std::optional<Request> RenameRequest() = 0;
  [[nodiscard]] virtual std::optional<Weapon> LoadWeapon(int weapon) = 0;
  // The item id of blade slot `blade` (0-based).
  [[nodiscard]] virtual std::optional<std::uint16_t> BladeItem(int blade) = 0;
  // The item's name in the loaded item names; empty when absent.
  [[nodiscard]] virtual std::span<const std::uint8_t> ItemName(int item) = 0;
  virtual void StoreWeaponName(int weapon,
                               std::span<const std::uint8_t> name) = 0;
  // vs_main_equipWeapon for the renamed equipped weapon.
  virtual void EquipWeapon(int weapon) = 0;
};

struct Context {
  State& state;
  Host& host;
};

// MENU8.PRG _insertDeleteHandler (21A0.c:29): Triangle inserts a space,
// Square deletes, L1 and R1 move the name cursor. Returns the cursor.
int EditNameAtCursor(Context& ctx, int position);
// MENU8.PRG _copyNormalizedString (21A0.c:92): the name with its trailing
// spaces cut and the tail zeroed; true when it is blank.
bool CopyTrimmedName(Context& ctx, std::span<std::uint8_t, kNameBytes> out);
// MENU8.PRG _renderRenameMenu (21A0.c:133), slid right by `x_offset`.
void DrawFrame(Context& ctx, int x_offset);
// MENU8.PRG _highlightSelection (21A0.c:318).
void DrawSelectionBox(Context& ctx, const Box& box);
// MENU8.PRG _highlightCharSelection (21A0.c:346): the cell's box and the
// cursor; returns the cursor's step.
int HighlightGridCharacter(Context& ctx, int cursor_step, GridCell cell);
// MENU8.PRG _confirmationScreen (21A0.c:364): 0 while it runs, then 1 for
// yes, 2 for no and 3 for cancel.
int UpdateConfirmation(Context& ctx, bool initialize);
// MENU8.PRG _updateMenuSelection (21A0.c:445).
void SetCommandHighlight(Context& ctx, int row);
// MENU8.PRG _renameMenuInput (21A0.c:463): 1 once the name is decided.
int UpdateNameEditor(Context& ctx, bool initialize);
// MENU8.PRG vs_menu8_execRename (21A0.c:820): one frame of the screen.
int Update(Context& ctx, std::uint8_t& step);
}  // namespace VS::Menu::Rename
