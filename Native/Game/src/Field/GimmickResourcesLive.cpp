#include "Field/GimmickResourcesLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Content/ModuleData.h"
#include "Core/Memory/HeapLive.hpp"
#include "Field/ActorCollisionLive.hpp"
#include "Field/GimmickPlacementLive.hpp"
#include "Field/GimmickResourcesLive.h"
#include "Field/InteractionTargetLive.hpp"
#include "Render/ModelGeometryLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
extern "C" {
#include "ActorAssets.h"
#include "Field/Gimmicks.h"
}

namespace {
using VS::Field::kGimmickResourceSlots;
using ResourceSlot = VS::Field::GimmickResourceSlot;
using ResourceSlots = std::array<ResourceSlot, kGimmickResourceSlots>;
using GimmickInstance = Field_GimmickInstance;

ResourceSlots SnapshotResourceSlots();

constexpr auto kGimmickPrefixBytes = offsetof(GimmickInstance, reserved178);
constexpr std::size_t kGimmickTemplateKinds = 8;
constexpr std::size_t kGimmickTemplatePointers = 3;
constexpr std::uint32_t kGimmickTemplateBlobAddress = 0x800E930C;
constexpr std::uint32_t kGimmickClutRow = 13;
constexpr std::size_t kTemplateFlagBytes = sizeof(std::int32_t);
constexpr std::size_t kUvPairBytes = 2;
constexpr unsigned kTileByteBits = 8;
constexpr unsigned kReservedTileShift = 24;

std::span<const std::byte> BytesAfter(const void* address,
                                      std::span<const std::byte> bytes) {
  const auto at = reinterpret_cast<std::uintptr_t>(address);
  const auto start = reinterpret_cast<std::uintptr_t>(bytes.data());
  if (at < start || at - start >= bytes.size()) return {};
  return bytes.subspan(static_cast<std::size_t>(at - start));
}

std::span<const std::byte> TemplateBytes(const void* address) {
  if (address == nullptr) return {};
  const auto allocation = VS::Core::Memory::LivePayloadContaining(address);
  if (allocation) return BytesAfter(address, *allocation);
  for (const auto& symbol :
       std::span{Content_moduleSymbols, Content_moduleSymbolCount}) {
    if (symbol.address == kGimmickTemplateBlobAddress &&
        symbol.target != nullptr &&
        std::strcmp(symbol.module, "BATTLE/BATTLE.PRG") == 0 &&
        symbol.sourceElementSize == symbol.nativeElementSize) {
      return BytesAfter(address, {static_cast<const std::byte*>(symbol.target),
                                  symbol.nativeSize});
    }
  }
  return {};
}

std::uint32_t TileWord(const Field_GimmickInstance& record) {
  return static_cast<std::uint8_t>(record.tileX) |
         (static_cast<std::uint32_t>(
              static_cast<std::uint8_t>(record.tileLayer))
          << kTileByteBits) |
         (static_cast<std::uint32_t>(static_cast<std::uint8_t>(record.tileZ))
          << (kTileByteBits * 2)) |
         (static_cast<std::uint32_t>(
              static_cast<std::uint8_t>(record.reserved5F))
          << kReservedTileShift);
}

VS::Field::GimmickCreationState CreationState(
    const Field_GimmickInstance& record) {
  return {.slot = record.slot,
          .object_id = record.objectInfo.objectId,
          .subtype = record.objectInfo.subtype,
          .carrier = record.objectInfo.carrier,
          .parent = record.parent,
          .interaction = record.objectInfo.interaction,
          .pulse_kind = record.palettePulse.kind,
          .base_color = record.baseColor,
          .scale_x = record.scaleX,
          .scale_y = record.scaleY,
          .scale_z = record.scaleZ,
          .mode = record.mode,
          .skipped = record.isSkipped != 0,
          .visible = record.isVisible != 0,
          .tile_word = TileWord(record),
          .x = record.positionX,
          .y = record.positionY,
          .z = record.positionZ};
}

void StoreCreationState(Field_GimmickInstance& record,
                        const VS::Field::GimmickCreationState& state) {
  record.slot = state.slot;
  record.objectInfo.objectId = state.object_id;
  record.objectInfo.subtype = state.subtype;
  record.objectInfo.carrier = state.carrier;
  record.parent = state.parent;
  record.objectInfo.interaction = state.interaction;
  record.palettePulse.kind = state.pulse_kind;
  record.baseColor = state.base_color;
  record.scaleX = state.scale_x;
  record.scaleY = state.scale_y;
  record.scaleZ = state.scale_z;
  record.mode = state.mode;
  record.isSkipped = static_cast<char>(state.skipped);
  record.isVisible = static_cast<char>(state.visible);
  record.tileX =
      std::bit_cast<char>(static_cast<std::uint8_t>(state.tile_word));
  record.tileLayer = std::bit_cast<char>(
      static_cast<std::uint8_t>(state.tile_word >> kTileByteBits));
  record.tileZ = std::bit_cast<char>(
      static_cast<std::uint8_t>(state.tile_word >> (kTileByteBits * 2)));
  record.reserved5F = std::bit_cast<char>(
      static_cast<std::uint8_t>(state.tile_word >> kReservedTileShift));
  record.positionX = state.x;
  record.positionY = state.y;
  record.positionZ = state.z;
}

bool CreateModel(Field_GimmickInstance& record, std::uint8_t kind,
                 std::uint8_t uv_kind) {
  if (kind >= kGimmickTemplateKinds) return false;
  const auto index = Field_gimmickTemplateIndices[kind];
  if (index >= kGimmickTemplatePointers) return false;
  const auto source = TemplateBytes(Field_gimmickModelTemplates[index]);
  if (source.size() < sizeof(Field_GimmickTemplate)) return false;
  Field_GimmickTemplate header{};
  std::memcpy(&header, source.data(), sizeof(header));
  std::memcpy(&record.boneCount, &header.instanceFlags, kTemplateFlagBytes);
  record.reserved4 = header.instanceWord4;
  if (header.modelDataSize < static_cast<int>(sizeof(Actors_ShpModel)) ||
      header.recolorCount < 0 ||
      static_cast<std::size_t>(header.modelDataSize) >
          source.size() - sizeof(header)) {
    return false;
  }
  auto* const geometry = const_cast<std::byte*>(source.data() + sizeof(header));
  record.model =
      static_cast<Actors_ShpModel*>(VS::Content::CopyLiveModelGeometry(
          nullptr, geometry, header.modelDataSize));
  if (record.model == nullptr) return false;
  const auto model_bytes = static_cast<std::size_t>(header.modelDataSize);
  VS::Render::RecordLiveModelExtent(record.model, model_bytes);
  if (record.model->polygonSection > model_bytes) return false;
  const auto polygons =
      std::span{reinterpret_cast<std::byte*>(record.model), model_bytes}
          .subspan(record.model->polygonSection);
  const auto uv = static_cast<std::size_t>(uv_kind) * kUvPairBytes;
  if (uv + 1 >= std::size(Field_gimmickPaletteParameters)) return false;
  return VS::Content::OffsetModelTextureCoordinates(
      polygons, header.recolorCount, true, Field_gimmickPaletteParameters[uv],
      Field_gimmickPaletteParameters[uv + 1]);
}

class LiveGimmickCreationHost final : public VS::Field::GimmickCreationHost {
 public:
  LiveGimmickCreationHost(std::size_t slot, Field_GimmickInstance* destination)
      : slot_(slot),
        record_(destination),
        generation_(VS::Field::LiveInteractionTargetDomain().generation) {}
  bool Occupied(std::size_t slot) const override {
    return slot >= kGimmickResourceSlots ||
           Field_gimmickInstances[slot] != nullptr;
  }
  bool PublishDestination(std::size_t slot) override {
    if (slot != slot_ ||
        generation_ != VS::Field::LiveInteractionTargetDomain().generation) {
      return false;
    }
    if (record_ != nullptr &&
        !VS::Field::IsLiveGimmickPrefixReadable(record_)) {
      return false;
    }
    Field_gimmickInstances[slot] = record_;
    return Valid(slot);
  }
  bool ClearPrefix(std::size_t slot) override {
    if (!Valid(slot)) return false;
    std::memset(record_, 0, kGimmickPrefixBytes);
    return true;
  }
  std::span<const ResourceSlot> Slots() override {
    slots_ = SnapshotResourceSlots();
    return slots_;
  }
  bool CopySharedPrefix(std::size_t slot, std::size_t source) override {
    if (!Valid(slot) || source >= kGimmickResourceSlots ||
        !VS::Field::IsLiveGimmickPrefixReadable(
            Field_gimmickInstances[source])) {
      return false;
    }
    std::memmove(record_, Field_gimmickInstances[source], kGimmickPrefixBytes);
    return true;
  }
  bool CreateModel(std::size_t slot, std::uint8_t kind,
                   std::uint8_t uv_kind) override {
    return Valid(slot) && ::CreateModel(*record_, kind, uv_kind) && Valid(slot);
  }
  std::optional<VS::Field::GimmickCreationState> ReadState(
      std::size_t slot) override {
    if (!Valid(slot_) || slot >= kGimmickResourceSlots ||
        !VS::Field::IsLiveGimmickPrefixReadable(Field_gimmickInstances[slot])) {
      return std::nullopt;
    }
    return CreationState(*Field_gimmickInstances[slot]);
  }
  bool StoreState(std::size_t slot,
                  const VS::Field::GimmickCreationState& state) override {
    if (!Valid(slot)) return false;
    StoreCreationState(*record_, state);
    return true;
  }
  bool CopyPalette(std::size_t slot, std::uint8_t kind) override {
    if (!Valid(slot) || kind >= VS::Field::kGimmickPaletteSets) return false;
    const auto offset =
        static_cast<std::size_t>(kind) * sizeof(record_->basePalette);
    std::memcpy(record_->basePalette, Field_gimmickPaletteColors + offset,
                sizeof(record_->basePalette));
    return true;
  }
  bool UploadPalette(std::size_t slot) override {
    return Valid(slot) &&
           VS::Render::LoadLiveClutRow(
               kGimmickClutRow,
               record_->slot * VS::Field::kGimmickPaletteColors,
               record_->basePalette) &&
           Valid(slot);
  }
  std::optional<std::int32_t> SupportHeight(
      const VS::Field::GimmickCreationState& state) override {
    if (!Valid(slot_)) return std::nullopt;
    const auto height = VS::Field::LiveSampleSupportHeightAt(state.x, state.y,
                                                             state.z, 0, 0, 1);
    return Valid(slot_) ? std::optional<std::int32_t>{height} : std::nullopt;
  }
  bool Orient(std::size_t slot, std::uint8_t material) override {
    if (!Valid(slot)) return false;
    static_cast<void>(VS::Field::SetLiveGimmickOrientation(
        static_cast<std::int32_t>(slot), material));
    return Valid(slot);
  }
  bool AddOccupancy(std::size_t slot) override {
    if (!Valid(slot)) return false;
    VS::Battle::Ai::AddLiveAiTileOccupant(record_);
    return Valid(slot);
  }

 private:
  bool Valid(std::size_t slot) const {
    return slot == slot_ && VS::Field::IsLiveGimmickPrefixReadable(record_) &&
           generation_ == VS::Field::LiveInteractionTargetDomain().generation &&
           Field_gimmickInstances[slot_] == record_;
  }
  std::size_t slot_;
  Field_GimmickInstance* record_;
  std::uint64_t generation_;
  ResourceSlots slots_{};
};

ResourceSlots SnapshotResourceSlots() {
  ResourceSlots slots{};
  for (std::size_t index = 0; index < slots.size(); ++index) {
    const auto* record = Field_gimmickInstances[index];
    if (record != nullptr) {
      slots[index] = {true,
                      static_cast<std::uint8_t>(record->objectInfo.objectId),
                      static_cast<std::uint8_t>(record->boneCount)};
    }
  }
  return slots;
}

class LiveGimmickReleaseHost final : public VS::Field::GimmickReleaseHost {
 public:
  explicit LiveGimmickReleaseHost(std::size_t slot)
      : slot_(slot),
        record_(Field_gimmickInstances[slot]),
        generation_(VS::Field::LiveInteractionTargetDomain().generation) {}
  bool FreeModel(std::size_t slot) override {
    if (!Valid(slot)) return false;
    VS::Render::ForgetLiveModelExtent(record_->model);
    VS::Core::Memory::FreeLiveFromLowEnd(record_->model);
    return Valid(slot);
  }
  bool RemoveOccupancy(std::size_t slot) override {
    if (!Valid(slot)) return false;
    VS::Battle::Ai::RemoveLiveAiTileOccupant(record_);
    return Valid(slot);
  }
  bool ClearSlot(std::size_t slot) override {
    if (!Valid(slot)) return false;
    Field_gimmickInstances[slot] = nullptr;
    return true;
  }

 private:
  bool Valid(std::size_t slot) const {
    return slot == slot_ && record_ != nullptr &&
           generation_ == VS::Field::LiveInteractionTargetDomain().generation &&
           Field_gimmickInstances[slot_] == record_;
  }
  std::size_t slot_;
  Field_GimmickInstance* record_;
  std::uint64_t generation_;
};
}  // namespace

namespace VS::Field {
int CreateLiveGimmickInstance(Content::ActorAssetRequest& request) {
  if (request.target_slot < 0 ||
      static_cast<std::size_t>(request.target_slot) >= kGimmickResourceSlots) {
    return Content::kAssetRefused;
  }
  auto* const destination = reinterpret_cast<Field_GimmickInstance*>(
      static_cast<std::uintptr_t>(request.destination));
  LiveGimmickCreationHost host(static_cast<std::size_t>(request.target_slot),
                               destination);
  return CreateGimmickInstance(request, host);
}

std::optional<std::size_t> FindLiveSharedGimmickModel(
    std::int8_t excluded_slot, std::uint8_t requested_kind) {
  return FindSharedGimmickModel(SnapshotResourceSlots(), excluded_slot,
                                requested_kind);
}

int ReleaseLiveGimmickInstance(std::int32_t slot) {
  if (slot < 0 || static_cast<std::size_t>(slot) >= kGimmickResourceSlots) {
    return -1;
  }
  const auto slots = SnapshotResourceSlots();
  LiveGimmickReleaseHost host(static_cast<std::size_t>(slot));
  return ReleaseGimmickInstance(slot, slots, host);
}

bool SetLiveGimmickPaletteTable(std::span<const std::byte> palette_bytes) {
  GimmickPaletteTable table{};
  if (!SetGimmickPaletteTable(table, palette_bytes)) return false;
  static_assert(sizeof(Field_gimmickPaletteColors) >= sizeof(table));
  std::memcpy(Field_gimmickPaletteColors, table.data(), sizeof(table));
  return true;
}
}  // namespace VS::Field

int Field_FindSharedGimmickModelEntry(Content_ActorAssetRequest* request) {
  if (request == nullptr) return 0;
  Content_ActorAssetRequest input{};
  std::memcpy(&input, request, sizeof(input));
  const auto slot =
      VS::Field::FindLiveSharedGimmickModel(input.targetSlot, input.subKind);
  if (!slot) return 0;
  const auto value = static_cast<std::uintptr_t>(*slot);
  std::memcpy(reinterpret_cast<std::byte*>(request) +
                  offsetof(Content_ActorAssetRequest, dataAddress),
              &value, sizeof(value));
  return 1;
}

void Field_SetGimmickPaletteTableEntry(const void* palette_data) {
  if (palette_data == nullptr) return;
  static_cast<void>(VS::Field::SetLiveGimmickPaletteTable(
      {static_cast<const std::byte*>(palette_data),
       sizeof(VS::Field::GimmickPaletteTable)}));
}

int Field_ReleaseGimmickInstanceEntry(int slot) {
  return VS::Field::ReleaseLiveGimmickInstance(slot);
}

int Field_CreateGimmickInstanceEntry(Content_ActorAssetRequest* request) {
  if (request == nullptr) return VS::Content::kAssetRefused;
  Content_ActorAssetRequest input{};
  std::memcpy(&input, request, sizeof(input));
  VS::Content::ActorAssetRequest state{
      .kind = input.assetKind,
      .target_slot = input.targetSlot,
      .model_id = input.modelId,
      .destination = std::bit_cast<VS::Content::AssetAddress>(
          reinterpret_cast<std::uintptr_t>(input.destination)),
      .data = std::bit_cast<VS::Content::AssetAddress>(input.dataAddress),
      .placement = std::bit_cast<std::uint32_t>(input.placement),
      .sub_kind = static_cast<std::uint8_t>(input.subKind),
      .texture_slot = static_cast<std::uint8_t>(input.textureSlot),
      .material = static_cast<std::uint8_t>(input.material),
      .variant = static_cast<std::uint8_t>(input.variant)};
  const auto result = VS::Field::CreateLiveGimmickInstance(state);
  const auto value = static_cast<std::uintptr_t>(state.data);
  std::memcpy(reinterpret_cast<std::byte*>(request) +
                  offsetof(Content_ActorAssetRequest, dataAddress),
              &value, sizeof(value));
  return result;
}
