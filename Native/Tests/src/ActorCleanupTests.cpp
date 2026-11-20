#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/ActorRecord.hpp"
#include "Actors/AppearanceLive.hpp"
#include "Actors/AppearanceTransition.hpp"
#include "Actors/AppearanceTransitionLive.hpp"
#include "Actors/SkeletonLiveState.hpp"
#include "Actors/WeaponModel.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorSettingsLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/BossOrbitLive.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/OrbitingBoss.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Feedback.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Legacy/Psx.hpp"
#include "State/Timers.hpp"

using VS::Legacy::LoadImage;
using VS::Legacy::RECT;
using VS::Legacy::StoreImage;

namespace {
struct CleanupFixture {
  decltype(VS::Battle::actors) saved_actors = VS::Battle::actors;
  decltype(VS::Battle::Ai::actorAiStates) saved_states =
      VS::Battle::Ai::actorAiStates;
  decltype(VS::Battle::actorMarkers) saved_markers = VS::Battle::actorMarkers;
  VS::Battle::Ai::RoomAiState* saved_room = VS::Battle::Ai::roomAiState;
  decltype(VS::Battle::Ai::tileFlagMap) saved_flags =
      VS::Battle::Ai::tileFlagMap;
  VS::Battle::Ai::ArenaOrbitState* saved_orbit = VS::Battle::Ai::arenaOrbit;
  VS::Battle::Ai::FrameContext saved_frame = VS::Battle::Ai::LiveFrameContext();
  int saved_attribute = VS::Battle::Ai::attributeActorMask;
  int saved_busy = VS::Battle::Ai::candidateEngagedThisUpdate;
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 3072> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  std::array<VS::Battle::Actor, 3> actors{};
  VS::Battle::Ai::RoomAiState room{};
  std::uint16_t flags[32][32]{};

  CleanupFixture() {
    VS::Battle::Ai::roomAiState = &room;
    VS::Battle::Ai::tileFlagMap = flags;
    VS::Battle::Ai::arenaOrbit = nullptr;
    VS::Battle::Ai::attributeActorMask = 0;
    VS::Battle::Ai::candidateEngagedThisUpdate = 0;
    std::fill_n(VS::Battle::actors.begin(), saved_actors.size(), nullptr);
    std::fill_n(VS::Battle::Ai::actorAiStates.begin(), saved_states.size(),
                nullptr);
    const std::array<int, 3> ids{0, 2, 5};
    for (std::size_t index = 0; index < actors.size(); ++index) {
      actors[index].id = ids[index];
      actors[index].kind = 4;
      actors[index].command_flags = 0xFF;
      actors[index].next =
          index + 1 < actors.size() ? &actors[index + 1] : nullptr;
      VS::Battle::actors[ids[index]] = &actors[index];
    }
    room.max_tile_x = room.max_tile_z = 31;
    room.min_tile_x = room.min_tile_z = 0;
    room.engagement_limit = 2;
    VS::Battle::Ai::StoreEngagedMask(room, 0);
    room.engaged_count = 0;
  }

  ~CleanupFixture() {
    std::copy(saved_actors.begin(), saved_actors.end(),
              VS::Battle::actors.begin());
    std::copy(saved_states.begin(), saved_states.end(),
              VS::Battle::Ai::actorAiStates.begin());
    std::copy(saved_markers.begin(), saved_markers.end(),
              VS::Battle::actorMarkers.begin());
    VS::Battle::Ai::roomAiState = saved_room;
    VS::Battle::Ai::tileFlagMap = saved_flags;
    VS::Battle::Ai::arenaOrbit = saved_orbit;
    VS::Battle::Ai::LiveFrameContext() = saved_frame;
    VS::Battle::Ai::attributeActorMask = saved_attribute;
    VS::Battle::Ai::candidateEngagedThisUpdate = saved_busy;
  }

  VS::Battle::Ai::ActorAiState* AllocateState(std::size_t index) {
    auto* state = static_cast<VS::Battle::Ai::ActorAiState*>(
        VS::Core::Memory::AllocateLiveFromLowEnd(
            sizeof(VS::Battle::Ai::ActorAiState)));
    REQUIRE(state != nullptr);
    std::construct_at(state);
    state->actor_id = static_cast<std::uint8_t>(actors[index].id);
    state->actor =
        reinterpret_cast<VS::Battle::Ai::EngagementData*>(&actors[index]);
    state->next_tile.word = 0x00070004;
    VS::Battle::Ai::actorAiStates[state->actor_id] = state;
    VS::Battle::Ai::attributeActorMask |= 1 << state->actor_id;
    return state;
  }
};

void CheckRetiredAiOwnership(const CleanupFixture& fixture,
                             const VS::Battle::Ai::ActorAiState* peer) {
  CHECK(VS::Battle::Ai::actorAiStates[2] == nullptr);
  CHECK(VS::Battle::Ai::LiveFrameContext().actors[2] == nullptr);
  CHECK(VS::Battle::Ai::attributeActorMask == (1 << 5));
  CHECK(fixture.room.engaged_count == 0);
  CHECK(VS::Battle::Ai::actorAiStates[5] == peer);
  CHECK(VS::Battle::Ai::LiveFrameContext().actors[5] == peer);
  CHECK(peer->linked_actor == 16);
}

// Applies zeroed settings to Ashley's state with the given sub-type. Her
// state keeps the action metadata to her cost threshold; the following
// actors' states let the link resolution run.
VS::Battle::Ai::ActorAiState* ApplySubTypeSettings(CleanupFixture& fixture,
                                                   int sub_type) {
  void* saved_has_waypoints = VS::Battle::Ai::roomHasWaypoints;
  VS::Battle::Ai::roomHasWaypoints = nullptr;
  auto* state = fixture.AllocateState(0);
  fixture.AllocateState(1);
  fixture.AllocateState(2);
  fixture.actors[0].sub_type = static_cast<std::uint16_t>(sub_type);
  state->linked_actor = 7;
  VS::Battle::Ai::SettingsWord settings{};
  VS::Battle::Ai::ApplyLiveActorAiSettings(
      reinterpret_cast<VS::Battle::Ai::SettingsView*>(state), &settings);
  VS::Battle::Ai::roomHasWaypoints = saved_has_waypoints;
  return state;
}

// Frees the orbit and the three states ApplySubTypeSettings allocated.
void ReleaseSubTypeSettings(CleanupFixture& fixture,
                            VS::Battle::Ai::ActorAiState* state) {
  VS::Core::Memory::FreeLiveFromLowEnd(VS::Battle::Ai::arenaOrbit);
  VS::Battle::Ai::arenaOrbit = nullptr;
  VS::Battle::Ai::FreeLiveActorAiState(&fixture.actors[2]);
  VS::Battle::Ai::FreeLiveActorAiState(&fixture.actors[1]);
  VS::Core::Memory::FreeLiveFromLowEnd(state);
  VS::Battle::Ai::actorAiStates[0] = nullptr;
}
}  // namespace

TEST_CASE("Reaction requests draw before checking full-width route ownership",
          "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* state = fixture.AllocateState(1);
  const auto saved_frame = VS::Battle::Ai::LiveFrameContext();
  VS::Battle::Ai::LiveFrameContext().randomState = 40;
  VS::Battle::Ai::LiveFrameContext().lastBudgetTick = 1;
  state->native_pointer_18c =
      reinterpret_cast<void*>(std::uintptr_t{0x100000000});
  VS::Battle::Ai::SetLiveActorAiReactionWithDelay(
      reinterpret_cast<VS::Battle::Ai::PerceptionUpdateView*>(state), 1, 64);
  CHECK(VS::Battle::Ai::LiveFrameContext().randomState == 41);
  CHECK(state->decision_delay == 0);
  CHECK(fixture.actors[1].command_flags == 0xFF);
  state->native_pointer_18c = nullptr;
  VS::Battle::Ai::SetLiveActorAiReactionWithDelay(
      reinterpret_cast<VS::Battle::Ai::PerceptionUpdateView*>(state), 0x123,
      64);
  CHECK(state->decision_delay == 30);
  CHECK(fixture.actors[1].command_flags == 0x23);
  VS::Battle::Ai::LiveFrameContext() = saved_frame;
  VS::Battle::Ai::FreeLiveActorAiState(&fixture.actors[1]);
}

TEST_CASE("Defeat detaches every native peer reference before repeat cleanup",
          "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* first = fixture.AllocateState(1);
  auto* second = fixture.AllocateState(2);
  first->linked_actor = 5;
  second->linked_actor = 5;
  VS::Battle::Ai::ClearLiveActorAiLinksAndPlan(5);
  CHECK(first->linked_actor == 16);
  CHECK(second->linked_actor == 16);
  VS::Battle::Ai::ClearLiveActorAiLinksAndPlan(5);
  VS::Battle::Ai::FreeLiveActorAiState(&fixture.actors[1]);
  VS::Battle::Ai::FreeLiveActorAiState(&fixture.actors[2]);
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE("AI navigation override views retain a full native pointer",
          "[ai][cleanup][layout]") {
  REQUIRE(sizeof(decltype(VS::Battle::Ai::PerceptionUpdateView::opaque_18c)) ==
          sizeof(void*));
  CHECK(std::is_pointer_v<
        decltype(VS::Battle::Ai::PerceptionUpdateView::opaque_18c)>);
  CHECK(offsetof(VS::Battle::Ai::PerceptionUpdateView, opaque_18c) ==
        offsetof(VS::Battle::Ai::ActorAiState, native_pointer_18c));
}

TEST_CASE(
    "AI navigation override release frees the actual full-width heap block",
    "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* state = fixture.AllocateState(1);
  void* route = VS::Core::Memory::AllocateLiveFromHighEnd(512);
  REQUIRE(route != nullptr);
  state->native_pointer_18c = route;
  const auto allocated = VS::Core::Memory::GameHeap().FreeUnits();
  CHECK(reinterpret_cast<uintptr_t>(route) > 0xFFFFFFFFu);
  VS::Battle::Ai::FreeLiveActorNavigationOverride(2);
  CHECK(state->native_pointer_18c == nullptr);
  CHECK(VS::Core::Memory::GameHeap().FreeUnits() > allocated);
  VS::Battle::Ai::FreeLiveActorNavigationOverride(2);
  VS::Battle::Ai::FreeLiveActorAiState(&fixture.actors[1]);
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE(
    "AI reservation cleanup respects bounds and preserves other cell flags",
    "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* state = fixture.AllocateState(1);
  fixture.flags[7][4] = 0xFFFF;
  state->opaque_1a = 7;
  VS::Battle::Ai::ReleaseLiveActorAiReservations(state);
  CHECK(state->opaque_1a == 0);
  CHECK(fixture.flags[7][4] == 0xFF7F);
  VS::Battle::Ai::ForceReleaseLiveActorAiReservations(2);
  CHECK(state->opaque_1a == 1);
  fixture.room.max_tile_z = 5;
  fixture.flags[7][4] = 0xFFFF;
  VS::Battle::Ai::ReleaseLiveActorAiReservations(state);
  CHECK(fixture.flags[7][4] == 0xFFFF);
  VS::Battle::Ai::FreeLiveActorAiState(&fixture.actors[1]);
}

TEST_CASE(
    "AI teardown detaches peers and special actors before releasing state",
    "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* removed = fixture.AllocateState(1);
  auto* peer = fixture.AllocateState(2);
  peer->linked_actor = 2;
  fixture.room.special_actors[0] = fixture.room.special_actors[3] =
      &fixture.actors[1];
  fixture.room.special_actors[1] = &fixture.actors[2];
  REQUIRE(VS::Battle::Ai::TryEngageLiveActor(removed));
  VS::Battle::Ai::FreeLiveActorAiState(&fixture.actors[1]);
  CHECK(VS::Battle::Ai::actorAiStates[2] == nullptr);
  CHECK(VS::Battle::Ai::actorAiStates[5] == peer);
  CHECK(peer->linked_actor == 16);
  CHECK(VS::Battle::Ai::attributeActorMask == (1 << 5));
  CHECK(fixture.room.special_actors[0] == nullptr);
  CHECK(fixture.room.special_actors[3] == nullptr);
  CHECK(fixture.room.special_actors[1] == &fixture.actors[2]);
  VS::Battle::Ai::FreeLiveActorAiState(&fixture.actors[2]);
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE("AI teardown leaves no engaged-list or marker references",
          "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* state = fixture.AllocateState(1);
  REQUIRE(VS::Battle::Ai::TryEngageLiveActor(state));
  VS::Battle::actorMarkers[2] = 0x47;
  VS::Battle::Ai::FreeLiveActorAiState(&fixture.actors[1]);
  CHECK(fixture.room.engaged_count == 0);
  CHECK(fixture.room.engaged_head == nullptr);
  CHECK(fixture.room.engaged_tail == nullptr);
  CHECK(VS::Battle::actorMarkers[2] == 0);
  CHECK(fixture.actors[1].command_flags == 0xF8);
  VS::Battle::Ai::FreeLiveActorAiState(&fixture.actors[1]);
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE("AI teardown preserves the original Ashley state lifetime",
          "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* state = fixture.AllocateState(0);
  state->native_pointer_18c = VS::Core::Memory::AllocateLiveFromHighEnd(128);
  REQUIRE(state->native_pointer_18c != nullptr);
  VS::Battle::Ai::FreeLiveActorAiState(&fixture.actors[0]);
  CHECK(VS::Battle::Ai::actorAiStates[0] == state);
  CHECK(state->native_pointer_18c == nullptr);
  VS::Core::Memory::FreeLiveFromLowEnd(state);
  VS::Battle::Ai::actorAiStates[0] = nullptr;
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE("AI orbit construction clears reused storage and sets its cooldown",
          "[ai][orbit-state]") {
  const bool supplied = GENERATE(false, true);
  CleanupFixture fixture;
  VS::Battle::Ai::SettingsView input{};
  std::array<unsigned char, sizeof(input)> before{};
  std::memcpy(before.data(), &input, before.size());
  auto* dirty = VS::Core::Memory::AllocateLiveFromLowEnd(
      sizeof(VS::Battle::Ai::ArenaOrbitState));
  REQUIRE(dirty != nullptr);
  std::memset(dirty, 0xA5, sizeof(VS::Battle::Ai::ArenaOrbitState));
  VS::Core::Memory::FreeLiveFromLowEnd(dirty);
  VS::Battle::Ai::AllocateLiveOrbitingBossState(supplied ? &input : nullptr);
  REQUIRE(VS::Battle::Ai::arenaOrbit != nullptr);
  CHECK(static_cast<void*>(VS::Battle::Ai::arenaOrbit) == dirty);
  std::array<unsigned char, sizeof(VS::Battle::Ai::ArenaOrbitState)> actual{};
  std::memcpy(actual.data(), VS::Battle::Ai::arenaOrbit, actual.size());
  auto expected = decltype(actual){};
  expected[offsetof(VS::Battle::Ai::ArenaOrbitState, cooldown)] = 0x84;
  expected[offsetof(VS::Battle::Ai::ArenaOrbitState, cooldown) + 1] = 0x03;
  CHECK(actual == expected);
  std::array<unsigned char, sizeof(input)> after{};
  std::memcpy(after.data(), &input, after.size());
  CHECK(after == before);
  CHECK_FALSE(VS::Core::Memory::GameHeap().Empty());
  VS::Core::Memory::FreeLiveFromLowEnd(VS::Battle::Ai::arenaOrbit);
  VS::Battle::Ai::arenaOrbit = nullptr;
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE("AI settings allocate the orbit state for orbiting boss sub-types",
          "[ai][actor-settings][settings-live][orbit-state]") {
  const auto sub_type = GENERATE(0xAC, 0xAD);
  CleanupFixture fixture;
  auto* state = ApplySubTypeSettings(fixture, sub_type);
  const auto* view =
      reinterpret_cast<const VS::Battle::Ai::SettingsView*>(state);
  CHECK(view->decision_callback ==
        VS::Battle::Ai::UpdateLiveOrbitingBossBehavior);
  CHECK(view->settings_constant19 == 1);
  CHECK(state->linked_actor == 16);
  CHECK(state->action_cost_enabled == 1);
  REQUIRE(VS::Battle::Ai::arenaOrbit != nullptr);
  CHECK(VS::Battle::Ai::arenaOrbit->cooldown == 900);
  ReleaseSubTypeSettings(fixture, state);
  CHECK(VS::Battle::Ai::actorAiStates[2] == nullptr);
  CHECK(VS::Battle::Ai::actorAiStates[5] == nullptr);
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE("AI special orbit teardown releases reconstructed ownership once",
          "[ai][cleanup][orbit-state]") {
  const auto type = GENERATE(0xAC, 0xAD);
  CleanupFixture fixture;
  for (int cycle = 0; cycle < 2; ++cycle) {
    auto* state = fixture.AllocateState(1);
    fixture.actors[1].sub_type = static_cast<std::uint16_t>(type);
    VS::Battle::Ai::AllocateLiveOrbitingBossState(
        reinterpret_cast<VS::Battle::Ai::SettingsView*>(state));
    REQUIRE(VS::Battle::Ai::arenaOrbit != nullptr);
    VS::Battle::Ai::FreeLiveActorAiState(&fixture.actors[1]);
    CHECK(VS::Battle::Ai::arenaOrbit == nullptr);
    CHECK(VS::Core::Memory::GameHeap().Empty());
  }
}
namespace {
struct AppearanceFixture {
  decltype(std::to_array(VS::Actors::weaponModels)) saved_weapons =
      std::to_array(VS::Actors::weaponModels);
  int saved_speed = VS::State::tickSpeed;
  decltype(VS::Actors::actorRecords) saved_models = VS::Actors::actorRecords;
  decltype(VS::Field::gimmickInstances) saved_objects =
      VS::Field::gimmickInstances;
  RECT palette_region{768, 224, 256, 32};
  alignas(std::uint32_t) std::array<std::uint8_t, 16384> saved_cluts{};
  VS::Actors::ActorRecord actor{};

  AppearanceFixture() {
    StoreImage(&palette_region,
               reinterpret_cast<std::uint32_t*>(saved_cluts.data()));
    std::fill_n(VS::Actors::weaponModels, saved_weapons.size(), nullptr);
    std::fill_n(VS::Actors::actorRecords.begin(), saved_models.size(), nullptr);
    std::fill_n(VS::Field::gimmickInstances.begin(), saved_objects.size(),
                nullptr);
    VS::State::tickSpeed = 2;
    actor.instance.is_parsed = -1;
    actor.instance.base_color = {128, 128, 128, 0};
    actor.instance.action_animation_state = 4;
    actor.fade_red = 30;
  }

  ~AppearanceFixture() {
    std::copy(saved_weapons.begin(), saved_weapons.end(),
              VS::Actors::weaponModels);
    std::copy(saved_models.begin(), saved_models.end(),
              VS::Actors::actorRecords.begin());
    std::copy(saved_objects.begin(), saved_objects.end(),
              VS::Field::gimmickInstances.begin());
    LoadImage(&palette_region,
              reinterpret_cast<std::uint32_t*>(saved_cluts.data()));
    VS::State::tickSpeed = saved_speed;
  }
};

constexpr std::array<std::uint16_t, 16> kPulseColors = {
    0,      0x001F, 0x03E0, 0x7C00, 0x8000, 0xFFFF, 0x4210, 0x1234,
    0x2222, 0x3333, 0x4444, 0x5555, 0x6666, 0x7777, 0x1357, 0x2468,
};

std::uint16_t PulseColor(std::uint16_t color, int level) {
  const int red = color & 31;
  const int green = (color >> 5) & 31;
  const int blue = (color >> 10) & 31;
  const int r = std::clamp(red + (level == 16 ? 8 : -level / 2), 0, 31);
  const int g = std::min(31, green + (level == 16 ? 18 : level / 2));
  const int b = std::min(31, blue + (level == 16 ? 30 : level));
  return static_cast<std::uint16_t>((color & 0x8000) | r | (g << 5) |
                                    (b << 10));
}

template <std::size_t Count>
std::array<std::uint16_t, Count> PulseClut(int row, int offset) {
  alignas(std::uint32_t) std::array<std::uint16_t, Count> colors{};
  RECT area{static_cast<std::int16_t>(768 + offset),
            static_cast<std::int16_t>(224 + row),
            static_cast<std::int16_t>(Count), 1};
  StoreImage(&area, reinterpret_cast<std::uint32_t*>(colors.data()));
  return colors;
}

template <typename Record>
std::array<std::uint8_t, sizeof(Record)> ObjectBytes(const Record& record) {
  std::array<std::uint8_t, sizeof(Record)> bytes;
  std::memcpy(bytes.data(), &record, bytes.size());
  return bytes;
}

void CheckPulseCycle(VS::Field::GimmickInstance& object,
                     std::array<std::uint16_t, 256>& expected_row) {
  const int slot = object.slot;
  REQUIRE(VS::Actors::EnableLiveModelPalettePulse(slot, 0, 0) == 0);
  CHECK(object.palette_pulse.kind == 128);
  CHECK(object.palette_pulse.level == 0);
  CHECK(PulseClut<16>(31, slot * 16) == kPulseColors);
  VS::Actors::UpdateLiveGimmickPalettePulse(object);
  const auto offset = static_cast<std::size_t>(slot) * kPulseColors.size();
  for (std::size_t color = 0; color < kPulseColors.size(); ++color) {
    expected_row[offset + color] =
        kPulseColors[color] == 0 ? 0 : PulseColor(kPulseColors[color], 2);
  }
  CHECK(PulseClut<256>(31, 0) == expected_row);
  REQUIRE(VS::Actors::DisableLiveModelPalettePulse(slot, 0) == 0);
  CHECK(object.palette_pulse.kind == 0);
  CHECK(std::equal(kPulseColors.begin(), kPulseColors.end(),
                   object.base_palette.begin()));
}
}  // namespace

TEST_CASE("Actor disappearance changes the owning fade byte, not a legacy view",
          "[actors][appearance]") {
  AppearanceFixture fixture;
  std::array<std::uint8_t, sizeof(VS::Actors::ActorRecord)> expected;
  std::memcpy(expected.data(), &fixture.actor, expected.size());
  const auto color = offsetof(VS::Actors::ActorRecord, instance) +
                     offsetof(VS::Actors::ModelInstance, base_color);
  expected[color] = expected[color + 1] = expected[color + 2] = 124;
  expected[offsetof(VS::Actors::ActorRecord, fade_red)] = 32;
  VS::Actors::UpdateLiveActorAppearanceTransition(fixture.actor);
  std::array<std::uint8_t, sizeof(VS::Actors::ActorRecord)> actual;
  std::memcpy(actual.data(), &fixture.actor, actual.size());
  CHECK(fixture.actor.fade_red == 32);
  CHECK(actual == expected);
}

TEST_CASE("Actor appearance palettes use their owning selected rows",
          "[actors][appearance]") {
  const int state = GENERATE(2, 3, 6, 7, 9, 10, 12, 14);
  const bool full_palette = GENERATE(false, true);
  AppearanceFixture fixture;
  auto& actor = fixture.actor;
  actor.instance.action_animation_state = state;
  actor.texture_header_bit = full_palette ? 1 : 0;
  actor.fade_green = 10;
  std::fill_n(actor.base_palette, 160, 0x1234);
  std::fill_n(actor.base_palette + 160, 160, 0x7FFF);
  std::fill_n(actor.current_palette, 160, 0xA55A);
  std::array<std::uint16_t, 160> expected;
  expected.fill(0x7FFF);
  const int count = full_palette ? 160 : 16;
  const int skip = full_palette ? 1 : 0;
  const int mode = state == 2                 ? 0
                   : state == 3 || state == 7 ? 1
                   : state == 6               ? 3
                   : state == 9               ? 2
                   : state == 10              ? 4
                   : state == 12              ? 5
                                              : 6;
  const auto expected_colors = std::span<std::uint16_t>(expected).subspan(
      static_cast<std::size_t>(skip), static_cast<std::size_t>(count - skip));
  const auto reference = std::span<const std::uint16_t>(actor.base_palette)
                             .subspan(static_cast<std::size_t>(skip),
                                      static_cast<std::size_t>(count - skip));
  const bool changed = VS::Actors::ApplyPaletteTransition(
      expected_colors, reference,
      static_cast<VS::Actors::PaletteTransition>(mode), 1);
  if (state == 3 && !changed) {
    VS::Actors::ApplyPaletteTransition(
        expected_colors, reference, VS::Actors::PaletteTransition::kWhite, 1);
  }
  VS::Actors::UpdateLiveActorAppearanceTransition(actor);
  for (int index = 0; index < 160; ++index) {
    CHECK(actor.base_palette[index] == 0x1234);
    CHECK(actor.base_palette[index + 160] == expected[index]);
    CHECK(actor.current_palette[index] == 0xA55A);
  }
}

TEST_CASE("Gimmick palette fades stay on their own typed path",
          "[actors][appearance][gimmick]") {
  STATIC_REQUIRE(
      std::tuple_size_v<decltype(VS::Field::GimmickInstance::pulsed_palette)> ==
      16);
  STATIC_REQUIRE(sizeof(VS::Field::GimmickInstance::pulsed_palette) == 32);
  AppearanceFixture fixture;
  VS::Field::GimmickInstance object{};
  object.motion_state = GENERATE(2, 3);
  std::fill_n(object.base_palette.begin(), 16, 0x1234);
  std::fill_n(object.pulsed_palette.begin(), 16, 0x7FFF);
  std::array<std::uint16_t, 16> expected;
  expected.fill(0x7FFF);
  const int speed = object.motion_state == 2 ? 2 : 1;
  const int mode = object.motion_state == 2 ? 0 : 1;
  const bool changed = VS::Actors::ApplyPaletteTransition(
      expected, object.base_palette,
      static_cast<VS::Actors::PaletteTransition>(mode), speed);
  if (object.motion_state == 3 && !changed) {
    VS::Actors::ApplyPaletteTransition(expected, object.base_palette,
                                       VS::Actors::PaletteTransition::kWhite,
                                       speed);
  }
  VS::Actors::UpdateLiveGimmickAppearanceTransition(object);
  for (int index = 0; index < 16; ++index) {
    CHECK(object.pulsed_palette[index] == expected[index]);
    CHECK(object.base_palette[index] == 0x1234);
  }
}

TEST_CASE("Gimmick targeting pulses preserve native ownership and CLUT indices",
          "[actors][appearance][gimmick][palette-pulse]") {
  const int speed = GENERATE(2, 4);
  const int phase = GENERATE(0, 1, 2, 3);
  const int prefix = GENERATE(0, 0x65);
  AppearanceFixture fixture;
  VS::State::tickSpeed = speed;
  VS::Field::GimmickInstance object{};
  object.slot = 3;
  std::fill_n(object.reserved114.begin(), sizeof object.reserved114, prefix);
  std::copy(kPulseColors.begin(), kPulseColors.end(),
            object.base_palette.begin());
  std::fill_n(object.pulsed_palette.begin(), 16, 0x5A5A);
  const std::array levels{0, 14, 16, 2};
  const std::array slow{2, 16, 14, 0};
  const std::array fast{4, 14, 12, 2};
  const std::array directions{0, 2, 2, 0};
  object.palette_pulse.kind = 128;
  object.palette_pulse.level = static_cast<std::uint8_t>(levels[phase]);
  object.palette_pulse.flags = phase >= 2 ? 3 : 1;
  VS::Field::GimmickInstance expected;
  std::memcpy(&expected, &object, sizeof object);
  const int level = speed == 4 ? fast[phase] : slow[phase];
  expected.palette_pulse.level = static_cast<std::uint8_t>(level);
  expected.palette_pulse.flags =
      static_cast<std::uint8_t>(directions[phase] | 1);
  for (std::size_t i = 0; i < kPulseColors.size(); ++i) {
    if (kPulseColors[i] != 0) {
      expected.pulsed_palette[i] = PulseColor(kPulseColors[i], level);
    }
  }
  VS::Actors::UpdateLiveGimmickPalettePulse(object);
  CHECK(ObjectBytes(object) == ObjectBytes(expected));
  CHECK(std::equal(kPulseColors.begin(), kPulseColors.end(),
                   object.base_palette.begin()));
  const auto uploaded = PulseClut<16>(31, object.slot * 16);
  CHECK(std::equal(uploaded.begin(), uploaded.end(),
                   expected.pulsed_palette.begin()));
}

TEST_CASE(
    "Actor targeting pulses retain their original small and full palettes",
    "[actors][appearance][palette-pulse]") {
  const bool full = GENERATE(false, true);
  AppearanceFixture fixture;
  auto& actor = fixture.actor;
  actor.instance.slot = 2;
  actor.texture_header_bit = full ? 1 : 0;
  actor.instance.palette_pulse.kind = 128;
  actor.instance.palette_pulse.level = 0;
  actor.instance.palette_pulse.flags = 0;
  for (int i = 0; i < 160; ++i) {
    actor.base_palette[i] = kPulseColors[i % kPulseColors.size()];
    actor.current_palette[i] = 0x5A5A;
  }
  VS::Actors::ActorRecord expected;
  std::memcpy(&expected, &actor, sizeof actor);
  expected.instance.palette_pulse.level = 2;
  const int count = full ? 160 : 16;
  for (int i = 0; i < count; ++i) {
    if (actor.base_palette[i] != 0) {
      expected.current_palette[i] = PulseColor(actor.base_palette[i], 2);
    }
  }
  auto expected_row = PulseClut<256>(24, 0);
  std::copy_n(expected.current_palette, count, expected_row.begin());
  VS::Actors::UpdateLiveActorPalettePulse(actor);
  CHECK(ObjectBytes(actor) == ObjectBytes(expected));
  CHECK(PulseClut<256>(24, 0) == expected_row);
}

TEST_CASE(
    "Gimmick pulse cancellation and reentry retain source colors and bounds",
    "[actors][appearance][gimmick][palette-pulse]") {
  const int slot = GENERATE(0, 15);
  AppearanceFixture fixture;
  VS::Field::GimmickInstance object{};
  object.slot = static_cast<std::uint8_t>(slot);
  std::copy(kPulseColors.begin(), kPulseColors.end(),
            object.base_palette.begin());
  VS::Field::gimmickInstances[slot] = &object;
  auto expected_row = PulseClut<256>(31, 0);
  for (int entry = 0; entry < 3; ++entry) {
    CheckPulseCycle(object, expected_row);
  }
}

TEST_CASE("AI retirement follows allocation ownership after kind changes",
          "[ai][cleanup][ownership-mask]") {
  const auto kind = GENERATE(0, 1, 2, 4, 8, 0x10, 0x80, 0xFF);
  CleanupFixture fixture;
  auto* removed = fixture.AllocateState(1);
  auto* peer = fixture.AllocateState(2);
  peer->linked_actor = 2;
  removed->native_pointer_18c = VS::Core::Memory::AllocateLiveFromHighEnd(128);
  REQUIRE(removed->native_pointer_18c != nullptr);
  REQUIRE(VS::Battle::Ai::TryEngageLiveActor(removed));
  VS::Battle::Ai::LiveFrameContext().actors[2] = removed;
  VS::Battle::Ai::LiveFrameContext().actors[5] = peer;
  fixture.actors[1].kind = static_cast<std::uint16_t>(kind);
  VS::Battle::Ai::FreeLiveActorAiState(&fixture.actors[1]);
  CheckRetiredAiOwnership(fixture, peer);
  fixture.actors[1].kind = 4;
  VS::Battle::Ai::FreeLiveActorAiState(&fixture.actors[1]);
  VS::Battle::Ai::FreeLiveActorAiState(&fixture.actors[2]);
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE(
    "Ashley retains owned state while temporary routes retire after kind "
    "changes",
    "[ai][cleanup][ownership-mask]") {
  const auto kind = GENERATE(0, 1, 4, 0x10);
  CleanupFixture fixture;
  auto* state = fixture.AllocateState(0);
  state->native_pointer_18c = VS::Core::Memory::AllocateLiveFromHighEnd(128);
  REQUIRE(state->native_pointer_18c != nullptr);
  VS::Battle::Ai::LiveFrameContext().actors[0] = state;
  fixture.actors[0].kind = static_cast<std::uint16_t>(kind);
  VS::Battle::Ai::FreeLiveActorAiState(&fixture.actors[0]);
  CHECK(VS::Battle::Ai::actorAiStates[0] == state);
  CHECK(VS::Battle::Ai::LiveFrameContext().actors[0] == state);
  CHECK(state->native_pointer_18c == nullptr);
  VS::Core::Memory::FreeLiveFromLowEnd(state);
  VS::Battle::Ai::actorAiStates[0] = nullptr;
}

TEST_CASE("Non-AI slots outside the perception mask do not retire live peers",
          "[ai][cleanup][ownership-mask]") {
  CleanupFixture fixture;
  auto* peer = fixture.AllocateState(1);
  VS::Battle::Actor preview{};
  preview.id = 20;
  VS::Battle::actors[20] = &preview;
  VS::Battle::Ai::FreeLiveActorAiState(&preview);
  CHECK(VS::Battle::Ai::actorAiStates[2] == peer);
  CHECK(VS::Battle::Ai::attributeActorMask == (1 << 2));
  VS::Battle::Ai::FreeLiveActorAiState(&fixture.actors[1]);
}

TEST_CASE("Actors past the equipment table fade without equipment",
          "[actors][appearance][equipment]") {
  const int slot = GENERATE(9, 10, 16);
  AppearanceFixture fixture;
  auto weapons = std::make_unique<std::array<VS::Actors::WeaponModel, 2>>();
  for (auto& weapon : *weapons) {
    weapon.palette_color_count = 16;
    weapon.owner_motion_state = 1;
    std::fill_n(weapon.upload_palette, 96, 0x7FFF);
    std::fill_n(weapon.working_palette, 96, 0x1234);
  }
  VS::Actors::weaponModels[18] = &(*weapons)[0];
  VS::Actors::weaponModels[19] = &(*weapons)[1];
  auto& actor = fixture.actor;
  actor.instance.slot = static_cast<std::uint8_t>(slot);
  actor.instance.action_animation_state = 2;
  std::fill_n(actor.base_palette, 320, 0x1234);
  VS::Actors::UpdateLiveActorAppearanceTransition(actor);
  for (const auto& weapon : *weapons) {
    const bool owned = slot == 9;
    CHECK((weapon.upload_palette[0] != 0x7FFF) == owned);
    CHECK((weapon.owner_motion_state == 2) == owned);
  }
}

TEST_CASE("Equipment palette counts stop at their palette storage",
          "[actors][appearance][equipment]") {
  AppearanceFixture fixture;
  auto weapon = std::make_unique<VS::Actors::WeaponModel>();
  weapon->palette_color_count = 200;
  std::fill_n(weapon->upload_palette, 96, 0x7FFF);
  std::fill_n(weapon->working_palette, 96, 0x1234);
  weapon->brightness = 0xA5;
  weapon->reserved5c1 = 0x5A;
  weapon->reserved5c2 = 0xBEEF;
  VS::Actors::weaponModels[0] = weapon.get();
  auto& actor = fixture.actor;
  actor.instance.slot = 0;
  actor.instance.action_animation_state = 2;
  std::fill_n(actor.base_palette, 320, 0x1234);
  VS::Actors::UpdateLiveActorAppearanceTransition(actor);
  CHECK(weapon->upload_palette[95] != 0x7FFF);
  CHECK(weapon->brightness == 0xA5);
  CHECK(weapon->reserved5c1 == 0x5A);
  CHECK(weapon->reserved5c2 == 0xBEEF);
}
