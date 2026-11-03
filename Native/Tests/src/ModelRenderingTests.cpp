#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ModelDrawing.hpp"
#include "Actors/ModelRendering.hpp"
#include "Actors/ModelRenderingLive.hpp"
#include "Core/Gte.h"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Legacy/Psx.h"
#include "TestSupport/ChannelWarnings.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/WeaponModel.h"
#include "Field/Gimmicks.h"
}

// Contracts of the #276 row 22 room model pass and model transforms
// (BATTLE.PRG 44F14.c:826-1143 and 1695, 49268.c:48-245; INITBTL.PRG FB0.c:19).
namespace {
namespace actors = VS::Actors;
namespace math = VS::Core::Math;
using actors::GimmickPlacement;
using actors::GimmickPose;
using actors::GimmickTransform;
using actors::RoomActorState;
using actors::RoomGimmickState;
using actors::RoomWeaponState;
using actors::TextureSlotIds;
using actors::WeaponBone;
using actors::WeaponFade;
using actors::WeaponFadeStep;
using math::Angles3;
using math::Mat3;
using math::Transform3;
using math::Vec3;
using VS::test_support::ChannelWarnings;
using VS::test_support::Refused;

constexpr std::int16_t kOne = 4096;

[[nodiscard]] Mat3 Diagonal(const std::array<std::int16_t, 3>& scale) {
  Mat3 matrix;
  for (std::size_t axis = 0; axis < scale.size(); ++axis) {
    matrix.m[axis][axis] = scale[axis];
  }
  return matrix;
}

[[nodiscard]] Mat3 Identity() { return Diagonal({kOne, kOne, kOne}); }

[[nodiscard]] Vec3 GteRotateTranslate(const Transform3& transform, Vec3 point) {
  MATRIX matrix{};
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 3; ++column) {
      matrix.m[row][column] = transform.rotation.m[row][column];
    }
  }
  matrix.t[0] = transform.translation.x;
  matrix.t[1] = transform.translation.y;
  matrix.t[2] = transform.translation.z;
  const SVECTOR vertex{static_cast<std::int16_t>(point.x),
                       static_cast<std::int16_t>(point.y),
                       static_cast<std::int16_t>(point.z), 0};
  Core_Gte_SetRotMatrix(&matrix);
  Core_Gte_SetTransMatrix(&matrix);
  Core_Gte_LoadV0(&vertex);
  Core_Gte_Rtv0Tr();
  VECTOR result{};
  Core_Gte_StoreLvnl(&result);
  return Vec3{static_cast<std::int32_t>(result.vx),
              static_cast<std::int32_t>(result.vy),
              static_cast<std::int32_t>(result.vz)};
}

TEST_CASE("RotateTranslate matches the GTE's RTV0TR",
          "[actors-appearance-rendering]") {
  const std::array<Transform3, 3> transforms{
      Transform3{Identity(), Vec3{}},
      Transform3{math::BuildEulerRotationMatrix(Angles3{300, 1500, -700}),
                 Vec3{1000, -2000, 300000}},
      Transform3{math::BuildEulerRotationMatrixQuadrant(Angles3{4000, 9, 2048}),
                 Vec3{-70000, 5, -1}}};
  const std::array<Vec3, 4> points{Vec3{100, -200, 300}, Vec3{70000, -40000, 5},
                                   Vec3{32767, -32768, 0}, Vec3{-1, 1, -1}};
  for (const Transform3& transform : transforms) {
    for (const Vec3& point : points) {
      CHECK(math::RotateTranslate(transform, point) ==
            GteRotateTranslate(transform, point));
    }
  }
  Core_Gte_Reset();
}

constexpr std::uint32_t kVramColumns = 1024;

// Every page row of one column matches libgpu's GetTPage.
[[nodiscard]] bool PageColumnMatchesLibgpu(std::uint32_t color,
                                           std::uint32_t blend,
                                           std::uint32_t x) {
  return std::ranges::all_of(
      std::array{0U, 256U, 512U, 768U}, [=](std::uint32_t y) {
        return actors::TexturePageId(color, blend, {x, y}) ==
               GetTPage(static_cast<int>(color), static_cast<int>(blend),
                        static_cast<int>(x), static_cast<int>(y));
      });
}

TEST_CASE("Texture page ids match libgpu", "[actors-appearance-rendering]") {
  constexpr std::uint32_t kBlendModes = 4;
  constexpr std::uint32_t kPageStep = 64;
  for (std::uint32_t mode = 0; mode < 3 * kBlendModes; ++mode) {
    const std::uint32_t color = mode / kBlendModes;
    const std::uint32_t blend = mode % kBlendModes;
    for (std::uint32_t x = 0; x < kVramColumns; x += kPageStep) {
      CHECK(PageColumnMatchesLibgpu(color, blend, x));
    }
  }
}

TEST_CASE("Palette ids match libgpu", "[actors-appearance-rendering]") {
  constexpr std::uint32_t kRows = 512;
  constexpr std::uint32_t kPaletteStep = 16;
  for (std::uint32_t y = 0; y < kRows; y += 7) {
    for (std::uint32_t x = 0; x < kVramColumns; x += kPaletteStep) {
      CHECK(actors::ClutId({x, y}) ==
            GetClut(static_cast<int>(x), static_cast<int>(y)));
    }
  }
}

TEST_CASE("Actor and weapon texture slots take their pages and palette rows",
          "[actors-appearance-rendering]") {
  CHECK(actors::ActorTextureSlotIds(3, 5, 1) ==
        TextureSlotIds{GetTPage(0, 0, 5 << 6, 1 << 8), GetClut(768, 231),
                       GetClut(768, 249)});
  CHECK(actors::ActorTextureSlotIds(11, 15, 0) ==
        TextureSlotIds{GetTPage(0, 0, 15 << 6, 0), GetClut(768, 239),
                       GetClut(768, 257)});
  // Slots from 12 share one palette row and one alternate row.
  CHECK(actors::ActorTextureSlotIds(12, 2, 1) ==
        TextureSlotIds{GetTPage(0, 0, 2 << 6, 1 << 8), GetClut(768, 229),
                       GetClut(768, 247)});
  CHECK(actors::ActorTextureSlotIds(23, 0, 0).palette == GetClut(768, 229));
  CHECK(actors::WeaponTextureSlotIds(0) ==
        TextureSlotIds{GetTPage(1, 0, 0, 0), GetClut(928, 228), 0});
  CHECK(actors::WeaponTextureSlotIds(5) ==
        TextureSlotIds{GetTPage(1, 0, 128, 0), GetClut(976, 230), 0});
  CHECK(actors::WeaponTextureSlotIds(19) ==
        TextureSlotIds{GetTPage(1, 0, 576, 0), GetClut(976, 237), 0});
}

TEST_CASE("The actor root offset halves or scales the root key",
          "[actors-appearance-rendering]") {
  constexpr Vec3 kUnit{kOne, kOne, kOne};
  CHECK(actors::ActorRootOffset({8, -4, 12}, kUnit, kOne) == Vec3{4, -2, 6});
  // Unscaled keys are halved by an arithmetic shift, rounding down, and wrap
  // to a halfword first.
  CHECK(actors::ActorRootOffset({-3, 7, 0x18000}, kUnit, kOne) ==
        Vec3{-2, 3, -16384});
  // The actor scale truncates toward zero.
  CHECK(actors::ActorRootOffset({-3, 7, -32768}, kUnit, 6144) ==
        Vec3{-3, 4, -24576});
  // Any scaled instance multiplies every key by its scale over 8192.
  CHECK(actors::ActorRootOffset({100, -100, 50}, {8192, kOne, 2048}, kOne) ==
        Vec3{100, -50, 12});
  CHECK(actors::ActorRootOffset({100, -100, 50}, {8192, kOne, 2048}, 2048) ==
        Vec3{50, -25, 6});
}

TEST_CASE("The root offset turns with the actor and moves to its position",
          "[actors-appearance-rendering]") {
  const actors::RootTransform root = actors::TransformRootOffset(
      Angles3{0, 1024, 0}, Vec3{4, -2, 6}, Vec3{100, 200, 300});
  CHECK(root.root_offset == Vec3{106, 198, 296});
  CHECK(root.matrix.rotation ==
        math::BuildEulerRotationMatrix(Angles3{0, 1024, 0}));
  CHECK(root.matrix.translation == Vec3{6, -2, -4});
  // The moved offset wraps to a signed halfword.
  CHECK(actors::TransformRootOffset({}, Vec3{10, 0, 0}, Vec3{32760, 0, 0})
            .root_offset == Vec3{-32766, 0, 0});
}

TEST_CASE("Gimmick placement follows the object id, holder and mode",
          "[actors-appearance-rendering]") {
  using actors::ClassifyGimmickPlacement;
  CHECK(ClassifyGimmickPlacement(4, actors::kNoGimmickHolder, 0) ==
        GimmickPlacement::kFree);
  CHECK(ClassifyGimmickPlacement(4, 3, 0) == GimmickPlacement::kHeld);
  CHECK(ClassifyGimmickPlacement(7, 0, 1) == GimmickPlacement::kHeld);
  CHECK(ClassifyGimmickPlacement(3, 3, 0) == GimmickPlacement::kFree);
  CHECK(ClassifyGimmickPlacement(2, actors::kNoGimmickHolder, 1) ==
        GimmickPlacement::kFree);
  CHECK(ClassifyGimmickPlacement(1, 0, 1) == GimmickPlacement::kScripted);
  CHECK(ClassifyGimmickPlacement(0, 0, -1) == GimmickPlacement::kScripted);
  CHECK(ClassifyGimmickPlacement(1, actors::kNoGimmickHolder, 0) ==
        GimmickPlacement::kPlaced);
}

TEST_CASE("A placed gimmick keeps its placement, lifted and scaled",
          "[actors-appearance-rendering]") {
  GimmickPose pose;
  pose.placement = Identity();
  const Transform3 camera{Identity(), Vec3{}};
  const GimmickTransform placed =
      actors::PlaceGimmick(GimmickPlacement::kPlaced, pose, camera,
                           Vec3{5, 10, 15}, Vec3{kOne, kOne, kOne});
  CHECK(placed.position == Vec3{5, 26, 15});
  CHECK(placed.model.rotation == Identity());
  CHECK(placed.model.translation == Vec3{5, 26, 15});
  CHECK_FALSE(placed.rotation.has_value());
  CHECK_FALSE(placed.scale.has_value());
}

TEST_CASE("A placed gimmick takes the room scale, then its own",
          "[actors-appearance-rendering]") {
  GimmickPose pose;
  pose.placement = Identity();
  const Transform3 camera{Identity(), Vec3{}};
  // The room scale applies to the columns, then the gimmick's own scale.
  GimmickTransform placed = actors::PlaceGimmick(
      GimmickPlacement::kPlaced, pose, camera, Vec3{}, Vec3{2048, kOne, 8192});
  CHECK(placed.model.rotation == Diagonal({2048, kOne, 8192}));
  pose.scale = Vec3{128, 64, 32};
  placed = actors::PlaceGimmick(GimmickPlacement::kPlaced, pose, camera, Vec3{},
                                Vec3{2048, kOne, 8192});
  CHECK(placed.model.rotation == Identity());
  REQUIRE(placed.scale.has_value());
  CHECK(*placed.scale == Vec3{8192, kOne, 2048});
}

TEST_CASE("The camera turns a placed gimmick and moves its lifted center",
          "[actors-appearance-rendering]") {
  const Transform3 turned{math::BuildEulerRotationMatrix(Angles3{0, 512, 0}),
                          Vec3{10, 20, 30}};
  GimmickPose pose;
  pose.scale = Vec3{64, 64, 64};
  pose.placement =
      math::BuildEulerRotationMatrixQuadrant(Angles3{100, 200, 300});
  const GimmickTransform placed =
      actors::PlaceGimmick(GimmickPlacement::kPlaced, pose, turned,
                           Vec3{40, -50, 60}, Vec3{kOne, kOne, kOne});
  CHECK(placed.model.rotation ==
        math::PremultiplyRotationMatrix(turned.rotation, pose.placement));
  CHECK(placed.model.translation ==
        math::RotateTranslate(turned, Vec3{40, -34, 60}));
}

TEST_CASE("A free gimmick turns by its masked rotation plus offset",
          "[actors-appearance-rendering]") {
  GimmickPose pose;
  pose.rotation = Angles3{4000, 100, -5};
  pose.rotation_offset = Angles3{200, 0, 10};
  const GimmickTransform placed = actors::PlaceGimmick(
      GimmickPlacement::kFree, pose, Transform3{Identity(), Vec3{}},
      Vec3{1, 2, 3}, Vec3{kOne, kOne, kOne});
  REQUIRE(placed.rotation.has_value());
  CHECK(*placed.rotation == Vec3{104, 100, 5});
  CHECK(placed.model.rotation ==
        math::BuildEulerRotationMatrixQuadrant(Angles3{104, 100, 5}));
  CHECK(placed.position == Vec3{1, 18, 3});
}

TEST_CASE("A held gimmick sits between its holder's anchors",
          "[actors-appearance-rendering]") {
  GimmickPose pose;
  pose.rotation = Angles3{10, 20, 30};
  pose.rotation_offset = Angles3{1, 2, 3};
  pose.anchors = {Vec3{10, 20, -30}, Vec3{-3, 5, 7}};
  pose.holder_rotation = Angles3{0, 1024, 0};
  const GimmickTransform placed = actors::PlaceGimmick(
      GimmickPlacement::kHeld, pose, Transform3{Identity(), Vec3{}},
      Vec3{500, 500, 500}, Vec3{kOne, kOne, kOne});
  // The midpoint truncates toward zero; the center is not used.
  CHECK(placed.position == Vec3{3, 12, -11});
  CHECK(placed.model.translation == Vec3{3, 12, -11});
  REQUIRE(placed.rotation.has_value());
  CHECK(*placed.rotation == Vec3{0, 1024, 0});
  CHECK(placed.model.rotation ==
        math::PremultiplyRotationMatrix(
            math::BuildEulerRotationMatrixQuadrant(Angles3{0, 1024, 0}),
            math::BuildEulerRotationMatrixQuadrant(Angles3{11, 22, 33})));
}

TEST_CASE("A scripted gimmick saturates its placement product (#284)",
          "[actors-appearance-rendering]") {
  GimmickPose pose;
  pose.placement = Diagonal({0x2000, 0x2000, 0x2000});
  const Transform3 camera{Diagonal({0x7FFF, 0x7FFF, 0x7FFF}), Vec3{}};
  const GimmickTransform placed =
      actors::PlaceGimmick(GimmickPlacement::kScripted, pose, camera, Vec3{},
                           Vec3{kOne, kOne, kOne});
  // 0x7FFF * 2.0 saturates; the GTE does not wrap it to -2.
  CHECK(placed.model.rotation == Diagonal({0x7FFF, 0x7FFF, 0x7FFF}));
  REQUIRE(placed.rotation.has_value());
  CHECK(*placed.rotation == Vec3{});

  // The camera is applied before the placement, not again after it.
  pose.rotation = Angles3{0, 0x10200, 0};
  pose.placement = math::BuildEulerRotationMatrixQuadrant(Angles3{300, 0, 0});
  const Transform3 turned{math::BuildEulerRotationMatrix(Angles3{0, 512, 0}),
                          Vec3{}};
  const GimmickTransform scripted =
      actors::PlaceGimmick(GimmickPlacement::kScripted, pose, turned, Vec3{},
                           Vec3{kOne, kOne, kOne});
  CHECK(scripted.model.rotation ==
        math::MultiplyMatrixIntoLeft(
            math::PremultiplyRotationMatrix(
                turned.rotation,
                math::BuildEulerRotationMatrix(Angles3{0, 0x200, 0})),
            pose.placement));
  REQUIRE(scripted.rotation.has_value());
  CHECK(*scripted.rotation == Vec3{0, 0x200, 0});
}

TEST_CASE("Gimmick centers, texture pages and palettes",
          "[actors-appearance-rendering]") {
  CHECK(actors::GimmickCenter({5, 100, 7}, 31) == Vec3{5, 115, 7});
  CHECK(actors::GimmickCenter({5, 100, 7}, -31) == Vec3{5, 85, 7});
  CHECK(actors::GimmickCenter({5, 32767, 7}, 2) == Vec3{5, -32768, 7});
  CHECK(actors::GimmickTexturePage() == GetTPage(0, 0, 64, 256));
  CHECK(actors::GimmickPalette(2, false, 0) == GetClut(800, 237));
  CHECK(actors::GimmickPalette(2, false, 5) == GetClut(800, 237));
  CHECK(actors::GimmickPalette(2, true, 0) == GetClut(800, 255));
  CHECK(actors::GimmickPalette(15, false, 6) == GetClut(1008, 255));
}

TEST_CASE("Gimmick and weapon motion states select draw overrides",
          "[actors-appearance-rendering]") {
  for (std::uint32_t state = 0; state < 16; ++state) {
    const bool gimmick_override =
        state == 4 || state == 5 || state == 7 || state == 8 || state >= 12;
    const bool weapon_override =
        state == 4 || state == 5 || state == 7 || state == 8 || state == 15;
    CHECK(actors::GimmickDrawsWithOverrides(state) == gimmick_override);
    CHECK(actors::WeaponDrawsWithOverrides(state) == weapon_override);
  }
  CHECK(actors::ModelColumnScale({64, 64, 64}) == std::nullopt);
  CHECK(actors::ModelColumnScale({64, 64, 65}) == Vec3{4096, 4096, 4160});
}

TEST_CASE("A weapon fade steps an even share of the change",
          "[actors-appearance-rendering]") {
  WeaponFadeStep step = actors::StepWeaponFade(WeaponFade{64, 0, 0});
  CHECK(step.fade == WeaponFade{64, 0, 0});
  CHECK_FALSE(step.hidden);

  std::vector<std::uint8_t> scales;
  WeaponFade fade{64, 0, 3};
  do {
    step = actors::StepWeaponFade(fade);
    fade = step.fade;
    scales.push_back(fade.scale);
    CHECK(step.hidden == (fade.frames == 0));
  } while (fade.frames != 0);
  // Each share truncates toward zero.
  CHECK(scales == std::vector<std::uint8_t>{43, 22, 0});

  fade = WeaponFade{32, 64, 3};
  scales.clear();
  do {
    step = actors::StepWeaponFade(fade);
    fade = step.fade;
    scales.push_back(fade.scale);
    CHECK_FALSE(step.hidden);
  } while (fade.frames != 0);
  CHECK(scales == std::vector<std::uint8_t>{42, 53, 64});
}

TEST_CASE("The weapon root scales its attachment except at unit scale",
          "[actors-appearance-rendering]") {
  const Transform3 attachment{Identity(), Vec3{7, 8, 9}};
  const Transform3 unit = actors::WeaponRootTransform(attachment, 64);
  CHECK(unit.rotation == Identity());
  CHECK(unit.translation == Vec3{7, 8, 9});
  const Transform3 half = actors::WeaponRootTransform(attachment, 32);
  CHECK(half.rotation == Diagonal({2048, 2048, 2048}));
  CHECK(half.translation == Vec3{7, 8, 9});
}

TEST_CASE("Weapon bones chain from their parents' tips",
          "[actors-appearance-rendering]") {
  const Transform3 root{Identity(), Vec3{100, 0, 0}};
  std::array<WeaponBone, 5> bones{};
  bones[0].length = 50;
  bones[1].parent = 0;
  bones[1].length = 20;
  bones[2].parent = 1;
  bones[2].rotation = Angles3{4096 + 100, 0, -4096};

  auto chain = actors::BuildWeaponBoneChain(root, std::span(bones).first(1));
  REQUIRE(chain.has_value());
  CHECK((*chain)[0].translation == Vec3{100, 0, 0});

  chain = actors::BuildWeaponBoneChain(root, std::span(bones).first(3));
  REQUIRE(chain.has_value());
  CHECK((*chain)[1].rotation == Identity());
  CHECK((*chain)[1].translation == Vec3{50, 0, 0});
  // Angles are masked to a turn.
  CHECK((*chain)[2].rotation ==
        math::BuildEulerRotationMatrixQuadrant(Angles3{100, 0, 0}));
  CHECK((*chain)[2].translation == Vec3{30, 0, 0});
}

TEST_CASE("A weapon bone chain refuses too many bones or a late parent",
          "[actors-appearance-rendering]") {
  const Transform3 root{Identity(), Vec3{100, 0, 0}};
  std::array<WeaponBone, 5> bones{};
  bones[1].parent = 0;
  bones[2].parent = 1;
  CHECK_FALSE(actors::BuildWeaponBoneChain(root, bones).has_value());
  bones[2].parent = 2;
  CHECK_FALSE(actors::BuildWeaponBoneChain(root, std::span(bones).first(3))
                  .has_value());
}

TEST_CASE("Weapon owners gate the weapon pass",
          "[actors-appearance-rendering]") {
  const int high = actors::kHiddenActorHeight;
  CHECK_FALSE(actors::OwnerShowsWeapon(false, 0, false, true));
  CHECK(actors::OwnerShowsWeapon(true, high - 1, true, false));
  CHECK_FALSE(actors::OwnerShowsWeapon(true, high, false, false));
  CHECK(actors::OwnerShowsWeapon(true, high, false, true));
  CHECK_FALSE(actors::OwnerShowsWeapon(true, high, true, true));
}

TEST_CASE("Room actors gate the room pass and carry weapons",
          "[actors-appearance-rendering]") {
  RoomActorState actor{.has_bones = true, .visible = true};
  CHECK(actors::DrawsInRoomPass(actor));
  CHECK(actors::OwnerCarriesWeapon(actor));
  actor.attach_bone = 0xFC;
  CHECK_FALSE(actors::DrawsInRoomPass(actor));
  actor = RoomActorState{.has_bones = true, .skipped = true, .visible = true};
  CHECK_FALSE(actors::DrawsInRoomPass(actor));
  CHECK_FALSE(actors::OwnerCarriesWeapon(actor));
  CHECK_FALSE(actors::DrawsInRoomPass(RoomActorState{}));
  CHECK_FALSE(actors::OwnerCarriesWeapon(RoomActorState{.visible = false}));
  CHECK_FALSE(actors::OwnerCarriesWeapon(
      RoomActorState{.visible = true, .culled = true}));
}

TEST_CASE("A room actor is marked drawn from its radius and blend frame",
          "[actors-appearance-rendering]") {
  RoomActorState actor{.collision_radius = 0x7F, .blend_frame = 1};
  CHECK_FALSE(actors::MarksActorDrawn(actor));
  actor.collision_radius = 0x80;
  CHECK(actors::MarksActorDrawn(actor));
  actor.blend_frame = 0;
  CHECK_FALSE(actors::MarksActorDrawn(actor));
}

// Records each host call of the room pass in order.
class RecordingHost final : public actors::RoomModelPassHost {
 public:
  std::array<std::optional<RoomActorState>, actors::kRoomActorSlots>
      room_actors{};
  std::array<std::optional<RoomWeaponState>, actors::kRoomWeaponSlots>
      room_weapons{};
  std::array<std::optional<RoomGimmickState>, actors::kRoomGimmickSlots>
      room_gimmicks{};
  actors::PushedGimmick pushed{.index = -1};
  // The actor that leaves the table during its texture animations, and the
  // gimmick that leaves it while being centered.
  std::optional<std::size_t> vanishing_actor;
  std::optional<std::size_t> vanishing_gimmick;
  std::vector<std::string> calls;

  void PrepareCamera() override { Record("camera"); }
  [[nodiscard]] std::optional<RoomActorState> Actor(
      std::size_t slot) const override {
    if (slot >= room_actors.size()) return std::nullopt;
    return room_actors[slot];
  }
  void UpdateTextureAnimations(std::size_t slot) override {
    Record("textures", slot);
    if (vanishing_actor == slot) room_actors[slot].reset();
  }
  void UpdatePalettePulse(std::size_t slot) override { Record("pulse", slot); }
  void UpdateAppearanceTransition(std::size_t slot) override {
    Record("transition", slot);
  }
  void SetDrawn(std::size_t slot, bool drawn) override {
    Record(drawn ? "drawn" : "undrawn", slot);
  }
  void UpdateSoundTransition(std::size_t slot) override {
    Record("sound", slot);
  }
  void UpdateTextureScrolling(std::size_t slot,
                              std::size_t carried_index) override {
    Record("scroll", slot, static_cast<std::int64_t>(carried_index));
  }
  void StepFrozenAnimation(std::size_t slot) override { Record("step", slot); }
  void PrepareRootTransform(std::size_t slot) override { Record("root", slot); }
  void DrawAttached(std::size_t slot) override { Record("attached", slot); }
  void DrawActor(std::size_t slot, std::uint8_t companion) override {
    Record("draw", slot, companion);
  }
  void MoveActor(std::size_t slot, Vec3 offset) override {
    Record("move", slot, offset.x);
  }
  void OffsetOrderingTable(std::int32_t entries) override {
    calls.push_back("ot " + std::to_string(entries));
  }
  void UpdateMotionAndLookAt(std::size_t slot,
                             std::uint8_t companion) override {
    Record("motion", slot, companion);
  }
  [[nodiscard]] std::optional<RoomWeaponState> Weapon(
      std::size_t index) const override {
    return room_weapons.at(index);
  }
  void DrawWeapon(std::size_t index) override { Record("weapon", index); }
  [[nodiscard]] std::optional<RoomGimmickState> Gimmick(
      std::size_t index) const override {
    return room_gimmicks.at(index);
  }
  [[nodiscard]] actors::PushedGimmick PushedGimmickState() const override {
    return pushed;
  }
  void UpdateGimmickMotion(std::size_t index) override {
    Record("gimmick-motion", index);
  }
  void CenterGimmick(std::size_t index) override {
    Record("center", index);
    if (vanishing_gimmick == index) room_gimmicks.at(index).reset();
  }
  void UpdateGimmickPalettePulse(std::size_t index) override {
    Record("gimmick-pulse", index);
  }
  void UpdateGimmickAppearanceTransition(std::size_t index) override {
    Record("gimmick-transition", index);
  }
  void DrawGimmick(std::size_t index) override {
    Record("gimmick-draw", index);
  }
  void ClearGimmickPushUpdate() override { Record("clear-push"); }
  void CountPass() override { Record("count"); }

 private:
  void Record(const std::string& call) { calls.push_back(call); }
  void Record(const std::string& call, std::size_t index) {
    calls.push_back(call + " " + std::to_string(index));
  }
  void Record(const std::string& call, std::size_t index,
              std::int64_t argument) {
    calls.push_back(call + " " + std::to_string(index) + " " +
                    std::to_string(argument));
  }
};

// One room actor per branch of the actor pass.
void AddRoomPassActors(RecordingHost& host) {
  host.room_actors[0] = RoomActorState{.has_bones = true,
                                       .visible = true,
                                       .collision_radius = 0x80,
                                       .blend_frame = 1};
  // Two frozen actors of one model: only the first scrolls the texture.
  host.room_actors[1] = RoomActorState{.has_bones = true,
                                       .visible = true,
                                       .frozen = true,
                                       .pulsing = true,
                                       .model_id = 7};
  host.room_actors[2] = RoomActorState{.has_bones = true,
                                       .visible = true,
                                       .frozen = true,
                                       .transitioning = true,
                                       .model_id = 7};
  host.room_actors[3] = RoomActorState{.has_bones = true, .attached = true};
  // A frozen actor with a linked companion, which is not drawn on its own.
  host.room_actors[4] = RoomActorState{.has_bones = true,
                                       .visible = true,
                                       .frozen = true,
                                       .companion_slot = 5,
                                       .model_id = 9,
                                       .companion_offset = Vec3{11, 0, 0},
                                       .depth_bias = 3};
  host.room_actors[5] = RoomActorState{.model_id = 11};
  host.room_actors[6] =
      RoomActorState{.has_bones = true, .skipped = true, .visible = true};
  host.room_actors[7] =
      RoomActorState{.has_bones = true, .visible = true, .attach_bone = 0xFC};
  host.room_actors[8] = RoomActorState{.has_bones = true, .visible = true};
  host.vanishing_actor = 8;
  // Companion 1 is the actor's own second model: no appearance of its own.
  host.room_actors[9] = RoomActorState{
      .has_bones = true, .visible = true, .companion_slot = 1, .depth_bias = 2};
}

// Weapons of shown, hidden, unattached and out-of-table owners.
void AddRoomPassWeapons(RecordingHost& host) {
  host.room_weapons[0] =
      RoomWeaponState{.shown = true, .owner = 0, .attachment_bone = 1};
  host.room_weapons[1] =
      RoomWeaponState{.shown = true, .owner = 3, .attachment_bone = 1};
  host.room_weapons[2] = RoomWeaponState{.owner = 0, .attachment_bone = 1};
  host.room_weapons[3] = RoomWeaponState{.shown = true, .owner = 0};
  host.room_weapons[4] =
      RoomWeaponState{.shown = true, .owner = 10, .attachment_bone = 1};
  host.room_weapons[5] =
      RoomWeaponState{.shown = true, .owner = 200, .attachment_bone = 1};
}

void AddRoomPassGimmicks(RecordingHost& host) {
  host.room_gimmicks[0] = RoomGimmickState{
      .has_bones = true, .pulsing = true, .motion_state = 3, .mode = 0xF7};
  host.room_gimmicks[1] = RoomGimmickState{.pulsing = true};
  host.room_gimmicks[2] = RoomGimmickState{.has_bones = true};
  host.room_gimmicks[3] = RoomGimmickState{.has_bones = true};
  host.room_gimmicks[4] = RoomGimmickState{.has_bones = true};
  host.vanishing_gimmick = 4;
  host.pushed = actors::PushedGimmick{.index = 2};
}

TEST_CASE("The room pass visits actors, weapons and gimmicks in order",
          "[actors-appearance-rendering]") {
  RecordingHost host;
  AddRoomPassActors(host);
  AddRoomPassWeapons(host);
  AddRoomPassGimmicks(host);

  actors::UpdateAndDrawRoomModels(host);
  const std::vector<std::string> expected{"camera",
                                          "textures 0",
                                          "undrawn 0",
                                          "root 0",
                                          "draw 0 0",
                                          "drawn 0",
                                          "textures 1",
                                          "pulse 1",
                                          "undrawn 1",
                                          "sound 1",
                                          "scroll 1 1",
                                          "step 1",
                                          "draw 1 0",
                                          "textures 2",
                                          "transition 2",
                                          "undrawn 2",
                                          "sound 2",
                                          "step 2",
                                          "draw 2 0",
                                          "textures 3",
                                          "undrawn 3",
                                          "root 3",
                                          "attached 3",
                                          "textures 4",
                                          "undrawn 4",
                                          "sound 4",
                                          "scroll 4 4",
                                          "step 4",
                                          "draw 4 0",
                                          "textures 5",
                                          "sound 5",
                                          "scroll 5 4",
                                          "ot 3",
                                          "move 4 11",
                                          "motion 4 5",
                                          "draw 4 5",
                                          "ot -3",
                                          "move 4 -11",
                                          "textures 8",
                                          "textures 9",
                                          "undrawn 9",
                                          "root 9",
                                          "draw 9 0",
                                          "ot 2",
                                          "move 9 0",
                                          "motion 9 1",
                                          "draw 9 1",
                                          "ot -2",
                                          "move 9 0",
                                          "weapon 0",
                                          "ot 4",
                                          "gimmick-motion 0",
                                          "center 0",
                                          "gimmick-pulse 0",
                                          "gimmick-transition 0",
                                          "gimmick-draw 0",
                                          "gimmick-motion 2",
                                          "center 2",
                                          "gimmick-draw 2",
                                          "center 3",
                                          "gimmick-draw 3",
                                          "center 4",
                                          "clear-push",
                                          "ot -4",
                                          "count"};
  CHECK(host.calls == expected);
}

TEST_CASE("A gimmick push handled this frame does not step its motion",
          "[actors-appearance-rendering]") {
  RecordingHost host;
  host.room_gimmicks[2] = RoomGimmickState{.has_bones = true};
  host.pushed = actors::PushedGimmick{.index = 2, .update_complete = true};
  actors::UpdateAndDrawRoomModels(host);
  CHECK(host.calls == std::vector<std::string>{"camera", "ot 4", "center 2",
                                               "gimmick-draw 2", "clear-push",
                                               "ot -4", "count"});
}

TEST_CASE("The live model transforms refuse missing records",
          "[actors-appearance-rendering]") {
  ChannelWarnings warnings("actors");
  MATRIX camera{};
  actors::PrepareLiveActorRootTransform(nullptr);
  actors::TransformLiveActorRootOffset(nullptr);
  actors::UpdateAndDrawLiveRoomActors(nullptr);
  actors::DrawLiveGimmickModel(nullptr, &camera);
  actors::DrawLiveActorWeaponModel(nullptr, &camera);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused("PrepareActorRootTransform", "no actor record"),
            Refused("TransformActorRootOffset", "no actor record"),
            Refused("UpdateAndDrawRoomActors", "no camera matrix"),
            Refused("DrawGimmickModel", "no gimmick or camera matrix"),
            Refused("DrawActorWeaponModel", "no weapon or attachment matrix")});
}

TEST_CASE("A held gimmick whose holder slot is empty is not drawn",
          "[actors-appearance-rendering]") {
  constexpr std::size_t kHolder = 6;
  Actors_ActorRecord* const saved = Actors_actorRecords[kHolder];
  Actors_actorRecords[kHolder] = nullptr;
  Field_GimmickInstance object{};
  object.objectInfo.objectId = 4;
  object.parent = static_cast<u_char>(kHolder);
  object.slot = 3;
  MATRIX camera{};
  {
    ChannelWarnings warnings("actors");
    actors::DrawLiveGimmickModel(&object, &camera);
    CHECK(warnings.messages ==
          std::vector<std::string>{Refused(
              "DrawGimmickModel", "gimmick 3's holder slot 6 is empty")});
  }
  // Nothing of the gimmick was written.
  CHECK(object.matrices[0].m[0][0] == 0);
  CHECK(object.matrices[1].t[0] == 0);
  Actors_actorRecords[kHolder] = saved;
}

TEST_CASE("A weapon with more bones than its chain holds is not drawn",
          "[actors-appearance-rendering]") {
  Actors_WeaponModel weapon{};
  weapon.boneCount = 5;
  weapon.reserved11 = 64;
  weapon.reserved12 = 0;
  weapon.reserved13 = 4;
  MATRIX source{};
  ChannelWarnings warnings("actors");
  actors::DrawLiveActorWeaponModel(&weapon, &source);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused("DrawActorWeaponModel", "the weapon has 5 bones")});
  // The fade did not step.
  CHECK(weapon.reserved11 == 64);
  CHECK(weapon.reserved13 == 4);
}
}  // namespace
