#pragma once
// The globals a live text-box update reads and writes, restored when a test
// ends. Shared by the text-box update and scene-notification live tests.

#include <iterator>

#include "Battle/Hud.h"
#include "Core/RandomLive.hpp"
#include "Effects/ScreenEffectState.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/TextBox.hpp"
#include "State/Timers.hpp"

namespace text_box_tests {
struct LiveUpdateRestore {
  equipment_detail_tests::ArrayRestore<
      u_long, std::size(VS::Menu::textBoxOrderingTables)>
      saved_tables{VS::Menu::textBoxOrderingTables};
  equipment_detail_tests::ValueRestore<u_long*> saved_active{
      VS::Menu::activeTextBoxOrderingTable};
  equipment_detail_tests::ValueRestore<char> saved_index{
      VS::Menu::textBoxBufferIndex};
  equipment_detail_tests::ValueRestore<decltype(VS::State::tickSpeed)>
      saved_speed{VS::State::tickSpeed};
  equipment_detail_tests::ValueRestore<decltype(Battle_lowerScreenUiState)>
      saved_ui{Battle_lowerScreenUiState};
  equipment_detail_tests::ValueRestore<decltype(VS::Core::interfaceRandomState)>
      saved_random{VS::Core::interfaceRandomState};
  equipment_detail_tests::ValueRestore<
      decltype(VS::Effects::screenEffectOverlay)>
      saved_overlay{VS::Effects::screenEffectOverlay};
  equipment_detail_tests::ValueRestore<
      decltype(VS::Effects::screenEffectDefaults)>
      saved_defaults{VS::Effects::screenEffectDefaults};
};
}  // namespace text_box_tests
