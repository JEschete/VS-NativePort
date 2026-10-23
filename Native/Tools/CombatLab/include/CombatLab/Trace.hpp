#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "Battle/Encounter.hpp"
#include "Core/Log.hpp"
#include "Input/Pad.hpp"
#include "nlohmann/json.hpp"

namespace VS::combat_lab {

inline constexpr std::uint64_t kScriptStopGraceTicks = 600;

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
  std::uint64_t Errors() const { return errors_.load(); }
  std::filesystem::path BeginScript(std::size_t number, std::uint64_t tick);
  void EndScript(std::uint64_t tick, const nlohmann::json& result);

 private:
  void WriteRecord(std::string_view category, const nlohmann::json& details,
                   std::uint64_t tick);
  const std::chrono::steady_clock::time_point started_ =
      std::chrono::steady_clock::now();
  std::filesystem::path directory_;
  std::ofstream stream_;
  std::ofstream script_stream_;
  std::size_t script_number_ = 0;
  std::uint64_t script_start_tick_ = 0;
  std::mutex mutex_;
  std::uint64_t sequence_ = 0;
  std::atomic<bool> failed_{false};
  std::atomic<std::uint64_t> errors_{0};
  Core::LogObserver previous_observer_;
  Core::LogLevel previous_console_level_ = Core::LogLevel::Trace;
  bool previous_flush_policy_ = true;
};

nlohmann::json EncodeSetup(const Game::EncounterSetup& setup);
nlohmann::json EncodeActor(const Game::EncounterActorState& actor);
Game::EncounterSetup DecodeSetup(const nlohmann::json& json);

struct ScriptPress {
  std::uint64_t tick = 0;
  std::uint32_t buttons = 0;
};

struct EncounterScript {
  std::string name;
  std::filesystem::path scenario;
  Game::EncounterSetup setup;
  std::uint64_t frames = 0;
  std::vector<ScriptPress> presses;
};

std::uint64_t ParseTick(std::string_view value);
std::vector<ScriptPress> DecodePresses(std::string_view value);
std::vector<EncounterScript> ReadScriptSequence(
    const std::filesystem::path& path);

}  // namespace VS::combat_lab
