#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "Actors/ActorStateRecord.hpp"
#include "Content/ActorAssets.hpp"
#include "Render/DepthCue.hpp"

// MAINMENU.PRG's actor preview (224.c): the model beside the equipment and
// status screens, cloned from an actor record into the preview slot with its
// companion and weapon models, posed, drawn with its companion outline and
// weapons, and released; and Ashley's equipment pose and weapon animation
// load. The functions reach the actor and weapon records, the game heap, the
// asset queue and the actor model, animation and appearance systems through
// an ActorPreviewHost (ActorPreviewLive.hpp binds it).
namespace VS::Menu::Framework {
// The actor table slots (Actors_actorRecords) and weapon model slots
// (Actors_weaponModels) the preview uses: each actor slot owns two weapon
// models, the preview's are slots 2 and 3.
inline constexpr int kAshleyActorSlot = 0;
inline constexpr int kPreviewActorSlot = 1;
inline constexpr int kPreviewCompanionSlot = 16;
inline constexpr int kActorSlots = Actors::kActorStateSlots;
inline constexpr int kWeaponModelSlots = 20;
inline constexpr int kWeaponModelsPerActor = 2;
inline constexpr int kPreviewWeaponModelSlot = 2;
static_assert(kPreviewWeaponModelSlot ==
              kPreviewActorSlot * kWeaponModelsPerActor);
// The bone matrices of an actor record (Actors_ActorRecord.boneMatrices).
inline constexpr int kActorBoneMatrices = 42;

// The heap blocks the clone allocates: a record for the clone and its
// companion, a weapon model per weapon and Ashley's battle sequence copy
// (0x7800 bytes, 224.c:81).
enum class PreviewAllocation : std::uint8_t {
  kActorRecord,
  kWeaponModel,
  kBattleSequence,
};

// DrawActorPreview's results (224.c:177).
enum class PreviewDrawResult : std::int8_t {
  kDrawn = 0,
  kNotDrawn = -1,
  // The model was culled (instance.isPreviewModel): no weapons drawn.
  kCulled = -2,
};

// The actor record fields the preview reads (Actors_ActorRecord).
struct PreviewActor {
  std::uint16_t model_id = 0;
  // instance.variant and instance.slot: the actor's colour variant and the
  // weapon model pair it owns.
  std::uint8_t variant = 0;
  std::uint8_t weapon_pair = 0;
  std::uint8_t companion_slot = 0;
  Actors::CompanionOffset companion{};
  std::uint8_t bone_count = 0;
  bool skipped = false;
  bool frozen = false;
  bool culled = false;
  bool weapon_drawn = false;

  friend bool operator==(const PreviewActor&, const PreviewActor&) = default;
};

// The weapon model fields the preview reads (Actors_WeaponModel).
struct PreviewWeapon {
  std::uint8_t model_id = 0;
  std::uint8_t attachment_slot = 0;
  std::uint8_t material = 0;
  std::uint8_t attachment_bone = 0;
  // reserved08Bit4: the model draws with its owner.
  bool attached = false;

  friend bool operator==(const PreviewWeapon&, const PreviewWeapon&) = default;
};

// The blocks of a clone's sequence copy (224.c:78-90). The source is the
// cloned record's block, read before the asset queue ran: the queue installs
// the clone in actor slot 1 (and its companion in slot 16) and may replace
// the source's record there.
struct PreviewSequenceCopy {
  Content::AssetAddress clone = Content::AssetAddress::kNone;
  Content::AssetAddress source = Content::AssetAddress::kNone;
  Content::AssetAddress battle_copy = Content::AssetAddress::kNone;
};

class ActorPreviewHost {
 public:
  ActorPreviewHost() = default;
  ActorPreviewHost(const ActorPreviewHost&) = delete;
  ActorPreviewHost& operator=(const ActorPreviewHost&) = delete;
  ActorPreviewHost(ActorPreviewHost&&) = delete;
  ActorPreviewHost& operator=(ActorPreviewHost&&) = delete;
  virtual ~ActorPreviewHost() = default;

  // The record in actor slot `slot` (below kActorSlots), or none.
  [[nodiscard]] virtual std::optional<PreviewActor> Actor(int slot) const = 0;
  // The model in weapon slot `slot` (below kWeaponModelSlots), or none.
  [[nodiscard]] virtual std::optional<PreviewWeapon> Weapon(int slot) const = 0;

  // The game heap's low end (AllocateLiveFromLowEnd, FreeLiveFromLowEnd) and
  // the blocks of the actor and weapon slots.
  [[nodiscard]] virtual Content::AssetAddress Allocate(
      PreviewAllocation kind) = 0;
  virtual void Free(Content::AssetAddress block) = 0;
  [[nodiscard]] virtual Content::AssetAddress ActorBlock(int slot) const = 0;
  [[nodiscard]] virtual Content::AssetAddress WeaponBlock(int slot) const = 0;

  // The battle asset queue: a request, one queue step (true while requests
  // remain), a released model and Ashley's released weapon sequences.
  virtual void QueueAsset(const Content::ActorAssetRequest& request) = 0;
  [[nodiscard]] virtual bool UpdateAssetQueue() = 0;
  virtual void ReleaseModel(int slot) = 0;
  virtual void ReleaseSequences(int slot) = 0;
  // Gives the record at `copy.clone` the sequences, sequence model, flags and
  // common animation count of the record at `copy.source`; the battle
  // sequence is copied into `copy.battle_copy`, or shared when that is kNone
  // (224.c:78-90).
  virtual void CopySequences(const PreviewSequenceCopy& copy) = 0;

  // The record writes of the pose resets.
  virtual void SetWeaponDrawn(int slot) = 0;
  virtual void SetWeaponAttached(int weapon_slot) = 0;
  virtual void SetWeaponPose(int weapon_slot, std::uint8_t pose) = 0;
  // Clears the position, preview pose, skip and sound flags (224.c:118-123).
  virtual void ResetPlacement(int slot) = 0;
  virtual void ClearSkipped(int slot) = 0;
  virtual void ClearPreviewFlag(int slot) = 0;

  // The actor systems (ActorStateLive.hpp, AnimationControlLive.hpp,
  // AppearanceLive.hpp, Gimmicks.h).
  virtual void PlayAnimation(int slot, int animation) = 0;
  virtual void ResetMotion(int slot) = 0;
  virtual void SetCompanion(int slot, int companion_slot,
                            const Actors::CompanionStyle& style) = 0;
  virtual void SetFade(int slot, const Render::ColorCode& fade) = 0;
  virtual void ClearTint(int slot) = 0;
  virtual void RefreshInteractionTile(int slot) = 0;

  // Ashley's pose restart (224.c:336-349): the animation and its header, the
  // pose built from it, one skeletal step, the blend pose copied from the
  // pose and the frame, queued animation and blend counters cleared.
  virtual void SetAnimation(int slot, int animation) = 0;
  virtual void InitializePose(int slot) = 0;
  virtual void AdvanceAnimation(int slot) = 0;
  virtual void SnapshotBlendPose(int slot) = 0;
  virtual void ClearPoseCounters(int slot) = 0;

  // The draw: the camera matrix, the tick speed, the model, companion and
  // weapon passes and the scene ordering table offset.
  virtual void PrepareCamera() = 0;
  [[nodiscard]] virtual int TickSpeed() const = 0;
  virtual void SetFramesPerStep(int slot, std::uint8_t frames) = 0;
  virtual void UpdateTextureScrolling(int slot) = 0;
  virtual void UpdateAnimationBlend(int slot) = 0;
  virtual void PrepareRootTransform(int slot) = 0;
  virtual void DrawModel(int slot, int companion_slot) = 0;
  virtual void OffsetOrderingTable(int entries) = 0;
  virtual void MoveActor(int slot, const Actors::CompanionOffset& offset,
                         int sign) = 0;
  virtual void UpdateMotionAndLookAt(int slot, int companion_slot) = 0;
  // Draws weapon `weapon_slot` from bone `bone` of actor `slot`, the bone
  // moved back by `offset` while it draws (224.c:243-254).
  virtual void DrawWeapon(int weapon_slot, int slot, std::uint8_t bone,
                          const Actors::CompanionOffset& offset) = 0;

  virtual void Refuse(std::string_view what) = 0;
};

// The animation the preview plays once loaded (224.c:17).
inline constexpr int kPreviewAnimation = 1;
// Ashley's equipment pose animations (224.c:330-334).
inline constexpr int kAshleyIdleAnimation = 1;
inline constexpr int kAshleyWeaponDrawnAnimation = 101;
// Weapon reserved11 while drawn (224.c:320).
inline constexpr std::uint8_t kDrawnWeaponPose = 0x40;
// The preview's companion outline (224.c:124-134): two units along each axis
// (up for y), 16 depth steps deeper, white. The original left the colour's
// code byte uninitialized; it is 0 here.
inline constexpr Actors::CompanionStyle kPreviewOutline{
    .offset = {.x = 2, .y = -2, .z = 2, .depth_bias = 16},
    .color = {.red = 0xFF, .green = 0xFF, .blue = 0xFF, .code = 0},
};
// The companion pass draws 0xC0 bytes (48 entries) plus four bytes per depth
// bias step deeper in the scene ordering table (224.c:218-219).
inline constexpr int kCompanionOrderingEntries = 48;

// The preview model's frames per animation step: half the tick speed,
// rounded toward zero (224.c:200-201).
[[nodiscard]] constexpr std::uint8_t PreviewFramesPerStep(int tick_speed) {
  return static_cast<std::uint8_t>(tick_speed / 2);
}

// The scene ordering table entries the companion pass moves by.
[[nodiscard]] constexpr int CompanionOrderingEntries(std::uint8_t depth_bias) {
  return kCompanionOrderingEntries + depth_bias;
}

// MAINMENU.PRG func_800F9A24: clones, poses and animates actor `source_slot`.
void LoadActorPreview(ActorPreviewHost& host, int source_slot);
// MAINMENU.PRG func_800F9A78: queues the preview record, companion and
// weapon clones and waits for the queue.
void CloneActorPreview(ActorPreviewHost& host, int source_slot);
// MAINMENU.PRG func_800F9CB0.
void ResetActorPreviewPose(ActorPreviewHost& host);
// MAINMENU.PRG func_800F9E0C.
void ReleaseActorPreview(ActorPreviewHost& host);
// MAINMENU.PRG func_800F9EB8.
[[nodiscard]] PreviewDrawResult DrawActorPreview(ActorPreviewHost& host);
// MAINMENU.PRG func_800FA2CC.
void ResetAshleyEquipmentPose(ActorPreviewHost& host);
// MAINMENU.PRG func_800FA3FC: replaces Ashley's weapon sequences with those
// of sequence model `sequence_model_id`.
void QueueAshleyWeaponAnimations(ActorPreviewHost& host, int sequence_model_id);
}  // namespace VS::Menu::Framework
