// Hand-maintained (tools/port/owned.csv).
// Live binding for the battle asset queue and loaders (ActorAssets.hpp): the
// legacy request queue, the loaders' globals (ModelLoading.h and
// ActorAssetsLiveState.h), the actor and weapon records, the shared sequence
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

#include "Actors/ActorStateLive.hpp"
#include "Actors/AppearanceLive.hpp"
#include "Actors/SequencePlaybackLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Actors/TextureAnimationLive.hpp"
#include "Audio/SfxLive.hpp"
#include "Content/ActorAssets.hpp"
#include "Content/CdDrive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Core/Diagnostics.h"
#include "Core/Memory/HeapLive.hpp"
#include "Field/GimmickResourcesLive.hpp"
#include "Render/ModelGeometryLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/TexturePagesLive.hpp"

extern "C" {
#include "ActorAssets.h"
#include "ActorAssetsLiveState.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/AppearanceLiveState.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Actors/SkeletonLiveState.h"
#include "Actors/TextureAnimation.h"
#include "Actors/Textures.h"
#include "Actors/WeaponModel.h"
#include "DiscQueue.h"
#include "Field/Actors.h"
#include "Field/Gimmicks.h"
#include "Items/Materials.h"
#include "Legacy/Psx.h"
#include "LegacyFiles.h"
#include "ModelLoading.h"
#include "Render/Textures.h"

// decomp: D_800E8F29 (D_800E8F29@BATTLE/BATTLE.PRG)
unsigned char Content_modelParseResetFlag;
// decomp: D_800E8FC4 (D_800E8FC4@BATTLE/BATTLE.PRG)
int Content_assetQueuePeakUsage;
// decomp: D_800F457C (D_800F457C@BATTLE/BATTLE.PRG)
int Content_lastLoadPhaseSnapshot;
// decomp: D_800F4580 (D_800F4580@BATTLE/BATTLE.PRG)
int Content_assetQueueFrameTicks;
// decomp: D_800F45D8 (D_800F45D8@BATTLE/BATTLE.PRG)
int Content_lastAssetRequestType;
}

namespace VS::Content {
namespace {
static_assert(std::size(Content_legacyActorAssetRequests) ==
              kActorAssetRequestCount);
static_assert(std::size(Actors_actorRecords) == kActorRecordSlots);
static_assert(std::size(Actors_weaponModels) == kWeaponModelSlots);
static_assert(std::size(Render_legacyActorTextureSlots) == kActorTextureSlots);
static_assert(std::size(Render_legacyWeaponTextureSlots) ==
              kWeaponTextureSlots);
static_assert(std::size(Actors_sharedSequenceData) == kSharedSequenceBanks);
static_assert(std::size(Actors_sharedSequenceAnimationIds) ==
              kSharedSequenceBanks);
static_assert(std::extent_v<decltype(Actors_sharedSequenceAnimationIds), 1> ==
              kSharedSequenceEntries);
static_assert(std::extent_v<decltype(Actors_sharedSequenceEntryParams), 1> ==
              kSharedSequenceEntries);
static_assert(std::extent_v<decltype(Actors_sharedSequenceAnimationData), 1> ==
              kSharedSequenceEntries);

static_assert(offsetof(Content_ActorAssetRequest, assetKind) == 0);
static_assert(offsetof(Content_ActorAssetRequest, targetSlot) == 1);
static_assert(offsetof(Content_ActorAssetRequest, modelId) == 2);
static_assert(sizeof(Field_ActorPlacement) == sizeof(std::uint32_t));
static_assert(offsetof(Content_ActorAssetRequest, subKind) ==
              offsetof(Content_ActorAssetRequest, placement) +
                  sizeof(Field_ActorPlacement));
static_assert(offsetof(Content_ActorAssetRequest, textureSlot) ==
              offsetof(Content_ActorAssetRequest, subKind) + 1);
static_assert(offsetof(Content_ActorAssetRequest, material) ==
              offsetof(Content_ActorAssetRequest, textureSlot) + 1);
static_assert(offsetof(Content_ActorAssetRequest, variant) ==
              offsetof(Content_ActorAssetRequest, material) + 1);

// The instance's flag byte (isSkipped .. isParsed) follows the counts; the
// Live binding is built without /J, so its char bit-fields are read raw.
constexpr std::size_t kInstanceFlagsAt = 8;
static_assert(offsetof(Actors_ModelInstance, otherPolygonCount) +
                  sizeof(u_short) ==
              kInstanceFlagsAt);
// The bit-field bytes run from the flag byte to reservedD.
constexpr std::size_t kInstanceBitFieldBytes = 5;
static_assert(offsetof(Actors_ModelInstance, reservedD) ==
              kInstanceFlagsAt + kInstanceBitFieldBytes);
static_assert(offsetof(Actors_ModelInstance, copiedWordCount) ==
              offsetof(Actors_ModelInstance, reservedD) + 1);
// The placement word runs from currentTileX to reserved5F.
static_assert(offsetof(Actors_ModelInstance, reserved5F) + 1 ==
              offsetof(Actors_ModelInstance, currentTileX) +
                  sizeof(std::uint32_t));
static_assert(sizeof(CVECTOR) == sizeof(std::uint32_t));
static_assert(sizeof(Render_ColorCode) == sizeof(std::uint32_t));
static_assert(offsetof(Actors_ActorRecord, reserved6F3) + 1 ==
              offsetof(Actors_ActorRecord, fadeBlue) + sizeof(std::uint32_t));
// textureRowOffsets shares four bytes with momentumDistance.
static_assert(offsetof(Actors_ActorRecord, momentumDirection) ==
              offsetof(Actors_ActorRecord, textureRowOffsets) +
                  kActorTextureRowOffsets);
static_assert(sizeof(Actors_ActorRecord::palettes) ==
              sizeof(ActorAssetRecord::palettes));
static_assert(sizeof(Actors_ActorRecord::basePalette) ==
              sizeof(ActorAssetRecord::base_palette));
static_assert(sizeof(Actors_ActorRecord::currentPalette) ==
              sizeof(ActorAssetRecord::current_palette));
static_assert(sizeof(Actors_ActorRecord::jointSlots) ==
              sizeof(ActorAssetRecord::joint_slots));
static_assert(
    std::extent_v<decltype(Actors_ActorRecord::battleSequenceLbas)> ==
    std::tuple_size_v<decltype(ActorAssetRecord::battle_sequence_lbas)>);
static_assert(
    std::extent_v<decltype(Actors_ActorRecord::chainAttackAnimationIds)> ==
    std::tuple_size_v<decltype(ActorAssetRecord::chain_attack_animation_ids)>);
static_assert(
    std::extent_v<decltype(Actors_ActorRecord::specialSequenceLbas)> ==
    std::tuple_size_v<decltype(ActorAssetRecord::special_sequence_lbas)>);
static_assert(
    std::extent_v<decltype(Actors_ActorRecord::specialSequenceSizes)> ==
    std::tuple_size_v<decltype(ActorAssetRecord::special_sequence_sizes)>);
static_assert(std::extent_v<decltype(Actors_ActorRecord::sharedSequences)> ==
              std::tuple_size_v<decltype(ActorAssetRecord::shared_sequences)>);
static_assert(
    std::extent_v<decltype(Actors_ActorRecord::textureScrollBounds)> ==
    std::tuple_size_v<decltype(ActorAssetRecord::texture_scroll_bounds)>);
static_assert(std::extent_v<decltype(Actors_ActorRecord::textureScroll)> ==
              std::tuple_size_v<decltype(ActorAssetRecord::texture_scroll)>);
// The SHP texture layout fills textureOverlays and the fields after it.
static_assert(offsetof(Actors_ActorRecord, textureOverlays) +
                  kActorTextureLayoutBytes <=
              sizeof(Actors_ActorRecord));
// The weapon record holds four bones.
constexpr std::size_t kWeaponRecordBones = 4;
static_assert(sizeof(Actors_WeaponModel::bones) ==
              kWeaponRecordBones * sizeof(SVECTOR));
static_assert(sizeof(Actors_WeaponModel::palettes) ==
              sizeof(WeaponAssetRecord::palettes));
static_assert(sizeof(Actors_WeaponModel::workingPalette) ==
              sizeof(WeaponAssetRecord::working_palette));
static_assert(sizeof(Actors_WeaponModel::uploadPalette) ==
              sizeof(WeaponAssetRecord::upload_palette));
static_assert(std::is_same_v<u_short, std::uint16_t>);
// The original's 8-byte texture slot entries.
constexpr std::size_t kTextureSlotBytes = 8;
static_assert(sizeof(Render_TextureSlot) == kTextureSlotBytes);
static_assert(sizeof(RECT) == sizeof(VramRect));
static_assert(std::is_same_v<decltype(Content_DiscFile::lba), std::int32_t>);
static_assert(std::is_same_v<decltype(Content_DiscFile::size), std::int32_t>);

constexpr std::uint8_t kSkippedBit = 0x01;
constexpr std::uint8_t kUvLayoutBit = 0x08;
constexpr std::uint8_t kParsedBit = 0x80;
// isVisible and isFrozen: not loader fields.
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
    Core_Warnf("content",
               "actor assets refused %s (%" PRId64 "); %" PRIu64 " refused",
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

std::uint32_t PlacementWord(const Field_ActorPlacement& placement) {
  std::uint32_t value = 0;
  std::memcpy(&value, &placement, sizeof value);
  return value;
}

Field_ActorPlacement Placement(std::uint32_t value) {
  Field_ActorPlacement placement;
  std::memcpy(&placement, &value, sizeof value);
  return placement;
}

bool InRange(int index, int count) { return index >= 0 && index < count; }

Actors_WeaponModel* WeaponAt(int slot) {
  return InRange(slot, kWeaponModelSlots) ? Actors_weaponModels[slot] : nullptr;
}

Actors_ActorRecord* ActorAt(int slot) {
  return InRange(slot, kActorRecordSlots) ? Actors_actorRecords[slot] : nullptr;
}

// --- Requests and loader state ---------------------------------------------

ActorAssetRequest LoadRequest(const Content_ActorAssetRequest& in) {
  return {
      .kind = in.assetKind,
      .target_slot = in.targetSlot,
      .model_id = in.modelId,
      .destination = Address(in.destination),
      .data = static_cast<AssetAddress>(in.dataAddress),
      .placement = PlacementWord(in.placement),
      .sub_kind = in.subKind,
      .texture_slot = static_cast<std::uint8_t>(in.textureSlot),
      .material = static_cast<std::uint8_t>(in.material),
      .variant = static_cast<std::uint8_t>(in.variant),
  };
}

void StoreRequestFields(const ActorAssetRequest& in,
                        Content_ActorAssetRequest& out) {
  out.assetKind = in.kind;
  out.targetSlot = in.target_slot;
  out.modelId = in.model_id;
  out.destination = Pointer(in.destination);
  out.dataAddress = static_cast<std::uintptr_t>(in.data);
  out.placement = Placement(in.placement);
  out.subKind = in.sub_kind;
  out.textureSlot = static_cast<char>(in.texture_slot);
  out.material = static_cast<char>(in.material);
  out.variant = static_cast<char>(in.variant);
}

ActorAssetLoadState LoadState() {
  ActorAssetLoadState state;
  state.model_phase = Content_legacyModelLoadPhase;
  state.parse_phase = Content_modelParseResetFlag;
  state.sequence_parse_phase = Content_legacySequenceParsePhase;
  state.effect_texture_phase = Content_legacyEtmLoadPhase;
  state.weapon_file_loaded = Content_legacyWeaponFileLoaded != 0;
  state.model_file_allocated = Content_legacyShpAllocated != 0;
  state.sequence_file_owned = Content_legacySequenceBufferOwned != 0;
  state.weapon_file = Address(Content_legacyWeaponFileBuffer);
  state.model_file = Address(Content_legacyShpFileBuffer);
  state.sequence_file = Address(Content_legacySequenceFileBuffer);
  state.effect_texture_file = Address(Content_legacyEtmFileBuffer);
  state.weapon_cursor = Address(Content_legacyWeaponParseCursor);
  state.model_cursor = Address(Content_legacyShpParseCursor);
  state.sequence_cursor = Address(Content_legacySequenceParseData);
  state.model_palette_words = Content_legacyShpCopiedWordCount;
  state.model_header_word = Content_legacyShpHeaderWord;
  state.model_texture_width = Content_legacyShpTextureWidth;
  state.model_texture_height = Content_legacyShpTextureHeight;
  return state;
}

void StoreState(const ActorAssetLoadState& state) {
  Content_legacyModelLoadPhase = state.model_phase;
  Content_modelParseResetFlag = state.parse_phase;
  Content_legacySequenceParsePhase = state.sequence_parse_phase;
  Content_legacyEtmLoadPhase = state.effect_texture_phase;
  Content_legacyWeaponFileLoaded = state.weapon_file_loaded ? 1 : 0;
  Content_legacyShpAllocated = state.model_file_allocated ? 1 : 0;
  Content_legacySequenceBufferOwned = state.sequence_file_owned ? 1 : 0;
  Content_legacyWeaponFileBuffer = Pointer(state.weapon_file);
  Content_legacyShpFileBuffer = Pointer(state.model_file);
  Content_legacySequenceFileBuffer = Pointer(state.sequence_file);
  Content_legacyEtmFileBuffer = Pointer<u_short>(state.effect_texture_file);
  Content_legacyWeaponParseCursor = Pointer(state.weapon_cursor);
  Content_legacyShpParseCursor = Pointer<u_char>(state.model_cursor);
  Content_legacySequenceParseData = Pointer<u_char>(state.sequence_cursor);
  Content_legacyShpCopiedWordCount = state.model_palette_words;
  Content_legacyShpHeaderWord = state.model_header_word;
  Content_legacyShpTextureWidth = state.model_texture_width;
  Content_legacyShpTextureHeight = state.model_texture_height;
}

ActorAssetQueueStats LoadStats() {
  return {
      .peak_frame_ticks = Content_assetQueuePeakUsage,
      .last_load_phase = Content_lastLoadPhaseSnapshot,
      .frame_ticks = Content_assetQueueFrameTicks,
      .last_kind = Content_lastAssetRequestType,
  };
}

void StoreStats(const ActorAssetQueueStats& stats) {
  Content_assetQueuePeakUsage = stats.peak_frame_ticks;
  Content_lastLoadPhaseSnapshot = stats.last_load_phase;
  Content_assetQueueFrameTicks = stats.frame_ticks;
  Content_lastAssetRequestType = stats.last_kind;
}

// --- Weapon records ----------------------------------------------------------

WeaponAssetRecord LoadWeaponRecord(const Actors_WeaponModel& in) {
  WeaponAssetRecord out;
  out.bone_count = in.boneCount;
  out.vertex_group_count = in.vertexGroupCount;
  out.triangle_count = in.triangleCount;
  out.quad_count = in.quadCount;
  out.other_primitive_count = in.otherPrimitiveCount;
  out.actor_id = in.actorId;
  out.reserved08_bit4 = in.reserved08Bit4 != 0;
  out.wide_texture = in.isWideTexture != 0;
  out.reserved08_bit6 = in.reserved08Bit6 != 0;
  out.double_clut = in.isDoubleClut != 0;
  out.material = in.material;
  out.palette_color_count = in.paletteColorCount;
  out.attachment_slot = in.attachmentSlot;
  out.attachment_bone = in.attachmentBone;
  out.model_id = in.modelId;
  out.texture_slot = in.textureSlot;
  out.palette_slot_offset = in.paletteSlotOffset;
  out.reserved11 = in.reserved11;
  out.reserved13 = in.reserved13;
  out.geometry = Address(in.sectionOffsets);
  for (std::size_t i = 0; i < out.palettes.size(); ++i) {
    std::copy(std::begin(in.palettes[i]), std::end(in.palettes[i]),
              out.palettes[i].begin());
  }
  std::copy(std::begin(in.workingPalette), std::end(in.workingPalette),
            out.working_palette.begin());
  std::copy(std::begin(in.uploadPalette), std::end(in.uploadPalette),
            out.upload_palette.begin());
  return out;
}

void StoreWeaponRecord(const WeaponAssetRecord& in, Actors_WeaponModel& out) {
  out.boneCount = in.bone_count;
  out.vertexGroupCount = in.vertex_group_count;
  out.triangleCount = in.triangle_count;
  out.quadCount = in.quad_count;
  out.otherPrimitiveCount = in.other_primitive_count;
  out.actorId = static_cast<u_char>(in.actor_id & kNibbleMask);
  out.reserved08Bit4 = in.reserved08_bit4 ? 1 : 0;
  out.isWideTexture = in.wide_texture ? 1 : 0;
  out.reserved08Bit6 = in.reserved08_bit6 ? 1 : 0;
  out.isDoubleClut = in.double_clut ? 1 : 0;
  out.material = static_cast<u_char>(in.material & kNibbleMask);
  out.paletteColorCount = in.palette_color_count;
  out.attachmentSlot = in.attachment_slot;
  out.attachmentBone = in.attachment_bone;
  out.modelId = in.model_id;
  out.textureSlot = in.texture_slot;
  out.paletteSlotOffset = in.palette_slot_offset;
  out.reserved11 = in.reserved11;
  out.reserved13 = in.reserved13;
  out.sectionOffsets = Pointer<Actors_WeaponModelData>(in.geometry);
  for (std::size_t i = 0; i < in.palettes.size(); ++i) {
    std::copy(in.palettes[i].begin(), in.palettes[i].end(),
              std::begin(out.palettes[i]));
  }
  std::copy(in.working_palette.begin(), in.working_palette.end(),
            std::begin(out.workingPalette));
  std::copy(in.upload_palette.begin(), in.upload_palette.end(),
            std::begin(out.uploadPalette));
}

// --- Actor records -----------------------------------------------------------

void LoadInstance(const Actors_ModelInstance& in, ActorAssetRecord& out) {
  out.bone_count = in.boneCount;
  out.vertex_group_count = in.vertexGroupCount;
  out.triangle_count = in.triangleCount;
  out.quad_count = in.quadCount;
  out.other_polygon_count = in.otherPolygonCount;
  const std::uint8_t flags = ByteOf(in, kInstanceFlagsAt);
  out.is_skipped = (flags & kSkippedBit) != 0;
  out.uv_layout = (flags & kUvLayoutBit) != 0;
  out.texture_slot_count_minus_one = static_cast<std::uint8_t>(
      (flags >> kTextureSlotCountShift) & kTextureSlotCountMask);
  out.is_parsed = (flags & kParsedBit) != 0;
  out.action_animation_state = in.actionAnimationState;
  out.is_attached = in.isAttached != 0;
  out.sound_enabled = in.soundEnabled != 0;
  out.weapon_drawn = in.weaponDrawn != 0;
  out.support_layer = in.supportLayer;
  out.placement_layer = in.placementLayer;
  out.palette_words = in.copiedWordCount;
  out.slot = in.slot;
  out.variant = in.variant;
  out.attach_parent = in.attachParent;
  out.attach_bone = in.attachBone;
  out.scale_x = in.scaleX;
  out.scale_y = in.scaleY;
  out.scale_z = in.scaleZ;
  out.base_color = WordAt(in, offsetof(Actors_ModelInstance, baseColor));
  out.tint = WordAt(in, offsetof(Actors_ModelInstance, tint));
  out.placement = WordAt(in, offsetof(Actors_ModelInstance, currentTileX));
  out.model = Address(in.model);
}

void StoreInstance(const ActorAssetRecord& in, Actors_ModelInstance& out) {
  out.boneCount = in.bone_count;
  out.vertexGroupCount = in.vertex_group_count;
  out.triangleCount = in.triangle_count;
  out.quadCount = in.quad_count;
  out.otherPolygonCount = in.other_polygon_count;
  std::uint8_t& flags = ByteAt(out, kInstanceFlagsAt);
  std::uint8_t value = flags & kKeptInstanceFlags;
  value |= in.is_skipped ? kSkippedBit : 0;
  value |= in.uv_layout ? kUvLayoutBit : 0;
  value |= static_cast<std::uint8_t>(
      (in.texture_slot_count_minus_one & kTextureSlotCountMask)
      << kTextureSlotCountShift);
  value |= in.is_parsed ? kParsedBit : 0;
  flags = value;
  out.actionAnimationState =
      static_cast<u_char>(in.action_animation_state & kNibbleMask);
  out.isAttached = in.is_attached ? 1 : 0;
  out.soundEnabled = in.sound_enabled ? 1 : 0;
  out.weaponDrawn = in.weapon_drawn ? 1 : 0;
  out.supportLayer = static_cast<u_char>(in.support_layer & kNibbleMask);
  out.placementLayer = static_cast<u_char>(in.placement_layer & kNibbleMask);
  out.copiedWordCount = in.palette_words;
  out.slot = in.slot;
  out.variant = in.variant;
  out.attachParent = in.attach_parent;
  out.attachBone = in.attach_bone;
  out.scaleX = in.scale_x;
  out.scaleY = in.scale_y;
  out.scaleZ = in.scale_z;
  SetWordAt(out, offsetof(Actors_ModelInstance, baseColor), in.base_color);
  SetWordAt(out, offsetof(Actors_ModelInstance, tint), in.tint);
  SetWordAt(out, offsetof(Actors_ModelInstance, currentTileX), in.placement);
  out.model = Pointer<Actors_ShpModel>(in.model);
}

void LoadActorFields(const Actors_ActorRecord& in, ActorAssetRecord& out) {
  out.holds_texture_slots = in.holdsTextureSlots != 0;
  out.texture_header_bit = in.textureHeaderBit != 0;
  out.advance_requested = in.advanceRequested != 0;
  out.custom_tint = in.customTint != 0;
  out.ignored_by_collision = in.ignoredByCollision != 0;
  out.sequence_model_id = in.sequenceModelId;
  out.frames_per_step = in.framesPerStep;
  out.locomotion_counter = in.locomotionCounter;
  out.sequence_flags = in.sequenceFlags;
  out.common_animation_count = in.commonAnimationCount;
  out.battle_animation_count = in.battleAnimationCount;
  out.texture_slot = in.textureSlot;
  out.travel_animation = in.travelAnimation;
  out.animation_length = in.animationLength;
  out.turn_rate = in.turnRate;
  out.move_rate = in.moveRate;
  out.owned_sequence = Address(in.ownedSequence);
  out.common_sequence = Address(in.commonSequence);
  out.battle_sequence = Address(in.battleSequence);
  for (std::size_t i = 0; i < out.shared_sequences.size(); ++i) {
    out.shared_sequences[i] = Address(in.sharedSequences[i]);
  }
  out.occupancy_radius = in.occupancyRadius;
  out.occupancy_height = in.occupancyHeight;
  out.opaque648 = static_cast<std::uint16_t>(in.opaque648);
  out.opaque6dc = in.opaque6DC;
  out.model_id = in.modelId;
  out.clut_a = in.clutA;
  out.clut_b = in.clutB;
  out.fade = WordAt(in, offsetof(Actors_ActorRecord, fadeBlue));
  out.companion_slot = in.companionSlot;
  out.companion = {.x = in.companionState.offsetX,
                   .y = in.companionState.offsetY,
                   .z = in.companionState.offsetZ,
                   .depth_bias = in.companionState.depthBias};
  out.scale = in.scale;
  const std::uint32_t rows =
      WordAt(in, offsetof(Actors_ActorRecord, textureRowOffsets));
  std::memcpy(out.texture_row_offsets.data(), &rows, sizeof rows);
  out.model_data_size = in.modelDataSize;
}

void StoreActorFields(const ActorAssetRecord& in, Actors_ActorRecord& out) {
  out.holdsTextureSlots = in.holds_texture_slots ? 1 : 0;
  out.textureHeaderBit = in.texture_header_bit ? 1 : 0;
  out.advanceRequested = in.advance_requested ? 1 : 0;
  out.customTint = in.custom_tint ? 1 : 0;
  out.ignoredByCollision = in.ignored_by_collision ? 1 : 0;
  out.sequenceModelId = in.sequence_model_id;
  out.framesPerStep = in.frames_per_step;
  out.locomotionCounter = in.locomotion_counter;
  out.sequenceFlags = in.sequence_flags;
  out.commonAnimationCount = in.common_animation_count;
  out.battleAnimationCount = in.battle_animation_count;
  out.textureSlot = in.texture_slot;
  out.travelAnimation = in.travel_animation;
  out.animationLength = in.animation_length;
  out.turnRate = in.turn_rate;
  out.moveRate = in.move_rate;
  out.ownedSequence = Pointer(in.owned_sequence);
  out.commonSequence = Pointer(in.common_sequence);
  out.battleSequence = Pointer(in.battle_sequence);
  for (std::size_t i = 0; i < in.shared_sequences.size(); ++i) {
    out.sharedSequences[i] = Pointer(in.shared_sequences[i]);
  }
  out.occupancyRadius = in.occupancy_radius;
  out.occupancyHeight = in.occupancy_height;
  out.opaque648 = in.opaque648;
  out.opaque6DC = in.opaque6dc;
  out.modelId = in.model_id;
  out.clutA = in.clut_a;
  out.clutB = in.clut_b;
  SetWordAt(out, offsetof(Actors_ActorRecord, fadeBlue), in.fade);
  out.companionSlot = in.companion_slot;
  out.companionState.offsetX = in.companion.x;
  out.companionState.offsetY = in.companion.y;
  out.companionState.offsetZ = in.companion.z;
  out.companionState.depthBias = in.companion.depth_bias;
  out.scale = in.scale;
  std::uint32_t rows = 0;
  std::memcpy(&rows, in.texture_row_offsets.data(), sizeof rows);
  SetWordAt(out, offsetof(Actors_ActorRecord, textureRowOffsets), rows);
  out.modelDataSize = in.model_data_size;
}

void LoadActorTables(const Actors_ActorRecord& in, ActorAssetRecord& out) {
  for (std::size_t i = 0; i < out.texture_scroll_bounds.size(); ++i) {
    const Actors_TextureScrollBounds& bounds = in.textureScrollBounds[i];
    out.texture_scroll_bounds[i] = {.x = bounds.x,
                                    .y = bounds.y,
                                    .initial_step_x = bounds.initialStepX,
                                    .initial_step_y = bounds.initialStepY,
                                    .interval = bounds.interval};
  }
  for (std::size_t i = 0; i < out.texture_scroll.size(); ++i) {
    const Actors_TextureScrollState& scroll = in.textureScroll[i];
    out.texture_scroll[i] = {.x = scroll.x,
                             .y = scroll.y,
                             .step_x = scroll.stepX,
                             .step_y = scroll.stepY,
                             .interval = scroll.interval,
                             .timer = scroll.timer};
  }
  std::ranges::copy(in.battleSequenceLbas, out.battle_sequence_lbas.begin());
  std::ranges::copy(in.chainAttackAnimationIds,
                    out.chain_attack_animation_ids.begin());
  std::ranges::copy(in.specialSequenceLbas, out.special_sequence_lbas.begin());
  std::ranges::copy(in.specialSequenceSizes,
                    out.special_sequence_sizes.begin());
  std::memcpy(out.palettes.data(), in.palettes, sizeof in.palettes);
  std::ranges::copy(in.basePalette, out.base_palette.begin());
  std::ranges::copy(in.currentPalette, out.current_palette.begin());
  std::memcpy(out.joint_slots.data(), in.jointSlots, sizeof in.jointSlots);
}

void StoreActorTables(const ActorAssetRecord& in, Actors_ActorRecord& out) {
  for (std::size_t i = 0; i < in.texture_scroll_bounds.size(); ++i) {
    Actors_TextureScrollBounds& bounds = out.textureScrollBounds[i];
    bounds.x = in.texture_scroll_bounds[i].x;
    bounds.y = in.texture_scroll_bounds[i].y;
    bounds.initialStepX = in.texture_scroll_bounds[i].initial_step_x;
    bounds.initialStepY = in.texture_scroll_bounds[i].initial_step_y;
    bounds.interval = in.texture_scroll_bounds[i].interval;
  }
  for (std::size_t i = 0; i < in.texture_scroll.size(); ++i) {
    Actors_TextureScrollState& scroll = out.textureScroll[i];
    scroll.x = in.texture_scroll[i].x;
    scroll.y = in.texture_scroll[i].y;
    scroll.stepX = in.texture_scroll[i].step_x;
    scroll.stepY = in.texture_scroll[i].step_y;
    scroll.interval = in.texture_scroll[i].interval;
    scroll.timer = in.texture_scroll[i].timer;
  }
  std::ranges::copy(in.battle_sequence_lbas,
                    std::begin(out.battleSequenceLbas));
  std::ranges::copy(in.chain_attack_animation_ids,
                    std::begin(out.chainAttackAnimationIds));
  std::ranges::copy(in.special_sequence_lbas,
                    std::begin(out.specialSequenceLbas));
  std::ranges::copy(in.special_sequence_sizes,
                    std::begin(out.specialSequenceSizes));
  std::memcpy(out.palettes, in.palettes.data(), sizeof out.palettes);
  std::ranges::copy(in.base_palette, std::begin(out.basePalette));
  std::ranges::copy(in.current_palette, std::begin(out.currentPalette));
  std::memcpy(out.jointSlots, in.joint_slots.data(), sizeof out.jointSlots);
}

// --- Files -------------------------------------------------------------------

template <typename T, std::size_t N>
std::optional<std::uint32_t> TableEntry(const T (&table)[N],
                                        std::size_t index) {
  if (index >= N) {
    return std::nullopt;
  }
  return table[index];
}

// The material-model tables, indexed material * 4 + model.
template <typename T, std::size_t Rows, std::size_t Columns>
std::optional<std::uint32_t> FlatTableEntry(const T (&table)[Rows][Columns],
                                            std::size_t index) {
  if (index >= Rows * Columns) {
    return std::nullopt;
  }
  return table[index / Columns][index % Columns];
}

struct LiveFileStream {
  Content_DiscFile* file;
  Content_DiscQueueSlot** request;
};

LiveFileStream Stream(AssetFileStream stream) {
  switch (stream) {
    case AssetFileStream::kWeapon:
      return {&Content_legacyWeaponFileLocation,
              &Content_legacyWeaponDiscRequest};
    case AssetFileStream::kActorModel:
      return {&Content_legacyShpFileLocation, &Content_legacyShpDiscRequest};
    case AssetFileStream::kSequence:
      return {&Content_legacySequenceFileLocation,
              &Content_legacySequenceDiscRequest};
    case AssetFileStream::kEffectTexture:
    default:
      return {&Content_legacyEtmFileLocation, &Content_legacyEtmDiscRequest};
  }
}

Render_TextureSlot* TextureSlotAt(AssetTextureTable table, int index) {
  if (table == AssetTextureTable::kActor) {
    return InRange(index, kActorTextureSlots)
               ? &Render_legacyActorTextureSlots[index]
               : nullptr;
  }
  return InRange(index, kWeaponTextureSlots)
             ? &Render_legacyWeaponTextureSlots[index]
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
               ? Content_legacyActorAssetRequests[slot].assetKind
               : 0;
  }
  void StoreRequest(int slot, const ActorAssetRequest& request) override {
    if (!InRange(slot, kActorAssetRequestCount)) {
      CountRefusal("asset request slot", slot);
      return;
    }
    StoreRequestFields(request, Content_legacyActorAssetRequests[slot]);
  }
  void OnRequestQueued(const ActorAssetRequest& request) override {
    const std::uint8_t phase = active_state_ != nullptr
                                   ? active_state_->model_phase
                                   : Content_legacyModelLoadPhase;
    const std::uint8_t parse = active_state_ != nullptr
                                   ? active_state_->parse_phase
                                   : Content_modelParseResetFlag;
    Core_Debugf("actors", "queue kind=%u actor=%u model=%u phase=%u parse=%u",
                static_cast<unsigned>(request.kind),
                static_cast<unsigned>(request.target_slot),
                static_cast<unsigned>(request.model_id),
                static_cast<unsigned>(phase), static_cast<unsigned>(parse));
  }
  void MoveRequest(int from, int to) override {
    Content_legacyActorAssetRequests[to] =
        Content_legacyActorAssetRequests[from];
  }
  void ClearRequestKind(int slot) override {
    Content_legacyActorAssetRequests[slot].assetKind = 0;
  }
  int ProcessHeadRequest(ActorAssetKind kind) override {
    Content_ActorAssetRequest& head = Content_legacyActorAssetRequests[0];
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
  std::int32_t FrameCounter() override { return VSync(1); }
  [[nodiscard]] std::uint8_t ModelLoadPhase() const override {
    return Content_legacyModelLoadPhase;
  }
  void OnHeadRequestDone(int result) override {
    const Content_ActorAssetRequest& head = Content_legacyActorAssetRequests[0];
    // The original indexed the records with any slot below 17.
    const void* record = ActorAt(head.targetSlot);
    Core_Debugf("actors",
                "loaded kind=%u actor=%u model=%d result=%d record=%p",
                static_cast<unsigned>(head.assetKind),
                static_cast<unsigned>(head.targetSlot),
                static_cast<int>(head.modelId), result, record);
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
    if (*live.request == nullptr) {
      return false;
    }
    EnqueueLiveDiscRequest(*live.request, Pointer(destination));
    return true;
  }
  void ReadRefusedFile(DiscFileExtent file, AssetAddress destination) override {
    Content_DiscFile location{};
    location.lba = file.sector;
    location.size = file.bytes;
    if (ReadLiveRefusedDiscFile(&location, Pointer(destination)) == 0) {
      CountRefusal("unread asset file sector", file.sector);
    }
  }
  bool FileReadFinished(AssetFileStream stream) override {
    const Content_DiscQueueSlot* request = *Stream(stream).request;
    return request == nullptr ||
           static_cast<DiscRequestState>(request->state) ==
               DiscRequestState::kLoaded;
  }
  void ReleaseFileRead(AssetFileStream stream) override {
    ReleaseLiveDiscRequest(*Stream(stream).request);
  }

  // VRAM.
  [[nodiscard]] AssetTextureSlot TextureSlot(AssetTextureTable table,
                                             int index) const override {
    const Render_TextureSlot* slot = TextureSlotAt(table, index);
    if (slot == nullptr) {
      CountRefusal("texture slot read", index);
      return {};
    }
    return {.texture_page = slot->texturePage,
            .palette = slot->palette,
            .alternate_palette = slot->alternatePalette,
            .references = slot->referenceCount};
  }
  void SetTextureSlot(AssetTextureTable table, int index,
                      const AssetTextureSlot& value) override {
    Render_TextureSlot* slot = TextureSlotAt(table, index);
    if (slot == nullptr) {
      CountRefusal("texture slot write", index);
      return;
    }
    slot->texturePage = value.texture_page;
    slot->palette = value.palette;
    slot->alternatePalette = value.alternate_palette;
    slot->referenceCount = value.references;
  }
  [[nodiscard]] std::span<const std::uint8_t> TextureSlotOrigins()
      const override {
    return Actors_textureSlotOrigins;
  }
  [[nodiscard]] std::span<const std::uint8_t> TextureSlotRowOffsets()
      const override {
    return Actors_textureSlotRowOffsets;
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
    LoadImage(&area,
              reinterpret_cast<u_long*>(const_cast<std::byte*>(pixels.data())));
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
    const Actors_WeaponModel* model = WeaponAt(slot);
    if (model == nullptr) {
      return {};
    }
    return {.present = true,
            .actor_id = model->actorId,
            .model_id = model->modelId,
            .bone_count = model->boneCount};
  }
  bool BindWeaponRecord(int slot, AssetAddress destination) override {
    auto* model = BindRecord<Actors_WeaponModel>(
        slot, kWeaponModelSlots, destination, "weapon record destination");
    if (model == nullptr) {
      return false;
    }
    Actors_weaponModels[slot] = model;
    model->textureSlot = kNoAssetTextureSlot;
    return true;
  }
  [[nodiscard]] WeaponAssetRecord LoadWeapon(int slot) const override {
    const Actors_WeaponModel* model = WeaponAt(slot);
    if (model == nullptr) {
      CountRefusal("weapon record read", slot);
      return {};
    }
    return LoadWeaponRecord(*model);
  }
  void StoreWeapon(int slot, const WeaponAssetRecord& record) override {
    Actors_WeaponModel* model = WeaponAt(slot);
    if (model == nullptr) {
      CountRefusal("weapon record write", slot);
      return;
    }
    StoreWeaponRecord(record, *model);
  }
  void CopyWeaponRecord(int target, int source) override {
    Actors_WeaponModel* to = WeaponAt(target);
    const Actors_WeaponModel* from = WeaponAt(source);
    if (to == nullptr || from == nullptr) {
      CountRefusal("weapon record copy", source);
      return;
    }
    std::memmove(to, from, sizeof *to);
  }
  void WriteWeaponBones(int slot, std::span<const std::byte> bones) override {
    Actors_WeaponModel* model = WeaponAt(slot);
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
      Actors_weaponModels[slot] = nullptr;
    }
  }
  [[nodiscard]] AssetAddress WeaponGeometryBuffer(int slot) const override {
    return slot == 0 || slot == 1
               ? Address(Content_legacyWeaponModelBuffers[slot])
               : AssetAddress::kNone;
  }
  [[nodiscard]] std::span<const std::uint8_t> WeaponMaterialPalettes()
      const override {
    return Items_weaponMaterialPaletteIndices;
  }
  int FindActorBoneTag(int actor, int tag) override {
    return VS::Actors::FindLiveActorBoneTag(actor, tag, 0);
  }

  // Actor records.
  [[nodiscard]] ActorAssetSummary ActorSummary(int slot) const override {
    const Actors_ActorRecord* actor = ActorAt(slot);
    if (actor == nullptr) {
      return {};
    }
    return {.present = true,
            .model_id = actor->modelId,
            .sequence_model_id = actor->sequenceModelId,
            .common_sequence = Address(actor->commonSequence)};
  }
  bool BindActorRecord(int slot, AssetAddress destination) override {
    auto* actor = BindRecord<Actors_ActorRecord>(
        slot, kActorRecordSlots, destination, "actor record destination");
    if (actor == nullptr) {
      return false;
    }
    Actors_actorRecords[slot] = actor;
    actor->textureSlot = kNoAssetTextureSlot;
    actor->instance.attachParent = kNoAssetTextureSlot;
    return true;
  }
  [[nodiscard]] ActorAssetRecord LoadActor(int slot) const override {
    const Actors_ActorRecord* actor = ActorAt(slot);
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
    Actors_ActorRecord* actor = ActorAt(slot);
    if (actor == nullptr) {
      CountRefusal("actor record write", slot);
      return;
    }
    StoreInstance(record, actor->instance);
    StoreActorFields(record, *actor);
    StoreActorTables(record, *actor);
  }
  void CopyActorRecord(int target, int source) override {
    Actors_ActorRecord* to = ActorAt(target);
    const Actors_ActorRecord* from = ActorAt(source);
    if (to == nullptr || from == nullptr) {
      CountRefusal("actor record copy", source);
      return;
    }
    std::memmove(to, from, sizeof *to);
  }
  void WriteActorTextureLayout(int slot,
                               std::span<const std::byte> layout) override {
    Actors_ActorRecord* actor = ActorAt(slot);
    if (actor == nullptr || layout.size() > kActorTextureLayoutBytes) {
      CountRefusal("actor texture layout", slot);
      return;
    }
    // The layout fills textureOverlays and the fields after it.
    std::memcpy(reinterpret_cast<std::byte*>(actor) +
                    offsetof(Actors_ActorRecord, textureOverlays),
                layout.data(), layout.size());
  }
  void ClearActorSlot(int slot) override {
    if (InRange(slot, kActorRecordSlots)) {
      Actors_actorRecords[slot] = nullptr;
    }
  }
  [[nodiscard]] std::int16_t RoomActorScale() const override {
    return static_cast<std::int16_t>(Actors_roomActorScale.vx);
  }
  void ResetActorMotion(int slot) override {
    VS::Actors::ResetLiveActorMotionState(slot, 0);
  }
  void ClearActorPaletteFade(int slot) override {
    VS::Actors::SetLiveActorPaletteFade(slot, 0);
  }
  void PlaceActorInRoom(int slot, std::uint32_t placement, int mode) override {
    Field_ActorPlacement value = Placement(placement);
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
    value.data = Address(Actors_sharedSequenceData[bank]);
    for (std::size_t i = 0; i < kSharedSequenceEntries; ++i) {
      value.animation_ids[i] = Actors_sharedSequenceAnimationIds[bank][i];
      value.entry_params[i] = Actors_sharedSequenceEntryParams[bank][i];
      value.animation_data[i] =
          Address(Actors_sharedSequenceAnimationData[bank][i]);
    }
    return value;
  }
  void StoreSequenceBank(int bank, const SharedSequenceBank& value) override {
    if (!InRange(bank, kSharedSequenceBanks)) {
      CountRefusal("sequence bank write", bank);
      return;
    }
    Actors_sharedSequenceData[bank] = Pointer(value.data);
    for (std::size_t i = 0; i < kSharedSequenceEntries; ++i) {
      Actors_sharedSequenceAnimationIds[bank][i] = value.animation_ids[i];
      Actors_sharedSequenceEntryParams[bank][i] = value.entry_params[i];
      Actors_sharedSequenceAnimationData[bank][i] =
          Pointer<char>(value.animation_data[i]);
    }
  }
  [[nodiscard]] std::uint8_t BankTexturePage(int bank) const override {
    if (!InRange(bank, kSharedSequenceBanks)) {
      CountRefusal("sequence bank page read", bank);
      return 0;
    }
    return Actors_sharedSequenceBankTexturePages[bank];
  }
  void SetBankTexturePage(int bank, std::uint8_t page) override {
    if (!InRange(bank, kSharedSequenceBanks)) {
      CountRefusal("sequence bank page write", bank);
      return;
    }
    Actors_sharedSequenceBankTexturePages[bank] = page;
  }
  [[nodiscard]] std::span<const std::uint8_t> SharedSequenceIdsByKind()
      const override {
    return Actors_sharedSequenceIdsByKind;
  }
  [[nodiscard]] AssetAddress PrimarySequenceBuffer() const override {
    return Address(Actors_primarySequenceBuffer);
  }
  [[nodiscard]] AssetAddress SequenceWorkBuffer() const override {
    return Address(Actors_sequenceWorkBuffer);
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
        return TableEntry(Content_legacyShpDiscOffsets, index);
      case AssetFileTable::kShpSectors:
        return TableEntry(Content_legacyShpSectorCounts, index);
      case AssetFileTable::kSequenceOffsets:
        return TableEntry(Content_legacySequenceDiscOffsets, index);
      case AssetFileTable::kSequenceSectors:
        return TableEntry(Content_legacySequenceSectorCounts, index);
      case AssetFileTable::kEtmOffsets:
        return TableEntry(Content_legacyEtmDiscOffsets, index);
      case AssetFileTable::kEtmSectors:
        return TableEntry(Content_legacyEtmSectorCounts, index);
      case AssetFileTable::kWepOffsets:
        return TableEntry(Content_legacyWepDiscOffsets, index);
      case AssetFileTable::kWepSectors:
        return TableEntry(Content_legacyWepSectorCounts, index);
      case AssetFileTable::kMaterialSequenceOffsets:
        return TableEntry(Content_legacyMaterialSequenceDiscOffsets, index);
      case AssetFileTable::kMaterialSequenceSectors:
        return TableEntry(Content_legacyMaterialSequenceSectorCounts, index);
      case AssetFileTable::kMaterialModelSequenceOffsets:
        return FlatTableEntry(Content_legacyMaterialModelSequenceDiscOffsets,
                              index);
      case AssetFileTable::kMaterialModelSequenceSectors:
        return FlatTableEntry(Content_legacyMaterialModelSequenceSectorCounts,
                              index);
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
}  // namespace

int FindLiveFreeActorAssetRequest() {
  LiveActorAssetHost host;
  return FindFreeActorAssetRequest(host);
}

int QueueLiveActorAssetRequest(const Content_ActorAssetRequest* request) {
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

void CopyLiveWeaponSequence(Content_SequenceBuffer* destination,
                            Content_SequenceBuffer* source) {
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
