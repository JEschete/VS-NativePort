#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

#include "Battle/Encounter.hpp"
#include "Core/Log.hpp"
#include "Input/Pad.hpp"
#include "nlohmann/json.hpp"

namespace VS::Game {

class GameplayTrace {
 public:
  explicit GameplayTrace(const std::filesystem::path& parent,
                         std::string_view host = "combat-lab",
                         const std::filesystem::path& compatibility_log = {},
                         bool player_observations = false,
                         bool verification_log = false);
  ~GameplayTrace();
  GameplayTrace(const GameplayTrace&) = delete;
  GameplayTrace& operator=(const GameplayTrace&) = delete;
  void Event(std::string_view category, const nlohmann::json& details,
             std::uint64_t tick = 0,
             const nlohmann::json* scoped_details = nullptr) noexcept;
  void Frame(std::uint64_t tick, const Input::PadState& input,
             const EncounterState& state,
             const nlohmann::json& scene = nullptr);
  const std::filesystem::path& Directory() const { return directory_; }
  bool Failed() const { return failed_.load(); }
  std::uint64_t Errors() const { return errors_.load(); }
  std::filesystem::path BeginScript(std::size_t number, std::uint64_t tick);
  void EndScript(std::uint64_t tick, const nlohmann::json& result);

 private:
  const bool player_observations_;
  const bool verification_log_;
  void WriteRecord(std::string_view category, const nlohmann::json& details,
                   std::uint64_t tick,
                   const nlohmann::json* scoped_details = nullptr);
  void Fail() noexcept;
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

nlohmann::json EncodeActor(const EncounterActorState& actor);
nlohmann::json InspectGameplayScene(const EncounterState& state);
void WriteTraceManifest(const std::filesystem::path& directory,
                        const nlohmann::json& metadata);
nlohmann::json TraceSourceProvenance(const std::filesystem::path& directory);

}  // namespace VS::Game
