// Hand-maintained (tools/port/owned.csv).
// Live binding for the memory-card save cores (GameFiles.cpp): the libapi
// card calls and libcard events, the card globals both save screens share,
// the low-end save workspace, the prologue snapshot disc reads and the game
// state a save carries.
#include "Save/MemoryCard/GameFilesLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>

#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Save/MemoryCard/GameFiles.hpp"
#include "Save/SnapshotLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Audio/SoundBanks.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Diagnostics.h"
#include "DiscQueue.h"
#include "Field/Map.h"
#include "Field/Mpd.h"
#include "Input/ControllerLiveState.h"
#include "Items/Inventory.h"
#include "Legacy/Psx.h"
#include "Menu/Files.h"
#include "Menu/Framework/Framework.h"
#include "Save/MemoryCard/GameFiles.h"
#include "Save/MemoryCard/Resources.h"
#include "State/Ashley.h"
#include "State/Location.h"
#include "State/PlayTime.h"
#include "State/Progress.h"
#include "State/RoomCache.h"
#include "State/RoomMemory.h"
#include "State/Settings.h"
#include "State/StateFlags.h"
#include "Title/SaveCard.h"
#include "Title/SaveFiles.h"
}

namespace VS::Save::MemoryCard {
namespace {
constexpr const char* kDiagnosticChannel = "save.card";
constexpr std::int32_t kTestEventFired = 1;
constexpr std::int32_t kCardCallFailed = -1;
// Four events for each of the software and hardware groups.
constexpr std::size_t kEventsPerGroup = 4;
constexpr std::size_t kEventDescriptorCount = 2 * kEventsPerGroup;
constexpr std::int32_t kFileIconCount = 7;
constexpr std::size_t kMinimumChecksumBlocks = 2;
constexpr int kEraseFailedMask = 0x80;
constexpr int kKeepContainerRestriction = 0x400;
constexpr int kLiveFailure = 1;
constexpr int kCreateFailed = -1;
constexpr std::size_t kWorkspaceBytes = 2 * kSaveImageBytes;

// The save image (savedata_t) and the record and settings layouts the cores
// address as bytes.
static_assert(sizeof(Save_GameSaveImage) == kSaveImageBytes);
static_assert(sizeof(Save_CardFileInfoRecord) == kRecordBytes);
static_assert(offsetof(Save_GameSaveImage, cardHeader) == kCardMagicOffset);
static_assert(offsetof(Save_GameSaveImage, iconBitmap) == kIconBitmapOffset);
static_assert(offsetof(Save_GameSaveImage, protectedHeader) == kSummaryOffset);
static_assert(offsetof(Save_GameSaveImage, protectedHeader.blockChecksums) ==
              kChecksumOffset);
static_assert(offsetof(Save_GameSaveImage, containerData) == kContainerOffset);
static_assert(sizeof(Save_GameSaveImage::containerData) == kContainerBytes);
static_assert(offsetof(Save_CardFileInfoRecord, iconHeaderBytes) ==
              kIconPaletteOffset);
static_assert(offsetof(Save_CardFileInfoRecord, summary.cardTitleTail) ==
              kCardTitleTailOffset);
static_assert(offsetof(Save_SaveProtectedHeader, summary) ==
              kSummaryStatsOffset);
static_assert(sizeof(Save_SaveFileStats) == kStatsBytes);
static_assert(offsetof(Save_SaveFileStats, mapCompletion) ==
              kStatsMapCompletionOffset);
static_assert(sizeof(State_Settings) == kSettingsBytes);
static_assert(offsetof(State_Settings, slotState) == kSettingsSlotStateOffset);
static_assert(offsetof(State_Settings, monoSound) == kSettingsMonoSoundOffset);
static_assert(offsetof(State_Settings, key) == kSettingsKeyOffset);
static_assert(offsetof(State_Settings, saveFileGeneration) ==
              kSettingsGenerationOffset);
static_assert(offsetof(State_StateFlags, puzzleMode) ==
              kStateFlagsPuzzleModeOffset);
static_assert(sizeof(State_RoomEnemyStates) == kRoomEnemyStatesBytes);
static_assert(offsetof(State_RoomEnemyStates, nextSlot) ==
              kRoomEnemyNextSlotOffset);
static_assert(std::extent_v<decltype(State_recentRoomEnemyStates)> ==
              kRoomStateSlotCount);
static_assert(sizeof(Save_CardUiResources::fileIconPalettes) ==
              kFileIconCount * kIconPaletteBytes);
static_assert(sizeof(Save_CardUiResources::fileIconBitmaps) ==
              kFileIconCount * kIconBitmapBytes);
static_assert(sizeof(DIRENTRY::name) == kDirectoryNameBytes);
static_assert(std::extent_v<decltype(Title_cardDirectoryEntries)> ==
              kDirectoryCapacity);
static_assert(std::extent_v<decltype(Title_cardEventDescriptors)> ==
              kEventDescriptorCount);
// The workspace is the first two images of the save-screen image buffer.
static_assert(kWorkspaceBytes <= VS_SPMCIMG_BIN_SIZE);
static_assert(VS_OPMCIMG1_BIN_SIZE == VS_OPMCIMG2_BIN_SIZE);
static_assert(VS_OPMCIMG1_BIN_SIZE <= kSaveImageBytes);

constexpr bool LaysOut(SaveSection section, std::size_t offset,
                       std::size_t size) {
  return LayoutOf(section).offset == offset && LayoutOf(section).size == size;
}
static_assert(LaysOut(SaveSection::kStateFlags,
                      offsetof(Save_GameSaveImage, stateFlags),
                      sizeof(Audio_Seq_unusedStateFlags)));
static_assert(LaysOut(SaveSection::kLearnedActions,
                      offsetof(Save_GameSaveImage, learnedActions),
                      sizeof(State_learnedActionFlags)));
static_assert(LaysOut(SaveSection::kMapStatus,
                      offsetof(Save_GameSaveImage, mapStatus),
                      sizeof(State_roomVisitProgress)));
static_assert(LaysOut(SaveSection::kSettings,
                      offsetof(Save_GameSaveImage, settings),
                      sizeof(Input_settings)));
static_assert(LaysOut(SaveSection::kAshleySnapshot,
                      offsetof(Save_GameSaveImage, ashleySnapshot),
                      sizeof(State_ashleySnapshot)));
static_assert(LaysOut(SaveSection::kInventory,
                      offsetof(Save_GameSaveImage, inventory),
                      sizeof(Audio_Seq_unusedInventory)));
static_assert(LaysOut(SaveSection::kInventoryIndices,
                      offsetof(Save_GameSaveImage, inventoryIndices),
                      sizeof(Audio_Seq_unusedInventoryIndices)));
static_assert(LaysOut(SaveSection::kEntryDoor,
                      offsetof(Save_GameSaveImage, entryDoor),
                      sizeof(State_savedEntryDoor)));
static_assert(LaysOut(SaveSection::kScoreData,
                      offsetof(Save_GameSaveImage, scoreData),
                      sizeof(Audio_Seq_unusedScoredata)));
static_assert(LaysOut(SaveSection::kRoomStateSlot,
                      offsetof(Save_GameSaveImage, roomStateSlot),
                      sizeof(State_currentRoomStateSlot)));
static_assert(LaysOut(SaveSection::kRecentRoomEnemyStates,
                      offsetof(Save_GameSaveImage, recentRoomEnemyStates),
                      sizeof(State_recentRoomEnemyStates)));
static_assert(LaysOut(SaveSection::kArtsStatus,
                      offsetof(Save_GameSaveImage, artsStatus),
                      sizeof(Audio_Seq_unusedArtsStatus)));

template <typename T>
std::span<std::byte> BytesOf(T& value) {
  return std::as_writable_bytes(std::span<T, 1>{&value, 1});
}

const char* SaveErrorName(SaveImageError error) {
  switch (error) {
    case SaveImageError::kBlockCountOutOfRange:
      return "checksum block count out of range";
    case SaveImageError::kChecksumMismatch:
      return "checksum mismatch";
    case SaveImageError::kSignatureMismatch:
      return "signature mismatch";
    case SaveImageError::kRoomStateSlotOutOfRange:
      return "recent-room slot out of range";
    case SaveImageError::kRoomEnemySlotOutOfRange:
      return "recent-room actor slot out of range";
    case SaveImageError::kSectionSizeMismatch:
      return "section size mismatch";
    case SaveImageError::kFileIconOutOfRange:
      return "file icon out of range";
  }
  return "unknown";
}

// The image being written and the image last read, at the start of the
// save-screen image buffer.
std::optional<SaveWorkspace> LiveWorkspace() {
  if (Title_memoryCardUiResource == nullptr) {
    Core_Errorf(kDiagnosticChannel, "Memory-card workspace is not loaded");
    return std::nullopt;
  }
  const std::span<std::byte, kWorkspaceBytes> bytes{
      reinterpret_cast<std::byte*>(Title_memoryCardUiResource),
      kWorkspaceBytes};
  return SaveWorkspace{.outgoing = bytes.first<kSaveImageBytes>(),
                       .incoming = bytes.last<kSaveImageBytes>()};
}

std::optional<SummaryTable> LiveSummaries() {
  if (Title_saveFileSummaries == nullptr) {
    Core_Errorf(kDiagnosticChannel,
                "Memory-card slot summaries are not loaded");
    return std::nullopt;
  }
  return std::as_writable_bytes(std::span<Save_CardFileInfoRecord, kSlotCount>{
      Title_saveFileSummaries, kSlotCount});
}

std::span<const std::byte, kSettingsBytes> LiveSettings() {
  return std::as_bytes(std::span<const State_Settings, 1>{&Input_settings, 1});
}

CardDirectoryEntry EntryOf(const DIRENTRY& entry) {
  CardDirectoryEntry copy;
  std::ranges::copy(entry.name, copy.name.begin());
  copy.size = static_cast<std::int32_t>(entry.size);
  return copy;
}

// The directory the last scan published in Title_cardDirectoryEntries.
CardDirectory PublishedDirectory() {
  CardDirectory directory;
  for (std::size_t index = 0; index < kDirectoryCapacity; ++index) {
    if (const DIRENTRY* entry = Title_cardDirectoryEntries[index];
        entry != nullptr) {
      directory.at(index) = EntryOf(*entry);
    }
  }
  return directory;
}

// _initSaveFileInfo's firstfile/nextfile targets. The original lists into
// fifteen entries from _dirEntBuf, which holds one; these hold all fifteen.
std::array<DIRENTRY, kDirectoryCapacity> live_directory_entries{};

class LiveCardHost final : public CardHost {
 public:
  bool TestEvent(CardEventGroup group, CardEvent event) override {
    const auto index =
        static_cast<std::size_t>(group) + static_cast<std::size_t>(event);
    if (index >= std::size(Title_cardEventDescriptors)) {
      return false;
    }
    return ::TestEvent(Title_cardEventDescriptors[index]) == kTestEventFired;
  }
  bool StartCardInfo(std::int32_t channel) override {
    return _card_info(channel) != 0;
  }
  bool StartCardClear(std::int32_t channel) override {
    return _card_clear(channel) != 0;
  }
  bool StartCardLoad(std::int32_t channel) override {
    return _card_load(channel) != 0;
  }
  std::optional<std::int32_t> Open(const CardFilename& path,
                                   std::uint32_t flags) override {
    CardFilename name = path;
    const auto file = LegacyApi_open(name.data(), flags);
    if (file == kCardCallFailed) {
      return std::nullopt;
    }
    return static_cast<std::int32_t>(file);
  }
  std::optional<std::int32_t> Read(std::int32_t file,
                                   std::span<std::byte> bytes) override {
    const auto count = LegacyApi_read(file, bytes.data(),
                                      static_cast<std::int32_t>(bytes.size()));
    if (count == kCardCallFailed) {
      return std::nullopt;
    }
    return static_cast<std::int32_t>(count);
  }
  bool Write(std::int32_t file, std::span<const std::byte> bytes) override {
    // write() only reads the buffer; its libapi signature is not const.
    return LegacyApi_write(file, const_cast<std::byte*>(bytes.data()),
                           static_cast<std::int32_t>(bytes.size())) !=
           kCardCallFailed;
  }
  void Close(std::int32_t file) override { LegacyApi_close(file); }
  bool Erase(const CardFilename& path) override {
    CardFilename name = path;
    return LegacyApi_erase(name.data()) != 0;
  }
  bool Rename(const CardFilename& from, const CardFilename& to) override {
    CardFilename from_name = from;
    CardFilename to_name = to;
    return LegacyApi_rename(from_name.data(), to_name.data()) != 0;
  }
  std::optional<CardDirectoryEntry> FirstFile(
      const CardFilename& pattern) override {
    CardFilename name = pattern;
    return Publish(
        0, LegacyApi_firstfile(name.data(), &live_directory_entries.front()));
  }
  std::optional<CardDirectoryEntry> NextFile(std::size_t index) override {
    if (index >= kDirectoryCapacity) {
      return std::nullopt;
    }
    return Publish(index,
                   LegacyApi_nextfile(&live_directory_entries.at(index)));
  }

 private:
  // Title_cardDirectoryEntries keeps what the calls returned, as
  // _memcardFiles does.
  static std::optional<CardDirectoryEntry> Publish(std::size_t index,
                                                   DIRENTRY* found) {
    Title_cardDirectoryEntries[index] = found;
    if (found == nullptr) {
      return std::nullopt;
    }
    return EntryOf(*found);
  }
};

enum class LiveKeystream : std::uint8_t { kInterface, kTitleCipher };

// _keystreamBits' seed. Boot_ResetTitleStatics, which reset it, has no
// callers, so it runs on from start-up.
CipherState title_cipher;

class LiveSaveState final : public SaveStateHost {
 public:
  explicit LiveSaveState(LiveKeystream keystream) : keystream_(keystream) {}

  std::span<std::byte> Section(SaveSection section) override {
    switch (section) {
      case SaveSection::kStateFlags:
        return BytesOf(Audio_Seq_unusedStateFlags);
      case SaveSection::kLearnedActions:
        return std::as_writable_bytes(std::span{State_learnedActionFlags});
      case SaveSection::kMapStatus:
        return BytesOf(State_roomVisitProgress);
      case SaveSection::kSettings:
        return BytesOf(Input_settings);
      case SaveSection::kAshleySnapshot:
        return BytesOf(State_ashleySnapshot);
      case SaveSection::kInventory:
        return BytesOf(Audio_Seq_unusedInventory);
      case SaveSection::kInventoryIndices:
        return BytesOf(Audio_Seq_unusedInventoryIndices);
      case SaveSection::kEntryDoor:
        return BytesOf(State_savedEntryDoor);
      case SaveSection::kScoreData:
        return BytesOf(Audio_Seq_unusedScoredata);
      case SaveSection::kRoomStateSlot:
        return BytesOf(State_currentRoomStateSlot);
      case SaveSection::kRecentRoomEnemyStates:
        return std::as_writable_bytes(std::span{State_recentRoomEnemyStates});
      case SaveSection::kArtsStatus:
        return BytesOf(Audio_Seq_unusedArtsStatus);
    }
    return {};
  }
  std::uint32_t PlayTime() override { return State_playTime.all; }
  void SetPlayTime(std::uint32_t play_time) override {
    State_playTime.all = play_time;
  }
  void CaptureSnapshot() override { VS::Save::CaptureLiveAshleySnapshot(); }
  void SetMonoOutput(std::int32_t mono) override { Audio_SetMonoOutput(mono); }
  std::uint8_t SaveLocation() override {
    return static_cast<std::uint8_t>(Menu_saveLocationId);
  }
  std::uint8_t MapCompletion() override {
    return static_cast<std::uint8_t>(Field_MapCompletionPercent());
  }
  bool KeepsContainerData() override {
    return (Menu_Framework_actionRestrictions & kKeepContainerRestriction) != 0;
  }
  std::uint32_t NextKeystreamBits(std::int32_t bits) override {
    if (keystream_ == LiveKeystream::kTitleCipher) {
      return NextCipherBits(title_cipher, bits);
    }
    return VS::Core::NextLiveInterfaceRandomBits(bits);
  }
  std::span<const std::byte> FileIconPalette(std::int32_t index) override {
    if (!HasIcon(index)) {
      return {};
    }
    return std::as_bytes(
        std::span{Save_MemoryCard_uiResources->fileIconPalettes}.subspan(
            static_cast<std::size_t>(index), 1));
  }
  std::span<const std::byte> FileIconBitmap(std::int32_t index) override {
    if (!HasIcon(index)) {
      return {};
    }
    return std::as_bytes(
        std::span{Save_MemoryCard_uiResources->fileIconBitmaps}.subspan(
            static_cast<std::size_t>(index), 1));
  }

 private:
  static bool HasIcon(std::int32_t index) {
    return Save_MemoryCard_uiResources != nullptr && index >= 0 &&
           index < kFileIconCount;
  }

  LiveKeystream keystream_;
};

// _loadIntroSaveFile's disc request and high-end image.
class LiveIntroHost final : public IntroSnapshotHost {
 public:
  void BeginLoad(IntroSnapshot snapshot) override {
    if (request_ != nullptr || image_ != nullptr) {
      Core_Errorf(kDiagnosticChannel,
                  "Intro snapshot requested while one is loading");
      return;
    }
    void* allocation =
        VS::Core::Memory::AllocateLiveFromHighEnd(kSaveImageBytes);
    if (allocation == nullptr) {
      return;
    }
    Content_DiscFile file{};
    file.lba = snapshot == IntroSnapshot::kCourtyard ? VS_OPMCIMG1_BIN_LBA
                                                     : VS_OPMCIMG2_BIN_LBA;
    file.size = VS_OPMCIMG1_BIN_SIZE;
    request_ = Content_AllocateDiscRequest(&file);
    if (request_ == nullptr) {
      Core_Errorf(kDiagnosticChannel, "No disc request for intro snapshot %d",
                  static_cast<int>(snapshot));
      VS::Core::Memory::FreeLiveFromHighEnd(allocation);
      return;
    }
    image_ = static_cast<std::byte*>(allocation);
    std::ranges::fill(Image(), std::byte{0});
    Content_EnqueueDiscRequest(request_, image_);
  }
  bool LoadQueued() override { return request_ != nullptr; }
  bool LoadComplete() override {
    return request_ != nullptr &&
           request_->state == Content_DiscRequestState_Loaded;
  }
  SaveImage TakeImage() override {
    Content_ReleaseDiscRequest(request_);
    request_ = nullptr;
    return Image();
  }
  void FreeImage() override {
    VS::Core::Memory::FreeLiveFromHighEnd(image_);
    image_ = nullptr;
  }

 private:
  [[nodiscard]] SaveImage Image() const {
    return SaveImage{image_, kSaveImageBytes};
  }

  Content_DiscQueueSlot* request_ = nullptr;
  std::byte* image_ = nullptr;
};

LiveCardHost live_card;
CardReadinessState live_readiness;
SaveReadState live_read;
SaveWriteState live_write;
LiveIntroHost live_intro;
CardFilename live_save_filename{};
CardFilename live_temporary_filename{};

TransferProgress LoadProgress() {
  return TransferProgress{
      .position = Title_transferProgressPosition,
      .previous = Title_previousTransferProgress,
      .target = Title_transferProgressTarget,
      .error_offset = static_cast<std::int8_t>(Title_saveTransferErrorOffset)};
}

void StoreProgress(const TransferProgress& progress) {
  Title_transferProgressPosition = progress.position;
  Title_previousTransferProgress = progress.previous;
  Title_transferProgressTarget = progress.target;
  Title_saveTransferErrorOffset = static_cast<char>(progress.error_offset);
}

void ReportPackageError(const std::expected<void, SaveImageError>& packaged) {
  if (!packaged.has_value()) {
    Core_Errorf(kDiagnosticChannel, "Save not packaged: %s",
                SaveErrorName(packaged.error()));
  }
}
}  // namespace

char* BuildLiveSaveFilename(int channel, int file_number) {
  live_save_filename = BuildSaveFilename(channel, file_number);
  return live_save_filename.data();
}

char* BuildLiveTemporaryFilename(int channel, int file_number) {
  live_temporary_filename = BuildTemporaryFilename(channel, file_number);
  return live_temporary_filename.data();
}

int NewestLiveSaveSlot() {
  const auto summaries = LiveSummaries();
  if (!summaries.has_value()) {
    return 0;
  }
  return static_cast<int>(NewestSaveSlot(*summaries));
}

int FindLiveMatchingSaveSlot() {
  const auto summaries = LiveSummaries();
  if (!summaries.has_value()) {
    return 0;
  }
  const auto slot = FindMatchingSaveSlot(*summaries, LiveSettings());
  return slot.has_value() ? static_cast<int>(*slot) + 1 : 0;
}

int ParseLiveSaveFileNumber(const char* name) {
  if (name == nullptr) {
    return 0;
  }
  const std::span<const char, kDirectoryNameBytes> bytes{name,
                                                         kDirectoryNameBytes};
  const auto end = std::ranges::find(bytes, '\0');
  const auto number = ParseSaveFileNumber(std::string_view{bytes.begin(), end});
  if (!number.has_value()) {
    return 0;
  }
  return number->temporary ? -number->number : number->number;
}

int HasLiveChecksumError(const Save_GameSaveImage* image, int block_count) {
  if (image == nullptr || block_count < 0 ||
      block_count > static_cast<int>(kImageBlockCount)) {
    return kLiveFailure;
  }
  const auto blocks = static_cast<std::size_t>(block_count);
  const std::span<const std::byte> bytes{
      reinterpret_cast<const std::byte*>(image),
      std::max(blocks, kMinimumChecksumBlocks) * kBlockBytes};
  const auto error = HasChecksumError(bytes, blocks);
  return !error.has_value() || *error ? kLiveFailure : 0;
}

void DecodeLiveSaveBytes(std::uint32_t key, void* bytes, std::size_t count) {
  if (bytes == nullptr) {
    return;
  }
  DecodeSaveBytes(key, std::span{static_cast<std::byte*>(bytes), count});
}

int DeleteLiveRedundantTemporaryFiles(int channel) {
  const auto erased =
      DeleteRedundantTemporaryFiles(live_card, PublishedDirectory(), channel);
  return erased.has_value() ? *erased : kEraseFailedMask;
}

int ScanLiveSaveDirectory(int port) {
  std::ranges::fill(Title_cardDirectoryEntries, nullptr);
  const auto summaries = LiveSummaries();
  if (!summaries.has_value()) {
    return kLiveFailure;
  }
  CardDirectory directory;
  return ScanSaveDirectory(live_card, directory, *summaries, port).has_value()
             ? 0
             : kLiveFailure;
}

int CreateLiveSaveFile(int port, int file_number) {
  return CreateSaveFile(live_card, port, file_number).has_value()
             ? 0
             : kCreateFailed;
}

int AdvanceLiveCardReadiness(int begin_port) {
  return static_cast<int>(
      AdvanceCardReadiness(live_readiness, live_card, begin_port));
}

int ValidateAndApplyLiveLoadedSave(int apply) {
  const auto workspace = LiveWorkspace();
  if (!workspace.has_value()) {
    return kLiveFailure;
  }
  LiveSaveState state{LiveKeystream::kInterface};
  return ValidateAndApplyLoadedSave(state, workspace->incoming, apply != 0)
                 .has_value()
             ? 0
             : kLiveFailure;
}

void PackageLiveGameSave(int target_file) {
  const auto workspace = LiveWorkspace();
  const auto summaries = LiveSummaries();
  if (!workspace.has_value() || !summaries.has_value()) {
    return;
  }
  LiveSaveState state{LiveKeystream::kInterface};
  ReportPackageError(
      PackageGameSave(state, *workspace, *summaries, target_file));
}

int ReadLiveSaveStep(int request) {
  const auto workspace = LiveWorkspace();
  if (!workspace.has_value()) {
    return static_cast<int>(TransferStatus::kFailed);
  }
  TransferProgress progress = LoadProgress();
  SaveTransfer transfer{.card = live_card,
                        .progress = progress,
                        .progress_counter = Menu_Files_fileProgressCounter,
                        .workspace = *workspace};
  const TransferStatus status = ReadSaveStep(live_read, transfer, request);
  StoreProgress(progress);
  return static_cast<int>(status);
}

int WriteLiveSaveStep(int request) {
  const auto workspace = LiveWorkspace();
  if (!workspace.has_value()) {
    return static_cast<int>(TransferStatus::kFailed);
  }
  TransferProgress progress = LoadProgress();
  SaveTransfer transfer{.card = live_card,
                        .progress = progress,
                        .progress_counter = Menu_Files_fileProgressCounter,
                        .workspace = *workspace};
  const TransferStatus status = WriteSaveStep(live_write, live_read, transfer,
                                              PublishedDirectory(), request);
  StoreProgress(progress);
  return static_cast<int>(status);
}

int LoadLiveIntroSnapshotStep(int action) {
  LiveSaveState state{LiveKeystream::kInterface};
  return LoadIntroSnapshotStep(live_intro, state, action) ==
                 IntroSnapshotResult::kPending
             ? 0
             : 1;
}

std::uint32_t NextLiveTitleCipherBits(int bits) {
  return NextCipherBits(title_cipher, bits);
}

void PackageLiveClearSave(int target_file) {
  const auto workspace = LiveWorkspace();
  const auto summaries = LiveSummaries();
  if (!workspace.has_value() || !summaries.has_value()) {
    return;
  }
  LiveSaveState state{LiveKeystream::kTitleCipher};
  ReportPackageError(
      PackageClearSave(state, *workspace, *summaries, target_file));
}
}  // namespace VS::Save::MemoryCard
