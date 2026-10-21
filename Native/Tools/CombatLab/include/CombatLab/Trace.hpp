#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

#include "Battle/Encounter.hpp"
#include "Core/Log.hpp"
#include "Input/Pad.hpp"
#include "nlohmann/json.hpp"

namespace VS::combat_lab {

class Trace {
 public:
  explicit Trace(const std::filesystem::path& parent);
  ~Trace();
  Trace(const Trace&) = delete;
  Trace& operator=(const Trace&) = delete;
  void Event(std::string_view category, const nlohmann::json& details,
             std::uint64_t tick = 0) noexcept;
  void Frame(std::uint64_t tick, const Input::PadState& input,
             const Game::EncounterState& state);
  const std::filesystem::path& Directory() const { return directory_; }
  bool Failed() const { return failed_.load(); }

 private:
  std::filesystem::path directory_;
  std::ofstream stream_;
  std::mutex mutex_;
  std::uint64_t sequence_ = 0;
  std::atomic<bool> failed_{false};
  Core::LogObserver previous_observer_;
};

nlohmann::json EncodeSetup(const Game::EncounterSetup& setup);
Game::EncounterSetup DecodeSetup(const nlohmann::json& json);

}  // namespace VS::combat_lab
