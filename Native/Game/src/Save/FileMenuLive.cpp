// Hand-maintained (tools/port/owned.csv).
// Live binding for the native file pickers (FileMenu.cpp): the native save
// files and their previews, the footer text, sounds, the staged container
// and clear-save documents, and the C entries the MENU7 and TITLE screens
// call.
#include "Save/FileMenuLive.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "Core/Log.hpp"
#include "Menu/Sounds.hpp"
#include "Platform/App.hpp"
#include "Save/FileMenu.hpp"
#include "Save/NativeFilePresentation.hpp"
#include "Save/NativeFiles.hpp"
#include "Save/NativeMenu.h"
#include "Save/NativeMenu.hpp"
#include "Save/NativeResume.h"
#include "Save/RuntimeSnapshot.hpp"
extern "C" {
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Boot/GameLoop.h"
#include "Core/Text/TextCode.h"
#include "Field/Gimmicks.h"
#include "Input/Controller.h"
#include "Menu/Files.h"
#include "Title/SaveFiles.h"
}

namespace VS::Game::Save {
namespace {
namespace file_menu = VS::Menu::Files;
using file_menu::FileMenuError;
using file_menu::FileMenuFault;
using file_menu::FileMenuScreen;
using file_menu::kFileRows;
using file_menu::kFileSlotCount;

constexpr int kUppercaseGlyph = 10;
constexpr int kLowercaseGlyph = 36;
constexpr int kMenuChannelGroup = 0x7E;
// The footer fits 288 pixels of six-pixel glyphs, not a 32-byte card label.
constexpr std::size_t kFooterTextLimit = 48;

struct FilePreview {
  FileInfo file;
  NativeFileSummary summary;
};

// The typed picker plus the documents and summaries its window shows.
struct LivePicker {
  file_menu::FileMenuSession session;
  std::array<FileInfo, kFileRows> files{};
  std::array<NativeFileSummary, kFileRows> summaries{};
  std::array<std::unique_ptr<FilePreview>, kFileSlotCount> previews{};
  std::optional<FileInfo> confirmed;
};

LivePicker picker;
std::optional<std::filesystem::path> directory;
std::optional<Files> files;
std::optional<Items_ContainerData> staged_storage;
std::optional<Document> staged_clear;
std::array<char, kFooterTextLimit + 1> footer{};

std::string& ClearError() {
  static std::string error;
  return error;
}

[[noreturn]] void Fail(const char* message) {
  VS_LOG_ERROR("save", "{}", message);
  std::abort();
}

[[noreturn]] void FailWithoutConfirmedDocument() {
  Fail("Native save request requires a confirmed document");
}

void Require(bool valid, const char* message) {
  if (!valid) Fail(message);
}

void ForgetPicker() {
  picker.files = {};
  picker.summaries = {};
  for (auto& preview : picker.previews) preview.reset();
  picker.confirmed.reset();
}

bool BankEmpty(const Items_ContainerData& bank) {
  const auto empty = [](const auto& item) { return item.id == 0; };
  return std::ranges::all_of(
             bank.weapons, [](const auto& item) { return item.blade == 0; }) &&
         std::ranges::all_of(
             bank.shields,
             [](const auto& item) { return item.base.id == 0; }) &&
         std::ranges::all_of(bank.blades, empty) &&
         std::ranges::all_of(bank.grips, empty) &&
         std::ranges::all_of(bank.armor, empty) &&
         std::ranges::all_of(bank.gems, empty) &&
         std::ranges::all_of(bank.misc, empty);
}

unsigned char Glyph(unsigned char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'A' && c <= 'Z') {
    return c - 'A' + kUppercaseGlyph;
  } else if (c >= 'a' && c <= 'z') {
    return c - 'a' + kLowercaseGlyph;
  } else if (c == ' ') {
    return Core_TextCode_Space;
  } else if (c == ':') {
    return Core_TextCode_Colon;
  } else if (c == '/') {
    return Core_TextCode_ForwardSlash;
  } else if (c == '-') {
    return Core_TextCode_Hyphen;
  }
  throw std::logic_error("Unsupported native save label character");
}

void Status(FileMenuScreen screen, std::string_view text) {
  if (text.size() > kFooterTextLimit) {
    throw std::logic_error("Native save prompt exceeds one game text line");
  }
  std::size_t n = 0;
  for (const unsigned char c : text) {
    footer[n++] = static_cast<char>(Glyph(c));
  }
  footer[n] = static_cast<char>(Core_TextCode_Terminator);
  if (screen == FileMenuScreen::kTitle) {
    Title_memoryCardMessage = footer.data();
  } else {
    Menu_Files_memoryCardMessage = footer.data();
  }
}

std::expected<Files*, std::string> OpenFiles() {
  if (files) return &*files;
  if (!directory) {
    auto root = Platform::UserDataDir();
    if (!root) return std::unexpected(root.error());
    directory = *root / "saves";
  }
  auto opened = Files::Open(*directory);
  if (!opened) return std::unexpected(opened.error());
  files = std::move(*opened);
  const auto utf8 = directory->u8string();
  VS_LOG_INFO("save", "Native save directory: {}",
              std::string(utf8.begin(), utf8.end()));
  return &*files;
}

std::int64_t MicrosecondsSince(std::chrono::steady_clock::time_point started) {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now() - started)
      .count();
}

file_menu::SlotStatus SlotStatusOf(FileStatus status) {
  switch (status) {
    case FileStatus::kValid:
      return file_menu::SlotStatus::kValid;
    case FileStatus::kRecovered:
      return file_menu::SlotStatus::kRecovered;
    case FileStatus::kInvalid:
      return file_menu::SlotStatus::kInvalid;
    case FileStatus::kEmpty:
      break;
  }
  return file_menu::SlotStatus::kEmpty;
}

file_menu::FileSlot SlotOf(const FileInfo& file) {
  return {.slot = file.slot,
          .status = SlotStatusOf(file.status),
          .has_document = file.document.has_value(),
          .message = file.message};
}

std::expected<std::unique_ptr<FilePreview>, FileMenuError> LoadPreview(
    Files& store, int slot) {
  const auto inspecting = std::chrono::steady_clock::now();
  auto info = store.Inspect(slot);
  const auto inspected = std::chrono::steady_clock::now();
  if (!info) {
    return std::unexpected(FileMenuError{FileMenuFault::kRead, info.error()});
  }
  NativeFileSummary summary;
  if (info->document) {
    const auto described = DescribeNativeFile(*info->document);
    if (!described) {
      return std::unexpected(
          FileMenuError{FileMenuFault::kSummary, described.error()});
    }
    summary = *described;
  }
  VS_LOG_DEBUG("save.perf", "Preview slot={} inspect_us={} summary_us={}", slot,
               std::chrono::duration_cast<std::chrono::microseconds>(inspected -
                                                                     inspecting)
                   .count(),
               MicrosecondsSince(inspected));
  if (!info->message.empty()) {
    VS_LOG_WARN("save", "Native slot {}: {}", info->slot, info->message);
  }
  return std::make_unique<FilePreview>(std::move(*info), summary);
}

std::expected<bool, std::string> MatchesPreview(const FileInfo& current,
                                                const FileInfo& preview) {
  if (current.status != preview.status ||
      current.document.has_value() != preview.document.has_value()) {
    return false;
  }
  if (!current.document) return true;
  const auto started = std::chrono::steady_clock::now();
  const auto same = SameDocument(*preview.document, *current.document);
  VS_LOG_DEBUG("save.perf", "Compare slot={} semantic_us={} valid={}",
               current.slot, MicrosecondsSince(started), same.has_value());
  return same;
}

// Previews are not load or overwrite consent for changed content. Inspect
// runs Files::Load's current primary/backup and content validation, including
// for an apparently empty container that appeared while browsing.
std::expected<FileInfo, FileMenuError> ReadConfirmedSelection(
    const FileInfo& selected) {
  const auto opened = OpenFiles();
  if (!opened) {
    return std::unexpected(FileMenuError{FileMenuFault::kOpen, opened.error()});
  }
  auto current = (*opened)->Inspect(selected.slot);
  if (!current) {
    return std::unexpected(
        FileMenuError{FileMenuFault::kRead, current.error()});
  }
  const auto matches = MatchesPreview(*current, selected);
  if (!matches) {
    return std::unexpected(
        FileMenuError{FileMenuFault::kVerify, matches.error()});
  }
  if (!*matches) {
    return std::unexpected(
        FileMenuError{FileMenuFault::kChanged, current->message});
  }
  return std::move(*current);
}

std::expected<Items_ContainerData, std::string> BankFor(
    const file_menu::FileMenu& menu, const FileInfo& selected) {
  if (menu.container) {
    if (!staged_storage) {
      return std::unexpected("No staged container transaction");
    }
    return *staged_storage;
  }
  if (selected.document) return selected.document->state.storage;
  return Items_ContainerData{};
}

std::expected<Document, std::string> BuildDocument(
    const file_menu::FileMenu& menu, const FileInfo& selected) {
  auto bank = BankFor(menu, selected);
  if (!bank) return std::unexpected(bank.error());
  Document document;
  if (menu.clear_game) {
    if (!staged_clear) {
      return std::unexpected("No staged original ending clear save");
    }
    document = *staged_clear;
    // F03601 TITLE 22C.c:698 preserves the source's partial container reset.
    std::fill_n(std::begin(bank->misc), kClearContainerResetStacks,
                Items_InventoryMisc{});
    document.state.storage = *bank;
  } else {
    auto captured = Capture(*bank);
    if (!captured) return std::unexpected(captured.error());
    document.state = std::move(*captured);
  }
  auto& settings = document.state.settings;
  settings.containerEmpty = BankEmpty(*bank);
  const auto counters = file_menu::AdvanceSaveCounters(
      {.generation = settings.saveFileGeneration,
       .cumulative = settings.cumulativeSaveCount,
       .current_game = settings.currentGameSaveCount},
      menu.clear_game);
  if (!counters) {
    return std::unexpected("Save generation has reached its limit");
  }
  settings.saveFileGeneration = counters->generation;
  settings.cumulativeSaveCount = counters->cumulative;
  settings.currentGameSaveCount = counters->current_game;
  const auto generation = file_menu::NextFileGeneration(
      selected.document ? selected.document->generation : 0);
  if (!generation) {
    return std::unexpected("Native file generation has reached its limit");
  }
  const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  if (now < 0) return std::unexpected("System clock precedes the save epoch");
  document.generation = *generation;
  document.saved_unix_ms = static_cast<std::uint64_t>(now);
  return document;
}

void AdoptSettings(const Document& document) {
  const auto& settings = document.state.settings;
  Input_settings.cumulativeSaveCount = settings.cumulativeSaveCount;
  Input_settings.currentGameSaveCount = settings.currentGameSaveCount;
  Input_settings.saveFileGeneration = settings.saveFileGeneration;
  Input_settings.containerEmpty = settings.containerEmpty;
}

class LiveFileMenuHost final : public file_menu::FileMenuHost {
 public:
  void BeginScreen(FileMenuScreen screen) override {
    ForgetPicker();
    if (screen == FileMenuScreen::kTitle) {
      Title_SaveFiles_ResetElements();
      Title_selectionCursorPosition = 0;
    } else {
      Menu_Files_ResetElements();
      Menu_Files_selectCursorXy = 0;
    }
    Menu_Files_dataNotSaved = 0;
  }
  void ShowStatus(FileMenuScreen screen, std::string_view text) override {
    Status(screen, text);
  }
  void PlaySound(file_menu::FileMenuSound sound) override;
  void ReportError(std::string_view text, std::string_view detail) override {
    VS_LOG_ERROR("save", "{}: {}", text, detail);
  }
  std::optional<std::string> TakeResumeError() override {
    if (!*Save_NativeResumeError()) return std::nullopt;
    std::string error = Save_NativeResumeError();
    Save_NativeClearResumeError();
    return error;
  }
  [[nodiscard]] std::expected<void, std::string> ClearSaveStaged()
      const override {
    if (staged_clear) return {};
    return std::unexpected(ClearError());
  }

  void ForgetPreviews() override {
    for (auto& preview : picker.previews) preview.reset();
  }
  [[nodiscard]] bool WindowCached(int first_slot) const override {
    return std::all_of(Window(first_slot).begin(), Window(first_slot).end(),
                       [](const auto& preview) { return preview != nullptr; });
  }
  std::expected<file_menu::FileWindow, FileMenuError> ReadWindow(
      int first_slot) override;
  std::expected<file_menu::FileSlot, FileMenuError> ConfirmSelection(
      const file_menu::FileMenu& menu) override;

  [[nodiscard]] bool GameplayActive() const override {
    return Boot_isGameplayActive != 0;
  }
  [[nodiscard]] bool SavePointEnabled() const override {
    return Field_CurrentSavePointId() && !Audio_Seq_unusedStateFlags.opaqueA7;
  }
  std::expected<void, std::string> WriteSelection(
      const file_menu::FileMenu& menu, VS::Save::WriteConsent consent) override;
  void OpenEmptyContainer() override {
    staged_storage.emplace();
    Input_settings.containerEmpty = 1;
  }
  void OpenConfirmedContainer() override;
  std::expected<void, std::string> ResumeConfirmedSelection() override;

 private:
  static std::span<std::unique_ptr<FilePreview>, kFileRows> Window(
      int first_slot) {
    Require(first_slot >= 0 && first_slot <= kFileSlotCount - kFileRows,
            "Native save window is outside the save slots");
    return std::span(picker.previews)
        .subspan(static_cast<std::size_t>(first_slot))
        .first<kFileRows>();
  }
  static const FileInfo& SelectedFile(const file_menu::FileMenu& menu) {
    Require(menu.row >= 0 && menu.row < kFileRows,
            "Native save selection is outside the window");
    return picker.files[static_cast<std::size_t>(menu.row)];
  }
  static const FileInfo& ConfirmedFile() {
    const auto& confirmed = picker.confirmed;
    if (!confirmed || !confirmed->document) FailWithoutConfirmedDocument();
    return *confirmed;
  }
  static const Document& ConfirmedDocument() {
    const auto& document = ConfirmedFile().document;
    if (!document) FailWithoutConfirmedDocument();
    return *document;
  }
};

void LiveFileMenuHost::PlaySound(file_menu::FileMenuSound sound) {
  switch (sound) {
    case file_menu::FileMenuSound::kChange:
      VS::Menu::PlayChangeSound();
      break;
    case file_menu::FileMenuSound::kSelect:
      VS::Menu::PlaySelectSound();
      break;
    case file_menu::FileMenuSound::kLeave:
      VS::Menu::PlayLeaveSound();
      break;
    case file_menu::FileMenuSound::kInvalid:
      VS::Menu::PlayInvalidSound();
      break;
    case file_menu::FileMenuSound::kComplete:
      Audio_PlaySfxCentered(kMenuChannelGroup, VS_SFX_FILEOPCOMPLETE);
      break;
  }
}

std::expected<file_menu::FileWindow, FileMenuError>
LiveFileMenuHost::ReadWindow(int first_slot) {
  const auto started = std::chrono::steady_clock::now();
  const auto window = Window(first_slot);
  int cache_hits = 0;
  const auto opened = OpenFiles();
  if (!opened) {
    return std::unexpected(FileMenuError{FileMenuFault::kOpen, opened.error()});
  }
  file_menu::FileWindow rows;
  for (std::size_t i = 0; i < window.size(); ++i) {
    auto& preview = window[i];
    if (!preview) {
      // At most 100 owned previews, cleared on Begin/Notice/session reset.
      // Sliding the three-card window does not decode unchanged saves again.
      auto loaded = LoadPreview(**opened, first_slot + static_cast<int>(i));
      if (!loaded) return std::unexpected(std::move(loaded.error()));
      preview = std::move(*loaded);
    } else {
      ++cache_hits;
    }
    picker.files[i] = preview->file;
    picker.summaries[i] = preview->summary;
    rows[i] = SlotOf(preview->file);
  }
  VS_LOG_DEBUG("save.perf", "ReadFiles first_slot={} cache_hits={} total_us={}",
               first_slot, cache_hits, MicrosecondsSince(started));
  return rows;
}

std::expected<file_menu::FileSlot, FileMenuError>
LiveFileMenuHost::ConfirmSelection(const file_menu::FileMenu& menu) {
  const auto& selected = SelectedFile(menu);
  VS_LOG_INFO("save", "Native {} request: slot={} status={} container={}",
              menu.select_container ? "container"
              : menu.save           ? "save"
                                    : "load",
              selected.slot, static_cast<int>(selected.status), menu.container);
  const auto validating = std::chrono::steady_clock::now();
  auto current = ReadConfirmedSelection(selected);
  VS_LOG_DEBUG("save.perf", "Confirmed slot={} revalidate_us={} valid={}",
               selected.slot, MicrosecondsSince(validating),
               current.has_value());
  if (!current) return std::unexpected(std::move(current.error()));
  picker.confirmed = std::move(*current);
  return SlotOf(*picker.confirmed);
}

std::expected<void, std::string> LiveFileMenuHost::WriteSelection(
    const file_menu::FileMenu& menu, VS::Save::WriteConsent consent) {
  const auto opened = OpenFiles();
  if (!opened) return std::unexpected(opened.error());
  const auto& selected = SelectedFile(menu);
  auto document = BuildDocument(menu, selected);
  if (!document) return std::unexpected(document.error());
  const auto summary = DescribeNativeFile(*document);
  if (!summary) return std::unexpected(summary.error());
  const auto written = (*opened)->Write(selected.slot, *document, consent);
  if (!written) return std::unexpected(written.error());
  AdoptSettings(*document);
  Menu_Files_dataNotSaved = 1;
  VS_LOG_INFO(
      "save", "Saved native slot {} generation {} zone {} room {} scene {}",
      selected.slot, document->generation, document->state.location.zone,
      document->state.location.room, document->state.location.scene);
  const auto row = static_cast<std::size_t>(menu.row);
  picker.summaries[row] = *summary;
  picker.files[row] = FileInfo{.slot = selected.slot,
                               .status = FileStatus::kValid,
                               .document = std::move(*document),
                               .message = {}};
  return {};
}

void LiveFileMenuHost::OpenConfirmedContainer() {
  const auto& document = ConfirmedDocument();
  const auto& current = ConfirmedFile();
  staged_storage = document.state.storage;
  Input_settings.containerEmpty = BankEmpty(*staged_storage);
  VS_LOG_INFO("save", "Opened native container in slot {} recovered {}",
              current.slot, current.status == FileStatus::kRecovered);
}

std::expected<void, std::string> LiveFileMenuHost::ResumeConfirmedSelection() {
  const auto& document = ConfirmedDocument();
  const auto& current = ConfirmedFile();
  const auto preparing = std::chrono::steady_clock::now();
  const auto prepared = PrepareResume(document);
  VS_LOG_DEBUG("save.perf", "Prepare slot={} total_us={} valid={}",
               current.slot, MicrosecondsSince(preparing),
               prepared.has_value());
  if (!prepared) return std::unexpected(prepared.error());
  VS_LOG_INFO("save", "Prepared native slot {} generation {} recovered {}",
              current.slot, document.generation,
              current.status == FileStatus::kRecovered);
  return {};
}

using PickerEntry = file_menu::FileMenuResult (*)(
    file_menu::FileMenuSession&, file_menu::FileMenuHost&, int,
    const file_menu::FileMenuInput&);

int RunPicker(PickerEntry entry, int init) {
  const auto pressed = Input_buttonsPressed.allButtons;
  const file_menu::FileMenuInput input{
      .accept = (pressed & PADRright) != 0,
      .cancel = (pressed & PADRdown) != 0,
      .up = (Input_buttonRepeat & PADLup) != 0,
      .down = (Input_buttonRepeat & PADLdown) != 0};
  LiveFileMenuHost host;
  return static_cast<int>(entry(picker.session, host, init, input));
}

int InitialTitleSelection() {
  const auto opened = OpenFiles();
  if (!opened) {
    VS_LOG_ERROR("save", "Cannot inspect native saves: {}", opened.error());
    // Focus Continue so its picker can display the actual storage failure.
    return 1;
  }
  const auto present = (*opened)->AnyFiles();
  if (!present) {
    VS_LOG_ERROR("save", "Cannot inspect native saves: {}", present.error());
    return 1;
  }
  return *present ? 1 : 0;
}
}  // namespace

void ConfigureDirectory(std::filesystem::path root) {
  if (ResumePending()) {
    throw std::logic_error("Cannot change save directory during resume");
  }
  ResetFileMenuSession();
  files.reset();
  directory = std::move(root);
}

void ResetFileMenuSession() {
  picker.session = {};
  ForgetPicker();
  staged_storage.reset();
  staged_clear.reset();
  ClearError().clear();
  if (Title_memoryCardMessage == footer.data()) {
    Title_memoryCardMessage = nullptr;
  }
  if (Menu_Files_memoryCardMessage == footer.data()) {
    Menu_Files_memoryCardMessage = nullptr;
  }
}

std::optional<NativeFileScreen> FileMenuPresentation() {
  const auto& menu = picker.session.menu;
  if (menu.phase == file_menu::FileMenuPhase::kInactive) return std::nullopt;
  return NativeFileScreen{
      .title = menu.screen == FileMenuScreen::kTitle,
      .save = menu.save,
      .select_container = menu.select_container,
      .files = menu.ready ? std::span<const FileInfo>(picker.files)
                          : std::span<const FileInfo>{},
      .summaries = picker.summaries,
      .selected_row = menu.row,
      .confirmation = menu.phase == file_menu::FileMenuPhase::kConfirm,
      .affirmative = menu.affirmative};
}

void ShutdownFileMenus() {
  CancelResume();
  ResetFileMenuSession();
  files.reset();
  directory.reset();
}
}  // namespace VS::Game::Save

namespace VS::Menu {
// F02764 _showSaveMenu, MENU/MENU7.PRG/260.c:2673.
int UpdateLiveDataSaveMenu(int init_state) {
  return VS::Game::Save::RunPicker(Files::UpdateDataSaveMenu, init_state);
}
// F02767 _loadFileMenu, MENU/MENU7.PRG/260.c:3187.
int UpdateLiveDataLoadMenu(int init_fadeout) {
  return VS::Game::Save::RunPicker(Files::UpdateDataLoadMenu, init_fadeout);
}
// F03628 _loadFileMenu, TITLE/TITLE.PRG/22C.c:2213.
int UpdateLiveLoadMenu(int init_fadeout) {
  return VS::Game::Save::RunPicker(Files::UpdateLoadMenu, init_fadeout);
}
// F03634 _showSaveMenu, TITLE/TITLE.PRG/22C.c:2880; clear-save gate only.
int UpdateLiveSaveMenu(int init_state) {
  return VS::Game::Save::RunPicker(Files::UpdateSaveMenu, init_state);
}
}  // namespace VS::Menu

extern "C" int Save_NativeSelectContainer(int init) {
  return VS::Game::Save::RunPicker(VS::Menu::Files::UpdateContainerMenu, init);
}

extern "C" int Save_NativeInitialTitleSelection(void) {
  return VS::Game::Save::InitialTitleSelection();
}

extern "C" void Save_NativeDrawFileOverlay(int title) {
  const auto screen = VS::Game::Save::FileMenuPresentation();
  if (screen && screen->title == (title != 0)) {
    VS::Game::Save::DrawNativeFileScreen(*screen);
  }
}

extern "C" Items_ContainerData* Save_NativeContainerData(void) {
  auto& storage = VS::Game::Save::staged_storage;
  if (!storage) {
    VS_LOG_ERROR("save", "Container access without a selected native file");
    std::abort();
  }
  return &*storage;
}

extern "C" void Save_NativeReleaseContainer(void) {
  VS::Game::Save::staged_storage.reset();
}

extern "C" void Save_NativeStageClearGame(void) {
  namespace Save = VS::Game::Save;
  Save::staged_clear.reset();
  Save::ClearError().clear();
  auto document = Save::CaptureClearGame();
  if (!document) {
    Save::ClearError() = document.error();
    VS_LOG_ERROR("save", "Cannot stage ending clear save: {}",
                 document.error());
    return;
  }
  Save::staged_clear = std::move(*document);
  VS_LOG_INFO("save", "Staged original ending clear save");
}
