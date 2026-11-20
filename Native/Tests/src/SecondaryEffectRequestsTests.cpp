#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Effects/SecondaryEffectRequests.hpp"

namespace {
namespace fx = VS::Effects;

struct BoneCall {
  std::uint8_t actor{};
  std::int8_t limb{};
  std::int32_t kind{};
};

// Records every service call in order: 'B' begin, 'T' bone tag, 'S' sounds.
class FakeWorld final : public fx::ActionImpactWorld {
 public:
  std::optional<fx::QueuedEffectAction> action;
  std::vector<fx::ImpactTarget> targets;
  std::uint8_t prerequisite = 0;
  std::int32_t bone_tag = 0;
  std::uint32_t busy = 0;
  std::vector<std::pair<fx::SingleTargetEffectRequest, std::int32_t>> begun;
  std::vector<BoneCall> bones;
  std::vector<std::int32_t> sounds;
  std::vector<char> order;

  void Begin(const fx::SingleTargetEffectRequest& request,
             std::int32_t parameter) override {
    begun.emplace_back(request, parameter);
    order.push_back('B');
  }
  std::uint32_t BusyFlags() override { return busy; }
  std::optional<fx::QueuedEffectAction> QueuedAction() override {
    return action;
  }
  std::optional<fx::ImpactTarget> QueuedTarget(std::size_t index) override {
    if (index >= targets.size()) return std::nullopt;
    return targets[index];
  }
  std::uint8_t HitPrerequisite(std::uint16_t /*action*/) override {
    return prerequisite;
  }
  std::int32_t BoneTag(std::uint8_t actor, std::int8_t limb,
                       std::int32_t kind) override {
    bones.push_back({actor, limb, kind});
    order.push_back('T');
    return bone_tag;
  }
  void PlayWeaponHitSounds(std::int32_t attacker) override {
    sounds.push_back(attacker);
    order.push_back('S');
  }
};

fx::QueuedEffectAction Action(std::uint16_t id, std::uint16_t count = 1) {
  return {
      .action_id = id, .affinity = 7, .source_actor = 3, .target_count = count};
}

fx::ImpactTarget ActorTarget(std::uint8_t impact) {
  return {.actor = 5, .limb = -2, .threshold = 9, .impact = impact};
}

fx::ImpactTarget PositionTarget() {
  return {.actor = 0x34,
          .limb = 0x12,
          .threshold = 0x78,
          .impact = 0x56,
          .hp = -100,
          .mp = 0x1234,
          .at_position = true};
}

// The effect and slot byte of every begun request.
std::vector<std::pair<std::uint16_t, std::uint8_t>> Effects(
    const FakeWorld& world) {
  std::vector<std::pair<std::uint16_t, std::uint8_t>> effects;
  for (const auto& [request, parameter] : world.begun) {
    effects.emplace_back(request.effect_id, request.target.slot);
  }
  return effects;
}
}  // namespace

TEST_CASE("Action hit effect asks for the hit limb bone tag first",
          "[effects][secondary-requests]") {
  FakeWorld world;
  world.action = Action(0x30);
  REQUIRE(fx::QueueActionHitEffect(0x41, ActorTarget(2), 0xC8, world));
  REQUIRE(world.bones.size() == 1);
  CHECK(world.bones[0].actor == 5);
  CHECK(world.bones[0].limb == -2);
  CHECK(world.bones[0].kind == fx::kHitEffectBoneTagKind);
  CHECK(world.order == std::vector<char>{'T', 'B'});
}

TEST_CASE("Action hit effect at an actor names its limb bone tag",
          "[effects][secondary-requests]") {
  FakeWorld world;
  world.action = Action(0x30);
  world.bone_tag = 0x1FE;
  REQUIRE(fx::QueueActionHitEffect(0x41, ActorTarget(2), 0xC8, world));
  REQUIRE(world.begun.size() == 1);
  const auto& [request, parameter] = world.begun[0];
  CHECK(parameter == fx::kSecondaryRequestParameter);
  CHECK(request.effect_id == 0x41);
  CHECK(request.source ==
        fx::EffectEndpoint{.kind = fx::EffectEndpointKind::kActor, .actor = 3});
  CHECK(request.target ==
        fx::EffectEndpoint{.kind = fx::EffectEndpointKind::kActor,
                           .color_preset = 7,
                           .actor = 5,
                           .slot = 0xC8,
                           .part = 0xFE});
}

TEST_CASE("Action hit effect at a position reads its hit word as a point",
          "[effects][secondary-requests]") {
  FakeWorld world;
  world.action = Action(0x30);
  REQUIRE(fx::QueueActionHitEffect(0x26, PositionTarget(), 9, world));
  CHECK(world.bones.empty());
  REQUIRE(world.begun.size() == 1);
  const fx::EffectEndpoint& target = world.begun[0].first.target;
  CHECK(target.kind == fx::EffectEndpointKind::kPosition);
  CHECK(target.color_preset == 7);
  CHECK(target.position ==
        fx::EffectPoint{.x = 0x1234, .y = 0x5678, .z = -100, .w = 0x1234});
  CHECK(target.slot == 0);
  CHECK(world.begun[0].first.source.actor == 3);
}

TEST_CASE("Action hit effect variant becomes the target color preset",
          "[effects][secondary-requests]") {
  fx::QueuedEffectAction action = Action(1);
  action.affinity = 0xFF;
  const auto at_actor =
      fx::ActionHitEffectRequest(1, action, ActorTarget(0), 0, 0x77);
  const auto at_position =
      fx::ActionHitEffectRequest(1, action, PositionTarget(), 0, 0);
  CHECK(at_actor.target.color_preset == 0xFF);
  CHECK(at_position.target.color_preset == 0xFF);
  CHECK(at_actor.source.color_preset == 0);
  CHECK(at_actor.target.part == 0x77);
}

TEST_CASE("Action hit effect without a queued action begins nothing",
          "[effects][secondary-requests]") {
  FakeWorld world;
  CHECK_FALSE(fx::QueueActionHitEffect(1, ActorTarget(0), 0, world));
  CHECK(world.begun.empty());
  CHECK(world.bones.empty());
}

TEST_CASE("Impact effects of defined actions follow the impact kind",
          "[effects][secondary-requests]") {
  FakeWorld world;
  world.action = Action(0x10, 8);
  for (std::uint8_t impact = 0; impact < 8; ++impact) {
    world.targets.push_back(ActorTarget(impact));
  }
  CHECK(fx::QueueActionImpactEffects(4, world) == 1);
  using Hit = std::pair<std::uint16_t, std::uint8_t>;
  CHECK(
      Effects(world) ==
      std::vector<Hit>{
          {1, 0}, {0x27, 1}, {1, 2}, {0x26, 3}, {0x26, 4}, {1, 5}, {0x26, 6}});
  CHECK(world.sounds == std::vector<std::int32_t>(8, 4));
}

TEST_CASE("Impact effects of later defined actions keep the action effect",
          "[effects][secondary-requests]") {
  FakeWorld world;
  world.action = Action(fx::kDefaultHitActions, 2);
  world.targets = {ActorTarget(0), ActorTarget(5)};
  fx::QueueActionImpactEffects(0, world);
  using Hit = std::pair<std::uint16_t, std::uint8_t>;
  CHECK(Effects(world) == std::vector<Hit>{{fx::kDefaultHitActions, 0},
                                           {fx::kDefaultHitActions, 5}});
  world.begun.clear();
  world.action = Action(fx::kDefaultHitActions - 1, 1);
  fx::QueueActionImpactEffects(0, world);
  CHECK(Effects(world) == std::vector<Hit>{{fx::kDefaultHitEffect, 0}});
}

TEST_CASE(
    "Impact effects keep the action for its own prerequisite and late actions",
    "[effects][secondary-requests]") {
  FakeWorld world;
  world.prerequisite = fx::kOwnEffectPrerequisite;
  world.action = Action(2, 2);
  world.targets = {ActorTarget(1), ActorTarget(9)};
  fx::QueueActionImpactEffects(0, world);
  using Hit = std::pair<std::uint16_t, std::uint8_t>;
  CHECK(Effects(world) == std::vector<Hit>{{2, 1}, {2, 9}});
  world.begun.clear();
  world.prerequisite = 0;
  world.action = Action(fx::kImpactDefinedActions, 1);
  world.targets = {ActorTarget(9)};
  fx::QueueActionImpactEffects(0, world);
  CHECK(Effects(world) == std::vector<Hit>{{fx::kImpactDefinedActions, 9}});
}

TEST_CASE("Impact effects at a position use the position effect at limb zero",
          "[effects][secondary-requests]") {
  FakeWorld world;
  world.prerequisite = fx::kOwnEffectPrerequisite;
  world.action = Action(0x50, 1);
  world.targets = {PositionTarget()};
  fx::QueueActionImpactEffects(2, world);
  REQUIRE(world.begun.size() == 1);
  CHECK(world.begun[0].first.effect_id == fx::kPositionHitEffect);
  CHECK(world.begun[0].first.target.kind == fx::EffectEndpointKind::kPosition);
  CHECK(world.bones.empty());
}

TEST_CASE("Impact effects play the weapon sounds once per target",
          "[effects][secondary-requests]") {
  FakeWorld world;
  world.action = Action(0x10, 3);
  world.targets = {ActorTarget(7), ActorTarget(0), PositionTarget()};
  fx::QueueActionImpactEffects(6, world);
  CHECK(world.order == std::vector<char>{'S', 'T', 'B', 'S', 'B', 'S'});
  CHECK(world.sounds == std::vector<std::int32_t>{6, 6, 6});
}

TEST_CASE("Impact effects stop at the target count or a missing target",
          "[effects][secondary-requests]") {
  FakeWorld world;
  world.action = Action(0x10, 0);
  world.targets = {ActorTarget(0)};
  CHECK(fx::QueueActionImpactEffects(1, world) == 1);
  CHECK(world.order.empty());
  world.action = Action(0x10, 4);
  CHECK(fx::QueueActionImpactEffects(1, world) == 1);
  CHECK(world.sounds.size() == 1);
  world.action.reset();
  world.sounds.clear();
  CHECK(fx::QueueActionImpactEffects(1, world) == 1);
  CHECK(world.sounds.empty());
}

TEST_CASE("Ground plane impact effect sits at the floor height",
          "[effects][secondary-requests]") {
  FakeWorld world;
  fx::SpawnGroundPlaneImpactEffect(-300, 0x7FFF, world);
  REQUIRE(world.begun.size() == 1);
  const auto& [request, parameter] = world.begun[0];
  CHECK(parameter == fx::kSecondaryRequestParameter);
  CHECK(request.effect_id == fx::kGroundPlaneImpactEffect);
  CHECK(request.source ==
        fx::EffectEndpoint{.kind = fx::EffectEndpointKind::kActor, .actor = 0});
  CHECK(request.target.kind == fx::EffectEndpointKind::kPosition);
  CHECK(request.target.position ==
        fx::EffectPoint{.x = -300, .y = 0, .z = 0x7FFF});
}

TEST_CASE("Trap tile effect sits at the tile centre and height",
          "[effects][secondary-requests]") {
  const auto request =
      fx::TrapTileEffectRequest({.tile_x = 3, .tile_z = 0xFF, .height = -64});
  CHECK(request.effect_id == fx::kTrapTileEffect);
  CHECK(request.source.kind == fx::EffectEndpointKind::kActor);
  CHECK(request.target.kind == fx::EffectEndpointKind::kPosition);
  CHECK(request.target.position.x == 3 * 128 + 64);
  CHECK(request.target.position.y == -64);
  // 0xFF * 128 + 64 = 0x7FC0 still fits a signed halfword.
  CHECK(request.target.position.z == 0x7FC0);
}

TEST_CASE("Trap tile effect zeroes the fields the original left unassigned",
          "[effects][secondary-requests]") {
  // #324: the original request left these as stack bytes.
  const auto request =
      fx::TrapTileEffectRequest({.tile_x = 3, .tile_z = 4, .height = -64});
  CHECK(request.source.part == 0);
  CHECK(request.source.slot == 0);
  CHECK(request.source.color_preset == 0);
  CHECK(request.target.part == 0);
  CHECK(request.target.position.w == 0);
}

TEST_CASE("Trap tile effect is suppressed by the secondary busy bit only",
          "[effects][secondary-requests]") {
  FakeWorld world;
  const fx::TrapTile tile{.tile_x = 1, .tile_z = 2, .height = 5};
  world.busy = fx::kTrapEffectBusyBit;
  CHECK_FALSE(fx::SpawnTrapTileEffect(tile, world));
  CHECK(world.begun.empty());
  world.busy = ~fx::kTrapEffectBusyBit;
  CHECK(fx::SpawnTrapTileEffect(tile, world));
  REQUIRE(world.begun.size() == 1);
  CHECK(world.begun[0].first == fx::TrapTileEffectRequest(tile));
}

TEST_CASE("Ability learned effect targets the player part 0xFD",
          "[effects][secondary-requests]") {
  FakeWorld world;
  fx::SpawnAbilityLearnedEffect(world);
  REQUIRE(world.begun.size() == 1);
  const auto& [request, parameter] = world.begun[0];
  CHECK(parameter == fx::kSecondaryRequestParameter);
  CHECK(request.effect_id == fx::kAbilityLearnedEffect);
  CHECK(request.source.kind == fx::EffectEndpointKind::kPosition);
  CHECK(request.target ==
        fx::EffectEndpoint{
            .kind = fx::EffectEndpointKind::kActor, .actor = 0, .part = 0xFD});
}
