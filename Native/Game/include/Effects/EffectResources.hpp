#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"
#include "Effects/PackageExtents.hpp"

// Effect resources (BATTLE.PRG 6F014.c:446-800 and EFFPURGE.BIN 0.c:590-975):
// the package context, the package block walk and binders, the disc reads of
// the purge overlay, the package, the textures and the plugin, and the purge
// tables that configure them. The cores work on typed records and reach the
// disc, the heap, VRAM, the sound banks and the particle pool through hosts;
// EffectResourcesLive.hpp binds them to the battle's globals. Disc requests,
// buffers and packages are opaque ids that are compared and never
// dereferenced here.
namespace VS::Effects {
// A disc request a host issued; kNone is no request.
enum class DiscReadId : std::uintptr_t { kNone = 0 };
// A buffer a host allocated or owns; kNone is no buffer.
enum class EffectBufferId : std::uintptr_t { kNone = 0 };
// One of the effect packages; kNone is no package.
enum class EffectPackageId : std::uintptr_t { kNone = 0 };
// An index into the plugin entry-point table (EFFPURGE.BIN D_800FB34C).
enum class PluginEntry : std::uint16_t {};

inline constexpr std::size_t kEffectRendererCapacity = 8;
inline constexpr std::size_t kTextureAssetCapacity = 8;
// Effect textures take VRAM texture pages 24-31.
inline constexpr std::uint32_t kEffectTexturePageBase = 24;
inline constexpr std::uint32_t kEffectTexturePageCount = 8;
inline constexpr std::int32_t kTexturePagesPerRow = 16;
inline constexpr std::int16_t kTexturePageWidth = 64;
inline constexpr std::int16_t kTexturePageHeight = 256;
// Effect CLUTs fill rows 240-244 from x 768.
inline constexpr std::int16_t kEffectClutX = 768;
inline constexpr std::int16_t kEffectClutFirstRow = 240;
inline constexpr std::int16_t kEffectClutWidth = 256;
inline constexpr std::int16_t kEffectClutRows = 5;
// vs_effpurge_exec reserves the full particle pool.
inline constexpr std::int32_t kEffectParticlePoolSize = 255;
// A package block's {u16 size, u16 type} header word.
inline constexpr std::size_t kPackageBlockHeaderBytes = 4;
// pFileBlock9 (5BF94.h:177).
inline constexpr std::size_t kPackageParametersBytes = 0x1A;
// Its fields through the transform count. Sixteen shipped packages (E025.P
// among them) end block 9 after two timeline ids.
inline constexpr std::size_t kPackageParametersRequiredBytes = 0x16;
// Its gravity range, drag range and timeline-id counts.
inline constexpr std::size_t kGravityRangeCount = 6;
inline constexpr std::size_t kDragRangeCount = 2;
inline constexpr std::size_t kParameterTimelineCount = 4;

// The active package and the one SaveEffectResourceContext kept.
struct EffectResourceContext {
  EffectPackageId current{};
  EffectPackageId saved{};
  EffectPackageId action{};
  EffectPackageId secondary{};
};

// _loadFileContext (6E644.h:7): sectors past E000.P and bytes.
struct EffectFileExtent {
  std::uint16_t lba{};
  std::uint16_t size{};

  friend bool operator==(const EffectFileExtent&,
                         const EffectFileExtent&) = default;
};

// An absolute disc read.
struct DiscRead {
  std::int32_t lba{};
  std::int32_t size{};

  friend bool operator==(const DiscRead&, const DiscRead&) = default;
};

// A destination buffer and the bytes it holds.
struct EffectBuffer {
  EffectBufferId id{};
  std::size_t capacity{};

  friend bool operator==(const EffectBuffer&, const EffectBuffer&) = default;
};

// One texture page (FBT) or CLUT (FBC) load.
struct TextureAsset {
  EffectFileExtent file;
  std::uint16_t slot{};
  bool is_clut{};

  friend bool operator==(const TextureAsset&, const TextureAsset&) = default;
};

// D_800F5638_t (6F014.c:56).
struct TextureLoadList {
  std::int32_t asset_count{};
  std::array<TextureAsset, kTextureAssetCapacity> assets{};
};

// D_800F5680 (func_800D7CFC).
enum class TextureLoadState : std::uint32_t {
  kIdle = 0,
  kLoading = 1,
  kLoaded = 2,
};

// _plgLoadState (func_800D7EF4).
enum class PluginLoadState : std::uint32_t {
  kIdle = 0,
  kAcquireOverlay = 1,
  kLoading = 2,
  kLoaded = 3,
};

// The loader state the original kept in BATTLE.PRG D_800F5630-D_800F5874.
struct EffectResourceState {
  DiscReadId read{};
  EffectBuffer package_buffer{};
  EffectBuffer texture_buffer{};
  bool purge_overlay_loaded{};
  EffectFileExtent package_file{};
  std::int32_t texture_index{};
  TextureLoadState texture_state{};
  TextureLoadList textures{};
  // Bit n set while texture page n holds an effect texture.
  std::uint32_t loaded_texture_pages{};
  PluginLoadState plugin_state{};
  EffectFileExtent plugin_file{};
  std::int32_t plugin_entry_count{};
};

// Where the effect files lie on the disc.
struct EffectDiscLayout {
  // E000.P, the base of every EffectFileExtent.
  std::int32_t package_base_lba{};
  DiscRead purge_overlay;
  // Every FBT has this size; the texture buffer holds one.
  std::size_t texture_staging_bytes{};
};

// A VRAM rectangle.
struct VramRect {
  std::int16_t x{};
  std::int16_t y{};
  std::int16_t w{};
  std::int16_t h{};

  friend bool operator==(const VramRect&, const VramRect&) = default;
};

// What a load step refused, beyond what the original did.
enum class ResourceRefusal : std::uint8_t {
  kNone,
  // A state the original's switch lacks: it returned an uninitialized value.
  kUnknownState,
  // A texture index past the load list.
  kAssetIndex,
  // A texture slot past pages 24-31 or a CLUT slot past row 244.
  kTextureSlot,
  // No destination buffer.
  kNoBuffer,
  // A read longer than its destination.
  kReadTooLarge,
};

// A load step's result: pending is the original's return value.
struct ResourcePoll {
  bool pending{};
  ResourceRefusal refusal{ResourceRefusal::kNone};
};

// The disc, heap and VRAM the loaders drive.
class EffectResourceHost {
 public:
  virtual ~EffectResourceHost() = default;
  [[nodiscard]] virtual EffectDiscLayout DiscLayout() = 0;
  // A queued request, or kNone when the queue is full.
  [[nodiscard]] virtual DiscReadId AllocateRead(DiscRead read) = 0;
  virtual void EnqueueRead(DiscReadId read, EffectBufferId destination) = 0;
  // A request the full queue refused, completed by the host.
  [[nodiscard]] virtual DiscReadId CompleteRefusedRead(
      DiscRead read, EffectBufferId destination) = 0;
  [[nodiscard]] virtual bool ReadLoaded(DiscReadId read) = 0;
  virtual void ReleaseRead(DiscReadId read) = 0;
  [[nodiscard]] virtual EffectBuffer AllocateBuffer(std::size_t bytes) = 0;
  virtual void FreeBuffer(EffectBufferId buffer) = 0;
  virtual void AcquirePurgeOverlayWorkspace() = 0;
  // The overlay slot EFFPURGE.BIN and the plugins load into.
  [[nodiscard]] virtual EffectBuffer PurgeOverlayBuffer() = 0;
  virtual void ReserveTexturePage(std::uint32_t page) = 0;
  virtual void RetireTexturePage(std::uint32_t page) = 0;
  virtual void UploadImage(const VramRect& area, EffectBufferId pixels) = 0;
};

// The package bookkeeping outside the loader state.
class EffectLifecycleHost {
 public:
  virtual ~EffectLifecycleHost() = default;
  // Points the action and secondary packages at their requests and clears
  // the secondary package's curve block.
  virtual void ResetPackageRequests() = 0;
  // Binds the built-in empty package into `package`.
  virtual void BindEmptyPackage(EffectPackageId package) = 0;
  virtual void AcquireParticlePool(std::int32_t count) = 0;
  // Clears the blocks a new effect reloads: the current package's
  // parameters and the action package's curves and sound blocks.
  virtual void ClearReloadedBlocks() = 0;
};

// An effect package's blocks, each a view of its payload after the block
// header. Binding a block sets its fields only, so the blocks a package
// lacks keep the previous package's views, as the original's pointers did.
struct EffectPackageBlocks {
  std::span<std::byte> particle_definitions;  // block 5
  std::span<std::byte> animation_table;       // block 2
  std::int32_t animation_count{};
  std::span<std::byte> timelines;    // block 3
  std::span<std::byte> curve_block;  // block 4
  std::array<std::span<std::byte>, kEffectCurveCapacity> curves{};
  std::span<std::byte> programs;               // block 8
  std::span<std::byte> parameters;             // block 9
  std::span<std::byte> transform_definitions;  // block 10
  std::span<std::byte> sound_sequences;        // block 11
  std::span<std::byte> sound_bank;             // block 12
  std::span<std::byte> auxiliary_sound;        // block 13
  std::int32_t auxiliary_sound_handle{};
};

// The package block types func_800D8060 binds.
enum class PackageBlockType : std::uint32_t {
  kTextureTables = 0x1,
  kAnimationTable = 0x2,
  kTimelines = 0x3,
  kCurves = 0x4,
  kParticleDefinitions = 0x5,
  kPrograms = 0x8,
  kParameters = 0x9,
  kTransformDefinitions = 0xA,
  kSoundSequences = 0xB,
  kSoundBank = 0xC,
  kAuxiliarySound = 0xD,
};

// The PackageWalk::bound_types bit of a block type.
[[nodiscard]] constexpr std::uint32_t PackageBlockBit(
    PackageBlockType type) noexcept {
  return 1U << static_cast<std::uint32_t>(type);
}

// The services the block walk binds texture and sound blocks to.
class EffectPackageHost {
 public:
  virtual ~EffectPackageHost() = default;
  virtual void BindTextureTables(std::span<std::byte> block) = 0;
  virtual void BindSoundBank(std::span<std::byte> block) = 0;
  // The stream handle of the bound auxiliary sound block.
  [[nodiscard]] virtual std::int32_t BindAuxiliarySound(
      std::span<std::byte> block) = 0;
};

// Curves the curve binder bound and refused; a block shorter than its
// header refuses one.
struct CurveBinding {
  std::int32_t header_count{};
  std::int32_t bound{};
  std::int32_t refused{};
};

// How the block walk ended.
enum class PackageWalkStatus : std::uint8_t {
  // A zero block type, as in the original.
  kEnded,
  // A block shorter than its header (refused: the original misaligned or
  // looped forever).
  kShortBlock,
  // A block past the package's end (refused).
  kLeavesPackage,
  // No terminating block before the package's end (refused).
  kNoTerminator,
};

// The walk's result; bit n of bound_types is set when a type-n block bound.
struct PackageWalk {
  std::uint32_t bound_types{};
  std::int32_t blocks{};
  CurveBinding curves;
  PackageWalkStatus status{PackageWalkStatus::kEnded};
};

// pFileBlock9 (5BF94.h:177) read from a block-9 payload.
struct PackageParameters {
  std::array<std::int16_t, kGravityRangeCount> gravity_range{};
  std::array<std::int16_t, kDragRangeCount> drag_range{};
  std::uint8_t gravity_curve{};
  std::uint8_t drag_curve{};
  std::uint8_t frame_count{};
  std::uint8_t transform_count{};
  std::array<std::uint8_t, kParameterTimelineCount> timeline_ids{};

  friend bool operator==(const PackageParameters&,
                         const PackageParameters&) = default;
};

// The particle pool's gravity and drag.
class ParticleForceHost {
 public:
  virtual ~ParticleForceHost() = default;
  virtual void SetParticleGravity(Core::Math::Vec3 gravity) = 0;
  virtual void SetParticleDrag(std::int32_t drag) = 0;
};

// What UpdateParticleGravityAndDrag did.
enum class ParticleForceStatus : std::uint8_t {
  kApplied,
  // No block-9 payload, or one ending before the transform count (refused).
  kNoParameters,
  // A curve byte past the sampled curves (refused).
  kCurveIndex,
};

// The renderer table's pending writes: the original stored function
// pointers; an entry is set when its slot was written.
using RendererEntry = std::optional<PluginEntry>;
using EffectRendererTable = std::array<RendererEntry, kEffectRendererCapacity>;

// The EFFPURGE.BIN purge tables (D_800FAE4C-D_800FB34C).
struct EffectPurgeTables {
  std::span<const std::uint8_t> uses_textures;
  std::span<const EffectFileExtent> clut_files;
  std::span<const std::uint8_t> clut_slots;
  std::span<const std::uint8_t> texture_counts;
  std::span<const EffectFileExtent> texture_files;
  std::span<const std::uint8_t> texture_slots;
  std::span<const EffectFileExtent> package_files;
  std::span<const EffectFileExtent> plugin_files;
  std::span<const std::uint16_t> entry_point_indices;
  std::size_t entry_point_count{};
};

// What a purge-table lookup refused.
enum class PurgeTableRefusal : std::uint8_t {
  kNone,
  // An effect id past the table (or, for plugins, without a following
  // index).
  kEffectId,
  // A texture range past the file tables or the load list.
  kTextureRange,
  // An entry-point range past the entry table or the renderer table.
  kPluginRange,
};

// vs_effpurge_exec's refusals per table.
struct EffectPurgeConfiguration {
  PurgeTableRefusal textures{};
  PurgeTableRefusal plugins{};
  PurgeTableRefusal package{};
};

// The plugin entry points func_800F9800 calls.
class PluginUpdateHost {
 public:
  virtual ~PluginUpdateHost() = default;
  [[nodiscard]] virtual std::size_t EntryCount() = 0;
  [[nodiscard]] virtual bool HasPluginEntry(std::size_t entry) = 0;
  // Calls the entry as an update (no slot, message 2, argument 0).
  [[nodiscard]] virtual std::int32_t UpdatePlugin(std::size_t entry) = 0;
};

// The last update's result and the entries updated.
struct PluginUpdateSweep {
  std::int32_t result{};
  std::int32_t updated{};
};

// func_800D7814 (6F014.c:446): clears the loader state, resets the package
// requests, selects the secondary package and binds the empty package.
void InitializeEffectResources(EffectResourceState& state,
                               EffectResourceContext& context,
                               EffectLifecycleHost& host);
// func_800D7890 (6F014.c:460): the action package when `use_action_package`
// is nonzero, else the secondary package.
void SelectEffectResourceContext(EffectResourceContext& context,
                                 std::int32_t use_action_package) noexcept;
// func_800D78B8 (6F014.c:471).
void SaveEffectResourceContext(EffectResourceContext& context) noexcept;
// func_800D78CC (6F014.c:473).
void RestoreEffectResourceContext(EffectResourceContext& context) noexcept;

// _parsePfileBlock5 (6F014.c:475).
void BindParticleDefinitions(EffectPackageBlocks& blocks,
                             std::span<std::byte> block) noexcept;
// _parsePfileBlock3 (6F014.c:490).
void BindEffectTimelines(EffectPackageBlocks& blocks,
                         std::span<std::byte> block) noexcept;
// _parsePfileCurveBlock (6F014.c:492): {u16 count, u16 data offset, u8
// sizes[count]}; curve i starts after curves 0..i-1 from the data offset.
// Curves past the capacity or the block are refused and left empty.
CurveBinding BindEffectCurves(EffectPackageBlocks& blocks,
                              std::span<std::byte> block) noexcept;
// _parsePfileBlock8 (6F014.c:506).
void BindEffectPrograms(EffectPackageBlocks& blocks,
                        std::span<std::byte> block) noexcept;
// _parsePfileBlock9 (6F014.c:508).
void BindEffectPackageParameters(EffectPackageBlocks& blocks,
                                 std::span<std::byte> block) noexcept;
// _parsePfileBlock10 (6F014.c:510).
void BindEffectTransformDefinitions(EffectPackageBlocks& blocks,
                                    std::span<std::byte> block) noexcept;
// _parsePfileBlock2 (6F014.c:524): {s32 count, u16 offsets[count]}; a block
// shorter than its count word binds no animations.
void BindParticleAnimationTable(EffectPackageBlocks& blocks,
                                std::span<std::byte> block) noexcept;
// func_800D7A90 (6F014.c:530): animation `index`'s data, from its offset to
// the block's end; refused (nullopt) past the count or the block.
[[nodiscard]] std::optional<std::span<std::byte>> ParticleAnimationData(
    const EffectPackageBlocks& blocks, std::int32_t index) noexcept;
// _parsePfileBlock11 (6F014.c:535).
void BindEffectSoundSequences(EffectPackageBlocks& blocks,
                              std::span<std::byte> block) noexcept;
// _parsePfileBlock12 (6F014.c:537).
void BindEffectSoundBank(EffectPackageBlocks& blocks,
                         std::span<std::byte> block, EffectPackageHost& host);
// _parsePfileBlock13 (6F014.c:543).
void BindEffectAuxiliarySoundBank(EffectPackageBlocks& blocks,
                                  std::span<std::byte> block,
                                  EffectPackageHost& host);
// func_800D8060 (6F014.c:746): binds each {u32 size | type << 16, payload}
// block until a zero type; unknown types are skipped.
PackageWalk BindEffectPackageBlocks(std::span<std::byte> package,
                                    EffectPackageBlocks& blocks,
                                    EffectPackageHost& host);

// The block-9 record; nullopt for a payload ending before the transform
// count. Timeline ids past the payload read 0 (the original read the next
// block's header there).
[[nodiscard]] std::optional<PackageParameters> ReadPackageParameters(
    std::span<const std::byte> block) noexcept;
// func_800D78F0 (6F014.c:477): the particle gravity and drag at their
// block-9 curves' current samples.
ParticleForceStatus UpdateParticleGravityAndDrag(
    std::span<const std::byte> parameters,
    std::span<const std::int32_t> curve_samples, ParticleForceHost& host);

// _loadfile (6F014.c:549): issues `read` into `destination` when no request
// is in flight (pending), then reports pending until it has loaded.
ResourcePoll PollEffectResourceRead(EffectResourceState& state,
                                    EffectResourceHost& host, DiscRead read,
                                    EffectBuffer destination);
// vs_battle_loadEffPurge (6F014.c:570).
ResourcePoll LoadEffectPurgeOverlay(EffectResourceState& state,
                                    EffectResourceHost& host);
// func_800D7BF8 (6F014.c:580).
ResourcePoll LoadEffectPackage(EffectResourceState& state,
                               EffectResourceHost& host);
// func_800D7C5C (6F014.c:592): retires the effect texture pages and frees
// the package buffer.
void ReleaseEffectPackageResources(EffectResourceState& state,
                                   EffectResourceHost& host);
// func_800D7CFC (6F014.c:612): loads and uploads one texture asset per
// completed read.
ResourcePoll UpdateEffectTextureLoading(EffectResourceState& state,
                                        EffectResourceHost& host);
// func_800D7EF4 (6F014.c:681).
ResourcePoll UpdateEffectPluginLoading(EffectResourceState& state,
                                       EffectResourceHost& host);

// func_800D7FB4 (6F014.c:712).
void ConfigureEffectPackageLoad(EffectResourceState& state,
                                EffectFileExtent file) noexcept;
// vs_battle_configurePlg (6F014.c:718).
void ConfigureEffectPluginLoad(EffectResourceState& state,
                               EffectFileExtent file,
                               std::int32_t entry_count) noexcept;
// vs_battle_setEffectExec (6F014.c:725): false (refused) past the table.
bool SetEffectRenderer(EffectRendererTable& renderers, PluginEntry entry,
                       std::int32_t index) noexcept;
// func_800D7FFC (6F014.c:730).
void SetEffectTextureAssetCount(EffectResourceState& state,
                                std::int32_t asset_count) noexcept;
// vs_battle_configureEffectFbLoad (6F014.c:732): false (refused) past the
// load list.
bool ConfigureEffectTextureAsset(EffectResourceState& state,
                                 const TextureAsset& asset,
                                 std::int32_t index) noexcept;
// func_800D8038 (6F014.c:740).
void SetEffectTextureLoadState(EffectResourceState& state,
                               TextureLoadState load_state) noexcept;
// func_800D8044 (6F014.c:742).
[[nodiscard]] TextureLoadState EffectTextureLoadState(
    const EffectResourceState& state) noexcept;
// vs_battle_setPlgLoadState (6F014.c:744).
void SetEffectPluginLoadState(EffectResourceState& state,
                              PluginLoadState load_state) noexcept;

// func_800F9800 (EFFPURGE 0.c:590): updates every plugin entry in table
// order. A development leftover with no callers.
PluginUpdateSweep InvokeAllPluginUpdates(PluginUpdateHost& host);
// vs_effpurge_exec (EFFPURGE 0.c:911): configures effect `effect_id`'s
// textures, plugin and package and resets the reloaded blocks.
EffectPurgeConfiguration ConfigureEffectResources(
    EffectResourceState& state, EffectRendererTable& renderers,
    const EffectPurgeTables& tables, std::int32_t effect_id,
    EffectLifecycleHost& host);
// _configureFb (EFFPURGE 0.c:925): the CLUT then the effect's FBTs. An
// effect without textures still configures the CLUT at index 0 and counts
// one asset fewer, so its last FBT is not loaded.
PurgeTableRefusal ConfigureTextureLoads(EffectResourceState& state,
                                        const EffectPurgeTables& tables,
                                        std::int32_t effect_id) noexcept;
// _configurePlg (EFFPURGE 0.c:953): the plugin file and its renderers.
PurgeTableRefusal ConfigurePluginLoad(EffectResourceState& state,
                                      EffectRendererTable& renderers,
                                      const EffectPurgeTables& tables,
                                      std::int32_t effect_id) noexcept;
}  // namespace VS::Effects
