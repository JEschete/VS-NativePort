#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Content/ActorAssets.hpp"

// MENU9.PRG's encyclopedia preview (57BC.c:10-59, 592C.c:171-299): the
// enemy model beside the encyclopedia entry. A ZUD file (the enemy's SHP, its
// weapon and shield WEPs and its sequences) is read into a high-end heap
// block; once read, its parts are queued to the battle asset queue into a
// model buffer of the preview layout, and the file block is released after
// every part has loaded. A failed read or part abandons the whole load (#48).
// The functions reach the load step, the ZUD file
// table, the disc queue, the game heap, the asset queue and the actor
// systems through an EnemyPreviewHost (EncyclopediaPreviewLive.hpp binds it).
namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
using Content::AssetAddress;

// The ZUD file table's extent (_zudLbas, _zudSizes, _paletteFlags).
inline constexpr int kEnemyPreviewFiles = 256;
// The actor slots the preview occupies: the enemy and its companion, and the
// weapon model slots of the enemy's weapon and shield.
inline constexpr int kEnemyPreviewActorSlot = 1;
inline constexpr int kEnemyPreviewCompanionSlot = 16;
inline constexpr int kEnemyPreviewWeaponSlot = 2;
inline constexpr int kEnemyPreviewShieldSlot = 3;
// The enemies drawn with a companion record (_modelVariants) and the
// companion model of each (_variantMap, indexed by the variant). Variant 0
// means no companion, so the first listed enemy (30) loads without one.
template <std::size_t N>
using IdTable = std::array<std::uint8_t, N>;
inline constexpr IdTable<6> kCompanionEnemies{30, 31, 32, 33, 172, 173};
inline constexpr IdTable<8> kCompanionModels{27, 27, 72, 72, 80, 80, 72, 52};
// The animation of Ashley's encyclopedia pose and the display flags of the
// preview (57BC.c:53-57).
inline constexpr std::uint8_t kAshleyPreviewAnimation = 33;
inline constexpr int kAshleyPreviewDisplayFlags = 3;
// The ZUD header's bytes (_zudData_t, 592C.c:25).
inline constexpr std::size_t kZudHeaderBytes = 0x30;
// The most parts one ZUD file queues: the enemy, its weapon and shield (or
// companion) and its two sequences.
inline constexpr std::size_t kEnemyPreviewParts = 5;
// Native (#48): LoadEnemyPreview's result when the load failed. The preview
// has cancelled its requests, released its records and freed its blocks.
inline constexpr int kEnemyPreviewFailed = -1;

// The load steps LoadEnemyPreview advances (vs_battle_loadObjectDataState):
// queue the read, queue the parts until consumed, pose the preview.
enum class EnemyPreviewStep : std::uint8_t {
  kQueueRead = 0,
  kLoadParts = 1,
  kPose = 2,
};

// The state of the ZUD file's disc read.
enum class EnemyPreviewRead : std::uint8_t {
  kPending,
  kLoaded,
  kFailed,
};

// One entry of the ZUD file table.
struct EnemyPreviewFile {
  std::int32_t sector = 0;
  std::int32_t bytes = 0;
  std::uint8_t palette_flag = 0;

  friend bool operator==(const EnemyPreviewFile&,
                         const EnemyPreviewFile&) = default;
};

// A section of a ZUD file: a byte offset from the file's start and a length.
struct ZudSection {
  std::uint32_t offset = 0;
  std::uint32_t bytes = 0;

  friend bool operator==(const ZudSection&, const ZudSection&) = default;
};

// _zudData_t (592C.c:25), read from the file's bytes.
struct ZudHeader {
  std::uint8_t character_id = 0;
  std::uint8_t weapon = 0;
  std::uint8_t battle_sequence_model = 0;
  std::uint8_t weapon_material = 0;
  std::uint8_t shield = 0;
  std::uint8_t shield_material = 0;
  std::uint8_t default_material = 0;
  ZudSection shape;
  ZudSection weapon_model;
  ZudSection shield_model;
  ZudSection common_sequence;
  ZudSection battle_sequence;

  friend bool operator==(const ZudHeader&, const ZudHeader&) = default;
};

// The header of a ZUD file, or none when the file is shorter than one.
[[nodiscard]] std::optional<ZudHeader> ParseZudHeader(
    std::span<const std::byte> file);
// Whether `section` lies within a file of `file_bytes`.
[[nodiscard]] bool ZudSectionFits(const ZudSection& section,
                                  std::size_t file_bytes);
// The companion variant of enemy `zud_id`: its index in kCompanionEnemies,
// or 0 when it has none (592C.c:187-194).
[[nodiscard]] std::uint8_t CompanionVariant(int zud_id);

// The model buffer's layouts: an enemy with its weapon and shield models, or
// an enemy with a companion record.
enum class EnemyPreviewLayout : std::uint8_t {
  kEquipped,
  kCompanion,
};

// The parts of a model buffer: the enemy's record, and the companion record
// (kCompanion) or the weapon and shield models (kEquipped) after it.
enum class EnemyPreviewPart : std::uint8_t {
  kEnemy,
  kCompanion,
  kWeapon,
  kShield,
};

// The preview's state (592C.c's statics, zeroed when MENU9.PRG loads).
struct EnemyPreviewState {
  // The enemy read last (written only, as the original's static).
  int zud_id = 0;
  // The pending disc read of the ZUD file, until its parts are queued.
  AssetAddress read = AssetAddress::kNone;
  // The ZUD file's heap block, until the asset queue has consumed it.
  AssetAddress file = AssetAddress::kNone;
  std::uint32_t file_bytes = 0;
  // The model buffer, until FreeEnemyModelBuffer.
  AssetAddress model_buffer = AssetAddress::kNone;
  std::uint8_t variant = 0;
  std::uint8_t palette_flag = 0;
  // Native queue identity; an address can be reused after a read is released.
  std::uint64_t read_generation = 0;
  // Native (#48): the tickets of the queued parts in submission order, kNone
  // past the last, until every part has loaded or the load is abandoned.
  std::array<Content::ActorAssetTicket, kEnemyPreviewParts> tickets{};

  friend bool operator==(const EnemyPreviewState&,
                         const EnemyPreviewState&) = default;
};

class EnemyPreviewHost {
 public:
  EnemyPreviewHost() = default;
  EnemyPreviewHost(const EnemyPreviewHost&) = delete;
  EnemyPreviewHost& operator=(const EnemyPreviewHost&) = delete;
  EnemyPreviewHost(EnemyPreviewHost&&) = delete;
  EnemyPreviewHost& operator=(EnemyPreviewHost&&) = delete;
  virtual ~EnemyPreviewHost() = default;

  // The shared model load step (vs_battle_loadObjectDataState).
  [[nodiscard]] virtual std::uint8_t LoadStep() const = 0;
  virtual void SetLoadStep(std::uint8_t step) = 0;

  // The ZUD file table entry of `zud_id` (below kEnemyPreviewFiles).
  [[nodiscard]] virtual EnemyPreviewFile File(int zud_id) const = 0;

  // The disc queue: a read request for `file` (kNone when the queue is
  // full), its priority enqueue into `destination`, the read of a refused
  // request (kNone when it cannot be recorded), its state and its release.
  [[nodiscard]] virtual AssetAddress AllocateRead(
      const EnemyPreviewFile& file) = 0;
  virtual void EnqueueRead(AssetAddress read, AssetAddress destination) = 0;
  [[nodiscard]] virtual AssetAddress ReadRefused(const EnemyPreviewFile& file,
                                                 AssetAddress destination) = 0;
  [[nodiscard]] virtual EnemyPreviewRead ReadStatus(
      AssetAddress read) const = 0;
  virtual void ReleaseRead(AssetAddress read) = 0;

  // The game heap: the file block from the high end, the model buffer from
  // the low end, and their releases (both through freeHeapR).
  [[nodiscard]] virtual AssetAddress AllocateFile(std::uint32_t bytes) = 0;
  virtual void FreeFile(AssetAddress file) = 0;
  [[nodiscard]] virtual AssetAddress AllocateModelBuffer(
      EnemyPreviewLayout layout) = 0;
  virtual void FreeModelBuffer(AssetAddress buffer) = 0;
  // The address of `part` in a model buffer.
  [[nodiscard]] virtual AssetAddress ModelPart(AssetAddress buffer,
                                               EnemyPreviewPart part) const = 0;
  // The bytes of the file block, `bytes` long, or empty when the block
  // cannot hold them.
  [[nodiscard]] virtual std::span<const std::byte> FileBytes(
      AssetAddress file, std::uint32_t bytes) const = 0;

  // The battle asset queue (#48): a part submitted under the preview's owner
  // (none when the queue or its ticket table is full or the request is
  // refused), one queue step, a part's outcome, forgetting a settled ticket,
  // and the cancellation of every preview request (false, changing nothing,
  // when the queue's head cannot be abandoned).
  [[nodiscard]] virtual std::optional<Content::ActorAssetTicket> SubmitAsset(
      const Content::ActorAssetRequest& request) = 0;
  virtual void UpdateAssetQueue() = 0;
  [[nodiscard]] virtual Content::ActorAssetOutcome AssetOutcome(
      Content::ActorAssetTicket ticket) = 0;
  virtual void ForgetAsset(Content::ActorAssetTicket ticket) = 0;
  [[nodiscard]] virtual bool CancelAssets() = 0;
  // Releases the record bound at `part` of `buffer`, if one is bound there.
  // False when the record stays bound or releasing it would release a
  // record outside the buffer; the buffer must then be kept.
  [[nodiscard]] virtual bool ReleasePart(AssetAddress buffer,
                                         EnemyPreviewPart part) = 0;

  // The actor systems (ActorPreviewLive.hpp, AnimationControlLive.hpp,
  // AnimationPoseLive.hpp, ActorStateLive.hpp) and the record writes.
  virtual void ResetPreviewPose() = 0;
  virtual void SetWeaponDrawn(int slot, bool drawn) = 0;
  virtual void SetAnimation(int slot, std::uint8_t animation) = 0;
  virtual void PlayAnimation(int slot, int animation, int blend_frames) = 0;
  virtual void InitializeAnimationPose(int slot) = 0;
  virtual void SetDisplayFlags(int slot, int flags) = 0;
  virtual void ClearCompanion(int slot) = 0;

  virtual void Refuse(std::string_view what) = 0;
};

// decomp: MENU9.PRG vs_menu9_loadZud (F02836, src/MENU/MENU9.PRG/57BC.c:10)
// One load step: 1 while loading, 0 once the preview is posed, and
// kEnemyPreviewFailed (resetting the step) when the load failed.
int LoadEnemyPreview(EnemyPreviewHost& host, EnemyPreviewState& state,
                     int zud_id);
// decomp: MENU9.PRG vs_menu9_freeZud (F02837, src/MENU/MENU9.PRG/57BC.c:44)
// Cancels any queued part and releases the records bound in the model
// buffer before freeing it; a record that cannot be released keeps it.
void FreeEnemyPreview(EnemyPreviewHost& host, EnemyPreviewState& state);
// decomp: MENU9.PRG func_801080C8 (F02838, src/MENU/MENU9.PRG/57BC.c:53)
void InitializeAshleyPreviewPose(EnemyPreviewHost& host);
// decomp: MENU9.PRG vs_menu9_loadZudFile (F02839,
// src/MENU/MENU9.PRG/592C.c:172): false, holding nothing new, when no read
// was queued.
[[nodiscard]] bool QueueEnemyModelRead(EnemyPreviewHost& host,
                                       EnemyPreviewState& state, int zud_id);
// decomp: MENU9.PRG vs_menu9_parseZudFile (F02840,
// src/MENU/MENU9.PRG/592C.c:206): 1 while the read or the queue is pending
// or after the parts are queued, 0 once no file block is held, and
// kEnemyPreviewFailed once a failed read, part or submission has abandoned
// the load.
int LoadEnemyModelParts(EnemyPreviewHost& host, EnemyPreviewState& state);
// decomp: MENU9.PRG vs_menu9_freeZudFile (F02841,
// src/MENU/MENU9.PRG/592C.c:300)
void FreeEnemyModelBuffer(EnemyPreviewHost& host, EnemyPreviewState& state);
}  // namespace VS::Menu::Score
