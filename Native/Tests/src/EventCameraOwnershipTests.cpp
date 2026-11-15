#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Event/EventCameraOwnership.hpp"
#include "Event/EventCameraOwnershipLive.hpp"
#include "Event/ScriptCommand.hpp"
#include "Field/CameraLive.hpp"
#include "Render/ScreenOverlayLive.hpp"

extern "C" {
#include "Event/Camera.h"
#include "Event/ScriptCamera.h"
#include "Event/ScriptControlOpcodes.h"
}

namespace {
namespace event = VS::Event;
using VS::Core::Math::Position3;
using VS::Core::Math::Vec3;

// F01215 (func_800BBDDC) is retired: LiveVmHost::FreeEventCamera makes this
// high-end free directly.
void FreeEventCamera() { VS::Core::Memory::FreeLiveFromHighEnd(Event_camera); }

class FakeOriginHost final : public event::EventCameraOriginHost {
 public:
  int resolved = 0;
  std::optional<Position3> position;
  std::vector<std::uint16_t> references;
  std::vector<int> actors;

  int ResolveActorReference(std::uint16_t reference) override {
    references.push_back(reference);
    return resolved;
  }

  std::optional<Position3> ActorPosition(int actor) override {
    actors.push_back(actor);
    return position;
  }
};

event::EventCameraOrigin ActorOrigin() {
  return {.actor = 7,
          .offset = {0x1000, -0x2000, 0x3000},
          .eye_position = {1, 2, 3},
          .look_at_position = {-4, -5, -6}};
}

Vec3 VectorOf(const VECTOR& vector) {
  return {static_cast<std::int32_t>(vector.vx),
          static_cast<std::int32_t>(vector.vy),
          static_cast<std::int32_t>(vector.vz)};
}

// Saves the event camera globals and the live framebuffer effect around a
// live heap.
struct LiveEventCameraFixture {
  static constexpr std::size_t kArenaBytes = std::size_t{16} * 1024;
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, kArenaBytes> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  Event_CameraState* saved_camera = Event_camera;
  char saved_override = Event_skipNextFieldCameraRestore;

  LiveEventCameraFixture() { VS::Render::InitializeLiveFramebufferEffect(); }
  LiveEventCameraFixture(const LiveEventCameraFixture&) = delete;
  LiveEventCameraFixture& operator=(const LiveEventCameraFixture&) = delete;
  LiveEventCameraFixture(LiveEventCameraFixture&&) = delete;
  LiveEventCameraFixture& operator=(LiveEventCameraFixture&&) = delete;
  ~LiveEventCameraFixture() {
    VS::Render::InitializeLiveFramebufferEffect();
    Event_camera = saved_camera;
    Event_skipNextFieldCameraRestore = saved_override;
  }
};

// A workspace for the origin entries, with the pads marked.
struct LiveOriginWorkspace {
  Event_CameraState camera{};
  Event_CameraState* saved_camera = Event_camera;

  LiveOriginWorkspace() {
    camera.originActorId = 7;
    camera.originOffset = {0x1000, -0x2000, 0x3000, 0x55};
    camera.eye.currentPosition = {1, 2, 3, 0x66};
    camera.lookAt.currentPosition = {-4, -5, -6, 0x77};
    Event_camera = &camera;
  }
  LiveOriginWorkspace(const LiveOriginWorkspace&) = delete;
  LiveOriginWorkspace& operator=(const LiveOriginWorkspace&) = delete;
  LiveOriginWorkspace(LiveOriginWorkspace&&) = delete;
  LiveOriginWorkspace& operator=(LiveOriginWorkspace&&) = delete;
  ~LiveOriginWorkspace() { Event_camera = saved_camera; }

  void RequireWorldWithOffsetApplied() const {
    CHECK(camera.originActorId == event::kEventCameraWorldOrigin);
    CHECK(VectorOf(camera.eye.currentPosition) ==
          Vec3{0x1001, -0x1FFE, 0x3003});
    CHECK(VectorOf(camera.lookAt.currentPosition) ==
          Vec3{0xFFC, -0x2005, 0x2FFA});
    CHECK(VectorOf(camera.originOffset) == Vec3{});
    CHECK(camera.eye.currentPosition.pad == 0x66);
    CHECK(camera.lookAt.currentPosition.pad == 0x77);
    CHECK(camera.originOffset.pad == 0x55);
  }
};
}  // namespace

TEST_CASE("Event camera start mirrors the field camera into both tracks",
          "[event][camera][ownership]") {
  const Vec3 eye{1, -2, 0x7FFFFFFF};
  const Vec3 look_at{4, 5, -6};
  const event::EventCameraStart start = event::StartEventCamera(eye, look_at);
  CHECK(start.eye.position == eye);
  CHECK(start.eye.partner == look_at);
  CHECK(start.look_at.position == look_at);
  CHECK(start.look_at.partner == eye);
  CHECK(start.eye.motion_mode == event::kEventCameraTrackIdle);
  CHECK(start.look_at.motion_mode == event::kEventCameraTrackIdle);
  CHECK_FALSE(start.eye.following);
  CHECK_FALSE(start.look_at.follow_requested);
}

TEST_CASE("Event camera start rests the angles and clears the shakes",
          "[event][camera][ownership]") {
  const event::EventCameraStart start = event::StartEventCamera({}, {});
  CHECK(start.camera_roll == event::EventAngleStart{});
  CHECK(start.screen_roll == event::EventAngleStart{});
  CHECK(start.origin_actor == event::kEventCameraWorldOrigin);
  CHECK(start.shake_count == 0);
  CHECK(start.shake_a_busy == 0);
  CHECK(start.shake_b_busy == 0);
  CHECK_FALSE(start.screen_capture_enabled);
  CHECK(start.screen_blend_mode == event::kEventScreenBlendMode);
}

TEST_CASE("Event camera start rests the screen tweens and the lens",
          "[event][camera][ownership]") {
  const event::EventCameraStart start = event::StartEventCamera({}, {});
  CHECK(start.screen_scale ==
        event::EventScreenTweenStart{.value0 = 0x1000, .value1 = 0x1000});
  CHECK(start.screen_translation ==
        event::EventScreenTweenStart{.value0 = 0x1000, .value1 = 0x1000});
  // Only the tint's third value is written.
  CHECK(start.screen_tint == event::EventScreenTweenStart{.value2 = 0});
  CHECK_FALSE(start.screen_scale.value2.has_value());
  CHECK(start.screen_warp == event::EventScreenTweenStart{.value0 = 0x1000});
  CHECK(start.projection_distance == event::EventLensStart{.value = 0x200});
  CHECK(start.near_clip == event::EventLensStart{.value = 0x40});
  CHECK(start.far_clip == event::EventLensStart{.value = 0x1000});
}

TEST_CASE("Event field camera override keeps the request low byte",
          "[event][camera][ownership]") {
  CHECK(event::EventFieldCameraOverride(1) == 1);
  CHECK(event::EventFieldCameraOverride(0x1FF) == 0xFF);
  CHECK(event::EventFieldCameraOverride(0x100) == 0);
  CHECK(event::EventFieldCameraOverride(-1) == 0xFF);
}

TEST_CASE("Event camera origin update clears the offset at the world origin",
          "[event][camera][ownership]") {
  FakeOriginHost host;
  host.position = Position3{1, 2, 3};
  event::EventCameraOrigin origin = ActorOrigin();
  origin.actor = event::kEventCameraWorldOrigin;
  event::UpdateEventCameraOrigin(origin, host);
  CHECK(origin.offset == Vec3{});
  CHECK(host.actors.empty());
  CHECK(origin.eye_position == ActorOrigin().eye_position);
}

TEST_CASE("Event camera origin update scales the actor position",
          "[event][camera][ownership]") {
  FakeOriginHost host;
  host.position = Position3{-0x8000, 0x7FFF, 1};
  event::EventCameraOrigin origin = ActorOrigin();
  origin.actor = 0xFFFF;
  event::UpdateEventCameraOrigin(origin, host);
  CHECK(origin.offset == Vec3{-0x8000000, 0x7FFF000, 0x1000});
  CHECK(host.actors == std::vector<int>{0xFFFF});
  CHECK(origin.actor == 0xFFFF);
}

TEST_CASE("Event camera origin update zeroes the offset for an empty slot",
          "[event][camera][ownership]") {
  FakeOriginHost host;
  event::EventCameraOrigin origin = ActorOrigin();
  event::UpdateEventCameraOrigin(origin, host);
  CHECK(origin.offset == Vec3{});
  CHECK(host.actors == std::vector<int>{7});
}

TEST_CASE("Event camera origin switch to the world applies the actor offset",
          "[event][camera][ownership]") {
  FakeOriginHost host;
  event::EventCameraOrigin origin = ActorOrigin();
  event::SetEventCameraOriginActor(origin, event::kEventCameraWorldOrigin,
                                   host);
  CHECK(origin.actor == event::kEventCameraWorldOrigin);
  CHECK(origin.eye_position == Vec3{0x1001, -0x1FFE, 0x3003});
  CHECK(origin.look_at_position == Vec3{0xFFC, -0x2005, 0x2FFA});
  CHECK(origin.offset == Vec3{});
  CHECK(host.actors.empty());
}

TEST_CASE("Event camera origin switch wraps the position sums",
          "[event][camera][ownership]") {
  FakeOriginHost host;
  event::EventCameraOrigin origin = ActorOrigin();
  origin.eye_position = {0x7FFFFFFF, 0, 0};
  origin.offset = {1, 0, 0};
  event::SetEventCameraOriginActor(origin, event::kEventCameraWorldOrigin,
                                   host);
  CHECK(origin.eye_position.x == static_cast<std::int32_t>(0x80000000U));
}

TEST_CASE("Event camera origin staying at the world moves nothing",
          "[event][camera][ownership]") {
  FakeOriginHost host;
  event::EventCameraOrigin origin = ActorOrigin();
  origin.actor = event::kEventCameraWorldOrigin;
  event::SetEventCameraOriginActor(origin, event::kEventCameraWorldOrigin,
                                   host);
  CHECK(origin.eye_position == ActorOrigin().eye_position);
  CHECK(origin.look_at_position == ActorOrigin().look_at_position);
  CHECK(origin.offset == Vec3{});
}

TEST_CASE("Event camera origin leaving the world subtracts nothing",
          "[event][camera][ownership]") {
  FakeOriginHost host;
  host.position = Position3{2, 3, 4};
  event::EventCameraOrigin origin = ActorOrigin();
  origin.actor = event::kEventCameraWorldOrigin;
  event::SetEventCameraOriginActor(origin, 9, host);
  CHECK(origin.actor == 9);
  CHECK(origin.eye_position == ActorOrigin().eye_position);
  CHECK(origin.look_at_position == ActorOrigin().look_at_position);
  CHECK(origin.offset == Vec3{0x2000, 0x3000, 0x4000});
  CHECK(host.actors == std::vector<int>{9});
}

TEST_CASE("Event camera origin switch between actors keeps the positions",
          "[event][camera][ownership]") {
  FakeOriginHost host;
  host.position = Position3{-1, 0, 1};
  event::EventCameraOrigin origin = ActorOrigin();
  event::SetEventCameraOriginActor(origin, 3, host);
  CHECK(origin.actor == 3);
  CHECK(origin.eye_position == ActorOrigin().eye_position);
  CHECK(origin.offset == Vec3{-0x1000, 0, 0x1000});
}

TEST_CASE("Event camera origin command reads an unaligned reference",
          "[event][camera][ownership]") {
  FakeOriginHost host;
  host.resolved = 9;
  host.position = Position3{1, 1, 1};
  const std::array<std::uint8_t, 4> bytes{0x00, 0xE8, 0x05, 0x10};
  event::EventCameraOrigin origin = ActorOrigin();
  CHECK(event::CommandSetCameraOriginActor(
      event::ScriptCommand{bytes}.subspan(1), origin, host));
  CHECK(host.references == std::vector<std::uint16_t>{0x1005});
  CHECK(origin.actor == 9);
  CHECK(origin.offset == Vec3{0x1000, 0x1000, 0x1000});
}

TEST_CASE("Event camera origin command truncates the resolved actor",
          "[event][camera][ownership]") {
  FakeOriginHost host;
  host.resolved = 0x12000;
  const std::array<std::uint8_t, 3> bytes{0xE8, 0x03, 0x00};
  event::EventCameraOrigin origin = ActorOrigin();
  CHECK(event::CommandSetCameraOriginActor(event::ScriptCommand{bytes}, origin,
                                           host));
  CHECK(origin.actor == event::kEventCameraWorldOrigin);
  CHECK(origin.eye_position == Vec3{0x1001, -0x1FFE, 0x3003});
  CHECK(origin.offset == Vec3{});
}

TEST_CASE("Event camera origin command refuses a short instruction",
          "[event][camera][ownership]") {
  FakeOriginHost host;
  const std::array<std::uint8_t, 2> bytes{0xE8, 0x00};
  event::EventCameraOrigin origin = ActorOrigin();
  CHECK_FALSE(event::CommandSetCameraOriginActor(event::ScriptCommand{bytes},
                                                 origin, host));
  CHECK(origin == ActorOrigin());
  CHECK(host.references.empty());
}

TEST_CASE("Event camera init entry allocates and clears the field override",
          "[event][camera][ownership]") {
  LiveEventCameraFixture fixture;
  Event_skipNextFieldCameraRestore = 1;
  event::InitializeEventCameraEntry();
  REQUIRE(Event_camera != nullptr);
  const auto capacity = VS::Core::Memory::LivePayloadCapacity(Event_camera);
  REQUIRE(capacity.has_value());
  CHECK(*capacity >= sizeof(Event_CameraState));
  CHECK(Event_skipNextFieldCameraRestore == 0);
  CHECK(Event_camera->originActorId == event::kEventCameraWorldOrigin);
}

TEST_CASE("Event camera init entry mirrors the field camera",
          "[event][camera][ownership]") {
  LiveEventCameraFixture fixture;
  VECTOR field_eye{};
  VECTOR field_look_at{};
  VS::Field::LiveCameraPosition(&field_eye);
  VS::Field::LiveCameraLookAt(&field_look_at);
  event::InitializeEventCameraEntry();
  REQUIRE(Event_camera != nullptr);
  const Event_CameraState& camera = *Event_camera;
  CHECK(VectorOf(camera.eye.currentPosition) == VectorOf(field_eye));
  CHECK(VectorOf(camera.lookAt.partnerPosition) == VectorOf(field_eye));
  CHECK(VectorOf(camera.lookAt.currentPosition) == VectorOf(field_look_at));
  CHECK(VectorOf(camera.eye.partnerPosition) == VectorOf(field_look_at));
  CHECK(camera.eye.motionMode == event::kEventCameraTrackIdle);
  CHECK(camera.lookAt.motionMode == event::kEventCameraTrackIdle);
}

TEST_CASE("Event camera init entry starts the angles lens and screen tweens",
          "[event][camera][ownership]") {
  LiveEventCameraFixture fixture;
  event::InitializeEventCameraEntry();
  REQUIRE(Event_camera != nullptr);
  const Event_CameraState& camera = *Event_camera;
  CHECK(camera.cameraRoll.hasChanged == 1);
  CHECK(camera.capturedScreenRoll.hasChanged == 1);
  CHECK(camera.screenScale.currentValue0 == 0x1000);
  CHECK(camera.screenTranslation.currentValue1 == 0x1000);
  CHECK(camera.screenEffectTween.currentValue0 == 0x1000);
  CHECK(camera.projectionDistance.value == 0x200);
  CHECK(camera.nearClip.value == 0x40);
  CHECK(camera.farClip.value == 0x1000);
}

TEST_CASE("Event camera re-init allocates a second workspace",
          "[event][camera][ownership]") {
  LiveEventCameraFixture fixture;
  event::InitializeEventCameraEntry();
  Event_CameraState* const first = Event_camera;
  REQUIRE(first != nullptr);
  event::InitializeEventCameraEntry();
  REQUIRE(Event_camera != nullptr);
  CHECK(Event_camera != first);
  // The original leaked the first workspace; it stays allocated.
  CHECK(VS::Core::Memory::LivePayloadCapacity(first).has_value());
}

TEST_CASE("Event camera free entry releases and keeps the pointer",
          "[event][camera][ownership]") {
  LiveEventCameraFixture fixture;
  event::InitializeEventCameraEntry();
  Event_CameraState* const camera = Event_camera;
  REQUIRE(camera != nullptr);
  const std::uint64_t refused = VS::Core::Memory::RefusedLiveHeapRequestCount();
  FreeEventCamera();
  CHECK(Event_camera == camera);
  CHECK_FALSE(VS::Core::Memory::LivePayloadCapacity(camera).has_value());
  CHECK(VS::Core::Memory::RefusedLiveHeapRequestCount() == refused);
  // A second free reaches the heap with the released pointer and is refused.
  FreeEventCamera();
  CHECK(Event_camera == camera);
  CHECK(VS::Core::Memory::RefusedLiveHeapRequestCount() == refused + 1);
}

TEST_CASE("Event camera free without a workspace is refused by the heap",
          "[event][camera][ownership]") {
  LiveEventCameraFixture fixture;
  Event_camera = nullptr;
  const std::uint64_t refused = VS::Core::Memory::RefusedLiveHeapRequestCount();
  FreeEventCamera();
  CHECK(Event_camera == nullptr);
  CHECK(VS::Core::Memory::RefusedLiveHeapRequestCount() == refused + 1);
}

TEST_CASE("Event camera free then re-init reuses the released block",
          "[event][camera][ownership]") {
  LiveEventCameraFixture fixture;
  event::InitializeEventCameraEntry();
  Event_CameraState* const first = Event_camera;
  REQUIRE(first != nullptr);
  first->originActorId = 3;
  FreeEventCamera();
  event::InitializeEventCameraEntry();
  CHECK(Event_camera == first);
  CHECK(Event_camera->originActorId == event::kEventCameraWorldOrigin);
}

TEST_CASE("Event field camera override entry stores the low byte",
          "[event][camera][ownership]") {
  const char saved = Event_skipNextFieldCameraRestore;
  event::SetEventFieldCameraOverrideEntry(0x1FF);
  CHECK(static_cast<std::uint8_t>(Event_skipNextFieldCameraRestore) == 0xFF);
  event::SetEventFieldCameraOverrideEntry(0x100);
  CHECK(Event_skipNextFieldCameraRestore == 0);
  Event_skipNextFieldCameraRestore = saved;
}

TEST_CASE("Event camera origin entry returns the live workspace to the world",
          "[event][camera][ownership]") {
  LiveOriginWorkspace workspace;
  event::SetEventCameraOriginActorEntry(event::kEventCameraWorldOrigin);
  workspace.RequireWorldWithOffsetApplied();
}

TEST_CASE("Event camera origin command entry resolves a world reference",
          "[event][camera][ownership]") {
  LiveOriginWorkspace workspace;
  std::array<std::uint8_t, 3> instruction{0xE8, 0x00, 0x20};
  CHECK(Event_CommandSetCameraOriginActorOpcode(instruction.data(), 0) == 0);
  workspace.RequireWorldWithOffsetApplied();
}

TEST_CASE("Event camera origin update entry clears the world offset",
          "[event][camera][ownership]") {
  LiveOriginWorkspace workspace;
  workspace.camera.originActorId = event::kEventCameraWorldOrigin;
  event::UpdateEventCameraOriginEntry();
  CHECK(VectorOf(workspace.camera.originOffset) == Vec3{});
  CHECK(workspace.camera.originOffset.pad == 0x55);
  CHECK(VectorOf(workspace.camera.eye.currentPosition) == Vec3{1, 2, 3});
}
