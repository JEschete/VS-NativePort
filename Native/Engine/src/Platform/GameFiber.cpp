#include "Platform/GameFiber.hpp"

#include <windows.h>

#include "Core/Log.hpp"

namespace VS::Platform {

namespace {
GameFiber* currentFiber = nullptr;
}

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
  if (m_fiber_ != nullptr) {
    DeleteFiber(m_fiber_);
  }
  if (currentFiber == this) {
    currentFiber = nullptr;
  }
}

bool GameFiber::Resume() {
  if (m_finished_) {
    return false;
  }
  currentFiber = this;
  SwitchToFiber(m_fiber_);
  currentFiber = nullptr;
  return !m_finished_;
}

void GameFiber::YieldToHost() {
  if (currentFiber != nullptr && GetCurrentFiber() == currentFiber->m_fiber_) {
    SwitchToFiber(currentFiber->m_host_);
  }
}

bool GameFiber::InGame() {
  return currentFiber != nullptr && GetCurrentFiber() == currentFiber->m_fiber_;
}

void __stdcall GameFiber::Start(void* self) {
  auto* fiber = static_cast<GameFiber*>(self);
  fiber->m_entry_();
  VS::Core::LogInfo("fiber", "game entry returned");
  fiber->m_finished_ = true;
  // a fiber must never return: hand control back for good
  for (;;) {
    SwitchToFiber(fiber->m_host_);
  }
}

}  // namespace VS::Platform
