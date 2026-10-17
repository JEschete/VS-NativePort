#include "Platform/GameFiber.hpp"

#include "Core/Log.hpp"

#include <windows.h>

namespace VS::Platform {

namespace {
GameFiber* g_current = nullptr;
}

GameFiber::GameFiber(Entry entry, usize stackBytes) : m_entry(entry)
{
    m_host = IsThreadAFiber() ? GetCurrentFiber() : ConvertThreadToFiber(nullptr);
    m_fiber = CreateFiber(stackBytes, &GameFiber::Start, this);
    if (m_fiber == nullptr) {
        VS_LOG_ERROR("fiber", "CreateFiber failed ({})", GetLastError());
        m_finished = true;
    }
}

GameFiber::~GameFiber()
{
    if (m_fiber != nullptr) {
        DeleteFiber(m_fiber);
    }
    if (g_current == this) {
        g_current = nullptr;
    }
}

bool GameFiber::Resume()
{
    if (m_finished) {
        return false;
    }
    g_current = this;
    SwitchToFiber(m_fiber);
    g_current = nullptr;
    return !m_finished;
}

void GameFiber::YieldToHost()
{
    if (g_current != nullptr && GetCurrentFiber() == g_current->m_fiber) {
        SwitchToFiber(g_current->m_host);
    }
}

bool GameFiber::InGame()
{
    return g_current != nullptr && GetCurrentFiber() == g_current->m_fiber;
}

void __stdcall GameFiber::Start(void* self)
{
    auto* fiber = static_cast<GameFiber*>(self);
    fiber->m_entry();
    VS_LOG_INFO("fiber", "game entry returned");
    fiber->m_finished = true;
    // a fiber must never return: hand control back for good
    for (;;) {
        SwitchToFiber(fiber->m_host);
    }
}

} // namespace VS::Platform
