#include "Boot/GameApp.hpp"

#include <memory>
#include <stdexcept>

#include "Battle/EncounterHooks.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/DataAccess.hpp"
#include "Core/Log.hpp"
#include "Input/LegacyPad.hpp"
#include "Input/Pad.hpp"
#include "Platform/GameFiber.hpp"
#include "Save/NativeMenu.hpp"

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

// The copied boot entry (vs_main_exec): the original's endless loop, run on the
// game fiber.
struct GameApp::Runner {
  Platform::GameFiber fiber{&VS::Boot::RunLiveGame};
};

GameApp::GameApp(Services services) : m_services_(services) {
  if (m_services_.save_directory) {
    Save::ConfigureDirectory(*m_services_.save_directory);
  }
  if (const int failed = VS::Content::LoadModuleData(); failed != 0) {
    VS::Core::LogError(
        "boot",
        "{} required module data entries failed; game execution was "
        "not started",
        failed);
    throw std::runtime_error("Required module data initialization failed");
  }
  m_runner_ = std::make_unique<Runner>();
}

GameApp::~GameApp() {
  Save::ShutdownFileMenus();
  VS::Boot::ResetGameplayInspection();
}

bool GameApp::Finished() const { return m_runner_->fiber.Finished(); }

void GameApp::Tick(const Input::PadState& pad, bool pad_connected) {
  Legacy::FeedPad(0, pad, pad_connected);
  if (!m_runner_->fiber.Resume()) {
    static bool logged = false;
    if (!logged) {
      VS::Core::LogWarn("boot", "the game's main loop returned");
      logged = true;
    }
  }
  ++m_ticks_;
}

}  // namespace VS::Game
