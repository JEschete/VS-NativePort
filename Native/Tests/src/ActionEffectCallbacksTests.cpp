#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/HudState.hpp"
#include "Effects/ActionEffectCallbacks.hpp"
#include "Field/GameMode.hpp"
#include "Render/PaletteAnimation.hpp"
#include "State/SceneTimer.hpp"

namespace {
namespace fx = VS::Effects;

// Records every service call in order with the state it saw.
class FakeHost final : public fx::ActionEffectCallbackHost {
 public:
  std::int32_t tinted = 0;
  std::int32_t tick_speed = 0;
  VS::Field::FieldControlMode mode = VS::Field::FieldControlMode::kNone;
  std::uint32_t busy = 0;
  std::vector<std::int32_t> advanced;
  std::vector<fx::ActorColorTransform> transforms;
  std::vector<std::int32_t> tinted_at_transform;
  std::vector<fx::PrimaryEffectRequest> begun;
  std::vector<std::pair<std::int32_t, std::int32_t>> begin_arguments;
  std::vector<VS::Battle::HudVisibility> hud;
  std::vector<bool> minimap;
  std::string order;

  void BeginActionEffect(const fx::PrimaryEffectRequest& request,
                         std::int32_t action_variant,
                         std::int32_t variant_argument) override {
    begun.push_back(request);
    begin_arguments.emplace_back(action_variant, variant_argument);
    order += 'B';
  }
  void RequestActorAnimationAdvance(std::int32_t actor) override {
    advanced.push_back(actor);
    order += 'A';
  }
  void TransformActorColors(const fx::ActorColorTransform& transform) override {
    transforms.push_back(transform);
    tinted_at_transform.push_back(tinted);
    order += 'P';
  }
  std::int32_t TintedActor() override { return tinted; }
  void SetTintedActor(std::int32_t actor) override {
    tinted = actor;
    order += 'R';
  }
  void SetFieldControlMode(VS::Field::FieldControlMode value) override {
    mode = value;
    order += 'M';
  }
  void SetTickSpeed(std::int32_t value) override {
    tick_speed = value;
    order += 'T';
  }
  void SetHudVisibility(VS::Battle::HudVisibility visibility) override {
    hud.push_back(visibility);
    order += 'H';
  }
  void SetMinimapVisible(bool visible) override {
    minimap.push_back(visible);
    order += 'N';
  }
  void EnterFieldExplorationMode() override { order += 'E'; }
  std::uint32_t EffectBusyFlags() override { return busy; }
};
}  // namespace

TEST_CASE("Actor effect animation signal passes the actor unchanged",
          "[effects][action-callbacks]") {
  FakeHost host;
  fx::SignalActorEffectAnimation(-7, host);
  fx::SignalActorEffectAnimation(0x10000, host);
  CHECK(host.advanced == std::vector<std::int32_t>{-7, 0x10000});
  CHECK(host.order == "AA");
}

TEST_CASE("Red action tint remembers the actor before the fade",
          "[effects][action-callbacks]") {
  FakeHost host;
  host.tinted = 3;
  fx::TintActionActorRed(5, host);
  CHECK(host.tinted == 5);
  CHECK(host.order == "RP");
  REQUIRE(host.transforms.size() == 1);
  CHECK(host.tinted_at_transform[0] == 5);
  CHECK(host.transforms[0] ==
        fx::ActorColorTransform{
            .mode = VS::Render::ColorTransform::kFadeThenRestore,
            .duration = 8,
            .actor = 5,
            .red_offset = 0x10});
}

TEST_CASE("Tint restore transforms the remembered actor then resets it",
          "[effects][action-callbacks]") {
  FakeHost host;
  host.tinted = 9;
  fx::RestoreActionActorTint(host);
  CHECK(host.tinted == fx::kNoTintedActor);
  CHECK(host.order == "PR");
  REQUIRE(host.transforms.size() == 1);
  CHECK(host.tinted_at_transform[0] == 9);
  CHECK(
      host.transforms[0] ==
      fx::ActorColorTransform{.mode = VS::Render::ColorTransform::kRestoreBase,
                              .duration = 4,
                              .actor = 9});
}

TEST_CASE("Tint and restore pair round trips the tinted actor",
          "[effects][action-callbacks]") {
  FakeHost host;
  fx::TintActionActorRed(-2, host);
  fx::RestoreActionActorTint(host);
  REQUIRE(host.transforms.size() == 2);
  CHECK(host.transforms[0].actor == -2);
  CHECK(host.transforms[1].actor == -2);
  CHECK(host.tinted == 0);
  // A second restore without a tint restores actor 0.
  fx::RestoreActionActorTint(host);
  REQUIRE(host.transforms.size() == 3);
  CHECK(host.transforms[2].actor == 0);
}

TEST_CASE("Standalone effect request names actor 0 at the origin",
          "[effects][action-callbacks]") {
  const fx::PrimaryEffectRequest request = fx::StandaloneEffectRequest(0xBEEF);
  const fx::EffectEndpoint player{.kind = fx::EffectEndpointKind::kActor};
  CHECK(request.effect_id == 0xBEEF);
  CHECK(request.source == player);
  CHECK(request.target == player);
  CHECK(request.other_count == 0);
  CHECK(request.position == fx::EffectPoint{});
}

TEST_CASE("Standalone effect start enters its mode then hides the HUD",
          "[effects][action-callbacks]") {
  FakeHost host;
  host.tick_speed = VS::State::kNormalTickSpeed;
  fx::BeginAshleyStandaloneEffect(0x42, host);
  CHECK(host.order == "MBTHN");
  CHECK(host.mode == VS::Field::FieldControlMode::kStandaloneEffect);
  CHECK(host.tick_speed == VS::State::kSlowedTickSpeed);
  CHECK(host.hud == std::vector<VS::Battle::HudVisibility>{
                        VS::Battle::HudVisibility::kHidden});
  CHECK(host.minimap == std::vector<bool>{false});
  REQUIRE(host.begun.size() == 1);
  CHECK(host.begun[0] == fx::StandaloneEffectRequest(0x42));
  CHECK(host.begin_arguments[0] == std::pair<std::int32_t, std::int32_t>{0, 0});
}

TEST_CASE("Standalone effect end restores exploration HUD minimap and tick",
          "[effects][action-callbacks]") {
  FakeHost host;
  fx::BeginAshleyStandaloneEffect(1, host);
  host.order.clear();
  fx::EndAshleyStandaloneEffect(host);
  CHECK(host.order == "ETHN");
  CHECK(host.tick_speed == VS::State::kNormalTickSpeed);
  CHECK(host.hud == std::vector<VS::Battle::HudVisibility>{
                        VS::Battle::HudVisibility::kHidden,
                        VS::Battle::HudVisibility::kShown});
  CHECK(host.minimap == std::vector<bool>{false, true});
  CHECK(host.begun.size() == 1);
}

TEST_CASE("Action effect busy flags observe only the two effect bits",
          "[effects][action-callbacks]") {
  FakeHost host;
  host.busy = 0;
  CHECK(fx::ActionEffectBusyFlags(host) == 0);
  host.busy = 1;
  CHECK(fx::ActionEffectBusyFlags(host) == 1);
  host.busy = 2;
  CHECK(fx::ActionEffectBusyFlags(host) == 2);
  host.busy = 0xFFFFFFFCU;
  CHECK(fx::ActionEffectBusyFlags(host) == 0);
  host.busy = 0x80000007U;
  CHECK(fx::ActionEffectBusyFlags(host) == 3);
}
