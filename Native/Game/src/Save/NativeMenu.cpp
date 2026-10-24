#include "Save/NativeMenu.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "Core/Log.hpp"
#include "Platform/App.hpp"
#include "Save/NativeFiles.hpp"
#include "Save/NativeMenu.h"
#include "Save/NativeResume.h"
#include "Save/RuntimeSnapshot.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
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
constexpr int kRows = 5;
constexpr int kPages = VS::Save::SaveStore::kSlotCount / kRows;
constexpr int kHalfWordBits = 16;
constexpr int kDialogX = 24;
constexpr int kDialogWidth = 272;
constexpr int kDialogHeight = 16;
constexpr int kUppercaseGlyph = 10;
constexpr int kLowercaseGlyph = 36;
constexpr int kSaveCountLimit = 9999;
constexpr unsigned char kElementTerminator = 0xFF;
constexpr auto kTextBytes = sizeof(Menu_Files_FileMenuElement::text);
constexpr std::uint64_t kSecondsPerMinute = 60;
constexpr std::uint64_t kTicksPerMinute = 60 * kSecondsPerMinute;
constexpr auto kTicksPerHour = kTicksPerMinute * kSecondsPerMinute;
enum Element {
  kHeader = 0,
  kStats = kRows + 1,
  kDetail,
  kStatus,
  kHelp,
  kElementCount
};
constexpr std::array kElementY{30, 54, 74, 94, 114, 134, 150, 172, 194, 216};
static_assert(kElementY.size() == kElementCount);
enum class Phase { kInactive, kRead, kPick, kConfirm, kExecute, kNotice };

struct Picker {
  Phase phase = Phase::kInactive;
  bool title = false;
  bool save = false;
  bool container = false;
  bool select_container = false;
  bool yes = false;
  int page = 0;
  int row = 0;
  int notice_result = 0;
  std::array<FileInfo, kRows> files{};
};

Picker picker;
std::optional<std::filesystem::path> directory;
std::optional<Files> files;
std::optional<Items_ContainerData> staged_storage;
int last_slot = 0;
std::array<char, kTextBytes * 2> footer{};

void RebuildFooter() {
  auto& elements =
      picker.title ? Title_fileMenuElements : Menu_Files_fileMenuElements;
  std::size_t n = 0;
  for (const int id : {kStatus, kHelp}) {
    if (elements[id].state != Menu_Files_ElementState_Inactive) {
      for (const unsigned char c : elements[id].text) {
        if (c == kElementTerminator) break;
        footer.at(n++) = static_cast<char>(c);
      }
    }
    footer.at(n++) = static_cast<char>(
        id == kStatus ? Core_TextCode_Newline : Core_TextCode_Terminator);
  }
  if (picker.title) {
    Title_memoryCardMessage = footer.data();
  } else {
    Menu_Files_memoryCardMessage = footer.data();
  }
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

int Packed(int low, int high) { return low | (high << kHalfWordBits); }

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

void Label(int id, std::string_view text, bool selected = false) {
  if (text.size() >= kTextBytes) {
    throw std::logic_error("Native save label exceeds the game text buffer");
  }
  std::array<char, kTextBytes> encoded{};
  std::size_t n = 0;
  for (const unsigned char c : text) {
    encoded[n++] = static_cast<char>(Glyph(c));
  }
  encoded[n] = static_cast<char>(Core_TextCode_Terminator);
  const auto xy = Packed(kDialogX, kElementY.at(id));
  const auto wh = Packed(kDialogWidth, kDialogHeight);
  auto* element =
      picker.title
          ? Title_SaveFiles_InitializeElement(id, xy, wh, encoded.data())
          : Menu_Files_InitializeElement(id, xy, wh, encoded.data());
  element->selected = selected ? 1 : 0;
  if (id == kStatus || id == kHelp) RebuildFooter();
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
  picker.phase = Phase::kNotice;
  picker.notice_result = result;
  Label(kStatus, text);
  Label(kHelp, "Circle or Cross to return");
  if (!detail.empty()) {
    VS_LOG_ERROR("save", "{}: {}", text, detail);
    Label(kDetail, "Details in current game log");
  }
}

void DrawRows() {
  Label(kHeader, std::string(picker.select_container ? "BANK"
                             : picker.save           ? "SAVE"
                                                     : "LOAD") +
                     " NATIVE - PAGE " + std::to_string(picker.page + 1) + "/" +
                     std::to_string(kPages));
  for (int i = 0; i < kRows; ++i) {
    const auto& file = picker.files[i];
    std::string text = "File " + std::to_string(file.slot + 1) + " - ";
    switch (file.status) {
      case FileStatus::kEmpty:
        text += "Empty";
        break;
      case FileStatus::kInvalid:
        text += "Invalid";
        break;
      case FileStatus::kRecovered:
        text += "Backup available";
        break;
      case FileStatus::kValid:
        if (!file.document) {
          throw std::logic_error("Valid native file has no staged document");
        }
        text += "Z" + std::to_string(file.document->state.location.zone) +
                " R" + std::to_string(file.document->state.location.room);
        break;
    }
    Label(i + 1, text, i == picker.row);
  }
  const auto& current = picker.files[picker.row];
  if (current.document) {
    const auto& state = current.document->state;
    Label(kStats, "HP " + std::to_string(state.ashley.currentHp) + "/" +
                      std::to_string(state.ashley.maxHp) + " MP " +
                      std::to_string(state.ashley.currentMp));
    Label(kDetail, "Time " + std::to_string(state.play_ticks / kTicksPerHour) +
                       "h " +
                       std::to_string(state.play_ticks / kTicksPerMinute %
                                      kSecondsPerMinute) +
                       "m");
  } else {
    Label(kStats, current.status == FileStatus::kInvalid
                      ? "Invalid data - not overwritten"
                      : "No saved game");
    Label(kDetail, current.status == FileStatus::kInvalid
                       ? "Details in current game log"
                       : "Native saves only");
  }
  Label(kStatus, "Circle confirm - Cross back");
  Label(kHelp, "Up Down file - Left Right page");
}

void ReadPage() {
  const auto opened = OpenFiles();
  if (!opened) {
    Notice("Cannot open native saves", opened.error(), -1);
    return;
  }
  for (int i = 0; i < kRows; ++i) {
    const auto info = (*opened)->Inspect(picker.page * kRows + i);
    if (!info) {
      Notice("Cannot read native saves", info.error(), -1);
      return;
    }
    picker.files[i] = *info;
    if (!info->message.empty()) {
      VS_LOG_WARN("save", "Native slot {}: {}", info->slot, info->message);
    }
  }
  picker.phase = Phase::kPick;
  DrawRows();
}

std::expected<void, std::string> WriteCurrent() {
  const auto opened = OpenFiles();
  if (!opened) return std::unexpected(opened.error());
  if (!Boot_isGameplayActive) {
    return std::unexpected("No active gameplay checkpoint");
  }
  if (!picker.container &&
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
  auto state = Capture(bank);
  if (!state) return std::unexpected(state.error());
  state->settings.containerEmpty = BankEmpty(bank);
  if (state->settings.saveFileGeneration ==
      std::numeric_limits<std::uint32_t>::max()) {
    return std::unexpected("Save generation has reached its limit");
  }
  ++state->settings.saveFileGeneration;
  if (state->settings.cumulativeSaveCount < kSaveCountLimit) {
    ++state->settings.cumulativeSaveCount;
  }
  if (state->settings.currentGameSaveCount < kSaveCountLimit) {
    ++state->settings.currentGameSaveCount;
  }
  const auto previous = selected.document ? selected.document->generation : 0;
  if (previous == std::numeric_limits<std::uint64_t>::max()) {
    return std::unexpected("Native file generation has reached its limit");
  }
  const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  if (now < 0) return std::unexpected("System clock precedes the save epoch");
  Document document{previous + 1, static_cast<std::uint64_t>(now),
                    std::move(*state)};
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
      Menu_PlaySelectSound();
      Notice("Native save complete", {}, 1);
    }
    return;
  }
  const auto opened = OpenFiles();
  if (!opened) {
    Notice("Cannot open native saves", opened.error(), -1);
    return;
  }
  auto loaded = (*opened)->Load(selected.slot);
  if (!loaded) {
    Notice("Load failed - game unchanged", loaded.error());
    return;
  }
  if (loaded->recovered != (selected.status == FileStatus::kRecovered)) {
    Notice("File changed - select again",
           "Recovery status changed after confirmation");
    return;
  }
  if (picker.select_container) {
    staged_storage = loaded->document.state.storage;
    Input_settings.containerEmpty = BankEmpty(*staged_storage);
    last_slot = selected.slot;
    picker.phase = Phase::kInactive;
    VS_LOG_INFO("save", "Opened native container in slot {} recovered {}",
                selected.slot, loaded->recovered);
    return;
  }
  const auto prepared = PrepareResume(loaded->document);
  if (!prepared) {
    Notice("Load failed - game unchanged", prepared.error());
    return;
  }
  VS_LOG_INFO("save", "Prepared native slot {} generation {} recovered {}",
              selected.slot, loaded->document.generation, loaded->recovered);
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
  picker.page = last_slot / kRows;
  picker.row = last_slot % kRows;
  if (title) {
    Title_SaveFiles_ResetElements();
  } else {
    Menu_Files_ResetElements();
  }
  Menu_Files_dataNotSaved = 0;
  Label(kHeader, select_container ? "SELECT NATIVE CONTAINER"
                 : save           ? "SAVE NATIVE GAME"
                                  : "LOAD NATIVE GAME");
  Label(kStatus, "Reading native saves");
  if (*Save_NativeResumeError()) {
    Notice("Previous resume failed", Save_NativeResumeError(), -1);
    Save_NativeClearResumeError();
  }
}

void Confirm(std::uint32_t buttons) {
  if (buttons & PADRdown) {
    picker.phase = Phase::kPick;
    DrawRows();
  } else if (buttons & PADRright) {
    if (picker.yes) {
      picker.phase = Phase::kExecute;
      Label(kStatus, picker.select_container ? "Opening native container"
                     : picker.save           ? "Writing native save"
                                             : "Loading native save");
    } else {
      picker.phase = Phase::kPick;
      DrawRows();
    }
  } else if (Input_buttonRepeat & (PADLleft | PADLright)) {
    picker.yes = (Input_buttonRepeat & PADLleft) != 0;
    Label(kStatus,
          picker.yes ? "YES - Circle to confirm" : "NO - Circle to cancel");
  }
}

int Pick(std::uint32_t buttons) {
  if (buttons & PADRdown) {
    picker.phase = Phase::kInactive;
    Menu_PlayLeaveSound();
    return -1;
  }
  if (Input_buttonRepeat & (PADLleft | PADLright)) {
    picker.page =
        (picker.page + ((Input_buttonRepeat & PADLleft) ? kPages - 1 : 1)) %
        kPages;
    picker.phase = Phase::kRead;
    Menu_PlayChangeSound();
  } else if (Input_buttonRepeat & (PADLup | PADLdown)) {
    picker.row =
        (picker.row + ((Input_buttonRepeat & PADLup) ? kRows - 1 : 1)) % kRows;
    Menu_PlayChangeSound();
    DrawRows();
  } else if (buttons & PADRright) {
    const auto& selected = picker.files[picker.row];
    if (selected.status == FileStatus::kInvalid ||
        (!picker.save && !picker.select_container &&
         selected.status == FileStatus::kEmpty)) {
      Menu_PlayInvalidSound();
      Notice("No usable native save", selected.message);
    } else {
      picker.yes = false;
      picker.phase = Phase::kConfirm;
      Label(
          kDetail,
          picker.select_container ? (selected.status == FileStatus::kRecovered
                                         ? "Use recovered container"
                                     : selected.status == FileStatus::kEmpty
                                         ? "Use empty container"
                                         : "Open selected container")
          : selected.status == FileStatus::kRecovered
              ? (picker.save ? "Replace invalid original"
                             : "Recover from valid backup")
              : (picker.save ? "Save to selected file" : "Load selected game"));
      Label(kStatus, "NO - Circle to cancel");
      Label(kHelp, "Left YES - Right NO - Cross");
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
      ReadPage();
      break;
    case Phase::kNotice:
      if (buttons & (PADRright | PADRdown)) {
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
    Notice("Native clear save unavailable", {}, -1);
    VS_LOG_WARN("save",
                "Clear-save conversion is not implemented; no files changed");
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
  last_slot = 0;
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
