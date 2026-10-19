#include "Platform/GameFiber.hpp"

#include <windows.h>

#include "Core/Log.hpp"

namespace VS::Platform {

namespace {
GameFiber* g_current = nullptr;
}

GameFiber::GameFiber(Entry entry, usize stack_bytes) : m_entry_(entry) {
  m_host_ =
      IsThreadAFiber() ? GetCurrentFiber() : ConvertThreadToFiber(nullptr);
  m_fiber_ = CreateFiber(stack_bytes, &GameFiber::Start, this);
  if (m_fiber_ == nullptr) {
    VS_LOG_ERROR("fiber", "CreateFiber failed ({})", GetLastError());
    m_finished_ = true;
  }
}

GameFiber::~GameFiber() {
  if (m_fiber_ != nullptr) {
    DeleteFiber(m_fiber_);
  }
  if (g_current == this) {
    g_current = nullptr;
  }
}

bool GameFiber::Resume() {
  if (m_finished_) {
    return false;
  }
  g_current = this;
  SwitchToFiber(m_fiber_);
  g_current = nullptr;
  return !m_finished_;
}

void GameFiber::YieldToHost() {
  if (g_current != nullptr && GetCurrentFiber() == g_current->m_fiber_) {
    SwitchToFiber(g_current->m_host_);
  }
}

bool GameFiber::InGame() {
  return g_current != nullptr && GetCurrentFiber() == g_current->m_fiber_;
}

void __stdcall GameFiber::Start(void* self) {
  auto* fiber = static_cast<GameFiber*>(self);
  fiber->m_entry_();
  VS_LOG_INFO("fiber", "game entry returned");
  fiber->m_finished_ = true;
  // a fiber must never return: hand control back for good
  for (;;) {
    SwitchToFiber(fiber->m_host_);
  }
}

}  // namespace VS::Platform
