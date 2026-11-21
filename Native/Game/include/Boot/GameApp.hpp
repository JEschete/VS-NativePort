#pragma once

#include <filesystem>
#include <memory>
#include <optional>

#include "Boot/Session.hpp"
#include "Core/Types.hpp"

namespace VS::Input {
struct PadState;
}

namespace VS::Game {

struct Services {
  std::optional<std::filesystem::path> save_directory = std::nullopt;
};

// Runs the game's session (VS::Boot::RunLiveGame: Boot -> Title -> Gameplay,
// with the ending and the jumps back to the title; plan §6.8) on an internal
// fiber, one logical tick per Tick(). Destroying it cancels the game, unwinding
// every frame, before the services shut down.
class GameApp {
 public:
  explicit GameApp(Services services);
  ~GameApp();
  GameApp(const GameApp&) = delete;
  GameApp& operator=(const GameApp&) = delete;
  GameApp(GameApp&&) = delete;
  GameApp& operator=(GameApp&&) = delete;
  // One logical tick: hands the pad to the game and runs it until it waits for
  // the next frame. Rethrows an exception that ended the game.
  void Tick(const Input::PadState& pad, bool pad_connected);
  // The running session's mode; nullopt before boot and once the game ended.
  std::optional<Boot::Mode> CurrentMode() const;
  u64 Ticks() const { return m_ticks_; }
  bool Finished() const;

 private:
  Services m_services_;
  u64 m_ticks_ = 0;
  struct Runner;
  std::unique_ptr<Runner> m_runner_;
};

}  // namespace VS::Game
