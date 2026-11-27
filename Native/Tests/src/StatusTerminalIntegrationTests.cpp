#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Boot/SessionLive.hpp"
#include "Content/DiscLayout.hpp"
#include "Menu/OverlayLoadFixture.hpp"
#include "Menu/StatusTerminalLiveFixture.hpp"

using VS::Content::disc::Locate;

namespace {
namespace st = VS::Menu::Status;
namespace fw = VS::Menu::Framework;
namespace memory = VS::Core::Memory;
using status_terminal_live_tests::Fixture;

template <typename Entry>
Entry ModuleEntry(unsigned address, Entry expected) {
  auto* resolved = VS::Content::ResolveModuleAddress(
      VS::Content::ModuleSymbols(), "MENU/MENU4.PRG", address);
  CHECK(resolved == reinterpret_cast<void*>(expected));
  if (resolved == nullptr) {
    throw std::runtime_error("missing status terminal module entry");
  }
  return reinterpret_cast<Entry>(resolved);
}

struct ReloadFixture : Fixture {
  void Prime() {
    REQUIRE(st::LiveUpdateEquipmentDetails(2) == 0);
    REQUIRE(st::LiveUpdateEquipmentList(0) == 0);
    st::terminal_live::detailState = {7, 2, 4, 5, 6};
    st::terminal_live::listState = {3, 8};
    st::terminal_live::screenState = {.animation_wait = 9,
                                      .screen_enabled = 1,
                                      .cursor_animation = 10,
                                      .saved_projection = 333,
                                      .prompt_animation = 6};
    auto& saved = st::terminal_live::screenState.saved_view;
    for (auto& row : saved.rotation.m) {
      row.fill(-17);
    }
    saved.rotation_padding = 23;
    saved.translation = {1, 2, 3};
    saved.camera = {{4, 5, 6}, 7, {8, 9, 10}, 11, {12, 13, 14}, 15, 16};
    st::selectedStatRow = 6;
    VS::Render::projectionDistance = 777;
    VS::Render::FrameScratchpad().camera.position.vx = 1234;
    VS::Render::FrameScratchpad().camera.angles.pad = 77;
    VS::Menu::Framework::menuMemory.selected_equipment = 5;
    VS::Battle::stringContext.strings[0] = VS::Menu::Framework::stringBuf;
    *State() = 7;
  }

  static void CheckUnchanged(const st::EquipmentDetailState& detail,
                             const st::EquipmentListState& list,
                             const st::StatusTerminalState& screen) {
    CHECK(st::LiveEquipmentDetailControllerState() == detail);
    CHECK(st::LiveEquipmentListControllerState() == list);
    CHECK(st::LiveStatusTerminalControllerState() == screen);
    CHECK(st::selectedStatRow == 6);
  }

  static void CheckReset() {
    CHECK(st::LiveEquipmentDetailControllerState() ==
          st::EquipmentDetailState{});
    CHECK(st::LiveEquipmentListControllerState() == st::EquipmentListState{});
    CHECK(st::LiveStatusTerminalControllerState() == st::StatusTerminalState{});
    CHECK(st::selectedStatRow == 0);
  }

  void CheckBorrowedOwners(bool cold) {
    CHECK(VS::Menu::Framework::menuItems == workspace);
    CHECK(VS::Menu::Framework::itemNames == text);
    CHECK(memory::LivePayloadCapacity(workspace).has_value());
    CHECK(memory::LivePayloadCapacity(text).has_value());
    CHECK(VS::Battle::actors[0] == &actor);
    CHECK(actor.stats == &stats);
    CHECK(VS::Menu::Framework::menuMemory.selected_equipment == 5);
    CHECK(VS::Battle::stringContext.strings[0] ==
          (cold ? nullptr : VS::Menu::Framework::stringBuf));
  }

  void CheckOtherAndPending() {
    const auto detail = st::LiveEquipmentDetailControllerState();
    const auto list = st::LiveEquipmentListControllerState();
    const auto screen = st::LiveStatusTerminalControllerState();
    {
      menu_overlay_tests::Load other(
          3, {Locate("MENU/MENU2.PRG").lba, Locate("MENU/MENU2.PRG").size});
      REQUIRE(other.Complete() == 1);
    }
    CheckUnchanged(detail, list, screen);
    {
      menu_overlay_tests::Load pending(
          5, {Locate("MENU/MENU4.PRG").lba, Locate("MENU/MENU4.PRG").size});
      REQUIRE(pending.Poll() == 0);
      CheckUnchanged(detail, list, screen);
    }
  }

  static void ResetThroughBoundary(bool cold) {
    if (cold) {
      const auto entry = +[] { return 0; };
      REQUIRE(VS::Boot::RunLiveSessionLoop(entry, entry) == 0);
    } else {
      menu_overlay_tests::Load load(
          5, {Locate("MENU/MENU4.PRG").lba, Locate("MENU/MENU4.PRG").size});
      REQUIRE(load.Complete() == 1);
    }
  }

  void Reenter(bool cold) {
    IdleRows();
    if (cold) {
      CHECK(VS::Actors::actorRecords[1] == nullptr);
    }
    *State() = 3;
    const auto refused = st::RefusedLiveTerminalControllerCount();
    REQUIRE(ModuleEntry(0x801064D4, &st::LiveUpdateStatusTerminal)(State()) ==
            0);
    CHECK(st::RefusedLiveTerminalControllerCount() == refused);
    CHECK(*State() == 5);
    CHECK(st::LiveStatusTerminalControllerState().saved_projection == 777);
    CHECK(st::LiveStatusTerminalControllerState().screen_enabled == 0);
  }

  void AdvanceReentry() {
    BindFrame();
    const auto refused = st::RefusedLiveTerminalControllerCount();
    REQUIRE(st::LiveUpdateStatusTerminal(State()) == 0);
    CHECK(st::RefusedLiveTerminalControllerCount() == refused);
    REQUIRE(VS::Actors::actorRecords[1] != nullptr);
    CHECK(st::LiveStatusTerminalControllerState().screen_enabled == 1);
  }
};
}  // namespace

TEST_CASE("Status terminal module entries drive real detail and list rows",
          "[status-terminal][status-terminal-production][F02675][F02676]") {
  Fixture fixture;
  const auto refused = st::RefusedLiveTerminalControllerCount();
  REQUIRE(ModuleEntry(0x80104F80, &st::LiveUpdateEquipmentDetails)(2) == 0);
  CHECK(fw::LiveMenuRow(11).selected == 1);
  CHECK(fw::LiveMenuRow(11).target0 == 155);
  REQUIRE(ModuleEntry(0x80105970, &st::LiveUpdateEquipmentList)(0) == 0);
  CHECK(st::LiveEquipmentListControllerState().phase == 1);
  CHECK(VS::Menu::Framework::itemsText != nullptr);
  CHECK(st::RefusedLiveTerminalControllerCount() == refused);
}

TEST_CASE(
    "Status terminal module renderers consume current frame owners",
    "[status-terminal][status-terminal-production][F02677][F02678][F02679]") {
  Fixture fixture;
  const auto refused = st::RefusedLiveTerminalControllerCount();
  st::fadeScreen = 1;
  ModuleEntry(0x80105E94, &st::LiveDrawModelSlideFade)();
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        fixture.primitives.data() + 14);
  ModuleEntry(0x80105F60, &st::LiveDrawBackground)();
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        fixture.primitives.data() + 51);
  fixture.BindFrame();
  st::fadeScreen = 0;
  st::delayScreenUpdate = 2;
  VS::Actors::actorRecords[1]->collision_height = 32768;
  ModuleEntry(0x80106150, &st::LiveDrawFrame)();
  CHECK(VS::Render::FrameScratchpad().camera.look_at.vy == -67108864);
  CHECK(st::delayScreenUpdate == 1);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        fixture.primitives.data() + 37);
  CHECK(st::RefusedLiveTerminalControllerCount() == refused);
}

TEST_CASE(
    "Production MENU4 and cold boundaries reset terminal owners before reentry",
    "[status-terminal][status-terminal-production][status-terminal-reload]["
    "F02682]") {
  ReloadFixture fixture;
  const bool cold = GENERATE(false, true);
  fixture.Prime();
  fixture.CheckOtherAndPending();
  const auto current = st::terminal_live::LoadView();
  const auto refused = memory::RefusedLiveHeapRequestCount();
  ReloadFixture::ResetThroughBoundary(cold);
  ReloadFixture::CheckReset();
  fixture.CheckBorrowedOwners(cold);
  CHECK(st::terminal_live::LoadView() == current);
  CHECK(VS::Render::projectionDistance == 777);
  CHECK(*fixture.State() == 7);
  CHECK(memory::RefusedLiveHeapRequestCount() == refused);
  fixture.Reenter(cold);
  fixture.AdvanceReentry();
}
