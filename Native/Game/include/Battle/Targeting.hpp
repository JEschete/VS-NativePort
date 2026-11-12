#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "Battle/TargetingDraw.hpp"

// Typed action targeting (#276 row 31): choosing what an action can hit. An
// action's target category decides whether the player picks an actor, a body
// part or a ground point, or whether the action hits everything around its
// source. The collectors fill the candidate previews inside the action's range
// or area volume, the targeting sphere moves its cursor from pad input, the
// target list sorts candidates into two columns and describes the selected
// one, and the confirmed choice becomes the action's hit list. The action
// state, actors, room, sphere and menus go through an ActionTargetingHost or a
// TargetSelectionHost.
namespace VS::Battle {
// The previews and hit targets an action can hold.
inline constexpr int kTargetPreviewCapacity = 255;
inline constexpr int kSelectionTargetCapacity = 255;
// The actor slots the collectors scan; ground-area actions scan fewer.
inline constexpr int kTargetableActorSlots = 16;
inline constexpr int kGroundAreaActorSlots = 10;
// The body parts an actor can expose.
inline constexpr int kTargetLimbCount = 6;
// The preview kind of a whole actor (no body part).
inline constexpr int kWholeActorTargetKind = 16;
// A preview limb that names no body part.
inline constexpr int kNoTargetLimb = -1;
// selectedPreview and cameraPreview when nothing is chosen.
inline constexpr int kNoPreviewSelected = 0xFF;
// An encoded target id from this value up names a position, not an actor; the
// chest bit marks a treasure chest among them.
inline constexpr int kFirstPositionTargetId = 16;
inline constexpr int kChestTargetFlag = 0x20;
// The sphere's target labels, list columns and their groups.
inline constexpr int kTargetLabelCount = 24;
inline constexpr int kTargetColumnCapacity = 16;
inline constexpr int kTargetColumnGroupCapacity = 8;
inline constexpr int kTargetGroupCount = 16;
// The limb names a target label can show (Battle_limbNames).
inline constexpr int kTargetLimbNameCount = 20;

// A position with its fourth halfword (SVECTOR order).
struct TargetPoint {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
  std::int16_t pad = 0;

  friend bool operator==(const TargetPoint&, const TargetPoint&) = default;
};

// An actor hit target: the actor slot and the body part it strikes.
struct ActorLimbTarget {
  int actor = 0;
  int limb = 0;

  friend bool operator==(const ActorLimbTarget&,
                         const ActorLimbTarget&) = default;
};

// One target candidate (Battle_TargetPreview): what the list shows and what
// the hit list is built from. Fields keep the original byte widths.
struct ActionTargetPreview {
  TargetPoint position{};
  std::uint8_t kind = 0;
  std::uint8_t actor_id = 0;
  std::int8_t limb = 0;
  std::uint8_t effect = 0;
  std::uint16_t preview_amount = 0;
  std::int16_t hit_chance = 0;
  std::uint32_t removed_statuses = 0;
  std::uint8_t action_kind = 0;
  std::uint8_t damage_type = 0;
  std::uint8_t effect_variant = 0;
  std::uint8_t enemy_class = 0;

  friend bool operator==(const ActionTargetPreview&,
                         const ActionTargetPreview&) = default;
};

// The previewed hit of an action against one target
// (PreviewLiveActionAgainstLimb's request and its first hit).
struct ActionPreviewResult {
  int action_id = 0;
  int damage_type = 0;
  int effect_variant = 0;
  int target_actor = 0;
  int target_hp = 0;
  int target_mp = 0;
  int hit_threshold = 0;
  std::uint32_t remove_statuses = 0;
};

// A targeting volume of the action state (shape, kind, facing, extents,
// angle and origin, as Battle_TargetVolume reads it).
struct TargetingVolume {
  int shape = 0;
  int kind = 0;
  int facing = 0;
  int x = 0;
  int y = 0;
  int z = 0;
  int angle = 0;
  TargetPoint origin{};

  friend bool operator==(const TargetingVolume&,
                         const TargetingVolume&) = default;
};

// The action state's two volumes: the range around the source and the area
// the effect covers.
enum class TargetingVolumeSlot : std::uint8_t { kRange, kArea };

// A range shape's extents (Battle_Ai_ActionRangeShape); only y and z set its
// reach.
struct RangeShape {
  int x = 0;
  int y = 0;
  int z = 0;
  int shape = 0;
  int angle = 0;
};

// The targeting fields of an action definition.
struct TargetingActionView {
  int category = 0;
  // The effect of the action's first hit.
  int effect = 0;
  bool returns_to_menu_on_cancel = false;
  // The range around the source; range.x == kWeaponRangeSentinel takes the
  // attacker's weapon range.
  RangeShape range{};
  // The covered area; area.shape and area.angle split the area shape byte.
  RangeShape area{};
  // AI area flags: a radial area reaches its y extent, a shaped area its
  // range-shape extent.
  bool radial_area = false;
  bool shaped_area = false;
};

// An action range x that stands for the attacker's weapon range.
inline constexpr int kWeaponRangeSentinel = 0xFF;

// What an action's target category asks of the targeting.
struct TargetCategoryRules {
  bool needs_target_selection = false;
  bool area_target = false;
  bool includes_acting_actor = false;
  bool targets_body_parts = false;
  int actor_mask = 0;
};

// One actor as the targeting reads it.
struct TargetingActor {
  int kind = 0;
  int default_limb = 0;
  int enemy_class = 0;
  int risk = 0;
  // The weapon range (stats currentRange..currentAttackShapeAngle).
  RangeShape weapon_range{};
  // The actor's command arguments: the targeted actor and limb, the point
  // the two command words form and the first command bytes.
  int command_actor = 0;
  int command_limb = 0;
  TargetPoint command_point{};
  int command_target_byte = 0;
  int command_limb_byte = 0;
};

// One treasure chest of the room.
struct TargetChest {
  int tile_x = 0;
  int tile_z = 0;
  int opened_flag = 0;
  int lock_level = 0;
  bool is_open = false;
};

// One room trigger or moving platform point an environment action can hit.
struct EnvironmentTrigger {
  int kind = 0;
  int tile_x = 0;
  int tile_z = 0;
};
struct PlatformPoint {
  int x = 0;
  int height = 0;
  int z = 0;
};

// The room palette transform targeting starts with.
struct RoomPaletteTransform {
  int mode = 0;
  int duration = 0;
  int red = 0;
  int green = 0;
  int blue = 0;
};

class ActionTargetingHost {
 public:
  virtual ~ActionTargetingHost() = default;

  // Action definitions and target categories.
  [[nodiscard]] virtual TargetingActionView Action(int action_id) const = 0;
  [[nodiscard]] virtual TargetCategoryRules CategoryRules(
      int category) const = 0;

  // The selection request.
  [[nodiscard]] virtual int SelectionActionId() const = 0;
  virtual void SetSelectionActionId(int action_id) = 0;
  // Zeroes the selection, then names its source actor and action.
  virtual void ResetSelection(int source_actor, int action_id) = 0;
  [[nodiscard]] virtual int PrimaryActor() const = 0;
  [[nodiscard]] virtual bool PrimaryTargetsPosition() const = 0;
  [[nodiscard]] virtual int SelectionTargetCount() const = 0;
  virtual void SetSelectionTargetCount(int count) = 0;
  virtual void SetSelectionActorTarget(int index, ActorLimbTarget target) = 0;
  // A position target: its mode word and the point written over its hit
  // target word.
  virtual void SetSelectionPositionTarget(int index, int mode,
                                          TargetPoint point) = 0;
  [[nodiscard]] virtual TargetPoint FocusPosition() const = 0;
  virtual void SetFocusPosition(TargetPoint point) = 0;

  // The action state.
  [[nodiscard]] virtual bool IsPlayerAction() const = 0;
  virtual void SetIsPlayerAction(bool player_action) = 0;
  virtual void SetSourceActorId(int source) = 0;
  [[nodiscard]] virtual int SelectedPreview() const = 0;
  virtual void SetSelectedPreview(int index) = 0;
  virtual void SetCameraPreview(int index) = 0;
  virtual void ResetSelectionTimers() = 0;
  [[nodiscard]] virtual TargetPoint AreaCenter() const = 0;
  virtual void SetAreaCenter(TargetPoint point) = 0;
  [[nodiscard]] virtual TargetPoint SavedAreaCenter() const = 0;
  virtual void SetSavedAreaCenter(TargetPoint point) = 0;
  [[nodiscard]] virtual TargetingVolume Volume(
      TargetingVolumeSlot slot) const = 0;
  virtual void SetVolume(TargetingVolumeSlot slot,
                         const TargetingVolume& volume) = 0;
  [[nodiscard]] virtual ActionTargetPreview Preview(int index) const = 0;
  virtual void SetPreview(int index, const ActionTargetPreview& preview) = 0;
  [[nodiscard]] virtual int PreviewCount() const = 0;
  virtual void SetPreviewCount(int count) = 0;
  [[nodiscard]] virtual bool IsPointInVolume(TargetingVolumeSlot slot,
                                             TargetPoint point) const = 0;
  [[nodiscard]] virtual ActionPreviewResult PreviewActionAgainstLimb(
      int action_id, int source_actor, int target_actor, int limb) = 0;

  // Actors.
  [[nodiscard]] virtual std::optional<TargetingActor> Actor(int slot) const = 0;
  [[nodiscard]] virtual std::uint32_t ActorStateFlags(int slot) const = 0;
  virtual void SetActorDisplayFlags(int slot, std::uint32_t flags) = 0;
  // The actor's position with its facing in pad; none for an empty slot.
  [[nodiscard]] virtual std::optional<TargetPoint> ActorPlacement(
      int slot) const = 0;
  [[nodiscard]] virtual int FindBoneTag(int slot, int tag, int kind) const = 0;
  [[nodiscard]] virtual int BoneTag(int slot, int bone, int kind) const = 0;
  // Bone positions overwrite x, y and z of `current`.
  [[nodiscard]] virtual TargetPoint LowGroupBonePosition(
      int slot, int limb, TargetPoint current) const = 0;
  [[nodiscard]] virtual TargetPoint TaggedBonePosition(
      int slot, int tag, TargetPoint current) const = 0;
  [[nodiscard]] virtual TargetPoint BonePosition(int slot, int bone,
                                                 TargetPoint current) const = 0;
  virtual void RestoreIdlePose(int slot) = 0;
  virtual void FreezeActorAnimations() = 0;

  // The room.
  [[nodiscard]] virtual std::vector<EnvironmentTrigger> RoomTriggers()
      const = 0;
  [[nodiscard]] virtual std::vector<PlatformPoint> MovingPlatformPoints()
      const = 0;
  [[nodiscard]] virtual std::vector<TargetChest> RoomChests() const = 0;
  [[nodiscard]] virtual int FloorHeightAt(int x, int z) const = 0;

  // The targeting sphere.
  virtual void InitSphere() = 0;
  virtual void FreeSphere() = 0;
  virtual void ClearSphereTargets() = 0;
  virtual void ClearPrimarySphereVolume() = 0;
  virtual void ClearSecondarySphereVolume() = 0;
  // Shows the range volume, with the area volume and its previews when
  // `with_area`.
  virtual void ShowRangeVolume(bool with_area, int preview_count) = 0;
  virtual void ShowAreaVolume() = 0;
  virtual void ShowSphereTargets(int preview_count, int mode) = 0;

  // Flow and presentation.
  virtual void SetFieldControlMode(int mode) = 0;
  virtual void SetActionFlowMode(int mode) = 0;
  [[nodiscard]] virtual bool WeaponDrawn() const = 0;
  virtual void SetHudBattleMode(int mode) = 0;
  virtual void ResumeAfterActionSelection() = 0;
  virtual void PauseFieldForMenu() = 0;
  virtual void ReleaseMenuRows() = 0;
  virtual void BeginPostActionRecovery(int mode) = 0;
  virtual void CommitEnvironmentAction(int mode) = 0;
  virtual void StorePreferredCameraDistance() = 0;
  virtual void ResetRiskPenaltyTimer() = 0;
  virtual void TransformRoomPalettes(const RoomPaletteTransform& transform) = 0;
  virtual void SetMinimapVisible(bool visible) = 0;
  virtual void SetRoomAnimationsPaused(bool paused) = 0;
  virtual void SetMovingPlatformsPaused(bool paused) = 0;
  virtual void SetActorMarkersHidden(bool hidden) = 0;
  virtual void SetClutAnimationEnabled(bool enabled) = 0;
};

// One volume of the targeting sphere (Battle_TargetVolume): the kind byte
// with its active bit, the range kind, the angle, the four extent bytes and
// the position.
struct SphereVolume {
  int kind = 0;
  int range_kind = 0;
  int angle = 0;
  TargetVolumeExtents extents{};
  TargetPoint position{};

  friend bool operator==(const SphereVolume&, const SphereVolume&) = default;
};

// The sphere cursor's offset from the volume centre; `locked` holds it on
// the ground plane.
struct SphereCursor {
  int x = 0;
  int y = 0;
  int z = 0;
  int locked = 0;

  friend bool operator==(const SphereCursor&, const SphereCursor&) = default;
};

// The ground-plane terms of the sphere volume transform (m00, m02, m20, m22).
struct SphereBasis {
  int m00 = 0;
  int m02 = 0;
  int m20 = 0;
  int m22 = 0;
};

// The highlighted target: its group (actor slot or encoded id) and pulse.
struct HighlightedTarget {
  int group = 0;
  int pulse_slot = 0;
};

// Where a target list row goes: its label state (which column it slides in
// from) and its row index.
struct TargetListRow {
  int state = 0;
  int index = 0;
};

// One label row of the target list (Battle_SphereTargetLabel without text).
struct TargetSelectionLabel {
  int state = 0;
  int depth = 0;
  int gradient = 0;
  int ring_size = 0;
  int screen_x = 0;
  int offset_y = 0;
  int fade = 0;
  TargetPoint world_position{};

  friend bool operator==(const TargetSelectionLabel&,
                         const TargetSelectionLabel&) = default;
};

// A label's text: a limb name, shown without its first character when set.
struct TargetLabelText {
  int limb_name = 0;
  bool skip_first_character = false;

  friend bool operator==(const TargetLabelText&,
                         const TargetLabelText&) = default;
};

// One column of the target list: its entries (target indices), its groups
// and the first target of each group.
struct TargetColumn {
  std::array<std::uint8_t, kTargetColumnCapacity> entries{};
  std::array<std::uint8_t, kTargetColumnGroupCapacity> groups{};
  std::array<std::uint8_t, kTargetColumnGroupCapacity> group_first_targets{};
  int entry_count = 0;
  int group_count = 0;

  friend bool operator==(const TargetColumn&, const TargetColumn&) = default;
};
struct TargetColumns {
  // The first column (D_800F4C70[0]) and the second (D_800F4C70[1]).
  std::array<TargetColumn, 2> columns{};
  int top_offset = 0;

  friend bool operator==(const TargetColumns&, const TargetColumns&) = default;
};

// Where a string of the target information comes from.
enum class TargetTextSource : std::uint8_t {
  // Battle_statusStrings through its index table.
  kStatusIndex,
  // Battle_statusStrings at a halfword offset.
  kStatusOffset,
  // The actor's name.
  kActorName,
  // Battle_emptyStatusString.
  kEmptyStatus,
  // Battle_targetStatusLineTemplate.
  kStatusLineTemplate,
};
struct TargetText {
  TargetTextSource source = TargetTextSource::kEmptyStatus;
  int value = 0;

  friend bool operator==(const TargetText&, const TargetText&) = default;
};

// The right stick after its dead zone.
struct StickInput {
  int x = 0;
  int y = 0;
};

class TargetSelectionHost {
 public:
  virtual ~TargetSelectionHost() = default;

  // The sphere.
  [[nodiscard]] virtual SphereVolume Volume(int index) const = 0;
  virtual void SetVolume(int index, const SphereVolume& volume) = 0;
  [[nodiscard]] virtual SphereCursor Cursor() const = 0;
  virtual void SetCursor(const SphereCursor& cursor) = 0;
  [[nodiscard]] virtual int WireShape() const = 0;
  [[nodiscard]] virtual int CameraYaw() const = 0;
  [[nodiscard]] virtual int TargetCount() const = 0;
  [[nodiscard]] virtual int SecondaryTargetCount() const = 0;
  [[nodiscard]] virtual int SelectedTarget() const = 0;
  virtual void SetSelectedTarget(int index) = 0;
  [[nodiscard]] virtual bool AllTargetsMode() const = 0;
  [[nodiscard]] virtual HighlightedTarget Highlighted() const = 0;
  virtual void SetHighlighted(HighlightedTarget target) = 0;
  virtual void SetHighlightTint(int rgb) = 0;
  // The sphere's target lists: 0 lists the candidates, 1 the area previews.
  [[nodiscard]] virtual ActionTargetPreview SphereTarget(int list,
                                                         int index) const = 0;
  virtual void BuildCameraBasis() = 0;
  // Builds and loads the transform of volume 0 for this frame.
  [[nodiscard]] virtual SphereBasis BuildVolumeTransform() = 0;
  virtual void CacheTerrainRow(int row) = 0;
  virtual void UpdateTargetHighlights() = 0;
  virtual void DrawTerrainIntersection(TargetPoint point, int scale) = 0;
  // The collision map size in tiles: width in the low halfword, depth above.
  [[nodiscard]] virtual int CollisionMapDimensions() const = 0;
  virtual void DrawPositionCursor(TargetPoint point, bool lock_pitch) = 0;
  virtual void ClearPrimarySphereVolume() = 0;
  virtual void SetReuseSphereVolumes(bool reuse) = 0;

  // Actor highlights.
  virtual void DisablePalettePulse(int actor, int pulse_slot) = 0;
  virtual void EnablePalettePulse(int actor, int pulse_flags,
                                  int pulse_slot) = 0;
  // Tints the actor with the sphere's highlight tint, or clears its tint.
  virtual void SetActorTint(int actor, bool tinted) = 0;
  [[nodiscard]] virtual std::optional<int> ActorKind(int actor) const = 0;

  // Input and sounds.
  [[nodiscard]] virtual std::uint32_t ButtonsHeld() const = 0;
  [[nodiscard]] virtual std::uint32_t ButtonsPressed() const = 0;
  [[nodiscard]] virtual std::uint32_t ButtonsRepeated() const = 0;
  [[nodiscard]] virtual StickInput RightStick() const = 0;
  virtual void PlayLeaveSound() = 0;
  virtual void PlaySelectSound() = 0;
  virtual void PlayChangeSound() = 0;

  // The target list.
  [[nodiscard]] virtual int SelectionStep() const = 0;
  virtual void SetSelectionStep(int step) = 0;
  [[nodiscard]] virtual TargetColumns Columns() const = 0;
  virtual void SetColumns(const TargetColumns& columns) = 0;
  [[nodiscard]] virtual TargetSelectionLabel Label(int index) const = 0;
  virtual void SetLabel(int index, const TargetSelectionLabel& label) = 0;
  virtual void SetLabelText(int index, TargetLabelText text) = 0;
  [[nodiscard]] virtual bool LabelsSettled() const = 0;
  [[nodiscard]] virtual bool HandlePageButtons() = 0;
  [[nodiscard]] virtual int MapPageIndex(int index) const = 0;

  // The information box.
  virtual void InitInformationBox() = 0;
  virtual void ShowNoTargetsMessage() = 0;
  virtual void SetContextText(int slot, TargetText text) = 0;
  virtual void SetContextInteger(int slot, int value) = 0;
  // Formats the target information template into the sphere's text buffer
  // and shows it.
  virtual void ShowTargetInfo() = 0;
  virtual void DismissInformationBox() = 0;
};

// decomp: BATTLE.PRG func_8006F630 (src/BATTLE/BATTLE.PRG/146C.c:3235)
void PopulateActionTargetPreview(const ActionTargetingHost& host,
                                 ActionTargetPreview& preview,
                                 const ActionPreviewResult& result);
// decomp: BATTLE.PRG func_800704D8 (src/BATTLE/BATTLE.PRG/146C.c:3578)
void ReturnToBattleActionMenu(ActionTargetingHost& host);
// decomp: BATTLE.PRG func_8007053C (src/BATTLE/BATTLE.PRG/146C.c:3588)
void FilterAreaActionTargets(ActionTargetingHost& host);
// decomp: BATTLE.PRG func_8007087C (src/BATTLE/BATTLE.PRG/146C.c:3650): the
// point half a turn from the source's facing, at its height.
[[nodiscard]] std::optional<TargetPoint> AimActionBehindSource(
    const ActionTargetingHost& host, int source_actor, TargetPoint focus);
// decomp: BATTLE.PRG func_800708EC (src/BATTLE/BATTLE.PRG/146C.c:3660)
void SetSingleActionTarget(ActionTargetingHost& host);
// decomp: BATTLE.PRG func_80070B04 (src/BATTLE/BATTLE.PRG/146C.c:3695)
void SetAllActionTargets(ActionTargetingHost& host);
// decomp: BATTLE.PRG func_80070CAC (src/BATTLE/BATTLE.PRG/146C.c:3728)
void CollectEnvironmentActionTargets(ActionTargetingHost& host);
// decomp: BATTLE.PRG func_800719FC (src/BATTLE/BATTLE.PRG/146C.c:4086)
void CollectSourceAreaActionTargets(ActionTargetingHost& host);
// decomp: BATTLE.PRG func_80071F68 (src/BATTLE/BATTLE.PRG/146C.c:4176)
void CollectDirectActionTargets(ActionTargetingHost& host, int source_slot);
// decomp: BATTLE.PRG func_80072734 (src/BATTLE/BATTLE.PRG/146C.c:4329)
void CollectGroundAreaActionTargets(ActionTargetingHost& host, int source_slot);
// decomp: BATTLE.PRG func_80072BA8 (src/BATTLE/BATTLE.PRG/146C.c:4416)
void InitializeActionTargeting(ActionTargetingHost& host, int action_id);
// decomp: BATTLE.PRG func_80072EC4 (src/BATTLE/BATTLE.PRG/146C.c:4489)
void BeginActorActionSelection(ActionTargetingHost& host, int source_slot,
                               int action_id);

// decomp: BATTLE.PRG func_800C4794 (src/BATTLE/BATTLE.PRG/5BF94.c:640):
// moves the cursor and returns whether the player confirmed; a cancel sets
// the target's pad.
[[nodiscard]] bool UpdateSphereTargetPosition(TargetSelectionHost& host,
                                              TargetPoint& target);
// decomp: BATTLE.PRG func_800C513C (src/BATTLE/BATTLE.PRG/5BF94.c:869)
void ClearTargetActorHighlight(TargetSelectionHost& host, int actor,
                               int pulse_slot);
// decomp: BATTLE.PRG func_800C5164 (src/BATTLE/BATTLE.PRG/5BF94.c:876)
void SetTargetActorHighlight(TargetSelectionHost& host, int actor,
                             int pulse_flags, int pulse_slot);
// decomp: BATTLE.PRG func_800C518C (src/BATTLE/BATTLE.PRG/5BF94.c:883)
void SetTargetActorColorOverride(TargetSelectionHost& host, int actor,
                                 bool tinted);
// decomp: BATTLE.PRG func_800C51B4 (src/BATTLE/BATTLE.PRG/5BF94.c:890):
// mode bit 0 clears the old highlight and bit 1 highlights the selection.
void UpdateTargetSelectionHighlight(TargetSelectionHost& host, int mode);
// decomp: BATTLE.PRG func_800C5360 (src/BATTLE/BATTLE.PRG/5BF94.c:939)
void AppendTargetGroupToColumn(const TargetSelectionHost& host,
                               TargetColumns& columns, int group);
// decomp: BATTLE.PRG func_800C553C (src/BATTLE/BATTLE.PRG/5BF94.c:970)
[[nodiscard]] TargetColumns BuildTargetSelectionColumns(
    const TargetSelectionHost& host);
// decomp: BATTLE.PRG func_800C56C0 (src/BATTLE/BATTLE.PRG/5BF94.c:1006)
void DismissTargetSelectionRows(TargetSelectionHost& host);
// decomp: BATTLE.PRG func_800C5798 (src/BATTLE/BATTLE.PRG/5BF94.c:1021)
void InitTargetSelectionRow(TargetSelectionHost& host, int target_index,
                            TargetListRow row);
// decomp: BATTLE.PRG _getStatusString (src/BATTLE/BATTLE.PRG/5BF94.c:1045)
[[nodiscard]] TargetText FirstTargetStatusText(std::uint32_t status_mask);
// decomp: BATTLE.PRG func_800C58F8 (src/BATTLE/BATTLE.PRG/5BF94.c:1058):
// one frame of the target list; `target` gets the chosen preview or
// kNoPreviewSelected, and the result is whether the list closed.
[[nodiscard]] bool UpdateTargetSelectionMenu(TargetSelectionHost& host,
                                             int& target);

// decomp: BATTLE.PRG func_800E5568 (src/BATTLE/BATTLE.PRG/7CD68.c:437)
[[nodiscard]] int CalculateRangeShapeExtent(const RangeShape& shape);
// decomp: BATTLE.PRG func_800E5600 (src/BATTLE/BATTLE.PRG/7CD68.c:459)
[[nodiscard]] int CalculateActionAreaReach(const TargetingActionView& action,
                                           std::uint32_t maximum_reach);
// decomp: BATTLE.PRG func_800E5698 (src/BATTLE/BATTLE.PRG/7CD68.c:476)
[[nodiscard]] int CalculateActionRangeSquared(const TargetingActionView& action,
                                              const RangeShape& weapon_range);
}  // namespace VS::Battle
