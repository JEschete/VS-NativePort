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
#include "Menu/Sounds.hpp"
#include "Render/UiPackets.hpp"

// MENU7.PRG's file screens (260.c): the save and load lists, their prompts,
// the frame and the element boxes, the game-over screen, the container
// confirmation and copy, the data menu, and the TITLE.PRG copies (22C.c) of
// the save search, the display set-up and the clear-save reminder. Every
// function works on a Context: the screens' State and a Host (FilesLive.hpp
// binds it to the memory-card driver, the menu framework, the save summaries,
// the pad and the GPU).
namespace VS::Menu::Files {
// _fileMenuElements: the cards (1, 2), the prompts (3, 4) and the slots
// (5..9).
inline constexpr std::size_t kElementCount = 10;
inline constexpr std::size_t kElementTextBytes = 32;
inline constexpr std::size_t kSaveSlots = 5;
inline constexpr int kFirstSlotElement = 5;
// _drawPlayTime's "00:00:00:00".
inline constexpr std::size_t kPlayTimeBytes = 12;
// _printCharacter's widths (MENU7.PRG 0x8010A338 onwards).
inline constexpr std::size_t kGlyphCount = 377;
// State_Settings (Input_settings).
inline constexpr std::size_t kSettingsBytes = 0x20;
inline constexpr std::size_t kClutColors = 16;
// _gameOverClutBuf: one 256-colour CLUT row.
inline constexpr std::size_t kClutRowColors = 256;
// GAMEOVER.BIN: a 24x128 16-bit image.
inline constexpr std::size_t kGameOverImageHalfwords = std::size_t{24} * 128;
// _containerStrings (MENU7.PRG container string table) in halfwords.
inline constexpr std::size_t kContainerStringHalfwords = 895;

// Text_Menu_MCMAN_BIN offsets (halfwords into the table) and indices.
inline constexpr std::uint16_t kTextChecking = 196;
inline constexpr std::uint16_t kTextInsertError = 227;
inline constexpr std::uint16_t kTextSaveFailed = 258;
inline constexpr std::uint16_t kTextLoadFailed = 286;
inline constexpr std::uint16_t kTextSelectFile = 315;
inline constexpr std::uint16_t kTextFileCorrupt = 341;
inline constexpr std::uint16_t kTextSaving = 379;
inline constexpr std::uint16_t kTextLoading = 403;
inline constexpr std::uint16_t kTextSaved = 428;
inline constexpr std::uint16_t kTextLoaded = 434;
inline constexpr std::uint16_t kTextInUse = 441;
inline constexpr std::uint16_t kTextEmpty = 453;
inline constexpr std::uint16_t kTextNew = 464;
inline constexpr std::uint16_t kTextRemoved = 478;
inline constexpr std::uint16_t kTextFormatPrompt = 596;
inline constexpr std::uint16_t kTextFormatting = 628;
inline constexpr std::uint16_t kTextFormatFailed = 672;
inline constexpr std::uint16_t kTextFormatCancelled = 686;
inline constexpr std::uint16_t kTextOverwritePrompt = 728;
inline constexpr std::uint16_t kTextNoData = 738;
inline constexpr std::uint16_t kTextCardFull = 753;
inline constexpr std::uint16_t kTextSaveDisabled = 836;
inline constexpr std::size_t kIndexAccessing0 = 22;
inline constexpr std::size_t kIndexYesOption = 26;
inline constexpr std::size_t kIndexYesIndent = 36;
inline constexpr std::size_t kIndexSaveLocations = 41;
// Text_Menu7_container offsets.
inline constexpr std::uint16_t kContainerTextYes = 16;
inline constexpr std::uint16_t kContainerTextNo = 18;

// fileMenuElementState_e (260.c:110).
enum class ElementState : std::uint8_t {
  kInactive,
  kStatic,
  kAnimateX,
  kAnimateY,
  kAnimateNegX,
};

// vs_fileMenuUiIds_e (260.c:1206): the save summary's labels.
enum class Label : std::uint8_t {
  kColon,
  kNumber,
  kMap,
  kSave,
  kClear,
  kTime,
  kPercent,
  kHp,
  kMp,
  kDot,
};
inline constexpr std::size_t kLabelCount = 10;

// statType_e (260.c:1265).
enum class Stat : std::uint8_t { kHp, kMp };

// slotState_e (260.c:21). GCC compares the enum as unsigned.
inline constexpr std::uint32_t kSlotUnavailable = 0;
inline constexpr std::uint32_t kSlotAvailable = 1;
inline constexpr std::uint32_t kSlotTemporary = 2;
inline constexpr std::uint32_t kSlotInUse = 3;

// fileMenuElements_t (260.c:134) in the record's layout (0x34 bytes);
// FilesLive.cpp pins it against Menu_Files_FileMenuElement.
struct Element {
  std::uint8_t state = 0;
  std::uint8_t slot_id = 0;
  std::uint8_t outer_blend = 0;
  std::uint8_t inner_blend = 0;
  std::uint8_t selected = 0;
  std::uint8_t slot_unavailable = 0;
  std::uint8_t save_location = 0;
  std::uint8_t reserved7 = 0;
  std::int16_t target = 0;
  std::int16_t reserved_a = 0;
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t w = 0;
  std::int16_t h = 0;
  std::array<std::uint8_t, kElementTextBytes> text{};
  friend bool operator==(const Element&, const Element&) = default;
};

// What the slot boxes read of Title_saveFileSummaries[slot].
struct SlotSummary {
  std::uint32_t status = kSlotUnavailable;
  std::int8_t hours = 0;
  std::int8_t minutes = 0;
  std::int8_t seconds = 0;
  std::uint16_t save_count = 0;
  std::uint16_t current_hp = 0;
  std::uint16_t max_hp = 0;
  std::uint16_t current_mp = 0;
  std::uint16_t max_mp = 0;
  std::uint8_t save_location = 0;
  std::uint8_t clear_count = 0;
  std::uint8_t map_completion = 0;
};

// Title_transferProgress* and Title_saveTransferErrorOffset.
struct TransferProgress {
  std::uint16_t position = 0;
  std::uint16_t previous = 0;
  std::uint16_t target = 0;
  std::uint8_t error_offset = 0;
};

// What the data and game-over screens read of vs_main_stateFlags.
struct GameFlags {
  std::uint8_t opaque_a7 = 0;
  std::uint8_t intro_state = 0;
  std::uint8_t clear_count = 0;
  std::uint8_t difficulty = 0;
};

// vs_menu_containerData and vs_menu_containerIndices (BATTLE.PRG
// func_8006B57C_t.h:117, 127) in the records' layout.
inline constexpr std::size_t kContainerWeapons = 32;
inline constexpr std::size_t kContainerShields = 32;
inline constexpr std::size_t kContainerBlades = 64;
inline constexpr std::size_t kContainerGrips = 64;
inline constexpr std::size_t kContainerArmor = 64;
inline constexpr std::size_t kContainerGems = 192;
inline constexpr std::size_t kContainerMisc = 256;
inline constexpr std::size_t kContainerSections = 7;
struct ContainerData {
  std::array<Items::InventoryWeapon, kContainerWeapons> weapons{};
  std::array<Items::InventoryShield, kContainerShields> shields{};
  std::array<Items::InventoryBlade, kContainerBlades> blades{};
  std::array<Items::InventoryGrip, kContainerGrips> grips{};
  std::array<Items::InventoryArmor, kContainerArmor> armor{};
  std::array<Items::InventoryGem, kContainerGems> gems{};
  std::array<Items::InventoryMisc, kContainerMisc> misc{};
};
struct ContainerIndices {
  std::array<std::uint16_t, kContainerWeapons> weapons{};
  std::array<std::uint16_t, kContainerBlades> blades{};
  std::array<std::uint16_t, kContainerGrips> grips{};
  std::array<std::uint16_t, kContainerShields> shields{};
  std::array<std::uint16_t, kContainerArmor> armor{};
  std::array<std::uint16_t, kContainerGems> gems{};
  std::array<std::uint16_t, kContainerMisc> misc{};
};

// _findCurrentSave's statics (both overlays).
struct SaveSearch {
  std::uint8_t step = 0;
  std::uint8_t timeout_step = 0;
};

// The statics of one save or load list (_showSaveFilesMenu,
// _showLoadFilesMenu).
struct FileList {
  std::uint8_t step = 0;
  std::uint8_t port = 0;
  std::uint8_t page = 0;
  std::uint8_t slot = 0;
  std::uint8_t leave_timer = 0;
  std::uint8_t complete_timer = 0;
  std::int32_t result = 0;
};

// The statics of one card picker (_selectSaveMemoryCard,
// _selectLoadMemoryCard).
struct CardPicker {
  std::uint8_t step = 0;
  std::uint8_t port = 0;
};

// _displayGameOverScreen's statics.
struct GameOverChoices {
  std::array<std::array<std::uint16_t, kClutColors>, 2> clut{};
  std::int32_t color1 = 0;
  std::int32_t color2 = 0;
  std::int32_t color3 = 0;
  std::int32_t blend1 = 0;
  std::int32_t blend2 = 0;
  std::int32_t option = 0;
};

struct State {
  std::array<Element, kElementCount> elements{};
  // _isSaving, _containerDataEmpty, _backupMainSetting and
  // _fileMenuScreenFade: MENU7.PRG never sets the first three and only
  // clears the fade, so they stay 0; the paths that read them are kept.
  bool is_saving = false;
  bool container_data_empty = false;
  std::uint8_t backup_main_setting = 0;
  std::uint8_t screen_fade = 0;
  // _selectCursorColor.
  std::uint8_t cursor_color = 0;
  // _printString's arrow animation.
  std::uint8_t text_arrow = 0;
  SaveSearch data_save_search{};
  // maskedHandler's event mask.
  std::uint8_t card_mask = 0;
  // _promptConfirm's statics.
  std::uint8_t confirmation_step = 0;
  std::uint8_t confirmation_cancelled = 0;
  // _promptOverwrite's statics.
  std::int32_t overwrite_result = 0;
  std::uint8_t overwrite_step = 0;
  bool overwrite_initialized = false;
  // _promptFormat's statics.
  std::uint8_t format_step = 0;
  std::uint8_t format_port = 0;
  FileList save_files{};
  std::array<std::uint8_t, kSettingsBytes> save_settings_backup{};
  CardPicker save_card{};
  FileList load_files{};
  std::uint32_t load_time_backup = 0;
  CardPicker load_card{};
  // _initGameOver's step.
  std::int32_t game_over_load_step = 0;
  GameOverChoices game_over{};
  // vs_menu7_gameOver's statics.
  std::array<std::uint16_t, kClutRowColors> game_over_clut{};
  std::array<std::uint8_t, kSettingsBytes> game_over_settings{};
  std::int32_t game_over_delay = 0;
  std::int32_t game_over_difficulty = 0;
  // _promptYesNo's statics.
  std::int32_t container_option = 0;
  std::uint8_t container_step = 0;
  std::uint8_t container_cancel_with_menu = 0;
  std::uint8_t container_cursor = 0;
  // _drawPlayTime's text.
  std::array<char, kPlayTimeBytes> play_time{'0', '0', ':', '0', '0', ':',
                                             '0', '0', ':', '0', '0', '\0'};
  // TITLE.PRG's _findCurrentSave and _saveScreenConfirmation statics.
  SaveSearch title_save_search{};
  std::uint8_t reminder_step = 0;
  std::uint8_t reminder_confirm = 0;
};

// What both overlays' save search uses.
class CardHost {
 public:
  virtual ~CardHost() = default;
  virtual void Refuse(std::string_view what) = 0;
  // MCMAN.BIN's table: halfword offsets, then the strings. Empty before the
  // menu resources load.
  [[nodiscard]] virtual std::span<const std::uint16_t> TextTable() = 0;
  // _memoryCardMessage (Title_memoryCardMessage on the title screen): the
  // table's text at `offset`, or none.
  virtual void SetCardMessage(std::optional<std::uint16_t> offset) = 0;
  // _memcardEventHandler: starts the port's check, or polls with 0.
  virtual int AdvanceCardReadiness(int port) = 0;
  virtual int ScanSaveDirectory(int port) = 0;
  [[nodiscard]] virtual int FindMatchingSaveSlot() = 0;
  [[nodiscard]] virtual Framework::Buttons Input() = 0;
};

class Host : public Render::ImmediatePacketTarget, public CardHost {
 public:
  // The frame's overlay ordering table.
  [[nodiscard]] virtual Render::PacketQueue& OverlayQueue() = 0;
  // _renderImage, _readImage and StoreImage: the pixels cover the packed
  // w | h << 16 size.
  virtual void UploadImage(std::int32_t position,
                           std::span<const std::uint16_t> pixels,
                           std::int32_t size) = 0;
  virtual void ReadImage(std::int32_t position, std::span<std::uint16_t> pixels,
                         std::int32_t size) = 0;
  virtual void StoreImage(std::int32_t position,
                          std::span<std::uint16_t> pixels,
                          std::int32_t size) = 0;
  virtual void DrawSync() = 0;
  // The native file overlay over the frame (Save_NativeDrawFileOverlay).
  virtual void DrawFileOverlay() = 0;
  // In front of the frame's scene ordering table.
  virtual void DrawCompactText(std::string_view text,
                               std::uint32_t position) = 0;
  [[nodiscard]] virtual std::uint32_t InterfaceRandomBits() = 0;
  // Save_MemoryCard_uiResources->locationPalettes[index] (256 colours).
  [[nodiscard]] virtual std::span<const std::uint16_t> LocationPalette(
      int index) = 0;
  virtual void SetDisplayMask(bool shown) = 0;
  virtual void LoadClutColors(std::span<const std::uint16_t> colors, int row,
                              int first) = 0;
  virtual void RetireTexturePages(std::uint32_t first, std::uint32_t count,
                                  int owner) = 0;
  // Render_frameBuf.
  [[nodiscard]] virtual int FrameBuffer() = 0;
  // ClearImage of both 320x240 buffers to black.
  virtual void ClearScreen() = 0;

  // _containerStrings.
  [[nodiscard]] virtual std::span<const std::uint16_t> ContainerStrings() = 0;
  [[nodiscard]] virtual bool CardMessageIs(std::uint16_t offset) = 0;
  // _memoryCardMessage's text, or empty for none.
  [[nodiscard]] virtual std::span<const std::uint8_t> CardMessage() = 0;

  // _selectCursorXy, _dataNotSaved and _fileProgressCounter.
  [[nodiscard]] virtual std::int32_t CursorPosition() = 0;
  virtual void SetCursorPosition(std::int32_t position) = 0;
  [[nodiscard]] virtual std::uint8_t DataNotSaved() = 0;
  virtual void SetDataNotSaved(std::uint8_t not_saved) = 0;
  [[nodiscard]] virtual std::int32_t ProgressCounter() = 0;
  virtual void SetProgressCounter(std::int32_t counter) = 0;
  [[nodiscard]] virtual TransferProgress Transfer() = 0;
  virtual void SetTransferTarget(std::uint16_t target) = 0;

  // Title_saveFileSummaries.
  [[nodiscard]] virtual SlotSummary Summary(int slot) = 0;
  virtual void ZeroSummary(int slot) = 0;
  virtual void SetSummaryStatus(int slot, std::uint32_t status) = 0;
  // The slot's summary copied from the save workspace's fourth 0x80-byte
  // record (_spmcimg + 0x180) and decoded.
  virtual void LoadWrittenSummary(int slot) = 0;
  // Title_cardDirectoryEntries.
  virtual void ClearCardDirectory() = 0;
  [[nodiscard]] virtual int NewestSaveSlot() = 0;
  virtual int CreateSaveFile(int port, int file) = 0;
  // _card_format.
  virtual int FormatCard(int channel) = 0;
  virtual int WriteSaveStep(int request) = 0;
  virtual int ReadSaveStep(int request) = 0;
  virtual int ValidateAndApplyLoadedSave(int apply) = 0;
  virtual void PackageGameSave(int slot) = 0;
  // The outgoing save image kept in the next image of the workspace, and
  // put back.
  virtual void StashSaveImage() = 0;
  virtual void RestoreSaveImage() = 0;
  // Input_settings as bytes.
  [[nodiscard]] virtual std::span<std::uint8_t> Settings() = 0;
  virtual void ClearDoorEntered() = 0;
  [[nodiscard]] virtual std::uint32_t PlayTime() = 0;
  virtual void SetPlayTime(std::uint32_t time) = 0;

  virtual void PlaySound(MenuSound sound) = 0;
  // An effect of the menu sound group (vs_main_playSfxDefault(0x7E, ...)).
  virtual void PlaySoundEffect(int effect) = 0;

  virtual Framework::MenuRow& SetMenuItem(
      const Framework::RowPlacement& placement,
      std::span<const std::uint8_t> text) = 0;
  [[nodiscard]] virtual Framework::MenuRow& MenuItem(int row) = 0;
  [[nodiscard]] virtual bool RowsIdle() = 0;
  virtual void ClearMenuExcept(int row) = 0;
  [[nodiscard]] virtual int DrawMainMenuCursor(int step, int position) = 0;
  // vs_mainmenu_setMenuRows with the rows' name and description text.
  virtual void CreateMenuRows(
      std::span<const std::span<const std::uint8_t>> strings,
      std::span<int> row_types, int row_info) = 0;
  [[nodiscard]] virtual int SelectedRow() = 0;
  [[nodiscard]] virtual int HighlightedRow() = 0;
  virtual void ClearCommandLabel() = 0;
  virtual void DismissInformationBox() = 0;
  virtual void SetBackgroundFade(int fade) = 0;
  [[nodiscard]] virtual int BackgroundFadeStep() = 0;
  virtual void InitInformationBox(Framework::InformationHeader header) = 0;
  virtual void SetInformationBoxState(int state) = 0;
  // vs_mainmenu_setMessage with the text table's text at `offset`.
  virtual void SetInformationMessage(std::uint16_t offset) = 0;
  // Menu_saveLocationId and the room's save point.
  [[nodiscard]] virtual int CurrentSavePointId() = 0;
  [[nodiscard]] virtual int SaveLocationId() = 0;
  virtual void SetSaveLocationId(int id) = 0;
  [[nodiscard]] virtual GameFlags Flags() = 0;
  virtual void SetGameOver(bool game_over) = 0;
  virtual void SetDifficulty(std::uint8_t difficulty) = 0;
  [[nodiscard]] virtual std::uint8_t CursorMemory() = 0;
  virtual void SetCursorMemory(std::uint8_t memory) = 0;
  [[nodiscard]] virtual std::uint8_t MonoSound() = 0;

  virtual int UpdateDataSaveMenu(int init) = 0;
  virtual int UpdateDataLoadMenu(int init) = 0;
  virtual int StepMenuResources(int init) = 0;
  virtual void ReleaseMenuResources() = 0;
  virtual void EnterBattle() = 0;
  virtual void ResetToTitle() = 0;
  virtual void SetFieldRendering(bool enabled) = 0;
  virtual void FreeMusicSlot(int slot) = 0;
  virtual void FadeMusicSlotVolume(int slot, int volume, int frames) = 0;
  virtual void SetMonoOutput(int mono) = 0;
  virtual int LoadIntroSnapshotStep(int action) = 0;
  virtual void SetCurrentMenu(int menu) = 0;

  // GAMEOVER.BIN: the read is queued; whether it has landed (none when no
  // request was made); the image once it has (the request is released);
  // the buffer freed.
  virtual void BeginGameOverLoad() = 0;
  [[nodiscard]] virtual std::optional<bool> GameOverLoaded() = 0;
  [[nodiscard]] virtual std::span<const std::uint16_t> FinishGameOverLoad() = 0;
  virtual void FreeGameOverImage() = 0;
};

// The title screen's copies (TITLE.PRG 22C.c).
class TitleHost : public Render::ImmediatePacketTarget, public CardHost {
 public:
  // Menu_Files_frameBuf (TITLE.PRG _frameBuf).
  [[nodiscard]] virtual std::uint8_t FrameBuffer() = 0;
  virtual void SetFrameBuffer(std::uint8_t buffer) = 0;
  // SetDefDispEnv at (display_x, 0) showing lines 8..231 and SetDefDrawEnv
  // at (draw_x, 0), both 320x240, put.
  virtual void PutDisplay(int display_x, int draw_x) = 0;
  virtual void ClearScreen() = 0;
  virtual void SetDisplayMask(bool shown) = 0;
  // Title_fileMenuElements.
  virtual void InitializeElement(int id, std::uint32_t position,
                                 std::uint32_t size,
                                 std::uint16_t text_offset) = 0;
  virtual void AnimateElement(int id, ElementState state,
                              std::int16_t target) = 0;
  virtual void SetElementSelected(int id, bool selected) = 0;
  [[nodiscard]] virtual bool AnimationsComplete() = 0;
  // Title_selectionCursorPosition.
  virtual void SetCursorPosition(std::int32_t position) = 0;
  virtual void PlaySound(MenuSound sound) = 0;
  virtual void PlaySoundEffect(int effect) = 0;
};

struct Context {
  State& state;
  Host& host;
};

struct TitleContext {
  State& state;
  TitleHost& host;
};

// MENU7.PRG _drawSaveInfoUI (260.c:1222).
void DrawSummaryLabel(Host& host, std::int32_t position, Label label);
// MENU7.PRG _renderInteger (260.c:1239).
void DrawInteger(Render::ImmediatePacketTarget& target, std::int32_t position,
                 std::uint32_t value, std::uint32_t divisor);
// MENU7.PRG _countDigits (260.c:1253) and its TITLE.PRG copy.
[[nodiscard]] int DecimalDigitCount(int value) noexcept;
// MENU7.PRG _drawHPMP (260.c:1270).
void DrawHpMpSummary(Host& host, std::int32_t position, Stat stat,
                     std::uint32_t current, std::uint32_t max);
// MENU7.PRG _fileProcessingAnim (260.c:1353).
void DrawTransferProgress(Context& context, std::int32_t x, std::int32_t y);
// MENU7.PRG _fileProcessingCompleteAnim (260.c:1400).
void DrawTransferComplete(Context& context, std::int32_t colour,
                          std::int32_t y);
// MENU7.PRG _initFileMenu (260.c:1447).
void ResetElements(Context& context);
// MENU7.PRG _initFileMenuElement (260.c:1463): the element, or none for an
// id outside the table.
Element* InitializeElement(Context& context, int id, std::uint32_t position,
                           std::uint32_t size,
                           std::span<const std::uint8_t> text);
// MENU7.PRG _clearFileMenuElement (260.c:1504).
void ClearElement(Context& context, int id);
// MENU7.PRG _fileMenuElementsActive (260.c:1512).
[[nodiscard]] bool AreAnimationsComplete(const State& state);
// MENU7.PRG _printCharacter (260.c:1524): the next x.
int DrawGlyph(Render::ImmediatePacketTarget& target, std::uint32_t glyph, int x,
              int y, int clut);
// MENU7.PRG _findCurrentSave (260.c:1572).
int FindCurrentDataSaveStep(Context& context, bool init);
// MENU7.PRG maskedHandler (260.c:1640): the port and event mask to start,
// or 0 to poll.
int CheckCardStep(Context& context, int port_mask);
// MENU7.PRG _printString (260.c:1675) and its TITLE.PRG copy.
void DrawText(Render::ImmediatePacketTarget& target, std::uint8_t& arrow,
              std::span<const std::uint8_t> text, int x, int y, int clut);
// MENU7.PRG _intepolateMenuItemBgColour (260.c:1738) and its TITLE.PRG
// copy; none for an inner factor past the original's colour tables.
[[nodiscard]] std::optional<std::uint32_t> ElementBackgroundColor(
    std::uint32_t outer, std::uint32_t inner) noexcept;
// MENU7.PRG _drawFileMenuElement (260.c:1765).
void DrawElement(Context& context, Element& element);
// MENU7.PRG _drawFileMenu (260.c:1951).
void DrawFrame(Context& context, int frame_buffer);
// MENU7.PRG _drawFileMenuBg (260.c:2078) and its TITLE.PRG copy.
void DrawBackground(Render::ImmediatePacketTarget& target);
// MENU7.PRG _promptConfirm (260.c:2092).
int UpdateConfirmation(Context& context, bool init);
// MENU7.PRG _promptOverwrite (260.c:2161).
int UpdateOverwritePrompt(Context& context, bool init);
// MENU7.PRG _promptFormat (260.c:2199).
int UpdateFormatPrompt(Context& context, int init_port);
// MENU7.PRG _showSaveFilesMenu (260.c:2277).
int UpdateSaveFiles(Context& context, int init_port);
// MENU7.PRG _selectSaveMemoryCard (260.c:2537).
int UpdateSaveCard(Context& context, int init_port);
// MENU7.PRG _showLoadFilesMenu (260.c:2878).
int UpdateLoadFiles(Context& context, int init_port);
// MENU7.PRG _selectLoadMemoryCard (260.c:3063).
int UpdateLoadCard(Context& context, int init_port);
// MENU7.PRG _initGameOver (260.c:3395).
int LoadGameOverAssetsStep(Context& context, bool begin);
// MENU7.PRG _setMenuItemClut (260.c:3434).
void BlendGameOverTextClut(std::span<std::uint16_t, kClutColors> clut,
                           int factor,
                           std::span<const std::uint16_t, kClutColors> from,
                           std::span<const std::uint16_t, kClutColors> to);
// MENU7.PRG _displayGameOverScreen (260.c:3475).
int UpdateGameOverChoices(Context& context, bool init);
// MENU7.PRG vs_menu7_gameOver (260.c:3603).
int UpdateGameOver(Context& context, std::uint8_t& step);
// MENU7.PRG _promptYesNo (260.c:3727).
int UpdateContainerConfirmation(Context& context, int init);
// MENU7.PRG _getContainerIndexOffset (260.c:3804): the section's first
// halfword in vs_menu_containerIndices.
[[nodiscard]] std::size_t ContainerIndexSection(int section) noexcept;
// MENU7.PRG _copyContainer (260.c:3821).
void CopySelectedContainerItems(ContainerData& target,
                                const ContainerData& source,
                                const ContainerIndices& to_copy);
// MENU7.PRG _drawPlayTime (260.c:4157).
void DrawPlayTime(Context& context);
// MENU7.PRG vs_menu7_dataMenu (260.c:4187).
int UpdateDataMenu(Context& context, std::uint8_t& step);

// TITLE.PRG _findCurrentSave (22C.c:1427).
int FindCurrentSaveStep(TitleContext& context, bool init);
// TITLE.PRG _initSaveScreen (22C.c:3074).
void InitializeDisplay(TitleHost& host);
// TITLE.PRG _saveScreenSwapBuf (22C.c:3093).
void SwapBuffers(TitleHost& host);
// TITLE.PRG _saveScreenConfirmation (22C.c:3226).
int UpdateClearSaveReminder(TitleContext& context, bool init);
// TITLE.PRG _saveScreenExitScreen (22C.c:3299).
void DrawExitFade(TitleHost& host);

// What a fresh load of MENU7.PRG brings back.
void ResetStatics(State& state);
}  // namespace VS::Menu::Files
