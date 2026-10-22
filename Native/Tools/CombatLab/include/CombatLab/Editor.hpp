#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Battle/Encounter.hpp"
#include "CombatLab/Canvas.hpp"
#include "Input/Pad.hpp"

namespace VS::combat_lab {

class Editor {
 public:
  enum class Action { kNone, kStart, kSave };
  explicit Editor(std::vector<Game::EncounterZone> catalog);
  Action Update(const Input::PadState& pad, bool confirm);
  void Draw(Canvas& canvas) const;
  Game::EncounterSetup Setup() const;
  const Game::EncounterZone& Zone() const;
  void Load(const Game::EncounterSetup& setup);
  void Message(std::string message) { message_ = std::move(message); }

 private:
  void Change(int direction);
  void ResetRoom();
  void AddGroup();
  void UpdateStats(const Input::PadState& pad, bool confirm);
  void DrawStats(Canvas& canvas) const;
  std::vector<Game::EncounterZone> catalog_;
  std::size_t zone_index_ = 0;
  std::size_t room_index_ = 0;
  std::size_t enemy_index_ = 0;
  int row_ = 0;
  int stat_row_ = 0;
  bool stats_open_ = false;
  int quantity_ = 1;
  int offset_x_ = 0;
  int offset_z_ = 1;
  int spacing_ = 1;
  Game::EncounterSetup setup_;
  std::string message_;
};

}  // namespace VS::combat_lab
