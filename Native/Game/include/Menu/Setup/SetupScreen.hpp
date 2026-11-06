#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Sounds.hpp"

// MENUC.PRG, the workshop ("Setup") menu (src/MENU/MENUC.PRG/168.c and
// 9258.c): the top menu, the shared prompts and item list, disassembly,
// renaming and repair (this header), weapon assembly and gem attachment
// (Assembly.hpp) and blade, shield and armor combination (Combination.hpp).
// Every function works on a Context: the menu's State and a Host
// (SetupLive.hpp binds it to the menu framework, the pad, the inventory, the
// item texts, the disc queue and Ashley's equipment).
namespace VS::Menu::Setup {
// MENU12.BIN's fixed texts as halfword offsets
// (Text_Menu_MENU12_BIN_Offset_*); SetupLive.cpp pins them.
namespace menu12 {
inline constexpr std::size_t kSetup = 87;
inline constexpr std::size_t kAttachGems = 123;
inline constexpr std::size_t kDisassemble = 147;
inline constexpr std::size_t kRename = 184;
inline constexpr std::size_t kCombine = 237;
inline constexpr std::size_t kAssemble = 273;
inline constexpr std::size_t kInvalidGrip = 309;
inline constexpr std::size_t kInvalidBlade = 337;
inline constexpr std::size_t kInvalidBladeMaterial = 365;
inline constexpr std::size_t kInvalidShieldMaterial = 395;
inline constexpr std::size_t kInvalidArmorMaterial = 426;
inline constexpr std::size_t kChooseWeaponRename = 456;
inline constexpr std::size_t kRenamePrompt = 470;
inline constexpr std::size_t kNoGems = 480;
inline constexpr std::size_t kChooseWeaponDisassemble = 497;
inline constexpr std::size_t kChooseShieldDisassemble = 513;
inline constexpr std::size_t kRepairConfirm = 770;
inline constexpr std::size_t kRepairComplete = 781;
inline constexpr std::size_t kNotInWorkshop = 817;
inline constexpr std::size_t kWeaponsFull = 840;
inline constexpr std::size_t kNoWeapons = 858;
inline constexpr std::size_t kNoItems = 884;
inline constexpr std::size_t kSelectGem = 919;
inline constexpr std::size_t kPressOk = 926;
inline constexpr std::size_t kWeapon = 1030;
inline constexpr std::size_t kBlades = 1034;
inline constexpr std::size_t kBladesDesc = 1038;
inline constexpr std::size_t kGrips = 1046;
inline constexpr std::size_t kGripsDesc = 1049;
inline constexpr std::size_t kGems = 1057;
inline constexpr std::size_t kGemsDesc = 1060;
inline constexpr std::size_t kAttachmentConfirm = 1067;
inline constexpr std::size_t kBlade = 1078;
inline constexpr std::size_t kShield = 1081;
inline constexpr std::size_t kArmor = 1085;
inline constexpr std::size_t kSelectBladeToCombine = 1113;
inline constexpr std::size_t kSelectShield = 1121;
inline constexpr std::size_t kSelectArmor = 1130;
inline constexpr std::size_t kCombineBlade = 1159;
inline constexpr std::size_t kCombineShield = 1167;
inline constexpr std::size_t kCombineArmor = 1175;
inline constexpr std::size_t kCancelCombine = 1183;
inline constexpr std::size_t kCancelCombineConfirm = 1191;

// MENU12.BIN's offset-table entries (Text_Menu_MENU12_BIN_Index_*).
inline constexpr int kAssembleWeaponsIndex = 1;
inline constexpr int kAssembleWeaponsDescIndex = 2;
inline constexpr int kAssembleIndex = 13;
inline constexpr int kWeaponsIndex = 28;
inline constexpr int kAttachWeaponIndex = 32;
inline constexpr int kOptionYesIndex = 34;
inline constexpr int kDisassembleWeaponDescIndex = 37;
inline constexpr int kDisassembleWeaponConfirmIndex = 39;
inline constexpr int kDisassembleWeaponIndex = 41;
inline constexpr int kNoItemsIndex = 53;
inline constexpr int kSelectBladeToAssembleIndex = 54;
inline constexpr int kSelectGemIndex = 56;
inline constexpr int kPressOkIndex = 57;
inline constexpr int kWeaponGemSlotsIndex = 58;
inline constexpr int kWeaponNoGemSlotsIndex = 59;
inline constexpr int kBladeIndex = 70;
inline constexpr int kCombineBladesIndex = 73;
inline constexpr int kNewBladeIndex = 79;
}  // namespace menu12

// The pad bits the menu reads (Input_buttonsPressed / Input_buttonRepeat).
namespace pad {
inline constexpr std::uint32_t kTriangle = 0x10;
inline constexpr std::uint32_t kCircle = 0x20;
inline constexpr std::uint32_t kCross = 0x40;
inline constexpr std::uint32_t kStart = 0x800;
inline constexpr std::uint32_t kUp = 0x1000;
inline constexpr std::uint32_t kDown = 0x4000;
// Any button in the low byte (the repair result's dismissal).
inline constexpr std::uint32_t kLowByte = 0xFF;
}  // namespace pad

[[nodiscard]] inline bool Pressed(const Framework::Buttons& buttons,
                                  std::uint32_t bits) {
  return (buttons.pressed & bits) != 0;
}
[[nodiscard]] inline bool Repeated(const Framework::Buttons& buttons,
                                   std::uint32_t bits) {
  return (buttons.repeat & bits) != 0;
}

// The menu rows the screens use.
inline constexpr int kTitleRow = 0;
inline constexpr int kDetailRow = 10;
inline constexpr int kItemListRow = 20;
inline constexpr int kPromptRow = 29;
inline constexpr int kHeaderRow = 32;
inline constexpr int kNoRow = Framework::kNoRow;
// The item list shows six rows and holds up to 1 + 48 entries.
inline constexpr std::size_t kItemListCapacity = 49;
inline constexpr int kItemListVisibleRows = 6;
// The description buffer one formatted row text takes.
inline constexpr auto kRowTextBytes = Framework::kRowDescriptionBufferBytes;
// _availableItems and _slotSelectionHistory.
inline constexpr std::size_t kAvailableItemCount = 64;
inline constexpr std::size_t kSlotHistoryCount = 5;
// The header row icons: a weapon, a shield and an armor piece.
inline constexpr int kWeaponIcon = 24;
inline constexpr int kShieldIcon = 27;
inline constexpr int kArmorIcon = 28;

// The workshop's sound effects (vs_main_playSfxDefault(0x7E, id)).
namespace sfx {
inline constexpr int kRepair = 0xD;
inline constexpr int kCombine = 0x18;
inline constexpr int kDisassemble = 0x19;
inline constexpr int kListChoice = 0x21;
inline constexpr int kListFirstChoice = 0x22;
}  // namespace sfx

// Audio_Seq_unusedMaterialModifiers' strength, intelligence and agility
// bonuses of one material.
struct MaterialBonus {
  std::int8_t strength = 0;
  std::int8_t intelligence = 0;
  std::int8_t agility = 0;
};
inline constexpr std::size_t kMaterialCount = 8;
using MaterialBonuses = std::array<MaterialBonus, kMaterialCount>;

// BLADE.SYD, SHIELD.SYD and ARMOR.SYD: _loadSyd's id - 1.
enum class RecipeKind : std::uint8_t { kBlade, kShield, kArmor };
inline constexpr std::size_t kRecipeKindCount = 3;
// A decoded recipe file (Combination.hpp).
struct RecipeBook;

// The item list (_populateItemsList, _navigateItemsList).
struct ItemListState {
  // _itemsList: the entries, open while the list runs.
  std::array<Framework::ItemText, kItemListCapacity> items{};
  bool open = false;
  // _itemCount, _itemsListWindow, _itemsListSelection, _itemsListState,
  // _rowInfoUnused and the cursor animation.
  std::uint8_t count = 0;
  std::uint8_t window = 0;
  int selection = 0;
  std::uint8_t step = 0;
  std::uint8_t row_info_unused = 0;
  std::uint8_t cursor = 0;
};

// _confirmationPrompt and _confirmCombine.
struct PromptState {
  std::uint8_t step = 0;
  std::uint8_t selected = 0;
  // The three-choice prompt's row that reads "cancel combine".
  std::uint8_t cancel_row = 0;
  std::uint8_t cursor = 0;
};

// _disassembleMenu, _renameWeaponMenu and _attachGemsMenu's selection.
struct EquipmentScreenState {
  int selected_row = 0;
  std::uint8_t step = 0;
  std::uint8_t appear_step = 0;
  bool is_shield = false;
};

// A category menu's state and whether it keeps the cursor where it was.
struct CategoryState {
  std::uint8_t step = 0;
  bool keep_cursor = false;
};

// _repairMenu.
struct RepairState {
  int returned_from_prompt = 0;
  std::uint8_t step = 0;
};

// Gem ids by slot, as many as a shield takes; the original packs them into
// one int, byte i being gem i.
using GemIds = std::array<std::uint8_t, Items::kShieldGemSlots>;

// _assembleMenu.
struct AssemblyState {
  std::uint8_t step = 0;
  std::uint8_t selected_option = 0;
  // The 1-based blade and grip and the staged gems.
  std::uint8_t blade = 0;
  std::uint8_t grip = 0;
  GemIds gems{};
  std::uint8_t cursor = 0;
  std::uint8_t ok_blink = 0;
};

// _assembleBladeMenu, _assembleGripMenu and _attachGem: the chosen row, the
// part on show and the other part it must fit.
struct PartSelectionState {
  int selected_row = 0;
  std::uint8_t step = 0;
  std::uint8_t current = 0;
  std::uint8_t partner = 0;
};

// _attachGemsMenu.
struct GemAttachmentState {
  EquipmentScreenState screen;
  std::uint8_t selected_slot = 0;
  std::uint8_t gem_slots = 0;
  std::uint8_t cursor = 0;
  std::uint8_t ok_blink = 0;
  GemIds staged{};
  GemIds target{};
};

// _combineBladeMenu, _combineShieldMenu and _combineArmorMenu.
struct CombinationMenuState {
  std::uint8_t step = 0;
  std::uint8_t row = 0;
  std::uint8_t ok_blink = 0;
  std::uint8_t cursor = 0;
};

// _selectBlade, _selectShields and _initUiArmor.
struct IngredientSelectionState {
  int selected_row = 0;
  std::uint8_t step = 0;
  std::uint8_t slot = 0;
};

struct CombinationState {
  CombinationMenuState blade_menu;
  CombinationMenuState shield_menu;
  CombinationMenuState armor_menu;
  IngredientSelectionState blade_selection;
  IngredientSelectionState shield_selection;
  IngredientSelectionState armor_selection;
  // _bladeBuf, _shieldBuf and _armorBuf: the preview.
  Items::InventoryBlade blade{};
  Items::InventoryShield shield{};
  Items::InventoryArmor armor{};
  // _combiningBladeIds, _itemsToCombine and _combiningArmorIds.
  // (_itemsToCombine holds 16 bytes; the menu only uses the first two.)
  std::array<std::uint8_t, 2> blade_ids{};
  std::array<std::uint8_t, 2> shield_ids{};
  std::array<std::uint8_t, 2> armor_ids{};
  // _currentWorkshop.
  std::uint8_t workshop = 0;
  // _loadSyd's request in flight.
  bool recipes_loading = false;
  // The preview's recipe lookup was refused; confirming refuses too.
  bool refused = false;
  CategoryState category;
};

// The menu state the copied statics held.
struct State {
  ItemListState item_list;
  PromptState confirmation;
  PromptState three_choice;
  EquipmentScreenState disassembly;
  CategoryState disassembly_category;
  EquipmentScreenState rename;
  RepairState repair;
  bool first_entry = false;
  AssemblyState assembly;
  PartSelectionState blade_selection;
  PartSelectionState grip_selection;
  PartSelectionState gem_selection;
  GemAttachmentState gem_attachment;
  CategoryState gem_category;
  CombinationState combination;
  // _availableItems, _combiningItem and _slotSelectionHistory.
  std::array<std::uint8_t, kAvailableItemCount> available_items{};
  std::uint8_t combining_item = 0;
  std::array<std::uint32_t, kSlotHistoryCount> slot_history{};
};

class Host {
 public:
  virtual ~Host() = default;
  virtual void Refuse(std::string_view what) = 0;
  [[nodiscard]] virtual Framework::Buttons Input() = 0;
  virtual void PlaySound(MenuSound sound) = 0;
  // vs_main_playSfxDefault(0x7E, effect).
  virtual void PlaySoundEffect(int effect) = 0;

  // The menu rows.
  [[nodiscard]] virtual Framework::MenuRow& MenuItem(int row) = 0;
  virtual Framework::MenuRow& SetMenuItem(
      const Framework::RowPlacement& placement,
      std::span<const std::uint8_t> text) = 0;
  [[nodiscard]] virtual bool RowsIdle() = 0;
  virtual void ClearMenuExcept(int row) = 0;
  virtual void FlyOutRowLeft(int row) = 0;
  virtual void FlyOutRowRight(int row) = 0;
  virtual void FlyOutRowsAndHoistSelection(int row, int offset) = 0;
  virtual void DeactivateRow(int row) = 0;
  // vs_mainmenu_setMenuRows: one row per text pair.
  virtual void CreateMenuRows(int row_info,
                              std::span<const Framework::RowText> texts,
                              std::span<const std::int32_t> row_types) = 0;
  [[nodiscard]] virtual int SelectedRow() = 0;
  [[nodiscard]] virtual int HighlightedRow() = 0;
  [[nodiscard]] virtual int DrawCursor(int step, std::uint32_t position) = 0;
  [[nodiscard]] virtual int DrawMainMenuCursor(int step, int position) = 0;
  // "OK" in the compact font, in front of the scene's ordering table.
  virtual void DrawConfirmationText(std::uint32_t position,
                                    std::uint32_t color) = 0;
  // The command label: the menu's.
  virtual void SetMenuCommandLabel() = 0;
  virtual void SetBackgroundPriority(int ot_offset, int brightness) = 0;
  virtual void SetCursorColor(int color) = 0;
  [[nodiscard]] virtual int ItemsListRow() = 0;
  // vs_battle_rowTypeBuf and vs_battle_stringBuf.
  [[nodiscard]] virtual std::span<std::int32_t> RowTypes() = 0;
  [[nodiscard]] virtual std::span<std::uint8_t> StringBuffer() = 0;
  // Input_settings.cursorMemory.
  [[nodiscard]] virtual int CursorMemory() = 0;
  virtual void SetCursorMemory(int value) = 0;

  // The information box.
  virtual void InitInformationTextBox(int header) = 0;
  virtual void InitializeInformationBox() = 0;
  virtual void DismissInformationBox() = 0;
  // Menu_textBoxes[7].state = 0.
  virtual void CloseInformationTextBox() = 0;
  virtual void SetInformationMessage(std::span<const std::uint8_t> text) = 0;
  virtual void UpdateInformationVisibility(int row, int state) = 0;

  // Leaves for the menu framework: currentMenu takes returnState, which
  // becomes `return_state`, and the action restriction 0x200 lifts.
  virtual void ReturnFromSetup(int return_state) = 0;
  // currentMenu = `menu`.
  virtual void OpenMenu(int menu) = 0;

  // The equipment details (EquipmentDetails.hpp).
  virtual void ResetEquipmentStats() = 0;
  virtual void SetWeaponStats(int weapon) = 0;
  virtual void SetBladeStats(int blade) = 0;
  virtual void SetGripStats(int grip) = 0;
  virtual void SetShieldStats(int shield) = 0;
  virtual void SetArmorStats(int armor) = 0;
  virtual void SetGemStats(int gem) = 0;
  virtual void SetEnabledStatPages(int pages) = 0;
  virtual void UpdateEquipmentStatPages(int command) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
  virtual void SetRangeAndRisk(int range, int risk, int unused, int shown) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
  virtual void SetPrimaryStats(int strength, int intelligence, int agility,
                               int shown) = 0;
  virtual void UpdatePrimaryStatPanel(int command) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
  virtual void SetDpPp(int current_dp, int max_dp, int current_pp,
                       int max_pp) = 0;
  virtual void UpdateDpPpBars(int flags) = 0;
  // Menu_Framework_equipmentSubtype, equipmentStats[] and strIntAgi[].
  virtual void SetEquipmentSubtype(int subtype) = 0;
  virtual void SetEquipmentStat(std::size_t index, int value) = 0;
  virtual void SetPrimaryStatValue(std::size_t index, int value) = 0;

  // MENU12.BIN (Framework::kMenu12Bytes); empty before the item text load.
  [[nodiscard]] virtual std::span<const std::uint8_t> Menu12() = 0;
  // The item rows (texts, tables, the string context).
  [[nodiscard]] virtual Framework::ItemRowsHost& ItemRows() = 0;
  // Battle_stringContext.integers[slot].
  virtual void SetTextInteger(std::size_t slot, int value) = 0;

  // Ashley's inventory; changes reach the game before the next call that
  // reads it.
  [[nodiscard]] virtual Items::InventoryContents& Inventory() = 0;
  [[nodiscard]] virtual Items::InventoryIndices& Indices() = 0;
  virtual void UnequipWeapon() = 0;
  virtual void UnequipShield() = 0;
  // Equips the 0-based slot's record on Ashley.
  virtual void EquipWeapon(std::size_t slot) = 0;
  virtual void EquipShield(std::size_t slot) = 0;
  // A body part's armor: the 0-based slot, or nothing to take it off.
  virtual void EquipArmor(int body_part, std::optional<std::size_t> slot) = 0;
  [[nodiscard]] virtual MaterialBonuses Materials() = 0;
  // VS::Field::IsCurrentRoomWorkshop: 0 outside a workshop.
  [[nodiscard]] virtual int CurrentWorkshop() = 0;

  // _loadSyd's disc read into a high-end block.
  virtual void StartRecipeLoad(RecipeKind kind) = 0;
  // True once read; nothing when no read is pending.
  [[nodiscard]] virtual std::optional<bool> RecipeLoaded() = 0;
  // Releases the finished read; the file decodes into the recipe tables.
  virtual void ReleaseRecipeRequest() = 0;
  // The decoded recipe tables; null before a file decoded or after it was
  // freed.
  [[nodiscard]] virtual const RecipeBook* Recipes() = 0;
  virtual void FreeRecipes() = 0;

  // Ashley's weapon model (_loadObject): nothing without an actor.
  [[nodiscard]] virtual std::optional<int> AshleySequenceModel() = 0;
  virtual void ReleaseWeaponModel(int slot) = 0;
  virtual void QueueWeaponAnimations(int sequence_model) = 0;
  [[nodiscard]] virtual bool ActorAssetsPending() = 0;
  virtual void ResetEquipmentPose() = 0;
  // Menu_Inventory_equipmentModelLoadStep.
  [[nodiscard]] virtual int EquipmentModelLoadStep() = 0;
  virtual void SetEquipmentModelLoadStep(int step) = 0;
};

struct Context {
  State& state;
  Host& host;
};

// MENU12.BIN's text at a halfword offset, or at an offset-table entry; empty
// (refused) outside the block.
[[nodiscard]] std::span<const std::uint8_t> Menu12Text(Context& context,
                                                       std::size_t offset);
// The offset-table entry `index` (an offset past the block when refused).
[[nodiscard]] std::size_t Menu12EntryOffset(Context& context, int index);
[[nodiscard]] std::span<const std::uint8_t> Menu12Entry(Context& context,
                                                        int index);
// ITEMNAME.BIN from row `index` (24 bytes a row); empty (refused) outside
// the table.
[[nodiscard]] std::span<const std::uint8_t> ItemName(Context& context,
                                                     int index);
// Whether 1-based `reference` names a slot of a `count`-slot table; refuses
// (naming `table`) when it does not.
[[nodiscard]] bool CheckReference(Context& context, int reference,
                                  std::size_t count, std::string_view table);
// The 1-based `reference`'s record, or null (refused) outside the table.
template <typename Record, std::size_t kCount>
[[nodiscard]] Record* InventorySlot(Context& context,
                                    std::array<Record, kCount>& table,
                                    int reference, std::string_view name) {
  if (!CheckReference(context, reference, kCount, name)) {
    return nullptr;
  }
  return &table[static_cast<std::size_t>(reference - 1)];
}
// A formatted row's text: the empty text, ITEMHELP or ITEMNAME bytes, the
// formatter's buffer or the weapon's name; empty (refused) outside them.
[[nodiscard]] std::span<const std::uint8_t> RowTextBytes(
    Context& context, std::optional<Framework::TextRef> text,
    std::span<const std::uint8_t> buffer,
    std::span<const std::uint8_t> weapon_name = {});
// Lays a formatted row's texts over `row`; an absent text leaves its part as
// it was.
void ApplyRowText(Context& context, Framework::RowText& row,
                  const Framework::ItemRowText& text,
                  std::span<const std::uint8_t> buffer);

// The selection screens' shared steps: the title, the list, picking a row,
// settling it into the header and the parts appearing.
enum ScreenStep : std::uint8_t {
  kScreenTitle,
  kScreenList,
  kScreenPick,
  kScreenSettle,
  kScreenParts,
};
// The category menus' steps: their rows, the choice, the screen it opened
// and the return to the rows.
enum CategoryStep : std::uint8_t {
  kCategoryRows,
  kCategoryChoose,
  kCategoryRun,
  kCategoryReturn,
};

// The weapon or shield list the selection screens build (_disassembleMenu,
// _renameWeaponMenu, _attachGemsMenu): one row per listed record, its texts
// held here until CreateMenuRows copies them.
inline constexpr std::size_t kEquipmentListCapacity = 8;
inline constexpr std::size_t kItemNameRowBytes = 24;
struct EquipmentRows {
  std::array<Framework::RowText, kEquipmentListCapacity> texts{};
  std::array<std::int32_t, kEquipmentListCapacity> types{};
  std::array<std::array<std::uint8_t, kItemNameRowBytes>,
             kEquipmentListCapacity>
      names{};
  std::array<std::array<std::uint8_t, kRowTextBytes>, kEquipmentListCapacity>
      descriptions{};
  std::size_t count = 0;
};
// Formats 0-based weapon or shield slot `slot` into row `rows.count` with
// its description buffer `buffer` (the shared string buffer, or the row's).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void FormatEquipmentRow(Context& context, EquipmentRows& rows, bool is_shield,
                        std::size_t slot, std::span<std::uint8_t> buffer);
// The part lists (_assembleBladeMenu, _assembleGripMenu, _attachGem,
// _selectBlade, _selectShields, _initUiArmor): row 0 shows the current part
// or its placeholder, then one row per listed part, each formatted into its
// own description buffer.
struct PartListRows {
  std::array<Framework::RowText, kItemListCapacity> texts{};
  std::array<std::int32_t, kItemListCapacity> types{};
  std::array<std::array<std::uint8_t, kRowTextBytes>, kItemListCapacity>
      buffers{};
  std::size_t count = 0;
};
// The buffer the next row formats into; empty once the list is full.
[[nodiscard]] std::span<std::uint8_t> NextPartBuffer(PartListRows& rows);
// Adds the formatted row of the 1-based part `id` (0 for an empty row 0);
// false (refused) once the list is full.
bool AddPartRow(Context& context, PartListRows& rows,
                const Framework::ItemRowText& text, int id);
// Adds row 0's placeholder: two MENU12 texts, unselectable.
void AddPlaceholderRow(Context& context, PartListRows& rows,
                       std::size_t name_offset, std::size_t description_offset);
// Marks the last row unselectable with the MENU12 text saying why.
void MarkLastPartInvalid(Context& context, PartListRows& rows,
                         std::size_t description_offset);
// Opens the item list over the rows; their types also fill the shared row
// type buffer the information box reads.
void ShowPartList(Context& context, const PartListRows& rows);
// The available part under the list's selection; 0 (refused) outside it.
[[nodiscard]] int ListedPart(Context& context);
// The part lists' step 1 after the stats: runs the list; a chosen row
// becomes its part (or stays negative when the list closed) and `step`
// moves on to 2. Returns -2 when the list was left for the field.
int ChoosePart(Context& context, int& selected_row, std::uint8_t& step);

// The category menus' rows (_disassembleTopMenu, _attachGemsTopMenu,
// _combineTopMenu): the cursor starts on the first row unless the menu was
// just entered.
void CreateCategoryRows(Context& context, CategoryState& menu, int row_info,
                        std::span<const Framework::RowText> texts,
                        std::span<const std::int32_t> row_types);
// Their return from a screen: once the rows are idle, the titles come back.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void ReturnToCategory(Context& context, CategoryState& menu, int subtitle_row,
                      std::size_t subtitle_offset);

// The selection screens' step 2: the chosen row (0 while choosing, < 0 when
// left); sliding the title and the chosen row up.
int PickEquipmentRow(Context& context, EquipmentScreenState& screen);
// Their step 3: the header and the chosen row move into place and the
// chosen 1-based record becomes the combining item; 0 when refused.
int SettleEquipmentRow(Context& context, const EquipmentScreenState& screen,
                       int icon);

void DrawConfirmationLabel(Context& context, int row, int brightness);
void SetTitleRow(Context& context, int row_id, std::size_t text_offset);
void InitializeEquipmentHeader(Context& context, std::size_t text_offset,
                               int icon);
void MoveMenuRow(Context& context, int source, int target);
int UpdateConfirmationPrompt(Context& context, bool initialize);
int UpdateThreeChoicePrompt(Context& context, int cancel_row);
int LeaveItemSelection(Context& context, bool hide_equipment_panels);
void BuildItemList(Context& context, std::span<const Framework::RowText> texts,
                   std::span<const std::int32_t> row_types);
[[nodiscard]] int GetSelectedItemRow(const State& state);
void UpdateItemList(Context& context, int flags);
void AnimateItemHeader(Context& context, int row);
int UpdateDisassembly(Context& context, int category);
int UpdateDisassemblyCategory(Context& context, bool initialize);
int UpdateWeaponRename(Context& context, bool initialize);
int UpdateRepair(Context& context, bool initialize);
int Update(Context& context, std::uint8_t& step);
int ReloadAshleyEquipment(Context& context, int mode);

// MENUC.PRG reloads restore the statics' initial values: the cursor and
// blink animations and the staged gems.
void ResetModuleStatics(State& state);
}  // namespace VS::Menu::Setup
