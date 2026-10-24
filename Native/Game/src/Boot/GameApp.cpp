#include "Boot/GameApp.hpp"

#include <memory>
#include <stdexcept>

#include "Content/DataAccess.h"
#include "Core/Log.hpp"
#include "Input/LegacyPad.hpp"
#include "Input/Pad.hpp"
#include "Platform/GameFiber.hpp"

// The copied boot entry (vs_main_exec): the original's endless loop, run on the
// game fiber.
extern "C" void Boot_RunGame(void);
extern "C" void Boot_ResetGameplayInspection(void);

namespace VS::Game {

const char* ModeName(Mode mode) {
  switch (mode) {
    case Mode::Boot:
      return "Boot";
    case Mode::Title:
      return "Title";
    case Mode::Field:
      return "Field";
    case Mode::Battle:
      return "Battle";
    case Mode::Menu:
      return "Menu";
    case Mode::Ending:
      return "Ending";
  }
  return "?";
}

struct GameApp::Runner {
  Platform::GameFiber fiber{&Boot_RunGame};
};

GameApp::GameApp(Services services) : m_services_(services) {
  if (const int failed = Content_LoadModuleData(); failed != 0) {
    VS_LOG_ERROR("boot",
                 "{} required module data entries failed; game execution was "
                 "not started",
                 failed);
    throw std::runtime_error("Required module data initialization failed");
  }
  m_runner_ = std::make_unique<Runner>();
}

GameApp::~GameApp() { Boot_ResetGameplayInspection(); }

bool GameApp::Finished() const { return m_runner_->fiber.Finished(); }

void GameApp::Tick(const Input::PadState& pad, bool pad_connected) {
  Legacy::FeedPad(0, pad, pad_connected);
  if (!m_runner_->fiber.Resume()) {
    static bool logged = false;
    if (!logged) {
      VS_LOG_WARN("boot", "the game's main loop returned");
      logged = true;
    }
  }
  ++m_ticks_;
}

}  // namespace VS::Game
