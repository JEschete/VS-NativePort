#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Save/NativeContainer.hpp"

namespace {

using VS::Menu::Files::ContainerPrompt;
using VS::Menu::Files::ContainerSaveHost;
using VS::Menu::Files::ContainerSaveState;
using VS::Menu::Files::ContainerScreenResult;
using VS::Menu::Files::UpdateContainerSave;
using Calls = std::vector<std::string>;

class RecordingHost final : public ContainerSaveHost {
 public:
  Calls calls;
  int prompt_option = 0;
  int picker_option = 0;
  int write_option = 0;
  bool rows_idle = true;
  bool resources_ready = true;
  bool data_not_saved = false;
  bool information_box_closed = true;
  ContainerScreenResult screen = ContainerScreenResult::kSave;
  unsigned reported = 0;

  void BeginSession() override { calls.emplace_back("BeginSession"); }
  void OpenInformationBox() override {
    calls.emplace_back("OpenInformationBox");
  }
  void ShowPrompt(ContainerPrompt prompt) override {
    calls.emplace_back(prompt == ContainerPrompt::kOpenContainer
                           ? "ShowPrompt:Open"
                           : "ShowPrompt:Discard");
  }
  int PollPrompt() override {
    calls.emplace_back("PollPrompt");
    return prompt_option;
  }
  void ShowContainerHeading() override {
    calls.emplace_back("ShowContainerHeading");
  }
  bool MenuRowsIdle() override {
    calls.emplace_back("MenuRowsIdle");
    return rows_idle;
  }
  void BeginResourceLoad() override { calls.emplace_back("BeginResourceLoad"); }
  bool ResourcesReady() override {
    calls.emplace_back("ResourcesReady");
    return resources_ready;
  }
  void OpenFilePicker() override { calls.emplace_back("OpenFilePicker"); }
  int PollFilePicker() override {
    calls.emplace_back("PollFilePicker");
    return picker_option;
  }
  void DrawFiles() override { calls.emplace_back("DrawFiles"); }
  void OpenContainerScreen() override {
    calls.emplace_back("OpenContainerScreen");
  }
  ContainerScreenResult ContainerScreenOutcome() override {
    calls.emplace_back("ContainerScreenOutcome");
    return screen;
  }
  void BeginWrite() override { calls.emplace_back("BeginWrite"); }
  int PollWrite() override {
    calls.emplace_back("PollWrite");
    return write_option;
  }
  bool DataNotSaved() override {
    calls.emplace_back("DataNotSaved");
    return data_not_saved;
  }
  void RestoreInventory() override { calls.emplace_back("RestoreInventory"); }
  void CloseFilePicker() override { calls.emplace_back("CloseFilePicker"); }
  void DismissMenu() override { calls.emplace_back("DismissMenu"); }
  void ReleaseResources() override { calls.emplace_back("ReleaseResources"); }
  bool InformationBoxClosed() override {
    calls.emplace_back("InformationBoxClosed");
    return information_box_closed;
  }
  void EndSession() override { calls.emplace_back("EndSession"); }
  void ReportInvalidState(unsigned state) override {
    calls.emplace_back("ReportInvalidState");
    reported = state;
  }
};

struct Step {
  ContainerSaveState state;
  bool finished;
};

Step Run(ContainerSaveState state, RecordingHost& host) {
  host.calls.clear();
  const bool finished = UpdateContainerSave(state, host);
  return {state, finished};
}

}  // namespace

TEST_CASE("Container save opens a session behind the opening prompt",
          "[container-save]") {
  RecordingHost host;
  const Step step = Run(ContainerSaveState::kInitialize, host);
  CHECK(step.state == ContainerSaveState::kConfirm);
  CHECK_FALSE(step.finished);
  CHECK(host.calls ==
        Calls{"BeginSession", "OpenInformationBox", "ShowPrompt:Open"});
}

TEST_CASE("Container save opening prompt waits, cancels or loads files",
          "[container-save]") {
  RecordingHost host;
  host.prompt_option = 0;
  CHECK(Run(ContainerSaveState::kConfirm, host).state ==
        ContainerSaveState::kConfirm);
  host.prompt_option = -1;
  CHECK(Run(ContainerSaveState::kConfirm, host).state ==
        ContainerSaveState::kReleaseStorage);
  CHECK(host.calls == Calls{"PollPrompt"});
  host.prompt_option = 2;
  CHECK(Run(ContainerSaveState::kConfirm, host).state ==
        ContainerSaveState::kLoadFiles);
  CHECK(host.calls == Calls{"PollPrompt", "ShowContainerHeading"});
}

TEST_CASE("Container save loads files only after idle rows and resources",
          "[container-save]") {
  RecordingHost host;
  host.rows_idle = false;
  CHECK(Run(ContainerSaveState::kLoadFiles, host).state ==
        ContainerSaveState::kLoadFiles);
  CHECK(host.calls == Calls{"MenuRowsIdle"});
  host.rows_idle = true;
  host.resources_ready = false;
  CHECK(Run(ContainerSaveState::kLoadFiles, host).state ==
        ContainerSaveState::kLoadFiles);
  host.resources_ready = true;
  CHECK(Run(ContainerSaveState::kLoadFiles, host).state ==
        ContainerSaveState::kSelectFile);
  CHECK(host.calls ==
        Calls{"MenuRowsIdle", "ResourcesReady", "OpenFilePicker"});
}

TEST_CASE("Container save file picker draws every frame and routes choices",
          "[container-save]") {
  RecordingHost host;
  host.picker_option = 0;
  CHECK(Run(ContainerSaveState::kSelectFile, host).state ==
        ContainerSaveState::kSelectFile);
  CHECK(host.calls == Calls{"PollFilePicker", "DrawFiles"});
  host.picker_option = 1;
  CHECK(Run(ContainerSaveState::kSelectFile, host).state ==
        ContainerSaveState::kStageItems);
  CHECK(host.calls == Calls{"PollFilePicker", "DrawFiles"});
  host.picker_option = -1;
  CHECK(Run(ContainerSaveState::kSelectFile, host).state ==
        ContainerSaveState::kCloseFiles);
}

TEST_CASE("Container save stages the screen and reloads its resources",
          "[container-save]") {
  RecordingHost host;
  CHECK(Run(ContainerSaveState::kStageItems, host).state ==
        ContainerSaveState::kWaitForScreen);
  CHECK(host.calls == Calls{"OpenContainerScreen"});
  host.rows_idle = false;
  CHECK(Run(ContainerSaveState::kWaitForScreen, host).state ==
        ContainerSaveState::kWaitForScreen);
  host.rows_idle = true;
  CHECK(Run(ContainerSaveState::kWaitForScreen, host).state ==
        ContainerSaveState::kReloadFiles);
  CHECK(host.calls == Calls{"MenuRowsIdle", "BeginResourceLoad"});
}

TEST_CASE("Container screen results restore, prompt to discard or save",
          "[container-save]") {
  RecordingHost host;
  host.resources_ready = false;
  CHECK(Run(ContainerSaveState::kReloadFiles, host).state ==
        ContainerSaveState::kReloadFiles);
  CHECK(host.calls == Calls{"ResourcesReady"});
  host.resources_ready = true;
  host.screen = ContainerScreenResult::kRestore;
  CHECK(Run(ContainerSaveState::kReloadFiles, host).state ==
        ContainerSaveState::kRestoreItems);
  CHECK(host.calls == Calls{"ResourcesReady", "ContainerScreenOutcome"});
  host.screen = ContainerScreenResult::kConfirmDiscard;
  CHECK(Run(ContainerSaveState::kReloadFiles, host).state ==
        ContainerSaveState::kConfirmDiscard);
  CHECK(host.calls == Calls{"ResourcesReady", "ContainerScreenOutcome",
                            "OpenInformationBox", "ShowPrompt:Discard"});
  host.screen = ContainerScreenResult::kSave;
  CHECK(Run(ContainerSaveState::kReloadFiles, host).state ==
        ContainerSaveState::kBeginWrite);
  CHECK(host.calls == Calls{"ResourcesReady", "ContainerScreenOutcome",
                            "OpenInformationBox"});
}

TEST_CASE("Container save writes and offers to discard a cancelled write",
          "[container-save]") {
  RecordingHost host;
  CHECK(Run(ContainerSaveState::kBeginWrite, host).state ==
        ContainerSaveState::kWaitForWrite);
  CHECK(host.calls == Calls{"BeginWrite"});
  host.write_option = 0;
  CHECK(Run(ContainerSaveState::kWaitForWrite, host).state ==
        ContainerSaveState::kWaitForWrite);
  CHECK(host.calls == Calls{"PollWrite", "DrawFiles"});
  host.write_option = -1;
  CHECK(Run(ContainerSaveState::kWaitForWrite, host).state ==
        ContainerSaveState::kConfirmDiscard);
  CHECK(host.calls == Calls{"PollWrite", "DataNotSaved", "OpenInformationBox",
                            "ShowPrompt:Discard", "DrawFiles"});
  host.data_not_saved = true;
  CHECK(Run(ContainerSaveState::kWaitForWrite, host).state ==
        ContainerSaveState::kCloseFiles);
  host.data_not_saved = false;
  host.write_option = 1;
  CHECK(Run(ContainerSaveState::kWaitForWrite, host).state ==
        ContainerSaveState::kCloseFiles);
  CHECK(host.calls == Calls{"PollWrite", "DataNotSaved", "DrawFiles"});
}

TEST_CASE("Container discard prompt retries the write or restores",
          "[container-save]") {
  RecordingHost host;
  host.prompt_option = 0;
  CHECK(Run(ContainerSaveState::kConfirmDiscard, host).state ==
        ContainerSaveState::kConfirmDiscard);
  host.prompt_option = 1;
  CHECK(Run(ContainerSaveState::kConfirmDiscard, host).state ==
        ContainerSaveState::kBeginWrite);
  host.prompt_option = 2;
  CHECK(Run(ContainerSaveState::kConfirmDiscard, host).state ==
        ContainerSaveState::kRestoreItems);
  host.prompt_option = -1;
  CHECK(Run(ContainerSaveState::kConfirmDiscard, host).state ==
        ContainerSaveState::kRestoreItems);
}

TEST_CASE("Container restore closes the picker in the same frame",
          "[container-save]") {
  RecordingHost host;
  CHECK(Run(ContainerSaveState::kRestoreItems, host).state ==
        ContainerSaveState::kDismissMenu);
  CHECK(host.calls == Calls{"RestoreInventory", "CloseFilePicker"});
  CHECK(Run(ContainerSaveState::kCloseFiles, host).state ==
        ContainerSaveState::kDismissMenu);
  CHECK(host.calls == Calls{"CloseFilePicker"});
}

TEST_CASE("Container save releases its storage once the box has closed",
          "[container-save]") {
  RecordingHost host;
  host.rows_idle = false;
  CHECK(Run(ContainerSaveState::kDismissMenu, host).state ==
        ContainerSaveState::kDismissMenu);
  CHECK(host.calls == Calls{"DismissMenu", "MenuRowsIdle"});
  host.rows_idle = true;
  CHECK(Run(ContainerSaveState::kDismissMenu, host).state ==
        ContainerSaveState::kReleaseStorage);
  CHECK(host.calls == Calls{"DismissMenu", "MenuRowsIdle", "ReleaseResources"});
  host.information_box_closed = false;
  Step step = Run(ContainerSaveState::kReleaseStorage, host);
  CHECK(step.state == ContainerSaveState::kReleaseStorage);
  CHECK_FALSE(step.finished);
  host.information_box_closed = true;
  step = Run(ContainerSaveState::kReleaseStorage, host);
  CHECK(step.state == ContainerSaveState::kInitialize);
  CHECK(step.finished);
  CHECK(host.calls == Calls{"InformationBoxClosed", "EndSession"});
}

TEST_CASE("Container save reports states the submenu never enters",
          "[container-save]") {
  for (const std::uint8_t raw : {4, 5, 6, 17, 255}) {
    RecordingHost host;
    const Step step = Run(static_cast<ContainerSaveState>(raw), host);
    CHECK(static_cast<std::uint8_t>(step.state) == raw);
    CHECK_FALSE(step.finished);
    CHECK(host.calls == Calls{"ReportInvalidState"});
    CHECK(host.reported == raw);
  }
}
