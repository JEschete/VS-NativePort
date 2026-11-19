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

#include "Audio/SoundBanksLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscLayout.h"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "DiscQueue.h"
#include "Field/Map.h"
#include "Field/MapLive.hpp"
#include "Field/Mpd.h"
#include "Input/ControllerLiveState.hpp"
#include "Input/Settings.hpp"
#include "Items/InventoryLive.hpp"
#include "Legacy/Psx.h"
#include "Menu/FilesLiveState.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Save/MemoryCard/GameFiles.h"
#include "Save/MemoryCard/GameFiles.hpp"
#include "Save/MemoryCard/Resources.h"
#include "Save/SnapshotLive.hpp"
#include "State/Ashley.hpp"
#include "State/Location.hpp"
#include "State/PlayTime.hpp"
#include "State/Progress.hpp"
#include "State/RoomCache.hpp"
#include "State/RoomMemory.hpp"
#include "State/StateFlags.hpp"
#include "Title/SaveCard.hpp"
#include "Title/SaveFiles.hpp"

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
static_assert(sizeof(VS::Input::Settings) == kSettingsBytes);
static_assert(offsetof(VS::Input::Settings, menu_flags) ==
              kSettingsMenuFlagsOffset);
static_assert(offsetof(VS::Input::Settings, slot_state) ==
              kSettingsSlotStateOffset);
static_assert(offsetof(VS::Input::Settings, mono_sound) ==
              kSettingsMonoSoundOffset);
static_assert(offsetof(VS::Input::Settings, key) == kSettingsKeyOffset);
static_assert(offsetof(VS::Input::Settings, cumulative_save_count) ==
              kSettingsCumulativeSavesOffset);
static_assert(offsetof(VS::Input::Settings, current_game_save_count) ==
              kSettingsCurrentGameSavesOffset);
static_assert(offsetof(VS::Input::Settings, save_file_generation) ==
              kSettingsGenerationOffset);
static_assert(offsetof(VS::State::StateFlags, puzzle_mode) ==
              kStateFlagsPuzzleModeOffset);
static_assert(sizeof(VS::State::RoomEnemyStates) == kRoomEnemyStatesBytes);
static_assert(offsetof(VS::State::RoomEnemyStates, next_slot) ==
              kRoomEnemyNextSlotOffset);
static_assert(std::extent_v<decltype(VS::State::recentRoomEnemyStates)> ==
              kRoomStateSlotCount);
static_assert(sizeof(Save_CardUiResources::fileIconPalettes) ==
              kFileIconCount * kIconPaletteBytes);
static_assert(sizeof(Save_CardUiResources::fileIconBitmaps) ==
              kFileIconCount * kIconBitmapBytes);
static_assert(sizeof(DIRENTRY::name) == kDirectoryNameBytes);
static_assert(std::extent_v<decltype(VS::Title::cardDirectoryEntries)> ==
              kDirectoryCapacity);
static_assert(std::extent_v<decltype(VS::Title::cardEventDescriptors)> ==
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
                      sizeof(VS::State::stateFlags)));
static_assert(LaysOut(SaveSection::kLearnedActions,
                      offsetof(Save_GameSaveImage, learnedActions),
                      sizeof(VS::State::learnedActionFlags)));
static_assert(LaysOut(SaveSection::kMapStatus,
                      offsetof(Save_GameSaveImage, mapStatus),
                      sizeof(VS::State::roomVisitProgress)));
static_assert(LaysOut(SaveSection::kSettings,
                      offsetof(Save_GameSaveImage, settings),
                      sizeof(VS::Input::settings)));
static_assert(LaysOut(SaveSection::kAshleySnapshot,
                      offsetof(Save_GameSaveImage, ashleySnapshot),
                      sizeof(VS::State::ashleySnapshot)));
static_assert(LaysOut(SaveSection::kInventory,
                      offsetof(Save_GameSaveImage, inventory),
                      sizeof(VS::Items::inventory)));
static_assert(LaysOut(SaveSection::kInventoryIndices,
                      offsetof(Save_GameSaveImage, inventoryIndices),
                      sizeof(VS::Items::inventoryIndices)));
static_assert(LaysOut(SaveSection::kEntryDoor,
                      offsetof(Save_GameSaveImage, entryDoor),
                      sizeof(VS::State::savedEntryDoor)));
static_assert(LaysOut(SaveSection::kScoreData,
                      offsetof(Save_GameSaveImage, scoreData),
                      sizeof(VS::State::scoreData)));
static_assert(LaysOut(SaveSection::kRoomStateSlot,
                      offsetof(Save_GameSaveImage, roomStateSlot),
                      sizeof(VS::State::currentRoomStateSlot)));
static_assert(LaysOut(SaveSection::kRecentRoomEnemyStates,
                      offsetof(Save_GameSaveImage, recentRoomEnemyStates),
                      sizeof(VS::State::recentRoomEnemyStates)));
static_assert(LaysOut(SaveSection::kArtsStatus,
                      offsetof(Save_GameSaveImage, artsStatus),
                      sizeof(VS::State::artsStatus)));

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
  if (VS::Title::memoryCardUiResource == nullptr) {
    VS::Core::Errorf(kDiagnosticChannel, "Memory-card workspace is not loaded");
    return std::nullopt;
  }
  const std::span<std::byte, kWorkspaceBytes> bytes{
      reinterpret_cast<std::byte*>(VS::Title::memoryCardUiResource),
      kWorkspaceBytes};
  return SaveWorkspace{.outgoing = bytes.first<kSaveImageBytes>(),
                       .incoming = bytes.last<kSaveImageBytes>()};
}

std::optional<SummaryTable> LiveSummaries() {
  if (VS::Title::saveFileSummaries == nullptr) {
    VS::Core::Errorf(kDiagnosticChannel,
                     "Memory-card slot summaries are not loaded");
    return std::nullopt;
  }
  return std::as_writable_bytes(std::span<Save_CardFileInfoRecord, kSlotCount>{
      VS::Title::saveFileSummaries, kSlotCount});
}

std::span<const std::byte, kSettingsBytes> LiveSettings() {
  return std::as_bytes(
      std::span<const VS::Input::Settings, 1>{&VS::Input::settings, 1});
}

CardDirectoryEntry EntryOf(const DIRENTRY& entry) {
  CardDirectoryEntry copy;
  std::ranges::copy(entry.name, copy.name.begin());
  copy.size = static_cast<std::int32_t>(entry.size);
  return copy;
}

// The directory the last scan published in VS::Title::cardDirectoryEntries.
CardDirectory PublishedDirectory() {
  CardDirectory directory;
  for (std::size_t index = 0; index < kDirectoryCapacity; ++index) {
    if (const DIRENTRY* entry = VS::Title::cardDirectoryEntries[index];
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
    if (index >= std::size(VS::Title::cardEventDescriptors)) {
      return false;
    }
    return ::TestEvent(VS::Title::cardEventDescriptors[index]) ==
           kTestEventFired;
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
  // VS::Title::cardDirectoryEntries keeps what the calls returned, as
  // _memcardFiles does.
  static std::optional<CardDirectoryEntry> Publish(std::size_t index,
                                                   DIRENTRY* found) {
    VS::Title::cardDirectoryEntries[index] = found;
    if (found == nullptr) {
      return std::nullopt;
    }
    return EntryOf(*found);
  }
};

enum class LiveKeystream : std::uint8_t { kInterface, kTitleCipher };

// _keystreamBits' seed. No native TITLE.PRG reload resets it (OverlayReset.cpp
// has no TITLE entry), so it runs on from start-up.
CipherState title_cipher;

class LiveSaveState final : public SaveStateHost {
 public:
  explicit LiveSaveState(LiveKeystream keystream) : keystream_(keystream) {}

  std::span<std::byte> Section(SaveSection section) override {
    switch (section) {
      case SaveSection::kStateFlags:
        return BytesOf(VS::State::stateFlags);
      case SaveSection::kLearnedActions:
        return std::as_writable_bytes(std::span{VS::State::learnedActionFlags});
      case SaveSection::kMapStatus:
        return BytesOf(VS::State::roomVisitProgress);
      case SaveSection::kSettings:
        return BytesOf(VS::Input::settings);
      case SaveSection::kAshleySnapshot:
        return BytesOf(VS::State::ashleySnapshot);
      case SaveSection::kInventory:
        return BytesOf(VS::Items::inventory);
      case SaveSection::kInventoryIndices:
        return BytesOf(VS::Items::inventoryIndices);
      case SaveSection::kEntryDoor:
        return BytesOf(VS::State::savedEntryDoor);
      case SaveSection::kScoreData:
        return BytesOf(VS::State::scoreData);
      case SaveSection::kRoomStateSlot:
        return BytesOf(VS::State::currentRoomStateSlot);
      case SaveSection::kRecentRoomEnemyStates:
        return std::as_writable_bytes(
            std::span{VS::State::recentRoomEnemyStates});
      case SaveSection::kArtsStatus:
        return BytesOf(VS::State::artsStatus);
    }
    return {};
  }
  std::uint32_t PlayTime() override {
    return VS::State::PackPlayTime(VS::State::playTime);
  }
  void SetPlayTime(std::uint32_t play_time) override {
    VS::State::playTime = VS::State::UnpackPlayTime(play_time);
  }
  void CaptureSnapshot() override { VS::Save::CaptureLiveAshleySnapshot(); }
  void SetMonoOutput(std::int32_t mono) override {
    VS::Audio::LiveSetMonoOutput(mono);
  }
  std::uint8_t SaveLocation() override {
    return static_cast<std::uint8_t>(VS::Menu::saveLocationId);
  }
  std::uint8_t MapCompletion() override {
    return static_cast<std::uint8_t>(VS::Field::LiveMapCompletionPercent());
  }
  bool KeepsContainerData() override {
    return (VS::Menu::Framework::actionRestrictions &
            kKeepContainerRestriction) != 0;
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
      VS::Core::Errorf(kDiagnosticChannel,
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
    request_ = VS::Content::AllocateLiveDiscRequest(&file);
    if (request_ == nullptr) {
      VS::Core::Errorf(kDiagnosticChannel,
                       "No disc request for intro snapshot %d",
                       static_cast<int>(snapshot));
      VS::Core::Memory::FreeLiveFromHighEnd(allocation);
      return;
    }
    image_ = static_cast<std::byte*>(allocation);
    std::ranges::fill(Image(), std::byte{0});
    VS::Content::EnqueueLiveDiscRequest(request_, image_);
  }
  bool LoadQueued() override { return request_ != nullptr; }
  bool LoadComplete() override {
    return request_ != nullptr &&
           request_->state == Content_DiscRequestState_Loaded;
  }
  SaveImage TakeImage() override {
    VS::Content::ReleaseLiveDiscRequest(request_);
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
CardReadinessState live_title_readiness;
SaveReadState live_read;
SaveWriteState live_write;
LiveIntroHost live_intro;
CardFilename live_save_filename{};
CardFilename live_temporary_filename{};

TransferProgress LoadProgress() {
  return TransferProgress{.position = VS::Title::transferProgressPosition,
                          .previous = VS::Title::previousTransferProgress,
                          .target = VS::Title::transferProgressTarget,
                          .error_offset = static_cast<std::int8_t>(
                              VS::Title::saveTransferErrorOffset)};
}

void StoreProgress(const TransferProgress& progress) {
  VS::Title::transferProgressPosition = progress.position;
  VS::Title::previousTransferProgress = progress.previous;
  VS::Title::transferProgressTarget = progress.target;
  VS::Title::saveTransferErrorOffset =
      static_cast<std::uint8_t>(progress.error_offset);
}

void ReportPackageError(const std::expected<void, SaveImageError>& packaged) {
  if (!packaged.has_value()) {
    VS::Core::Errorf(kDiagnosticChannel, "Save not packaged: %s",
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
  std::ranges::fill(VS::Title::cardDirectoryEntries, nullptr);
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

int AdvanceLiveTitleCardReadiness(int begin_port) {
  return static_cast<int>(
      AdvanceCardReadiness(live_title_readiness, live_card, begin_port));
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
  SaveTransfer transfer{
      .card = live_card,
      .progress = progress,
      .progress_counter = VS::Menu::Files::fileProgressCounter,
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
  SaveTransfer transfer{
      .card = live_card,
      .progress = progress,
      .progress_counter = VS::Menu::Files::fileProgressCounter,
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
