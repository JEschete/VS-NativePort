// Hand-maintained (tools/port/owned.csv).
// Live binding for the encyclopedia preview (EncyclopediaPreview.hpp):
// implements the host over the shared model load step, the ZUD file table,
// the disc queue, the game heap, the battle asset queue and the actor
// records.
#include "Menu/Score/EncyclopediaPreviewLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <string_view>

#include "Actors/ActorRecord.h"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/AnimationPoseLive.hpp"
#include "Actors/Movement.h"
#include "Actors/SkeletonLiveState.hpp"
#include "Actors/WeaponModel.h"
#include "Content/ActorAssets.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "DiscQueue.h"
#include "Legacy/Psx.h"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Score/Encyclopedia.hpp"
#include "Menu/Score/EncyclopediaPreview.hpp"

namespace VS::Menu::Score {
namespace {
static_assert(kEnemyPreviewActorSlot < std::size(Actors_actorRecords));

// The model buffer's parts (592C.c:197-199, 229-253): the enemy's record,
// then a companion record or the weapon and shield models.
constexpr std::size_t kRecordBytes = sizeof(Actors_ActorRecord);
constexpr std::size_t kWeaponBytes = sizeof(Actors_WeaponModel);
constexpr std::size_t kAttachmentOffset = kRecordBytes;
constexpr std::size_t kShieldOffset = kRecordBytes + kWeaponBytes;
constexpr std::size_t kCompanionBufferBytes = 2 * kRecordBytes;
constexpr std::size_t kEquippedBufferBytes = kRecordBytes + (2 * kWeaponBytes);

EnemyPreviewState live_state;
std::uint64_t refusals = 0;

void Report(std::string_view what) {
  ++refusals;
  VS::Core::LogWarn("menu.score", "encyclopedia preview: {}", what);
}

AssetAddress Address(const void* pointer) {
  return static_cast<AssetAddress>(reinterpret_cast<std::uintptr_t>(pointer));
}

template <typename T = void>
T* Pointer(AssetAddress address) {
  return reinterpret_cast<T*>(static_cast<std::uintptr_t>(address));
}

[[nodiscard]] Content_DiscFile DiscFile(const EnemyPreviewFile& file) {
  return {.lba = file.sector, .size = file.bytes};
}

[[nodiscard]] std::size_t PartOffset(EnemyPreviewPart part) {
  switch (part) {
    case EnemyPreviewPart::kEnemy:
      return 0;
    case EnemyPreviewPart::kCompanion:
    case EnemyPreviewPart::kWeapon:
      return kAttachmentOffset;
    case EnemyPreviewPart::kShield:
      return kShieldOffset;
  }
  return 0;
}

[[nodiscard]] std::size_t PartBytes(EnemyPreviewPart part) {
  switch (part) {
    case EnemyPreviewPart::kEnemy:
    case EnemyPreviewPart::kCompanion:
      return sizeof(Actors_ActorRecord);
    case EnemyPreviewPart::kWeapon:
    case EnemyPreviewPart::kShield:
      return sizeof(Actors_WeaponModel);
  }
  return 0;
}

class LiveEnemyPreviewHost final : public EnemyPreviewHost {
 public:
  [[nodiscard]] std::uint8_t LoadStep() const override {
    return static_cast<std::uint8_t>(Menu_Inventory_equipmentModelLoadStep);
  }

  void SetLoadStep(std::uint8_t step) override {
    Menu_Inventory_equipmentModelLoadStep = static_cast<char>(step);
  }

  [[nodiscard]] EnemyPreviewFile File(int zud_id) const override {
    if (zud_id < 0 || zud_id >= kEnemyPreviewFiles) return {};
    const auto index = static_cast<std::size_t>(zud_id);
    return {.sector = zudLbas[index],
            .bytes = zudSizes[index],
            .palette_flag = static_cast<std::uint8_t>(paletteFlags[index])};
  }

  [[nodiscard]] AssetAddress AllocateRead(
      const EnemyPreviewFile& file) override {
    const Content_DiscFile disc = DiscFile(file);
    auto* request = Content::AllocateLiveDiscRequest(&disc);
    live_state.read_generation = Content::LiveDiscRequestGeneration(request);
    return Address(request);
  }

  void EnqueueRead(AssetAddress read, AssetAddress destination) override {
    Content::EnqueueLivePriorityDiscRequest(
        Pointer<Content_DiscQueueSlot>(read), Pointer(destination));
  }

  [[nodiscard]] AssetAddress ReadRefused(const EnemyPreviewFile& file,
                                         AssetAddress destination) override {
    const Content_DiscFile disc = DiscFile(file);
    auto* request =
        Content::CompleteLiveRefusedDiscRequest(&disc, Pointer(destination));
    live_state.read_generation = Content::LiveDiscRequestGeneration(request);
    return Address(request);
  }

  [[nodiscard]] bool ReadLoaded(AssetAddress read) const override {
    const auto* request = Pointer<const Content_DiscQueueSlot>(read);
    return request != nullptr &&
           request->state == Content_DiscRequestState_Loaded;
  }

  void ReleaseRead(AssetAddress read) override {
    Content::ReleaseLiveDiscRequest(Pointer<Content_DiscQueueSlot>(read));
    live_state.read_generation = 0;
  }

  [[nodiscard]] AssetAddress AllocateFile(std::uint32_t bytes) override {
    return Address(Core::Memory::AllocateLiveFromHighEnd(bytes));
  }

  void FreeFile(AssetAddress file) override {
    auto* pointer = Pointer<std::byte>(file);
    const auto capacity = Core::Memory::LivePayloadCapacity(pointer);
    if (!capacity.has_value() ||
        !Content::CancelLiveActorAssetBorrows({pointer, *capacity}, {})) {
      Report("ZUD borrow cleanup refused before file release");
      std::abort();
    }
    Core::Memory::FreeLiveFromHighEnd(pointer);
  }

  [[nodiscard]] AssetAddress AllocateModelBuffer(
      EnemyPreviewLayout layout) override {
    return Address(Core::Memory::AllocateLiveFromLowEnd(
        layout == EnemyPreviewLayout::kCompanion ? kCompanionBufferBytes
                                                 : kEquippedBufferBytes));
  }

  // The original freed the low-end buffer with freeHeapR; the heap's frees
  // find the same neighbours from either end.
  void FreeModelBuffer(AssetAddress buffer) override {
    Core::Memory::FreeLiveFromHighEnd(Pointer(buffer));
  }

  [[nodiscard]] AssetAddress ModelPart(AssetAddress buffer,
                                       EnemyPreviewPart part) const override {
    const auto capacity = Core::Memory::LivePayloadCapacity(Pointer(buffer));
    const std::size_t offset = PartOffset(part);
    if (!capacity.has_value() || *capacity < offset + PartBytes(part)) {
      Report("model part past the model buffer");
      return AssetAddress::kNone;
    }
    return Content::AdvanceAddress(buffer, static_cast<std::ptrdiff_t>(offset));
  }

  [[nodiscard]] std::span<const std::byte> FileBytes(
      AssetAddress file, std::uint32_t bytes) const override {
    const auto* data = Pointer<const std::byte>(file);
    const auto capacity = Core::Memory::LivePayloadCapacity(data);
    if (!capacity.has_value() || *capacity < bytes) {
      Report("ZUD file past its heap block");
      return {};
    }
    return {data, bytes};
  }

  void QueueAsset(const Content::ActorAssetRequest& request) override {
    Content::QueueLiveActorAsset(request);
  }

  void UpdateAssetQueue() override { Content::UpdateLiveActorAssetQueue(); }

  [[nodiscard]] int FirstFreeAssetRequest() const override {
    return Content::FindLiveFreeActorAssetRequest();
  }

  void ReleaseModel(int slot) override { Content::ReleaseLiveActorModel(slot); }

  void ResetPreviewPose() override { Framework::ResetLiveActorPreviewPose(); }

  void SetWeaponDrawn(int slot, bool drawn) override {
    if (Actors_ActorRecord* record = Record(slot)) {
      record->instance.weaponDrawn = drawn ? 1 : 0;
    }
  }

  void SetAnimation(int slot, std::uint8_t animation) override {
    if (Actors_ActorRecord* record = Record(slot)) {
      record->animationId = animation;
    }
  }

  void PlayAnimation(int slot, int animation, int blend_frames) override {
    Actors::PlayLiveActorAnimation(slot, animation, 0, blend_frames);
  }

  // decomp: MENU9.PRG func_801080C8 (src/MENU/MENU9.PRG/57BC.c:56): the
  // pose blends from the record's own blend pose (unk704).
  void InitializeAnimationPose(int slot) override {
    if (Actors_ActorRecord* record = Record(slot)) {
      Actors::InitializeLiveActorAnimationPose(record, &record->blendPose);
    }
  }

  void SetDisplayFlags(int slot, int flags) override {
    Actors::SetLiveActorDisplayFlags(slot, flags);
  }

  void ClearCompanion(int slot) override {
    Actors::SetLiveActorCompanion(slot, 0, nullptr, nullptr);
  }

  void Refuse(std::string_view what) override { Report(what); }

 private:
  [[nodiscard]] static Actors_ActorRecord* Record(int slot) {
    if (slot < 0 ||
        static_cast<std::size_t>(slot) >= std::size(Actors_actorRecords)) {
      Report("actor slot past the actor table");
      return nullptr;
    }
    Actors_ActorRecord* record =
        Actors_actorRecords[static_cast<std::size_t>(slot)];
    if (record == nullptr) Report("preview actor record missing");
    return record;
  }
};

LiveEnemyPreviewHost& Host() {
  static LiveEnemyPreviewHost host;
  return host;
}

std::optional<std::span<std::byte>> PreviewAllocation(AssetAddress address,
                                                      std::size_t minimum) {
  if (address == AssetAddress::kNone) return std::span<std::byte>{};
  auto* pointer = Pointer<std::byte>(address);
  const auto capacity = Core::Memory::LivePayloadCapacity(pointer);
  if (!capacity.has_value() || *capacity < minimum) {
    Report("cleanup allocation no longer belongs to the preview");
    return std::nullopt;
  }
  return std::span<std::byte>{pointer, *capacity};
}

bool InPreviewBuffer(std::span<const std::byte> buffer, const void* pointer) {
  const auto begin = reinterpret_cast<std::uintptr_t>(buffer.data());
  const auto value = reinterpret_cast<std::uintptr_t>(pointer);
  return !buffer.empty() && value >= begin && value - begin < buffer.size();
}

bool ValidatePreviewRecords(std::span<const std::byte> buffer) {
  for (std::size_t slot = 0; slot < std::size(Actors_actorRecords); ++slot) {
    const auto* actor = Actors_actorRecords[slot];
    if (!InPreviewBuffer(buffer, actor)) continue;
    const auto offset = reinterpret_cast<std::uintptr_t>(actor) -
                        reinterpret_cast<std::uintptr_t>(buffer.data());
    if (sizeof(*actor) > buffer.size() - offset) {
      Report("cleanup actor record extends past the preview");
      return false;
    }
    if (slot >= std::size(VS::Actors::weaponModels) / 2) continue;
    for (const auto weapon_slot : {slot * 2, (slot * 2) | 1}) {
      const auto* weapon = VS::Actors::weaponModels[weapon_slot];
      if (weapon != nullptr && !InPreviewBuffer(buffer, weapon)) {
        Report("cleanup actor has a foreign weapon owner");
        return false;
      }
    }
  }
  for (const auto* weapon : VS::Actors::weaponModels) {
    if (!InPreviewBuffer(buffer, weapon)) continue;
    const auto offset = reinterpret_cast<std::uintptr_t>(weapon) -
                        reinterpret_cast<std::uintptr_t>(buffer.data());
    if (sizeof(*weapon) > buffer.size() - offset) {
      Report("cleanup weapon record extends past the preview");
      return false;
    }
  }
  return true;
}

void ReleasePreviewRecords(std::span<const std::byte> buffer) {
  for (std::size_t slot = 0; slot < std::size(Actors_actorRecords); ++slot) {
    if (InPreviewBuffer(buffer, Actors_actorRecords[slot])) {
      Content::ReleaseLiveActorModel(static_cast<int>(slot));
    }
  }
  for (std::size_t slot = 0; slot < std::size(VS::Actors::weaponModels);
       ++slot) {
    if (InPreviewBuffer(buffer, VS::Actors::weaponModels[slot])) {
      Content::ReleaseLiveWeaponModel(static_cast<int>(slot));
    }
  }
}
}  // namespace

// decomp: MENU9.PRG vs_menu9_loadZud (F02836, src/MENU/MENU9.PRG/57BC.c:10)
int LoadLiveEnemyPreview(int zud_id) {
  return LoadEnemyPreview(Host(), live_state, zud_id);
}

// decomp: MENU9.PRG vs_menu9_freeZud (F02837, src/MENU/MENU9.PRG/57BC.c:42)
void FreeLiveEnemyPreview() { FreeEnemyPreview(Host(), live_state); }

// decomp: MENU9.PRG func_801080C8 (F02838, src/MENU/MENU9.PRG/57BC.c:49)
void InitializeLiveAshleyPreviewPose() { InitializeAshleyPreviewPose(Host()); }

// decomp: MENU9.PRG vs_menu9_loadZudFile (F02839,
// src/MENU/MENU9.PRG/592C.c:171)
void QueueLiveEnemyModelRead(int zud_id) {
  QueueEnemyModelRead(Host(), live_state, zud_id);
}

// decomp: MENU9.PRG vs_menu9_parseZudFile (F02840,
// src/MENU/MENU9.PRG/592C.c:203)
int LoadLiveEnemyModelParts() {
  return LoadEnemyModelParts(Host(), live_state);
}

// decomp: MENU9.PRG vs_menu9_freeZudFile (F02841,
// src/MENU/MENU9.PRG/592C.c:295)
void FreeLiveEnemyModelBuffer() { FreeEnemyModelBuffer(Host(), live_state); }

bool CleanupLiveEnemyPreviewOwnedState() {
  const bool owned = live_state.read != AssetAddress::kNone ||
                     live_state.file != AssetAddress::kNone ||
                     live_state.model_buffer != AssetAddress::kNone;
  const auto file = PreviewAllocation(live_state.file, live_state.file_bytes);
  const auto model = PreviewAllocation(live_state.model_buffer, kRecordBytes);
  if (!file || !model || !ValidatePreviewRecords(*model)) return false;
  if (!file->empty() && !Content::CancelLiveDiscWrites(*file)) return false;
  if (!Content::CancelLiveActorAssetBorrows(*file, *model)) return false;
  auto* request = Pointer<Content_DiscQueueSlot>(live_state.read);
  if (live_state.read_generation != 0 &&
      Content::LiveDiscRequestGeneration(request) ==
          live_state.read_generation) {
    Content::ReleaseLiveDiscRequest(request);
  }
  ReleasePreviewRecords(*model);
  if (!file->empty()) Core::Memory::FreeLiveFromHighEnd(file->data());
  if (!model->empty()) Core::Memory::FreeLiveFromHighEnd(model->data());
  live_state = {};
  if (owned) Host().SetLoadStep(0);
  return true;
}

void ResetLiveEnemyPreview() { live_state = {}; }

EnemyPreviewState& LiveEnemyPreviewState() noexcept { return live_state; }

std::uint64_t LiveEnemyPreviewRefusals() noexcept { return refusals; }
}  // namespace VS::Menu::Score
