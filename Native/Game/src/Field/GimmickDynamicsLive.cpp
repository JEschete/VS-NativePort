#include "Field/GimmickDynamicsLive.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>

#include "Audio/SfxLive.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/ActorCollisionLive.hpp"
#include "Field/GimmickDynamics.hpp"
#include "Field/GimmickDynamicsLive.hpp"
#include "Field/GimmickPlacementLive.hpp"
#include "Field/GimmickReactionsLive.hpp"
#include "Field/InteractionTargetLive.hpp"

extern "C" {
#include "Battle/Ai/Navigation.h"
#include "Field/Gimmicks.h"
}

namespace {
using Record = Field_GimmickInstance;
using PhysicsView = Field_GimmickPhysicsView;
constexpr unsigned kOccupancyCellBytes = 2;
constexpr unsigned kLayerShift = 2;
constexpr unsigned kOccupancySide = VS::Battle::Ai::kNavigationGridSide;
constexpr unsigned kOccupancyRowBytes = kOccupancyCellBytes * kOccupancySide;
constexpr unsigned kOccupancyBytes = kOccupancyRowBytes * kOccupancySide;
constexpr int kLandingSoundGroup = 0x7E;
static_assert(offsetof(PhysicsView, delay) == offsetof(Record, delay));
static_assert(offsetof(PhysicsView, mode) == offsetof(Record, mode));
static_assert(offsetof(PhysicsView, position) == offsetof(Record, positionX));
static_assert(offsetof(PhysicsView, rotationRemainingX) ==
              offsetof(Record, rotationDeltaX));
static_assert(offsetof(PhysicsView, tileX) == offsetof(Record, tileX));
static_assert(offsetof(PhysicsView, nativePointer68) ==
              offsetof(Record, model));
static_assert(offsetof(PhysicsView, material) == offsetof(Record, objectInfo));

VS::Field::GimmickDynamicsState LoadState(const Record& record) {
  return {
      .reaction = {record.objectInfo.supportIndex,
                   record.slot,
                   record.objectInfo.material,
                   record.mode,
                   record.orientation,
                   {record.positionX, record.positionY, record.positionZ},
                   {record.rotationX,
                    VS::Core::Math::WrapSigned16(record.rotationY),
                    VS::Core::Math::WrapSigned16(record.rotationZ)},
                   {record.scaleX, record.scaleY, record.scaleZ},
                   record.velocityY},
      .delay = static_cast<std::uint8_t>(record.delay),
      .rotation_ticks = record.rotationTicks,
      .rotation_remaining = {record.rotationDeltaX, record.rotationDeltaY,
                             record.rotationDeltaZ},
      .slide = {record.slideX, record.slideZ},
      .tile_x = static_cast<std::uint8_t>(record.tileX),
      .tile_z = static_cast<std::uint8_t>(record.tileZ),
      .parent = record.parent,
      .carrier = record.objectInfo.carrier,
      .kind = static_cast<std::uint8_t>(record.objectInfo.objectId),
      .durability = record.objectInfo.subtype,
      .interaction = static_cast<std::uint8_t>(record.objectInfo.interaction),
      .skipped = record.isSkipped != 0};
}

class LiveDynamicsHost final : public VS::Field::GimmickDynamicsHost {
 public:
  explicit LiveDynamicsHost(Record* record)
      : record_(record),
        generation_(VS::Field::LiveInteractionTargetDomain().generation) {
    std::copy_n(Field_gimmickInstances, records_.size(), records_.begin());
    for (std::size_t slot = 0; slot < records_.size(); ++slot) {
      if (records_[slot] == record_) {
        table_slot_ = slot;
        break;
      }
    }
  }
  std::optional<VS::Field::GimmickDynamicsState> State() override {
    return Valid() ? std::optional<VS::Field::GimmickDynamicsState>{LoadState(
                         *record_)}
                   : std::nullopt;
  }
  bool Store(const VS::Field::GimmickDynamicsState& state) override {
    if (!Valid()) return false;
    record_->delay = std::bit_cast<std::int8_t>(state.delay);
    record_->rotationTicks = state.rotation_ticks;
    record_->rotationDeltaX = state.rotation_remaining[0];
    record_->rotationDeltaY = state.rotation_remaining[1];
    record_->rotationDeltaZ = state.rotation_remaining[2];
    record_->mode = state.reaction.mode;
    record_->positionX = state.reaction.position[0];
    record_->positionY = state.reaction.position[1];
    record_->positionZ = state.reaction.position[2];
    record_->rotationX = state.reaction.rotation[0];
    record_->rotationY = static_cast<std::uint16_t>(state.reaction.rotation[1]);
    record_->rotationZ = static_cast<std::uint16_t>(state.reaction.rotation[2]);
    record_->scaleX = state.reaction.scale[0];
    record_->scaleY = state.reaction.scale[1];
    record_->scaleZ = state.reaction.scale[2];
    record_->slideX = state.slide[0];
    record_->slideZ = state.slide[1];
    record_->velocityY = state.reaction.vertical_delta;
    record_->tileX = std::bit_cast<char>(state.tile_x);
    record_->tileZ = std::bit_cast<char>(state.tile_z);
    record_->parent = state.parent;
    record_->objectInfo.carrier = state.carrier;
    record_->objectInfo.subtype = state.durability;
    record_->objectInfo.interaction = state.interaction;
    record_->objectInfo.supportIndex = state.reaction.frame;
    return true;
  }
  VS::Field::GimmickReactionResult React() override {
    if (!Valid()) return VS::Field::GimmickReactionResult::kRefused;
    return VS::Field::UpdateLiveGimmickReactionAnimation(record_);
  }
  bool PointAllowed(const VS::Field::GimmickPlacementPoint& point) override {
    return Valid() && VS::Field::CanPlaceLiveGimmickAtPoint(point) && Valid();
  }
  std::optional<std::int32_t> SupportHeight(
      const std::array<std::int16_t, VS::Field::kGimmickReactionAxes>& position,
      std::int32_t mode) override {
    if (!Valid()) return std::nullopt;
    SVECTOR origin{};
    origin.vx = position[0];
    origin.vy = position[1];
    origin.vz = position[2];
    const auto height =
        VS::Field::LiveSampleActorSupportHeight(&origin, 0, 0, mode);
    return Valid() ? std::optional<std::int32_t>{height} : std::nullopt;
  }
  std::optional<std::uint8_t> TopKind(
      const VS::Field::GimmickTileQuery& tile) override {
    if (!Valid()) return std::nullopt;
    const auto slot = VS::Field::FindLiveTopGimmickAtTile(tile, true);
    if (!Valid() || !slot || *slot == 0 || !ValidChild(*slot)) {
      return std::nullopt;
    }
    return static_cast<std::uint8_t>(records_[*slot]->objectInfo.objectId);
  }
  bool AddOccupant() override {
    if (!Valid() || !ValidOccupancy()) return false;
    VS::Battle::Ai::AddLiveAiTileOccupant(record_);
    return Valid();
  }
  bool ReleaseTile(const VS::Field::GimmickTileQuery& tile) override {
    if (!Valid()) return false;
    if (Battle_Ai_tileFlagMap != nullptr &&
        (tile.tile_x < 0 || tile.tile_z < 0 ||
         tile.tile_x >= VS::Battle::Ai::kNavigationGridSide ||
         tile.tile_z >= VS::Battle::Ai::kNavigationGridSide)) {
      return false;
    }
    VS::Battle::Ai::ReleaseLiveAiDestinationTile(tile.tile_x, tile.tile_z);
    return Valid();
  }
  bool PlaySound(
      std::uint16_t effect,
      const std::array<std::int16_t, VS::Field::kGimmickReactionAxes>& position)
      override {
    if (!Valid()) return false;
    SVECTOR point{};
    point.vx = position[0];
    point.vy = position[1];
    point.vz = position[2];
    VS::Audio::LivePlayWorldSfx(kLandingSoundGroup, effect, &point);
    return Valid();
  }
  std::optional<VS::Field::GimmickDynamicsState> Child(
      std::size_t slot) override {
    return ValidChild(slot)
               ? std::optional<VS::Field::GimmickDynamicsState>{LoadState(
                     *records_[slot])}
               : std::nullopt;
  }
  bool StoreChild(std::size_t slot, std::int16_t height,
                  std::uint8_t carrier) override {
    if (!Valid() || !ValidChild(slot)) return false;
    records_[slot]->positionY = height;
    records_[slot]->objectInfo.carrier = carrier;
    return Valid();
  }

 private:
  bool SameRoom() const {
    return generation_ == VS::Field::LiveInteractionTargetDomain().generation;
  }
  bool Valid() const {
    return SameRoom() && VS::Field::IsLiveGimmickPrefixReadable(record_) &&
           (!table_slot_ || Field_gimmickInstances[*table_slot_] == record_);
  }
  bool ValidChild(std::size_t slot) const {
    return SameRoom() && slot < records_.size() &&
           Field_gimmickInstances[slot] == records_[slot] &&
           VS::Field::IsLiveGimmickPrefixReadable(records_[slot]);
  }
  bool ValidOccupancy() const {
    if (Battle_Ai_tileFlagMap == nullptr) return true;
    if (Battle_Ai_specialTerrainHeights == nullptr) return false;
    const auto offset =
        static_cast<unsigned>(static_cast<std::uint8_t>(record_->tileX)) *
            kOccupancyCellBytes +
        (static_cast<std::uint8_t>(record_->tileLayer) >> kLayerShift) +
        static_cast<unsigned>(static_cast<std::uint8_t>(record_->tileZ)) *
            kOccupancyRowBytes;
    return offset <= kOccupancyBytes - kOccupancyCellBytes &&
           offset % kOccupancyCellBytes == 0;
  }
  Record* record_;
  std::uint64_t generation_;
  std::optional<std::size_t> table_slot_;
  std::array<Record*, VS::Field::kGimmickQuerySlots> records_{};
};
}  // namespace

namespace VS::Field {
bool UpdateLiveGimmickMotion(Field_GimmickInstance* record) {
  LiveDynamicsHost host(record);
  return UpdateGimmickMotion(host);
}
}  // namespace VS::Field

void Field_UpdateGimmickMotionEntry(Field_GimmickPhysicsView* record, int) {
  static_cast<void>(VS::Field::UpdateLiveGimmickMotion(
      reinterpret_cast<Field_GimmickInstance*>(record)));
}
