#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "Battle/ActionPreview.hpp"
#include "Battle/ActionSequence.hpp"

namespace VS::test_support::sequence {
namespace b = Battle;
using Word = std::uint32_t;
inline constexpr Word kNotification = 0x800CB45C, kFlags = 0x800A0BE0;
inline constexpr Word kAsset = 0x800995B0, kCanStart = 0x800CEEBC;
inline constexpr Word kAdvance = 0x800704B0, kAnimation = 0x80070278;
inline constexpr Word kTrack = 0x800CEF74, kPlans = 0x800DEC88;
inline constexpr Word kRelease = 0x80069D14, kFade = 0x8006FCBC;
inline constexpr Word kBusy = 0x8009E4B0, kDefeat = 0x80074A20;
inline constexpr Word kRecovery = 0x8006FBCC, kProjection = 0x80041534;
inline constexpr Word kLook = 0x80077DF0, kResume = 0x8006C40C;
inline constexpr Word kHud = 0x800CB654, kIdle = 0x8009E5C4;
inline constexpr Word kResolve = 0x800792E4, kEvent = 0x800BEC58;
inline constexpr Word kCutscene = 0x80073898, kLoot = 0x8006FB48;

struct World;
struct Action final : b::SequenceAction {
  b::ActionRequest data{};
  World* world{};
  Word owner{};
  b::ActionTargetHit Primary() const override {
    return {data.primary.position_kind != 0, data.primary.actor, 0};
  }
  b::ActionTargetHit Target(std::size_t index) const override {
    const auto& hit = data.targets.at(index);
    return {hit.position_kind != 0, hit.actor, 0};
  }
  std::uint16_t Count() const override { return data.count; }
  int RebuildPlans() override;
};
struct State {
  std::array<Action, 4> queue{};
  Word index{}, count{};
  std::uint8_t selected{};
  std::int16_t preferred = -9;
};
struct World final : b::ActionSequenceWorld {
  std::array<State, 2> states{};
  std::size_t current{};
  std::array<Word, 42> flags{};
  std::array<int, 42> busy{}, defeats{};
  int pending{}, free_asset{}, can_start = 1, plan_result{};
  Word effect_flags{}, control = 3;
  std::int32_t camera_mode{}, projection = 512, distance = -9, event_result{};
  int look = 1;
  Word queued_reads{}, chain_reads{}, preferred_reads{}, selected_reads{};
  std::vector<Word> owners, events;
  std::function<void(Word)> callback;

  World() {
    for (std::size_t state = 0; state < states.size(); ++state) {
      for (std::size_t queue_index = 0;
           queue_index < states.at(state).queue.size(); ++queue_index) {
        auto& action = states.at(state).queue.at(queue_index);
        action.world = this;
        action.owner = static_cast<Word>(state * 4 + queue_index);
      }
    }
  }
  State& Current() { return states.at(current); }
  template <typename... T>
  void Event(Word address, T... args) {
    events.push_back(address);
    (events.push_back(static_cast<Word>(args)), ...);
    if (callback) callback(address);
  }
  b::SequenceAction& Queued() override {
    ++queued_reads;
    return Current().queue.at(Current().index & 3U);
  }
  b::SequenceAction& Step(std::size_t index) override {
    auto& action = Current().queue.at(index);
    owners.push_back(action.owner);
    return action;
  }
  Word ChainCount() override {
    ++chain_reads;
    return Current().count;
  }
  int NotificationPending() override {
    Event(kNotification);
    return pending;
  }
  Word ActorFlags(int actor) override {
    Event(kFlags, actor);
    return flags.at(static_cast<std::size_t>(actor));
  }
  int FreeAssetRequest() override {
    Event(kAsset);
    return free_asset;
  }
  int CanStartEffect() override {
    Event(kCanStart);
    return can_start;
  }
  void AdvanceEffectPhase() override { Event(kAdvance); }
  Word EffectBusyFlags() override { return effect_flags; }
  void StartAnimation() override { Event(kAnimation); }
  void StartEffectTrack() override { Event(kTrack, 0); }
  void ReleaseAttacks() override { Event(kRelease); }
  void FadeTrails() override {
    Event(kFade, Current().queue.at(0).data.primary.actor);
  }
  int AnimationBusy(int actor) override {
    const int result = busy.at(static_cast<std::size_t>(actor));
    Event(kBusy, actor);
    return result;
  }
  int BeginDefeat(int actor) override {
    const int result = defeats.at(static_cast<std::size_t>(actor));
    Event(kDefeat, actor);
    return result;
  }
  void BeginRecovery(int mode) override { Event(kRecovery, mode); }
  std::int32_t CameraMode() override { return camera_mode; }
  std::int32_t Projection() override { return projection; }
  void ApplyProjection(std::int32_t value) override {
    projection = value;
    Event(kProjection, value);
  }
  std::int32_t Distance() override { return distance; }
  void SetDistance(std::int32_t value) override { distance = value; }
  std::int16_t PreferredDistance() override {
    ++preferred_reads;
    return Current().preferred;
  }
  int LookAtOnAshley() override {
    Event(kLook);
    return look;
  }
  void ResumeField() override { Event(kResume); }
  void ShowHud() override { Event(kHud, 0); }
  std::uint8_t SelectedActor() override {
    ++selected_reads;
    return Current().selected;
  }
  void RestoreIdle(int actor) override { Event(kIdle, actor); }
  void ResolveDefeat(int source, int target) override {
    Event(kResolve, source, target, 1);
  }
  Word ControlMode() override { return control; }
  std::int32_t EvaluateRoomEvents() override {
    Event(kEvent, 12, 0, 0, 0);
    return event_result;
  }
  void BeginCutscene() override { Event(kCutscene); }
  void OpenLoot() override { Event(kLoot); }
};
inline int Action::RebuildPlans() {
  const int result = world->plan_result;
  world->Event(kPlans, owner);
  return result;
}
}  // namespace VS::test_support::sequence
