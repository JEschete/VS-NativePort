#pragma once

#include <optional>
#include <string>

// The session side of a staged native resume (RuntimeSnapshot.hpp stages and
// applies the snapshot itself).
namespace VS::Game::Save {

// Applies the staged resume before room-event dispatch. False on failure,
// which is logged and kept for TakeResumeError.
bool FinishStagedResume();
// Cancels any staged resume and resets the file-menu session.
void ResetSession();
// The last resume failure, cleared by reading it; nullopt when there is none.
std::optional<std::string> TakeResumeError();

}  // namespace VS::Game::Save
