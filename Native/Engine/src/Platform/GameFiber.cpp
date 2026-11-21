#include "Platform/GameFiber.hpp"

#include <windows.h>

#include <exception>
#include <utility>

#include "Core/Log.hpp"
#include "Platform/CrashHandler.hpp"

namespace VS::Platform {

namespace {
GameFiber* currentFiber = nullptr;
// The exception code of every MSVC C++ throw ('msc').
constexpr u32 kCppExceptionCode = 0xE06D7363;
}  // namespace

GameFiber::GameFiber(Entry entry, usize stack_bytes) : m_entry_(entry) {
  m_host_ =
      IsThreadAFiber() ? GetCurrentFiber() : ConvertThreadToFiber(nullptr);
  m_fiber_ = CreateFiber(stack_bytes, &GameFiber::Start, this);
  if (m_fiber_ == nullptr) {
    VS::Core::LogError("fiber", "CreateFiber failed ({})", GetLastError());
    m_finished_ = true;
  }
}

GameFiber::~GameFiber() {
  // Deleting the running fiber would end its thread under the game's frames.
  if (InGame()) {
    std::terminate();
  }
  // Start has already logged any exception this discards.
  Unwind();
  if (m_fiber_ != nullptr) {
    DeleteFiber(m_fiber_);
  }
  if (currentFiber == this) {
    currentFiber = nullptr;
  }
}

void GameFiber::SwitchIn() {
  GameFiber* const previous = currentFiber;
  currentFiber = this;
  SwitchToFiber(m_fiber_);
  currentFiber = previous;
}

bool GameFiber::Resume() {
  if (m_finished_) {
    return false;
  }
  if (InGame()) {
    VS::Core::LogError("fiber", "the game fiber cannot resume itself");
    return true;
  }
  m_started_ = true;
  SwitchIn();
  if (m_failure_) {
    std::rethrow_exception(std::exchange(m_failure_, nullptr));
  }
  return !m_finished_;
}

void GameFiber::Cancel() {
  if (m_finished_) {
    return;
  }
  if (InGame()) {
    VS::Core::LogError("fiber", "the game fiber cannot cancel itself");
    return;
  }
  if (Unwind()) {
    VS::Core::LogWarn("fiber", "discarding the game's exception on cancel");
  }
}

bool GameFiber::Unwind() noexcept {
  if (m_finished_) {
    return false;
  }
  m_cancel_requested_ = true;
  if (!m_started_) {
    m_cancelled_ = true;
    m_finished_ = true;
    return false;
  }
  // The pending wait throws FiberCancelled; waits reached while it unwinds
  // return at once, so the fiber comes back only once it has finished.
  SwitchIn();
  return std::exchange(m_failure_, nullptr) != nullptr;
}

void GameFiber::YieldToHost() {
  GameFiber* const fiber = currentFiber;
  if (fiber == nullptr || GetCurrentFiber() != fiber->m_fiber_) {
    return;
  }
  if (!fiber->m_cancel_requested_) {
    SwitchToFiber(fiber->m_host_);
  }
  // A destructor that waits while the cancellation unwinds just continues.
  if (fiber->m_cancel_requested_ && std::uncaught_exceptions() == 0) {
    throw FiberCancelled{};
  }
}

// Start's catch handlers run only after the throwing frames are unwound; this
// filter runs while they are still on the stack, so it can log the throw site.
void GameFiber::RunEntry(GameFiber* fiber) {
  __try {
    fiber->m_entry_();
    // NOLINTNEXTLINE(whitespace/parens)
  } __except (fiber->LogThrowSite(GetExceptionCode())) {
  }
}

int GameFiber::LogThrowSite(u32 code) const {
  if (code == kCppExceptionCode && !m_cancel_requested_) {
    LogCallStack("game fiber exception thrown here");
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

bool GameFiber::InGame() {
  return currentFiber != nullptr && GetCurrentFiber() == currentFiber->m_fiber_;
}

void __stdcall GameFiber::Start(void* self) {
  auto* fiber = static_cast<GameFiber*>(self);
  // Nothing may leave a fiber's start routine: an exception ends the entry
  // here and the host sees it from Resume().
  try {
    RunEntry(fiber);
    VS::Core::LogInfo("fiber", "game entry returned");
  } catch (const FiberCancelled&) {
    fiber->m_cancelled_ = true;
    VS::Core::LogInfo("fiber", "game cancelled; its frames unwound");
  } catch (const std::exception& error) {
    VS::Core::LogError("fiber", "game failed: {}", error.what());
    fiber->m_failure_ = std::current_exception();
  } catch (...) {
    VS::Core::LogError("fiber", "game failed with a nonstandard exception");
    fiber->m_failure_ = std::current_exception();
  }
  fiber->m_finished_ = true;
  // a fiber must never return: hand control back for good
  for (;;) {
    SwitchToFiber(fiber->m_host_);
  }
}

}  // namespace VS::Platform
