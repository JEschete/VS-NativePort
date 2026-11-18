#include "Save/NativeResume.hpp"

#include <optional>
#include <string>
#include <utility>

#include "Core/Log.hpp"
#include "Save/NativeMenu.hpp"
#include "Save/RuntimeSnapshot.hpp"

namespace VS::Game::Save {
namespace {
std::string& ResumeError() {
  static std::string error;
  return error;
}
}  // namespace

bool FinishStagedResume() {
  const auto finished = FinishResume();
  if (!finished) {
    ResumeError() = finished.error();
    VS::Core::LogError("save", "Native resume failed: {}", ResumeError());
    return false;
  }
  ResumeError().clear();
  VS::Core::LogInfo("save", "Native resume applied before room-event dispatch");
  return true;
}

void ResetSession() {
  if (ResumePending()) {
    VS::Core::LogInfo("save", "Cancelling a staged native resume");
  }
  CancelResume();
  ResetFileMenuSession();
}

std::optional<std::string> TakeResumeError() {
  if (ResumeError().empty()) return std::nullopt;
  return std::exchange(ResumeError(), std::string{});
}

}  // namespace VS::Game::Save
