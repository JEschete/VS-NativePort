#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Content/ActorAssets.hpp"
#include "Content/CdDrive.hpp"

namespace VS::Actors {
struct WeaponModelData;
}

// Live binding for the battle asset queue and loaders (ActorAssets.hpp) over
// the legacy request queue (legacyActorAssetRequests), disc file tables
// (below), the actor and weapon records
// (VS::Actors::actorRecords, VS::Actors::weaponModels), the shared sequence
// banks and the texture slot tables. Generated callers keep the original call
// shapes.
//
// A record, buffer or file the typed loaders cannot bound, or a request they
// refuse, is counted and warned about on the "content" channel. A loader
// read the disc queue refuses (#282) is read before the loader returns.
// Actor, weapon and sequence parser state belongs to each queue entry and
// moves or clears with its identity and record-memory resolver. The legacy
// parser globals below are output-only diagnostic snapshots; only the
// unowned ETM loader still reads its legacy phase and buffer (#207).
namespace VS::Content {

struct DiscFile;
struct DiscQueueSlot;

// decomp: vs_battle_objectData (src/BATTLE/BATTLE.PRG/2842C.h:36), upstream
// vs_ObjectData (src/BATTLE/BATTLE.PRG/objectData.h:6-17): one slot of the
// legacy request queue. ActorAssetRequest is its typed form; the Live
// binding converts between them.
struct LegacyActorAssetRequest {
  std::uint8_t asset_kind = 0;
  std::int8_t target_slot = 0;
  std::int16_t model_id = 0;
  void* destination = nullptr;
  std::uintptr_t data_address = 0;
  // The four placement bytes (func_80089888_t: Actors::PackPlacement).
  std::uint32_t placement = 0;
  std::uint8_t sub_kind = 0;
  std::uint8_t texture_slot = 0;
  std::uint8_t material = 0;
  std::uint8_t variant = 0;
};

// The bytes of a loaded SEQ buffer (func_8009C378_t).
inline constexpr std::size_t kSequenceBufferBytes = 0x7900;

// decomp: func_8009C378_t (src/BATTLE/BATTLE.PRG/battleUnk.h:23), upstream
// src/BATTLE/BATTLE.PRG/battleUnk.h:19-23: a loaded SEQ buffer whose two
// section pointers stay struct-relative on-disc offsets.
struct SequenceBuffer {
  std::uint32_t first_section_offset;
  std::uint32_t second_section_offset;
  std::uint8_t body[kSequenceBufferBytes - (2 * sizeof(std::uint32_t))];
};
static_assert(sizeof(SequenceBuffer) == kSequenceBufferBytes,
              "SequenceBuffer must keep its PS1 layout");

int FindLiveFreeActorAssetRequest();
// Copies the typed `request` into the first free queue slot; -1 when the
// queue is full.
int QueueLiveActorAsset(const ActorAssetRequest& request);
int UpdateLiveActorAssetQueue();
// Owned requests (#48; ActorAssets.hpp SubmitActorAssetRequest). Forgetting
// a pending ticket disowns its request, which then completes unobserved;
// owners cancel first. An owned request names no memory: `memory` is where
// the owner keeps the record of `table` at `slot` that the request binds, or
// kNone to refuse it, asked when the record binds. A submission without one
// is refused.
using ActorRecordMemory = AssetAddress (*)(ActorAssetOwner owner,
                                           ActorRecordTable table, int slot);
[[nodiscard]] std::optional<ActorAssetTicket> SubmitLiveActorAsset(
    const ActorAssetRequest& request, ActorAssetOwner owner,
    ActorRecordMemory memory);
[[nodiscard]] bool LiveActorAssetsQueued(ActorAssetOwner owner);
[[nodiscard]] ActorAssetCompletion LiveActorAssetCompletion(
    ActorAssetTicket ticket);
void ForgetLiveActorAsset(ActorAssetTicket ticket);
// Between queue updates, removes the owner's requests and abandons its head;
// false, changing nothing, when the head cannot be abandoned.
[[nodiscard]] bool CancelLiveActorAssetRequests(ActorAssetOwner owner);
// The battle start-up's queue reset (INITBTL func_800FA7B0).
void ClearLiveActorAssetQueue();
// Adds the texture offsets to the UVs of `count` polygons in a game heap
// block; polygons running past the block are refused.
void OffsetLiveModelTextureCoordinates(void* polygons, int count, int colored,
                                       int u_offset, int v_offset);
void* CopyLiveModelGeometry(int* destination, void* source, int bytes);
int ReleaseLiveWeaponModel(int slot);
void CopyLiveWeaponSequence(SequenceBuffer* destination,
                            SequenceBuffer* source);
int ReleaseLiveActorSequence(int target, int kind);
int ReleaseLiveActorModel(int slot);
int ReleaseLiveSequenceEffectTexture(int bank);

// Inputs the loaders refused since start-up.
[[nodiscard]] std::uint64_t LiveActorAssetRefusals() noexcept;

// Native (#200): fills `destination` with the converted enemy archetype
// (models/zud, ASSET_PIPELINE.md §6.2.5) a ZUD read of `file` names, and
// whether it loaded; one that does not load or fit leaves zeros. Nothing when
// `file` is not an extracted MAP\*.ZUD, which the caller reads from the disc.
[[nodiscard]] std::optional<bool> ReadLiveConvertedArchetype(
    DiscFileExtent file, std::span<std::byte> destination);

// The battle asset queue's legacy globals. ActorAssetsLive.cpp defines them;
// clearing the queue also clears its per-entry parser state and snapshots.

// decomp: vs_battle_loadObjectDataState
// (src/BATTLE/BATTLE.PRG/objectData.h:19): the step of the menus' object-data
// reloads (MENU3.PRG _loadObjectData, the MENUC.PRG and MENU9.PRG model
// previews).
extern std::uint8_t objectDataLoadStep;
// decomp: D_800E8F29 (BATTLE/BATTLE.PRG src/BATTLE/BATTLE.PRG/30DB0.c:74): the
// parse phase of the model being parsed; 1 while its primitives wait for the
// second frame.
extern unsigned char modelParseResetFlag;
// Native (#492), not a module symbol: whether the model being parsed bound
// its record (ActorAssetLoadState::model_record_bound); cleared with the
// parse phase.
extern bool modelParseBoundRecord;
// The original's request profile (BATTLE/BATTLE.PRG 30DB0.c:176): the slowest
// request in VSync(1) scanlines while a model file parsed (D_800E8FC4), its
// load phase (D_800F457C), the last request's scanlines (D_800F4580) and the
// slowest kind (D_800F45D8). The native queue keeps no scanline profile
// (UpdateActorAssetQueue), so these keep their module-image values.
extern int assetQueuePeakUsage;
extern int lastLoadPhaseSnapshot;
extern int assetQueueFrameTicks;
extern int lastAssetRequestType;

// The loaders' legacy snapshots (BATTLE.PRG 30DB0.c,
// declared upstream at src/BATTLE/BATTLE.PRG/30DB0.c:73-105). The module image
// fills the three phases (ModuleData.cpp) and its symbol table names the
// rest (ModulePointers.cpp). Submitted requests own their file locations and
// generation-checked reads; these symbols only mirror the active entry.

// decomp: _loadShpState, upstream loadShpState
extern std::uint8_t legacyModelLoadPhase;
// decomp: D_800E8F2B
extern std::uint8_t legacySequenceParsePhase;
// decomp: _loadEtmState, upstream loadEtmState
extern std::uint8_t legacyEtmLoadPhase;
// decomp: _wepFileData, upstream wepFileData
extern void* legacyWeaponFileBuffer;
// decomp: _wepFileLoaded, upstream wepFileLoaded
extern std::int32_t legacyWeaponFileLoaded;
// decomp: _wepFile, upstream wepFile
extern DiscFile legacyWeaponFileLocation;
// decomp: _wepCdSlot, upstream wepCdSlot
extern DiscQueueSlot* legacyWeaponDiscRequest;
// decomp: _wepFileEpilog, upstream wepFileEpilog
extern void* legacyWeaponParseCursor;
// decomp: _shpData, upstream shpDataBuf
extern void* legacyShpFileBuffer;
// decomp: _shpAllocated, upstream shpAllocated
extern std::int32_t legacyShpAllocated;
// decomp: _shpFile, upstream shpFile
extern DiscFile legacyShpFileLocation;
// decomp: _shpCdSlot, upstream shpCdSlot
extern DiscQueueSlot* legacyShpDiscRequest;
// decomp: D_800F22B4
extern std::uint8_t* legacyShpParseCursor;
// decomp: D_800F22B8
extern std::uint8_t legacyShpCopiedWordCount;
// decomp: D_800F22BC
extern std::int32_t legacyShpHeaderWord;
// decomp: D_800F22C0
extern std::int32_t legacyShpTextureWidth;
// decomp: D_800F22C4
extern std::int32_t legacyShpTextureHeight;
// decomp: D_800F22C8
extern void* legacySequenceFileBuffer;
// decomp: D_800F22D0
extern std::int32_t legacySequenceBufferOwned;
// decomp: D_800F22D8
extern DiscFile legacySequenceFileLocation;
// decomp: D_800F22E0
extern DiscQueueSlot* legacySequenceDiscRequest;
// decomp: D_800F22E4
extern std::uint8_t* legacySequenceParseData;
// decomp: _etmData, upstream etmData
extern std::uint16_t* legacyEtmFileBuffer;
// decomp: _etmFile, upstream etmFile
extern DiscFile legacyEtmFileLocation;
// decomp: _etmFileCdSlot, upstream etmFileCdSlot
extern DiscQueueSlot* legacyEtmDiscRequest;

// The record arrays and disc file tables of BATTLE.PRG (upstream
// src/BATTLE/BATTLE.PRG/30DB0.c:59-72): per-model disc offsets and sector
// counts, sized to the module image's symbols. The module image fills the
// tables (ModuleData.cpp).
namespace legacy_files {
inline constexpr std::size_t kShpOffsets = 256;
inline constexpr std::size_t kShpSectors = 848;
inline constexpr std::size_t kSequenceOffsets = 168;
inline constexpr std::size_t kSequenceSectors = 112;
inline constexpr std::size_t kEtmOffsets = 112;
inline constexpr std::size_t kEtmSectors = 368;
inline constexpr std::size_t kWepOffsets = 128;
inline constexpr std::size_t kWepSectors = 296;
inline constexpr std::size_t kMaterialSequenceOffsets = 77;
inline constexpr std::size_t kMaterialSequenceSectors = 133;
inline constexpr std::size_t kMaterialModelOffsetRows = 16;
inline constexpr std::size_t kMaterialModelSectorRows = 11;
// The models per material in the material-model tables.
inline constexpr std::size_t kMaterialModels = 4;
inline constexpr std::size_t kWeaponModelBuffers = 84;

using Requests = std::array<LegacyActorAssetRequest, kActorAssetRequestCount>;
using WeaponModels = std::array<Actors::WeaponModelData*, kWeaponModelBuffers>;
using ShpOffsets = std::array<std::uint16_t, kShpOffsets>;
using ShpSectors = std::array<std::uint8_t, kShpSectors>;
using SequenceOffsets = std::array<std::uint16_t, kSequenceOffsets>;
using SequenceSectors = std::array<std::uint8_t, kSequenceSectors>;
using EtmOffsets = std::array<std::uint16_t, kEtmOffsets>;
using EtmSectors = std::array<std::uint8_t, kEtmSectors>;
using WepOffsets = std::array<std::uint16_t, kWepOffsets>;
using WepSectors = std::array<std::uint8_t, kWepSectors>;
using MaterialOffsets = std::array<std::uint16_t, kMaterialSequenceOffsets>;
using MaterialSectors = std::array<std::uint8_t, kMaterialSequenceSectors>;
// Indexed [material][model].
template <typename T, std::size_t Rows>
using ModelTable = std::array<std::array<T, kMaterialModels>, Rows>;
using ModelOffsets = ModelTable<std::uint16_t, kMaterialModelOffsetRows>;
using ModelSectors = ModelTable<std::uint8_t, kMaterialModelSectorRows>;
}  // namespace legacy_files

// decomp: vs_battle_objectDataSlots (src/BATTLE/BATTLE.PRG/2842C.h:19)
extern legacy_files::Requests legacyActorAssetRequests;
// decomp: vs_battle_commonWepBuffs (src/BATTLE/BATTLE.PRG/30DB0.h:40)
extern legacy_files::WeaponModels legacyWeaponModelBuffers;

// decomp: _shpLbaOffsets, upstream shpLbaOffsets
extern legacy_files::ShpOffsets legacyShpDiscOffsets;
// decomp: _shpFileSectorSizes, upstream shpFileSectorSizes
extern legacy_files::ShpSectors legacyShpSectorCounts;
// decomp: D_800E8A60
extern legacy_files::SequenceOffsets legacySequenceDiscOffsets;
// decomp: D_800E8B40
extern legacy_files::SequenceSectors legacySequenceSectorCounts;
// decomp: _etmLbaOffsets, upstream etmLbaOffsets
extern legacy_files::EtmOffsets legacyEtmDiscOffsets;
// decomp: _etmFileSectorSizes, upstream etmFileSectorSizes
extern legacy_files::EtmSectors legacyEtmSectorCounts;
// decomp: D_800E8D00
extern legacy_files::WepOffsets legacyWepDiscOffsets;
// decomp: _wepFileSectorOffsets, upstream wepFileSectorOffsets
extern legacy_files::WepSectors legacyWepSectorCounts;
// decomp: D_800E8E8E
extern legacy_files::MaterialOffsets legacyMaterialSequenceDiscOffsets;
// decomp: D_800E8EA3
extern legacy_files::MaterialSectors legacyMaterialSequenceSectorCounts;
// decomp: D_800E8EA8, indexed [material][model]
extern legacy_files::ModelOffsets legacyMaterialModelSequenceDiscOffsets;
// decomp: D_800E8EFC, indexed [material][model]
extern legacy_files::ModelSectors legacyMaterialModelSequenceSectorCounts;

}  // namespace VS::Content
