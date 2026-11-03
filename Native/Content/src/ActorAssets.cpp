// Hand-maintained (tools/port/owned.csv).
#include "Content/ActorAssets.hpp"  // NOLINT(build/include): typed API

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

#include "Core/Memory/WordCopy.hpp"

namespace VS::Content {
namespace {
// The model load phase while a file is parsing (_loadShpState 2).
constexpr std::uint8_t kParsingLoadPhase = 2;
// The loaders' phases.
constexpr std::uint8_t kPhaseStart = 0;
constexpr std::uint8_t kPhaseReading = 1;
constexpr std::uint8_t kPhaseParsing = 2;
constexpr std::uint8_t kParseFixPrimitives = 1;
constexpr std::int32_t kSectorShift = 11;

// The slot byte's sign bit and the range of a byte.
constexpr unsigned kSlotSignBit = 0x80U;
constexpr int kByteRange = 0x100;

// The request's target slot as a signed int; lb sign-extends the slot byte.
int TargetSlot(const ActorAssetRequest& request) {
  const auto byte = std::bit_cast<std::uint8_t>(request.target_slot);
  return byte < kSlotSignBit ? static_cast<int>(byte)
                             : static_cast<int>(byte) - kByteRange;
}

// The disc sectors the file tables are relative to (DiscLayout).
constexpr std::int32_t kWepFirstSector = 104024;
constexpr std::int32_t kShpFirstSector = 95000;
constexpr std::int32_t kEtmFirstSector = 104351;
constexpr std::int32_t kSequenceFirstSector = 0x19797;
constexpr std::int32_t kMaterialSequenceFirstSector = 0x18043;
constexpr std::int32_t kMaterialModelSequenceFirstSector = 0x1943E;

// Polygon records (func_80099E7C).
constexpr std::uint8_t kFlatTriangleCode = 0x24;
constexpr std::uint8_t kColoredTriangleCode = 0x34;
constexpr std::size_t kColoredCodeAt = 11;
constexpr std::size_t kFlatTriangleUvAt = 10;
constexpr std::size_t kFlatQuadUvAt = 12;
constexpr std::size_t kColoredTriangleFirstUvAt = 6;
constexpr std::size_t kColoredTriangleUvAt = 20;
constexpr std::size_t kColoredQuadUvAt = 24;
constexpr int kTriangleUvPairs = 3;
constexpr int kQuadUvPairs = 4;
constexpr int kColoredTriangleUvPairs = 2;
constexpr std::size_t kUvPairBytes = 2;

// The model header word holding the polygon and sound section offsets.
constexpr std::size_t kPolygonSectionAt = 0x3C;
constexpr std::size_t kSoundSectionAt = 0x30;
// sizeof(Actors_ShpModel): the header before the bones.
constexpr std::int32_t kShpModelHeaderBytes = 0x40;
constexpr std::size_t kShpBoneBytes = 16;
constexpr std::size_t kShpBoneTagAt = 8;
constexpr std::size_t kShpBoneJointInitAt = 10;
constexpr std::size_t kWeaponBoneBytes = 8;

constexpr std::size_t kPixelBytes = 2;
constexpr std::size_t kBitsPerByte = 8;
constexpr std::size_t kHalfWordBits = 16;

std::uint8_t ByteAt(std::span<const std::byte> bytes, std::size_t at) {
  return std::to_integer<std::uint8_t>(bytes[at]);
}

std::uint32_t ReadLittle(std::span<const std::byte> bytes, std::size_t at,
                         std::size_t width) {
  std::uint32_t value = 0;
  for (std::size_t i = 0; i < width; ++i) {
    value |= std::uint32_t{ByteAt(bytes, at + i)} << (kBitsPerByte * i);
  }
  return value;
}

void AddToByte(std::byte& value, int offset) {
  value = static_cast<std::byte>(
      static_cast<std::uint8_t>(std::to_integer<std::uint32_t>(value) +
                                static_cast<std::uint32_t>(offset)));
}

// Reads little-endian fields from file bytes the host resolves; a read past
// the bytes fails and stays failed.
class FileCursor {
 public:
  FileCursor(ActorAssetHost& host, AssetAddress at) : host_(host), at_(at) {}

  [[nodiscard]] AssetAddress At() const { return at_; }
  [[nodiscard]] bool Ok() const { return ok_; }
  void Seek(AssetAddress at) { at_ = at; }
  void Skip(std::ptrdiff_t bytes) { at_ = AdvanceAddress(at_, bytes); }

  std::span<const std::byte> Take(std::size_t bytes) {
    const std::span<const std::byte> view = host_.Bytes(at_, bytes);
    Skip(static_cast<std::ptrdiff_t>(bytes));
    if (view.size() < bytes) {
      ok_ = false;
      return {};
    }
    return view.first(bytes);
  }

  std::uint32_t U8() { return Read(1); }
  std::uint32_t U16() { return Read(sizeof(std::uint16_t)); }
  std::uint32_t U32() { return Read(sizeof(std::uint32_t)); }
  std::int32_t I32() {
    return static_cast<std::int32_t>(Read(sizeof(std::int32_t)));
  }

 private:
  std::uint32_t Read(std::size_t width) {
    const std::span<const std::byte> bytes = Take(width);
    return bytes.empty() ? 0 : ReadLittle(bytes, 0, width);
  }

  ActorAssetHost& host_;
  AssetAddress at_;
  bool ok_ = true;
};

// A memcpy the host bounds; false (refused) when either side is short.
bool CopyExact(ActorAssetHost& host, AssetAddress destination,
               AssetAddress source, std::size_t bytes, const char* what) {
  if (bytes == 0) {
    return true;
  }
  const std::span<std::byte> to = host.Bytes(destination, bytes);
  const std::span<std::byte> from = host.Bytes(source, bytes);
  const std::size_t copied = std::min({bytes, to.size(), from.size()});
  if (copied != 0) {
    std::memmove(to.data(), from.data(), copied);
  }
  if (copied < bytes) {
    host.Refuse(what, static_cast<std::int64_t>(bytes));
    return false;
  }
  return true;
}

// vs_main_memcpy: the word copy; false (refused) when it is short.
bool CopyWordsInto(ActorAssetHost& host, AssetAddress destination,
                   AssetAddress source, std::uint32_t size, const char* what) {
  const std::size_t wanted = Core::Memory::AlignedWordBytes(size);
  const std::size_t written = Core::Memory::CopyAlignedWords(
      host.Bytes(destination, wanted), host.Bytes(source, wanted), size);
  if (written < wanted) {
    host.Refuse(what, static_cast<std::int64_t>(size));
    return false;
  }
  return true;
}

// A copy of a fixed-size block from a source the original reads past its
// end: the destination must hold it; a short source copies what it holds.
void CopyFixedBlock(ActorAssetHost& host, AssetAddress destination,
                    AssetAddress source, std::size_t bytes, const char* what) {
  const std::span<std::byte> to = host.Bytes(destination, bytes);
  const std::span<std::byte> from = host.Bytes(source, bytes);
  if (to.size() < bytes || from.empty()) {
    host.Refuse(what, static_cast<std::int64_t>(bytes));
    return;
  }
  static_cast<void>(Core::Memory::CopyAlignedWords(
      to, from, static_cast<std::uint32_t>(bytes)));
}

// The original rounds a sequence size up with size + ((4 - size) & 3).
std::uint32_t RoundUpToWord(std::int32_t size) {
  constexpr std::uint32_t kWordBytes = 4;
  const auto value = static_cast<std::uint32_t>(size);
  return value + ((kWordBytes - value) & (kWordBytes - 1));
}

std::optional<std::uint32_t> PolygonSectionOffset(ActorAssetHost& host,
                                                  AssetAddress model) {
  const std::span<std::byte> header =
      host.Bytes(model, kPolygonSectionAt + sizeof(std::uint32_t));
  if (header.size() < kPolygonSectionAt + sizeof(std::uint32_t)) {
    return std::nullopt;
  }
  return ReadLittle(header, kPolygonSectionAt, sizeof(std::uint32_t));
}

bool UploadFrom(ActorAssetHost& host, const VramRect& rect, AssetAddress pixels,
                const char* what) {
  if (rect.w < 0 || rect.h < 0) {
    host.Refuse(what, rect.w);
    return false;
  }
  const std::size_t bytes = static_cast<std::size_t>(rect.w) *
                            static_cast<std::size_t>(rect.h) * kPixelBytes;
  const std::span<std::byte> view = host.Bytes(pixels, bytes);
  if (view.size() < bytes) {
    host.Refuse(what, static_cast<std::int64_t>(bytes));
    return false;
  }
  host.UploadImage(rect, view.first(bytes));
  return true;
}

// Bounded texture reference counts; the original's u8 counters wrap.
void AddTextureReference(ActorAssetHost& host, AssetTextureTable table,
                         int index, int delta) {
  const int count = table == AssetTextureTable::kActor ? kActorTextureSlots
                                                       : kWeaponTextureSlots;
  if (index < 0 || index >= count) {
    host.Refuse("texture slot", index);
    return;
  }
  AssetTextureSlot slot = host.TextureSlot(table, index);
  slot.references = static_cast<std::uint8_t>(slot.references + delta);
  host.SetTextureSlot(table, index, slot);
}

void SetTextureReferences(ActorAssetHost& host, AssetTextureTable table,
                          int index, std::uint8_t references) {
  const int count = table == AssetTextureTable::kActor ? kActorTextureSlots
                                                       : kWeaponTextureSlots;
  if (index < 0 || index >= count) {
    host.Refuse("texture slot", index);
    return;
  }
  AssetTextureSlot slot = host.TextureSlot(table, index);
  slot.references = references;
  host.SetTextureSlot(table, index, slot);
}

// Starts a loader's read, or reads it at once when the disc queue refused
// the request (#282).
void StartFileRead(ActorAssetHost& host, AssetFileStream stream,
                   DiscFileExtent file, AssetAddress destination) {
  if (!host.BeginFileRead(stream, file, destination)) {
    host.ReadRefusedFile(file, destination);
  }
}

bool IsDispatchedKind(std::uint8_t kind) {
  switch (static_cast<ActorAssetKind>(kind)) {
    case ActorAssetKind::kActorModelLoad:
    case ActorAssetKind::kActorModelParse:
    case ActorAssetKind::kWeaponModelLoad:
    case ActorAssetKind::kWeaponModelParse:
    case ActorAssetKind::kGimmick:
    case ActorAssetKind::kSequenceLoad:
    case ActorAssetKind::kSequenceParse:
    case ActorAssetKind::kEffectTextureLoad:
      return true;
    default:
      return false;
  }
}
}  // namespace

// decomp: BATTLE.PRG vs_battle_getEmptyObjectDataSlot
// (src/BATTLE/BATTLE.PRG/30DB0.c:149)
int FindFreeActorAssetRequest(const ActorAssetHost& host) {
  for (int i = 0; i < kActorAssetRequestCount; ++i) {
    if (host.RequestKind(i) == 0) {
      return i;
    }
  }
  return -1;
}

// decomp: BATTLE.PRG vs_battle_populateDataSlot
// (src/BATTLE/BATTLE.PRG/30DB0.c:162)
int QueueActorAssetRequest(ActorAssetHost& host,
                           const ActorAssetRequest& request) {
  const int slot = FindFreeActorAssetRequest(host);
  if (slot == -1) {
    return -1;
  }
  host.OnRequestQueued(request);
  host.StoreRequest(slot, request);
  return slot;
}

// decomp: BATTLE.PRG vs_battle_processObjectDataQueue
// (src/BATTLE/BATTLE.PRG/30DB0.c:176)
int UpdateActorAssetQueue(ActorAssetHost& host, ActorAssetQueueStats& stats) {
  stats.frame_ticks = host.FrameCounter();
  const std::uint8_t kind = host.RequestKind(0);
  if (kind == 0) {
    return 0;
  }
  if (!IsDispatchedKind(kind)) {
    return -1;
  }
  const int result = host.ProcessHeadRequest(static_cast<ActorAssetKind>(kind));
  stats.frame_ticks = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(host.FrameCounter()) -
      static_cast<std::uint32_t>(stats.frame_ticks));
  if (host.ModelLoadPhase() == kParsingLoadPhase &&
      stats.peak_frame_ticks < stats.frame_ticks) {
    stats.peak_frame_ticks = stats.frame_ticks;
    stats.last_kind = host.RequestKind(0);
    stats.last_load_phase = host.ModelLoadPhase();
  }
  if (result == kAssetPending) {
    return 1;
  }
  host.OnHeadRequestDone(result);
  // The original shifts slots 1..14 down and clears slot 14; slot 15 never
  // moves.
  constexpr int kLastShifted = kActorAssetRequestCount - 2;
  for (int i = 1; i <= kLastShifted; ++i) {
    host.MoveRequest(i, i - 1);
  }
  host.ClearRequestKind(kLastShifted);
  return result < 0 ? -1 : 1;
}

// decomp: BATTLE.PRG func_80099854 (src/BATTLE/BATTLE.PRG/30DB0.c:239)
void QueueAshleyWeaponSequence(ActorAssetHost& host, int sequence_id) {
  constexpr std::uint8_t kMaterialSequence = 3;
  if (sequence_id == 0) {
    return;
  }
  // The original leaves the other fields as stack bytes; they are zero here.
  ActorAssetRequest request;
  request.kind = static_cast<std::uint8_t>(ActorAssetKind::kSequenceLoad);
  request.target_slot = 0;
  request.sub_kind = kMaterialSequence;
  request.material = static_cast<std::uint8_t>(sequence_id);
  const int slot = FindFreeActorAssetRequest(host);
  if (slot != -1) {
    host.StoreRequest(slot, request);
  }
}

// decomp: BATTLE.PRG func_80099E7C (src/BATTLE/BATTLE.PRG/30DB0.c:445)
bool OffsetModelTextureCoordinates(std::span<std::byte> polygons, int count,
                                   bool colored, int u_offset, int v_offset) {
  std::size_t at = 0;
  for (int polygon = 0; polygon < count; ++polygon) {
    std::size_t uv_at = 0;
    std::size_t record_bytes = 0;
    int pairs = 0;
    const std::size_t code_at = colored ? kColoredCodeAt : 0;
    if (polygons.size() <= at + code_at) {
      return false;
    }
    const std::uint8_t code = ByteAt(polygons, at + code_at);
    if (!colored) {
      const bool triangle = code == kFlatTriangleCode;
      uv_at = triangle ? kFlatTriangleUvAt : kFlatQuadUvAt;
      pairs = triangle ? kTriangleUvPairs : kQuadUvPairs;
    } else {
      const bool triangle = code == kColoredTriangleCode;
      uv_at = triangle ? kColoredTriangleUvAt : kColoredQuadUvAt;
      pairs = triangle ? kColoredTriangleUvPairs : kQuadUvPairs;
    }
    record_bytes = uv_at + static_cast<std::size_t>(pairs) * kUvPairBytes;
    if (polygons.size() - at < record_bytes) {
      return false;
    }
    if (colored && code == kColoredTriangleCode) {
      AddToByte(polygons[at + kColoredTriangleFirstUvAt], u_offset);
      AddToByte(polygons[at + kColoredTriangleFirstUvAt + 1], v_offset);
    }
    for (int uv_pair = 0; uv_pair < pairs; ++uv_pair) {
      const std::size_t uv =
          at + uv_at + static_cast<std::size_t>(uv_pair) * kUvPairBytes;
      AddToByte(polygons[uv], u_offset);
      AddToByte(polygons[uv + 1], v_offset);
    }
    at += record_bytes;
  }
  return true;
}

// decomp: BATTLE.PRG func_8009A028 (src/BATTLE/BATTLE.PRG/30DB0.c:507)
AssetAddress CopyModelGeometry(ActorAssetHost& host, AssetAddress destination,
                               AssetAddress source, std::int32_t bytes) {
  if (bytes < 0) {
    host.Refuse("model geometry size", bytes);
    return AssetAddress::kNone;
  }
  const auto size = static_cast<std::size_t>(bytes);
  if (destination == AssetAddress::kNone) {
    destination = host.AllocateLow(size);
    if (destination == AssetAddress::kNone) {
      host.Refuse("model geometry block", bytes);
      return AssetAddress::kNone;
    }
  }
  static_cast<void>(
      CopyExact(host, destination, source, size, "model geometry copy"));
  return destination;
}

// decomp: BATTLE.PRG func_80099FA8 (src/BATTLE/BATTLE.PRG/30DB0.c:489)
int FindSharedWeaponModel(const ActorAssetHost& host,
                          ActorAssetRequest& request) {
  const int target = TargetSlot(request);
  constexpr int kSharedAnyOwnerSlots = 4;
  for (int i = 0; i < kWeaponModelSlots; ++i) {
    if (i == target) {
      continue;
    }
    const WeaponAssetSummary model = host.WeaponSummary(i);
    if (model.present &&
        (model.actor_id != 0 || target < kSharedAnyOwnerSlots) &&
        int{model.model_id} == int{request.model_id}) {
      request.data = SharedAssetAddress(i);
      return 1;
    }
  }
  return 0;
}

namespace {
constexpr std::int16_t kFirstInvalidWeaponModel = 0x80;
// Weapon slots 0 and 1 parse into fixed geometry buffers; slots below 4
// copy a shared model's geometry instead of pointing at it.
constexpr int kFixedGeometrySlots = 2;
constexpr int kCopiedGeometrySlots = 4;
constexpr std::ptrdiff_t kFileHeaderBytes = 4;
constexpr std::ptrdiff_t kWeaponTextureWidthAt = 5;
constexpr std::uint32_t kWideWeaponTextureWidth = 64;
constexpr std::uint32_t kDoubleClutMarker = 96;
constexpr int kWeaponPalettes = 7;
constexpr std::size_t kDoubleClutWorkingColors = 32;
constexpr std::size_t kSingleClutWorkingColors = 16;
constexpr std::size_t kDoubleClutColors = 64;
constexpr std::size_t kSingleClutColors = 32;
constexpr std::uint8_t kDoubleClutPaletteColors = 96;
constexpr std::uint8_t kSingleClutPaletteColors = 48;
constexpr std::int16_t kWeaponTextureY = 0xE0;
constexpr std::int16_t kWeaponTextureHeight = 32;
constexpr int kWeaponTexturePairWidth = 64;
constexpr int kWeaponTextureHalfWidth = 32;
constexpr int kWeaponPaletteSlotBase = 16;
constexpr std::uint8_t kWeaponReserved11 = 64;
constexpr std::uint8_t kActorIdMask = 0xF;
constexpr std::uint8_t kMaterialMask = 0xF;
constexpr std::uint32_t kWeaponClutFirstRow = 4;
constexpr std::uint32_t kWeaponClutFirstColumn = 160;
constexpr std::uint32_t kWeaponClutHalfColumns = 48;

int WaitForFileRead(ActorAssetHost& host, std::uint8_t& phase,
                    AssetFileStream stream) {
  if (!host.FileReadFinished(stream)) {
    return kAssetPending;
  }
  host.ReleaseFileRead(stream);
  phase = kPhaseParsing;
  return kAssetPending;
}

std::uint8_t WeaponPaletteSlotOffset(const ActorAssetRequest& request) {
  return static_cast<std::uint8_t>(
      request.texture_slot + (request.sub_kind << 1U) + kWeaponPaletteSlotBase);
}

bool ValidWeaponSlot(ActorAssetHost& host, int slot) {
  if (slot < 0 || slot >= kWeaponModelSlots) {
    host.Refuse("weapon slot", slot);
    return false;
  }
  return true;
}

// Reads `bytes` little-endian halfwords from the file into `colors`.
bool ReadColors(FileCursor& cursor, std::span<std::uint16_t> colors,
                std::size_t count) {
  const std::span<const std::byte> bytes =
      cursor.Take(count * sizeof(std::uint16_t));
  if (bytes.empty() || count > colors.size()) {
    return false;
  }
  for (std::size_t i = 0; i < count; ++i) {
    colors[i] = static_cast<std::uint16_t>(
        ReadLittle(bytes, i * sizeof(std::uint16_t), sizeof(std::uint16_t)));
  }
  return true;
}

int StartWeaponFileRead(ActorAssetHost& host, ActorAssetLoadState& state,
                        ActorAssetRequest& request) {
  const auto index = static_cast<std::size_t>(request.model_id);
  const std::optional<std::uint32_t> sectors =
      host.FileTableEntry(AssetFileTable::kWepSectors, index);
  const std::optional<std::uint32_t> offset =
      host.FileTableEntry(AssetFileTable::kWepOffsets, index);
  if (!sectors || !offset) {
    host.Refuse("weapon file table", request.model_id);
    return kAssetRefused;
  }
  const auto bytes = static_cast<std::int32_t>(*sectors << kSectorShift);
  state.weapon_file = host.AllocateHigh(static_cast<std::size_t>(bytes));
  request.data = state.weapon_file;
  if (state.weapon_file == AssetAddress::kNone) {
    return kAssetRefused;
  }
  state.model_phase = kPhaseReading;
  StartFileRead(host, AssetFileStream::kWeapon,
                {static_cast<std::int32_t>(*offset) + kWepFirstSector, bytes},
                request.data);
  return kAssetPending;
}

// The fields every parsed or copied weapon ends with (30DB0.c:840-852).
int FinishWeaponModel(ActorAssetHost& host, ActorAssetLoadState& state,
                      const ActorAssetRequest& request) {
  const int target = TargetSlot(request);
  static_cast<void>(LoadWeaponMaterialPalette(host, target, request.material));
  WeaponAssetRecord model = host.LoadWeapon(target);
  model.reserved11 = kWeaponReserved11;
  model.reserved08_bit4 = false;
  model.reserved08_bit6 = false;
  model.reserved13 = 0;
  model.actor_id = request.sub_kind & kActorIdMask;
  model.attachment_slot = request.texture_slot;
  host.StoreWeapon(target, model);
  const int bone =
      host.FindActorBoneTag(request.sub_kind, request.texture_slot);
  model = host.LoadWeapon(target);
  model.attachment_bone = static_cast<std::uint8_t>(bone);
  host.StoreWeapon(target, model);
  state.parse_phase = 0;
  return kAssetDone;
}

int CopySharedWeapon(ActorAssetHost& host, ActorAssetLoadState& state,
                     const ActorAssetRequest& request, int source) {
  const int target = TargetSlot(request);
  if (!host.WeaponSummary(source).present) {
    host.Refuse("shared weapon record", source);
    return kAssetRefused;
  }
  host.CopyWeaponRecord(target, source);
  WeaponAssetRecord model = host.LoadWeapon(target);
  if (model.texture_slot != kNoAssetTextureSlot) {
    AddTextureReference(host, AssetTextureTable::kWeapon, model.texture_slot,
                        1);
    if (model.wide_texture) {
      AddTextureReference(host, AssetTextureTable::kWeapon,
                          model.texture_slot + 1, 1);
    }
  }
  if (target < kCopiedGeometrySlots) {
    const AssetAddress original = model.geometry;
    model.geometry = target < kFixedGeometrySlots
                         ? host.WeaponGeometryBuffer(target)
                         : host.AllocateLow(kWeaponGeometryBytes);
    if (model.geometry == AssetAddress::kNone) {
      host.StoreWeapon(target, model);
      host.Refuse("weapon geometry block", target);
      return kAssetRefused;
    }
    CopyFixedBlock(host, model.geometry, original, kWeaponGeometryBytes,
                   "weapon geometry copy");
    host.RecordModelCopy(model.geometry, original, kWeaponGeometryBytes);
  }
  model.palette_slot_offset = WeaponPaletteSlotOffset(request);
  host.StoreWeapon(target, model);
  return FinishWeaponModel(host, state, request);
}

// The first free weapon texture slot, or the first free pair for a wide
// texture, marked used.
std::optional<int> ReserveWeaponTextureSlots(ActorAssetHost& host, bool wide) {
  const int step = wide ? 2 : 1;
  for (int i = 0; i + step <= kWeaponTextureSlots; i += step) {
    bool free = true;
    for (int j = 0; j < step; ++j) {
      free =
          free &&
          host.TextureSlot(AssetTextureTable::kWeapon, i + j).references == 0;
    }
    if (free) {
      for (int j = 0; j < step; ++j) {
        SetTextureReferences(host, AssetTextureTable::kWeapon, i + j, 1);
      }
      return i;
    }
  }
  return std::nullopt;
}

// The palettes and texture image after the geometry (30DB0.c:740-800).
int ParseWeaponTexture(ActorAssetHost& host, ActorAssetLoadState& state,
                       const ActorAssetRequest& request, FileCursor& cursor,
                       WeaponAssetRecord& model) {
  const int target = TargetSlot(request);
  const std::uint32_t texture_width = cursor.U8() * 2U;
  cursor.Skip(1);
  const std::uint32_t marker = cursor.U8();
  if (!cursor.Ok()) {
    host.StoreWeapon(target, model);
    host.Refuse("weapon file", target);
    return kAssetRefused;
  }
  model.wide_texture = texture_width > kWideWeaponTextureWidth;
  const std::optional<int> slot =
      ReserveWeaponTextureSlots(host, model.wide_texture);
  if (!slot) {
    host.StoreWeapon(target, model);
    return kAssetRefused;
  }
  model.texture_slot = static_cast<std::uint8_t>(*slot);
  model.palette_slot_offset = WeaponPaletteSlotOffset(request);
  model.double_clut = marker == kDoubleClutMarker;
  model.palette_color_count =
      model.double_clut ? kDoubleClutPaletteColors : kSingleClutPaletteColors;
  bool read = ReadColors(
      cursor, model.working_palette,
      model.double_clut ? kDoubleClutWorkingColors : kSingleClutWorkingColors);
  for (int i = 0; i < kWeaponPalettes; ++i) {
    read =
        ReadColors(cursor, model.palettes[static_cast<std::size_t>(i)],
                   model.double_clut ? kDoubleClutColors : kSingleClutColors) &&
        read;
  }
  const int ts = model.texture_slot;
  const VramRect rect{
      .x = static_cast<std::int16_t>((ts / 2) * kWeaponTexturePairWidth +
                                     (ts & 1) * kWeaponTextureHalfWidth),
      .y = kWeaponTextureY,
      .w = static_cast<std::int16_t>(texture_width / 2U),
      .h = kWeaponTextureHeight};
  if (!read || !UploadFrom(host, rect, cursor.At(), "weapon texture")) {
    host.StoreWeapon(target, model);
    host.Refuse("weapon file", target);
    return kAssetRefused;
  }
  cursor.Skip(static_cast<std::ptrdiff_t>(texture_width) *
              kWeaponTextureHeight);
  state.parse_phase = kParseFixPrimitives;
  state.weapon_cursor = cursor.At();
  host.StoreWeapon(target, model);
  return kAssetPending;
}

int ParseWeaponFile(ActorAssetHost& host, ActorAssetLoadState& state,
                    const ActorAssetRequest& request,
                    WeaponAssetRecord& model) {
  const int target = TargetSlot(request);
  FileCursor cursor(host, request.data);
  cursor.Skip(kFileHeaderBytes);
  model.bone_count = static_cast<std::uint8_t>(cursor.U8());
  model.vertex_group_count = static_cast<std::uint8_t>(cursor.U8());
  model.triangle_count = static_cast<std::uint16_t>(cursor.U16());
  model.quad_count = static_cast<std::uint16_t>(cursor.U16());
  model.other_primitive_count = static_cast<std::uint16_t>(cursor.U16());
  const std::int32_t size = cursor.I32();
  if (!cursor.Ok() || size < 0) {
    host.StoreWeapon(target, model);
    host.Refuse("weapon geometry size", size);
    return kAssetRefused;
  }
  const AssetAddress fixed = target < kFixedGeometrySlots
                                 ? host.WeaponGeometryBuffer(target)
                                 : AssetAddress::kNone;
  model.geometry = fixed != AssetAddress::kNone
                       ? fixed
                       : host.AllocateLow(static_cast<std::size_t>(size));
  if (model.geometry == AssetAddress::kNone) {
    host.StoreWeapon(target, model);
    return kAssetRefused;
  }
  const auto bytes = static_cast<std::size_t>(size);
  const bool copied = CopyExact(host, model.geometry, cursor.At(), bytes,
                                "weapon geometry copy");
  host.RecordModelGeometry(model.geometry,
                           fixed != AssetAddress::kNone
                               ? std::min(bytes, kWeaponGeometryBytes)
                               : bytes);
  if (!copied) {
    host.StoreWeapon(target, model);
    return kAssetRefused;
  }
  cursor.Skip(size + kWeaponTextureWidthAt);
  return ParseWeaponTexture(host, state, request, cursor, model);
}

// The second frame: offsets the UVs into the texture slot and copies the
// bones (30DB0.c:805-838).
int FinishWeaponPrimitives(ActorAssetHost& host, ActorAssetLoadState& state,
                           const ActorAssetRequest& request) {
  const int target = TargetSlot(request);
  const WeaponAssetRecord model = host.LoadWeapon(target);
  const int count =
      model.other_primitive_count + model.triangle_count + model.quad_count;
  const std::optional<std::uint32_t> section =
      PolygonSectionOffset(host, model.geometry);
  const std::size_t bone_bytes =
      std::size_t{model.bone_count} * kWeaponBoneBytes;
  const std::span<std::byte> bones =
      host.Bytes(state.weapon_cursor, bone_bytes);
  const bool fixed =
      section.has_value() &&
      OffsetModelTextureCoordinates(
          host.Bytes(AdvanceAddress(model.geometry, *section), 0), count, false,
          (model.texture_slot & 1) * kWeaponTexturePairWidth, kWeaponTextureY);
  if (!fixed || bones.size() < bone_bytes) {
    host.Refuse("weapon primitives", target);
    state.parse_phase = 0;
    return kAssetRefused;
  }
  host.WriteWeaponBones(target, bones.first(bone_bytes));
  return FinishWeaponModel(host, state, request);
}
}  // namespace

// decomp: BATTLE.PRG _loadWep (src/BATTLE/BATTLE.PRG/30DB0.c:523)
int UpdateWeaponModelLoad(ActorAssetHost& host, ActorAssetLoadState& state,
                          ActorAssetRequest& request) {
  if (request.model_id == 0 || request.model_id >= kFirstInvalidWeaponModel) {
    return kAssetDone;
  }
  switch (state.model_phase) {
    case kPhaseStart:
      if (FindSharedWeaponModel(host, request) != 0) {
        state.weapon_file_loaded = false;
        break;
      }
      state.weapon_file_loaded = true;
      [[fallthrough]];
    default:
      if (const int started = StartWeaponFileRead(host, state, request);
          started != kAssetPending) {
        return started;
      }
      [[fallthrough]];
    case kPhaseReading:
      return WaitForFileRead(host, state.model_phase, AssetFileStream::kWeapon);
    case kPhaseParsing:
      break;
  }
  const int result = ParseWeaponModel(host, state, request);
  if (result == kAssetPending) {
    return kAssetPending;
  }
  if (state.weapon_file_loaded) {
    state.weapon_file_loaded = false;
    host.FreeHigh(state.weapon_file);
  }
  state.model_phase = kPhaseStart;
  return result;
}

// decomp: BATTLE.PRG _parseWep (src/BATTLE/BATTLE.PRG/30DB0.c:598)
int ParseWeaponModel(ActorAssetHost& host, ActorAssetLoadState& state,
                     ActorAssetRequest& request) {
  const int target = TargetSlot(request);
  if (!ValidWeaponSlot(host, target)) {
    return kAssetRefused;
  }
  if (state.parse_phase == kParseFixPrimitives) {
    if (!host.WeaponSummary(target).present) {
      host.Refuse("weapon record", target);
      return kAssetRefused;
    }
    return FinishWeaponPrimitives(host, state, request);
  }
  if (state.parse_phase != 0) {
    host.Refuse("weapon parse phase", state.parse_phase);
    return kAssetRefused;
  }
  if (!host.WeaponSummary(target).present &&
      !host.BindWeaponRecord(target, request.destination)) {
    return kAssetRefused;
  }
  WeaponAssetRecord model = host.LoadWeapon(target);
  if (model.bone_count != 0) {
    return kAssetBusy;
  }
  std::optional<int> source = SharedAssetSlot(request.data, kWeaponModelSlots);
  if (!source && FindSharedWeaponModel(host, request) != 0) {
    source = SharedAssetSlot(request.data, kWeaponModelSlots);
  }
  if (source) {
    return CopySharedWeapon(host, state, request, *source);
  }
  model.model_id = static_cast<std::uint8_t>(request.model_id);
  return ParseWeaponFile(host, state, request, model);
}

// decomp: BATTLE.PRG _loadMaterialPalette (src/BATTLE/BATTLE.PRG/30DB0.c:855)
bool LoadWeaponMaterialPalette(ActorAssetHost& host, int slot, int material) {
  if (!ValidWeaponSlot(host, slot) || !host.WeaponSummary(slot).present) {
    host.Refuse("weapon palette record", slot);
    return false;
  }
  WeaponAssetRecord model = host.LoadWeapon(slot);
  model.material = static_cast<std::uint8_t>(material) & kMaterialMask;
  const std::span<const std::uint8_t> palettes = host.WeaponMaterialPalettes();
  if (material < 0 || static_cast<std::size_t>(material) >= palettes.size() ||
      palettes[static_cast<std::size_t>(material)] >= kWeaponPalettes) {
    host.StoreWeapon(slot, model);
    host.Refuse("weapon material palette", material);
    return false;
  }
  const std::size_t palette = palettes[static_cast<std::size_t>(material)];
  const std::size_t offset =
      model.double_clut ? kDoubleClutWorkingColors : kSingleClutWorkingColors;
  const std::size_t count =
      model.double_clut ? kDoubleClutColors : kSingleClutColors;
  std::copy_n(
      model.palettes[palette].begin(), count,
      model.working_palette.begin() + static_cast<std::ptrdiff_t>(offset));
  model.upload_palette = model.working_palette;
  host.StoreWeapon(slot, model);
  host.UploadClut(model.upload_palette,
                  (model.palette_slot_offset >> 1U) + kWeaponClutFirstRow,
                  ((model.palette_slot_offset & 1U) * kWeaponClutHalfColumns) +
                      kWeaponClutFirstColumn,
                  static_cast<std::uint32_t>(offset + count));
  return true;
}

// decomp: BATTLE.PRG func_8009AA84 (src/BATTLE/BATTLE.PRG/30DB0.c:882)
int ReleaseWeaponModel(ActorAssetHost& host, int slot) {
  if (!ValidWeaponSlot(host, slot)) {
    return -1;
  }
  const WeaponAssetSummary summary = host.WeaponSummary(slot);
  if (!summary.present || summary.bone_count == 0) {
    return -1;
  }
  const WeaponAssetRecord model = host.LoadWeapon(slot);
  if (model.actor_id != 0) {
    bool shared = false;
    if (slot >= kCopiedGeometrySlots) {
      ActorAssetRequest probe;
      probe.target_slot = static_cast<std::int8_t>(slot);
      probe.model_id = model.model_id;
      shared = FindSharedWeaponModel(host, probe) != 0;
    }
    // Weapon slots 0 and 1 are Ashley's (actor 0) and never reach here; a
    // slot 0 or 1 record of another actor would free its fixed buffer, as
    // the original does.
    if (!shared) {
      host.ForgetModelGeometry(model.geometry);
      host.FreeLow(model.geometry);
    }
  }
  AddTextureReference(host, AssetTextureTable::kWeapon, model.texture_slot, -1);
  if (model.wide_texture) {
    AddTextureReference(host, AssetTextureTable::kWeapon,
                        model.texture_slot + 1, -1);
  }
  host.ClearWeaponSlot(slot);
  return 0;
}

namespace {
constexpr std::int16_t kAshleyModelId = 6;
constexpr std::int32_t kAshleyTextureSize = 0x80;
// The SHP header before the model size: magic, two header words and the
// texture layout.
constexpr std::ptrdiff_t kShpModelSizeAt = 0xF4;
constexpr std::ptrdiff_t kAshleyPaletteSkip = 8;
constexpr std::uint16_t kModellessRadius = 0x40;
constexpr std::uint16_t kModellessHeight = 10000;
constexpr int kModellessPlacementMode = 1;
// A texture size of 4 borrows another actor's texture (30DB0.c:1136).
constexpr std::int32_t kBorrowedTextureMarker = 4;
constexpr std::uint32_t kWideActorTextureWidth = 0x100;
constexpr std::uint32_t kActorTextureSlotHeight = 64;
constexpr int kWideActorTextureSlots = 8;
constexpr std::uint8_t kSlotCountMask = 7;
constexpr int kCompanionTextureSlotsFirst = 12;
constexpr int kCompanionTextureSlotsEnd = 20;
constexpr int kWideTextureSlotsFirst = 4;
constexpr int kActorTextureSlotsEnd = 12;
constexpr int kSmallTextureLimit = 3;
constexpr std::uint32_t kFallbackTexturePage = 31;
constexpr int kFallbackTextureSlot = 20;
constexpr int kTextureSlotGroupMask = 3;
constexpr std::uint16_t kTexturePageHeaderBit = 0x80;
constexpr std::uint16_t kTexturePageClearBits = 0x180;
constexpr std::size_t kTextureOriginBytes = 4;
constexpr int kTextureOriginXScale = 64;
constexpr int kTextureOriginYScale = 256;
constexpr int kTextureRowScale = 64;
constexpr std::size_t kPaletteWordBytes = 4;
constexpr std::uint8_t kHeaderBitPaletteWords = 0xA0;
constexpr std::uint8_t kPlainPaletteWords = 0x10;
constexpr std::size_t kPaletteVariantBytes = 0x140;
constexpr std::size_t kBasePaletteCopyAt = 0x140;
constexpr int kCompanionSourceSlot = 1;
constexpr int kLastActorSlot = kActorRecordSlots - 1;
constexpr std::uint32_t kLastActorClutRow = 0x17;
constexpr std::uint32_t kActorClutFirstRow = 4;
constexpr std::uint32_t kActorAlternateClutFirstRow = 0x16;
constexpr std::uint8_t kInitialTravelAnimation = 13;
constexpr std::uint16_t kInitialMoveRate = 8;
constexpr std::uint16_t kInitialTurnRate = 0x20;
constexpr std::int16_t kUnitScale = 4096;
constexpr std::uint16_t kInitialOpaque648 = 800;
constexpr std::uint32_t kInitialBaseColor = 0x808080;
constexpr std::uint32_t kInitialTint = 0x404040;
constexpr std::uint8_t kFirstJointTag = 3;
constexpr std::uint8_t kJointTagEnd = 7;
constexpr int kBoundSharedBanks = 3;
constexpr std::uint8_t kPlaceInRoom = 0xFF;
constexpr std::uint8_t kCompanionAttachment = 0xFC;
constexpr std::uint8_t kNoAttachParent = 0xFF;
constexpr std::uint8_t kCompanionDepthBias = 8;
constexpr int kWeaponOwnerSlots = 10;
constexpr std::uint8_t kEmbeddedSequenceModel = 0xFF;

bool ValidActorSlot(ActorAssetHost& host, int slot) {
  if (slot < 0 || slot >= kActorRecordSlots) {
    host.Refuse("actor slot", slot);
    return false;
  }
  return true;
}

int RefuseActor(ActorAssetHost& host, const char* what, std::int64_t value) {
  host.Refuse(what, value);
  return kAssetRefused;
}

// The VRAM rectangle of actor texture slot `slot` (D_800E8F30, D_800E8F90).
std::optional<VramRect> ActorTextureRect(const ActorAssetHost& host, int slot,
                                         std::int32_t width,
                                         std::int32_t height) {
  const std::span<const std::uint8_t> origins = host.TextureSlotOrigins();
  const std::span<const std::uint8_t> rows = host.TextureSlotRowOffsets();
  const auto at = static_cast<std::size_t>(slot) * kTextureOriginBytes;
  if (slot < 0 || at + kTextureOriginBytes > origins.size() ||
      static_cast<std::size_t>(slot) >= rows.size()) {
    return std::nullopt;
  }
  const auto read_short = [&](std::size_t offset) {
    return static_cast<std::int16_t>(origins[offset] |
                                     (origins[offset + 1] << kBitsPerByte));
  };
  const int x = read_short(at) * kTextureOriginXScale;
  const int y = (read_short(at + 2) * kTextureOriginYScale) +
                (rows[static_cast<std::size_t>(slot)] * kTextureRowScale);
  return VramRect{.x = static_cast<std::int16_t>(x),
                  .y = static_cast<std::int16_t>(y),
                  .w = static_cast<std::int16_t>(width / 2),
                  .h = static_cast<std::int16_t>(height)};
}

// Copies `words` palette words from the file into the actor's palettes.
bool ReadActorPalettes(FileCursor& cursor, ActorAssetRecord& actor,
                       std::size_t words) {
  const std::size_t bytes = words * kPaletteWordBytes;
  const std::span<const std::byte> source = cursor.Take(bytes);
  if (!cursor.Ok() || bytes > actor.palettes.size()) {
    return false;
  }
  std::copy_n(source.begin(), bytes, actor.palettes.begin());
  return true;
}

int StartActorModelFileRead(ActorAssetHost& host, ActorAssetLoadState& state,
                            ActorAssetRequest& request) {
  std::optional<std::uint32_t> sectors = host.FileTableEntry(
      AssetFileTable::kShpSectors, static_cast<std::size_t>(request.model_id));
  if (sectors && *sectors == 0) {
    request.model_id = 0;
    sectors = host.FileTableEntry(AssetFileTable::kShpSectors, 0);
  }
  const std::optional<std::uint32_t> offset = host.FileTableEntry(
      AssetFileTable::kShpOffsets, static_cast<std::size_t>(request.model_id));
  if (!sectors || !offset) {
    return RefuseActor(host, "actor model file table", request.model_id);
  }
  const auto bytes = static_cast<std::int32_t>(*sectors << kSectorShift);
  state.model_file = host.AllocateHigh(static_cast<std::size_t>(bytes));
  request.data = state.model_file;
  if (state.model_file == AssetAddress::kNone) {
    return kAssetRefused;
  }
  state.model_phase = kPhaseReading;
  StartFileRead(host, AssetFileStream::kActorModel,
                {static_cast<std::int32_t>(*offset) + kShpFirstSector, bytes},
                request.data);
  return kAssetPending;
}

// The joint slots each tagged bone starts (30DB0.c:1352-1360).
void ResetActorJoints(ActorAssetHost& host, ActorAssetRecord& actor) {
  const std::size_t bytes = std::size_t{actor.bone_count} * kShpBoneBytes;
  const std::span<std::byte> bones =
      host.Bytes(AdvanceAddress(actor.model, kShpModelHeaderBytes), bytes);
  if (bones.size() < bytes) {
    host.Refuse("actor bones", actor.bone_count);
    return;
  }
  for (std::size_t i = 0; i < actor.bone_count; ++i) {
    const std::size_t bone = i * kShpBoneBytes;
    const std::uint8_t tag = ByteAt(bones, bone + kShpBoneTagAt);
    if (tag >= kFirstJointTag && tag < kJointTagEnd) {
      const auto joint = static_cast<std::size_t>(tag - kFirstJointTag) * 2U;
      actor.joint_slots[joint] = static_cast<std::uint16_t>(
          ReadLittle(bones, bone + kShpBoneJointInitAt, sizeof(std::uint16_t)));
      actor.joint_slots[joint + 1] = 0;
    }
  }
}

// The record fields a newly parsed actor starts with (30DB0.c:1318-1373).
void ResetActorRecordFields(ActorAssetHost& host, ActorAssetRecord& actor,
                            const ActorAssetRequest& request) {
  actor.slot = static_cast<std::uint8_t>(request.target_slot);
  actor.variant = request.variant;
  actor.is_parsed = true;
  actor.action_animation_state = 0;
  actor.travel_animation = kInitialTravelAnimation;
  actor.common_sequence = AssetAddress::kNone;
  actor.battle_sequence = AssetAddress::kNone;
  actor.shared_sequences[kSpecialSequenceRef] = AssetAddress::kNone;
  actor.sequence_flags = 0;
  actor.common_animation_count = 0;
  actor.battle_animation_count = 0;
  actor.locomotion_counter = 0;
  actor.sequence_model_id = 0;
  actor.animation_length = 0;
  actor.opaque6dc = 0;
  actor.weapon_drawn = false;
  actor.advance_requested = false;
  actor.custom_tint = false;
  actor.move_rate = kInitialMoveRate;
  actor.turn_rate = kInitialTurnRate;
  actor.scale_x = kUnitScale;
  actor.scale_y = kUnitScale;
  actor.scale_z = kUnitScale;
  actor.frames_per_step = 1;
  actor.support_layer = 0;
  actor.placement_layer = 0;
  actor.ignored_by_collision = false;
  actor.sound_enabled = true;
  actor.fade = 0;
  actor.opaque648 = kInitialOpaque648;
  actor.base_color = kInitialBaseColor;
  actor.tint = kInitialTint;
  actor.scale = host.RoomActorScale();
  ResetActorJoints(host, actor);
  for (std::size_t i = 0; i < actor.texture_scroll.size(); ++i) {
    const ActorTextureScrollBounds& bounds = actor.texture_scroll_bounds[i];
    actor.texture_scroll[i] = {
        .x = bounds.x,
        .y = bounds.y,
        .step_x = static_cast<std::int8_t>(bounds.initial_step_x),
        .step_y = static_cast<std::int8_t>(bounds.initial_step_y),
        .interval = bounds.interval,
        .timer = 0};
  }
  actor.shared_sequences[0] = AssetAddress::kNone;
  actor.shared_sequences[1] = AssetAddress::kNone;
  for (int bank = 0; bank < kBoundSharedBanks; ++bank) {
    const SharedSequenceBank value = host.LoadSequenceBank(bank);
    if (value.data != AssetAddress::kNone) {
      static_cast<void>(BindSharedActorSequence(
          value, bank, host.SharedSequenceIdsByKind(), actor));
    }
  }
}

// Places the actor in the room, makes it a companion or attaches it to its
// parent's bone (30DB0.c:1381-1400).
int AttachLoadedActor(ActorAssetHost& host, ActorAssetLoadState& state,
                      const ActorAssetRequest& request,
                      ActorAssetRecord& actor) {
  const int target = TargetSlot(request);
  state.parse_phase = 0;
  if (request.sub_kind == kPlaceInRoom) {
    host.StoreActor(target, actor);
    host.PlaceActorInRoom(target, request.placement, request.material);
    actor = host.LoadActor(target);
    actor.attach_parent = kNoAttachParent;
    host.StoreActor(target, actor);
    return kAssetDone;
  }
  if (request.texture_slot == kCompanionAttachment) {
    actor.attach_parent = request.sub_kind;
    actor.attach_bone = kCompanionAttachment;
    host.StoreActor(target, actor);
    const int parent = request.sub_kind;
    if (parent >= kActorRecordSlots || !host.ActorSummary(parent).present) {
      return RefuseActor(host, "companion parent", parent);
    }
    ActorAssetRecord owner = host.LoadActor(parent);
    owner.companion_slot = static_cast<std::uint8_t>(target);
    owner.companion = {
        .x = 0, .y = 0, .z = 0, .depth_bias = kCompanionDepthBias};
    host.StoreActor(parent, owner);
    return kAssetDone;
  }
  actor.attach_parent = kNoAttachParent;
  host.StoreActor(target, actor);
  host.AttachActorToParentBone(target, request.sub_kind, request.texture_slot);
  return kAssetDone;
}

int InitializeActorRecord(ActorAssetHost& host, ActorAssetLoadState& state,
                          const ActorAssetRequest& request) {
  const int target = TargetSlot(request);
  ActorAssetRecord actor = host.LoadActor(target);
  ResetActorRecordFields(host, actor, request);
  host.StoreActor(target, actor);
  host.ClearActorPaletteFade(target);
  host.ResetActorMotion(target);
  actor = host.LoadActor(target);
  actor.attach_bone = 0;
  actor.is_attached = false;
  return AttachLoadedActor(host, state, request, actor);
}

// Copies the palette variant into the base and current palettes and
// uploads the CLUTs (30DB0.c:1290-1310).
void SetupActorPalettes(ActorAssetHost& host, const ActorAssetLoadState& state,
                        const ActorAssetRequest& request) {
  const int target = TargetSlot(request);
  ActorAssetRecord actor = host.LoadActor(target);
  const auto bytes = static_cast<std::uint32_t>(state.model_palette_words * 2U);
  const std::size_t wanted = Core::Memory::AlignedWordBytes(bytes);
  const std::span<const std::byte> palettes(actor.palettes);
  const std::size_t variant_at = request.variant * kPaletteVariantBytes;
  const std::span<const std::byte> variant = variant_at < palettes.size()
                                                 ? palettes.subspan(variant_at)
                                                 : std::span<const std::byte>{};
  const std::span<std::byte> base =
      std::as_writable_bytes(std::span<std::uint16_t>(actor.base_palette));
  bool copied = Core::Memory::CopyAlignedWords(base, variant, bytes) == wanted;
  copied = Core::Memory::CopyAlignedWords(base.subspan(kBasePaletteCopyAt),
                                          base, bytes) == wanted &&
           copied;
  copied = Core::Memory::CopyAlignedWords(
               std::as_writable_bytes(
                   std::span<std::uint16_t>(actor.current_palette)),
               base, bytes) == wanted &&
           copied;
  if (!copied) {
    host.Refuse("actor palette words", state.model_palette_words);
  }
  const std::uint32_t words = state.model_palette_words;
  if (target == kLastActorSlot) {
    host.UploadClut(actor.base_palette, kLastActorClutRow, 0, words);
    const std::int16_t clut =
        host.TextureSlot(AssetTextureTable::kActor, kCompanionSourceSlot)
            .alternate_palette;
    actor.clut_a = clut;
    actor.clut_b = clut;
  } else {
    const auto row = static_cast<std::uint32_t>(target);
    host.UploadClut(actor.base_palette, row + kActorClutFirstRow, 0, words);
    host.UploadClut(actor.base_palette, row + kActorAlternateClutFirstRow, 0,
                    words);
    const AssetTextureSlot slot =
        host.TextureSlot(AssetTextureTable::kActor, target);
    actor.clut_a = slot.palette;
    actor.clut_b = slot.alternate_palette;
  }
  host.StoreActor(target, actor);
}

int CompleteActorModel(ActorAssetHost& host, ActorAssetLoadState& state,
                       const ActorAssetRequest& request) {
  SetupActorPalettes(host, state, request);
  if (request.target_slot == 0 && request.model_id == kAshleyModelId) {
    return kAssetDone;
  }
  return InitializeActorRecord(host, state, request);
}

// The second frame: offsets the UVs into the texture slot's rows
// (30DB0.c:1250-1288).
int ResumeActorModel(ActorAssetHost& host, ActorAssetLoadState& state,
                     const ActorAssetRequest& request) {
  const int target = TargetSlot(request);
  const ActorAssetRecord actor = host.LoadActor(target);
  const int count =
      actor.other_polygon_count + actor.triangle_count + actor.quad_count;
  const std::optional<std::uint32_t> section =
      PolygonSectionOffset(host, actor.model);
  const std::span<const std::uint8_t> rows = host.TextureSlotRowOffsets();
  if (!section || actor.texture_slot >= rows.size() ||
      !OffsetModelTextureCoordinates(
          host.Bytes(AdvanceAddress(actor.model, *section), 0), count,
          actor.uv_layout, 0, rows[actor.texture_slot] * kTextureRowScale)) {
    state.parse_phase = 0;
    return RefuseActor(host, "actor primitives", target);
  }
  return CompleteActorModel(host, state, request);
}

// Ashley's model is resident: only her texture and palettes reload
// (30DB0.c:1036-1062).
int ReloadAshleyTexture(ActorAssetHost& host, ActorAssetLoadState& state,
                        ActorAssetRequest& request) {
  if (!host.ActorSummary(0).present) {
    return RefuseActor(host, "actor record", 0);
  }
  ActorAssetRecord actor = host.LoadActor(0);
  FileCursor cursor(host, request.data);
  cursor.Skip(kShpModelSizeAt);
  const std::int32_t model_bytes = cursor.I32();
  cursor.Skip(std::ptrdiff_t{model_bytes} + kFileHeaderBytes);
  const std::int32_t skip = cursor.I32();
  cursor.Skip(std::ptrdiff_t{skip} + kAshleyPaletteSkip);
  state.model_palette_words = actor.palette_words;
  state.model_texture_width = kAshleyTextureSize;
  state.model_texture_height = kAshleyTextureSize;
  const std::optional<VramRect> rect = ActorTextureRect(
      host, actor.texture_slot, kAshleyTextureSize, kAshleyTextureSize);
  if (!ReadActorPalettes(cursor, actor, actor.palette_words) || !rect) {
    return RefuseActor(host, "actor file", 0);
  }
  host.StoreActor(0, actor);
  if (!UploadFrom(host, *rect, cursor.At(), "actor texture")) {
    return kAssetRefused;
  }
  request.variant = 0;
  return CompleteActorModel(host, state, request);
}

int PlaceModellessActor(ActorAssetHost& host, ActorAssetLoadState& state,
                        const ActorAssetRequest& request) {
  const int target = TargetSlot(request);
  ActorAssetRecord actor = host.LoadActor(target);
  actor.model_id = static_cast<std::uint16_t>(kModellessActorId);
  actor.slot = static_cast<std::uint8_t>(target);
  actor.occupancy_radius = kModellessRadius;
  actor.occupancy_height = kModellessHeight;
  host.StoreActor(target, actor);
  host.ResetActorMotion(target);
  actor = host.LoadActor(target);
  actor.is_skipped = false;
  host.StoreActor(target, actor);
  host.PlaceActorInRoom(target, request.placement, kModellessPlacementMode);
  state.parse_phase = 0;
  return kAssetDone;
}

int CopySharedActorModel(ActorAssetHost& host, ActorAssetLoadState& state,
                         const ActorAssetRequest& request, int source) {
  const int target = TargetSlot(request);
  if (!host.ActorSummary(source).present) {
    return RefuseActor(host, "shared actor record", source);
  }
  host.CopyActorRecord(target, source);
  ActorAssetRecord actor = host.LoadActor(target);
  if (actor.holds_texture_slots && actor.texture_slot != kNoAssetTextureSlot) {
    for (int i = 0; i <= actor.texture_slot_count_minus_one; ++i) {
      AddTextureReference(host, AssetTextureTable::kActor,
                          actor.texture_slot + i, 1);
    }
  }
  actor.placement = request.placement;
  state.model_palette_words =
      actor.texture_header_bit ? kHeaderBitPaletteWords : kPlainPaletteWords;
  host.StoreActor(target, actor);
  return CompleteActorModel(host, state, request);
}
}  // namespace

namespace {
// The first free run of `count` actor texture slots in [first, end)
// (30DB0.c:1180-1222); nullopt when the original falls back.
std::optional<int> FindFreeActorTextureSlots(const ActorAssetHost& host,
                                             int count, int first, int end) {
  const auto references = [&](int index) {
    return host.TextureSlot(AssetTextureTable::kActor, index).references;
  };
  if (count == kWideActorTextureSlots) {
    for (int i = first; i < end; ++i) {
      if (references(i) != 0) {
        return std::nullopt;
      }
    }
    return first;
  }
  if (count < kSmallTextureLimit) {
    for (int i = first; i < end; i += count) {
      if (references(i) == 0) {
        return i;
      }
    }
    return std::nullopt;
  }
  for (int i = first; i < end; ++i) {
    int free = 0;
    while (free < count && references(i + free) == 0) {
      ++free;
    }
    if (free == count) {
      return i;
    }
    if (((i + count) & kTextureSlotGroupMask) == 0) {
      i += count - 1;
    }
  }
  return std::nullopt;
}

// Marks the slots used, uploads the palettes and texture and leaves the UV
// pass for the next frame (30DB0.c:1236-1258).
int PlaceActorTexture(ActorAssetHost& host, ActorAssetLoadState& state,
                      const ActorAssetRequest& request, FileCursor& cursor,
                      ActorAssetRecord& actor, int first, int count) {
  const int target = TargetSlot(request);
  for (int j = 0; j < count; ++j) {
    if (first + j >= kActorTextureSlots) {
      // The original's fallback slot 20 with more than five slots writes
      // past the table.
      host.Refuse("actor texture slot run", first + j);
      break;
    }
    SetTextureReferences(host, AssetTextureTable::kActor, first + j, 1);
  }
  actor.texture_slot = static_cast<std::uint8_t>(first);
  AssetTextureSlot page = host.TextureSlot(AssetTextureTable::kActor, first);
  page.texture_page = static_cast<std::int16_t>(
      actor.texture_header_bit ? (page.texture_page | kTexturePageHeaderBit)
                               : (page.texture_page & ~kTexturePageClearBits));
  host.SetTextureSlot(AssetTextureTable::kActor, first, page);
  const std::optional<VramRect> rect = ActorTextureRect(
      host, first, state.model_texture_width, state.model_texture_height);
  if (!ReadActorPalettes(cursor, actor, state.model_palette_words) || !rect) {
    host.StoreActor(target, actor);
    return RefuseActor(host, "actor texture", target);
  }
  host.StoreActor(target, actor);
  if (!UploadFrom(host, *rect, cursor.At(), "actor texture")) {
    return kAssetRefused;
  }
  cursor.Skip(std::ptrdiff_t{state.model_texture_width} *
              state.model_texture_height);
  state.parse_phase = kParseFixPrimitives;
  state.model_cursor = cursor.At();
  return kAssetPending;
}

// The texture header, the slot search and its fallback (30DB0.c:1150-1235).
int ParseActorTexture(ActorAssetHost& host, ActorAssetLoadState& state,
                      const ActorAssetRequest& request, FileCursor& cursor,
                      ActorAssetRecord& actor) {
  const int target = TargetSlot(request);
  const std::uint32_t header = cursor.U8();
  actor.texture_header_bit = (header & 1U) != 0;
  constexpr unsigned kUvLayoutShift = 4;
  actor.uv_layout = ((header >> kUvLayoutShift) & 1U) != 0;
  state.model_header_word = static_cast<std::int32_t>(header);
  state.model_texture_width = static_cast<std::int32_t>(cursor.U8() * 2U);
  state.model_texture_height = static_cast<std::int32_t>(cursor.U8() * 2U);
  actor.palette_words = static_cast<std::uint8_t>(cursor.U8());
  state.model_palette_words = actor.palette_words;
  int count = state.model_texture_height /
              static_cast<std::int32_t>(kActorTextureSlotHeight);
  if (state.model_texture_width ==
      static_cast<std::int32_t>(kWideActorTextureWidth)) {
    count = kWideActorTextureSlots;
  }
  actor.texture_slot_count_minus_one =
      static_cast<std::uint8_t>((count - 1) & kSlotCountMask);
  const bool companion = target == 1 || target == kLastActorSlot;
  const int first = companion                     ? kCompanionTextureSlotsFirst
                    : count >= kSmallTextureLimit ? kWideTextureSlotsFirst
                                                  : 0;
  const int end = companion ? kCompanionTextureSlotsEnd : kActorTextureSlotsEnd;
  if (!cursor.Ok() || count == 0 ||
      (count > kSmallTextureLimit + 1 && count != kWideActorTextureSlots)) {
    // The original searches no slots and uses a stale index.
    host.StoreActor(target, actor);
    return RefuseActor(host, "actor texture slot count", count);
  }
  std::optional<int> slot = FindFreeActorTextureSlots(host, count, first, end);
  if (!slot) {
    if (host.ReserveTexturePages(kFallbackTexturePage, 1) == 0) {
      actor.texture_slot = 0;
      actor.texture_slot_count_minus_one = 0;
      host.StoreActor(target, actor);
      AddTextureReference(host, AssetTextureTable::kActor, 0, 1);
      return CompleteActorModel(host, state, request);
    }
    slot = kFallbackTextureSlot;
  }
  return PlaceActorTexture(host, state, request, cursor, actor, *slot, count);
}

// A texture size of 4 names the model whose texture this one shares
// (30DB0.c:1132-1148).
int BorrowActorTexture(ActorAssetHost& host, ActorAssetLoadState& state,
                       const ActorAssetRequest& request,
                       const FileCursor& cursor, ActorAssetRecord& actor) {
  const int target = TargetSlot(request);
  FileCursor peek = cursor;
  const std::int32_t wanted = peek.I32();
  host.StoreActor(target, actor);
  // The search includes this actor; without a match the original reads the
  // last record it looked at.
  int source = kLastActorSlot;
  for (int i = 0; i < kActorRecordSlots; ++i) {
    const ActorAssetSummary other = host.ActorSummary(i);
    if (other.present && int{other.model_id} == wanted) {
      source = i;
      break;
    }
  }
  if (!peek.Ok() || !host.ActorSummary(source).present) {
    return RefuseActor(host, "borrowed actor texture", wanted);
  }
  const ActorAssetRecord other = host.LoadActor(source);
  actor.palette_words = other.palette_words;
  state.model_palette_words = other.palette_words;
  const std::size_t bytes =
      std::size_t{other.palette_words} * kPaletteWordBytes;
  if (bytes > other.palettes.size()) {
    host.Refuse("borrowed actor palette words", other.palette_words);
  }
  std::copy_n(other.palettes.begin(), std::min(bytes, other.palettes.size()),
              actor.palettes.begin());
  actor.texture_slot = other.texture_slot;
  actor.uv_layout = other.uv_layout;
  actor.holds_texture_slots = false;
  actor.texture_header_bit = true;
  host.StoreActor(target, actor);
  return ResumeActorModel(host, state, request);
}

// The file's header, texture layout, model and embedded sequence
// (30DB0.c:1100-1131).
int ParseActorModelFile(ActorAssetHost& host, ActorAssetLoadState& state,
                        const ActorAssetRequest& request) {
  const int target = TargetSlot(request);
  ActorAssetRecord actor = host.LoadActor(target);
  actor.model_id = static_cast<std::uint16_t>(request.model_id);
  FileCursor cursor(host, request.data);
  cursor.Skip(kFileHeaderBytes);
  actor.placement = request.placement;
  actor.is_skipped = true;
  const std::uint32_t counts = cursor.U32();
  const std::uint32_t polygons = cursor.U32();
  actor.bone_count = static_cast<std::uint8_t>(counts);
  actor.vertex_group_count = static_cast<std::uint8_t>(counts >> kBitsPerByte);
  actor.triangle_count = static_cast<std::uint16_t>(counts >> kHalfWordBits);
  actor.quad_count = static_cast<std::uint16_t>(polygons);
  actor.other_polygon_count =
      static_cast<std::uint16_t>(polygons >> kHalfWordBits);
  const std::span<const std::byte> layout =
      cursor.Take(kActorTextureLayoutBytes);
  const std::int32_t size = cursor.I32();
  host.StoreActor(target, actor);
  if (!cursor.Ok()) {
    return RefuseActor(host, "actor file", target);
  }
  host.WriteActorTextureLayout(target, layout);
  actor = host.LoadActor(target);
  if (size < kShpModelHeaderBytes) {
    return RefuseActor(host, "actor model size", size);
  }
  const AssetAddress model = host.AllocateLow(static_cast<std::size_t>(size));
  if (model == AssetAddress::kNone) {
    return kAssetRefused;
  }
  static_cast<void>(CopyExact(host, model, cursor.At(),
                              static_cast<std::size_t>(size), "actor model"));
  actor.model = model;
  actor.model_data_size = static_cast<std::uint32_t>(size);
  host.StoreActor(target, actor);
  cursor.Skip(size);
  ActorAssetRequest embedded;
  embedded.kind = static_cast<std::uint8_t>(ActorAssetKind::kSequenceParse);
  embedded.target_slot = request.target_slot;
  embedded.data = cursor.At();
  embedded.model_id = kEmbeddedSequenceModel;
  static_cast<void>(ParseActorSequence(host, state, embedded));
  actor = host.LoadActor(target);
  cursor.Skip(kFileHeaderBytes);
  const std::int32_t skip = cursor.I32();
  cursor.Skip(skip);
  const std::int32_t texture_size = cursor.I32();
  actor.holds_texture_slots = true;
  if (texture_size == kBorrowedTextureMarker) {
    return BorrowActorTexture(host, state, request, cursor, actor);
  }
  return ParseActorTexture(host, state, request, cursor, actor);
}
}  // namespace

// decomp: BATTLE.PRG func_8009AC24 (src/BATTLE/BATTLE.PRG/30DB0.c:930)
int FindSharedActorModel(const ActorAssetHost& host,
                         ActorAssetRequest& request) {
  for (int i = 0; i < kActorRecordSlots; ++i) {
    if (i == request.target_slot) {
      continue;
    }
    const ActorAssetSummary other = host.ActorSummary(i);
    if (other.present && int{other.model_id} == int{request.model_id}) {
      request.data = SharedAssetAddress(i);
      return 1;
    }
  }
  return 0;
}

// decomp: BATTLE.PRG _loadShp (src/BATTLE/BATTLE.PRG/30DB0.c:944)
int UpdateActorModelLoad(ActorAssetHost& host, ActorAssetLoadState& state,
                         ActorAssetRequest& request) {
  switch (state.model_phase) {
    case kPhaseStart:
      if (request.model_id == kModellessActorId ||
          FindSharedActorModel(host, request) != 0) {
        state.model_file_allocated = false;
        break;
      }
      state.model_file_allocated = true;
      [[fallthrough]];
    default:
      if (const int started = StartActorModelFileRead(host, state, request);
          started != kAssetPending) {
        return started;
      }
      [[fallthrough]];
    case kPhaseReading:
      return WaitForFileRead(host, state.model_phase,
                             AssetFileStream::kActorModel);
    case kPhaseParsing:
      break;
  }
  const int result = ParseActorModel(host, state, request);
  if (result == kAssetPending) {
    return kAssetPending;
  }
  if (state.model_file_allocated) {
    state.model_file_allocated = false;
    host.FreeHigh(state.model_file);
  }
  state.model_phase = kPhaseStart;
  return result;
}

// decomp: BATTLE.PRG _parseShp (src/BATTLE/BATTLE.PRG/30DB0.c:1026)
int ParseActorModel(ActorAssetHost& host, ActorAssetLoadState& state,
                    ActorAssetRequest& request) {
  const int target = TargetSlot(request);
  if (!ValidActorSlot(host, target)) {
    return kAssetRefused;
  }
  if (state.parse_phase == kParseFixPrimitives) {
    if (!host.ActorSummary(target).present) {
      return RefuseActor(host, "actor record", target);
    }
    return ResumeActorModel(host, state, request);
  }
  if (state.parse_phase != 0) {
    return RefuseActor(host, "actor parse phase", state.parse_phase);
  }
  if (target == 0 && request.model_id == kAshleyModelId) {
    return ReloadAshleyTexture(host, state, request);
  }
  if (!host.ActorSummary(target).present &&
      !host.BindActorRecord(target, request.destination)) {
    return kAssetRefused;
  }
  if (request.model_id == kModellessActorId) {
    return PlaceModellessActor(host, state, request);
  }
  std::optional<int> source = SharedAssetSlot(request.data, kActorRecordSlots);
  if (!source && FindSharedActorModel(host, request) != 0) {
    source = SharedAssetSlot(request.data, kActorRecordSlots);
  }
  if (source) {
    return CopySharedActorModel(host, state, request, *source);
  }
  return ParseActorModelFile(host, state, request);
}

// decomp: BATTLE.PRG func_8009CFB0 (src/BATTLE/BATTLE.PRG/30DB0.c:1920)
int ReleaseActorModel(ActorAssetHost& host, int slot) {
  if (slot < 0 || slot >= kActorRecordSlots ||
      !host.ActorSummary(slot).present) {
    return -1;
  }
  const ActorAssetRecord actor = host.LoadActor(slot);
  if (actor.model_id != static_cast<std::uint16_t>(kModellessActorId)) {
    ActorAssetRequest probe;
    probe.target_slot = static_cast<std::int8_t>(slot);
    probe.model_id = static_cast<std::int16_t>(actor.model_id);
    if (FindSharedActorModel(host, probe) == 0) {
      const std::span<std::byte> header =
          host.Bytes(actor.model, kSoundSectionAt + sizeof(std::uint32_t));
      if (header.size() < kSoundSectionAt + sizeof(std::uint32_t)) {
        host.Refuse("actor model header", slot);
      } else {
        host.StopModelSounds(AdvanceAddress(
            actor.model, static_cast<std::ptrdiff_t>(ReadLittle(
                             header, kSoundSectionAt, sizeof(std::uint32_t)))));
      }
      host.FreeLow(actor.model);
      host.FreeLow(actor.owned_sequence);
    }
    if (actor.common_sequence != AssetAddress::kNone) {
      bool used = false;
      for (int i = 0; i < kActorRecordSlots && !used; ++i) {
        const ActorAssetSummary other = host.ActorSummary(i);
        used = i != slot && other.present &&
               other.common_sequence == actor.common_sequence;
      }
      if (!used) {
        host.FreeLow(actor.common_sequence);
      }
    }
    static_cast<void>(ReleaseActorSequence(host, slot, 0));
    if (slot < kWeaponOwnerSlots) {
      static_cast<void>(ReleaseWeaponModel(host, slot * 2));
      static_cast<void>(ReleaseWeaponModel(host, (slot * 2) | 1));
    }
    if (actor.holds_texture_slots) {
      for (int i = 0; i <= actor.texture_slot_count_minus_one; ++i) {
        AddTextureReference(host, AssetTextureTable::kActor,
                            actor.texture_slot + i, -1);
      }
      if (actor.texture_slot == kFallbackTextureSlot) {
        static_cast<void>(host.RetireTexturePages(kFallbackTexturePage, 1));
      }
    }
  }
  host.ClearActorSlot(slot);
  return 0;
}

namespace {
constexpr std::uint8_t kBattleSequence = 0;
constexpr std::uint8_t kCommonBankSequence = 1;
constexpr std::uint8_t kEffectBankSequence = 2;
constexpr std::uint8_t kMaterialSequence = 3;
constexpr std::uint8_t kMaterialModelSequence = 4;
constexpr std::uint8_t kSpecialSequence = 5;
constexpr std::size_t kMaterialModelColumns = 4;
constexpr std::uint8_t kFallbackSequenceMaterial = 2;
constexpr std::uint16_t kMaterialModelActorId = 2;
constexpr int kSequenceSizeShift = 11;
constexpr int kFirstUnsharedSlot = 2;
constexpr int kPlayerSequenceSlots = 2;
constexpr std::ptrdiff_t kSequenceFlagsPadding = 3;
constexpr std::ptrdiff_t kBattleCountPadding = 3;
constexpr std::ptrdiff_t kBankEntryPadding = 2;
constexpr std::size_t kBankOffsetBytes = 4;
constexpr std::ptrdiff_t kSharedAnimationHeaderBytes = 8;
constexpr std::int16_t kEndOfBank = -1;
constexpr std::uint8_t kNoRowOffset = 0xFF;
constexpr auto kEndOfAnimations = static_cast<AssetAddress>(UINTPTR_MAX);
constexpr int kFirstEffectBank = 1;
constexpr int kLastEffectBank = 3;
constexpr std::ptrdiff_t kEffectTextureHeightAt = 2;
constexpr std::ptrdiff_t kEffectTextureHeaderBytes = 4;
constexpr std::uint8_t kEffectTextureColumnMask = 0xF;
constexpr int kEffectTextureColumnWidth = 64;
constexpr std::int16_t kEffectTextureY = 256;
constexpr std::int16_t kEffectTextureWidth = 64;

// A sequence file to read, or the loader's result when there is none
// (30DB0.c:1500-1580).
struct SequenceFileExtent {
  int result = kAssetPending;
  std::uint32_t sectors = 0;
  std::int32_t sector = 0;
  bool low_end = false;
};

SequenceFileExtent SequenceResult(int result) {
  SequenceFileExtent extent;
  extent.result = result;
  return extent;
}

SequenceFileExtent RefuseSequenceFile(ActorAssetHost& host, const char* what,
                                      std::int64_t value) {
  host.Refuse(what, value);
  return SequenceResult(kAssetRefused);
}

std::optional<ActorAssetRecord> SequenceActor(ActorAssetHost& host,
                                              int target) {
  if (target < 0 || target >= kActorRecordSlots ||
      !host.ActorSummary(target).present) {
    host.Refuse("sequence actor", target);
    return std::nullopt;
  }
  return host.LoadActor(target);
}

SequenceFileExtent BattleSequenceFile(ActorAssetHost& host,
                                      const ActorAssetRequest& request) {
  const std::optional<ActorAssetRecord> actor =
      SequenceActor(host, request.target_slot);
  if (!actor) {
    return SequenceResult(kAssetRefused);
  }
  const int model = request.model_id;
  if (model < 0 ||
      static_cast<std::size_t>(model) >= actor->battle_sequence_lbas.size()) {
    return RefuseSequenceFile(host, "battle sequence", model);
  }
  const auto index = static_cast<std::size_t>(model);
  const std::uint32_t sectors =
      std::uint32_t{actor->chain_attack_animation_ids[index]} >>
      kSequenceSizeShift;
  if (sectors == 0) {
    return SequenceResult(kAssetDone);
  }
  return {
      .result = kAssetPending,
      .sectors = sectors,
      .sector = static_cast<std::int32_t>(actor->battle_sequence_lbas[index]),
      .low_end = false};
}

SequenceFileExtent TableSequenceFile(ActorAssetHost& host,
                                     AssetFileTable sectors_table,
                                     AssetFileTable offsets_table, int index,
                                     std::int32_t first_sector) {
  const std::optional<std::uint32_t> sectors =
      index < 0
          ? std::nullopt
          : host.FileTableEntry(sectors_table, static_cast<std::size_t>(index));
  const std::optional<std::uint32_t> offset =
      index < 0
          ? std::nullopt
          : host.FileTableEntry(offsets_table, static_cast<std::size_t>(index));
  if (!sectors || !offset) {
    return RefuseSequenceFile(host, "sequence file table", index);
  }
  return {.result = kAssetPending,
          .sectors = *sectors,
          .sector = static_cast<std::int32_t>(*offset) + first_sector,
          .low_end = false};
}

// Falls back to material 2's first model when the pair has no file.
SequenceFileExtent MaterialModelSequenceFile(ActorAssetHost& host,
                                             ActorAssetRequest& request) {
  const int index =
      (request.material * static_cast<int>(kMaterialModelColumns)) +
      request.model_id;
  const std::optional<std::uint32_t> sectors =
      index < 0
          ? std::nullopt
          : host.FileTableEntry(AssetFileTable::kMaterialModelSequenceSectors,
                                static_cast<std::size_t>(index));
  if (sectors && *sectors == 0) {
    request.material = kFallbackSequenceMaterial;
    request.model_id = 0;
  }
  const int file =
      (request.material * static_cast<int>(kMaterialModelColumns)) +
      request.model_id;
  return TableSequenceFile(host, AssetFileTable::kMaterialModelSequenceSectors,
                           AssetFileTable::kMaterialModelSequenceOffsets, file,
                           kMaterialModelSequenceFirstSector);
}

SequenceFileExtent SpecialSequenceFile(ActorAssetHost& host,
                                       ActorAssetRequest& request) {
  if (request.model_id == 0) {
    return SequenceResult(kAssetDone);
  }
  --request.model_id;
  const std::optional<ActorAssetRecord> actor =
      SequenceActor(host, request.target_slot);
  if (!actor) {
    return SequenceResult(kAssetRefused);
  }
  if (actor->model_id == kMaterialModelActorId) {
    return MaterialModelSequenceFile(host, request);
  }
  const int model = request.model_id;
  if (model < 0 ||
      static_cast<std::size_t>(model) >= actor->special_sequence_sizes.size()) {
    return RefuseSequenceFile(host, "special sequence", model);
  }
  const auto index = static_cast<std::size_t>(model);
  const std::uint32_t sectors =
      std::uint32_t{actor->special_sequence_sizes[index]} >> kSequenceSizeShift;
  if (sectors == 0) {
    return SequenceResult(kAssetRefused);
  }
  return {
      .result = kAssetPending,
      .sectors = sectors,
      .sector = static_cast<std::int32_t>(actor->special_sequence_lbas[index]),
      .low_end = false};
}

SequenceFileExtent SequenceFile(ActorAssetHost& host,
                                ActorAssetRequest& request) {
  switch (request.sub_kind) {
    case kBattleSequence:
      return BattleSequenceFile(host, request);
    case kCommonBankSequence:
    case kEffectBankSequence: {
      SequenceFileExtent file =
          TableSequenceFile(host, AssetFileTable::kSequenceSectors,
                            AssetFileTable::kSequenceOffsets, request.model_id,
                            kSequenceFirstSector);
      if (file.result == kAssetPending && file.sectors == 0) {
        return SequenceResult(kAssetRefused);
      }
      file.low_end = true;
      return file;
    }
    case kMaterialSequence:
      return TableSequenceFile(host, AssetFileTable::kMaterialSequenceSectors,
                               AssetFileTable::kMaterialSequenceOffsets,
                               request.material, kMaterialSequenceFirstSector);
    case kMaterialModelSequence:
      return MaterialModelSequenceFile(host, request);
    case kSpecialSequence:
      return SpecialSequenceFile(host, request);
    default:
      // The original reads an uninitialized sector count.
      return RefuseSequenceFile(host, "sequence sub kind", request.sub_kind);
  }
}

int StartSequenceFileRead(ActorAssetHost& host, ActorAssetLoadState& state,
                          ActorAssetRequest& request) {
  const SequenceFileExtent file = SequenceFile(host, request);
  if (file.result != kAssetPending) {
    return file.result;
  }
  const std::uint32_t bytes = file.sectors << kSectorShift;
  state.sequence_file =
      file.low_end ? host.AllocateLow(bytes) : host.AllocateHigh(bytes);
  request.data = state.sequence_file;
  if (state.sequence_file == AssetAddress::kNone) {
    return kAssetRefused;
  }
  if (request.sub_kind == kBattleSequence && request.target_slot == 0 &&
      request.model_id != 0) {
    // The original leaves the other fields as stack bytes.
    ActorAssetRequest weapon;
    weapon.kind = static_cast<std::uint8_t>(ActorAssetKind::kSequenceLoad);
    weapon.sub_kind = kMaterialSequence;
    weapon.material = static_cast<std::uint8_t>(request.model_id);
    static_cast<void>(QueueActorAssetRequest(host, weapon));
  }
  state.model_phase = kPhaseReading;
  StartFileRead(host, AssetFileStream::kSequence,
                {file.sector, static_cast<std::int32_t>(bytes)}, request.data);
  return kAssetPending;
}

int FinishSequenceLoad(ActorAssetHost& host, ActorAssetLoadState& state,
                       ActorAssetRequest& request) {
  const int result = ParseActorSequence(host, state, request);
  if (result == kAssetPending) {
    return kAssetPending;
  }
  if (state.sequence_file_owned) {
    state.sequence_file_owned = false;
    host.FreeHigh(state.sequence_file);
  }
  state.model_phase = kPhaseStart;
  return result;
}

int CopySequence(ActorAssetHost& host, AssetAddress sequence,
                 const FileCursor& cursor, std::uint32_t bytes) {
  static_cast<void>(
      CopyWordsInto(host, sequence, cursor.At(), bytes, "sequence copy"));
  host.RecordSequenceBank(sequence, bytes);
  return kAssetDone;
}
}  // namespace

// decomp: BATTLE.PRG func_8009BD90 (src/BATTLE/BATTLE.PRG/30DB0.c:1433)
// decomp: BATTLE.PRG vs_battleFindSharedSequence
// (src/BATTLE/BATTLE.PRG/30DB0.c:1454)
int FindSharedActorSequence(const ActorAssetHost& host,
                            ActorAssetRequest& request) {
  const int target = TargetSlot(request);
  // The original reads the target's record without a check.
  if (target < 0 || target >= kActorRecordSlots ||
      !host.ActorSummary(target).present) {
    return 0;
  }
  const std::uint16_t model_id = host.ActorSummary(target).model_id;
  for (int i = target >= kFirstUnsharedSlot ? kFirstUnsharedSlot : 0;
       i < kActorRecordSlots; ++i) {
    if (i == target) {
      continue;
    }
    const ActorAssetSummary other = host.ActorSummary(i);
    if (!other.present || other.model_id != model_id) {
      continue;
    }
    if ((request.model_id == 0 &&
         other.common_sequence != AssetAddress::kNone) ||
        (request.model_id != 0 &&
         int{other.sequence_model_id} == int{request.model_id})) {
      request.data = SharedAssetAddress(i);
      return 1;
    }
  }
  return 0;
}

// decomp: BATTLE.PRG func_8009BE5C (src/BATTLE/BATTLE.PRG/30DB0.c:1474)
int UpdateActorSequenceLoad(ActorAssetHost& host, ActorAssetLoadState& state,
                            ActorAssetRequest& request) {
  switch (state.model_phase) {
    case kPhaseStart:
      if (request.sub_kind == kBattleSequence && request.target_slot != 0 &&
          FindSharedActorSequence(host, request) != 0) {
        state.sequence_file_owned = false;
        return FinishSequenceLoad(host, state, request);
      }
      state.sequence_file_owned = request.sub_kind != kCommonBankSequence &&
                                  request.sub_kind != kEffectBankSequence;
      break;
    case kPhaseReading:
      return WaitForFileRead(host, state.model_phase,
                             AssetFileStream::kSequence);
    case kPhaseParsing:
      return FinishSequenceLoad(host, state, request);
    default:
      break;
  }
  if (const int started = StartSequenceFileRead(host, state, request);
      started != kAssetPending) {
    return started;
  }
  return WaitForFileRead(host, state.model_phase, AssetFileStream::kSequence);
}

// decomp: BATTLE.PRG func_8009C378 (src/BATTLE/BATTLE.PRG/30DB0.c:1608)
void CopyWeaponSequence(ActorAssetHost& host, AssetAddress destination,
                        AssetAddress source) {
  CopyFixedBlock(host, destination, source, kWeaponSequenceBytes,
                 "weapon sequence copy");
  host.RecordSequenceCopy(destination, source, kWeaponSequenceBytes);
}

namespace {
// The common sequence: shared with a record or read from the file
// (30DB0.c:1634-1650).
int ParseCommonSequence(ActorAssetHost& host, FileCursor& cursor, int target,
                        ActorAssetRecord& actor) {
  if (const std::optional<int> source =
          SharedAssetSlot(cursor.At(), kActorRecordSlots)) {
    if (!host.ActorSummary(*source).present) {
      return RefuseActor(host, "shared sequence record", *source);
    }
    const ActorAssetRecord other = host.LoadActor(*source);
    actor.sequence_flags = other.sequence_flags;
    actor.common_sequence = other.common_sequence;
    host.StoreActor(target, actor);
    return kAssetDone;
  }
  actor.sequence_flags = static_cast<std::uint8_t>(cursor.U8());
  cursor.Skip(kSequenceFlagsPadding);
  const std::uint32_t size = RoundUpToWord(cursor.I32());
  host.StoreActor(target, actor);
  if (!cursor.Ok()) {
    return RefuseActor(host, "common sequence", target);
  }
  const AssetAddress sequence = host.AllocateLow(size);
  if (sequence == AssetAddress::kNone) {
    return kAssetRefused;
  }
  actor.common_sequence = sequence;
  host.StoreActor(target, actor);
  return CopySequence(host, sequence, cursor, size);
}

// The sequence a model file carries (30DB0.c:1651-1662).
int ParseOwnedSequence(ActorAssetHost& host, FileCursor& cursor, int target,
                       ActorAssetRecord& actor) {
  cursor.Skip(kFileHeaderBytes);
  const std::uint32_t size = RoundUpToWord(cursor.I32());
  if (!cursor.Ok()) {
    return RefuseActor(host, "model sequence", target);
  }
  const AssetAddress sequence = host.AllocateLow(size);
  if (sequence == AssetAddress::kNone) {
    return kAssetRefused;
  }
  actor.owned_sequence = sequence;
  host.StoreActor(target, actor);
  return CopySequence(host, sequence, cursor, size);
}

// Another actor's battle sequence; the player slots take a copy
// (30DB0.c:1663-1680).
int ShareBattleSequence(ActorAssetHost& host, const ActorAssetRequest& request,
                        ActorAssetRecord& actor, int source) {
  const int target = TargetSlot(request);
  if (!host.ActorSummary(source).present) {
    return RefuseActor(host, "shared sequence record", source);
  }
  const ActorAssetRecord other = host.LoadActor(source);
  actor.common_animation_count = other.common_animation_count;
  AssetAddress sequence = other.battle_sequence;
  if (target < kPlayerSequenceSlots) {
    if (actor.slot == 0) {
      sequence = host.PrimarySequenceBuffer();
      CopyWeaponSequence(host, sequence, other.battle_sequence);
    } else if (source == 0) {
      sequence = host.AllocateLow(kWeaponSequenceBytes);
      if (sequence == AssetAddress::kNone) {
        host.StoreActor(target, actor);
        return RefuseActor(host, "battle sequence copy", target);
      }
      CopyWeaponSequence(host, sequence, other.battle_sequence);
    }
  }
  actor.battle_sequence = sequence;
  actor.sequence_model_id = static_cast<std::uint8_t>(request.model_id);
  host.StoreActor(target, actor);
  return kAssetDone;
}

// A battle sequence from the file (30DB0.c:1681-1704).
int ParseBattleSequence(ActorAssetHost& host, const ActorAssetRequest& request,
                        FileCursor& cursor, ActorAssetRecord& actor) {
  const int target = TargetSlot(request);
  actor.common_animation_count = static_cast<std::uint8_t>(cursor.U8());
  cursor.Skip(1);
  const std::uint32_t bones = cursor.U16();
  if (bones != 0 && bones != actor.bone_count) {
    host.StoreActor(target, actor);
    return kAssetDone;
  }
  const std::uint32_t size = RoundUpToWord(cursor.I32());
  if (!cursor.Ok()) {
    host.StoreActor(target, actor);
    return RefuseActor(host, "battle sequence", target);
  }
  AssetAddress sequence = host.PrimarySequenceBuffer();
  if (actor.slot != 0) {
    sequence = host.AllocateLow(size);
    if (sequence == AssetAddress::kNone) {
      host.StoreActor(target, actor);
      return kAssetRefused;
    }
  }
  actor.battle_sequence = sequence;
  actor.sequence_model_id = static_cast<std::uint8_t>(request.model_id);
  host.StoreActor(target, actor);
  return CopySequence(host, sequence, cursor, size);
}

int ParseActorSequenceKind(ActorAssetHost& host, ActorAssetRequest& request,
                           FileCursor& cursor) {
  const int target = TargetSlot(request);
  std::optional<ActorAssetRecord> actor = SequenceActor(host, target);
  if (!actor) {
    return kAssetRefused;
  }
  if (request.model_id != 0 && FindSharedActorSequence(host, request) != 0) {
    cursor.Seek(request.data);
  }
  if (request.model_id == 0) {
    return ParseCommonSequence(host, cursor, target, *actor);
  }
  if (request.model_id == kEmbeddedSequenceModel) {
    return ParseOwnedSequence(host, cursor, target, *actor);
  }
  if (const std::optional<int> source =
          SharedAssetSlot(cursor.At(), kActorRecordSlots)) {
    return ShareBattleSequence(host, request, *actor, *source);
  }
  return ParseBattleSequence(host, request, cursor, *actor);
}

// The sequences of a material (sub kind 3) or a special action (4 and 5).
int ParseWorkSequence(ActorAssetHost& host, const ActorAssetRequest& request,
                      FileCursor& cursor) {
  if (request.sub_kind == kMaterialSequence) {
    cursor.Skip(kFileHeaderBytes);
    const std::uint32_t size = RoundUpToWord(cursor.I32());
    if (!cursor.Ok()) {
      return RefuseActor(host, "material sequence", request.material);
    }
    return CopySequence(host, host.SequenceWorkBuffer(), cursor, size);
  }
  const int target = TargetSlot(request);
  std::optional<ActorAssetRecord> actor = SequenceActor(host, target);
  if (!actor) {
    return kAssetRefused;
  }
  actor->battle_animation_count = static_cast<std::uint8_t>(cursor.U8());
  cursor.Skip(kBattleCountPadding);
  const std::uint32_t size = RoundUpToWord(cursor.I32());
  host.StoreActor(target, *actor);
  if (!cursor.Ok()) {
    return RefuseActor(host, "special sequence", target);
  }
  const AssetAddress sequence = host.AllocateLow(size);
  if (sequence == AssetAddress::kNone) {
    return kAssetRefused;
  }
  actor->shared_sequences[kSpecialSequenceRef] = sequence;
  host.StoreActor(target, *actor);
  return CopySequence(host, sequence, cursor, size);
}

// A shared sequence bank: its animation ids, entry parameters and
// animations, bound into every actor (30DB0.c:1732-1770).
int ParseSequenceBank(ActorAssetHost& host, int bank, FileCursor& cursor) {
  host.SetBankTexturePage(bank, 0);
  SharedSequenceBank value = host.LoadSequenceBank(bank);
  if (value.data != AssetAddress::kNone) {
    return kAssetRefused;
  }
  value.data = cursor.At();
  const std::int32_t count = cursor.I32();
  if (!cursor.Ok() || count < 0 || count >= kSharedSequenceEntries) {
    return RefuseActor(host, "shared sequence count", count);
  }
  const auto entries = static_cast<std::size_t>(count);
  for (std::size_t i = 0; i < entries; ++i) {
    value.animation_ids[i] = static_cast<std::int16_t>(cursor.U16());
    cursor.Skip(kBankEntryPadding);
  }
  for (std::size_t i = 0; i < entries; ++i) {
    value.entry_params[i] = static_cast<std::int16_t>(cursor.U16());
    cursor.Skip(kBankEntryPadding);
  }
  value.animation_ids[entries] = kEndOfBank;
  for (std::size_t i = 0; i < entries; ++i) {
    const AssetAddress at = cursor.At();
    const std::size_t offset =
        ((entries - i) * kBankOffsetBytes) + cursor.U32();
    value.animation_data[i] =
        AdvanceAddress(at, static_cast<std::ptrdiff_t>(offset));
  }
  if (!cursor.Ok()) {
    return RefuseActor(host, "shared sequence bank", bank);
  }
  for (std::size_t i = 0; i < entries; ++i) {
    host.RecordEmbeddedSequenceBank(value.animation_data[i]);
  }
  value.animation_data[entries] = kEndOfAnimations;
  host.StoreSequenceBank(bank, value);
  for (int i = 0; i < kActorRecordSlots; ++i) {
    if (host.ActorSummary(i).present) {
      ActorAssetRecord actor = host.LoadActor(i);
      static_cast<void>(BindSharedActorSequence(
          value, bank, host.SharedSequenceIdsByKind(), actor));
      host.StoreActor(i, actor);
    }
  }
  return kAssetDone;
}

// The bank's effect texture: the parse is done once it has loaded; its
// refusals are not reported (30DB0.c:1772-1778).
int FinishBankEffectTexture(ActorAssetHost& host, ActorAssetLoadState& state,
                            const ActorAssetRequest& request) {
  if (UpdateSequenceEffectTextureLoad(host, state, request) == kAssetPending) {
    return kAssetPending;
  }
  state.sequence_parse_phase = kPhaseStart;
  return kAssetDone;
}
}  // namespace

// decomp: BATTLE.PRG _loadSeq (src/BATTLE/BATTLE.PRG/30DB0.c:1617)
int ParseActorSequence(ActorAssetHost& host, ActorAssetLoadState& state,
                       ActorAssetRequest& request) {
  switch (state.sequence_parse_phase) {
    case kPhaseStart:
      state.sequence_cursor = request.data;
      break;
    case kPhaseReading:
      return FinishBankEffectTexture(host, state, request);
    default:
      break;
  }
  FileCursor cursor(host, state.sequence_cursor);
  int result = kAssetDone;
  if (request.sub_kind == kBattleSequence) {
    result = ParseActorSequenceKind(host, request, cursor);
  } else if (request.sub_kind == kMaterialSequence ||
             request.sub_kind == kMaterialModelSequence ||
             request.sub_kind == kSpecialSequence) {
    result = ParseWorkSequence(host, request, cursor);
  } else {
    const int bank = request.target_slot - 1;
    if (bank < 0 || bank >= kSharedSequenceBanks) {
      return RefuseActor(host, "shared sequence bank", request.target_slot);
    }
    result = ParseSequenceBank(host, bank, cursor);
    state.sequence_cursor = cursor.At();
    if (result != kAssetDone || request.sub_kind == kCommonBankSequence) {
      return result;
    }
    state.sequence_parse_phase = kPhaseReading;
    return FinishBankEffectTexture(host, state, request);
  }
  state.sequence_cursor = cursor.At();
  return result;
}

// decomp: BATTLE.PRG func_8009CAEC (src/BATTLE/BATTLE.PRG/30DB0.c:1795)
bool BindSharedActorSequence(const SharedSequenceBank& bank, int bank_index,
                             std::span<const std::uint8_t> ids_by_kind,
                             ActorAssetRecord& actor) {
  if (bank_index < 0 || bank_index >= kSharedSequenceBanks) {
    return false;
  }
  const auto index = static_cast<std::size_t>(bank_index);
  const int model = actor.model_id;
  const int by_kind = model < static_cast<int>(ids_by_kind.size())
                          ? int{ids_by_kind[static_cast<std::size_t>(model)]}
                          : -1;
  for (std::size_t i = 0; i < bank.animation_ids.size(); ++i) {
    const int id = bank.animation_ids[i];
    if (id == kEndOfBank) {
      break;
    }
    if (id == by_kind || id == model) {
      actor.shared_sequences[index] =
          AdvanceAddress(bank.animation_data[i], kSharedAnimationHeaderBytes);
      break;
    }
  }
  actor.texture_row_offsets[index] = kNoRowOffset;
  for (std::size_t i = 0; i < bank.animation_ids.size(); ++i) {
    const int id = bank.animation_ids[i];
    if (id == kEndOfBank) {
      break;
    }
    if (id == model) {
      actor.texture_row_offsets[index] =
          static_cast<std::uint8_t>(bank.entry_params[i]);
      break;
    }
  }
  return true;
}

namespace {
int ReleaseBattleSequence(ActorAssetHost& host, int target) {
  if (target < 0 || target >= kActorRecordSlots ||
      !host.ActorSummary(target).present) {
    return -1;
  }
  ActorAssetRecord actor = host.LoadActor(target);
  if (actor.battle_sequence == AssetAddress::kNone) {
    return 0;
  }
  if (target != 0) {
    ActorAssetRequest probe;
    probe.target_slot = static_cast<std::int8_t>(target);
    probe.model_id = actor.sequence_model_id;
    if ((target == 1 && actor.model_id == 0) ||
        FindSharedActorSequence(host, probe) == 0) {
      host.FreeLow(actor.battle_sequence);
    }
  }
  actor.battle_sequence = AssetAddress::kNone;
  actor.sequence_model_id = 0;
  actor.common_animation_count = 0;
  host.StoreActor(target, actor);
  return 0;
}

int ReleaseSequenceBank(ActorAssetHost& host, int target) {
  const int bank = target - 1;
  if (bank < 0 || bank >= kSharedSequenceBanks) {
    // The original clears shared[-1] (the battle sequence) for a bank 0.
    host.Refuse("shared sequence bank", target);
    return -1;
  }
  for (int i = 0; i < kActorRecordSlots; ++i) {
    if (host.ActorSummary(i).present) {
      ActorAssetRecord actor = host.LoadActor(i);
      actor.shared_sequences[static_cast<std::size_t>(bank)] =
          AssetAddress::kNone;
      host.StoreActor(i, actor);
    }
  }
  if (const int result = ReleaseSequenceEffectTexture(host, target);
      result < 0) {
    return result;
  }
  SharedSequenceBank value = host.LoadSequenceBank(bank);
  if (value.data != AssetAddress::kNone) {
    host.FreeLow(value.data);
  }
  value.data = AssetAddress::kNone;
  host.StoreSequenceBank(bank, value);
  return 0;
}

int ReleaseSpecialSequence(ActorAssetHost& host, int target) {
  if (target < 0 || target >= kActorRecordSlots ||
      !host.ActorSummary(target).present) {
    return -1;
  }
  ActorAssetRecord actor = host.LoadActor(target);
  if (actor.shared_sequences[kSpecialSequenceRef] == AssetAddress::kNone) {
    return 0;
  }
  host.FreeLow(actor.shared_sequences[kSpecialSequenceRef]);
  actor.shared_sequences[kSpecialSequenceRef] = AssetAddress::kNone;
  actor.battle_animation_count = 0;
  host.StoreActor(target, actor);
  return 0;
}
}  // namespace

// decomp: BATTLE.PRG func_8009CC20 (src/BATTLE/BATTLE.PRG/30DB0.c:1828)
int ReleaseActorSequence(ActorAssetHost& host, int target, int kind) {
  switch (kind) {
    case kBattleSequence:
      return ReleaseBattleSequence(host, target);
    case kCommonBankSequence:
    case kEffectBankSequence:
      return ReleaseSequenceBank(host, target);
    case kMaterialModelSequence:
    case kSpecialSequence:
      return ReleaseSpecialSequence(host, target);
    default:
      return 0;
  }
}

// decomp: BATTLE.PRG func_8009D208 (src/BATTLE/BATTLE.PRG/30DB0.c:1996)
int ReleaseSequenceEffectTexture(ActorAssetHost& host, int bank) {
  if (bank < kFirstEffectBank || bank > kLastEffectBank) {
    return -1;
  }
  const std::uint8_t page = host.BankTexturePage(bank - 1);
  if (page != 0) {
    host.ResetActorTextureFrames(bank - 1);
    if (host.RetireTexturePages(page, 1) == 0) {
      return kAssetBusy;
    }
    host.SetBankTexturePage(bank - 1, 0);
  }
  return 0;
}

namespace {
int StartEffectTextureRead(ActorAssetHost& host, ActorAssetLoadState& state,
                           const ActorAssetRequest& request) {
  state.effect_texture_phase = kPhaseReading;
  const auto model =
      static_cast<std::size_t>(static_cast<std::uint16_t>(request.model_id));
  const std::optional<std::uint32_t> sectors =
      request.model_id < 0
          ? std::nullopt
          : host.FileTableEntry(AssetFileTable::kEtmSectors, model);
  const std::optional<std::uint32_t> offset =
      request.model_id < 0
          ? std::nullopt
          : host.FileTableEntry(AssetFileTable::kEtmOffsets, model);
  if (!sectors || !offset) {
    state.effect_texture_phase = kPhaseStart;
    return RefuseActor(host, "effect texture file table", request.model_id);
  }
  if (*sectors == 0) {
    state.effect_texture_phase = kPhaseStart;
    return kAssetRefused;
  }
  const std::uint32_t bytes = *sectors << kSectorShift;
  state.effect_texture_file = host.AllocateHigh(bytes);
  if (state.effect_texture_file == AssetAddress::kNone) {
    // The original leaves the phase at 1 with no read started.
    return kAssetRefused;
  }
  StartFileRead(host, AssetFileStream::kEffectTexture,
                {static_cast<std::int32_t>(*offset) + kEtmFirstSector,
                 static_cast<std::int32_t>(bytes)},
                state.effect_texture_file);
  return kAssetPending;
}

int UploadEffectTexture(ActorAssetHost& host, ActorAssetLoadState& state,
                        const ActorAssetRequest& request) {
  if (!host.FileReadFinished(AssetFileStream::kEffectTexture)) {
    return kAssetPending;
  }
  host.ReleaseFileRead(AssetFileStream::kEffectTexture);
  const int bank = request.target_slot - 1;
  if (const std::uint8_t page = host.BankTexturePage(bank); page != 0) {
    if (host.RetireTexturePages(page, 1) == 0) {
      return kAssetBusy;
    }
    host.SetBankTexturePage(bank, 0);
  }
  if (host.ReserveTexturePages(request.texture_slot, 1) == 0) {
    return kAssetRefused;
  }
  host.SetBankTexturePage(bank, request.texture_slot);
  const std::span<const std::byte> header = host.Bytes(
      AdvanceAddress(state.effect_texture_file, kEffectTextureHeightAt),
      sizeof(std::uint16_t));
  state.effect_texture_file =
      AdvanceAddress(state.effect_texture_file, kEffectTextureHeaderBytes);
  state.effect_texture_phase = kPhaseParsing;
  if (header.size() < sizeof(std::uint16_t)) {
    host.Refuse("effect texture header", request.model_id);
    return kAssetPending;
  }
  const VramRect rect{.x = static_cast<std::int16_t>(
                          (request.texture_slot & kEffectTextureColumnMask) *
                          kEffectTextureColumnWidth),
                      .y = kEffectTextureY,
                      .w = kEffectTextureWidth,
                      .h = static_cast<std::int16_t>(
                          ReadLittle(header, 0, sizeof(std::uint16_t)))};
  static_cast<void>(
      UploadFrom(host, rect, state.effect_texture_file, "effect texture"));
  return kAssetPending;
}
}  // namespace

// decomp: BATTLE.PRG _loadEtm (src/BATTLE/BATTLE.PRG/30DB0.c:2008)
int UpdateSequenceEffectTextureLoad(ActorAssetHost& host,
                                    ActorAssetLoadState& state,
                                    const ActorAssetRequest& request) {
  if (request.target_slot < kFirstEffectBank ||
      request.target_slot > kLastEffectBank) {
    return kAssetRefused;
  }
  switch (state.effect_texture_phase) {
    default:
      if (const int started = StartEffectTextureRead(host, state, request);
          started != kAssetPending) {
        return started;
      }
      [[fallthrough]];
    case kPhaseReading:
      return UploadEffectTexture(host, state, request);
    case kPhaseParsing:
      host.FreeHigh(AdvanceAddress(state.effect_texture_file,
                                   -kEffectTextureHeaderBytes));
      state.effect_texture_phase = kPhaseStart;
      return kAssetDone;
  }
}
}  // namespace VS::Content
