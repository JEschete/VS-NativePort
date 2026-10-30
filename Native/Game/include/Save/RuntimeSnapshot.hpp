#pragma once

#include <expected>
#include <string>

#include "Save/NativeSnapshot.hpp"

namespace VS::Game::Save {

// Game-fiber only. This does not grant save-point eligibility: callers must
// retain Field_CurrentSavePointId's floor-trigger and actor-state checks.
// Schema3 supports static collision layers0/1 and quiescent orbit cameras.
// A field menu may freeze horizontal walk/turn intent; only its position and
// facing are saved, and the normal room spawn resumes idle. Capture is
// read-only. Moving supports, attachment, active actor/camera motion,
// first-person, non-orbit/custom projection and unfinished transitions remain
// unsupported.
std::expected<Snapshot, std::string> Capture(
    const Items_ContainerData& storage);
// No application/mutation. Requires stable DataStore and module-table lifetime;
// concurrent DataStore::Open or module initialization is not synchronized here.
std::expected<void, std::string> ValidateContent(const Snapshot& state);
std::expected<void, std::string> ValidateContent(const Document& document);

// TITLE 22C.c:616, after ENDING D4.c:1502 and the original Ashley capture.
// Uses only persistent values; never follows actors left by the ending overlay.
// Storage ownership remains per native file. No campaign completion is forged.
std::expected<Document, std::string> CaptureClearGame();

// After confirmation, before the C-only boot transition. Stages an owned copy
// and installs the persistent inputs consumed by the original boot/spawn code.
// Container ownership remains with the caller's per-file session manager.
// Schema4 clear-game documents install their original entry door and persistent
// carryover. They use natural starting-room placement/camera/RNG, not schema3's
// checkpoint finalization. Cancellation restores either kind of staged input.
std::expected<void, std::string> PrepareResume(const Document& document);
bool ResumePending();
// Also call on reset, return to title, shutdown and an abandoned boot. Restores
// the pre-prepare persistent inputs and output mode; never follows old
// actor/room pointers. Cancel before tearing down the audio consumer.
void CancelResume();

// Field_FinishRoomLoad: after camera setup and Render_ConfigureFieldDisplay,
// before the temp_s4/event-trigger branch. The first zone load must preserve
// saved zone flags while ResumePending(), rather than resetting flags64..127.
// No boot/longjmp is performed here. Failure leaves the pending state intact.
std::expected<void, std::string> FinishResume();

}  // namespace VS::Game::Save
