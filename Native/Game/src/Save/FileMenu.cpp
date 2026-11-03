// Hand-maintained (tools/port/owned.csv).
#include "Save/FileMenu.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace VS::Menu::Files {
namespace {
constexpr std::string_view kReading = "Reading save files";
constexpr std::string_view kLoadFailed = "Load failed - game unchanged";
constexpr std::string_view kSaveFailed = "Save failed - see log";

struct Request {
  FileMenuScreen screen = FileMenuScreen::kData;
  bool save = false;
  bool select_container = false;
};

// Fields in order: screen, save, select_container.
constexpr Request kDataSave{FileMenuScreen::kData, true, false};
constexpr Request kDataLoad{FileMenuScreen::kData, false, false};
constexpr Request kTitleLoad{FileMenuScreen::kTitle, false, false};
constexpr Request kTitleSave{FileMenuScreen::kTitle, true, false};
constexpr Request kContainer{FileMenuScreen::kData, false, true};

FileSlot& Selected(FileMenu& menu) {
  return menu.rows.at(static_cast<std::size_t>(menu.row));
}

std::string_view FaultText(FileMenuFault fault) {
  switch (fault) {
    case FileMenuFault::kRead:
      return "Cannot read native saves";
    case FileMenuFault::kSummary:
      return "Save summary unavailable";
    case FileMenuFault::kVerify:
      return "Cannot verify selected save";
    case FileMenuFault::kChanged:
      return "File changed - select again";
    case FileMenuFault::kOpen:
      break;
  }
  return "Cannot open native saves";
}

// Notices leave the picker or offer a retry. Neither may reuse a preview
// captured before a failed write, changed file or content/storage error.
void Notice(FileMenu& menu, FileMenuHost& host, std::string_view text,
            std::string_view detail, FileMenuResult result) {
  host.ForgetPreviews();
  menu.phase = FileMenuPhase::kNotice;
  menu.notice_result = result;
  host.ShowStatus(menu.screen, text);
  if (!detail.empty()) host.ReportError(text, detail);
}

void PickStatus(FileMenu& menu, FileMenuHost& host) {
  const auto status = Selected(menu).status;
  host.ShowStatus(menu.screen, status == SlotStatus::kRecovered
                                   ? "Backup available - select to use"
                               : status == SlotStatus::kInvalid
                                   ? "Invalid data - not overwritten"
                                   : "Select a file");
}

void ReadFiles(FileMenu& menu, FileMenuHost& host) {
  auto window = host.ReadWindow(menu.first_slot);
  if (!window) {
    Notice(menu, host, FaultText(window.error().fault), window.error().detail,
           FileMenuResult::kLeft);
    return;
  }
  menu.rows = std::move(*window);
  menu.ready = true;
  menu.phase = FileMenuPhase::kPick;
  PickStatus(menu, host);
}

void Begin(FileMenuSession& session, FileMenuHost& host, const Request& request,
           int init) {
  auto& menu = session.menu;
  menu = FileMenu{};
  menu.phase = FileMenuPhase::kRead;
  menu.screen = request.screen;
  menu.save = request.save;
  menu.container = request.save && init == kContainerSaveInit;
  menu.select_container = request.select_container;
  const int last = std::clamp(session.last_slot, 0, kFileSlotCount - 1);
  menu.first_slot = std::clamp(last - 1, 0, kFileSlotCount - kFileRows);
  menu.row = last - menu.first_slot;
  host.BeginScreen(menu.screen);
  host.ShowStatus(menu.screen, kReading);
  if (const auto error = host.TakeResumeError()) {
    Notice(menu, host, "Previous resume failed", *error, FileMenuResult::kLeft);
  }
}

std::string_view ConfirmPrompt(const FileMenu& menu, SlotStatus status) {
  if (menu.select_container) {
    return status == SlotStatus::kRecovered ? "Use recovered container"
           : status == SlotStatus::kEmpty   ? "Use empty container"
                                            : "Open selected container";
  }
  if (status == SlotStatus::kRecovered) {
    return menu.save ? "Replace invalid original" : "Recover from valid backup";
  }
  return menu.save ? "Save to selected file" : "Load selected game";
}

// F02759/F03629 _promptConfirm: affirmative Circle selects; No or Cross
// leaves. Dispatch before Execute's next frame and the C-only load jump.
void Confirm(FileMenu& menu, FileMenuHost& host, const FileMenuInput& input) {
  if (input.cancel || (input.accept && !menu.affirmative)) {
    host.PlaySound(FileMenuSound::kLeave);
    menu.phase = FileMenuPhase::kPick;
    PickStatus(menu, host);
  } else if (input.accept) {
    host.PlaySound(FileMenuSound::kSelect);
    menu.phase = FileMenuPhase::kExecute;
    host.ShowStatus(menu.screen, menu.select_container ? "Opening container"
                                 : menu.save           ? "Saving game"
                                                       : "Loading game");
  } else if (input.up || input.down) {
    menu.affirmative = !menu.affirmative;
    host.PlaySound(FileMenuSound::kChange);
  }
}

void Move(FileMenu& menu, FileMenuHost& host, bool up) {
  const int delta = up ? kFileSlotCount - 1 : 1;
  const int slot = (menu.first_slot + menu.row + delta) % kFileSlotCount;
  const int first = slot < menu.first_slot                ? slot
                    : slot >= menu.first_slot + kFileRows ? slot - kFileRows + 1
                                                          : menu.first_slot;
  menu.row = slot - first;
  if (first != menu.first_slot) {
    menu.first_slot = first;
    if (host.WindowCached(first)) {
      ReadFiles(menu, host);
    } else {
      menu.ready = false;
      menu.phase = FileMenuPhase::kRead;
      host.ShowStatus(menu.screen, kReading);
    }
  } else {
    PickStatus(menu, host);
  }
  host.PlaySound(FileMenuSound::kChange);
}

void Accept(FileMenu& menu, FileMenuHost& host) {
  const auto& selected = Selected(menu);
  if (selected.status == SlotStatus::kInvalid ||
      (!menu.save && !menu.select_container &&
       selected.status == SlotStatus::kEmpty)) {
    host.PlaySound(FileMenuSound::kInvalid);
    Notice(menu, host, "No usable native save", selected.message,
           FileMenuResult::kRunning);
    return;
  }
  // F02765/F03626 _showLoadFilesMenu and F02762/F03632 _showSaveFilesMenu
  // play MENUSELECT on the accepted file, not just after a save succeeds.
  host.PlaySound(FileMenuSound::kSelect);
  menu.affirmative = false;
  menu.phase = FileMenuPhase::kConfirm;
  host.ShowStatus(menu.screen, ConfirmPrompt(menu, selected.status));
}

FileMenuResult Pick(FileMenu& menu, FileMenuHost& host,
                    const FileMenuInput& input) {
  if (input.cancel) {
    menu.phase = FileMenuPhase::kInactive;
    host.PlaySound(FileMenuSound::kLeave);
    return FileMenuResult::kLeft;
  }
  if (input.up || input.down) {
    Move(menu, host, input.up);
  } else if (input.accept) {
    Accept(menu, host);
  }
  return FileMenuResult::kRunning;
}

void Finish(FileMenuSession& session, int slot) {
  session.last_slot = slot;
  session.menu.phase = FileMenuPhase::kInactive;
}

void RefuseSelection(FileMenu& menu, FileMenuHost& host,
                     const FileMenuError& error) {
  const auto text = FaultText(error.fault);
  if (error.fault == FileMenuFault::kOpen) {
    Notice(menu, host, text, error.detail, FileMenuResult::kLeft);
  } else if (error.fault == FileMenuFault::kChanged && error.detail.empty()) {
    Notice(menu, host, text, "Selected file content or status changed",
           FileMenuResult::kRunning);
  } else {
    Notice(menu, host, text, error.detail, FileMenuResult::kRunning);
  }
}

std::optional<std::string_view> SaveRefusal(const FileMenu& menu,
                                            const FileMenuHost& host) {
  if (menu.clear_game) return std::nullopt;
  if (!host.GameplayActive()) return "No active gameplay checkpoint";
  if (!menu.container && !host.SavePointEnabled()) {
    return "Saving is available only at an enabled save point";
  }
  return std::nullopt;
}

void SaveSelection(FileMenuSession& session, FileMenuHost& host) {
  auto& menu = session.menu;
  auto& selected = Selected(menu);
  const auto consent = selected.status == SlotStatus::kRecovered
                           ? VS::Save::WriteConsent::kReplaceRecovered
                           : VS::Save::WriteConsent::kNormal;
  const auto refusal = SaveRefusal(menu, host);
  const auto written = refusal ? std::expected<void, std::string>(
                                     std::unexpected(std::string(*refusal)))
                               : host.WriteSelection(menu, consent);
  if (!written) {
    host.PlaySound(FileMenuSound::kInvalid);
    Notice(menu, host, kSaveFailed, written.error(), FileMenuResult::kRunning);
    return;
  }
  selected = FileSlot{.slot = selected.slot,
                      .status = SlotStatus::kValid,
                      .has_document = true,
                      .message = {}};
  session.last_slot = selected.slot;
  host.PlaySound(FileMenuSound::kComplete);
  Notice(menu, host, "Save complete", {}, FileMenuResult::kFinished);
}

void Execute(FileMenuSession& session, FileMenuHost& host) {
  auto& menu = session.menu;
  const auto current = host.ConfirmSelection(menu);
  if (!current) {
    RefuseSelection(menu, host, current.error());
    return;
  }
  const auto& selected = Selected(menu);
  if (menu.select_container && selected.status == SlotStatus::kEmpty) {
    host.OpenEmptyContainer();
    Finish(session, selected.slot);
  } else if (menu.save) {
    SaveSelection(session, host);
  } else if (!current->has_document) {
    Notice(menu, host, kLoadFailed, "Selected file has no document",
           FileMenuResult::kRunning);
  } else if (menu.select_container) {
    host.OpenConfirmedContainer();
    Finish(session, selected.slot);
  } else if (const auto resumed = host.ResumeConfirmedSelection(); !resumed) {
    Notice(menu, host, kLoadFailed, resumed.error(), FileMenuResult::kRunning);
  } else {
    Finish(session, selected.slot);
  }
}

FileMenuResult Update(FileMenuSession& session, FileMenuHost& host,
                      const Request& request, int init,
                      const FileMenuInput& input) {
  auto& menu = session.menu;
  if (init != 0) {
    Begin(session, host, request, init);
    return FileMenuResult::kRunning;
  }
  switch (menu.phase) {
    case FileMenuPhase::kRead:
      ReadFiles(menu, host);
      break;
    case FileMenuPhase::kNotice:
      if (input.accept || input.cancel) {
        host.PlaySound(FileMenuSound::kLeave);
        const auto result = menu.notice_result;
        menu.phase = result == FileMenuResult::kRunning
                         ? FileMenuPhase::kRead
                         : FileMenuPhase::kInactive;
        return result;
      }
      break;
    case FileMenuPhase::kExecute:
      Execute(session, host);
      return menu.phase == FileMenuPhase::kInactive ? FileMenuResult::kFinished
                                                    : FileMenuResult::kRunning;
    case FileMenuPhase::kConfirm:
      Confirm(menu, host, input);
      break;
    case FileMenuPhase::kPick:
      return Pick(menu, host, input);
    case FileMenuPhase::kInactive:
      break;
  }
  return FileMenuResult::kRunning;
}
}  // namespace

std::optional<SaveCounters> AdvanceSaveCounters(SaveCounters counters,
                                                bool clear_game) {
  if (counters.generation == std::numeric_limits<std::uint32_t>::max()) {
    return std::nullopt;
  }
  ++counters.generation;
  if (counters.cumulative < kSaveCountLimit) ++counters.cumulative;
  if (!clear_game && counters.current_game < kSaveCountLimit) {
    ++counters.current_game;
  }
  return counters;
}

std::optional<std::uint64_t> NextFileGeneration(std::uint64_t previous) {
  if (previous == std::numeric_limits<std::uint64_t>::max()) {
    return std::nullopt;
  }
  return previous + 1;
}

// F02764 MENU7.PRG _showSaveMenu (src/MENU/MENU7.PRG/260.c:2673)
FileMenuResult UpdateDataSaveMenu(FileMenuSession& session, FileMenuHost& host,
                                  int init_state, const FileMenuInput& input) {
  return Update(session, host, kDataSave, init_state, input);
}

// F02767 MENU7.PRG _loadFileMenu (src/MENU/MENU7.PRG/260.c:3187)
FileMenuResult UpdateDataLoadMenu(FileMenuSession& session, FileMenuHost& host,
                                  int init_fadeout,
                                  const FileMenuInput& input) {
  return Update(session, host, kDataLoad, init_fadeout, input);
}

// F03628 TITLE.PRG _loadFileMenu (src/TITLE/TITLE.PRG/22C.c:2213)
FileMenuResult UpdateLoadMenu(FileMenuSession& session, FileMenuHost& host,
                              int init_fadeout, const FileMenuInput& input) {
  return Update(session, host, kTitleLoad, init_fadeout, input);
}

// F03634 TITLE.PRG _showSaveMenu (src/TITLE/TITLE.PRG/22C.c:2880)
FileMenuResult UpdateSaveMenu(FileMenuSession& session, FileMenuHost& host,
                              int init_state, const FileMenuInput& input) {
  if (init_state == 0) return Update(session, host, kTitleSave, 0, input);
  Begin(session, host, kTitleSave, init_state);
  session.menu.clear_game = true;
  if (const auto staged = host.ClearSaveStaged(); !staged) {
    Notice(session.menu, host, "No ending clear save available", staged.error(),
           FileMenuResult::kLeft);
  }
  return FileMenuResult::kRunning;
}

FileMenuResult UpdateContainerMenu(FileMenuSession& session, FileMenuHost& host,
                                   int init, const FileMenuInput& input) {
  return Update(session, host, kContainer, init, input);
}
}  // namespace VS::Menu::Files
