#include "Field/GimmickDynamicsLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>

#include "Audio/SfxLive.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/ActorCollisionLive.hpp"
#include "Field/GimmickDynamics.hpp"
#include "Field/GimmickPlacementLive.hpp"
#include "Field/GimmickReactionsLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/InteractionTargetLive.hpp"

namespace {
using Record = VS::Field::GimmickInstance;
using PhysicsView = VS::Field::GimmickPhysicsView;
constexpr unsigned kOccupancyCellBytes = 2;
constexpr unsigned kLayerShift = 2;
constexpr unsigned kOccupancySide = VS::Battle::Ai::kNavigationGridSide;
constexpr unsigned kOccupancyRowBytes = kOccupancyCellBytes * kOccupancySide;
constexpr unsigned kOccupancyBytes = kOccupancyRowBytes * kOccupancySide;
constexpr int kLandingSoundGroup = 0x7E;
static_assert(offsetof(PhysicsView, delay) == offsetof(Record, delay));
static_assert(offsetof(PhysicsView, mode) == offsetof(Record, mode));
static_assert(offsetof(PhysicsView, position) == offsetof(Record, position_x));
static_assert(offsetof(PhysicsView, rotation_remaining_x) ==
              offsetof(Record, rotation_delta_x));
static_assert(offsetof(PhysicsView, tile_x) == offsetof(Record, tile_x));
static_assert(offsetof(PhysicsView, native_pointer68) ==
              offsetof(Record, model));
static_assert(offsetof(PhysicsView, material) == offsetof(Record, object_info));

VS::Field::GimmickDynamicsState LoadState(const Record& record) {
  return {
      .reaction = {record.object_info.support_index,
                   record.slot,
                   record.object_info.material,
                   record.mode,
                   record.orientation,
                   {record.position_x, record.position_y, record.position_z},
                   {record.rotation_x,
                    VS::Core::Math::WrapSigned16(record.rotation_y),
                    VS::Core::Math::WrapSigned16(record.rotation_z)},
                   {record.scale_x, record.scale_y, record.scale_z},
                   record.velocity_y},
      .delay = static_cast<std::uint8_t>(record.delay),
      .rotation_ticks = record.rotation_ticks,
      .rotation_remaining = {record.rotation_delta_x, record.rotation_delta_y,
                             record.rotation_delta_z},
      .slide = {record.slide_x, record.slide_z},
      .tile_x = static_cast<std::uint8_t>(record.tile_x),
      .tile_z = static_cast<std::uint8_t>(record.tile_z),
      .parent = record.parent,
      .carrier = record.object_info.carrier,
      .kind = static_cast<std::uint8_t>(record.object_info.object_id),
      .durability = record.object_info.subtype,
      .interaction = static_cast<std::uint8_t>(record.object_info.interaction),
      .skipped = record.is_skipped != 0};
}

class LiveDynamicsHost final : public VS::Field::GimmickDynamicsHost {
 public:
  explicit LiveDynamicsHost(Record* record)
      : record_(record),
        generation_(VS::Field::LiveInteractionTargetDomain().generation) {
    std::copy_n(VS::Field::gimmickInstances.begin(), records_.size(),
                records_.begin());
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
    record_->rotation_ticks = state.rotation_ticks;
    record_->rotation_delta_x = state.rotation_remaining[0];
    record_->rotation_delta_y = state.rotation_remaining[1];
    record_->rotation_delta_z = state.rotation_remaining[2];
    record_->mode = state.reaction.mode;
    record_->position_x = state.reaction.position[0];
    record_->position_y = state.reaction.position[1];
    record_->position_z = state.reaction.position[2];
    record_->rotation_x = state.reaction.rotation[0];
    record_->rotation_y =
        static_cast<std::uint16_t>(state.reaction.rotation[1]);
    record_->rotation_z =
        static_cast<std::uint16_t>(state.reaction.rotation[2]);
    record_->scale_x = state.reaction.scale[0];
    record_->scale_y = state.reaction.scale[1];
    record_->scale_z = state.reaction.scale[2];
    record_->slide_x = state.slide[0];
    record_->slide_z = state.slide[1];
    record_->velocity_y = state.reaction.vertical_delta;
    record_->tile_x = std::bit_cast<char>(state.tile_x);
    record_->tile_z = std::bit_cast<char>(state.tile_z);
    record_->parent = state.parent;
    record_->object_info.carrier = state.carrier;
    record_->object_info.subtype = state.durability;
    record_->object_info.interaction = state.interaction;
    record_->object_info.support_index = state.reaction.frame;
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
    return static_cast<std::uint8_t>(records_[*slot]->object_info.object_id);
  }
  bool AddOccupant() override {
    if (!Valid() || !ValidOccupancy()) return false;
    VS::Battle::Ai::AddLiveAiTileOccupant(record_);
    return Valid();
  }
  bool ReleaseTile(const VS::Field::GimmickTileQuery& tile) override {
    if (!Valid()) return false;
    if (VS::Battle::Ai::tileFlagMap != nullptr &&
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
    records_[slot]->position_y = height;
    records_[slot]->object_info.carrier = carrier;
    return Valid();
  }

 private:
  bool SameRoom() const {
    return generation_ == VS::Field::LiveInteractionTargetDomain().generation;
  }
  bool Valid() const {
    return SameRoom() && VS::Field::IsLiveGimmickPrefixReadable(record_) &&
           (!table_slot_ ||
            VS::Field::gimmickInstances[*table_slot_] == record_);
  }
  bool ValidChild(std::size_t slot) const {
    return SameRoom() && slot < records_.size() &&
           VS::Field::gimmickInstances[slot] == records_[slot] &&
           VS::Field::IsLiveGimmickPrefixReadable(records_[slot]);
  }
  bool ValidOccupancy() const {
    if (VS::Battle::Ai::tileFlagMap == nullptr) return true;
    if (VS::Battle::Ai::specialTerrainHeights == nullptr) return false;
    const auto offset =
        static_cast<unsigned>(static_cast<std::uint8_t>(record_->tile_x)) *
            kOccupancyCellBytes +
        (static_cast<std::uint8_t>(record_->tile_layer) >> kLayerShift) +
        static_cast<unsigned>(static_cast<std::uint8_t>(record_->tile_z)) *
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
bool UpdateLiveGimmickMotion(VS::Field::GimmickInstance* record) {
  LiveDynamicsHost host(record);
  return UpdateGimmickMotion(host);
}
}  // namespace VS::Field

void VS::Field::UpdateGimmickMotionEntry(VS::Field::GimmickPhysicsView* record,
                                         int) {
  static_cast<void>(VS::Field::UpdateLiveGimmickMotion(
      reinterpret_cast<VS::Field::GimmickInstance*>(record)));
}
