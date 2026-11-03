#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <expected>
#include <format>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Legacy/Psx.h"
#include "Save/MemoryCard/GameFiles.hpp"
#include "Save/MemoryCard/GameFilesLive.hpp"
#include "Save/Snapshot.hpp"
#include "Save/SnapshotLive.hpp"

extern "C" {
#include "Field/Mpd.h"
#include "Input/ControllerLiveState.h"
#include "Save/MemoryCard/GameFiles.h"
#include "Save/MemoryCard/Resources.h"
#include "State/Ashley.h"
#include "Title/SaveFiles.h"
}

namespace Files = VS::Save::MemoryCard;
namespace Save = VS::Save;

namespace {
using Bytes = std::vector<std::byte>;
using Checked = std::expected<bool, Files::SaveImageError>;
using Applied = std::expected<void, Files::SaveImageError>;
using Summaries = std::array<std::byte, Files::SummaryTable::extent>;
using Event = Files::CardEvent;
using Group = Files::CardEventGroup;
using Readiness = Files::CardReadiness;
using Status = Files::TransferStatus;
using SectionId = Files::SaveSection;
using Error = Files::SaveImageError;
using TitleTail = std::array<std::uint16_t, 16>;

constexpr std::size_t kImageBytes = Files::kSaveImageBytes;
constexpr std::uint32_t kInUseState = 0x1000;
constexpr std::size_t kIconCount = 7;
constexpr std::size_t kDevicePrefixBytes = 5;
constexpr std::size_t kCorruptOffset = 0x300;

template <typename T>
T ValueAt(std::span<const std::byte> bytes, std::size_t offset) {
  T value{};
  std::memcpy(&value, bytes.subspan(offset, sizeof(T)).data(), sizeof(T));
  return value;
}

template <typename T>
void Put(std::span<std::byte> bytes, std::size_t offset, T value) {
  std::memcpy(bytes.subspan(offset, sizeof(T)).data(), &value, sizeof(T));
}

std::uint8_t BlockXor(std::span<const std::byte> image, std::size_t block) {
  std::byte value{};
  for (const std::byte part :
       image.subspan(block * Files::kBlockBytes, Files::kBlockBytes)) {
    value ^= part;
  }
  return std::to_integer<std::uint8_t>(value);
}

// The encoding _packageGameSaveData applies: each byte adds the top byte of
// the next keystream value after `key`.
void Encode(std::span<std::byte> bytes, std::uint32_t key) {
  for (std::byte& value : bytes) {
    key *= Files::kCipherMultiplier;
    value = static_cast<std::byte>(
        (std::to_integer<std::uint32_t>(value) + (key >> 24U)) & 0xFFU);
  }
}

// Block checksums as the save image stores them; block 1 then cancels out.
void StoreChecksums(std::span<std::byte> image, std::size_t blocks) {
  for (std::size_t block = 0; block < blocks; ++block) {
    if (block != 1) {
      image[Files::kChecksumOffset + block] = std::byte{BlockXor(image, block)};
    }
  }
  image[Files::kChecksumOffset + 1] = std::byte{};
  image[Files::kChecksumOffset + 1] = std::byte{BlockXor(image, 1)};
}

struct SummaryFields {
  std::uint32_t slot_state = kInUseState;
  std::uint32_t generation = 1;
  std::uint32_t key = 0x01234567;
  std::uint16_t cumulative_saves = 0;
};

void PutSummary(std::span<std::byte> record, const SummaryFields& fields) {
  Put(record, Files::kSummaryKeyOffset, fields.key);
  Put(record, Files::kSummarySlotStateOffset, fields.slot_state);
  Put(record, Files::kSummaryGenerationOffset, fields.generation);
  Put(record, Files::kSummarySignatureOffset, Files::kSaveSignature);
  Put(record, Files::kSummaryStatsOffset + Files::kStatsCumulativeSavesOffset,
      fields.cumulative_saves);
}

std::span<std::byte> SlotOf(Summaries& table, std::size_t slot) {
  return std::span{table}.subspan(slot * Files::kRecordBytes,
                                  Files::kRecordBytes);
}

std::array<std::uint32_t, Files::kSlotCount> SlotStates(
    const Summaries& table) {
  std::array<std::uint32_t, Files::kSlotCount> states{};
  for (std::size_t slot = 0; slot < states.size(); ++slot) {
    states.at(slot) = ValueAt<std::uint32_t>(
        table, slot * Files::kRecordBytes + Files::kSummarySlotStateOffset);
  }
  return states;
}

std::span<std::byte> SectionOf(std::span<std::byte> image, SectionId section) {
  const Files::SectionLayout layout = Files::LayoutOf(section);
  return image.subspan(layout.offset, layout.size);
}

std::size_t NextSlotOffset(std::size_t room) {
  return room * Files::kRoomEnemyStatesBytes + Files::kRoomEnemyNextSlotOffset;
}

// Recent-room slot 2, and next actor slots 3..6 for the four rooms.
void SetRoomState(std::span<std::byte> slot, std::span<std::byte> rooms) {
  Put<std::uint32_t>(slot, 0, 2);
  for (std::size_t room = 0; room < Files::kRoomStateSlotCount; ++room) {
    Put(rooms, NextSlotOffset(room), static_cast<std::uint32_t>(room + 3));
  }
}

// A decoded save image: patterned game state, valid room indices and the
// summary record, without checksums.
Bytes PlainImage(const SummaryFields& fields) {
  Bytes image(kImageBytes);
  for (std::size_t index = Files::kHeaderBytes; index < image.size(); ++index) {
    image[index] = static_cast<std::byte>((index * 31 + 7) & 0xFF);
  }
  SetRoomState(SectionOf(image, SectionId::kRoomStateSlot),
               SectionOf(image, SectionId::kRecentRoomEnemyStates));
  PutSummary(
      std::span{image}.subspan(Files::kSummaryOffset, Files::kRecordBytes),
      fields);
  return image;
}

// A card image: checksummed over `blocks` and encoded with the summary key.
Bytes Sealed(Bytes image, std::size_t blocks = Files::kImageBlockCount) {
  StoreChecksums(image, blocks);
  Encode(std::span{image}.subspan(Files::kCipherStart),
         ValueAt<std::uint32_t>(image, Files::kSummaryOffset));
  return image;
}

Bytes SealedImage(const SummaryFields& fields = {}) {
  return Sealed(PlainImage(fields));
}

Files::SaveImage ImageOf(Bytes& bytes) {
  return std::span{bytes}.first<kImageBytes>();
}

Bytes Decoded(std::span<const std::byte> image) {
  Bytes bytes(image.begin(), image.end());
  Files::DecodeSaveBytes(ValueAt<std::uint32_t>(bytes, Files::kSummaryOffset),
                         std::span{bytes}.subspan(Files::kCipherStart));
  return bytes;
}

bool AllEqual(std::span<const std::byte> bytes, std::byte value) {
  return std::ranges::all_of(bytes,
                             [value](std::byte part) { return part == value; });
}

std::string Text(const Files::CardFilename& name) {
  return {name.begin(), std::ranges::find(name, '\0')};
}

Files::CardDirectoryEntry EntryOf(std::string_view name, std::int32_t size) {
  Files::CardDirectoryEntry entry{.size = size};
  std::ranges::copy(name.substr(0, entry.name.size() - 1), entry.name.begin());
  return entry;
}

Files::CardDirectory DirectoryOf(
    std::initializer_list<std::string_view> names) {
  Files::CardDirectory directory{};
  std::size_t index = 0;
  for (const std::string_view name : names) {
    directory.at(index++) =
        EntryOf(name, static_cast<std::int32_t>(kImageBytes));
  }
  return directory;
}

TitleTail TailOf(std::span<const std::byte> image) {
  TitleTail tail{};
  for (std::size_t glyph = 0; glyph < tail.size(); ++glyph) {
    tail.at(glyph) = ValueAt<std::uint16_t>(
        image, Files::kCardTitleTailOffset + glyph * sizeof(std::uint16_t));
  }
  return tail;
}

// "VAGRANT STORY FILE " is followed by "L", " ", "1", the hours, minutes and
// seconds glyphs and a terminator: "LE 1   00:00:00".
constexpr TitleTail kBlankTail = {
    0x6B82, 0x6482, 0x4081, 0x5082, 0x4081, 0x4081, 0x4081, 0x4F82,
    0x4F82, 0x4681, 0x4F82, 0x4F82, 0x4681, 0x4F82, 0x4F82, 0x0000,
};

struct Statistics {
  std::uint32_t game_time = 0;
  std::uint16_t cumulative_saves = 0;
  std::uint16_t current_saves = 0;
  std::uint16_t hp = 0;
  std::uint16_t max_hp = 0;
  std::uint8_t location = 0;
  std::uint8_t clear_count = 0;
  std::uint8_t map_completion = 0;
  std::uint16_t mp = 0;
  std::uint16_t max_mp = 0;
  bool operator==(const Statistics&) const = default;
};

Statistics StatisticsOf(std::span<const std::byte> decoded) {
  const auto stats = decoded.subspan(
      Files::kSummaryOffset + Files::kSummaryStatsOffset, Files::kStatsBytes);
  return {
      .game_time = ValueAt<std::uint32_t>(stats, Files::kStatsGameTimeOffset),
      .cumulative_saves =
          ValueAt<std::uint16_t>(stats, Files::kStatsCumulativeSavesOffset),
      .current_saves =
          ValueAt<std::uint16_t>(stats, Files::kStatsCurrentGameSavesOffset),
      .hp = ValueAt<std::uint16_t>(stats, Files::kStatsHpOffset),
      .max_hp = ValueAt<std::uint16_t>(stats, Files::kStatsMaxHpOffset),
      .location = ValueAt<std::uint8_t>(stats, Files::kStatsSaveLocationOffset),
      .clear_count =
          ValueAt<std::uint8_t>(stats, Files::kStatsClearCountOffset),
      .map_completion =
          ValueAt<std::uint8_t>(stats, Files::kStatsMapCompletionOffset),
      .mp = ValueAt<std::uint16_t>(stats, Files::kStatsMpOffset),
      .max_mp = ValueAt<std::uint16_t>(stats, Files::kStatsMaxMpOffset),
  };
}

struct Clock {
  std::uint32_t hours = 0;
  std::uint32_t minutes = 0;
  std::uint32_t seconds = 0;
  std::uint32_t frames = 0;
};

constexpr std::uint32_t PlayTimeOf(Clock clock) {
  return clock.frames | (clock.seconds << 8U) | (clock.minutes << 16U) |
         (clock.hours << 24U);
}

std::array<int, 4> Fields(const Files::TransferProgress& progress) {
  return {progress.position, progress.previous, progress.target,
          progress.error_offset};
}

// Restores a global when the test ends.
template <typename T>
class Restore {
 public:
  explicit Restore(T& target) : target_(target), saved_(target) {}
  ~Restore() { target_ = saved_; }
  Restore(const Restore&) = delete;
  Restore& operator=(const Restore&) = delete;

 private:
  T& target_;
  T saved_;
};

// The libapi card calls over in-memory files. Opens without kOpenNoWait
// complete at once; the others fire `io_event` once read or written.
class FakeCard final : public Files::CardHost {
 public:
  std::map<std::string, Bytes, std::less<>> files;
  std::vector<std::string> log;
  std::deque<Event> info_events;
  std::deque<Event> load_events;
  Event clear_event = Event::kIoEnd;
  Event io_event = Event::kIoEnd;
  int failing_opens = 0;
  bool start_fails = false;
  bool erase_fails = false;
  bool failing_renames = false;
  bool corrupt_writes = false;

  void Fire(Group group, Event event) { fired_.emplace_back(group, event); }
  bool TestEvent(Group group, Event event) override {
    const auto fired = std::ranges::find(fired_, std::pair{group, event});
    if (fired == fired_.end()) {
      return false;
    }
    fired_.erase(fired);
    return true;
  }
  bool StartCardInfo(std::int32_t channel) override {
    return Start("info", channel, Group::kSoftware, Pop(info_events));
  }
  bool StartCardClear(std::int32_t channel) override {
    return Start("clear", channel, Group::kHardware, clear_event);
  }
  bool StartCardLoad(std::int32_t channel) override {
    return Start("load", channel, Group::kSoftware, Pop(load_events));
  }
  std::optional<std::int32_t> Open(const Files::CardFilename& path,
                                   std::uint32_t flags) override {
    std::string name = Text(path);
    log.push_back(std::format("open {} {:x}", name, flags));
    if (failing_opens > 0) {
      --failing_opens;
      return std::nullopt;
    }
    if ((flags & Files::kOpenCreate) != 0) {
      files[name] = Bytes{};
    } else if (!files.contains(name)) {
      return std::nullopt;
    }
    handles_.push_back({.path = std::move(name), .flags = flags});
    return static_cast<std::int32_t>(handles_.size() - 1);
  }
  std::optional<std::int32_t> Read(std::int32_t file,
                                   std::span<std::byte> bytes) override {
    const Handle& handle = HandleOf(file);
    const Bytes& content = files.at(handle.path);
    const std::size_t count = std::min(content.size(), bytes.size());
    std::copy_n(content.begin(), count, bytes.begin());
    Complete(handle);
    return static_cast<std::int32_t>(count);
  }
  bool Write(std::int32_t file, std::span<const std::byte> bytes) override {
    const Handle& handle = HandleOf(file);
    Bytes& content = files.at(handle.path);
    content.assign(bytes.begin(), bytes.end());
    if (corrupt_writes) {
      content.at(kCorruptOffset) ^= std::byte{1};
    }
    Complete(handle);
    return true;
  }
  void Close(std::int32_t /*file*/) override { log.emplace_back("close"); }
  bool Erase(const Files::CardFilename& path) override {
    log.push_back("erase " + Text(path));
    return !erase_fails && files.erase(Text(path)) != 0;
  }
  bool Rename(const Files::CardFilename& from,
              const Files::CardFilename& to) override {
    log.push_back(std::format("rename {} {}", Text(from), Text(to)));
    const auto source = files.find(Text(from));
    if (failing_renames || source == files.end()) {
      return false;
    }
    Bytes content = std::move(source->second);
    files.erase(source);
    files[Text(to)] = std::move(content);
    return true;
  }
  std::optional<Files::CardDirectoryEntry> FirstFile(
      const Files::CardFilename& pattern) override {
    const std::string prefix = Text(pattern).substr(0, kDevicePrefixBytes);
    listing_.clear();
    for (const auto& [path, content] : files) {
      if (path.starts_with(prefix)) {
        listing_.push_back(EntryOf(std::string_view{path}.substr(prefix.size()),
                                   static_cast<std::int32_t>(content.size())));
      }
    }
    return NextFile(0);
  }
  std::optional<Files::CardDirectoryEntry> NextFile(
      std::size_t index) override {
    if (index >= listing_.size()) {
      return std::nullopt;
    }
    return listing_.at(index);
  }

 private:
  struct Handle {
    std::string path;
    std::uint32_t flags = 0;
  };

  static std::optional<Event> Pop(std::deque<Event>& events) {
    if (events.empty()) {
      return std::nullopt;
    }
    const Event event = events.front();
    events.pop_front();
    return event;
  }
  bool Start(std::string_view request, std::int32_t channel, Group group,
             std::optional<Event> event) {
    log.push_back(std::format("{} {}", request, channel));
    if (start_fails) {
      return false;
    }
    if (event.has_value()) {
      fired_.emplace_back(group, *event);
    }
    return true;
  }
  const Handle& HandleOf(std::int32_t file) const {
    return handles_.at(static_cast<std::size_t>(file));
  }
  void Complete(const Handle& handle) {
    if ((handle.flags & Files::kOpenNoWait) != 0) {
      fired_.emplace_back(Group::kSoftware, io_event);
    }
  }

  std::vector<std::pair<Group, Event>> fired_;
  std::vector<Handle> handles_;
  std::vector<Files::CardDirectoryEntry> listing_;
};

// Live game state as byte sections. The scripted draws come first; the key
// and the encoding bytes then share one keystream, as in the game.
class FakeSaveState final : public Files::SaveStateHost {
 public:
  std::array<Bytes, Files::kSaveSectionCount> sections;
  std::vector<std::uint32_t> play_time_sets;
  std::deque<std::uint32_t> scripted;
  Files::CipherState cipher;
  std::optional<std::int32_t> mono;
  std::uint32_t play_time = 0;
  int captures = 0;
  std::uint8_t location = 0;
  std::uint8_t map_completion = 0;
  bool keeps_container = false;

  FakeSaveState() {
    for (std::size_t index = 0; index < sections.size(); ++index) {
      sections.at(index).resize(Files::kSectionLayouts.at(index).size);
    }
    for (std::size_t icon = 0; icon < kIconCount; ++icon) {
      palettes_.at(icon).fill(static_cast<std::byte>(icon + 1));
      bitmaps_.at(icon).fill(static_cast<std::byte>(icon + 0x10));
    }
  }

  Bytes& Of(SectionId section) {
    return sections.at(static_cast<std::size_t>(section));
  }
  std::span<std::byte> Section(SectionId section) override {
    return Of(section);
  }
  std::uint32_t PlayTime() override { return play_time; }
  void SetPlayTime(std::uint32_t value) override {
    play_time = value;
    play_time_sets.push_back(value);
  }
  void CaptureSnapshot() override { ++captures; }
  void SetMonoOutput(std::int32_t value) override { mono = value; }
  std::uint8_t SaveLocation() override { return location; }
  std::uint8_t MapCompletion() override { return map_completion; }
  bool KeepsContainerData() override { return keeps_container; }
  std::uint32_t NextKeystreamBits(std::int32_t bits) override {
    if (scripted.empty()) {
      return Files::NextCipherBits(cipher, bits);
    }
    const std::uint32_t value = scripted.front();
    scripted.pop_front();
    return value;
  }
  std::span<const std::byte> FileIconPalette(std::int32_t index) override {
    if (!HasIcon(index)) {
      return {};
    }
    return palettes_.at(static_cast<std::size_t>(index));
  }
  std::span<const std::byte> FileIconBitmap(std::int32_t index) override {
    if (!HasIcon(index)) {
      return {};
    }
    return bitmaps_.at(static_cast<std::size_t>(index));
  }

 private:
  static bool HasIcon(std::int32_t index) {
    return index >= 0 && index < static_cast<std::int32_t>(kIconCount);
  }

  std::array<std::array<std::byte, Files::kIconPaletteBytes>, kIconCount>
      palettes_{};
  std::array<std::array<std::byte, Files::kIconBitmapBytes>, kIconCount>
      bitmaps_{};
};

// Patterned sections with an in-use slot, 41 saves (7 this game), mono sound,
// a valid snapshot and room indices, and 12:34:07 of play.
void FillGameState(FakeSaveState& state) {
  for (std::size_t index = 0; index < state.sections.size(); ++index) {
    Bytes& bytes = state.sections.at(index);
    for (std::size_t offset = 0; offset < bytes.size(); ++offset) {
      bytes[offset] = static_cast<std::byte>((index * 17 + offset) & 0xFF);
    }
  }
  Bytes& settings = state.Of(SectionId::kSettings);
  Put(settings, Files::kSettingsSlotStateOffset, kInUseState);
  Put<std::uint16_t>(settings, Files::kSettingsCumulativeSavesOffset, 41);
  Put<std::uint16_t>(settings, Files::kSettingsCurrentGameSavesOffset, 7);
  settings.at(Files::kSettingsMonoSoundOffset) = std::byte{0xFF};
  Put<std::uint32_t>(state.Of(SectionId::kAshleySnapshot), 0, 1);
  SetRoomState(state.Of(SectionId::kRoomStateSlot),
               state.Of(SectionId::kRecentRoomEnemyStates));
  state.play_time = PlayTimeOf({.hours = 12, .minutes = 34, .seconds = 7});
  state.location = 0x21;
  state.map_completion = 55;
}

struct Workspace {
  Bytes bytes = Bytes(2 * kImageBytes);

  Files::SaveWorkspace View() {
    return {.outgoing = std::span{bytes}.first<kImageBytes>(),
            .incoming = std::span{bytes}.subspan<kImageBytes, kImageBytes>()};
  }
};

// A card, the workspace and the progress bar, with the read and write states
// stepped one call at a time.
struct Transfer {
  FakeCard card;
  Workspace work;
  Files::TransferProgress progress{.target = 240};
  Files::SaveReadState read;
  Files::SaveWriteState write;
  std::int32_t counter = 24;

  Files::SaveTransfer View() {
    return {.card = card,
            .progress = progress,
            .progress_counter = counter,
            .workspace = work.View()};
  }
  std::vector<Status> Reads(std::int32_t request, std::size_t calls) {
    std::vector<Status> statuses;
    for (std::size_t call = 0; call < calls; ++call) {
      Files::SaveTransfer transfer = View();
      statuses.push_back(
          Files::ReadSaveStep(read, transfer, call == 0 ? request : 0));
    }
    return statuses;
  }
  std::vector<Status> Writes(const Files::CardDirectory& directory,
                             std::int32_t request, std::size_t calls) {
    std::vector<Status> statuses;
    for (std::size_t call = 0; call < calls; ++call) {
      Files::SaveTransfer transfer = View();
      statuses.push_back(Files::WriteSaveStep(write, read, transfer, directory,
                                              call == 0 ? request : 0));
    }
    return statuses;
  }
};

// Begins readiness for `port`, then polls until `calls` results.
std::vector<Readiness> Advance(Files::CardReadinessState& state, FakeCard& card,
                               std::int32_t port, std::size_t calls) {
  std::vector<Readiness> results;
  for (std::size_t call = 0; call < calls; ++call) {
    results.push_back(
        Files::AdvanceCardReadiness(state, card, call == 0 ? port : 0));
  }
  return results;
}

class FakeIntro final : public Files::IntroSnapshotHost {
 public:
  Bytes image = Bytes(kImageBytes);
  std::vector<Files::IntroSnapshot> loads;
  bool queued = false;
  bool complete = false;
  int frees = 0;

  void BeginLoad(Files::IntroSnapshot snapshot) override {
    loads.push_back(snapshot);
    queued = true;
  }
  bool LoadQueued() override { return queued; }
  bool LoadComplete() override { return complete; }
  Files::SaveImage TakeImage() override {
    queued = false;
    return ImageOf(image);
  }
  void FreeImage() override { ++frees; }
};

class FakeProbe final : public Files::TitleCardProbeHost {
 public:
  std::map<std::int32_t, std::deque<std::int32_t>> polls;
  std::set<std::string, std::less<>> matching;
  std::vector<std::string> log;
  int loading_frames = 0;

  void BeginResources() override { log.emplace_back("begin"); }
  bool ResourcesReady() override { return loading_frames >= 2; }
  void WaitLoadingFrame() override { ++loading_frames; }
  void BeginCardReadiness(std::int32_t port) override {
    log.push_back(std::format("port {}", port));
    port_ = port;
  }
  std::int32_t PollCardReadiness() override {
    std::deque<std::int32_t>& results = polls[port_];
    if (results.empty()) {
      return static_cast<std::int32_t>(Readiness::kTimeout);
    }
    const std::int32_t result = results.front();
    results.pop_front();
    return result;
  }
  void WaitFrames() override {}
  bool AnyFileMatches(const Files::CardFilename& pattern) override {
    log.push_back(Text(pattern));
    return matching.contains(Text(pattern));
  }
  void ReleaseResources() override { log.emplace_back("release"); }

 private:
  std::int32_t port_ = 0;
};

class FakeSnapshotHost final : public Save::SnapshotHost {
 public:
  std::set<std::size_t> unlocked;
  std::vector<std::string> log;
  std::optional<Save::AshleyCombatStats> stats;
  std::optional<Save::AshleySnapshot> stored_snapshot;
  Save::EntryDoor pending{};
  Save::EntryDoor stored_door{};
  Save::LearnedActions stored_learned{};
  Save::TilePlacement placement;
  Save::CameraPose camera;
  int camera_reads = 0;
  std::uint16_t flag_id = 0;

  Save::EntryDoor PendingDestinationDoor() override { return pending; }
  std::uint16_t SnapshotDoorFlagId() override { return flag_id; }
  Save::TilePlacement AshleyPlacement() override { return placement; }
  bool ActionUnlocked(std::size_t action) override {
    return unlocked.contains(action);
  }
  std::optional<Save::AshleyCombatStats> AshleyStats() override {
    return stats;
  }
  Save::CameraPose Camera() override {
    ++camera_reads;
    return camera;
  }
  void StoreEntryDoor(const Save::EntryDoor& door) override {
    stored_door = door;
    log.emplace_back("door");
  }
  void StoreLearnedActions(const Save::LearnedActions& learned) override {
    stored_learned = learned;
    log.emplace_back("learned");
  }
  void StoreSnapshot(const Save::AshleySnapshot& snapshot) override {
    stored_snapshot = snapshot;
    log.emplace_back("snapshot");
  }
  void StoreRoomActorState(std::int32_t actor) override {
    log.push_back(std::format("actor {}", actor));
  }
  void StoreEntranceDoorForReload() override { log.emplace_back("entrance"); }
};

}  // namespace

namespace {
static_assert(sizeof(Save_CardFileInfoRecord) == Files::kRecordBytes);

// The current game's save in SlotTable().
constexpr SummaryFields kCurrentSlot = {
    .slot_state = 0x3000,
    .generation = 9,
    .key = 0x33,
    .cumulative_saves = 6,
};

// An available slot, three in-use saves (the newest last) and a temporary
// file whose generation is ignored.
Summaries SlotTable() {
  constexpr std::array<SummaryFields, Files::kSlotCount> kSlots = {{
      {.slot_state = 1, .generation = 99},
      {.slot_state = 0x1000, .generation = 5, .key = 0x11},
      {.slot_state = 0x2000, .generation = 7, .key = 0x22},
      kCurrentSlot,
      {.slot_state = 2, .generation = 50},
  }};
  Summaries table{};
  for (std::size_t slot = 0; slot < kSlots.size(); ++slot) {
    PutSummary(SlotOf(table, slot), kSlots.at(slot));
  }
  return table;
}

std::array<std::byte, Files::kSettingsBytes> SettingsFor(
    const SummaryFields& fields) {
  std::array<std::byte, Files::kSettingsBytes> settings{};
  Put(settings, Files::kSettingsSlotStateOffset, fields.slot_state);
  Put(settings, Files::kSettingsKeyOffset, fields.key);
  Put(settings, Files::kSettingsCumulativeSavesOffset, fields.cumulative_saves);
  Put(settings, Files::kSettingsGenerationOffset, fields.generation);
  return settings;
}

std::vector<std::string> Names(const Files::CardDirectory& directory) {
  std::vector<std::string> names;
  for (const auto& entry : directory) {
    if (entry.has_value()) {
      names.emplace_back(entry->name.begin(),
                         std::ranges::find(entry->name, '\0'));
    }
  }
  return names;
}

std::int64_t CountOf(const std::vector<std::string>& log,
                     std::string_view line) {
  return std::ranges::count(log, line);
}
}  // namespace

TEST_CASE("Card events poll in libcard order and testing clears them",
          "[save][save-memory-card]") {
  FakeCard card;
  card.Fire(Group::kSoftware, Event::kUnformatted);
  card.Fire(Group::kSoftware, Event::kError);
  card.Fire(Group::kHardware, Event::kIoEnd);
  const std::array polled = {Files::PollEvents(card, Group::kSoftware),
                             Files::PollEvents(card, Group::kSoftware),
                             Files::PollEvents(card, Group::kSoftware)};
  CHECK(polled == std::array{Event::kError, Event::kUnformatted, Event::kNone});
  card.Fire(Group::kSoftware, Event::kTimeout);
  Files::ClearEvents(card, Group::kSoftware);
  CHECK(Files::PollEvents(card, Group::kSoftware) == Event::kNone);
  CHECK(Files::PollEvents(card, Group::kHardware) == Event::kIoEnd);
}

TEST_CASE("Save and temporary file names follow the card port and number",
          "[save][save-memory-card]") {
  const std::array names = {Text(Files::BuildSaveFilename(0, 5)),
                            Text(Files::BuildSaveFilename(16, 1)),
                            Text(Files::BuildTemporaryFilename(0, 5)),
                            Text(Files::BuildTemporaryFilename(1, 3))};
  CHECK(names == std::array<std::string, 4>{
                     "bu00:BASLUS-01040VAG5", "bu10:BASLUS-01040VAG1",
                     "bu00:BASLUS-01040VAGE", "bu10:BASLUS-01040VAGC"});
  // A rename passes both live names, so each keeps its own buffer.
  const char* save = Files::BuildLiveSaveFilename(0, 2);
  const char* temporary = Files::BuildLiveTemporaryFilename(16, 2);
  CHECK(save == std::string_view{"bu00:BASLUS-01040VAG2"});
  CHECK(temporary == std::string_view{"bu10:BASLUS-01040VAGB"});
}

TEST_CASE("Save file numbers parse from directory names",
          "[save][save-memory-card]") {
  struct Row {
    std::string_view name;
    int live = 0;
  };
  constexpr std::array<Row, 9> kRows = {{
      {.name = "BASLUS-01040VAG1", .live = 1},
      {.name = "BASLUS-01040VAG5", .live = 5},
      {.name = "BASLUS-01040VAGA", .live = -1},
      {.name = "BASLUS-01040VAGE", .live = -5},
      {.name = "BASLUS-01040VAG6", .live = 0},
      {.name = "BASLUS-01040VAGF", .live = 0},
      {.name = "BASLUS-01040VAG", .live = 0},
      {.name = "BASLUS-01041VAG1", .live = 0},
      {.name = "BASLUS-01040VAG3X", .live = 3},
  }};
  for (const Row& row : kRows) {
    CAPTURE(row.name);
    const auto number = Files::ParseSaveFileNumber(row.name);
    const int sign =
        number.value_or(Files::SaveFileNumber{}).temporary ? -1 : 1;
    CHECK(sign * number.value_or(Files::SaveFileNumber{}).number == row.live);
    std::array<char, Files::kDirectoryNameBytes> buffer{};
    std::ranges::copy(row.name, buffer.begin());
    CHECK(Files::ParseLiveSaveFileNumber(buffer.data()) == row.live);
  }
  // The live parser reads at most a directory name's 20 characters.
  constexpr std::array<char, 20> kUnterminated = {
      'B', 'A', 'S', 'L', 'U', 'S', '-', '0', '1', '0',
      '4', '0', 'V', 'A', 'G', '3', 'X', 'X', 'X', 'X'};
  CHECK(Files::ParseLiveSaveFileNumber(kUnterminated.data()) == 3);
  CHECK(Files::ParseLiveSaveFileNumber(nullptr) == 0);
}

TEST_CASE("Block checksums cover the requested blocks and block 1 cancels out",
          "[save][save-memory-card]") {
  Bytes image = PlainImage({});
  StoreChecksums(image, Files::kImageBlockCount);
  CHECK(Files::HasChecksumError(image, Files::kImageBlockCount) ==
        Checked{false});
  struct Row {
    std::size_t offset = 0;
    std::size_t blocks = 0;
    bool error = false;
  };
  constexpr std::array<Row, 6> kRows = {{
      {.offset = 0x10, .blocks = 92, .error = true},
      {.offset = 0x150, .blocks = 2, .error = true},
      {.offset = 0x4000, .blocks = 92, .error = true},
      {.offset = 0x4000, .blocks = 32, .error = false},
      {.offset = 0x5BFF, .blocks = 92, .error = true},
      {.offset = 0x5BFF, .blocks = 91, .error = false},
  }};
  for (const Row& row : kRows) {
    CAPTURE(row.offset, row.blocks);
    Bytes corrupt = image;
    corrupt.at(row.offset) ^= std::byte{0x40};
    CHECK(Files::HasChecksumError(corrupt, row.blocks) == Checked{row.error});
  }
}

TEST_CASE("Checksum checks refuse spans shorter than their blocks",
          "[save][save-memory-card]") {
  const Bytes image(kImageBytes);
  const std::span<const std::byte> bytes{image};
  const Checked range = std::unexpected(Error::kBlockCountOutOfRange);
  CHECK(Files::HasChecksumError(bytes, 93) == range);
  CHECK(Files::HasChecksumError(bytes.first(0x1FF), 0) == range);
  CHECK(Files::HasChecksumError(bytes.first(0x200), 3) == range);
  CHECK(Files::HasChecksumError(bytes.first(0x200), 2) == Checked{false});
  CHECK(Files::HasChecksumError(bytes, 92) == Checked{false});
}

TEST_CASE("The live checksum check refuses missing images and bad counts",
          "[save][save-memory-card]") {
  const auto image = std::make_unique<Save_GameSaveImage>();
  CHECK(Files::HasLiveChecksumError(nullptr, 2) == 1);
  CHECK(Files::HasLiveChecksumError(image.get(), -1) == 1);
  CHECK(Files::HasLiveChecksumError(image.get(), 93) == 1);
  CHECK(Files::HasLiveChecksumError(image.get(), 92) == 0);
  CHECK(Files::HasLiveChecksumError(image.get(), 0) == 0);
}

TEST_CASE("Decoding subtracts the keystream that follows the key",
          "[save][save-memory-card]") {
  // Key 1's keystream top bytes: 0x19660D, 0x17385CA9, 0xAF490A95, 0x0979E791.
  constexpr std::array kEncoded = {std::byte{0x00}, std::byte{0x17},
                                   std::byte{0xAF}, std::byte{0x09}};
  std::array bytes = kEncoded;
  Files::DecodeSaveBytes(1, bytes);
  CHECK(AllEqual(bytes, std::byte{0}));
  const Bytes plain = PlainImage({});
  Bytes round = plain;
  Encode(round, 0xDEADBEEF);
  Files::DecodeSaveBytes(0xDEADBEEF, round);
  CHECK(std::ranges::equal(round, plain));
  std::array live = kEncoded;
  Files::DecodeLiveSaveBytes(1, live.data(), live.size());
  Files::DecodeLiveSaveBytes(1, nullptr, live.size());
  CHECK(AllEqual(live, std::byte{0}));
}

TEST_CASE("The newest and the current save slot come from the summaries",
          "[save][save-memory-card]") {
  const Summaries table = SlotTable();
  CHECK(Files::NewestSaveSlot(table) == 3);
  CHECK(Files::NewestSaveSlot(Summaries{}) == 0);
  struct Row {
    SummaryFields settings;
    std::optional<std::size_t> slot;
  };
  const std::array<Row, 5> rows = {{
      {.settings = kCurrentSlot, .slot = 3},
      {.settings = {.slot_state = 0x3000,
                    .generation = 8,
                    .key = 0x33,
                    .cumulative_saves = 6},
       .slot = std::nullopt},
      {.settings = {.slot_state = 0x3000,
                    .generation = 9,
                    .key = 0x34,
                    .cumulative_saves = 6},
       .slot = std::nullopt},
      {.settings = {.slot_state = 0x3000,
                    .generation = 9,
                    .key = 0x33,
                    .cumulative_saves = 7},
       .slot = std::nullopt},
      // A temporary file is never the current save.
      {.settings = {.slot_state = 2, .generation = 50}, .slot = std::nullopt},
  }};
  for (const Row& row : rows) {
    CHECK(Files::FindMatchingSaveSlot(table, SettingsFor(row.settings)) ==
          row.slot);
  }
}

TEST_CASE("The live slot lookups read the published summaries and settings",
          "[save][save-memory-card]") {
  Restore summaries_global{Title_saveFileSummaries};
  Restore settings_global{Input_settings};
  const Summaries table = SlotTable();
  std::array<Save_CardFileInfoRecord, Files::kSlotCount> records{};
  std::memcpy(records.data(), table.data(), table.size());
  const auto settings = SettingsFor(kCurrentSlot);
  std::memcpy(&Input_settings, settings.data(), settings.size());
  Title_saveFileSummaries = records.data();
  CHECK(Files::NewestLiveSaveSlot() == 3);
  CHECK(Files::FindLiveMatchingSaveSlot() == 4);
  Title_saveFileSummaries = nullptr;
  CHECK(Files::NewestLiveSaveSlot() == 0);
  CHECK(Files::FindLiveMatchingSaveSlot() == 0);
}

TEST_CASE("A directory has a save suffix only under a valid save name",
          "[save][save-memory-card]") {
  const Files::CardDirectory directory =
      DirectoryOf({"BASLUS-01040VAG2", "BASLUS-01040VAGC", "BASLUS-01040VAGF",
                   "BASLUS-01041VAG4"});
  const std::array found = {Files::HasSaveSuffix(directory, '2'),
                            Files::HasSaveSuffix(directory, 'C'),
                            Files::HasSaveSuffix(directory, '3'),
                            Files::HasSaveSuffix(directory, 'F'),
                            Files::HasSaveSuffix(directory, '4')};
  CHECK(found == std::array{true, true, false, false, false});
}

TEST_CASE("Temporary files are erased when their save also exists",
          "[save][save-memory-card]") {
  FakeCard card;
  for (const char* name : {"bu10:BASLUS-01040VAGA", "bu10:BASLUS-01040VAGC",
                           "bu10:BASLUS-01040VAGE"}) {
    card.files[name] = Bytes{};
  }
  const Files::CardDirectory directory =
      DirectoryOf({"BASLUS-01040VAG1", "BASLUS-01040VAGA", "BASLUS-01040VAG3",
                   "BASLUS-01040VAGC", "BASLUS-01040VAGE"});
  CHECK(Files::DeleteRedundantTemporaryFiles(card, directory, 16) == 5);
  CHECK(card.log == std::vector<std::string>{"erase bu10:BASLUS-01040VAGA",
                                             "erase bu10:BASLUS-01040VAGC"});
  CHECK(card.files.contains("bu10:BASLUS-01040VAGE"));
  card.erase_fails = true;
  CHECK(Files::DeleteRedundantTemporaryFiles(card, directory, 16) ==
        std::unexpected(Files::CardError::kEraseFailed));
}

TEST_CASE("A directory scan reads each summary and frees slots for space",
          "[save][save-memory-card]") {
  FakeCard card;
  card.files["bu10:BASLUS-01040VAG1"] =
      SealedImage({.slot_state = 0x1000, .generation = 4});
  card.files["bu10:BASLUS-01040VAG3"] =
      SealedImage({.slot_state = 0x2000, .generation = 6});
  card.files["bu00:BASLUS-01040VAG2"] = SealedImage({});
  Files::CardDirectory directory{};
  Summaries table{};
  table.fill(std::byte{0xAB});
  CHECK(Files::ScanSaveDirectory(card, directory, table, 2).has_value());
  CHECK(Names(directory) ==
        std::vector<std::string>{"BASLUS-01040VAG1", "BASLUS-01040VAG3"});
  // Fifteen card blocks less two three-block saves free three slots.
  CHECK(SlotStates(table) ==
        std::array<std::uint32_t, Files::kSlotCount>{0x1000, 1, 0x2000, 1, 1});
  CHECK(ValueAt<std::uint32_t>(table, Files::kSummaryGenerationOffset) == 4);
}

TEST_CASE("A directory scan marks staged files and erases redundant ones",
          "[save][save-memory-card]") {
  FakeCard card;
  card.files["bu00:BASLUS-01040VAG2"] = SealedImage({});
  card.files["bu00:BASLUS-01040VAGB"] = Bytes(kImageBytes);
  card.files["bu00:BASLUS-01040VAGD"] = Bytes(kImageBytes);
  Files::CardDirectory directory{};
  Summaries table{};
  CHECK(Files::ScanSaveDirectory(card, directory, table, 1).has_value());
  CHECK(SlotStates(table) ==
        std::array<std::uint32_t, Files::kSlotCount>{1, kInUseState, 1, 2, 1});
  CHECK_FALSE(card.files.contains("bu00:BASLUS-01040VAGB"));
  CHECK(card.files.contains("bu00:BASLUS-01040VAGD"));
}

TEST_CASE("A directory scan frees the space of unreadable saves",
          "[save][save-memory-card]") {
  FakeCard card;
  for (const char number : {'1', '2', '3', '4', '5'}) {
    card.files[std::format("bu10:BASLUS-01040VAG{}", number)] =
        Bytes(kImageBytes, std::byte{0x11});
  }
  Files::CardDirectory directory{};
  Summaries table{};
  CHECK(Files::ScanSaveDirectory(card, directory, table, 2).has_value());
  CHECK(SlotStates(table) ==
        std::array<std::uint32_t, Files::kSlotCount>{1, 1, 1, 1, 1});
  CHECK(CountOf(card.log, "open bu10:BASLUS-01040VAG1 1") == 3);
  card.files["bu10:BASLUS-01040VAGA"] = Bytes{};
  card.erase_fails = true;
  CHECK(Files::ScanSaveDirectory(card, directory, table, 2) ==
        std::unexpected(Files::CardError::kEraseFailed));
}

TEST_CASE("A summary read refuses other file numbers and retries short reads",
          "[save][save-memory-card]") {
  FakeCard card;
  card.files["bu00:BASLUS-01040VAG2"] = Bytes(Files::kHeaderBytes - 1);
  Summaries table{};
  table.fill(std::byte{0xAB});
  CHECK_FALSE(Files::ReadSaveSummary(card, table, 0, 0));
  CHECK_FALSE(Files::ReadSaveSummary(card, table, 0, 6));
  CHECK(AllEqual(table, std::byte{0xAB}));
  CHECK_FALSE(Files::ReadSaveSummary(card, table, 0, 2));
  CHECK(CountOf(card.log, "open bu00:BASLUS-01040VAG2 1") == 3);
  CHECK(AllEqual(SlotOf(table, 1), std::byte{0}));
  CHECK(AllEqual(SlotOf(table, 0), std::byte{0xAB}));
}

TEST_CASE("A summary read keeps the decoded record of a later attempt",
          "[save][save-memory-card]") {
  FakeCard card;
  card.files["bu10:BASLUS-01040VAG4"] =
      SealedImage({.slot_state = 0x4000, .generation = 12});
  card.failing_opens = 2;
  Summaries table{};
  CHECK(Files::ReadSaveSummary(card, table, 16, 4));
  CHECK(card.log.size() == 4);
  const std::span<const std::byte> record = SlotOf(table, 3);
  const std::array fields = {
      ValueAt<std::uint32_t>(record, Files::kSummaryKeyOffset),
      ValueAt<std::uint32_t>(record, Files::kSummarySlotStateOffset),
      ValueAt<std::uint32_t>(record, Files::kSummaryGenerationOffset),
      ValueAt<std::uint32_t>(record, Files::kSummarySignatureOffset)};
  CHECK(fields == std::array<std::uint32_t, 4>{SummaryFields{}.key, 0x4000, 12,
                                               Files::kSaveSignature});
  CHECK(AllEqual(SlotOf(table, 0), std::byte{0}));
}

TEST_CASE("Creating a save file erases it and opens it for three blocks",
          "[save][save-memory-card]") {
  FakeCard card;
  CHECK(Files::CreateSaveFile(card, 2, 3).has_value());
  CHECK(card.log == std::vector<std::string>{"erase bu10:BASLUS-01040VAG3",
                                             "open bu10:BASLUS-01040VAG3 30200",
                                             "close"});
  CHECK(card.files.contains("bu10:BASLUS-01040VAG3"));
  card.failing_opens = 1;
  CHECK(Files::CreateSaveFile(card, 1, 1) ==
        std::unexpected(Files::CardError::kCreateFailed));
  CHECK(card.log.back() == "open bu00:BASLUS-01040VAG1 30200");
}

TEST_CASE("Card readiness follows the info, clear and load events",
          "[save][save-memory-card]") {
  constexpr Readiness kWait = Readiness::kPending;
  struct Row {
    Event info = Event::kIoEnd;
    Event load = Event::kIoEnd;
    std::vector<Readiness> results;
    std::string_view last;
  };
  const std::array<Row, 4> rows = {{
      {.info = Event::kIoEnd,
       .load = Event::kIoEnd,
       .results = {kWait, kWait, Readiness::kReady},
       .last = "load 16"},
      {.info = Event::kUnformatted,
       .load = Event::kIoEnd,
       .results = {kWait, kWait, kWait, Readiness::kReadyAfterClear},
       .last = "load 16"},
      {.info = Event::kUnformatted,
       .load = Event::kUnformatted,
       .results = {kWait, kWait, kWait, Readiness::kUnformattedAfterClear},
       .last = "load 16"},
      {.info = Event::kIoEnd,
       .load = Event::kUnformatted,
       .results = {kWait, kWait, Readiness::kUnformatted},
       .last = "load 16"},
  }};
  for (const Row& row : rows) {
    FakeCard card;
    card.info_events = {row.info};
    card.load_events = {row.load};
    Files::CardReadinessState state;
    CHECK(Advance(state, card, 2, row.results.size()) == row.results);
    CHECK(state.channel == 16);
    CHECK(card.log.back() == row.last);
    CHECK(CountOf(card.log, "clear 16") ==
          (row.info == Event::kUnformatted ? 1 : 0));
  }
}

TEST_CASE("Card readiness times out after failed starts and restarts silence",
          "[save][save-memory-card]") {
  constexpr Readiness kWait = Readiness::kPending;
  FakeCard refusing;
  refusing.start_fails = true;
  Files::CardReadinessState state;
  CHECK(Advance(state, refusing, 1, 5) ==
        std::vector{kWait, kWait, kWait, kWait, Readiness::kTimeout});
  CHECK(CountOf(refusing.log, "info 0") == 3);
  // A card that never answers is asked again after 65 empty polls.
  FakeCard silent;
  Files::CardReadinessState waiting;
  static_cast<void>(Advance(waiting, silent, 1, 67));
  CHECK(CountOf(silent.log, "info 0") == 1);
  static_cast<void>(Files::AdvanceCardReadiness(waiting, silent, 0));
  CHECK(CountOf(silent.log, "info 0") == 2);
}

TEST_CASE("A full save read moves the progress bar and fills the image",
          "[save][save-memory-card]") {
  Transfer transfer;
  const Bytes image = SealedImage({});
  transfer.card.files["bu00:BASLUS-01040VAG3"] = image;
  CHECK(transfer.Reads(3, 2) == std::vector{Status::kPending, Status::kDone});
  // 24 frames of the 240 expected move the bar 24 of its remaining 240.
  CHECK(Fields(transfer.progress) == std::array{104, 24, 192, 0});
  CHECK(std::ranges::equal(transfer.work.View().incoming, image));
  CHECK(transfer.card.log ==
        std::vector<std::string>{"open bu00:BASLUS-01040VAG3 8001", "close"});
}

TEST_CASE("A header-only read of a temporary file reads its first 32 blocks",
          "[save][save-memory-card]") {
  Transfer transfer;
  const Bytes image = SealedImage({});
  transfer.card.files["bu10:BASLUS-01040VAGB"] = image;
  std::ranges::fill(transfer.work.bytes, std::byte{0xEE});
  CHECK(transfer.Reads(0x110A, 2) ==
        std::vector{Status::kPending, Status::kDone});
  const Files::SaveImage incoming = transfer.work.View().incoming;
  CHECK(std::ranges::equal(incoming.first(0x2000),
                           std::span{image}.first(0x2000)));
  CHECK(AllEqual(incoming.subspan(0x2000), std::byte{0}));
  CHECK(transfer.progress.target == 64);
  CHECK(transfer.card.log.front() == "open bu10:BASLUS-01040VAGB 8001");
}

TEST_CASE("A save read gives up after three failed attempts",
          "[save][save-memory-card]") {
  const std::vector<Status> gave_up = {Status::kPending, Status::kPending,
                                       Status::kPending, Status::kFailed};
  Transfer missing;
  CHECK(missing.Reads(1, 4) == gave_up);
  CHECK(CountOf(missing.card.log, "open bu00:BASLUS-01040VAG1 8001") == 3);
  Transfer failing;
  failing.card.files["bu00:BASLUS-01040VAG1"] = Bytes(kImageBytes);
  failing.card.io_event = Event::kError;
  CHECK(failing.Reads(1, 4) == gave_up);
  CHECK(CountOf(failing.card.log, "close") == 3);
  CHECK(failing.progress.error_offset == 2);
}

TEST_CASE("A save write stages, verifies and commits the outgoing image",
          "[save][save-memory-card]") {
  Transfer transfer;
  transfer.card.files["bu00:BASLUS-01040VAG3"] = Bytes(kImageBytes);
  const Bytes image = SealedImage({});
  std::ranges::copy(image, transfer.work.View().outgoing.begin());
  const std::vector<Status> statuses =
      transfer.Writes(DirectoryOf({"BASLUS-01040VAG3"}), 3, 5);
  CHECK(statuses == std::vector{Status::kPending, Status::kPending,
                                Status::kPending, Status::kPending,
                                Status::kDone});
  CHECK(std::ranges::equal(transfer.card.files.at("bu00:BASLUS-01040VAG3"),
                           image));
  CHECK_FALSE(transfer.card.files.contains("bu00:BASLUS-01040VAGC"));
  CHECK(transfer.card.log.front() ==
        "rename bu00:BASLUS-01040VAG3 bu00:BASLUS-01040VAGC");
  CHECK(transfer.card.log.back() ==
        "rename bu00:BASLUS-01040VAGC bu00:BASLUS-01040VAG3");
  CHECK(Fields(transfer.progress) == std::array{104, 24, 192, 0});
}

TEST_CASE("A staged save write skips the rename and a bad read-back fails",
          "[save][save-memory-card]") {
  Transfer staged;
  staged.card.files["bu10:BASLUS-01040VAGB"] = Bytes(kImageBytes);
  CHECK(staged.Writes(DirectoryOf({"BASLUS-01040VAGB"}), 0x1002, 4) ==
        std::vector{Status::kPending, Status::kPending, Status::kPending,
                    Status::kDone});
  CHECK(staged.card.files.contains("bu10:BASLUS-01040VAG2"));
  Transfer corrupt;
  corrupt.card.files["bu00:BASLUS-01040VAG1"] = Bytes(kImageBytes);
  corrupt.card.corrupt_writes = true;
  CHECK(corrupt.Writes(DirectoryOf({"BASLUS-01040VAG1"}), 1, 4) ==
        std::vector{Status::kPending, Status::kPending, Status::kPending,
                    Status::kFailed});
  CHECK(corrupt.card.files.contains("bu00:BASLUS-01040VAGA"));
}

TEST_CASE("A save write gives up after 48 failed renames",
          "[save][save-memory-card]") {
  Transfer transfer;
  transfer.card.failing_renames = true;
  std::vector<Status> results(48, Status::kPending);
  results.push_back(Status::kFailed);
  CHECK(transfer.Writes(Files::CardDirectory{}, 1, results.size()) == results);
  CHECK(transfer.write.rename_errors == 48);
}

namespace {
// The game state of FillGameState, the workspace and the slot summaries of
// SlotTable, packaged as file 2 unless a test says otherwise.
struct Package {
  FakeSaveState state;
  Workspace work;
  Summaries summaries = SlotTable();

  Package() { FillGameState(state); }
  Applied Game(std::int32_t target = 2) {
    return Files::PackageGameSave(state, work.View(), summaries, target);
  }
  Applied Clear(std::int32_t target = 2) {
    return Files::PackageClearSave(state, work.View(), summaries, target);
  }
  Bytes Outgoing() {
    const Files::SaveImage outgoing = work.View().outgoing;
    return {outgoing.begin(), outgoing.end()};
  }
  std::span<std::byte> Container() {
    return work.View().incoming.subspan(Files::kContainerOffset,
                                        Files::kContainerBytes);
  }
};

// The container data before its misc items.
constexpr std::size_t ContainerItemBytes() {
  return Files::kContainerMiscOffset - Files::kContainerOffset;
}

struct Glyph {
  std::size_t index = 0;
  std::uint16_t value = 0;
};

TitleTail TailWith(std::initializer_list<Glyph> glyphs) {
  TitleTail tail = kBlankTail;
  for (const Glyph& glyph : glyphs) {
    tail.at(glyph.index) = glyph.value;
  }
  return tail;
}
}  // namespace

TEST_CASE("A packaged save decodes, checks and applies back into the game",
          "[save][save-memory-card]") {
  Package package;
  REQUIRE(package.Game().has_value());
  CHECK(package.state.captures == 1);
  Bytes image = package.Outgoing();
  CHECK(Files::HasChecksumError(Decoded(image), Files::kImageBlockCount) ==
        Checked{false});
  FakeSaveState loaded;
  CHECK(Files::ValidateAndApplyLoadedSave(loaded, ImageOf(image), true)
            .has_value());
  CHECK(std::ranges::equal(loaded.sections, package.state.sections));
  CHECK(loaded.play_time_sets == std::vector{package.state.play_time});
  CHECK(loaded.mono == std::optional<std::int32_t>{-1});
}

TEST_CASE("A packaged save carries the magic, the card title and the icon",
          "[save][save-memory-card]") {
  Package package;
  REQUIRE(package.Game(2).has_value());
  const Bytes image = package.Outgoing();
  constexpr std::array kMagic = {std::byte{0x53}, std::byte{0x43},
                                 std::byte{0x11}, std::byte{0x03}};
  CHECK(std::ranges::equal(std::span{image}.first(kMagic.size()), kMagic));
  // File 3 (the zero-based 2) after 12:34:07 of play.
  CHECK(TailOf(image) == TailWith({{3, 0x5282},
                                   {7, 0x5082},
                                   {8, 0x5182},
                                   {10, 0x5282},
                                   {11, 0x5382},
                                   {13, 0x4F82},
                                   {14, 0x5682}}));
  CHECK(AllEqual(std::span{image}.subspan(Files::kIconPaletteOffset,
                                          Files::kIconPaletteBytes),
                 std::byte{3}));
  CHECK(AllEqual(std::span{image}.subspan(Files::kIconBitmapOffset,
                                          Files::kIconBitmapBytes),
                 std::byte{0x12}));
  // A hundred hours shows only the hundred-hours glyph.
  package.state.play_time = PlayTimeOf({.hours = 100, .minutes = 59});
  REQUIRE(package.Game(0).has_value());
  CHECK(TailOf(package.Outgoing()) == TailWith({{6, 0x5082}}));
}

TEST_CASE("A packaged save records its statistics and the next generation",
          "[save][save-memory-card]") {
  Package package;
  Bytes& snapshot = package.state.Of(SectionId::kAshleySnapshot);
  Put<std::uint16_t>(snapshot, Files::kSnapshotHpOffset, 310);
  Put<std::uint16_t>(snapshot, Files::kSnapshotMaxHpOffset, 400);
  Put<std::uint16_t>(snapshot, Files::kSnapshotMpOffset, 55);
  Put<std::uint16_t>(snapshot, Files::kSnapshotMaxMpOffset, 60);
  package.state.Of(SectionId::kStateFlags).at(0) = std::byte{2};
  REQUIRE(package.Game().has_value());
  const Bytes decoded = Decoded(package.Outgoing());
  CHECK(StatisticsOf(decoded) ==
        Statistics{.game_time = package.state.play_time,
                   .cumulative_saves = 42,
                   .current_saves = 8,
                   .hp = 310,
                   .max_hp = 400,
                   .location = 0x21,
                   .clear_count = 2,
                   .map_completion = 55,
                   .mp = 55,
                   .max_mp = 60});
  // The newest in-use generation is 9; slots 0 and 4 are not in use.
  const std::span<const std::byte> record =
      std::span{decoded}.subspan(Files::kSummaryOffset, Files::kRecordBytes);
  const Bytes& settings = package.state.Of(SectionId::kSettings);
  const std::array<std::uint32_t, 3> identity = {
      ValueAt<std::uint32_t>(record, Files::kSummaryKeyOffset),
      ValueAt<std::uint32_t>(record, Files::kSummarySlotStateOffset),
      ValueAt<std::uint32_t>(record, Files::kSummaryGenerationOffset),
  };
  CHECK(identity == std::array<std::uint32_t, 3>{Files::kCipherMultiplier,
                                                 kInUseState, 10});
  CHECK(ValueAt<std::uint32_t>(settings, Files::kSettingsKeyOffset) ==
        Files::kCipherMultiplier);
  CHECK(ValueAt<std::uint32_t>(settings, Files::kSettingsGenerationOffset) ==
        10);
}

TEST_CASE("Save counts stop at 9999", "[save][save-memory-card]") {
  Package package;
  Bytes& settings = package.state.Of(SectionId::kSettings);
  Put<std::uint16_t>(settings, Files::kSettingsCumulativeSavesOffset, 9999);
  Put<std::uint16_t>(settings, Files::kSettingsCurrentGameSavesOffset, 9998);
  REQUIRE(package.Game().has_value());
  REQUIRE(package.Game().has_value());
  CHECK(ValueAt<std::uint16_t>(settings,
                               Files::kSettingsCumulativeSavesOffset) == 9999);
  CHECK(ValueAt<std::uint16_t>(settings,
                               Files::kSettingsCurrentGameSavesOffset) == 9999);
}

TEST_CASE("A first save draws its slot identity and resets the container",
          "[save][save-memory-card]") {
  struct Row {
    std::uint32_t draw = 0;
    bool keeps_container = false;
    bool clear = false;
    std::uint32_t slot_state = 0;
    std::byte container{};
  };
  constexpr std::array<Row, 4> kRows = {{
      {.draw = 0x12345678, .slot_state = 0x12345678},
      // Identities below the in-use states fall back to a fixed one.
      {.draw = 2, .slot_state = 0x17385CA9},
      {.draw = 0x12345678,
       .keeps_container = true,
       .slot_state = 0x12345678,
       .container = std::byte{0x5A}},
      {.draw = 0x12345678,
       .keeps_container = true,
       .clear = true,
       .slot_state = 0x12345678},
  }};
  for (const Row& row : kRows) {
    CAPTURE(row.draw, row.keeps_container, row.clear);
    Package package;
    Put<std::uint32_t>(package.state.Of(SectionId::kSettings),
                       Files::kSettingsSlotStateOffset, 0);
    package.state.scripted = {row.draw};
    package.state.keeps_container = row.keeps_container;
    std::ranges::fill(package.Container(), std::byte{0x5A});
    REQUIRE((row.clear ? package.Clear() : package.Game()).has_value());
    const Bytes decoded = Decoded(package.Outgoing());
    CHECK(ValueAt<std::uint32_t>(decoded, Files::kSummaryOffset +
                                              Files::kSummarySlotStateOffset) ==
          row.slot_state);
    CHECK(AllEqual(package.Container().first(ContainerItemBytes()),
                   row.container));
  }
}

TEST_CASE("Packaging refuses bad icons and section sizes before any change",
          "[save][save-memory-card]") {
  const Applied bad_icon = std::unexpected(Error::kFileIconOutOfRange);
  for (const std::int32_t icon : {-1, 7}) {
    CAPTURE(icon);
    Package package;
    std::ranges::fill(package.work.bytes, std::byte{0xEE});
    const auto sections = package.state.sections;
    CHECK(package.Game(icon) == bad_icon);
    CHECK(package.Clear(icon) == bad_icon);
    CHECK(AllEqual(package.work.bytes, std::byte{0xEE}));
    CHECK(std::ranges::equal(package.state.sections, sections));
    CHECK(package.state.captures == 0);
  }
  Package wrong;
  wrong.state.Of(SectionId::kInventory).resize(0x10);
  CHECK(wrong.Clear() == Applied{std::unexpected(Error::kSectionSizeMismatch)});
  CHECK(wrong.state.play_time_sets.empty());
}

TEST_CASE("A clear save restarts the play time and keeps the puzzle setting",
          "[save][save-memory-card]") {
  struct Row {
    std::uint16_t flags = 0;
    std::byte puzzle_mode{};
  };
  constexpr std::array<Row, 2> kRows = {{
      {.flags = 0, .puzzle_mode = std::byte{1}},
      {.flags = 8, .puzzle_mode = std::byte{0}},
  }};
  for (const Row& row : kRows) {
    Package package;
    Bytes& settings = package.state.Of(SectionId::kSettings);
    Put(settings, Files::kSettingsFlagsOffset, row.flags);
    Put<std::uint16_t>(settings, Files::kSettingsMenuFlagsOffset, 0xFFFF);
    REQUIRE(package.Clear().has_value());
    CHECK(package.state.play_time_sets == std::vector<std::uint32_t>{0});
    CHECK(ValueAt<std::uint16_t>(settings, Files::kSettingsMenuFlagsOffset) ==
          0xFFDF);
    CHECK(package.state.Of(SectionId::kStateFlags)
              .at(Files::kStateFlagsPuzzleModeOffset) == row.puzzle_mode);
    CHECK(package.state.captures == 0);
  }
}

TEST_CASE("A clear save drops the misc items and the current save count",
          "[save][save-memory-card]") {
  Package package;
  std::ranges::fill(package.Container(), std::byte{0x5A});
  REQUIRE(package.Clear().has_value());
  const Statistics stats = StatisticsOf(Decoded(package.Outgoing()));
  CHECK(std::array{stats.game_time, std::uint32_t{stats.current_saves},
                   std::uint32_t{stats.cumulative_saves},
                   std::uint32_t{stats.location},
                   std::uint32_t{stats.map_completion}} ==
        std::array<std::uint32_t, 5>{0, 0, 42, 0x30, 0});
  const std::span<std::byte> inventory =
      package.state.Of(SectionId::kInventory);
  CHECK(AllEqual(inventory.subspan(Files::kInventoryMiscOffset,
                                   Files::kInventoryMiscBytes),
                 std::byte{0}));
  CHECK(inventory[Files::kInventoryMiscOffset - 1] != std::byte{0});
  const std::span<std::byte> indices =
      package.state.Of(SectionId::kInventoryIndices);
  CHECK(AllEqual(indices.subspan(Files::kInventoryIndicesMiscOffset,
                                 Files::kInventoryIndicesMiscBytes),
                 std::byte{0}));
  const auto misc = package.Container().subspan(ContainerItemBytes());
  CHECK(AllEqual(misc.first(Files::kClearedContainerMiscBytes), std::byte{0}));
  CHECK(AllEqual(misc.subspan(Files::kClearedContainerMiscBytes),
                 std::byte{0x5A}));
}

TEST_CASE("A loaded save with bad checks or indices applies nothing",
          "[save][save-memory-card]") {
  struct Row {
    std::size_t offset = 0;
    std::uint32_t value = 0;
    // Corrupts a sealed byte instead of storing a plain word.
    bool sealed = false;
    Error error = Error::kChecksumMismatch;
  };
  const std::array<Row, 4> rows = {{
      {.offset = kCorruptOffset, .value = 0x40, .sealed = true},
      {.offset = Files::kSummaryOffset + Files::kSummarySignatureOffset,
       .error = Error::kSignatureMismatch},
      {.offset = Files::LayoutOf(SectionId::kRoomStateSlot).offset,
       .value = 4,
       .error = Error::kRoomStateSlotOutOfRange},
      {.offset = Files::LayoutOf(SectionId::kRecentRoomEnemyStates).offset +
                 NextSlotOffset(3),
       .value = Files::kRoomEnemyActorStateCount,
       .error = Error::kRoomEnemySlotOutOfRange},
  }};
  for (const Row& row : rows) {
    CAPTURE(row.offset);
    Bytes plain = PlainImage({});
    if (!row.sealed) {
      Put(plain, row.offset, row.value);
    }
    Bytes image = Sealed(std::move(plain));
    if (row.sealed) {
      image.at(row.offset) ^= static_cast<std::byte>(row.value & 0xFF);
    }
    FakeSaveState loaded;
    CHECK(Files::ValidateAndApplyLoadedSave(loaded, ImageOf(image), true) ==
          Applied{std::unexpected(row.error)});
    CHECK(loaded.play_time_sets.empty());
    CHECK_FALSE(loaded.mono.has_value());
    CHECK(AllEqual(loaded.Of(SectionId::kSettings), std::byte{0}));
  }
}

TEST_CASE("Checking a loaded save without applying it leaves the game alone",
          "[save][save-memory-card]") {
  Bytes plain = PlainImage({});
  Put<std::uint32_t>(plain, Files::LayoutOf(SectionId::kRoomStateSlot).offset,
                     9);
  Bytes image = Sealed(plain);
  FakeSaveState loaded;
  CHECK(Files::ValidateAndApplyLoadedSave(loaded, ImageOf(image), false)
            .has_value());
  CHECK(loaded.play_time_sets.empty());
  CHECK(AllEqual(loaded.Of(SectionId::kRoomStateSlot), std::byte{0}));
  // An unchecked block past the applied ones fails the whole-image check.
  Bytes tail = Sealed(PlainImage({}));
  tail.at(Files::kAppliedBlockCount * Files::kBlockBytes) ^= std::byte{1};
  CHECK(Files::ValidateAndApplyLoadedSave(loaded, ImageOf(tail), false) ==
        Applied{std::unexpected(Error::kChecksumMismatch)});
}

TEST_CASE("A header-only load applies its game state from the first blocks",
          "[save][save-memory-card]") {
  Bytes image = Sealed(PlainImage({}), Files::kAppliedBlockCount);
  const Bytes plain = Decoded(image);
  std::ranges::fill(
      std::span{image}.subspan(Files::kAppliedBlockCount * Files::kBlockBytes),
      std::byte{0});
  FakeSaveState loaded;
  CHECK(Files::ValidateAndApplyLoadedSave(loaded, ImageOf(image), true)
            .has_value());
  const auto entry =
      std::span{plain}.subspan(Files::LayoutOf(SectionId::kEntryDoor).offset,
                               Files::LayoutOf(SectionId::kEntryDoor).size);
  CHECK(std::ranges::equal(loaded.Of(SectionId::kEntryDoor), entry));
  FakeSaveState wrong;
  wrong.Of(SectionId::kArtsStatus).resize(4);
  Bytes again = Sealed(PlainImage({}));
  CHECK(Files::ValidateAndApplyLoadedSave(wrong, ImageOf(again), true) ==
        Applied{std::unexpected(Error::kSectionSizeMismatch)});
  CHECK(wrong.play_time_sets.empty());
}

TEST_CASE("An intro snapshot applies once loaded and keeps the play time",
          "[save][save-memory-card]") {
  using Result = Files::IntroSnapshotResult;
  FakeIntro intro;
  FakeSaveState state;
  state.play_time = PlayTimeOf({.hours = 1});
  CHECK(Files::LoadIntroSnapshotStep(intro, state, 1) == Result::kPending);
  CHECK(Files::LoadIntroSnapshotStep(intro, state, 0) == Result::kPending);
  Bytes plain = PlainImage({});
  Put(plain, Files::kSummaryOffset + Files::kSummaryStatsOffset,
      PlayTimeOf({.minutes = 3}));
  intro.image = Sealed(std::move(plain), Files::kAppliedBlockCount);
  intro.complete = true;
  CHECK(Files::LoadIntroSnapshotStep(intro, state, 0) == Result::kApplied);
  CHECK(intro.loads == std::vector{Files::IntroSnapshot::kCourtyard});
  CHECK(state.play_time_sets ==
        std::vector{PlayTimeOf({.minutes = 3}), PlayTimeOf({.hours = 1})});
  CHECK(intro.frees == 1);
}

TEST_CASE("Intro snapshot steps without a valid load are rejected",
          "[save][save-memory-card]") {
  using Result = Files::IntroSnapshotResult;
  FakeIntro intro;
  FakeSaveState state;
  CHECK(Files::LoadIntroSnapshotStep(intro, state, 0) == Result::kRejected);
  CHECK(Files::LoadIntroSnapshotStep(intro, state, 5) == Result::kPending);
  CHECK(Files::LoadIntroSnapshotStep(intro, state, 2) == Result::kPending);
  // A zero image has no save signature.
  intro.complete = true;
  CHECK(Files::LoadIntroSnapshotStep(intro, state, 0) == Result::kRejected);
  CHECK(intro.loads == std::vector{Files::IntroSnapshot::kDtok});
  CHECK(state.play_time_sets == std::vector<std::uint32_t>{0});
  CHECK(intro.frees == 1);
}

TEST_CASE("The title keystream returns the seed's top bits and advances it",
          "[save][save-memory-card]") {
  Files::CipherState cipher;
  const std::array draws = {
      Files::NextCipherBits(cipher, 32), Files::NextCipherBits(cipher, 8),
      Files::NextCipherBits(cipher, 16), Files::NextCipherBits(cipher, 0)};
  CHECK(draws ==
        std::array<std::uint32_t, 4>{0x19660D, 0x17, 0xAF49, 0x0979E791});
  CHECK(cipher.seed == 0xAA9D885D);
  const std::uint32_t first = Files::NextLiveTitleCipherBits(32);
  CHECK(Files::NextLiveTitleCipherBits(32) == first * Files::kCipherMultiplier);
}

TEST_CASE("The title card probe looks for a save on each ready card",
          "[save][save-memory-card]") {
  const std::string one = "bu00:BASLUS-01040VAG?";
  const std::string two = "bu10:BASLUS-01040VAG?";
  struct Row {
    std::deque<std::int32_t> port_one;
    std::deque<std::int32_t> port_two;
    std::string matching;
    bool found = false;
    std::vector<std::string> log;
  };
  const std::array<Row, 3> rows = {{
      {.port_one = {0, 0, 1},
       .port_two = {},
       .matching = one,
       .found = true,
       .log = {"begin", "port 1", one, "release"}},
      // 5 is ready after a clear.
      {.port_one = {1},
       .port_two = {0, 5},
       .matching = two,
       .found = true,
       .log = {"begin", "port 1", one, "port 2", two, "release"}},
      {.port_one = {},
       .port_two = {3},
       .matching = one,
       .log = {"begin", "port 1", "port 2", "release"}},
  }};
  for (const Row& row : rows) {
    FakeProbe probe;
    probe.polls[1] = row.port_one;
    probe.polls[2] = row.port_two;
    probe.matching.insert(row.matching);
    CHECK(Files::AnySaveFileExists(probe) == row.found);
    CHECK(probe.log == row.log);
    CHECK(probe.loading_frames == 2);
  }
}

namespace {
static_assert(sizeof(Field_MpdDoorRecord) == Save::kEntryDoorBytes);

Field_MpdDoorRecord DoorRecordOf(const Save::EntryDoor& bytes) {
  Field_MpdDoorRecord door{};
  std::memcpy(&door, bytes.data(), sizeof(door));
  return door;
}

// The door fields a capture sets.
std::array<int, 5> EntryFields(const Field_MpdDoorRecord& door) {
  return {door.tileLocation.entryDirection, door.entryTileX,
          door.entryElevation, door.requiredFlagId, door.entryTileZ};
}

// The door fields a capture keeps.
std::array<int, 12> KeptFields(const Field_MpdDoorRecord& door) {
  return {door.destinationZoneId,
          door.destinationRoomId,
          door.tileLocation.tileX,
          door.tileLocation.tileZ,
          door.tileLocation.floorLayer,
          door.requiredFlagValue,
          door.hasEntryStep,
          door.reserved06Bit15,
          door.doorId,
          door.reserved09,
          door.reserved0A,
          door.reserved0B};
}

std::vector<std::string> CaptureLog(bool with_stats) {
  std::vector<std::string> log = {"door", "learned"};
  if (with_stats) {
    log.emplace_back("snapshot");
  }
  for (std::int32_t actor = 0; actor < Save::kSnapshotRoomActorCount; ++actor) {
    log.push_back(std::format("actor {}", actor));
  }
  log.emplace_back("entrance");
  return log;
}

std::span<std::byte> IncomingOf(std::vector<char>& workspace) {
  return std::as_writable_bytes(std::span{workspace}).subspan(kImageBytes);
}
}  // namespace

TEST_CASE("The entry door takes Ashley's tile, layer, elevation and flag",
          "[save][save-memory-card]") {
  Save::EntryDoor pending{};
  for (std::size_t index = 0; index < pending.size(); ++index) {
    pending.at(index) = static_cast<std::byte>((0xA5 ^ (index * 0x3B)) & 0xFF);
  }
  const Save::EntryDoor door = Save::PlaceEntryDoor(
      pending,
      {.tile_x = 0x3D, .tile_layer = 6, .tile_z = 0x2B, .facing = 0x95},
      0x1ABC);
  // Each field keeps only its width: x 0x1D, elevation 2, flag 0x2BC, z 0xB.
  CHECK(EntryFields(DoorRecordOf(door)) == std::array{6, 0x1D, 2, 0x2BC, 0x0B});
  CHECK(KeptFields(DoorRecordOf(door)) == KeptFields(DoorRecordOf(pending)));
}

TEST_CASE("Learned actions pack eight to a byte from the high bit",
          "[save][save-memory-card]") {
  FakeSnapshotHost host;
  host.unlocked = {0, 7, 8, 100, 255};
  Save::LearnedActions expected{};
  expected.at(0) = 0x81;
  expected.at(1) = 0x80;
  expected.at(12) = 0x08;
  expected.at(31) = 0x01;
  CHECK(Save::PackLearnedActions(host) == expected);
}

TEST_CASE("A capture stores the door, actions, statistics and room actors",
          "[save][save-memory-card]") {
  FakeSnapshotHost host;
  host.stats = Save::AshleyCombatStats{.current_hp = 120, .max_hp = 150};
  host.camera = {.pitch = -64, .distance = 0x900};
  host.unlocked = {3};
  host.flag_id = 0x155;
  Save::CaptureAshleySnapshot(host);
  CHECK(host.log == CaptureLog(true));
  const Save::AshleySnapshot stored =
      host.stored_snapshot.value_or(Save::AshleySnapshot{});
  const std::array hp = {stored.stats.current_hp, stored.stats.max_hp};
  CHECK(hp == std::array<std::int16_t, 2>{120, 150});
  const std::array camera = {stored.camera.pitch, stored.camera.distance};
  CHECK(camera == std::array{-64, 0x900});
  CHECK(host.stored_learned.at(0) == 0x10);
  CHECK(EntryFields(DoorRecordOf(host.stored_door)).at(3) == 0x155);
}

TEST_CASE("A capture without Ashley's actor keeps the stored statistics",
          "[save][save-memory-card]") {
  FakeSnapshotHost host;
  Save::CaptureAshleySnapshot(host);
  CHECK(host.log == CaptureLog(false));
  CHECK_FALSE(host.stored_snapshot.has_value());
  CHECK(host.camera_reads == 0);
}

TEST_CASE("Marking the snapshot valid sets only its pending flag",
          "[save][save-memory-card]") {
  std::array<std::byte, Save::kAshleySnapshotBytes> bytes{};
  bytes.fill(std::byte{0xEE});
  Save::MarkAshleySnapshotValid(bytes);
  CHECK(ValueAt<std::uint32_t>(bytes, 0) == 1);
  CHECK(AllEqual(std::span{bytes}.subspan(sizeof(std::uint32_t)),
                 std::byte{0xEE}));
  Restore snapshot_global{State_ashleySnapshot};
  State_ashleySnapshot.stats.isPending = 0;
  State_ashleySnapshot.stats.currentHp = 77;
  Save::MarkLiveAshleySnapshotValid();
  CHECK(State_ashleySnapshot.stats.isPending == 1);
  CHECK(State_ashleySnapshot.stats.currentHp == 77);
}

TEST_CASE("The live save check decodes the workspace's incoming image",
          "[save][save-memory-card]") {
  Restore resource_global{Title_memoryCardUiResource};
  std::vector<char> workspace(2 * kImageBytes);
  const std::span<std::byte> incoming = IncomingOf(workspace);
  Title_memoryCardUiResource = workspace.data();
  const Bytes image = SealedImage();
  std::ranges::copy(image, incoming.begin());
  CHECK(Files::ValidateAndApplyLiveLoadedSave(0) == 0);
  CHECK(std::ranges::equal(incoming, Decoded(image)));
  Bytes corrupt = image;
  corrupt.at(kCorruptOffset) ^= std::byte{0x40};
  std::ranges::copy(corrupt, incoming.begin());
  CHECK(Files::ValidateAndApplyLiveLoadedSave(0) == 1);
  Title_memoryCardUiResource = nullptr;
  CHECK(Files::ValidateAndApplyLiveLoadedSave(0) == 1);
}

TEST_CASE("Live packaging refuses a save without the save menu's icons",
          "[save][save-memory-card]") {
  Restore resource_global{Title_memoryCardUiResource};
  Restore summaries_global{Title_saveFileSummaries};
  Restore icons_global{Save_MemoryCard_uiResources};
  Restore settings_global{Input_settings};
  std::vector<char> workspace(2 * kImageBytes, '\x5A');
  std::array<Save_CardFileInfoRecord, Files::kSlotCount> records{};
  Title_memoryCardUiResource = workspace.data();
  Title_saveFileSummaries = records.data();
  Save_MemoryCard_uiResources = nullptr;
  const State_Settings settings = Input_settings;
  Files::PackageLiveGameSave(1);
  Files::PackageLiveClearSave(1);
  CHECK(AllEqual(IncomingOf(workspace), std::byte{0x5A}));
  CHECK(AllEqual(std::as_bytes(std::span{workspace}).first(kImageBytes),
                 std::byte{0x5A}));
  CHECK(std::memcmp(&settings, &Input_settings, sizeof(settings)) == 0);
}

TEST_CASE("The snapshot routines are no longer resident module exports",
          "[save][save-memory-card]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40",
                                          0x80042CA0U) == nullptr);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40",
                                          0x80042CB0U) == nullptr);
}
