// Hand-maintained (tools/port/owned.csv).
#include "Effects/EffectResourcesLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "Audio/SoundBanksLive.hpp"
#include "Audio/StreamControlLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/ConvertedImages.hpp"
#include "Content/DataStore.hpp"
#include "Content/DiscLayout.h"
#include "Content/DiscQueue.hpp"
#include "Content/NativeImage.hpp"
#include "Content/OverlayWorkspaceLive.hpp"
#include "Core/Arena.hpp"
#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Overlays.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/EffectResources.hpp"
#include "Effects/EffectResourcesLiveState.hpp"
#include "Effects/Package.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/ParticlePoolLive.hpp"
#include "Effects/Particles.hpp"
#include "Effects/PurgeTables.hpp"
#include "Effects/Runtime.hpp"
#include "Effects/TextureBindings.hpp"
#include "Legacy/Psx.hpp"
#include "Render/PageSpace.hpp"
#include "Render/TexturePagesLive.hpp"

using VS::Core::Gte::VECTOR;

using VS::Legacy::LoadImage;
using VS::Legacy::RECT;

// decomp: D_800F5630 (D_800F5630@BATTLE/BATTLE.PRG)
int VS::Effects::textureLoadIndex;
// decomp: D_800F5638 (D_800F5638@BATTLE/BATTLE.PRG)
VS::Effects::LiveTextureLoadList VS::Effects::textureLoads;
// decomp: vs_battle_pfileBuf (vs_battle_pfileBuf@BATTLE/BATTLE.PRG)
void* VS::Effects::packageBuffer;
// decomp: D_800F5680 (D_800F5680@BATTLE/BATTLE.PRG)
std::uint32_t VS::Effects::textureLoadState;
// decomp: D_800F5684 (D_800F5684@BATTLE/BATTLE.PRG)
int VS::Effects::purgeOverlayLoaded;
// decomp: vs_battle_pFileLoadContext
// (vs_battle_pFileLoadContext@BATTLE/BATTLE.PRG)
VS::Effects::EffectFileExtent VS::Effects::packageFile;
// decomp: D_800F568C (D_800F568C@BATTLE/BATTLE.PRG)
VS::Content::DiscQueueSlot* VS::Effects::resourceReadSlot;
// decomp: _plgLoadState (_plgLoadState@BATTLE/BATTLE.PRG)
std::uint32_t VS::Effects::pluginLoadState;
// decomp: _plgLoadContext (_plgLoadContext@BATTLE/BATTLE.PRG)
VS::Effects::EffectFileExtent VS::Effects::pluginFile;
// decomp: D_800F56A0 (D_800F56A0@BATTLE/BATTLE.PRG)
std::uint32_t* VS::Effects::textureLoadBuffer;
// decomp: _plgEntryPointCount (_plgEntryPointCount@BATTLE/BATTLE.PRG)
int VS::Effects::pluginEntryPointCount;
// decomp: D_800F5874 (D_800F5874@BATTLE/BATTLE.PRG)
int VS::Effects::loadedTextureSlots;

static_assert(
    std::tuple_size_v<decltype(VS::Effects::LiveEffectPackage::curves)> ==
    VS::Effects::kEffectCurveCapacity);
static_assert(std::size(VS::Effects::effectRenderers) ==
              VS::Effects::kEffectRendererCapacity);

namespace VS::Effects {
namespace {
namespace Memory = VS::Core::Memory;

// The packages whose block views are kept: the action and secondary packages
// and the ones tests bind.
constexpr std::size_t kPackageRecords = 4;
// VS::Core::kOverlaySlots[3]: EFFPURGE.BIN and the plugins load there.
constexpr std::size_t kPurgeOverlaySlot = 3;
// The owner tag of the overlay workspace and the effect texture pages.
constexpr int kEffectOwnerTag = 2;
// func_800F9800 calls each plugin entry with message 2 (update).
constexpr unsigned int kPluginUpdateMessage = 2;

std::uint64_t refusedCalls = 0;
std::uint64_t walkReports = 0;
std::uint64_t paletteErrors = 0;

// True on the 1st, 2nd, 4th, 8th... occurrence, to keep repeated
// diagnostics readable.
[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

void ReportRefusal(const char* entry, const char* reason) {
  if (ShouldReport(refusedCalls)) {
    VS::Core::LogWarn("effects",
                      "effect resources: {} refused: {} ({} refusals)", entry,
                      reason, refusedCalls);
  }
}

// A package's typed block views, kept beside the legacy pointers the walk
// publishes into it.
struct PackageRecord {
  const LiveEffectPackage* package{};
  EffectPackageBlocks blocks;
};

std::array<PackageRecord, kPackageRecords> packageRecords{};
std::size_t nextPackageRecord = 0;

[[nodiscard]] PackageRecord* FindPackageRecord(
    const LiveEffectPackage* package) {
  const auto found =
      std::ranges::find(packageRecords, package, &PackageRecord::package);
  return found == packageRecords.end() ? nullptr : &*found;
}

[[nodiscard]] PackageRecord& PackageRecordFor(
    const LiveEffectPackage* package) {
  if (PackageRecord* record = FindPackageRecord(package)) return *record;
  PackageRecord& record = packageRecords.at(nextPackageRecord);
  nextPackageRecord = (nextPackageRecord + 1) % packageRecords.size();
  record = {.package = package, .blocks = {}};
  return record;
}

[[nodiscard]] std::span<std::byte> BlockView(const EffectPackageBlocks& blocks,
                                             PackageBlockType type) {
  switch (type) {
    case PackageBlockType::kAnimationTable:
      return blocks.animation_table;
    case PackageBlockType::kTimelines:
      return blocks.timelines;
    case PackageBlockType::kCurves:
      return blocks.curve_block;
    case PackageBlockType::kParticleDefinitions:
      return blocks.particle_definitions;
    case PackageBlockType::kPrograms:
      return blocks.programs;
    case PackageBlockType::kParameters:
      return blocks.parameters;
    case PackageBlockType::kTransformDefinitions:
      return blocks.transform_definitions;
    case PackageBlockType::kSoundSequences:
      return blocks.sound_sequences;
    case PackageBlockType::kSoundBank:
      return blocks.sound_bank;
    case PackageBlockType::kAuxiliarySound:
      return blocks.auxiliary_sound;
    case PackageBlockType::kTextureTables:
      break;
  }
  return {};
}

[[nodiscard]] const void* LegacyBlock(const LiveEffectPackage& package,
                                      PackageBlockType type) {
  switch (type) {
    case PackageBlockType::kAnimationTable:
      return package.animation_table;
    case PackageBlockType::kTimelines:
      return package.timelines;
    case PackageBlockType::kCurves:
      return package.curve_block;
    case PackageBlockType::kParticleDefinitions:
      return package.particle_definitions;
    case PackageBlockType::kPrograms:
      return package.programs;
    case PackageBlockType::kParameters:
      return package.parameters;
    case PackageBlockType::kTransformDefinitions:
      return package.transform_definitions;
    case PackageBlockType::kSoundSequences:
      return package.sound_sequences;
    case PackageBlockType::kSoundBank:
      return package.sound_bank;
    case PackageBlockType::kAuxiliarySound:
      return package.auxiliary_sound;
    case PackageBlockType::kTextureTables:
      break;
  }
  return nullptr;
}

template <typename T>
[[nodiscard]] T* LegacyPointer(std::span<std::byte> view) {
  return reinterpret_cast<T*>(view.data());
}

void PublishCurves(LiveEffectPackage& package,
                   const EffectPackageBlocks& blocks,
                   const CurveBinding& binding) {
  package.curve_block = LegacyPointer<LiveCurveTable>(blocks.curve_block);
  const std::size_t count =
      std::min(static_cast<std::size_t>(std::max(binding.header_count, 0)),
               blocks.curves.size());
  // A refused curve has no view and publishes no pointer.
  for (std::size_t i = 0; i < count; ++i) {
    package.curves[i] = LegacyPointer<std::uint8_t>(blocks.curves.at(i));
  }
}

// The pointers the original's binders stored, for each block type the walk
// bound; the others keep the previous package's pointers.
void PublishBlocks(LiveEffectPackage& package,
                   const EffectPackageBlocks& blocks, const PackageWalk& walk) {
  const auto bound = [&walk](PackageBlockType type) {
    return (walk.bound_types & PackageBlockBit(type)) != 0;
  };
  if (bound(PackageBlockType::kAnimationTable)) {
    package.animation_count = blocks.animation_count;
    package.animation_table =
        LegacyPointer<std::uint16_t>(blocks.animation_table);
  }
  if (bound(PackageBlockType::kTimelines)) {
    package.timelines = LegacyPointer<LiveTimelineTable>(blocks.timelines);
  }
  if (bound(PackageBlockType::kCurves)) {
    PublishCurves(package, blocks, walk.curves);
  }
  if (bound(PackageBlockType::kParticleDefinitions)) {
    package.particle_definitions =
        LegacyPointer<LiveParticleDefinitionTable>(blocks.particle_definitions);
  }
  if (bound(PackageBlockType::kPrograms)) {
    package.programs = LegacyPointer<char>(blocks.programs);
  }
  if (bound(PackageBlockType::kParameters)) {
    package.parameters =
        LegacyPointer<LivePackageParameters>(blocks.parameters);
  }
  if (bound(PackageBlockType::kTransformDefinitions)) {
    package.transform_definitions =
        LegacyPointer<LiveTransformDefinition>(blocks.transform_definitions);
  }
  if (bound(PackageBlockType::kSoundSequences)) {
    package.sound_sequences =
        LegacyPointer<std::uint8_t>(blocks.sound_sequences);
  }
  if (bound(PackageBlockType::kSoundBank)) {
    package.sound_bank = blocks.sound_bank.data();
  }
  if (bound(PackageBlockType::kAuxiliarySound)) {
    package.auxiliary_sound = blocks.auxiliary_sound.data();
    package.auxiliary_sound_handle = blocks.auxiliary_sound_handle;
  }
}

void RecordExtents(const EffectPackageBlocks& blocks, const PackageWalk& walk) {
  if ((walk.bound_types & PackageBlockBit(PackageBlockType::kCurves)) != 0) {
    RecordLiveEffectCurves(blocks.curve_block);
  }
  if ((walk.bound_types &
       PackageBlockBit(PackageBlockType::kParticleDefinitions)) != 0) {
    RecordLiveParticleDefinitions(blocks.particle_definitions);
  }
  if ((walk.bound_types & PackageBlockBit(PackageBlockType::kPrograms)) != 0) {
    RecordLiveEffectPrograms(blocks.programs);
  }
  if ((walk.bound_types & PackageBlockBit(PackageBlockType::kAnimationTable)) !=
      0) {
    RecordLiveAnimationTable(blocks.animation_table);
  }
}

void ReportWalk(const PackageWalk& walk) {
  if (walk.status == PackageWalkStatus::kEnded && walk.curves.refused == 0) {
    return;
  }
  if (ShouldReport(walkReports)) {
    VS::Core::LogWarn(
        "effects",
        "effect package: walk stopped with status {} after {} blocks; "
        "{} of {} curves refused ({} reports)",
        static_cast<int>(walk.status), walk.blocks, walk.curves.refused,
        walk.curves.header_count, walkReports);
  }
}

// The texture tables and sound blocks the walk hands over.
class LivePackageHost final : public EffectPackageHost {
 public:
  void BindTextureTables(std::span<std::byte> block) override {
    BindLiveEffectTextureTables(block);
  }

  void BindSoundBank(std::span<std::byte> block) override {
    static_cast<void>(Audio::LiveBindRoomSfxBank(
        reinterpret_cast<std::uintptr_t>(block.data())));
  }

  std::int32_t BindAuxiliarySound(std::span<std::byte> block) override {
    return Audio::BindLiveRoomCutsceneStream(block.data(), block.size());
  }
};

// The package buffer within its file size and heap payload, or the empty
// package; nullopt for anything else.
[[nodiscard]] std::optional<std::span<std::byte>> PackageBytes(void* package) {
  if (package == nullptr) return std::nullopt;
  if (package == static_cast<void*>(emptyPackage)) {
    return std::as_writable_bytes(std::span(emptyPackage));
  }
  if (package != packageBuffer) return std::nullopt;
  const std::size_t capacity = Memory::LivePayloadCapacity(package).value_or(0);
  return std::span(static_cast<std::byte*>(package),
                   std::min<std::size_t>(packageFile.size, capacity));
}

// The ids name only kNone; the live addresses they carry are made here.
template <typename Id, typename T>
[[nodiscard]] Id IdOf(T* pointer) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<Id>(reinterpret_cast<std::uintptr_t>(pointer));
}

template <typename T, typename Id>
[[nodiscard]] T* PointerOf(Id id) {
  return reinterpret_cast<T*>(static_cast<std::uintptr_t>(id));
}

[[nodiscard]] EffectBuffer BufferOf(const void* pointer) {
  if (pointer == nullptr) return {};
  return {.id = IdOf<EffectBufferId>(pointer),
          .capacity = Memory::LivePayloadCapacity(pointer).value_or(0)};
}

[[nodiscard]] TextureLoadList TexturesOf(const LiveTextureLoadList& list) {
  TextureLoadList textures{.asset_count = list.asset_count};
  for (std::size_t i = 0; i < textures.assets.size(); ++i) {
    textures.assets.at(i) = {.file = list.files[i],
                             .slot = list.slots[i],
                             .is_clut = list.is_clut[i] != 0};
  }
  return textures;
}

void StoreTextures(LiveTextureLoadList& list, const TextureLoadList& textures) {
  list.asset_count = textures.asset_count;
  for (std::size_t i = 0; i < textures.assets.size(); ++i) {
    const TextureAsset& asset = textures.assets.at(i);
    list.files[i] = asset.file;
    list.slots[i] = asset.slot;
    list.is_clut[i] = asset.is_clut ? 1U : 0U;
  }
}

[[nodiscard]] VS::Content::DiscFile DiscFileOf(DiscRead read) {
  VS::Content::DiscFile file{};
  file.lba = read.lba;
  file.size = read.size;
  return file;
}
}  // namespace

// The converted palette (ASSET_PIPELINE.md §6.4) of the FBC `source` reads,
// found by its sector until assets have stable ids (#47).
std::expected<std::vector<std::uint16_t>, std::string> LoadLiveEffectPalette(
    DiscRead source) {
  const Content::DataStore* const store = Content::DataStore::Get();
  if (store == nullptr) {
    return std::unexpected("no extracted-data store is open");
  }
  if (source.lba <= 0 || source.size <= 0) {
    return std::unexpected(
        std::format("invalid LBA {} and size {}", source.lba, source.size));
  }
  auto converted = Content::FindConvertedSource(
      *store, {.lba = static_cast<std::uint32_t>(source.lba),
               .rounded_size = static_cast<std::uint64_t>(source.size),
               .extension = ".FBC",
               .converter = "fbc-palette-png"});
  if (!converted) return std::unexpected(converted.error());
  auto document = Content::LoadConvertedImage(*store, *converted);
  if (!document) return std::unexpected(document.error());
  if (document->image.depth != Content::ImageDepth::kDirect16 ||
      document->image.width != static_cast<std::uint32_t>(kEffectClutWidth)) {
    return std::unexpected(std::format("{} is not a {}-entry effect palette",
                                       converted->entry->path,
                                       kEffectClutWidth));
  }
  return std::move(document->image.texels);
}

namespace {
// The disc queue, the high heap, the overlay arena, VRAM and the texture
// pages.
class LiveResourceHost final : public EffectResourceHost {
 public:
  EffectDiscLayout DiscLayout() override {
    return {.package_base_lba = VS_E000_P_LBA,
            .purge_overlay = {.lba = VS_EFFPURGE_BIN_LBA,
                              .size = VS_EFFPURGE_BIN_SIZE},
            .texture_staging_bytes = VS_E000_0_FBT_SIZE};
  }

  DiscReadId AllocateRead(DiscRead read) override {
    const VS::Content::DiscFile file = DiscFileOf(read);
    return IdOf<DiscReadId>(Content::AllocateLiveDiscRequest(&file));
  }

  void EnqueueRead(DiscReadId read, EffectBufferId destination) override {
    Content::EnqueueLiveDiscRequest(PointerOf<VS::Content::DiscQueueSlot>(read),
                                    PointerOf<void>(destination));
  }

  DiscReadId CompleteRefusedRead(DiscRead read,
                                 EffectBufferId destination) override {
    const VS::Content::DiscFile file = DiscFileOf(read);
    return IdOf<DiscReadId>(Content::CompleteLiveRefusedDiscRequest(
        &file, PointerOf<void>(destination)));
  }

  bool ReadLoaded(DiscReadId read) override {
    return PointerOf<const VS::Content::DiscQueueSlot>(read)->state ==
           VS::Content::DiscRequestState::kLoaded;
  }

  void ReleaseRead(DiscReadId read) override {
    Content::ReleaseLiveDiscRequest(
        PointerOf<VS::Content::DiscQueueSlot>(read));
  }

  EffectBuffer AllocateBuffer(std::size_t bytes) override {
    return BufferOf(Memory::AllocateLiveFromHighEnd(bytes));
  }

  void FreeBuffer(EffectBufferId buffer) override {
    Memory::FreeLiveFromHighEnd(PointerOf<const void>(buffer));
  }

  void AcquirePurgeOverlayWorkspace() override {
    static_cast<void>(Content::AcquireLiveOverlayWorkspace(kEffectOwnerTag));
  }

  // The overlay slot up to the end of the overlay arena.
  EffectBuffer PurgeOverlayBuffer() override {
    const void* slot = VS::Core::kOverlaySlots[kPurgeOverlaySlot];
    const auto address = reinterpret_cast<std::uintptr_t>(slot);
    const auto arena =
        reinterpret_cast<std::uintptr_t>(Core::overlayArena.data());
    const std::uintptr_t end = arena + Core::overlayArena.size();
    if (slot == nullptr) return {};
    return {.id = IdOf<EffectBufferId>(slot),
            .capacity = address < arena || address >= end ? 0 : end - address};
  }

  void ReserveTexturePage(std::uint32_t page) override {
    static_cast<void>(
        Render::ReserveLiveTexturePageRange(page, 1, kEffectOwnerTag));
  }

  void RetireTexturePage(std::uint32_t page) override {
    static_cast<void>(
        Render::RetireLiveTexturePageRange(page, 1, kEffectOwnerTag));
  }

  void UploadImage(const VramRect& area, EffectBufferId pixels) override {
    RECT rectangle{};
    rectangle.x = area.x;
    rectangle.y = area.y;
    rectangle.w = area.w;
    rectangle.h = area.h;
    static_cast<void>(LoadImage(&rectangle, PointerOf<std::uint32_t>(pixels)));
  }

  std::optional<std::vector<std::uint16_t>> LoadPalette(
      DiscRead source) override {
    auto palette = LoadLiveEffectPalette(source);
    if (palette) return std::move(*palette);
    if (ShouldReport(paletteErrors)) {
      VS::Core::LogError("effects", "effect palette at LBA {}: {} ({} errors)",
                         source.lba, palette.error(), paletteErrors);
    }
    return std::nullopt;
  }

  void UploadPalette(const VramRect& area,
                     std::span<const std::uint16_t> words) override {
    Render::GamePages().Upload(
        {.x = area.x, .y = area.y, .w = area.w, .h = area.h}, words.data());
  }
};

[[nodiscard]] EffectResourceHost& ResourceHost() {
  static LiveResourceHost host;
  return host;
}

// The request pointers, the particle pool and the reloaded block pointers of
// the battle's package records.
class LiveLifecycleHost final : public EffectLifecycleHost {
 public:
  void ResetPackageRequests() override {
    secondaryPackage.curve_block = nullptr;
    secondaryPackage.request = &secondaryRequest;
    actionPackage.request = &actionRequest;
  }

  void BindEmptyPackage(EffectPackageId package) override {
    VS::Effects::package = PointerOf<LiveEffectPackage>(package);
    LiveBindEffectPackageBlocks(emptyPackage);
  }

  void AcquireParticlePool(std::int32_t count) override {
    LiveAcquireParticlePool(count);
  }

  void ClearReloadedBlocks() override {
    if (VS::Effects::package != nullptr) {
      VS::Effects::package->parameters = nullptr;
    }
    actionPackage.curve_block = nullptr;
    actionPackage.sound_sequences = nullptr;
    actionPackage.sound_bank = nullptr;
    actionPackage.auxiliary_sound = nullptr;
  }
};

[[nodiscard]] EffectLifecycleHost& LifecycleHost() {
  static LiveLifecycleHost host;
  return host;
}

// The particle pool's gravity and drag records.
class LiveParticleForceHost final : public ParticleForceHost {
 public:
  void SetParticleGravity(Core::Math::Vec3 gravity) override {
    VECTOR vector{gravity.x, gravity.y, gravity.z, 0};
    LiveSetParticleGravity(&vector);
  }

  void SetParticleDrag(std::int32_t drag) override {
    int value = drag;
    LiveSetParticleDrag(&value);
  }
};

[[nodiscard]] std::span<const EffectRenderer> EntryPoints() {
  return kEffectEntryPoints;
}

// EFFPURGE.BIN's entry points, called as updates.
class LivePluginUpdateHost final : public PluginUpdateHost {
 public:
  std::size_t EntryCount() override { return EntryPoints().size(); }

  bool HasPluginEntry(std::size_t entry) override {
    return EntryPoints()[entry] != nullptr;
  }

  std::int32_t UpdatePlugin(std::size_t entry) override {
    return EntryPoints()[entry](nullptr, kPluginUpdateMessage, 0);
  }
};

void PublishRenderers(const EffectRendererTable& renderers) {
  for (std::size_t i = 0; i < renderers.size(); ++i) {
    if (const std::optional<PluginEntry> entry = renderers.at(i)) {
      effectRenderers[i] = EntryPoints()[static_cast<std::size_t>(*entry)];
    }
  }
}

[[nodiscard]] const char* RefusalReason(ResourceRefusal refusal) {
  switch (refusal) {
    case ResourceRefusal::kUnknownState:
      return "a load state the original's switch lacks";
    case ResourceRefusal::kAssetIndex:
      return "a texture index past the load list";
    case ResourceRefusal::kTextureSlot:
      return "a texture slot past the effect pages";
    case ResourceRefusal::kNoBuffer:
      return "no destination buffer";
    case ResourceRefusal::kReadTooLarge:
      return "a read longer than its buffer";
    case ResourceRefusal::kPalette:
      return "a CLUT without a valid converted palette that fits its slot";
    case ResourceRefusal::kNone:
      break;
  }
  return "nothing";
}

using LoadStep = ResourcePoll (*)(EffectResourceState&, EffectResourceHost&);

// One load step over the mirrored loader state; the original's int result.
int PollLoader(const char* entry, LoadStep step) {
  EffectResourceState state = LoadLiveEffectResourceState();
  const ResourcePoll poll = step(state, ResourceHost());
  StoreLiveEffectResourceState(state);
  if (poll.refusal != ResourceRefusal::kNone) {
    ReportRefusal(entry, RefusalReason(poll.refusal));
  }
  return poll.pending ? 1 : 0;
}

using ContextStep = void (*)(EffectResourceContext&) noexcept;

void UpdateContext(ContextStep step) {
  EffectResourceContext context = LoadLiveEffectResourceContext();
  step(context);
  StoreLiveEffectResourceContext(context);
}

void ReportConfiguration(const EffectPurgeConfiguration& configuration,
                         int effect_id) {
  if (configuration.textures == PurgeTableRefusal::kNone &&
      configuration.plugins == PurgeTableRefusal::kNone &&
      configuration.package == PurgeTableRefusal::kNone) {
    return;
  }
  if (ShouldReport(refusedCalls)) {
    VS::Core::LogWarn(
        "effects",
        "effect resources: effect {} refused tables: textures {}, "
        "plugins {}, package {} ({} refusals)",
        effect_id, static_cast<int>(configuration.textures),
        static_cast<int>(configuration.plugins),
        static_cast<int>(configuration.package), refusedCalls);
  }
}
}  // namespace

std::span<std::byte> LiveBoundPackageBlock(PackageBlockType type) {
  if (VS::Effects::package == nullptr) return {};
  const PackageRecord* record = FindPackageRecord(VS::Effects::package);
  if (record == nullptr) return {};
  const std::span<std::byte> view = BlockView(record->blocks, type);
  if (view.data() == nullptr ||
      view.data() != LegacyBlock(*VS::Effects::package, type)) {
    return {};
  }
  return view;
}

std::optional<PackageWalk> BindLiveEffectPackage(std::span<std::byte> package) {
  if (VS::Effects::package == nullptr) {
    ReportRefusal("bind package", "no current package");
    return std::nullopt;
  }
  ForgetLivePackageExtents();
  PackageRecord& record = PackageRecordFor(VS::Effects::package);
  LivePackageHost host;
  const PackageWalk walk =
      BindEffectPackageBlocks(package, record.blocks, host);
  PublishBlocks(*VS::Effects::package, record.blocks, walk);
  RecordExtents(record.blocks, walk);
  ReportWalk(walk);
  return walk;
}

EffectResourceState LoadLiveEffectResourceState() {
  return {
      .read = IdOf<DiscReadId>(resourceReadSlot),
      .package_buffer = BufferOf(packageBuffer),
      .texture_buffer = BufferOf(textureLoadBuffer),
      .purge_overlay_loaded = purgeOverlayLoaded != 0,
      .package_file = packageFile,
      .texture_index = textureLoadIndex,
      .texture_state = static_cast<TextureLoadState>(textureLoadState),
      .textures = TexturesOf(textureLoads),
      .loaded_texture_pages = static_cast<std::uint32_t>(loadedTextureSlots),
      .plugin_state = static_cast<PluginLoadState>(pluginLoadState),
      .plugin_file = pluginFile,
      .plugin_entry_count = pluginEntryPointCount};
}

void StoreLiveEffectResourceState(const EffectResourceState& state) {
  resourceReadSlot = PointerOf<VS::Content::DiscQueueSlot>(state.read);
  packageBuffer = PointerOf<void>(state.package_buffer.id);
  textureLoadBuffer = PointerOf<std::uint32_t>(state.texture_buffer.id);
  purgeOverlayLoaded = state.purge_overlay_loaded ? 1 : 0;
  packageFile = state.package_file;
  textureLoadIndex = state.texture_index;
  textureLoadState = static_cast<std::uint32_t>(state.texture_state);
  StoreTextures(textureLoads, state.textures);
  loadedTextureSlots = static_cast<int>(state.loaded_texture_pages);
  pluginLoadState = static_cast<std::uint32_t>(state.plugin_state);
  pluginFile = state.plugin_file;
  pluginEntryPointCount = state.plugin_entry_count;
}

EffectResourceContext LoadLiveEffectResourceContext() {
  return {.current = IdOf<EffectPackageId>(VS::Effects::package),
          .saved = IdOf<EffectPackageId>(savedPackage),
          .action = IdOf<EffectPackageId>(&actionPackage),
          .secondary = IdOf<EffectPackageId>(&secondaryPackage)};
}

void StoreLiveEffectResourceContext(const EffectResourceContext& context) {
  VS::Effects::package = PointerOf<LiveEffectPackage>(context.current);
  savedPackage = PointerOf<LiveEffectPackage>(context.saved);
}

EffectPurgeTables LiveEffectPurgeTables() {
  return {.clut_rows = kFbcClutRows,
          .clut_files = kFbcFiles,
          .clut_slots = kFbcSlots,
          .texture_counts = kFbtCounts,
          .texture_files = kFbtFiles,
          .texture_slots = kFbtSlots,
          .package_files = kPackageFiles,
          .plugin_files = kPluginFiles,
          .entry_point_indices = kEffectEntryPointIndices,
          .entry_point_count = kEntryPointCount};
}

// decomp: BATTLE.PRG func_800D7814 (src/BATTLE/BATTLE.PRG/6E4CC.c:75)
void LiveInitializeEffectResources() {
  EffectResourceState state = LoadLiveEffectResourceState();
  EffectResourceContext context = LoadLiveEffectResourceContext();
  InitializeEffectResources(state, context, LifecycleHost());
  StoreLiveEffectResourceState(state);
  StoreLiveEffectResourceContext(context);
}

// decomp: BATTLE.PRG func_800D7890 (src/BATTLE/BATTLE.PRG/6E4CC.c:89)
void LiveSelectEffectResourceContext(int use_primary_context) {
  EffectResourceContext context = LoadLiveEffectResourceContext();
  SelectEffectResourceContext(context, use_primary_context);
  StoreLiveEffectResourceContext(context);
}

// decomp: BATTLE.PRG func_800D78B8 (src/BATTLE/BATTLE.PRG/6E4CC.c:100)
void LiveSaveEffectResourceContext() {
  UpdateContext(&SaveEffectResourceContext);
}

// decomp: BATTLE.PRG func_800D78CC (src/BATTLE/BATTLE.PRG/6E4CC.c:102)
void LiveRestoreEffectResourceContext() {
  UpdateContext(&RestoreEffectResourceContext);
}

// decomp: BATTLE.PRG func_800D78F0 (src/BATTLE/BATTLE.PRG/6E4CC.c:106)
void LiveUpdateParticleGravityAndDrag() {
  LiveParticleForceHost host;
  const ParticleForceStatus status = UpdateParticleGravityAndDrag(
      LiveBoundPackageBlock(PackageBlockType::kParameters),
      LiveEffectCurveSamples(), host);
  if (status == ParticleForceStatus::kNoParameters) {
    ReportRefusal("particle forces", "no block-9 parameters");
  } else if (status == ParticleForceStatus::kCurveIndex) {
    ReportRefusal("particle forces", "a curve past the sampled curves");
  }
}

// decomp: BATTLE.PRG loadEffPurge (src/BATTLE/BATTLE.PRG/6E4CC.c:199; was
// vs_battle_loadEffPurge)
int LiveLoadEffectPurgeOverlay() {
  return PollLoader("purge overlay load", &LoadEffectPurgeOverlay);
}

// decomp: BATTLE.PRG func_800D7BF8 (src/BATTLE/BATTLE.PRG/6E4CC.c:209)
int LiveLoadEffectPackage() {
  return PollLoader("package load", &LoadEffectPackage);
}

// decomp: BATTLE.PRG func_800D7C5C (src/BATTLE/BATTLE.PRG/6E4CC.c:221)
void LiveReleaseEffectPackageResources() {
  EffectResourceState state = LoadLiveEffectResourceState();
  ReleaseEffectPackageResources(state, ResourceHost());
  StoreLiveEffectResourceState(state);
}

// decomp: BATTLE.PRG func_800D7CFC (src/BATTLE/BATTLE.PRG/6E4CC.c:241)
int LiveUpdateEffectTextureLoading() {
  return PollLoader("texture load", &UpdateEffectTextureLoading);
}

// decomp: BATTLE.PRG func_800D7EF4 (src/BATTLE/BATTLE.PRG/6E4CC.c:310)
int LiveUpdateEffectPluginLoading() {
  return PollLoader("plugin load", &UpdateEffectPluginLoading);
}

// decomp: BATTLE.PRG vs_battle_setEffectExec
// (src/BATTLE/BATTLE.PRG/6E4CC.c:354)
bool LiveSetEffectRenderer(PluginEntry entry, int index) {
  EffectRendererTable renderers{};
  if (static_cast<std::size_t>(entry) >= kEntryPointCount ||
      !SetEffectRenderer(renderers, entry, index)) {
    ReportRefusal("set renderer", "an entry or slot past its table");
    return false;
  }
  PublishRenderers(renderers);
  return true;
}

// decomp: BATTLE.PRG func_800D8060 (src/BATTLE/BATTLE.PRG/6E4CC.c:375)
void LiveBindEffectPackageBlocks(void* package) {
  const std::optional<std::span<std::byte>> bytes = PackageBytes(package);
  if (!bytes) {
    ReportRefusal("bind package",
                  "neither the package buffer nor the empty package");
    return;
  }
  static_cast<void>(BindLiveEffectPackage(*bytes));
}

// decomp: EFFPURGE.BIN func_800F9800 (src/EFFECT/EFFPURGE.BIN/0.c:609)
int LiveInvokeAllPluginUpdates(LiveRendererSlot* /*slot*/,
                               unsigned int /*command*/, int /*packed*/) {
  LivePluginUpdateHost host;
  return InvokeAllPluginUpdates(host).result;
}

// decomp: EFFPURGE.BIN vs_effpurge_exec (src/EFFECT/EFFPURGE.BIN/0.c:930)
void LiveConfigureEffectResources() {
  const int effect_id = actionRequest.effect_id;
  EffectResourceState state = LoadLiveEffectResourceState();
  EffectRendererTable renderers{};
  const EffectPurgeConfiguration configuration = ConfigureEffectResources(
      state, renderers, LiveEffectPurgeTables(), effect_id, LifecycleHost());
  StoreLiveEffectResourceState(state);
  PublishRenderers(renderers);
  ReportConfiguration(configuration, effect_id);
}
}  // namespace VS::Effects
