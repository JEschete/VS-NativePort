#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ModelDrawing.hpp"
#include "Actors/ModelDrawingLive.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Core/Gte.h"
}

namespace {
using VS::Actors::ActorModelDrawHost;
using VS::Actors::BoneEndpoint;
using VS::Actors::BoneLookup;
using VS::Actors::BoneProjectionHost;
using VS::Actors::BoneSelection;
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
  bool GeometryHasUvLayout() const override { return uv_layout; }
  void DrawPolygons(ModelPolygons polygons) override {
    calls.emplace_back("polygons" + std::to_string(static_cast<int>(polygons)));
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

TEST_CASE("Model draw orders the actor's own textured draw before its shadow",
          "[actors][model-drawing]") {
  RecordingDrawHost host;
  CHECK(VS::Actors::DrawActorModel(host, 0) == ModelDrawResult::kDrawn);
  CHECK(host.selected_slot == 0);
  CHECK(host.calls == Calls{"pose", "select", "bind", "project", "visible",
                            "colors0", "polygons0", "shadow"});
}

TEST_CASE("Model draw overlays the companion color without a shadow",
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
                            "colors2", "polygons0"});
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
