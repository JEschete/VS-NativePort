// Hand-maintained (tools/port/owned.csv).
#include "Menu/Framework/ActorPreview.hpp"  // NOLINT(build/include): typed API

#include <cstdint>
#include <optional>

#include "Actors/ActorStateRecord.hpp"
#include "Content/ActorAssets.hpp"
#include "Render/DepthCue.hpp"

namespace VS::Menu::Framework {
namespace {
using Content::ActorAssetKind;
using Content::ActorAssetRequest;
using Content::AssetAddress;

// The clone requests' sub-kinds and the companion clone's texture slot
// (224.c:36, 50-51, 68).
constexpr std::uint8_t kCloneRecordSubKind = 0xFF;
constexpr std::uint8_t kCloneLinkedSubKind = 1;
constexpr std::uint8_t kCloneCompanionTextureSlot = 0xFC;
// The companion marker the pose reset gives a preview without a companion
// record: the outline (224.c:134).
constexpr int kOutlineCompanion = 1;
// The pose reset's fade: none (224.c:139-143).
constexpr Render::ColorCode kNoFade{};

[[nodiscard]] bool ValidActorSlot(int slot) {
  return slot >= 0 && slot < kActorSlots;
}

[[nodiscard]] bool ValidWeaponSlot(int slot) {
  return slot >= 0 && slot < kWeaponModelSlots;
}

[[nodiscard]] std::uint8_t Kind(ActorAssetKind kind) {
  return static_cast<std::uint8_t>(kind);
}

[[nodiscard]] std::int8_t TargetSlot(int slot) {
  return static_cast<std::int8_t>(slot);
}

// decomp: MAINMENU.PRG func_800F9A78 (src/MENU/MAINMENU.PRG/224.c:42-56)
bool QueueCompanionClone(ActorPreviewHost& host, const PreviewActor& source,
                         ActorAssetRequest& request) {
  const int companion_slot = source.companion_slot;
  // The original indexed the actor table with the byte unchecked and read
  // the model of a missing companion (224.c:43-47).
  if (!ValidActorSlot(companion_slot)) {
    host.Refuse("preview clone of a companion past the actor table");
    return true;
  }
  const std::optional<PreviewActor> companion = host.Actor(companion_slot);
  if (!companion) {
    host.Refuse("preview clone of a missing companion");
    return true;
  }
  request.kind = Kind(ActorAssetKind::kActorModelParse);
  request.target_slot = TargetSlot(kPreviewCompanionSlot);
  request.model_id = static_cast<std::int16_t>(companion->model_id);
  request.shared_record_slot = companion_slot;
  request.sub_kind = kCloneLinkedSubKind;
  request.texture_slot = kCloneCompanionTextureSlot;
  request.variant = source.variant;
  request.material = 0;
  return host.QueueAsset(request);
}

// decomp: MAINMENU.PRG func_800F9A78 (src/MENU/MAINMENU.PRG/224.c:58-73)
// The request keeps the variant the record clone set.
bool QueueWeaponClone(ActorPreviewHost& host, int source_slot,
                      const PreviewActor& source, int weapon,
                      ActorAssetRequest& request) {
  const int weapon_slot = (source.weapon_pair * kWeaponModelsPerActor) + weapon;
  // The original indexed the weapon models with the pair unchecked
  // (224.c:59-60).
  if (!ValidWeaponSlot(weapon_slot)) {
    host.Refuse("preview clone of a weapon past the weapon models");
    return true;
  }
  const std::optional<PreviewWeapon> model = host.Weapon(weapon_slot);
  if (!model) return true;
  request.kind = Kind(ActorAssetKind::kWeaponModelParse);
  request.target_slot = TargetSlot(kPreviewWeaponModelSlot + weapon);
  request.model_id = model->model_id;
  request.shared_record_slot = (source_slot * kWeaponModelsPerActor) + weapon;
  request.sub_kind = kCloneLinkedSubKind;
  request.texture_slot = model->attachment_slot;
  request.material = model->material;
  return host.QueueAsset(request);
}

// decomp: MAINMENU.PRG func_800F9EB8 (src/MENU/MAINMENU.PRG/224.c:216-233)
// The model pass reads the companion slot back after the motion update.
void DrawCompanionPass(ActorPreviewHost& host, const PreviewActor& actor) {
  host.OffsetOrderingTable(
      CompanionOrderingEntries(actor.companion.depth_bias));
  host.MoveActor(kPreviewActorSlot, actor.companion, 1);
  host.UpdateMotionAndLookAt(kPreviewActorSlot, actor.companion_slot);
  const PreviewActor moved = host.Actor(kPreviewActorSlot).value_or(actor);
  host.DrawModel(kPreviewActorSlot, moved.companion_slot);
  const PreviewActor after = host.Actor(kPreviewActorSlot).value_or(moved);
  host.OffsetOrderingTable(
      -CompanionOrderingEntries(after.companion.depth_bias));
  host.MoveActor(kPreviewActorSlot, after.companion, -1);
}

// decomp: MAINMENU.PRG func_800F9EB8 (src/MENU/MAINMENU.PRG/224.c:173-255)
void DrawPreviewWeapons(ActorPreviewHost& host, const PreviewActor& actor) {
  for (int weapon = 0; weapon < kWeaponModelsPerActor; ++weapon) {
    const int slot = kPreviewWeaponModelSlot + weapon;
    const std::optional<PreviewWeapon> model = host.Weapon(slot);
    if (!model || !model->attached) continue;
    // The original indexed the bone matrices with the byte unchecked
    // (224.c:244).
    if (model->attachment_bone >= kActorBoneMatrices) {
      host.Refuse("preview weapon on a bone past the bone matrices");
      continue;
    }
    host.DrawWeapon(slot, kPreviewActorSlot, model->attachment_bone,
                    actor.companion);
  }
}
}  // namespace

// decomp: MAINMENU.PRG func_800F9A24 (src/MENU/MAINMENU.PRG/224.c:14)
void LoadActorPreview(ActorPreviewHost& host, int source_slot) {
  if (!CloneActorPreview(host, source_slot)) return;
  ResetActorPreviewPose(host);
  // The original wrote the flag through the record unchecked (224.c:15).
  if (!host.Actor(kPreviewActorSlot)) {
    host.Refuse("preview load without a preview record");
    return;
  }
  host.SetWeaponDrawn(kPreviewActorSlot);
  host.PlayAnimation(kPreviewActorSlot, kPreviewAnimation);
}

// decomp: MAINMENU.PRG func_800F9A78 (src/MENU/MAINMENU.PRG/224.c:24)
bool CloneActorPreview(ActorPreviewHost& host, int source_slot) {
  // The original indexed the actor table unchecked and read a missing
  // record's model (224.c:29-33).
  if (!ValidActorSlot(source_slot)) {
    host.Refuse("preview clone of an actor past the actor table");
    return false;
  }
  const std::optional<PreviewActor> source = host.Actor(source_slot);
  if (!source) {
    host.Refuse("preview clone of a missing actor");
    return false;
  }
  if (!host.BeginPreview(source_slot)) return false;
  if (source_slot == kPreviewActorSlot) return true;
  // The original kept the source record's pointer across the asset queue
  // (224.c:29, 78-90).
  const AssetAddress source_block = host.ActorBlock(source_slot);
  ActorAssetRequest request{};
  request.kind = Kind(ActorAssetKind::kActorModelParse);
  request.target_slot = TargetSlot(kPreviewActorSlot);
  request.model_id = static_cast<std::int16_t>(source->model_id);
  request.shared_record_slot = source_slot;
  request.sub_kind = kCloneRecordSubKind;
  request.variant = source->variant;
  request.material = 0;
  bool queued = host.QueueAsset(request);
  if (queued && source->companion_slot >= Actors::kFirstCompanionRecord) {
    queued = QueueCompanionClone(host, *source, request);
  }
  for (int weapon = 0; queued && weapon < kWeaponModelsPerActor; ++weapon) {
    queued = QueueWeaponClone(host, source_slot, *source, weapon, request);
  }
  if (!queued) {
    host.Refuse("preview asset submission failed");
    host.ReleasePreviewAssets();
    return false;
  }
  if (!host.FinishPreviewAssets()) return false;
  const AssetAddress clone = host.ActorBlock(kPreviewActorSlot);
  // Ashley's clone copies her battle sequence, which her weapon changes
  // replace (QueueAshleyWeaponAnimations); other clones share the source's.
  const AssetAddress battle_copy =
      source->weapon_pair == 0
          ? host.Allocate(PreviewAllocation::kBattleSequence)
          : AssetAddress::kNone;
  if (source->weapon_pair == 0 && battle_copy == AssetAddress::kNone) {
    host.Refuse("preview battle sequence allocation failed");
    host.ReleasePreviewAssets();
    return false;
  }
  if (!host.CopySequences({.clone = clone,
                           .source = source_block,
                           .battle_copy = battle_copy})) {
    if (battle_copy != AssetAddress::kNone) host.Free(battle_copy);
    host.ReleasePreviewAssets();
    return false;
  }
  return true;
}

// decomp: MAINMENU.PRG func_800F9CB0 (src/MENU/MAINMENU.PRG/224.c:100)
void ResetActorPreviewPose(ActorPreviewHost& host) {
  for (int weapon = 0; weapon < kWeaponModelsPerActor; ++weapon) {
    const int slot = kPreviewWeaponModelSlot + weapon;
    if (host.Weapon(slot)) host.SetWeaponAttached(slot);
  }
  host.ResetMotion(kPreviewActorSlot);
  // The original wrote the placement through the record unchecked
  // (224.c:118-123).
  if (!host.Actor(kPreviewActorSlot)) {
    host.Refuse("preview pose without a preview record");
    return;
  }
  host.ResetPlacement(kPreviewActorSlot);
  if (host.Actor(kPreviewCompanionSlot)) {
    host.ClearSkipped(kPreviewCompanionSlot);
  } else {
    host.SetCompanion(kPreviewActorSlot, kOutlineCompanion, kPreviewOutline);
  }
  host.SetFade(kPreviewActorSlot, kNoFade);
  host.ClearTint(kPreviewActorSlot);
  host.ClearPreviewFlag(kPreviewActorSlot);
}

// decomp: MAINMENU.PRG func_800F9E0C (src/MENU/MAINMENU.PRG/224.c:149)
void ReleaseActorPreview(ActorPreviewHost& host) {
  host.ReleasePreviewAssets();
}

// decomp: MAINMENU.PRG func_800F9EB8 (src/MENU/MAINMENU.PRG/224.c:177)
PreviewDrawResult DrawActorPreview(ActorPreviewHost& host) {
  std::optional<PreviewActor> actor = host.Actor(kPreviewActorSlot);
  if (!actor || actor->bone_count == 0 || actor->skipped) {
    return PreviewDrawResult::kNotDrawn;
  }
  host.PrepareCamera();
  host.SetFramesPerStep(kPreviewActorSlot,
                        PreviewFramesPerStep(host.TickSpeed()));
  if (actor->frozen) {
    host.UpdateTextureScrolling(kPreviewActorSlot);
    host.UpdateAnimationBlend(kPreviewActorSlot);
  } else {
    host.PrepareRootTransform(kPreviewActorSlot);
  }
  host.DrawModel(kPreviewActorSlot, 0);
  actor = host.Actor(kPreviewActorSlot);
  if (!actor) return PreviewDrawResult::kNotDrawn;
  if (actor->companion_slot >= Actors::kFirstCompanionRecord) {
    host.UpdateTextureScrolling(kPreviewCompanionSlot);
  }
  if (actor->companion_slot != 0) DrawCompanionPass(host, *actor);
  actor = host.Actor(kPreviewActorSlot);
  if (!actor) return PreviewDrawResult::kNotDrawn;
  if (actor->culled) return PreviewDrawResult::kCulled;
  DrawPreviewWeapons(host, *actor);
  return PreviewDrawResult::kDrawn;
}

// decomp: MAINMENU.PRG func_800FA2CC (src/MENU/MAINMENU.PRG/224.c:306)
void ResetAshleyEquipmentPose(ActorPreviewHost& host) {
  // The original read the record unchecked (224.c:313-319).
  const std::optional<PreviewActor> ashley = host.Actor(kAshleyActorSlot);
  if (!ashley) {
    host.Refuse("Ashley's equipment pose without her record");
    return;
  }
  for (int slot = 0; slot < kWeaponModelsPerActor; ++slot) {
    if (!host.Weapon(slot)) continue;
    if (ashley->weapon_drawn) {
      host.SetWeaponPose(slot, kDrawnWeaponPose);
      host.SetWeaponAttached(slot);
    } else {
      host.SetWeaponPose(slot, 0);
    }
  }
  host.RefreshInteractionTile(kAshleyActorSlot);
  const bool drawn =
      host.Actor(kAshleyActorSlot).value_or(*ashley).weapon_drawn;
  host.SetAnimation(kAshleyActorSlot,
                    drawn ? kAshleyWeaponDrawnAnimation : kAshleyIdleAnimation);
  host.InitializePose(kAshleyActorSlot);
  host.AdvanceAnimation(kAshleyActorSlot);
  host.SnapshotBlendPose(kAshleyActorSlot);
  host.ClearPoseCounters(kAshleyActorSlot);
}

// decomp: MAINMENU.PRG func_800FA3FC (src/MENU/MAINMENU.PRG/224.c:352)
void QueueAshleyWeaponAnimations(ActorPreviewHost& host,
                                 int sequence_model_id) {
  host.ReleaseSequences(kAshleyActorSlot);
  // The original left the other request fields uninitialized
  // (224.c:354-361); they are zero here.
  ActorAssetRequest request{};
  request.kind = Kind(ActorAssetKind::kSequenceLoad);
  request.target_slot = TargetSlot(kAshleyActorSlot);
  request.model_id = static_cast<std::int16_t>(sequence_model_id);
  request.sub_kind = 0;
  if (!host.QueueAsset(request)) {
    host.Refuse("Ashley's weapon animation request failed");
  }
}
}  // namespace VS::Menu::Framework
