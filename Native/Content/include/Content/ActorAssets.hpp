#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Content/CdDrive.hpp"
#include "Content/SystemResources.hpp"

// The battle asset queue and the SHP, WEP, SEQ and ETM loaders (BATTLE.PRG
// 30DB0.c:149-2060). Loaders queue asset requests; the battle loop runs the
// head request once per frame until it reports done, then shifts the queue.
// A file load reads the file into a game heap block, then parses it into an
// actor, weapon or shared sequence record over one or more frames.
//
// Records, buffers and file bytes are named by AssetAddress handles; the
// host resolves a handle to bounded bytes, so a short or unknown buffer is
// refused instead of read or written past its end. The live binding
// (ActorAssetsLive.hpp) keeps the legacy records and globals.
namespace VS::Content {

inline constexpr int kActorAssetRequestCount = 16;
inline constexpr int kWeaponModelSlots = 20;
inline constexpr int kActorRecordSlots = 17;
inline constexpr int kSharedSequenceBanks = 4;
inline constexpr int kSharedSequenceEntries = 16;
inline constexpr int kActorTextureSlots = 25;
inline constexpr int kWeaponTextureSlots = 20;
inline constexpr std::uint8_t kNoAssetTextureSlot = 0xFF;
// The model id that places an actor without a model (30DB0.c:1080).
inline constexpr std::int16_t kModellessActorId = 0x7F;
// The bytes a weapon geometry copy moves (sizeof _wepModelOffsets).
inline constexpr std::size_t kWeaponGeometryBytes = 0x800;
// The bytes an actor battle sequence copy moves (func_8009C378).
inline constexpr std::size_t kWeaponSequenceBytes = 0x7800;
// The SHP texture layout block copied into the actor record (0xE8 bytes).
inline constexpr std::size_t kActorTextureLayoutBytes = 0xE8;

// The array extents of the actor record (D_800F4538_t).
inline constexpr std::size_t kActorSequenceRefs = 4;
// The shared_sequences entry a special-action sequence occupies.
inline constexpr std::size_t kSpecialSequenceRef = 3;
inline constexpr std::size_t kActorTextureScrolls = 4;
inline constexpr std::size_t kActorBattleSequences = 12;
inline constexpr std::size_t kActorSpecialSequences = 4;
inline constexpr std::size_t kActorPaletteBytes = 0x280;
inline constexpr std::size_t kActorBasePaletteColors = 0x140;
inline constexpr std::size_t kActorCurrentPaletteColors = 0xA0;
inline constexpr std::size_t kActorJointSlots = 8;
inline constexpr std::size_t kActorTextureRowOffsets = 4;
// The array extents of the weapon record.
inline constexpr std::size_t kWeaponPaletteSets = 7;
inline constexpr std::size_t kWeaponPaletteColors = 64;
inline constexpr std::size_t kWeaponWorkingColors = 96;

// Results the loaders return, as the original's int codes.
inline constexpr int kAssetDone = 0;
inline constexpr int kAssetPending = -1;
inline constexpr int kAssetRefused = -2;
inline constexpr int kAssetBusy = -3;

// vs_battle_objectData.dataType (30DB0.c:176-230). 5 is never dispatched.
enum class ActorAssetKind : std::uint8_t {
  kNone = 0,
  kActorModelLoad = 1,
  kActorModelParse = 2,
  kWeaponModelLoad = 3,
  kWeaponModelParse = 4,
  kGimmick = 6,
  kSequenceLoad = 7,
  kSequenceParse = 8,
  kEffectTextureLoad = 9,
};

// A native address: a record, a heap block or a cursor into file bytes. A
// request's data field also carries a shared record index in-band (below
// the record count), as the original does.
enum class AssetAddress : std::uintptr_t { kNone = 0 };

[[nodiscard]] constexpr AssetAddress AdvanceAddress(AssetAddress address,
                                                    std::ptrdiff_t bytes) {
  // An address names any byte, not only kNone.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<AssetAddress>(static_cast<std::uintptr_t>(address) +
                                   static_cast<std::uintptr_t>(bytes));
}

// The record index carried in `data` when it is below `slots`.
[[nodiscard]] constexpr std::optional<int> SharedAssetSlot(AssetAddress data,
                                                           int slots) {
  const auto value = static_cast<std::uintptr_t>(data);
  if (value < static_cast<std::uintptr_t>(slots)) {
    return static_cast<int>(value);
  }
  return std::nullopt;
}

[[nodiscard]] constexpr AssetAddress SharedAssetAddress(int slot) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<AssetAddress>(static_cast<std::uintptr_t>(slot));
}

// vs_battle_objectData (30DB0.h:36). The original reads target_slot with lb,
// model_id with lh and the last three bytes with lbu.
struct ActorAssetRequest {
  std::uint8_t kind = 0;
  std::int8_t target_slot = 0;
  std::int16_t model_id = 0;
  AssetAddress destination = AssetAddress::kNone;
  AssetAddress data = AssetAddress::kNone;
  // The four placement bytes (tile X, direction, tile Z, height).
  std::uint32_t placement = 0;
  std::uint8_t sub_kind = 0;
  std::uint8_t texture_slot = 0;
  std::uint8_t material = 0;
  std::uint8_t variant = 0;

  friend bool operator==(const ActorAssetRequest&,
                         const ActorAssetRequest&) = default;
};

// The disc file tables (LegacyFiles.h): per-model disc offsets and sector
// counts. The material-model sequence tables are indexed material * 4 +
// model, as the original's flattened arrays.
enum class AssetFileTable : std::uint8_t {
  kShpOffsets,
  kShpSectors,
  kSequenceOffsets,
  kSequenceSectors,
  kEtmOffsets,
  kEtmSectors,
  kWepOffsets,
  kWepSectors,
  kMaterialSequenceOffsets,
  kMaterialSequenceSectors,
  kMaterialModelSequenceOffsets,
  kMaterialModelSequenceSectors,
};

// The four loaders each own one disc request.
enum class AssetFileStream : std::uint8_t {
  kWeapon,
  kActorModel,
  kSequence,
  kEffectTexture,
};

enum class AssetTextureTable : std::uint8_t { kActor, kWeapon };

// Render_TextureSlot (D_800F46A8 and the weapon table).
struct AssetTextureSlot {
  std::int16_t texture_page = 0;
  std::int16_t palette = 0;
  std::int16_t alternate_palette = 0;
  std::uint8_t references = 0;

  friend bool operator==(const AssetTextureSlot&,
                         const AssetTextureSlot&) = default;
};

// The vs_battle_wepModels_t fields the loaders use.
struct WeaponAssetRecord {
  std::uint8_t bone_count = 0;
  std::uint8_t vertex_group_count = 0;
  std::uint16_t triangle_count = 0;
  std::uint16_t quad_count = 0;
  std::uint16_t other_primitive_count = 0;
  std::uint8_t actor_id = 0;  // 4 bits
  bool reserved08_bit4 = false;
  bool wide_texture = false;
  bool reserved08_bit6 = false;
  bool double_clut = false;
  std::uint8_t material = 0;  // 4 bits
  std::uint8_t palette_color_count = 0;
  std::uint8_t attachment_slot = 0;
  std::uint8_t attachment_bone = 0;
  std::uint8_t model_id = 0;
  std::uint8_t texture_slot = 0;
  std::uint8_t palette_slot_offset = 0;
  std::uint8_t reserved11 = 0;
  std::uint8_t reserved13 = 0;
  AssetAddress geometry = AssetAddress::kNone;
  std::array<std::array<std::uint16_t, kWeaponPaletteColors>,
             kWeaponPaletteSets>
      palettes{};
  std::array<std::uint16_t, kWeaponWorkingColors> working_palette{};
  std::array<std::uint16_t, kWeaponWorkingColors> upload_palette{};

  friend bool operator==(const WeaponAssetRecord&,
                         const WeaponAssetRecord&) = default;
};

struct WeaponAssetSummary {
  bool present = false;
  std::uint8_t actor_id = 0;
  std::uint8_t model_id = 0;
  std::uint8_t bone_count = 0;
};

struct ActorTextureScrollBounds {
  std::uint8_t x = 0;
  std::uint8_t y = 0;
  std::uint8_t initial_step_x = 0;
  std::uint8_t initial_step_y = 0;
  std::uint8_t interval = 0;

  friend bool operator==(const ActorTextureScrollBounds&,
                         const ActorTextureScrollBounds&) = default;
};

struct ActorTextureScroll {
  std::uint8_t x = 0;
  std::uint8_t y = 0;
  std::int8_t step_x = 0;
  std::int8_t step_y = 0;
  std::uint8_t interval = 0;
  std::uint8_t timer = 0;

  friend bool operator==(const ActorTextureScroll&,
                         const ActorTextureScroll&) = default;
};

struct ActorCompanionOffset {
  std::int8_t x = 0;
  std::int8_t y = 0;
  std::int8_t z = 0;
  std::uint8_t depth_bias = 0;

  friend bool operator==(const ActorCompanionOffset&,
                         const ActorCompanionOffset&) = default;
};

// The D_800F4538_t (actor record) fields the loaders use.
struct ActorAssetRecord {
  // The model instance header.
  std::uint8_t bone_count = 0;
  std::uint8_t vertex_group_count = 0;
  std::uint16_t triangle_count = 0;
  std::uint16_t quad_count = 0;
  std::uint16_t other_polygon_count = 0;
  bool is_skipped = false;
  bool uv_layout = false;
  std::uint8_t texture_slot_count_minus_one = 0;  // 3 bits
  bool is_parsed = false;
  std::uint8_t action_animation_state = 0;  // 4 bits
  bool is_attached = false;
  bool sound_enabled = false;
  bool weapon_drawn = false;
  std::uint8_t support_layer = 0;    // 4 bits
  std::uint8_t placement_layer = 0;  // 4 bits
  std::uint8_t palette_words = 0;    // copiedWordCount
  std::uint8_t slot = 0;
  std::uint8_t variant = 0;
  std::uint8_t attach_parent = 0;
  std::uint8_t attach_bone = 0;
  std::int16_t scale_x = 0;
  std::int16_t scale_y = 0;
  std::int16_t scale_z = 0;
  std::uint32_t base_color = 0;
  std::uint32_t tint = 0;
  std::uint32_t placement = 0;
  AssetAddress model = AssetAddress::kNone;
  // The record.
  bool holds_texture_slots = false;
  bool texture_header_bit = false;
  bool advance_requested = false;
  bool custom_tint = false;
  bool ignored_by_collision = false;
  std::uint8_t sequence_model_id = 0;
  std::uint8_t frames_per_step = 0;
  std::uint8_t locomotion_counter = 0;
  std::uint8_t sequence_flags = 0;
  std::uint8_t common_animation_count = 0;
  std::uint8_t battle_animation_count = 0;
  std::uint8_t texture_slot = 0;
  std::uint8_t travel_animation = 0;
  std::int16_t animation_length = 0;
  std::uint16_t turn_rate = 0;
  std::uint16_t move_rate = 0;
  AssetAddress owned_sequence = AssetAddress::kNone;
  AssetAddress common_sequence = AssetAddress::kNone;
  AssetAddress battle_sequence = AssetAddress::kNone;
  std::array<AssetAddress, kActorSequenceRefs> shared_sequences{};
  std::array<ActorTextureScrollBounds, kActorTextureScrolls>
      texture_scroll_bounds{};
  std::uint16_t occupancy_radius = 0;
  std::uint16_t occupancy_height = 0;
  std::uint16_t opaque648 = 0;  // the low 16 bits of the 0x648 word
  std::array<std::uint32_t, kActorBattleSequences> battle_sequence_lbas{};
  std::array<std::uint16_t, kActorBattleSequences> chain_attack_animation_ids{};
  std::array<std::uint32_t, kActorSpecialSequences> special_sequence_lbas{};
  std::array<std::uint16_t, kActorSpecialSequences> special_sequence_sizes{};
  std::int32_t opaque6dc = 0;
  std::uint16_t model_id = 0;
  std::int16_t clut_a = 0;
  std::int16_t clut_b = 0;
  std::uint32_t fade = 0;  // fadeBlue, fadeGreen, fadeRed and the pad
  std::array<std::byte, kActorPaletteBytes> palettes{};
  std::array<std::uint16_t, kActorBasePaletteColors> base_palette{};
  std::array<std::uint16_t, kActorCurrentPaletteColors> current_palette{};
  std::uint8_t companion_slot = 0;
  ActorCompanionOffset companion{};
  std::array<std::uint16_t, kActorJointSlots> joint_slots{};
  std::array<ActorTextureScroll, kActorTextureScrolls> texture_scroll{};
  std::int16_t scale = 0;
  // textureRowOffsets[3] and the byte past it (momentumDistance's high
  // byte), which bank 3 writes.
  std::array<std::uint8_t, kActorTextureRowOffsets> texture_row_offsets{};
  std::uint32_t model_data_size = 0;

  friend bool operator==(const ActorAssetRecord&,
                         const ActorAssetRecord&) = default;
};

struct ActorAssetSummary {
  bool present = false;
  std::uint16_t model_id = 0;
  std::uint8_t sequence_model_id = 0;
  AssetAddress common_sequence = AssetAddress::kNone;
};

// One shared sequence bank (D_800F4588 .. D_800F45E0): the file, its
// animation ids, entry parameters and per-animation data, each list ended
// by -1.
struct SharedSequenceBank {
  AssetAddress data = AssetAddress::kNone;
  std::array<std::int16_t, kSharedSequenceEntries> animation_ids{};
  std::array<std::int16_t, kSharedSequenceEntries> entry_params{};
  std::array<AssetAddress, kSharedSequenceEntries> animation_data{};

  friend bool operator==(const SharedSequenceBank&,
                         const SharedSequenceBank&) = default;
};

// The loaders' phases, buffers and parse cursors (_loadShpState, D_800E8F29,
// D_800F2288 ...), kept between frames.
struct ActorAssetLoadState {
  std::uint8_t model_phase = 0;
  std::uint8_t parse_phase = 0;
  std::uint8_t sequence_parse_phase = 0;
  std::uint8_t effect_texture_phase = 0;
  bool weapon_file_loaded = false;
  bool model_file_allocated = false;
  bool sequence_file_owned = false;
  AssetAddress weapon_file = AssetAddress::kNone;
  AssetAddress model_file = AssetAddress::kNone;
  AssetAddress sequence_file = AssetAddress::kNone;
  // The ETM cursor: the file plus 4 once the image is uploaded.
  AssetAddress effect_texture_file = AssetAddress::kNone;
  AssetAddress weapon_cursor = AssetAddress::kNone;
  AssetAddress model_cursor = AssetAddress::kNone;
  AssetAddress sequence_cursor = AssetAddress::kNone;
  std::uint8_t model_palette_words = 0;
  std::int32_t model_header_word = 0;
  std::int32_t model_texture_width = 0;
  std::int32_t model_texture_height = 0;

  friend bool operator==(const ActorAssetLoadState&,
                         const ActorAssetLoadState&) = default;
};

// D_800E8FC4, D_800F457C, D_800F4580 and D_800F45D8: the slowest request
// while a model file was parsing.
struct ActorAssetQueueStats {
  std::int32_t peak_frame_ticks = 0;
  std::int32_t last_load_phase = 0;
  std::int32_t frame_ticks = 0;
  std::int32_t last_kind = 0;

  friend bool operator==(const ActorAssetQueueStats&,
                         const ActorAssetQueueStats&) = default;
};

// The records, heap, drive, VRAM and actor services the loaders use.
class ActorAssetHost {
 public:
  ActorAssetHost() = default;
  ActorAssetHost(const ActorAssetHost&) = delete;
  ActorAssetHost& operator=(const ActorAssetHost&) = delete;
  ActorAssetHost(ActorAssetHost&&) = delete;
  ActorAssetHost& operator=(ActorAssetHost&&) = delete;
  virtual ~ActorAssetHost() = default;

  // The request queue, in place: a request being processed can queue more.
  virtual std::uint8_t RequestKind(int slot) const = 0;
  virtual void StoreRequest(int slot, const ActorAssetRequest& request) = 0;
  // A request QueueActorAssetRequest accepted, before it is stored.
  virtual void OnRequestQueued(const ActorAssetRequest& request) = 0;
  virtual void MoveRequest(int from, int to) = 0;
  virtual void ClearRequestKind(int slot) = 0;
  // Runs the head request's loader; returns its result.
  virtual int ProcessHeadRequest(ActorAssetKind kind) = 0;
  virtual std::int32_t FrameCounter() = 0;
  virtual std::uint8_t ModelLoadPhase() const = 0;
  virtual void OnHeadRequestDone(int result) = 0;

  // The game heap. Allocations return kNone when refused.
  virtual AssetAddress AllocateHigh(std::size_t bytes) = 0;
  virtual AssetAddress AllocateLow(std::size_t bytes) = 0;
  virtual void FreeHigh(AssetAddress block) = 0;
  virtual void FreeLow(AssetAddress block) = 0;
  // The bytes from `address`: to the end of its heap block, or `wanted`
  // bytes outside the heap; empty for an unknown address.
  virtual std::span<std::byte> Bytes(AssetAddress address,
                                     std::size_t wanted) = 0;

  // The disc file tables and the loaders' disc requests.
  virtual std::optional<std::uint32_t> FileTableEntry(AssetFileTable table,
                                                      std::size_t index) = 0;
  // Starts an asynchronous read; false when the queue has no free request
  // (#282), in which case nothing was started.
  virtual bool BeginFileRead(AssetFileStream stream, DiscFileExtent file,
                             AssetAddress destination) = 0;
  // Reads the file before returning, for a read the queue refused.
  virtual void ReadRefusedFile(DiscFileExtent file,
                               AssetAddress destination) = 0;
  // True once the stream's read has loaded (or no read is outstanding).
  virtual bool FileReadFinished(AssetFileStream stream) = 0;
  virtual void ReleaseFileRead(AssetFileStream stream) = 0;

  // VRAM texture slots, pages and uploads.
  virtual AssetTextureSlot TextureSlot(AssetTextureTable table,
                                       int index) const = 0;
  virtual void SetTextureSlot(AssetTextureTable table, int index,
                              const AssetTextureSlot& slot) = 0;
  // D_800E8F30 (x, y halfword pairs) and D_800E8F90 (row offsets).
  virtual std::span<const std::uint8_t> TextureSlotOrigins() const = 0;
  virtual std::span<const std::uint8_t> TextureSlotRowOffsets() const = 0;
  virtual int ReserveTexturePages(std::uint32_t first, std::uint32_t count) = 0;
  virtual int RetireTexturePages(std::uint32_t first, std::uint32_t count) = 0;
  // `pixels` holds rect.w * rect.h 16-bit pixels.
  virtual void UploadImage(const VramRect& rect,
                           std::span<const std::byte> pixels) = 0;
  virtual void UploadClut(std::span<const std::uint16_t> colors,
                          std::uint32_t row, std::uint32_t first,
                          std::uint32_t count) = 0;

  // Weapon records (D_800F4540).
  virtual WeaponAssetSummary WeaponSummary(int slot) const = 0;
  // Points an empty slot at `destination`, cleared, with no texture slot;
  // false when `destination` is kNone.
  virtual bool BindWeaponRecord(int slot, AssetAddress destination) = 0;
  virtual WeaponAssetRecord LoadWeapon(int slot) const = 0;
  virtual void StoreWeapon(int slot, const WeaponAssetRecord& record) = 0;
  virtual void CopyWeaponRecord(int target, int source) = 0;
  virtual void WriteWeaponBones(int slot, std::span<const std::byte> bones) = 0;
  virtual void ClearWeaponSlot(int slot) = 0;
  // The fixed geometry buffer of weapon slots 0 and 1 (D_8010245C).
  virtual AssetAddress WeaponGeometryBuffer(int slot) const = 0;
  // D_800EB9C4: the palette index of each material.
  virtual std::span<const std::uint8_t> WeaponMaterialPalettes() const = 0;
  virtual int FindActorBoneTag(int actor, int tag) = 0;

  // Actor records (D_800F4538).
  virtual ActorAssetSummary ActorSummary(int slot) const = 0;
  virtual bool BindActorRecord(int slot, AssetAddress destination) = 0;
  virtual ActorAssetRecord LoadActor(int slot) const = 0;
  virtual void StoreActor(int slot, const ActorAssetRecord& record) = 0;
  virtual void CopyActorRecord(int target, int source) = 0;
  virtual void WriteActorTextureLayout(int slot,
                                       std::span<const std::byte> layout) = 0;
  virtual void ClearActorSlot(int slot) = 0;
  virtual std::int16_t RoomActorScale() const = 0;
  virtual void ResetActorMotion(int slot) = 0;
  virtual void ClearActorPaletteFade(int slot) = 0;
  virtual void PlaceActorInRoom(int slot, std::uint32_t placement,
                                int mode) = 0;
  virtual void AttachActorToParentBone(int slot, int parent, int bone) = 0;
  virtual void StopModelSounds(AssetAddress sound_bank) = 0;

  // Shared sequence banks and buffers.
  virtual SharedSequenceBank LoadSequenceBank(int bank) const = 0;
  virtual void StoreSequenceBank(int bank, const SharedSequenceBank& value) = 0;
  virtual std::uint8_t BankTexturePage(int bank) const = 0;
  virtual void SetBankTexturePage(int bank, std::uint8_t page) = 0;
  // D_800EB6D4: the shared animation id each model kind uses.
  virtual std::span<const std::uint8_t> SharedSequenceIdsByKind() const = 0;
  virtual AssetAddress PrimarySequenceBuffer() const = 0;
  virtual AssetAddress SequenceWorkBuffer() const = 0;
  virtual void ResetActorTextureFrames(int bank) = 0;

  // Native bookkeeping for the model and sequence validators.
  virtual void RecordModelGeometry(AssetAddress model, std::size_t bytes) = 0;
  virtual void RecordModelCopy(AssetAddress destination, AssetAddress source,
                               std::size_t bytes) = 0;
  virtual void ForgetModelGeometry(AssetAddress model) = 0;
  virtual void RecordSequenceBank(AssetAddress bank, std::size_t bytes) = 0;
  virtual void RecordEmbeddedSequenceBank(AssetAddress file) = 0;
  virtual void RecordSequenceCopy(AssetAddress destination, AssetAddress source,
                                  std::size_t bytes) = 0;
  // A refused input or a truncated copy.
  virtual void Refuse(const char* what, std::int64_t value) = 0;
};

// The request queue (30DB0.c:149-260).
[[nodiscard]] int FindFreeActorAssetRequest(const ActorAssetHost& host);
int QueueActorAssetRequest(ActorAssetHost& host,
                           const ActorAssetRequest& request);
int UpdateActorAssetQueue(ActorAssetHost& host, ActorAssetQueueStats& stats);
void QueueAshleyWeaponSequence(ActorAssetHost& host, int sequence_id);

// Adds the texture offsets to `count` polygons' UVs (func_80099E7C).
// Refuses (false) when the polygons run past `polygons`.
bool OffsetModelTextureCoordinates(std::span<std::byte> polygons, int count,
                                   bool colored, int u_offset, int v_offset);
// Copies `bytes` from `source` into `destination`, or into a new low-end
// block when `destination` is kNone (func_8009A028).
AssetAddress CopyModelGeometry(ActorAssetHost& host, AssetAddress destination,
                               AssetAddress source, std::int32_t bytes);

// WEP files (30DB0.c:489-929).
int FindSharedWeaponModel(const ActorAssetHost& host,
                          ActorAssetRequest& request);
int UpdateWeaponModelLoad(ActorAssetHost& host, ActorAssetLoadState& state,
                          ActorAssetRequest& request);
int ParseWeaponModel(ActorAssetHost& host, ActorAssetLoadState& state,
                     ActorAssetRequest& request);
bool LoadWeaponMaterialPalette(ActorAssetHost& host, int slot, int material);
int ReleaseWeaponModel(ActorAssetHost& host, int slot);

// SHP files (30DB0.c:930-1432).
int FindSharedActorModel(const ActorAssetHost& host,
                         ActorAssetRequest& request);
int UpdateActorModelLoad(ActorAssetHost& host, ActorAssetLoadState& state,
                         ActorAssetRequest& request);
int ParseActorModel(ActorAssetHost& host, ActorAssetLoadState& state,
                    ActorAssetRequest& request);
int ReleaseActorModel(ActorAssetHost& host, int slot);

// SEQ and ETM files (30DB0.c:1433-2060).
int FindSharedActorSequence(const ActorAssetHost& host,
                            ActorAssetRequest& request);
int UpdateActorSequenceLoad(ActorAssetHost& host, ActorAssetLoadState& state,
                            ActorAssetRequest& request);
void CopyWeaponSequence(ActorAssetHost& host, AssetAddress destination,
                        AssetAddress source);
int ParseActorSequence(ActorAssetHost& host, ActorAssetLoadState& state,
                       ActorAssetRequest& request);
// Points the actor at its animation in `bank` (func_8009CAEC); false when
// the bank index is outside the banks.
bool BindSharedActorSequence(const SharedSequenceBank& bank, int bank_index,
                             std::span<const std::uint8_t> ids_by_kind,
                             ActorAssetRecord& actor);
int ReleaseActorSequence(ActorAssetHost& host, int target, int kind);
int ReleaseSequenceEffectTexture(ActorAssetHost& host, int bank);
int UpdateSequenceEffectTextureLoad(ActorAssetHost& host,
                                    ActorAssetLoadState& state,
                                    const ActorAssetRequest& request);
}  // namespace VS::Content
