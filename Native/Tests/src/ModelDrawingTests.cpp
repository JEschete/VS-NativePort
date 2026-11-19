#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ActorRecord.h"
#include "Actors/ModelDrawing.hpp"
#include "Actors/ModelDrawingLive.hpp"
#include "Actors/ModelRenderingLiveState.hpp"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Actors/SkeletonLiveState.hpp"
#include "Core/Gte.h"
#include "Legacy/Psx.h"
#include "Render/ModelDraw.hpp"
#include "Render/ModelWorkspace.hpp"

namespace {
using VS::Actors::ActorModelDrawHost;
using VS::Actors::BoneEndpoint;
using VS::Actors::BoneLookup;
using VS::Actors::BoneProjectionHost;
using VS::Actors::BoneSelection;
using VS::Actors::ModelAppearance;
using VS::Actors::ModelDrawResult;
using VS::Actors::ModelDrawRole;
using VS::Actors::ModelPolygons;
using VS::Actors::ProjectedBonePoint;

class RecordingDrawHost final : public ActorModelDrawHost {
 public:
  bool pose = true;
  bool visible = true;
  int height = 0;
  bool leader = false;
  bool event_playing = false;
  bool geometry = true;
  bool bindable = true;
  bool projects = true;
  bool uv_layout = true;
  bool forced = false;
  ModelAppearance geometry_appearance;
  ModelAppearance actor_appearance;
  bool shadow = true;
  int selected_slot = -1;
  std::vector<std::string> calls;

  bool BuildPose() override {
    calls.emplace_back("pose");
    return pose;
  }
  bool IsVisible() const override { return visible; }
  int Height() const override { return height; }
  bool IsLeader() const override { return leader; }
  bool IsEventPlaying() const override { return event_playing; }
  bool SelectGeometry(int slot) override {
    selected_slot = slot;
    calls.emplace_back("select");
    return geometry;
  }
  bool BindGeometry() override {
    calls.emplace_back("bind");
    return bindable;
  }
  bool ProjectVertices() override {
    calls.emplace_back("project");
    return projects;
  }
  void SetCulled(bool culled) override {
    calls.emplace_back(culled ? "culled" : "visible");
  }
  void UpdateColors(ModelDrawRole role) override {
    calls.emplace_back("colors" + std::to_string(static_cast<int>(role)));
  }
  bool ActorHasUvLayout() const override { return uv_layout; }
  bool ModelOverridesForced() const override { return forced; }
  ModelAppearance GeometryAppearance() const override {
    return geometry_appearance;
  }
  ModelAppearance ActorAppearance() const override { return actor_appearance; }
  // Records "polygons<pass>", with ":<pulse>,<state>" for an override draw.
  void DrawPolygons(ModelPolygons polygons,
                    const std::optional<ModelAppearance>& overrides) override {
    std::string call = "polygons" + std::to_string(static_cast<int>(polygons));
    if (overrides) {
      call += ":" + std::to_string(overrides->pulse_kind) + "," +
              std::to_string(overrides->state);
    }
    calls.push_back(std::move(call));
  }
  bool CastsShadow() const override { return shadow; }
  void DrawShadow() override { calls.emplace_back("shadow"); }
};

using Calls = std::vector<std::string>;

class FixedBoneHost final : public BoneProjectionHost {
 public:
  BoneSelection selection;
  ProjectedBonePoint point;
  std::int16_t projected_offset = 0x7777;

  BoneSelection SelectBone(int /*slot*/, int /*bone*/) override {
    return selection;
  }
  ProjectedBonePoint Project(std::int16_t offset_x) override {
    projected_offset = offset_x;
    return point;
  }
};
}  // namespace

TEST_CASE("Model draw classifies the companion argument as a byte",
          "[actors][model-drawing]") {
  CHECK(VS::Actors::ClassifyModelDraw(0) == ModelDrawRole::kSelf);
  CHECK(VS::Actors::ClassifyModelDraw(1) == ModelDrawRole::kCompanionOverlay);
  CHECK(VS::Actors::ClassifyModelDraw(2) == ModelDrawRole::kCompanionGeometry);
  CHECK(VS::Actors::ClassifyModelDraw(0x100) == ModelDrawRole::kSelf);
  CHECK(VS::Actors::ClassifyModelDraw(0x101) ==
        ModelDrawRole::kCompanionOverlay);
}

TEST_CASE("Model draw stops before geometry when the pose or height rejects it",
          "[actors][model-drawing]") {
  RecordingDrawHost host;
  host.pose = false;
  CHECK(VS::Actors::DrawActorModel(host, 0) == ModelDrawResult::kNoPose);
  CHECK(host.calls == Calls{"pose"});

  host = RecordingDrawHost{};
  host.visible = false;
  CHECK(VS::Actors::DrawActorModel(host, 0) == ModelDrawResult::kNoPose);

  host = RecordingDrawHost{};
  host.height = VS::Actors::kHiddenActorHeight;
  CHECK(VS::Actors::DrawActorModel(host, 0) == ModelDrawResult::kHidden);
  CHECK(host.calls == Calls{"pose"});

  host = RecordingDrawHost{};
  host.height = VS::Actors::kHiddenActorHeight;
  host.leader = true;
  host.event_playing = true;
  CHECK(VS::Actors::DrawActorModel(host, 0) == ModelDrawResult::kHidden);

  // A non-leader below the floor is still drawn while an event is playing.
  host = RecordingDrawHost{};
  host.height = VS::Actors::kHiddenActorHeight;
  host.event_playing = true;
  CHECK(VS::Actors::DrawActorModel(host, 0) == ModelDrawResult::kDrawn);

  host = RecordingDrawHost{};
  host.height = VS::Actors::kHiddenActorHeight - 1;
  CHECK(VS::Actors::DrawActorModel(host, 0) == ModelDrawResult::kDrawn);
}

TEST_CASE("Model draw returns without a shadow for missing or invalid geometry",
          "[actors][model-drawing]") {
  RecordingDrawHost host;
  host.geometry = false;
  CHECK(VS::Actors::DrawActorModel(host, 0) ==
        ModelDrawResult::kMissingGeometry);
  CHECK(host.calls == Calls{"pose", "select"});

  host = RecordingDrawHost{};
  host.bindable = false;
  CHECK(VS::Actors::DrawActorModel(host, 0) ==
        ModelDrawResult::kInvalidGeometry);
  CHECK(host.calls == Calls{"pose", "select", "bind"});
}

TEST_CASE("Model draw keeps the shadow of an actor whose projection failed",
          "[actors][model-drawing]") {
  RecordingDrawHost host;
  host.projects = false;
  CHECK(VS::Actors::DrawActorModel(host, 0) == ModelDrawResult::kCulled);
  CHECK(host.calls ==
        Calls{"pose", "select", "bind", "project", "culled", "shadow"});

  host = RecordingDrawHost{};
  host.projects = false;
  host.shadow = false;
  CHECK(VS::Actors::DrawActorModel(host, 0) == ModelDrawResult::kCulled);
  CHECK(host.calls == Calls{"pose", "select", "bind", "project", "culled"});
}

TEST_CASE("Model draw orders the actor's own draw before its shadow",
          "[actors][model-drawing]") {
  RecordingDrawHost host;
  CHECK(VS::Actors::DrawActorModel(host, 0) == ModelDrawResult::kDrawn);
  CHECK(host.selected_slot == 0);
  CHECK(host.calls == Calls{"pose", "select", "bind", "project", "visible",
                            "colors0", "polygons1", "shadow"});

  host = RecordingDrawHost{};
  host.uv_layout = false;
  CHECK(VS::Actors::DrawActorModel(host, 0) == ModelDrawResult::kDrawn);
  CHECK(host.calls == Calls{"pose", "select", "bind", "project", "visible",
                            "colors0", "polygons0", "shadow"});
}

// 49EB8.c:304-307: a drawing actor with its own UV layout takes the Gouraud
// pass even as the companion overlay; only the flat overlay uses the
// companion color.
TEST_CASE("Model draw overlays the companion without a shadow",
          "[actors][model-drawing]") {
  RecordingDrawHost host;
  CHECK(VS::Actors::DrawActorModel(host, 1) == ModelDrawResult::kDrawn);
  CHECK(host.selected_slot == 0);
  CHECK(host.calls == Calls{"pose", "select", "bind", "project", "visible",
                            "colors1", "polygons1"});

  host = RecordingDrawHost{};
  host.uv_layout = false;
  CHECK(VS::Actors::DrawActorModel(host, 1) == ModelDrawResult::kDrawn);
  CHECK(host.calls.back() == "polygons2");
}

TEST_CASE("Model draw selects the companion slot's geometry without a shadow",
          "[actors][model-drawing]") {
  RecordingDrawHost host;
  CHECK(VS::Actors::DrawActorModel(host, 0x105) == ModelDrawResult::kDrawn);
  CHECK(host.selected_slot == 5);
  CHECK(host.calls == Calls{"pose", "select", "bind", "project", "visible",
                            "colors2", "polygons1"});

  host = RecordingDrawHost{};
  host.uv_layout = false;
  CHECK(VS::Actors::DrawActorModel(host, 5) == ModelDrawResult::kDrawn);
  CHECK(host.calls.back() == "polygons0");
}

// 49EB8.c:292-304 (0x800B3650..0x800B36C4).
TEST_CASE("Model overrides follow the forced flag, pulse and fade states",
          "[actors][model-drawing]") {
  // The appearance state is a 4-bit field.
  for (int state = 0; state < 16; ++state) {
    const auto appearance = ModelAppearance{
        .pulse_kind = 0, .state = static_cast<std::uint8_t>(state)};
    const bool fading = (state >= 4 && state <= 8 && state != 6) || state == 15;
    INFO("state " << state);
    CHECK(VS::Actors::UsesModelOverrides(false, appearance) == fading);
    CHECK(VS::Actors::UsesModelOverrides(true, appearance));
    CHECK(VS::Actors::UsesModelOverrides(
        false, {.pulse_kind = 1, .state = appearance.state}));
  }
}

TEST_CASE("Model overrides follow the geometry and draw with the actor",
          "[actors][model-drawing]") {
  RecordingDrawHost host;
  host.geometry_appearance = {.pulse_kind = 0, .state = 4};
  host.actor_appearance = {.pulse_kind = 3, .state = 9};
  CHECK(VS::Actors::DrawActorModel(host, 5) == ModelDrawResult::kDrawn);
  CHECK(host.calls.back() == "polygons1:3,9");

  // The actor's own fade does not select the override drawers.
  host = RecordingDrawHost{};
  host.uv_layout = false;
  host.actor_appearance = {.pulse_kind = 2, .state = 4};
  CHECK(VS::Actors::DrawActorModel(host, 5) == ModelDrawResult::kDrawn);
  CHECK(host.calls.back() == "polygons0");
}

TEST_CASE("The first-person flag forces override drawers except the overlay",
          "[actors][model-drawing]") {
  RecordingDrawHost host;
  host.forced = true;
  host.uv_layout = false;
  CHECK(VS::Actors::DrawActorModel(host, 0) == ModelDrawResult::kDrawn);
  CHECK(host.calls == Calls{"pose", "select", "bind", "project", "visible",
                            "colors0", "polygons0:0,0", "shadow"});

  host = RecordingDrawHost{};
  host.forced = true;
  CHECK(VS::Actors::DrawActorModel(host, 1) == ModelDrawResult::kDrawn);
  CHECK(host.calls.back() == "polygons1:0,0");

  host = RecordingDrawHost{};
  host.forced = true;
  host.uv_layout = false;
  CHECK(VS::Actors::DrawActorModel(host, 1) == ModelDrawResult::kDrawn);
  CHECK(host.calls.back() == "polygons2");
}

TEST_CASE("Model color helpers apply signed adjustments and shadow tint",
          "[actors][model-drawing]") {
  CHECK(VS::Actors::AdjustModelColorChannel(128, 0) == 128);
  CHECK(VS::Actors::AdjustModelColorChannel(128, 10) == 168);
  CHECK(VS::Actors::AdjustModelColorChannel(128, 0xF6) == 88);
  CHECK(VS::Actors::AdjustModelColorChannel(250, 10) == 255);
  CHECK(VS::Actors::AdjustModelColorChannel(10, 0x80) == 0);
  CHECK(VS::Actors::GroundShadowChannel(0) == 64);
  CHECK(VS::Actors::GroundShadowChannel(255) == 191);
}

TEST_CASE("Bone endpoint arguments classify base, tip and midpoint",
          "[actors][model-drawing]") {
  CHECK(VS::Actors::ClassifyBoneEndpoint(0) == BoneEndpoint::kBase);
  CHECK(VS::Actors::ClassifyBoneEndpoint(1) == BoneEndpoint::kTip);
  CHECK(VS::Actors::ClassifyBoneEndpoint(2) == BoneEndpoint::kMiddle);
  CHECK(VS::Actors::ClassifyBoneEndpoint(-1) == BoneEndpoint::kMiddle);
}

TEST_CASE("Bone endpoint offsets negate the length and halve the midpoint",
          "[actors][model-drawing]") {
  CHECK(VS::Actors::BoneEndpointOffset(129, BoneEndpoint::kBase) == 0);
  CHECK(VS::Actors::BoneEndpointOffset(129, BoneEndpoint::kTip) == -129);
  CHECK(VS::Actors::BoneEndpointOffset(129, BoneEndpoint::kMiddle) == -64);
  CHECK(VS::Actors::BoneEndpointOffset(-129, BoneEndpoint::kMiddle) == 64);
  // The tip negates only the low halfword of the length.
  CHECK(VS::Actors::BoneEndpointOffset(0x10080, BoneEndpoint::kTip) == -0x80);
}

TEST_CASE("Projected bone depth clamps to the 16-bit range",
          "[actors][model-drawing]") {
  CHECK(VS::Actors::ClampProjectedDepth(-5) == 0);
  CHECK(VS::Actors::ClampProjectedDepth(70000) == 65535);
  CHECK(VS::Actors::ClampProjectedDepth(1024) == 1024);
}

TEST_CASE("Bone projection reports lookup failures without writing output",
          "[actors][model-drawing]") {
  FixedBoneHost host;
  std::array<std::int32_t, 2> output = {11, 22};
  host.selection = {BoneLookup::kMissing, 0};
  CHECK(VS::Actors::ProjectActorBoneEndpoint(host, 0, 0, 0, output) ==
        VS::Actors::kBoneProjectionMissing);
  host.selection = {BoneLookup::kInvalidBone, 0};
  CHECK(VS::Actors::ProjectActorBoneEndpoint(host, 0, 0, 0, output) ==
        VS::Actors::kBoneProjectionInvalidBone);
  CHECK(output == std::array<std::int32_t, 2>{11, 22});
  CHECK(host.projected_offset == 0x7777);

  host.selection = {BoneLookup::kFound, 200};
  host.point = {0x00780050, 90000};
  CHECK(VS::Actors::ProjectActorBoneEndpoint(host, 0, 0, 2, output) == 0);
  CHECK(host.projected_offset == -100);
  CHECK(output == std::array<std::int32_t, 2>{0x00780050, 65535});
}

TEST_CASE("Live bone projection bounds the bone by the pose and bone table",
          "[actors][model-drawing]") {
  const auto saved = std::to_array(Actors_actorRecords);
  Actors_ActorRecord actor{};
  std::array<unsigned, 256> model_bytes{};
  auto* model = reinterpret_cast<Actors_ShpModel*>(model_bytes.data());
  Actors_actorRecords[0] = &actor;
  actor.instance.model = model;
  Core_Gte_Reset();
  int output[2] = {33, 44};

  // #263: a bone count and bone table beyond the 42 pose matrices still
  // reject bones outside the pose.
  actor.instance.boneCount = 60;
  actor.modelDataSize = sizeof model_bytes;
  CHECK(VS::Actors::ProjectLiveActorBoneEndpoint(0, 41, output, 0) == 0);
  output[0] = 33;
  output[1] = 44;
  CHECK(VS::Actors::ProjectLiveActorBoneEndpoint(0, 42, output, 0) ==
        VS::Actors::kBoneProjectionInvalidBone);
  // #263: a bone past the loaded bone table is rejected; two bones fit here.
  actor.modelDataSize = sizeof(Actors_ShpModel) + (2 * sizeof(Actors_ShpBone));
  CHECK(VS::Actors::ProjectLiveActorBoneEndpoint(0, 2, output, 0) ==
        VS::Actors::kBoneProjectionInvalidBone);
  CHECK(VS::Actors::ProjectLiveActorBoneEndpoint(0, -1, output, 0) ==
        VS::Actors::kBoneProjectionInvalidBone);
  CHECK(output[0] == 33);
  CHECK(output[1] == 44);
  CHECK(VS::Actors::ProjectLiveActorBoneEndpoint(0, 1, output, 0) == 0);
  CHECK(VS::Actors::ProjectLiveActorBoneEndpoint(-1, 0, output, 0) ==
        VS::Actors::kBoneProjectionMissing);

  std::copy(saved.begin(), saved.end(), std::begin(Actors_actorRecords));
  Core_Gte_Reset();
}

namespace {
// A drawing actor and the geometry it borrows from slot 5, both over a
// one-vertex model in front of the camera.
struct BorrowedGeometryFixture {
  static constexpr int kGeometrySlot = 5;
  decltype(std::to_array(Actors_actorRecords)) saved_records =
      std::to_array(Actors_actorRecords);
  Actors_SkeletonPose saved_pose = VS::Actors::skeletonPose;
  Actors_ModelRenderContext saved_context = VS::Actors::modelRenderContext;
  VS::Render::ModelScratchpad saved_workspace = *VS::Render::ModelWorkspace();
  std::array<unsigned, 32> model_bytes{};
  Actors_ActorRecord actor{};
  Actors_ActorRecord geometry{};
  MATRIX camera{};

  BorrowedGeometryFixture() {
    auto* model = reinterpret_cast<Actors_ShpModel*>(model_bytes.data());
    model->groupSection = 96;
    model->vertexSection = 100;
    model->polygonSection = 108;
    const std::array<std::uint16_t, 2> group{0, 1};
    std::memcpy(model_bytes.data() + 24, group.data(), sizeof group);
    for (Actors_ActorRecord* record : {&actor, &geometry}) {
      record->instance.model = model;
      record->modelDataSize = sizeof model_bytes;
      record->instance.boneCount = 1;
      record->instance.vertexGroupCount = 1;
      record->instance.isVisible = -1;
      record->instance.scaleX = record->instance.scaleY =
          record->instance.scaleZ = record->scale = 4096;
      record->blendPose.scaleKeys[0] = {64, 64, 64, 0};
    }
    actor.customTint = 1;
    actor.instance.tint = {100, 70, 20, 0};
    geometry.fadeRed = 255;
    geometry.fadeGreen = 50;
    geometry.fadeBlue = 246;
    Actors_actorRecords[kGeometrySlot] = &geometry;
    for (int axis = 0; axis < 3; ++axis) camera.m[axis][axis] = 4096;
    setVector(&VS::Render::ModelWorkspace()->root_offset, 0, 0, 1024);
    Core_Gte_Reset();
    Core_Gte_SetGeomScreen(256);
  }
  ~BorrowedGeometryFixture() {
    std::copy(saved_records.begin(), saved_records.end(),
              std::begin(Actors_actorRecords));
    VS::Actors::skeletonPose = saved_pose;
    VS::Actors::modelRenderContext = saved_context;
    *VS::Render::ModelWorkspace() = saved_workspace;
    Core_Gte_Reset();
  }
  BorrowedGeometryFixture(const BorrowedGeometryFixture&) = delete;
  BorrowedGeometryFixture& operator=(const BorrowedGeometryFixture&) = delete;

  void Draw() { VS::Actors::DrawLiveActorModel(actor, camera, kGeometrySlot); }
  [[nodiscard]] static bool Unlit(const Actors_ActorRecord& record) {
    return record.instance.baseColor.r == 0 &&
           record.instance.baseColor.g == 0 &&
           record.instance.baseColor.b == 0 &&
           (record.shadowBlend.reserved0 & 0xFFFFFF) == 0;
  }
  // The flat pass's colour: the actor's tint {100, 70, 20} adjusted by the
  // geometry's fades {255, 50, 246}, and the tint itself as the shadow.
  [[nodiscard]] static bool LitInTint(const Actors_ActorRecord& record) {
    return record.instance.baseColor.r == 96 &&
           record.instance.baseColor.g == 255 &&
           record.instance.baseColor.b == 0 &&
           (record.shadowBlend.reserved0 & 0xFFFFFF) == 0x144664;
  }
  // The Gouraud pass's ground shadow is never darker than 64 per channel.
  [[nodiscard]] static bool GroundShaded(const Actors_ActorRecord& record) {
    const auto shadow =
        static_cast<std::uint32_t>(record.shadowBlend.reserved0);
    return (shadow & 0xFF) >= 64 && ((shadow >> 8) & 0xFF) >= 64 &&
           ((shadow >> 16) & 0xFF) >= 64;
  }
};
}  // namespace

// #444: 0x800B36C8 tests the drawing actor's UV layout, and the Gouraud
// lighting at 0x800B385C gates on its own state, whatever geometry it borrows.
TEST_CASE("Live model lighting follows the drawing actor's layout and state",
          "[actors][model-drawing]") {
  BorrowedGeometryFixture fixture;
  SECTION("a flat actor lights the borrowed UV-laid-out geometry") {
    fixture.geometry.instance.uvLayout = -1;
    fixture.Draw();
    CHECK(BorrowedGeometryFixture::LitInTint(fixture.geometry));
    CHECK(BorrowedGeometryFixture::Unlit(fixture.actor));
  }
  SECTION("a UV-laid-out actor shades itself while the geometry fades") {
    fixture.actor.instance.uvLayout = -1;
    fixture.geometry.instance.actionAnimationState = 5;
    fixture.Draw();
    CHECK(BorrowedGeometryFixture::GroundShaded(fixture.actor));
    CHECK(BorrowedGeometryFixture::Unlit(fixture.geometry));
  }
  SECTION("a fading UV-laid-out actor skips the lighting") {
    fixture.actor.instance.uvLayout = -1;
    fixture.actor.instance.actionAnimationState = 4;
    fixture.Draw();
    CHECK(BorrowedGeometryFixture::Unlit(fixture.actor));
    CHECK(BorrowedGeometryFixture::Unlit(fixture.geometry));
  }
}
