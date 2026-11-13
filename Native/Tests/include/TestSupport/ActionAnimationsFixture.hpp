#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "Actors/AnimationPose.hpp"
#include "Battle/ActionAnimations.hpp"

namespace VS::test_support::action_animations {
namespace b = VS::Battle;
namespace a = VS::Actors;
using Result = b::ActionAnimationResult;

enum class Kind : std::uint32_t {
  kFacing = 1,
  kPitch,
  kHeader,
  kPlay,
  kReset,
  kBone,
  kQueue,
  kSide,
  kSpecialPitch
};
struct Event {
  Kind kind = Kind::kFacing;
  std::uint32_t owner{}, first{}, second{}, third{};
  bool operator==(const Event&) const = default;
};

struct Actor final : b::ActionAnimationActor {
  std::uint32_t flags = 0x00800000;
  std::uint8_t slot = 1;
  std::uint8_t jump = 99;
  std::uint16_t turn = 777;
  a::MotionVector position{};
  std::int16_t facing = 0;
  std::int16_t delta = 333;
  std::uint16_t radius = 127;
  std::uint16_t model = 40;
  std::uint8_t bones = 1;
  std::uint8_t sequence_model = 1;
  std::uint8_t common_entries = 100;
  std::uint8_t battle_entries = 102;
  std::uint8_t special_entries = 64;
  bool map_present = true;

  std::uint8_t Slot() const override { return slot; }
  bool WeaponDrawn() const override { return (flags & 0x00800000U) != 0; }
  std::uint8_t MovementMode() const override {
    return static_cast<std::uint8_t>((flags >> 16) & 7U);
  }
  bool MovementBlocked() const override { return (flags & 0x00600000U) != 0; }
  std::uint16_t Radius() const override { return radius; }
  std::uint16_t ModelId() const override { return model; }
  a::MotionVector Position() const override { return position; }
  std::int16_t Facing() const override { return facing; }
  void SetFacingDelta(std::int16_t value) override { delta = value; }
  void SetTurnFrames(std::int16_t value) override {
    turn = static_cast<std::uint16_t>(value);
  }
  void ClearMovementMode() override { flags &= ~0x00070000U; }
  void ClearJumpCountdown() override { jump = 0; }
};

class Banks final : public a::AnimationBankSource {
 public:
  explicit Banks(bool present) {
    bytes_.fill(0);
    // One clip header at offset 8, with a 256-entry map at offset 1024.
    bytes_.at(5) = 4;
    for (std::size_t i = 1024; i < 1280; ++i) {
      bytes_.at(i) = present ? 0 : 0xFF;
    }
  }
  a::AnimationBankView Bank(const a::AnimationEntry&) override {
    return {.loaded = true, .bytes = bytes_};
  }

 private:
  std::array<std::uint8_t, 2048> bytes_{};
};

struct World final : b::ActionAnimationWorld {
  std::array<::VS::test_support::action_animations::Actor, 3> actors;
  int table_owner = 0;
  std::optional<a::MotionVector> target = a::MotionVector{0, 0, 1024, 0};
  bool controlled_helpers = true;
  std::int32_t helper_facing = 123;
  std::int32_t helper_pitch = 0;
  bool loaded_headers = false;
  std::array<bool, 2> headers{true, true};
  std::size_t queries = 0;
  std::uint8_t group = 0;
  std::uint32_t side = 0;
  std::uint8_t special_pitch = 77;
  std::int16_t bone_y = 0;
  bool bone_success = true;
  int preset = 0;
  std::vector<Event> events;
  std::function<void(const Event&)> callback;
  mutable unsigned target_reads = 0;
  unsigned group_reads = 0;
  unsigned actor_reads = 0;

  b::ActionAnimationActor* Actor(int) override {
    ++actor_reads;
    return table_owner < 0 ? nullptr
                           : &actors.at(static_cast<std::size_t>(table_owner));
  }
  std::optional<a::MotionVector> Target() const override {
    ++target_reads;
    return target;
  }
  int Owner(const b::ActionAnimationActor& actor) const {
    for (std::size_t i = 0; i < actors.size(); ++i) {
      if (&actors.at(i) == &actor) return static_cast<int>(i);
    }
    return -1;
  }
  void Record(Event event) {
    events.push_back(event);
    ApplyPreset(event);
    if (callback) callback(event);
  }
  std::optional<std::int16_t> RootBoneHeight(std::uint8_t slot) override {
    Record({Kind::kBone, slot, 0, 0, 0});
    return bone_success ? std::optional{bone_y} : std::nullopt;
  }
  std::int32_t FacingDeltaToTarget(b::ActionAnimationActor& actor) override {
    Record({Kind::kFacing, static_cast<std::uint32_t>(Owner(actor)),
            target.has_value() ? 1U : 0U});
    if (controlled_helpers) return target ? helper_facing : 0;
    return b::ActionAnimationWorld::FacingDeltaToTarget(actor);
  }
  b::ActionAnimationPitch PitchToTarget(
      b::ActionAnimationActor& actor) override {
    Record({Kind::kPitch, static_cast<std::uint32_t>(Owner(actor)),
            target.has_value() ? 1U : 0U});
    if (controlled_helpers) return {target ? helper_pitch : 0};
    return b::ActionAnimationWorld::PitchToTarget(actor);
  }
  bool HasAnimation(b::ActionAnimationActor& actor, int animation) override {
    const auto index = static_cast<std::size_t>(Owner(actor));
    bool present = headers.at(queries < headers.size() ? queries : 1);
    if (loaded_headers) {
      const auto& record = actors.at(index);
      const a::AnimationBankLimits limits{
          .bones = record.bones,
          .slot = record.slot,
          .model_id = record.model,
          .sequence_model_id = record.sequence_model,
          .common_entries = record.common_entries,
          .battle_entries = record.battle_entries,
          .special_entries = record.special_entries,
          .special_pitch = special_pitch,
          .shared_slot = 0};
      Banks banks(record.map_present);
      present = a::FindActorAnimationHeader(limits, banks, animation).status ==
                a::AnimationHeaderStatus::kFound;
    }
    ++queries;
    Record({Kind::kHeader, static_cast<std::uint32_t>(index),
            static_cast<std::uint32_t>(animation)});
    return present;
  }
  std::uint8_t ChainGroup(int) override {
    ++group_reads;
    return group;
  }
  std::uint32_t ChainSide() override { return side; }
  void SetChainSide(std::uint32_t value) override {
    side = value;
    Record({Kind::kSide, value});
  }
  void SetSpecialPitch(std::uint8_t value) override {
    special_pitch = value;
    Record({Kind::kSpecialPitch, value});
  }
  void Play(int slot, std::int32_t animation, int blend) override {
    Record({Kind::kPlay, static_cast<std::uint32_t>(slot),
            static_cast<std::uint32_t>(animation), 0,
            static_cast<std::uint32_t>(blend)});
  }
  void Queue(int slot, int animation, int blend, int start) override {
    Record({Kind::kQueue, static_cast<std::uint32_t>(slot),
            static_cast<std::uint32_t>(animation),
            static_cast<std::uint32_t>(blend),
            static_cast<std::uint32_t>(start)});
  }
  void ResetHead(b::ActionAnimationActor& actor) override {
    Record({Kind::kReset, static_cast<std::uint32_t>(Owner(actor))});
  }
  std::vector<Event> Calls(Kind kind) const {
    std::vector<Event> result;
    for (const auto& event : events) {
      if (event.kind == kind) result.push_back(event);
    }
    return result;
  }
  void ApplyPreset(const Event& event) {
    if (preset == 1 && event.kind == Kind::kFacing) {
      auto& actor = actors.at(event.owner);
      actor.radius = 129;
      actor.model = 40;
      actor.flags ^= 0x100;
      table_owner = 1;
    } else if (preset == 2 && event.kind == Kind::kPitch) {
      actors.at(event.owner).model = 40;
      actors.at(event.owner).flags ^= 0x100;
      table_owner = 1;
    } else if ((preset == 3 || preset == 4 || preset == 5 || preset == 10) &&
               event.kind == Kind::kHeader && queries == 1) {
      auto& actor = actors.at(event.owner);
      if (preset == 3 || preset == 10) actor.flags ^= 0x00800000;
      if (preset == 4) {
        actor.slot = 5;
        actor.position.x = -700;
        actor.position.z = 400;
        actor.facing = 30000;
        if (target) target->y = 1900;
      }
      if (preset == 3 || preset == 4) table_owner = 1;
      if (preset == 5) table_owner = -1;
    } else if (preset == 6 && event.kind == Kind::kPlay) {
      table_owner = 2;
      for (auto& actor : actors) {
        actor.slot = 0;
        actor.flags = (actor.flags & ~0x00070000U) | 0x00020000U;
        actor.jump = 199;
      }
    } else if ((preset == 7 || preset == 8) && event.kind == Kind::kReset) {
      auto& actor = actors.at(event.owner);
      actor.slot = 0;
      actor.flags = (actor.flags & ~0x00070000U) |
                    (preset == 7 ? 0x00020000U : 0x00030000U);
      actor.jump = 199;
      table_owner = 2;
    } else if (preset == 9 && event.kind == Kind::kBone) {
      for (auto& actor : actors) {
        actor.position.x = -1234;
        actor.position.z = 345;
        actor.facing = 31500;
      }
      if (target) {
        target->x = 999;
        target->y = 1200;
        target->z = -744;
      }
    } else if (preset == 11 && event.kind == Kind::kFacing) {
      auto& actor = actors.at(event.owner);
      actor.radius = 127;
      actor.slot = 0;
      actor.flags = (actor.flags & ~0x00070000U) | 0x00020000U;
    }
  }
};

inline Result Run(int function, int slot, int parameter, int sequence,
                  b::ActionAnimationWorld& world) {
  switch (function) {
    case 817:
      return b::PlayMeleeAttackAnimation(slot, parameter, world);
    case 818:
      return b::PlayDefenseAnimation(slot, world);
    case 823:
      return b::PlaySpellCastAnimation(slot, parameter, world);
    case 831:
      return b::PlayChainAbilityAnimation(slot, parameter, sequence, world);
    case 833:
      return b::PlaySpecialActionAnimation(slot, parameter, world);
    case 834:
      return b::PlayBreakArtAnimation(slot, parameter, world);
    default:
      return Result::kInvalidCastPhase;
  }
}
}  // namespace VS::test_support::action_animations
