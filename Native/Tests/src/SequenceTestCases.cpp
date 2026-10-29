#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Actors/SequencePlayback.h"
#include "Content/ModuleData.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Render/ModelGeometry.h"
}

extern "C" int Tests_SharedSequenceOffsets(void);
extern "C" int Tests_AdvanceSequenceWithoutScratch(void);
extern "C" int Tests_BlendSequencePose(int mode);
extern "C" int Tests_ModelWorkspaceIsolation(void);
extern "C" int Tests_ModelProjectionGroups(void);
extern "C" int Tests_ModelPolygonPackets(int colored, int split);
extern "C" int TestsModelGimmickOverridePackets(unsigned state,
                                                unsigned pulse_mask, int limb);
extern "C" int TestsModelWeaponOverridePackets(unsigned state,
                                               unsigned pulse_mask, int limb);
extern "C" int TestsModelLimbPalettePackets(int colored, int split, int limb,
                                            unsigned pulse_mask);
extern "C" int TestsModelAppearancePackets(int colored, int split,
                                           unsigned state, unsigned pulse_mask);
extern "C" int Tests_ActorPoseHierarchy(void);
extern "C" int Tests_ProjectedBoneEndpoints(void);
extern "C" int TestBoneEndpointOwner(int owner, int endpoint);
extern "C" int TestScriptedEffectTargetBinding(int scenario);
extern "C" int TestSharedSequenceRetirement(void);
extern "C" int TestSharedSequenceRetirementControl(int scenario);
extern "C" int TestAnimationEventUnwind(int scenario);
extern "C" int TestOptionalActorScriptSlide(int present, int rate, int skip);
extern "C" int TestOptionalMovementWrapper(int operation, int rate);
extern "C" int TestGimmickStackTop(int scenario);
extern "C" int Tests_RoomSpawnCountGuard(void);
extern "C" int Tests_ActorTextureFrameRectangles(int wide, int fallback,
                                                 unsigned* command);
extern "C" int Tests_FacialTextureBankHandoff(void);
extern "C" int Tests_GimmickRoomScale(void);
extern "C" int Tests_DirectMovementCommand(void);
extern "C" int Tests_TargetSphereWireframe(void);
extern "C" int Tests_TargetLabelStride(void);
extern "C" int Tests_SparseIdlePoseReset(void);
extern "C" int Tests_ActionCameraRecovery(void);
extern "C" int TestBattleAnimationHeaderOutputs(int consumer, int mode,
                                                int available);

TEST_CASE(
    "Action recovery reads the owning camera state rather than detached "
    "aliases",
    "[targeting][recovery]") {
  REQUIRE(Tests_ActionCameraRecovery() == 1);
}

TEST_CASE(
    "Idle pose reset accepts empty actor slots and preserves protected "
    "transitions",
    "[actors][movement]") {
  REQUIRE(Tests_SparseIdlePoseReset() == 1);
}

TEST_CASE(
    "Target-label animation follows native label stride without damaging text "
    "pointers",
    "[targeting][labels]") {
  REQUIRE(Tests_TargetLabelStride() == 1);
}

TEST_CASE("Targeting wireframes project and link shaded segments natively",
          "[targeting][render]") {
  REQUIRE(Tests_TargetSphereWireframe() == 1);
}

TEST_CASE(
    "Direct movement writes direction and speed without changing unrelated "
    "actor state",
    "[actors][movement]") {
  REQUIRE(Tests_DirectMovementCommand() == 1);
}

TEST_CASE(
    "Gimmicks use the canonical room scale rather than detached scratch "
    "storage",
    "[gimmick][model]") {
  REQUIRE(Tests_GimmickRoomScale() == 1);
}

TEST_CASE("ETM load, bank switching, and release preserve facial copies",
          "[actors][texture]") {
  REQUIRE(Tests_FacialTextureBankHandoff() == 1);
}

TEST_CASE("Actor facial texture frames retain 4-bit and 8-bit copy geometry",
          "[actors][texture]") {
  for (int wide : {0, 1}) {
    for (int fallback : {0, 1}) {
      CAPTURE(wide, fallback);
      CHECK(Tests_ActorTextureFrameRectangles(wide, fallback, nullptr) == 1);
    }
  }
}

TEST_CASE(
    "Room actor spawning uses the owning MPD count rather than a detached "
    "alias",
    "[actors][loading]") {
  REQUIRE(Tests_RoomSpawnCountGuard() == 1);
}

TEST_CASE(
    "Projected bone endpoints retain packed screen coordinates and full depth",
    "[actors][model]") {
  REQUIRE(Tests_ProjectedBoneEndpoints() == 1);
}

TEST_CASE("Live actor pose retains hierarchy and independent accumulated scale",
          "[actors][model]") {
  REQUIRE(Tests_ActorPoseHierarchy() == 1);
}

TEST_CASE("Flat and colored model polygons preserve UVs and native ordering",
          "[actors][model]") {
  CHECK(Tests_ModelPolygonPackets(0, 0) == 1);
  CHECK(Tests_ModelPolygonPackets(1, 0) == 1);
  CHECK(Tests_ModelPolygonPackets(0, 1) == 1);
  CHECK(Tests_ModelPolygonPackets(0, -1) == 1);
}

TEST_CASE(
    "Model projection applies each vertex group's bone and rejects invalid "
    "bounds",
    "[actors][model]") {
  REQUIRE(Tests_ModelProjectionGroups() == 1);
}

TEST_CASE(
    "Actor model workspace is independent of frame scratch and copies the "
    "camera pointee",
    "[actors][model]") {
  REQUIRE(Tests_ModelWorkspaceIsolation() == 1);
}

TEST_CASE(
    "Shared SEQ tables decode adjacent 32-bit offsets into native animation "
    "banks",
    "[content][actors][sequence]") {
  REQUIRE(Tests_SharedSequenceOffsets() == 1);
}

TEST_CASE("Animation tracks retain delta cursors across frame ranges",
          "[actors][sequence]") {
  const unsigned char data[] = {0xE2, 1, 254, 3, 0xE1, 255, 2, 253, 0};
  Actors_AnimationTrackContext context{data, 1, 3};
  SVECTOR value{}, delta{};
  Actors_DecodeAnimationTrack(&context, 0, &value, &delta, 0);
  CHECK(value.vx == 3);
  CHECK(value.vy == -6);
  CHECK(value.vz == 9);
  CHECK(value.pad == 4);
  CHECK(delta.pad == 3);
  context.firstFrame = 4;
  context.lastFrame = 5;
  Actors_DecodeAnimationTrack(&context, 0, &value, &delta, 0);
  CHECK(value.vx == 1);
  CHECK(value.vy == -2);
  CHECK(value.vz == 3);
  CHECK(value.pad == 8);
  CHECK(delta.pad == 5);
}

TEST_CASE("Animation track rotation wrapping and packed deltas match SEQ rules",
          "[actors][sequence]") {
  SECTION("rotations wrap to thirteen bits") {
    const unsigned char data[] = {0xE0, 1, 255, 1, 0};
    Actors_AnimationTrackContext context{data, 1, 1};
    SVECTOR value{8191, 0, 8191, 0}, delta{};
    Actors_DecodeAnimationTrack(&context, 0, &value, &delta, 1);
    CHECK(value.vx == 0);
    CHECK(value.vy == 8191);
    CHECK(value.vz == 0);
  }
  SECTION("signed packed delta") {
    const unsigned char data[] = {0x10, 0xFF, 0xF4, 0};
    Actors_AnimationTrackContext context{data, 1, 1};
    SVECTOR value{}, delta{};
    Actors_DecodeAnimationTrack(&context, 0, &value, &delta, 0);
    CHECK(value.vx == -2);
    CHECK(value.vy == 0);
    CHECK(value.vz == 0);
    CHECK(value.pad == 3);
  }
  SECTION("extended frame count") {
    const unsigned char data[] = {0xFF, 2, 2, 255, 0, 0};
    Actors_AnimationTrackContext context{data, 1, 34};
    SVECTOR value{}, delta{};
    Actors_DecodeAnimationTrack(&context, 0, &value, &delta, 0);
    CHECK(value.vx == 68);
    CHECK(value.vy == -34);
    CHECK(delta.pad == 34);
  }
  SECTION("halfword-aligned vector is sufficient") {
    alignas(std::int32_t)
        std::array<std::byte, sizeof(SVECTOR) + sizeof(std::int32_t)>
            storage{};
    auto* value = std::construct_at(
        reinterpret_cast<SVECTOR*>(storage.data() + sizeof(std::int16_t)),
        SVECTOR{8191, 0, 8191, 0});
    REQUIRE(reinterpret_cast<std::uintptr_t>(value) % alignof(std::int32_t) !=
            0);
    const unsigned char data[] = {0xE0, 1, 255, 1, 0};
    Actors_AnimationTrackContext context{data, 1, 1};
    SVECTOR delta{};
    Actors_DecodeAnimationTrack(&context, 0, value, &delta, 1);
    CHECK(value->vx == 0);
    CHECK(value->vy == 8191);
    CHECK(value->vz == 0);
    CHECK(value->pad == 4);
    CHECK(delta.pad == 1);
    std::destroy_at(value);
  }
}

TEST_CASE(
    "Skeletal playback resolves file offsets without using shared scratch",
    "[actors][sequence]") {
  REQUIRE(Tests_AdvanceSequenceWithoutScratch() == 1);
}

TEST_CASE(
    "Pose blending preserves fixed-point branches and native event pointers",
    "[actors][sequence]") {
  CHECK(Tests_BlendSequencePose(0) == 1);
  CHECK(Tests_BlendSequencePose(1) == 1);
  CHECK(Tests_BlendSequencePose(2) == 1);
}

namespace {
void CheckAnimationHeaderOutput(int consumer, int mode, int available) {
  REQUIRE(TestBattleAnimationHeaderOutputs(consumer, mode, available) == 1);
}
}  // namespace

TEST_CASE("Battle animation queries retain native header outputs",
          "[actors][sequence][animation-headers]") {
  for (int consumer : {0, 1, 2, 3}) {
    for (int mode : {0, 1}) {
      for (int available : {0, 1}) {
        CheckAnimationHeaderOutput(consumer, mode, available);
      }
    }
  }
}

TEST_CASE("Selected limb polygons use only their own pulsed palette",
          "[actors][model][targeting][palette]") {
  for (int limb = 0; limb < 8; ++limb) {
    const unsigned selected = 0x80u >> limb;
    const unsigned other = 0x80u >> ((limb + 1) % 8);
    CHECK(TestsModelLimbPalettePackets(0, 0, limb, selected) == 1);
    CHECK(TestsModelLimbPalettePackets(1, 0, limb, selected) == 1);
    CHECK(TestsModelLimbPalettePackets(0, 1, limb, selected) == 1);
    CHECK(TestsModelLimbPalettePackets(0, -1, limb, selected) == 1);
    CHECK(TestsModelLimbPalettePackets(0, 0, limb, other) == 1);
    CHECK(TestsModelLimbPalettePackets(1, 0, limb, other) == 1);
  }
  CHECK(TestsModelLimbPalettePackets(0, 0, 8, 0xFF) == 1);
  CHECK(TestsModelLimbPalettePackets(1, 0, 8, 0xFF) == 1);
}
TEST_CASE("Actor appearance materials preserve original blend dispatch",
          "[actors][appearance][render]") {
  for (unsigned state = 0; state < 16; ++state) {
    for (unsigned pulse : {0U, 0xFFU}) {
      CAPTURE(state, pulse);
      CHECK(TestsModelAppearancePackets(0, 0, state, pulse) == 1);
      CHECK(TestsModelAppearancePackets(1, 0, state, pulse) == 1);
      CHECK(TestsModelAppearancePackets(0, 1, state, pulse) == 1);
      CHECK(TestsModelAppearancePackets(0, -1, state, pulse) == 1);
    }
  }
}

namespace {
void CheckGimmickOverride(unsigned state, unsigned pulse) {
  for (int limb = 0; limb < 9; ++limb) {
    CAPTURE(state, pulse, limb);
    CHECK(TestsModelGimmickOverridePackets(state, pulse, limb) == 1);
  }
}

void CheckWeaponOverride(unsigned state, unsigned pulse) {
  for (int limb = 0; limb < 9; ++limb) {
    CAPTURE(state, pulse, limb);
    CHECK(TestsModelWeaponOverridePackets(state, pulse, limb) == 1);
  }
}
}  // namespace

TEST_CASE("Gimmick overrides preserve every material state and limb palette",
          "[actors][appearance][render][gimmick-override]") {
  for (unsigned state = 0; state < 16; ++state) {
    for (unsigned pulse : {0U, 0x80U, 0xFFU}) {
      CheckGimmickOverride(state, pulse);
    }
  }
}

TEST_CASE(
    "Original gimmick override address resolves to the typed native entry",
    "[actors][appearance][render][gimmick-override]") {
  using Renderer =
      void (*)(Actors_ModelRenderContext*, Field_GimmickInstance*, int*, void*);
  static_assert(
      std::is_same_v<decltype(&Render_DrawModelGouraudPolygonsWithOverrides),
                     Renderer>);
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  auto* resolved = VS::Content::ResolveModuleAddress(
      symbols, "BATTLE/BATTLE.PRG", 0x800B5C30u);
  REQUIRE(resolved != nullptr);
  CHECK(reinterpret_cast<Renderer>(resolved) ==
        Render_DrawModelGouraudPolygonsWithOverrides);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800B5C31u) == nullptr);
}

TEST_CASE("Weapon overrides preserve the source matrix-byte palette mask",
          "[actors][appearance][render][weapon-override]") {
  for (unsigned state = 0; state < 16; ++state) {
    for (unsigned pulse : {0U, 0x80U, 0xFFU}) {
      CheckWeaponOverride(state, pulse);
    }
  }
}

TEST_CASE("Original weapon override address resolves to the typed native entry",
          "[actors][appearance][render][weapon-override]") {
  using Renderer = void (*)(Actors_ModelRenderContext*, Actors_WeaponModel*,
                            CVECTOR*, void*);
  static_assert(
      std::is_same_v<decltype(&Render_DrawModelFlatPolygonsWithOverrides),
                     Renderer>);
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  auto* resolved = VS::Content::ResolveModuleAddress(
      symbols, "BATTLE/BATTLE.PRG", 0x800B516Cu);
  REQUIRE(resolved != nullptr);
  CHECK(reinterpret_cast<Renderer>(resolved) ==
        Render_DrawModelFlatPolygonsWithOverrides);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800B516Du) == nullptr);
}

TEST_CASE("Shared sequence retirement unwinds vertex edits before detaching",
          "[actors][animation][sequence-retirement]") {
  CHECK(TestSharedSequenceRetirement() == 1);
}

TEST_CASE("Animation event unwind preserves unavailable pose obligations",
          "[actors][animation][sequence-retirement]") {
  for (int scenario = 0; scenario < 8; ++scenario) {
    CAPTURE(scenario);
    CHECK(TestAnimationEventUnwind(scenario) == 1);
  }
}

TEST_CASE(
    "Pose unwind retains its bound shared clip after selecting another bank",
    "[actors][animation][sequence-retirement]") {
  CHECK(TestAnimationEventUnwind(8) == 1);
  CHECK(TestAnimationEventUnwind(9) == 1);
}

TEST_CASE("Shared-bank retirement retains borrowers across selector changes",
          "[actors][animation][sequence-retirement]") {
  CHECK(TestSharedSequenceRetirementControl(6) == 1);
}

TEST_CASE("Bound shared clips preserve the full native physical-index byte",
          "[actors][animation][sequence-retirement]") {
  CHECK(TestAnimationEventUnwind(10) == 1);
}

TEST_CASE("Shared bank retirement preserves unrelated and failed borrowers",
          "[actors][animation][sequence-retirement]") {
  for (int scenario = 1; scenario < 6; ++scenario) {
    CAPTURE(scenario);
    CHECK(TestSharedSequenceRetirementControl(scenario) == 1);
  }
}

TEST_CASE(
    "Scripted actor effects wait for their required binding before playback",
    "[event][effects][effect-binding]") {
  for (int scenario = 0; scenario < 10; ++scenario) {
    CAPTURE(scenario);
    CHECK(TestScriptedEffectTargetBinding(scenario) == 1);
  }
}

TEST_CASE(
    "Bone endpoints resolve actor and gimmick owners before reading models",
    "[actors][gimmick][model][bone-owner]") {
  for (int index = 0; index < 24; ++index) {
    const int owner = (index / 3) % 4;
    const int endpoint = index % 3;
    CAPTURE(index, owner, endpoint);
    CHECK(TestBoneEndpointOwner(owner, endpoint) == 1);
  }
}

TEST_CASE("Scripted world slides preserve optional actor bindings and rates",
          "[event][actors][optional-actor]") {
  constexpr int kRates[] = {-1, 0, 3};
  for (int index = 0; index < 12; ++index) {
    const int present = index < 6;
    const int rate = kRates[(index / 2) % 3];
    const int skip = index % 2;
    CAPTURE(present, rate, skip);
    CHECK(TestOptionalActorScriptSlide(present, rate, skip) == 1);
  }
}

TEST_CASE("Optional actor movement wrappers preserve absent binding no-ops",
          "[event][actors][optional-movement]") {
  constexpr int kRates[] = {-1, 0, 3};
  for (int index = 0; index < 24; ++index) {
    const int operation = index / 3;
    const int rate = kRates[index % 3];
    CAPTURE(operation, rate);
    CHECK(TestOptionalMovementWrapper(operation, rate) == 1);
  }
}

TEST_CASE(
    "Gimmick stack teardown accepts no top and retains supported fall behavior",
    "[gimmick][stack-top]") {
  CHECK(TestGimmickStackTop(0) == 1);
  CHECK(TestGimmickStackTop(1) == 1);
  CHECK(TestGimmickStackTop(2) == 1);
}
