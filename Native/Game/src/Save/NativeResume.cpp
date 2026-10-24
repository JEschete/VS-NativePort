#include "Save/NativeResume.h"

#include <string>

#include "Core/Log.hpp"
#include "Save/NativeMenu.hpp"
#include "Save/RuntimeSnapshot.hpp"

namespace {
std::string& ResumeError() {
  static std::string error;
  return error;
}
}  // namespace

extern "C" int Save_NativeResumePending(void) {
  return VS::Game::Save::ResumePending() ? 1 : 0;
}

extern "C" int Save_NativeFinishResume(void) {
  const auto finished = VS::Game::Save::FinishResume();
  if (!finished) {
    ResumeError() = finished.error();
    VS_LOG_ERROR("save", "Native resume failed: {}", ResumeError());
    return 0;
  }
  ResumeError().clear();
  VS_LOG_INFO("save", "Native resume applied before room-event dispatch");
  return 1;
}

extern "C" void Save_NativeCancelResume(void) {
  if (VS::Game::Save::ResumePending()) {
    VS_LOG_INFO("save", "Cancelling a staged native resume");
  }
  VS::Game::Save::CancelResume();
}

extern "C" void Save_NativeResetSession(void) {
  Save_NativeCancelResume();
  VS::Game::Save::ResetFileMenuSession();
}

extern "C" const char* Save_NativeResumeError(void) {
  return ResumeError().c_str();
}

extern "C" void Save_NativeClearResumeError(void) { ResumeError().clear(); }
