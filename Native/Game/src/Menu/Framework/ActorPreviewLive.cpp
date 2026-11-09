// Hand-maintained (tools/port/owned.csv).
// Live binding for the actor preview (ActorPreview.hpp): implements the host
// over the actor and weapon records, the game heap, the battle asset queue,
// the actor model, animation and appearance systems and the scene ordering
// table.
#include "Menu/Framework/ActorPreviewLive.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string_view>
#include <type_traits>

#include "Actors/ActorStateLive.hpp"
#include "Actors/ActorStateRecord.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/AnimationPoseLive.hpp"
#include "Actors/AppearanceLive.hpp"
#include "Actors/ModelDrawingLive.hpp"
#include "Actors/ModelRenderingLive.hpp"
#include "Actors/SequencePlaybackLive.hpp"
#include "Actors/TextureAnimationLive.hpp"
#include "Content/ActorAssets.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Field/ChestInteractionsLive.h"
#include "Menu/Framework/ActorPreview.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/DepthCue.hpp"

extern "C" {
#include "ActorAssets.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Movement.h"
#include "Actors/SkeletonLiveState.h"
#include "Actors/WeaponModel.h"
#include "Field/Gimmicks.h"
#include "Legacy/Psx.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/Timers.h"
}

namespace VS::Menu::Framework {
namespace {
using Content::AssetAddress;

static_assert(kActorSlots == std::size(Actors_actorRecords));
static_assert(kWeaponModelSlots == std::size(Actors_weaponModels));
static_assert(kActorBoneMatrices ==
              std::extent_v<decltype(Actors_ActorRecord::boneMatrices)>);

// Ashley's battle sequence copy (224.c:81).
constexpr std::size_t kBattleSequenceBytes = 0x7800;
// The loop counter the original passed as ResetLiveActorMotionState's unread
// argument (224.c:116).
constexpr int kMotionResetArgument = kWeaponModelsPerActor;

void Warn(std::string_view what) {
  VS_LOG_WARN("menu.framework", "actor preview: {}", what);
}

AssetAddress Address(const void* pointer) {
  return static_cast<AssetAddress>(reinterpret_cast<std::uintptr_t>(pointer));
}

template <typename T = void>
T* Pointer(AssetAddress address) {
  return reinterpret_cast<T*>(static_cast<std::uintptr_t>(address));
}

[[nodiscard]] std::size_t AllocationBytes(PreviewAllocation kind) {
  switch (kind) {
    case PreviewAllocation::kActorRecord:
      return sizeof(Actors_ActorRecord);
    case PreviewAllocation::kWeaponModel:
      return sizeof(Actors_WeaponModel);
    case PreviewAllocation::kBattleSequence:
      return kBattleSequenceBytes;
  }
  return 0;
}

[[nodiscard]] Render_ColorCode LiveColor(const Render::ColorCode& color) {
  return {color.red, color.green, color.blue, color.code};
}

[[nodiscard]] std::int16_t Moved(std::int16_t value, int offset, int sign) {
  return static_cast<std::int16_t>(value + (sign * offset));
}

[[nodiscard]] PreviewActor PreviewOf(const Actors_ActorRecord& record) {
  const Actors_ModelInstance& instance = record.instance;
  const Actors_CompanionOffset& companion = record.companionState;
  return {.model_id = record.modelId,
          .variant = instance.variant,
          .weapon_pair = instance.slot,
          .companion_slot = record.companionSlot,
          .companion = {.x = companion.offsetX,
                        .y = companion.offsetY,
                        .z = companion.offsetZ,
                        .depth_bias = companion.depthBias},
          .bone_count = instance.boneCount,
          .skipped = instance.isSkipped != 0,
          .frozen = instance.isFrozen != 0,
          .culled = instance.isPreviewModel != 0,
          .weapon_drawn = instance.weaponDrawn != 0};
}

[[nodiscard]] PreviewWeapon PreviewOf(const Actors_WeaponModel& model) {
  return {.model_id = model.modelId,
          .attachment_slot = model.attachmentSlot,
          .material = model.material,
          .attachment_bone = model.attachmentBone,
          .attached = model.reserved08Bit4 != 0};
}

class LiveActorPreviewHost final : public ActorPreviewHost {
 public:
  explicit LiveActorPreviewHost(MATRIX* camera = nullptr) : camera_(camera) {}

  [[nodiscard]] std::optional<PreviewActor> Actor(int slot) const override {
    const Actors_ActorRecord* record = Record(slot);
    if (record == nullptr) return std::nullopt;
    return PreviewOf(*record);
  }

  [[nodiscard]] std::optional<PreviewWeapon> Weapon(int slot) const override {
    const Actors_WeaponModel* model = Model(slot);
    if (model == nullptr) return std::nullopt;
    return PreviewOf(*model);
  }

  [[nodiscard]] AssetAddress Allocate(PreviewAllocation kind) override {
    return Address(Core::Memory::AllocateLiveFromLowEnd(AllocationBytes(kind)));
  }

  void Free(AssetAddress block) override {
    Core::Memory::FreeLiveFromLowEnd(Pointer(block));
  }

  [[nodiscard]] AssetAddress ActorBlock(int slot) const override {
    return Address(Record(slot));
  }

  [[nodiscard]] AssetAddress WeaponBlock(int slot) const override {
    return Address(Model(slot));
  }

  void QueueAsset(const Content::ActorAssetRequest& request) override {
    Content::QueueLiveActorAsset(request);
  }

  [[nodiscard]] bool UpdateAssetQueue() override {
    return Content::UpdateLiveActorAssetQueue() != 0;
  }

  void ReleaseModel(int slot) override { Content::ReleaseLiveActorModel(slot); }

  void ReleaseSequences(int slot) override {
    Content::ReleaseLiveActorSequence(slot, 0);
  }

  // decomp: MAINMENU.PRG func_800F9A78 (src/MENU/MAINMENU.PRG/224.c:78-90)
  void CopySequences(const PreviewSequenceCopy& copy) override {
    auto* preview = Pointer<Actors_ActorRecord>(copy.clone);
    const auto* source = Pointer<Actors_ActorRecord>(copy.source);
    if (preview == nullptr || source == nullptr) {
      Refuse("sequence copy without the clone or its source");
      return;
    }
    preview->commonSequence = source->commonSequence;
    if (copy.battle_copy != AssetAddress::kNone) {
      preview->battleSequence = Pointer(copy.battle_copy);
      Content::CopyLiveWeaponSequence(
          static_cast<Content_SequenceBuffer*>(preview->battleSequence),
          static_cast<Content_SequenceBuffer*>(source->battleSequence));
    } else {
      preview->battleSequence = source->battleSequence;
    }
    preview->sequenceModelId = source->sequenceModelId;
    preview->sequenceFlags = source->sequenceFlags;
    preview->commonAnimationCount = source->commonAnimationCount;
  }

  void SetWeaponDrawn(int slot) override {
    if (Actors_ActorRecord* record = Record(slot)) {
      record->instance.weaponDrawn = 1;
    }
  }

  void SetWeaponAttached(int weapon_slot) override {
    if (Actors_WeaponModel* model = Model(weapon_slot)) {
      model->reserved08Bit4 = 1;
    }
  }

  void SetWeaponPose(int weapon_slot, std::uint8_t pose) override {
    if (Actors_WeaponModel* model = Model(weapon_slot)) {
      model->reserved11 = pose;
    }
  }

  // decomp: MAINMENU.PRG func_800F9CB0 (src/MENU/MAINMENU.PRG/224.c:118-123)
  void ResetPlacement(int slot) override {
    Actors_ActorRecord* record = Record(slot);
    if (record == nullptr) return;
    record->instance.position.vx = 0;
    record->instance.position.vy = 0;
    record->instance.position.vz = 0;
    record->previewPose = 0;
    record->instance.isSkipped = 0;
    record->instance.soundEnabled = 0;
  }

  void ClearSkipped(int slot) override {
    if (Actors_ActorRecord* record = Record(slot)) {
      record->instance.isSkipped = 0;
    }
  }

  void ClearPreviewFlag(int slot) override {
    if (Actors_ActorRecord* record = Record(slot)) {
      record->previewFlag = 0;
    }
  }

  void PlayAnimation(int slot, int animation) override {
    Actors::PlayLiveActorAnimation(slot, animation, 0, 0);
  }

  void ResetMotion(int slot) override {
    Actors::ResetLiveActorMotionState(slot, kMotionResetArgument);
  }

  void SetCompanion(int slot, int companion_slot,
                    const Actors::CompanionStyle& style) override {
    const Render_ColorCode state{static_cast<u_char>(style.offset.x),
                                 static_cast<u_char>(style.offset.y),
                                 static_cast<u_char>(style.offset.z),
                                 style.offset.depth_bias};
    const Render_ColorCode color = LiveColor(style.color);
    Actors::SetLiveActorCompanion(slot, companion_slot, &state, &color);
  }

  void SetFade(int slot, const Render::ColorCode& fade) override {
    const Render_ColorCode color = LiveColor(fade);
    Actors::SetLiveActorFadeParameters(slot, &color);
  }

  void ClearTint(int slot) override { Actors::SetLiveActorTint(slot, nullptr); }

  void RefreshInteractionTile(int slot) override {
    if (Actors_ActorRecord* record = Record(slot)) {
      static_cast<void>(Field::RefreshLiveAshleyInteractionTile(record));
    }
  }

  // decomp: MAINMENU.PRG func_800FA2CC (src/MENU/MAINMENU.PRG/224.c:336-340)
  void SetAnimation(int slot, int animation) override {
    Actors_ActorRecord* record = Record(slot);
    if (record == nullptr) return;
    record->animationId = static_cast<u_char>(animation);
    record->weaponPoseActive = 0;
    u_short* length = nullptr;
    record->animationHeader =
        Actors::FindLiveActorAnimationHeader(record, animation, &length);
    record->animationSet =
        static_cast<u_char>(Actors_lastResolvedSequenceIndex);
    // The original read the clip length through the header unchecked
    // (224.c:340).
    if (length == nullptr) {
      Refuse("Ashley's equipment pose without the animation's header");
      record->animationLength = 0;
      return;
    }
    record->animationLength = static_cast<std::int16_t>(*length);
  }

  void InitializePose(int slot) override {
    if (Actors_ActorRecord* record = Record(slot)) {
      Actors::InitializeLiveActorAnimationPose(record, &record->pose);
    }
  }

  void AdvanceAnimation(int slot) override {
    if (Actors_ActorRecord* record = Record(slot)) {
      Actors::AdvanceLiveSkeletalAnimation(*record, 1);
    }
  }

  void SnapshotBlendPose(int slot) override {
    if (Actors_ActorRecord* record = Record(slot)) {
      Core::Memory::CopyLiveAlignedWords(&record->blendPose, &record->pose,
                                         sizeof record->blendPose);
    }
  }

  // decomp: MAINMENU.PRG func_800FA2CC (src/MENU/MAINMENU.PRG/224.c:346-349)
  void ClearPoseCounters(int slot) override {
    Actors_ActorRecord* record = Record(slot);
    if (record == nullptr) return;
    record->pose.frame = 0;
    record->queuedAnimation = 0;
    record->blendFrames = 0;
    record->blendFrame = 0;
  }

  void PrepareCamera() override {
    if (camera_ != nullptr) Render::PrepareLiveActorCameraMatrices(camera_);
  }

  [[nodiscard]] int TickSpeed() const override { return State_tickSpeed; }

  void SetFramesPerStep(int slot, std::uint8_t frames) override {
    if (Actors_ActorRecord* record = Record(slot)) {
      record->framesPerStep = frames;
    }
  }

  // The original scrolled the companion record unchecked (224.c:212-213).
  void UpdateTextureScrolling(int slot) override {
    Actors_ActorRecord* record = Record(slot);
    if (record == nullptr) {
      Refuse("texture scrolling of a missing record");
      return;
    }
    Actors::UpdateLiveActorTextureScrolling(record);
  }

  void UpdateAnimationBlend(int slot) override {
    if (Actors_ActorRecord* record = Record(slot)) {
      Actors::UpdateLiveActorAnimationBlend(record);
    }
  }

  void PrepareRootTransform(int slot) override {
    if (Actors_ActorRecord* record = Record(slot)) {
      Actors::PrepareLiveActorRootTransform(record);
    }
  }

  void DrawModel(int slot, int companion_slot) override {
    Actors_ActorRecord* record = Record(slot);
    if (record == nullptr || camera_ == nullptr) return;
    Actors::DrawLiveActorModel(*record, *camera_, companion_slot);
  }

  // The original moved the scene ordering table by bytes, (depth bias * 4 +
  // 0xC0) / 4 entries of four bytes (224.c:218-219).
  void OffsetOrderingTable(int entries) override {
    if (Render_frameScratch.orderingTable == nullptr) return;
    Render_frameScratch.orderingTable =
        static_cast<u_long*>(Render_frameScratch.orderingTable) + entries;
  }

  void MoveActor(int slot, const Actors::CompanionOffset& offset,
                 int sign) override {
    Actors_ActorRecord* record = Record(slot);
    if (record == nullptr) return;
    SVECTOR& position = record->instance.position;
    position.vx = Moved(position.vx, offset.x, sign);
    position.vy = Moved(position.vy, offset.y, sign);
    position.vz = Moved(position.vz, offset.z, sign);
  }

  void UpdateMotionAndLookAt(int slot, int companion_slot) override {
    if (Actors_ActorRecord* record = Record(slot)) {
      Actors::UpdateLiveMotionAndLookAt(record, companion_slot);
    }
  }

  // decomp: MAINMENU.PRG func_800F9EB8 (src/MENU/MAINMENU.PRG/224.c:243-254)
  void DrawWeapon(int weapon_slot, int slot, std::uint8_t bone,
                  const Actors::CompanionOffset& offset) override {
    Actors_WeaponModel* model = Model(weapon_slot);
    Actors_ActorRecord* record = Record(slot);
    if (model == nullptr || record == nullptr) return;
    if (bone >= kActorBoneMatrices) {
      Refuse("weapon on a bone past the bone matrices");
      return;
    }
    MATRIX& matrix = record->boneMatrices[bone];
    matrix.t[0] -= offset.x;
    matrix.t[1] -= offset.y;
    matrix.t[2] -= offset.z;
    Actors::DrawLiveActorWeaponModel(model, &matrix);
    matrix.t[0] += offset.x;
    matrix.t[1] += offset.y;
    matrix.t[2] += offset.z;
  }

  void Refuse(std::string_view what) override { Warn(what); }

 private:
  [[nodiscard]] static Actors_ActorRecord* Record(int slot) {
    if (slot < 0 || slot >= kActorSlots) {
      Warn("actor slot past the actor table");
      return nullptr;
    }
    return Actors_actorRecords[slot];
  }

  [[nodiscard]] static Actors_WeaponModel* Model(int slot) {
    if (slot < 0 || slot >= kWeaponModelSlots) {
      Warn("weapon slot past the weapon models");
      return nullptr;
    }
    return Actors_weaponModels[slot];
  }

  MATRIX* camera_;
};
}  // namespace

void LoadLiveActorPreview(int source_actor_slot) {
  LiveActorPreviewHost host;
  LoadActorPreview(host, source_actor_slot);
}

void CloneLiveActorPreview(int source_actor_slot) {
  LiveActorPreviewHost host;
  CloneActorPreview(host, source_actor_slot);
}

void ResetLiveActorPreviewPose() {
  LiveActorPreviewHost host;
  ResetActorPreviewPose(host);
}

void ReleaseLiveActorPreview() {
  LiveActorPreviewHost host;
  ReleaseActorPreview(host);
}

int DrawLiveActorPreview(MATRIX* preview_camera_matrix) {
  if (preview_camera_matrix == nullptr) {
    Warn("draw without a camera matrix");
    return static_cast<int>(PreviewDrawResult::kNotDrawn);
  }
  LiveActorPreviewHost host(preview_camera_matrix);
  return static_cast<int>(DrawActorPreview(host));
}

void ResetLiveAshleyEquipmentPose() {
  LiveActorPreviewHost host;
  ResetAshleyEquipmentPose(host);
}

void QueueLiveAshleyWeaponAnimations(int sequence_model_id) {
  LiveActorPreviewHost host;
  QueueAshleyWeaponAnimations(host, sequence_model_id);
}
}  // namespace VS::Menu::Framework
