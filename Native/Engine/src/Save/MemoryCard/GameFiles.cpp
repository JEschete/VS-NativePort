// Hand-maintained (tools/port/owned.csv).
#include "Save/MemoryCard/GameFiles.hpp"  // NOLINT(build/include): native C++ interface.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

#include "Core/Math/ScalarMath.hpp"
#include "Save/Snapshot.hpp"

namespace VS::Save::MemoryCard {
namespace {

using VS::Core::Math::R3000Quotient;
using VS::Core::Math::WrappingAdd;
using VS::Core::Math::WrappingMultiply;
using VS::Core::Math::WrappingSubtract;

constexpr unsigned kBitsPerByte = 8;
constexpr std::uint32_t kByteMask = 0xFF;
constexpr std::size_t kWordBytes = sizeof(std::uint32_t);
constexpr std::size_t kHalfBytes = sizeof(std::uint16_t);

// Save_CardFileSlotStatus below the in-use states.
constexpr std::uint32_t kSlotUnavailable = 0;
constexpr std::uint32_t kSlotAvailable = 1;
constexpr std::uint32_t kSlotTemporary = 2;

// The card filename template and the characters the builders replace.
constexpr std::string_view kFilenameTemplate = "bu00:BASLUS-01040VAG0";
constexpr std::size_t kFilenamePortIndex = 2;
constexpr std::size_t kFilenameNumberIndex = 20;
constexpr std::size_t kGameCodeOffset = 5;
// "BASLUS-01040VAG": a directory name before its file number.
constexpr std::string_view kGameCode = kFilenameTemplate.substr(
    kGameCodeOffset, kFilenameNumberIndex - kGameCodeOffset);
constexpr std::size_t kNameNumberIndex = kGameCode.size();
constexpr std::uint32_t kDigitZero = '0';
constexpr std::uint32_t kTemporaryBase = 'A' - 1;
constexpr std::uint32_t kSaveSuffixBase = '1';
constexpr std::uint32_t kTemporarySuffixBase = 'A';
constexpr char kFirstTemporarySuffix = 'A';
constexpr char kLastTemporarySuffix = 'E';
constexpr char kFirstSaveSuffix = '1';
constexpr char kLastSaveSuffix = '5';
constexpr std::string_view kPortOnePattern = "bu00:*";
constexpr std::string_view kPortTwoPattern = "bu10:*";
// _saveFileExists' probe: "bu?0:BASLUS-01040VAG?" with '/' + port.
constexpr std::uint32_t kProbePortBase = '/';
constexpr char kAnyFileNumber = '?';
constexpr std::int32_t kChannelsPerPort = 16;
constexpr std::int32_t kPortCount = 2;

constexpr std::size_t kChecksumBlock = 1;
constexpr std::size_t kMinimumChecksumBlocks = 2;
constexpr std::size_t kHeaderBlockCount = kHeaderBytes / kBlockBytes;
constexpr int kSummaryReadAttempts = 3;
constexpr unsigned kCipherByteShift = 24;
constexpr std::uint32_t kWordBits = 32;
constexpr std::uint32_t kShiftMask = kWordBits - 1;

// Card space: a file takes whole 8 KiB card blocks and a save needs three.
constexpr std::int32_t kCardBlockRoundUp = 0x1FFF;
constexpr unsigned kCardBlockShift = 13;
constexpr std::int32_t kSaveFileCardBlocks = 3;

constexpr std::int8_t kInitPollLimit = 4;
constexpr std::int8_t kEventPollLimit = 64;
constexpr std::uint32_t kReadinessMask = 3;

// Transfer requests and the progress bar.
constexpr unsigned kRequestChannelShift = 12;
constexpr unsigned kRequestHeaderOnlyShift = 8;
constexpr std::int32_t kRequestFileIndexMask = 0xF;
constexpr std::int32_t kFileNumberMask = 7;
constexpr std::int32_t kTemporaryFileBit = 8;
constexpr std::int8_t kTransferErrorLimit = 3;
constexpr unsigned kRenameErrorShift = 4;
constexpr std::size_t kHeaderReadBytes = 0x2000;
constexpr std::uint16_t kProgressStart = 80;
constexpr std::int32_t kProgressEnd = 320;
constexpr std::int32_t kProgressErrorStep = 20;
constexpr std::uint16_t kReadProgressSpan = 192;
constexpr unsigned kHeaderOnlyProgressShift = 7;
constexpr auto kHeaderReadProgressSpan = static_cast<std::uint16_t>(
    kReadProgressSpan - (1U << kHeaderOnlyProgressShift));
constexpr std::uint16_t kWriteProgressSpan = 384;

// Packaging.
constexpr std::array kCardMagic = {
    std::byte{0x53},
    std::byte{0x43},
    std::byte{0x11},
    std::byte{0x03},
};
// The Shift-JIS card title "VAGRANT STORY FILE 1   00:00:00 ".
using CardTitleGlyphs = std::array<std::uint16_t, kCardTitleBytes / kHalfBytes>;
constexpr CardTitleGlyphs kCardTitleTemplate = {
    0x7582, 0x6082, 0x6682, 0x7182, 0x6082, 0x6D82, 0x7382, 0x4081,
    0x7282, 0x7382, 0x6E82, 0x7182, 0x7882, 0x4081, 0x6582, 0x6882,
    0x6B82, 0x6482, 0x4081, 0x5082, 0x4081, 0x4081, 0x4081, 0x4F82,
    0x4F82, 0x4681, 0x4F82, 0x4F82, 0x4681, 0x4F82, 0x4F82, 0x4081,
};
constexpr std::size_t kTailFileDigit = 3;
constexpr std::size_t kTailHundredHours = 6;
// Each two-digit field is a tens glyph followed by its ones glyph.
constexpr std::size_t kTailHoursTens = 7;
constexpr std::size_t kTailMinutesTens = 10;
constexpr std::size_t kTailSecondsTens = 13;
constexpr std::size_t kTailTerminator = 15;
constexpr unsigned kGlyphDigitShift = 8;
constexpr std::int32_t kDecimalBase = 10;
constexpr std::int32_t kHundredHours = 100;
constexpr std::uint16_t kHundredHoursGlyph = 0x5082;
// State_PlayTime bytes: frames, seconds, minutes, hours.
constexpr unsigned kPlayTimeSecondsShift = 8;
constexpr unsigned kPlayTimeMinutesShift = 16;
constexpr unsigned kPlayTimeHoursShift = 24;
constexpr std::int32_t kRandomWordBits = 32;
constexpr std::int32_t kRandomByteBits = 8;
constexpr std::uint32_t kFallbackSlotState = 0x17385CA9;
constexpr std::uint16_t kMaxSaveCount = 9999;
constexpr std::uint8_t kClearSaveLocation = 0x30;
// State_Settings.menuFlags bit 5 and reserved0Bit0 bit 3.
constexpr std::uint32_t kClearedMenuFlags = 0xFFDF;
constexpr unsigned kPuzzleModeSettingShift = 3;

static_assert(kHeaderBlockCount == kMinimumChecksumBlocks);
static_assert(LayoutOf(SaveSection::kSettings).size == kSettingsBytes);
static_assert(LayoutOf(SaveSection::kAshleySnapshot).size ==
              VS::Save::kAshleySnapshotBytes);
static_assert(LayoutOf(SaveSection::kRecentRoomEnemyStates).size ==
              kRoomStateSlotCount * kRoomEnemyStatesBytes);
static_assert(kChecksumOffset + kImageBlockCount ==
              kSummaryOffset + kRecordBytes);
static_assert(LayoutOf(SaveSection::kArtsStatus).offset +
                  LayoutOf(SaveSection::kArtsStatus).size ==
              kContainerOffset);
// The image ends with reserved bytes after the container data.
static_assert(kContainerOffset + kContainerBytes < kSaveImageBytes);
static_assert(kCardTitleTailOffset + (kTailTerminator + 1) * kHalfBytes ==
              kCardTitleOffset + kCardTitleBytes);

std::uint32_t LoadWord(std::span<const std::byte> bytes, std::size_t offset) {
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < kWordBytes; ++index) {
    value |= std::to_integer<std::uint32_t>(bytes[offset + index])
             << (index * kBitsPerByte);
  }
  return value;
}

std::uint16_t LoadHalf(std::span<const std::byte> bytes, std::size_t offset) {
  const auto low = std::to_integer<std::uint32_t>(bytes[offset]);
  const auto high = std::to_integer<std::uint32_t>(bytes[offset + 1]);
  return static_cast<std::uint16_t>(low | (high << kBitsPerByte));
}

void StoreWord(std::span<std::byte> bytes, std::size_t offset,
               std::uint32_t value) {
  for (std::size_t index = 0; index < kWordBytes; ++index) {
    bytes[offset + index] =
        static_cast<std::byte>((value >> (index * kBitsPerByte)) & kByteMask);
  }
}

// Stores the low halfword of `value`.
void StoreHalf(std::span<std::byte> bytes, std::size_t offset,
               std::uint32_t value) {
  bytes[offset] = static_cast<std::byte>(value & kByteMask);
  bytes[offset + 1] =
      static_cast<std::byte>((value >> kBitsPerByte) & kByteMask);
}

char NarrowChar(std::uint32_t value) {
  return static_cast<char>(static_cast<std::uint8_t>(value & kByteMask));
}

// Copies `source` over the start of `target`, never past either end; callers
// check the sizes first.
void CopyBytes(std::span<const std::byte> source, std::span<std::byte> target) {
  std::ranges::copy(source.first(std::min(source.size(), target.size())),
                    target.begin());
}

std::int8_t Incremented(std::int8_t value) {
  return static_cast<std::int8_t>(value + 1);
}

std::uint16_t LowHalf(std::int32_t value) {
  return static_cast<std::uint16_t>(static_cast<std::uint32_t>(value));
}

std::int32_t ChannelOf(std::int32_t port) {
  return WrappingMultiply(WrappingSubtract(port, 1), kChannelsPerPort);
}

CardFilename PathOf(std::string_view text) {
  CardFilename path{};
  std::ranges::copy(text.substr(0, kFilenameBytes - 1), path.begin());
  return path;
}

CardFilename FilenameFor(std::int32_t channel, char number) {
  CardFilename name = PathOf(kFilenameTemplate);
  name.at(kFilenamePortIndex) = channel == 0 ? '0' : '1';
  name.at(kFilenameNumberIndex) = number;
  return name;
}

std::string_view NameOf(const CardDirectoryEntry& entry) {
  return {entry.name.begin(), std::ranges::find(entry.name, '\0')};
}

std::span<std::byte, kRecordBytes> SlotRecord(SummaryTable summaries,
                                              std::size_t slot) {
  return summaries.subspan(slot * kRecordBytes).first<kRecordBytes>();
}

std::span<const std::byte, kRecordBytes> SlotRecord(ConstSummaryTable summaries,
                                                    std::size_t slot) {
  return summaries.subspan(slot * kRecordBytes).first<kRecordBytes>();
}

std::uint32_t SlotStateOf(std::span<const std::byte, kRecordBytes> record) {
  return LoadWord(record, kSummarySlotStateOffset);
}

std::optional<std::size_t> FirstUnavailableSlot(ConstSummaryTable summaries) {
  for (std::size_t slot = 0; slot < kSlotCount; ++slot) {
    if (SlotStateOf(SlotRecord(summaries, slot)) == kSlotUnavailable) {
      return slot;
    }
  }
  return std::nullopt;
}

std::uint8_t BlockChecksum(std::span<const std::byte> image,
                           std::size_t block) {
  std::byte checksum{0};
  for (const std::byte value :
       image.subspan(block * kBlockBytes, kBlockBytes)) {
    checksum ^= value;
  }
  return std::to_integer<std::uint8_t>(checksum);
}

std::span<std::byte> ImageSection(SaveImage image, SaveSection section) {
  const SectionLayout layout = LayoutOf(section);
  return image.subspan(layout.offset, layout.size);
}

SaveSection SectionAt(std::size_t index) {
  return static_cast<SaveSection>(index);
}

std::expected<void, SaveImageError> CheckHostSections(SaveStateHost& state) {
  for (std::size_t index = 0; index < kSaveSectionCount; ++index) {
    const SaveSection section = SectionAt(index);
    if (state.Section(section).size() != LayoutOf(section).size) {
      return std::unexpected(SaveImageError::kSectionSizeMismatch);
    }
  }
  return {};
}

// The recent-room slot and each room's next actor slot index live arrays when
// the save is applied.
std::expected<void, SaveImageError> CheckRoomState(ConstSaveImage image) {
  const SectionLayout slot = LayoutOf(SaveSection::kRoomStateSlot);
  if (LoadWord(image, slot.offset) >= kRoomStateSlotCount) {
    return std::unexpected(SaveImageError::kRoomStateSlotOutOfRange);
  }
  const SectionLayout recent = LayoutOf(SaveSection::kRecentRoomEnemyStates);
  for (std::size_t room = 0; room < kRoomStateSlotCount; ++room) {
    const std::size_t offset =
        recent.offset + room * kRoomEnemyStatesBytes + kRoomEnemyNextSlotOffset;
    if (LoadWord(image, offset) >= kRoomEnemyActorStateCount) {
      return std::unexpected(SaveImageError::kRoomEnemySlotOutOfRange);
    }
  }
  return {};
}

// _memcardEventHandler's phases.
CardReadiness ReadyStep(CardReadinessState& state, CardHost& card) {
  switch (PollEvents(card, CardEventGroup::kSoftware)) {
    case CardEvent::kIoEnd:
      state.phase = CardReadinessPhase::kLoadReady;
      break;
    case CardEvent::kError:
    case CardEvent::kTimeout:
      state.phase = CardReadinessPhase::kInit;
      break;
    case CardEvent::kUnformatted:
      state.phase = CardReadinessPhase::kUnformatted;
      break;
    case CardEvent::kNone:
      if (state.event_polls > kEventPollLimit) {
        state.phase = CardReadinessPhase::kInit;
      }
      state.event_polls = Incremented(state.event_polls);
      break;
  }
  return CardReadiness::kPending;
}

CardReadiness InitStep(CardReadinessState& state, CardHost& card) {
  state.init_polls = Incremented(state.init_polls);
  if (state.init_polls >= kInitPollLimit) {
    return CardReadiness::kTimeout;
  }
  ClearEvents(card, CardEventGroup::kSoftware);
  if (!card.StartCardInfo(state.channel)) {
    return CardReadiness::kPending;
  }
  state.phase = CardReadinessPhase::kReady;
  state.event_polls = 0;
  state.event_group = CardEventGroup::kSoftware;
  return ReadyStep(state, card);
}

// The original spins here until an event arrives; this polls once a call.
CardReadiness ConfirmedStep(CardReadinessState& state, CardHost& card) {
  switch (PollEvents(card, CardEventGroup::kHardware)) {
    case CardEvent::kNone:
      break;
    case CardEvent::kIoEnd:
      state.phase = CardReadinessPhase::kLoadReady;
      break;
    case CardEvent::kError:
    case CardEvent::kTimeout:
    case CardEvent::kUnformatted:
      state.phase = CardReadinessPhase::kInit;
      break;
  }
  return CardReadiness::kPending;
}

CardReadiness UnformattedStep(CardReadinessState& state, CardHost& card) {
  ClearEvents(card, CardEventGroup::kHardware);
  if (!card.StartCardClear(state.channel)) {
    return CardReadiness::kPending;
  }
  state.phase = CardReadinessPhase::kConfirmed;
  state.event_polls = 0;
  state.event_group = CardEventGroup::kHardware;
  return ConfirmedStep(state, card);
}

CardReadiness LoadedStep(CardReadinessState& state, CardHost& card) {
  const bool cleared = state.event_group == CardEventGroup::kHardware;
  switch (PollEvents(card, CardEventGroup::kSoftware)) {
    case CardEvent::kIoEnd:
      return cleared ? CardReadiness::kReadyAfterClear : CardReadiness::kReady;
    case CardEvent::kError:
    case CardEvent::kTimeout:
      state.phase = CardReadinessPhase::kInit;
      break;
    case CardEvent::kUnformatted:
      return cleared ? CardReadiness::kUnformattedAfterClear
                     : CardReadiness::kUnformatted;
    case CardEvent::kNone:
      if (state.event_polls > kEventPollLimit) {
        state.phase = CardReadinessPhase::kInit;
      }
      state.event_polls = Incremented(state.event_polls);
      break;
  }
  return CardReadiness::kPending;
}

CardReadiness LoadReadyStep(CardReadinessState& state, CardHost& card) {
  ClearEvents(card, CardEventGroup::kSoftware);
  if (!card.StartCardLoad(state.channel)) {
    return CardReadiness::kPending;
  }
  state.phase = CardReadinessPhase::kLoaded;
  state.event_polls = 0;
  return LoadedStep(state, card);
}

TransferStatus StatusAfter(std::int8_t errors) {
  return errors == kTransferErrorLimit ? TransferStatus::kFailed
                                       : TransferStatus::kPending;
}

// Moves the bar by `remaining` scaled by the frames since the last step over
// the expected frames (the original divides without a zero check).
void AdvanceProgress(SaveTransfer& transfer, std::int32_t remaining) {
  TransferProgress& progress = transfer.progress;
  const std::int32_t elapsed =
      WrappingSubtract(transfer.progress_counter, progress.previous);
  const std::int32_t delta =
      R3000Quotient(WrappingMultiply(elapsed, remaining), progress.target);
  progress.position = LowHalf(WrappingAdd(progress.position, delta));
}

CardFilename ReadFilename(const SaveReadState& state) {
  if ((state.file_index & kTemporaryFileBit) != 0) {
    return BuildTemporaryFilename(state.channel,
                                  state.file_index & kFileNumberMask);
  }
  return BuildSaveFilename(state.channel, state.file_index);
}

// _loadSaveData's open phase; true once the read is queued.
bool QueueRead(SaveReadState& state, SaveTransfer& transfer) {
  TransferProgress& progress = transfer.progress;
  AdvanceProgress(transfer, progress.error_offset * kProgressErrorStep -
                                (progress.position - kProgressEnd));
  progress.error_offset = state.errors;
  progress.target =
      state.header_only ? kHeaderReadProgressSpan : kReadProgressSpan;
  progress.previous = LowHalf(transfer.progress_counter);
  std::ranges::fill(transfer.workspace.incoming, std::byte{0});
  const auto file =
      transfer.card.Open(ReadFilename(state), kOpenNoWait | kOpenRead);
  if (!file.has_value()) {
    state.errors = Incremented(state.errors);
    return false;
  }
  state.file = *file;
  ClearEvents(transfer.card, CardEventGroup::kSoftware);
  const std::span<std::byte> target =
      state.header_only ? transfer.workspace.incoming.first(kHeaderReadBytes)
                        : std::span<std::byte>{transfer.workspace.incoming};
  if (!transfer.card.Read(state.file, target).has_value()) {
    transfer.card.Close(state.file);
    state.errors = Incremented(state.errors);
    return false;
  }
  state.phase = SaveReadPhase::kReading;
  return true;
}

void RenameToTemporary(SaveWriteState& state, CardHost& card) {
  if (card.Rename(BuildSaveFilename(state.channel, state.file_index),
                  BuildTemporaryFilename(state.channel, state.file_index))) {
    state.errors = 0;
    state.rename_errors = 0;
    state.phase = SaveWritePhase::kWrite;
    return;
  }
  state.rename_errors = Incremented(state.rename_errors);
  state.errors =
      static_cast<std::int8_t>(state.rename_errors >> kRenameErrorShift);
}

// _saveFile's write phase; true once the write is queued.
bool QueueWrite(SaveWriteState& state, SaveTransfer& transfer) {
  TransferProgress& progress = transfer.progress;
  AdvanceProgress(transfer, kProgressEnd - progress.position);
  progress.previous = LowHalf(transfer.progress_counter);
  progress.target = kWriteProgressSpan;
  const auto file = transfer.card.Open(
      BuildTemporaryFilename(state.channel, state.file_index),
      kOpenNoWait | kOpenWrite);
  if (!file.has_value()) {
    state.errors = Incremented(state.errors);
    return false;
  }
  state.file = *file;
  ClearEvents(transfer.card, CardEventGroup::kSoftware);
  if (!transfer.card.Write(state.file, transfer.workspace.outgoing)) {
    transfer.card.Close(state.file);
    state.errors = Incremented(state.errors);
    return false;
  }
  state.phase = SaveWritePhase::kWritePending;
  return true;
}

void AwaitWrite(SaveWriteState& state, SaveReadState& read_state,
                SaveTransfer& transfer) {
  const CardEvent event = PollEvents(transfer.card, CardEventGroup::kSoftware);
  if (event == CardEvent::kNone) {
    return;
  }
  transfer.card.Close(state.file);
  if (event != CardEvent::kIoEnd) {
    state.errors = Incremented(state.errors);
    state.phase = SaveWritePhase::kWrite;
    return;
  }
  state.phase = SaveWritePhase::kVerify;
  // Starting the verifying read resets the bar; keep its position.
  const std::uint16_t position = transfer.progress.position;
  const std::uint16_t previous = transfer.progress.previous;
  // The original shifts the sign-extended char port.
  const auto request = static_cast<std::int32_t>(
      (static_cast<std::uint32_t>(state.channel) << kRequestChannelShift) |
      static_cast<std::uint32_t>(state.file_index + kTemporaryFileBit));
  static_cast<void>(ReadSaveStep(read_state, transfer, request));
  transfer.progress.position = position;
  transfer.progress.previous = previous;
}

// Nothing while the read-back runs; kFailed when it failed or differs.
std::optional<TransferStatus> VerifyWrite(SaveWriteState& state,
                                          SaveReadState& read_state,
                                          SaveTransfer& transfer) {
  const TransferStatus read = ReadSaveStep(read_state, transfer, 0);
  if (read == TransferStatus::kPending) {
    return std::nullopt;
  }
  if (read == TransferStatus::kFailed ||
      !std::ranges::equal(transfer.workspace.outgoing,
                          transfer.workspace.incoming)) {
    return TransferStatus::kFailed;
  }
  state.phase = SaveWritePhase::kCommit;
  return std::nullopt;
}

bool CommitWrite(SaveWriteState& state, CardHost& card) {
  if (card.Rename(BuildTemporaryFilename(state.channel, state.file_index),
                  BuildSaveFilename(state.channel, state.file_index))) {
    return true;
  }
  state.rename_errors = Incremented(state.rename_errors);
  state.errors =
      static_cast<std::int8_t>(state.rename_errors >> kRenameErrorShift);
  return false;
}

enum class PackageKind : std::uint8_t { kGame, kClear };

std::expected<void, SaveImageError> CheckPackageInputs(
    SaveStateHost& state, std::int32_t target_file) {
  if (state.FileIconPalette(target_file).size() != kIconPaletteBytes ||
      state.FileIconBitmap(target_file).size() != kIconBitmapBytes) {
    return std::unexpected(SaveImageError::kFileIconOutOfRange);
  }
  return CheckHostSections(state);
}

std::int32_t PlayTimeField(std::uint32_t play_time, unsigned shift) {
  return static_cast<std::int8_t>((play_time >> shift) & kByteMask);
}

void AddToTitleTail(SaveImage image, std::size_t glyph, std::uint32_t amount) {
  const std::size_t offset = kCardTitleTailOffset + glyph * kHalfBytes;
  StoreHalf(image, offset, LoadHalf(image, offset) + amount);
}

// Adds `value`'s two decimal digits to the two glyphs from `tens`.
void AddTitleDigits(SaveImage image, std::size_t tens, std::int32_t value) {
  AddToTitleTail(image, tens,
                 static_cast<std::uint32_t>(value / kDecimalBase)
                     << kGlyphDigitShift);
  AddToTitleTail(image, tens + 1,
                 static_cast<std::uint32_t>(value % kDecimalBase)
                     << kGlyphDigitShift);
}

// The magic, Shift-JIS title with the file number and play time, and icon.
void WriteCardHeader(SaveStateHost& state, SaveImage outgoing,
                     std::int32_t target_file) {
  std::ranges::fill(outgoing, std::byte{0});
  CopyBytes(kCardMagic, outgoing.subspan(kCardMagicOffset, kCardMagic.size()));
  for (std::size_t glyph = 0; glyph < kCardTitleTemplate.size(); ++glyph) {
    StoreHalf(outgoing, kCardTitleOffset + glyph * kHalfBytes,
              kCardTitleTemplate.at(glyph));
  }
  AddToTitleTail(outgoing, kTailFileDigit,
                 static_cast<std::uint32_t>(target_file) << kGlyphDigitShift);
  const std::uint32_t play_time = state.PlayTime();
  if (PlayTimeField(play_time, kPlayTimeHoursShift) == kHundredHours) {
    StoreHalf(outgoing, kCardTitleTailOffset + kTailHundredHours * kHalfBytes,
              kHundredHoursGlyph);
  } else {
    AddTitleDigits(outgoing, kTailHoursTens,
                   PlayTimeField(play_time, kPlayTimeHoursShift));
    AddTitleDigits(outgoing, kTailMinutesTens,
                   PlayTimeField(play_time, kPlayTimeMinutesShift));
    AddTitleDigits(outgoing, kTailSecondsTens,
                   PlayTimeField(play_time, kPlayTimeSecondsShift));
  }
  StoreHalf(outgoing, kCardTitleTailOffset + kTailTerminator * kHalfBytes, 0);
  CopyBytes(state.FileIconPalette(target_file),
            outgoing.subspan(kIconPaletteOffset, kIconPaletteBytes));
  CopyBytes(state.FileIconBitmap(target_file),
            outgoing.subspan(kIconBitmapOffset, kIconBitmapBytes));
}

// A first save draws its slot identity; it starts a fresh container unless
// the game keeps a container save's data.
void AssignSlotIdentity(SaveStateHost& state, SaveImage incoming,
                        PackageKind kind) {
  const std::span<std::byte> settings = state.Section(SaveSection::kSettings);
  if (LoadWord(settings, kSettingsSlotStateOffset) != kSlotUnavailable) {
    return;
  }
  std::uint32_t slot_state = state.NextKeystreamBits(kRandomWordBits);
  if (slot_state < kFirstInUseSlotState) {
    slot_state = kFallbackSlotState;
  }
  StoreWord(settings, kSettingsSlotStateOffset, slot_state);
  if (kind == PackageKind::kClear || !state.KeepsContainerData()) {
    std::ranges::fill(incoming.subspan<kContainerOffset, kContainerBytes>(),
                      std::byte{0});
  }
}

std::uint32_t NextGeneration(ConstSummaryTable summaries) {
  std::uint32_t generation = 0;
  for (std::size_t slot = 0; slot < kSlotCount; ++slot) {
    const auto record = SlotRecord(summaries, slot);
    if (SlotStateOf(record) >= kFirstInUseSlotState) {
      generation =
          std::max(generation, LoadWord(record, kSummaryGenerationOffset));
    }
  }
  return generation + 1U;
}

void CountSave(std::span<std::byte> settings, PackageKind kind) {
  const std::uint16_t cumulative =
      LoadHalf(settings, kSettingsCumulativeSavesOffset);
  if (cumulative < kMaxSaveCount) {
    StoreHalf(settings, kSettingsCumulativeSavesOffset, cumulative + 1U);
  }
  const std::uint16_t current =
      LoadHalf(settings, kSettingsCurrentGameSavesOffset);
  if (kind == PackageKind::kClear) {
    StoreHalf(settings, kSettingsCurrentGameSavesOffset, 0);
  } else if (current < kMaxSaveCount) {
    StoreHalf(settings, kSettingsCurrentGameSavesOffset, current + 1U);
  }
}

// The cipher header: key, slot state, generation and signature.
void WriteCipherHeader(SaveStateHost& state, SaveImage outgoing,
                       ConstSummaryTable summaries) {
  const std::span<std::byte> settings = state.Section(SaveSection::kSettings);
  const auto record = outgoing.subspan<kSummaryOffset, kRecordBytes>();
  const std::uint32_t key = state.NextKeystreamBits(kRandomWordBits);
  StoreWord(settings, kSettingsKeyOffset, key);
  StoreWord(record, kSummaryKeyOffset, key);
  StoreWord(record, kSummarySlotStateOffset,
            LoadWord(settings, kSettingsSlotStateOffset));
  const std::uint32_t generation = NextGeneration(summaries);
  StoreWord(settings, kSettingsGenerationOffset, generation);
  StoreWord(record, kSummaryGenerationOffset, generation);
  StoreWord(record, kSummarySignatureOffset, kSaveSignature);
}

void WriteStatistics(SaveStateHost& state, SaveImage outgoing,
                     PackageKind kind) {
  const std::span<std::byte> settings = state.Section(SaveSection::kSettings);
  const std::span<std::byte> snapshot =
      state.Section(SaveSection::kAshleySnapshot);
  const auto stats =
      outgoing.subspan<kSummaryOffset + kSummaryStatsOffset, kStatsBytes>();
  const bool clear = kind == PackageKind::kClear;
  StoreWord(stats, kStatsGameTimeOffset, state.PlayTime());
  StoreHalf(stats, kStatsHpOffset, LoadHalf(snapshot, kSnapshotHpOffset));
  StoreHalf(stats, kStatsMaxHpOffset, LoadHalf(snapshot, kSnapshotMaxHpOffset));
  StoreHalf(stats, kStatsCumulativeSavesOffset,
            LoadHalf(settings, kSettingsCumulativeSavesOffset));
  StoreHalf(stats, kStatsCurrentGameSavesOffset,
            LoadHalf(settings, kSettingsCurrentGameSavesOffset));
  stats[kStatsSaveLocationOffset] =
      std::byte{clear ? kClearSaveLocation : state.SaveLocation()};
  stats[kStatsMapCompletionOffset] =
      std::byte{clear ? std::uint8_t{0} : state.MapCompletion()};
  stats[kStatsClearCountOffset] =
      state.Section(SaveSection::kStateFlags)[kStateFlagsClearCountOffset];
  StoreHalf(stats, kStatsMpOffset, LoadHalf(snapshot, kSnapshotMpOffset));
  StoreHalf(stats, kStatsMaxMpOffset, LoadHalf(snapshot, kSnapshotMaxMpOffset));
}

// A clear save starts the next game without misc items.
void ClearMiscItems(SaveStateHost& state, SaveImage incoming) {
  std::ranges::fill(state.Section(SaveSection::kInventory)
                        .subspan(kInventoryMiscOffset, kInventoryMiscBytes),
                    std::byte{0});
  std::ranges::fill(
      state.Section(SaveSection::kInventoryIndices)
          .subspan(kInventoryIndicesMiscOffset, kInventoryIndicesMiscBytes),
      std::byte{0});
  std::ranges::fill(
      incoming.subspan<kContainerMiscOffset, kClearedContainerMiscBytes>(),
      std::byte{0});
}

void CopySectionsOut(SaveStateHost& state, SaveWorkspace workspace) {
  for (std::size_t index = 0; index < kSaveSectionCount; ++index) {
    const SaveSection section = SectionAt(index);
    CopyBytes(state.Section(section),
              ImageSection(workspace.outgoing, section));
  }
  CopyBytes(workspace.incoming.subspan<kContainerOffset, kContainerBytes>(),
            workspace.outgoing.subspan<kContainerOffset, kContainerBytes>());
}

void WriteChecksums(SaveImage image) {
  for (std::size_t block = 0; block < kImageBlockCount; ++block) {
    if (block != kChecksumBlock) {
      image[kChecksumOffset + block] = std::byte{BlockChecksum(image, block)};
    }
  }
  image[kChecksumOffset + kChecksumBlock] =
      std::byte{BlockChecksum(image, kChecksumBlock)};
}

void EncodeImage(SaveStateHost& state, SaveImage image) {
  for (std::byte& value : image.subspan<kCipherStart>()) {
    value = static_cast<std::byte>((std::to_integer<std::uint32_t>(value) +
                                    state.NextKeystreamBits(kRandomByteBits)) &
                                   kByteMask);
  }
}

// _packageGameSaveData (MENU7.PRG and TITLE.PRG): both overlays build the
// image the same way around their own prologue and statistics.
std::expected<void, SaveImageError> PackageSave(SaveStateHost& state,
                                                SaveWorkspace workspace,
                                                ConstSummaryTable summaries,
                                                std::int32_t target_file,
                                                PackageKind kind) {
  if (auto inputs = CheckPackageInputs(state, target_file); !inputs) {
    return inputs;
  }
  if (kind == PackageKind::kGame) {
    state.CaptureSnapshot();
  } else {
    const std::span<std::byte> settings = state.Section(SaveSection::kSettings);
    state.SetPlayTime(0);
    StoreHalf(settings, kSettingsMenuFlagsOffset,
              LoadHalf(settings, kSettingsMenuFlagsOffset) & kClearedMenuFlags);
    const std::uint32_t flags = LoadWord(settings, kSettingsFlagsOffset);
    state.Section(SaveSection::kStateFlags)[kStateFlagsPuzzleModeOffset] =
        std::byte{static_cast<std::uint8_t>(
            ~(flags >> kPuzzleModeSettingShift) & 1U)};
  }
  WriteCardHeader(state, workspace.outgoing, target_file);
  AssignSlotIdentity(state, workspace.incoming, kind);
  WriteCipherHeader(state, workspace.outgoing, summaries);
  CountSave(state.Section(SaveSection::kSettings), kind);
  if (kind == PackageKind::kClear) {
    ClearMiscItems(state, workspace.incoming);
  }
  WriteStatistics(state, workspace.outgoing, kind);
  CopySectionsOut(state, workspace);
  WriteChecksums(workspace.outgoing);
  EncodeImage(state, workspace.outgoing);
  return {};
}

IntroSnapshotResult ApplyIntroSnapshot(IntroSnapshotHost& intro,
                                       SaveStateHost& state) {
  if (!intro.LoadQueued()) {
    return IntroSnapshotResult::kRejected;
  }
  if (!intro.LoadComplete()) {
    return IntroSnapshotResult::kPending;
  }
  const SaveImage image = intro.TakeImage();
  const std::uint32_t play_time = state.PlayTime();
  const auto applied = ValidateAndApplyLoadedSave(state, image, true);
  state.SetPlayTime(play_time);
  intro.FreeImage();
  return applied.has_value() ? IntroSnapshotResult::kApplied
                             : IntroSnapshotResult::kRejected;
}

constexpr std::array kPolledEvents = {
    CardEvent::kIoEnd,
    CardEvent::kError,
    CardEvent::kTimeout,
    CardEvent::kUnformatted,
};

}  // namespace

// decomp: MENU7.PRG _testMemcardEvents (src/MENU/MENU7.PRG/260.c:172),
// TITLE.PRG _testMemcardEvents (src/TITLE/TITLE.PRG/22C.c:61)
CardEvent PollEvents(CardHost& card, CardEventGroup group) {
  for (const CardEvent event : kPolledEvents) {
    if (card.TestEvent(group, event)) {
      return event;
    }
  }
  return CardEvent::kNone;
}

// decomp: MENU7.PRG _resetMemcardEvents (src/MENU/MENU7.PRG/260.c:187),
// TITLE.PRG _resetMemcardEvents (src/TITLE/TITLE.PRG/22C.c:73): testing an
// event clears it.
void ClearEvents(CardHost& card, CardEventGroup group) {
  for (const CardEvent event : kPolledEvents) {
    static_cast<void>(card.TestEvent(group, event));
  }
}

// decomp: MENU7.PRG _memcardMakeFilename (src/MENU/MENU7.PRG/260.c:238),
// TITLE.PRG _memcardMakeFilename (src/TITLE/TITLE.PRG/22C.c:110)
CardFilename BuildSaveFilename(std::int32_t channel, std::int32_t file_number) {
  return FilenameFor(
      channel,
      NarrowChar(static_cast<std::uint32_t>(file_number) + kDigitZero));
}

// decomp: MENU7.PRG _memcardMakeTempFilename (src/MENU/MENU7.PRG/260.c:254),
// TITLE.PRG _memcardMakeTempFilename (src/TITLE/TITLE.PRG/22C.c:121)
CardFilename BuildTemporaryFilename(std::int32_t channel,
                                    std::int32_t file_number) {
  return FilenameFor(
      channel,
      NarrowChar(static_cast<std::uint32_t>(file_number) + kTemporaryBase));
}

// decomp: MENU7.PRG _getNewestSaveFile (src/MENU/MENU7.PRG/260.c:278),
// TITLE.PRG _getNewestSaveFile (src/TITLE/TITLE.PRG/22C.c:192)
std::size_t NewestSaveSlot(ConstSummaryTable summaries) {
  std::uint32_t newest = 0;
  std::size_t newest_slot = 0;
  for (std::size_t slot = 0; slot < kSlotCount; ++slot) {
    const auto record = SlotRecord(summaries, slot);
    const std::uint32_t generation = LoadWord(record, kSummaryGenerationOffset);
    if (SlotStateOf(record) >= kFirstInUseSlotState && newest < generation) {
      newest = generation;
      newest_slot = slot;
    }
  }
  return newest_slot;
}

// decomp: MENU7.PRG _findCurrentSaveOnActiveMemcard
// (src/MENU/MENU7.PRG/260.c:300), TITLE.PRG _findCurrentSaveOnActiveMemcard
// (src/TITLE/TITLE.PRG/22C.c:209)
std::optional<std::size_t> FindMatchingSaveSlot(
    ConstSummaryTable summaries,
    std::span<const std::byte, kSettingsBytes> settings) {
  const std::uint32_t slot_state = LoadWord(settings, kSettingsSlotStateOffset);
  for (std::size_t slot = 0; slot < kSlotCount; ++slot) {
    const auto record = SlotRecord(summaries, slot);
    if (SlotStateOf(record) >= kFirstInUseSlotState &&
        SlotStateOf(record) == slot_state &&
        LoadWord(record, kSummaryKeyOffset) ==
            LoadWord(settings, kSettingsKeyOffset) &&
        LoadHalf(record, kSummaryStatsOffset + kStatsCumulativeSavesOffset) ==
            LoadHalf(settings, kSettingsCumulativeSavesOffset) &&
        LoadWord(record, kSummaryGenerationOffset) ==
            LoadWord(settings, kSettingsGenerationOffset)) {
      return slot;
    }
  }
  return std::nullopt;
}

// decomp: MENU7.PRG _memcardFileNumberFromFilename
// (src/MENU/MENU7.PRG/260.c:323), TITLE.PRG _memcardFileNumberFromFilename
// (src/TITLE/TITLE.PRG/22C.c:226)
std::optional<SaveFileNumber> ParseSaveFileNumber(std::string_view name) {
  if (name.size() <= kNameNumberIndex || !name.starts_with(kGameCode)) {
    return std::nullopt;
  }
  const char suffix = name[kNameNumberIndex];
  if (suffix >= kFirstTemporarySuffix && suffix <= kLastTemporarySuffix) {
    return SaveFileNumber{.number = suffix - kFirstTemporarySuffix + 1,
                          .temporary = true};
  }
  if (suffix >= kFirstSaveSuffix && suffix <= kLastSaveSuffix) {
    return SaveFileNumber{.number = suffix - kFirstSaveSuffix + 1,
                          .temporary = false};
  }
  return std::nullopt;
}

// decomp: MENU7.PRG _verifySaveChecksums (src/MENU/MENU7.PRG/260.c:355),
// TITLE.PRG _verifySaveChecksums (src/TITLE/TITLE.PRG/22C.c:284). The
// originals read the bytes unsigned (lbu).
std::expected<bool, SaveImageError> HasChecksumError(
    std::span<const std::byte> image, std::size_t block_count) {
  if (block_count > kImageBlockCount ||
      image.size() <
          std::max(block_count, kMinimumChecksumBlocks) * kBlockBytes) {
    return std::unexpected(SaveImageError::kBlockCountOutOfRange);
  }
  for (std::size_t block = 0; block < block_count; ++block) {
    if (block != kChecksumBlock &&
        BlockChecksum(image, block) !=
            std::to_integer<std::uint8_t>(image[kChecksumOffset + block])) {
      return true;
    }
  }
  return BlockChecksum(image, kChecksumBlock) != 0;
}

// decomp: MENU7.PRG _decode (src/MENU/MENU7.PRG/260.c:388), TITLE.PRG _decode
// (src/TITLE/TITLE.PRG/22C.c:314)
void DecodeSaveBytes(std::uint32_t key, std::span<std::byte> bytes) {
  for (std::byte& value : bytes) {
    key *= kCipherMultiplier;
    value = static_cast<std::byte>(
        (std::to_integer<std::uint32_t>(value) - (key >> kCipherByteShift)) &
        kByteMask);
  }
}

// decomp: MENU7.PRG _readSaveFileInfo (src/MENU/MENU7.PRG/260.c:404),
// TITLE.PRG _readSaveFileInfo (src/TITLE/TITLE.PRG/22C.c:322)
bool ReadSaveSummary(CardHost& card, SummaryTable summaries,
                     std::int32_t channel, std::int32_t file_number) {
  if (file_number < 1 || file_number > static_cast<std::int32_t>(kSlotCount)) {
    return false;
  }
  const auto slot =
      SlotRecord(summaries, static_cast<std::size_t>(file_number - 1));
  std::array<std::byte, kHeaderBytes> header{};
  const auto record = std::span{header}.subspan<kSummaryOffset, kRecordBytes>();
  for (int attempt = 0; attempt < kSummaryReadAttempts; ++attempt) {
    const auto file =
        card.Open(BuildSaveFilename(channel, file_number), kOpenRead);
    if (!file.has_value()) {
      continue;
    }
    const auto bytes_read = card.Read(*file, header);
    card.Close(*file);
    if (bytes_read != static_cast<std::int32_t>(kHeaderBytes)) {
      continue;
    }
    DecodeSaveBytes(LoadWord(record, kSummaryKeyOffset),
                    record.subspan<kSummarySlotStateOffset>());
    const auto checksum_error = HasChecksumError(header, kHeaderBlockCount);
    if (checksum_error.has_value() && !*checksum_error) {
      CopyBytes(record, slot);
      return true;
    }
  }
  std::ranges::fill(slot, std::byte{0});
  return false;
}

// decomp: MENU7.PRG _memcardSaveIdExists (src/MENU/MENU7.PRG/260.c:439),
// TITLE.PRG _memcardSaveIdExists (src/TITLE/TITLE.PRG/22C.c:354)
bool HasSaveSuffix(const CardDirectory& directory, char suffix) {
  return std::ranges::any_of(
      directory, [suffix](const std::optional<CardDirectoryEntry>& entry) {
        if (!entry.has_value()) {
          return false;
        }
        const std::string_view name = NameOf(*entry);
        return ParseSaveFileNumber(name).has_value() &&
               name[kNameNumberIndex] == suffix;
      });
}

// decomp: MENU7.PRG _deleteRedundantTempFiles (src/MENU/MENU7.PRG/260.c:458),
// TITLE.PRG _deleteRedundantTempFiles (src/TITLE/TITLE.PRG/22C.c:368)
std::expected<std::uint8_t, CardError> DeleteRedundantTemporaryFiles(
    CardHost& card, const CardDirectory& directory, std::int32_t channel) {
  std::uint8_t erased = 0;
  for (std::int32_t slot = 0; slot < static_cast<std::int32_t>(kSlotCount);
       ++slot) {
    const auto offset = static_cast<std::uint32_t>(slot);
    if (HasSaveSuffix(directory, NarrowChar(kSaveSuffixBase + offset)) &&
        HasSaveSuffix(directory, NarrowChar(kTemporarySuffixBase + offset))) {
      if (!card.Erase(BuildTemporaryFilename(channel, slot + 1))) {
        return std::unexpected(CardError::kEraseFailed);
      }
      erased = static_cast<std::uint8_t>(erased | (1U << offset));
    }
  }
  return erased;
}

// decomp: MENU7.PRG _initSaveFileInfo (src/MENU/MENU7.PRG/260.c:480),
// TITLE.PRG _initSaveFileInfo (src/TITLE/TITLE.PRG/22C.c:385)
std::expected<void, CardError> ScanSaveDirectory(CardHost& card,
                                                 CardDirectory& directory,
                                                 SummaryTable summaries,
                                                 std::int32_t port) {
  directory.fill(std::nullopt);
  directory[0] =
      card.FirstFile(PathOf(port == 1 ? kPortOnePattern : kPortTwoPattern));
  for (std::size_t index = 1;
       index < kDirectoryCapacity && directory.at(index - 1).has_value();
       ++index) {
    directory.at(index) = card.NextFile(index);
  }
  std::ranges::fill(summaries, std::byte{0});
  const std::int32_t channel = ChannelOf(port);
  const auto erased = DeleteRedundantTemporaryFiles(card, directory, channel);
  if (!erased.has_value()) {
    return std::unexpected(erased.error());
  }
  auto available = static_cast<std::int32_t>(kDirectoryCapacity);
  for (const auto& entry : directory) {
    if (!entry.has_value()) {
      continue;
    }
    const std::int32_t blocks =
        WrappingAdd(entry->size, kCardBlockRoundUp) >> kCardBlockShift;
    const auto number = ParseSaveFileNumber(NameOf(*entry));
    if (number.has_value() && number->temporary) {
      const auto slot = static_cast<std::size_t>(number->number - 1);
      if (((*erased >> slot) & 1U) != 0) {
        continue;
      }
      std::ranges::fill(SlotRecord(summaries, slot), std::byte{0});
      StoreWord(SlotRecord(summaries, slot), kSummarySlotStateOffset,
                kSlotTemporary);
    } else if (number.has_value() &&
               !ReadSaveSummary(card, summaries, channel, number->number)) {
      available += blocks;
    }
    available -= blocks;
  }
  // Each three free card blocks make one unavailable slot available.
  for (; available >= kSaveFileCardBlocks; available -= kSaveFileCardBlocks) {
    const auto slot = FirstUnavailableSlot(summaries);
    if (!slot.has_value()) {
      break;
    }
    StoreWord(SlotRecord(summaries, *slot), kSummarySlotStateOffset,
              kSlotAvailable);
  }
  return {};
}

// decomp: MENU7.PRG _createSaveFile (src/MENU/MENU7.PRG/260.c:543), TITLE.PRG
// _createSaveFile (src/TITLE/TITLE.PRG/22C.c:443)
std::expected<void, CardError> CreateSaveFile(CardHost& card, std::int32_t port,
                                              std::int32_t file_number) {
  const CardFilename name = BuildSaveFilename(ChannelOf(port), file_number);
  static_cast<void>(card.Erase(name));
  const auto file = card.Open(
      name, kOpenCreate | (static_cast<std::uint32_t>(kSaveFileCardBlocks)
                           << kOpenBlockCountShift));
  if (!file.has_value()) {
    return std::unexpected(CardError::kCreateFailed);
  }
  card.Close(*file);
  return {};
}

// decomp: MENU7.PRG _memcardEventHandler (src/MENU/MENU7.PRG/260.c:563),
// TITLE.PRG _memcardEventHandler (src/TITLE/TITLE.PRG/22C.c:457)
CardReadiness AdvanceCardReadiness(CardReadinessState& state, CardHost& card,
                                   std::int32_t begin_port) {
  if (begin_port != 0) {
    state.channel = ChannelOf(begin_port);
    state.phase = CardReadinessPhase::kInit;
    state.init_polls = 0;
    return CardReadiness::kPending;
  }
  switch (state.phase) {
    case CardReadinessPhase::kInit:
      return InitStep(state, card);
    case CardReadinessPhase::kReady:
      return ReadyStep(state, card);
    case CardReadinessPhase::kUnformatted:
      return UnformattedStep(state, card);
    case CardReadinessPhase::kConfirmed:
      return ConfirmedStep(state, card);
    case CardReadinessPhase::kLoadReady:
      return LoadReadyStep(state, card);
    case CardReadinessPhase::kLoaded:
      return LoadedStep(state, card);
  }
  return CardReadiness::kPending;
}

// decomp: MENU7.PRG _applyLoadedSaveFile (src/MENU/MENU7.PRG/260.c:681),
// TITLE.PRG _applyLoadedSaveFile (src/TITLE/TITLE.PRG/22C.c:569)
std::expected<void, SaveImageError> ValidateAndApplyLoadedSave(
    SaveStateHost& state, SaveImage incoming, bool apply) {
  const auto record = incoming.subspan<kSummaryOffset, kRecordBytes>();
  DecodeSaveBytes(LoadWord(record, kSummaryKeyOffset),
                  incoming.subspan<kCipherStart>());
  const auto checksum_error =
      HasChecksumError(incoming, apply ? kAppliedBlockCount : kImageBlockCount);
  if (!checksum_error.has_value()) {
    return std::unexpected(checksum_error.error());
  }
  if (*checksum_error) {
    return std::unexpected(SaveImageError::kChecksumMismatch);
  }
  if (LoadWord(record, kSummarySignatureOffset) != kSaveSignature) {
    return std::unexpected(SaveImageError::kSignatureMismatch);
  }
  if (!apply) {
    return {};
  }
  if (auto room_state = CheckRoomState(incoming); !room_state) {
    return room_state;
  }
  if (auto sections = CheckHostSections(state); !sections) {
    return sections;
  }
  for (std::size_t index = 0; index < kSaveSectionCount; ++index) {
    const SaveSection section = SectionAt(index);
    CopyBytes(ImageSection(incoming, section), state.Section(section));
  }
  state.SetPlayTime(
      LoadWord(record, kSummaryStatsOffset + kStatsGameTimeOffset));
  VS::Save::MarkAshleySnapshotValid(
      state.Section(SaveSection::kAshleySnapshot)
          .first<VS::Save::kAshleySnapshotBytes>());
  const std::span<std::byte> settings = state.Section(SaveSection::kSettings);
  state.SetMonoOutput(static_cast<std::int8_t>(
      std::to_integer<std::uint8_t>(settings[kSettingsMonoSoundOffset])));
  return {};
}

// decomp: MENU7.PRG _packageGameSaveData (src/MENU/MENU7.PRG/260.c:735)
std::expected<void, SaveImageError> PackageGameSave(SaveStateHost& state,
                                                    SaveWorkspace workspace,
                                                    ConstSummaryTable summaries,
                                                    std::int32_t target_file) {
  return PackageSave(state, workspace, summaries, target_file,
                     PackageKind::kGame);
}

// decomp: MENU7.PRG _loadSaveData (src/MENU/MENU7.PRG/260.c:871),
// TITLE.PRG _loadSaveData (src/TITLE/TITLE.PRG/22C.c:745)
TransferStatus ReadSaveStep(SaveReadState& state, SaveTransfer& transfer,
                            std::int32_t request) {
  if (request != 0) {
    state.phase = SaveReadPhase::kOpen;
    state.errors = 0;
    transfer.progress.error_offset = 0;
    state.channel = static_cast<std::int8_t>(request >> kRequestChannelShift);
    state.header_only = ((request >> kRequestHeaderOnlyShift) & 1) != 0;
    state.file_index =
        static_cast<std::int8_t>(request & kRequestFileIndexMask);
    transfer.progress.position = kProgressStart;
    transfer.progress.previous = 0;
    return TransferStatus::kPending;
  }
  if (state.phase == SaveReadPhase::kOpen && !QueueRead(state, transfer)) {
    return StatusAfter(state.errors);
  }
  const CardEvent event = PollEvents(transfer.card, CardEventGroup::kSoftware);
  if (event != CardEvent::kNone) {
    transfer.card.Close(state.file);
    if (event == CardEvent::kIoEnd) {
      return TransferStatus::kDone;
    }
    state.phase = SaveReadPhase::kOpen;
    state.errors = Incremented(state.errors);
  }
  return StatusAfter(state.errors);
}

// decomp: MENU7.PRG _saveFile (src/MENU/MENU7.PRG/260.c:960),
// TITLE.PRG _saveFile (src/TITLE/TITLE.PRG/22C.c:826)
TransferStatus WriteSaveStep(SaveWriteState& state, SaveReadState& read_state,
                             SaveTransfer& transfer,
                             const CardDirectory& directory,
                             std::int32_t request) {
  if (request != 0) {
    state.errors = 0;
    state.rename_errors = 0;
    transfer.progress.error_offset = 0;
    state.channel = static_cast<std::int8_t>(request >> kRequestChannelShift);
    state.file_index = static_cast<std::int8_t>(request & kFileNumberMask);
    transfer.progress.position = kProgressStart;
    transfer.progress.previous = 0;
    const bool staged = HasSaveSuffix(
        directory, NarrowChar(static_cast<std::uint32_t>(state.file_index) +
                              kTemporaryBase));
    state.phase = staged ? SaveWritePhase::kWrite : SaveWritePhase::kRename;
    return TransferStatus::kPending;
  }
  switch (state.phase) {
    case SaveWritePhase::kRename:
      RenameToTemporary(state, transfer.card);
      break;
    case SaveWritePhase::kWrite:
      if (QueueWrite(state, transfer)) {
        AwaitWrite(state, read_state, transfer);
      }
      break;
    case SaveWritePhase::kWritePending:
      AwaitWrite(state, read_state, transfer);
      break;
    case SaveWritePhase::kVerify:
      if (const auto failed = VerifyWrite(state, read_state, transfer)) {
        return *failed;
      }
      break;
    case SaveWritePhase::kCommit:
      if (CommitWrite(state, transfer.card)) {
        return TransferStatus::kDone;
      }
      break;
  }
  return StatusAfter(state.errors);
}

// decomp: MENU7.PRG _loadIntroSaveFile (src/MENU/MENU7.PRG/260.c:3315)
IntroSnapshotResult LoadIntroSnapshotStep(IntroSnapshotHost& intro,
                                          SaveStateHost& state,
                                          std::int32_t action) {
  switch (action) {
    case 0:
      return ApplyIntroSnapshot(intro, state);
    case static_cast<std::int32_t>(IntroSnapshot::kCourtyard):
      intro.BeginLoad(IntroSnapshot::kCourtyard);
      break;
    case static_cast<std::int32_t>(IntroSnapshot::kDtok):
      intro.BeginLoad(IntroSnapshot::kDtok);
      break;
    default:
      break;
  }
  return IntroSnapshotResult::kPending;
}

// decomp: TITLE.PRG _keystreamBits (src/TITLE/TITLE.PRG/22C.c:50): the shift
// is a variable srlv, so 32 bits returns the whole seed.
std::uint32_t NextCipherBits(CipherState& cipher, std::int32_t bits) {
  const std::uint32_t seed = cipher.seed;
  cipher.seed = seed * kCipherMultiplier;
  return seed >> ((kWordBits - static_cast<std::uint32_t>(bits)) & kShiftMask);
}

// decomp: TITLE.PRG _packageGameSaveData (src/TITLE/TITLE.PRG/22C.c:616)
std::expected<void, SaveImageError> PackageClearSave(
    SaveStateHost& state, SaveWorkspace workspace, ConstSummaryTable summaries,
    std::int32_t target_file) {
  return PackageSave(state, workspace, summaries, target_file,
                     PackageKind::kClear);
}

// decomp: TITLE.PRG _saveFileExists (src/TITLE/TITLE.PRG/22C.c:3189)
bool AnySaveFileExists(TitleCardProbeHost& host) {
  host.BeginResources();
  do {
    host.WaitLoadingFrame();
  } while (!host.ResourcesReady());
  bool found = false;
  for (std::int32_t port = 1; port <= kPortCount && !found; ++port) {
    host.BeginCardReadiness(port);
    std::uint32_t readiness = 0;
    do {
      readiness =
          static_cast<std::uint32_t>(host.PollCardReadiness()) & kReadinessMask;
      host.WaitFrames();
    } while (readiness == static_cast<std::uint32_t>(CardReadiness::kPending));
    if (readiness == static_cast<std::uint32_t>(CardReadiness::kReady)) {
      CardFilename pattern = FilenameFor(0, kAnyFileNumber);
      pattern.at(kFilenamePortIndex) =
          NarrowChar(kProbePortBase + static_cast<std::uint32_t>(port));
      found = host.AnyFileMatches(pattern);
    }
  }
  host.ReleaseResources();
  return found;
}
}  // namespace VS::Save::MemoryCard
