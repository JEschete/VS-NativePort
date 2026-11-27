#include "Field/GimmickResourcesLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "Actors/ShpModel.hpp"
#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Content/ModuleData.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/ActorCollisionLive.hpp"
#include "Field/GimmickPlacementLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/InteractionTargetLive.hpp"
#include "Render/ModelGeometryLive.hpp"
#include "Render/PaletteAnimationLive.hpp"

namespace {
using VS::Field::kGimmickResourceSlots;
using ResourceSlot = VS::Field::GimmickResourceSlot;
using ResourceSlots = std::array<ResourceSlot, kGimmickResourceSlots>;
using GimmickInstance = VS::Field::GimmickInstance;

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

struct GimmickResourceBinding {
  GimmickInstance* record = nullptr;
  VS::Actors::ShpModel* geometry = nullptr;
  VS::Field::GimmickModelResourceRef model;
};

struct GimmickResources {
  std::array<std::weak_ptr<const VS::Field::GimmickModelResource>,
             kGimmickTemplateKinds>
      models;
  std::array<GimmickResourceBinding, kGimmickResourceSlots> instances;
  std::uint64_t room_generation = 0;
  std::uint64_t epoch = 1;
};

GimmickResources& Resources() {
  static GimmickResources resources;
  return resources;
}

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
  for (const auto& symbol : VS::Content::ModuleSymbols()) {
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

std::uint32_t TileWord(const VS::Field::GimmickInstance& record) {
  return static_cast<std::uint8_t>(record.tile_x) |
         (static_cast<std::uint32_t>(
              static_cast<std::uint8_t>(record.tile_layer))
          << kTileByteBits) |
         (static_cast<std::uint32_t>(static_cast<std::uint8_t>(record.tile_z))
          << (kTileByteBits * 2)) |
         (static_cast<std::uint32_t>(
              static_cast<std::uint8_t>(record.tile_extra))
          << kReservedTileShift);
}

VS::Field::GimmickCreationState CreationState(
    const VS::Field::GimmickInstance& record) {
  return {.slot = record.slot,
          .object_id = record.object_info.object_id,
          .subtype = record.object_info.subtype,
          .carrier = record.object_info.carrier,
          .parent = record.parent,
          .interaction = record.object_info.stack_state,
          .pulse_kind = record.palette_pulse.kind,
          .base_color = record.base_color,
          .scale_x = record.scale_x,
          .scale_y = record.scale_y,
          .scale_z = record.scale_z,
          .mode = record.mode,
          .skipped = record.is_skipped != 0,
          .visible = record.is_visible != 0,
          .tile_word = TileWord(record),
          .x = record.position_x,
          .y = record.position_y,
          .z = record.position_z};
}

void StoreCreationState(VS::Field::GimmickInstance& record,
                        const VS::Field::GimmickCreationState& state) {
  record.slot = state.slot;
  record.object_info.object_id = state.object_id;
  record.object_info.subtype = state.subtype;
  record.object_info.carrier = state.carrier;
  record.parent = state.parent;
  record.object_info.stack_state = state.interaction;
  record.palette_pulse.kind = state.pulse_kind;
  record.base_color = state.base_color;
  record.scale_x = state.scale_x;
  record.scale_y = state.scale_y;
  record.scale_z = state.scale_z;
  record.mode = state.mode;
  record.is_skipped = static_cast<char>(state.skipped);
  record.is_visible = static_cast<char>(state.visible);
  record.tile_x =
      std::bit_cast<char>(static_cast<std::uint8_t>(state.tile_word));
  record.tile_layer = std::bit_cast<char>(
      static_cast<std::uint8_t>(state.tile_word >> kTileByteBits));
  record.tile_z = std::bit_cast<char>(
      static_cast<std::uint8_t>(state.tile_word >> (kTileByteBits * 2)));
  record.tile_extra = std::bit_cast<char>(
      static_cast<std::uint8_t>(state.tile_word >> kReservedTileShift));
  record.position_x = state.x;
  record.position_y = state.y;
  record.position_z = state.z;
}

VS::Field::GimmickModelResourceRef ModelResource(std::uint8_t kind,
                                                 std::uint8_t uv_kind) {
  if (kind >= kGimmickTemplateKinds) return {};
  auto& resources = Resources();
  const auto generation = VS::Field::LiveInteractionTargetDomain().generation;
  if (resources.room_generation != generation) {
    resources.models = {};
    resources.room_generation = generation;
  }
  if (auto resource = resources.models[kind].lock()) return resource;
  const auto index = VS::Field::gimmickTemplateIndices[kind];
  if (index >= kGimmickTemplatePointers) return {};
  const auto source = TemplateBytes(VS::Field::gimmickModelTemplates[index]);
  if (source.size() < sizeof(VS::Field::GimmickTemplate)) return {};
  VS::Field::GimmickTemplate header{};
  std::memcpy(&header, source.data(), sizeof(header));
  if (header.model_data_size < static_cast<int>(sizeof(VS::Actors::ShpModel)) ||
      header.polygon_count < 0 ||
      static_cast<std::size_t>(header.model_data_size) >
          source.size() - sizeof(header)) {
    return {};
  }
  const auto bytes = source.subspan(
      sizeof(header), static_cast<std::size_t>(header.model_data_size));
  std::vector<std::byte> geometry(bytes.begin(), bytes.end());
  VS::Actors::ShpModel model{};
  std::memcpy(&model, geometry.data(), sizeof(model));
  if (model.polygon_section > geometry.size()) return {};
  const auto polygons = std::span(geometry).subspan(model.polygon_section);
  const auto uv = static_cast<std::size_t>(uv_kind) * kUvPairBytes;
  if (uv + 1 >= std::size(VS::Field::gimmickPaletteParameters) ||
      !VS::Content::OffsetModelTextureCoordinates(
          polygons, header.polygon_count, true,
          VS::Field::gimmickPaletteParameters[uv],
          VS::Field::gimmickPaletteParameters[uv + 1])) {
    return {};
  }
  auto resource = std::make_shared<const VS::Field::GimmickModelResource>(
      header.instance_flags, header.instance_word4, std::move(geometry));
  resources.models[kind] = resource;
  return resource;
}

bool CreateModel(VS::Field::GimmickInstance& record, std::size_t slot,
                 std::uint8_t kind, std::uint8_t uv_kind) {
  auto resource = ModelResource(kind, uv_kind);
  if (!resource) return false;
  const auto bytes = resource->Geometry();
  const auto model_bytes = bytes.size();
  record.model = static_cast<VS::Actors::ShpModel*>(
      VS::Core::Memory::AllocateLiveFromLowEnd(model_bytes));
  if (record.model == nullptr) return false;
  std::memcpy(record.model, bytes.data(), model_bytes);
  const auto flags = resource->InstanceFlags();
  std::memcpy(&record.bone_count, &flags, kTemplateFlagBytes);
  record.reserved4 = resource->InstanceWord4();
  VS::Render::RecordLiveModelExtent(record.model, model_bytes);
  Resources().instances[slot] = {.record = &record,
                                 .geometry = record.model,
                                 .model = std::move(resource)};
  return true;
}

void DiscardGimmickRecord(std::size_t slot, GimmickInstance* expected,
                          bool remove_occupancy) {
  auto& binding = Resources().instances[slot];
  const bool bound = binding.record == expected;
  const bool published = VS::Field::gimmickInstances[slot] == expected;
  if (bound) {
    VS::Render::ForgetLiveModelExtent(binding.geometry);
    VS::Core::Memory::FreeLiveFromLowEnd(binding.geometry);
    binding = {};
  } else if (published && VS::Field::IsLiveGimmickPrefixReadable(expected) &&
             expected->model != nullptr) {
    VS::Render::ForgetLiveModelExtent(expected->model);
    VS::Core::Memory::FreeLiveFromLowEnd(expected->model);
  }
  if ((bound || published) &&
      VS::Field::IsLiveGimmickPrefixReadable(expected)) {
    if (remove_occupancy) VS::Battle::Ai::RemoveLiveAiTileOccupant(expected);
    expected->model = nullptr;
  }
  if (published) VS::Field::gimmickInstances[slot] = nullptr;
}

class LiveGimmickCreationHost final : public VS::Field::GimmickCreationHost {
 public:
  LiveGimmickCreationHost(std::size_t slot,
                          VS::Field::GimmickInstance* destination)
      : slot_(slot),
        record_(destination),
        generation_(VS::Field::LiveInteractionTargetDomain().generation),
        epoch_(Resources().epoch) {}
  bool Occupied(std::size_t slot) const override {
    return slot >= kGimmickResourceSlots ||
           VS::Field::gimmickInstances[slot] != nullptr ||
           Resources().instances[slot].record != nullptr;
  }
  bool PublishDestination(std::size_t slot) override {
    if (slot != slot_ || epoch_ != Resources().epoch ||
        generation_ != VS::Field::LiveInteractionTargetDomain().generation) {
      return false;
    }
    if (record_ != nullptr &&
        !VS::Field::IsLiveGimmickPrefixReadable(record_)) {
      return false;
    }
    VS::Field::gimmickInstances[slot] = record_;
    return Valid(slot);
  }
  bool ClearPrefix(std::size_t slot) override {
    if (!Valid(slot)) return false;
    std::memset(record_, 0, kGimmickPrefixBytes);
    return true;
  }
  void Discard(std::size_t slot) override {
    if (slot != slot_ || epoch_ != Resources().epoch) return;
    const bool current =
        generation_ == VS::Field::LiveInteractionTargetDomain().generation;
    DiscardGimmickRecord(slot, record_, current && occupancy_added_);
    if (current && VS::Field::IsLiveGimmickPrefixReadable(record_)) {
      std::memset(record_, 0, kGimmickPrefixBytes);
    }
  }
  bool CreateModel(std::size_t slot, std::uint8_t kind,
                   std::uint8_t uv_kind) override {
    return Valid(slot) && ::CreateModel(*record_, slot, kind, uv_kind) &&
           Valid(slot);
  }
  std::optional<VS::Field::GimmickCreationState> ReadState(
      std::size_t slot) override {
    if (!Valid(slot_) || slot >= kGimmickResourceSlots ||
        !VS::Field::IsLiveGimmickPrefixReadable(
            VS::Field::gimmickInstances[slot])) {
      return std::nullopt;
    }
    return CreationState(*VS::Field::gimmickInstances[slot]);
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
        static_cast<std::size_t>(kind) * sizeof(record_->base_palette);
    std::memcpy(record_->base_palette.data(),
                VS::Field::gimmickPaletteColors.data() + offset,
                sizeof(record_->base_palette));
    return true;
  }
  bool UploadPalette(std::size_t slot) override {
    return Valid(slot) &&
           VS::Render::LoadLiveClutRow(
               kGimmickClutRow,
               record_->slot * VS::Field::kGimmickPaletteColors,
               record_->base_palette) &&
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
    occupancy_added_ = true;
    return Valid(slot);
  }

 private:
  bool Valid(std::size_t slot) const {
    return slot == slot_ && VS::Field::IsLiveGimmickPrefixReadable(record_) &&
           epoch_ == Resources().epoch &&
           generation_ == VS::Field::LiveInteractionTargetDomain().generation &&
           VS::Field::gimmickInstances[slot_] == record_;
  }
  std::size_t slot_;
  VS::Field::GimmickInstance* record_;
  std::uint64_t generation_;
  std::uint64_t epoch_;
  bool occupancy_added_ = false;
};

ResourceSlots SnapshotResourceSlots() {
  ResourceSlots slots{};
  for (std::size_t index = 0; index < slots.size(); ++index) {
    const auto* record = VS::Field::gimmickInstances[index];
    if (record != nullptr) {
      slots[index] = {true,
                      static_cast<std::uint8_t>(record->object_info.object_id),
                      static_cast<std::uint8_t>(record->bone_count)};
    }
  }
  return slots;
}

class LiveGimmickReleaseHost final : public VS::Field::GimmickReleaseHost {
 public:
  explicit LiveGimmickReleaseHost(std::size_t slot)
      : slot_(slot),
        record_(VS::Field::gimmickInstances[slot]),
        generation_(VS::Field::LiveInteractionTargetDomain().generation) {}
  bool FreeModel(std::size_t slot) override {
    if (!Valid(slot)) return false;
    if (record_->model != nullptr) {
      VS::Render::ForgetLiveModelExtent(record_->model);
      VS::Core::Memory::FreeLiveFromLowEnd(record_->model);
      record_->model = nullptr;
    }
    if (Resources().instances[slot].record == record_) {
      Resources().instances[slot] = {};
    }
    return Valid(slot);
  }
  bool RemoveOccupancy(std::size_t slot) override {
    if (!Valid(slot)) return false;
    VS::Battle::Ai::RemoveLiveAiTileOccupant(record_);
    return Valid(slot);
  }
  bool ClearSlot(std::size_t slot) override {
    if (!Valid(slot)) return false;
    VS::Field::gimmickInstances[slot] = nullptr;
    return true;
  }

 private:
  bool Valid(std::size_t slot) const {
    return slot == slot_ && record_ != nullptr &&
           generation_ == VS::Field::LiveInteractionTargetDomain().generation &&
           VS::Field::gimmickInstances[slot_] == record_;
  }
  std::size_t slot_;
  VS::Field::GimmickInstance* record_;
  std::uint64_t generation_;
};
}  // namespace

namespace VS::Field {
int CreateLiveGimmickInstance(const Content::ActorAssetRequest& request,
                              GimmickInstance* destination) {
  if (request.target_slot < 0 ||
      static_cast<std::size_t>(request.target_slot) >= kGimmickResourceSlots) {
    VS::Core::Warnf("field", "gimmick creation refused invalid slot %d",
                    request.target_slot);
    return Content::kAssetRefused;
  }
  LiveGimmickCreationHost host(static_cast<std::size_t>(request.target_slot),
                               destination);
  const int result = CreateGimmickInstance(request, host);
  if (result != Content::kAssetDone) {
    VS::Core::Warnf("field",
                    "gimmick creation failed: slot=%d kind=%u result=%d",
                    request.target_slot, request.sub_kind, result);
  }
  return result;
}

void DiscardLiveGimmickInstance(std::int32_t slot, GimmickInstance* expected) {
  if (slot < 0 || static_cast<std::size_t>(slot) >= kGimmickResourceSlots ||
      expected == nullptr) {
    VS::Core::Warnf("field",
                    "gimmick discard refused invalid owner for slot %d", slot);
    return;
  }
  DiscardGimmickRecord(static_cast<std::size_t>(slot), expected, true);
}

GimmickModelResourceRef LiveGimmickModelResource(std::int32_t slot) {
  if (slot < 0 || static_cast<std::size_t>(slot) >= kGimmickResourceSlots) {
    VS::Core::Warnf("field", "gimmick resource refused invalid slot %d", slot);
    return {};
  }
  const auto& binding = Resources().instances[slot];
  return binding.record == gimmickInstances[slot] ? binding.model
                                                  : GimmickModelResourceRef{};
}

void ResetLiveGimmickResources() {
  auto& resources = Resources();
  for (std::size_t slot = 0; slot < resources.instances.size(); ++slot) {
    auto& binding = resources.instances[slot];
    if (binding.geometry != nullptr) {
      VS::Render::ForgetLiveModelExtent(binding.geometry);
      VS::Core::Memory::FreeLiveFromLowEnd(binding.geometry);
    }
    if (gimmickInstances[slot] == binding.record) {
      gimmickInstances[slot] = nullptr;
    }
    binding = {};
  }
  resources.models = {};
  if (++resources.epoch == 0) ++resources.epoch;
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
  static_assert(sizeof(VS::Field::gimmickPaletteColors) >= sizeof(table));
  std::memcpy(VS::Field::gimmickPaletteColors.data(), table.data(),
              sizeof(table));
  return true;
}
}  // namespace VS::Field

int VS::Field::FindSharedGimmickModelEntry(
    VS::Content::LegacyActorAssetRequest* request) {
  if (request == nullptr) return 0;
  VS::Content::LegacyActorAssetRequest input{};
  std::memcpy(&input, request, sizeof(input));
  const auto slot =
      VS::Field::FindLiveSharedGimmickModel(input.target_slot, input.sub_kind);
  if (!slot) return 0;
  const auto value = static_cast<std::uintptr_t>(*slot);
  std::memcpy(reinterpret_cast<std::byte*>(request) +
                  offsetof(VS::Content::LegacyActorAssetRequest, data_address),
              &value, sizeof(value));
  return 1;
}

void VS::Field::SetGimmickPaletteTableEntry(const void* palette_data) {
  if (palette_data == nullptr) return;
  static_cast<void>(VS::Field::SetLiveGimmickPaletteTable(
      {static_cast<const std::byte*>(palette_data),
       sizeof(VS::Field::GimmickPaletteTable)}));
}

int VS::Field::ReleaseGimmickInstanceEntry(int slot) {
  return VS::Field::ReleaseLiveGimmickInstance(slot);
}

int VS::Field::CreateGimmickInstanceEntry(
    VS::Content::LegacyActorAssetRequest* request) {
  if (request == nullptr) return VS::Content::kAssetRefused;
  VS::Content::LegacyActorAssetRequest input{};
  std::memcpy(&input, request, sizeof(input));
  VS::Content::ActorAssetRequest state{
      .kind = input.asset_kind,
      .target_slot = input.target_slot,
      .model_id = input.model_id,
      .data = std::bit_cast<VS::Content::AssetAddress>(input.data_address),
      .placement = input.placement,
      .sub_kind = static_cast<std::uint8_t>(input.sub_kind),
      .texture_slot = static_cast<std::uint8_t>(input.texture_slot),
      .material = static_cast<std::uint8_t>(input.material),
      .variant = static_cast<std::uint8_t>(input.variant)};
  return VS::Field::CreateLiveGimmickInstance(
      state, static_cast<VS::Field::GimmickInstance*>(input.destination));
}
