#pragma once
// Hand-maintained (tools/port/owned.csv).
#include <cstdint>

namespace VS::Menu::Files {

// The container save submenu's states. The original's memory-card picker
// states 4..6 are replaced by the native file picker polled in kSelectFile.
enum class ContainerSaveState : std::uint8_t {
  kInitialize = 0,
  kConfirm = 1,
  kLoadFiles = 2,
  kSelectFile = 3,
  kStageItems = 7,
  kWaitForScreen = 8,
  kReloadFiles = 9,
  kBeginWrite = 10,
  kWaitForWrite = 11,
  kConfirmDiscard = 12,
  kRestoreItems = 13,
  kCloseFiles = 14,
  kDismissMenu = 15,
  kReleaseStorage = 16,
};

// What the container inventory screen's return state asks the submenu to do
// next (260.c:4071: 0 restores, 1 asks to discard, anything else saves).
enum class ContainerScreenResult : std::uint8_t {
  kRestore,
  kConfirmDiscard,
  kSave,
};

enum class ContainerPrompt : std::uint8_t {
  kOpenContainer,
  kDiscardChanges,
};

// The menu, inventory and save-file operations the container submenu drives.
// Poll* calls return the original tri-state option: 0 while pending, a
// positive choice, or a negative cancel.
class ContainerSaveHost {
 public:
  virtual ~ContainerSaveHost() = default;

  virtual void BeginSession() = 0;
  virtual void OpenInformationBox() = 0;
  virtual void ShowPrompt(ContainerPrompt prompt) = 0;
  virtual int PollPrompt() = 0;
  virtual void ShowContainerHeading() = 0;
  virtual bool MenuRowsIdle() = 0;
  virtual void BeginResourceLoad() = 0;
  virtual bool ResourcesReady() = 0;
  virtual void OpenFilePicker() = 0;
  virtual int PollFilePicker() = 0;
  virtual void DrawFiles() = 0;
  virtual void OpenContainerScreen() = 0;
  virtual ContainerScreenResult ContainerScreenOutcome() = 0;
  virtual void BeginWrite() = 0;
  virtual int PollWrite() = 0;
  virtual bool DataNotSaved() = 0;
  virtual void RestoreInventory() = 0;
  virtual void CloseFilePicker() = 0;
  virtual void DismissMenu() = 0;
  virtual void ReleaseResources() = 0;
  virtual bool InformationBoxClosed() = 0;
  virtual void EndSession() = 0;
  virtual void ReportInvalidState(unsigned state) = 0;
};

// decomp: MENU7.PRG vs_menu7_saveContainerMenu (src/MENU/MENU7.PRG/260.c:3944)
// Advances the submenu once; returns true when it has finished and reset.
bool UpdateContainerSave(ContainerSaveState& state, ContainerSaveHost& host);

}  // namespace VS::Menu::Files
