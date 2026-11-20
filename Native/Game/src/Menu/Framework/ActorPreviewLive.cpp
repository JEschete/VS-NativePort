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

#include "Actors/ActorRecord.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/ActorStateRecord.hpp"
#include "Actors/Animation.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/AnimationPoseLive.hpp"
#include "Actors/AppearanceLive.hpp"
#include "Actors/ModelDrawingLive.hpp"
#include "Actors/ModelRenderingLive.hpp"
#include "Actors/SequencePlaybackLive.hpp"
#include "Actors/SkeletonLiveState.hpp"
#include "Actors/TextureAnimationLive.hpp"
#include "Actors/WeaponModel.hpp"
#include "Content/ActorAssets.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Field/ChestInteractionsLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Framework/ActorPreview.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/DepthCue.hpp"
#include "Render/Frame.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"
#include "State/Timers.hpp"

using VS::Core::Gte::MATRIX;
using VS::Core::Gte::SVECTOR;

namespace VS::Menu::Framework {
namespace {
using Content::AssetAddress;

static_assert(kActorSlots == std::size(VS::Actors::actorRecords));
static_assert(kWeaponModelSlots == std::size(VS::Actors::weaponModels));
static_assert(kActorBoneMatrices ==
              std::extent_v<decltype(VS::Actors::ActorRecord::bone_matrices)>);

// Ashley's battle sequence copy (224.c:81).
constexpr std::size_t kBattleSequenceBytes = 0x7800;
// The loop counter the original passed as ResetLiveActorMotionState's unread
// argument (224.c:116).
constexpr int kMotionResetArgument = kWeaponModelsPerActor;

void Warn(std::string_view what) {
  VS::Core::LogWarn("menu.framework", "actor preview: {}", what);
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
      return sizeof(VS::Actors::ActorRecord);
    case PreviewAllocation::kWeaponModel:
      return sizeof(VS::Actors::WeaponModel);
    case PreviewAllocation::kBattleSequence:
      return kBattleSequenceBytes;
  }
  return 0;
}

[[nodiscard]] VS::Render::ColorCode LiveColor(const Render::ColorCode& color) {
  return {color.red, color.green, color.blue, color.code};
}

[[nodiscard]] std::int16_t Moved(std::int16_t value, int offset, int sign) {
  return static_cast<std::int16_t>(value + (sign * offset));
}

[[nodiscard]] PreviewActor PreviewOf(const VS::Actors::ActorRecord& record) {
  const VS::Actors::ModelInstance& instance = record.instance;
  const VS::Actors::CompanionOffsetRecord& companion = record.companion_state;
  return {.model_id = record.model_id,
          .variant = instance.variant,
          .weapon_pair = instance.slot,
          .companion_slot = record.companion_slot,
          .companion = {.x = companion.offset_x,
                        .y = companion.offset_y,
                        .z = companion.offset_z,
                        .depth_bias = companion.depth_bias},
          .bone_count = instance.bone_count,
          .skipped = instance.is_skipped != 0,
          .frozen = instance.is_frozen != 0,
          .culled = instance.is_culled != 0,
          .weapon_drawn = instance.weapon_drawn != 0};
}

[[nodiscard]] PreviewWeapon PreviewOf(const VS::Actors::WeaponModel& model) {
  return {.model_id = model.model_id,
          .attachment_slot = model.attachment_slot,
          .material = model.material,
          .attachment_bone = model.attachment_bone,
          .attached = model.shown != 0};
}

class LiveActorPreviewHost final : public ActorPreviewHost {
 public:
  explicit LiveActorPreviewHost(MATRIX* camera = nullptr) : camera_(camera) {}

  [[nodiscard]] std::optional<PreviewActor> Actor(int slot) const override {
    const VS::Actors::ActorRecord* record = Record(slot);
    if (record == nullptr) return std::nullopt;
    return PreviewOf(*record);
  }

  [[nodiscard]] std::optional<PreviewWeapon> Weapon(int slot) const override {
    const VS::Actors::WeaponModel* model = Model(slot);
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
    auto* preview = Pointer<VS::Actors::ActorRecord>(copy.clone);
    const auto* source = Pointer<VS::Actors::ActorRecord>(copy.source);
    if (preview == nullptr || source == nullptr) {
      Refuse("sequence copy without the clone or its source");
      return;
    }
    preview->common_sequence = source->common_sequence;
    if (copy.battle_copy != AssetAddress::kNone) {
      preview->battle_sequence = Pointer(copy.battle_copy);
      Content::CopyLiveWeaponSequence(
          static_cast<VS::Content::SequenceBuffer*>(preview->battle_sequence),
          static_cast<VS::Content::SequenceBuffer*>(source->battle_sequence));
    } else {
      preview->battle_sequence = source->battle_sequence;
    }
    preview->sequence_model_id = source->sequence_model_id;
    preview->common_entry_count = source->common_entry_count;
    preview->common_animation_count = source->common_animation_count;
  }

  void SetWeaponDrawn(int slot) override {
    if (VS::Actors::ActorRecord* record = Record(slot)) {
      record->instance.weapon_drawn = 1;
    }
  }

  void SetWeaponAttached(int weapon_slot) override {
    if (VS::Actors::WeaponModel* model = Model(weapon_slot)) {
      model->shown = 1;
    }
  }

  void SetWeaponPose(int weapon_slot, std::uint8_t pose) override {
    if (VS::Actors::WeaponModel* model = Model(weapon_slot)) {
      model->blend_level = pose;
    }
  }

  // decomp: MAINMENU.PRG func_800F9CB0 (src/MENU/MAINMENU.PRG/224.c:118-123)
  void ResetPlacement(int slot) override {
    VS::Actors::ActorRecord* record = Record(slot);
    if (record == nullptr) return;
    record->instance.position.vx = 0;
    record->instance.position.vy = 0;
    record->instance.position.vz = 0;
    record->root_key_pinned = 0;
    record->instance.is_skipped = 0;
    record->instance.sound_enabled = 0;
  }

  void ClearSkipped(int slot) override {
    if (VS::Actors::ActorRecord* record = Record(slot)) {
      record->instance.is_skipped = 0;
    }
  }

  void ClearPreviewFlag(int slot) override {
    if (VS::Actors::ActorRecord* record = Record(slot)) {
      record->floor_lit = 0;
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
    const VS::Render::ColorCode state{static_cast<std::uint8_t>(style.offset.x),
                                      static_cast<std::uint8_t>(style.offset.y),
                                      static_cast<std::uint8_t>(style.offset.z),
                                      style.offset.depth_bias};
    const VS::Render::ColorCode color = LiveColor(style.color);
    Actors::SetLiveActorCompanion(slot, companion_slot, &state, &color);
  }

  void SetFade(int slot, const Render::ColorCode& fade) override {
    const VS::Render::ColorCode color = LiveColor(fade);
    Actors::SetLiveActorFadeParameters(slot, &color);
  }

  void ClearTint(int slot) override { Actors::SetLiveActorTint(slot, nullptr); }

  void RefreshInteractionTile(int slot) override {
    if (VS::Actors::ActorRecord* record = Record(slot)) {
      static_cast<void>(Field::RefreshLiveAshleyInteractionTile(record));
    }
  }

  // decomp: MAINMENU.PRG func_800FA2CC (src/MENU/MAINMENU.PRG/224.c:336-340)
  void SetAnimation(int slot, int animation) override {
    VS::Actors::ActorRecord* record = Record(slot);
    if (record == nullptr) return;
    record->animation_id = static_cast<std::uint8_t>(animation);
    record->action_locked = 0;
    std::uint16_t* length = nullptr;
    record->animation_header =
        Actors::FindLiveActorAnimationHeader(record, animation, &length);
    record->animation_set =
        static_cast<std::uint8_t>(VS::Actors::lastResolvedSequenceIndex);
    // The original read the clip length through the header unchecked
    // (224.c:340).
    if (length == nullptr) {
      Refuse("Ashley's equipment pose without the animation's header");
      record->animation_length = 0;
      return;
    }
    record->animation_length = static_cast<std::int16_t>(*length);
  }

  void InitializePose(int slot) override {
    if (VS::Actors::ActorRecord* record = Record(slot)) {
      Actors::InitializeLiveActorAnimationPose(record, &record->pose);
    }
  }

  void AdvanceAnimation(int slot) override {
    if (VS::Actors::ActorRecord* record = Record(slot)) {
      Actors::AdvanceLiveSkeletalAnimation(*record, 1);
    }
  }

  void SnapshotBlendPose(int slot) override {
    if (VS::Actors::ActorRecord* record = Record(slot)) {
      Core::Memory::CopyLiveAlignedWords(&record->blend_pose, &record->pose,
                                         sizeof record->blend_pose);
    }
  }

  // decomp: MAINMENU.PRG func_800FA2CC (src/MENU/MAINMENU.PRG/224.c:346-349)
  void ClearPoseCounters(int slot) override {
    VS::Actors::ActorRecord* record = Record(slot);
    if (record == nullptr) return;
    record->pose.frame = 0;
    record->queued_animation = 0;
    record->blend_frames = 0;
    record->blend_frame = 0;
  }

  void PrepareCamera() override {
    if (camera_ != nullptr) Render::PrepareLiveActorCameraMatrices(camera_);
  }

  [[nodiscard]] int TickSpeed() const override { return VS::State::tickSpeed; }

  void SetFramesPerStep(int slot, std::uint8_t frames) override {
    if (VS::Actors::ActorRecord* record = Record(slot)) {
      record->frames_per_step = frames;
    }
  }

  // The original scrolled the companion record unchecked (224.c:212-213).
  void UpdateTextureScrolling(int slot) override {
    VS::Actors::ActorRecord* record = Record(slot);
    if (record == nullptr) {
      Refuse("texture scrolling of a missing record");
      return;
    }
    Actors::UpdateLiveActorTextureScrolling(record);
  }

  void UpdateAnimationBlend(int slot) override {
    if (VS::Actors::ActorRecord* record = Record(slot)) {
      Actors::UpdateLiveActorAnimationBlend(record);
    }
  }

  void PrepareRootTransform(int slot) override {
    if (VS::Actors::ActorRecord* record = Record(slot)) {
      Actors::PrepareLiveActorRootTransform(record);
    }
  }

  void DrawModel(int slot, int companion_slot) override {
    VS::Actors::ActorRecord* record = Record(slot);
    if (record == nullptr || camera_ == nullptr) return;
    Actors::DrawLiveActorModel(*record, *camera_, companion_slot);
  }

  // The original moved the scene ordering table by bytes, (depth bias * 4 +
  // 0xC0) / 4 entries of four bytes (224.c:218-219).
  void OffsetOrderingTable(int entries) override {
    if (VS::Render::FrameScratchpad().ordering_table == nullptr) return;
    VS::Render::FrameScratchpad().ordering_table =
        static_cast<std::uint32_t*>(
            VS::Render::FrameScratchpad().ordering_table) +
        entries;
  }

  void MoveActor(int slot, const Actors::CompanionOffset& offset,
                 int sign) override {
    VS::Actors::ActorRecord* record = Record(slot);
    if (record == nullptr) return;
    SVECTOR& position = record->instance.position;
    position.vx = Moved(position.vx, offset.x, sign);
    position.vy = Moved(position.vy, offset.y, sign);
    position.vz = Moved(position.vz, offset.z, sign);
  }

  void UpdateMotionAndLookAt(int slot, int companion_slot) override {
    if (VS::Actors::ActorRecord* record = Record(slot)) {
      Actors::UpdateLiveMotionAndLookAt(record, companion_slot);
    }
  }

  // decomp: MAINMENU.PRG func_800F9EB8 (src/MENU/MAINMENU.PRG/224.c:243-254)
  void DrawWeapon(int weapon_slot, int slot, std::uint8_t bone,
                  const Actors::CompanionOffset& offset) override {
    VS::Actors::WeaponModel* model = Model(weapon_slot);
    VS::Actors::ActorRecord* record = Record(slot);
    if (model == nullptr || record == nullptr) return;
    if (bone >= kActorBoneMatrices) {
      Refuse("weapon on a bone past the bone matrices");
      return;
    }
    MATRIX& matrix = record->bone_matrices[bone];
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
  [[nodiscard]] static VS::Actors::ActorRecord* Record(int slot) {
    if (slot < 0 || slot >= kActorSlots) {
      Warn("actor slot past the actor table");
      return nullptr;
    }
    return VS::Actors::actorRecords[slot];
  }

  [[nodiscard]] static VS::Actors::WeaponModel* Model(int slot) {
    if (slot < 0 || slot >= kWeaponModelSlots) {
      Warn("weapon slot past the weapon models");
      return nullptr;
    }
    return VS::Actors::weaponModels[slot];
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
