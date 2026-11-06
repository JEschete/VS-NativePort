// Hand-maintained (tools/port/owned.csv).
#include "Effects/EffectResourcesLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <type_traits>

#include "Audio/SoundBanksLive.hpp"
#include "Audio/StreamControlLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/OverlayWorkspaceLive.hpp"
#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/EffectResources.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/TextureBindings.hpp"
#include "Legacy/Psx.h"
#include "Render/TexturePagesLive.hpp"

extern "C" {
#include "Content/DiscLayout.h"
#include "Core/Arena.h"
#include "Core/Overlays.h"
#include "DiscQueue.h"
#include "Effects/EffectResourcesLiveState.h"
#include "Effects/Loading.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/PurgeTables.h"
#include "Effects/Runtime.h"

// decomp: D_800F5630 (D_800F5630@BATTLE/BATTLE.PRG)
int Effects_textureLoadIndex;
// decomp: D_800F5638 (D_800F5638@BATTLE/BATTLE.PRG)
Effects_TextureLoadList Effects_textureLoads;
// decomp: vs_battle_pfileBuf (vs_battle_pfileBuf@BATTLE/BATTLE.PRG)
void* Effects_packageBuffer;
// decomp: D_800F5680 (D_800F5680@BATTLE/BATTLE.PRG)
u_int Effects_textureLoadState;
// decomp: D_800F5684 (D_800F5684@BATTLE/BATTLE.PRG)
int Effects_purgeOverlayLoaded;
// decomp: vs_battle_pFileLoadContext
// (vs_battle_pFileLoadContext@BATTLE/BATTLE.PRG)
Effects_FileLoadContext Effects_packageFile;
// decomp: D_800F568C (D_800F568C@BATTLE/BATTLE.PRG)
Content_DiscQueueSlot* Effects_resourceReadSlot;
// decomp: _plgLoadState (_plgLoadState@BATTLE/BATTLE.PRG)
u_int Effects_pluginLoadState;
// decomp: _plgLoadContext (_plgLoadContext@BATTLE/BATTLE.PRG)
Effects_FileLoadContext Effects_pluginFile;
// decomp: D_800F56A0 (D_800F56A0@BATTLE/BATTLE.PRG)
u_long* Effects_textureLoadBuffer;
// decomp: _plgEntryPointCount (_plgEntryPointCount@BATTLE/BATTLE.PRG)
int Effects_pluginEntryPointCount;
// decomp: D_800F5874 (D_800F5874@BATTLE/BATTLE.PRG)
int Effects_loadedTextureSlots;
}

static_assert(sizeof(Effects_FileLoadContext) == sizeof(std::uint32_t));
static_assert(std::extent_v<decltype(Effects_TextureLoadList::loadContexts)> ==
              VS::Effects::kTextureAssetCapacity);
static_assert(std::extent_v<decltype(Effects_EffectPackage::curves)> ==
              VS::Effects::kEffectCurveCapacity);
static_assert(std::size(Effects_effectRenderers) ==
              VS::Effects::kEffectRendererCapacity);
static_assert(sizeof(Effects_PackageBlock9) ==
              VS::Effects::kPackageParametersBytes);
static_assert(offsetof(Effects_PackageBlock9, timelineIds) ==
              VS::Effects::kPackageParametersRequiredBytes);

namespace VS::Effects {
namespace {
namespace Memory = VS::Core::Memory;

// EFFPURGE.BIN's tables (src/EFFECT/EFFPURGE.BIN/0.c); PurgeTables.h
// declares most of them without bounds.
// D_800FAE4C, _fbcFiles, _fbcSlots, _fbtCounts, _pFiles and
// _effectEntryPointIndices: one entry per effect.
constexpr std::size_t kEffectCount = 400;
// _fbtFiles and _fbtSlots (the slot table has one trailing entry).
constexpr std::size_t kTextureFileCount = 755;
constexpr std::size_t kTextureSlotCount = 756;
// _plgFiles: effects past it have no plugin.
constexpr std::size_t kPluginFileCount = 302;
// _effectEntryPoints: 343 entries, 314 of them set.
constexpr std::size_t kEntryPointCount = 343;
static_assert(std::extent_v<decltype(Effects_effectEntryPointIndices)> ==
              kEffectCount);

// The packages whose block views are kept: the action and secondary packages
// and the ones tests bind.
constexpr std::size_t kPackageRecords = 4;
// Core_overlaySlots[3]: EFFPURGE.BIN and the plugins load there.
constexpr std::size_t kPurgeOverlaySlot = 3;
// The owner tag of the overlay workspace and the effect texture pages.
constexpr int kEffectOwnerTag = 2;
// func_800F9800 calls each plugin entry with message 2 (update).
constexpr unsigned int kPluginUpdateMessage = 2;

std::uint64_t refused_calls = 0;
std::uint64_t walk_reports = 0;

// True on the 1st, 2nd, 4th, 8th... occurrence, to keep repeated
// diagnostics readable.
[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

void ReportRefusal(const char* entry, const char* reason) {
  if (ShouldReport(refused_calls)) {
    VS_LOG_WARN("effects", "effect resources: {} refused: {} ({} refusals)",
                entry, reason, refused_calls);
  }
}

// A package's typed block views, kept beside the legacy pointers the walk
// publishes into it.
struct PackageRecord {
  const Effects_EffectPackage* package{};
  EffectPackageBlocks blocks;
};

std::array<PackageRecord, kPackageRecords> package_records{};
std::size_t next_package_record = 0;

[[nodiscard]] PackageRecord* FindPackageRecord(
    const Effects_EffectPackage* package) {
  const auto found =
      std::ranges::find(package_records, package, &PackageRecord::package);
  return found == package_records.end() ? nullptr : &*found;
}

[[nodiscard]] PackageRecord& PackageRecordFor(
    const Effects_EffectPackage* package) {
  if (PackageRecord* record = FindPackageRecord(package)) return *record;
  PackageRecord& record = package_records.at(next_package_record);
  next_package_record = (next_package_record + 1) % package_records.size();
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

[[nodiscard]] const void* LegacyBlock(const Effects_EffectPackage& package,
                                      PackageBlockType type) {
  switch (type) {
    case PackageBlockType::kAnimationTable:
      return package.block2Data;
    case PackageBlockType::kTimelines:
      return package.block3Data;
    case PackageBlockType::kCurves:
      return package.curveBlock;
    case PackageBlockType::kParticleDefinitions:
      return package.block5Data;
    case PackageBlockType::kPrograms:
      return package.block8Data;
    case PackageBlockType::kParameters:
      return package.block9Data;
    case PackageBlockType::kTransformDefinitions:
      return package.block10Data;
    case PackageBlockType::kSoundSequences:
      return package.block11Data;
    case PackageBlockType::kSoundBank:
      return package.block12Data;
    case PackageBlockType::kAuxiliarySound:
      return package.block13Data;
    case PackageBlockType::kTextureTables:
      break;
  }
  return nullptr;
}

template <typename T>
[[nodiscard]] T* LegacyPointer(std::span<std::byte> view) {
  return reinterpret_cast<T*>(view.data());
}

void PublishCurves(Effects_EffectPackage& package,
                   const EffectPackageBlocks& blocks,
                   const CurveBinding& binding) {
  package.curveBlock = LegacyPointer<Effects_CurveBlock>(blocks.curve_block);
  const std::size_t count =
      std::min(static_cast<std::size_t>(std::max(binding.header_count, 0)),
               blocks.curves.size());
  // A refused curve has no view and publishes no pointer.
  for (std::size_t i = 0; i < count; ++i) {
    package.curves[i] = LegacyPointer<u_char>(blocks.curves.at(i));
  }
}

// The pointers the original's binders stored, for each block type the walk
// bound; the others keep the previous package's pointers.
void PublishBlocks(Effects_EffectPackage& package,
                   const EffectPackageBlocks& blocks, const PackageWalk& walk) {
  const auto bound = [&walk](PackageBlockType type) {
    return (walk.bound_types & PackageBlockBit(type)) != 0;
  };
  if (bound(PackageBlockType::kAnimationTable)) {
    package.block2Count = blocks.animation_count;
    package.block2Data = LegacyPointer<u_short>(blocks.animation_table);
  }
  if (bound(PackageBlockType::kTimelines)) {
    package.block3Data = LegacyPointer<Effects_PackageBlock3>(blocks.timelines);
  }
  if (bound(PackageBlockType::kCurves)) {
    PublishCurves(package, blocks, walk.curves);
  }
  if (bound(PackageBlockType::kParticleDefinitions)) {
    package.block5Data =
        LegacyPointer<Effects_PackageBlock5>(blocks.particle_definitions);
  }
  if (bound(PackageBlockType::kPrograms)) {
    package.block8Data = LegacyPointer<char>(blocks.programs);
  }
  if (bound(PackageBlockType::kParameters)) {
    package.block9Data =
        LegacyPointer<Effects_PackageBlock9>(blocks.parameters);
  }
  if (bound(PackageBlockType::kTransformDefinitions)) {
    package.block10Data = LegacyPointer<Effects_TransformDefinition>(
        blocks.transform_definitions);
  }
  if (bound(PackageBlockType::kSoundSequences)) {
    package.block11Data = LegacyPointer<u_char>(blocks.sound_sequences);
  }
  if (bound(PackageBlockType::kSoundBank)) {
    package.block12Data = blocks.sound_bank.data();
  }
  if (bound(PackageBlockType::kAuxiliarySound)) {
    package.block13Data = blocks.auxiliary_sound.data();
    package.auxiliarySoundHandle = blocks.auxiliary_sound_handle;
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
}

void ReportWalk(const PackageWalk& walk) {
  if (walk.status == PackageWalkStatus::kEnded && walk.curves.refused == 0) {
    return;
  }
  if (ShouldReport(walk_reports)) {
    VS_LOG_WARN("effects",
                "effect package: walk stopped with status {} after {} blocks; "
                "{} of {} curves refused ({} reports)",
                static_cast<int>(walk.status), walk.blocks, walk.curves.refused,
                walk.curves.header_count, walk_reports);
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
  if (package == static_cast<void*>(Effects_emptyPackage)) {
    return std::as_writable_bytes(std::span(Effects_emptyPackage));
  }
  if (package != Effects_packageBuffer) return std::nullopt;
  const std::size_t capacity = Memory::LivePayloadCapacity(package).value_or(0);
  return std::span(static_cast<std::byte*>(package),
                   std::min<std::size_t>(Effects_packageFile.size, capacity));
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

[[nodiscard]] EffectFileExtent ExtentOf(const Effects_FileLoadContext& file) {
  return {.lba = file.lba, .size = file.size};
}

void StoreExtent(Effects_FileLoadContext& file, EffectFileExtent extent) {
  file.lba = extent.lba;
  file.size = extent.size;
}

[[nodiscard]] TextureLoadList TexturesOf(const Effects_TextureLoadList& list) {
  TextureLoadList textures{.asset_count = list.assetCount};
  for (std::size_t i = 0; i < textures.assets.size(); ++i) {
    textures.assets.at(i) = {.file = ExtentOf(list.loadContexts[i]),
                             .slot = list.slots[i],
                             .is_clut = list.isClut[i] != 0};
  }
  return textures;
}

void StoreTextures(Effects_TextureLoadList& list,
                   const TextureLoadList& textures) {
  list.assetCount = textures.asset_count;
  for (std::size_t i = 0; i < textures.assets.size(); ++i) {
    const TextureAsset& asset = textures.assets.at(i);
    StoreExtent(list.loadContexts[i], asset.file);
    list.slots[i] = asset.slot;
    list.isClut[i] = asset.is_clut ? 1U : 0U;
  }
}

[[nodiscard]] Content_DiscFile DiscFileOf(DiscRead read) {
  Content_DiscFile file{};
  file.lba = read.lba;
  file.size = read.size;
  return file;
}

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
    const Content_DiscFile file = DiscFileOf(read);
    return IdOf<DiscReadId>(Content::AllocateLiveDiscRequest(&file));
  }

  void EnqueueRead(DiscReadId read, EffectBufferId destination) override {
    Content::EnqueueLiveDiscRequest(PointerOf<Content_DiscQueueSlot>(read),
                                    PointerOf<void>(destination));
  }

  DiscReadId CompleteRefusedRead(DiscRead read,
                                 EffectBufferId destination) override {
    const Content_DiscFile file = DiscFileOf(read);
    return IdOf<DiscReadId>(Content::CompleteLiveRefusedDiscRequest(
        &file, PointerOf<void>(destination)));
  }

  bool ReadLoaded(DiscReadId read) override {
    return PointerOf<const Content_DiscQueueSlot>(read)->state ==
           Content_DiscRequestState_Loaded;
  }

  void ReleaseRead(DiscReadId read) override {
    Content::ReleaseLiveDiscRequest(PointerOf<Content_DiscQueueSlot>(read));
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
    const void* slot = Core_overlaySlots[kPurgeOverlaySlot];
    const auto address = reinterpret_cast<std::uintptr_t>(slot);
    const auto arena = reinterpret_cast<std::uintptr_t>(Core_overlayArena);
    const std::uintptr_t end = arena + sizeof(Core_overlayArena);
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
    static_cast<void>(LoadImage(&rectangle, PointerOf<u_long>(pixels)));
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
    Effects_secondaryPackage.curveBlock = nullptr;
    Effects_secondaryPackage.request = &Effects_secondaryRequest;
    Effects_actionPackage.request = &Effects_actionRequest;
  }

  void BindEmptyPackage(EffectPackageId package) override {
    Effects_package = PointerOf<Effects_EffectPackage>(package);
    LiveBindEffectPackageBlocks(Effects_emptyPackage);
  }

  void AcquireParticlePool(std::int32_t count) override {
    Effects_AcquireParticlePool(count);
  }

  void ClearReloadedBlocks() override {
    if (Effects_package != nullptr) Effects_package->block9Data = nullptr;
    Effects_actionPackage.curveBlock = nullptr;
    Effects_actionPackage.block11Data = nullptr;
    Effects_actionPackage.block12Data = nullptr;
    Effects_actionPackage.block13Data = nullptr;
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
    Effects_ParticleGravity vector{gravity.x, gravity.y, gravity.z, 0};
    Effects_SetParticleGravity(&vector);
  }

  void SetParticleDrag(std::int32_t drag) override {
    int value = drag;
    Effects_SetParticleDrag(&value);
  }
};

[[nodiscard]] std::span<const Effects_EffectRenderer> EntryPoints() {
  return {Effects_effectEntryPoints, kEntryPointCount};
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
      Effects_effectRenderers[i] =
          EntryPoints()[static_cast<std::size_t>(*entry)];
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

template <std::size_t N>
[[nodiscard]] std::span<const EffectFileExtent> CopyExtents(
    const Effects_FileLoadContext* files,
    std::array<EffectFileExtent, N>& out) {
  std::transform(files, files + N, out.begin(), ExtentOf);
  return out;
}

void ReportConfiguration(const EffectPurgeConfiguration& configuration,
                         int effect_id) {
  if (configuration.textures == PurgeTableRefusal::kNone &&
      configuration.plugins == PurgeTableRefusal::kNone &&
      configuration.package == PurgeTableRefusal::kNone) {
    return;
  }
  if (ShouldReport(refused_calls)) {
    VS_LOG_WARN("effects",
                "effect resources: effect {} refused tables: textures {}, "
                "plugins {}, package {} ({} refusals)",
                effect_id, static_cast<int>(configuration.textures),
                static_cast<int>(configuration.plugins),
                static_cast<int>(configuration.package), refused_calls);
  }
}
}  // namespace

std::span<std::byte> LiveBoundPackageBlock(PackageBlockType type) {
  if (Effects_package == nullptr) return {};
  const PackageRecord* record = FindPackageRecord(Effects_package);
  if (record == nullptr) return {};
  const std::span<std::byte> view = BlockView(record->blocks, type);
  if (view.data() == nullptr ||
      view.data() != LegacyBlock(*Effects_package, type)) {
    return {};
  }
  return view;
}

std::optional<PackageWalk> BindLiveEffectPackage(std::span<std::byte> package) {
  if (Effects_package == nullptr) {
    ReportRefusal("bind package", "no current package");
    return std::nullopt;
  }
  ForgetLivePackageExtents();
  PackageRecord& record = PackageRecordFor(Effects_package);
  LivePackageHost host;
  const PackageWalk walk =
      BindEffectPackageBlocks(package, record.blocks, host);
  PublishBlocks(*Effects_package, record.blocks, walk);
  RecordExtents(record.blocks, walk);
  ReportWalk(walk);
  return walk;
}

EffectResourceState LoadLiveEffectResourceState() {
  return {
      .read = IdOf<DiscReadId>(Effects_resourceReadSlot),
      .package_buffer = BufferOf(Effects_packageBuffer),
      .texture_buffer = BufferOf(Effects_textureLoadBuffer),
      .purge_overlay_loaded = Effects_purgeOverlayLoaded != 0,
      .package_file = ExtentOf(Effects_packageFile),
      .texture_index = Effects_textureLoadIndex,
      .texture_state = static_cast<TextureLoadState>(Effects_textureLoadState),
      .textures = TexturesOf(Effects_textureLoads),
      .loaded_texture_pages =
          static_cast<std::uint32_t>(Effects_loadedTextureSlots),
      .plugin_state = static_cast<PluginLoadState>(Effects_pluginLoadState),
      .plugin_file = ExtentOf(Effects_pluginFile),
      .plugin_entry_count = Effects_pluginEntryPointCount};
}

void StoreLiveEffectResourceState(const EffectResourceState& state) {
  Effects_resourceReadSlot = PointerOf<Content_DiscQueueSlot>(state.read);
  Effects_packageBuffer = PointerOf<void>(state.package_buffer.id);
  Effects_textureLoadBuffer = PointerOf<u_long>(state.texture_buffer.id);
  Effects_purgeOverlayLoaded = state.purge_overlay_loaded ? 1 : 0;
  StoreExtent(Effects_packageFile, state.package_file);
  Effects_textureLoadIndex = state.texture_index;
  Effects_textureLoadState = static_cast<u_int>(state.texture_state);
  StoreTextures(Effects_textureLoads, state.textures);
  Effects_loadedTextureSlots = static_cast<int>(state.loaded_texture_pages);
  Effects_pluginLoadState = static_cast<u_int>(state.plugin_state);
  StoreExtent(Effects_pluginFile, state.plugin_file);
  Effects_pluginEntryPointCount = state.plugin_entry_count;
}

EffectResourceContext LoadLiveEffectResourceContext() {
  return {.current = IdOf<EffectPackageId>(Effects_package),
          .saved = IdOf<EffectPackageId>(Effects_savedPackage),
          .action = IdOf<EffectPackageId>(&Effects_actionPackage),
          .secondary = IdOf<EffectPackageId>(&Effects_secondaryPackage)};
}

void StoreLiveEffectResourceContext(const EffectResourceContext& context) {
  Effects_package = PointerOf<Effects_EffectPackage>(context.current);
  Effects_savedPackage = PointerOf<Effects_EffectPackage>(context.saved);
}

EffectPurgeTables LiveEffectPurgeTables() {
  static std::array<EffectFileExtent, kEffectCount> clut_files{};
  static std::array<EffectFileExtent, kTextureFileCount> texture_files{};
  static std::array<EffectFileExtent, kEffectCount> package_files{};
  static std::array<EffectFileExtent, kPluginFileCount> plugin_files{};
  return {.uses_textures = {Effects_effectUsesTextures, kEffectCount},
          .clut_files = CopyExtents(Effects_fbcFiles, clut_files),
          .clut_slots = {Effects_fbcSlots, kEffectCount},
          .texture_counts = {Effects_fbtCounts, kEffectCount},
          .texture_files = CopyExtents(Effects_fbtFiles, texture_files),
          .texture_slots = {Effects_fbtSlots, kTextureSlotCount},
          .package_files = CopyExtents(Effects_packageFiles, package_files),
          .plugin_files = CopyExtents(Effects_pluginFiles, plugin_files),
          .entry_point_indices = Effects_effectEntryPointIndices,
          .entry_point_count = kEntryPointCount};
}

// decomp: BATTLE.PRG func_800D7814 (src/BATTLE/BATTLE.PRG/6F014.c:446)
void LiveInitializeEffectResources() {
  EffectResourceState state = LoadLiveEffectResourceState();
  EffectResourceContext context = LoadLiveEffectResourceContext();
  InitializeEffectResources(state, context, LifecycleHost());
  StoreLiveEffectResourceState(state);
  StoreLiveEffectResourceContext(context);
}

// decomp: BATTLE.PRG func_800D7890 (src/BATTLE/BATTLE.PRG/6F014.c:460)
void LiveSelectEffectResourceContext(int use_primary_context) {
  EffectResourceContext context = LoadLiveEffectResourceContext();
  SelectEffectResourceContext(context, use_primary_context);
  StoreLiveEffectResourceContext(context);
}

// decomp: BATTLE.PRG func_800D78B8 (src/BATTLE/BATTLE.PRG/6F014.c:471)
void LiveSaveEffectResourceContext() {
  UpdateContext(&SaveEffectResourceContext);
}

// decomp: BATTLE.PRG func_800D78CC (src/BATTLE/BATTLE.PRG/6F014.c:473)
void LiveRestoreEffectResourceContext() {
  UpdateContext(&RestoreEffectResourceContext);
}

// decomp: BATTLE.PRG func_800D78F0 (src/BATTLE/BATTLE.PRG/6F014.c:477)
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

// decomp: BATTLE.PRG vs_battle_loadEffPurge (src/BATTLE/BATTLE.PRG/6F014.c:570)
int LiveLoadEffectPurgeOverlay() {
  return PollLoader("purge overlay load", &LoadEffectPurgeOverlay);
}

// decomp: BATTLE.PRG func_800D7BF8 (src/BATTLE/BATTLE.PRG/6F014.c:580)
int LiveLoadEffectPackage() {
  return PollLoader("package load", &LoadEffectPackage);
}

// decomp: BATTLE.PRG func_800D7C5C (src/BATTLE/BATTLE.PRG/6F014.c:592)
void LiveReleaseEffectPackageResources() {
  EffectResourceState state = LoadLiveEffectResourceState();
  ReleaseEffectPackageResources(state, ResourceHost());
  StoreLiveEffectResourceState(state);
}

// decomp: BATTLE.PRG func_800D7CFC (src/BATTLE/BATTLE.PRG/6F014.c:612)
int LiveUpdateEffectTextureLoading() {
  return PollLoader("texture load", &UpdateEffectTextureLoading);
}

// decomp: BATTLE.PRG func_800D7EF4 (src/BATTLE/BATTLE.PRG/6F014.c:681)
int LiveUpdateEffectPluginLoading() {
  return PollLoader("plugin load", &UpdateEffectPluginLoading);
}

// decomp: BATTLE.PRG vs_battle_setEffectExec
// (src/BATTLE/BATTLE.PRG/6F014.c:725)
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

// decomp: BATTLE.PRG func_800D8060 (src/BATTLE/BATTLE.PRG/6F014.c:746)
void LiveBindEffectPackageBlocks(void* package) {
  const std::optional<std::span<std::byte>> bytes = PackageBytes(package);
  if (!bytes) {
    ReportRefusal("bind package",
                  "neither the package buffer nor the empty package");
    return;
  }
  static_cast<void>(BindLiveEffectPackage(*bytes));
}

// decomp: EFFPURGE.BIN func_800F9800 (src/EFFECT/EFFPURGE.BIN/0.c:590)
int LiveInvokeAllPluginUpdates(Effects_RendererSlot* /*slot*/,
                               unsigned int /*command*/, int /*packed*/) {
  LivePluginUpdateHost host;
  return InvokeAllPluginUpdates(host).result;
}

// decomp: EFFPURGE.BIN vs_effpurge_exec (src/EFFECT/EFFPURGE.BIN/0.c:911)
void LiveConfigureEffectResources() {
  const int effect_id = Effects_actionRequest.effectId;
  EffectResourceState state = LoadLiveEffectResourceState();
  EffectRendererTable renderers{};
  const EffectPurgeConfiguration configuration = ConfigureEffectResources(
      state, renderers, LiveEffectPurgeTables(), effect_id, LifecycleHost());
  StoreLiveEffectResourceState(state);
  PublishRenderers(renderers);
  ReportConfiguration(configuration, effect_id);
}
}  // namespace VS::Effects
