#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include "Save/SaveStore.hpp"

// The native file pickers that replace the MENU7 data screen's and the TITLE
// screen's memory-card menus: a three-card window over the native save slots,
// explicit Yes/No consent, save-point eligibility, refusal of invalid or
// empty selections and of files that changed since their preview, and the
// TITLE ending clear-save gate. The host owns the save files, their decoded
// previews, the footer text, sounds and the game state a save captures or a
// load resumes; this core reaches no menu global or PS1 address.
namespace VS::Menu::Files {
inline constexpr int kFileRows = 3;
inline constexpr int kFileSlotCount = VS::Save::SaveStore::kSlotCount;
static_assert(kFileSlotCount >= kFileRows);
// MENU7's container submenu opens the save picker with this init value.
inline constexpr int kContainerSaveInit = 2;
inline constexpr std::uint16_t kSaveCountLimit = 9999;

enum class SlotStatus : std::uint8_t { kEmpty, kValid, kRecovered, kInvalid };

// One previewed save slot.
struct FileSlot {
  int slot = 0;
  SlotStatus status = SlotStatus::kEmpty;
  bool has_document = false;
  // Why the slot is invalid or recovered; empty otherwise.
  std::string message;
};
using FileWindow = std::array<FileSlot, kFileRows>;

enum class FileMenuScreen : std::uint8_t { kData, kTitle };
enum class FileMenuPhase : std::uint8_t {
  kInactive,
  kRead,
  kPick,
  kConfirm,
  kExecute,
  kNotice,
};
// The original tri-state frame result: still running, finished with a file,
// or left (cancelled or failed).
enum class FileMenuResult : std::int8_t {
  kLeft = -1,
  kRunning = 0,
  kFinished = 1,
};
enum class FileMenuSound : std::uint8_t {
  kChange,
  kSelect,
  kLeave,
  kInvalid,
  kComplete,
};
enum class FileMenuFault : std::uint8_t {
  kOpen,
  kRead,
  kSummary,
  kVerify,
  kChanged,
};
struct FileMenuError {
  FileMenuFault fault = FileMenuFault::kOpen;
  std::string detail;
};

// One frame's buttons: accept/cancel pressed (Circle/Cross), up/down repeat.
struct FileMenuInput {
  bool accept = false;
  bool cancel = false;
  bool up = false;
  bool down = false;
};

struct FileMenu {
  FileMenuPhase phase = FileMenuPhase::kInactive;
  FileMenuScreen screen = FileMenuScreen::kData;
  bool save = false;
  // A save from the container submenu needs no enabled save point.
  bool container = false;
  bool clear_game = false;
  bool select_container = false;
  bool affirmative = false;
  // Whether `rows` holds the current window.
  bool ready = false;
  int first_slot = 0;
  int row = 0;
  FileMenuResult notice_result = FileMenuResult::kRunning;
  FileWindow rows{};
};

// The picker and the slot the next picker opens on.
struct FileMenuSession {
  FileMenu menu;
  int last_slot = 0;
};

struct SaveCounters {
  std::uint32_t generation = 0;
  std::uint16_t cumulative = 0;
  std::uint16_t current_game = 0;
  bool operator==(const SaveCounters&) const = default;
};

class FileMenuHost {
 public:
  virtual ~FileMenuHost() = default;
  // Resets the screen's elements and cursor, clears the not-saved flag and
  // forgets every preview and confirmed selection.
  virtual void BeginScreen(FileMenuScreen screen) = 0;
  virtual void ShowStatus(FileMenuScreen screen, std::string_view text) = 0;
  virtual void PlaySound(FileMenuSound sound) = 0;
  virtual void ReportError(std::string_view text, std::string_view detail) = 0;
  // The failure of the previous resume attempt, which is then forgotten.
  virtual std::optional<std::string> TakeResumeError() = 0;
  [[nodiscard]] virtual std::expected<void, std::string> ClearSaveStaged()
      const = 0;

  virtual void ForgetPreviews() = 0;
  [[nodiscard]] virtual bool WindowCached(int first_slot) const = 0;
  virtual std::expected<FileWindow, FileMenuError> ReadWindow(
      int first_slot) = 0;
  // Re-reads the selected file and compares it with its preview, keeping the
  // current file for the open, write or resume that follows.
  virtual std::expected<FileSlot, FileMenuError> ConfirmSelection(
      const FileMenu& menu) = 0;

  [[nodiscard]] virtual bool GameplayActive() const = 0;
  [[nodiscard]] virtual bool SavePointEnabled() const = 0;
  virtual std::expected<void, std::string> WriteSelection(
      const FileMenu& menu, VS::Save::WriteConsent consent) = 0;
  virtual void OpenEmptyContainer() = 0;
  virtual void OpenConfirmedContainer() = 0;
  virtual std::expected<void, std::string> ResumeConfirmedSelection() = 0;
};

// The settings counters after one save: refused once the generation is at
// its limit; the save counts stop at 9999 and a clear save leaves the
// current game's count alone.
[[nodiscard]] std::optional<SaveCounters> AdvanceSaveCounters(
    SaveCounters counters, bool clear_game);
// The next native file generation, refused at its limit.
[[nodiscard]] std::optional<std::uint64_t> NextFileGeneration(
    std::uint64_t previous);

// Each runs one frame of its picker. A nonzero init opens the picker and
// returns kRunning.
// F02764 MENU7.PRG _showSaveMenu (src/MENU/MENU7.PRG/260.c:2673)
FileMenuResult UpdateDataSaveMenu(FileMenuSession& session, FileMenuHost& host,
                                  int init_state, const FileMenuInput& input);
// F02767 MENU7.PRG _loadFileMenu (src/MENU/MENU7.PRG/260.c:3187)
FileMenuResult UpdateDataLoadMenu(FileMenuSession& session, FileMenuHost& host,
                                  int init_fadeout, const FileMenuInput& input);
// F03628 TITLE.PRG _loadFileMenu (src/TITLE/TITLE.PRG/22C.c:2213)
FileMenuResult UpdateLoadMenu(FileMenuSession& session, FileMenuHost& host,
                              int init_fadeout, const FileMenuInput& input);
// F03634 TITLE.PRG _showSaveMenu (src/TITLE/TITLE.PRG/22C.c:2880); only the
// ending clear save, refused unless the host staged one.
FileMenuResult UpdateSaveMenu(FileMenuSession& session, FileMenuHost& host,
                              int init_state, const FileMenuInput& input);
// The container submenu's file picker; an empty slot opens an empty bank.
FileMenuResult UpdateContainerMenu(FileMenuSession& session, FileMenuHost& host,
                                   int init, const FileMenuInput& input);
}  // namespace VS::Menu::Files
