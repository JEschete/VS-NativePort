#include "Boot/GameApp.hpp"

#include "Content/DataAccess.h"
#include "Core/Log.hpp"
#include "Input/LegacyPad.hpp"
#include "Input/Pad.hpp"
#include "Platform/GameFiber.hpp"

// The copied boot entry (vs_main_exec): the original's endless loop, run on the game fiber.
extern "C" void Boot_RunGame(void);

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

struct GameApp::Runner {
    Platform::GameFiber fiber{&Boot_RunGame};
};

GameApp::GameApp(Services services) : m_services(services)
{
    if (const int failed = Content_LoadModuleData(); failed != 0) {
        VS_LOG_WARN("boot", "{} module data entries not loaded", failed);
    }
    m_runner = std::make_unique<Runner>();
}

GameApp::~GameApp() = default;

bool GameApp::Finished() const
{
    return m_runner->fiber.Finished();
}

void GameApp::Tick(const Input::PadState& pad, bool padConnected)
{
    Legacy::FeedPad(0, pad, padConnected);
    if (!m_runner->fiber.Resume()) {
        static bool logged = false;
        if (!logged) {
            VS_LOG_WARN("boot", "the game's main loop returned");
            logged = true;
        }
    }
    ++m_ticks;
}

} // namespace VS::Game
