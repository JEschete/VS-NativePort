#include <array>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Field/DoorRecord.hpp"
#include "Field/GameMode.hpp"
#include "Field/RoomState.hpp"
#include "Field/Transitions.hpp"

namespace {
namespace fx = VS::Field;
using Calls = std::vector<std::string>;

class FakeTransitionHost final : public fx::TransitionHost {
 public:
  [[nodiscard]] int CurrentZoneId() const override { return zone; }
  [[nodiscard]] std::optional<fx::LocationDestination> Location(
      int location_id) const override {
    for (const auto& [id, destination] : locations) {
      if (id == location_id) return destination;
    }
    return std::nullopt;
  }
  [[nodiscard]] std::optional<VS::Actors::ActorPlacementView> AshleyPlacement()
      const override {
    return ashley;
  }
  void QueueRoomLoad(int room_id) override {
    calls.push_back(std::format("QueueRoomLoad {}", room_id));
  }
  void QueueZoneLoad(int zone_id) override {
    calls.push_back(std::format("QueueZoneLoad {}", zone_id));
  }
  void RestoreAshleyIdlePose() override { calls.emplace_back("Idle"); }
  void FreezeActorAnimations() override { calls.emplace_back("Freeze"); }
  void BypassAshleyCollision() override { calls.emplace_back("Bypass"); }
  void ClearEntranceDoor() override { calls.emplace_back("ClearEntrance"); }
  void FindEntranceDoorNearTile(int tile_x, int tile_z,
                                int floor_layer) override {
    calls.push_back(
        std::format("FindEntrance {} {} {}", tile_x, tile_z, floor_layer));
  }
  void ReportRefusedLocation(int location_id) override {
    calls.push_back(std::format("Refused {}", location_id));
  }

  int zone = 9;
  std::vector<std::pair<int, fx::LocationDestination>> locations;
  std::optional<VS::Actors::ActorPlacementView> ashley;
  Calls calls;
};

fx::DoorRecord SampleDoor() {
  return fx::DoorRecord{.destination_zone_id = 4,
                        .destination_room_id = 17,
                        .tile_x = 3,
                        .tile_z = 5,
                        .floor_layer = 1,
                        .entry_direction = 2,
                        .entry_tile_x = 6,
                        .entry_tile_z = 7,
                        .door_id = 11};
}

VS::Actors::ActorPlacementView AshleyAt(std::uint8_t x, std::uint8_t z,
                                        std::uint8_t layer,
                                        std::int16_t facing) {
  VS::Actors::ActorPlacementView placement;
  placement.tile = {.x = x, .layer = layer, .z = z};
  placement.position.w = facing;
  return placement;
}

constexpr std::uint32_t Step(fx::TransitionStep step) {
  return static_cast<std::uint32_t>(step);
}
}  // namespace

TEST_CASE("Screen transition phases name each step", "[field-rooms]") {
  using P = fx::TransitionPhase;
  using S = fx::TransitionStep;
  CHECK(fx::ScreenTransitionPhase(S::kIdle) == P::kIdle);
  CHECK(fx::ScreenTransitionPhase(S::kRoomExit) == P::kExiting);
  CHECK(fx::ScreenTransitionPhase(S::kZoneExit) == P::kExiting);
  CHECK(fx::ScreenTransitionPhase(S::kRoomEntryLoading) == P::kEntryLoading);
  CHECK(fx::ScreenTransitionPhase(S::kRoomEntryReveal) == P::kEntryReveal);
  CHECK(fx::ScreenTransitionPhase(S::kFadeOut) == P::kFadingOut);
  CHECK(fx::ScreenTransitionPhase(S::kFadedOut) == P::kFadedOut);
  CHECK(fx::ScreenTransitionPhase(S::kFadeIn) == P::kFadingIn);
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  CHECK(fx::ScreenTransitionPhase(static_cast<S>(8)) == P::kIdle);
}

TEST_CASE("Screen fades keep a room or zone exit under way", "[field-rooms]") {
  VS::Render::ScreenTransition screen{.alpha = 40};
  fx::ConfigureScreenTransition(screen,
                                {.effect = 2, .speed = 3, .wipe_angle = 4});
  CHECK(screen == VS::Render::ScreenTransition{
                      .alpha = 40, .effect = 2, .wipe_angle = 4, .speed = 3});
  fx::SetTransitionWipeAngle(screen, 9);
  CHECK(screen.wipe_angle == 9);
  constexpr fx::ScreenTransitionStyle kFade{
      .effect = 1, .speed = 6, .wipe_angle = 5};
  fx::BeginScreenFadeOut(screen, kFade);
  CHECK(screen.step == Step(fx::TransitionStep::kFadeOut));
  CHECK(screen.speed == 6);
  for (const auto exit :
       {fx::TransitionStep::kRoomExit, fx::TransitionStep::kZoneExit}) {
    VS::Render::ScreenTransition leaving{.step = Step(exit), .speed = 2};
    const VS::Render::ScreenTransition before = leaving;
    fx::BeginScreenFadeOut(leaving, kFade);
    CHECK(leaving == before);
  }
  VS::Render::ScreenTransition exiting{.step =
                                           Step(fx::TransitionStep::kRoomExit)};
  fx::BeginScreenFadeIn(exiting, {.effect = 3, .speed = 4, .wipe_angle = 7});
  CHECK(exiting ==
        VS::Render::ScreenTransition{.step = Step(fx::TransitionStep::kFadeIn),
                                     .effect = 3,
                                     .wipe_angle = 7,
                                     .speed = 4});
}

TEST_CASE("A room transition stays in Ashley's zone", "[field-rooms]") {
  FakeTransitionHost host;
  fx::RoomTransitionState state;
  state.screen.wipe_angle = 12;
  fx::BeginRoomTransition(state, host, SampleDoor(), 1, 8);
  fx::DoorRecord expected = SampleDoor();
  expected.destination_zone_id = 9;
  CHECK(state.pending_door == expected);
  CHECK(state.screen == VS::Render::ScreenTransition{
                            .step = Step(fx::TransitionStep::kRoomExit),
                            .effect = 1,
                            .wipe_angle = 12,
                            .speed = 8});
  CHECK(host.calls ==
        Calls{"QueueRoomLoad 17", "Idle", "Bypass", "ClearEntrance"});
}

TEST_CASE("A scripted transition then takes the door under Ashley",
          "[field-rooms]") {
  FakeTransitionHost host;
  host.ashley = AshleyAt(13, 21, 2, 0);
  fx::RoomTransitionState state;
  fx::BeginScriptedRoomTransition(state, host, SampleDoor(), 0, 4);
  CHECK(host.calls.back() == "FindEntrance 13 21 2");
  CHECK(state.screen.step == Step(fx::TransitionStep::kRoomExit));

  host.calls.clear();
  host.ashley.reset();
  fx::BeginScriptedZoneTransition(state, host, SampleDoor(), 2, 5);
  CHECK(host.calls == Calls{"QueueZoneLoad 4", "Idle", "Bypass",
                            "ClearEntrance", "FindEntrance 0 0 0"});
  CHECK(state.pending_door == SampleDoor());
  CHECK(state.screen.step == Step(fx::TransitionStep::kZoneExit));
  CHECK(state.screen.effect == 2);
  CHECK(state.screen.speed == 5);
}

TEST_CASE("A teleport to a location outside the table changes nothing",
          "[field-rooms]") {
  FakeTransitionHost host;
  fx::RoomTransitionState state;
  state.pending_door = SampleDoor();
  const fx::RoomTransitionState before = state;
  CHECK_FALSE(fx::TeleportToLocation(state, host, {.location_id = 600}));
  CHECK(state == before);
  CHECK(host.calls == Calls{"Refused 600"});
}

TEST_CASE("A teleport loads only what differs from the pending door",
          "[field-rooms]") {
  FakeTransitionHost host;
  host.locations = {{30, {.zone_id = 4, .room_id = 17}},
                    {31, {.zone_id = 4, .room_id = 18}},
                    {32, {.zone_id = 5, .room_id = 17}}};
  host.ashley = AshleyAt(1, 1, 0, 0);
  const auto teleport = [&](int location) {
    fx::RoomTransitionState state;
    state.pending_door = SampleDoor();
    host.calls.clear();
    REQUIRE(fx::TeleportToLocation(
        state, host,
        {.location_id = location, .tile_x = 2, .tile_z = 3, .speed = 8}));
    return state;
  };
  CHECK(teleport(30).screen.step == Step(fx::TransitionStep::kRoomExit));
  CHECK(host.calls == Calls{"Freeze", "Bypass", "ClearEntrance"});
  CHECK(teleport(31).pending_door.destination_room_id == 18);
  CHECK(host.calls.front() == "QueueRoomLoad 18");
  const fx::RoomTransitionState zone = teleport(32);
  CHECK(zone.screen.step == Step(fx::TransitionStep::kZoneExit));
  CHECK(zone.pending_door.destination_zone_id == 5);
  CHECK(host.calls.front() == "QueueZoneLoad 5");
}

namespace {
fx::DoorRecord TeleportEntry(std::int16_t facing) {
  FakeTransitionHost host;
  host.locations = {{30, {.zone_id = 4, .room_id = 17}}};
  host.ashley = AshleyAt(1, 1, 0, facing);
  fx::RoomTransitionState state;
  state.pending_door = SampleDoor();
  state.pending_door.entry_direction = 5;
  state.pending_door.reserved_entry_bit = true;
  REQUIRE(fx::TeleportToLocation(
      state, host, {.location_id = 30, .tile_x = 0x25, .tile_z = 0x3F}));
  return state.pending_door;
}
}  // namespace

TEST_CASE("A teleport enters at the clamped tile facing ahead",
          "[field-rooms]") {
  const fx::DoorRecord door = TeleportEntry(0x300);
  CHECK(door.entry_tile_x == 5);
  CHECK(door.entry_tile_z == 0x1F);
  CHECK(door.entry_direction == 0);
  CHECK_FALSE(door.reserved_entry_bit);
  CHECK(door.entry_elevation == 1);
}

TEST_CASE("A teleport keeps Ashley's facing quadrant", "[field-rooms]") {
  CHECK(TeleportEntry(0x0E00).entry_elevation == 0);
  CHECK(TeleportEntry(0x0A00).entry_elevation == 3);
  // A negative facing wraps as the original's signed remainder did.
  CHECK(TeleportEntry(-0x0800).entry_elevation == 3);
  CHECK(TeleportEntry(-0x0100).entry_elevation == 0);
}

TEST_CASE("Transition packets start covered in the original style",
          "[field-rooms]") {
  class Target final : public fx::TransitionPrimitiveTarget {
   public:
    void InitializeFade(const fx::TransitionPrimitiveStyle& s) override {
      calls.push_back(std::format("Fade {}", s.fade_semi_transparent));
    }
    void InitializeWipe(const fx::TransitionPrimitiveStyle& s) override {
      calls.push_back(std::format("Wipe {}", s.wipe_color.blue));
    }
    void InitializeEdge(const fx::TransitionPrimitiveStyle& s) override {
      calls.push_back(std::format("Edge {}", s.edge_lit_color.red));
    }
    void InitializeDrawMode(const fx::TransitionPrimitiveStyle& s) override {
      calls.push_back(std::format("Mode {} {}", s.blend_mode, s.dithered));
    }
    void SetTransitionAlpha(std::int32_t alpha) override {
      calls.push_back(std::format("Alpha {}", alpha));
    }
    Calls calls;
  };
  Target target;
  fx::InitializeRoomTransitionPrimitives(target);
  CHECK(target.calls ==
        Calls{"Fade true", "Wipe 1", "Edge 255", "Mode 2 true", "Alpha 255"});
  CHECK(fx::kTransitionPrimitiveStyle.screen_width == 320);
  CHECK(fx::kTransitionPrimitiveStyle.screen_height == 240);
}

TEST_CASE("Location ids name the first matching room", "[field-rooms]") {
  std::array<fx::LocationDestination, fx::kLocationCount> table{};
  table[0] = {.zone_id = 1, .room_id = 1};
  table[7] = {.zone_id = 9, .room_id = 3};
  table[8] = {.zone_id = 9, .room_id = 3};
  table[511] = {.zone_id = 250, .room_id = 40};
  CHECK(fx::FindLocationId(table, 9, 3) == 7);
  CHECK(fx::FindLocationId(table, 250, 40) == 511);
  CHECK(fx::FindLocationId(table, 1, 1) == 0);
  CHECK(fx::FindLocationId(table, 77, 77) == fx::kUnknownLocationId);
  CHECK(fx::CurrentRoomId(table, {.zone_id = 9, .room_id = 3}) == 7);
  CHECK(fx::CurrentRoomId({}, {.zone_id = 9, .room_id = 3}) ==
        fx::kUnknownLocationId);
}

namespace {
class FakeVisitHost final : public fx::RoomVisitHost {
 public:
  [[nodiscard]] int CurrentSceneId() const override { return scene; }
  [[nodiscard]] int CurrentRoomId() const override { return room; }
  void ReportRefusedVisit(const char* record, int id) override {
    calls.push_back(std::format("Refused {} {}", record, id));
  }
  int scene = 0;
  int room = 0;
  Calls calls;
};
}  // namespace

TEST_CASE("A pending room visit marks the room and its area once",
          "[field-rooms]") {
  FakeVisitHost host;
  host.scene = 33;
  host.room = 70;
  fx::RoomVisitState state;
  fx::MarkCurrentRoomVisited(state, host);
  CHECK(state == fx::RoomVisitState{});
  state.pending = true;
  fx::MarkCurrentRoomVisited(state, host);
  CHECK_FALSE(state.pending);
  CHECK(state.visited.areas[1] == (std::uint32_t{1} << 1));
  CHECK(state.visited.rooms[2] == (std::uint32_t{1} << 6));
  CHECK(host.calls.empty());
  state.pending = true;
  fx::CancelPendingRoomVisit(state);
  CHECK_FALSE(state.pending);
}

TEST_CASE("A visit past the records is refused, not stored", "[field-rooms]") {
  FakeVisitHost host;
  host.scene = 64;
  host.room = -1;
  fx::RoomVisitState state{.pending = true};
  fx::MarkCurrentRoomVisited(state, host);
  CHECK_FALSE(state.pending);
  CHECK(state.visited == fx::VisitedPlaces{});
  CHECK(host.calls == Calls{"Refused area 64", "Refused room -1"});
  host.scene = 63;
  host.room = 511;
  state.pending = true;
  fx::MarkCurrentRoomVisited(state, host);
  CHECK(state.visited.areas[1] == (std::uint32_t{1} << 31));
  CHECK(state.visited.rooms[15] == (std::uint32_t{1} << 31));
}

TEST_CASE("Only the owner of the loaded room shows or hides it",
          "[field-rooms]") {
  fx::RoomDisplay display{.loaded = true, .owner_room_index = 2};
  CHECK(fx::EnableOwnedRoomRendering(display, 2));
  CHECK(display.displayed);
  CHECK_FALSE(fx::DisableOwnedRoomRendering(display, 1));
  CHECK(display.displayed);
  CHECK(fx::DisableOwnedRoomRendering(display, 2));
  CHECK_FALSE(display.displayed);
  CHECK_FALSE(fx::ShowPrimaryRoomGeometry(display));
  fx::RoomDisplay unloaded{.owner_room_index = fx::kPrimaryRoomOwner};
  CHECK_FALSE(fx::ShowPrimaryRoomGeometry(unloaded));
  CHECK(unloaded == fx::RoomDisplay{});
}

TEST_CASE("The primary room shows and hides its geometry", "[field-rooms]") {
  fx::RoomDisplay display{.loaded = true,
                          .owner_room_index = fx::kPrimaryRoomOwner};
  CHECK(fx::ShowPrimaryRoomGeometry(display));
  CHECK(display.displayed);
  CHECK(fx::HidePrimaryRoomGeometry(display));
  CHECK_FALSE(display.displayed);
}

TEST_CASE("A session sets up its room state in the original order",
          "[field-rooms]") {
  class Host final : public fx::RoomRuntimeHost {
   public:
    void Clear(fx::RoomRecord record) override {
      calls.push_back(std::format("Clear {}", static_cast<int>(record)));
    }
    void Allocate(fx::RoomBuffer buffer) override {
      calls.push_back(std::format("Allocate {}", static_cast<int>(buffer)));
    }
    void ApplyDefaults(const fx::RoomRuntimeDefaults& defaults) override {
      calls.push_back(std::format("Defaults {} {}", defaults.entered_door_id,
                                  defaults.minimap_slide_frame));
    }
    void InitializeRoomBackground() override { calls.emplace_back("Bg"); }
    void ResetGimmickAnimationQueue() override {
      calls.emplace_back("Gimmicks");
    }
    void ApplyBackgroundReferenceColors() override {
      calls.emplace_back("RefColors");
    }
    Calls calls;
  };
  Host host;
  fx::InitializeRoomRuntimeState(host);
  CHECK(host.calls == Calls{"Clear 0", "Clear 1", "Clear 2", "Clear 3",
                            "Clear 4", "Allocate 0", "Allocate 1", "Allocate 2",
                            "Defaults -1 8", "Bg", "Gimmicks", "RefColors"});
  CHECK(fx::kRoomRuntimeDefaults.minimap_visible);
}

namespace {
class FakeModeHost final : public fx::FieldModeHost {
 public:
  void SetControlMode(fx::FieldControlMode mode) override {
    Add(std::format("Mode {}", static_cast<int>(mode)));
  }
  void SetActionFlowMode(int mode) override {
    Add(std::format("Flow {}", mode));
  }
  void SetInteractionPaused(bool paused) override {
    Add(std::format("Paused {}", paused));
  }
  void MarkCurrentRoomVisited() override { Add("Visited"); }
  void RestoreActorIdlePose(int actor_id) override {
    Add(std::format("Idle {}", actor_id));
  }
  void FreezeActorAnimations() override { Add("Freeze"); }
  void UnfreezeActorAnimations() override { Add("Unfreeze"); }
  void SetActorMarkersHidden(bool hidden) override {
    Add(std::format("Markers {}", hidden));
  }
  void SetMinimapVisible(bool visible) override {
    Add(std::format("Minimap {}", visible));
  }
  void TransformRoomPalettes(const fx::ColorTransform& t) override {
    Add(std::format("Palettes {} {}", t.mode, t.red_offset));
  }
  void TransformSceneColors(const fx::ColorTransform& t) override {
    Add(std::format("Scene {} {}", t.mode, t.duration));
  }
  void SetClutAnimationEnabled(bool enabled) override {
    Add(std::format("Clut {}", enabled));
  }
  void SetRoomAnimationsPaused(bool paused) override {
    Add(std::format("RoomAnim {}", paused));
  }
  void SetMovingPlatformsPaused(bool paused) override {
    Add(std::format("Platforms {}", paused));
  }
  void SetRoomAmbienceDucked(bool ducked) override {
    Add(std::format("Ambience {}", ducked));
  }
  [[nodiscard]] bool IsWeaponDrawn() const override { return weapon_drawn; }
  void SetHudBattleMode(int mode) override { Add(std::format("Hud {}", mode)); }
  void ReleaseMenuRows() override { Add("ReleaseRows"); }
  void ClearActionMenuStatus() override { Add("ClearStatus"); }
  void BeginActorActionSelection(int actor_id,
                                 std::uint16_t action_id) override {
    Add(std::format("Select {} {}", actor_id, action_id));
  }
  void SpawnTrapTileEffectAtAshley() override { Add("TrapEffect"); }
  void SetScriptFlag(int flag, int value) override {
    Add(std::format("Flag {:#x} {}", flag, value));
  }
  void TeleportToLocation(const fx::TeleportRequest& r) override {
    Add(std::format("Teleport {} {} {} {} {}", r.location_id, r.tile_x,
                    r.tile_z, r.effect, r.speed));
  }
  [[nodiscard]] std::int16_t AshleyMp() const override { return mp; }
  void SetAshleyMp(std::int16_t value) override {
    Add(std::format("Mp {}", value));
  }
  void PlayAshleyActionPose(int pose) override {
    Add(std::format("Pose {}", pose));
  }

  void Add(std::string call) { calls.push_back(std::move(call)); }

  bool weapon_drawn = false;
  std::int16_t mp = 0;
  Calls calls;
};

Calls MenuPause() {
  return {"Palettes 13 -4", "Clut false",     "Unfreeze",
          "RoomAnim true",  "Platforms true", "Minimap false",
          "Ambience true",  "Markers true",   "Paused true"};
}
Calls MenuResume() {
  return {"Clut true",      "Scene 8 4",       "Freeze",
          "RoomAnim false", "Platforms false", "Minimap true",
          "Ambience false", "Markers false",   "Paused false"};
}

Calls Join(std::initializer_list<Calls> parts) {
  Calls joined;
  for (const Calls& part : parts) {
    joined.insert(joined.end(), part.begin(), part.end());
  }
  return joined;
}

Calls ActorIdles() {
  Calls idles;
  for (int actor = fx::kFirstPausedActor; actor < fx::kPausedActorEnd;
       ++actor) {
    idles.push_back(std::format("Idle {}", actor));
  }
  return idles;
}
}  // namespace

TEST_CASE("Dialogs and objects pause the field with settled actors",
          "[field-rooms]") {
  FakeModeHost host;
  fx::PauseFieldForInteractionDialog(host);
  CHECK(host.calls ==
        Join({ActorIdles(), {"Minimap false", "Markers true", "Paused true"}}));
  host.calls.clear();
  fx::PauseFieldForObjectInteraction(host);
  CHECK(host.calls == Join({ActorIdles(), {"Markers true", "Paused true"}}));
}

TEST_CASE("Menus pause the field and resuming restores it", "[field-rooms]") {
  FakeModeHost host;
  fx::PauseFieldForMenu(host);
  CHECK(host.calls == MenuPause());
  host.calls.clear();
  fx::ResumeFieldAfterMenu(host);
  CHECK(host.calls == MenuResume());
  host.calls.clear();
  fx::EnterFieldExplorationMode(host);
  CHECK(host.calls == Calls{"Mode 1", "Flow 0", "Visited"});
  host.calls.clear();
  fx::BeginShortcutMenu(host);
  CHECK(host.calls == Join({{"Mode 6"}, MenuPause()}));
  host.calls.clear();
  fx::EndShortcutMenu(host);
  CHECK(host.calls == Join({{"Mode 1"}, MenuResume()}));
}

TEST_CASE("The battle action menu keeps the drawn weapon on the HUD",
          "[field-rooms]") {
  FakeModeHost host;
  fx::BeginBattleActionMenu(host);
  CHECK(host.calls == Join({{"Mode 5", "Flow 1", "Hud 2"}, MenuPause()}));
  host.calls.clear();
  host.weapon_drawn = true;
  fx::BeginBattleActionMenu(host);
  CHECK(host.calls[2] == "Hud 3");
  host.calls.clear();
  fx::CloseBattleActionMenu(host);
  CHECK(host.calls == Join({{"ReleaseRows", "ClearStatus"}, MenuResume()}));
}

TEST_CASE("An action menu choice starts its action selection",
          "[field-rooms]") {
  using C = fx::ActionMenuChoice;
  for (const C choice : {C::kBreakArt, C::kSpell, C::kItem}) {
    FakeModeHost host;
    host.weapon_drawn = true;
    fx::DispatchBattleActionMenuResult(
        host, {.kind = static_cast<std::int16_t>(choice), .action_id = 300});
    CHECK(host.calls == Calls{"Minimap true", "Hud 1", "Select 0 300"});
  }
  FakeModeHost host;
  fx::DispatchBattleActionMenuResult(host, {.kind = 0});
  CHECK(host.calls == Join({{"Minimap true", "Hud 0", "Mode 1", "Flow 0",
                             "Visited", "ReleaseRows", "ClearStatus"},
                            MenuResume()}));
}

TEST_CASE("The teleport spell pays its cost only when Ashley can",
          "[field-rooms]") {
  const auto cast = [](std::int16_t mp) {
    FakeModeHost host;
    host.mp = mp;
    fx::DispatchBattleActionMenuResult(
        host,
        {.kind = static_cast<std::int16_t>(fx::ActionMenuChoice::kTeleport),
         .location_id = 40,
         .tile_x = 6,
         .tile_z = 9,
         .mp_cost = 12});
    return host.calls;
  };
  const Calls paid = cast(20);
  CHECK(Calls(paid.begin(), paid.begin() + 7) ==
        Calls{"Minimap true", "Hud 0", "TrapEffect", "Flag 0xb7 1",
              "Teleport 40 6 9 0 8", "Mp 8", "Pose 7"});
  CHECK(paid[7] == "Mode 1");
  CHECK(cast(12)[5] == "Mp 0");
  const Calls short_of_mp = cast(11);
  CHECK(short_of_mp[5] == "Pose 7");
  CHECK(short_of_mp.size() == paid.size() - 1);
}
