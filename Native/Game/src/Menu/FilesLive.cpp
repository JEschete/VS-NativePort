// Hand-maintained (tools/port/owned.csv).
#include "Menu/FilesLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

#include "Audio/MusicLive.hpp"
#include "Audio/SfxLive.hpp"
#include "Audio/SoundBanksLive.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Field/ChestInteractionsLive.h"
#include "Field/RoomRenderingLive.hpp"
#include "Menu/Files.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Sounds.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/UiPackets.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Save/FileMenuLive.hpp"
#include "Save/MemoryCard/GameFilesLive.hpp"
#include "Save/MenuAssetsLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Content/DiscLayout.h"
#include "DiscQueue.h"
#include "Field/Gimmicks.h"
#include "Input/ControllerLiveState.h"
#include "Items/Inventory.h"
#include "Legacy/Psx.h"
#include "Menu/Files.h"
#include "Menu/FilesLiveState.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "Save/MemoryCard/GameFiles.h"
#include "Save/MemoryCard/Resources.h"
#include "Save/NativeMenu.h"
#include "State/PlayTime.h"
#include "State/Settings.h"
#include "State/StateFlags.h"
#include "Title/SaveCard.h"
#include "Title/SaveFiles.h"
}

// decomp: _containerStrings (_containerStrings@src/MENU/MENU7.PRG/260.c)
u_short Menu_Files_containerStrings[kMenuFilesContainerStringCount];
// decomp: _dataNotSaved (_dataNotSaved@src/MENU/MENU7.PRG/260.c)
char Menu_Files_dataNotSaved;
// decomp: _fileProgressCounter (_fileProgressCounter@src/MENU/MENU7.PRG/260.c)
int Menu_Files_fileProgressCounter;
// decomp: _memoryCardMessage (_memoryCardMessage@src/MENU/MENU7.PRG/260.c)
char* Menu_Files_memoryCardMessage;
// decomp: _selectCursorXy (_selectCursorXy@src/MENU/MENU7.PRG/260.c)
int Menu_Files_selectCursorXy;
// decomp: _textTable (_textTable@src/MENU/MENU7.PRG/260.c)
u_short* Menu_Files_textTable;
// decomp: _frameBuf (_frameBuf@src/TITLE/TITLE.PRG/22C.c)
char Menu_Files_frameBuf;

namespace VS::Menu::Files {
namespace {
using Element_ = Menu_Files_FileMenuElement;
static_assert(sizeof(Element) == sizeof(Element_));
static_assert(std::is_trivially_copyable_v<Element>);
static_assert(offsetof(Element, state) == offsetof(Element_, state));
static_assert(offsetof(Element, slot_id) == offsetof(Element_, slotId));
static_assert(offsetof(Element, outer_blend) ==
              offsetof(Element_, outertextBlendFactor));
static_assert(offsetof(Element, inner_blend) ==
              offsetof(Element_, innertextBlendFactor));
static_assert(offsetof(Element, selected) == offsetof(Element_, selected));
static_assert(offsetof(Element, slot_unavailable) ==
              offsetof(Element_, slotUnavailable));
static_assert(offsetof(Element, save_location) ==
              offsetof(Element_, saveLocation));
static_assert(offsetof(Element, target) == offsetof(Element_, targetPosition));
static_assert(offsetof(Element, x) == offsetof(Element_, x));
static_assert(offsetof(Element, y) == offsetof(Element_, y));
static_assert(offsetof(Element, w) == offsetof(Element_, w));
static_assert(offsetof(Element, h) == offsetof(Element_, h));
static_assert(offsetof(Element, text) == offsetof(Element_, text));
static_assert(sizeof(Element::text) == sizeof(Element_::text));

static_assert(sizeof(ContainerData) == sizeof(Items_ContainerData));
static_assert(offsetof(ContainerData, shields) ==
              offsetof(Items_ContainerData, shields));
static_assert(offsetof(ContainerData, blades) ==
              offsetof(Items_ContainerData, blades));
static_assert(offsetof(ContainerData, grips) ==
              offsetof(Items_ContainerData, grips));
static_assert(offsetof(ContainerData, armor) ==
              offsetof(Items_ContainerData, armor));
static_assert(offsetof(ContainerData, gems) ==
              offsetof(Items_ContainerData, gems));
static_assert(offsetof(ContainerData, misc) ==
              offsetof(Items_ContainerData, misc));
static_assert(sizeof(ContainerIndices) == sizeof(Items_ContainerIndices));
static_assert(offsetof(ContainerIndices, blades) ==
              offsetof(Items_ContainerIndices, blades));
static_assert(offsetof(ContainerIndices, grips) ==
              offsetof(Items_ContainerIndices, grips));
static_assert(offsetof(ContainerIndices, shields) ==
              offsetof(Items_ContainerIndices, shields));
static_assert(offsetof(ContainerIndices, armor) ==
              offsetof(Items_ContainerIndices, armor));
static_assert(offsetof(ContainerIndices, gems) ==
              offsetof(Items_ContainerIndices, gems));
static_assert(offsetof(ContainerIndices, misc) ==
              offsetof(Items_ContainerIndices, misc));

static_assert(sizeof(State_Settings) == kSettingsBytes);
static_assert(sizeof(Menu_Files_containerStrings) ==
              kContainerStringHalfwords * sizeof(std::uint16_t));
static_assert(VS_GAMEOVER_BIN_SIZE >=
              kGameOverImageHalfwords * sizeof(std::uint16_t));
static_assert(std::extent_v<decltype(Title_fileMenuElements)> == kElementCount);

// MCMAN.BIN's table (Save_CardUiResources::textTable) in halfwords and
// bytes.
using TextTable = decltype(Save_CardUiResources::textTable);
using Palettes = decltype(Save_CardUiResources::locationPalettes);
constexpr std::size_t kTextTableHalfwords = std::extent_v<TextTable>;
constexpr std::size_t kTextTableBytes = sizeof(TextTable);
static_assert(kTextTableBytes == kTextTableHalfwords * sizeof(std::uint16_t));
constexpr std::size_t kLocationPalettes = std::extent_v<Palettes>;
constexpr std::size_t kLocationPaletteColors = std::extent_v<Palettes, 1>;
// _spmcimg's fourth 0x80-byte record (_rMemcpy(&_saveFileInfo[saveId],
// _spmcimg + sizeof _saveFileInfo[saveId] * 3, ...), 260.c:2471).
constexpr std::size_t kFileInfoRecordBytes = sizeof(Save_CardFileInfoRecord);
constexpr std::size_t kWrittenSummaryOffset = kFileInfoRecordBytes * 3;
// The workspace (_spmcimg) starts with three save images; the outgoing image
// is kept in the third.
constexpr std::size_t kSaveImageBytes = sizeof(Save_GameSaveImage);
constexpr std::size_t kWorkspaceImages = 3;
static_assert(kSaveImageBytes * kWorkspaceImages <= VS_SPMCIMG_BIN_SIZE);
static_assert(kWrittenSummaryOffset + sizeof(Save_CardFileInfoRecord) <=
              kSaveImageBytes);
static_assert(offsetof(Save_CardFileInfoRecord, summary) ==
              sizeof(Save_CardFileInfoRecord::key));
// The packets the screens build fill the original's 16-word primBuf_t.
constexpr std::size_t kPacketBytes = sizeof(Menu_Files_PrimitiveBuffer);
constexpr std::size_t kPacketWordBytes = sizeof(std::uint32_t);
constexpr std::size_t kImmediatePacketWords = kPacketBytes / kPacketWordBytes;
// The compact text links 0x14 bytes before the scene ordering table
// (((u_char*)orderingTable) - 0x14).
constexpr int kCompactTextSlot = -5;
// vs_battle_menuState.currentMenu's row width for the data menu's strings.
constexpr std::size_t kMaxRowStrings = 8;
// The information text box (Menu_textBoxes[7]).
constexpr std::size_t kInformationTextBox = 7;
// The screen both buffers cover, and the title screen's 320x240 buffers
// shown from line 8 for 224 lines.
constexpr std::int16_t kScreenWidth = 640;
constexpr std::int16_t kScreenHeight = 240;
constexpr int kBufferWidth = 320;
constexpr int kBufferHeight = 240;
constexpr std::int16_t kDisplayTop = 8;
constexpr std::int16_t kDisplayHeight = 224;
constexpr int kHalfwordBits = 16;
constexpr std::uint32_t kHalfwordMask = 0xFFFF;
// vs_main_playSfxDefault's interface random draw.
constexpr std::int32_t kInterfaceRandomBits = 15;

State g_state{};
// _initGameOver's queue slot and GAMEOVER.BIN buffer.
Content_DiscQueueSlot* g_game_over_request = nullptr;
void* g_game_over_image = nullptr;

void Warn(std::string_view what) { VS_LOG_WARN("menu.files", "{}", what); }

// The packet drawn at once from a copy (DrawPrim of the original's primBuf).
void SubmitLive(std::span<const std::uint32_t> packet) {
  std::array<u_long, kImmediatePacketWords> words{};
  if (packet.size() > words.size()) {
    Warn(std::format("file menu packet of {} words exceeds the {}-word buffer",
                     packet.size(), words.size()));
    return;
  }
  std::ranges::copy(packet, words.begin());
  DrawPrim(words.data());
}

class LiveImmediateTarget final : public Render::ImmediatePacketTarget {
 public:
  void Submit(std::span<const std::uint32_t> packet) override {
    SubmitLive(packet);
  }
};

[[nodiscard]] std::uint32_t Word(std::int32_t value) {
  return static_cast<std::uint32_t>(value);
}

// The pixels a packed w | h << 16 size covers.
[[nodiscard]] std::size_t PixelCount(std::int32_t size) {
  const std::uint32_t packed = Word(size);
  return static_cast<std::size_t>(packed & kHalfwordMask) *
         static_cast<std::size_t>(packed >> kHalfwordBits);
}

[[nodiscard]] RECT RectAt(std::int32_t position, std::int32_t size) {
  const std::uint32_t xy = Word(position);
  const std::uint32_t wh = Word(size);
  return {.x = static_cast<std::int16_t>(xy & kHalfwordMask),
          .y = static_cast<std::int16_t>(xy >> kHalfwordBits),
          .w = static_cast<std::int16_t>(wh & kHalfwordMask),
          .h = static_cast<std::int16_t>(wh >> kHalfwordBits)};
}

// ClearImage of both buffers to black.
void ClearBothBuffers() {
  RECT rect{.x = 0, .y = 0, .w = kScreenWidth, .h = kScreenHeight};
  ClearImage(&rect, 0, 0, 0);
}

Framework::Buttons LiveButtons() {
  return {.pressed = Input_buttonsPressed.allButtons,
          .repeat = static_cast<std::uint32_t>(Input_buttonRepeat),
          .held = static_cast<std::uint32_t>(Input_buttonsState)};
}

[[nodiscard]] std::span<const std::uint16_t> TableSpan(const u_short* table) {
  if (table == nullptr) {
    return {};
  }
  return {table, kTextTableHalfwords};
}

// The byte offset of `text` inside the `bytes` at `base`, or none.
[[nodiscard]] std::optional<std::size_t> OffsetIn(const void* base,
                                                  std::size_t bytes,
                                                  const void* text) {
  if (base == nullptr || text == nullptr) {
    return std::nullopt;
  }
  const auto start = reinterpret_cast<std::uintptr_t>(base);
  const auto address = reinterpret_cast<std::uintptr_t>(text);
  if (address < start || address - start >= bytes) {
    return std::nullopt;
  }
  return address - start;
}

// The text from `text` to the end of the MCMAN.BIN table (either screen's)
// or of the native file menu's footer it lies in; empty for other text.
[[nodiscard]] std::span<const std::uint8_t> TextTail(const char* text) {
  const std::span<const char> footer = VS::Menu::LiveFileMenuFooter();
  const std::array<std::span<const std::uint8_t>, 3> regions{
      std::span(reinterpret_cast<const std::uint8_t*>(Menu_Files_textTable),
                Menu_Files_textTable != nullptr ? kTextTableBytes : 0),
      std::span(reinterpret_cast<const std::uint8_t*>(Title_saveMenuText),
                Title_saveMenuText != nullptr ? kTextTableBytes : 0),
      std::span(reinterpret_cast<const std::uint8_t*>(footer.data()),
                footer.size())};
  for (const std::span<const std::uint8_t> region : regions) {
    const std::optional<std::size_t> offset =
        OffsetIn(region.data(), region.size(), text);
    if (offset) {
      return region.subspan(*offset);
    }
  }
  return {};
}

// The table's text at `offset` as the message pointer, or null for none or
// an offset the table does not hold.
char* TableMessage(u_short* table, std::optional<std::uint16_t> offset,
                   CardHost& host) {
  if (!offset) {
    return nullptr;
  }
  if (table == nullptr || *offset >= kTextTableHalfwords) {
    host.Refuse(std::format(
        "memory-card message at halfword {} without its loaded table",
        *offset));
    return nullptr;
  }
  return reinterpret_cast<char*>(table + *offset);
}

Save_CardFileInfoRecord* SummaryRecord(int slot) {
  if (Title_saveFileSummaries == nullptr || slot < 0 ||
      slot >= static_cast<int>(kSaveSlots)) {
    return nullptr;
  }
  return &Title_saveFileSummaries[slot];
}

Menu_Files_FileMenuElement* TitleElement(int id) {
  if (id < 0 || id >= static_cast<int>(kElementCount)) {
    return nullptr;
  }
  return &Title_fileMenuElements[id];
}

// The ordering table and GPU half of the data and game-over screens.
class LiveGpuHost : public Host {
 public:
  void Submit(std::span<const std::uint32_t> packet) override {
    SubmitLive(packet);
  }
  void Refuse(std::string_view what) override { Warn(what); }
  Render::PacketQueue& OverlayQueue() override {
    return queue_.emplace(
        Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kOverlay, 0));
  }
  void UploadImage(std::int32_t position, std::span<const std::uint16_t> pixels,
                   std::int32_t size) override {
    if (PixelCount(size) > pixels.size()) {
      Refuse("file menu image upload larger than its pixels");
      return;
    }
    Render::UploadLiveImageRectangle(position, pixels.data(), size);
  }
  void ReadImage(std::int32_t position, std::span<std::uint16_t> pixels,
                 std::int32_t size) override {
    if (PixelCount(size) > pixels.size()) {
      Refuse("file menu image read larger than its buffer");
      return;
    }
    Render::ReadLiveImageRectangle(position, pixels.data(), size);
  }
  void StoreImage(std::int32_t position, std::span<std::uint16_t> pixels,
                  std::int32_t size) override {
    if (PixelCount(size) > pixels.size()) {
      Refuse("file menu image store larger than its buffer");
      return;
    }
    RECT rect = RectAt(position, size);
    ::StoreImage(&rect, reinterpret_cast<u_long*>(pixels.data()));
  }
  void DrawSync() override { ::DrawSync(0); }
  void DrawFileOverlay() override { Save_NativeDrawFileOverlay(0); }
  void DrawCompactText(std::string_view text, std::uint32_t position) override {
    u_long* const chain = Render::LiveFrameOrderingEntry(
        Render::LiveOrderingBase::kScene, kCompactTextSlot);
    if (chain == nullptr) {
      Refuse("play time text outside the frame's ordering table");
      return;
    }
    const std::string terminated(text);
    Render::DrawLiveCompactText(terminated.c_str(), static_cast<int>(position),
                                chain);
  }
  std::uint32_t InterfaceRandomBits() override {
    return Core::NextLiveInterfaceRandomBits(kInterfaceRandomBits);
  }
  std::span<const std::uint16_t> LocationPalette(int index) override {
    if (Save_MemoryCard_uiResources == nullptr || index < 0 ||
        index >= static_cast<int>(kLocationPalettes)) {
      return {};
    }
    return {Save_MemoryCard_uiResources->locationPalettes[index],
            kLocationPaletteColors};
  }
  void SetDisplayMask(bool shown) override { SetDispMask(shown ? 1 : 0); }
  void LoadClutColors(std::span<const std::uint16_t> colors, int row,
                      int first) override {
    Render::LoadLiveClutColors(colors.data(), static_cast<u_int>(row),
                               static_cast<u_int>(first),
                               static_cast<u_int>(colors.size()));
  }
  void RetireTexturePages(std::uint32_t first, std::uint32_t count,
                          int owner) override {
    Render::RetireLiveTexturePageRange(first, count, owner);
  }
  int FrameBuffer() override { return Render_frameBuf; }
  void ClearScreen() override { ClearBothBuffers(); }

 private:
  std::optional<Render::LivePacketQueue> queue_;
};

// The memory card, the save summaries and the MENU7.PRG message globals.
class LiveCardHost : public LiveGpuHost {
 public:
  std::span<const std::uint16_t> TextTable() override {
    return TableSpan(Menu_Files_textTable);
  }
  void SetCardMessage(std::optional<std::uint16_t> offset) override {
    Menu_Files_memoryCardMessage =
        TableMessage(Menu_Files_textTable, offset, *this);
  }
  int AdvanceCardReadiness(int port) override {
    return VS::Save::MemoryCard::AdvanceLiveCardReadiness(port);
  }
  int ScanSaveDirectory(int port) override {
    return VS::Save::MemoryCard::ScanLiveSaveDirectory(port);
  }
  int FindMatchingSaveSlot() override {
    return VS::Save::MemoryCard::FindLiveMatchingSaveSlot();
  }
  Framework::Buttons Input() override { return LiveButtons(); }
  std::span<const std::uint16_t> ContainerStrings() override {
    return Menu_Files_containerStrings;
  }
  bool CardMessageIs(std::uint16_t offset) override {
    return Menu_Files_textTable != nullptr && offset < kTextTableHalfwords &&
           Menu_Files_memoryCardMessage ==
               reinterpret_cast<char*>(Menu_Files_textTable + offset);
  }
  std::span<const std::uint8_t> CardMessage() override {
    if (Menu_Files_memoryCardMessage == nullptr) {
      return {};
    }
    const std::span<const std::uint8_t> text =
        TextTail(Menu_Files_memoryCardMessage);
    if (text.empty()) {
      Refuse("memory-card message outside the MCMAN.BIN table and footer");
    }
    return text;
  }
  std::int32_t CursorPosition() override { return Menu_Files_selectCursorXy; }
  void SetCursorPosition(std::int32_t position) override {
    Menu_Files_selectCursorXy = position;
  }
  std::uint8_t DataNotSaved() override {
    return static_cast<std::uint8_t>(Menu_Files_dataNotSaved);
  }
  void SetDataNotSaved(std::uint8_t not_saved) override {
    Menu_Files_dataNotSaved = static_cast<char>(not_saved);
  }
  std::int32_t ProgressCounter() override {
    return Menu_Files_fileProgressCounter;
  }
  void SetProgressCounter(std::int32_t counter) override {
    Menu_Files_fileProgressCounter = counter;
  }
  TransferProgress Transfer() override {
    return {.position = Title_transferProgressPosition,
            .previous = Title_previousTransferProgress,
            .target = Title_transferProgressTarget,
            .error_offset =
                static_cast<std::uint8_t>(Title_saveTransferErrorOffset)};
  }
  void SetTransferTarget(std::uint16_t target) override {
    Title_transferProgressTarget = target;
  }

  SlotSummary Summary(int slot) override {
    const Save_CardFileInfoRecord* const record = SummaryRecord(slot);
    if (record == nullptr) {
      return {};
    }
    const Save_SaveFileStats& stats = record->summary.stats;
    return {
        .status = static_cast<std::uint32_t>(record->summary.base.slotState),
        .hours = stats.gameTime.t.hours,
        .minutes = stats.gameTime.t.minutes,
        .seconds = stats.gameTime.t.seconds,
        .save_count = stats.cumulativeSaveCount,
        .current_hp = stats.currentHP,
        .max_hp = stats.maxHP,
        .current_mp = stats.currentMP,
        .max_mp = stats.maxMP,
        .save_location = static_cast<std::uint8_t>(stats.saveLocation),
        .clear_count = static_cast<std::uint8_t>(stats.clearCount),
        .map_completion = static_cast<std::uint8_t>(stats.mapCompletion)};
  }
  void ZeroSummary(int slot) override {
    Save_CardFileInfoRecord* const record = SummaryRecord(slot);
    if (record == nullptr) {
      Refuse("cleared save summary outside the loaded summaries");
      return;
    }
    std::memset(record, 0, sizeof *record);
  }
  void SetSummaryStatus(int slot, std::uint32_t status) override {
    Save_CardFileInfoRecord* const record = SummaryRecord(slot);
    if (record == nullptr) {
      Refuse("save summary status outside the loaded summaries");
      return;
    }
    record->summary.base.slotState =
        static_cast<enum Save_CardFileSlotStatus>(status);
  }
  void LoadWrittenSummary(int slot) override {
    Save_CardFileInfoRecord* const record = SummaryRecord(slot);
    if (record == nullptr || Title_memoryCardUiResource == nullptr) {
      Refuse("written save summary without the loaded save workspace");
      return;
    }
    std::memcpy(record, Title_memoryCardUiResource + kWrittenSummaryOffset,
                sizeof *record);
    VS::Save::MemoryCard::DecodeLiveSaveBytes(
        record->key, &record->summary.base.slotState,
        sizeof *record - sizeof record->key);
  }
  void ClearCardDirectory() override {
    std::ranges::fill(Title_cardDirectoryEntries, nullptr);
  }
  int NewestSaveSlot() override {
    return VS::Save::MemoryCard::NewestLiveSaveSlot();
  }
  int CreateSaveFile(int port, int file) override {
    return VS::Save::MemoryCard::CreateLiveSaveFile(port, file);
  }
  int FormatCard(int channel) override { return _card_format(channel); }
  int WriteSaveStep(int request) override {
    return VS::Save::MemoryCard::WriteLiveSaveStep(request);
  }
  int ReadSaveStep(int request) override {
    return VS::Save::MemoryCard::ReadLiveSaveStep(request);
  }
  int ValidateAndApplyLoadedSave(int apply) override {
    return VS::Save::MemoryCard::ValidateAndApplyLiveLoadedSave(apply);
  }
  void PackageGameSave(int slot) override {
    VS::Save::MemoryCard::PackageLiveGameSave(slot);
  }
  void StashSaveImage() override { MoveSaveImage(2, 1); }
  void RestoreSaveImage() override { MoveSaveImage(1, 2); }
  std::span<std::uint8_t> Settings() override {
    return {reinterpret_cast<std::uint8_t*>(&Input_settings),
            sizeof Input_settings};
  }
  void ClearDoorEntered() override {
    Audio_Seq_unusedStateFlags.doorEntered = 0;
  }
  std::uint32_t PlayTime() override { return State_playTime.all; }
  void SetPlayTime(std::uint32_t time) override { State_playTime.all = time; }

 private:
  // One save image of the workspace over another (_rMemcpy of
  // (savedata_t*)_spmcimg + from); the images do not overlap.
  void MoveSaveImage(std::size_t to, std::size_t from) {
    if (Title_memoryCardUiResource == nullptr) {
      Refuse("save image copy without the loaded save workspace");
      return;
    }
    std::memcpy(Title_memoryCardUiResource + (to * kSaveImageBytes),
                Title_memoryCardUiResource + (from * kSaveImageBytes),
                kSaveImageBytes);
  }
};

// The menu framework, the boot transitions and the game-over image.
class LiveHost : public LiveCardHost {
 public:
  void PlaySound(MenuSound sound) override { PlayMenuSound(sound); }
  void PlaySoundEffect(int effect) override {
    Audio::LivePlaySfxCentered(kMenuSoundChannelGroup, effect);
  }
  Framework::MenuRow& SetMenuItem(const Framework::RowPlacement& placement,
                                  std::span<const std::uint8_t> text) override {
    return Framework::SetLiveMenuItemText(placement, text);
  }
  Framework::MenuRow& MenuItem(int row) override {
    return Framework::LiveMenuRow(row);
  }
  bool RowsIdle() override { return Framework::AreLiveMenuRowsIdle() != 0; }
  void ClearMenuExcept(int row) override {
    Framework::ClearLiveMenuExcept(row);
  }
  int DrawMainMenuCursor(int step, int position) override {
    return Framework::DrawLiveMainMenuCursor(step, position);
  }
  void CreateMenuRows(std::span<const std::span<const std::uint8_t>> strings,
                      std::span<int> row_types, int row_info) override {
    std::array<char*, kMaxRowStrings> pointers{};
    if (strings.size() > pointers.size() ||
        strings.size() != row_types.size() * 2) {
      Refuse("data menu rows do not pair a name and a description each");
      return;
    }
    for (std::size_t i = 0; i < strings.size(); ++i) {
      const std::optional<std::size_t> offset =
          OffsetIn(Menu_Files_textTable,
                   Menu_Files_textTable != nullptr ? kTextTableBytes : 0,
                   strings[i].data());
      if (!offset) {
        Refuse("data menu row text outside the MCMAN.BIN table");
        return;
      }
      pointers[i] = reinterpret_cast<char*>(Menu_Files_textTable) + *offset;
    }
    Framework::CreateLiveMenuRows(static_cast<int>(row_types.size()), row_info,
                                  pointers.data(), row_types.data());
  }
  int SelectedRow() override { return Framework::LiveSelectedRow(); }
  int HighlightedRow() override { return Framework::LiveHighlightedRow(); }
  void ClearCommandLabel() override {
    Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_None);
  }
  void DismissInformationBox() override {
    Framework::DismissLiveInformationBox();
  }
  void SetBackgroundFade(int fade) override {
    Framework::SetLiveBackgroundFade(fade);
  }
  int BackgroundFadeStep() override {
    return Menu_Framework_backgroundFadeStep;
  }
  void InitInformationBox(Framework::InformationHeader header) override {
    Menu_InitInformationTextBox(
        static_cast<enum Menu_InformationBoxHeader>(header));
  }
  void SetInformationBoxState(int state) override {
    Menu_textBoxes[kInformationTextBox].state =
        static_cast<std::int16_t>(state);
  }
  void SetInformationMessage(std::uint16_t offset) override {
    if (char* message = TableMessage(Menu_Files_textTable, offset, *this)) {
      Framework::SetLiveInformationMessage(message);
    }
  }
  int CurrentSavePointId() override {
    return VS::Field::LiveCurrentSavePointId();
  }
  int SaveLocationId() override {
    return static_cast<unsigned char>(Menu_saveLocationId);
  }
  void SetSaveLocationId(int id) override {
    Menu_saveLocationId = static_cast<char>(id);
  }
  GameFlags Flags() override {
    const State_StateFlags& flags = Audio_Seq_unusedStateFlags;
    return {.opaque_a7 = static_cast<std::uint8_t>(flags.opaqueA7),
            .intro_state = static_cast<std::uint8_t>(flags.introState),
            .clear_count = static_cast<std::uint8_t>(flags.clearCount),
            .difficulty = static_cast<std::uint8_t>(flags.difficulty)};
  }
  void SetGameOver(bool game_over) override {
    Audio_Seq_unusedStateFlags.gameOver = game_over ? 1 : 0;
  }
  void SetDifficulty(std::uint8_t difficulty) override {
    Audio_Seq_unusedStateFlags.difficulty = static_cast<char>(difficulty);
  }
  std::uint8_t CursorMemory() override {
    return static_cast<std::uint8_t>(Input_settings.cursorMemory);
  }
  void SetCursorMemory(std::uint8_t memory) override {
    Input_settings.cursorMemory = static_cast<char>(memory);
  }
  std::uint8_t MonoSound() override {
    return static_cast<std::uint8_t>(Input_settings.monoSound);
  }

  int UpdateDataSaveMenu(int init) override {
    return VS::Menu::UpdateLiveDataSaveMenu(init);
  }
  int UpdateDataLoadMenu(int init) override {
    return VS::Menu::UpdateLiveDataLoadMenu(init);
  }
  int StepMenuResources(int init) override {
    return VS::Save::MemoryCard::StepLiveMenuResources(init);
  }
  void ReleaseMenuResources() override {
    VS::Save::MemoryCard::ReleaseLiveMenuResources();
  }
  void EnterBattle() override { Boot::EnterBattleLive(); }
  void ResetToTitle() override { Boot::ResetToTitleLive(); }
  void SetFieldRendering(bool enabled) override {
    VS::Field::LiveSetFieldRenderingEnabled(enabled ? 1 : 0);
  }
  void FreeMusicSlot(int slot) override { Audio::LiveFreeMusicSlot(slot); }
  void FadeMusicSlotVolume(int slot, int volume, int frames) override {
    Audio::LiveFadeMusicSlotVolume(slot, volume, frames);
  }
  void SetMonoOutput(int mono) override { Audio::LiveSetMonoOutput(mono); }
  int LoadIntroSnapshotStep(int action) override {
    return VS::Save::MemoryCard::LoadLiveIntroSnapshotStep(action);
  }
  void SetCurrentMenu(int menu) override {
    Menu_Framework_menuState.currentMenu = static_cast<char>(menu);
  }

  void BeginGameOverLoad() override {
    Content_DiscFile file{.lba = VS_GAMEOVER_BIN_LBA,
                          .size = VS_GAMEOVER_BIN_SIZE};
    g_game_over_request = Content::AllocateLiveDiscRequest(&file);
    g_game_over_image =
        Core::Memory::AllocateLiveFromHighEnd(VS_GAMEOVER_BIN_SIZE);
    if (g_game_over_request != nullptr) {
      Content::EnqueueLiveDiscRequest(g_game_over_request, g_game_over_image);
    } else {
      g_game_over_request =
          Content::CompleteLiveRefusedDiscRequest(&file, g_game_over_image);
    }
  }
  std::optional<bool> GameOverLoaded() override {
    if (g_game_over_request == nullptr) {
      return std::nullopt;
    }
    return g_game_over_request->state == Content_DiscRequestState_Loaded;
  }
  std::span<const std::uint16_t> FinishGameOverLoad() override {
    Content::ReleaseLiveDiscRequest(g_game_over_request);
    g_game_over_request = nullptr;
    if (g_game_over_image == nullptr) {
      return {};
    }
    return {static_cast<const std::uint16_t*>(g_game_over_image),
            kGameOverImageHalfwords};
  }
  void FreeGameOverImage() override {
    if (g_game_over_image != nullptr) {
      Core::Memory::FreeLiveFromHighEnd(g_game_over_image);
      g_game_over_image = nullptr;
    }
  }
};

// The same screens over TITLE.PRG's copies (22C.c) of the message, cursor,
// unsaved-data and progress variables, its card check and its file overlay;
// the summaries, the card driver and the 0x7E sound group are shared.
class TitleFilesLiveHost final : public LiveHost {
 public:
  std::span<const std::uint16_t> TextTable() override {
    return TableSpan(Title_saveMenuText);
  }
  void SetCardMessage(std::optional<std::uint16_t> offset) override {
    Title_memoryCardMessage = TableMessage(Title_saveMenuText, offset, *this);
  }
  int AdvanceCardReadiness(int port) override {
    return VS::Save::MemoryCard::AdvanceLiveTitleCardReadiness(port);
  }
  bool CardMessageIs(std::uint16_t offset) override {
    return Title_saveMenuText != nullptr && offset < kTextTableHalfwords &&
           Title_memoryCardMessage ==
               reinterpret_cast<char*>(Title_saveMenuText + offset);
  }
  std::span<const std::uint8_t> CardMessage() override {
    if (Title_memoryCardMessage == nullptr) {
      return {};
    }
    const std::span<const std::uint8_t> text =
        TextTail(Title_memoryCardMessage);
    if (text.empty()) {
      Refuse("title memory-card message outside the MCMAN.BIN table");
    }
    return text;
  }
  std::int32_t CursorPosition() override {
    return Title_selectionCursorPosition;
  }
  void SetCursorPosition(std::int32_t position) override {
    Title_selectionCursorPosition = position;
  }
  std::uint8_t DataNotSaved() override {
    return static_cast<std::uint8_t>(Title_hasUnsavedData);
  }
  void SetDataNotSaved(std::uint8_t not_saved) override {
    Title_hasUnsavedData = static_cast<char>(not_saved);
  }
  std::int32_t ProgressCounter() override {
    return Title_transferProgressCounter;
  }
  void SetProgressCounter(std::int32_t counter) override {
    Title_transferProgressCounter = counter;
  }
  void DrawFileOverlay() override { Save_NativeDrawFileOverlay(1); }
};

// TITLE.PRG's file screen variables as their own State: Title_fileMenuElements
// and the scalars are read in before each call and written back after it, so
// the copied title code and the tests keep seeing them.
State g_title_state{};

void LoadTitleState() {
  for (std::size_t i = 0; i < kElementCount; ++i) {
    g_title_state.elements.at(i) =
        std::bit_cast<Element>(Title_fileMenuElements[i]);
  }
  g_title_state.is_saving = Title_isSaving != 0;
  g_title_state.container_data_empty = Title_isContainerDataEmpty != 0;
  g_title_state.backup_main_setting =
      static_cast<std::uint8_t>(Title_savedMainMenuSetting);
  g_title_state.screen_fade =
      static_cast<std::uint8_t>(Title_fileMenuScreenFade);
  g_title_state.cursor_color = Title_selectionCursorColor;
}

void StoreTitleState() {
  for (std::size_t i = 0; i < kElementCount; ++i) {
    Title_fileMenuElements[i] =
        std::bit_cast<Menu_Files_FileMenuElement>(g_title_state.elements.at(i));
  }
  Title_isSaving = static_cast<char>(g_title_state.is_saving);
  Title_isContainerDataEmpty =
      static_cast<char>(g_title_state.container_data_empty);
  Title_savedMainMenuSetting =
      static_cast<char>(g_title_state.backup_main_setting);
  Title_fileMenuScreenFade = static_cast<char>(g_title_state.screen_fade);
  Title_selectionCursorColor = g_title_state.cursor_color;
}

class TitleStateBinding {
 public:
  TitleStateBinding() { LoadTitleState(); }
  ~TitleStateBinding() { StoreTitleState(); }
  TitleStateBinding(const TitleStateBinding&) = delete;
  TitleStateBinding& operator=(const TitleStateBinding&) = delete;
  TitleStateBinding(TitleStateBinding&&) = delete;
  TitleStateBinding& operator=(TitleStateBinding&&) = delete;
};

template <typename Function>
auto WithLiveTitleContext(Function&& function) {
  TitleFilesLiveHost host;
  const TitleStateBinding binding;
  Context context{.state = g_title_state, .host = host};
  return function(context);
}

// TITLE.PRG's save search, display and clear-save reminder.
class TitleLiveHost final : public TitleHost {
 public:
  void Submit(std::span<const std::uint32_t> packet) override {
    SubmitLive(packet);
  }
  void Refuse(std::string_view what) override { Warn(what); }
  std::span<const std::uint16_t> TextTable() override {
    return TableSpan(Title_saveMenuText);
  }
  void SetCardMessage(std::optional<std::uint16_t> offset) override {
    Title_memoryCardMessage = TableMessage(Title_saveMenuText, offset, *this);
  }
  int AdvanceCardReadiness(int port) override {
    return VS::Save::MemoryCard::AdvanceLiveTitleCardReadiness(port);
  }
  int ScanSaveDirectory(int port) override {
    return VS::Save::MemoryCard::ScanLiveSaveDirectory(port);
  }
  int FindMatchingSaveSlot() override {
    return VS::Save::MemoryCard::FindLiveMatchingSaveSlot();
  }
  Framework::Buttons Input() override { return LiveButtons(); }

  std::uint8_t FrameBuffer() override {
    return static_cast<std::uint8_t>(Menu_Files_frameBuf);
  }
  void SetFrameBuffer(std::uint8_t buffer) override {
    Menu_Files_frameBuf = static_cast<char>(buffer);
  }
  void PutDisplay(int display_x, int draw_x) override {
    DISPENV display{};
    DRAWENV draw{};
    SetDefDispEnv(&display, display_x, 0, kBufferWidth, kBufferHeight);
    SetDefDrawEnv(&draw, draw_x, 0, kBufferWidth, kBufferHeight);
    display.screen.y = kDisplayTop;
    display.screen.h = kDisplayHeight;
    PutDispEnv(&display);
    PutDrawEnv(&draw);
  }
  void ClearScreen() override { ClearBothBuffers(); }
  void SetDisplayMask(bool shown) override { SetDispMask(shown ? 1 : 0); }
  void InitializeElement(int id, std::uint32_t position, std::uint32_t size,
                         std::uint16_t text_offset) override {
    if (!InitializeLiveTitleElement(id, position, size, text_offset)) {
      Refuse("title reminder element without its text or slot");
    }
  }
  void AnimateElement(int id, ElementState state,
                      std::int16_t target) override {
    Menu_Files_FileMenuElement* const element = TitleElement(id);
    if (element == nullptr) {
      Refuse("title reminder element outside the table");
      return;
    }
    element->state = static_cast<char>(state);
    element->targetPosition = target;
  }
  void SetElementSelected(int id, bool selected) override {
    Menu_Files_FileMenuElement* const element = TitleElement(id);
    if (element == nullptr) {
      Refuse("title reminder element outside the table");
      return;
    }
    element->selected = selected ? 1 : 0;
  }
  bool AnimationsComplete() override {
    return AreLiveTitleAnimationsComplete();
  }
  void SetCursorPosition(std::int32_t position) override {
    Title_selectionCursorPosition = position;
  }
  // TITLE.PRG's _playMenuSelectSfx, _playMenuLeaveSfx and
  // _playMenuChangeSfx play the same 0x7E effects.
  void PlaySound(MenuSound sound) override { PlayMenuSound(sound); }
  void PlaySoundEffect(int effect) override {
    Audio::LivePlaySfxCentered(kMenuSoundChannelGroup, effect);
  }
};

template <typename Function>
auto WithLiveContext(Function&& function) {
  LiveHost host;
  Context context{.state = g_state, .host = host};
  return function(context);
}
}  // namespace

int UpdateLiveDataMenu(std::uint8_t& step) {
  return WithLiveContext(
      [&step](Context& context) { return UpdateDataMenu(context, step); });
}

int UpdateLiveGameOver(std::uint8_t& step) {
  return WithLiveContext(
      [&step](Context& context) { return UpdateGameOver(context, step); });
}

int UpdateLiveContainerConfirmation(int init) {
  return WithLiveContext([init](Context& context) {
    return UpdateContainerConfirmation(context, init);
  });
}

void ResetLiveElements() {
  WithLiveContext([](Context& context) { ResetElements(context); });
}

void DrawLiveFrame(int frame_buffer) {
  WithLiveContext(
      [frame_buffer](Context& context) { DrawFrame(context, frame_buffer); });
}

void ResetLiveTitleElements() {
  WithLiveTitleContext([](Context& context) { ResetElements(context); });
}

void DrawLiveTitleFrame(int frame_buffer) {
  WithLiveTitleContext(
      [frame_buffer](Context& context) { DrawFrame(context, frame_buffer); });
}

void DrawLiveTitleElement(Element& element) {
  WithLiveTitleContext(
      [&element](Context& context) { DrawElement(context, element); });
}

bool InitializeLiveTitleElement(int id, std::uint32_t position,
                                std::uint32_t size, std::uint16_t text_offset) {
  if (Title_saveMenuText == nullptr || text_offset >= kTextTableHalfwords) {
    return false;
  }
  const std::span<const std::uint8_t> text =
      std::span(reinterpret_cast<const std::uint8_t*>(Title_saveMenuText),
                kTextTableBytes)
          .subspan(static_cast<std::size_t>(text_offset) *
                   sizeof(std::uint16_t));
  return WithLiveTitleContext([&](Context& context) {
    return InitializeElement(context, id, position, size, text) != nullptr;
  });
}

bool AreLiveTitleAnimationsComplete() {
  LoadTitleState();
  return AreAnimationsComplete(g_title_state);
}

void CopyLiveSelectedContainerItems(Items_ContainerData* target,
                                    const Items_ContainerData* source,
                                    const Items_ContainerIndices* to_copy) {
  if (target == nullptr || source == nullptr || to_copy == nullptr) {
    Warn("container copy without its records");
    return;
  }
  CopySelectedContainerItems(
      *reinterpret_cast<ContainerData*>(target),
      *reinterpret_cast<const ContainerData*>(source),
      *reinterpret_cast<const ContainerIndices*>(to_copy));
}

int DrawLiveGlyph(std::uint32_t glyph, int x, int y, int clut) {
  LiveImmediateTarget target;
  return DrawGlyph(target, glyph, x, y, clut);
}

void DrawLiveSummaryLabel(std::int32_t position, Label label) {
  LiveHost host;
  DrawSummaryLabel(host, position, label);
}

void DrawLiveHpMpSummary(std::int32_t position, Stat stat,
                         std::uint32_t current, std::uint32_t max) {
  LiveHost host;
  DrawHpMpSummary(host, position, stat, current, max);
}

void DrawLiveElement(Element& element) {
  WithLiveContext(
      [&element](Context& context) { DrawElement(context, element); });
}

void DrawLiveInteger(std::int32_t position, std::uint32_t value,
                     std::uint32_t divisor) {
  LiveImmediateTarget target;
  DrawInteger(target, position, value, divisor);
}

void DrawLiveText(const char* text, int x, int y, int clut) {
  const std::span<const std::uint8_t> tail = TextTail(text);
  if (tail.empty()) {
    Warn("file menu text outside the MCMAN.BIN table and footer");
    return;
  }
  LiveImmediateTarget target;
  DrawText(target, g_state.text_arrow, tail, x, y, clut);
}

std::uint32_t LiveElementBackgroundColor(std::uint32_t outer,
                                         std::uint32_t inner) {
  const std::optional<std::uint32_t> color =
      ElementBackgroundColor(outer, inner);
  if (!color) {
    Warn(std::format("file menu element colour factor {} lies past the tables",
                     inner));
    return 0;
  }
  return *color;
}

void DrawLiveBackground() {
  LiveImmediateTarget target;
  DrawBackground(target);
}

void InitializeLiveDisplay() {
  TitleLiveHost host;
  InitializeDisplay(host);
}

void SwapLiveBuffers() {
  TitleLiveHost host;
  SwapBuffers(host);
}

int UpdateLiveClearSaveReminder(int init) {
  TitleLiveHost host;
  TitleContext context{.state = g_state, .host = host};
  return UpdateClearSaveReminder(context, init != 0);
}

void DrawLiveExitFade() {
  TitleLiveHost host;
  DrawExitFade(host);
}

State& LiveFilesState() { return g_state; }

Host& LiveFilesHost() {
  static LiveHost host;
  return host;
}

void ResetLiveFilesStatics() { ResetStatics(g_state); }
}  // namespace VS::Menu::Files
