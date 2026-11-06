#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Core/Math/ScalarMath.hpp"
#include "Render/GameFont.hpp"
#include "Render/UiPackets.hpp"

// The menu framework of BATTLE.PRG (5BF94.c) and MAINMENU.PRG (58EC.c,
// C48.c) with the copies MENU5, MENU9, MENUD, MENUE, MENU3 and MENUF carry:
// the forty-row menu table, the action and list menus with their cursors,
// the information box, the command label, the action cost panel, the menu
// background, the main menu and its submenu dispatch, the Ashley preview and
// the item sort keys. Every function works on value records over a Context:
// the framework State, the module Tables and a Host (FrameworkLive.hpp binds
// it to the generated globals, the frame ordering tables, the disc queue and
// the submenus).
namespace VS::Menu::Framework {
inline constexpr std::size_t kRowCount = 40;
inline constexpr std::size_t kRowTextBytes = 32;
// The characters a row keeps before its text stops.
inline constexpr std::size_t kRowTextCharacters = kRowTextBytes - 1;
// vs_battle_stringBuf and vs_battle_rowTypeBuf follow the menu table.
inline constexpr std::size_t kStringBufferBytes = 0x60;
inline constexpr std::size_t kRowTypeCount = 49;
inline constexpr std::size_t kSubmenuStateCount = 16;
// D_800F4EE8_t: the cursor memories (two bytes per menu) open the record.
inline constexpr std::size_t kMenuMemoryBytes = 0xF2;
inline constexpr std::size_t kTextHeaderCount = 2;
inline constexpr std::size_t kTextHeaderBytes = 14;
inline constexpr std::size_t kItemNameBytes = 28;
inline constexpr std::size_t kItemDescriptionBytes = 96;
// The fixed copies the row builders take from caller text.
inline constexpr std::size_t kRowNameCopyBytes = 26;
inline constexpr std::size_t kRowDescriptionCopyBytes = 92;
// The information message: chunk code, 96 copied bytes, the end and a NUL.
inline constexpr std::size_t kInformationMessageBytes = 101;
inline constexpr std::size_t kInformationMessageCopyBytes = 96;
// The background image: a 96x240 16-bit image as words.
inline constexpr std::size_t kBackgroundWords = 0x2D00;
// The interface texture InitGameInterface uploads (three images).
inline constexpr std::size_t kInterfaceTextureBytes = 0x12F00;
// Menu_Framework_MenuItemId_None: no row is kept.
inline constexpr int kNoRow = 40;
inline constexpr int kActionRowBase = 32;

// Packed screen coordinates and texture words (vs_getXY, vs_getUV, getClut
// and vs_getUV0Clut).
inline constexpr int kHalfwordShift = 16;
inline constexpr std::uint32_t kHalfwordMask = 0xFFFF;
inline constexpr int kByteShift = 8;
inline constexpr int kClutRowShift = 6;
inline constexpr int kClutColumnShift = 4;
inline constexpr std::uint32_t kClutColumnMask = 0x3F;

[[nodiscard]] constexpr std::uint32_t PackXy(int x, int y) noexcept {
  return (static_cast<std::uint32_t>(x) & kHalfwordMask) |
         (static_cast<std::uint32_t>(y) << kHalfwordShift);
}

[[nodiscard]] constexpr std::uint32_t PackUv(int u, int v) noexcept {
  return static_cast<std::uint32_t>(u) |
         (static_cast<std::uint32_t>(v) << kByteShift);
}

[[nodiscard]] constexpr std::uint32_t PackClut(int x, int y) noexcept {
  return (static_cast<std::uint32_t>(y) << kClutRowShift) |
         ((static_cast<std::uint32_t>(x) >> kClutColumnShift) &
          kClutColumnMask);
}

[[nodiscard]] constexpr std::uint32_t PackUvClut(int u, int v, int clut_x,
                                                 int clut_y) noexcept {
  return PackUv(u, v) | (PackClut(clut_x, clut_y) << kHalfwordShift);
}

// vs_battle_menuItem_t's state.
enum class RowState : std::uint8_t {
  kInactive,
  kIdle,
  kSlideX,
  kSlideY,
  kSlideXy,
  kDismiss,
};

// vs_battle_menuItem_t (BATTLE.PRG 5BF94.h:66) in the record's layout, so
// the live table is viewed in place (FrameworkLive.cpp pins the layout).
struct MenuRow {
  RowState state = RowState::kInactive;
  std::uint8_t w = 0;
  std::uint8_t background_width = 0;
  std::uint8_t animation_step = 0;
  std::uint8_t gradient_state = 0;
  std::uint8_t is_scrollable = 0;
  std::uint8_t selected = 0;
  std::uint8_t unselectable = 0;
  std::uint8_t button_id = 0;
  std::uint8_t row_icon = 0;
  std::uint8_t font_color = 0;
  std::uint8_t invert_gradient = 0;
  std::uint8_t material = 0;
  std::uint8_t outset_icon = 0;
  std::uint16_t item_page = 0;
  std::uint16_t count = 0;
  std::uint16_t max = 0;
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t target0 = 0;
  std::int16_t target1 = 0;
  std::array<std::uint8_t, kRowTextBytes> text{};
  const char* sub_text = nullptr;
};

// One entry of _itemsText and _sortItemsTextBuf (MAINMENU.PRG 58EC.c): the
// name, the row's flags and the description, in the record's layout.
struct ItemText {
  std::array<std::uint8_t, kItemNameBytes> name{};
  std::int32_t flags = 0;
  std::array<std::uint8_t, kItemDescriptionBytes> description{};
};

// textHeader_t (MAINMENU.PRG 58EC.c:21).
struct TextHeader {
  std::array<std::uint8_t, kTextHeaderBytes> text{};
  std::uint8_t disabled = 0;
  std::uint8_t x = 0;
};

// D_800F18EC_t (BATTLE.PRG 5BF94.h:160).
struct ActionSelection {
  std::int16_t menu_status = 0;
  std::int16_t selection_kind = 0;
  std::int32_t execute_ability = 0;
};

// vs_battle_menuState_t without its disc request (the host keeps it).
struct MenuControl {
  std::uint8_t current_menu = 0;
  std::uint8_t return_state = 0;
  std::uint8_t execute_ability_type = 0;
  std::uint8_t loading = 0;
};

// The MAINMENU.PRG statics a fresh load of the overlay restores, with their
// module-image values (its bss is not among them).
inline constexpr std::int16_t kInitialBackgroundBrightness = 128;
inline constexpr std::int16_t kInitialBackgroundOtOffset = -4;
inline constexpr std::uint8_t kActionCostPanelHidden = 8;
inline constexpr std::uint8_t kTabArrowAnimationStart = 11;

struct MainMenuStatics {
  std::uint8_t actions_sub_menu_id = 0;
  std::int16_t background_brightness = kInitialBackgroundBrightness;
  std::uint8_t background_fade_state = 0;
  std::int16_t background_ot_offset = kInitialBackgroundOtOffset;
  std::uint8_t item_action_count = 0;
  std::uint8_t item_action_list_offset = 0;
  std::uint8_t menu_action_current = 0;
  std::uint8_t menu_action_next = 0;
  std::uint8_t menu_row_y_offset = 0;
  std::int32_t selected_item_action = 0;
  std::int32_t selected_row = 0;
  std::uint8_t sub_menu_id = 0;
  std::uint8_t sub_menu_page = 0;
  std::uint16_t sub_menu_row_count = 0;
  std::uint8_t sub_menu_row_info = 0;
  std::uint8_t action_menu_state = 0;
  std::int32_t background_fade_step = 0;
  std::uint8_t cursor_color = 0;
  std::uint8_t display_action_cost = 0;
  std::uint8_t hide_menu = 0;
  std::uint8_t items_list_row = 0;
  std::uint8_t action_menu_cursor_step = 0;
  std::uint8_t row_selection_cursor_step = 0;
  std::int32_t command_label_y = 0;
  std::uint8_t action_cost_panel_step = kActionCostPanelHidden;
  std::uint8_t tab_arrow_step = kTabArrowAnimationStart;
  std::uint8_t scrollable_row_step = 0;
  std::uint8_t scrollable_row_frame = 0;
  std::int32_t quit_delay = 0;
};

// _renderAshley's statics (shared by the MENU3.PRG and MENUF.PRG copies).
struct AshleyPreviewStatics {
  std::int32_t render = 0;
  std::uint8_t skip_frame = 0;
};

// The loot the loot menu shows: a chain of loot lists or one entry. The
// framework compares and stores them and never dereferences them.
enum class LootListId : std::uintptr_t {};
enum class LootEntryId : std::uintptr_t {};

// The framework's variables.
struct State {
  std::int8_t loaded_sub_menu = 0;
  MenuControl control;
  ActionSelection action_selection;
  std::uint8_t shortcut_invoked = 0;
  std::uint8_t opened_without_main_menu = 0;
  std::uint8_t unlocked_break_art = 0;
  std::uint8_t unlocked_battle_ability = 0;
  std::uint8_t save_location_id = 0;
  std::int32_t action_restrictions = 0;
  std::int32_t unused_interface_flag = 0;
  std::int32_t main_menu_cursor_animation = 0;
  std::array<std::uint8_t, kSubmenuStateCount> submenu_states{};
  MainMenuStatics main_menu;
  std::uint8_t menu_actions_page_offset = 0;
  std::array<TextHeader, kTextHeaderCount> text_headers{};
  AshleyPreviewStatics ashley;
  LootListId loot_lists{};
  LootEntryId loot_entry{};
  // vs_mainmenu_setInformationMessage's buffer (the text box keeps it).
  std::array<std::uint8_t, kInformationMessageBytes> information_message{};
  // Where a refused row request writes; nothing reads it.
  MenuRow discard_row;
};

// The gradient window: the top table's first word and the bottom table's,
// and the words FrameworkLive.cpp copies from 0x800EBBB4.
inline constexpr std::size_t kGradientWindowTop = 1;
inline constexpr std::size_t kGradientWindowBottom = 3;
inline constexpr std::size_t kGradientWindowWords = 10;

// The module tables the framework reads.
struct Tables {
  // The words from 0x800EBBB4 that _getMenuItemGradientColor indexes: the
  // help text's last word, D_800EBBB8 (top, two words), D_800EBBC0 (bottom,
  // two words) and the row animation steps; the gradient's stop index
  // reaches one word before the top table (kGradientWindowTop).
  std::span<const std::uint32_t> gradient_colors;
  std::span<const std::uint8_t> row_animation_steps;
  std::span<const std::uint8_t> ability_group_action_ranges;
  std::span<const std::uint8_t> cursor_brightness;
  // Menu_menuStrings as bytes: halfword offsets, then the strings.
  std::span<const std::uint8_t> menu_strings;
  std::span<const std::uint8_t> glyph_widths;
  std::span<const std::uint8_t> main_menu_help_text;
};

// The frame ordering table a packet is linked into: the scene or overlay
// table at a slot, or the caller's `before` link.
enum class LayerBase : std::uint8_t { kScene, kOverlay, kCaller };

struct Layer {
  LayerBase base = LayerBase::kOverlay;
  int slot = 0;
};

enum class MenuSound : std::uint8_t {
  kChange,
  kSelect,
  kLeave,
  kInvalid,
  kOpen
};

// Menu_InformationBoxHeader.
enum class InformationHeader : std::uint8_t { kCaution = 1, kInformation = 2 };

// The two item lists: _itemsText and _sortItemsTextBuf.
enum class ItemList : std::uint8_t { kRows, kActions };

// The screens UpdateMainMenu runs: the ten main-menu entries, then the
// screens opened without the main menu.
enum class Submenu : std::uint8_t {
  kMagic,
  kBreakArts,
  kBattleAbilities,
  kInventory,
  kStatus,
  kMap,
  kData,
  kOptions,
  kScore,
  kManual,
  kResults,
  kUnlockNotifications,
  kContainer,
  kRename,
  kSetup,
  kLoot,
  kContainerSave,
  kGameOver,
};

struct Buttons {
  std::uint32_t pressed = 0;
  std::uint32_t repeat = 0;
  std::uint32_t held = 0;
};

struct Settings {
  std::uint8_t cursor_memory = 0;
  std::uint8_t information = 0;
  std::uint32_t menu_flags = 0;
};

// What ActionRestrictionFlags reads of Ashley.
struct RestrictionInputs {
  std::uint32_t state_flags = 0;
  bool weapon_drawn = false;
  std::int32_t mp = 0;
  std::int32_t hp = 0;
  std::uint32_t statuses = 0;
};

struct StoryFlags {
  std::uint8_t opaque_b5 = 0;
  std::int32_t intro_state = 0;
  std::int32_t game_over = 0;
};

// Menu_Inventory_ashleyYOffset and Menu_Inventory_ashleyRenderState.
struct AshleyPreviewState {
  std::int32_t y_offset = 0;
  std::int32_t render_state = 0;
};

// Ashley's preview camera distance and collision height.
struct AshleyMetrics {
  std::int32_t camera_distance = 0;
  std::int32_t collision_height = 0;
};

// A VRAM rectangle.
struct ImageRect {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t w = 0;
  std::int16_t h = 0;
};

class Host {
 public:
  virtual ~Host() = default;

  // A request the original would have run past its data: logged, skipped.
  virtual void Refuse(std::string_view what) = 0;

  // The live menu table, string buffer and row types (empty with no
  // workspace) and the menu memory record.
  [[nodiscard]] virtual std::span<MenuRow> Rows() = 0;
  [[nodiscard]] virtual std::span<std::uint8_t> Strings() = 0;
  [[nodiscard]] virtual std::span<std::int32_t> RowTypes() = 0;
  [[nodiscard]] virtual std::span<std::uint8_t> MenuMemory() = 0;
  [[nodiscard]] virtual bool AllocateWorkspace() = 0;
  virtual void ReleaseWorkspace() = 0;
  virtual void ForgetWorkspace() = 0;

  [[nodiscard]] virtual std::span<ItemText> Items(ItemList list) = 0;
  [[nodiscard]] virtual bool HasItems(ItemList list) = 0;
  [[nodiscard]] virtual std::span<ItemText> AllocateItems(ItemList list,
                                                          int count) = 0;
  virtual void FreeItems(ItemList list) = 0;

  [[nodiscard]] virtual Buttons Input() = 0;
  [[nodiscard]] virtual Settings MenuSettings() = 0;
  virtual void SetMenuFlags(std::uint32_t flags) = 0;
  virtual void PlaySound(MenuSound sound) = 0;

  virtual void InitInformationBox(InformationHeader header) = 0;
  virtual void SetInformationBoxState(std::uint8_t state) = 0;
  virtual void SetInformationText(std::span<const std::uint8_t> text) = 0;
  // The text with the information portrait (speaker texture 32).
  virtual void ShowInformationMessage(std::span<const std::uint8_t> text) = 0;
  virtual void DismissInformationBox() = 0;
  virtual void SetStringArgument(std::span<const std::uint8_t> text) = 0;

  [[nodiscard]] virtual int CurrentSceneId() = 0;
  [[nodiscard]] virtual std::optional<RestrictionInputs> Restrictions() = 0;
  [[nodiscard]] virtual bool IsActionUnlocked(int action) = 0;
  [[nodiscard]] virtual StoryFlags Story() = 0;
  [[nodiscard]] virtual bool HasUnlockedSpellClass(int spell_class) = 0;
  [[nodiscard]] virtual bool HasUnlockedWeaponBreakArt() = 0;
  [[nodiscard]] virtual bool HasUnlockedBattleAbility(int ability) = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> AshleyName() = 0;
  [[nodiscard]] virtual const char* LimbName(int limb) = 0;

  // The menu overlay: MAINMENU.PRG (with its background) for menu 0, the
  // submenu's MENU*.PRG otherwise, or MAINMENU.PRG alone for the item
  // shortcut.
  virtual void BeginOverlayLoad(int sub_menu, bool main_menu_program) = 0;
  [[nodiscard]] virtual bool IsOverlayLoaded() = 0;
  virtual void FinishOverlayLoad() = 0;
  // InitGameInterface clears the menu state with its disc request.
  virtual void ForgetOverlayLoad() = 0;
  virtual void ResetMainMenuOverlay() = 0;

  [[nodiscard]] virtual bool ReleaseBackgroundBuffer() = 0;
  virtual void StoreEquippedStats() = 0;
  [[nodiscard]] virtual std::span<std::uint32_t> AllocateBackgroundBuffer() = 0;
  virtual void ForgetBackgroundBuffer() = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> BackgroundSource() = 0;

  virtual void UpdateMiscItemShortcut(int mode) = 0;
  [[nodiscard]] virtual int RunSubmenu(Submenu submenu, int state_index) = 0;
  virtual void BindInventoryViews() = 0;
  virtual void UpdateStatPanels() = 0;

  virtual void ReserveMenuTexturePages() = 0;
  virtual void RetireMenuTexturePages() = 0;
  virtual void UploadImage(std::uint32_t position,
                           std::span<const std::byte> pixels,
                           std::uint32_t size) = 0;
  virtual void ClearImage(const ImageRect& rect) = 0;
  virtual void LoadBackgroundClut(std::span<const std::uint8_t> colors) = 0;
  // The battle interface state InitGameInterface resets outside the menu.
  virtual void ResetBattleInterface() = 0;
  virtual void SetBackgroundRaised(bool raised) = 0;

  [[nodiscard]] virtual Render::PacketQueue& Queue(Layer layer) = 0;
  [[nodiscard]] virtual Render::FontStyle& FontStyle() = 0;
  virtual void DrawCompactText(Layer layer, std::string_view text,
                               std::uint32_t position, std::uint32_t color) = 0;
  [[nodiscard]] virtual int FrameBufferIndex() = 0;
  [[nodiscard]] virtual bool TabArrowsFrozen() = 0;

  [[nodiscard]] virtual AshleyPreviewState AshleyPreview() = 0;
  virtual void SetAshleyPreview(const AshleyPreviewState& preview) = 0;
  [[nodiscard]] virtual std::optional<AshleyMetrics> Ashley() = 0;
  virtual void BeginAshleyPreview() = 0;
  virtual void LoadActorPreview() = 0;
  virtual void ReleaseActorPreview() = 0;
  virtual void EndAshleyPreview() = 0;
  virtual void DrawAshleyPreview(const Core::Math::Vec3& look_at,
                                 const Core::Math::Vec3& position) = 0;
  virtual void UpdateActorAssetQueue() = 0;
  [[nodiscard]] virtual int FindFreeActorAssetRequest() = 0;
};

struct Context {
  State& state;
  Host& host;
  const Tables& tables;
};

// A row's place: SetMenuItem's row, position, width and background.
struct RowPlacement {
  int row = 0;
  int x = 0;
  int y = 0;
  int w = 0;
  int background_width = 0;
};

// The fixed-length text a row builder copies (name, then description); an
// empty span stands for a null pointer.
struct RowText {
  std::span<const std::uint8_t> name;
  std::span<const std::uint8_t> description;
};

// A signed number in the compact font.
struct NumberText {
  std::int32_t value = 0;
  std::uint32_t position = 0;
  std::uint32_t color = 0;
};

// A screen box.
struct Box {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

// The equipment fields the sort keys read; the class and affinity arrays
// keep both rows of the loadout records ([2][8]).
inline constexpr std::size_t kSortTypeCount = 4;
inline constexpr std::size_t kSortClassCount = 16;

struct SortStats {
  std::int32_t category = 0;
  std::int32_t material = 0;
  std::int32_t range = 0;
  std::int32_t damage_type = 0;
  std::int32_t current_dp = 0;
  std::int32_t max_dp = 0;
  std::int32_t current_pp = 0;
  std::int32_t max_pp = 0;
  std::int32_t strength = 0;
  std::int32_t intelligence = 0;
  std::int32_t agility = 0;
  std::array<std::int32_t, kSortTypeCount> types{};
  std::array<std::int32_t, kSortClassCount> classes{};
  std::array<std::int32_t, kSortClassCount> affinities{};
};

// The misc item fields _getMiscStatValue reads.
struct MiscSortStats {
  std::int32_t id = 0;
  std::int32_t count = 0;
};

// The rows.
[[nodiscard]] MenuRow& MenuItem(Context& ctx, int row);
MenuRow& SetMenuItem(Context& ctx, const RowPlacement& placement,
                     std::span<const std::uint8_t> text);
[[nodiscard]] std::uint32_t GradientStopColor(Context& ctx,
                                              std::uint32_t gradient,
                                              std::uint32_t color_index,
                                              int brightness);
void DrawMenuItem(Context& ctx, MenuRow& row);
void DeactivateRow(Context& ctx, int row);
void FlyOutRowLeft(Context& ctx, int row);
void FlyOutRowsLeftExcept(Context& ctx, int row);
void FlyOutRowRight(Context& ctx, int row);
void ClearMenuExcept(Context& ctx, int row);
void ClearMenuExceptTitle(Context& ctx);
void FlyOutRowsAndHoistSelection(Context& ctx, int row, int offset);
[[nodiscard]] bool AreMenuRowsIdle(Context& ctx);
void ShowSecondaryTitle(Context& ctx, std::span<const std::uint8_t> text);
void ContinueItemDetailsTransition(Context& ctx, int row);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void UpdateItemDetailsSelection(Context& ctx, int row, const RowText& text,
                                std::uint32_t row_type, int page);

// The main menu and the menu overlays.
[[nodiscard]] int LoadMenuOverlay(Context& ctx, int packed_menu_state);
int UpdateMainMenuSelection(Context& ctx, int mode);
void UpdateMenuOverlay(Context& ctx);
void UpdateMainMenu(Context& ctx, int packed_menu_state);
[[nodiscard]] int QuitDelay(Context& ctx);
int ReturnFromSubmenu(Context& ctx, int packed_menu_state);
void UpdateScreen(Context& ctx);
void UpdateAndDrawRows(Context& ctx);
void SetMenuVisible(Context& ctx, int display);
int DrawMainMenuCursor(Context& ctx, int step, int position);

// Opening and closing the game menu.
void InitGameInterface(Context& ctx,
                       std::span<const std::byte> interface_texture);
void ResumeAfterActionSelection(Context& ctx);
[[nodiscard]] std::int32_t RestrictionFlagsOf(const RestrictionInputs& inputs,
                                              int mode);
[[nodiscard]] std::int32_t ActionRestrictionFlags(Context& ctx, int mode);
int OpenGameMenu(Context& ctx, int menu_state, int mode);
int OpenShortcutMenu(Context& ctx, int shortcut);
void ReleaseMenuRows(Context& ctx);
void OpenLootListMenu(Context& ctx, LootListId lists);
void OpenLootEntryMenu(Context& ctx, LootEntryId entry);
void OpenChestStorageMenu(Context& ctx, int location);
void OpenAbilityUnlockMenu(Context& ctx, int art, int ability);
void OpenChallengeMenu(Context& ctx);
[[nodiscard]] ActionSelection& ActionSelectionState(Context& ctx);

// The action menu.
void CreateActionRows(Context& ctx, int row_count, int sub_menu_id,
                      std::span<const RowText> texts,
                      std::span<const std::int32_t> row_types);
[[nodiscard]] int HighlightedActionRow(Context& ctx);
[[nodiscard]] int ActionSelectionResult(Context& ctx);
MenuRow& CreateActionRow(Context& ctx, int row, int x);
void UpdateActionMenu(Context& ctx);

// The list menu.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void CreateMenuRows(Context& ctx, int row_count, int row_info,
                    std::span<const RowText> texts,
                    std::span<const std::int32_t> row_types);
int CloseRowsAndRememberCursor(Context& ctx);
[[nodiscard]] int HighlightedRow(Context& ctx);
[[nodiscard]] int GetSelectedRow(Context& ctx);
MenuRow& CreateListRow(Context& ctx, int row, int x);
void UpdateRowSelection(Context& ctx);

// The information box, the command label and the action cost panel.
void UpdateInformationVisibility(Context& ctx, int source_row,
                                 int action_menu_state);
void SetCommandLabel(Context& ctx, int action);
void DrawCommandLabel(Context& ctx);
void DismissInformationBox(Context& ctx);
void InitializeInformationBox(Context& ctx);
void SetInformationMessage(Context& ctx, std::span<const std::uint8_t> message);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetActionCostText(Context& ctx, int index,
                       std::span<const std::uint8_t> text, int x_offset,
                       int disabled);
void DrawActionCostPanel(Context& ctx);

// Icons, cursors and numbers.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void DrawButtonIcon(Context& ctx, int button_id, int x, int y, Layer layer);
int DrawCursor(Context& ctx, std::uint32_t step, std::uint32_t position);
std::uint32_t DrawColoredInteger(Context& ctx, const NumberText& number,
                                 Layer layer);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
std::uint32_t DrawComparedInteger(Context& ctx, std::int32_t value,
                                  std::uint32_t position,
                                  std::int32_t threshold, Layer layer);
void DrawNeutralInteger(Context& ctx, std::int32_t value,
                        std::uint32_t position, Layer layer);
void DrawFeatheredRowIcon(Context& ctx, int direction, std::uint32_t position,
                          std::uint32_t uv_clut, Layer layer);
void DrawMaterialIndicator(Context& ctx, int material, std::uint32_t position);
void DrawEquipmentStateIcon(Context& ctx, int icon, std::uint32_t position);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void DrawRowIcon(Context& ctx, int icon, int x, int y);
void DrawButtonPromptBackground(Context& ctx, const Box& box);
void DrawTabNavigation(Context& ctx, int mode);
int DrawFeatheredRowCharacter(Context& ctx, std::uint32_t character, int x,
                              const MenuRow& row, Layer layer);
void DrawScrollableRow(Context& ctx, MenuRow& row);

// The background.
void SetBackgroundFade(Context& ctx, int fade_state);
void SetBackgroundPriority(Context& ctx, int ot_offset, int brightness);
void LoadBackgroundTexture(Context& ctx, std::span<std::uint32_t> buffer);
void DrawBackground(Context& ctx, int brightness);

// The Ashley preview and the actor asset queue.
void RenderAshley(Context& ctx, int mode);
[[nodiscard]] Core::Math::Mat3 BuildYawMatrix(std::int16_t angle);
[[nodiscard]] Core::Math::Mat3 BuildPitchMatrix(std::int16_t angle);
[[nodiscard]] int NextFreeActorAssetRequest(Context& ctx);
void NoOperation();

// The item sort keys.
[[nodiscard]] std::int32_t WeaponSortValue(Context& ctx, int stat,
                                           const SortStats& weapon);
[[nodiscard]] std::int32_t ShieldSortValue(Context& ctx, int stat,
                                           const SortStats& shield);
[[nodiscard]] std::int32_t MiscSortValue(int stat, const MiscSortStats& misc);
[[nodiscard]] std::int32_t EquipmentSortValue(Context& ctx, int stat,
                                              const SortStats& equipment);
}  // namespace VS::Menu::Framework
