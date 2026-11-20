#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <span>

#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Framework/Prompts.hpp"

// Binds the menu framework (Framework.hpp) to its variables
// (FrameworkLiveState.hpp), the frame ordering tables, the disc queue, the text
// boxes, the battle interface and the submenus, in the shapes the copied
// menus and the battle flow call. Each entry runs the typed function over the
// live State and writes the State back before it returns.
namespace VS::Menu::Framework {
struct ActionSelection;
enum class AshleyRenderMode : std::int32_t;
struct Buttons;
struct MenuRow;
struct RowPlacement;
struct RowText;

// The live pad state (Input/ControllerLiveState.hpp) in the menus' shape.
[[nodiscard]] Buttons LiveButtons();

// BATTLE.PRG (5BF94.c).
int LoadLiveMenuOverlay(int packed_menu_state);
MenuRow* LiveMenuItem(int row);
MenuRow* SetLiveMenuItem(int row, int x, int y, int w, int background_width,
                         char* text);
// SetLiveMenuItem for typed hosts: the row builder trusts its text for
// kTrustedEncodedTextBytes, so it reads a terminator-padded copy of `text`.
MenuRow& SetLiveMenuItemText(const RowPlacement& placement,
                             std::span<const std::uint8_t> text);
// LiveMenuItem for typed hosts.
MenuRow& LiveMenuRow(int row);
[[nodiscard]] std::span<std::uint8_t> LiveMenuCursorMemory();
int LiveGradientStopColor(std::uint32_t gradient, std::uint32_t color_index,
                          int brightness);
void DrawLiveMenuItem(MenuRow* row);
int UpdateLiveMainMenuSelection(int mode);
void UpdateLiveMenuOverlay();
void InitLiveGameInterface(void* interface_texture);
void ResumeLiveAfterActionSelection();
int LiveActionRestrictionFlags(int mode);
int OpenLiveGameMenu(int menu_state, int mode);
int OpenLiveShortcutMenu(int shortcut);
void ReleaseLiveMenuRows();
void OpenLiveLootListMenu(VS::Items::LootListNode* lists);
void OpenLiveLootEntryMenu(VS::Items::BattleLoot* entry);
void OpenLiveChestStorageMenu(int location);
void OpenLiveAbilityUnlockMenu(int art, int ability);
void OpenLiveChallengeMenu();
ActionSelection* LiveActionSelectionState();
int DrawLiveMainMenuCursor(int step, int position);

// MAINMENU.PRG (58EC.c, C48.c).
void CreateLiveActionRows(int row_count, int sub_menu_id, char* menu_text[],
                          int row_types[]);
void CreateLiveActionRowsText(int sub_menu_id, std::span<const RowText> texts,
                              std::span<const std::int32_t> row_types);
int LiveHighlightedActionRow();
int LiveActionSelectionResult();
MenuRow* CreateLiveActionRow(int row, int x);
void UpdateLiveActionMenu();
void UpdateLiveInformationVisibility(int source_row, int action_menu_state);
void SetLiveCommandLabel(MenuCommand action);
void DrawLiveCommandLabel();
void SetLiveBackgroundFade(int fade_state);
void DeactivateLiveRow(int row);
void DismissLiveInformationBox();
void InitializeLiveInformationBox();
void SetLiveInformationMessage(char* message);
void SetLiveInformationMessageText(std::span<const std::uint8_t> message);
// A null `before` links into the overlay ordering table.
void DrawLiveButtonIcon(ButtonIcon button, int x, int y, std::uint32_t* before);
int DrawLiveCursor(std::uint32_t step, int position);
int DrawLiveColoredInteger(int value, int xy, int color, std::uint32_t* before);
int DrawLiveComparedInteger(int value, int xy, int threshold,
                            std::uint32_t* before);
void DrawLiveNeutralInteger(int value, int xy, std::uint32_t* before);
void DrawLiveFeatheredRowIcon(int direction, int xy, int uv_clut,
                              std::uint32_t* before);
void DrawLiveMaterialIndicator(int material, int xy);
void DrawLiveEquipmentStateIcon(int icon, int xy);
void DrawLiveRowIcon(int icon, int x, int y);
void SetLiveActionCostText(int index, char const* text, int x_offset,
                           int disabled);
void DrawLiveActionCostPanel();
void DrawLiveButtonPromptBackground(int x, int y, int w, int h);
void SetLiveBackgroundPriority(int ot_offset, int brightness);
void LoadLiveBackgroundTexture(std::uint32_t* buffer);
void CreateLiveMenuRows(int row_count, int row_info, char* strings[],
                        int row_types[]);
int CloseLiveRowsAndRememberCursor();
int LiveHighlightedRow();
int LiveSelectedRow();
MenuRow* CreateLiveListRow(int row, int x);
void UpdateLiveRowSelection();
void DrawLiveBackground(int brightness);
int DrawLiveFeatheredRowCharacter(std::uint32_t character, int x, MenuRow* row,
                                  std::uint32_t* before);
void DrawLiveTabNavigation(int mode);
void DrawLiveScrollableRow(MenuRow* row);
void UpdateAndDrawLiveRows();
void UpdateLiveScreen();
int LiveNextFreeActorAssetRequest();
void FlyOutLiveRowLeft(int row);
void FlyOutLiveRowsLeftExcept(int row);
void FlyOutLiveRowRight(int row);
void ClearLiveMenuExcept(int row);
void FlyOutLiveRowsAndHoistSelection(int row, int offset);
int AreLiveMenuRowsIdle();
int LiveQuitDelay();
int ReturnLiveFromSubmenu(int packed_menu_state);
void UpdateLiveMainMenu(int packed_menu_state);

// The MENU*.PRG copies.
void RenderLiveAshley(AshleyRenderMode mode);
void ContinueLiveItemDetailsTransition(int row);
void UpdateLiveItemDetailsSelection(int row, char* text[],
                                    std::uint32_t row_type, int page);
int LiveWeaponSortValue(int stat, const VS::Items::WeaponLoadout* weapon);
int LiveShieldSortValue(int stat, const VS::Items::ShieldLoadout* shield);
int LiveMiscSortValue(int stat, const VS::Items::InventoryMisc* misc);
int LiveEquipmentSortValue(int stat, const VS::Items::EquipmentPart* equipment);
void SetLiveMenuVisible(int display);
void ClearLiveMenuExceptTitle();
void BuildLiveYawMatrix(VS::Core::Gte::MATRIX* matrix, std::int16_t angle);
void BuildLivePitchMatrix(VS::Core::Gte::MATRIX* matrix, std::int16_t angle);
void ShowLiveSecondaryTitle(char* text);

// Restores the MAINMENU.PRG statics a fresh load of the overlay brings back
// (the module image values) and drops both item lists' pointers.
void ResetLiveMainMenuStatics();
}  // namespace VS::Menu::Framework
