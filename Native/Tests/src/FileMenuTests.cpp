#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Save/FileMenu.hpp"

namespace {
namespace file_menu = VS::Menu::Files;
using file_menu::FileMenuFault;
using file_menu::FileMenuInput;
using file_menu::FileMenuPhase;
using file_menu::FileMenuResult;
using file_menu::FileMenuSound;
using file_menu::SlotStatus;
using VS::Save::WriteConsent;

// Fields in order: accept, cancel, up, down.
constexpr FileMenuInput kAccept{true, false, false, false};
constexpr FileMenuInput kCancel{false, true, false, false};
constexpr FileMenuInput kUp{false, false, true, false};
constexpr FileMenuInput kDown{false, false, false, true};
constexpr FileMenuInput kUpAndDown{false, false, true, true};
constexpr FileMenuInput kNone{};

class FakeFileHost final : public file_menu::FileMenuHost {
 public:
  FakeFileHost() {
    for (std::size_t i = 0; i < slots.size(); ++i) {
      slots.at(i).slot = static_cast<int>(i);
    }
  }

  std::array<file_menu::FileSlot, file_menu::kFileSlotCount> slots{};
  std::vector<std::string> statuses;
  std::vector<FileMenuSound> sounds;
  std::vector<std::pair<std::string, std::string>> errors;
  std::optional<std::string> resume_error;
  std::optional<std::string> clear_error;
  bool cached = false;
  std::optional<file_menu::FileMenuError> read_error;
  std::optional<file_menu::FileMenuError> confirm_error;
  bool gameplay = true;
  bool save_point = true;
  std::optional<std::string> write_error;
  std::optional<std::string> resume_failure;
  std::vector<WriteConsent> writes;
  std::vector<int> windows;
  int begins = 0;
  int forgets = 0;
  int empty_opens = 0;
  int opens = 0;
  int resumes = 0;

  file_menu::FileSlot& Slot(int slot) {
    return slots.at(static_cast<std::size_t>(slot));
  }

  void BeginScreen(file_menu::FileMenuScreen) override { ++begins; }
  void ShowStatus(file_menu::FileMenuScreen, std::string_view text) override {
    statuses.emplace_back(text);
  }
  void PlaySound(FileMenuSound sound) override { sounds.push_back(sound); }
  void ReportError(std::string_view text, std::string_view detail) override {
    errors.emplace_back(text, detail);
  }
  std::optional<std::string> TakeResumeError() override {
    return std::exchange(resume_error, std::nullopt);
  }
  [[nodiscard]] std::expected<void, std::string> ClearSaveStaged()
      const override {
    if (clear_error) return std::unexpected(*clear_error);
    return {};
  }
  void ForgetPreviews() override { ++forgets; }
  [[nodiscard]] bool WindowCached(int) const override { return cached; }
  std::expected<file_menu::FileWindow, file_menu::FileMenuError> ReadWindow(
      int first_slot) override {
    windows.push_back(first_slot);
    if (read_error) return std::unexpected(*read_error);
    return file_menu::FileWindow{Slot(first_slot), Slot(first_slot + 1),
                                 Slot(first_slot + 2)};
  }
  std::expected<file_menu::FileSlot, file_menu::FileMenuError> ConfirmSelection(
      const file_menu::FileMenu& menu) override {
    if (confirm_error) return std::unexpected(*confirm_error);
    return Slot(menu.first_slot + menu.row);
  }
  [[nodiscard]] bool GameplayActive() const override { return gameplay; }
  [[nodiscard]] bool SavePointEnabled() const override { return save_point; }
  std::expected<void, std::string> WriteSelection(
      const file_menu::FileMenu&, WriteConsent consent) override {
    writes.push_back(consent);
    if (write_error) return std::unexpected(*write_error);
    return {};
  }
  void OpenEmptyContainer() override { ++empty_opens; }
  void OpenConfirmedContainer() override { ++opens; }
  std::expected<void, std::string> ResumeConfirmedSelection() override {
    ++resumes;
    if (resume_failure) return std::unexpected(*resume_failure);
    return {};
  }
};

using Entry = FileMenuResult (*)(file_menu::FileMenuSession&,
                                 file_menu::FileMenuHost&, int,
                                 const FileMenuInput&);

struct Picker {
  FakeFileHost host;
  file_menu::FileMenuSession session;

  FileMenuResult Frame(Entry entry, const FileMenuInput& input = kNone) {
    return entry(session, host, 0, input);
  }
  void Open(Entry entry, int init = 1) {
    REQUIRE(entry(session, host, init, kNone) == FileMenuResult::kRunning);
    REQUIRE(Frame(entry) == FileMenuResult::kRunning);
  }
  void Store(int slot, SlotStatus status, std::string message = {}) {
    auto& stored = host.Slot(slot);
    stored.status = status;
    stored.has_document =
        status == SlotStatus::kValid || status == SlotStatus::kRecovered;
    stored.message = std::move(message);
  }
  // Accepts the selected file and moves the consent cursor to Yes.
  void ChooseYes(Entry entry) {
    REQUIRE(Frame(entry, kAccept) == FileMenuResult::kRunning);
    REQUIRE(session.menu.phase == FileMenuPhase::kConfirm);
    REQUIRE(Frame(entry, kDown) == FileMenuResult::kRunning);
    REQUIRE(Frame(entry, kAccept) == FileMenuResult::kRunning);
    REQUIRE(session.menu.phase == FileMenuPhase::kExecute);
  }
};
}  // namespace

TEST_CASE("File pickers open on the window around the last chosen slot",
          "[save-menu][file-menu]") {
  const auto [last, first, row] = GENERATE(
      std::tuple{0, 0, 0}, std::tuple{1, 0, 1}, std::tuple{50, 49, 1},
      std::tuple{99, 97, 2}, std::tuple{500, 97, 2}, std::tuple{-4, 0, 0});
  Picker picker;
  picker.session.last_slot = last;
  picker.Open(file_menu::UpdateDataLoadMenu);
  CHECK(picker.host.begins == 1);
  CHECK(picker.session.menu.first_slot == first);
  CHECK(picker.session.menu.row == row);
  CHECK(picker.session.menu.ready);
  CHECK(picker.session.menu.phase == FileMenuPhase::kPick);
  CHECK(picker.host.windows == std::vector<int>{first});
  CHECK(picker.host.statuses ==
        std::vector<std::string>{"Reading save files", "Select a file"});
}

TEST_CASE("A failed resume is reported once and leaves the picker",
          "[save-menu][file-menu]") {
  Picker picker;
  picker.host.resume_error = "Missing room";
  REQUIRE(file_menu::UpdateLoadMenu(picker.session, picker.host, 1, kNone) ==
          FileMenuResult::kRunning);
  CHECK(picker.session.menu.phase == FileMenuPhase::kNotice);
  CHECK(picker.host.errors.at(0) ==
        std::pair<std::string, std::string>{"Previous resume failed",
                                            "Missing room"});
  CHECK(picker.Frame(file_menu::UpdateLoadMenu) == FileMenuResult::kRunning);
  CHECK(picker.Frame(file_menu::UpdateLoadMenu, kCancel) ==
        FileMenuResult::kLeft);
  CHECK(picker.session.menu.phase == FileMenuPhase::kInactive);
  CHECK(picker.host.sounds == std::vector{FileMenuSound::kLeave});
}

TEST_CASE("Storage failures while reading the window leave the picker",
          "[save-menu][file-menu]") {
  const auto [fault, text] =
      GENERATE(std::pair{FileMenuFault::kOpen, "Cannot open native saves"},
               std::pair{FileMenuFault::kRead, "Cannot read native saves"},
               std::pair{FileMenuFault::kSummary, "Save summary unavailable"});
  Picker picker;
  picker.host.read_error = file_menu::FileMenuError{fault, "disk"};
  REQUIRE(file_menu::UpdateDataSaveMenu(picker.session, picker.host, 1,
                                        kNone) == FileMenuResult::kRunning);
  CHECK(picker.Frame(file_menu::UpdateDataSaveMenu) ==
        FileMenuResult::kRunning);
  CHECK(picker.session.menu.phase == FileMenuPhase::kNotice);
  CHECK(picker.host.statuses.back() == text);
  CHECK(picker.host.forgets == 1);
  CHECK(picker.Frame(file_menu::UpdateDataSaveMenu, kAccept) ==
        FileMenuResult::kLeft);
}

namespace {
// Up from the first slot wraps to the last window and rereads it.
void CheckWrapToLastWindow(Picker& picker) {
  CHECK(picker.Frame(file_menu::UpdateLoadMenu, kUp) ==
        FileMenuResult::kRunning);
  CHECK(picker.session.menu.first_slot == 97);
  CHECK(picker.session.menu.row == 2);
  CHECK_FALSE(picker.session.menu.ready);
  CHECK(picker.session.menu.phase == FileMenuPhase::kRead);
  CHECK(picker.host.statuses.back() == "Reading save files");
  CHECK(picker.host.sounds.back() == FileMenuSound::kChange);
  CHECK(picker.Frame(file_menu::UpdateLoadMenu) == FileMenuResult::kRunning);
  CHECK(picker.host.windows == std::vector{0, 97});
}

// Rows move inside a cached window without rereading it; Up wins when both
// repeat.
void CheckRowsMoveInsideWindow(Picker& picker) {
  CHECK(picker.Frame(file_menu::UpdateLoadMenu, kDown) ==
        FileMenuResult::kRunning);
  CHECK(picker.session.menu.row == 1);
  CHECK(picker.host.windows.size() == 3);
  CHECK(picker.Frame(file_menu::UpdateLoadMenu, kUpAndDown) ==
        FileMenuResult::kRunning);
  CHECK(picker.session.menu.row == 0);
}
}  // namespace

TEST_CASE("Pickers wrap through all slots and reread uncached windows",
          "[save-menu][file-menu]") {
  Picker picker;
  picker.Open(file_menu::UpdateLoadMenu);
  CheckWrapToLastWindow(picker);

  picker.host.cached = true;
  CHECK(picker.Frame(file_menu::UpdateLoadMenu, kDown) ==
        FileMenuResult::kRunning);
  CHECK(picker.session.menu.first_slot == 0);
  CHECK(picker.session.menu.row == 0);
  CHECK(picker.session.menu.phase == FileMenuPhase::kPick);
  CHECK(picker.host.windows == std::vector{0, 97, 0});
  CheckRowsMoveInsideWindow(picker);

  CHECK(picker.Frame(file_menu::UpdateLoadMenu, kCancel) ==
        FileMenuResult::kLeft);
  CHECK(picker.session.menu.phase == FileMenuPhase::kInactive);
}

namespace {
// Dismisses the first refusal: a load notice rereads the window and a save
// confirmation is cancelled.
void DismissFirstRefusal(Picker& picker, Entry entry, bool save) {
  if (!save) {
    CHECK(picker.host.statuses.back() == "No usable native save");
    CHECK(picker.Frame(entry, kAccept) == FileMenuResult::kRunning);
    CHECK(picker.session.menu.phase == FileMenuPhase::kRead);
    CHECK(picker.Frame(entry) == FileMenuResult::kRunning);
  } else {
    CHECK(picker.host.statuses.back() == "Save to selected file");
    CHECK(picker.Frame(entry, kCancel) == FileMenuResult::kRunning);
  }
}
}  // namespace

TEST_CASE("Loads refuse empty and invalid slots and saves refuse invalid ones",
          "[save-menu][file-menu]") {
  const bool save = GENERATE(false, true);
  const auto entry =
      save ? file_menu::UpdateDataSaveMenu : file_menu::UpdateDataLoadMenu;
  Picker picker;
  picker.Store(1, SlotStatus::kInvalid, "Bad checksum");
  picker.Open(entry);

  CHECK(picker.Frame(entry, kAccept) == FileMenuResult::kRunning);
  CHECK(picker.session.menu.phase ==
        (save ? FileMenuPhase::kConfirm : FileMenuPhase::kNotice));
  CHECK(picker.host.sounds.back() ==
        (save ? FileMenuSound::kSelect : FileMenuSound::kInvalid));
  DismissFirstRefusal(picker, entry, save);
  CHECK(picker.Frame(entry, kDown) == FileMenuResult::kRunning);
  CHECK(picker.host.statuses.back() == "Invalid data - not overwritten");
  CHECK(picker.Frame(entry, kAccept) == FileMenuResult::kRunning);
  CHECK(picker.session.menu.phase == FileMenuPhase::kNotice);
  CHECK(picker.host.errors.back() ==
        std::pair<std::string, std::string>{"No usable native save",
                                            "Bad checksum"});
  CHECK(picker.host.writes.empty());
}

namespace {
// Consent opens on No; accepting No returns to the picker.
void CheckConsentDefaultsToNo(Picker& picker) {
  CHECK(picker.host.statuses.back() == "Backup available - select to use");
  CHECK(picker.Frame(file_menu::UpdateLoadMenu, kAccept) ==
        FileMenuResult::kRunning);
  CHECK(picker.host.statuses.back() == "Recover from valid backup");
  CHECK_FALSE(picker.session.menu.affirmative);
  CHECK(picker.Frame(file_menu::UpdateLoadMenu, kAccept) ==
        FileMenuResult::kRunning);
  CHECK(picker.session.menu.phase == FileMenuPhase::kPick);
  CHECK(picker.host.sounds.back() == FileMenuSound::kLeave);
}

// Up and Down toggle the answer and Cancel leaves without resuming.
void CheckConsentTogglesAndCancels(Picker& picker) {
  CHECK(picker.Frame(file_menu::UpdateLoadMenu, kAccept) ==
        FileMenuResult::kRunning);
  CHECK(picker.Frame(file_menu::UpdateLoadMenu, kUp) ==
        FileMenuResult::kRunning);
  CHECK(picker.session.menu.affirmative);
  CHECK(picker.Frame(file_menu::UpdateLoadMenu, kDown) ==
        FileMenuResult::kRunning);
  CHECK_FALSE(picker.session.menu.affirmative);
  CHECK(picker.Frame(file_menu::UpdateLoadMenu, kCancel) ==
        FileMenuResult::kRunning);
  CHECK(picker.session.menu.phase == FileMenuPhase::kPick);
  CHECK(picker.host.resumes == 0);
}
}  // namespace

TEST_CASE("Consent defaults to No, toggles and can be cancelled",
          "[save-menu][file-menu]") {
  Picker picker;
  picker.Store(0, SlotStatus::kRecovered);
  picker.Open(file_menu::UpdateLoadMenu);
  CheckConsentDefaultsToNo(picker);
  CheckConsentTogglesAndCancels(picker);

  picker.ChooseYes(file_menu::UpdateLoadMenu);
  CHECK(picker.host.statuses.back() == "Loading game");
  CHECK(picker.Frame(file_menu::UpdateLoadMenu) == FileMenuResult::kFinished);
  CHECK(picker.host.resumes == 1);
  CHECK(picker.session.menu.phase == FileMenuPhase::kInactive);
}

TEST_CASE("Saves need active gameplay and an enabled save point",
          "[save-menu][file-menu]") {
  const auto [gameplay, save_point, init, detail] =
      GENERATE(std::tuple{false, true, 1, "No active gameplay checkpoint"},
               std::tuple{false, true, 2, "No active gameplay checkpoint"},
               std::tuple{true, false, 1,
                          "Saving is available only at an enabled save point"});
  Picker picker;
  picker.host.gameplay = gameplay;
  picker.host.save_point = save_point;
  picker.Open(file_menu::UpdateDataSaveMenu, init);
  picker.ChooseYes(file_menu::UpdateDataSaveMenu);
  CHECK(picker.Frame(file_menu::UpdateDataSaveMenu) ==
        FileMenuResult::kRunning);
  CHECK(picker.host.writes.empty());
  CHECK(picker.host.sounds.back() == FileMenuSound::kInvalid);
  CHECK(picker.host.statuses.back() == "Save failed - see log");
  CHECK(picker.host.errors.back() ==
        std::pair<std::string, std::string>{"Save failed - see log", detail});
  CHECK(picker.Frame(file_menu::UpdateDataSaveMenu, kAccept) ==
        FileMenuResult::kRunning);
  CHECK(picker.session.menu.phase == FileMenuPhase::kRead);
}

namespace {
// A container save on a recovered file asks for replacement consent first.
void ConfirmRecoveredReplacement(Picker& picker) {
  CHECK(picker.session.menu.container);
  CHECK(picker.Frame(file_menu::UpdateDataSaveMenu, kAccept) ==
        FileMenuResult::kRunning);
  CHECK(picker.host.statuses.back() == "Replace invalid original");
  CHECK(picker.Frame(file_menu::UpdateDataSaveMenu, kUp) ==
        FileMenuResult::kRunning);
  CHECK(picker.Frame(file_menu::UpdateDataSaveMenu, kAccept) ==
        FileMenuResult::kRunning);
  CHECK(picker.host.statuses.back() == "Saving game");
}

void CheckSavedRow(const Picker& picker, int slot) {
  const auto& row = picker.session.menu.rows[picker.session.menu.row];
  CHECK(row.status == SlotStatus::kValid);
  CHECK(row.has_document);
  CHECK(picker.session.last_slot == slot);
}
}  // namespace

TEST_CASE(
    "Container saves need no save point and recovered files need "
    "replacement consent",
    "[save-menu][file-menu]") {
  Picker picker;
  picker.host.save_point = false;
  picker.session.last_slot = 42;
  picker.Store(42, SlotStatus::kRecovered);
  picker.Open(file_menu::UpdateDataSaveMenu, file_menu::kContainerSaveInit);
  ConfirmRecoveredReplacement(picker);
  CHECK(picker.Frame(file_menu::UpdateDataSaveMenu) ==
        FileMenuResult::kRunning);
  CHECK(picker.host.writes == std::vector{WriteConsent::kReplaceRecovered});
  CHECK(picker.host.sounds.back() == FileMenuSound::kComplete);
  CHECK(picker.host.statuses.back() == "Save complete");
  CHECK(picker.host.errors.empty());
  CheckSavedRow(picker, 42);
  CHECK(picker.Frame(file_menu::UpdateDataSaveMenu, kCancel) ==
        FileMenuResult::kFinished);
  CHECK(picker.session.menu.phase == FileMenuPhase::kInactive);
}

TEST_CASE("Failed writes keep the file and offer a retry",
          "[save-menu][file-menu]") {
  Picker picker;
  picker.host.write_error = "Disk full";
  picker.session.last_slot = 5;
  picker.Open(file_menu::UpdateDataSaveMenu);
  picker.ChooseYes(file_menu::UpdateDataSaveMenu);
  CHECK(picker.Frame(file_menu::UpdateDataSaveMenu) ==
        FileMenuResult::kRunning);
  CHECK(picker.host.writes == std::vector{WriteConsent::kNormal});
  CHECK(picker.session.menu.rows[1].status == SlotStatus::kEmpty);
  CHECK_FALSE(picker.session.menu.rows[1].has_document);
  CHECK(picker.host.errors.back().second == "Disk full");
  CHECK(picker.session.last_slot == 5);
  CHECK(picker.Frame(file_menu::UpdateDataSaveMenu, kAccept) ==
        FileMenuResult::kRunning);
  CHECK(picker.session.menu.phase == FileMenuPhase::kRead);
}

TEST_CASE("Changed or unreadable selections return to the picker",
          "[save-menu][file-menu]") {
  const auto [fault, detail, text, shown, result] = GENERATE(
      std::tuple{FileMenuFault::kChanged, "",
                 "File changed - select "
                 "again",
                 "Selected file content or status changed",
                 FileMenuResult::kRunning},
      std::tuple{FileMenuFault::kChanged, "Backup used",
                 "File changed - select again", "Backup used",
                 FileMenuResult::kRunning},
      std::tuple{FileMenuFault::kRead, "io", "Cannot read native saves", "io",
                 FileMenuResult::kRunning},
      std::tuple{FileMenuFault::kVerify, "diff", "Cannot verify selected save",
                 "diff", FileMenuResult::kRunning},
      std::tuple{FileMenuFault::kOpen, "gone", "Cannot open native saves",
                 "gone", FileMenuResult::kLeft});
  Picker picker;
  picker.Store(0, SlotStatus::kValid);
  picker.Open(file_menu::UpdateDataLoadMenu);
  picker.ChooseYes(file_menu::UpdateDataLoadMenu);
  picker.host.confirm_error = file_menu::FileMenuError{fault, detail};
  CHECK(picker.Frame(file_menu::UpdateDataLoadMenu) ==
        FileMenuResult::kRunning);
  CHECK(picker.host.resumes == 0);
  CHECK(picker.host.statuses.back() == text);
  CHECK(picker.host.errors.back() ==
        std::pair<std::string, std::string>{text, shown});
  CHECK(picker.Frame(file_menu::UpdateDataLoadMenu, kAccept) == result);
}

TEST_CASE("Loads without a document or with a failed resume change nothing",
          "[save-menu][file-menu]") {
  Picker picker;
  picker.Store(0, SlotStatus::kValid);
  picker.Open(file_menu::UpdateDataLoadMenu);
  picker.ChooseYes(file_menu::UpdateDataLoadMenu);
  picker.host.Slot(0).has_document = false;
  CHECK(picker.Frame(file_menu::UpdateDataLoadMenu) ==
        FileMenuResult::kRunning);
  CHECK(picker.host.errors.back() ==
        std::pair<std::string, std::string>{"Load failed - game unchanged",
                                            "Selected file has no document"});
  CHECK(picker.host.resumes == 0);

  picker.host.Slot(0).has_document = true;
  picker.host.resume_failure = "Unknown zone";
  CHECK(picker.Frame(file_menu::UpdateDataLoadMenu, kAccept) ==
        FileMenuResult::kRunning);
  CHECK(picker.Frame(file_menu::UpdateDataLoadMenu) ==
        FileMenuResult::kRunning);
  picker.ChooseYes(file_menu::UpdateDataLoadMenu);
  CHECK(picker.Frame(file_menu::UpdateDataLoadMenu) ==
        FileMenuResult::kRunning);
  CHECK(picker.host.resumes == 1);
  CHECK(picker.host.errors.back().second == "Unknown zone");
  CHECK(picker.session.menu.phase == FileMenuPhase::kNotice);
}

namespace {
// Opening a container neither resumes nor writes and keeps the chosen slot.
void CheckContainerOpened(const Picker& picker, bool empty) {
  CHECK(picker.host.empty_opens == (empty ? 1 : 0));
  CHECK(picker.host.opens == (empty ? 0 : 1));
  CHECK(picker.host.resumes == 0);
  CHECK(picker.host.writes.empty());
  CHECK(picker.session.last_slot == 7);
}
}  // namespace

TEST_CASE("Container pickers open empty banks and confirmed containers",
          "[save-menu][file-menu]") {
  const bool empty = GENERATE(false, true);
  Picker picker;
  picker.session.last_slot = 7;
  if (!empty) picker.Store(7, SlotStatus::kValid);
  picker.Open(file_menu::UpdateContainerMenu);
  CHECK(picker.Frame(file_menu::UpdateContainerMenu, kAccept) ==
        FileMenuResult::kRunning);
  CHECK(picker.host.statuses.back() ==
        (empty ? "Use empty container" : "Open selected container"));
  CHECK(picker.Frame(file_menu::UpdateContainerMenu, kUp) ==
        FileMenuResult::kRunning);
  CHECK(picker.Frame(file_menu::UpdateContainerMenu, kAccept) ==
        FileMenuResult::kRunning);
  CHECK(picker.host.statuses.back() == "Opening container");
  CHECK(picker.Frame(file_menu::UpdateContainerMenu) ==
        FileMenuResult::kFinished);
  CheckContainerOpened(picker, empty);
}

TEST_CASE("The ending clear save is refused unless one is staged",
          "[save-menu][file-menu][save-clear]") {
  Picker picker;
  picker.host.clear_error = "Ending not reached";
  picker.host.gameplay = false;
  picker.host.save_point = false;
  REQUIRE(file_menu::UpdateSaveMenu(picker.session, picker.host, 1, kNone) ==
          FileMenuResult::kRunning);
  CHECK(picker.session.menu.clear_game);
  CHECK(picker.session.menu.phase == FileMenuPhase::kNotice);
  CHECK(picker.host.errors.back() ==
        std::pair<std::string, std::string>{"No ending clear save available",
                                            "Ending not reached"});
  CHECK(picker.Frame(file_menu::UpdateSaveMenu, kAccept) ==
        FileMenuResult::kLeft);

  picker.host.clear_error.reset();
  picker.Open(file_menu::UpdateSaveMenu);
  picker.ChooseYes(file_menu::UpdateSaveMenu);
  CHECK(picker.Frame(file_menu::UpdateSaveMenu) == FileMenuResult::kRunning);
  CHECK(picker.host.writes == std::vector{WriteConsent::kNormal});
  CHECK(picker.host.statuses.back() == "Save complete");
}

TEST_CASE("Save counters stop at 9999 and refuse an exhausted generation",
          "[save-menu][file-menu]") {
  using file_menu::SaveCounters;
  CHECK(file_menu::AdvanceSaveCounters({1, 2, 3}, false) ==
        SaveCounters{2, 3, 4});
  CHECK(file_menu::AdvanceSaveCounters({1, 2, 3}, true) ==
        SaveCounters{2, 3, 3});
  CHECK(file_menu::AdvanceSaveCounters({7, 9999, 9999}, false) ==
        SaveCounters{8, 9999, 9999});
  CHECK_FALSE(file_menu::AdvanceSaveCounters(
                  {std::numeric_limits<std::uint32_t>::max(), 0, 0}, false)
                  .has_value());
  CHECK(file_menu::NextFileGeneration(0) == 1U);
  CHECK_FALSE(
      file_menu::NextFileGeneration(std::numeric_limits<std::uint64_t>::max())
          .has_value());
}
