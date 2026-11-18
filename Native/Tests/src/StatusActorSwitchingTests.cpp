#include <array>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Status/ActorStatus.h"
#include "Menu/Status/ActorSwitching.hpp"
#include "Menu/Status/ActorSwitchingLive.hpp"

namespace {
namespace st = VS::Menu::Status;
namespace vp = VS::Menu::Status::vitals_phase;
namespace sp = VS::Menu::Status::actor_switch_phase;
using Calls = std::vector<std::string>;

class FakeVitalsHost final : public st::VitalsHost {
 public:
  void SetHudHidden(std::int32_t hidden) override {
    calls.push_back(std::format("Hud {}", hidden));
  }
  std::int32_t DrawValue(std::int32_t font, std::int32_t xy,
                         std::int32_t value) override {
    calls.push_back(std::format("Value {} {:#x} {}", font, Bits(xy), value));
    return xy - value_width;
  }
  void QueueSprite(const st::VitalsSprite& sprite) override {
    calls.push_back(std::format("Sprite {:#x} {:#x} {:#x}", Bits(sprite.xy),
                                sprite.wh, sprite.uv_clut));
  }
  void QueueLabelSprite(const st::VitalsSprite& sprite) override {
    calls.push_back(std::format("Label {} {:#x} {:#x} {:#x}", sprite.brightness,
                                Bits(sprite.xy), sprite.wh, sprite.uv_clut));
  }
  void DrawGauge(std::int32_t color, std::int32_t fill,
                 std::int32_t xy) override {
    calls.push_back(std::format("Gauge {} {} {:#x}", color, fill, Bits(xy)));
  }

  std::int32_t value_width = 0x20;
  Calls calls;

 private:
  // A packed position as the halfword pair it holds.
  static std::uint32_t Bits(std::int32_t xy) {
    return static_cast<std::uint32_t>(xy);
  }
};

class FakeRoster final : public st::InspectableActorRoster {
 public:
  std::uint32_t StateFlags(std::int32_t slot) override {
    probes.push_back(slot);
    return flags.at(static_cast<std::size_t>(slot));
  }
  std::optional<std::uint16_t> Kind(std::int32_t slot) override {
    return kinds.at(static_cast<std::size_t>(slot));
  }

  std::array<std::uint32_t, st::kInspectableActorSlots> flags{};
  std::array<std::optional<std::uint16_t>, st::kInspectableActorSlots> kinds{};
  std::vector<std::int32_t> probes;
};

class FakeSwitchHost final : public st::ActorSwitchHost {
 public:
  void PlaySwitchSound() override { calls.emplace_back("Sound"); }
  void ShowActorName(std::int32_t slot, const st::ActorNameRow& row) override {
    calls.push_back(std::format("Name {} {} {} {} {} {} {}", slot, row.row,
                                row.x, row.y, row.width, row.background_width,
                                row.selected));
  }
  void ReleaseActorPreview() override { calls.emplace_back("Release"); }
  void LoadActorPreview(std::int32_t slot) override {
    calls.push_back(std::format("Load {}", slot));
  }

  Calls calls;
};

constexpr std::int32_t kPresent = 1;
constexpr std::uint16_t kHuman = 0;
constexpr std::uint16_t kObject = 5;

st::ActorVitals Analyzed() {
  return {.current_hp = 50,
          .max_hp = 100,
          .current_mp = 7,
          .max_mp = 0,
          .risk = 30,
          .analyzed = true};
}
}  // namespace

TEST_CASE("Vitals transition picks hiding swapping or awaiting the HUD",
          "[status-switching]") {
  FakeVitalsHost host;
  st::VitalsPanelState state{.phase = vp::kIdle, .delay = 0, .shown_actor = 2};
  st::BeginVitalsTransition(state, 0, host);
  CHECK(state.phase == vp::kHiding);
  CHECK(state.delay == 4);
  CHECK(state.shown_actor == 0);
  CHECK(host.calls.empty());

  st::BeginVitalsTransition(state, 3, host);
  CHECK(state.phase == vp::kAwaitingHud);
  CHECK(host.calls == Calls{"Hud 1"});

  st::BeginVitalsTransition(state, 0x105, host);
  CHECK(state.phase == vp::kSwapping);
  CHECK(state.shown_actor == 5);
  CHECK(host.calls.size() == 1);
}

TEST_CASE("Next inspectable actor wraps and skips hidden and object actors",
          "[status-switching]") {
  FakeRoster roster;
  roster.flags[13] = 0x01000001;
  roster.kinds[13] = kHuman;
  roster.flags[14] = kPresent;
  roster.kinds[14] = kObject;
  roster.flags[1] = kPresent;
  roster.kinds[1] = kHuman;
  const std::optional<std::int32_t> next =
      st::FindNextInspectableActor(roster, 12, 1);
  REQUIRE(next.has_value());
  CHECK(*next == 1);
  CHECK(roster.probes == std::vector<std::int32_t>{13, 14, 0, 1});

  const std::optional<std::int32_t> back =
      st::FindNextInspectableActor(roster, 1, 14);
  REQUIRE(back.has_value());
  CHECK(*back == 1);
}

TEST_CASE("Next inspectable actor refuses a negative slot and an empty cycle",
          "[status-switching]") {
  FakeRoster roster;
  CHECK_FALSE(st::FindNextInspectableActor(roster, 0, -1).has_value());
  CHECK(roster.probes.empty());
  CHECK_FALSE(st::FindNextInspectableActor(roster, 3, 1).has_value());
  CHECK(roster.probes.size() == st::kInspectableActorSlots);
}

TEST_CASE("Actor switch request names the actor and picks the direction",
          "[status-switching]") {
  FakeSwitchHost host;
  st::ActorSwitchState state{.phase = sp::kComplete, .selected_actor = 0};
  CHECK_FALSE(st::AnimateActorSwitch(state, 0x13, host));
  CHECK(state.phase == sp::kMenuLoadingReverse);
  CHECK(state.selected_actor == 3);
  CHECK(host.calls == Calls{"Name 2 4 180 18 140 8 true"});

  host.calls.clear();
  CHECK_FALSE(st::AnimateActorSwitch(state, 0x2, host));
  CHECK(state.phase == sp::kMenuLoaded);
  CHECK(state.selected_actor == 2);
  CHECK(host.calls == Calls{"Sound", "Name 1 4 180 18 140 8 true"});
}

TEST_CASE("Forward actor switch slides the model out", "[status-switching]") {
  FakeSwitchHost host;
  st::ActorSwitchState state{
      .phase = sp::kMenuLoaded, .selected_actor = 2, .animation_step = 128};
  CHECK_FALSE(st::AnimateActorSwitch(state, 0, host));
  CHECK(state.animation_step == 160);
  CHECK(state.draw_background_first == 1);
  CHECK(host.calls.empty());
}

TEST_CASE("Forward actor switch reloads the model off screen",
          "[status-switching]") {
  FakeSwitchHost host;
  st::ActorSwitchState state{.phase = sp::kMenuLoaded,
                             .selected_actor = 2,
                             .animation_step = 160,
                             .draw_background_first = 1};
  CHECK_FALSE(st::AnimateActorSwitch(state, 0, host));
  CHECK(host.calls == Calls{"Release", "Load 1"});
  CHECK(state.draw_background_first == 0);
  CHECK(state.fade_screen == 1);
  CHECK(state.delay_screen_update == 1);
  CHECK(state.animation_step == -160);
  CHECK(state.phase == sp::kAnimateForward);
}

TEST_CASE("Forward actor switch slides in and completes",
          "[status-switching]") {
  FakeSwitchHost host;
  st::ActorSwitchState state{.phase = sp::kAnimateForward,
                             .selected_actor = 2,
                             .animation_step = -32,
                             .fade_screen = 1};
  CHECK_FALSE(st::AnimateActorSwitch(state, 0, host));
  CHECK(st::AnimateActorSwitch(state, 0, host));
  CHECK(state.fade_screen == 0);
  CHECK(state.phase == sp::kComplete);
}

TEST_CASE("Reverse actor switch slides the other way and stays complete",
          "[status-switching]") {
  FakeSwitchHost host;
  st::ActorSwitchState state{.phase = sp::kMenuLoadedReverse,
                             .selected_actor = 1,
                             .animation_step = -160};
  CHECK_FALSE(st::AnimateActorSwitch(state, 0, host));
  CHECK(host.calls == Calls{"Release", "Load 0"});
  CHECK(state.fade_screen == 0);
  CHECK(state.animation_step == 160);
  CHECK(state.phase == sp::kAnimateReverse);
  state.animation_step = 0;
  CHECK(st::AnimateActorSwitch(state, 0, host));
  CHECK(state.draw_background_first == 0);
  CHECK(st::AnimateActorSwitch(state, 0, host));
  state.phase = sp::kComplete + 1;
  CHECK_FALSE(st::AnimateActorSwitch(state, 0, host));
}

TEST_CASE("Unanalyzed value draws question marks with an optional maximum",
          "[status-switching]") {
  FakeVitalsHost host;
  st::DrawUnanalyzedValue(host, 0x100040, false);
  CHECK(host.calls == Calls{"Sprite 0xf0040 0xa0007 0x37f400ea",
                            "Sprite 0xf003a 0xa0007 0x37f400ea",
                            "Sprite 0xf0034 0xa0007 0x37f400ea"});
  host.calls.clear();
  st::DrawUnanalyzedValue(host, 0x100040, true);
  REQUIRE(host.calls.size() == 7);
  CHECK(host.calls[0] == "Sprite 0x100040 0x90006 0x37f400e4");
  CHECK(host.calls[3] == "Value 2 0x100031 0");
  CHECK(host.calls[4] == "Sprite 0xf002a 0xa0007 0x37f400ea");
}

TEST_CASE("Vital bar rounds up and fills an unanalyzed or empty gauge",
          "[status-switching]") {
  FakeVitalsHost host;
  st::DrawVitalBar(host, 0x101, 1, 100, 0x50);
  st::DrawVitalBar(host, 0x002, 1, 100, 0x50);
  st::DrawVitalBar(host, 0x100, 5, 0, 0x50);
  st::DrawVitalBar(host, 0x000, 5, 0, 0x50);
  st::DrawVitalBar(host, 0x100, -100, 100, 0x50);
  CHECK(host.calls == Calls{"Gauge 1 1 0x50", "Gauge 2 64 0x50",
                            "Gauge 0 0 0x50", "Gauge 0 64 0x50",
                            "Gauge 0 -63 0x50"});
}

TEST_CASE("Vitals panel draws the selected actor once fully shown",
          "[status-switching]") {
  FakeVitalsHost host;
  st::VitalsPanelState state{.phase = vp::kIdle, .slide = 4};
  const st::VitalsFrame frame{.selected = Analyzed(),
                              .previous = std::nullopt,
                              .labels = {.positions = {0x10, 0x20, 0x30},
                                         .widths = {0x80, 2, 3},
                                         .texture_u = {0xF0, 5, 6}}};
  REQUIRE(st::DrawVitals(state, frame, host));
  REQUIRE(host.calls.size() == 13);
  CHECK(host.calls[0] == "Value 0 0xa0042 100");
  CHECK(host.calls[2] == "Value 1 0x9001b 50");
  CHECK(host.calls[6] == "Value 1 0x190040 30");
  CHECK(host.calls[7] == "Label 384 0x10 0x90080 0x37f600f0");
  CHECK(host.calls[11] == "Gauge 1 0 0x120050");
  CHECK(host.calls[12] == "Gauge 2 20 0x16000a");
}

TEST_CASE("Vitals panel slides the previous actor out before the swap",
          "[status-switching]") {
  FakeVitalsHost host;
  st::VitalsPanelState state{.phase = vp::kSwapping, .slide = 2, .delay = 1};
  st::ActorVitals previous = Analyzed();
  previous.analyzed = false;
  const st::VitalsFrame frame{.selected = Analyzed(), .previous = previous};
  REQUIRE(st::DrawVitals(state, frame, host));
  CHECK(state.slide == 1);
  CHECK(host.calls.front() == "Sprite 0xfff20042 0x90006 0x37f400e4");
  CHECK(host.calls.back() == "Gauge 2 64 0xfffe000a");

  state.slide = 2;
  const st::VitalsFrame orphan{.selected = Analyzed(),
                               .previous = std::nullopt};
  CHECK_FALSE(st::DrawVitals(state, orphan, host));
  CHECK(state.slide == 1);
  CHECK_FALSE(st::DrawVitals(state, st::VitalsFrame{}, host));
  CHECK(state.slide == 1);
}

TEST_CASE("Vitals panel hiding shows the HUD again", "[status-switching]") {
  FakeVitalsHost host;
  const st::VitalsFrame frame{.selected = Analyzed(), .previous = std::nullopt};
  st::VitalsPanelState state{.phase = vp::kHiding, .slide = 0, .delay = 0};
  CHECK(st::DrawVitals(state, frame, host));
  CHECK(state.phase == vp::kIdle);
  CHECK(host.calls == Calls{"Hud 0"});
}

TEST_CASE("Vitals panel waits for the HUD and then shows",
          "[status-switching]") {
  FakeVitalsHost host;
  const st::VitalsFrame frame{.selected = Analyzed(), .previous = std::nullopt};
  st::VitalsPanelState state{.phase = vp::kAwaitingHud, .slide = 0, .delay = 1};
  CHECK(st::DrawVitals(state, frame, host));
  CHECK(state.delay == 1);
  st::VitalsFrame hidden = frame;
  hidden.hud_slide_offset = st::kHudFullyHiddenOffset;
  CHECK(st::DrawVitals(state, hidden, host));
  CHECK(state.delay == 0);
  CHECK(st::DrawVitals(state, hidden, host));
  CHECK(state.phase == vp::kShowing);
  CHECK(st::DrawVitals(state, frame, host));
  CHECK(state.slide == 1);
}

TEST_CASE("Limb and equipment row counts stop at the first unnamed limb",
          "[status-switching]") {
  constexpr std::array<std::uint8_t, 6> kThree{1, 2, 3, 0, 5, 6};
  constexpr std::array<std::uint8_t, 8> kAll{1, 1, 1, 1, 1, 1, 1, 1};
  CHECK(st::LimbCount(kThree) == 3);
  CHECK(st::LimbCount(kAll) == 6);
  CHECK(st::LimbCount({}) == 0);
  CHECK(st::EquipmentRowCount(kThree, 0) == 5);
  CHECK(st::EquipmentRowCount(kThree, 0x8001) == 6);
  CHECK(st::EquipmentRowCount(kAll, 1) == 9);
}

namespace {
struct LiveActorSlot {
  explicit LiveActorSlot(int slot)
      : slot(slot),
        saved(Battle_actors[slot]),
        saved_selected(Menu_Status_selectedActor) {
    actor->stats = stats.get();
    Battle_actors[slot] = actor.get();
  }
  LiveActorSlot(const LiveActorSlot&) = delete;
  LiveActorSlot& operator=(const LiveActorSlot&) = delete;
  LiveActorSlot(LiveActorSlot&&) = delete;
  LiveActorSlot& operator=(LiveActorSlot&&) = delete;
  ~LiveActorSlot() {
    Battle_actors[slot] = saved;
    Menu_Status_selectedActor = saved_selected;
  }

  int slot;
  Battle_Actor* saved;
  int saved_selected;
  std::unique_ptr<Battle_ActorStats> stats =
      std::make_unique<Battle_ActorStats>();
  std::unique_ptr<Battle_Actor> actor = std::make_unique<Battle_Actor>();
};

// A local menu table and the switch globals, restored when the test ends.
struct LiveSwitchScene {
  LiveSwitchScene() {
    Menu_Framework_menuItems = rows.data();
    st::ResetLiveActorSwitchPhase();
  }
  LiveSwitchScene(const LiveSwitchScene&) = delete;
  LiveSwitchScene& operator=(const LiveSwitchScene&) = delete;
  LiveSwitchScene(LiveSwitchScene&&) = delete;
  LiveSwitchScene& operator=(LiveSwitchScene&&) = delete;
  ~LiveSwitchScene() {
    st::ResetLiveActorSwitchPhase();
    Menu_Framework_menuItems = saved_rows;
    Menu_Status_animationStep = saved_step;
    Menu_Status_drawBackgroundFirst = saved_first;
    Menu_Status_fadeScreen = saved_fade;
    Menu_Status_delayScreenUpdate = saved_delay;
  }

  std::array<Menu_Framework_MenuRow, VS::Menu::Framework::kRowCount> rows{};
  Menu_Framework_MenuRow* saved_rows = Menu_Framework_menuItems;
  int saved_step = Menu_Status_animationStep;
  int saved_first = Menu_Status_drawBackgroundFirst;
  int saved_fade = Menu_Status_fadeScreen;
  char saved_delay = Menu_Status_delayScreenUpdate;
};
}  // namespace

TEST_CASE("Live limb and row counts read the actor's stats record",
          "[status-switching-live]") {
  const LiveActorSlot live(3);
  live.stats->limbs[0].nameIndex = 4;
  live.stats->limbs[1].nameIndex = 9;
  live.stats->accessory.accessory.id = 7;
  Menu_Status_selectedActor = 4;
  CHECK(st::LiveLimbCount(3) == 2);
  CHECK(st::LiveEquipmentRowCount() == 5);
  CHECK(st::LiveLimbCount(-1) == 0);
  CHECK(st::LiveLimbCount(42) == 0);
  Battle_actors[3] = nullptr;
  CHECK(st::LiveLimbCount(3) == 0);
  CHECK(st::LiveEquipmentRowCount() == 0);
}

TEST_CASE("Live vitals transition and switch keep the copied globals",
          "[status-switching-live]") {
  const char saved_state = Menu_Status_renderBasicStatsState;
  const char saved_shown = Menu_Status_initializedActor;
  const char saved_delay = Menu_Status_basicStatsAnimDelay;
  const int saved_step = Menu_Status_animationStep;
  const int saved_first = Menu_Status_drawBackgroundFirst;
  Menu_Status_initializedActor = 2;
  st::LiveBeginVitalsTransition(0);
  CHECK(Menu_Status_renderBasicStatsState == vp::kHiding);
  CHECK(Menu_Status_basicStatsAnimDelay == 4);
  CHECK(Menu_Status_initializedActor == 0);

  st::ResetLiveActorSwitchPhase();
  Menu_Status_animationStep = 0;
  CHECK(st::LiveAnimateActorSwitch(0) == 0);
  CHECK(Menu_Status_animationStep == 32);
  CHECK(Menu_Status_drawBackgroundFirst == 1);

  Menu_Status_renderBasicStatsState = saved_state;
  Menu_Status_initializedActor = saved_shown;
  Menu_Status_basicStatsAnimDelay = saved_delay;
  Menu_Status_animationStep = saved_step;
  Menu_Status_drawBackgroundFirst = saved_first;
}

TEST_CASE("Live actor switch names only an actor with a record",
          "[status-switching-live]") {
  LiveSwitchScene scene;
  Menu_Framework_MenuRow& row = scene.rows[4];
  const LiveActorSlot named(1);
  const LiveActorSlot absent(2);
  Battle_actors[2] = nullptr;

  row.x = 7;
  CHECK(st::LiveAnimateActorSwitch(0x10) == 0);
  CHECK(Menu_Status_selectedActor == 0);
  CHECK(row.x == 7);
  CHECK(st::LiveAnimateActorSwitch(3) == 0);
  CHECK(Menu_Status_selectedActor == 3);
  CHECK(row.x == 7);
  CHECK(st::LiveAnimateActorSwitch(2) == 0);
  CHECK(Menu_Status_selectedActor == 2);
  CHECK(row.x == 180);
}

TEST_CASE("Live vitals refuse a selected actor without a record",
          "[status-switching-live]") {
  const int saved_selected = Menu_Status_selectedActor;
  const char saved_slide = Menu_Status_basicStatsAnimState;
  Menu_Status_selectedActor = 0;
  Menu_Status_basicStatsAnimState = 3;
  st::LiveDrawVitals();
  CHECK(Menu_Status_basicStatsAnimState == 3);
  Menu_Status_selectedActor = saved_selected;
  Menu_Status_basicStatsAnimState = saved_slide;
}
