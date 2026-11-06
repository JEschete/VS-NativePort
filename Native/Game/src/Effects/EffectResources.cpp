// Hand-maintained (tools/port/owned.csv).
#include "Effects/EffectResources.hpp"  // NOLINT(build/include): typed API

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/Interpolation.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/PackageExtents.hpp"

namespace VS::Effects {
namespace {
namespace Math = VS::Core::Math;
constexpr int kByteBits = 8;
constexpr std::size_t kHalfwordBytes = 2;
constexpr std::size_t kWordBytes = 4;
// p_file_t (6F014.c:303): the size in the low halfword, the type above it.
constexpr std::uint32_t kBlockSizeMask = 0xFFFF;
constexpr int kBlockTypeShift = 16;
// pFileCurveBlock (5BF94.h:203): {u16 count, u16 data offset, u8 sizes[]}.
constexpr std::size_t kCurveDataOffsetAt = 2;
constexpr std::size_t kCurveSizesAt = 4;
// pFileBlock2 (6F014.c:298): {s32 count, u16 offsets[count]}.
constexpr std::size_t kAnimationOffsetsAt = 4;
// pFileBlock9 (5BF94.h:177) field offsets.
constexpr std::size_t kGravityRangeAt = 0x0;
constexpr std::size_t kDragRangeAt = 0xC;
constexpr std::size_t kGravityCurveAt = 0x12;
constexpr std::size_t kDragCurveAt = 0x13;
constexpr std::size_t kFrameCountAt = 0x14;
constexpr std::size_t kTransformCountAt = 0x15;
constexpr std::size_t kTimelineIdsAt = 0x16;
// One past the last effect texture page.
constexpr auto kPageEnd = kEffectTexturePageBase + kEffectTexturePageCount;

[[nodiscard]] std::uint32_t ReadLittleEndian(std::span<const std::byte> bytes,
                                             std::size_t offset,
                                             std::size_t width) noexcept {
  std::uint32_t value = 0;
  for (std::size_t i = 0; i < width; ++i) {
    value |= std::to_integer<std::uint32_t>(bytes[offset + i])
             << (i * kByteBits);
  }
  return value;
}

[[nodiscard]] std::uint16_t ReadHalfword(std::span<const std::byte> bytes,
                                         std::size_t offset) noexcept {
  return static_cast<std::uint16_t>(
      ReadLittleEndian(bytes, offset, kHalfwordBytes));
}

[[nodiscard]] std::uint32_t ReadWord(std::span<const std::byte> bytes,
                                     std::size_t offset) noexcept {
  return ReadLittleEndian(bytes, offset, kWordBytes);
}

[[nodiscard]] std::uint8_t ReadByte(std::span<const std::byte> bytes,
                                    std::size_t offset) noexcept {
  return std::to_integer<std::uint8_t>(bytes[offset]);
}

template <typename T>
[[nodiscard]] std::optional<T> TableEntry(std::span<const T> table,
                                          std::int64_t index) noexcept {
  if (index < 0 || static_cast<std::uint64_t>(index) >= table.size()) {
    return std::nullopt;
  }
  return table[static_cast<std::size_t>(index)];
}

[[nodiscard]] Math::ScalarSpan RangeAt(std::span<const std::int16_t> range,
                                       std::size_t pair) noexcept {
  return {range[pair * 2], range[(pair * 2) + 1]};
}

// The VRAM rectangle of an effect texture or CLUT in `slot`, or nullopt for
// a slot past pages 24-31 or CLUT rows 240-244.
[[nodiscard]] std::optional<VramRect> TextureArea(
    const TextureAsset& asset) noexcept {
  if (asset.is_clut) {
    if (asset.slot > static_cast<std::uint16_t>(kEffectClutRows)) {
      return std::nullopt;
    }
    return VramRect{
        .x = kEffectClutX,
        .y = static_cast<std::int16_t>(kEffectClutFirstRow + asset.slot),
        .w = kEffectClutWidth,
        .h = static_cast<std::int16_t>(kEffectClutRows - asset.slot)};
  }
  if (asset.slot >= kEffectTexturePageCount) return std::nullopt;
  const auto page =
      static_cast<std::int32_t>(kEffectTexturePageBase + asset.slot);
  return VramRect{.x = static_cast<std::int16_t>((page % kTexturePagesPerRow) *
                                                 kTexturePageWidth),
                  .y = static_cast<std::int16_t>((page / kTexturePagesPerRow) *
                                                 kTexturePageHeight),
                  .w = kTexturePageWidth,
                  .h = kTexturePageHeight};
}

// Uploads a loaded texture asset, reserving and recording its page.
ResourceRefusal UploadTexture(EffectResourceState& state,
                              EffectResourceHost& host,
                              const TextureAsset& asset) {
  const auto area = TextureArea(asset);
  if (!area) return ResourceRefusal::kTextureSlot;
  if (!asset.is_clut) {
    const std::uint32_t page = kEffectTexturePageBase + asset.slot;
    host.ReserveTexturePage(page);
    state.loaded_texture_pages |= 1U << page;
  }
  host.UploadImage(*area, state.texture_buffer.id);
  return ResourceRefusal::kNone;
}

// func_800D7CFC's state 1: reads the current asset and, once it has loaded,
// uploads it and moves to the next.
ResourcePoll LoadNextTexture(EffectResourceState& state,
                             EffectResourceHost& host) {
  const auto index = state.texture_index;
  if (index < 0 ||
      static_cast<std::size_t>(index) >= state.textures.assets.size()) {
    state.texture_state = TextureLoadState::kLoaded;
    return {.pending = true, .refusal = ResourceRefusal::kAssetIndex};
  }
  const auto layout = host.DiscLayout();
  if (state.texture_buffer.id == EffectBufferId::kNone) {
    state.texture_buffer = host.AllocateBuffer(layout.texture_staging_bytes);
  }
  const auto& asset = state.textures.assets[static_cast<std::size_t>(index)];
  const auto poll =
      PollEffectResourceRead(state, host,
                             {.lba = asset.file.lba + layout.package_base_lba,
                              .size = asset.file.size},
                             state.texture_buffer);
  if (poll.pending) return poll;
  const auto refusal = poll.refusal == ResourceRefusal::kNone
                           ? UploadTexture(state, host, asset)
                           : poll.refusal;
  ++state.texture_index;
  if (state.textures.asset_count == state.texture_index) {
    SetEffectTextureLoadState(state, TextureLoadState::kLoaded);
  }
  return {.pending = true, .refusal = refusal};
}

void RefusePluginLoad(EffectResourceState& state) noexcept {
  ConfigureEffectPluginLoad(state, {}, 0);
  SetEffectPluginLoadState(state, PluginLoadState::kIdle);
}
}  // namespace

// decomp: BATTLE.PRG func_800D7814 (src/BATTLE/BATTLE.PRG/6F014.c:446)
void InitializeEffectResources(EffectResourceState& state,
                               EffectResourceContext& context,
                               EffectLifecycleHost& host) {
  state.read = DiscReadId::kNone;
  state.package_buffer = {};
  state.texture_buffer = {};
  state.loaded_texture_pages = 0;
  state.purge_overlay_loaded = false;
  host.ResetPackageRequests();
  SelectEffectResourceContext(context, 0);
  host.BindEmptyPackage(context.current);
}

// decomp: BATTLE.PRG func_800D7890 (src/BATTLE/BATTLE.PRG/6F014.c:460)
void SelectEffectResourceContext(EffectResourceContext& context,
                                 std::int32_t use_action_package) noexcept {
  context.current =
      use_action_package == 0 ? context.secondary : context.action;
}

// decomp: BATTLE.PRG func_800D78B8 (src/BATTLE/BATTLE.PRG/6F014.c:471)
void SaveEffectResourceContext(EffectResourceContext& context) noexcept {
  context.saved = context.current;
}

// decomp: BATTLE.PRG func_800D78CC (src/BATTLE/BATTLE.PRG/6F014.c:473)
void RestoreEffectResourceContext(EffectResourceContext& context) noexcept {
  context.current = context.saved;
}

// decomp: BATTLE.PRG _parsePfileBlock5 (src/BATTLE/BATTLE.PRG/6F014.c:475)
void BindParticleDefinitions(EffectPackageBlocks& blocks,
                             std::span<std::byte> block) noexcept {
  blocks.particle_definitions = block;
}

// decomp: BATTLE.PRG _parsePfileBlock3 (src/BATTLE/BATTLE.PRG/6F014.c:490)
void BindEffectTimelines(EffectPackageBlocks& blocks,
                         std::span<std::byte> block) noexcept {
  blocks.timelines = block;
}

// decomp: BATTLE.PRG _parsePfileCurveBlock (src/BATTLE/BATTLE.PRG/6F014.c:492)
CurveBinding BindEffectCurves(EffectPackageBlocks& blocks,
                              std::span<std::byte> block) noexcept {
  blocks.curve_block = block;
  if (block.size() < kCurveSizesAt) return {.refused = 1};
  const std::span<const std::byte> bytes = block;
  const std::size_t count = ReadHalfword(bytes, 0);
  CurveBinding binding{.header_count = static_cast<std::int32_t>(count)};
  std::size_t offset = ReadHalfword(bytes, kCurveDataOffsetAt);
  for (std::size_t i = 0; i < count; ++i) {
    // Curves past the capacity overran the package record in the original.
    if (i >= blocks.curves.size()) {
      binding.refused += static_cast<std::int32_t>(count - i);
      break;
    }
    // A size byte past the block leaves its curve and the rest empty, so no
    // view of a previous package survives.
    const std::size_t size_at = kCurveSizesAt + i;
    if (size_at >= block.size()) {
      blocks.curves.at(i) = {};
      ++binding.refused;
      continue;
    }
    const std::size_t size = ReadByte(bytes, size_at);
    if (offset > block.size() || size > block.size() - offset) {
      blocks.curves.at(i) = {};
      ++binding.refused;
    } else {
      blocks.curves.at(i) = block.subspan(offset, size);
      ++binding.bound;
    }
    offset += size;
  }
  return binding;
}

// decomp: BATTLE.PRG _parsePfileBlock8 (src/BATTLE/BATTLE.PRG/6F014.c:506)
void BindEffectPrograms(EffectPackageBlocks& blocks,
                        std::span<std::byte> block) noexcept {
  blocks.programs = block;
}

// decomp: BATTLE.PRG _parsePfileBlock9 (src/BATTLE/BATTLE.PRG/6F014.c:508)
void BindEffectPackageParameters(EffectPackageBlocks& blocks,
                                 std::span<std::byte> block) noexcept {
  blocks.parameters = block;
}

// decomp: BATTLE.PRG _parsePfileBlock10 (src/BATTLE/BATTLE.PRG/6F014.c:510)
void BindEffectTransformDefinitions(EffectPackageBlocks& blocks,
                                    std::span<std::byte> block) noexcept {
  blocks.transform_definitions = block;
}

// decomp: BATTLE.PRG _parsePfileBlock2 (src/BATTLE/BATTLE.PRG/6F014.c:524)
void BindParticleAnimationTable(EffectPackageBlocks& blocks,
                                std::span<std::byte> block) noexcept {
  blocks.animation_table = block;
  blocks.animation_count = block.size() < kWordBytes
                               ? 0
                               : static_cast<std::int32_t>(ReadWord(
                                     std::span<const std::byte>(block), 0));
}

// decomp: BATTLE.PRG func_800D7A90 (src/BATTLE/BATTLE.PRG/6F014.c:530)
std::optional<std::span<std::byte>> ParticleAnimationData(
    const EffectPackageBlocks& blocks, std::int32_t index) noexcept {
  const auto table = blocks.animation_table;
  if (index < 0 || index >= blocks.animation_count) return std::nullopt;
  const std::size_t entry =
      kAnimationOffsetsAt + (static_cast<std::size_t>(index) * kHalfwordBytes);
  if (entry > table.size() || table.size() - entry < kHalfwordBytes) {
    return std::nullopt;
  }
  const std::size_t offset =
      ReadHalfword(std::span<const std::byte>(table), entry);
  if (offset > table.size()) return std::nullopt;
  return table.subspan(offset);
}

// decomp: BATTLE.PRG _parsePfileBlock11 (src/BATTLE/BATTLE.PRG/6F014.c:535)
void BindEffectSoundSequences(EffectPackageBlocks& blocks,
                              std::span<std::byte> block) noexcept {
  blocks.sound_sequences = block;
}

// decomp: BATTLE.PRG _parsePfileBlock12 (src/BATTLE/BATTLE.PRG/6F014.c:537)
void BindEffectSoundBank(EffectPackageBlocks& blocks,
                         std::span<std::byte> block, EffectPackageHost& host) {
  blocks.sound_bank = block;
  host.BindSoundBank(block);
}

// decomp: BATTLE.PRG _parsePfileBlock13 (src/BATTLE/BATTLE.PRG/6F014.c:543)
void BindEffectAuxiliarySoundBank(EffectPackageBlocks& blocks,
                                  std::span<std::byte> block,
                                  EffectPackageHost& host) {
  blocks.auxiliary_sound = block;
  blocks.auxiliary_sound_handle = host.BindAuxiliarySound(block);
}

// decomp: BATTLE.PRG func_800D8060 (src/BATTLE/BATTLE.PRG/6F014.c:746)
PackageWalk BindEffectPackageBlocks(std::span<std::byte> package,
                                    EffectPackageBlocks& blocks,
                                    EffectPackageHost& host) {
  PackageWalk walk{};
  std::size_t offset = 0;
  while (true) {
    if (package.size() - offset < kPackageBlockHeaderBytes) {
      walk.status = PackageWalkStatus::kNoTerminator;
      return walk;
    }
    const std::uint32_t meta =
        ReadWord(std::span<const std::byte>(package), offset);
    const std::uint32_t type = meta >> kBlockTypeShift;
    const std::size_t size = meta & kBlockSizeMask;
    if (type == 0) {
      walk.status = PackageWalkStatus::kEnded;
      return walk;
    }
    if (size < kPackageBlockHeaderBytes) {
      walk.status = PackageWalkStatus::kShortBlock;
      return walk;
    }
    if (size > package.size() - offset) {
      walk.status = PackageWalkStatus::kLeavesPackage;
      return walk;
    }
    const auto payload = package.subspan(offset + kPackageBlockHeaderBytes,
                                         size - kPackageBlockHeaderBytes);
    bool bound = true;
    switch (static_cast<PackageBlockType>(type)) {
      case PackageBlockType::kTextureTables:
        host.BindTextureTables(payload);
        break;
      case PackageBlockType::kAnimationTable:
        BindParticleAnimationTable(blocks, payload);
        break;
      case PackageBlockType::kTimelines:
        BindEffectTimelines(blocks, payload);
        break;
      case PackageBlockType::kCurves:
        walk.curves = BindEffectCurves(blocks, payload);
        break;
      case PackageBlockType::kParticleDefinitions:
        BindParticleDefinitions(blocks, payload);
        break;
      case PackageBlockType::kPrograms:
        BindEffectPrograms(blocks, payload);
        break;
      case PackageBlockType::kParameters:
        BindEffectPackageParameters(blocks, payload);
        break;
      case PackageBlockType::kTransformDefinitions:
        BindEffectTransformDefinitions(blocks, payload);
        break;
      case PackageBlockType::kSoundSequences:
        BindEffectSoundSequences(blocks, payload);
        break;
      case PackageBlockType::kSoundBank:
        BindEffectSoundBank(blocks, payload, host);
        break;
      case PackageBlockType::kAuxiliarySound:
        BindEffectAuxiliarySoundBank(blocks, payload, host);
        break;
      default:
        bound = false;
        break;
    }
    if (bound) walk.bound_types |= 1U << type;
    ++walk.blocks;
    offset += size;
  }
}

std::optional<PackageParameters> ReadPackageParameters(
    std::span<const std::byte> block) noexcept {
  if (block.size() < kPackageParametersRequiredBytes) return std::nullopt;
  PackageParameters parameters{};
  for (std::size_t i = 0; i < parameters.gravity_range.size(); ++i) {
    parameters.gravity_range.at(i) = static_cast<std::int16_t>(
        ReadHalfword(block, kGravityRangeAt + (i * kHalfwordBytes)));
  }
  for (std::size_t i = 0; i < parameters.drag_range.size(); ++i) {
    parameters.drag_range.at(i) = static_cast<std::int16_t>(
        ReadHalfword(block, kDragRangeAt + (i * kHalfwordBytes)));
  }
  parameters.gravity_curve = ReadByte(block, kGravityCurveAt);
  parameters.drag_curve = ReadByte(block, kDragCurveAt);
  parameters.frame_count = ReadByte(block, kFrameCountAt);
  parameters.transform_count = ReadByte(block, kTransformCountAt);
  for (std::size_t i = 0;
       i < parameters.timeline_ids.size() && kTimelineIdsAt + i < block.size();
       ++i) {
    parameters.timeline_ids.at(i) = ReadByte(block, kTimelineIdsAt + i);
  }
  return parameters;
}

// decomp: BATTLE.PRG func_800D78F0 (src/BATTLE/BATTLE.PRG/6F014.c:477)
ParticleForceStatus UpdateParticleGravityAndDrag(
    std::span<const std::byte> parameters,
    std::span<const std::int32_t> curve_samples, ParticleForceHost& host) {
  const auto block = ReadPackageParameters(parameters);
  if (!block) return ParticleForceStatus::kNoParameters;
  const auto gravity_weight =
      TableEntry(curve_samples, std::int64_t{block->gravity_curve});
  const auto drag_weight =
      TableEntry(curve_samples, std::int64_t{block->drag_curve});
  if (!gravity_weight || !drag_weight) return ParticleForceStatus::kCurveIndex;
  const std::span<const std::int16_t> gravity = block->gravity_range;
  host.SetParticleGravity(Math::InterpolateSevenBitVector(
      {RangeAt(gravity, 0), RangeAt(gravity, 1), RangeAt(gravity, 2)},
      *gravity_weight));
  host.SetParticleDrag(Math::InterpolateSevenBitScalar(
      RangeAt(block->drag_range, 0), *drag_weight));
  return ParticleForceStatus::kApplied;
}

// decomp: BATTLE.PRG _loadfile (src/BATTLE/BATTLE.PRG/6F014.c:549)
ResourcePoll PollEffectResourceRead(EffectResourceState& state,
                                    EffectResourceHost& host, DiscRead read,
                                    EffectBuffer destination) {
  if (state.read == DiscReadId::kNone) {
    if (destination.id == EffectBufferId::kNone) {
      return {.pending = false, .refusal = ResourceRefusal::kNoBuffer};
    }
    if (read.size < 0 ||
        static_cast<std::size_t>(read.size) > destination.capacity) {
      return {.pending = false, .refusal = ResourceRefusal::kReadTooLarge};
    }
    state.read = host.AllocateRead(read);
    if (state.read != DiscReadId::kNone) {
      host.EnqueueRead(state.read, destination.id);
    } else {
      state.read = host.CompleteRefusedRead(read, destination.id);
    }
    return {.pending = true};
  }
  if (host.ReadLoaded(state.read)) {
    host.ReleaseRead(state.read);
    state.read = DiscReadId::kNone;
    return {.pending = false};
  }
  return {.pending = true};
}

// decomp: BATTLE.PRG vs_battle_loadEffPurge (src/BATTLE/BATTLE.PRG/6F014.c:570)
ResourcePoll LoadEffectPurgeOverlay(EffectResourceState& state,
                                    EffectResourceHost& host) {
  if (!state.purge_overlay_loaded) {
    host.AcquirePurgeOverlayWorkspace();
    state.purge_overlay_loaded = true;
  }
  return PollEffectResourceRead(state, host, host.DiscLayout().purge_overlay,
                                host.PurgeOverlayBuffer());
}

// decomp: BATTLE.PRG func_800D7BF8 (src/BATTLE/BATTLE.PRG/6F014.c:580)
ResourcePoll LoadEffectPackage(EffectResourceState& state,
                               EffectResourceHost& host) {
  if (state.package_buffer.id == EffectBufferId::kNone) {
    state.package_buffer = host.AllocateBuffer(state.package_file.size);
  }
  const DiscRead read{
      .lba = state.package_file.lba + host.DiscLayout().package_base_lba,
      .size = state.package_file.size};
  return PollEffectResourceRead(state, host, read, state.package_buffer);
}

// decomp: BATTLE.PRG func_800D7C5C (src/BATTLE/BATTLE.PRG/6F014.c:592)
void ReleaseEffectPackageResources(EffectResourceState& state,
                                   EffectResourceHost& host) {
  for (std::uint32_t page = kEffectTexturePageBase; page < kPageEnd; ++page) {
    const std::uint32_t bit = 1U << page;
    if ((state.loaded_texture_pages & bit) != 0) {
      host.RetireTexturePage(page);
      state.loaded_texture_pages &= ~bit;
    }
  }
  if (state.package_buffer.id != EffectBufferId::kNone) {
    host.FreeBuffer(state.package_buffer.id);
    state.package_buffer = {};
  }
}

// decomp: BATTLE.PRG func_800D7CFC (src/BATTLE/BATTLE.PRG/6F014.c:612)
ResourcePoll UpdateEffectTextureLoading(EffectResourceState& state,
                                        EffectResourceHost& host) {
  if (state.texture_state == TextureLoadState::kIdle &&
      state.textures.asset_count != 0) {
    state.texture_index = 0;
    SetEffectTextureLoadState(state, TextureLoadState::kLoading);
  }
  switch (state.texture_state) {
    case TextureLoadState::kLoading:
      return LoadNextTexture(state, host);
    case TextureLoadState::kLoaded:
      if (state.texture_buffer.id != EffectBufferId::kNone) {
        host.FreeBuffer(state.texture_buffer.id);
        state.texture_buffer = {};
      }
      return {.pending = false};
    case TextureLoadState::kIdle:
      return {.pending = false};
  }
  return {.pending = false, .refusal = ResourceRefusal::kUnknownState};
}

// decomp: BATTLE.PRG func_800D7EF4 (src/BATTLE/BATTLE.PRG/6F014.c:681)
ResourcePoll UpdateEffectPluginLoading(EffectResourceState& state,
                                       EffectResourceHost& host) {
  switch (state.plugin_state) {
    case PluginLoadState::kAcquireOverlay:
      if (!state.purge_overlay_loaded) {
        host.AcquirePurgeOverlayWorkspace();
        state.purge_overlay_loaded = true;
      }
      [[fallthrough]];
    case PluginLoadState::kLoading: {
      const DiscRead read{
          .lba = state.plugin_file.lba + host.DiscLayout().package_base_lba,
          .size = state.plugin_file.size};
      const auto poll =
          PollEffectResourceRead(state, host, read, host.PurgeOverlayBuffer());
      if (!poll.pending) {
        SetEffectPluginLoadState(state, PluginLoadState::kLoaded);
      }
      return poll;
    }
    case PluginLoadState::kIdle:
    case PluginLoadState::kLoaded:
      return {.pending = false};
  }
  return {.pending = false, .refusal = ResourceRefusal::kUnknownState};
}

// decomp: BATTLE.PRG func_800D7FB4 (src/BATTLE/BATTLE.PRG/6F014.c:712)
void ConfigureEffectPackageLoad(EffectResourceState& state,
                                EffectFileExtent file) noexcept {
  state.package_file = file;
}

// decomp: BATTLE.PRG vs_battle_configurePlg (src/BATTLE/BATTLE.PRG/6F014.c:718)
void ConfigureEffectPluginLoad(EffectResourceState& state,
                               EffectFileExtent file,
                               std::int32_t entry_count) noexcept {
  state.plugin_file = file;
  state.plugin_entry_count = entry_count;
}

// decomp: BATTLE.PRG vs_battle_setEffectExec
// (src/BATTLE/BATTLE.PRG/6F014.c:725)
bool SetEffectRenderer(EffectRendererTable& renderers, PluginEntry entry,
                       std::int32_t index) noexcept {
  if (index < 0 || static_cast<std::size_t>(index) >= renderers.size()) {
    return false;
  }
  renderers.at(static_cast<std::size_t>(index)) = entry;
  return true;
}

// decomp: BATTLE.PRG func_800D7FFC (src/BATTLE/BATTLE.PRG/6F014.c:730)
void SetEffectTextureAssetCount(EffectResourceState& state,
                                std::int32_t asset_count) noexcept {
  state.textures.asset_count = asset_count;
}

// decomp: BATTLE.PRG vs_battle_configureEffectFbLoad
// (src/BATTLE/BATTLE.PRG/6F014.c:732)
bool ConfigureEffectTextureAsset(EffectResourceState& state,
                                 const TextureAsset& asset,
                                 std::int32_t index) noexcept {
  if (index < 0 ||
      static_cast<std::size_t>(index) >= state.textures.assets.size()) {
    return false;
  }
  state.textures.assets.at(static_cast<std::size_t>(index)) = asset;
  return true;
}

// decomp: BATTLE.PRG func_800D8038 (src/BATTLE/BATTLE.PRG/6F014.c:740)
void SetEffectTextureLoadState(EffectResourceState& state,
                               TextureLoadState load_state) noexcept {
  state.texture_state = load_state;
}

// decomp: BATTLE.PRG func_800D8044 (src/BATTLE/BATTLE.PRG/6F014.c:742)
TextureLoadState EffectTextureLoadState(
    const EffectResourceState& state) noexcept {
  return state.texture_state;
}

// decomp: BATTLE.PRG vs_battle_setPlgLoadState
// (src/BATTLE/BATTLE.PRG/6F014.c:744)
void SetEffectPluginLoadState(EffectResourceState& state,
                              PluginLoadState load_state) noexcept {
  state.plugin_state = load_state;
}

// decomp: EFFPURGE.BIN func_800F9800 (src/EFFECT/EFFPURGE.BIN/0.c:590)
PluginUpdateSweep InvokeAllPluginUpdates(PluginUpdateHost& host) {
  PluginUpdateSweep sweep{};
  const std::size_t count = host.EntryCount();
  for (std::size_t entry = 0; entry < count; ++entry) {
    if (!host.HasPluginEntry(entry)) continue;
    sweep.result = host.UpdatePlugin(entry);
    ++sweep.updated;
  }
  return sweep;
}

// decomp: EFFPURGE.BIN vs_effpurge_exec (src/EFFECT/EFFPURGE.BIN/0.c:911)
EffectPurgeConfiguration ConfigureEffectResources(
    EffectResourceState& state, EffectRendererTable& renderers,
    const EffectPurgeTables& tables, std::int32_t effect_id,
    EffectLifecycleHost& host) {
  EffectPurgeConfiguration configuration{};
  SetEffectTextureLoadState(state, TextureLoadState::kIdle);
  configuration.textures = ConfigureTextureLoads(state, tables, effect_id);
  configuration.plugins =
      ConfigurePluginLoad(state, renderers, tables, effect_id);
  const auto package = TableEntry(tables.package_files, effect_id);
  ConfigureEffectPackageLoad(state, package.value_or(EffectFileExtent{}));
  if (!package) configuration.package = PurgeTableRefusal::kEffectId;
  host.AcquireParticlePool(kEffectParticlePoolSize);
  host.ClearReloadedBlocks();
  return configuration;
}

// decomp: EFFPURGE.BIN _configureFb (src/EFFECT/EFFPURGE.BIN/0.c:925)
PurgeTableRefusal ConfigureTextureLoads(EffectResourceState& state,
                                        const EffectPurgeTables& tables,
                                        std::int32_t effect_id) noexcept {
  const auto uses = TableEntry(tables.uses_textures, effect_id);
  const auto count = TableEntry(tables.texture_counts, effect_id);
  const auto clut_file = TableEntry(tables.clut_files, effect_id);
  const auto clut_slot = TableEntry(tables.clut_slots, effect_id);
  if (!uses || !count || !clut_file || !clut_slot) {
    SetEffectTextureAssetCount(state, 0);
    return PurgeTableRefusal::kEffectId;
  }
  std::size_t first = 0;
  for (std::size_t id = 0; id < static_cast<std::size_t>(effect_id); ++id) {
    first += tables.texture_counts[id];
  }
  const std::size_t texture_count = *count;
  // The CLUT and the FBTs take indices 0..count of the load list.
  if (texture_count >= kTextureAssetCapacity ||
      first + texture_count > tables.texture_files.size() ||
      first + texture_count > tables.texture_slots.size()) {
    SetEffectTextureAssetCount(state, 0);
    return PurgeTableRefusal::kTextureRange;
  }
  SetEffectTextureAssetCount(
      state, static_cast<std::int32_t>(texture_count) + (*uses != 0 ? 1 : 0));
  ConfigureEffectTextureAsset(
      state, {.file = *clut_file, .slot = *clut_slot, .is_clut = true}, 0);
  for (std::size_t i = 0; i < texture_count; ++i) {
    ConfigureEffectTextureAsset(state,
                                {.file = tables.texture_files[first + i],
                                 .slot = tables.texture_slots[first + i],
                                 .is_clut = false},
                                static_cast<std::int32_t>(i + 1));
  }
  return PurgeTableRefusal::kNone;
}

// decomp: EFFPURGE.BIN _configurePlg (src/EFFECT/EFFPURGE.BIN/0.c:953)
PurgeTableRefusal ConfigurePluginLoad(EffectResourceState& state,
                                      EffectRendererTable& renderers,
                                      const EffectPurgeTables& tables,
                                      std::int32_t effect_id) noexcept {
  const auto first = TableEntry(tables.entry_point_indices, effect_id);
  const auto next =
      TableEntry(tables.entry_point_indices, std::int64_t{effect_id} + 1);
  if (!first || !next) {
    RefusePluginLoad(state);
    return PurgeTableRefusal::kEffectId;
  }
  const std::int32_t count = *next - *first;
  // An effect without plugins never reads its plugin file, whose entry the
  // table may lack.
  const auto file = count == 0 ? TableEntry(tables.plugin_files, effect_id)
                                     .value_or(EffectFileExtent{})
                               : TableEntry(tables.plugin_files, effect_id);
  if (count < 0 || static_cast<std::size_t>(count) > renderers.size() ||
      std::size_t{*first} + static_cast<std::size_t>(count) >
          tables.entry_point_count ||
      !file) {
    RefusePluginLoad(state);
    return PurgeTableRefusal::kPluginRange;
  }
  ConfigureEffectPluginLoad(state, *file, count);
  for (std::int32_t i = 0; i < count; ++i) {
    SetEffectRenderer(renderers, static_cast<PluginEntry>(*first + i), i);
  }
  SetEffectPluginLoadState(state, count != 0 ? PluginLoadState::kAcquireOverlay
                                             : PluginLoadState::kIdle);
  return PurgeTableRefusal::kNone;
}
}  // namespace VS::Effects
