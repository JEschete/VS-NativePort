// Hand-maintained (tools/port/owned.csv).
#include "Save/NativeContainer.hpp"

namespace VS::Menu::Files {
namespace {

void Advance(ContainerSaveState& state, ContainerSaveState next) {
  state = next;
}

void PromptDiscard(ContainerSaveState& state, ContainerSaveHost& host) {
  host.OpenInformationBox();
  host.ShowPrompt(ContainerPrompt::DiscardChanges);
  Advance(state, ContainerSaveState::ConfirmDiscard);
}

void CloseFiles(ContainerSaveState& state, ContainerSaveHost& host) {
  host.CloseFilePicker();
  Advance(state, ContainerSaveState::DismissMenu);
}

void UpdateOpening(ContainerSaveState& state, ContainerSaveHost& host) {
  switch (state) {
    case ContainerSaveState::Initialize:
      host.BeginSession();
      host.OpenInformationBox();
      host.ShowPrompt(ContainerPrompt::OpenContainer);
      Advance(state, ContainerSaveState::Confirm);
      break;
    case ContainerSaveState::Confirm:
      if (const int option = host.PollPrompt(); option > 0) {
        host.ShowContainerHeading();
        Advance(state, ContainerSaveState::LoadFiles);
      } else if (option < 0) {
        Advance(state, ContainerSaveState::ReleaseStorage);
      }
      break;
    case ContainerSaveState::LoadFiles:
      if (host.MenuRowsIdle() && host.ResourcesReady()) {
        host.OpenFilePicker();
        Advance(state, ContainerSaveState::SelectFile);
      }
      break;
    case ContainerSaveState::SelectFile:
      if (const int option = host.PollFilePicker(); option > 0) {
        Advance(state, ContainerSaveState::StageItems);
      } else if (option < 0) {
        Advance(state, ContainerSaveState::CloseFiles);
      }
      host.DrawFiles();
      break;
    default:
      break;
  }
}

void UpdateContainerScreen(ContainerSaveState& state, ContainerSaveHost& host) {
  switch (state) {
    case ContainerSaveState::StageItems:
      host.OpenContainerScreen();
      Advance(state, ContainerSaveState::WaitForScreen);
      break;
    case ContainerSaveState::WaitForScreen:
      if (host.MenuRowsIdle()) {
        host.BeginResourceLoad();
        Advance(state, ContainerSaveState::ReloadFiles);
      }
      break;
    case ContainerSaveState::ReloadFiles:
      if (!host.ResourcesReady()) break;
      switch (host.ContainerScreenOutcome()) {
        case ContainerScreenResult::Restore:
          Advance(state, ContainerSaveState::RestoreItems);
          break;
        case ContainerScreenResult::ConfirmDiscard:
          PromptDiscard(state, host);
          break;
        case ContainerScreenResult::Save:
          host.OpenInformationBox();
          Advance(state, ContainerSaveState::BeginWrite);
          break;
      }
      break;
    default:
      break;
  }
}

void UpdateWriting(ContainerSaveState& state, ContainerSaveHost& host) {
  switch (state) {
    case ContainerSaveState::BeginWrite:
      host.BeginWrite();
      Advance(state, ContainerSaveState::WaitForWrite);
      break;
    case ContainerSaveState::WaitForWrite:
      if (const int option = host.PollWrite(); option != 0) {
        if (!host.DataNotSaved() && option < 0) {
          PromptDiscard(state, host);
        } else {
          Advance(state, ContainerSaveState::CloseFiles);
        }
      }
      host.DrawFiles();
      break;
    case ContainerSaveState::ConfirmDiscard:
      if (const int option = host.PollPrompt(); option == 1) {
        Advance(state, ContainerSaveState::BeginWrite);
      } else if (option != 0) {
        Advance(state, ContainerSaveState::RestoreItems);
      }
      break;
    default:
      break;
  }
}

bool UpdateClosing(ContainerSaveState& state, ContainerSaveHost& host) {
  switch (state) {
    case ContainerSaveState::RestoreItems:
      host.RestoreInventory();
      CloseFiles(state, host);
      break;
    case ContainerSaveState::CloseFiles:
      CloseFiles(state, host);
      break;
    case ContainerSaveState::DismissMenu:
      host.DismissMenu();
      if (host.MenuRowsIdle()) {
        host.ReleaseResources();
        Advance(state, ContainerSaveState::ReleaseStorage);
      }
      break;
    case ContainerSaveState::ReleaseStorage:
      if (host.InformationBoxClosed()) {
        host.EndSession();
        Advance(state, ContainerSaveState::Initialize);
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
    case ContainerSaveState::Initialize:
    case ContainerSaveState::Confirm:
    case ContainerSaveState::LoadFiles:
    case ContainerSaveState::SelectFile:
      UpdateOpening(state, host);
      return false;
    case ContainerSaveState::StageItems:
    case ContainerSaveState::WaitForScreen:
    case ContainerSaveState::ReloadFiles:
      UpdateContainerScreen(state, host);
      return false;
    case ContainerSaveState::BeginWrite:
    case ContainerSaveState::WaitForWrite:
    case ContainerSaveState::ConfirmDiscard:
      UpdateWriting(state, host);
      return false;
    case ContainerSaveState::RestoreItems:
    case ContainerSaveState::CloseFiles:
    case ContainerSaveState::DismissMenu:
    case ContainerSaveState::ReleaseStorage:
      return UpdateClosing(state, host);
  }
  host.ReportInvalidState(static_cast<unsigned>(state));
  return false;
}

}  // namespace VS::Menu::Files
