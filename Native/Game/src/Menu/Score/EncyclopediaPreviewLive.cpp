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

#include "Actors/ActorRecord.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/AnimationPoseLive.hpp"
#include "Actors/SkeletonLiveState.hpp"
#include "Actors/WeaponModel.hpp"
#include "Content/ActorAssets.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscQueue.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/Score/Encyclopedia.hpp"
#include "Menu/Score/EncyclopediaPreview.hpp"

namespace VS::Menu::Score {
namespace {
static_assert(kEnemyPreviewActorSlot < std::size(VS::Actors::actorRecords));
static_assert(kEnemyPreviewCompanionSlot < std::size(VS::Actors::actorRecords));
static_assert(kEnemyPreviewShieldSlot < std::size(VS::Actors::weaponModels));

// The model buffer's parts (592C.c:197-199, 229-253): the enemy's record,
// then a companion record or the weapon and shield models.
constexpr std::size_t kRecordBytes = sizeof(VS::Actors::ActorRecord);
constexpr std::size_t kWeaponBytes = sizeof(VS::Actors::WeaponModel);
constexpr std::size_t kAttachmentOffset = kRecordBytes;
constexpr std::size_t kShieldOffset = kRecordBytes + kWeaponBytes;
constexpr std::size_t kCompanionBufferBytes = 2 * kRecordBytes;
constexpr std::size_t kEquippedBufferBytes = kRecordBytes + (2 * kWeaponBytes);

EnemyPreviewState liveState;
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

[[nodiscard]] VS::Content::DiscFile DiscFile(const EnemyPreviewFile& file) {
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
      return sizeof(VS::Actors::ActorRecord);
    case EnemyPreviewPart::kWeapon:
    case EnemyPreviewPart::kShield:
      return sizeof(VS::Actors::WeaponModel);
  }
  return 0;
}

bool InPreviewBuffer(std::span<const std::byte> buffer, const void* pointer) {
  const auto begin = reinterpret_cast<std::uintptr_t>(buffer.data());
  const auto value = reinterpret_cast<std::uintptr_t>(pointer);
  return !buffer.empty() && value >= begin && value - begin < buffer.size();
}

// Releases actor `slot` when it is bound at `address` in the model buffer.
// Releasing actor 1 releases weapons 2 and 3 with it, so it is kept while
// either belongs to another owner.
bool ReleaseActor(AssetAddress buffer, AssetAddress address, int slot) {
  VS::Actors::ActorRecord*& record =
      VS::Actors::actorRecords[static_cast<std::size_t>(slot)];
  if (Address(record) != address) return true;
  if (static_cast<std::size_t>(slot) <
      std::size(VS::Actors::weaponModels) / 2) {
    const auto* data = Pointer<const std::byte>(buffer);
    const auto capacity = Core::Memory::LivePayloadCapacity(data);
    if (!capacity.has_value()) {
      Report("preview model buffer left the heap");
      return false;
    }
    const std::span<const std::byte> owned{data, *capacity};
    const auto weapon = static_cast<std::size_t>(slot) * 2;
    for (const std::size_t weapon_slot : {weapon, weapon | 1U}) {
      const auto* model = VS::Actors::weaponModels[weapon_slot];
      if (model != nullptr && !InPreviewBuffer(owned, model)) {
        Report("preview actor holds another owner's weapon: kept");
        return false;
      }
    }
  }
  Content::ReleaseLiveActorModel(slot);
  if (record != nullptr) {
    Report("preview actor record stays bound");
    return false;
  }
  return true;
}

// Releases weapon `slot` when it is bound at `address` in the model buffer.
bool ReleaseWeapon(AssetAddress address, int slot) {
  VS::Actors::WeaponModel*& model =
      VS::Actors::weaponModels[static_cast<std::size_t>(slot)];
  if (Address(model) != address) return true;
  Content::ReleaseLiveWeaponModel(slot);
  if (model != nullptr) {
    Report("preview weapon record stays bound");
    return false;
  }
  return true;
}

class LiveEnemyPreviewHost final : public EnemyPreviewHost {
 public:
  [[nodiscard]] std::uint8_t LoadStep() const override {
    return VS::Content::objectDataLoadStep;
  }

  void SetLoadStep(std::uint8_t step) override {
    VS::Content::objectDataLoadStep = step;
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
    const VS::Content::DiscFile disc = DiscFile(file);
    auto* request = Content::AllocateLiveDiscRequest(&disc);
    liveState.read_generation = Content::LiveDiscRequestGeneration(request);
    return Address(request);
  }

  void EnqueueRead(AssetAddress read, AssetAddress destination) override {
    Content::EnqueueLivePriorityDiscRequest(
        Pointer<VS::Content::DiscQueueSlot>(read), Pointer(destination));
  }

  [[nodiscard]] AssetAddress ReadRefused(const EnemyPreviewFile& file,
                                         AssetAddress destination) override {
    const VS::Content::DiscFile disc = DiscFile(file);
    auto* request =
        Content::CompleteLiveRefusedDiscRequest(&disc, Pointer(destination));
    liveState.read_generation = Content::LiveDiscRequestGeneration(request);
    return Address(request);
  }

  [[nodiscard]] EnemyPreviewRead ReadStatus(AssetAddress read) const override {
    const auto* request = Pointer<const VS::Content::DiscQueueSlot>(read);
    // A request recycled for another read is not this preview's.
    if (request == nullptr || liveState.read_generation == 0 ||
        Content::LiveDiscRequestGeneration(request) !=
            liveState.read_generation) {
      Report("ZUD read no longer belongs to the preview");
      return EnemyPreviewRead::kFailed;
    }
    switch (request->state) {
      case VS::Content::DiscRequestState::kLoaded:
        return EnemyPreviewRead::kLoaded;
      case VS::Content::DiscRequestState::kError:
        return EnemyPreviewRead::kFailed;
      default:
        return EnemyPreviewRead::kPending;
    }
  }

  void ReleaseRead(AssetAddress read) override {
    auto* request = Pointer<VS::Content::DiscQueueSlot>(read);
    if (liveState.read_generation != 0 &&
        Content::LiveDiscRequestGeneration(request) ==
            liveState.read_generation) {
      Content::ReleaseLiveDiscRequest(request);
    }
    liveState.read_generation = 0;
  }

  [[nodiscard]] AssetAddress AllocateFile(std::uint32_t bytes) override {
    return Address(Core::Memory::AllocateLiveFromHighEnd(bytes));
  }

  // The preview's parts are cancelled or loaded before its file is freed.
  void FreeFile(AssetAddress file) override {
    Core::Memory::FreeLiveFromHighEnd(Pointer(file));
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

  [[nodiscard]] std::optional<Content::ActorAssetTicket> SubmitAsset(
      const Content::ActorAssetRequest& request) override {
    return Content::SubmitLiveActorAsset(
        request, Content::ActorAssetOwner::kEnemyPreview);
  }

  void UpdateAssetQueue() override { Content::UpdateLiveActorAssetQueue(); }

  [[nodiscard]] Content::ActorAssetOutcome AssetOutcome(
      Content::ActorAssetTicket ticket) override {
    return Content::LiveActorAssetCompletion(ticket).outcome;
  }

  void ForgetAsset(Content::ActorAssetTicket ticket) override {
    Content::ForgetLiveActorAsset(ticket);
  }

  [[nodiscard]] bool CancelAssets() override {
    return Content::CancelLiveActorAssetRequests(
        Content::ActorAssetOwner::kEnemyPreview);
  }

  // Releases the record bound at `part` of `buffer`, if any: actor 1 with
  // the weapons 2 and 3 it owns, actor 16, or weapon 2 or 3.
  [[nodiscard]] bool ReleasePart(AssetAddress buffer,
                                 EnemyPreviewPart part) override {
    const AssetAddress address = Content::AdvanceAddress(
        buffer, static_cast<std::ptrdiff_t>(PartOffset(part)));
    switch (part) {
      case EnemyPreviewPart::kEnemy:
        return ReleaseActor(buffer, address, kEnemyPreviewActorSlot);
      case EnemyPreviewPart::kCompanion:
        return ReleaseActor(buffer, address, kEnemyPreviewCompanionSlot);
      case EnemyPreviewPart::kWeapon:
        return ReleaseWeapon(address, kEnemyPreviewWeaponSlot);
      case EnemyPreviewPart::kShield:
        return ReleaseWeapon(address, kEnemyPreviewShieldSlot);
    }
    return false;
  }

  void ResetPreviewPose() override { Framework::ResetLiveActorPreviewPose(); }

  void SetWeaponDrawn(int slot, bool drawn) override {
    if (VS::Actors::ActorRecord* record = Record(slot)) {
      record->instance.weapon_drawn = drawn ? 1 : 0;
    }
  }

  void SetAnimation(int slot, std::uint8_t animation) override {
    if (VS::Actors::ActorRecord* record = Record(slot)) {
      record->animation_id = animation;
    }
  }

  void PlayAnimation(int slot, int animation, int blend_frames) override {
    Actors::PlayLiveActorAnimation(slot, animation, 0, blend_frames);
  }

  // decomp: MENU9.PRG func_801080C8 (src/MENU/MENU9.PRG/57BC.c:56): the
  // pose blends from the record's own blend pose (unk704).
  void InitializeAnimationPose(int slot) override {
    if (VS::Actors::ActorRecord* record = Record(slot)) {
      Actors::InitializeLiveActorAnimationPose(record, &record->blend_pose);
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
  [[nodiscard]] static VS::Actors::ActorRecord* Record(int slot) {
    if (slot < 0 ||
        static_cast<std::size_t>(slot) >= std::size(VS::Actors::actorRecords)) {
      Report("actor slot past the actor table");
      return nullptr;
    }
    VS::Actors::ActorRecord* record =
        VS::Actors::actorRecords[static_cast<std::size_t>(slot)];
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

bool ValidatePreviewRecords(std::span<const std::byte> buffer) {
  for (std::size_t slot = 0; slot < std::size(VS::Actors::actorRecords);
       ++slot) {
    const auto* actor = VS::Actors::actorRecords[slot];
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
  for (std::size_t slot = 0; slot < std::size(VS::Actors::actorRecords);
       ++slot) {
    if (InPreviewBuffer(buffer, VS::Actors::actorRecords[slot])) {
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
  return LoadEnemyPreview(Host(), liveState, zud_id);
}

// decomp: MENU9.PRG vs_menu9_freeZud (F02837, src/MENU/MENU9.PRG/57BC.c:42)
void FreeLiveEnemyPreview() { FreeEnemyPreview(Host(), liveState); }

// decomp: MENU9.PRG func_801080C8 (F02838, src/MENU/MENU9.PRG/57BC.c:49)
void InitializeLiveAshleyPreviewPose() { InitializeAshleyPreviewPose(Host()); }

// decomp: MENU9.PRG vs_menu9_loadZudFile (F02839,
// src/MENU/MENU9.PRG/592C.c:171)
bool QueueLiveEnemyModelRead(int zud_id) {
  return QueueEnemyModelRead(Host(), liveState, zud_id);
}

// decomp: MENU9.PRG vs_menu9_parseZudFile (F02840,
// src/MENU/MENU9.PRG/592C.c:203)
int LoadLiveEnemyModelParts() { return LoadEnemyModelParts(Host(), liveState); }

// decomp: MENU9.PRG vs_menu9_freeZudFile (F02841,
// src/MENU/MENU9.PRG/592C.c:295)
void FreeLiveEnemyModelBuffer() { FreeEnemyModelBuffer(Host(), liveState); }

bool CleanupLiveEnemyPreviewOwnedState() {
  const bool owned = liveState.read != AssetAddress::kNone ||
                     liveState.file != AssetAddress::kNone ||
                     liveState.model_buffer != AssetAddress::kNone;
  const auto file = PreviewAllocation(liveState.file, liveState.file_bytes);
  const auto model = PreviewAllocation(liveState.model_buffer, kRecordBytes);
  if (!file || !model || !ValidatePreviewRecords(*model)) return false;
  if (!file->empty() && !Content::CancelLiveDiscWrites(*file)) return false;
  if (!Content::CancelLiveActorAssetRequests(
          Content::ActorAssetOwner::kEnemyPreview)) {
    Report("cleanup could not cancel the preview's parts");
    return false;
  }
  auto* request = Pointer<VS::Content::DiscQueueSlot>(liveState.read);
  if (liveState.read_generation != 0 &&
      Content::LiveDiscRequestGeneration(request) ==
          liveState.read_generation) {
    Content::ReleaseLiveDiscRequest(request);
  }
  for (const Content::ActorAssetTicket ticket : liveState.tickets) {
    if (ticket != Content::ActorAssetTicket::kNone) {
      Content::ForgetLiveActorAsset(ticket);
    }
  }
  ReleasePreviewRecords(*model);
  if (!file->empty()) Core::Memory::FreeLiveFromHighEnd(file->data());
  if (!model->empty()) Core::Memory::FreeLiveFromHighEnd(model->data());
  liveState = {};
  if (owned) Host().SetLoadStep(0);
  return true;
}

void ResetLiveEnemyPreview() { liveState = {}; }

EnemyPreviewState& LiveEnemyPreviewState() noexcept { return liveState; }

std::uint64_t LiveEnemyPreviewRefusals() noexcept { return refusals; }
}  // namespace VS::Menu::Score
