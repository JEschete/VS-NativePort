#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event script's actor opcodes: movement, facing, animation, appearance,
// shared object models, actor groups and the commands that read or award
// items and actor distances. Each handler decodes its little-endian operands
// itself and drives the world only through ScriptActorHost, so a mod can run
// the same commands against any actor store.
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Actors/ActorPlacement.hpp"
#include "Actors/ActorSpawning.hpp"
#include "Actors/ActorStateRecord.hpp"
#include "Actors/AnimationControl.hpp"
#include "Actors/MotionRequests.hpp"
#include "Event/ScriptCommand.hpp"
#include "Event/ScriptStep.hpp"
#include "Render/DepthCue.hpp"

namespace VS::Event {

// Instruction lengths in bytes, opcode included, from the event VM's opcode
// size table (BATTLE.PRG D_800E9C30).
inline constexpr std::size_t kResetActorGroupStateLength = 4;
inline constexpr std::size_t kStoreActorTileLength = 5;
inline constexpr std::size_t kSpawnActorLength = 9;
inline constexpr std::size_t kNavigateActorToTileLength = 6;
inline constexpr std::size_t kPlayActorAnimationLength = 6;
inline constexpr std::size_t kSetActorGroupFlagsLength = 5;
inline constexpr std::size_t kSetActorSoundMixLength = 7;
inline constexpr std::size_t kWalkActorToTileLength = 9;
inline constexpr std::size_t kWalkActorToWorldPosLength = 12;
inline constexpr std::size_t kSlideActorToTileLength = 7;
inline constexpr std::size_t kSlideActorToWorldPosLength = 10;
inline constexpr std::size_t kSetActorAttachmentLength = 7;
inline constexpr std::size_t kNavigateActorToWorldPosLength = 7;
inline constexpr std::size_t kTurnActorToFacingLength = 6;
inline constexpr std::size_t kFaceOtherActorLength = 6;
inline constexpr std::size_t kFaceTileLength = 7;
inline constexpr std::size_t kScaleActorLength = 10;
inline constexpr std::size_t kRemoveActorGroupLength = 3;
inline constexpr std::size_t kWaitForActorFlagsLength = 6;
inline constexpr std::size_t kSetActorColorOverrideLength = 6;
inline constexpr std::size_t kQueueActorAnimationLength = 6;
inline constexpr std::size_t kRotateActorAxesLength = 11;
inline constexpr std::size_t kSetActorHeadAnglesLength = 8;
inline constexpr std::size_t kPlayActorAnimationAtFrameLength = 7;
inline constexpr std::size_t kQueueActorAnimationAtFrameLength = 7;
inline constexpr std::size_t kSetActorIdleAnimationLength = 5;
inline constexpr std::size_t kLookAtActorLength = 6;
inline constexpr std::size_t kLookAtWorldPosLength = 10;
inline constexpr std::size_t kLoadObjectModelLength = 4;
inline constexpr std::size_t kWaitForObjectLoadLength = 1;
inline constexpr std::size_t kReleaseObjectModelLength = 2;
inline constexpr std::size_t kSelectObjectTextureBankLength = 2;
inline constexpr std::size_t kSetActorOverlayPassLength = 10;
inline constexpr std::size_t kSetActorWeaponDrawnLength = 4;
inline constexpr std::size_t kPlayActorAnimationSoundLength = 4;
inline constexpr std::size_t kSetNpcUpdatesEnabledLength = 2;
inline constexpr std::size_t kRestoreActorVitalsLength = 4;
inline constexpr std::size_t kSetActorEquipmentModelsLength = 7;
inline constexpr std::size_t kSetActorYawOscillationLength = 4;
inline constexpr std::size_t kClearAshleyStatusEffectsLength = 1;
inline constexpr std::size_t kSetActorRenderModeLength = 4;
inline constexpr std::size_t kSetEnteredDoorLength = 2;
inline constexpr std::size_t kStoreActorRangeBearingLength = 8;
inline constexpr std::size_t kSetActorActivityLength = 4;
inline constexpr std::size_t kAwardMiscLootLength = 7;
inline constexpr std::size_t kConsumeMiscItemLength = 3;
inline constexpr std::size_t kStoreActorDistanceLength = 7;

// Actor slots the selection and activity commands scan; the script-control
// marks cover the same slots.
inline constexpr int kScriptActorSlots = 17;
// The selection's id capacity (Event_ActorSelection.actorIds).
inline constexpr std::size_t kSelectionCapacity = 22;
// What NextScriptSelectedActor returns once the selection is spent. Actor 1
// is also a valid id, so a selection that reaches it stops there, as the
// original did.
inline constexpr int kSelectionEnd = 1;

// The actors one group command applies to.
struct ActorSelection {
  std::uint8_t remaining = 0;
  std::uint8_t cursor = 0;
  std::array<std::uint8_t, kSelectionCapacity> ids{};

  friend bool operator==(const ActorSelection&,
                         const ActorSelection&) = default;
};

// An animation request: bank-relative ids already resolved to an animation.
struct AnimationRequest {
  int animation = 0;
  int blend_frames = 0;
  int start_frame = 0;

  friend bool operator==(const AnimationRequest&,
                         const AnimationRequest&) = default;
};

// A shared object model load: the shared sequence slot, the model and the
// effect texture slot it draws from.
struct ObjectModelRequest {
  int slot = 0;
  int model = 0;
  int texture_slot = 0;

  friend bool operator==(const ObjectModelRequest&,
                         const ObjectModelRequest&) = default;
};

// The blade and shield an actor's equipment models load from.
struct EquipmentModels {
  int blade = 0;
  int shield = 0;

  friend bool operator==(const EquipmentModels&,
                         const EquipmentModels&) = default;
};

// The parent actor and bone a model attaches to.
struct BoneAttachment {
  int parent = 0;
  int bone = 0;

  friend bool operator==(const BoneAttachment&,
                         const BoneAttachment&) = default;
};

// The two items a scripted loot window offers.
struct ScriptLoot {
  std::int16_t first_item = 0;
  std::uint8_t first_count = 0;
  std::int16_t second_item = 0;
  std::uint8_t second_count = 0;

  friend bool operator==(const ScriptLoot&, const ScriptLoot&) = default;
};

// An actor's dialog anchor: the projected tagged-bone endpoint and the screen
// point of the anchor offset projected through that bone.
struct DialogAnchorProjection {
  Actors::MotionVector anchor;
  std::int16_t screen_x = 0;
  std::int16_t screen_y = 0;

  friend bool operator==(const DialogAnchorProjection&,
                         const DialogAnchorProjection&) = default;
};

// Script requests the commands refuse rather than overrun or fault on.
enum class ScriptActorRefusal : std::uint8_t {
  kControlledSlot,     // a script-control mark past the marked slots
  kWeaponDrawState,    // a weapon draw-state index past its table
  kZeroDistanceScale,  // a range command that divides by zero
  kSelectionCursor,    // a selection read past its id capacity
  kMissingPlacement,   // a command that reads an absent actor's placement
  kShortCommand,       // an instruction shorter than its operands
};

// The world the actor opcodes read and change.
class ScriptActorHost {
 public:
  ScriptActorHost() = default;
  ScriptActorHost(const ScriptActorHost&) = delete;
  ScriptActorHost& operator=(const ScriptActorHost&) = delete;
  ScriptActorHost(ScriptActorHost&&) = delete;
  ScriptActorHost& operator=(ScriptActorHost&&) = delete;
  virtual ~ScriptActorHost() = default;

  virtual void Refuse(ScriptActorRefusal refusal, int value) = 0;

  // Actor references and the event VM.
  [[nodiscard]] virtual int ResolveActor(std::uint16_t reference) = 0;
  [[nodiscard]] virtual bool SkipActive() const = 0;
  virtual void MarkScriptControlled(int actor) = 0;
  [[nodiscard]] virtual std::optional<int> PresentActorKind(
      int actor) const = 0;
  [[nodiscard]] virtual int FindActorByModelId(int model_id) const = 0;
  [[nodiscard]] virtual std::optional<Actors::ActorPlacementView>
  ActorPlacement(int actor) const = 0;
  virtual void SetFlag(int flag, std::uint8_t value) = 0;

  // Motion. A rate of -1 keeps the actor's own rate.
  virtual void InterruptAction(int actor, int animate) = 0;
  virtual void BeginScriptedPath(int actor,
                                 const Actors::MotionVector& target) = 0;
  [[nodiscard]] virtual bool ScriptedPathFinished(int actor) = 0;
  virtual void ClearMovementModes(int actor) = 0;
  virtual void MoveToTile(int actor, Actors::PackedTile tile, int move_rate,
                          int turn_rate) = 0;
  virtual void MoveToPosition(int actor, const Actors::MotionVector& target,
                              int move_rate, int turn_rate) = 0;
  virtual void TranslateToTile(int actor, Actors::PackedTile tile,
                               int move_rate) = 0;
  virtual void TranslateTo(int actor, const Actors::MotionVector& target,
                           int move_rate) = 0;
  virtual void TurnBy(int actor, std::int16_t yaw_delta, int turn_rate) = 0;
  virtual void TurnTo(int actor, std::int16_t yaw, int turn_rate) = 0;
  virtual void TurnTowardTile(int actor, Actors::PackedTile tile,
                              int turn_rate) = 0;
  virtual void RotateBy(int actor, const Actors::MotionVector& delta,
                        int turn_rate) = 0;
  virtual void RotateTo(int actor, const Actors::MotionVector& rotation,
                        int turn_rate) = 0;
  virtual void ScaleTo(int actor, const Actors::MotionVector& scale,
                       int steps) = 0;
  virtual void SetHeadTracking(int actor, int target,
                               std::optional<Actors::MotionVector> aim,
                               int rate) = 0;

  // Animation and sound.
  [[nodiscard]] virtual std::uint8_t AnimationBankBase(int bank) const = 0;
  virtual void PlayAnimation(int actor, AnimationRequest request) = 0;
  virtual void PlayAnimationAtFrame(int actor, AnimationRequest request) = 0;
  virtual void QueueAnimation(int actor, AnimationRequest request) = 0;
  virtual void SetTravelAnimation(int actor, int animation) = 0;
  virtual void SetSoundOverrides(int actor, Actors::SoundEffectSlot slot) = 0;
  virtual void PlayWeaponDrawSound(int actor, int sound) = 0;

  // Attachment, display and logic state.
  virtual void ReleaseAttachmentLock(int actor) = 0;
  virtual void AttachToParentBone(int actor, BoneAttachment attachment) = 0;
  [[nodiscard]] virtual std::uint32_t StateFlags(int actor) const = 0;
  virtual void SetDisplayFlags(int actor, std::uint32_t flags) = 0;
  virtual void SetDetached(int actor, bool detached) = 0;
  virtual void ResetWeaponVisibilityBlend(int actor) = 0;
  virtual void SetLogicEnabled(int actor, int enabled) = 0;
  virtual void SetOtherLogicEnabled(int enabled) = 0;

  // Appearance.
  virtual void SetTint(int actor, std::optional<Render::ColorCode> tint) = 0;
  virtual void SetCompanion(int actor,
                            std::optional<Actors::CompanionStyle> style) = 0;
  virtual void SetRootSpin(int actor, int rate) = 0;
  virtual void StartAppearanceTransition(int actor, int mode) = 0;

  // Shared object models.
  virtual void QueueSharedSequence(int slot, int model) = 0;
  virtual void QueueSharedSequenceWithTextures(ObjectModelRequest request) = 0;
  virtual void QueueSequenceEffectTextures(ObjectModelRequest request) = 0;
  virtual void ReleaseSequenceEffectTextures(int bank) = 0;
  [[nodiscard]] virtual int UnloadSharedSequence(int bank) = 0;
  virtual void SetSharedSequenceBank(int bank) = 0;
  [[nodiscard]] virtual int FindFreeAssetRequest() const = 0;

  // Actors, battle state and items.
  virtual void QueueScriptedSpawn(int actor, int model,
                                  Actors::SpawnPlacement placement) = 0;
  virtual void RemoveScriptedActor(int actor) = 0;
  virtual void RefreshEquipmentModels(int actor) = 0;
  virtual void LoadEquipmentModels(int actor, EquipmentModels models) = 0;
  virtual void RestoreVitals(int actor) = 0;
  [[nodiscard]] virtual std::optional<int> WeaponDrawState(int index) const = 0;
  virtual void SetBattleMode(int actor, int mode) = 0;
  virtual void PreserveAshleyStatuses() = 0;
  virtual void MarkEnteredDoor(int door) = 0;
  [[nodiscard]] virtual bool MenuTransitionActive() const = 0;
  virtual void SetMenuTransitionActive(bool active) = 0;
  virtual void PresentScriptedLoot(ScriptLoot loot) = 0;
  [[nodiscard]] virtual int UpdateRewardSequence() = 0;
  virtual void ConsumeMiscItem(int item) = 0;

  // The tagged bone's matrix and projection for a dialog anchor.
  [[nodiscard]] virtual DialogAnchorProjection ProjectDialogAnchor(
      int actor, const Actors::MotionVector& anchor) = 0;
};

// Selections and actor activity (BATTLE.PRG 4A0A8.c).
// decomp: func_800BB68C (src/BATTLE/BATTLE.PRG/4A0A8.c:2880)
[[nodiscard]] ActorSelection BuildScriptActorSelection(ScriptActorHost& host,
                                                       std::uint16_t selector);
// decomp: func_800BB788 (src/BATTLE/BATTLE.PRG/4A0A8.c:2910)
[[nodiscard]] int NextScriptSelectedActor(ScriptActorHost& host,
                                          ActorSelection& selection);
// decomp: func_800BAF6C (src/BATTLE/BATTLE.PRG/4A0A8.c:2634): `enabled` is
// the script's byte, passed on unchanged to the other actors' logic switch.
void SetEventActorActivity(ScriptActorHost& host, int enabled);
// decomp: func_800BB7C4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2920): projects the
// anchor above the actor's tagged bone into `anchor` and returns its z, the
// screen heading from the bone endpoint to the anchor point. The anchor's w
// is the caller's until the projection writes it.
int ActorDialogAnchor(ScriptActorHost& host, int actor,
                      Actors::MotionVector& anchor);

// Opcode handlers; each returns how the event VM continues.
// decomp: func_800B7428 (src/BATTLE/BATTLE.PRG/4A0A8.c:967), opcode 0x1E
ScriptStep ScriptResetActorGroupState(ScriptCommand command,
                                      ScriptActorHost& host);
// decomp: func_800B7490 (src/BATTLE/BATTLE.PRG/4A0A8.c:983), opcode 0x1F
ScriptStep ScriptStoreActorTile(ScriptCommand command, ScriptActorHost& host);
// decomp: func_800B7574 (src/BATTLE/BATTLE.PRG/4A0A8.c:1009), opcode 0x20
ScriptStep ScriptSpawnActor(ScriptCommand command, ScriptActorHost& host);
// decomp: func_800B75F4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1022), opcode 0x21
ScriptStep ScriptNavigateActorToTile(ScriptCommand command,
                                     ScriptActorHost& host);
// decomp: func_800B76A8 (src/BATTLE/BATTLE.PRG/4A0A8.c:1040), opcode 0x22
ScriptStep ScriptPlayActorAnimation(ScriptCommand command,
                                    ScriptActorHost& host);
// decomp: func_800B770C (src/BATTLE/BATTLE.PRG/4A0A8.c:1047), opcode 0x23
ScriptStep ScriptSetActorGroupFlags(ScriptCommand command,
                                    ScriptActorHost& host);
// decomp: func_800B77DC (src/BATTLE/BATTLE.PRG/4A0A8.c:1080), opcode 0x24
ScriptStep ScriptSetActorSoundMix(ScriptCommand command, ScriptActorHost& host);
// decomp: func_800B784C (src/BATTLE/BATTLE.PRG/4A0A8.c:1097), opcode 0x25
ScriptStep ScriptWalkActorToTile(ScriptCommand command, ScriptActorHost& host);
// decomp: func_800B792C (src/BATTLE/BATTLE.PRG/4A0A8.c:1116), opcode 0x26
ScriptStep ScriptWalkActorToWorldPos(ScriptCommand command,
                                     ScriptActorHost& host);
// decomp: func_800B7A98 (src/BATTLE/BATTLE.PRG/4A0A8.c:1148), opcode 0x27
ScriptStep ScriptSlideActorToTile(ScriptCommand command, ScriptActorHost& host);
// decomp: func_800B7B58 (src/BATTLE/BATTLE.PRG/4A0A8.c:1167), opcode 0x28
ScriptStep ScriptSlideActorToWorldPos(ScriptCommand command,
                                      ScriptActorHost& host);
// decomp: func_800B7C90 (src/BATTLE/BATTLE.PRG/4A0A8.c:1200), opcode 0x29
ScriptStep ScriptSetActorAttachment(ScriptCommand command,
                                    ScriptActorHost& host);
// decomp: func_800B7D10 (src/BATTLE/BATTLE.PRG/4A0A8.c:1212), opcode 0x2A
ScriptStep ScriptNavigateActorToWorldPos(ScriptCommand command,
                                         ScriptActorHost& host);
// decomp: func_800B7DC4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1230), opcode 0x2B
ScriptStep ScriptTurnActorToFacing(ScriptCommand command,
                                   ScriptActorHost& host);
// decomp: func_800B7EF0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1266), opcode 0x2C
ScriptStep ScriptFaceOtherActor(ScriptCommand command, ScriptActorHost& host);
// decomp: func_800B7F84 (src/BATTLE/BATTLE.PRG/4A0A8.c:1285), opcode 0x2D
ScriptStep ScriptFaceTile(ScriptCommand command, ScriptActorHost& host);
// decomp: func_800B804C (src/BATTLE/BATTLE.PRG/4A0A8.c:1299), opcode 0x2E
ScriptStep ScriptScaleActor(ScriptCommand command, ScriptActorHost& host);
// decomp: func_800B80B4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1309), opcode 0x2F
ScriptStep ScriptRemoveActorGroup(ScriptCommand command, ScriptActorHost& host);
// decomp: func_800B810C (src/BATTLE/BATTLE.PRG/4A0A8.c:1324), opcode 0x30
ScriptStep ScriptWaitForActorFlags(ScriptCommand command,
                                   ScriptActorHost& host);
// decomp: func_800B81A0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1344), opcode 0x31
ScriptStep ScriptSetActorColorOverride(ScriptCommand command,
                                       ScriptActorHost& host);
// decomp: func_800B822C (src/BATTLE/BATTLE.PRG/4A0A8.c:1360), opcode 0x32
ScriptStep ScriptQueueActorAnimation(ScriptCommand command,
                                     ScriptActorHost& host);
// decomp: func_800B8290 (src/BATTLE/BATTLE.PRG/4A0A8.c:1367), opcode 0x33
ScriptStep ScriptRotateActorAxes(ScriptCommand command, ScriptActorHost& host);
// decomp: func_800B8360 (src/BATTLE/BATTLE.PRG/4A0A8.c:1390), opcode 0x34
ScriptStep ScriptSetActorHeadAngles(ScriptCommand command,
                                    ScriptActorHost& host);
// decomp: func_800B83D4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1408), opcode 0x35
ScriptStep ScriptPlayActorAnimationAtFrame(ScriptCommand command,
                                           ScriptActorHost& host);
// decomp: func_800B8438 (src/BATTLE/BATTLE.PRG/4A0A8.c:1415), opcode 0x36
ScriptStep ScriptQueueActorAnimationAtFrame(ScriptCommand command,
                                            ScriptActorHost& host);
// decomp: func_800B849C (src/BATTLE/BATTLE.PRG/4A0A8.c:1422), opcode 0x37
ScriptStep ScriptSetActorIdleAnimation(ScriptCommand command,
                                       ScriptActorHost& host);
// decomp: func_800B8510 (src/BATTLE/BATTLE.PRG/4A0A8.c:1434), opcode 0x38
ScriptStep ScriptLookAtActor(ScriptCommand command, ScriptActorHost& host);
// decomp: func_800B8590 (src/BATTLE/BATTLE.PRG/4A0A8.c:1454), opcode 0x39
ScriptStep ScriptLookAtWorldPos(ScriptCommand command, ScriptActorHost& host);
// decomp: func_800B860C (src/BATTLE/BATTLE.PRG/4A0A8.c:1471), opcode 0x3A
ScriptStep ScriptLoadObjectModel(ScriptCommand command, ScriptActorHost& host);
// decomp: func_800B86E8 (src/BATTLE/BATTLE.PRG/4A0A8.c:1488), opcode 0x3B
ScriptStep ScriptWaitForObjectLoad(ScriptCommand command,
                                   ScriptActorHost& host);
// decomp: func_800B8708 (src/BATTLE/BATTLE.PRG/4A0A8.c:1490), opcode 0x3C
ScriptStep ScriptReleaseObjectModel(ScriptCommand command,
                                    ScriptActorHost& host);
// decomp: func_800B8764 (src/BATTLE/BATTLE.PRG/4A0A8.c:1500), opcode 0x3D
ScriptStep ScriptSelectObjectTextureBank(ScriptCommand command,
                                         ScriptActorHost& host);
// decomp: func_800B8788 (src/BATTLE/BATTLE.PRG/4A0A8.c:1506), opcode 0x3E
ScriptStep ScriptSetActorOverlayPass(ScriptCommand command,
                                     ScriptActorHost& host);
// decomp: func_800B884C (src/BATTLE/BATTLE.PRG/4A0A8.c:1526), opcode 0x3F
ScriptStep ScriptSetActorWeaponDrawn(ScriptCommand command,
                                     ScriptActorHost& host);
// decomp: func_800B91F0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1786), opcode 0x54
ScriptStep ScriptPlayActorAnimationSound(ScriptCommand command,
                                         ScriptActorHost& host);
// decomp: func_800B9230 (src/BATTLE/BATTLE.PRG/4A0A8.c:1792), opcode 0x55
ScriptStep ScriptSetNpcUpdatesEnabled(ScriptCommand command,
                                      ScriptActorHost& host);
// decomp: func_800B92A0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1807), opcode 0x57
ScriptStep ScriptRestoreActorVitals(ScriptCommand command,
                                    ScriptActorHost& host);
// decomp: func_800B9608 (src/BATTLE/BATTLE.PRG/4A0A8.c:1916), opcode 0x5C
ScriptStep ScriptSetActorEquipmentModels(ScriptCommand command,
                                         ScriptActorHost& host);
// decomp: func_800B9670 (src/BATTLE/BATTLE.PRG/4A0A8.c:1928), opcode 0x5D:
// starts or stops the actor's root spin.
ScriptStep ScriptSetActorYawOscillation(ScriptCommand command,
                                        ScriptActorHost& host);
// decomp: func_800B97F8 (src/BATTLE/BATTLE.PRG/4A0A8.c:1988), opcode 0x63
ScriptStep ScriptClearAshleyStatusEffects(ScriptCommand command,
                                          ScriptActorHost& host);
// decomp: func_800B9D74 (src/BATTLE/BATTLE.PRG/4A0A8.c:2151), opcode 0x71
ScriptStep ScriptSetActorRenderMode(ScriptCommand command,
                                    ScriptActorHost& host);
// decomp: func_800BAC38 (src/BATTLE/BATTLE.PRG/4A0A8.c:2549), opcode 0xB1
ScriptStep CommandSetEnteredDoor(ScriptCommand command, ScriptActorHost& host);
// decomp: func_800BAC5C (src/BATTLE/BATTLE.PRG/4A0A8.c:2555), opcode 0xB2
ScriptStep CommandStoreActorRangeBearing(ScriptCommand command,
                                         ScriptActorHost& host);
// decomp: func_800BAD78 (src/BATTLE/BATTLE.PRG/4A0A8.c:2575), opcode 0xB3
ScriptStep CommandSetActorActivity(ScriptCommand command,
                                   ScriptActorHost& host);
// decomp: func_800BADE0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2590), opcode 0xB4
ScriptStep CommandAwardMiscLoot(ScriptCommand command, ScriptActorHost& host);
// decomp: func_800BAE74 (src/BATTLE/BATTLE.PRG/4A0A8.c:2606), opcode 0xB5
ScriptStep CommandConsumeMiscItem(ScriptCommand command, ScriptActorHost& host);
// decomp: func_800BAEA0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2612), opcode 0xB6
ScriptStep CommandStoreActorDistance(ScriptCommand command,
                                     ScriptActorHost& host);

}  // namespace VS::Event
