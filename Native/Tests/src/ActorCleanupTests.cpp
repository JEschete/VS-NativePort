#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Appearance.h"
#include "Actors/AppearanceTransition.h"
#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/Engagement.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Core/Memory.h"
#include "Core/Memory/Heap.h"
#include "Field/Gimmicks.h"
#include "Render/Palette.h"
#include "State/Timers.h"
}

namespace {
struct CleanupFixture {
  Core_HeapBlockHeader saved_heap = Core_legacyHeapList;
  Core_HeapBlockHeader saved_secondary = Core_legacySecondaryHeapList;
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  decltype(std::to_array(Battle_Ai_actorAiStates)) saved_states =
      std::to_array(Battle_Ai_actorAiStates);
  decltype(std::to_array(Battle_actorMarkers)) saved_markers =
      std::to_array(Battle_actorMarkers);
  Battle_Ai_RoomAiState* saved_room = Battle_Ai_roomAiState;
  decltype(Battle_Ai_tileFlagMap) saved_flags = Battle_Ai_tileFlagMap;
  Battle_Ai_ArenaOrbitState* saved_orbit = Battle_Ai_arenaOrbit;
  Battle_Ai_FrameContext saved_frame = *Battle_Ai_FrameState();
  int saved_attribute = Battle_Ai_attributeActorMask;
  int saved_busy = Battle_Ai_candidateEngagedThisUpdate;
  std::array<Core_HeapBlockHeader, 2048> heap{};
  std::array<Battle_Actor, 3> actors{};
  Battle_Ai_RoomAiState room{};
  u_short flags[32][32]{};
  unsigned free_units = 0;

  CleanupFixture() {
    Core_Memory_InitializeHeap(heap.data(), sizeof(heap));
    free_units = Core_legacyHeapList.next->blockSize;
    Battle_Ai_roomAiState = &room;
    Battle_Ai_tileFlagMap = flags;
    Battle_Ai_arenaOrbit = nullptr;
    Battle_Ai_attributeActorMask = 0;
    Battle_Ai_candidateEngagedThisUpdate = 0;
    std::fill_n(Battle_actors, saved_actors.size(), nullptr);
    std::fill_n(Battle_Ai_actorAiStates, saved_states.size(), nullptr);
    const std::array<int, 3> ids{0, 2, 5};
    for (std::size_t index = 0; index < actors.size(); ++index) {
      actors[index].id = ids[index];
      actors[index].kind = 4;
      actors[index].commandFlags = 0xFF;
      actors[index].next =
          index + 1 < actors.size() ? &actors[index + 1] : nullptr;
      Battle_actors[ids[index]] = &actors[index];
    }
    room.maxTileX = room.minTileX = 31;
    room.maxTileZ = room.minTileZ = 0;
    room.engagementLimit = 2;
  }

  ~CleanupFixture() {
    Core_legacyHeapList = saved_heap;
    Core_legacySecondaryHeapList = saved_secondary;
    std::copy(saved_actors.begin(), saved_actors.end(), Battle_actors);
    std::copy(saved_states.begin(), saved_states.end(),
              Battle_Ai_actorAiStates);
    std::copy(saved_markers.begin(), saved_markers.end(), Battle_actorMarkers);
    Battle_Ai_roomAiState = saved_room;
    Battle_Ai_tileFlagMap = saved_flags;
    Battle_Ai_arenaOrbit = saved_orbit;
    *Battle_Ai_FrameState() = saved_frame;
    Battle_Ai_attributeActorMask = saved_attribute;
    Battle_Ai_candidateEngagedThisUpdate = saved_busy;
  }

  Battle_Ai_ActorAiState* AllocateState(std::size_t index) {
    auto* state = static_cast<Battle_Ai_ActorAiState*>(
        Core_Memory_AllocateFromLowEnd(sizeof(Battle_Ai_ActorAiState)));
    REQUIRE(state != nullptr);
    std::construct_at(state);
    state->actorId = static_cast<u_char>(actors[index].id);
    state->actor = reinterpret_cast<Battle_Ai_EngagementData*>(&actors[index]);
    state->nextTile.word = 0x00070004;
    Battle_Ai_actorAiStates[state->actorId] = state;
    Battle_Ai_attributeActorMask |= 1 << state->actorId;
    return state;
  }
};

void CheckRetiredAiOwnership(const CleanupFixture& fixture,
                             const Battle_Ai_ActorAiState* peer) {
  CHECK(Battle_Ai_actorAiStates[2] == nullptr);
  CHECK(Battle_Ai_FrameState()->actors[2] == nullptr);
  CHECK(Battle_Ai_attributeActorMask == (1 << 5));
  CHECK(fixture.room.engagedCount == 0);
  CHECK(Battle_Ai_actorAiStates[5] == peer);
  CHECK(Battle_Ai_FrameState()->actors[5] == peer);
  CHECK(peer->linkedActor == 16);
}
}  // namespace

TEST_CASE("Reaction requests draw before checking full-width route ownership",
          "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* state = fixture.AllocateState(1);
  const auto saved_frame = *Battle_Ai_FrameState();
  Battle_Ai_FrameState()->randomState = 40;
  Battle_Ai_FrameState()->lastBudgetTick = 1;
  state->nativePointer18C =
      reinterpret_cast<void*>(std::uintptr_t{0x100000000});
  Battle_Ai_SetActorAiReactionWithDelay(
      reinterpret_cast<Battle_Ai_PerceptionUpdateView*>(state), 1, 64);
  CHECK(Battle_Ai_FrameState()->randomState == 41);
  CHECK(state->decisionDelay == 0);
  CHECK(fixture.actors[1].commandFlags == 0xFF);
  state->nativePointer18C = nullptr;
  Battle_Ai_SetActorAiReactionWithDelay(
      reinterpret_cast<Battle_Ai_PerceptionUpdateView*>(state), 0x123, 64);
  CHECK(state->decisionDelay == 30);
  CHECK(fixture.actors[1].commandFlags == 0x23);
  *Battle_Ai_FrameState() = saved_frame;
  Battle_Ai_FreeActorAiState(&fixture.actors[1]);
}

TEST_CASE("Defeat detaches every native peer reference before repeat cleanup",
          "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* first = fixture.AllocateState(1);
  auto* second = fixture.AllocateState(2);
  first->linkedActor = 5;
  second->linkedActor = 5;
  Battle_Ai_ClearActorAiLinksAndPlan(5);
  CHECK(first->linkedActor == 16);
  CHECK(second->linkedActor == 16);
  Battle_Ai_ClearActorAiLinksAndPlan(5);
  Battle_Ai_FreeActorAiState(&fixture.actors[1]);
  Battle_Ai_FreeActorAiState(&fixture.actors[2]);
  CHECK(Core_legacyHeapList.next->blockSize == fixture.free_units);
}

TEST_CASE("AI navigation override views retain a full native pointer",
          "[ai][cleanup][layout]") {
  REQUIRE(sizeof(decltype(Battle_Ai_PerceptionUpdateView::opaque18C)) ==
          sizeof(void*));
  CHECK(std::is_pointer_v<decltype(Battle_Ai_PerceptionUpdateView::opaque18C)>);
  CHECK(offsetof(Battle_Ai_PerceptionUpdateView, opaque18C) ==
        offsetof(Battle_Ai_ActorAiState, nativePointer18C));
}

TEST_CASE(
    "AI navigation override release frees the actual full-width heap block",
    "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* state = fixture.AllocateState(1);
  void* route = Core_Memory_AllocateFromHighEnd(512);
  REQUIRE(route != nullptr);
  state->nativePointer18C = route;
  const auto allocated = Core_legacyHeapList.next->blockSize;
  CHECK(reinterpret_cast<uintptr_t>(route) > 0xFFFFFFFFu);
  Battle_Ai_FreeActorNavigationOverride(2);
  CHECK(state->nativePointer18C == nullptr);
  CHECK(Core_legacyHeapList.next->blockSize > allocated);
  Battle_Ai_FreeActorNavigationOverride(2);
  Battle_Ai_FreeActorAiState(&fixture.actors[1]);
  CHECK(Core_legacyHeapList.next->blockSize == fixture.free_units);
}

TEST_CASE(
    "AI reservation cleanup respects bounds and preserves other cell flags",
    "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* state = fixture.AllocateState(1);
  fixture.flags[7][4] = 0xFFFF;
  state->opaque1A = 7;
  Battle_Ai_ReleaseActorAiReservations(state);
  CHECK(state->opaque1A == 0);
  CHECK(fixture.flags[7][4] == 0xFF7F);
  Battle_Ai_ForceReleaseActorAiReservations(2);
  CHECK(state->opaque1A == 1);
  fixture.room.maxTileZ = 5;
  fixture.flags[7][4] = 0xFFFF;
  Battle_Ai_ReleaseActorAiReservations(state);
  CHECK(fixture.flags[7][4] == 0xFFFF);
  Battle_Ai_FreeActorAiState(&fixture.actors[1]);
}

TEST_CASE(
    "AI teardown detaches peers and special actors before releasing state",
    "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* removed = fixture.AllocateState(1);
  auto* peer = fixture.AllocateState(2);
  peer->linkedActor = 2;
  fixture.room.specialActors[0] = fixture.room.specialActors[3] =
      &fixture.actors[1];
  fixture.room.specialActors[1] = &fixture.actors[2];
  REQUIRE(Battle_Ai_TryEngageActor(removed) == 1);
  Battle_Ai_FreeActorAiState(&fixture.actors[1]);
  CHECK(Battle_Ai_actorAiStates[2] == nullptr);
  CHECK(Battle_Ai_actorAiStates[5] == peer);
  CHECK(peer->linkedActor == 16);
  CHECK(Battle_Ai_attributeActorMask == (1 << 5));
  CHECK(fixture.room.specialActors[0] == nullptr);
  CHECK(fixture.room.specialActors[3] == nullptr);
  CHECK(fixture.room.specialActors[1] == &fixture.actors[2]);
  Battle_Ai_FreeActorAiState(&fixture.actors[2]);
  CHECK(Core_legacyHeapList.next->blockSize == fixture.free_units);
}

TEST_CASE("AI teardown leaves no engaged-list or marker references",
          "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* state = fixture.AllocateState(1);
  REQUIRE(Battle_Ai_TryEngageActor(state) == 1);
  Battle_actorMarkers[2] = 0x47;
  Battle_Ai_FreeActorAiState(&fixture.actors[1]);
  CHECK(fixture.room.engagedCount == 0);
  CHECK(fixture.room.engagedHead == nullptr);
  CHECK(fixture.room.engagedTail == nullptr);
  CHECK(Battle_actorMarkers[2] == 0);
  CHECK(fixture.actors[1].commandFlags == 0xF8);
  Battle_Ai_FreeActorAiState(&fixture.actors[1]);
  CHECK(Core_legacyHeapList.next->blockSize == fixture.free_units);
}

TEST_CASE("AI teardown preserves the original Ashley state lifetime",
          "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* state = fixture.AllocateState(0);
  state->nativePointer18C = Core_Memory_AllocateFromHighEnd(128);
  REQUIRE(state->nativePointer18C != nullptr);
  Battle_Ai_FreeActorAiState(&fixture.actors[0]);
  CHECK(Battle_Ai_actorAiStates[0] == state);
  CHECK(state->nativePointer18C == nullptr);
  Core_Memory_FreeFromLowEnd(state);
  Battle_Ai_actorAiStates[0] = nullptr;
  CHECK(Core_legacyHeapList.next->blockSize == fixture.free_units);
}

TEST_CASE("AI special orbit teardown releases shared ownership once",
          "[ai][cleanup]") {
  const auto type = GENERATE(0xAC, 0xAD);
  CleanupFixture fixture;
  fixture.AllocateState(1);
  fixture.actors[1].subType = static_cast<u_short>(type);
  Battle_Ai_arenaOrbit = static_cast<Battle_Ai_ArenaOrbitState*>(
      Core_Memory_AllocateFromLowEnd(sizeof(Battle_Ai_ArenaOrbitState)));
  REQUIRE(Battle_Ai_arenaOrbit != nullptr);
  Battle_Ai_FreeActorAiState(&fixture.actors[1]);
  CHECK(Battle_Ai_arenaOrbit == nullptr);
  CHECK(Core_legacyHeapList.next->blockSize == fixture.free_units);
}
namespace {
struct AppearanceFixture {
  decltype(std::to_array(Actors_weaponModels)) saved_weapons =
      std::to_array(Actors_weaponModels);
  int saved_speed = State_tickSpeed;
  decltype(std::to_array(Actors_actorRecords)) saved_models =
      std::to_array(Actors_actorRecords);
  decltype(std::to_array(Field_gimmickInstances)) saved_objects =
      std::to_array(Field_gimmickInstances);
  RECT palette_region{768, 224, 256, 32};
  alignas(u_long) std::array<u_char, 16384> saved_cluts{};
  Actors_ActorRecord actor{};

  AppearanceFixture() {
    StoreImage(&palette_region, reinterpret_cast<u_long*>(saved_cluts.data()));
    std::fill_n(Actors_weaponModels, saved_weapons.size(), nullptr);
    std::fill_n(Actors_actorRecords, saved_models.size(), nullptr);
    std::fill_n(Field_gimmickInstances, saved_objects.size(), nullptr);
    State_tickSpeed = 2;
    actor.instance.isParsed = -1;
    actor.instance.baseColor = {128, 128, 128, 0};
    actor.instance.actionAnimationState = 4;
    actor.fadeRed = 30;
  }

  ~AppearanceFixture() {
    std::copy(saved_weapons.begin(), saved_weapons.end(), Actors_weaponModels);
    std::copy(saved_models.begin(), saved_models.end(), Actors_actorRecords);
    std::copy(saved_objects.begin(), saved_objects.end(),
              Field_gimmickInstances);
    LoadImage(&palette_region, reinterpret_cast<u_long*>(saved_cluts.data()));
    State_tickSpeed = saved_speed;
  }
};

constexpr std::array<u_short, 16> kPulseColors = {
    0,      0x001F, 0x03E0, 0x7C00, 0x8000, 0xFFFF, 0x4210, 0x1234,
    0x2222, 0x3333, 0x4444, 0x5555, 0x6666, 0x7777, 0x1357, 0x2468,
};

u_short PulseColor(u_short color, int level) {
  const int red = color & 31;
  const int green = (color >> 5) & 31;
  const int blue = (color >> 10) & 31;
  const int r = std::clamp(red + (level == 16 ? 8 : -level / 2), 0, 31);
  const int g = std::min(31, green + (level == 16 ? 18 : level / 2));
  const int b = std::min(31, blue + (level == 16 ? 30 : level));
  return static_cast<u_short>((color & 0x8000) | r | (g << 5) | (b << 10));
}

template <std::size_t Count>
std::array<u_short, Count> PulseClut(int row, int offset) {
  alignas(u_long) std::array<u_short, Count> colors{};
  RECT area{static_cast<std::int16_t>(768 + offset),
            static_cast<std::int16_t>(224 + row),
            static_cast<std::int16_t>(Count), 1};
  StoreImage(&area, reinterpret_cast<u_long*>(colors.data()));
  return colors;
}

template <typename Record>
std::array<u_char, sizeof(Record)> ObjectBytes(const Record& record) {
  std::array<u_char, sizeof(Record)> bytes;
  std::memcpy(bytes.data(), &record, bytes.size());
  return bytes;
}

void CheckPulseCycle(Field_GimmickInstance& object,
                     std::array<u_short, 256>& expected_row) {
  const int slot = object.slot;
  REQUIRE(Actors_EnableModelPalettePulse(slot, 0, 0) == 0);
  CHECK(object.palettePulse.kind == 128);
  CHECK(object.palettePulse.level == 0);
  CHECK(PulseClut<16>(31, slot * 16) == kPulseColors);
  Actors_UpdateActorPalettePulse(nullptr, &object, 0);
  const auto offset = static_cast<std::size_t>(slot) * kPulseColors.size();
  for (std::size_t color = 0; color < kPulseColors.size(); ++color) {
    expected_row[offset + color] =
        kPulseColors[color] == 0 ? 0 : PulseColor(kPulseColors[color], 2);
  }
  CHECK(PulseClut<256>(31, 0) == expected_row);
  REQUIRE(Actors_DisableModelPalettePulse(slot, 0) == 0);
  CHECK(object.palettePulse.kind == 0);
  CHECK(
      std::equal(kPulseColors.begin(), kPulseColors.end(), object.basePalette));
}
}  // namespace

TEST_CASE("Actor disappearance changes the owning fade byte, not a legacy view",
          "[actors][appearance]") {
  AppearanceFixture fixture;
  std::array<u_char, sizeof(Actors_ActorRecord)> expected;
  std::memcpy(expected.data(), &fixture.actor, expected.size());
  const auto color = offsetof(Actors_ActorRecord, instance) +
                     offsetof(Actors_ModelInstance, baseColor);
  expected[color] = expected[color + 1] = expected[color + 2] = 124;
  expected[offsetof(Actors_ActorRecord, fadeRed)] = 32;
  Actors_UpdateActorAppearanceTransition(&fixture.actor);
  std::array<u_char, sizeof(Actors_ActorRecord)> actual;
  std::memcpy(actual.data(), &fixture.actor, actual.size());
  CHECK(fixture.actor.fadeRed == 32);
  CHECK(actual == expected);
}

TEST_CASE("Actor appearance palettes use their owning selected rows",
          "[actors][appearance]") {
  const int state = GENERATE(2, 3, 6, 7, 9, 10, 12, 14);
  const bool full_palette = GENERATE(false, true);
  AppearanceFixture fixture;
  auto& actor = fixture.actor;
  actor.instance.actionAnimationState = state;
  actor.textureHeaderBit = full_palette ? 1 : 0;
  actor.fadeGreen = 10;
  std::fill_n(actor.basePalette, 160, 0x1234);
  std::fill_n(actor.basePalette + 160, 160, 0x7FFF);
  std::fill_n(actor.currentPalette, 160, 0xA55A);
  std::array<u_short, 160> expected;
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
  const int changed = Render_ApplyPaletteTransition(
      expected.data() + skip, count - skip, 1, mode, actor.basePalette + skip);
  if (state == 3 && changed == 0) {
    Render_ApplyPaletteTransition(expected.data() + skip, count - skip, 1, 2,
                                  actor.basePalette + skip);
  }
  Actors_UpdateActorAppearanceTransition(&actor);
  for (int index = 0; index < 160; ++index) {
    CHECK(actor.basePalette[index] == 0x1234);
    CHECK(actor.basePalette[index + 160] == expected[index]);
    CHECK(actor.currentPalette[index] == 0xA55A);
  }
}

TEST_CASE("Gimmick palette fades stay on their own typed path",
          "[actors][appearance][gimmick]") {
  STATIC_REQUIRE(
      std::extent_v<decltype(Field_GimmickInstance::pulsedPalette)> == 16);
  STATIC_REQUIRE(sizeof(Field_GimmickInstance::pulsedPalette) == 32);
  AppearanceFixture fixture;
  Field_GimmickInstance object{};
  object.motionState = GENERATE(2, 3);
  std::fill_n(object.basePalette, 16, 0x1234);
  std::fill_n(object.pulsedPalette, 16, 0x7FFF);
  std::array<u_short, 16> expected;
  expected.fill(0x7FFF);
  const int speed = object.motionState == 2 ? 2 : 1;
  const int mode = object.motionState == 2 ? 0 : 1;
  const int changed = Render_ApplyPaletteTransition(expected.data(), 16, speed,
                                                    mode, object.basePalette);
  if (object.motionState == 3 && changed == 0) {
    Render_ApplyPaletteTransition(expected.data(), 16, speed, 2,
                                  object.basePalette);
  }
  Field_UpdateGimmickAppearanceTransition(&object);
  for (int index = 0; index < 16; ++index) {
    CHECK(object.pulsedPalette[index] == expected[index]);
    CHECK(object.basePalette[index] == 0x1234);
  }
}

TEST_CASE("Gimmick targeting pulses preserve native ownership and CLUT indices",
          "[actors][appearance][gimmick][palette-pulse]") {
  const int speed = GENERATE(2, 4);
  const int phase = GENERATE(0, 1, 2, 3);
  const int prefix = GENERATE(0, 0x65);
  AppearanceFixture fixture;
  State_tickSpeed = speed;
  Field_GimmickInstance object{};
  object.slot = 3;
  std::fill_n(object.reserved114, sizeof object.reserved114, prefix);
  std::copy(kPulseColors.begin(), kPulseColors.end(), object.basePalette);
  std::fill_n(object.pulsedPalette, 16, 0x5A5A);
  const std::array levels{0, 14, 16, 2};
  const std::array slow{2, 16, 14, 0};
  const std::array fast{4, 14, 12, 2};
  const std::array directions{0, 2, 2, 0};
  object.palettePulse.kind = 128;
  object.palettePulse.level = static_cast<u_char>(levels[phase]);
  object.palettePulse.flags = phase >= 2 ? 3 : 1;
  Field_GimmickInstance expected;
  std::memcpy(&expected, &object, sizeof object);
  const int level = speed == 4 ? fast[phase] : slow[phase];
  expected.palettePulse.level = static_cast<u_char>(level);
  expected.palettePulse.flags = static_cast<u_char>(directions[phase] | 1);
  for (std::size_t i = 0; i < kPulseColors.size(); ++i) {
    if (kPulseColors[i] != 0) {
      expected.pulsedPalette[i] = PulseColor(kPulseColors[i], level);
    }
  }
  Actors_UpdateActorPalettePulse(nullptr, &object, 0);
  CHECK(ObjectBytes(object) == ObjectBytes(expected));
  CHECK(
      std::equal(kPulseColors.begin(), kPulseColors.end(), object.basePalette));
  const auto uploaded = PulseClut<16>(31, object.slot * 16);
  CHECK(std::equal(uploaded.begin(), uploaded.end(), expected.pulsedPalette));
}

TEST_CASE(
    "Actor targeting pulses retain their original small and full palettes",
    "[actors][appearance][palette-pulse]") {
  const bool full = GENERATE(false, true);
  AppearanceFixture fixture;
  auto& actor = fixture.actor;
  actor.instance.slot = 2;
  actor.textureHeaderBit = full ? 1 : 0;
  actor.instance.palettePulse.kind = 128;
  actor.instance.palettePulse.level = 0;
  actor.instance.palettePulse.flags = 0;
  for (int i = 0; i < 160; ++i) {
    actor.basePalette[i] = kPulseColors[i % kPulseColors.size()];
    actor.currentPalette[i] = 0x5A5A;
  }
  Actors_ActorRecord expected;
  std::memcpy(&expected, &actor, sizeof actor);
  expected.instance.palettePulse.level = 2;
  const int count = full ? 160 : 16;
  for (int i = 0; i < count; ++i) {
    if (actor.basePalette[i] != 0) {
      expected.currentPalette[i] = PulseColor(actor.basePalette[i], 2);
    }
  }
  auto expected_row = PulseClut<256>(24, 0);
  std::copy_n(expected.currentPalette, count, expected_row.begin());
  Actors_UpdateActorPalettePulse(&actor, nullptr, 0);
  CHECK(ObjectBytes(actor) == ObjectBytes(expected));
  CHECK(PulseClut<256>(24, 0) == expected_row);
}

TEST_CASE(
    "Gimmick pulse cancellation and reentry retain source colors and bounds",
    "[actors][appearance][gimmick][palette-pulse]") {
  const int slot = GENERATE(0, 15);
  AppearanceFixture fixture;
  Field_GimmickInstance object{};
  object.slot = static_cast<u_char>(slot);
  std::copy(kPulseColors.begin(), kPulseColors.end(), object.basePalette);
  Field_gimmickInstances[slot] = &object;
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
  peer->linkedActor = 2;
  removed->nativePointer18C = Core_Memory_AllocateFromHighEnd(128);
  REQUIRE(removed->nativePointer18C != nullptr);
  REQUIRE(Battle_Ai_TryEngageActor(removed) == 1);
  Battle_Ai_FrameState()->actors[2] = removed;
  Battle_Ai_FrameState()->actors[5] = peer;
  fixture.actors[1].kind = static_cast<u_short>(kind);
  Battle_Ai_FreeActorAiState(&fixture.actors[1]);
  CheckRetiredAiOwnership(fixture, peer);
  fixture.actors[1].kind = 4;
  Battle_Ai_FreeActorAiState(&fixture.actors[1]);
  Battle_Ai_FreeActorAiState(&fixture.actors[2]);
  CHECK(Core_legacyHeapList.next->blockSize == fixture.free_units);
}

TEST_CASE(
    "Ashley retains owned state while temporary routes retire after kind "
    "changes",
    "[ai][cleanup][ownership-mask]") {
  const auto kind = GENERATE(0, 1, 4, 0x10);
  CleanupFixture fixture;
  auto* state = fixture.AllocateState(0);
  state->nativePointer18C = Core_Memory_AllocateFromHighEnd(128);
  REQUIRE(state->nativePointer18C != nullptr);
  Battle_Ai_FrameState()->actors[0] = state;
  fixture.actors[0].kind = static_cast<u_short>(kind);
  Battle_Ai_FreeActorAiState(&fixture.actors[0]);
  CHECK(Battle_Ai_actorAiStates[0] == state);
  CHECK(Battle_Ai_FrameState()->actors[0] == state);
  CHECK(state->nativePointer18C == nullptr);
  Core_Memory_FreeFromLowEnd(state);
  Battle_Ai_actorAiStates[0] = nullptr;
}

TEST_CASE("Non-AI slots outside the perception mask do not retire live peers",
          "[ai][cleanup][ownership-mask]") {
  CleanupFixture fixture;
  auto* peer = fixture.AllocateState(1);
  Battle_Actor preview{};
  preview.id = 20;
  Battle_actors[20] = &preview;
  Battle_Ai_FreeActorAiState(&preview);
  CHECK(Battle_Ai_actorAiStates[2] == peer);
  CHECK(Battle_Ai_attributeActorMask == (1 << 2));
  Battle_Ai_FreeActorAiState(&fixture.actors[1]);
}
