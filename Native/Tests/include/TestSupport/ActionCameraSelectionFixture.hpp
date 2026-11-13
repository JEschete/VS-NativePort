#pragma once

#include <array>
#include <functional>
#include <optional>
#include <vector>

#include "Battle/ActionCameraSelection.hpp"

namespace VS::test_support::camera_selection {
namespace b = Battle;
using Event = std::array<std::int32_t, 4>;
struct Queue final : b::ActionCameraRequest {
  b::ActionHit primary{}, target{};
  b::ActionPoint focus{};
  std::uint16_t count{};
  std::uint8_t PrimaryActor() const override { return primary.actor; }
  b::ActionHit FirstTarget() const override { return target; }
  std::uint16_t Count() const override { return count; }
  b::ActionPoint Focus() const override { return focus; }
};
struct Selection final : b::ActionSelectionState {
  bool selecting{};
  std::uint8_t actor{}, frames{}, preview{}, selected{};
  std::uint16_t action{};
  std::function<int()> menu = [] { return 0; };
  std::function<int()> area = [] { return 0; };
  std::function<void()> selected_callback = [] {};
  bool Selecting() const override { return selecting; }
  std::uint8_t SourceActor() const override { return actor; }
  std::uint8_t Frames() const override { return frames; }
  void SetFrames(std::uint8_t value) override { frames = value; }
  std::uint8_t Preview() const override { return preview; }
  std::uint8_t Selected() const override { return selected; }
  void SetSelected(std::uint8_t value) override {
    selected = value;
    selected_callback();
  }
  std::uint16_t ActionId() const override { return action; }
  int UpdateMenu() override { return menu(); }
  int UpdateArea() override { return area(); }
};
struct World final : b::ActionCameraWorld, b::ActionSelectionWorld {
  std::array<Queue, 2> queues{};
  std::array<Selection, 2> states{};
  int current{}, queue_owner{};
  bool queue_present = true, position_present = true;
  std::optional<std::int16_t> zoom = 0;
  std::optional<bool> targeting = false;
  int bone = -255, mode = 1, yaw{}, distance = 1536, projection = 512;
  int reads{}, tags{}, positions{}, commits{}, starts{}, definition_reads{};
  Core::Math::Vec3 displacement{};
  b::ActionPoint source{-1, -5, 9, 41}, target{-4, -2, 2, 42}, stepped{};
  std::vector<Event> events;
  std::function<void()> on_position = [] {};
  std::function<void()> on_step = [] {};
  std::function<void()> on_projection = [] {};
  std::function<void()> on_tracking = [] {};
  b::ActionCameraRequest* Queued() override {
    return queue_present ? &queues.at(queue_owner) : nullptr;
  }
  int BoneTag(int actor, int limb) override {
    ++tags;
    events.push_back({1, actor, limb, 2});
    return bone;
  }
  bool BonePosition(int actor, int selected_bone,
                    b::ActionPoint& output) override {
    events.push_back({2, actor, selected_bone, 0});
    const auto point = positions++ == 0 ? source : target;
    if (!position_present) return false;
    output.x = point.x;
    output.y = point.y;
    output.z = point.z;
    on_position();
    return true;
  }
  void StepCamera(b::ActionPoint point) override {
    stepped = point;
    events.push_back({3, point.x, point.y, point.z});
    on_step();
  }
  Core::Math::Vec3 TrackingDisplacement(b::ActionPoint point) override {
    events.push_back({4, point.x, point.y, point.z});
    on_tracking();
    return displacement;
  }
  std::int32_t CameraMode() override { return mode; }
  std::optional<std::int16_t> Zoom() override { return zoom; }
  std::int32_t YawDelta() override { return yaw; }
  std::int32_t Distance() override { return distance; }
  void SetDistance(std::int32_t value) override { distance = value; }
  std::int32_t Projection() override { return projection; }
  void SetProjection(std::int32_t value) override { projection = value; }
  void ApplyProjection(std::int32_t value) override {
    events.push_back({5, value, 0, 0});
    on_projection();
  }
  void StartPresentation() override {
    ++starts;
    events.push_back({6, 0, 0, 0});
  }
  b::ActionSelectionState* Current() override {
    ++reads;
    return current < 0 ? nullptr : &states.at(current);
  }
  void Commit(int value) override {
    ++commits;
    events.push_back({7, value, 0, 0});
  }
  void FilterArea() override { events.push_back({8, 0, 0, 0}); }
  std::optional<bool> NeedsTargeting(std::uint16_t action) override {
    ++definition_reads;
    events.push_back({9, action, 0, 0});
    return targeting;
  }
  void InitializeTargeting(std::uint16_t action) override {
    events.push_back({10, action, 0, 0});
  }
  void ClearSphereTargets() override { events.push_back({11, 0, 0, 0}); }
  void ClearPrimaryVolume() override { events.push_back({12, 0, 0, 0}); }
  void ClearSecondaryVolume() override { events.push_back({13, 0, 0, 0}); }
  void FreeSphere() override { events.push_back({14, 0, 0, 0}); }
  void ReturnToMenu() override { events.push_back({15, 0, 0, 0}); }
};
}  // namespace VS::test_support::camera_selection
