// Hand-maintained (tools/port/owned.csv).
// Live binding for the battle asset queue and loaders (ActorAssets.hpp): the
// legacy request queue, the loaders' globals and disc file tables
// (ActorAssetsLive.hpp), the actor and weapon records, the shared sequence
// banks, the texture slot tables, the game heap and the disc request queue
// (CdDriveLive.hpp).
#include "Content/ActorAssetsLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>
#include <span>
#include <tuple>
#include <type_traits>

#include "Actors/ActorRecord.hpp"
#include "Actors/ActorSpawning.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/Animation.hpp"
#include "Actors/AppearanceLive.hpp"
#include "Actors/AppearanceLiveState.hpp"
#include "Actors/ModelInstance.hpp"
#include "Actors/SequencePlaybackLive.hpp"
#include "Actors/ShpModel.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Actors/SkeletonLiveState.hpp"
#include "Actors/TextureAnimationLive.hpp"
#include "Actors/TextureAnimationRecords.hpp"
#include "Actors/Textures.hpp"
#include "Actors/WeaponModel.hpp"
#include "Audio/SfxLive.hpp"
#include "Content/ActorAssets.hpp"
#include "Content/CdDrive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscQueue.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/GimmickResourcesLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Items/Materials.hpp"
#include "Legacy/Psx.hpp"
#include "Render/ModelGeometryLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/Textures.hpp"

using VS::Core::Gte::CVECTOR;
using VS::Core::Gte::SVECTOR;

using VS::Legacy::LoadImage;
using VS::Legacy::RECT;

// decomp: vs_battle_loadObjectDataState
// (vs_battle_loadObjectDataState@BATTLE/BATTLE.PRG)
std::uint8_t VS::Content::objectDataLoadStep;
// decomp: D_800E8F29 (D_800E8F29@BATTLE/BATTLE.PRG)
unsigned char VS::Content::modelParseResetFlag;
// decomp: D_800E8FC4 (D_800E8FC4@BATTLE/BATTLE.PRG)
int VS::Content::assetQueuePeakUsage;
// decomp: D_800F457C (D_800F457C@BATTLE/BATTLE.PRG)
int VS::Content::lastLoadPhaseSnapshot;
// decomp: D_800F4580 (D_800F4580@BATTLE/BATTLE.PRG)
int VS::Content::assetQueueFrameTicks;
// decomp: D_800F45D8 (D_800F45D8@BATTLE/BATTLE.PRG)
int VS::Content::lastAssetRequestType;

namespace VS::Content {
// The loaders' legacy state (ActorAssetsLive.hpp). The phases' contents come
// from the module image (ModuleData.cpp).

// decomp: _loadShpState (_loadShpState@BATTLE/BATTLE.PRG)
std::uint8_t legacyModelLoadPhase;
// decomp: D_800E8F2B (D_800E8F2B@BATTLE/BATTLE.PRG)
std::uint8_t legacySequenceParsePhase;
// decomp: _loadEtmState (_loadEtmState@BATTLE/BATTLE.PRG)
std::uint8_t legacyEtmLoadPhase;
// decomp: _wepFileData (_wepFileData@BATTLE/BATTLE.PRG)
void* legacyWeaponFileBuffer;
// decomp: _wepFileLoaded (_wepFileLoaded@BATTLE/BATTLE.PRG)
std::int32_t legacyWeaponFileLoaded;
// decomp: _wepFile (_wepFile@BATTLE/BATTLE.PRG)
DiscFile legacyWeaponFileLocation;
// decomp: _wepCdSlot (_wepCdSlot@BATTLE/BATTLE.PRG)
DiscQueueSlot* legacyWeaponDiscRequest;
// decomp: _wepFileEpilog (_wepFileEpilog@BATTLE/BATTLE.PRG)
void* legacyWeaponParseCursor;
// decomp: _shpData (_shpData@BATTLE/BATTLE.PRG)
void* legacyShpFileBuffer;
// decomp: _shpAllocated (_shpAllocated@BATTLE/BATTLE.PRG)
std::int32_t legacyShpAllocated;
// decomp: _shpFile (_shpFile@BATTLE/BATTLE.PRG)
DiscFile legacyShpFileLocation;
// decomp: _shpCdSlot (_shpCdSlot@BATTLE/BATTLE.PRG)
DiscQueueSlot* legacyShpDiscRequest;
// decomp: D_800F22B4 (D_800F22B4@BATTLE/BATTLE.PRG)
std::uint8_t* legacyShpParseCursor;
// decomp: D_800F22B8 (D_800F22B8@BATTLE/BATTLE.PRG)
std::uint8_t legacyShpCopiedWordCount;
// decomp: D_800F22BC (D_800F22BC@BATTLE/BATTLE.PRG)
std::int32_t legacyShpHeaderWord;
// decomp: D_800F22C0 (D_800F22C0@BATTLE/BATTLE.PRG)
std::int32_t legacyShpTextureWidth;
// decomp: D_800F22C4 (D_800F22C4@BATTLE/BATTLE.PRG)
std::int32_t legacyShpTextureHeight;
// decomp: D_800F22C8 (D_800F22C8@BATTLE/BATTLE.PRG)
void* legacySequenceFileBuffer;
// decomp: D_800F22D0 (D_800F22D0@BATTLE/BATTLE.PRG)
std::int32_t legacySequenceBufferOwned;
// decomp: D_800F22D8 (D_800F22D8@BATTLE/BATTLE.PRG)
DiscFile legacySequenceFileLocation;
// decomp: D_800F22E0 (D_800F22E0@BATTLE/BATTLE.PRG)
DiscQueueSlot* legacySequenceDiscRequest;
// decomp: D_800F22E4 (D_800F22E4@BATTLE/BATTLE.PRG)
std::uint8_t* legacySequenceParseData;
// decomp: _etmData (_etmData@BATTLE/BATTLE.PRG)
std::uint16_t* legacyEtmFileBuffer;
// decomp: _etmFile (_etmFile@BATTLE/BATTLE.PRG)
DiscFile legacyEtmFileLocation;
// decomp: _etmFileCdSlot (_etmFileCdSlot@BATTLE/BATTLE.PRG)
DiscQueueSlot* legacyEtmDiscRequest;
// decomp: vs_battle_objectDataSlots
// (vs_battle_objectDataSlots@BATTLE/BATTLE.PRG)
legacy_files::Requests legacyActorAssetRequests;
// decomp: vs_battle_commonWepBuffs (vs_battle_commonWepBuffs@BATTLE/BATTLE.PRG)
legacy_files::WeaponModels legacyWeaponModelBuffers;

// The disc file tables; their contents come from the module image
// (ModuleData.cpp).

// decomp: _shpLbaOffsets (_shpLbaOffsets@BATTLE/BATTLE.PRG)
legacy_files::ShpOffsets legacyShpDiscOffsets;
// decomp: _shpFileSectorSizes (_shpFileSectorSizes@BATTLE/BATTLE.PRG)
legacy_files::ShpSectors legacyShpSectorCounts;
// decomp: D_800E8A60 (D_800E8A60@BATTLE/BATTLE.PRG)
legacy_files::SequenceOffsets legacySequenceDiscOffsets;
// decomp: D_800E8B40 (D_800E8B40@BATTLE/BATTLE.PRG)
legacy_files::SequenceSectors legacySequenceSectorCounts;
// decomp: _etmLbaOffsets (_etmLbaOffsets@BATTLE/BATTLE.PRG)
legacy_files::EtmOffsets legacyEtmDiscOffsets;
// decomp: _etmFileSectorSizes (_etmFileSectorSizes@BATTLE/BATTLE.PRG)
legacy_files::EtmSectors legacyEtmSectorCounts;
// decomp: D_800E8D00 (D_800E8D00@BATTLE/BATTLE.PRG)
legacy_files::WepOffsets legacyWepDiscOffsets;
// decomp: _wepFileSectorOffsets (_wepFileSectorOffsets@BATTLE/BATTLE.PRG)
legacy_files::WepSectors legacyWepSectorCounts;
// decomp: D_800E8E8E (D_800E8E8E@BATTLE/BATTLE.PRG)
legacy_files::MaterialOffsets legacyMaterialSequenceDiscOffsets;
// decomp: D_800E8EA3 (D_800E8EA3@BATTLE/BATTLE.PRG)
legacy_files::MaterialSectors legacyMaterialSequenceSectorCounts;
// decomp: D_800E8EA8 (D_800E8EA8@BATTLE/BATTLE.PRG)
legacy_files::ModelOffsets legacyMaterialModelSequenceDiscOffsets;
// decomp: D_800E8EFC (D_800E8EFC@BATTLE/BATTLE.PRG)
legacy_files::ModelSectors legacyMaterialModelSequenceSectorCounts;
}  // namespace VS::Content

namespace VS::Content {
namespace {
static_assert(std::size(legacyActorAssetRequests) == kActorAssetRequestCount);
static_assert(std::size(VS::Actors::actorRecords) == kActorRecordSlots);
static_assert(std::size(VS::Actors::weaponModels) == kWeaponModelSlots);
static_assert(std::size(VS::Render::legacyActorTextureSlots) ==
              kActorTextureSlots);
static_assert(std::size(VS::Render::legacyWeaponTextureSlots) ==
              kWeaponTextureSlots);
static_assert(std::size(VS::Actors::sharedSequenceData) ==
              kSharedSequenceBanks);
static_assert(std::size(VS::Actors::sharedSequenceAnimationIds) ==
              kSharedSequenceBanks);
static_assert(std::size(VS::Actors::sharedSequenceAnimationIds[0]) ==
              kSharedSequenceEntries);
static_assert(std::size(VS::Actors::sharedSequenceEntryParams[0]) ==
              kSharedSequenceEntries);
static_assert(std::size(VS::Actors::sharedSequenceAnimationData[0]) ==
              kSharedSequenceEntries);

static_assert(offsetof(LegacyActorAssetRequest, asset_kind) == 0);
static_assert(offsetof(LegacyActorAssetRequest, target_slot) == 1);
static_assert(offsetof(LegacyActorAssetRequest, model_id) == 2);
static_assert(offsetof(LegacyActorAssetRequest, sub_kind) ==
              offsetof(LegacyActorAssetRequest, placement) +
                  sizeof(std::uint32_t));
static_assert(offsetof(LegacyActorAssetRequest, texture_slot) ==
              offsetof(LegacyActorAssetRequest, sub_kind) + 1);
static_assert(offsetof(LegacyActorAssetRequest, material) ==
              offsetof(LegacyActorAssetRequest, texture_slot) + 1);
static_assert(offsetof(LegacyActorAssetRequest, variant) ==
              offsetof(LegacyActorAssetRequest, material) + 1);

// The instance's flag byte (is_skipped .. is_parsed) follows the counts; the
// Live binding is built without /J, so its char bit-fields are read raw.
constexpr std::size_t kInstanceFlagsAt = 8;
static_assert(offsetof(VS::Actors::ModelInstance, other_polygon_count) +
                  sizeof(std::uint16_t) ==
              kInstanceFlagsAt);
// The bit-field bytes run from the flag byte to reservedD.
constexpr std::size_t kInstanceBitFieldBytes = 5;
static_assert(offsetof(VS::Actors::ModelInstance, reserved0d) ==
              kInstanceFlagsAt + kInstanceBitFieldBytes);
static_assert(offsetof(VS::Actors::ModelInstance, copied_word_count) ==
              offsetof(VS::Actors::ModelInstance, reserved0d) + 1);
// The placement word runs from current_tile_x to reserved5f.
static_assert(offsetof(VS::Actors::ModelInstance, reserved5f) + 1 ==
              offsetof(VS::Actors::ModelInstance, current_tile_x) +
                  sizeof(std::uint32_t));
static_assert(sizeof(CVECTOR) == sizeof(std::uint32_t));
static_assert(sizeof(VS::Render::ColorCode) == sizeof(std::uint32_t));
static_assert(offsetof(VS::Actors::ActorRecord, reserved6f3) + 1 ==
              offsetof(VS::Actors::ActorRecord, fade_blue) +
                  sizeof(std::uint32_t));
// texture_row_offsets shares four bytes with momentum_distance.
static_assert(offsetof(VS::Actors::ActorRecord, momentum_direction) ==
              offsetof(VS::Actors::ActorRecord, texture_row_offsets) +
                  kActorTextureRowOffsets);
static_assert(sizeof(VS::Actors::ActorRecord::palettes) ==
              sizeof(ActorAssetRecord::palettes));
static_assert(sizeof(VS::Actors::ActorRecord::base_palette) ==
              sizeof(ActorAssetRecord::base_palette));
static_assert(sizeof(VS::Actors::ActorRecord::current_palette) ==
              sizeof(ActorAssetRecord::current_palette));
static_assert(sizeof(VS::Actors::ActorRecord::joint_slots) ==
              sizeof(ActorAssetRecord::joint_slots));
static_assert(
    std::extent_v<decltype(VS::Actors::ActorRecord::battle_sequence_lbas)> ==
    std::tuple_size_v<decltype(ActorAssetRecord::battle_sequence_lbas)>);
static_assert(
    std::extent_v<
        decltype(VS::Actors::ActorRecord::chain_attack_animation_ids)> ==
    std::tuple_size_v<decltype(ActorAssetRecord::chain_attack_animation_ids)>);
static_assert(
    std::extent_v<decltype(VS::Actors::ActorRecord::special_sequence_lbas)> ==
    std::tuple_size_v<decltype(ActorAssetRecord::special_sequence_lbas)>);
static_assert(
    std::extent_v<decltype(VS::Actors::ActorRecord::special_sequence_sizes)> ==
    std::tuple_size_v<decltype(ActorAssetRecord::special_sequence_sizes)>);
static_assert(
    std::extent_v<decltype(VS::Actors::ActorRecord::shared_sequences)> ==
    std::tuple_size_v<decltype(ActorAssetRecord::shared_sequences)>);
static_assert(
    std::extent_v<decltype(VS::Actors::ActorRecord::texture_scroll_bounds)> ==
    std::tuple_size_v<decltype(ActorAssetRecord::texture_scroll_bounds)>);
static_assert(
    std::extent_v<decltype(VS::Actors::ActorRecord::texture_scroll)> ==
    std::tuple_size_v<decltype(ActorAssetRecord::texture_scroll)>);
// The SHP texture layout fills texture_overlays and the fields after it.
static_assert(offsetof(VS::Actors::ActorRecord, texture_overlays) +
                  kActorTextureLayoutBytes <=
              sizeof(VS::Actors::ActorRecord));
static_assert(sizeof(VS::Actors::WeaponModel::bones) ==
              kWeaponModelBones * sizeof(SVECTOR));
static_assert(sizeof(VS::Actors::WeaponModel::palettes) ==
              sizeof(WeaponAssetRecord::palettes));
static_assert(sizeof(VS::Actors::WeaponModel::working_palette) ==
              sizeof(WeaponAssetRecord::working_palette));
static_assert(sizeof(VS::Actors::WeaponModel::upload_palette) ==
              sizeof(WeaponAssetRecord::upload_palette));
static_assert(std::is_same_v<std::uint16_t, std::uint16_t>);
// The original's 8-byte texture slot entries.
constexpr std::size_t kTextureSlotBytes = 8;
static_assert(sizeof(VS::Render::TextureSlot) == kTextureSlotBytes);
static_assert(sizeof(RECT) == sizeof(VramRect));
static_assert(std::is_same_v<decltype(DiscFile::lba), std::int32_t>);
static_assert(std::is_same_v<decltype(DiscFile::size), std::int32_t>);

constexpr std::uint8_t kSkippedBit = 0x01;
constexpr std::uint8_t kUvLayoutBit = 0x08;
constexpr std::uint8_t kParsedBit = 0x80;
// is_visible and is_frozen: not loader fields.
constexpr std::uint8_t kKeptInstanceFlags = 0x06;
constexpr unsigned kTextureSlotCountShift = 4;
constexpr std::uint8_t kTextureSlotCountMask = 7;
constexpr std::uint8_t kNibbleMask = 0x0F;
// Handles below this are shared record indices, never memory.
constexpr std::uintptr_t kLowestAddress = 0x10000;
// The largest polygon record func_80099E7C walks (a coloured quad).
constexpr std::size_t kLargestPolygonBytes = 32;
// The sound channels a released model's bank stops (_releaseShp).
constexpr int kModelSoundChannels = 0x180;
// The texture page owner tag of the battle loaders.
constexpr int kLoaderPageOwner = 4;
// Every refusal is reported until this count, then at each power of two.
constexpr std::uint64_t kAlwaysReportedRefusals = 16;

std::uint64_t refusals = 0;

void CountRefusal(const char* what, std::int64_t value) {
  ++refusals;
  if (refusals <= kAlwaysReportedRefusals || std::has_single_bit(refusals)) {
    VS::Core::Warnf(
        "content", "actor assets refused %s (%" PRId64 "); %" PRIu64 " refused",
        what, value, refusals);
  }
}

AssetAddress Address(const void* pointer) {
  return static_cast<AssetAddress>(reinterpret_cast<std::uintptr_t>(pointer));
}

template <typename T = void>
T* Pointer(AssetAddress address) {
  return reinterpret_cast<T*>(static_cast<std::uintptr_t>(address));
}

template <typename Record>
std::uint32_t WordAt(const Record& record, std::size_t at) {
  std::uint32_t value = 0;
  std::memcpy(&value, reinterpret_cast<const std::byte*>(&record) + at,
              sizeof value);
  return value;
}

template <typename Record>
void SetWordAt(Record& record, std::size_t at, std::uint32_t value) {
  std::memcpy(reinterpret_cast<std::byte*>(&record) + at, &value, sizeof value);
}

template <typename Record>
std::uint8_t& ByteAt(Record& record, std::size_t at) {
  return reinterpret_cast<std::uint8_t*>(&record)[at];
}

template <typename Record>
std::uint8_t ByteOf(const Record& record, std::size_t at) {
  return reinterpret_cast<const std::uint8_t*>(&record)[at];
}

bool InRange(int index, int count) { return index >= 0 && index < count; }

VS::Actors::WeaponModel* WeaponAt(int slot) {
  return InRange(slot, kWeaponModelSlots) ? VS::Actors::weaponModels[slot]
                                          : nullptr;
}

VS::Actors::ActorRecord* ActorAt(int slot) {
  return InRange(slot, kActorRecordSlots) ? VS::Actors::actorRecords[slot]
                                          : nullptr;
}

// --- Requests and loader state ---------------------------------------------

ActorAssetRequest LoadRequest(const LegacyActorAssetRequest& in) {
  return {
      .kind = in.asset_kind,
      .target_slot = in.target_slot,
      .model_id = in.model_id,
      .destination = Address(in.destination),
      .data = static_cast<AssetAddress>(in.data_address),
      .placement = in.placement,
      .sub_kind = in.sub_kind,
      .texture_slot = in.texture_slot,
      .material = in.material,
      .variant = in.variant,
  };
}

void StoreRequestFields(const ActorAssetRequest& in,
                        LegacyActorAssetRequest& out) {
  out.asset_kind = in.kind;
  out.target_slot = in.target_slot;
  out.model_id = in.model_id;
  out.destination = Pointer(in.destination);
  out.data_address = static_cast<std::uintptr_t>(in.data);
  out.placement = in.placement;
  out.sub_kind = in.sub_kind;
  out.texture_slot = in.texture_slot;
  out.material = in.material;
  out.variant = in.variant;
}

ActorAssetLoadState LoadState() {
  ActorAssetLoadState state;
  state.model_phase = legacyModelLoadPhase;
  state.parse_phase = modelParseResetFlag;
  state.sequence_parse_phase = legacySequenceParsePhase;
  state.effect_texture_phase = legacyEtmLoadPhase;
  state.weapon_file_loaded = legacyWeaponFileLoaded != 0;
  state.model_file_allocated = legacyShpAllocated != 0;
  state.sequence_file_owned = legacySequenceBufferOwned != 0;
  state.weapon_file = Address(legacyWeaponFileBuffer);
  state.model_file = Address(legacyShpFileBuffer);
  state.sequence_file = Address(legacySequenceFileBuffer);
  state.effect_texture_file = Address(legacyEtmFileBuffer);
  state.weapon_cursor = Address(legacyWeaponParseCursor);
  state.model_cursor = Address(legacyShpParseCursor);
  state.sequence_cursor = Address(legacySequenceParseData);
  state.model_palette_words = legacyShpCopiedWordCount;
  state.model_header_word = legacyShpHeaderWord;
  state.model_texture_width = legacyShpTextureWidth;
  state.model_texture_height = legacyShpTextureHeight;
  return state;
}

void StoreState(const ActorAssetLoadState& state) {
  legacyModelLoadPhase = state.model_phase;
  modelParseResetFlag = state.parse_phase;
  legacySequenceParsePhase = state.sequence_parse_phase;
  legacyEtmLoadPhase = state.effect_texture_phase;
  legacyWeaponFileLoaded = state.weapon_file_loaded ? 1 : 0;
  legacyShpAllocated = state.model_file_allocated ? 1 : 0;
  legacySequenceBufferOwned = state.sequence_file_owned ? 1 : 0;
  legacyWeaponFileBuffer = Pointer(state.weapon_file);
  legacyShpFileBuffer = Pointer(state.model_file);
  legacySequenceFileBuffer = Pointer(state.sequence_file);
  legacyEtmFileBuffer = Pointer<std::uint16_t>(state.effect_texture_file);
  legacyWeaponParseCursor = Pointer(state.weapon_cursor);
  legacyShpParseCursor = Pointer<std::uint8_t>(state.model_cursor);
  legacySequenceParseData = Pointer<std::uint8_t>(state.sequence_cursor);
  legacyShpCopiedWordCount = state.model_palette_words;
  legacyShpHeaderWord = state.model_header_word;
  legacyShpTextureWidth = state.model_texture_width;
  legacyShpTextureHeight = state.model_texture_height;
}

ActorAssetQueueStats LoadStats() {
  return {
      .peak_frame_ticks = assetQueuePeakUsage,
      .last_load_phase = lastLoadPhaseSnapshot,
      .frame_ticks = assetQueueFrameTicks,
      .last_kind = lastAssetRequestType,
  };
}

void StoreStats(const ActorAssetQueueStats& stats) {
  assetQueuePeakUsage = stats.peak_frame_ticks;
  lastLoadPhaseSnapshot = stats.last_load_phase;
  assetQueueFrameTicks = stats.frame_ticks;
  lastAssetRequestType = stats.last_kind;
}

// --- Weapon records ----------------------------------------------------------

WeaponAssetRecord LoadWeaponRecord(const VS::Actors::WeaponModel& in) {
  WeaponAssetRecord out;
  out.bone_count = in.bone_count;
  out.vertex_group_count = in.vertex_group_count;
  out.triangle_count = in.triangle_count;
  out.quad_count = in.quad_count;
  out.other_primitive_count = in.other_primitive_count;
  out.actor_id = in.actor_id;
  out.reserved08_bit4 = in.shown != 0;
  out.wide_texture = in.is_wide_texture != 0;
  out.reserved08_bit6 = in.brightness_override != 0;
  out.double_clut = in.is_double_clut != 0;
  out.material = in.material;
  out.palette_color_count = in.palette_color_count;
  out.attachment_slot = in.attachment_slot;
  out.attachment_bone = in.attachment_bone;
  out.model_id = in.model_id;
  out.texture_slot = in.texture_slot;
  out.palette_slot_offset = in.palette_slot_offset;
  out.reserved11 = in.blend_level;
  out.reserved13 = in.blend_frames;
  out.geometry = Address(in.section_offsets);
  for (std::size_t i = 0; i < out.palettes.size(); ++i) {
    std::copy(std::begin(in.palettes[i]), std::end(in.palettes[i]),
              out.palettes[i].begin());
  }
  std::copy(std::begin(in.working_palette), std::end(in.working_palette),
            out.working_palette.begin());
  std::copy(std::begin(in.upload_palette), std::end(in.upload_palette),
            out.upload_palette.begin());
  return out;
}

void StoreWeaponRecord(const WeaponAssetRecord& in,
                       VS::Actors::WeaponModel& out) {
  out.bone_count = in.bone_count;
  out.vertex_group_count = in.vertex_group_count;
  out.triangle_count = in.triangle_count;
  out.quad_count = in.quad_count;
  out.other_primitive_count = in.other_primitive_count;
  out.actor_id = static_cast<std::uint8_t>(in.actor_id & kNibbleMask);
  out.shown = in.reserved08_bit4 ? 1 : 0;
  out.is_wide_texture = in.wide_texture ? 1 : 0;
  out.brightness_override = in.reserved08_bit6 ? 1 : 0;
  out.is_double_clut = in.double_clut ? 1 : 0;
  out.material = static_cast<std::uint8_t>(in.material & kNibbleMask);
  out.palette_color_count = in.palette_color_count;
  out.attachment_slot = in.attachment_slot;
  out.attachment_bone = in.attachment_bone;
  out.model_id = in.model_id;
  out.texture_slot = in.texture_slot;
  out.palette_slot_offset = in.palette_slot_offset;
  out.blend_level = in.reserved11;
  out.blend_frames = in.reserved13;
  out.section_offsets = Pointer<VS::Actors::WeaponModelData>(in.geometry);
  for (std::size_t i = 0; i < in.palettes.size(); ++i) {
    std::copy(in.palettes[i].begin(), in.palettes[i].end(),
              std::begin(out.palettes[i]));
  }
  std::copy(in.working_palette.begin(), in.working_palette.end(),
            std::begin(out.working_palette));
  std::copy(in.upload_palette.begin(), in.upload_palette.end(),
            std::begin(out.upload_palette));
}

// --- Actor records -----------------------------------------------------------

void LoadInstance(const VS::Actors::ModelInstance& in, ActorAssetRecord& out) {
  out.bone_count = in.bone_count;
  out.vertex_group_count = in.vertex_group_count;
  out.triangle_count = in.triangle_count;
  out.quad_count = in.quad_count;
  out.other_polygon_count = in.other_polygon_count;
  const std::uint8_t flags = ByteOf(in, kInstanceFlagsAt);
  out.is_skipped = (flags & kSkippedBit) != 0;
  out.uv_layout = (flags & kUvLayoutBit) != 0;
  out.texture_slot_count_minus_one = static_cast<std::uint8_t>(
      (flags >> kTextureSlotCountShift) & kTextureSlotCountMask);
  out.is_parsed = (flags & kParsedBit) != 0;
  out.action_animation_state = in.action_animation_state;
  out.is_attached = in.is_attached != 0;
  out.sound_enabled = in.sound_enabled != 0;
  out.weapon_drawn = in.weapon_drawn != 0;
  out.support_layer = in.support_layer;
  out.placement_layer = in.placement_layer;
  out.palette_words = in.copied_word_count;
  out.slot = in.slot;
  out.variant = in.variant;
  out.attach_parent = in.attach_parent;
  out.attach_bone = in.attach_bone;
  out.scale_x = in.scale_x;
  out.scale_y = in.scale_y;
  out.scale_z = in.scale_z;
  out.base_color = WordAt(in, offsetof(VS::Actors::ModelInstance, base_color));
  out.tint = WordAt(in, offsetof(VS::Actors::ModelInstance, tint));
  out.placement =
      WordAt(in, offsetof(VS::Actors::ModelInstance, current_tile_x));
  out.model = Address(in.model);
}

void StoreInstance(const ActorAssetRecord& in, VS::Actors::ModelInstance& out) {
  out.bone_count = in.bone_count;
  out.vertex_group_count = in.vertex_group_count;
  out.triangle_count = in.triangle_count;
  out.quad_count = in.quad_count;
  out.other_polygon_count = in.other_polygon_count;
  std::uint8_t& flags = ByteAt(out, kInstanceFlagsAt);
  std::uint8_t value = flags & kKeptInstanceFlags;
  value |= in.is_skipped ? kSkippedBit : 0;
  value |= in.uv_layout ? kUvLayoutBit : 0;
  value |= static_cast<std::uint8_t>(
      (in.texture_slot_count_minus_one & kTextureSlotCountMask)
      << kTextureSlotCountShift);
  value |= in.is_parsed ? kParsedBit : 0;
  flags = value;
  out.action_animation_state =
      static_cast<std::uint8_t>(in.action_animation_state & kNibbleMask);
  out.is_attached = in.is_attached ? 1 : 0;
  out.sound_enabled = in.sound_enabled ? 1 : 0;
  out.weapon_drawn = in.weapon_drawn ? 1 : 0;
  out.support_layer = static_cast<std::uint8_t>(in.support_layer & kNibbleMask);
  out.placement_layer =
      static_cast<std::uint8_t>(in.placement_layer & kNibbleMask);
  out.copied_word_count = in.palette_words;
  out.slot = in.slot;
  out.variant = in.variant;
  out.attach_parent = in.attach_parent;
  out.attach_bone = in.attach_bone;
  out.scale_x = in.scale_x;
  out.scale_y = in.scale_y;
  out.scale_z = in.scale_z;
  SetWordAt(out, offsetof(VS::Actors::ModelInstance, base_color),
            in.base_color);
  SetWordAt(out, offsetof(VS::Actors::ModelInstance, tint), in.tint);
  SetWordAt(out, offsetof(VS::Actors::ModelInstance, current_tile_x),
            in.placement);
  out.model = Pointer<VS::Actors::ShpModel>(in.model);
}

void LoadActorFields(const VS::Actors::ActorRecord& in, ActorAssetRecord& out) {
  out.holds_texture_slots = in.holds_texture_slots != 0;
  out.is_8bit_texture = in.is_8bit_texture != 0;
  out.advance_requested = in.advance_requested != 0;
  out.custom_tint = in.custom_tint != 0;
  out.ignored_by_collision = in.ignored_by_collision != 0;
  out.sequence_model_id = in.sequence_model_id;
  out.frames_per_step = in.frames_per_step;
  out.idle_ticks = in.idle_ticks;
  out.common_entry_count = in.common_entry_count;
  out.common_animation_count = in.common_animation_count;
  out.battle_animation_count = in.battle_animation_count;
  out.texture_slot = in.texture_slot;
  out.travel_animation = in.travel_animation;
  out.animation_length = in.animation_length;
  out.turn_rate = in.turn_rate;
  out.move_rate = in.move_rate;
  out.owned_sequence = Address(in.owned_sequence);
  out.common_sequence = Address(in.common_sequence);
  out.battle_sequence = Address(in.battle_sequence);
  for (std::size_t i = 0; i < out.shared_sequences.size(); ++i) {
    out.shared_sequences[i] = Address(in.shared_sequences[i]);
  }
  out.occupancy_radius = in.occupancy_radius;
  out.occupancy_height = in.occupancy_height;
  out.reserved648 = in.reserved648;
  out.opaque6dc = in.opaque6dc;
  out.model_id = in.model_id;
  out.clut_a = in.clut_a;
  out.clut_b = in.clut_b;
  out.fade = WordAt(in, offsetof(VS::Actors::ActorRecord, fade_blue));
  out.companion_slot = in.companion_slot;
  out.companion = {.x = in.companion_state.offset_x,
                   .y = in.companion_state.offset_y,
                   .z = in.companion_state.offset_z,
                   .depth_bias = in.companion_state.depth_bias};
  out.scale = in.scale;
  const std::uint32_t rows =
      WordAt(in, offsetof(VS::Actors::ActorRecord, texture_row_offsets));
  std::memcpy(out.texture_row_offsets.data(), &rows, sizeof rows);
  out.model_data_size = in.model_data_size;
}

void StoreActorFields(const ActorAssetRecord& in,
                      VS::Actors::ActorRecord& out) {
  out.holds_texture_slots = in.holds_texture_slots ? 1 : 0;
  out.is_8bit_texture = in.is_8bit_texture ? 1 : 0;
  out.advance_requested = in.advance_requested ? 1 : 0;
  out.custom_tint = in.custom_tint ? 1 : 0;
  out.ignored_by_collision = in.ignored_by_collision ? 1 : 0;
  out.sequence_model_id = in.sequence_model_id;
  out.frames_per_step = in.frames_per_step;
  out.idle_ticks = in.idle_ticks;
  out.common_entry_count = in.common_entry_count;
  out.common_animation_count = in.common_animation_count;
  out.battle_animation_count = in.battle_animation_count;
  out.texture_slot = in.texture_slot;
  out.travel_animation = in.travel_animation;
  out.animation_length = in.animation_length;
  out.turn_rate = in.turn_rate;
  out.move_rate = in.move_rate;
  out.owned_sequence = Pointer(in.owned_sequence);
  out.common_sequence = Pointer(in.common_sequence);
  out.battle_sequence = Pointer(in.battle_sequence);
  for (std::size_t i = 0; i < in.shared_sequences.size(); ++i) {
    out.shared_sequences[i] = Pointer(in.shared_sequences[i]);
  }
  out.occupancy_radius = in.occupancy_radius;
  out.occupancy_height = in.occupancy_height;
  out.reserved648 = in.reserved648;
  out.opaque6dc = in.opaque6dc;
  out.model_id = in.model_id;
  out.clut_a = in.clut_a;
  out.clut_b = in.clut_b;
  SetWordAt(out, offsetof(VS::Actors::ActorRecord, fade_blue), in.fade);
  out.companion_slot = in.companion_slot;
  out.companion_state.offset_x = in.companion.x;
  out.companion_state.offset_y = in.companion.y;
  out.companion_state.offset_z = in.companion.z;
  out.companion_state.depth_bias = in.companion.depth_bias;
  out.scale = in.scale;
  std::uint32_t rows = 0;
  std::memcpy(&rows, in.texture_row_offsets.data(), sizeof rows);
  SetWordAt(out, offsetof(VS::Actors::ActorRecord, texture_row_offsets), rows);
  out.model_data_size = in.model_data_size;
}

void LoadActorTables(const VS::Actors::ActorRecord& in, ActorAssetRecord& out) {
  for (std::size_t i = 0; i < out.texture_scroll_bounds.size(); ++i) {
    const VS::Actors::TextureScrollBoundsRecord& bounds =
        in.texture_scroll_bounds[i];
    out.texture_scroll_bounds[i] = {.x = bounds.x,
                                    .y = bounds.y,
                                    .initial_step_x = bounds.initial_step_x,
                                    .initial_step_y = bounds.initial_step_y,
                                    .interval = bounds.interval};
  }
  for (std::size_t i = 0; i < out.texture_scroll.size(); ++i) {
    const VS::Actors::TextureScrollStateRecord& scroll = in.texture_scroll[i];
    out.texture_scroll[i] = {.x = scroll.x,
                             .y = scroll.y,
                             .step_x = scroll.step_x,
                             .step_y = scroll.step_y,
                             .interval = scroll.interval,
                             .timer = scroll.timer};
  }
  std::ranges::copy(in.battle_sequence_lbas, out.battle_sequence_lbas.begin());
  std::ranges::copy(in.chain_attack_animation_ids,
                    out.chain_attack_animation_ids.begin());
  std::ranges::copy(in.special_sequence_lbas,
                    out.special_sequence_lbas.begin());
  std::ranges::copy(in.special_sequence_sizes,
                    out.special_sequence_sizes.begin());
  std::memcpy(out.palettes.data(), in.palettes, sizeof in.palettes);
  std::ranges::copy(in.base_palette, out.base_palette.begin());
  std::ranges::copy(in.current_palette, out.current_palette.begin());
  std::memcpy(out.joint_slots.data(), in.joint_slots, sizeof in.joint_slots);
}

void StoreActorTables(const ActorAssetRecord& in,
                      VS::Actors::ActorRecord& out) {
  for (std::size_t i = 0; i < in.texture_scroll_bounds.size(); ++i) {
    VS::Actors::TextureScrollBoundsRecord& bounds =
        out.texture_scroll_bounds[i];
    bounds.x = in.texture_scroll_bounds[i].x;
    bounds.y = in.texture_scroll_bounds[i].y;
    bounds.initial_step_x = in.texture_scroll_bounds[i].initial_step_x;
    bounds.initial_step_y = in.texture_scroll_bounds[i].initial_step_y;
    bounds.interval = in.texture_scroll_bounds[i].interval;
  }
  for (std::size_t i = 0; i < in.texture_scroll.size(); ++i) {
    VS::Actors::TextureScrollStateRecord& scroll = out.texture_scroll[i];
    scroll.x = in.texture_scroll[i].x;
    scroll.y = in.texture_scroll[i].y;
    scroll.step_x = in.texture_scroll[i].step_x;
    scroll.step_y = in.texture_scroll[i].step_y;
    scroll.interval = in.texture_scroll[i].interval;
    scroll.timer = in.texture_scroll[i].timer;
  }
  std::ranges::copy(in.battle_sequence_lbas,
                    std::begin(out.battle_sequence_lbas));
  std::ranges::copy(in.chain_attack_animation_ids,
                    std::begin(out.chain_attack_animation_ids));
  std::ranges::copy(in.special_sequence_lbas,
                    std::begin(out.special_sequence_lbas));
  std::ranges::copy(in.special_sequence_sizes,
                    std::begin(out.special_sequence_sizes));
  std::memcpy(out.palettes, in.palettes.data(), sizeof out.palettes);
  std::ranges::copy(in.base_palette, std::begin(out.base_palette));
  std::ranges::copy(in.current_palette, std::begin(out.current_palette));
  std::memcpy(out.joint_slots, in.joint_slots.data(), sizeof out.joint_slots);
}

// --- Files -------------------------------------------------------------------

template <typename T, std::size_t N>
std::optional<std::uint32_t> TableEntry(const std::array<T, N>& table,
                                        std::size_t index) {
  if (index >= N) {
    return std::nullopt;
  }
  return table[index];
}

// The material-model tables, indexed material * 4 + model.
template <typename T, std::size_t Rows, std::size_t Columns>
std::optional<std::uint32_t> FlatTableEntry(
    const std::array<std::array<T, Columns>, Rows>& table, std::size_t index) {
  if (index >= Rows * Columns) {
    return std::nullopt;
  }
  return table[index / Columns][index % Columns];
}

struct LiveFileStream {
  DiscFile* file;
  DiscQueueSlot** request;
};

struct ModelReadOwner {
  DiscQueueSlot* request = nullptr;
  std::uint64_t generation = 0;
};

ModelReadOwner modelReadOwner;

LiveFileStream Stream(AssetFileStream stream) {
  switch (stream) {
    case AssetFileStream::kWeapon:
      return {&legacyWeaponFileLocation, &legacyWeaponDiscRequest};
    case AssetFileStream::kActorModel:
      return {&legacyShpFileLocation, &legacyShpDiscRequest};
    case AssetFileStream::kSequence:
      return {&legacySequenceFileLocation, &legacySequenceDiscRequest};
    case AssetFileStream::kEffectTexture:
    default:
      return {&legacyEtmFileLocation, &legacyEtmDiscRequest};
  }
}

VS::Render::TextureSlot* TextureSlotAt(AssetTextureTable table, int index) {
  if (table == AssetTextureTable::kActor) {
    return InRange(index, kActorTextureSlots)
               ? &VS::Render::legacyActorTextureSlots[index]
               : nullptr;
  }
  return InRange(index, kWeaponTextureSlots)
             ? &VS::Render::legacyWeaponTextureSlots[index]
             : nullptr;
}

int RunLoader(ActorAssetHost& host, ActorAssetLoadState& state,
              ActorAssetRequest& request, ActorAssetKind kind) {
  switch (kind) {
    case ActorAssetKind::kActorModelLoad:
      return UpdateActorModelLoad(host, state, request);
    case ActorAssetKind::kActorModelParse:
      return ParseActorModel(host, state, request);
    case ActorAssetKind::kWeaponModelLoad:
      return UpdateWeaponModelLoad(host, state, request);
    case ActorAssetKind::kWeaponModelParse:
      return ParseWeaponModel(host, state, request);
    case ActorAssetKind::kSequenceLoad:
      return UpdateActorSequenceLoad(host, state, request);
    case ActorAssetKind::kSequenceParse:
      return ParseActorSequence(host, state, request);
    case ActorAssetKind::kEffectTextureLoad:
      return UpdateSequenceEffectTextureLoad(host, state, request);
    default:
      return kAssetPending;
  }
}

class LiveActorAssetHost final : public ActorAssetHost {
 public:
  // The request queue.
  [[nodiscard]] std::uint8_t RequestKind(int slot) const override {
    return InRange(slot, kActorAssetRequestCount)
               ? legacyActorAssetRequests[slot].asset_kind
               : 0;
  }
  void StoreRequest(int slot, const ActorAssetRequest& request) override {
    if (!InRange(slot, kActorAssetRequestCount)) {
      CountRefusal("asset request slot", slot);
      return;
    }
    StoreRequestFields(request, legacyActorAssetRequests[slot]);
  }
  void OnRequestQueued(const ActorAssetRequest& request) override {
    const std::uint8_t phase = active_state_ != nullptr
                                   ? active_state_->model_phase
                                   : legacyModelLoadPhase;
    const std::uint8_t parse = active_state_ != nullptr
                                   ? active_state_->parse_phase
                                   : modelParseResetFlag;
    VS::Core::Debugf(
        "actors", "queue kind=%u actor=%u model=%u phase=%u parse=%u",
        static_cast<unsigned>(request.kind),
        static_cast<unsigned>(request.target_slot),
        static_cast<unsigned>(request.model_id), static_cast<unsigned>(phase),
        static_cast<unsigned>(parse));
  }
  void MoveRequest(int from, int to) override {
    legacyActorAssetRequests[to] = legacyActorAssetRequests[from];
  }
  void ClearRequestKind(int slot) override {
    legacyActorAssetRequests[slot].asset_kind = 0;
  }
  int ProcessHeadRequest(ActorAssetKind kind) override {
    LegacyActorAssetRequest& head = legacyActorAssetRequests[0];
    ActorAssetRequest request = LoadRequest(head);
    if (kind == ActorAssetKind::kGimmick) {
      const auto result = VS::Field::CreateLiveGimmickInstance(request);
      StoreRequestFields(request, head);
      return result;
    }
    ActorAssetLoadState state = LoadState();
    active_state_ = &state;
    const int result = RunLoader(*this, state, request, kind);
    active_state_ = nullptr;
    StoreState(state);
    StoreRequestFields(request, head);
    return result;
  }
  std::int32_t FrameCounter() override { return VS::Legacy::VSync(1); }
  [[nodiscard]] std::uint8_t ModelLoadPhase() const override {
    return legacyModelLoadPhase;
  }
  void OnHeadRequestDone(int result) override {
    const LegacyActorAssetRequest& head = legacyActorAssetRequests[0];
    // The original indexed the records with any slot below 17.
    const void* record = ActorAt(head.target_slot);
    VS::Core::Debugf("actors",
                     "loaded kind=%u actor=%u model=%d result=%d record=%p",
                     static_cast<unsigned>(head.asset_kind),
                     static_cast<unsigned>(head.target_slot),
                     static_cast<int>(head.model_id), result, record);
  }

  // The game heap.
  AssetAddress AllocateHigh(std::size_t bytes) override {
    return Address(Core::Memory::AllocateLiveFromHighEnd(bytes));
  }
  AssetAddress AllocateLow(std::size_t bytes) override {
    return Address(Core::Memory::AllocateLiveFromLowEnd(bytes));
  }
  void FreeHigh(AssetAddress block) override {
    Core::Memory::FreeLiveFromHighEnd(Pointer(block));
  }
  void FreeLow(AssetAddress block) override {
    Core::Memory::FreeLiveFromLowEnd(Pointer(block));
  }
  std::span<std::byte> Bytes(AssetAddress address,
                             std::size_t wanted) override {
    return LiveBytes(address, wanted);
  }

  // Disc files.
  std::optional<std::uint32_t> FileTableEntry(AssetFileTable table,
                                              std::size_t index) override {
    return LiveFileTableEntry(table, index);
  }
  bool BeginFileRead(AssetFileStream stream, DiscFileExtent file,
                     AssetAddress destination) override {
    const LiveFileStream live = Stream(stream);
    live.file->lba = file.sector;
    live.file->size = file.bytes;
    *live.request = AllocateLiveDiscRequest(live.file);
    if (stream == AssetFileStream::kActorModel) {
      modelReadOwner = {*live.request,
                        LiveDiscRequestGeneration(*live.request)};
    }
    if (*live.request == nullptr) {
      return false;
    }
    EnqueueLiveDiscRequest(*live.request, Pointer(destination));
    return true;
  }
  void ReadRefusedFile(DiscFileExtent file, AssetAddress destination) override {
    DiscFile location{};
    location.lba = file.sector;
    location.size = file.bytes;
    if (ReadLiveRefusedDiscFile(&location, Pointer(destination)) == 0) {
      CountRefusal("unread asset file sector", file.sector);
    }
  }
  bool FileReadFinished(AssetFileStream stream) override {
    const DiscQueueSlot* request = *Stream(stream).request;
    return request == nullptr || request->state == DiscRequestState::kLoaded;
  }
  void ReleaseFileRead(AssetFileStream stream) override {
    ReleaseLiveDiscRequest(*Stream(stream).request);
    if (stream == AssetFileStream::kActorModel) modelReadOwner = {};
  }

  // VRAM.
  [[nodiscard]] AssetTextureSlot TextureSlot(AssetTextureTable table,
                                             int index) const override {
    const VS::Render::TextureSlot* slot = TextureSlotAt(table, index);
    if (slot == nullptr) {
      CountRefusal("texture slot read", index);
      return {};
    }
    return {.texture_page = slot->texture_page,
            .palette = slot->palette,
            .alternate_palette = slot->alternate_palette,
            .references = slot->reference_count};
  }
  void SetTextureSlot(AssetTextureTable table, int index,
                      const AssetTextureSlot& value) override {
    VS::Render::TextureSlot* slot = TextureSlotAt(table, index);
    if (slot == nullptr) {
      CountRefusal("texture slot write", index);
      return;
    }
    slot->texture_page = value.texture_page;
    slot->palette = value.palette;
    slot->alternate_palette = value.alternate_palette;
    slot->reference_count = value.references;
  }
  [[nodiscard]] std::span<const std::uint8_t> TextureSlotOrigins()
      const override {
    return VS::Actors::textureSlotOrigins;
  }
  [[nodiscard]] std::span<const std::uint8_t> TextureSlotRowOffsets()
      const override {
    return VS::Actors::textureSlotRowOffsets;
  }
  int ReserveTexturePages(std::uint32_t first, std::uint32_t count) override {
    return Render::ReserveLiveTexturePageRange(first, count, kLoaderPageOwner);
  }
  int RetireTexturePages(std::uint32_t first, std::uint32_t count) override {
    return Render::RetireLiveTexturePageRange(first, count, kLoaderPageOwner);
  }
  void UploadImage(const VramRect& rect,
                   std::span<const std::byte> pixels) override {
    RECT area{.x = rect.x, .y = rect.y, .w = rect.w, .h = rect.h};
    // LoadImage only reads the pixels.
    LoadImage(&area, reinterpret_cast<std::uint32_t*>(
                         const_cast<std::byte*>(pixels.data())));
  }
  void UploadClut(std::span<const std::uint16_t> colors, std::uint32_t row,
                  std::uint32_t first, std::uint32_t count) override {
    if (count > colors.size()) {
      CountRefusal("CLUT colour count", count);
      return;
    }
    Render::LoadLiveClutColors(colors.data(), row, first, count);
  }

  // Weapon records.
  [[nodiscard]] WeaponAssetSummary WeaponSummary(int slot) const override {
    const VS::Actors::WeaponModel* model = WeaponAt(slot);
    if (model == nullptr) {
      return {};
    }
    return {.present = true,
            .actor_id = model->actor_id,
            .model_id = model->model_id,
            .bone_count = model->bone_count};
  }
  bool BindWeaponRecord(int slot, AssetAddress destination) override {
    auto* model = BindRecord<VS::Actors::WeaponModel>(
        slot, kWeaponModelSlots, destination, "weapon record destination");
    if (model == nullptr) {
      return false;
    }
    VS::Actors::weaponModels[slot] = model;
    model->texture_slot = kNoAssetTextureSlot;
    return true;
  }
  [[nodiscard]] WeaponAssetRecord LoadWeapon(int slot) const override {
    const VS::Actors::WeaponModel* model = WeaponAt(slot);
    if (model == nullptr) {
      CountRefusal("weapon record read", slot);
      return {};
    }
    return LoadWeaponRecord(*model);
  }
  void StoreWeapon(int slot, const WeaponAssetRecord& record) override {
    VS::Actors::WeaponModel* model = WeaponAt(slot);
    if (model == nullptr) {
      CountRefusal("weapon record write", slot);
      return;
    }
    StoreWeaponRecord(record, *model);
  }
  void CopyWeaponRecord(int target, int source) override {
    VS::Actors::WeaponModel* to = WeaponAt(target);
    const VS::Actors::WeaponModel* from = WeaponAt(source);
    if (to == nullptr || from == nullptr) {
      CountRefusal("weapon record copy", source);
      return;
    }
    std::memmove(to, from, sizeof *to);
  }
  void WriteWeaponBones(int slot, std::span<const std::byte> bones) override {
    VS::Actors::WeaponModel* model = WeaponAt(slot);
    if (model == nullptr) {
      CountRefusal("weapon bones", slot);
      return;
    }
    // The record holds four bones; the original copied boneCount * 8 bytes.
    const std::size_t bytes = std::min(bones.size(), sizeof model->bones);
    if (bytes < bones.size()) {
      CountRefusal("weapon bone bytes",
                   static_cast<std::int64_t>(bones.size()));
    }
    std::memcpy(model->bones, bones.data(), bytes);
  }
  void ClearWeaponSlot(int slot) override {
    if (InRange(slot, kWeaponModelSlots)) {
      VS::Actors::weaponModels[slot] = nullptr;
    }
  }
  [[nodiscard]] AssetAddress WeaponGeometryBuffer(int slot) const override {
    return slot == 0 || slot == 1 ? Address(legacyWeaponModelBuffers[slot])
                                  : AssetAddress::kNone;
  }
  [[nodiscard]] std::span<const std::uint8_t> WeaponMaterialPalettes()
      const override {
    return VS::Items::weaponMaterialPaletteIndices;
  }
  int FindActorBoneTag(int actor, int tag) override {
    return VS::Actors::FindLiveActorBoneTag(actor, tag, 0);
  }

  // Actor records.
  [[nodiscard]] ActorAssetSummary ActorSummary(int slot) const override {
    const VS::Actors::ActorRecord* actor = ActorAt(slot);
    if (actor == nullptr) {
      return {};
    }
    return {.present = true,
            .model_id = actor->model_id,
            .sequence_model_id = actor->sequence_model_id,
            .common_sequence = Address(actor->common_sequence)};
  }
  bool BindActorRecord(int slot, AssetAddress destination) override {
    auto* actor = BindRecord<VS::Actors::ActorRecord>(
        slot, kActorRecordSlots, destination, "actor record destination");
    if (actor == nullptr) {
      return false;
    }
    VS::Actors::actorRecords[slot] = actor;
    actor->texture_slot = kNoAssetTextureSlot;
    actor->instance.attach_parent = kNoAssetTextureSlot;
    return true;
  }
  [[nodiscard]] ActorAssetRecord LoadActor(int slot) const override {
    const VS::Actors::ActorRecord* actor = ActorAt(slot);
    if (actor == nullptr) {
      CountRefusal("actor record read", slot);
      return {};
    }
    ActorAssetRecord record;
    LoadInstance(actor->instance, record);
    LoadActorFields(*actor, record);
    LoadActorTables(*actor, record);
    return record;
  }
  void StoreActor(int slot, const ActorAssetRecord& record) override {
    VS::Actors::ActorRecord* actor = ActorAt(slot);
    if (actor == nullptr) {
      CountRefusal("actor record write", slot);
      return;
    }
    StoreInstance(record, actor->instance);
    StoreActorFields(record, *actor);
    StoreActorTables(record, *actor);
  }
  void CopyActorRecord(int target, int source) override {
    VS::Actors::ActorRecord* to = ActorAt(target);
    const VS::Actors::ActorRecord* from = ActorAt(source);
    if (to == nullptr || from == nullptr) {
      CountRefusal("actor record copy", source);
      return;
    }
    std::memmove(to, from, sizeof *to);
  }
  void WriteActorTextureLayout(int slot,
                               std::span<const std::byte> layout) override {
    VS::Actors::ActorRecord* actor = ActorAt(slot);
    if (actor == nullptr || layout.size() > kActorTextureLayoutBytes) {
      CountRefusal("actor texture layout", slot);
      return;
    }
    // The layout fills texture_overlays and the fields after it.
    std::memcpy(reinterpret_cast<std::byte*>(actor) +
                    offsetof(VS::Actors::ActorRecord, texture_overlays),
                layout.data(), layout.size());
  }
  void ClearActorSlot(int slot) override {
    if (InRange(slot, kActorRecordSlots)) {
      VS::Actors::actorRecords[slot] = nullptr;
    }
  }
  [[nodiscard]] std::int16_t RoomActorScale() const override {
    return static_cast<std::int16_t>(VS::Actors::roomActorScale.vx);
  }
  void ResetActorMotion(int slot) override {
    VS::Actors::ResetLiveActorMotionState(slot, 0);
  }
  void ClearActorPaletteFade(int slot) override {
    VS::Actors::SetLiveActorPaletteFade(slot, 0);
  }
  void PlaceActorInRoom(int slot, std::uint32_t placement, int mode) override {
    const VS::Actors::SpawnPlacement value =
        VS::Actors::UnpackPlacement(placement);
    VS::Actors::PlaceLiveActorInRoom(slot, &value, 0, mode);
  }
  void AttachActorToParentBone(int slot, int parent, int bone) override {
    VS::Actors::AttachLiveActorToParentBone(slot, parent, bone, 0);
  }
  void StopModelSounds(AssetAddress sound_bank) override {
    VS::Audio::LiveStopBankSfx(kModelSoundChannels, Pointer<int>(sound_bank),
                               0);
  }

  // Shared sequence banks.
  [[nodiscard]] SharedSequenceBank LoadSequenceBank(int bank) const override {
    if (!InRange(bank, kSharedSequenceBanks)) {
      CountRefusal("sequence bank read", bank);
      return {};
    }
    SharedSequenceBank value;
    value.data = Address(VS::Actors::sharedSequenceData[bank]);
    for (std::size_t i = 0; i < kSharedSequenceEntries; ++i) {
      value.animation_ids[i] = VS::Actors::sharedSequenceAnimationIds[bank][i];
      value.entry_params[i] = VS::Actors::sharedSequenceEntryParams[bank][i];
      value.animation_data[i] =
          Address(VS::Actors::sharedSequenceAnimationData[bank][i]);
    }
    return value;
  }
  void StoreSequenceBank(int bank, const SharedSequenceBank& value) override {
    if (!InRange(bank, kSharedSequenceBanks)) {
      CountRefusal("sequence bank write", bank);
      return;
    }
    VS::Actors::sharedSequenceData[bank] = Pointer(value.data);
    for (std::size_t i = 0; i < kSharedSequenceEntries; ++i) {
      VS::Actors::sharedSequenceAnimationIds[bank][i] = value.animation_ids[i];
      VS::Actors::sharedSequenceEntryParams[bank][i] = value.entry_params[i];
      VS::Actors::sharedSequenceAnimationData[bank][i] =
          Pointer<char>(value.animation_data[i]);
    }
  }
  [[nodiscard]] std::uint8_t BankTexturePage(int bank) const override {
    if (!InRange(bank, kSharedSequenceBanks)) {
      CountRefusal("sequence bank page read", bank);
      return 0;
    }
    return VS::Actors::sharedSequenceBankTexturePages[bank];
  }
  void SetBankTexturePage(int bank, std::uint8_t page) override {
    if (!InRange(bank, kSharedSequenceBanks)) {
      CountRefusal("sequence bank page write", bank);
      return;
    }
    VS::Actors::sharedSequenceBankTexturePages[bank] = page;
  }
  [[nodiscard]] std::span<const std::uint8_t> SharedSequenceIdsByKind()
      const override {
    return VS::Actors::sharedSequenceIdsByKind;
  }
  [[nodiscard]] AssetAddress PrimarySequenceBuffer() const override {
    return Address(VS::Actors::primarySequenceBuffer);
  }
  [[nodiscard]] AssetAddress SequenceWorkBuffer() const override {
    return Address(VS::Actors::sequenceWorkBuffer);
  }
  [[nodiscard]] std::optional<std::size_t> SequenceBankSize(
      AssetAddress bank) const override {
    const std::span<const std::uint8_t> bytes =
        Actors::SequenceBankBytes(Pointer(bank));
    if (bytes.empty()) {
      return std::nullopt;
    }
    return bytes.size();
  }
  void ResetActorTextureFrames(int bank) override {
    VS::Actors::ResetLiveActorTextureFrames(bank);
  }

  // Native bookkeeping.
  void RecordModelGeometry(AssetAddress model, std::size_t bytes) override {
    Render::RecordLiveModelExtent(Pointer(model), bytes);
  }
  void RecordModelCopy(AssetAddress destination, AssetAddress source,
                       std::size_t bytes) override {
    Render::RecordLiveModelCopy(Pointer(destination), Pointer(source), bytes);
  }
  void ForgetModelGeometry(AssetAddress model) override {
    Render::ForgetLiveModelExtent(Pointer(model));
  }
  void RecordSequenceBank(AssetAddress bank, std::size_t bytes) override {
    Actors::RegisterSequenceBank(Pointer(bank), bytes);
  }
  void RecordEmbeddedSequenceBank(AssetAddress file) override {
    Actors::RegisterEmbeddedSequenceBank(Pointer(file));
  }
  void RecordSequenceCopy(AssetAddress destination, AssetAddress source,
                          std::size_t bytes) override {
    Actors::RegisterSequenceBankCopy(Pointer(destination), Pointer(source),
                                     bytes);
  }
  void Refuse(const char* what, std::int64_t value) override {
    CountRefusal(what, value);
  }

  // The game heap block holding `address` to its end; `wanted` bytes of
  // memory outside the heap, which the binding cannot bound.
  static std::span<std::byte> LiveBytes(AssetAddress address,
                                        std::size_t wanted) {
    const auto value = static_cast<std::uintptr_t>(address);
    if (value < kLowestAddress) {
      return {};
    }
    auto* const at = Pointer<std::byte>(address);
    const auto payload = Core::Memory::LivePayloadContaining(at);
    if (payload.has_value()) {
      const auto end =
          reinterpret_cast<std::uintptr_t>(payload->data()) + payload->size();
      return {at, static_cast<std::size_t>(end - value)};
    }
    if (payload.error() == Core::Memory::HeapError::kOutsideArena &&
        wanted != 0) {
      return {at, wanted};
    }
    return {};
  }

 private:
  static std::optional<std::uint32_t> LiveFileTableEntry(AssetFileTable table,
                                                         std::size_t index) {
    switch (table) {
      case AssetFileTable::kShpOffsets:
        return TableEntry(legacyShpDiscOffsets, index);
      case AssetFileTable::kShpSectors:
        return TableEntry(legacyShpSectorCounts, index);
      case AssetFileTable::kSequenceOffsets:
        return TableEntry(legacySequenceDiscOffsets, index);
      case AssetFileTable::kSequenceSectors:
        return TableEntry(legacySequenceSectorCounts, index);
      case AssetFileTable::kEtmOffsets:
        return TableEntry(legacyEtmDiscOffsets, index);
      case AssetFileTable::kEtmSectors:
        return TableEntry(legacyEtmSectorCounts, index);
      case AssetFileTable::kWepOffsets:
        return TableEntry(legacyWepDiscOffsets, index);
      case AssetFileTable::kWepSectors:
        return TableEntry(legacyWepSectorCounts, index);
      case AssetFileTable::kMaterialSequenceOffsets:
        return TableEntry(legacyMaterialSequenceDiscOffsets, index);
      case AssetFileTable::kMaterialSequenceSectors:
        return TableEntry(legacyMaterialSequenceSectorCounts, index);
      case AssetFileTable::kMaterialModelSequenceOffsets:
        return FlatTableEntry(legacyMaterialModelSequenceDiscOffsets, index);
      case AssetFileTable::kMaterialModelSequenceSectors:
        return FlatTableEntry(legacyMaterialModelSequenceSectorCounts, index);
      default:
        return std::nullopt;
    }
  }

  // A cleared record at `destination` for an empty slot. The loaders bind
  // only empty slots, so a refused destination leaves the slot empty, as the
  // original's null destination did.
  template <typename Record>
  Record* BindRecord(int slot, int slots, AssetAddress destination,
                     const char* what) {
    if (!InRange(slot, slots)) {
      CountRefusal(what, slot);
      return nullptr;
    }
    const std::span<std::byte> bytes = LiveBytes(destination, sizeof(Record));
    if (bytes.size() < sizeof(Record)) {
      CountRefusal(what, static_cast<std::int64_t>(destination));
      return nullptr;
    }
    std::memset(bytes.data(), 0, sizeof(Record));
    return reinterpret_cast<Record*>(bytes.data());
  }

  const ActorAssetLoadState* active_state_ = nullptr;
};

bool ContainsAddress(std::span<const std::byte> bytes, AssetAddress address) {
  const auto begin = reinterpret_cast<std::uintptr_t>(bytes.data());
  const auto value = static_cast<std::uintptr_t>(address);
  return !bytes.empty() && value >= begin && value - begin < bytes.size();
}

bool ValidBorrowAllocation(std::span<const std::byte> bytes) {
  if (bytes.empty()) return true;
  const auto capacity = Core::Memory::LivePayloadCapacity(bytes.data());
  if (capacity.has_value() && *capacity >= bytes.size()) return true;
  CountRefusal("asset borrow outside its heap allocation",
               static_cast<std::int64_t>(bytes.size()));
  return false;
}

bool CanCancelBorrow(const ActorAssetRequest& request) {
  switch (static_cast<ActorAssetKind>(request.kind)) {
    case ActorAssetKind::kActorModelLoad:
    case ActorAssetKind::kActorModelParse:
    case ActorAssetKind::kWeaponModelParse:
      return true;
    case ActorAssetKind::kSequenceParse:
      if (request.sub_kind == 0) return true;
      break;
    default:
      break;
  }
  CountRefusal("unsupported asset borrow cancellation", request.kind);
  return false;
}

bool ClearBorrowCursors(ActorAssetLoadState& state,
                        std::span<const std::byte> bytes, bool owns_head) {
  if (bytes.empty()) return true;
  const auto contains = [bytes](AssetAddress cursor) {
    return ContainsAddress(bytes, cursor) ||
           cursor == Address(bytes.data() + bytes.size());
  };
  const bool model = contains(state.model_cursor);
  const bool weapon = contains(state.weapon_cursor);
  const bool sequence = contains(state.sequence_cursor);
  if (!owns_head && (((model || weapon) && state.parse_phase != 0) ||
                     (sequence && state.sequence_parse_phase != 0))) {
    CountRefusal("active foreign parser borrows the cancelled allocation", 0);
    return false;
  }
  if (model) state.model_cursor = AssetAddress::kNone;
  if (weapon) state.weapon_cursor = AssetAddress::kNone;
  if (sequence) state.sequence_cursor = AssetAddress::kNone;
  return true;
}

bool CancelModelFile(ActorAssetLoadState& state) {
  if (!state.model_file_allocated) return true;
  auto* file = Pointer<std::byte>(state.model_file);
  const auto capacity = Core::Memory::LivePayloadCapacity(file);
  if (!capacity.has_value()) {
    CountRefusal("cancelled actor file outside its heap allocation",
                 static_cast<std::int64_t>(state.model_file));
    return false;
  }
  if (!ClearBorrowCursors(state, {file, *capacity}, true)) return false;
  if (!CancelLiveDiscWrites({file, *capacity})) return false;
  if (modelReadOwner.generation != 0 &&
      LiveDiscRequestGeneration(modelReadOwner.request) ==
          modelReadOwner.generation) {
    ReleaseLiveDiscRequest(modelReadOwner.request);
  }
  modelReadOwner = {};
  Core::Memory::FreeLiveFromHighEnd(file);
  state.model_file_allocated = false;
  return true;
}

bool CancelBorrowHead(ActorAssetKind kind, ActorAssetLoadState& state) {
  switch (kind) {
    case ActorAssetKind::kActorModelLoad:
      if (!CancelModelFile(state)) return false;
      [[fallthrough]];
    case ActorAssetKind::kActorModelParse:
      state.model_phase = 0;
      state.parse_phase = 0;
      state.model_file = AssetAddress::kNone;
      state.model_cursor = AssetAddress::kNone;
      state.model_palette_words = 0;
      state.model_header_word = 0;
      state.model_texture_width = 0;
      state.model_texture_height = 0;
      break;
    case ActorAssetKind::kWeaponModelParse:
      state.parse_phase = 0;
      state.weapon_cursor = AssetAddress::kNone;
      break;
    case ActorAssetKind::kSequenceParse:
      state.sequence_parse_phase = 0;
      state.sequence_cursor = AssetAddress::kNone;
      break;
    default:
      CountRefusal("unsupported asset head cancellation",
                   static_cast<std::int64_t>(kind));
      return false;
  }
  return true;
}

void RemoveBorrowRequests(
    const std::array<bool, kActorAssetRequestCount>& cancelled) {
  // Match the normal queue's immobile last slot rather than promoting it
  // into the first fifteen slots while cancelling unrelated requests.
  constexpr std::size_t kImmobile = kActorAssetRequestCount - 1;
  std::size_t destination = 0;
  for (std::size_t source = 0; source < kImmobile; ++source) {
    if (!cancelled[source]) {
      legacyActorAssetRequests[destination++] =
          legacyActorAssetRequests[source];
    }
  }
  while (destination < kImmobile) {
    legacyActorAssetRequests[destination++] = {};
  }
  if (cancelled[kImmobile]) legacyActorAssetRequests[kImmobile] = {};
}
}  // namespace

int FindLiveFreeActorAssetRequest() {
  LiveActorAssetHost host;
  return FindFreeActorAssetRequest(host);
}

int QueueLiveActorAssetRequest(const LegacyActorAssetRequest* request) {
  if (request == nullptr) {
    CountRefusal("null asset request", 0);
    return -1;
  }
  LiveActorAssetHost host;
  return QueueActorAssetRequest(host, LoadRequest(*request));
}

int QueueLiveActorAsset(const ActorAssetRequest& request) {
  LiveActorAssetHost host;
  return QueueActorAssetRequest(host, request);
}

int UpdateLiveActorAssetQueue() {
  LiveActorAssetHost host;
  ActorAssetQueueStats stats = LoadStats();
  const int result = UpdateActorAssetQueue(host, stats);
  StoreStats(stats);
  return result;
}

bool CancelLiveActorAssetBorrows(std::span<std::byte> source,
                                 std::span<std::byte> destination) {
  if (!ValidBorrowAllocation(source) || !ValidBorrowAllocation(destination)) {
    return false;
  }
  std::array<bool, kActorAssetRequestCount> cancelled{};
  bool any = false;
  for (std::size_t index = 0; index < cancelled.size(); ++index) {
    const auto request = LoadRequest(legacyActorAssetRequests[index]);
    if (request.kind == 0 ||
        (!ContainsAddress(source, request.data) &&
         !ContainsAddress(destination, request.destination))) {
      continue;
    }
    if (!CanCancelBorrow(request)) return false;
    cancelled[index] = true;
    any = true;
  }
  auto state = LoadState();
  if (!ClearBorrowCursors(state, source, cancelled.front()) ||
      !ClearBorrowCursors(state, destination, cancelled.front())) {
    return false;
  }
  if (cancelled.front() &&
      !CancelBorrowHead(
          static_cast<ActorAssetKind>(legacyActorAssetRequests[0].asset_kind),
          state)) {
    return false;
  }
  StoreState(state);
  if (any) RemoveBorrowRequests(cancelled);
  return true;
}

void OffsetLiveModelTextureCoordinates(void* polygons, int count, int colored,
                                       int u_offset, int v_offset) {
  if (count <= 0) {
    return;
  }
  const std::span<std::byte> bytes = LiveActorAssetHost::LiveBytes(
      Address(polygons),
      static_cast<std::size_t>(count) * kLargestPolygonBytes);
  if (!OffsetModelTextureCoordinates(bytes, count, colored != 0, u_offset,
                                     v_offset)) {
    CountRefusal("model polygons", count);
  }
}

void* CopyLiveModelGeometry(int* destination, void* source, int bytes) {
  LiveActorAssetHost host;
  return Pointer(
      CopyModelGeometry(host, Address(destination), Address(source), bytes));
}

int ReleaseLiveWeaponModel(int slot) {
  LiveActorAssetHost host;
  return ReleaseWeaponModel(host, slot);
}

void CopyLiveWeaponSequence(SequenceBuffer* destination,
                            SequenceBuffer* source) {
  LiveActorAssetHost host;
  CopyWeaponSequence(host, Address(destination), Address(source));
}

int ReleaseLiveActorSequence(int target, int kind) {
  LiveActorAssetHost host;
  return ReleaseActorSequence(host, target, kind);
}

int ReleaseLiveActorModel(int slot) {
  LiveActorAssetHost host;
  return ReleaseActorModel(host, slot);
}

int ReleaseLiveSequenceEffectTexture(int bank) {
  LiveActorAssetHost host;
  return ReleaseSequenceEffectTexture(host, bank);
}

std::uint64_t LiveActorAssetRefusals() noexcept { return refusals; }

}  // namespace VS::Content
