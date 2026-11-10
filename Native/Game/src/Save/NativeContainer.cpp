// Hand-maintained (tools/port/owned.csv).
#include "Save/NativeContainer.hpp"

namespace VS::Menu::Files {
namespace {

void Advance(ContainerSaveState& state, ContainerSaveState next) {
  state = next;
}

void PromptDiscard(ContainerSaveState& state, ContainerSaveHost& host) {
  host.OpenInformationBox();
  host.ShowPrompt(ContainerPrompt::kDiscardChanges);
  Advance(state, ContainerSaveState::kConfirmDiscard);
}

void CloseFiles(ContainerSaveState& state, ContainerSaveHost& host) {
  host.CloseFilePicker();
  Advance(state, ContainerSaveState::kDismissMenu);
}

void UpdateOpening(ContainerSaveState& state, ContainerSaveHost& host) {
  switch (state) {
    case ContainerSaveState::kInitialize:
      host.BeginSession();
      host.OpenInformationBox();
      host.ShowPrompt(ContainerPrompt::kOpenContainer);
      Advance(state, ContainerSaveState::kConfirm);
      break;
    case ContainerSaveState::kConfirm:
      if (const int option = host.PollPrompt(); option > 0) {
        host.ShowContainerHeading();
        Advance(state, ContainerSaveState::kLoadFiles);
      } else if (option < 0) {
        Advance(state, ContainerSaveState::kReleaseStorage);
      }
      break;
    case ContainerSaveState::kLoadFiles:
      if (host.MenuRowsIdle() && host.ResourcesReady()) {
        host.OpenFilePicker();
        Advance(state, ContainerSaveState::kSelectFile);
      }
      break;
    case ContainerSaveState::kSelectFile:
      if (const int option = host.PollFilePicker(); option > 0) {
        Advance(state, ContainerSaveState::kStageItems);
      } else if (option < 0) {
        Advance(state, ContainerSaveState::kCloseFiles);
      }
      host.DrawFiles();
      break;
    default:
      break;
  }
}

void UpdateContainerScreen(ContainerSaveState& state, ContainerSaveHost& host) {
  switch (state) {
    case ContainerSaveState::kStageItems:
      host.OpenContainerScreen();
      Advance(state, ContainerSaveState::kWaitForScreen);
      break;
    case ContainerSaveState::kWaitForScreen:
      if (host.MenuRowsIdle()) {
        host.BeginResourceLoad();
        Advance(state, ContainerSaveState::kReloadFiles);
      }
      break;
    case ContainerSaveState::kReloadFiles:
      if (!host.ResourcesReady()) break;
      switch (host.ContainerScreenOutcome()) {
        case ContainerScreenResult::kRestore:
          Advance(state, ContainerSaveState::kRestoreItems);
          break;
        case ContainerScreenResult::kConfirmDiscard:
          PromptDiscard(state, host);
          break;
        case ContainerScreenResult::kSave:
          host.OpenInformationBox();
          Advance(state, ContainerSaveState::kBeginWrite);
          break;
      }
      break;
    default:
      break;
  }
}

void UpdateWriting(ContainerSaveState& state, ContainerSaveHost& host) {
  switch (state) {
    case ContainerSaveState::kBeginWrite:
      host.BeginWrite();
      Advance(state, ContainerSaveState::kWaitForWrite);
      break;
    case ContainerSaveState::kWaitForWrite:
      if (const int option = host.PollWrite(); option != 0) {
        if (!host.DataNotSaved() && option < 0) {
          PromptDiscard(state, host);
        } else {
          Advance(state, ContainerSaveState::kCloseFiles);
        }
      }
      host.DrawFiles();
      break;
    case ContainerSaveState::kConfirmDiscard:
      if (const int option = host.PollPrompt(); option == 1) {
        Advance(state, ContainerSaveState::kBeginWrite);
      } else if (option != 0) {
        Advance(state, ContainerSaveState::kRestoreItems);
      }
      break;
    default:
      break;
  }
}

bool UpdateClosing(ContainerSaveState& state, ContainerSaveHost& host) {
  switch (state) {
    case ContainerSaveState::kRestoreItems:
      host.RestoreInventory();
      CloseFiles(state, host);
      break;
    case ContainerSaveState::kCloseFiles:
      CloseFiles(state, host);
      break;
    case ContainerSaveState::kDismissMenu:
      host.DismissMenu();
      if (host.MenuRowsIdle()) {
        host.ReleaseResources();
        Advance(state, ContainerSaveState::kReleaseStorage);
      }
      break;
    case ContainerSaveState::kReleaseStorage:
      if (host.InformationBoxClosed()) {
        host.EndSession();
        Advance(state, ContainerSaveState::kInitialize);
        return true;
      }
      break;
    default:
      break;
  }
  return false;
}

}  // namespace

// decomp: MENU7.PRG vs_menu7_saveContainerMenu (src/MENU/MENU7.PRG/260.c:3944)
bool UpdateContainerSave(ContainerSaveState& state, ContainerSaveHost& host) {
  switch (state) {
    case ContainerSaveState::kInitialize:
    case ContainerSaveState::kConfirm:
    case ContainerSaveState::kLoadFiles:
    case ContainerSaveState::kSelectFile:
      UpdateOpening(state, host);
      return false;
    case ContainerSaveState::kStageItems:
    case ContainerSaveState::kWaitForScreen:
    case ContainerSaveState::kReloadFiles:
      UpdateContainerScreen(state, host);
      return false;
    case ContainerSaveState::kBeginWrite:
    case ContainerSaveState::kWaitForWrite:
    case ContainerSaveState::kConfirmDiscard:
      UpdateWriting(state, host);
      return false;
    case ContainerSaveState::kRestoreItems:
    case ContainerSaveState::kCloseFiles:
    case ContainerSaveState::kDismissMenu:
    case ContainerSaveState::kReleaseStorage:
      return UpdateClosing(state, host);
  }
  host.ReportInvalidState(static_cast<unsigned>(state));
  return false;
}

}  // namespace VS::Menu::Files
