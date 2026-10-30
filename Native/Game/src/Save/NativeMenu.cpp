#include "Save/NativeMenu.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "Core/Log.hpp"
#include "Platform/App.hpp"
#include "Save/NativeFilePresentation.hpp"
#include "Save/NativeFiles.hpp"
#include "Save/NativeMenu.h"
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
#include "Menu/Sounds.h"
#include "Title/SaveFiles.h"
}

namespace VS::Game::Save {
namespace {
constexpr int kRows = 3;
constexpr int kUppercaseGlyph = 10;
constexpr int kLowercaseGlyph = 36;
constexpr int kSaveCountLimit = 9999;
constexpr int kMenuChannelGroup = 0x7E;
// The footer fits 288 pixels of six-pixel glyphs, not a 32-byte card label.
constexpr std::size_t kFooterTextLimit = 48;
enum class Phase { kInactive, kRead, kPick, kConfirm, kExecute, kNotice };

struct FilePreview {
  FileInfo file;
  NativeFileSummary summary;
};

struct Picker {
  Phase phase = Phase::kInactive;
  bool title = false;
  bool save = false;
  bool container = false;
  bool clear_game = false;
  bool select_container = false;
  bool yes = false;
  bool ready = false;
  int first_slot = 0;
  int row = 0;
  int notice_result = 0;
  std::array<FileInfo, kRows> files{};
  std::array<NativeFileSummary, kRows> summaries{};
  std::array<std::unique_ptr<FilePreview>, VS::Save::SaveStore::kSlotCount>
      previews{};
};

Picker picker;
std::optional<std::filesystem::path> directory;
std::optional<Files> files;
std::optional<Items_ContainerData> staged_storage;
std::optional<Document> staged_clear;
int last_slot = 0;
std::array<char, kFooterTextLimit + 1> footer{};

std::string& ClearError() {
  static std::string error;
  return error;
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

void Status(std::string_view text) {
  if (text.size() > kFooterTextLimit) {
    throw std::logic_error("Native save prompt exceeds one game text line");
  }
  std::size_t n = 0;
  for (const unsigned char c : text) {
    footer[n++] = static_cast<char>(Glyph(c));
  }
  footer[n] = static_cast<char>(Core_TextCode_Terminator);
  if (picker.title) {
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

void Notice(std::string_view text, std::string_view detail, int result = 0) {
  // Notices leave the picker or offer a retry. Neither may reuse a preview
  // captured before a failed write, changed file or content/storage error.
  for (auto& preview : picker.previews) preview.reset();
  picker.phase = Phase::kNotice;
  picker.notice_result = result;
  Status(text);
  if (!detail.empty()) {
    VS_LOG_ERROR("save", "{}: {}", text, detail);
  }
}

void PickStatus() {
  const auto status = picker.files[picker.row].status;
  Status(status == FileStatus::kRecovered ? "Backup available - select to use"
         : status == FileStatus::kInvalid ? "Invalid data - not overwritten"
                                          : "Select a file");
}

void ReadFiles() {
  const auto started = std::chrono::steady_clock::now();
  int cache_hits = 0;
  const auto opened = OpenFiles();
  if (!opened) {
    Notice("Cannot open native saves", opened.error(), -1);
    return;
  }
  for (int i = 0; i < kRows; ++i) {
    auto& preview = picker.previews[picker.first_slot + i];
    if (!preview) {
      const auto inspecting = std::chrono::steady_clock::now();
      auto info = (*opened)->Inspect(picker.first_slot + i);
      const auto inspected = std::chrono::steady_clock::now();
      if (!info) {
        Notice("Cannot read native saves", info.error(), -1);
        return;
      }
      NativeFileSummary summary;
      if (info->document) {
        const auto described = DescribeNativeFile(*info->document);
        if (!described) {
          Notice("Save summary unavailable", described.error(), -1);
          return;
        }
        summary = *described;
      }
      const auto summarized = std::chrono::steady_clock::now();
      VS_LOG_DEBUG("save.perf", "Preview slot={} inspect_us={} summary_us={}",
                   picker.first_slot + i,
                   std::chrono::duration_cast<std::chrono::microseconds>(
                       inspected - inspecting)
                       .count(),
                   std::chrono::duration_cast<std::chrono::microseconds>(
                       summarized - inspected)
                       .count());
      if (!info->message.empty()) {
        VS_LOG_WARN("save", "Native slot {}: {}", info->slot, info->message);
      }
      // At most 100 owned previews, cleared on Begin/Notice/session reset.
      // Sliding the three-card window does not decode unchanged saves again.
      preview = std::make_unique<FilePreview>(std::move(*info), summary);
    } else {
      ++cache_hits;
    }
    picker.files[i] = preview->file;
    picker.summaries[i] = preview->summary;
  }
  picker.ready = true;
  picker.phase = Phase::kPick;
  PickStatus();
  VS_LOG_DEBUG("save.perf", "ReadFiles first_slot={} cache_hits={} total_us={}",
               picker.first_slot, cache_hits,
               std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now() - started)
                   .count());
}

bool WindowCached() {
  for (int row = 0; row < kRows; ++row) {
    if (!picker.previews[picker.first_slot + row]) return false;
  }
  return true;
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
               current.slot,
               std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now() - started)
                   .count(),
               same.has_value());
  return same;
}

std::optional<FileInfo> ReadConfirmedSelection(const FileInfo& selected) {
  // Previews are not load or overwrite consent for changed content. Inspect
  // runs Files::Load's current primary/backup and content validation, including
  // for an apparently empty container that appeared while browsing.
  const auto opened = OpenFiles();
  if (!opened) {
    Notice("Cannot open native saves", opened.error(), -1);
    return std::nullopt;
  }
  auto current = (*opened)->Inspect(selected.slot);
  if (!current) {
    Notice("Cannot read native saves", current.error());
    return std::nullopt;
  }
  const auto matches = MatchesPreview(*current, selected);
  if (!matches) {
    Notice("Cannot verify selected save", matches.error());
    return std::nullopt;
  }
  if (!*matches) {
    Notice("File changed - select again",
           current->message.empty() ? "Selected file content or status changed"
                                    : current->message);
    return std::nullopt;
  }
  return std::move(*current);
}

std::expected<void, std::string> WriteCurrent() {
  const auto opened = OpenFiles();
  if (!opened) return std::unexpected(opened.error());
  if (!picker.clear_game && !Boot_isGameplayActive) {
    return std::unexpected("No active gameplay checkpoint");
  }
  if (!picker.clear_game && !picker.container &&
      (!Field_CurrentSavePointId() || Audio_Seq_unusedStateFlags.opaqueA7)) {
    return std::unexpected("Saving is available only at an enabled save point");
  }
  const auto& selected = picker.files[picker.row];
  Items_ContainerData bank{};
  if (picker.container) {
    if (!staged_storage) {
      return std::unexpected("No staged container transaction");
    }
    bank = *staged_storage;
  } else if (selected.document) {
    bank = selected.document->state.storage;
  }
  Document document;
  if (picker.clear_game) {
    if (!staged_clear) {
      return std::unexpected("No staged original ending clear save");
    }
    document = *staged_clear;
    // F03601 TITLE 22C.c:698 preserves the source's partial container reset.
    std::fill_n(std::begin(bank.misc), kClearContainerResetStacks,
                Items_InventoryMisc{});
    document.state.storage = bank;
  } else {
    auto captured = Capture(bank);
    if (!captured) return std::unexpected(captured.error());
    document.state = std::move(*captured);
  }
  auto& state = document.state;
  state.settings.containerEmpty = BankEmpty(bank);
  if (state.settings.saveFileGeneration ==
      std::numeric_limits<std::uint32_t>::max()) {
    return std::unexpected("Save generation has reached its limit");
  }
  ++state.settings.saveFileGeneration;
  if (state.settings.cumulativeSaveCount < kSaveCountLimit) {
    ++state.settings.cumulativeSaveCount;
  }
  if (!picker.clear_game &&
      state.settings.currentGameSaveCount < kSaveCountLimit) {
    ++state.settings.currentGameSaveCount;
  }
  const auto previous = selected.document ? selected.document->generation : 0;
  if (previous == std::numeric_limits<std::uint64_t>::max()) {
    return std::unexpected("Native file generation has reached its limit");
  }
  const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  if (now < 0) return std::unexpected("System clock precedes the save epoch");
  document.generation = previous + 1;
  document.saved_unix_ms = static_cast<std::uint64_t>(now);
  const auto summary = DescribeNativeFile(document);
  if (!summary) return std::unexpected(summary.error());
  const auto consent = selected.status == FileStatus::kRecovered
                           ? VS::Save::WriteConsent::kReplaceRecovered
                           : VS::Save::WriteConsent::kNormal;
  const auto written = (*opened)->Write(selected.slot, document, consent);
  if (!written) return std::unexpected(written.error());
  Input_settings.cumulativeSaveCount =
      document.state.settings.cumulativeSaveCount;
  Input_settings.currentGameSaveCount =
      document.state.settings.currentGameSaveCount;
  Input_settings.saveFileGeneration =
      document.state.settings.saveFileGeneration;
  Input_settings.containerEmpty = document.state.settings.containerEmpty;
  last_slot = selected.slot;
  Menu_Files_dataNotSaved = 1;
  VS_LOG_INFO("save",
              "Saved native slot {} generation {} zone {} room {} scene {}",
              selected.slot, document.generation, document.state.location.zone,
              document.state.location.room, document.state.location.scene);
  picker.summaries[picker.row] = *summary;
  picker.files[picker.row] = FileInfo{.slot = last_slot,
                                      .status = FileStatus::kValid,
                                      .document = std::move(document),
                                      .message = {}};
  return {};
}

void Execute() {
  const auto& selected = picker.files[picker.row];
  VS_LOG_INFO("save", "Native {} request: slot={} status={} container={}",
              picker.select_container ? "container"
              : picker.save           ? "save"
                                      : "load",
              selected.slot, static_cast<int>(selected.status),
              picker.container);
  const auto validating = std::chrono::steady_clock::now();
  const auto current = ReadConfirmedSelection(selected);
  VS_LOG_DEBUG("save.perf", "Confirmed slot={} revalidate_us={} valid={}",
               selected.slot,
               std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now() - validating)
                   .count(),
               current.has_value());
  if (!current) return;
  if (picker.select_container && selected.status == FileStatus::kEmpty) {
    staged_storage.emplace();
    Input_settings.containerEmpty = 1;
    last_slot = selected.slot;
    picker.phase = Phase::kInactive;
    return;
  }
  if (picker.save) {
    const auto written = WriteCurrent();
    if (!written) {
      Menu_PlayInvalidSound();
      Notice("Save failed - see log", written.error());
    } else {
      Audio_PlaySfxCentered(kMenuChannelGroup, VS_SFX_FILEOPCOMPLETE);
      Notice("Save complete", {}, 1);
    }
    return;
  }
  if (!current->document) {
    Notice("Load failed - game unchanged", "Selected file has no document");
    return;
  }
  if (picker.select_container) {
    staged_storage = current->document->state.storage;
    Input_settings.containerEmpty = BankEmpty(*staged_storage);
    last_slot = selected.slot;
    picker.phase = Phase::kInactive;
    VS_LOG_INFO("save", "Opened native container in slot {} recovered {}",
                selected.slot, current->status == FileStatus::kRecovered);
    return;
  }
  const auto preparing = std::chrono::steady_clock::now();
  const auto prepared = PrepareResume(*current->document);
  VS_LOG_DEBUG("save.perf", "Prepare slot={} total_us={} valid={}",
               selected.slot,
               std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now() - preparing)
                   .count(),
               prepared.has_value());
  if (!prepared) {
    Notice("Load failed - game unchanged", prepared.error());
    return;
  }
  VS_LOG_INFO("save", "Prepared native slot {} generation {} recovered {}",
              selected.slot, current->document->generation,
              current->status == FileStatus::kRecovered);
  last_slot = selected.slot;
  picker.phase = Phase::kInactive;
}

void Begin(int init, bool save, bool title, bool select_container) {
  picker = {};
  picker.phase = Phase::kRead;
  picker.save = save;
  picker.title = title;
  picker.container = save && init == 2;
  picker.select_container = select_container;
  picker.first_slot =
      std::clamp(last_slot - 1, 0, VS::Save::SaveStore::kSlotCount - kRows);
  picker.row = last_slot - picker.first_slot;
  if (title) {
    Title_SaveFiles_ResetElements();
    Title_selectionCursorPosition = 0;
  } else {
    Menu_Files_ResetElements();
    Menu_Files_selectCursorXy = 0;
  }
  Menu_Files_dataNotSaved = 0;
  Status("Reading save files");
  if (*Save_NativeResumeError()) {
    Notice("Previous resume failed", Save_NativeResumeError(), -1);
    Save_NativeClearResumeError();
  }
}

void Confirm(std::uint32_t buttons) {
  // F02759/F03629 _promptConfirm: affirmative Circle selects; No or Cross
  // leaves. Dispatch before Execute's next frame and the C-only load jump.
  if (buttons & PADRdown) {
    Menu_PlayLeaveSound();
    picker.phase = Phase::kPick;
    PickStatus();
  } else if (buttons & PADRright) {
    if (picker.yes) {
      Menu_PlaySelectSound();
      picker.phase = Phase::kExecute;
      Status(picker.select_container ? "Opening container"
             : picker.save           ? "Saving game"
                                     : "Loading game");
    } else {
      Menu_PlayLeaveSound();
      picker.phase = Phase::kPick;
      PickStatus();
    }
  } else if (Input_buttonRepeat & (PADLup | PADLdown)) {
    picker.yes = !picker.yes;
    Menu_PlayChangeSound();
  }
}

int Pick(std::uint32_t buttons) {
  if (buttons & PADRdown) {
    picker.phase = Phase::kInactive;
    Menu_PlayLeaveSound();
    return -1;
  }
  if (Input_buttonRepeat & (PADLup | PADLdown)) {
    constexpr int kSlots = VS::Save::SaveStore::kSlotCount;
    const int delta = (Input_buttonRepeat & PADLup) ? kSlots - 1 : 1;
    const int slot = (picker.first_slot + picker.row + delta) % kSlots;
    const int first = slot < picker.first_slot            ? slot
                      : slot >= picker.first_slot + kRows ? slot - kRows + 1
                                                          : picker.first_slot;
    picker.row = slot - first;
    if (first != picker.first_slot) {
      picker.first_slot = first;
      if (WindowCached()) {
        ReadFiles();
      } else {
        picker.ready = false;
        picker.phase = Phase::kRead;
        Status("Reading save files");
      }
    } else {
      PickStatus();
    }
    Menu_PlayChangeSound();
  } else if (buttons & PADRright) {
    const auto& selected = picker.files[picker.row];
    if (selected.status == FileStatus::kInvalid ||
        (!picker.save && !picker.select_container &&
         selected.status == FileStatus::kEmpty)) {
      Menu_PlayInvalidSound();
      Notice("No usable native save", selected.message);
    } else {
      // F02765/F03626 _showLoadFilesMenu and F02762/F03632 _showSaveFilesMenu
      // play MENUSELECT on the accepted file, not just after a save succeeds.
      Menu_PlaySelectSound();
      picker.yes = false;
      picker.phase = Phase::kConfirm;
      Status(
          picker.select_container ? (selected.status == FileStatus::kRecovered
                                         ? "Use recovered container"
                                     : selected.status == FileStatus::kEmpty
                                         ? "Use empty container"
                                         : "Open selected container")
          : selected.status == FileStatus::kRecovered
              ? (picker.save ? "Replace invalid original"
                             : "Recover from valid backup")
              : (picker.save ? "Save to selected file" : "Load selected game"));
    }
  }
  return 0;
}

int Update(int init, bool save, bool title, bool select_container = false) {
  if (init) {
    Begin(init, save, title, select_container);
    return 0;
  }
  const auto buttons = Input_buttonsPressed.allButtons;
  switch (picker.phase) {
    case Phase::kRead:
      ReadFiles();
      break;
    case Phase::kNotice:
      if (buttons & (PADRright | PADRdown)) {
        Menu_PlayLeaveSound();
        const int result = picker.notice_result;
        picker.phase = result ? Phase::kInactive : Phase::kRead;
        return result;
      }
      break;
    case Phase::kExecute:
      Execute();
      return picker.phase == Phase::kInactive ? 1 : 0;
    case Phase::kConfirm:
      Confirm(buttons);
      break;
    case Phase::kPick:
      return Pick(buttons);
    case Phase::kInactive:
      break;
  }
  return 0;
}

int ClearSave(int init) {
  if (init) {
    Begin(init, true, true, false);
    picker.clear_game = true;
    if (!staged_clear) {
      Notice("No ending clear save available", ClearError(), -1);
    }
    return 0;
  }
  return Update(0, true, true);
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
  picker = {};
  staged_storage.reset();
  staged_clear.reset();
  ClearError().clear();
  last_slot = 0;
  if (Title_memoryCardMessage == footer.data()) {
    Title_memoryCardMessage = nullptr;
  }
  if (Menu_Files_memoryCardMessage == footer.data()) {
    Menu_Files_memoryCardMessage = nullptr;
  }
}

std::optional<NativeFileScreen> FileMenuPresentation() {
  if (picker.phase == Phase::kInactive) return std::nullopt;
  return NativeFileScreen{.title = picker.title,
                          .save = picker.save,
                          .select_container = picker.select_container,
                          .files = picker.ready
                                       ? std::span<const FileInfo>(picker.files)
                                       : std::span<const FileInfo>{},
                          .summaries = picker.summaries,
                          .selected_row = picker.row,
                          .confirmation = picker.phase == Phase::kConfirm,
                          .affirmative = picker.yes};
}

void ShutdownFileMenus() {
  CancelResume();
  ResetFileMenuSession();
  files.reset();
  directory.reset();
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

}  // namespace VS::Game::Save

// Boot transitions remain in the original C callers, after these frames return.
// F02764 _showSaveMenu, MENU/MENU7.PRG/260.c:2673.
extern "C" int Menu_Files_UpdateDataSaveMenu(int init_state) {
  return VS::Game::Save::Update(init_state, true, false);
}
// F02767 _loadFileMenu, MENU/MENU7.PRG/260.c:3187.
extern "C" int Menu_Files_UpdateDataLoadMenu(int init_fadeout) {
  return VS::Game::Save::Update(init_fadeout, false, false);
}
// F03628 _loadFileMenu, TITLE/TITLE.PRG/22C.c:2213.
extern "C" int Menu_Files_UpdateLoadMenu(int init_fadeout) {
  return VS::Game::Save::Update(init_fadeout, false, true);
}

// F03634 _showSaveMenu, TITLE/TITLE.PRG/22C.c:2880; clear-save gate only.
extern "C" int Menu_Files_UpdateSaveMenu(int init_state) {
  return VS::Game::Save::ClearSave(init_state);
}

extern "C" int Save_NativeSelectContainer(int init) {
  return VS::Game::Save::Update(init, false, false, true);
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
