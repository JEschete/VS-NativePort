#include "Field/GimmickPlacementLive.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>

#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorMovementLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Audio/ActorSoundLive.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/ActorCollisionLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/GimmickPlacement.hpp"
#include "Field/GimmickPlacementLive.hpp"
#include "Field/InteractionTargetLive.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Location.h"
#include "Battle/Ai/Navigation.h"
#include "Core/Diagnostics.h"
#include "Field/Gimmicks.h"
}

namespace {

constexpr std::uint8_t kNoParent = 0xFF;
using GimmickInstance = Field_GimmickInstance;
using QueryStateValue = VS::Field::GimmickQueryState;
using QueryStates = std::array<QueryStateValue, VS::Field::kGimmickQuerySlots>;
constexpr auto kQueryPrefixBytes = offsetof(GimmickInstance, reserved178);
constexpr unsigned kTileByteBits = 8;
constexpr unsigned kTileZShift = 16;
constexpr unsigned kTileReservedShift = 24;
constexpr std::size_t kOrientationMatrixIndex = 4;
using OrientationRow = VS::Field::GimmickOrientationRow;
constexpr auto kOrientationRows = VS::Field::kGimmickOrientationCount;
using OrientationRows = std::array<OrientationRow, kOrientationRows>;
constexpr auto kRollColumns = VS::Field::kGimmickRollColumns;
constexpr auto kGimmickRollBytes = kOrientationRows * kRollColumns;
constexpr int kGimmickInteractionFrames = 4;
constexpr int kGimmickIdleAnimation = 1;
constexpr int kGimmickIdleBlendFrames = 8;
constexpr std::int16_t kGimmickDropMode = 0xFC;
constexpr std::int16_t kGimmickDropVelocity = 24;
constexpr std::int16_t kGimmickCarryMode = 8;
constexpr std::int32_t kGimmickTileUnit = 128;
constexpr std::int32_t kGimmickTileCenter = 64;

bool ReadableQueryRecord(const Field_GimmickInstance* record) {
  if (record == nullptr || reinterpret_cast<std::uintptr_t>(record) %
                                   alignof(Field_GimmickInstance) !=
                               0) {
    return false;
  }
  const auto allocation = VS::Core::Memory::LivePayloadContaining(record);
  if (!allocation) {
    return allocation.error() == VS::Core::Memory::HeapError::kOutsideArena;
  }
  const auto at = reinterpret_cast<std::uintptr_t>(record);
  const auto start = reinterpret_cast<std::uintptr_t>(allocation->data());
  return at >= start && at - start <= allocation->size() &&
         kQueryPrefixBytes <= allocation->size() - (at - start);
}

VS::Field::GimmickQueryState QueryState(const Field_GimmickInstance& record) {
  const auto tile_word =
      static_cast<std::uint8_t>(record.tileX) |
      (static_cast<std::uint32_t>(static_cast<std::uint8_t>(record.tileLayer))
       << kTileByteBits) |
      (static_cast<std::uint32_t>(static_cast<std::uint8_t>(record.tileZ))
       << kTileZShift) |
      (static_cast<std::uint32_t>(static_cast<std::uint8_t>(record.reserved5F))
       << kTileReservedShift);
  return {.present = true,
          .skipped = record.isSkipped != 0,
          .falling = record.objectInfo.isFalling != 0,
          .movement_mode = record.mode,
          .parent = record.parent,
          .carrier = record.objectInfo.carrier,
          .tile_word = tile_word,
          .position_y = record.positionY,
          .object_id = record.objectInfo.objectId,
          .subtype = record.objectInfo.subtype,
          .motion_state = record.motionState,
          .interaction = record.objectInfo.interaction};
}

QueryStates SnapshotQueryStates() {
  QueryStates states{};
  for (std::size_t slot = 0; slot < states.size(); ++slot) {
    if (const auto state = VS::Field::LiveGimmickQueryState(slot)) {
      states[slot] = *state;
    }
  }
  return states;
}

class LiveGimmickOrientationHost final
    : public VS::Field::GimmickOrientationHost {
 public:
  explicit LiveGimmickOrientationHost(std::size_t slot)
      : slot_(slot),
        record_(Field_gimmickInstances[slot]),
        generation_(VS::Field::LiveInteractionTargetDomain().generation) {}
  std::optional<std::uint8_t> ObjectKind() const override {
    if (!Valid()) return std::nullopt;
    return static_cast<std::uint8_t>(record_->objectInfo.objectId);
  }
  bool StoreMaterial(std::uint8_t material) override {
    if (!Valid()) return false;
    record_->objectInfo.material = material;
    return true;
  }
  bool StoreRotation(const VS::Core::Math::Mat3& rotation) override {
    if (!Valid()) return false;
    VS::Core::Math::StoreMatrix(record_->matrices[kOrientationMatrixIndex],
                                rotation);
    return true;
  }
  bool ResetRotation() override {
    if (!Valid()) return false;
    record_->rotationTicks = 0;
    record_->rotationX = 0;
    record_->rotationY = 0;
    record_->rotationZ = 0;
    record_->reserved2A = 0;
    return true;
  }

 private:
  bool Valid() const {
    return VS::Field::IsLiveGimmickPrefixReadable(record_) &&
           generation_ == VS::Field::LiveInteractionTargetDomain().generation &&
           Field_gimmickInstances[slot_] == record_;
  }
  std::size_t slot_;
  Field_GimmickInstance* record_;
  std::uint64_t generation_;
};

class LiveGimmickPointHost final : public VS::Field::GimmickPointHost {
 public:
  LiveGimmickPointHost()
      : generation_(VS::Field::LiveInteractionTargetDomain().generation) {}
  std::int32_t SupportHeight(
      const VS::Field::GimmickPlacementPoint& point) override {
    SVECTOR origin{};
    return VS::Field::LiveSampleActorSupportHeight(&origin, point.x, point.z,
                                                   1);
  }
  std::uint32_t LastProbeWord() const override {
    if (!Valid()) return UINT32_MAX;
    return VS::Field::LiveCollisionProbeCellWord();
  }
  bool ActorOccupies(const VS::Field::GimmickOccupancyQuery& query) override {
    if (!Valid()) return true;
    return VS::Field::LiveFindActorOccupyingTile(
               query.tile.tile_x, query.tile.tile_z, query.minimum_height, 1) !=
           nullptr;
  }

 private:
  bool Valid() const {
    return generation_ == VS::Field::LiveInteractionTargetDomain().generation;
  }
  std::uint64_t generation_;
};

class LiveGimmickStackDropHost final : public VS::Field::GimmickStackDropHost {
 public:
  LiveGimmickStackDropHost()
      : generation_(VS::Field::LiveInteractionTargetDomain().generation) {
    std::copy_n(Field_gimmickInstances, gimmicks_.size(), gimmicks_.begin());
    std::copy_n(Actors_actorRecords, actors_.size(), actors_.begin());
  }
  std::optional<VS::Field::GimmickStackState> Gimmick(
      std::size_t slot) override {
    if (!ValidGimmick(slot)) return std::nullopt;
    return VS::Field::GimmickStackState{QueryState(*gimmicks_[slot]),
                                        gimmicks_[slot]->velocityY};
  }
  bool StoreGimmick(std::size_t slot,
                    const VS::Field::GimmickStackState& state) override {
    if (!ValidGimmick(slot)) return false;
    auto& record = *gimmicks_[slot];
    record.objectInfo.isFalling = state.query.falling;
    record.mode = state.query.movement_mode;
    record.objectInfo.carrier = state.query.carrier;
    record.velocityY = state.velocity_y;
    return true;
  }
  bool SameGimmick(std::size_t first, std::size_t second) const override {
    return first < gimmicks_.size() && second < gimmicks_.size() &&
           gimmicks_[first] == gimmicks_[second];
  }
  bool RemoveOccupant(std::size_t slot) override {
    if (!ValidGimmick(slot)) return false;
    VS::Battle::Ai::RemoveLiveAiTileOccupant(gimmicks_[slot]);
    return ValidGimmick(slot);
  }
  std::optional<VS::Field::GimmickSupportedActor> ActorAbove(
      const VS::Field::GimmickQueryState& support,
      std::int32_t first_actor) override {
    if (!Valid()) return std::nullopt;
    auto* actor = VS::Field::LiveFindActorOccupyingTile(
        static_cast<std::uint8_t>(support.tile_word),
        static_cast<std::uint8_t>(support.tile_word >> kTileZShift),
        support.position_y, first_actor);
    if (actor == nullptr || !Valid()) return std::nullopt;
    for (std::size_t index = 0; index < actors_.size(); ++index) {
      if (actors_[index] == actor && Actors_actorRecords[index] == actor) {
        return VS::Field::GimmickSupportedActor{
            index, VS::Actors::LoadLiveMovementActor(*actor)};
      }
    }
    return std::nullopt;
  }
  bool StoreActor(const VS::Field::GimmickSupportedActor& actor) override {
    const auto index = actor.table_index;
    if (!Valid() || index >= actors_.size() || actors_[index] == nullptr ||
        Actors_actorRecords[index] != actors_[index]) {
      return false;
    }
    VS::Actors::StoreLiveMovementActor(actor.state, *actors_[index]);
    return Valid() && Actors_actorRecords[index] == actors_[index];
  }

 private:
  bool Valid() const {
    return generation_ == VS::Field::LiveInteractionTargetDomain().generation;
  }
  bool ValidGimmick(std::size_t slot) const {
    return Valid() && slot < gimmicks_.size() &&
           Field_gimmickInstances[slot] == gimmicks_[slot] &&
           ReadableQueryRecord(gimmicks_[slot]);
  }
  std::uint64_t generation_;
  std::array<Field_GimmickInstance*, VS::Field::kGimmickQuerySlots> gimmicks_{};
  decltype(std::to_array(Actors_actorRecords)) actors_{};
};

class LiveGimmickPlacementHost final : public VS::Field::GimmickPlacementHost {
 public:
  std::size_t GimmickSlots() const override {
    return std::size(Field_gimmickInstances);
  }

  std::optional<VS::Field::GimmickPlacementState> Gimmick(
      std::size_t slot) const override {
    if (slot >= GimmickSlots() || Field_gimmickInstances[slot] == nullptr) {
      return std::nullopt;
    }
    const auto& gimmick = *Field_gimmickInstances[slot];
    if (gimmick.parent != kNoParent) {
      return VS::Field::GimmickPlacementState{.attached = true};
    }
    if (gimmick.objectInfo.isFalling != 0) {
      return VS::Field::GimmickPlacementState{.falling = true};
    }
    if (gimmick.mode != 0) {
      return VS::Field::GimmickPlacementState{.movement_mode = gimmick.mode};
    }
    return VS::Field::GimmickPlacementState{
        .tile_x = static_cast<std::uint8_t>(gimmick.tileX),
        .tile_z = static_cast<std::uint8_t>(gimmick.tileZ),
        .orientation = static_cast<std::uint8_t>(gimmick.objectInfo.material),
        .position_x = gimmick.positionX,
        .position_y = gimmick.positionY,
        .position_z = gimmick.positionZ,
        .position_tail = gimmick.reserved22};
  }

  std::int32_t SupportHeight(
      const VS::Field::GimmickPlacementState& state) override {
    return VS::Field::LiveSampleSupportHeightAt(
        state.position_x, state.position_y, state.position_z, 0, 0, 0);
  }
};

class LiveGimmickInteractionQueryHost final
    : public VS::Field::GimmickInteractionQueryHost {
 public:
  explicit LiveGimmickInteractionQueryHost(std::size_t actor_slot)
      : actor_slot_(actor_slot),
        actor_(Actors_actorRecords[actor_slot]),
        generation_(VS::Field::LiveInteractionTargetDomain().generation) {
    std::copy_n(Field_gimmickInstances, gimmicks_.size(), gimmicks_.begin());
  }
  bool Valid() const {
    return actor_ != nullptr && Actors_actorRecords[actor_slot_] == actor_ &&
           generation_ == VS::Field::LiveInteractionTargetDomain().generation;
  }
  std::optional<VS::Actors::MovementActor> Actor(std::int32_t slot) override {
    if (!Valid() || slot != static_cast<std::int32_t>(actor_slot_)) {
      return std::nullopt;
    }
    return VS::Actors::LoadLiveMovementActor(*actor_);
  }
  bool StoreDirection(std::uint8_t direction) override {
    if (!Valid()) return false;
    Field_gimmickInteractionDirection = direction;
    return true;
  }
  std::optional<VS::Field::GimmickQueryState> Gimmick(
      std::size_t slot) override {
    if (!Valid() || slot >= gimmicks_.size() ||
        Field_gimmickInstances[slot] != gimmicks_[slot] ||
        !ReadableQueryRecord(gimmicks_[slot])) {
      return std::nullopt;
    }
    return QueryState(*gimmicks_[slot]);
  }
  std::optional<std::size_t> TopAt(
      const VS::Field::GimmickTileQuery& tile) override {
    QueryStates states{};
    for (std::size_t slot = 0; slot < states.size(); ++slot) {
      if (const auto value = Gimmick(slot)) states[slot] = *value;
    }
    return VS::Field::FindTopGimmickAtTile(states, tile, false);
  }
  bool PointAllowed(const VS::Field::GimmickPlacementPoint& point) override {
    return Valid() && VS::Field::CanPlaceLiveGimmickAtPoint(point) && Valid();
  }
  std::int32_t CeilingHeight(
      const VS::Field::GimmickWorldPoint& point) override {
    if (!Valid()) return INT32_MAX;
    const auto height = VS::Field::LiveRoomCeilingHeight(point.x, point.z);
    return Valid() ? height : INT32_MAX;
  }
  bool ActorAbove(const VS::Field::GimmickQueryState& support) override {
    if (!Valid()) return true;
    const auto* actor = VS::Field::LiveFindActorOccupyingTile(
        static_cast<std::uint8_t>(support.tile_word),
        static_cast<std::uint8_t>(support.tile_word >> kTileZShift),
        support.position_y, 1);
    return actor != nullptr || !Valid();
  }

 private:
  std::size_t actor_slot_;
  Actors_ActorRecord* actor_;
  std::uint64_t generation_;
  std::array<Field_GimmickInstance*, VS::Field::kGimmickQuerySlots> gimmicks_{};
};

class LiveGimmickInteractionStartHost final
    : public VS::Field::GimmickInteractionStartHost {
 public:
  explicit LiveGimmickInteractionStartHost(std::size_t actor_slot)
      : actor_slot_(actor_slot),
        actor_(Actors_actorRecords[actor_slot]),
        generation_(VS::Field::LiveInteractionTargetDomain().generation) {
    std::copy_n(Field_gimmickInstances, gimmicks_.size(), gimmicks_.begin());
  }
  std::optional<VS::Actors::MovementActor> Actor(std::int32_t slot) override {
    if (!ValidActor(slot)) return std::nullopt;
    return VS::Actors::LoadLiveMovementActor(*actor_);
  }
  std::optional<std::size_t> SelectAhead(std::int32_t slot) override {
    if (!ValidActor(slot)) return std::nullopt;
    const auto selected = VS::Field::FindLiveInteractableGimmickAhead(slot);
    if (!selected || !ValidObject(*selected)) return std::nullopt;
    selected_ = selected;
    return selected;
  }
  std::optional<std::int32_t> Direction() override {
    return Valid()
               ? std::optional<std::int32_t>{Field_gimmickInteractionDirection}
               : std::nullopt;
  }
  std::optional<VS::Field::GimmickInteractionObject> Object(
      std::size_t slot) override {
    if (!ValidObject(slot)) return std::nullopt;
    const auto& object = *gimmicks_[slot];
    return VS::Field::GimmickInteractionObject{
        QueryState(object), object.positionX, object.positionZ, object.slideX,
        object.slideZ};
  }
  bool StoreOrientation(std::size_t slot, std::uint16_t direction) override {
    if (!ValidObject(slot)) return false;
    gimmicks_[slot]->orientation = VS::Core::Math::WrapSigned16(direction);
    return true;
  }
  bool PublishSelected(std::size_t slot) override {
    if (!ValidObject(slot)) return false;
    Field_interactingGimmickId = static_cast<int>(slot);
    return true;
  }
  bool ClearSlide(std::size_t slot) override {
    if (!ValidObject(slot)) return false;
    gimmicks_[slot]->slideX = 0;
    gimmicks_[slot]->slideZ = 0;
    return true;
  }
  std::optional<std::uint8_t> Material(std::size_t slot) override {
    if (!ValidObject(slot)) return std::nullopt;
    return static_cast<std::uint8_t>(gimmicks_[slot]->objectInfo.material);
  }
  std::span<const std::uint8_t> RollOrientations() override {
    static_assert(kGimmickRollBytes <= sizeof(Field_kGimmickRollOrientations));
    return Valid() ? std::span(Field_kGimmickRollOrientations)
                         .first(kGimmickRollBytes)
                   : std::span<const std::uint8_t>{};
  }
  bool StoreMaterial(std::size_t slot, std::uint8_t material) override {
    if (!ValidObject(slot)) return false;
    gimmicks_[slot]->objectInfo.material = material;
    return true;
  }
  bool StartSlide(std::size_t slot, std::int16_t x, std::int16_t z) override {
    if (!ValidObject(slot)) return false;
    auto& object = *gimmicks_[slot];
    object.slideX = x;
    object.slideZ = z;
    object.velocityY = 0;
    return true;
  }
  bool PlaySounds(std::int32_t actor_slot, std::uint16_t first,
                  std::uint16_t second) override {
    if (!ValidActor(actor_slot)) return false;
    VS::Audio::LivePlayActorSoundPair(actor_, first, second);
    return Valid();
  }
  bool PlayAnimation(std::int32_t actor_slot, std::int32_t animation) override {
    if (!ValidActor(actor_slot)) return false;
    static_cast<void>(VS::Actors::PlayLiveActorAnimation(
        actor_slot, animation, 0, kGimmickInteractionFrames));
    return Valid();
  }
  bool SetWeaponPose(std::int32_t actor_slot) override {
    if (!ValidActor(actor_slot)) return false;
    actor_->weaponPoseActive = 1;
    return true;
  }
  bool ReserveTile(const VS::Field::GimmickTileQuery& tile) override {
    constexpr auto kGridSide = VS::Battle::Ai::kNavigationGridSide;
    if (!Valid() || Battle_Ai_tileFlagMap == nullptr || tile.tile_x < 0 ||
        tile.tile_z < 0 || tile.tile_x >= kGridSide ||
        tile.tile_z >= kGridSide) {
      return false;
    }
    VS::Battle::Ai::ReserveLiveAiDestinationTile(tile.tile_x, tile.tile_z);
    return Valid();
  }
  bool TurnToward(std::int32_t actor_slot,
                  const VS::Actors::PackedTile& tile) override {
    if (!ValidActor(actor_slot)) return false;
    Actors_TileTarget target{tile.x, tile.layer, tile.z, tile.spare};
    VS::Actors::TurnLiveActorTowardTileOverTime(actor_slot, &target,
                                                kGimmickInteractionFrames);
    return Valid();
  }
  bool TranslateToward(std::int32_t actor_slot,
                       const VS::Actors::MotionVector& point) override {
    if (!ValidActor(actor_slot)) return false;
    SVECTOR target{};
    target.vx = point.x;
    target.vy = point.y;
    target.vz = point.z;
    VS::Actors::TranslateLiveActorToOverTime(actor_slot, &target,
                                             kGimmickInteractionFrames);
    return Valid();
  }

 private:
  bool Valid() const {
    if (actor_ == nullptr || Actors_actorRecords[actor_slot_] != actor_ ||
        generation_ != VS::Field::LiveInteractionTargetDomain().generation) {
      return false;
    }
    return !selected_ || ValidCapturedObject(*selected_);
  }
  bool ValidCapturedObject(std::size_t slot) const {
    return slot < gimmicks_.size() &&
           Field_gimmickInstances[slot] == gimmicks_[slot] &&
           ReadableQueryRecord(gimmicks_[slot]);
  }
  bool ValidObject(std::size_t slot) const {
    return Valid() && ValidCapturedObject(slot);
  }
  bool ValidActor(std::int32_t slot) const {
    return slot == static_cast<std::int32_t>(actor_slot_) && Valid();
  }
  std::size_t actor_slot_;
  Actors_ActorRecord* actor_;
  std::uint64_t generation_;
  std::optional<std::size_t> selected_;
  std::array<Field_GimmickInstance*, VS::Field::kGimmickQuerySlots> gimmicks_{};
};
class LiveGimmickInteractionCommandHost final
    : public VS::Field::GimmickInteractionCommandHost {
 public:
  explicit LiveGimmickInteractionCommandHost(Actors_ActorRecord* actor)
      : actor_(actor),
        generation_(VS::Field::LiveInteractionTargetDomain().generation) {
    const auto selected = Field_interactingGimmickId;
    if (selected >= 0 &&
        static_cast<std::size_t>(selected) < VS::Field::kGimmickQuerySlots) {
      object_slot_ = static_cast<std::size_t>(selected);
      object_ = Field_gimmickInstances[*object_slot_];
    }
    for (std::size_t index = 0;
         actor_ != nullptr && index < std::size(Actors_actorRecords); ++index) {
      if (Actors_actorRecords[index] == actor_) {
        actor_slot_ = index;
        break;
      }
    }
  }
  std::optional<VS::Actors::MovementActor> Actor() override {
    if (!ValidActor()) return std::nullopt;
    return VS::Actors::LoadLiveMovementActor(*actor_);
  }
  std::optional<VS::Field::GimmickCommandObject> Object() override {
    if (!ValidObject()) return std::nullopt;
    return VS::Field::GimmickCommandObject{
        {QueryState(*object_), object_->positionX, object_->positionZ,
         object_->slideX, object_->slideZ},
        static_cast<std::uint16_t>(object_->rotationY)};
  }
  std::optional<std::int32_t> Direction() override {
    return ValidDomain()
               ? std::optional<std::int32_t>{Field_gimmickInteractionDirection}
               : std::nullopt;
  }
  bool PreparePush(std::optional<std::int16_t> mode) override {
    if (!ValidObject()) return false;
    if (mode) object_->mode = *mode;
    object_->objectInfo.supportIndex = 0;
    return true;
  }
  bool RemoveOccupant() override {
    if (!ValidObject()) return false;
    VS::Battle::Ai::RemoveLiveAiTileOccupant(object_);
    return ValidObject();
  }
  bool ReleaseTile(const VS::Field::GimmickTileQuery& tile) override {
    constexpr auto kGridSide = VS::Battle::Ai::kNavigationGridSide;
    if (!ValidObject() || Battle_Ai_tileFlagMap == nullptr || tile.tile_x < 0 ||
        tile.tile_z < 0 || tile.tile_x >= kGridSide ||
        tile.tile_z >= kGridSide) {
      return false;
    }
    VS::Battle::Ai::ReleaseLiveAiDestinationTile(tile.tile_x, tile.tile_z);
    return ValidObject();
  }
  bool StoreDropPlacement(const VS::Field::GimmickTileQuery& tile,
                          std::uint16_t yaw) override {
    if (!ValidObject()) return false;
    object_->positionX = VS::Core::Math::WrapSigned16(
        tile.tile_x * kGimmickTileUnit + kGimmickTileCenter);
    object_->positionZ = VS::Core::Math::WrapSigned16(
        tile.tile_z * kGimmickTileUnit + kGimmickTileCenter);
    object_->tileX = static_cast<char>(static_cast<std::uint8_t>(tile.tile_x));
    object_->tileZ = static_cast<char>(static_cast<std::uint8_t>(tile.tile_z));
    object_->rotationY = VS::Core::Math::WrapSigned16(yaw);
    return true;
  }
  std::optional<std::int16_t> ParentBoneHeight(std::uint8_t tag) override {
    if (!ValidObject()) return std::nullopt;
    SVECTOR position{};
    const auto status = VS::Actors::LiveActorTaggedBonePosition(
        object_->parent, tag, &position, 1);
    return status >= 0 && ValidObject()
               ? std::optional<std::int16_t>{position.vy}
               : std::nullopt;
  }
  bool FinishDrop(std::int16_t height) override {
    if (!ValidObject()) return false;
    object_->mode = kGimmickDropMode;
    object_->velocityY = kGimmickDropVelocity;
    object_->parent = kNoParent;
    object_->positionY = height;
    return true;
  }
  bool SetCarrying(bool carrying) override {
    if (!ValidActor()) return false;
    if (carrying) {
      actor_->instance.attachBone =
          static_cast<std::uint8_t>(Field_interactingGimmickId);
    }
    actor_->actionPoseActive = carrying;
    return true;
  }
  bool StoreSlide(std::int16_t x, std::int16_t z) override {
    if (!ValidObject()) return false;
    object_->slideX = x;
    object_->slideZ = z;
    return true;
  }
  bool StoreYaw(std::uint16_t yaw) override {
    if (!ValidObject()) return false;
    object_->rotationY = VS::Core::Math::WrapSigned16(yaw);
    return true;
  }
  bool AttachObject(std::uint8_t actor_slot) override {
    if (!ValidObject()) return false;
    object_->tileX = static_cast<char>(kNoParent);
    object_->mode = kGimmickCarryMode;
    object_->parent = actor_slot;
    return true;
  }
  bool PlayIdleAnimation() override {
    if (!ValidActor()) return false;
    static_cast<void>(VS::Actors::PlayLiveActorAnimation(
        actor_->instance.slot, kGimmickIdleAnimation, 0,
        kGimmickIdleBlendFrames));
    return ValidActor();
  }

 private:
  bool ValidDomain() const {
    return generation_ == VS::Field::LiveInteractionTargetDomain().generation &&
           (!actor_slot_ || Actors_actorRecords[*actor_slot_] == actor_);
  }
  bool ValidActor() const {
    if (!ValidDomain() || actor_ == nullptr ||
        reinterpret_cast<std::uintptr_t>(actor_) %
                alignof(Actors_ActorRecord) !=
            0) {
      return false;
    }
    const auto allocation = VS::Core::Memory::LivePayloadContaining(actor_);
    if (!allocation) {
      return allocation.error() == VS::Core::Memory::HeapError::kOutsideArena;
    }
    const auto at = reinterpret_cast<std::uintptr_t>(actor_);
    const auto start = reinterpret_cast<std::uintptr_t>(allocation->data());
    return at >= start && at - start <= allocation->size() &&
           sizeof(Actors_ActorRecord) <= allocation->size() - (at - start);
  }
  bool ValidObject() const {
    return ValidDomain() && object_slot_ &&
           Field_gimmickInstances[*object_slot_] == object_ &&
           ReadableQueryRecord(object_);
  }
  Actors_ActorRecord* actor_;
  std::uint64_t generation_;
  std::optional<std::size_t> actor_slot_;
  std::optional<std::size_t> object_slot_;
  Field_GimmickInstance* object_ = nullptr;
};
}  // namespace

namespace VS::Field {
bool ApplyLiveGimmickInteractionCommand(Actors_ActorRecord* actor,
                                        std::int32_t mode) {
  LiveGimmickInteractionCommandHost host(actor);
  return ApplyGimmickInteractionCommand(mode, host);
}

bool BeginLiveGimmickInteraction(std::int32_t actor_slot) {
  if (actor_slot < 0 ||
      static_cast<std::size_t>(actor_slot) >= std::size(Actors_actorRecords)) {
    return false;
  }
  LiveGimmickInteractionStartHost host(static_cast<std::size_t>(actor_slot));
  return BeginGimmickInteraction(actor_slot, host);
}

std::optional<std::size_t> FindLiveInteractableGimmickAhead(
    std::int32_t actor_slot) {
  if (actor_slot < 0 ||
      static_cast<std::size_t>(actor_slot) >= std::size(Actors_actorRecords)) {
    return std::nullopt;
  }
  LiveGimmickInteractionQueryHost host(static_cast<std::size_t>(actor_slot));
  const auto result = FindInteractableGimmickAhead(actor_slot, host);
  return result && host.Valid() && host.Gimmick(*result) ? result
                                                         : std::nullopt;
}

bool DropLiveUnsupportedGimmickStack(std::int32_t slot) {
  LiveGimmickStackDropHost host;
  return DropUnsupportedGimmickStack(slot, host);
}

bool CanPlaceLiveGimmickAtPoint(const GimmickPlacementPoint& point) {
  LiveGimmickPointHost host;
  return CanPlaceGimmickAtPoint(point, host);
}

bool SetLiveGimmickOrientation(std::int32_t slot, std::int32_t orientation) {
  if (slot < 0 || static_cast<std::size_t>(slot) >= kGimmickQuerySlots) {
    return false;
  }
  OrientationRows rows{};
  static_assert(sizeof(rows) <= sizeof(Field_kGimmickOrientationRotations));
  std::memcpy(rows.data(), Field_kGimmickOrientationRotations, sizeof(rows));
  LiveGimmickOrientationHost host(static_cast<std::size_t>(slot));
  return SetGimmickOrientation(orientation, rows, host);
}

bool AreLiveGimmicksMoving() {
  return AreGimmicksMoving(SnapshotQueryStates());
}

bool IsLiveGimmickPrefixReadable(const Field_GimmickInstance* record) {
  return ReadableQueryRecord(record);
}

std::optional<GimmickQueryState> LiveGimmickQueryState(std::size_t slot) {
  if (slot >= kGimmickQuerySlots ||
      !ReadableQueryRecord(Field_gimmickInstances[slot])) {
    return std::nullopt;
  }
  return QueryState(*Field_gimmickInstances[slot]);
}

std::optional<std::size_t> FindLiveTopGimmickAtTile(
    const GimmickTileQuery& query, bool require_idle_mode) {
  return FindTopGimmickAtTile(SnapshotQueryStates(), query, require_idle_mode);
}
}  // namespace VS::Field

int Field_FindGimmickBelowEntry(const Field_GimmickInstance* reference) {
  if (!ReadableQueryRecord(reference)) return -1;
  const auto index = VS::Field::FindGimmickBelow(SnapshotQueryStates(),
                                                 QueryState(*reference));
  return index ? static_cast<int>(*index) : -1;
}

int Field_FindTopGimmickAtTileEntry(int tile_x, int tile_z,
                                    int require_idle_mode) {
  const auto index = VS::Field::FindLiveTopGimmickAtTile(
      {tile_x, tile_z}, require_idle_mode != 0);
  return index ? static_cast<int>(*index) : 0;
}

int Field_GimmickSubtypeEntry(int slot) {
  const auto value = VS::Field::GimmickSubtype(SnapshotQueryStates(), slot);
  return value ? static_cast<int>(*value) : -1;
}

int Field_AreGimmicksMovingEntry(void) {
  return VS::Field::AreLiveGimmicksMoving() ? 1 : 0;
}

int Field_MeasureTileStackEntry(int tile_x, int tile_z, int* height_offset) {
  if (height_offset == nullptr) return 0;
  const auto result =
      VS::Field::MeasureTileStack(SnapshotQueryStates(), {tile_x, tile_z});
  std::memcpy(height_offset, &result.height_offset,
              sizeof(result.height_offset));
  return result.interaction_two ? 1 : 0;
}

int Field_IsGimmickWithinHeightBandEntry(int tile_x, int tile_z,
                                         int minimum_height) {
  return VS::Field::IsGimmickWithinHeightBand(SnapshotQueryStates(),
                                              {tile_x, tile_z}, minimum_height)
             ? 1
             : 0;
}

int Field_SetGimmickOrientationEntry(int slot, int orientation) {
  return VS::Field::SetLiveGimmickOrientation(slot, orientation) ? 0 : -1;
}

int Field_CanPlaceGimmickAtPointEntry(int x, int z, int minimum_height) {
  return VS::Field::CanPlaceLiveGimmickAtPoint({x, z, minimum_height}) ? 1 : 0;
}

void Field_DropUnsupportedGimmickStackEntry(int slot) {
  static_cast<void>(VS::Field::DropLiveUnsupportedGimmickStack(slot));
}

int Field_FindInteractableGimmickAheadEntry(int actor_slot) {
  const auto result = VS::Field::FindLiveInteractableGimmickAhead(actor_slot);
  return result ? static_cast<int>(*result) : -1;
}

int Field_BeginGimmickInteractionEntry(int actor_slot) {
  return VS::Field::BeginLiveGimmickInteraction(actor_slot) ? 0 : -1;
}

void Field_ApplyGimmickInteractionCommandEntry(Actors_ActorRecord* actor,
                                               int mode) {
  static_cast<void>(VS::Field::ApplyLiveGimmickInteractionCommand(actor, mode));
}

// decomp: BATTLE.PRG func_800A0024 (src/BATTLE/BATTLE.PRG/30DB0.c:3654).
// Original 800A0034 tests inherited s0 before the slot load at 800A004C.
int Field_GimmickPlacementEntry(int gimmick_slot,
                                Field_GimmickSnapshot* output_snapshot) {
  if (output_snapshot == nullptr || gimmick_slot < 0 ||
      static_cast<std::size_t>(gimmick_slot) >=
          std::size(Field_gimmickInstances)) {
    Core_Warnf("field", "Gimmick snapshot refused: slot=%d output=%p",
               gimmick_slot, static_cast<void*>(output_snapshot));
    return -1;
  }
  LiveGimmickPlacementHost host;
  const auto snapshot = VS::Field::GimmickPlacement(gimmick_slot, host);
  if (!snapshot) {
    return -1;
  }
  *output_snapshot = {snapshot->tile_x,      snapshot->height_above_ground,
                      snapshot->tile_z,      snapshot->orientation,
                      snapshot->position_xy, snapshot->position_z_tail};
  return 0;
}
