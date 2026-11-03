#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ActorAssets.hpp"
#include "Content/ActorAssetsLive.hpp"

namespace {
namespace Content = VS::Content;
using Content::ActorAssetKind;
using Content::ActorAssetLoadState;
using Content::ActorAssetQueueStats;
using Content::ActorAssetRecord;
using Content::ActorAssetRequest;
using Content::AssetAddress;
using Content::AssetFileStream;
using Content::AssetFileTable;
using Content::AssetTextureSlot;
using Content::AssetTextureTable;
using Content::kAssetBusy;
using Content::kAssetDone;
using Content::kAssetPending;
using Content::kAssetRefused;
using Content::SharedSequenceBank;
using Content::WeaponAssetRecord;

// Fake heap blocks sit far above the shared record indices a request's data
// field can carry.
constexpr std::uintptr_t kFirstBlock = 0x1000000;
constexpr std::uintptr_t kBlockStride = 0x100000;
constexpr std::size_t kSectorBytes = 2048;

// AssetAddress is an opaque address handle; any address is valid.
constexpr AssetAddress Address(std::uintptr_t value) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<AssetAddress>(value);
}

constexpr std::uintptr_t Raw(AssetAddress address) {
  return static_cast<std::uintptr_t>(address);
}

constexpr AssetAddress Offset(AssetAddress address, std::uintptr_t bytes) {
  return Address(Raw(address) + bytes);
}

constexpr std::size_t Index(int value) {
  return static_cast<std::size_t>(value);
}

void Put16(std::span<std::byte> bytes, std::size_t at, std::uint32_t value) {
  bytes[at] = static_cast<std::byte>(value & 0xFFU);
  bytes[at + 1] = static_cast<std::byte>((value >> 8U) & 0xFFU);
}

void Put32(std::span<std::byte> bytes, std::size_t at, std::uint32_t value) {
  Put16(bytes, at, value & 0xFFFFU);
  Put16(bytes, at + 2, value >> 16U);
}

bool SameRect(const Content::VramRect& left, const Content::VramRect& right) {
  return left.x == right.x && left.y == right.y && left.w == right.w &&
         left.h == right.h;
}

struct FileRead {
  AssetFileStream stream = AssetFileStream::kWeapon;
  Content::DiscFileExtent file;
  AssetAddress destination = AssetAddress::kNone;
};

struct ImageUpload {
  Content::VramRect rect;
  std::size_t bytes = 0;
  std::byte first{};
};

struct ClutUpload {
  std::uint32_t row = 0;
  std::uint32_t first = 0;
  std::uint32_t count = 0;
};

struct ActorPlacement {
  int slot = 0;
  std::uint32_t placement = 0;
  int mode = 0;
};

struct PageRange {
  std::uint32_t first = 0;
  std::uint32_t count = 0;
};

struct BlockRecord {
  AssetAddress destination = AssetAddress::kNone;
  AssetAddress source = AssetAddress::kNone;
  std::size_t bytes = 0;
};

// Records the loaders' requests, reads, uploads and frees. Heap blocks are
// byte vectors at fake addresses; a read the queue accepts or refuses copies
// the disc file at its sector into the destination block at once.
class FakeAssetHost final : public Content::ActorAssetHost {
 public:
  FakeAssetHost() {
    weapon_buffers = {AddBlock(Content::kWeaponGeometryBytes),
                      AddBlock(Content::kWeaponGeometryBytes)};
    primary_sequence = AddBlock(Content::kWeaponSequenceBytes);
    work_sequence = AddBlock(Content::kWeaponSequenceBytes);
  }

  AssetAddress AddBlock(std::size_t bytes) {
    const AssetAddress at = Address(next_block_);
    next_block_ += kBlockStride;
    blocks_.emplace(Raw(at), std::vector<std::byte>(bytes));
    return at;
  }

  std::span<std::byte> Block(AssetAddress at) { return Bytes(at, 0); }

  void SetTable(AssetFileTable table, std::size_t index, std::uint32_t value) {
    tables_[{table, index}] = value;
  }

  void AddDiscFile(std::int32_t sector, std::vector<std::byte> bytes) {
    disc_[sector] = std::move(bytes);
  }

  std::uint8_t RequestKind(int slot) const override {
    return requests.at(Index(slot)).kind;
  }
  void StoreRequest(int slot, const ActorAssetRequest& request) override {
    requests.at(Index(slot)) = request;
  }
  void OnRequestQueued(const ActorAssetRequest& request) override {
    queued.push_back(request);
  }
  void MoveRequest(int from, int to) override {
    requests.at(Index(to)) = requests.at(Index(from));
  }
  void ClearRequestKind(int slot) override {
    requests.at(Index(slot)).kind = 0;
  }
  int ProcessHeadRequest(ActorAssetKind kind) override {
    processed.push_back(kind);
    frame_counter += process_ticks;
    return process_result;
  }
  std::int32_t FrameCounter() override { return frame_counter; }
  std::uint8_t ModelLoadPhase() const override { return load_phase; }
  void OnHeadRequestDone(int result) override { finished.push_back(result); }

  AssetAddress AllocateHigh(std::size_t bytes) override {
    return Allocate(bytes, high_blocks);
  }
  AssetAddress AllocateLow(std::size_t bytes) override {
    return Allocate(bytes, low_blocks);
  }
  void FreeHigh(AssetAddress block) override { high_frees.push_back(block); }
  void FreeLow(AssetAddress block) override { low_frees.push_back(block); }
  std::span<std::byte> Bytes(AssetAddress address,
                             std::size_t /*wanted*/) override {
    auto block = blocks_.upper_bound(Raw(address));
    if (block == blocks_.begin()) {
      return {};
    }
    --block;
    const std::uintptr_t at = Raw(address) - block->first;
    if (at > block->second.size()) {
      return {};
    }
    return std::span<std::byte>(block->second).subspan(at);
  }

  std::optional<std::uint32_t> FileTableEntry(AssetFileTable table,
                                              std::size_t index) override {
    const auto entry = tables_.find({table, index});
    if (entry == tables_.end()) {
      return std::nullopt;
    }
    return entry->second;
  }
  bool BeginFileRead(AssetFileStream stream, Content::DiscFileExtent file,
                     AssetAddress destination) override {
    reads.push_back({stream, file, destination});
    if (accept_reads) {
      Deliver(file, destination);
    }
    return accept_reads;
  }
  void ReadRefusedFile(Content::DiscFileExtent file,
                       AssetAddress destination) override {
    refused_reads.push_back({AssetFileStream::kWeapon, file, destination});
    Deliver(file, destination);
  }
  bool FileReadFinished(AssetFileStream /*stream*/) override {
    return read_finished;
  }
  void ReleaseFileRead(AssetFileStream stream) override {
    released_reads.push_back(stream);
  }

  AssetTextureSlot TextureSlot(AssetTextureTable table,
                               int index) const override {
    return table == AssetTextureTable::kActor
               ? actor_textures.at(Index(index))
               : weapon_textures.at(Index(index));
  }
  void SetTextureSlot(AssetTextureTable table, int index,
                      const AssetTextureSlot& slot) override {
    if (table == AssetTextureTable::kActor) {
      actor_textures.at(Index(index)) = slot;
    } else {
      weapon_textures.at(Index(index)) = slot;
    }
  }
  std::span<const std::uint8_t> TextureSlotOrigins() const override {
    return origins;
  }
  std::span<const std::uint8_t> TextureSlotRowOffsets() const override {
    return rows;
  }
  int ReserveTexturePages(std::uint32_t first, std::uint32_t count) override {
    reserved_pages.push_back({first, count});
    return reserve_result;
  }
  int RetireTexturePages(std::uint32_t first, std::uint32_t count) override {
    retired_pages.push_back({first, count});
    return retire_result;
  }
  void UploadImage(const Content::VramRect& rect,
                   std::span<const std::byte> pixels) override {
    images.push_back(
        {rect, pixels.size(), pixels.empty() ? std::byte{} : pixels.front()});
  }
  void UploadClut(std::span<const std::uint16_t> /*colors*/, std::uint32_t row,
                  std::uint32_t first, std::uint32_t count) override {
    cluts.push_back({row, first, count});
  }

  Content::WeaponAssetSummary WeaponSummary(int slot) const override {
    const std::optional<WeaponAssetRecord>& model = weapons.at(Index(slot));
    if (!model) {
      return {};
    }
    return {.present = true,
            .actor_id = model->actor_id,
            .model_id = model->model_id,
            .bone_count = model->bone_count};
  }
  bool BindWeaponRecord(int slot, AssetAddress destination) override {
    if (destination == AssetAddress::kNone) {
      return false;
    }
    WeaponAssetRecord model;
    model.texture_slot = Content::kNoAssetTextureSlot;
    weapons.at(Index(slot)) = model;
    return true;
  }
  WeaponAssetRecord LoadWeapon(int slot) const override {
    return weapons.at(Index(slot)).value_or(WeaponAssetRecord{});
  }
  void StoreWeapon(int slot, const WeaponAssetRecord& record) override {
    weapons.at(Index(slot)) = record;
  }
  void CopyWeaponRecord(int target, int source) override {
    weapons.at(Index(target)) = weapons.at(Index(source));
  }
  void WriteWeaponBones(int slot, std::span<const std::byte> bones) override {
    weapon_bones.at(Index(slot)).assign(bones.begin(), bones.end());
  }
  void ClearWeaponSlot(int slot) override {
    weapons.at(Index(slot)).reset();
    cleared_weapons.push_back(slot);
  }
  AssetAddress WeaponGeometryBuffer(int slot) const override {
    return weapon_buffers.at(Index(slot));
  }
  std::span<const std::uint8_t> WeaponMaterialPalettes() const override {
    return material_palettes;
  }
  int FindActorBoneTag(int actor, int tag) override {
    bone_tag_queries.emplace_back(actor, tag);
    return bone_tag;
  }

  Content::ActorAssetSummary ActorSummary(int slot) const override {
    const std::optional<ActorAssetRecord>& actor = actors.at(Index(slot));
    if (!actor) {
      return {};
    }
    return {.present = true,
            .model_id = actor->model_id,
            .sequence_model_id = actor->sequence_model_id,
            .common_sequence = actor->common_sequence};
  }
  bool BindActorRecord(int slot, AssetAddress destination) override {
    if (destination == AssetAddress::kNone) {
      return false;
    }
    actors.at(Index(slot)) = ActorAssetRecord{};
    return true;
  }
  ActorAssetRecord LoadActor(int slot) const override {
    return actors.at(Index(slot)).value_or(ActorAssetRecord{});
  }
  void StoreActor(int slot, const ActorAssetRecord& record) override {
    actors.at(Index(slot)) = record;
  }
  void CopyActorRecord(int target, int source) override {
    actors.at(Index(target)) = actors.at(Index(source));
  }
  void WriteActorTextureLayout(int /*slot*/,
                               std::span<const std::byte> /*layout*/) override {
    ++texture_layout_writes;
  }
  void ClearActorSlot(int slot) override {
    actors.at(Index(slot)).reset();
    cleared_actors.push_back(slot);
  }
  std::int16_t RoomActorScale() const override { return room_scale; }
  void ResetActorMotion(int slot) override { motion_resets.push_back(slot); }
  void ClearActorPaletteFade(int slot) override { fade_clears.push_back(slot); }
  void PlaceActorInRoom(int slot, std::uint32_t placement, int mode) override {
    placements.push_back({slot, placement, mode});
  }
  void AttachActorToParentBone(int slot, int /*parent*/,
                               int /*bone*/) override {
    attached_actors.push_back(slot);
  }
  void StopModelSounds(AssetAddress sound_bank) override {
    stopped_sounds.push_back(sound_bank);
  }

  SharedSequenceBank LoadSequenceBank(int bank) const override {
    return banks.at(Index(bank));
  }
  void StoreSequenceBank(int bank, const SharedSequenceBank& value) override {
    banks.at(Index(bank)) = value;
  }
  std::uint8_t BankTexturePage(int bank) const override {
    return bank_pages.at(Index(bank));
  }
  void SetBankTexturePage(int bank, std::uint8_t page) override {
    bank_pages.at(Index(bank)) = page;
  }
  std::span<const std::uint8_t> SharedSequenceIdsByKind() const override {
    return ids_by_kind;
  }
  AssetAddress PrimarySequenceBuffer() const override {
    return primary_sequence;
  }
  AssetAddress SequenceWorkBuffer() const override { return work_sequence; }
  void ResetActorTextureFrames(int bank) override {
    texture_frame_resets.push_back(bank);
  }

  void RecordModelGeometry(AssetAddress model, std::size_t bytes) override {
    geometry_records.push_back({model, AssetAddress::kNone, bytes});
  }
  void RecordModelCopy(AssetAddress destination, AssetAddress source,
                       std::size_t bytes) override {
    model_copies.push_back({destination, source, bytes});
  }
  void ForgetModelGeometry(AssetAddress model) override {
    forgotten_geometry.push_back(model);
  }
  void RecordSequenceBank(AssetAddress bank, std::size_t bytes) override {
    sequence_banks.push_back({bank, AssetAddress::kNone, bytes});
  }
  void RecordEmbeddedSequenceBank(AssetAddress file) override {
    embedded_banks.push_back(file);
  }
  void RecordSequenceCopy(AssetAddress destination, AssetAddress source,
                          std::size_t bytes) override {
    sequence_copies.push_back({destination, source, bytes});
  }
  void Refuse(const char* what, std::int64_t /*value*/) override {
    refusals.emplace_back(what);
  }

  std::array<ActorAssetRequest, Content::kActorAssetRequestCount> requests{};
  std::vector<ActorAssetRequest> queued;
  std::vector<ActorAssetKind> processed;
  std::vector<int> finished;
  std::int32_t frame_counter = 0;
  std::int32_t process_ticks = 0;
  int process_result = kAssetDone;
  std::uint8_t load_phase = 0;

  bool refuse_allocations = false;
  std::vector<AssetAddress> high_blocks;
  std::vector<AssetAddress> low_blocks;
  std::vector<AssetAddress> high_frees;
  std::vector<AssetAddress> low_frees;

  bool accept_reads = true;
  bool read_finished = true;
  std::vector<FileRead> reads;
  std::vector<FileRead> refused_reads;
  std::vector<AssetFileStream> released_reads;

  std::array<AssetTextureSlot, Content::kActorTextureSlots> actor_textures{};
  std::array<AssetTextureSlot, Content::kWeaponTextureSlots> weapon_textures{};
  std::vector<std::uint8_t> origins;
  std::vector<std::uint8_t> rows;
  int reserve_result = 1;
  int retire_result = 1;
  std::vector<PageRange> reserved_pages;
  std::vector<PageRange> retired_pages;
  std::vector<ImageUpload> images;
  std::vector<ClutUpload> cluts;

  std::array<std::optional<WeaponAssetRecord>, Content::kWeaponModelSlots>
      weapons{};
  std::array<std::vector<std::byte>, Content::kWeaponModelSlots> weapon_bones{};
  std::vector<int> cleared_weapons;
  std::array<AssetAddress, 2> weapon_buffers{};
  std::vector<std::uint8_t> material_palettes;
  int bone_tag = 0;
  std::vector<std::pair<int, int>> bone_tag_queries;

  std::array<std::optional<ActorAssetRecord>, Content::kActorRecordSlots>
      actors{};
  std::vector<int> cleared_actors;
  std::vector<int> motion_resets;
  std::vector<int> fade_clears;
  std::vector<int> attached_actors;
  std::vector<ActorPlacement> placements;
  std::vector<AssetAddress> stopped_sounds;
  std::int16_t room_scale = 0;
  int texture_layout_writes = 0;

  std::array<SharedSequenceBank, Content::kSharedSequenceBanks> banks{};
  std::array<std::uint8_t, Content::kSharedSequenceBanks> bank_pages{};
  std::vector<int> texture_frame_resets;
  std::vector<std::uint8_t> ids_by_kind;
  AssetAddress primary_sequence = AssetAddress::kNone;
  AssetAddress work_sequence = AssetAddress::kNone;

  std::vector<BlockRecord> geometry_records;
  std::vector<BlockRecord> model_copies;
  std::vector<BlockRecord> sequence_banks;
  std::vector<BlockRecord> sequence_copies;
  std::vector<AssetAddress> forgotten_geometry;
  std::vector<AssetAddress> embedded_banks;
  std::vector<std::string> refusals;

 private:
  AssetAddress Allocate(std::size_t bytes, std::vector<AssetAddress>& log) {
    if (refuse_allocations) {
      return AssetAddress::kNone;
    }
    const AssetAddress at = AddBlock(bytes);
    log.push_back(at);
    return at;
  }

  void Deliver(const Content::DiscFileExtent& file, AssetAddress destination) {
    const auto disc_file = disc_.find(file.sector);
    if (disc_file == disc_.end()) {
      return;
    }
    const std::span<std::byte> to = Bytes(destination, 0);
    std::copy_n(disc_file->second.begin(),
                std::min(to.size(), disc_file->second.size()), to.begin());
  }

  std::uintptr_t next_block_ = kFirstBlock;
  std::map<std::uintptr_t, std::vector<std::byte>> blocks_;
  std::map<std::pair<AssetFileTable, std::size_t>, std::uint32_t> tables_;
  std::map<std::int32_t, std::vector<std::byte>> disc_;
};

ActorAssetRequest Request(ActorAssetKind kind, int target, int model) {
  ActorAssetRequest request;
  request.kind = static_cast<std::uint8_t>(kind);
  request.target_slot = static_cast<std::int8_t>(target);
  request.model_id = static_cast<std::int16_t>(model);
  return request;
}

// A WEP file (30DB0.c:640-800): two bones, one flat triangle, a 32-pixel wide
// texture with one CLUT, then the bones.
constexpr int kWepModel = 12;
constexpr std::uint32_t kWepTableOffset = 40;
constexpr std::int32_t kWepSector = 40 + 104024;
constexpr std::size_t kWepGeometryAt = 0x10;
constexpr std::uint32_t kWepGeometryBytes = 0x50;
constexpr std::size_t kWepPolygonAt = 0x40;
constexpr std::size_t kWepTextureBytes = 1024;
constexpr std::size_t kTriangleUvAt = 10;

std::size_t PutWeaponPalettes(std::span<std::byte> bytes, std::size_t at) {
  for (std::uint32_t color = 0; color < 16; ++color, at += 2) {
    Put16(bytes, at, 0x1000 + color);
  }
  for (std::uint32_t palette = 0; palette < 7; ++palette) {
    for (std::uint32_t color = 0; color < 32; ++color, at += 2) {
      Put16(bytes, at, 0x2000 + (palette << 8U) + color);
    }
  }
  return at;
}

std::vector<std::byte> WeaponFile() {
  std::vector<std::byte> file(kSectorBytes);
  const std::span<std::byte> bytes(file);
  bytes[4] = std::byte{2};
  bytes[5] = std::byte{1};
  Put16(bytes, 6, 1);
  Put32(bytes, 0xC, kWepGeometryBytes);
  const std::span<std::byte> geometry =
      bytes.subspan(kWepGeometryAt, kWepGeometryBytes);
  Put32(geometry, 0x3C, static_cast<std::uint32_t>(kWepPolygonAt));
  geometry[kWepPolygonAt] = std::byte{0x24};
  std::ranges::fill(geometry.subspan(kWepPolygonAt + kTriangleUvAt, 6),
                    std::byte{0x08});
  std::size_t at = kWepGeometryAt + kWepGeometryBytes + 5;
  bytes[at] = std::byte{16};
  at = PutWeaponPalettes(bytes, at + 3);
  bytes[at] = std::byte{0x77};
  at += kWepTextureBytes;
  for (std::size_t bone = 0; bone < 16; ++bone) {
    bytes[at + bone] = static_cast<std::byte>(0xB0 + bone);
  }
  return file;
}

void AddWeaponFile(FakeAssetHost& host) {
  host.SetTable(AssetFileTable::kWepSectors, Index(kWepModel), 1);
  host.SetTable(AssetFileTable::kWepOffsets, Index(kWepModel), kWepTableOffset);
  host.AddDiscFile(kWepSector, WeaponFile());
  host.material_palettes = {3, 0};
  host.bone_tag = 5;
}

ActorAssetRequest WeaponRequest(FakeAssetHost& host) {
  ActorAssetRequest request =
      Request(ActorAssetKind::kWeaponModelLoad, 4, kWepModel);
  request.destination = host.AddBlock(16);
  request.sub_kind = 1;
  request.texture_slot = 2;
  return request;
}

// An ETM file: the image height at +2 and the pixels after the header.
constexpr std::int32_t kEtmSector = 6 + 104351;

void AddEffectTextureFile(FakeAssetHost& host) {
  host.SetTable(AssetFileTable::kEtmSectors, 4, 1);
  host.SetTable(AssetFileTable::kEtmOffsets, 4, 6);
  std::vector<std::byte> file(kSectorBytes);
  file[2] = std::byte{8};
  file[4] = std::byte{0x5A};
  host.AddDiscFile(kEtmSector, std::move(file));
}

ActorAssetRequest EffectTextureRequest() {
  ActorAssetRequest request = Request(ActorAssetKind::kEffectTextureLoad, 2, 4);
  request.texture_slot = 0x13;
  return request;
}

// A shared sequence bank file with two entries: ids 10 and 9, parameters
// 0x30 and 0x41, animations at offsets 0 and 0x10 past the offset table.
AssetAddress AddSequenceBankFile(FakeAssetHost& host, std::uint32_t count) {
  const AssetAddress file = host.AddBlock(64);
  const std::span<std::byte> bytes = host.Block(file);
  Put32(bytes, 0, count);
  Put16(bytes, 4, 10);
  Put16(bytes, 8, 9);
  Put16(bytes, 12, 0x30);
  Put16(bytes, 16, 0x41);
  Put32(bytes, 24, 0x10);
  return file;
}

ActorAssetRecord ActorWithModel(std::uint16_t model) {
  ActorAssetRecord actor;
  actor.model_id = model;
  return actor;
}

// Catch2's REQUIRE throws, but clang-tidy cannot see that; the explicit throw
// lets bugprone-unchecked-optional-access prove the accesses through it.
template <typename T>
T& Required(std::optional<T>& value) {
  REQUIRE(value.has_value());
  if (!value) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *value;
}

bool SameRead(const FileRead& read, AssetFileStream stream, std::int32_t sector,
              std::int32_t bytes) {
  return read.stream == stream && read.file.sector == sector &&
         read.file.bytes == bytes;
}

bool SameClut(const ClutUpload& left, const ClutUpload& right) {
  return left.row == right.row && left.first == right.first &&
         left.count == right.count;
}

std::vector<std::int32_t> ReadSectors(const FakeAssetHost& host) {
  std::vector<std::int32_t> sectors;
  for (const FileRead& read : host.reads) {
    sectors.push_back(read.file.sector);
  }
  return sectors;
}

// Runs load steps whose results the test checks elsewhere.
void StepWeaponLoad(FakeAssetHost& host, ActorAssetLoadState& state,
                    ActorAssetRequest& request, int steps) {
  for (int i = 0; i < steps; ++i) {
    static_cast<void>(Content::UpdateWeaponModelLoad(host, state, request));
  }
}

void StepEffectTextureLoad(FakeAssetHost& host, ActorAssetLoadState& state,
                           const ActorAssetRequest& request, int steps) {
  for (int i = 0; i < steps; ++i) {
    static_cast<void>(
        Content::UpdateSequenceEffectTextureLoad(host, state, request));
    host.read_finished = true;
  }
}

// A pending model parse that takes 7 frames from frame 100.
void SetSlowModelParse(FakeAssetHost& host) {
  host.requests[0].kind = 2;
  host.process_result = kAssetPending;
  host.frame_counter = 100;
  host.process_ticks = 7;
  host.load_phase = 2;
}

// Two colored polygons of 24 bytes: a triangle (0x34) and a quad (0x3C).
std::array<std::byte, 56> ColoredPolygons() {
  std::array<std::byte, 56> polygons{};
  polygons.fill(std::byte{1});
  polygons[11] = std::byte{0x34};
  polygons[24 + 11] = std::byte{0x3C};
  return polygons;
}

// Model 20 has no SHP file; model 0's is 2 sectors at offset 7.
void AddShpTables(FakeAssetHost& host) {
  host.SetTable(AssetFileTable::kShpSectors, 20, 0);
  host.SetTable(AssetFileTable::kShpSectors, 0, 2);
  host.SetTable(AssetFileTable::kShpOffsets, 0, 7);
}

ActorAssetRequest ModellessRequest(FakeAssetHost& host) {
  ActorAssetRequest request =
      Request(ActorAssetKind::kActorModelLoad, 4, Content::kModellessActorId);
  request.destination = host.AddBlock(8);
  request.placement = 0x01020304;
  return request;
}

// Actor 3 loads model 9, which actor 2 holds with texture slots 5 and 6.
ActorAssetRequest SharedModelRequest(FakeAssetHost& host) {
  ActorAssetRecord peer = ActorWithModel(9);
  peer.holds_texture_slots = true;
  peer.texture_slot = 5;
  peer.texture_slot_count_minus_one = 1;
  host.actors[2] = peer;
  host.room_scale = 0x1234;
  ActorAssetRequest request = Request(ActorAssetKind::kActorModelLoad, 3, 9);
  request.destination = host.AddBlock(8);
  request.sub_kind = 0xFF;
  request.material = 2;
  request.placement = 0xAABB;
  return request;
}

// Actor 3's battle sequence 2 is at sector 5000; sequence 3 has no file.
void AddBattleSequenceActor(FakeAssetHost& host) {
  ActorAssetRecord actor = ActorWithModel(9);
  actor.battle_sequence_lbas[2] = 5000;
  actor.chain_attack_animation_ids[2] = 0x1800;
  actor.chain_attack_animation_ids[3] = 0x7FF;
  host.actors[3] = actor;
  host.read_finished = false;
}

void AddSequenceTables(FakeAssetHost& host) {
  host.SetTable(AssetFileTable::kSequenceSectors, 5, 2);
  host.SetTable(AssetFileTable::kSequenceOffsets, 5, 10);
  host.SetTable(AssetFileTable::kMaterialSequenceSectors, 6, 1);
  host.SetTable(AssetFileTable::kMaterialSequenceOffsets, 6, 20);
  host.SetTable(AssetFileTable::kMaterialModelSequenceSectors, 7, 0);
  host.SetTable(AssetFileTable::kMaterialModelSequenceSectors, 8, 1);
  host.SetTable(AssetFileTable::kMaterialModelSequenceOffsets, 8, 30);
  host.read_finished = false;
}

int StartSequenceLoad(FakeAssetHost& host, ActorAssetRequest& request) {
  ActorAssetLoadState state;
  return Content::UpdateActorSequenceLoad(host, state, request);
}

// Actors 2-4 hold model 9; actor 2's common sequence has flags 5.
AssetAddress AddCommonSequencePeers(FakeAssetHost& host) {
  ActorAssetRecord peer = ActorWithModel(9);
  peer.sequence_flags = 5;
  peer.common_sequence = host.AddBlock(8);
  host.actors[2] = peer;
  host.actors[3] = ActorWithModel(9);
  host.actors[4] = ActorWithModel(9);
  return peer.common_sequence;
}

// A common sequence file with flags 7 and 6 bytes of data ending in 0xC7.
AssetAddress OwnCommonSequenceFile(FakeAssetHost& host) {
  const AssetAddress file = host.AddBlock(16);
  host.Block(file)[0] = std::byte{7};
  Put32(host.Block(file), 4, 6);
  host.Block(file)[15] = std::byte{0xC7};
  return file;
}

// Parses a two-entry bank into shared bank 1 with actors 1 (model 9) and 4
// (model 10) present.
AssetAddress ParseSequenceBank(FakeAssetHost& host) {
  host.actors[1] = ActorWithModel(9);
  host.actors[4] = ActorWithModel(10);
  host.ids_by_kind.assign(16, 0);
  host.ids_by_kind[9] = 77;
  const AssetAddress file = AddSequenceBankFile(host, 2);
  ActorAssetLoadState state;
  ActorAssetRequest request = Request(ActorAssetKind::kSequenceParse, 2, 0);
  request.sub_kind = 1;
  request.data = file;
  REQUIRE(Content::ParseActorSequence(host, state, request) == kAssetDone);
  return file;
}

ActorAssetRecord BattleSequenceActor(FakeAssetHost& host) {
  ActorAssetRecord actor = ActorWithModel(9);
  actor.sequence_model_id = 2;
  actor.common_animation_count = 3;
  actor.battle_sequence = host.AddBlock(4);
  return actor;
}
}  // namespace

TEST_CASE("Battle asset requests take the first free slot of the queue",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  host.requests[0].kind = 1;
  CHECK(Content::FindFreeActorAssetRequest(host) == 1);
  const ActorAssetRequest request =
      Request(ActorAssetKind::kWeaponModelLoad, 4, 9);
  CHECK(Content::QueueActorAssetRequest(host, request) == 1);
  CHECK(host.requests[1] == request);
  REQUIRE(host.queued.size() == 1);
  CHECK(host.queued[0] == request);
  for (ActorAssetRequest& slot : host.requests) {
    slot.kind = 1;
  }
  CHECK(Content::FindFreeActorAssetRequest(host) == -1);
  CHECK(Content::QueueActorAssetRequest(host, request) == -1);
  CHECK(host.queued.size() == 1);
}

TEST_CASE("Battle asset queue dispatch skips empty and undispatched requests",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  host.frame_counter = 50;
  ActorAssetQueueStats stats;
  CHECK(Content::UpdateActorAssetQueue(host, stats) == 0);
  CHECK(stats.frame_ticks == 50);
  host.requests[0].kind = 5;
  CHECK(Content::UpdateActorAssetQueue(host, stats) == -1);
  host.requests[0].kind = 10;
  CHECK(Content::UpdateActorAssetQueue(host, stats) == -1);
  CHECK(host.processed.empty());
  CHECK(host.finished.empty());
  CHECK(host.requests[0].kind == 10);
}

TEST_CASE("Battle asset queue keeps a pending head and shifts a finished one",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  host.requests[0] = Request(ActorAssetKind::kActorModelLoad, 0, 1);
  host.requests[1] = Request(ActorAssetKind::kWeaponModelLoad, 0, 2);
  host.requests[14] = Request(ActorAssetKind::kSequenceLoad, 0, 14);
  host.requests[15] = Request(ActorAssetKind::kEffectTextureLoad, 0, 15);
  host.process_result = kAssetPending;
  ActorAssetQueueStats stats;
  CHECK(Content::UpdateActorAssetQueue(host, stats) == 1);
  CHECK(host.finished.empty());
  host.process_result = kAssetDone;
  CHECK(Content::UpdateActorAssetQueue(host, stats) == 1);
  CHECK(host.finished == std::vector{0});
  CHECK(host.requests[0].model_id == 2);
  CHECK(host.requests[13].model_id == 14);
  CHECK(host.requests[14].kind == 0);
  CHECK(host.requests[15].model_id == 15);
}

TEST_CASE("Battle asset queue reports a head request the loader refuses",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  host.requests[0] = Request(ActorAssetKind::kWeaponModelLoad, 0, 2);
  host.process_result = kAssetRefused;
  ActorAssetQueueStats stats;
  CHECK(Content::UpdateActorAssetQueue(host, stats) == -1);
  CHECK(host.processed == std::vector{ActorAssetKind::kWeaponModelLoad});
  CHECK(host.finished == std::vector{kAssetRefused});
}

TEST_CASE("Battle asset queue records the slowest request while a model parses",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  SetSlowModelParse(host);
  ActorAssetQueueStats stats;
  stats.peak_frame_ticks = 5;
  CHECK(Content::UpdateActorAssetQueue(host, stats) == 1);
  CHECK(stats.frame_ticks == 7);
  CHECK(stats.peak_frame_ticks == 7);
  CHECK(stats.last_kind == 2);
  CHECK(stats.last_load_phase == 2);
}

TEST_CASE(
    "Battle asset queue keeps the peak of a model parse for later requests",
    "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  SetSlowModelParse(host);
  ActorAssetQueueStats stats;
  stats.peak_frame_ticks = 5;
  static_cast<void>(Content::UpdateActorAssetQueue(host, stats));
  host.process_ticks = 3;
  static_cast<void>(Content::UpdateActorAssetQueue(host, stats));
  CHECK(stats.frame_ticks == 3);
  host.process_ticks = 9;
  host.load_phase = 1;
  static_cast<void>(Content::UpdateActorAssetQueue(host, stats));
  CHECK(stats.frame_ticks == 9);
  CHECK(stats.peak_frame_ticks == 7);
  CHECK(stats.last_load_phase == 2);
}

TEST_CASE("Ashley auxiliary sequence requests preserve kind and selector",
          "[content-slice][content][actor-assets][semantic-names][sequence]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  Content::QueueAshleyWeaponSequence(host, 0);
  CHECK(host.requests[0].kind == 0);
  Content::QueueAshleyWeaponSequence(host, 9);
  const ActorAssetRequest& request = host.requests[0];
  CHECK(request.kind == 7);
  CHECK(request.target_slot == 0);
  CHECK(request.sub_kind == 3);
  CHECK(request.material == 9);
  CHECK(host.queued.empty());
}

TEST_CASE("Shared actor model lookups write only the matched peer slot",
          "[content-slice][content][actor-assets][semantic-names][sequence]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  for (std::size_t slot = 0; slot < 4; ++slot) {
    host.actors[slot] = ActorWithModel(7);
  }
  ActorAssetRequest request = Request(ActorAssetKind::kActorModelLoad, 2, 7);
  CHECK(Content::FindSharedActorModel(host, request) == 1);
  CHECK(request.data == Content::SharedAssetAddress(0));
  request.model_id = 8;
  request.data = Address(99);
  CHECK(Content::FindSharedActorModel(host, request) == 0);
  CHECK(request.data == Address(99));
}

TEST_CASE("Shared actor sequence searches respect player and model boundaries",
          "[content-slice][content][actor-assets][semantic-names][sequence]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  for (std::size_t slot = 0; slot < 4; ++slot) {
    host.actors[slot] = ActorWithModel(7);
  }
  const AssetAddress sequence = host.AddBlock(4);
  Required(host.actors[0]).common_sequence = sequence;
  Required(host.actors[3]).common_sequence = sequence;
  ActorAssetRequest request = Request(ActorAssetKind::kSequenceLoad, 2, 0);
  CHECK(Content::FindSharedActorSequence(host, request) == 1);
  CHECK(request.data == Content::SharedAssetAddress(3));
  request.model_id = 4;
  Required(host.actors[3]).sequence_model_id = 4;
  CHECK(Content::FindSharedActorSequence(host, request) == 1);
  CHECK(request.data == Content::SharedAssetAddress(3));
  Required(host.actors[3]).model_id = 8;
  request.data = Address(99);
  CHECK(Content::FindSharedActorSequence(host, request) == 0);
  CHECK(request.data == Address(99));
}

TEST_CASE("Model texture offsets move flat polygon UVs and wrap each byte",
          "[content-slice][content][actor-assets]") {
  std::array<std::byte, 36> polygons{};
  polygons[0] = std::byte{0x24};
  std::ranges::fill(std::span(polygons).subspan(10, 6), std::byte{0xF0});
  polygons[16] = std::byte{0x2C};
  std::ranges::fill(std::span(polygons).subspan(28, 8), std::byte{0x05});
  CHECK(Content::OffsetModelTextureCoordinates(polygons, 2, false, 0x20, 1));
  CHECK(polygons[9] == std::byte{0});
  CHECK(polygons[10] == std::byte{0x10});
  CHECK(polygons[11] == std::byte{0xF1});
  CHECK(polygons[15] == std::byte{0xF1});
  CHECK(polygons[28] == std::byte{0x25});
  CHECK(polygons[35] == std::byte{0x06});
  CHECK_FALSE(Content::OffsetModelTextureCoordinates(
      std::span(polygons).first(35), 2, false, 1, 1));
}

TEST_CASE(
    "Model texture offsets move colored UVs and the triangle's first pair",
    "[content-slice][content][actor-assets]") {
  std::array<std::byte, 56> polygons = ColoredPolygons();
  CHECK(Content::OffsetModelTextureCoordinates(polygons, 2, true, 2, 3));
  CHECK(polygons[5] == std::byte{1});
  CHECK(polygons[6] == std::byte{3});
  CHECK(polygons[7] == std::byte{4});
  CHECK(polygons[20] == std::byte{3});
  CHECK(polygons[23] == std::byte{4});
}

TEST_CASE(
    "Model texture offsets move every colored quad UV and refuse a short "
    "buffer",
    "[content-slice][content][actor-assets]") {
  std::array<std::byte, 56> polygons = ColoredPolygons();
  REQUIRE(Content::OffsetModelTextureCoordinates(polygons, 2, true, 2, 3));
  CHECK(polygons[24 + 23] == std::byte{1});
  CHECK(polygons[24 + 24] == std::byte{3});
  CHECK(polygons[24 + 31] == std::byte{4});
  CHECK_FALSE(Content::OffsetModelTextureCoordinates(polygons, 3, true, 0, 0));
}

TEST_CASE("Model geometry copies allocate a low block and refuse bad sizes",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  const AssetAddress source = host.AddBlock(16);
  std::ranges::generate(host.Block(source), [value = 1]() mutable {
    return static_cast<std::byte>(value++);
  });
  const AssetAddress copy =
      Content::CopyModelGeometry(host, AssetAddress::kNone, source, 16);
  REQUIRE(host.low_blocks.size() == 1);
  CHECK(copy == host.low_blocks[0]);
  CHECK(std::ranges::equal(host.Block(copy), host.Block(source)));
  const AssetAddress small = host.AddBlock(8);
  CHECK(Content::CopyModelGeometry(host, small, source, 16) == small);
  CHECK(host.Block(small)[7] == std::byte{8});
  CHECK(Content::CopyModelGeometry(host, AssetAddress::kNone, source, -1) ==
        AssetAddress::kNone);
  host.refuse_allocations = true;
  CHECK(Content::CopyModelGeometry(host, AssetAddress::kNone, source, 16) ==
        AssetAddress::kNone);
  CHECK(host.refusals == std::vector<std::string>{"model geometry copy",
                                                  "model geometry size",
                                                  "model geometry block"});
}

TEST_CASE("Shared weapon lookups need an owning actor or a player slot",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  WeaponAssetRecord model;
  model.model_id = 12;
  host.weapons[5] = model;
  ActorAssetRequest player = Request(ActorAssetKind::kWeaponModelLoad, 2, 12);
  CHECK(Content::FindSharedWeaponModel(host, player) == 1);
  CHECK(player.data == Content::SharedAssetAddress(5));
  ActorAssetRequest enemy = Request(ActorAssetKind::kWeaponModelLoad, 6, 12);
  enemy.data = Address(99);
  CHECK(Content::FindSharedWeaponModel(host, enemy) == 0);
  CHECK(enemy.data == Address(99));
  Required(host.weapons[5]).actor_id = 3;
  CHECK(Content::FindSharedWeaponModel(host, enemy) == 1);
  CHECK(enemy.data == Content::SharedAssetAddress(5));
  ActorAssetRequest self = Request(ActorAssetKind::kWeaponModelLoad, 5, 12);
  CHECK(Content::FindSharedWeaponModel(host, self) == 0);
}

TEST_CASE("Weapon loads read their WEP file at the table's disc sector",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  AddWeaponFile(host);
  host.read_finished = false;
  ActorAssetLoadState state;
  ActorAssetRequest request = WeaponRequest(host);
  CHECK(Content::UpdateWeaponModelLoad(host, state, request) == kAssetPending);
  REQUIRE(host.reads.size() == 1);
  CHECK(SameRead(host.reads[0], AssetFileStream::kWeapon, kWepSector, 2048));
  REQUIRE(host.high_blocks.size() == 1);
  CHECK(host.reads[0].destination == host.high_blocks[0]);
  CHECK(state.model_phase == 1);
}

TEST_CASE("Weapon loads wait for the WEP read before releasing it",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  AddWeaponFile(host);
  host.read_finished = false;
  ActorAssetLoadState state;
  ActorAssetRequest request = WeaponRequest(host);
  StepWeaponLoad(host, state, request, 1);
  CHECK(Content::UpdateWeaponModelLoad(host, state, request) == kAssetPending);
  CHECK(host.released_reads.empty());
  host.read_finished = true;
  CHECK(Content::UpdateWeaponModelLoad(host, state, request) == kAssetPending);
  CHECK(state.model_phase == 2);
  CHECK(host.released_reads == std::vector{AssetFileStream::kWeapon});
}

TEST_CASE("Weapon loads read a file the disc queue refuses before waiting",
          "[content-slice][content][actor-assets][disc-queue]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  AddWeaponFile(host);
  host.accept_reads = false;
  ActorAssetLoadState state;
  ActorAssetRequest request = WeaponRequest(host);
  CHECK(Content::UpdateWeaponModelLoad(host, state, request) == kAssetPending);
  CHECK(host.reads.size() == 1);
  REQUIRE(host.refused_reads.size() == 1);
  CHECK(host.refused_reads[0].file.sector == kWepSector);
  CHECK(host.refused_reads[0].destination == host.high_blocks.at(0));
  CHECK(state.model_phase == 2);
}

TEST_CASE("Weapon loads parse a WEP file the disc queue refused",
          "[content-slice][content][actor-assets][disc-queue]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  AddWeaponFile(host);
  host.accept_reads = false;
  ActorAssetLoadState state;
  ActorAssetRequest request = WeaponRequest(host);
  StepWeaponLoad(host, state, request, 1);
  CHECK(Content::UpdateWeaponModelLoad(host, state, request) == kAssetPending);
  CHECK(Content::UpdateWeaponModelLoad(host, state, request) == kAssetDone);
  const WeaponAssetRecord& model = Required(host.weapons[4]);
  CHECK(model.bone_count == 2);
  CHECK(host.refusals.empty());
}

TEST_CASE("Weapon files parse the header, geometry and palettes",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  AddWeaponFile(host);
  ActorAssetLoadState state;
  ActorAssetRequest request = WeaponRequest(host);
  CHECK(Content::UpdateWeaponModelLoad(host, state, request) == kAssetPending);
  CHECK(Content::UpdateWeaponModelLoad(host, state, request) == kAssetPending);
  CHECK(state.parse_phase == 1);
  const WeaponAssetRecord& model = Required(host.weapons[4]);
  CHECK(model.bone_count == 2);
  CHECK(model.triangle_count == 1);
  CHECK(model.model_id == kWepModel);
}

TEST_CASE("Weapon files keep their geometry block and every palette",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  AddWeaponFile(host);
  ActorAssetLoadState state;
  ActorAssetRequest request = WeaponRequest(host);
  StepWeaponLoad(host, state, request, 2);
  const WeaponAssetRecord& model = Required(host.weapons[4]);
  REQUIRE(host.low_blocks.size() == 1);
  CHECK(model.geometry == host.low_blocks[0]);
  CHECK(model.palette_color_count == 48);
  CHECK(model.working_palette[15] == 0x100F);
  CHECK(model.palettes[6][31] == 0x261F);
}

TEST_CASE("Weapon files upload their texture into the first free slot",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  AddWeaponFile(host);
  host.weapon_textures[0].references = 1;
  ActorAssetLoadState state;
  ActorAssetRequest request = WeaponRequest(host);
  static_cast<void>(Content::UpdateWeaponModelLoad(host, state, request));
  CHECK(Content::UpdateWeaponModelLoad(host, state, request) == kAssetPending);
  const WeaponAssetRecord& model = Required(host.weapons[4]);
  CHECK(model.texture_slot == 1);
  CHECK(model.palette_slot_offset == 20);
  CHECK(host.weapon_textures[1].references == 1);
  REQUIRE(host.images.size() == 1);
  CHECK(SameRect(host.images[0].rect, {.x = 32, .y = 0xE0, .w = 16, .h = 32}));
  CHECK(host.images[0].bytes == kWepTextureBytes);
  CHECK(host.images[0].first == std::byte{0x77});
}

TEST_CASE("Weapon primitives move UVs into the texture rows and copy the bones",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  AddWeaponFile(host);
  ActorAssetLoadState state;
  ActorAssetRequest request = WeaponRequest(host);
  static_cast<void>(Content::UpdateWeaponModelLoad(host, state, request));
  static_cast<void>(Content::UpdateWeaponModelLoad(host, state, request));
  CHECK(Content::UpdateWeaponModelLoad(host, state, request) == kAssetDone);
  const std::span<std::byte> triangle =
      host.Block(host.low_blocks.at(0)).subspan(kWepPolygonAt);
  CHECK(triangle[kTriangleUvAt] == std::byte{0x08});
  CHECK(triangle[kTriangleUvAt + 1] == std::byte{0xE8});
  CHECK(triangle[kTriangleUvAt + 5] == std::byte{0xE8});
  REQUIRE(host.weapon_bones[4].size() == 16);
  CHECK(host.weapon_bones[4][0] == std::byte{0xB0});
  REQUIRE(host.geometry_records.size() == 1);
  CHECK(host.geometry_records[0].bytes == kWepGeometryBytes);
}

TEST_CASE("Weapon loads finish with the owner's attachment bone",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  AddWeaponFile(host);
  ActorAssetLoadState state;
  ActorAssetRequest request = WeaponRequest(host);
  StepWeaponLoad(host, state, request, 2);
  CHECK(Content::UpdateWeaponModelLoad(host, state, request) == kAssetDone);
  const WeaponAssetRecord& model = Required(host.weapons[4]);
  CHECK(model.actor_id == 1);
  CHECK(model.attachment_slot == 2);
  CHECK(model.attachment_bone == 5);
  CHECK(model.reserved11 == 64);
  CHECK(host.bone_tag_queries == std::vector{std::pair{1, 2}});
}

TEST_CASE("Weapon loads finish with the material palette and free the WEP file",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  AddWeaponFile(host);
  ActorAssetLoadState state;
  ActorAssetRequest request = WeaponRequest(host);
  StepWeaponLoad(host, state, request, 3);
  const WeaponAssetRecord& model = Required(host.weapons[4]);
  CHECK(model.working_palette[16] == 0x2300);
  REQUIRE(host.cluts.size() == 1);
  CHECK(SameClut(host.cluts[0], {.row = 14, .first = 160, .count = 48}));
  CHECK(host.high_frees == host.high_blocks);
  CHECK(state.model_phase == 0);
}

TEST_CASE("Weapon material palettes copy the palette and upload the CLUT",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  WeaponAssetRecord model;
  model.double_clut = true;
  model.palette_slot_offset = 5;
  model.palettes[1].fill(0x1234);
  host.weapons[3] = model;
  host.material_palettes = {1, 9};
  CHECK(Content::LoadWeaponMaterialPalette(host, 3, 0));
  const WeaponAssetRecord& loaded = Required(host.weapons[3]);
  CHECK(loaded.working_palette[31] == 0);
  CHECK(loaded.working_palette[32] == 0x1234);
  CHECK(loaded.upload_palette[95] == 0x1234);
  REQUIRE(host.cluts.size() == 1);
  CHECK(SameClut(host.cluts[0], {.row = 6, .first = 208, .count = 96}));
}

TEST_CASE("Weapon material palettes refuse materials without a palette",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  host.weapons[3] = WeaponAssetRecord{};
  host.material_palettes = {1, 9};
  CHECK_FALSE(Content::LoadWeaponMaterialPalette(host, 3, 1));
  CHECK(Required(host.weapons[3]).material == 1);
  CHECK_FALSE(Content::LoadWeaponMaterialPalette(host, 3, 0x12));
  CHECK(Required(host.weapons[3]).material == 2);
  CHECK_FALSE(Content::LoadWeaponMaterialPalette(host, 7, 0));
  CHECK(host.cluts.empty());
  CHECK(host.refusals == std::vector<std::string>{"weapon material palette",
                                                  "weapon material palette",
                                                  "weapon palette record"});
}

TEST_CASE("Weapon releases free unshared geometry and drop texture references",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  WeaponAssetRecord model;
  model.bone_count = 2;
  model.actor_id = 1;
  model.model_id = 12;
  model.geometry = host.AddBlock(16);
  model.texture_slot = 6;
  model.wide_texture = true;
  host.weapons[5] = model;
  host.weapon_textures[6].references = 2;
  host.weapon_textures[7].references = 1;
  CHECK(Content::ReleaseWeaponModel(host, 5) == 0);
  CHECK(host.forgotten_geometry == std::vector{model.geometry});
  CHECK(host.low_frees == std::vector{model.geometry});
  CHECK(host.weapon_textures[6].references == 1);
  CHECK(host.weapon_textures[7].references == 0);
  CHECK(host.cleared_weapons == std::vector{5});
}

TEST_CASE("Weapon releases refuse an empty or out of range slot",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  CHECK(Content::ReleaseWeaponModel(host, 5) == -1);
  CHECK(Content::ReleaseWeaponModel(host, 25) == -1);
  CHECK(host.refusals == std::vector<std::string>{"weapon slot"});
  CHECK(host.cleared_weapons.empty());
}

TEST_CASE("Weapon releases keep geometry another owner shares past slot three",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  WeaponAssetRecord model;
  model.bone_count = 1;
  model.actor_id = 1;
  model.model_id = 12;
  model.geometry = host.AddBlock(16);
  model.texture_slot = 3;
  host.weapons[2] = model;
  host.weapons[6] = model;
  host.weapons[8] = model;
  host.weapon_textures[3].references = 3;
  CHECK(Content::ReleaseWeaponModel(host, 6) == 0);
  CHECK(host.low_frees.empty());
  CHECK(Content::ReleaseWeaponModel(host, 2) == 0);
  CHECK(host.low_frees == std::vector{model.geometry});
  CHECK(host.weapon_textures[3].references == 1);
  CHECK(host.cleared_weapons == std::vector{6, 2});
}

TEST_CASE("Actor model loads fall back to model zero's SHP file",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  AddShpTables(host);
  host.read_finished = false;
  ActorAssetLoadState state;
  ActorAssetRequest request = Request(ActorAssetKind::kActorModelLoad, 3, 20);
  CHECK(Content::UpdateActorModelLoad(host, state, request) == kAssetPending);
  CHECK(request.model_id == 0);
  REQUIRE(host.reads.size() == 1);
  CHECK(SameRead(host.reads[0], AssetFileStream::kActorModel, 95007, 4096));
  CHECK(state.model_phase == 1);
}

TEST_CASE("Actor model loads refuse a model missing from the SHP table",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  AddShpTables(host);
  ActorAssetLoadState state;
  ActorAssetRequest missing = Request(ActorAssetKind::kActorModelLoad, 3, 30);
  CHECK(Content::UpdateActorModelLoad(host, state, missing) == kAssetRefused);
  CHECK(host.refusals == std::vector<std::string>{"actor model file table"});
}

TEST_CASE("Modelless actors are placed in the room without a model file",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  ActorAssetLoadState state;
  ActorAssetRequest request = ModellessRequest(host);
  CHECK(Content::UpdateActorModelLoad(host, state, request) == kAssetDone);
  const ActorAssetRecord& actor = Required(host.actors[4]);
  CHECK(actor.model_id == 0x7F);
  CHECK(actor.slot == 4);
  CHECK(actor.occupancy_radius == 0x40);
  CHECK(actor.occupancy_height == 10000);
  CHECK(host.reads.empty());
}

TEST_CASE("Modelless actors reset their motion and take placement mode one",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  ActorAssetLoadState state;
  ActorAssetRequest request = ModellessRequest(host);
  CHECK(Content::UpdateActorModelLoad(host, state, request) == kAssetDone);
  CHECK(host.motion_resets == std::vector{4});
  REQUIRE(host.placements.size() == 1);
  CHECK(host.placements[0].placement == 0x01020304);
  CHECK(host.placements[0].mode == 1);
}

TEST_CASE("Shared actor models copy the peer record and its texture references",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  ActorAssetLoadState state;
  ActorAssetRequest request = SharedModelRequest(host);
  CHECK(Content::UpdateActorModelLoad(host, state, request) == kAssetDone);
  CHECK(request.data == Content::SharedAssetAddress(2));
  CHECK(host.actor_textures[5].references == 1);
  CHECK(host.actor_textures[6].references == 1);
  const ActorAssetRecord& actor = Required(host.actors[3]);
  CHECK(actor.scale == 0x1234);
  CHECK(actor.attach_parent == 0xFF);
}

TEST_CASE("Shared actor models place the actor and upload both palettes",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  ActorAssetLoadState state;
  ActorAssetRequest request = SharedModelRequest(host);
  CHECK(Content::UpdateActorModelLoad(host, state, request) == kAssetDone);
  REQUIRE(host.placements.size() == 1);
  CHECK(host.placements[0].mode == 2);
  REQUIRE(host.cluts.size() == 2);
  CHECK(host.cluts[0].row == 7);
  CHECK(host.cluts[1].row == 0x19);
}

TEST_CASE("Actor model releases stop sounds and free files and texture slots",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  ActorAssetRecord actor = ActorWithModel(9);
  actor.model = host.AddBlock(0x40);
  Put32(host.Block(actor.model), 0x30, 0x20);
  actor.owned_sequence = host.AddBlock(4);
  actor.common_sequence = host.AddBlock(4);
  actor.holds_texture_slots = true;
  actor.texture_slot = 20;
  host.actors[3] = actor;
  host.actor_textures[20].references = 1;
  CHECK(Content::ReleaseActorModel(host, 3) == 0);
  CHECK(host.stopped_sounds == std::vector{Offset(actor.model, 0x20)});
  CHECK(host.low_frees ==
        std::vector{actor.model, actor.owned_sequence, actor.common_sequence});
  CHECK(host.actor_textures[20].references == 0);
  REQUIRE(host.retired_pages.size() == 1);
  CHECK(host.retired_pages[0].first == 31);
  CHECK(host.cleared_actors == std::vector{3});
}

TEST_CASE("Actor model releases refuse an empty or out of range slot",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  CHECK(Content::ReleaseActorModel(host, 3) == -1);
  CHECK(Content::ReleaseActorModel(host, 17) == -1);
  CHECK(host.refusals.empty());
  CHECK(host.cleared_actors.empty());
}

TEST_CASE("Actor model releases keep files and sequences a peer shares",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  ActorAssetRecord actor = ActorWithModel(9);
  actor.model = host.AddBlock(0x40);
  actor.common_sequence = host.AddBlock(4);
  host.actors[3] = actor;
  host.actors[5] = actor;
  CHECK(Content::ReleaseActorModel(host, 3) == 0);
  CHECK(host.stopped_sounds.empty());
  CHECK(host.low_frees.empty());
  CHECK(host.actors[5].has_value());
  ActorAssetRecord modelless = ActorWithModel(0x7F);
  modelless.common_sequence = actor.common_sequence;
  host.actors[6] = modelless;
  CHECK(Content::ReleaseActorModel(host, 6) == 0);
  CHECK(host.low_frees.empty());
  CHECK(host.cleared_actors == std::vector{3, 6});
}

TEST_CASE("Battle sequence loads read the actor's own sequence table",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  AddBattleSequenceActor(host);
  ActorAssetLoadState state;
  ActorAssetRequest request = Request(ActorAssetKind::kSequenceLoad, 3, 2);
  CHECK(Content::UpdateActorSequenceLoad(host, state, request) ==
        kAssetPending);
  REQUIRE(host.reads.size() == 1);
  CHECK(SameRead(host.reads[0], AssetFileStream::kSequence, 5000, 6144));
  CHECK(request.data == host.high_blocks.at(0));
  CHECK(state.sequence_file_owned);
  CHECK(host.queued.empty());
}

TEST_CASE("Battle sequence loads end at once without a sequence file",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  AddBattleSequenceActor(host);
  ActorAssetLoadState state;
  ActorAssetRequest empty = Request(ActorAssetKind::kSequenceLoad, 3, 3);
  CHECK(Content::UpdateActorSequenceLoad(host, state, empty) == kAssetDone);
  CHECK(host.reads.empty());
}

TEST_CASE("Player battle sequence loads also queue the material sequence",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  ActorAssetRecord actor = ActorWithModel(0);
  actor.battle_sequence_lbas[4] = 6000;
  actor.chain_attack_animation_ids[4] = 0x0800;
  host.actors[0] = actor;
  host.read_finished = false;
  ActorAssetLoadState state;
  ActorAssetRequest request = Request(ActorAssetKind::kSequenceLoad, 0, 4);
  CHECK(Content::UpdateActorSequenceLoad(host, state, request) ==
        kAssetPending);
  REQUIRE(host.queued.size() == 1);
  CHECK(host.queued[0].kind == 7);
  CHECK(host.queued[0].sub_kind == 3);
  CHECK(host.queued[0].material == 4);
  CHECK(host.requests[0] == host.queued[0]);
  REQUIRE(host.reads.size() == 1);
  CHECK(SameRead(host.reads[0], AssetFileStream::kSequence, 6000, 2048));
}

TEST_CASE("Sequence files come from the table of each request sub kind",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  AddSequenceTables(host);
  ActorAssetRequest bank = Request(ActorAssetKind::kSequenceLoad, 2, 5);
  bank.sub_kind = 1;
  CHECK(StartSequenceLoad(host, bank) == kAssetPending);
  ActorAssetRequest material = Request(ActorAssetKind::kSequenceLoad, 0, 0);
  material.sub_kind = 3;
  material.material = 6;
  CHECK(StartSequenceLoad(host, material) == kAssetPending);
  ActorAssetRequest pair = Request(ActorAssetKind::kSequenceLoad, 0, 3);
  pair.sub_kind = 4;
  pair.material = 1;
  CHECK(StartSequenceLoad(host, pair) == kAssetPending);
  CHECK(ReadSectors(host) ==
        std::vector<std::int32_t>{10 + 0x19797, 20 + 0x18043, 30 + 0x1943E});
  CHECK(host.low_blocks.size() == 1);
  CHECK(host.high_blocks.size() == 2);
}

TEST_CASE("Material model sequence requests rewrite their material and model",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  AddSequenceTables(host);
  ActorAssetRequest pair = Request(ActorAssetKind::kSequenceLoad, 0, 3);
  pair.sub_kind = 4;
  pair.material = 1;
  CHECK(StartSequenceLoad(host, pair) == kAssetPending);
  CHECK(pair.material == 2);
  CHECK(pair.model_id == 0);
  CHECK(ReadSectors(host) == std::vector<std::int32_t>{30 + 0x1943E});
}

TEST_CASE("Special sequence files come from the actor or end without one",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  ActorAssetRecord actor = ActorWithModel(9);
  actor.special_sequence_lbas[1] = 7000;
  actor.special_sequence_sizes[1] = 0x1000;
  host.actors[3] = actor;
  host.read_finished = false;
  ActorAssetLoadState none_state;
  ActorAssetRequest none = Request(ActorAssetKind::kSequenceLoad, 3, 0);
  none.sub_kind = 5;
  CHECK(Content::UpdateActorSequenceLoad(host, none_state, none) == kAssetDone);
  ActorAssetLoadState special_state;
  ActorAssetRequest special = Request(ActorAssetKind::kSequenceLoad, 3, 2);
  special.sub_kind = 5;
  CHECK(Content::UpdateActorSequenceLoad(host, special_state, special) ==
        kAssetPending);
  REQUIRE(host.reads.size() == 1);
  CHECK(host.reads[0].file.sector == 7000);
  CHECK(host.reads[0].file.bytes == 4096);
  ActorAssetLoadState unknown_state;
  ActorAssetRequest unknown = Request(ActorAssetKind::kSequenceLoad, 3, 1);
  unknown.sub_kind = 6;
  CHECK(Content::UpdateActorSequenceLoad(host, unknown_state, unknown) ==
        kAssetRefused);
  CHECK(host.refusals == std::vector<std::string>{"sequence sub kind"});
}

TEST_CASE("Common sequences share a peer's sequence",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  const AssetAddress peer_sequence = AddCommonSequencePeers(host);
  ActorAssetLoadState state;
  ActorAssetRequest shared = Request(ActorAssetKind::kSequenceParse, 3, 0);
  shared.data = Content::SharedAssetAddress(2);
  CHECK(Content::ParseActorSequence(host, state, shared) == kAssetDone);
  const ActorAssetRecord& actor = Required(host.actors[3]);
  CHECK(actor.common_sequence == peer_sequence);
  CHECK(actor.sequence_flags == 5);
}

TEST_CASE("Common sequences copy an actor's own sequence file",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  static_cast<void>(AddCommonSequencePeers(host));
  ActorAssetLoadState state;
  ActorAssetRequest shared = Request(ActorAssetKind::kSequenceParse, 3, 0);
  shared.data = Content::SharedAssetAddress(2);
  static_cast<void>(Content::ParseActorSequence(host, state, shared));
  ActorAssetRequest own = Request(ActorAssetKind::kSequenceParse, 4, 0);
  own.data = OwnCommonSequenceFile(host);
  CHECK(Content::ParseActorSequence(host, state, own) == kAssetDone);
  REQUIRE(host.low_blocks.size() == 1);
  const ActorAssetRecord& actor = Required(host.actors[4]);
  CHECK(actor.common_sequence == host.low_blocks[0]);
  CHECK(actor.sequence_flags == 7);
  CHECK(host.Block(host.low_blocks[0])[7] == std::byte{0xC7});
  REQUIRE(host.sequence_banks.size() == 1);
  CHECK(host.sequence_banks[0].bytes == 8);
}

TEST_CASE("Shared sequence banks parse their entries",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  const AssetAddress file = ParseSequenceBank(host);
  const SharedSequenceBank& bank = host.banks[1];
  CHECK(bank.data == file);
  CHECK(bank.animation_ids[1] == 9);
  CHECK(bank.animation_ids[2] == -1);
  CHECK(bank.entry_params[1] == 0x41);
  CHECK(bank.animation_data[0] == Offset(file, 28));
  CHECK(bank.animation_data[1] == Offset(file, 44));
  CHECK(host.embedded_banks.size() == 2);
}

TEST_CASE("Shared sequence banks bind every present actor",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  const AssetAddress file = ParseSequenceBank(host);
  const ActorAssetRecord& first = Required(host.actors[1]);
  const ActorAssetRecord& second = Required(host.actors[4]);
  CHECK(first.shared_sequences[1] == Offset(file, 52));
  CHECK(first.texture_row_offsets[1] == 0x41);
  CHECK(second.shared_sequences[1] == Offset(file, 36));
  CHECK(second.texture_row_offsets[1] == 0x30);
}

TEST_CASE("Shared sequence banks refuse a loaded bank, a bad count or bank",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  ActorAssetLoadState state;
  ActorAssetRequest request = Request(ActorAssetKind::kSequenceParse, 2, 0);
  request.sub_kind = 1;
  request.data = AddSequenceBankFile(host, 2);
  host.banks[1].data = host.AddBlock(4);
  CHECK(Content::ParseActorSequence(host, state, request) == kAssetRefused);
  CHECK(host.refusals.empty());
  host.banks[1] = SharedSequenceBank{};
  request.data = AddSequenceBankFile(host, 16);
  CHECK(Content::ParseActorSequence(host, state, request) == kAssetRefused);
  CHECK(host.banks[1].data == AssetAddress::kNone);
  request.target_slot = 5;
  CHECK(Content::ParseActorSequence(host, state, request) == kAssetRefused);
  CHECK(host.refusals == std::vector<std::string>{"shared sequence count",
                                                  "shared sequence bank"});
}

TEST_CASE("Shared sequence binding prefers the kind's id and marks no row",
          "[content-slice][content][actor-assets]") {
  SharedSequenceBank bank;
  bank.animation_ids = {4, 7, -1};
  bank.entry_params = {0x11, 0x22};
  bank.animation_data[0] = Address(0x1000);
  bank.animation_data[1] = Address(0x2000);
  const std::array<std::uint8_t, 8> ids_by_kind{0, 0, 0, 7, 0, 0, 0, 0};
  ActorAssetRecord actor = ActorWithModel(3);
  CHECK(Content::BindSharedActorSequence(bank, 2, ids_by_kind, actor));
  CHECK(actor.shared_sequences[2] == Address(0x2008));
  CHECK(actor.texture_row_offsets[2] == 0xFF);
  actor.model_id = 4;
  CHECK(Content::BindSharedActorSequence(bank, 0, ids_by_kind, actor));
  CHECK(actor.shared_sequences[0] == Address(0x1008));
  CHECK(actor.texture_row_offsets[0] == 0x11);
  const ActorAssetRecord before = actor;
  CHECK_FALSE(Content::BindSharedActorSequence(bank, 4, ids_by_kind, actor));
  CHECK(actor == before);
}

TEST_CASE("Battle sequence releases free only sequences no peer shares",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  const ActorAssetRecord actor = BattleSequenceActor(host);
  host.actors[3] = actor;
  CHECK(Content::ReleaseActorSequence(host, 3, 0) == 0);
  CHECK(host.low_frees == std::vector{actor.battle_sequence});
  const ActorAssetRecord& released = Required(host.actors[3]);
  CHECK(released.battle_sequence == AssetAddress::kNone);
  CHECK(released.sequence_model_id == 0);
  CHECK(released.common_animation_count == 0);
}

TEST_CASE("Battle sequence releases keep a sequence a peer shares",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  const ActorAssetRecord actor = BattleSequenceActor(host);
  host.actors[3] = actor;
  host.actors[5] = actor;
  CHECK(Content::ReleaseActorSequence(host, 3, 0) == 0);
  CHECK(host.low_frees.empty());
  CHECK(Required(host.actors[3]).battle_sequence == AssetAddress::kNone);
}

TEST_CASE("Battle sequence releases free player slot one and never slot zero",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  ActorAssetRecord actor = BattleSequenceActor(host);
  actor.model_id = 0;
  host.actors[0] = actor;
  host.actors[1] = actor;
  CHECK(Content::ReleaseActorSequence(host, 1, 0) == 0);
  CHECK(host.low_frees == std::vector{actor.battle_sequence});
  CHECK(Content::ReleaseActorSequence(host, 0, 0) == 0);
  CHECK(host.low_frees.size() == 1);
}

TEST_CASE("Shared sequence bank releases clear every actor and free the bank",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  const AssetAddress data = host.AddBlock(8);
  host.banks[1].data = data;
  ActorAssetRecord actor;
  actor.shared_sequences[1] = Address(0x5000);
  host.actors[0] = actor;
  host.actors[5] = actor;
  CHECK(Content::ReleaseActorSequence(host, 2, 1) == 0);
  CHECK(Required(host.actors[0]).shared_sequences[1] == AssetAddress::kNone);
  CHECK(Required(host.actors[5]).shared_sequences[1] == AssetAddress::kNone);
  CHECK(host.low_frees == std::vector{data});
  CHECK(host.banks[1].data == AssetAddress::kNone);
}

TEST_CASE(
    "Shared sequence bank releases wait on a busy page and refuse bank zero",
    "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  host.banks[2].data = host.AddBlock(8);
  host.bank_pages[2] = 9;
  host.retire_result = 0;
  CHECK(Content::ReleaseActorSequence(host, 3, 2) == kAssetBusy);
  CHECK(host.banks[2].data != AssetAddress::kNone);
  CHECK(host.texture_frame_resets == std::vector{2});
  CHECK(Content::ReleaseActorSequence(host, 0, 1) == -1);
  CHECK(host.refusals == std::vector<std::string>{"shared sequence bank"});
}

TEST_CASE("Special sequence releases free the actor's work sequence",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  ActorAssetRecord actor;
  actor.shared_sequences[3] = host.AddBlock(8);
  actor.battle_animation_count = 4;
  host.actors[5] = actor;
  CHECK(Content::ReleaseActorSequence(host, 5, 4) == 0);
  CHECK(host.low_frees == std::vector{actor.shared_sequences[3]});
  const ActorAssetRecord& released = Required(host.actors[5]);
  CHECK(released.shared_sequences[3] == AssetAddress::kNone);
  CHECK(released.battle_animation_count == 0);
  CHECK(Content::ReleaseActorSequence(host, 5, 3) == 0);
  CHECK(Content::ReleaseActorSequence(host, 6, 5) == -1);
  CHECK(host.low_frees.size() == 1);
}

TEST_CASE(
    "Effect texture releases refuse a bad bank and skip a bank with no page",
    "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  CHECK(Content::ReleaseSequenceEffectTexture(host, 0) == -1);
  CHECK(Content::ReleaseSequenceEffectTexture(host, 4) == -1);
  CHECK(Content::ReleaseSequenceEffectTexture(host, 1) == 0);
  CHECK(host.retired_pages.empty());
  CHECK(host.texture_frame_resets.empty());
}

TEST_CASE("Effect texture releases retire the bank's page",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  host.bank_pages[2] = 12;
  CHECK(Content::ReleaseSequenceEffectTexture(host, 3) == 0);
  REQUIRE(host.retired_pages.size() == 1);
  CHECK(host.retired_pages[0].first == 12);
  CHECK(host.retired_pages[0].count == 1);
  CHECK(host.bank_pages[2] == 0);
  CHECK(host.texture_frame_resets == std::vector{2});
}

TEST_CASE("Effect texture loads read the ETM file at the table's disc sector",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  AddEffectTextureFile(host);
  host.read_finished = false;
  ActorAssetLoadState state;
  const ActorAssetRequest request = EffectTextureRequest();
  CHECK(Content::UpdateSequenceEffectTextureLoad(host, state, request) ==
        kAssetPending);
  REQUIRE(host.reads.size() == 1);
  CHECK(host.reads[0].stream == AssetFileStream::kEffectTexture);
  CHECK(host.reads[0].file.sector == kEtmSector);
  CHECK(host.images.empty());
}

TEST_CASE("Effect texture loads upload the ETM image into the bank's page",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  AddEffectTextureFile(host);
  host.read_finished = false;
  ActorAssetLoadState state;
  const ActorAssetRequest request = EffectTextureRequest();
  StepEffectTextureLoad(host, state, request, 1);
  CHECK(Content::UpdateSequenceEffectTextureLoad(host, state, request) ==
        kAssetPending);
  CHECK(host.bank_pages[1] == 0x13);
  REQUIRE(host.images.size() == 1);
  CHECK(SameRect(host.images[0].rect, {.x = 192, .y = 256, .w = 64, .h = 8}));
  CHECK(host.images[0].first == std::byte{0x5A});
}

TEST_CASE("Effect texture loads free the ETM file when they finish",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  AddEffectTextureFile(host);
  host.read_finished = false;
  ActorAssetLoadState state;
  const ActorAssetRequest request = EffectTextureRequest();
  StepEffectTextureLoad(host, state, request, 2);
  CHECK(Content::UpdateSequenceEffectTextureLoad(host, state, request) ==
        kAssetDone);
  CHECK(host.high_frees == host.high_blocks);
  CHECK(state.effect_texture_phase == 0);
}

TEST_CASE("Effect texture loads refuse a busy page, full VRAM or a bad bank",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  AddEffectTextureFile(host);
  host.bank_pages[1] = 7;
  host.retire_result = 0;
  const ActorAssetRequest request = EffectTextureRequest();
  ActorAssetLoadState busy;
  CHECK(Content::UpdateSequenceEffectTextureLoad(host, busy, request) ==
        kAssetBusy);
  host.retire_result = 1;
  host.reserve_result = 0;
  ActorAssetLoadState full;
  CHECK(Content::UpdateSequenceEffectTextureLoad(host, full, request) ==
        kAssetRefused);
  CHECK(host.bank_pages[1] == 0);
  CHECK(host.images.empty());
  ActorAssetRequest outside = request;
  outside.target_slot = 0;
  ActorAssetLoadState unused;
  CHECK(Content::UpdateSequenceEffectTextureLoad(host, unused, outside) ==
        kAssetRefused);
  CHECK(host.reads.size() == 2);
}

TEST_CASE("Weapon sequence copies move the fixed block into the destination",
          "[content-slice][content][actor-assets]") {
  const auto owner = std::make_unique<FakeAssetHost>();
  FakeAssetHost& host = *owner;
  const AssetAddress source = host.AddBlock(Content::kWeaponSequenceBytes);
  const AssetAddress destination =
      host.AddBlock(Content::kWeaponSequenceBytes + 4);
  std::ranges::generate(host.Block(source), [value = 0U]() mutable {
    return static_cast<std::byte>(value++ & 0xFFU);
  });
  std::ranges::fill(host.Block(destination), std::byte{0x6B});
  Content::CopyWeaponSequence(host, destination, source);
  const std::span<std::byte> copied = host.Block(destination);
  CHECK(std::ranges::equal(copied.first(Content::kWeaponSequenceBytes),
                           host.Block(source)));
  CHECK(copied[Content::kWeaponSequenceBytes] == std::byte{0x6B});
  REQUIRE(host.sequence_copies.size() == 1);
  CHECK(host.sequence_copies[0].bytes == Content::kWeaponSequenceBytes);
  Content::CopyWeaponSequence(host, host.AddBlock(0x100), source);
  CHECK(host.refusals == std::vector<std::string>{"weapon sequence copy"});
  CHECK(host.sequence_copies.size() == 2);
}

TEST_CASE("Live battle asset entry points refuse a null request and bad slots",
          "[content-slice][content][actor-assets]") {
  const std::uint64_t before = Content::LiveActorAssetRefusals();
  CHECK(Content::QueueLiveActorAssetRequest(nullptr) == -1);
  CHECK(Content::ReleaseLiveWeaponModel(25) == -1);
  CHECK(Content::ReleaseLiveSequenceEffectTexture(0) == -1);
  CHECK(Content::LiveActorAssetRefusals() == before + 2);
}
