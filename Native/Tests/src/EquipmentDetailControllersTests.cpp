#include <array>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Input/Pad.hpp"
#include "Menu/EquipmentDetailControllersWorld.hpp"

namespace {
namespace inventory = VS::Menu::Inventory;
using equipment_detail_tests::Call;
using equipment_detail_tests::Event;
using equipment_detail_tests::World;
using inventory::DetailStage;
using inventory::EquipmentDetailState;
}  // namespace

TEST_CASE("F02575 entry stores before callbacks and resets afterwards",
          "[equipment-detail-controllers][F02575]") {
  World world;
  EquipmentDetailState state{DetailStage::kReturn, 9, 1, 2, 0xAA};
  world.on_call = [&state](Call call) {
    if (call.event == Event::kBegin) {
      CHECK(state.selected_row == 20);
      CHECK(state.selected_page == 7);
      CHECK(state.stage == DetailStage::kReturn);
      state.selected_page = 3;
      state.animation_step = 8;
    }
  };
  CHECK(inventory::UpdateWeaponDetails(
            state, world, inventory::DetailSelection{{276}, {263}}) == 0);
  CHECK(state == EquipmentDetailState{DetailStage::kInitialize, 0, 20, 3, 0});
}

TEST_CASE("F02575 entry applies the prepared typed row transition",
          "[equipment-detail-controllers][F02575]") {
  World world;
  EquipmentDetailState state;
  CHECK(inventory::UpdateWeaponDetails(
            state, world, inventory::DetailSelection{{20}, {7}}) == 0);
  CHECK(world.policies.rows[20].target0 == 155);
  CHECK(world.policies.rows[31].target0 == 18);
}

TEST_CASE("F02575 uses five rows and does not wait twice for readiness",
          "[equipment-detail-controllers][F02575]") {
  World world;
  EquipmentDetailState state{DetailStage::kAnimate, 4, 20, 0};
  world.ready = 0;
  CHECK(inventory::UpdateWeaponDetails(state, world) == 0);
  CHECK(world.calls ==
        std::vector<Call>{{Event::kCurrent, 0}, {Event::kComponent, 5, 1, 1}});
  world.calls.clear();
  CHECK(inventory::UpdateWeaponDetails(state, world) == 0);
  CHECK(world.calls.empty());
  state.animation_step = 10;
  CHECK(inventory::UpdateWeaponDetails(state, world) == 0);
  CHECK(state.stage == DetailStage::kInput);
  CHECK(world.ready_reads == 0);
}

TEST_CASE("F02575 paging retains its slot and reads the post-stats row",
          "[equipment-detail-controllers][F02575]") {
  World world;
  EquipmentDetailState state{DetailStage::kInput, 10, 20, 0};
  world.policies.repeat = VS::Input::R1;
  world.on_call = [&state](Call call) {
    if (call.event == Event::kStats) {
      state.selected_row = 21;
    }
  };
  CHECK(inventory::UpdateWeaponDetails(state, world) == 0);
  CHECK(state.selected_page == 1);
  CHECK(world.policies.cursors[0] == inventory::CursorMemory{0, 1});
  CHECK(world.calls[2] == Call{Event::kFormat, 2});
  CHECK(world.calls[3] == Call{Event::kStats, 2});
  CHECK(world.calls[4] == Call{Event::kPage, 21, 1});
}

TEST_CASE("F02575 paging reloads current mappings between component callbacks",
          "[equipment-detail-controllers][F02575]") {
  World world;
  EquipmentDetailState state{DetailStage::kInput, 10, 20, 0};
  world.policies.repeat = VS::Input::R1;
  world.on_call = [&world](Call call) {
    if (call.event == Event::kComponent) {
      ++world.order[1];
    }
  };
  CHECK(inventory::UpdateWeaponDetails(state, world) == 0);
  CHECK(world.calls[6] == Call{Event::kComponent, 1, 2, 0});
  CHECK(world.calls.back() == Call{Event::kComponent, 5, 6, 0});
}

TEST_CASE("F02576 initializes only class affinity and DP PP",
          "[equipment-detail-controllers][F02576]") {
  World world;
  EquipmentDetailState state{DetailStage::kInitialize, 10, 20, 15, 0xAA};
  world.order[15] = 16;
  CHECK(inventory::UpdateBladeDetails(state, world) == 0);
  CHECK(world.calls == std::vector<Call>{{Event::kLayout, 20, 3},
                                         {Event::kCurrent, 15},
                                         {Event::kStats, 16},
                                         {Event::kBars, 3}});
  CHECK(state.stage == DetailStage::kAnimate);
  CHECK(state.animation_step == 10);
  CHECK(state.unused_entry_byte == 0xAA);
}

TEST_CASE("F02576 counts down before entering input",
          "[equipment-detail-controllers][F02576]") {
  World world;
  EquipmentDetailState state{DetailStage::kAnimate, 1, 20, 0};
  CHECK(inventory::UpdateBladeDetails(state, world) == 0);
  CHECK(state.animation_step == 0);
  CHECK(state.stage == DetailStage::kAnimate);
  CHECK(inventory::UpdateBladeDetails(state, world) == 0);
  CHECK(state.stage == DetailStage::kInput);
}

TEST_CASE("F02576 paging compares the current post-callback selection",
          "[equipment-detail-controllers][F02576]") {
  World world;
  EquipmentDetailState state{DetailStage::kInput, 0, 20, 0};
  world.policies.repeat = VS::Input::R1;
  world.on_call = [&state](Call call) {
    if (call.event == Event::kPoll) {
      state.selected_page = 1;
    }
  };
  CHECK(inventory::UpdateBladeDetails(state, world) == 0);
  CHECK(world.calls == std::vector<Call>{{Event::kPoll, 0}});
}

TEST_CASE("F02576 gives exit priority and returns the actual ready result",
          "[equipment-detail-controllers][F02576]") {
  World world;
  EquipmentDetailState state{DetailStage::kInput, 0, 20, 0};
  world.policies.pressed = VS::Input::Cross | VS::Input::R1;
  CHECK(inventory::UpdateBladeDetails(state, world) == 0);
  CHECK(world.calls == std::vector<Call>{{Event::kExit, 1}});
  CHECK(state.stage == DetailStage::kReturn);
  world.ready = -7;
  CHECK(inventory::UpdateBladeDetails(state, world) == -7);
  CHECK(world.ready_reads == 1);
}

TEST_CASE("F02577 initializes type-only without DP PP and counts upward",
          "[equipment-detail-controllers][F02577]") {
  World world;
  EquipmentDetailState state{DetailStage::kInitialize, 0, 20, 0};
  CHECK(inventory::UpdateGripDetails(state, world) == 0);
  CHECK(world.calls == std::vector<Call>{{Event::kLayout, 20, 4},
                                         {Event::kCurrent, 0},
                                         {Event::kStats, 1}});
  state.animation_step = 9;
  CHECK(inventory::UpdateGripDetails(state, world) == 0);
  CHECK(state.animation_step == 10);
  CHECK(state.stage == DetailStage::kAnimate);
  CHECK(inventory::UpdateGripDetails(state, world) == 0);
  CHECK(state.stage == DetailStage::kInput);
}

TEST_CASE("F02577 exit still draws the post-callback current grip slots",
          "[equipment-detail-controllers][F02577]") {
  World world;
  EquipmentDetailState state{DetailStage::kInput, 10, 20, 0};
  world.policies.pressed = VS::Input::Triangle;
  world.on_call = [&world, &state](Call call) {
    if (call.event == Event::kExit) {
      state.selected_page = 1;
      world.order[1] = 16;
      world.slots[15] = 255;
    }
  };
  CHECK(inventory::UpdateGripDetails(state, world) == 0);
  CHECK(state.stage == DetailStage::kReturn);
  CHECK(world.calls == std::vector<Call>{{Event::kExit, 0},
                                         {Event::kCurrent, 1},
                                         {Event::kSlots, 16},
                                         {Event::kDraw, 255}});
  CHECK(world.policies.return_intent == inventory::BattleReturnIntent::kBattle);
}

TEST_CASE("F02577 page change draws the newly mapped gem slot count",
          "[equipment-detail-controllers][F02577]") {
  World world;
  EquipmentDetailState state{DetailStage::kInput, 10, 20, 1};
  world.policies.repeat = VS::Input::L1;
  world.policies.category = 2;
  world.slots[0] = 3;
  CHECK(inventory::UpdateGripDetails(state, world) == 0);
  CHECK(state.selected_page == 0);
  CHECK(world.policies.cursors[2] == inventory::CursorMemory{0, 0});
  CHECK(world.calls[3] == Call{Event::kStats, 1});
  CHECK(world.calls[4] == Call{Event::kPage, 20, 0});
  CHECK(world.calls.back() == Call{Event::kDraw, 3});
}

TEST_CASE("F02578 creates only three gem rows and keeps the extra idle gate",
          "[equipment-detail-controllers][F02578]") {
  World world;
  EquipmentDetailState state{DetailStage::kAnimate, 2, 20, 7};
  world.ready = 0;
  CHECK(inventory::UpdateShieldDetails(state, world) == 0);
  CHECK(world.calls.back() == Call{Event::kComponent, 3, 8, 1});
  world.calls.clear();
  CHECK(inventory::UpdateShieldDetails(state, world) == 0);
  CHECK(world.calls.empty());
}

TEST_CASE("F02578 waits for row readiness after all ten animation frames",
          "[equipment-detail-controllers][F02578]") {
  World world;
  EquipmentDetailState state{DetailStage::kAnimate, 10, 20, 7};
  world.ready = 0;
  state.animation_step = 10;
  CHECK(inventory::UpdateShieldDetails(state, world) == 0);
  CHECK(state.stage == DetailStage::kAnimate);
  world.ready = 1;
  CHECK(inventory::UpdateShieldDetails(state, world) == 0);
  CHECK(state.stage == DetailStage::kInput);
  CHECK(world.ready_reads == 2);
}

TEST_CASE("F02578 shares selection services without weapon row count",
          "[equipment-detail-controllers][F02578]") {
  World world;
  EquipmentDetailState state{DetailStage::kInput, 10, 20, 7};
  world.policies.repeat = VS::Input::R1;
  world.policies.category = 3;
  CHECK(inventory::UpdateShieldDetails(state, world) == 0);
  CHECK(state.selected_page == 0);
  CHECK(world.policies.cursors[3] == inventory::CursorMemory{0, 0});
  CHECK(world.calls[2] == Call{Event::kFormat, 1});
  CHECK(world.calls.back() == Call{Event::kComponent, 3, 1, 0});
  CHECK(world.calls.size() == 11);
}

TEST_CASE("F02578 repeated entry replaces its old controller animation",
          "[equipment-detail-controllers][F02578]") {
  World world;
  EquipmentDetailState state{DetailStage::kReturn, 255, 20, 7, 0xFE};
  CHECK(inventory::UpdateShieldDetails(
            state, world, inventory::DetailSelection{{31}, {3}}) == 0);
  CHECK(state == EquipmentDetailState{DetailStage::kInitialize, 0, 31, 3, 0});
  CHECK(inventory::UpdateShieldDetails(state, world) == 0);
  CHECK(world.calls[1] == Call{Event::kLayout, 31, 7});
  CHECK(world.calls.back() == Call{Event::kBars, 3});
}
