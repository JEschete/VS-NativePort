// Hand-maintained (tools/port/owned.csv).
#include "Battle/FloatingLabelPlacementLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

#include "Actors/SkeletonLive.hpp"
#include "Battle/Feedback.h"
#include "Battle/FloatingLabelPlacement.hpp"
#include "Battle/FloatingLabelProducers.hpp"
#include "Battle/FloatingLabelQueue.hpp"
#include "Battle/FloatingLabelQueueLive.hpp"
#include "Battle/FloatingLabelRendering.hpp"
#include "Battle/FloatingLabelRenderingLive.hpp"
#include "Core/Gte.h"
#include "Core/Log.hpp"
#include "Field/RoomRenderingLiveState.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Render/TextureRegionDraw.hpp"
#include "State/Timers.hpp"

namespace VS::Battle {
namespace {
static_assert(sizeof(FloatingLabelRecord) == sizeof(Battle_FloatingLabel));
static_assert(std::is_trivially_copyable_v<Battle_FloatingLabel>);
static_assert(sizeof(LabelPoint) == sizeof(SVECTOR));
static_assert(sizeof(LabelScreenPoint) == sizeof(DVECTOR));

// The generated BATTLE.PRG tables' extents (D_80068FAC, D_800690B4,
// D_80069134, D_80069144 and D_8006917C).
constexpr std::size_t kCaptionRects = 33;
constexpr std::size_t kStatusLayouts = 32;
constexpr std::size_t kStatusColors = 4;
constexpr std::size_t kFirstWords = 7;
constexpr std::size_t kSecondWords = 13;
// The equipment stat-change layer: ot + 8 bytes (two entries past the head).
constexpr int kEquipmentLayer = 2;
constexpr std::uint32_t kHalfMask = 0xFFFF;
constexpr unsigned kHalfShift = 16;

std::uint64_t refusals = 0;

void Refuse(std::string_view what) {
  if (std::has_single_bit(++refusals)) {
    VS_LOG_WARN("battle.labels",
                "Floating label placement refused: {} ({} refusals)", what,
                refusals);
  }
}

std::optional<FloatingLabelAtlasRect> Atlas(const RECT* table,
                                            std::size_t count,
                                            std::size_t index) {
  if (index >= count) return std::nullopt;
  const auto& rect = std::span(table, count)[index];
  return FloatingLabelAtlasRect{rect.x, rect.y, rect.w, rect.h};
}

class LiveFeedbackWorld final : public FloatingLabelFeedbackWorld {
 public:
  std::optional<FloatingLabelAtlasRect> ValueCaption(
      std::uint8_t raw_mode) override {
    return renderer_.ValueCaption(raw_mode);
  }
  std::optional<std::int16_t> LockedHalfHeight(std::int32_t step) override {
    return renderer_.LockedHalfHeight(step);
  }
  std::optional<FloatingLabelAtlasRect> LockedPiece(
      std::size_t piece) override {
    return renderer_.LockedPiece(piece);
  }
  bool DrawQuad(const FloatingLabelQuad& quad) override {
    return renderer_.DrawQuad(quad);
  }
  bool DrawText(std::string_view text, FloatingLabelScreenPoint origin,
                std::uint32_t color) override {
    return renderer_.DrawText(text, origin, color);
  }
  std::optional<StatusLabelLayout> StatusLayout(std::uint16_t status) override {
    if (status >= kStatusLayouts) return std::nullopt;
    const auto& layout =
        std::span(Battle_kStatusLabelLayouts, kStatusLayouts)[status];
    return StatusLabelLayout{.count = layout.count,
                             .captions = {layout.labels[0], layout.labels[1]},
                             .color = layout.spacing};
  }
  std::optional<FloatingLabelAtlasRect> CaptionRect(
      std::uint8_t caption) override {
    return Atlas(Battle_kLabelCaptionRects, kCaptionRects, caption);
  }
  std::optional<Render::QuadColor> StatusColor(std::uint8_t color) override {
    if (color >= kStatusColors) return std::nullopt;
    const auto& entry =
        std::span(Battle_kStatusLabelColors, kStatusColors)[color];
    return Render::QuadColor{entry.r, entry.g, entry.b};
  }
  std::optional<FloatingLabelAtlasRect> StatChangeFirstWord(
      std::uint8_t word) override {
    return Atlas(Battle_kStatChangeFirstWordRects, kFirstWords, word);
  }
  std::optional<FloatingLabelAtlasRect> StatChangeSecondWord(
      std::uint8_t word) override {
    return Atlas(Battle_kStatChangeSecondWordRects, kSecondWords, word);
  }
  bool DrawEquipmentQuad(const FloatingLabelQuad& quad) override {
    return DrawLiveFloatingLabelQuad(quad, kEquipmentLayer);
  }

 private:
  FloatingLabelRenderWorld& renderer_ = LiveFloatingLabelRenderer();
};

Battle_FloatingLabel* LiveLabel(std::int32_t index) {
  if (Battle_floatingLabels == nullptr || index < 0 ||
      index >= kFloatingLabelCapacity) {
    return nullptr;
  }
  return &(*Battle_floatingLabels)[index];
}

// The record a leaf entry was given, when it is one of the current owner's.
std::optional<FloatingLabelRecord> OwnedRecord(
    const Battle_FloatingLabel* label) {
  if (Battle_floatingLabels != nullptr && label != nullptr) {
    for (const auto& current : *Battle_floatingLabels) {
      if (&current == label) return std::bit_cast<FloatingLabelRecord>(current);
    }
  }
  return std::nullopt;
}

class LiveLabelRecords : public EquipmentStatChangeQueue {
 public:
  std::int16_t LabelCount() override { return LiveFloatingLabelCount(); }
  std::optional<FloatingLabelRecord> Label(std::int32_t index) override {
    const auto* label = LiveLabel(index);
    if (label == nullptr) {
      Refuse("label outside the 32-record owner");
      return std::nullopt;
    }
    return std::bit_cast<FloatingLabelRecord>(*label);
  }
  bool WriteLabel(std::int32_t index,
                  const FloatingLabelRecord& record) override {
    auto* label = LiveLabel(index);
    if (label == nullptr) {
      Refuse("label write outside the 32-record owner");
      return false;
    }
    *label = std::bit_cast<Battle_FloatingLabel>(record);
    return true;
  }
  std::optional<std::int32_t> ReserveLabel() override {
    const auto* reserved = ReserveLiveFloatingLabel();
    for (std::int32_t index = 0; index < kFloatingLabelCapacity; ++index) {
      if (LiveLabel(index) == reserved) return index;
    }
    Refuse("reserved label outside the 32-record owner");
    return std::nullopt;
  }
  bool CommitLabel(std::int32_t index) override {
    const auto* label = LiveLabel(index);
    if (label == nullptr) {
      Refuse("commit outside the 32-record owner");
      return false;
    }
    CommitLiveFloatingLabel(label);
    return true;
  }
};

class LivePlacedPositions final : public PlacedLabelPositions {
 public:
  std::int16_t Count() override { return Battle_placedLabelCount; }
  void SetCount(std::int16_t count) override {
    Battle_placedLabelCount = count;
  }
  std::optional<LabelScreenPoint> Position(std::int32_t index) override {
    const auto* placed = Slot(index);
    if (placed == nullptr) return std::nullopt;
    return LabelScreenPoint{placed->vx, placed->vy};
  }
  bool SetPosition(std::int32_t index, LabelScreenPoint point) override {
    auto* placed = Slot(index);
    if (placed == nullptr) return false;
    placed->vx = point.x;
    placed->vy = point.y;
    return true;
  }

 private:
  static DVECTOR* Slot(std::int32_t index) {
    if (Battle_placedLabelPositions == nullptr || index < 0 ||
        index >= kPlacedLabelCapacity) {
      Refuse("placed position outside the 32-position owner");
      return nullptr;
    }
    return &std::span(Battle_placedLabelPositions,
                      kPlacedLabelCapacity)[static_cast<std::size_t>(index)];
  }
};

bool DrawFeedbackLeaf(FloatingLabelLeaf leaf, FloatingLabelScreenPoint origin,
                      const FloatingLabelRecord& record) {
  LiveFeedbackWorld world;
  switch (leaf) {
    case FloatingLabelLeaf::kActorValue:
      return DrawActorValueLabel(world, origin, record);
    case FloatingLabelLeaf::kActorStatus:
      return DrawActorStatusLabel(world, origin, record);
    case FloatingLabelLeaf::kEquipmentStatChange:
      return DrawEquipmentStatChange(world, origin, record);
    default:
      return false;
  }
}

class LiveUpdateWorld final : public FloatingLabelUpdateWorld {
 public:
  std::int16_t LabelCount() override { return records_.LabelCount(); }
  std::optional<FloatingLabelRecord> Label(std::int32_t index) override {
    return records_.Label(index);
  }
  bool WriteLabel(std::int32_t index,
                  const FloatingLabelRecord& record) override {
    return records_.WriteLabel(index, record);
  }
  PlacedLabelPositions& Placed() override { return placed_; }
  std::int32_t TaggedBonePosition(std::int32_t actor, std::int32_t tag,
                                  std::int32_t endpoint,
                                  LabelPoint& point) override {
    auto output = std::bit_cast<SVECTOR>(point);
    const int status =
        Actors::LiveActorTaggedBonePosition(actor, tag, &output, endpoint);
    point = std::bit_cast<LabelPoint>(output);
    return status;
  }
  std::int32_t Project(const LabelPoint& point,
                       LabelScreenPoint& screen) override {
    auto input = std::bit_cast<SVECTOR>(point);
    Core_GteWord packed = 0;
    Core_GteWord depth = 0;
    Core_GteWord flag = 0;
    const auto visible = static_cast<std::int32_t>(
        Core_Gte_RotTransPers(&input, &packed, &depth, &flag));
    const auto word = static_cast<std::uint32_t>(packed);
    screen = {static_cast<std::int16_t>(word & kHalfMask),
              static_cast<std::int16_t>(word >> kHalfShift)};
    return visible;
  }
  bool DrawAbilityTimingCue() override {
    DrawLiveAbilityTimingCue();
    return true;
  }
  bool DrawLeaf(FloatingLabelLeaf leaf, std::int32_t index,
                FloatingLabelScreenPoint origin) override {
    auto* label = LiveLabel(index);
    if (label == nullptr) return false;
    switch (leaf) {
      case FloatingLabelLeaf::kLocked:
        DrawLiveLockedFloatingLabel(origin.x, origin.y, label);
        return true;
      case FloatingLabelLeaf::kAbilityTimingResult:
        DrawLiveAbilityTimingResult(origin.x, origin.y, label);
        return true;
      case FloatingLabelLeaf::kChainCount:
        DrawLiveChainCount(origin.x, origin.y, label);
        return true;
      default:
        return DrawFeedbackLeaf(leaf, origin,
                                std::bit_cast<FloatingLabelRecord>(*label));
    }
  }
  bool CompactLabels(std::int32_t removal_count) override {
    CompactLiveFloatingLabels(removal_count);
    return true;
  }

 private:
  LiveLabelRecords records_;
  LivePlacedPositions placed_;
};

void DrawLeafEntry(FloatingLabelLeaf leaf, int x, int y,
                   const Battle_FloatingLabel* label, std::string_view what) {
  const auto record = OwnedRecord(label);
  if (!record) {
    Refuse("leaf record outside the 32-record owner");
    return;
  }
  if (!DrawFeedbackLeaf(leaf, {x, y}, record.value())) Refuse(what);
}
}  // namespace

// decomp: F00719 func_800941FC (src/BATTLE/BATTLE.PRG/2842C.c:1757).
void LiveUpdateAndDrawFloatingLabels() {
  LiveUpdateWorld world;
  const FloatingLabelUpdateState state{
      .ability_cue_frames = Battle_abilityTimingCueFrames,
      .status_step_timer = Battle_statusLabelStepTimer,
      .tick_speed = VS::State::tickSpeed,
      .room_updates_paused = VS::Field::areRoomUpdatesPaused != 0,
      .ashley_status_labels_hidden = Battle_ashleyStatusLabelsSuppressed != 0};
  if (!UpdateAndDrawFloatingLabels(world, state)) {
    Refuse("floating label update");
  }
}

// decomp: F00720 func_80094844 (src/BATTLE/BATTLE.PRG/2842C.c:1971).
void LivePlaceActorValueLabel(std::int16_t* position,
                              Battle_FloatingLabel* label) {
  auto record = OwnedRecord(label);
  if (position == nullptr || !record) {
    Refuse("value label placement without its position or record");
    return;
  }
  const auto point = std::span(position, 2);
  LabelScreenPoint screen{point[0], point[1]};
  LivePlacedPositions placed;
  if (!PlaceActorValueLabel(screen, record.value(), placed)) {
    Refuse("value label placement");
    return;
  }
  point[0] = screen.x;
  point[1] = screen.y;
  *label = std::bit_cast<Battle_FloatingLabel>(record.value());
}

// decomp: F00722 func_80094B0C (src/BATTLE/BATTLE.PRG/2842C.c:2073).
void LiveDrawActorValueLabel(int x, int y, Battle_FloatingLabel* label) {
  DrawLeafEntry(FloatingLabelLeaf::kActorValue, x, y, label,
                "actor value label");
}

// decomp: F00725 func_80095258 (src/BATTLE/BATTLE.PRG/2842C.c:2210).
void LiveDrawActorStatusLabel(int x, int y, Battle_FloatingLabel* label) {
  DrawLeafEntry(FloatingLabelLeaf::kActorStatus, x, y, label,
                "actor status label");
}

// decomp: F00738 func_80096768 (src/BATTLE/BATTLE.PRG/2842C.c:2694).
int LiveQueueEquipmentStatChange(int equipment_kind, int stat_label_kind,
                                 int encoded_amount) {
  LiveLabelRecords queue;
  const EquipmentStatChangeSettings settings{
      .weapon = VS::Input::settings.weaponStatChange != 0,
      .armor = VS::Input::settings.armorStatChange != 0};
  const auto result = QueueEquipmentStatChange(
      equipment_kind, stat_label_kind,
      static_cast<std::uint32_t>(encoded_amount), settings, queue);
  if (!result) {
    Refuse("equipment stat change");
    return 0;
  }
  return result.value();
}

// decomp: F00739 func_8009695C (src/BATTLE/BATTLE.PRG/2842C.c:2765).
void LiveDrawEquipmentStatChange(int x, int y, Battle_FloatingLabel* label) {
  DrawLeafEntry(FloatingLabelLeaf::kEquipmentStatChange, x, y, label,
                "equipment stat change label");
}

// decomp: F00742 func_800971D4 (src/BATTLE/BATTLE.PRG/2842C.c:2881).
void LiveCommitEquipmentStatChanges() {
  LiveLabelRecords records;
  if (!CommitEquipmentStatChanges(records)) {
    Refuse("equipment stat change commit");
  }
}
}  // namespace VS::Battle
