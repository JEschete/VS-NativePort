#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Battle/FloatingLabelProducers.hpp"
#include "Battle/FloatingLabelRendering.hpp"
#include "Render/TextureRegionDraw.hpp"

// The floating label updater, the actor value and status label leaves and the
// equipment stat-change feedback (BATTLE.PRG 2842C.c:1757-2310, 2694-2890).
// The updater walks Battle_floatingLabels once per frame: it anchors value and
// status labels to their actor, places value labels apart from each other,
// dispatches each kind's leaf draw and ages and removes the labels. The
// equipment producer queues one stat-change record per changed stat; the
// updater shows the queued records as a stack after they are committed.
namespace VS::Battle {

// Battle_floatingLabels kinds the updater and the equipment feedback own
// beyond the producers' FloatingLabelKind.
inline constexpr std::uint8_t kEquipmentStatChangeKind = 6;
inline constexpr std::uint8_t kPendingEquipmentStatChangeKind = 7;
// The actor byte of an equipment stat-change record.
inline constexpr std::uint8_t kEquipmentStatChangeActor = 0x7F;
// The Battle_placedLabelPositions owner: 32 DVECTORs (0x80 bytes).
inline constexpr std::int32_t kPlacedLabelCapacity = 32;
// Battle_StatusLabelLayout.labels.
inline constexpr std::size_t kStatusLabelCaptions = 2;
// The equipment kind the weapon stat-change setting covers.
inline constexpr std::int32_t kWeaponStatChange = 240;

// One DVECTOR of Battle_placedLabelPositions or a projected screen point.
struct LabelScreenPoint {
  std::int16_t x{};
  std::int16_t y{};
  friend bool operator==(const LabelScreenPoint&,
                         const LabelScreenPoint&) = default;
};

// One Battle_StatusLabelLayout (D_800690B4): up to two caption rectangles of
// Battle_kLabelCaptionRects and the Battle_kStatusLabelColors index.
struct StatusLabelLayout {
  std::uint8_t count{};
  std::array<std::uint8_t, kStatusLabelCaptions> captions{};
  std::uint8_t color{};
};

// Battle_floatingLabels' published records. Label() is nullopt and
// WriteLabel() false for a record outside the queue's owner.
class FloatingLabelRecords {
 public:
  virtual ~FloatingLabelRecords() = default;
  // Battle_floatingLabelCount.
  [[nodiscard]] virtual std::int16_t LabelCount() = 0;
  [[nodiscard]] virtual std::optional<FloatingLabelRecord> Label(
      std::int32_t index) = 0;
  virtual bool WriteLabel(std::int32_t index,
                          const FloatingLabelRecord& record) = 0;
};

// The label queue the equipment producer reserves and publishes through.
class EquipmentStatChangeQueue : public FloatingLabelRecords {
 public:
  // func_800962E4: the reserved record's index, nullopt when refused.
  [[nodiscard]] virtual std::optional<std::int32_t> ReserveLabel() = 0;
  // Publishes the reserved record (++Battle_floatingLabelCount).
  virtual bool CommitLabel(std::int32_t index) = 0;
};

// Battle_placedLabelPositions and Battle_placedLabelCount.
class PlacedLabelPositions {
 public:
  virtual ~PlacedLabelPositions() = default;
  [[nodiscard]] virtual std::int16_t Count() = 0;
  virtual void SetCount(std::int16_t count) = 0;
  [[nodiscard]] virtual std::optional<LabelScreenPoint> Position(
      std::int32_t index) = 0;
  virtual bool SetPosition(std::int32_t index, LabelScreenPoint point) = 0;
};

// The value, status and equipment leaves' tables and draws in addition to the
// shared floating label renderer. DrawQuad (the base) orders a quad on the
// label layer (ot - 20 bytes); DrawEquipmentQuad on the stat-change layer
// (ot + 8 bytes). Each table lookup is nullopt outside its table.
class FloatingLabelFeedbackWorld : public FloatingLabelRenderWorld {
 public:
  // Battle_kStatusLabelLayouts (D_800690B4).
  virtual std::optional<StatusLabelLayout> StatusLayout(
      std::uint16_t status) = 0;
  // Battle_kLabelCaptionRects (D_80068FAC).
  virtual std::optional<FloatingLabelAtlasRect> CaptionRect(
      std::uint8_t caption) = 0;
  // Battle_kStatusLabelColors (D_80069134).
  virtual std::optional<Render::QuadColor> StatusColor(std::uint8_t color) = 0;
  // Battle_kStatChangeFirstWordRects (D_80069144) and
  // Battle_kStatChangeSecondWordRects (D_8006917C).
  virtual std::optional<FloatingLabelAtlasRect> StatChangeFirstWord(
      std::uint8_t word) = 0;
  virtual std::optional<FloatingLabelAtlasRect> StatChangeSecondWord(
      std::uint8_t word) = 0;
  virtual bool DrawEquipmentQuad(const FloatingLabelQuad& quad) = 0;
};

// The leaf the updater dispatches a label to.
enum class FloatingLabelLeaf : std::uint8_t {
  kLocked,               // func_8009506C
  kActorValue,           // func_80094B0C
  kActorStatus,          // func_80095258
  kAbilityTimingResult,  // func_800958A4
  kChainCount,           // func_80095A4C
  kEquipmentStatChange,  // func_8009695C
};

// The updater's services. DrawLeaf draws the published record `index`, which
// the updater has written before the call, at `origin`.
class FloatingLabelUpdateWorld : public FloatingLabelRecords {
 public:
  virtual PlacedLabelPositions& Placed() = 0;
  // func_800A190C: writes the bone's camera-relative position and returns 0
  // when found.
  virtual std::int32_t TaggedBonePosition(std::int32_t actor, std::int32_t tag,
                                          std::int32_t endpoint,
                                          LabelPoint& point) = 0;
  // RotTransPers: writes the screen point and returns the visibility the
  // original tests (positive when in front of the camera).
  virtual std::int32_t Project(const LabelPoint& point,
                               LabelScreenPoint& screen) = 0;
  // func_800970BC.
  virtual bool DrawAbilityTimingCue() = 0;
  virtual bool DrawLeaf(FloatingLabelLeaf leaf, std::int32_t index,
                        FloatingLabelScreenPoint origin) = 0;
  // func_8009639C.
  virtual bool CompactLabels(std::int32_t removal_count) = 0;
};

// The updater's scalar state: the cue countdown (D_800F2272), the status step
// timer (D_800F2278), the frame's tick speed, whether room updates are paused
// (D_800F1BA4) and whether Ashley's status labels are hidden (D_800F227A).
struct FloatingLabelUpdateState {
  std::int16_t& ability_cue_frames;
  std::int16_t& status_step_timer;
  std::int32_t tick_speed = 0;
  bool room_updates_paused = false;
  bool ashley_status_labels_hidden = false;
};

// vs_main_settings.weaponStatChange and armorStatChange.
struct EquipmentStatChangeSettings {
  bool weapon = false;
  bool armor = false;
};

// Each function returns false (or nullopt) when a world service, record or
// table entry is refused; draws and record writes made before the refusal
// remain, as with the original's sequential stores.

// decomp: F00719 func_800941FC (src/BATTLE/BATTLE.PRG/2842C.c:1757).
bool UpdateAndDrawFloatingLabels(FloatingLabelUpdateWorld& world,
                                 FloatingLabelUpdateState state);
// Refuses a placed count outside the 32-position owner before any change.
// decomp: F00720 func_80094844 (src/BATTLE/BATTLE.PRG/2842C.c:1971).
bool PlaceActorValueLabel(LabelScreenPoint& position,
                          FloatingLabelRecord& label,
                          PlacedLabelPositions& placed);
// Refuses a text length beyond the record's four text bytes.
// decomp: F00722 func_80094B0C (src/BATTLE/BATTLE.PRG/2842C.c:2073).
bool DrawActorValueLabel(FloatingLabelFeedbackWorld& world,
                         FloatingLabelScreenPoint origin,
                         const FloatingLabelRecord& label);
// Refuses a layout with more than two captions and a closing frame past the
// four-step slide and fade tables, before any draw.
// decomp: F00725 func_80095258 (src/BATTLE/BATTLE.PRG/2842C.c:2210).
bool DrawActorStatusLabel(FloatingLabelFeedbackWorld& world,
                          FloatingLabelScreenPoint origin,
                          const FloatingLabelRecord& label);
// Returns 0, as the original always does.
// decomp: F00738 func_80096768 (src/BATTLE/BATTLE.PRG/2842C.c:2694).
std::optional<std::int32_t> QueueEquipmentStatChange(
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): original order.
    std::int32_t equipment_kind, std::int32_t stat_label,
    std::uint32_t encoded_amount, EquipmentStatChangeSettings settings,
    EquipmentStatChangeQueue& queue);
// Refuses a text length beyond the record's four text bytes, before any draw.
// decomp: F00739 func_8009695C (src/BATTLE/BATTLE.PRG/2842C.c:2765).
bool DrawEquipmentStatChange(FloatingLabelFeedbackWorld& world,
                             FloatingLabelScreenPoint origin,
                             const FloatingLabelRecord& label);
// decomp: F00742 func_800971D4 (src/BATTLE/BATTLE.PRG/2842C.c:2881).
bool CommitEquipmentStatChanges(FloatingLabelRecords& records);
}  // namespace VS::Battle
