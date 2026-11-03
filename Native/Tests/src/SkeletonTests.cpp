#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/Appearance.hpp"
#include "Actors/Skeleton.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Content/ModuleData.h"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Render/ModelGeometryLive.hpp"
#include "TestSupport/ChannelWarnings.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Actors/SkeletonLiveState.h"
#include "Actors/WeaponModel.h"
#include "Field/Gimmicks.h"
}

namespace actors = VS::Actors;
using actors::BoneLabels;
using actors::BoneTagKind;
using VS::Core::Math::Mat3;
using VS::Core::Math::Transform3;
using VS::Core::Math::Vec3;
using VS::test_support::Refused;

namespace {
constexpr std::int16_t kOne = 4096;
constexpr std::size_t kTestBones = 4;
constexpr int kTagKind = static_cast<int>(BoneTagKind::kTag);
constexpr int kHighLimbKind = static_cast<int>(BoneTagKind::kHighLimb);
constexpr int kAuxiliaryKind = static_cast<int>(BoneTagKind::kAuxiliary);

// An SHP model header followed by its bone table, as the loaders place it.
struct TestModel {
  Actors_ShpModel header{};
  std::array<Actors_ShpBone, kTestBones> bones{};
};
static_assert(offsetof(TestModel, bones) == sizeof(Actors_ShpModel));

// Bone 0 is the root; bone 1 the head (auxiliary tag 2) in limb groups 1 and
// 0; bone 2 has tag 7 in limb groups 0 and 1; bone 3 repeats tag 5.
constexpr std::array<BoneLabels, kTestBones> kLabels{{
    {.tag = 1, .limb_ids = 0x00, .auxiliary_tag = 0},
    {.tag = 5, .limb_ids = 0x21, .auxiliary_tag = 2},
    {.tag = 7, .limb_ids = 0x12, .auxiliary_tag = 9},
    {.tag = 5, .limb_ids = 0x30, .auxiliary_tag = 0},
}};

[[nodiscard]] MATRIX TranslationMatrix(Vec3 translation) {
  MATRIX matrix{};
  matrix.m[0][0] = kOne;
  matrix.m[1][1] = kOne;
  matrix.m[2][2] = kOne;
  matrix.t[0] = translation.x;
  matrix.t[1] = translation.y;
  matrix.t[2] = translation.z;
  return matrix;
}

[[nodiscard]] Mat3 QuarterTurnAboutZ() {
  Mat3 rotation;
  rotation.m = {
      {{0, static_cast<std::int16_t>(-kOne), 0}, {kOne, 0, 0}, {0, 0, kOne}}};
  return rotation;
}

[[nodiscard]] Vec3 PointOf(const SVECTOR& vector) {
  return {vector.vx, vector.vy, vector.vz};
}

[[nodiscard]] Vec3 TranslationOf(const MATRIX& matrix) {
  return {matrix.t[0], matrix.t[1], matrix.t[2]};
}

// The actor, gimmick and weapon tables and the bone camera, emptied for one
// test and then restored. The bone camera sits at (10, 20, 30) unrotated.
struct LiveSkeletonState {
  decltype(std::to_array(Actors_actorRecords)) saved_actors =
      std::to_array(Actors_actorRecords);
  decltype(std::to_array(Actors_weaponModels)) saved_weapons =
      std::to_array(Actors_weaponModels);
  decltype(std::to_array(Field_gimmickInstances)) saved_gimmicks =
      std::to_array(Field_gimmickInstances);
  MATRIX saved_camera = Actors_boneMatrixScratch;
  std::vector<std::unique_ptr<Actors_ActorRecord>> records;
  std::vector<std::unique_ptr<Field_GimmickInstance>> gimmicks;
  std::vector<std::unique_ptr<Actors_WeaponModel>> weapons;
  std::vector<std::unique_ptr<TestModel>> models;

  LiveSkeletonState() {
    std::fill(std::begin(Actors_actorRecords), std::end(Actors_actorRecords),
              nullptr);
    std::fill(std::begin(Actors_weaponModels), std::end(Actors_weaponModels),
              nullptr);
    std::fill(std::begin(Field_gimmickInstances),
              std::end(Field_gimmickInstances), nullptr);
    Actors_boneMatrixScratch = TranslationMatrix({10, 20, 30});
  }
  LiveSkeletonState(const LiveSkeletonState&) = delete;
  LiveSkeletonState& operator=(const LiveSkeletonState&) = delete;
  LiveSkeletonState(LiveSkeletonState&&) = delete;
  LiveSkeletonState& operator=(LiveSkeletonState&&) = delete;
  ~LiveSkeletonState() {
    std::ranges::copy(saved_actors, std::begin(Actors_actorRecords));
    std::ranges::copy(saved_weapons, std::begin(Actors_weaponModels));
    std::ranges::copy(saved_gimmicks, std::begin(Field_gimmickInstances));
    Actors_boneMatrixScratch = saved_camera;
    for (const auto& model : models) {
      VS::Render::ForgetLiveModelExtent(model.get());
    }
  }

  // A model with the test labels whose extent the loader recorded.
  TestModel& AddModel() {
    models.push_back(std::make_unique<TestModel>());
    TestModel& model = *models.back();
    for (std::size_t bone = 0; bone < kTestBones; ++bone) {
      model.bones.at(bone).tag = kLabels.at(bone).tag;
      model.bones.at(bone).limbIds = kLabels.at(bone).limb_ids;
      model.bones.at(bone).auxiliaryTag = kLabels.at(bone).auxiliary_tag;
    }
    VS::Render::RecordLiveModelExtent(&model, sizeof model);
    return model;
  }

  Actors_ActorRecord& AddActor(int actor_id, TestModel& model) {
    records.push_back(std::make_unique<Actors_ActorRecord>());
    Actors_ActorRecord& actor = *records.back();
    actor.instance.slot = static_cast<u_char>(actor_id);
    actor.instance.model = &model.header;
    actor.instance.boneCount = static_cast<u_char>(kTestBones);
    actor.modelDataSize = static_cast<unsigned>(sizeof model);
    actor.scale = kOne;
    Actors_actorRecords[actor_id] = &actor;
    return actor;
  }

  Field_GimmickInstance& AddGimmick(int actor_id, TestModel& model) {
    gimmicks.push_back(std::make_unique<Field_GimmickInstance>());
    Field_GimmickInstance& gimmick = *gimmicks.back();
    gimmick.model = &model.header;
    gimmick.boneCount = static_cast<char>(kTestBones);
    Field_gimmickInstances[actor_id] = &gimmick;
    return gimmick;
  }

  Actors_WeaponModel& AddFirstWeapon(int actor_id, TestModel& model) {
    weapons.push_back(std::make_unique<Actors_WeaponModel>());
    Actors_WeaponModel& weapon = *weapons.back();
    weapon.sectionOffsets = reinterpret_cast<Actors_WeaponModelData*>(&model);
    Actors_weaponModels[static_cast<std::size_t>(actor_id) *
                        actors::kWeaponsPerActor] = &weapon;
    return weapon;
  }
};

// Actor 2 with bones 1 and 2 posed in front of the bone camera.
Actors_ActorRecord& AddPosedActor(LiveSkeletonState& state) {
  TestModel& model = state.AddModel();
  Actors_ActorRecord& actor = state.AddActor(2, model);
  actor.boneMatrices[1] = TranslationMatrix({10, 20, 130});
  actor.boneMatrices[2] = TranslationMatrix({110, 220, 330});
  model.bones[2].length = 64;
  return actor;
}
}  // namespace

TEST_CASE("Bone tag searches match each label kind and the root at once",
          "[actors-animation-skeleton][actors]") {
  CHECK(actors::FindBoneTag(kLabels, 5, BoneTagKind::kTag) == 1U);
  // Only the tag's low byte is compared.
  CHECK(actors::FindBoneTag(kLabels, 0x107, BoneTagKind::kTag) == 2U);
  // A limb group matches its nibble less one.
  CHECK(actors::FindBoneTag(kLabels, 1, BoneTagKind::kHighLimb) == 1U);
  CHECK(actors::FindBoneTag(kLabels, 2, BoneTagKind::kHighLimb) == 3U);
  CHECK(actors::FindBoneTag(kLabels, 0, BoneTagKind::kLowLimb) == 1U);
  CHECK(actors::FindBoneTag(kLabels, 1, BoneTagKind::kLowLimb) == 2U);
  CHECK(actors::FindBoneTag(kLabels, 9, BoneTagKind::kAuxiliary) == 2U);
  CHECK_FALSE(actors::FindBoneTag(kLabels, 6, BoneTagKind::kTag));
  CHECK(actors::FindBoneTag({}, actors::kRootSelector, BoneTagKind::kTag) ==
        0U);
}

TEST_CASE("Bone tag kinds accept only the four original label kinds",
          "[actors-animation-skeleton][actors]") {
  CHECK(actors::ClassifyBoneTagKind(0) == BoneTagKind::kTag);
  CHECK(actors::ClassifyBoneTagKind(1) == BoneTagKind::kHighLimb);
  CHECK(actors::ClassifyBoneTagKind(2) == BoneTagKind::kLowLimb);
  CHECK(actors::ClassifyBoneTagKind(3) == BoneTagKind::kAuxiliary);
  CHECK_FALSE(actors::ClassifyBoneTagKind(4));
  CHECK_FALSE(actors::ClassifyBoneTagKind(-1));
}

TEST_CASE("Bone tag values read a label, with no limb group for nibble 0",
          "[actors-animation-skeleton][actors]") {
  CHECK(actors::BoneTagValue(kLabels[1], BoneTagKind::kTag) == 5);
  CHECK(actors::BoneTagValue(kLabels[1], BoneTagKind::kHighLimb) == 2);
  CHECK(actors::BoneTagValue(kLabels[1], BoneTagKind::kLowLimb) == 1);
  CHECK(actors::BoneTagValue(kLabels[2], BoneTagKind::kAuxiliary) == 9);
  CHECK_FALSE(actors::BoneTagValue(kLabels[0], BoneTagKind::kHighLimb));
  CHECK_FALSE(actors::BoneTagValue(kLabels[3], BoneTagKind::kLowLimb));
  CHECK(actors::IsWeaponBoneSelector(0x40));
  CHECK(actors::IsWeaponBoneSelector(0x4F));
  CHECK_FALSE(actors::IsWeaponBoneSelector(0x50));
  CHECK(actors::WeaponBoneIndex(0x40) == 1);
}

TEST_CASE("Head tracking takes the aim and step a request gives",
          "[actors-animation-skeleton][actors]") {
  CHECK(actors::HeadTrackingSpeed(128) == 30);
  CHECK(actors::HeadTrackingSpeed(129) == 20);
  const actors::HeadTracking current{
      .target = 1, .aim_x = 11, .aim_y = 22, .aim_z = 33, .speed = 4};
  const actors::ActorLook look{
      .pitch = 0x123, .yaw = 0xF456, .collision_radius = 200};

  const actors::HeadTracking aimed = actors::StartHeadTracking(
      current, {.target = 3, .aim = Vec3{0x12345, -2, 7}, .step = 9}, look);
  CHECK(aimed.target == 3);
  CHECK(aimed.aim_x == 0x2345);
  CHECK(aimed.aim_y == -2);
  CHECK(aimed.aim_z == 7);
  CHECK(aimed.speed == 9);
}

TEST_CASE("Head tracking without an aim follows the look angles",
          "[actors-animation-skeleton][actors]") {
  const actors::HeadTracking current{
      .target = 1, .aim_x = 11, .aim_y = 22, .aim_z = 33, .speed = 4};
  const actors::ActorLook look{
      .pitch = 0x123, .yaw = 0xF456, .collision_radius = 200};
  const actors::HeadTracking looking =
      actors::StartHeadTracking(current,
                                {.target = 2,
                                 .aim = std::nullopt,
                                 .step = actors::kHeadTrackingUsesRadiusStep},
                                look);
  CHECK(looking.target == 2);
  CHECK(looking.aim_x == 0x123);
  CHECK(looking.aim_y == static_cast<std::int16_t>(0xF456));
  CHECK(looking.aim_z == 33);
  CHECK(looking.speed == 20);
}

TEST_CASE("Pitch to a point measures the wrapped horizontal distance",
          "[actors-animation-skeleton][actors]") {
  using VS::Core::Math::Arctangent2;
  using VS::Core::Math::SquareRootInteger;
  CHECK(actors::PitchToPoint({}, {300, 600, 400}, 100) ==
        Arctangent2(500, SquareRootInteger((300 * 300) + (400 * 400))));
  // 50000 squared wraps negative and the original negated it.
  CHECK(actors::PitchToPoint({}, {50000, 0, 0}, 0) ==
        Arctangent2(0, SquareRootInteger(1794967296)));
}

TEST_CASE("Bone view transforms rotate the bone's offset into camera space",
          "[actors-animation-skeleton][actors]") {
  const Transform3 camera{QuarterTurnAboutZ(), {10, 20, 30}};
  const Transform3 bone{QuarterTurnAboutZ(), {110, 20, 30}};
  const Transform3 view = actors::BoneViewTransform(camera, bone);
  CHECK(view.translation == Vec3{0, 100, 0});
  Mat3 half_turn;
  half_turn.m = {{{static_cast<std::int16_t>(-kOne), 0, 0},
                  {0, static_cast<std::int16_t>(-kOne), 0},
                  {0, 0, kOne}}};
  CHECK(view.rotation == half_turn);

  // MulMatrix saturates each element.
  Mat3 large;
  for (auto& row : large.m) row.fill(INT16_MAX);
  const Transform3 saturated =
      actors::BoneViewTransform({large, {}}, {large, {}});
  CHECK(saturated.rotation.m[0][0] == INT16_MAX);
}

TEST_CASE("The root selector halves and scales the root key at the actor",
          "[actors-animation-skeleton][actors]") {
  const Transform3 unit = actors::RootSelectorTransform(
      {}, {100, 200, 300}, kOne, {1000, 2000, 3000});
  CHECK(unit.rotation == VS::Core::Math::IdentityRotation());
  CHECK(unit.translation == Vec3{1050, 2100, 3150});
  const Transform3 half = actors::RootSelectorTransform(
      {}, {100, 200, 300}, kOne / 2, {1000, 2000, 3000});
  CHECK(half.translation == Vec3{1025, 2050, 3075});
}

TEST_CASE("Bone endpoints and the selector points truncate to halfwords",
          "[actors-animation-skeleton][actors]") {
  const Transform3 placed{VS::Core::Math::IdentityRotation(),
                          {0x12345, 20, 30}};
  CHECK(actors::BoneEndpointPoint(placed, 100, actors::BoneEndpoint::kBase) ==
        Vec3{0x2345, 20, 30});
  CHECK(actors::BoneEndpointPoint(placed, 100, actors::BoneEndpoint::kTip) ==
        Vec3{0x2345 - 100, 20, 30});
  CHECK(actors::BoneEndpointPoint(placed, 100, actors::BoneEndpoint::kMiddle) ==
        Vec3{0x2345 - 50, 20, 30});

  CHECK(actors::TargetPoint({1, 100, 3}, 40) == Vec3{1, 60, 3});
  CHECK(actors::TargetPoint({1, -32760, 3}, 100) == Vec3{1, 32676, 3});
  // C division truncates the scaled anchor height toward zero.
  CHECK(actors::AnchorPoint({0, 10, 0}, -3, kOne + 1) == Vec3{0, 7, 0});
  CHECK(actors::AnchorPoint({0, 10, 0}, 0x100, 2 * kOne) == Vec3{0, 522, 0});
  CHECK(actors::SplitWord(static_cast<std::int32_t>(0xFFFE0010U)) ==
        actors::WordHalves{.low = 16, .high = -2});
}

TEST_CASE("Live bone tags search the actor's bones, else the gimmick's",
          "[actors-animation-skeleton][actors][redirect-binding]") {
  LiveSkeletonState state;
  TestModel& model = state.AddModel();
  state.AddActor(2, model);
  state.AddGimmick(4, model);
  VS::test_support::ChannelWarnings warnings{"actors"};

  CHECK(actors::FindLiveActorBoneTag(2, 7, kTagKind) == 2);
  CHECK(actors::FindLiveActorBoneTag(2, 1, kHighLimbKind) == 1);
  CHECK(actors::FindLiveActorBoneTag(2, 9, kAuxiliaryKind) == 2);
  CHECK(actors::FindLiveActorBoneTag(2, 6, kTagKind) ==
        actors::kSkeletonTagNotFound);
  // Another kind matches no bone, but the root is still found.
  CHECK(actors::FindLiveActorBoneTag(2, 5, 5) == actors::kSkeletonTagNotFound);
  CHECK(actors::FindLiveActorBoneTag(2, actors::kRootSelector, 5) == 0);
  CHECK(actors::FindLiveActorBoneTag(4, 5, kTagKind) == 1);
  CHECK(actors::FindLiveActorBoneTag(6, 5, kTagKind) ==
        actors::kSkeletonNoOwner);
  CHECK(warnings.messages.empty());
}

TEST_CASE("Live bone labels read the actor's or the gimmick's bone",
          "[actors-animation-skeleton][actors][redirect-binding]") {
  LiveSkeletonState state;
  TestModel& model = state.AddModel();
  state.AddActor(2, model);
  state.AddGimmick(4, model);
  VS::test_support::ChannelWarnings warnings{"actors"};

  CHECK(actors::LiveActorBoneTag(2, 1, kTagKind) == 5);
  CHECK(actors::LiveActorBoneTag(2, 1, kHighLimbKind) == 2);
  CHECK(actors::LiveActorBoneTag(2, 0, kHighLimbKind) ==
        actors::kSkeletonNoLimbGroup);
  CHECK(actors::LiveActorBoneTag(4, 2, kAuxiliaryKind) == 9);
  CHECK(warnings.messages.empty());
}

TEST_CASE("Live bone tags refuse bad kinds and bones past the loaded table",
          "[actors-animation-skeleton][actors][redirect-binding]") {
  LiveSkeletonState state;
  TestModel& model = state.AddModel();
  Actors_ActorRecord& actor = state.AddActor(2, model);
  actor.instance.boneCount = static_cast<u_char>(kTestBones + 2);
  VS::test_support::ChannelWarnings warnings{"actors"};

  CHECK(actors::FindLiveActorBoneTag(2, 6, kTagKind) ==
        actors::kSkeletonTagNotFound);
  CHECK(actors::LiveActorBoneTag(2, 1, 4) == actors::kSkeletonNoOwner);
  CHECK(actors::LiveActorBoneTag(2, 4, kTagKind) == actors::kSkeletonNoOwner);
  CHECK(actors::FindLiveActorBoneTag(17, 5, kTagKind) ==
        actors::kSkeletonNoOwner);
  CHECK(
      warnings.messages ==
      std::vector<std::string>{
          Refused("FindActorBoneTag",
                  "bone 4 of 6 lies past the loaded bone table"),
          Refused("ActorBoneTag", "kind 4 names no bone label"),
          Refused("ActorBoneTag", "bone 4 lies past the loaded bone table"),
          Refused("FindActorBoneTag", "actor id 17 is outside the actor table"),
      });
}

TEST_CASE("Live head tracking aims the actor with a head bone",
          "[actors-animation-skeleton][actors][redirect-binding]") {
  LiveSkeletonState state;
  TestModel& model = state.AddModel();
  Actors_ActorRecord& actor = state.AddActor(2, model);
  actor.collisionRadius = 200;
  actor.instance.lookPitch = 0x123;
  actor.instance.lookYaw = 0x456;
  actor.headTracking = {11, 22, 33, 4};
  VS::test_support::ChannelWarnings warnings{"actors"};

  CHECK(actors::SetLiveActorHeadTracking(
            2, 3, nullptr, actors::kHeadTrackingUsesRadiusStep) == 0);
  CHECK(actor.headTrackTarget == 3);
  CHECK(PointOf(actor.headTracking) == Vec3{0x123, 0x456, 33});
  CHECK(actor.headTracking.pad == 20);

  const SVECTOR aim{7, 8, 9, 10};
  CHECK(actors::SetLiveActorHeadTracking(2, 1, &aim, 12) == 0);
  CHECK(PointOf(actor.headTracking) == Vec3{7, 8, 9});
  CHECK(actor.headTracking.pad == 12);
  CHECK(warnings.messages.empty());
}

TEST_CASE("Live head tracking resets its step and needs a head bone",
          "[actors-animation-skeleton][actors][redirect-binding]") {
  LiveSkeletonState state;
  TestModel& model = state.AddModel();
  Actors_ActorRecord& actor = state.AddActor(2, model);
  actor.headTracking = {11, 22, 33, 4};
  VS::test_support::ChannelWarnings warnings{"actors"};

  actor.collisionRadius = 100;
  actors::ResetLiveActorHeadTrackingSpeed(&actor);
  CHECK(actor.headTracking.pad == 30);

  const SVECTOR aim{7, 8, 9, 10};
  model.bones[1].auxiliaryTag = 0;
  CHECK(actors::SetLiveActorHeadTracking(2, 3, &aim, 12) ==
        actors::kSkeletonNoHeadBone);
  CHECK(PointOf(actor.headTracking) == Vec3{11, 22, 33});
  CHECK(actors::SetLiveActorHeadTracking(5, 3, &aim, 12) ==
        actors::kSkeletonNoOwner);
  CHECK(warnings.messages.empty());
}

TEST_CASE("Live bone matrices copy the stored matrix and bone length",
          "[actors-animation-skeleton][actors][redirect-binding]") {
  LiveSkeletonState state;
  TestModel& model = state.AddModel();
  Actors_ActorRecord& actor = state.AddActor(2, model);
  actor.boneMatrices[2] = TranslationMatrix({1, 2, 3});
  model.bones[2].length = 77;
  VS::test_support::ChannelWarnings warnings{"actors"};

  MATRIX matrix{};
  u_long length = 0;
  CHECK(actors::LiveActorTaggedBoneMatrix(2, 7, &matrix, &length) == 0);
  CHECK(TranslationOf(matrix) == Vec3{1, 2, 3});
  CHECK(length == 77);
  CHECK(actors::LiveActorTaggedBoneMatrix(2, 6, &matrix, &length) ==
        actors::kSkeletonTagNotFound);
  CHECK(warnings.messages.empty());

  constexpr auto kMatrices =
      std::extent_v<decltype(Actors_ActorRecord::boneMatrices)>;
  length = 5;
  actors::LiveActorBoneMatrix(2, kMatrices, &matrix, &length);
  CHECK(length == 5);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused("ActorBoneMatrix",
                    "bone " + std::to_string(kMatrices) + " lies past the " +
                        std::to_string(kMatrices) + " bone matrices")});
}

TEST_CASE("Live bone positions view the actor's bones from the bone camera",
          "[actors-animation-skeleton][actors][redirect-binding]") {
  LiveSkeletonState state;
  AddPosedActor(state);
  VS::test_support::ChannelWarnings warnings{"actors"};

  SVECTOR position{0, 0, 0, 99};
  CHECK(actors::LiveActorBonePosition(2, 2, &position, 0) == 0);
  CHECK(PointOf(position) == Vec3{100, 200, 300});
  CHECK(position.pad == 99);
  CHECK(actors::LiveActorBonePosition(2, 2, &position, 1) == 0);
  CHECK(PointOf(position) == Vec3{36, 200, 300});
  CHECK(actors::LiveActorTaggedBonePosition(2, 7, &position, 2) == 0);
  CHECK(PointOf(position) == Vec3{68, 200, 300});
  CHECK(warnings.messages.empty());
}

TEST_CASE("Live limb group positions view the group's bone",
          "[actors-animation-skeleton][actors][redirect-binding]") {
  LiveSkeletonState state;
  AddPosedActor(state);
  VS::test_support::ChannelWarnings warnings{"actors"};

  SVECTOR position{};
  CHECK(actors::LiveActorHighGroupBonePosition(2, 0, &position, 0) == 0);
  CHECK(PointOf(position) == Vec3{100, 200, 300});
  CHECK(actors::LiveActorLowGroupBonePosition(2, 0, &position, 0) == 0);
  CHECK(PointOf(position) == Vec3{0, 0, 100});
  CHECK(warnings.messages.empty());
}

TEST_CASE("Live bone world matrices view the bone and give its length",
          "[actors-animation-skeleton][actors][redirect-binding]") {
  LiveSkeletonState state;
  AddPosedActor(state);
  VS::test_support::ChannelWarnings warnings{"actors"};

  MATRIX matrix{};
  int length = 0;
  CHECK(actors::LiveActorBoneWorldMatrix(2, 2, &matrix, &length) == 0);
  CHECK(TranslationOf(matrix) == Vec3{100, 200, 300});
  CHECK(matrix.m[1][1] == kOne);
  CHECK(length == 64);
  CHECK(warnings.messages.empty());
}

TEST_CASE("Live bone positions read the actor root at its position",
          "[actors-animation-skeleton][actors][redirect-binding]") {
  LiveSkeletonState state;
  TestModel& model = state.AddModel();
  Actors_ActorRecord& actor = state.AddActor(1, model);
  actor.blendPose.boneKeys[std::size(actor.blendPose.boneKeys) - 1] = {100, 200,
                                                                       300, 0};
  actor.instance.position = {1000, 2000, 3000, 0};
  VS::test_support::ChannelWarnings warnings{"actors"};

  MATRIX matrix{};
  int length = -1;
  CHECK(actors::LiveActorBoneWorldMatrix(1, actors::kRootSelector, &matrix,
                                         &length) == 0);
  CHECK(TranslationOf(matrix) == Vec3{1050, 2100, 3150});
  CHECK(length == 0);
  SVECTOR position{};
  CHECK(actors::LiveActorBoneEndpointPosition(1, actors::kRootSelector,
                                              &position, 1) == 0);
  CHECK(PointOf(position) == Vec3{1050, 2100, 3150});
  CHECK(warnings.messages.empty());
}

TEST_CASE("Live bone positions read the actor's first weapon",
          "[actors-animation-skeleton][actors][redirect-binding]") {
  LiveSkeletonState state;
  TestModel& model = state.AddModel();
  TestModel& weapon_model = state.AddModel();
  state.AddActor(1, model);
  Actors_WeaponModel& weapon = state.AddFirstWeapon(1, weapon_model);
  weapon.boneWorldMatrices[1] = TranslationMatrix({10, 20, 80});
  weapon_model.bones[1].length = 40;
  VS::test_support::ChannelWarnings warnings{"actors"};

  SVECTOR position{};
  CHECK(actors::LiveActorBoneEndpointPosition(1, 0x40, &position, 1) == 0);
  CHECK(PointOf(position) == Vec3{-40, 0, 50});
  CHECK(actors::LiveActorTaggedBonePosition(1, 0x40, &position, 0) == 0);
  CHECK(PointOf(position) == Vec3{0, 0, 50});
  CHECK(warnings.messages.empty());
}

TEST_CASE("Live bone positions read a gimmick's bone matrices",
          "[actors-animation-skeleton][actors][redirect-binding]") {
  LiveSkeletonState state;
  TestModel& model = state.AddModel();
  Field_GimmickInstance& gimmick = state.AddGimmick(4, model);
  gimmick.matrices[1] = TranslationMatrix({10, 20, 230});
  VS::test_support::ChannelWarnings warnings{"actors"};

  SVECTOR position{};
  CHECK(actors::LiveActorBonePosition(4, 1, &position, 0) == 0);
  CHECK(PointOf(position) == Vec3{0, 0, 200});
  CHECK(actors::LiveActorBoneViewTransform(4, 1).has_value());
  CHECK_FALSE(actors::LiveActorBoneViewTransform(6, 1).has_value());
  CHECK(warnings.messages.empty());
}

TEST_CASE("Live weapon bone queries refuse a missing weapon or bone",
          "[actors-animation-skeleton][actors][redirect-binding]") {
  LiveSkeletonState state;
  TestModel& model = state.AddModel();
  state.AddActor(1, model);
  state.AddActor(12, model);
  state.AddFirstWeapon(1, model);
  VS::test_support::ChannelWarnings warnings{"actors"};

  SVECTOR position{1, 2, 3, 4};
  // An empty weapon slot is the original's missing weapon, without a report.
  state.AddActor(3, model);
  CHECK(actors::LiveActorBonePosition(3, 0x40, &position, 0) ==
        actors::kSkeletonNoOwner);
  CHECK(warnings.messages.empty());

  CHECK(actors::LiveActorBonePosition(12, 0x40, &position, 0) ==
        actors::kSkeletonNoOwner);
  CHECK(actors::LiveActorBonePosition(1, 0x43, &position, 0) ==
        actors::kSkeletonNoOwner);
  CHECK(PointOf(position) == Vec3{1, 2, 3});
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused("ActorBoneEndpointPosition", "actor 12 has no weapon slot"),
            Refused("ActorBoneEndpointPosition",
                    "weapon bone selector 0x43 lies past the 4 weapon bone "
                    "matrices"),
        });
}

TEST_CASE("Live target, ground and anchor points read the actor's position",
          "[actors-animation-skeleton][actors][redirect-binding]") {
  LiveSkeletonState state;
  TestModel& model = state.AddModel();
  Actors_ActorRecord& actor = state.AddActor(2, model);
  actor.instance.position = {100, -50, 300, 9};
  actor.targetPointHeight = 40;
  actor.anchorHeight = 0x100;
  actor.scale = 2 * kOne;
  VS::test_support::ChannelWarnings warnings{"actors"};

  SVECTOR point{};
  CHECK(actors::LiveActorTaggedBonePosition(2, actors::kTargetPointSelector,
                                            &point, 0) == 0);
  CHECK(PointOf(point) == Vec3{100, -90, 300});
  CHECK(point.pad == 9);
  CHECK(actors::LiveActorTaggedBonePosition(2, actors::kGroundAnchorSelector,
                                            &point, 0) == 0);
  CHECK(PointOf(point) == Vec3{100, -50, 300});
  CHECK(actors::LiveActorTaggedBonePosition(2, actors::kAnchorHeightSelector,
                                            &point, 0) == 0);
  CHECK(PointOf(point) == Vec3{100, 462, 300});
  CHECK(warnings.messages.empty());
}

TEST_CASE("Live gimmick target points and the ground anchor matrix",
          "[actors-animation-skeleton][actors][redirect-binding]") {
  LiveSkeletonState state;
  TestModel& model = state.AddModel();
  Actors_ActorRecord& actor = state.AddActor(2, model);
  actor.instance.position = {100, -50, 300, 9};
  Field_GimmickInstance& gimmick = state.AddGimmick(4, model);
  gimmick.positionX = 5;
  gimmick.positionY = 6;
  gimmick.positionZ = 7;
  gimmick.reserved22 = 0x55;
  VS::test_support::ChannelWarnings warnings{"actors"};

  SVECTOR point{};
  CHECK(actors::LiveActorTaggedBonePosition(4, actors::kTargetPointSelector,
                                            &point, 0) == 0);
  CHECK(PointOf(point) == Vec3{5, 6 - actors::kGimmickTargetHeight, 7});
  CHECK(point.pad == 0x55);

  actor.boneMatrices[0] = TranslationMatrix({110, 220, 330});
  model.bones[0].length = 12;
  MATRIX matrix{};
  int length = 0;
  CHECK(actors::LiveActorAnchorMatrix(2, actors::kGroundAnchorSelector, &matrix,
                                      &length) == 0);
  CHECK(TranslationOf(matrix) == Vec3{100, -50, 300});
  CHECK(length == 12);
  CHECK(warnings.messages.empty());
}

TEST_CASE("Live ground and anchor points refuse a gimmick and null outputs",
          "[actors-animation-skeleton][actors][redirect-binding]") {
  LiveSkeletonState state;
  TestModel& model = state.AddModel();
  state.AddActor(2, model);
  state.AddGimmick(4, model);
  VS::test_support::ChannelWarnings warnings{"actors"};

  SVECTOR point{};
  MATRIX matrix{};
  int length = 0;
  CHECK(actors::LiveActorTaggedBonePosition(4, actors::kGroundAnchorSelector,
                                            &point,
                                            0) == actors::kSkeletonNoOwner);
  CHECK(actors::LiveActorAnchorMatrix(4, actors::kGroundAnchorSelector, &matrix,
                                      &length) == actors::kSkeletonNoOwner);
  CHECK(actors::LiveActorTaggedBonePosition(2, 7, nullptr, 0) ==
        actors::kSkeletonNoOwner);
  CHECK(actors::LiveActorBoneWorldMatrix(2, 0, nullptr, &length) ==
        actors::kSkeletonNoOwner);
  CHECK(actors::ProjectLiveActorTaggedBone(2, 7, nullptr, 0) ==
        actors::kSkeletonNoOwner);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused("ActorTaggedBonePosition",
                    "the ground and anchor points need an actor"),
            Refused("ActorAnchorMatrix", "the ground anchor needs an actor"),
            Refused("ActorTaggedBonePosition", "no position output"),
            Refused("ActorBoneWorldMatrix", "no matrix or length output"),
            Refused("ProjectActorTaggedBone", "no screen output"),
        });
}

TEST_CASE("Live pitch measures from the actor's root bone",
          "[actors-animation-skeleton][actors][redirect-binding]") {
  LiveSkeletonState state;
  TestModel& model = state.AddModel();
  Actors_ActorRecord& actor = state.AddActor(2, model);
  actor.boneMatrices[0] = TranslationMatrix({10, 120, 30});
  actor.instance.reactionState = 1;
  VS::test_support::ChannelWarnings warnings{"actors"};

  const SVECTOR target{300, 600, 400, 0};
  CHECK(actors::LiveActorPitchToPoint(&actor, &target) ==
        actors::PitchToPoint({}, {300, 600, 400}, 100));
  CHECK(actors::LiveActorPitchToPoint(&actor, nullptr) == 0);
  // An actor taking a reaction has no target point to project.
  SVECTOR screen{1, 2, 3, 4};
  CHECK(actors::ProjectLiveActorTaggedBone(2, actors::kTargetPointSelector,
                                           &screen,
                                           0) == actors::kSkeletonNoOwner);
  CHECK(PointOf(screen) == Vec3{1, 2, 3});
  CHECK(warnings.messages.empty());

  CHECK(actors::LiveActorPitchToPoint(nullptr, &target) == 0);
  CHECK(warnings.messages == std::vector<std::string>{Refused(
                                 "ActorPitchToPoint", "no actor record")});
}

TEST_CASE("Skeleton bindings retire the module entries they replace",
          "[actors-animation-skeleton][actors][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [&](unsigned address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  // The skeleton queries are typed C++ now; no BATTLE.PRG image or table
  // stores these addresses.
  constexpr std::array<unsigned, 18> kRetired{
      0x8009E228u, 0x8009FC60u, 0x8009FD38u, 0x800A1178u, 0x800A11D8u,
      0x800A1280u, 0x800A13ECu, 0x800A152Cu, 0x800A1648u, 0x800A1720u,
      0x800A17BCu, 0x800A190Cu, 0x800A1AF8u, 0x800A1B28u, 0x800A1B9Cu,
      0x800A1C10u, 0x800A1DE8u, 0x800A4828u};
  for (const auto address : kRetired) {
    INFO("address " << address);
    CHECK(resolve(address) == nullptr);
  }
  CHECK(resolve(0x800F3C58u) == static_cast<void*>(&Actors_skeletonPose));
  CHECK(resolve(0x800F3C78u) == static_cast<void*>(&Actors_skeletonRootMatrix));
  CHECK(resolve(0x800F4588u) == static_cast<void*>(&Actors_weaponModels));
  CHECK(resolve(0x800F49B8u) == static_cast<void*>(&Actors_boneMatrixScratch));
}
