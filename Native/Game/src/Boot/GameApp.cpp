#include "vs/game/boot/GameApp.hpp"

#include "vs/input/Pad.hpp"

namespace VS::Game {

const char* ModeName(Mode mode)
{
    switch (mode) {
    case Mode::Boot: return "Boot";
    case Mode::Title: return "Title";
    case Mode::Field: return "Field";
    case Mode::Battle: return "Battle";
    case Mode::Menu: return "Menu";
    case Mode::Ending: return "Ending";
    }
    return "?";
}

GameApp::GameApp(Services services) : m_services(services) {}

void GameApp::Tick(const Input::PadState& /*pad*/)
{
    // Module dispatch arrives with the copied Boot/Title code (Phase 1a step 6 / 1b).
    ++m_ticks;
}

} // namespace VS::Game
