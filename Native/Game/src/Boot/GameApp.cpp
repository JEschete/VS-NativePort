#include "Boot/GameApp.hpp"

#include <memory>
#include <stdexcept>

#include "Battle/EncounterHooks.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/DataAccess.hpp"
#include "Core/Log.hpp"
#include "Input/Controller.hpp"
#include "Input/ControllerLive.hpp"
#include "Input/Pad.hpp"
#include "Platform/GameFiber.hpp"
#include "Save/NativeMenu.hpp"

namespace VS::Game {

// The session from boot (vs_main_exec), run on the game fiber.
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
  // Unwind the game first: its frames' destructors still use the session.
  m_runner_.reset();
  Save::ShutdownFileMenus();
  VS::Boot::ResetGameplayInspection();
  Input::ResetLivePadLinks();
}

bool GameApp::Finished() const { return m_runner_->fiber.Finished(); }

std::optional<Boot::Mode> GameApp::CurrentMode() const {
  return VS::Boot::LiveSessionMode();
}

Input::Rumble GameApp::Rumble() const {
  return Finished() ? Input::Rumble{}
                    : Input::LiveRumble(Input::ControllerPort::kFirst);
}

void GameApp::Tick(const Input::PadState& pad, bool pad_connected) {
  Input::SubmitLivePad(Input::ControllerPort::kFirst, pad, pad_connected);
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
