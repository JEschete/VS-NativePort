#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

// The memory-card save files of MENU7.PRG (260.c) and TITLE.PRG (22C.c): file
// names, the save-image cipher and block checksums, the directory scan, the
// card-readiness and transfer state machines, and packaging and applying a
// save image. Card I/O and the game state reach the cores through hosts. A
// save image read from a card or the disc is untrusted: it is only read
// through fixed-size byte spans, and the indices it carries are checked
// before any of it is applied.
namespace VS::Save::MemoryCard {

inline constexpr std::size_t kBlockBytes = 0x100;
inline constexpr std::size_t kRecordBytes = 0x80;
inline constexpr std::size_t kSaveImageBytes = 0x5C00;
inline constexpr std::size_t kImageBlockCount = kSaveImageBytes / kBlockBytes;
// The card header, icon and summary records a directory scan reads.
inline constexpr std::size_t kHeaderBytes = 0x200;
// The blocks _applyLoadedSaveFile checks before applying: everything it
// applies lies in them, and the intro snapshots on the disc are this long.
inline constexpr std::size_t kAppliedBlockCount = 32;
inline constexpr std::size_t kSlotCount = 5;
inline constexpr std::size_t kDirectoryCapacity = 15;
inline constexpr std::size_t kFilenameBytes = 32;
inline constexpr std::size_t kDirectoryNameBytes = 20;
inline constexpr std::uint32_t kSaveSignature = 0x20000107;
// Slot states at or above this are in-use saves; the value is the save's
// random identity.
inline constexpr std::uint32_t kFirstInUseSlotState = 3;

using SaveImage = std::span<std::byte, kSaveImageBytes>;
using ConstSaveImage = std::span<const std::byte, kSaveImageBytes>;
// Title_saveFileSummaries: one 0x80-byte summary record per slot, laid out as
// the image's bytes 0x180..0x1FF (key, slot state, generation, signature,
// statistics, block checksums).
using SummaryTable = std::span<std::byte, kSlotCount * kRecordBytes>;
using ConstSummaryTable = std::span<const std::byte, kSlotCount * kRecordBytes>;
// A NUL-terminated memory-card path such as "bu00:BASLUS-01040VAG1".
using CardFilename = std::array<char, kFilenameBytes>;

// Byte offsets in a Save_GameSaveImage.
inline constexpr std::size_t kCardMagicOffset = 0;
inline constexpr std::size_t kCardTitleOffset = 4;
inline constexpr std::size_t kCardTitleBytes = 0x40;
inline constexpr std::size_t kCardTitleTailOffset = 0x24;
inline constexpr std::size_t kIconPaletteOffset = 0x60;
inline constexpr std::size_t kIconPaletteBytes = 0x20;
inline constexpr std::size_t kIconBitmapOffset = 0x80;
inline constexpr std::size_t kIconBitmapBytes = 0x80;
inline constexpr std::size_t kSummaryOffset = 0x180;
// The cipher covers everything after the summary's key.
inline constexpr std::size_t kCipherStart = kSummaryOffset + 4;
inline constexpr std::size_t kChecksumOffset = 0x1A4;
inline constexpr std::size_t kContainerOffset = 0x1DE0;
inline constexpr std::size_t kContainerBytes = 0x3C00;
inline constexpr std::size_t kContainerMiscOffset = kContainerOffset + 0x3800;
// _packageGameSaveData (22C.c:616) clears this much of the container's misc
// items for a clear save.
inline constexpr std::size_t kClearedContainerMiscBytes = 0x100;

// Byte offsets in a summary record.
inline constexpr std::size_t kSummaryKeyOffset = 0;
inline constexpr std::size_t kSummarySlotStateOffset = 4;
inline constexpr std::size_t kSummaryGenerationOffset = 8;
inline constexpr std::size_t kSummarySignatureOffset = 0xC;
inline constexpr std::size_t kSummaryStatsOffset = 0x10;
inline constexpr std::size_t kStatsBytes = 0x14;

// Byte offsets in the save statistics (Save_SaveFileStats).
inline constexpr std::size_t kStatsGameTimeOffset = 0;
inline constexpr std::size_t kStatsCumulativeSavesOffset = 4;
inline constexpr std::size_t kStatsCurrentGameSavesOffset = 6;
inline constexpr std::size_t kStatsHpOffset = 8;
inline constexpr std::size_t kStatsMaxHpOffset = 0xA;
inline constexpr std::size_t kStatsSaveLocationOffset = 0xC;
inline constexpr std::size_t kStatsClearCountOffset = 0xD;
inline constexpr std::size_t kStatsMapCompletionOffset = 0xE;
inline constexpr std::size_t kStatsMpOffset = 0x10;
inline constexpr std::size_t kStatsMaxMpOffset = 0x12;

// Byte offsets in the settings section (State_Settings).
inline constexpr std::size_t kSettingsBytes = 0x20;
inline constexpr std::size_t kSettingsFlagsOffset = 0;
inline constexpr std::size_t kSettingsMenuFlagsOffset = 2;
inline constexpr std::size_t kSettingsSlotStateOffset = 4;
inline constexpr std::size_t kSettingsMonoSoundOffset = 0xA;
inline constexpr std::size_t kSettingsKeyOffset = 0x14;
inline constexpr std::size_t kSettingsCumulativeSavesOffset = 0x18;
inline constexpr std::size_t kSettingsCurrentGameSavesOffset = 0x1A;
inline constexpr std::size_t kSettingsGenerationOffset = 0x1C;

// Byte offsets in the Ashley snapshot section (State_AshleySnapshot).
inline constexpr std::size_t kSnapshotHpOffset = 4;
inline constexpr std::size_t kSnapshotMaxHpOffset = 6;
inline constexpr std::size_t kSnapshotMpOffset = 8;
inline constexpr std::size_t kSnapshotMaxMpOffset = 0xA;

// Byte offsets in the state-flag section (State_StateFlags).
inline constexpr std::size_t kStateFlagsClearCountOffset = 0;
inline constexpr std::size_t kStateFlagsPuzzleModeOffset = 0x11C;

// The misc items of the inventory and inventory-index sections.
inline constexpr std::size_t kInventoryMiscOffset = 0xE00;
inline constexpr std::size_t kInventoryMiscBytes = 0x100;
inline constexpr std::size_t kInventoryIndicesMiscOffset = 0x70;
inline constexpr std::size_t kInventoryIndicesMiscBytes = 0x40;

// The recent-room enemy states (State_RoomEnemyStates[4]).
inline constexpr std::size_t kRoomStateSlotCount = 4;
inline constexpr std::size_t kRoomEnemyStatesBytes = 0x148;
inline constexpr std::size_t kRoomEnemyNextSlotOffset = 4;
inline constexpr std::uint32_t kRoomEnemyActorStateCount = 8;

// The game state a save image carries, in the order the image stores it.
enum class SaveSection : std::uint8_t {
  kStateFlags,
  kLearnedActions,
  kMapStatus,
  kSettings,
  kAshleySnapshot,
  kInventory,
  kInventoryIndices,
  kEntryDoor,
  kScoreData,
  kRoomStateSlot,
  kRecentRoomEnemyStates,
  kArtsStatus,
};
inline constexpr std::size_t kSaveSectionCount = 12;

struct SectionLayout {
  std::size_t offset = 0;
  std::size_t size = 0;
};

using SectionLayoutTable = std::array<SectionLayout, kSaveSectionCount>;
inline constexpr SectionLayoutTable kSectionLayouts = {
    SectionLayout{.offset = 0x200, .size = 0x440},
    SectionLayout{.offset = 0x640, .size = 0x20},
    SectionLayout{.offset = 0x660, .size = 0x48},
    SectionLayout{.offset = 0x6A8, .size = 0x20},
    SectionLayout{.offset = 0x6C8, .size = 0x100},
    SectionLayout{.offset = 0x7C8, .size = 0xF00},
    SectionLayout{.offset = 0x16C8, .size = 0xB0},
    SectionLayout{.offset = 0x1778, .size = 0xC},
    SectionLayout{.offset = 0x1784, .size = 0x114},
    SectionLayout{.offset = 0x1898, .size = 4},
    SectionLayout{.offset = 0x189C, .size = 0x520},
    SectionLayout{.offset = 0x1DBC, .size = 0x24},
};

[[nodiscard]] constexpr SectionLayout LayoutOf(SaveSection section) {
  return kSectionLayouts.at(static_cast<std::size_t>(section));
}

enum class SaveImageError : std::uint8_t {
  // Fewer bytes than the requested checksum blocks, or more blocks than an
  // image has.
  kBlockCountOutOfRange,
  kChecksumMismatch,
  kSignatureMismatch,
  // The image names a recent-room slot outside State_recentRoomEnemyStates.
  kRoomStateSlotOutOfRange,
  // A recent-room record's next actor slot is outside its eight slots.
  kRoomEnemySlotOutOfRange,
  // A host returned live state of a different size than the section.
  kSectionSizeMismatch,
  // The save's file icon index is outside the loaded icons.
  kFileIconOutOfRange,
};

enum class CardError : std::uint8_t {
  kEraseFailed,
  kCreateFailed,
};

// _testMemcardEvents' event indices; kNone when no event fired.
enum class CardEvent : std::uint8_t {
  kIoEnd = 0,
  kError = 1,
  kTimeout = 2,
  kUnformatted = 3,
  kNone = 4,
};

// The software (card file I/O) and hardware (card controller) descriptors.
enum class CardEventGroup : std::uint8_t {
  kSoftware = 0,
  kHardware = 4,
};

// _memcardEventHandler's results (Save_CardReadinessResult). A card that was
// cleared after reporting unformatted adds four.
enum class CardReadiness : std::uint8_t {
  kPending = 0,
  kReady = 1,
  kTimeout = 2,
  kUnformatted = 3,
  kReadyAfterClear = 5,
  kUnformattedAfterClear = 7,
};

enum class CardReadinessPhase : std::uint8_t {
  kInit,
  kReady,
  kUnformatted,
  kConfirmed,
  kLoadReady,
  kLoaded,
};

struct CardReadinessState {
  CardReadinessPhase phase = CardReadinessPhase::kInit;
  // (port - 1) * 16, the libcard channel.
  std::int32_t channel = 0;
  std::int8_t init_polls = 0;
  std::int8_t event_polls = 0;
  CardEventGroup event_group = CardEventGroup::kSoftware;
};

// A transfer step's result: still running, finished, or given up.
enum class TransferStatus : std::int8_t {
  kFailed = -1,
  kPending = 0,
  kDone = 1,
};

enum class SaveReadPhase : std::uint8_t { kOpen, kReading };

struct SaveReadState {
  SaveReadPhase phase = SaveReadPhase::kOpen;
  std::int8_t channel = 0;
  // Bits 0-2 the file number; bit 3 reads the temporary file instead.
  std::int8_t file_index = 0;
  bool header_only = false;
  std::int8_t errors = 0;
  std::int32_t file = 0;
};

enum class SaveWritePhase : std::uint8_t {
  kRename,
  kWrite,
  kWritePending,
  kVerify,
  kCommit,
};

struct SaveWriteState {
  SaveWritePhase phase = SaveWritePhase::kRename;
  std::int8_t file_index = 0;
  std::int8_t channel = 0;
  std::int8_t errors = 0;
  std::int8_t rename_errors = 0;
  std::int32_t file = 0;
};

// The transfer progress bar both steps advance (Title_transferProgress*).
struct TransferProgress {
  std::uint16_t position = 0;
  std::uint16_t previous = 0;
  std::uint16_t target = 0;
  std::int8_t error_offset = 0;
};

// The low-end memory-card workspace: the image being written and the image
// last read.
struct SaveWorkspace {
  SaveImage outgoing;
  SaveImage incoming;
};

struct CardDirectoryEntry {
  std::array<char, kDirectoryNameBytes> name{};
  std::int32_t size = 0;
};
// An empty slot ends the directory.
using CardDirectorySlot = std::optional<CardDirectoryEntry>;
using CardDirectory = std::array<CardDirectorySlot, kDirectoryCapacity>;

// libapi open() flags.
inline constexpr std::uint32_t kOpenRead = 0x1;
inline constexpr std::uint32_t kOpenWrite = 0x2;
inline constexpr std::uint32_t kOpenCreate = 0x200;
inline constexpr std::uint32_t kOpenNoWait = 0x8000;
inline constexpr unsigned kOpenBlockCountShift = 16;

// The libapi file calls and libcard events the card cores issue.
class CardHost {
 public:
  virtual ~CardHost() = default;
  // TestEvent on the group's descriptor for the event.
  virtual bool TestEvent(CardEventGroup group, CardEvent event) = 0;
  // _card_info, _card_clear and _card_load; true once the request started.
  virtual bool StartCardInfo(std::int32_t channel) = 0;
  virtual bool StartCardClear(std::int32_t channel) = 0;
  virtual bool StartCardLoad(std::int32_t channel) = 0;
  // A descriptor, or nullopt when open() returned -1.
  virtual std::optional<std::int32_t> Open(const CardFilename& path,
                                           std::uint32_t flags) = 0;
  // The bytes read, or nullopt when read() returned -1.
  virtual std::optional<std::int32_t> Read(std::int32_t file,
                                           std::span<std::byte> bytes) = 0;
  // False when write() returned -1.
  virtual bool Write(std::int32_t file, std::span<const std::byte> bytes) = 0;
  virtual void Close(std::int32_t file) = 0;
  virtual bool Erase(const CardFilename& path) = 0;
  virtual bool Rename(const CardFilename& from, const CardFilename& to) = 0;
  // firstfile() into directory entry 0, then nextfile() into entry `index`.
  virtual std::optional<CardDirectoryEntry> FirstFile(
      const CardFilename& pattern) = 0;
  virtual std::optional<CardDirectoryEntry> NextFile(std::size_t index) = 0;
};

// The game state packaging and applying a save reads and writes.
class SaveStateHost {
 public:
  virtual ~SaveStateHost() = default;
  // The live state behind a section; it must be the section's size.
  virtual std::span<std::byte> Section(SaveSection section) = 0;
  virtual std::uint32_t PlayTime() = 0;
  virtual void SetPlayTime(std::uint32_t play_time) = 0;
  // VS::Save::CaptureAshleySnapshot over the live game state.
  virtual void CaptureSnapshot() = 0;
  virtual void SetMonoOutput(std::int32_t mono) = 0;
  virtual std::uint8_t SaveLocation() = 0;
  // vs_battle_getMapCompletion, stored as the statistics' one byte.
  virtual std::uint8_t MapCompletion() = 0;
  // Menu_Framework_actionRestrictions & 0x400: a container save keeps the
  // container data of a first save.
  virtual bool KeepsContainerData() = 0;
  // The keystream that draws the slot identity, the cipher key and the
  // encoding bytes.
  virtual std::uint32_t NextKeystreamBits(std::int32_t bits) = 0;
  // The save menu's palette and bitmap for file icon `index`, or empty spans
  // when there is no such icon.
  virtual std::span<const std::byte> FileIconPalette(std::int32_t index) = 0;
  virtual std::span<const std::byte> FileIconBitmap(std::int32_t index) = 0;
};

// _loadIntroSaveFile's disc requests for the prologue snapshots.
enum class IntroSnapshot : std::uint8_t { kCourtyard = 1, kDtok = 2 };

// kRejected: there was no load to apply, or its image failed validation and
// nothing was applied. Both end the step, as the original's 1 does.
enum class IntroSnapshotResult : std::uint8_t { kPending, kApplied, kRejected };

class IntroSnapshotHost {
 public:
  virtual ~IntroSnapshotHost() = default;
  // Allocates a zero-filled image and queues the snapshot's disc read.
  virtual void BeginLoad(IntroSnapshot snapshot) = 0;
  // Whether BeginLoad queued a read that has not been taken.
  virtual bool LoadQueued() = 0;
  virtual bool LoadComplete() = 0;
  // The image of a complete load; releases the disc request.
  virtual SaveImage TakeImage() = 0;
  virtual void FreeImage() = 0;
};

// The title screen's card probe.
class TitleCardProbeHost {
 public:
  virtual ~TitleCardProbeHost() = default;
  virtual void BeginResources() = 0;
  virtual bool ResourcesReady() = 0;
  virtual void WaitLoadingFrame() = 0;
  virtual void BeginCardReadiness(std::int32_t port) = 0;
  // Title_SaveCard_AdvanceCardReadiness(0).
  virtual std::int32_t PollCardReadiness() = 0;
  virtual void WaitFrames() = 0;
  virtual bool AnyFileMatches(const CardFilename& pattern) = 0;
  virtual void ReleaseResources() = 0;
};

// _keystreamBits (TITLE.PRG/22C.c:50) state.
inline constexpr std::uint32_t kCipherMultiplier = 0x19660D;
struct CipherState {
  std::uint32_t seed = kCipherMultiplier;
};

// A save file name's number: 1..5, and whether it names the temporary copy.
struct SaveFileNumber {
  std::int32_t number = 0;
  bool temporary = false;
};

[[nodiscard]] CardEvent PollEvents(CardHost& card, CardEventGroup group);
void ClearEvents(CardHost& card, CardEventGroup group);
// `channel` 0 is port 1; any other value is port 2.
[[nodiscard]] CardFilename BuildSaveFilename(std::int32_t channel,
                                             std::int32_t file_number);
[[nodiscard]] CardFilename BuildTemporaryFilename(std::int32_t channel,
                                                  std::int32_t file_number);
// The index of the newest in-use slot, or 0.
[[nodiscard]] std::size_t NewestSaveSlot(ConstSummaryTable summaries);
// The slot whose identity, key, save count and generation match the current
// settings.
[[nodiscard]] std::optional<std::size_t> FindMatchingSaveSlot(
    ConstSummaryTable summaries,
    std::span<const std::byte, kSettingsBytes> settings);
[[nodiscard]] std::optional<SaveFileNumber> ParseSaveFileNumber(
    std::string_view name);
// True when any of the first `block_count` blocks (block 1 excepted) fails
// its checksum, or block 1 does not cancel out. Needs at least two blocks'
// bytes.
[[nodiscard]] std::expected<bool, SaveImageError> HasChecksumError(
    std::span<const std::byte> image, std::size_t block_count);
void DecodeSaveBytes(std::uint32_t key, std::span<std::byte> bytes);
// Reads file `file_number`'s summary into its slot; false (and the slot
// cleared) when no attempt read a valid one.
bool ReadSaveSummary(CardHost& card, SummaryTable summaries,
                     std::int32_t channel, std::int32_t file_number);
[[nodiscard]] bool HasSaveSuffix(const CardDirectory& directory, char suffix);
// Erases the temporary files whose save also exists; the mask of the file
// numbers (bit n-1) it erased.
std::expected<std::uint8_t, CardError> DeleteRedundantTemporaryFiles(
    CardHost& card, const CardDirectory& directory, std::int32_t channel);
std::expected<void, CardError> ScanSaveDirectory(CardHost& card,
                                                 CardDirectory& directory,
                                                 SummaryTable summaries,
                                                 std::int32_t port);
std::expected<void, CardError> CreateSaveFile(CardHost& card, std::int32_t port,
                                              std::int32_t file_number);
// `begin_port` nonzero restarts for that port.
CardReadiness AdvanceCardReadiness(CardReadinessState& state, CardHost& card,
                                   std::int32_t begin_port);
// Decodes and checks `incoming`; when `apply`, copies its game state into the
// host. `apply` checks only the applied blocks.
std::expected<void, SaveImageError> ValidateAndApplyLoadedSave(
    SaveStateHost& state, SaveImage incoming, bool apply);
std::expected<void, SaveImageError> PackageGameSave(SaveStateHost& state,
                                                    SaveWorkspace workspace,
                                                    ConstSummaryTable summaries,
                                                    std::int32_t target_file);
// The title's clear-game save: play time, the misc items and the save counts
// restart.
std::expected<void, SaveImageError> PackageClearSave(
    SaveStateHost& state, SaveWorkspace workspace, ConstSummaryTable summaries,
    std::int32_t target_file);

// A read or write step's card, progress bar and workspace.
struct SaveTransfer {
  CardHost& card;
  TransferProgress& progress;
  // Menu_Files_fileProgressCounter.
  std::int32_t progress_counter = 0;
  SaveWorkspace workspace;
};

// `request` nonzero begins: bits 0-3 the file index, bit 8 a header-only
// read, bits 12 and up the channel.
TransferStatus ReadSaveStep(SaveReadState& state, SaveTransfer& transfer,
                            std::int32_t request);
// `request` nonzero begins: bits 0-2 the file number, bits 12 and up the
// channel. Writes the outgoing image through the temporary file and verifies
// it by reading it back into the incoming image.
TransferStatus WriteSaveStep(SaveWriteState& state, SaveReadState& read_state,
                             SaveTransfer& transfer,
                             const CardDirectory& directory,
                             std::int32_t request);
// Action 1 or 2 queues that snapshot; action 0 applies it once loaded, keeping
// the current play time.
IntroSnapshotResult LoadIntroSnapshotStep(IntroSnapshotHost& intro,
                                          SaveStateHost& state,
                                          std::int32_t action);
[[nodiscard]] std::uint32_t NextCipherBits(CipherState& cipher,
                                           std::int32_t bits);
bool AnySaveFileExists(TitleCardProbeHost& host);
}  // namespace VS::Save::MemoryCard
