#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Effects/ActionEffectCallbacks.hpp"
#include "Effects/ScriptedEffectTriggers.hpp"

namespace {
namespace fx = VS::Effects;

// Records every service call in order.
class FakeHost final : public fx::ScriptedEffectTriggerHost {
 public:
  std::array<bool, fx::kScriptedEffectActorSlots> loaded{};
  std::vector<std::size_t> scanned;
  std::vector<fx::PrimaryEffectRequest> begun;
  std::vector<std::pair<std::int32_t, std::int32_t>> begin_arguments;
  std::vector<std::int32_t> started_tracks;
  std::int32_t phase = 0;
  std::int32_t can_start = 0;
  std::string order;

  void BeginActionEffect(const fx::PrimaryEffectRequest& request,
                         std::int32_t action_variant,
                         std::int32_t variant_argument) override {
    begun.push_back(request);
    begin_arguments.emplace_back(action_variant, variant_argument);
    order += 'B';
  }
  bool ActorLoaded(std::size_t slot) override {
    scanned.push_back(slot);
    return slot < loaded.size() && loaded.at(slot);
  }
  void StartEffectTrack(std::int32_t track) override {
    started_tracks.push_back(track);
    order += 'S';
  }
  std::int32_t PrimaryEffectPhase() override {
    order += 'P';
    return phase;
  }
  std::int32_t CanStartEffectTrack() override {
    order += 'C';
    return can_start;
  }
};

[[nodiscard]] std::vector<std::uint8_t> Others(
    const fx::PrimaryEffectRequest& request) {
  return {request.others.begin(), request.others.begin() + request.other_count};
}
}  // namespace

TEST_CASE("Scripted actor effect targets one actor from the origin",
          "[effects][actor-effect-triggers]") {
  FakeHost host;
  fx::StartScriptedActorEffect(0x1234, 3, host);
  REQUIRE(host.begun.size() == 1);
  const fx::PrimaryEffectRequest& request = host.begun.front();
  CHECK(request.effect_id == 0x1234);
  CHECK(request.source ==
        fx::EffectEndpoint{.kind = fx::EffectEndpointKind::kPosition});
  CHECK(request.target ==
        fx::EffectEndpoint{
            .kind = fx::EffectEndpointKind::kActor, .actor = 3, .part = 0});
  CHECK(request.other_count == 0);
  CHECK(request.position == fx::EffectPoint{});
  CHECK(host.begin_arguments ==
        std::vector<std::pair<std::int32_t, std::int32_t>>{
            {fx::kPrimaryRequestVariant, fx::kPrimaryRequestArgument}});
  CHECK(host.order == "B");
}

TEST_CASE("Scripted actor effect includes the other loaded actors in order",
          "[effects][actor-effect-triggers]") {
  FakeHost host;
  for (const std::size_t slot : {0U, 2U, 5U, 9U, 15U}) {
    host.loaded.at(slot) = true;
  }
  fx::StartScriptedActorEffect(7, 5, host);
  REQUIRE(host.begun.size() == 1);
  CHECK(Others(host.begun.front()) == std::vector<std::uint8_t>{0, 2, 9, 15});
  CHECK(host.scanned.size() == fx::kScriptedEffectActorSlots);
}

TEST_CASE("Scripted actor effect compares the full target word",
          "[effects][actor-effect-triggers]") {
  FakeHost host;
  host.loaded.fill(true);
  // 0x102 names actor 2 in the request but excludes no slot from the others.
  fx::StartScriptedActorEffect(1, 0x102, host);
  fx::StartScriptedActorEffect(1, -1, host);
  REQUIRE(host.begun.size() == 2);
  CHECK(host.begun.at(0).target.actor == 2);
  CHECK(host.begun.at(0).other_count == fx::kScriptedEffectActorSlots);
  CHECK(host.begun.at(1).target.actor == 0xFF);
  CHECK(host.begun.at(1).other_count == fx::kScriptedEffectActorSlots);
}

TEST_CASE("Scripted actor effect with no other actor has no others",
          "[effects][actor-effect-triggers]") {
  FakeHost host;
  host.loaded.at(4) = true;
  const fx::PrimaryEffectRequest request =
      fx::ScriptedActorEffectRequest(9, 4, host);
  CHECK(request.other_count == 0);
  CHECK(request.others == std::array<std::uint8_t, fx::kPrimaryEffectOthers>{});
  CHECK(host.order.empty());
}

TEST_CASE("Scripted effect trigger release starts the track",
          "[effects][actor-effect-triggers]") {
  FakeHost host;
  fx::ReleaseScriptedEffectTrigger(6, host);
  fx::ReleaseScriptedEffectTrigger(-1, host);
  CHECK(host.started_tracks == std::vector<std::int32_t>{6, -1});
  CHECK(host.order == "SS");
}

TEST_CASE("Scripted effect playback state is the primary effect phase",
          "[effects][actor-effect-triggers]") {
  FakeHost host;
  host.phase = 3;
  CHECK(fx::ScriptedEffectPlaybackState(host) == 3);
  host.phase = -2;
  CHECK(fx::ScriptedEffectPlaybackState(host) == -2);
  CHECK(host.order == "PP");
}

TEST_CASE("Scripted effect trigger is ready for any nonzero answer",
          "[effects][actor-effect-triggers]") {
  FakeHost host;
  CHECK_FALSE(fx::IsScriptedEffectTriggerReady(host));
  host.can_start = 1;
  CHECK(fx::IsScriptedEffectTriggerReady(host));
  host.can_start = -0x10000;
  CHECK(fx::IsScriptedEffectTriggerReady(host));
  CHECK(host.order == "CCC");
}
