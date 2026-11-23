// Strict SEQ bank decoding and encoding (SeqFile.hpp).
#include "Content/SeqFile.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <format>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace VS::Content {
namespace {
using Bytes = std::span<const std::byte>;
using Lengths = std::span<const std::uint8_t>;
template <typename T>
using Result = std::expected<T, std::string>;
using Status = std::expected<void, std::string>;
using Keys = std::array<std::int16_t, kSeqAxes>;

// The bank header: the motion and map offsets.
constexpr std::size_t kMotionField = 0;
constexpr std::size_t kMapField = 4;
constexpr std::size_t kBankHeaderBytes = 8;
// A clip header: length, base, flags, then the events, root and travel
// words, a rotation word per bone and a scale word per bone.
constexpr std::size_t kClipFixedBytes = 10;
constexpr std::size_t kClipBytesPerBone = 4;
constexpr std::size_t kClipBaseAt = 2;
constexpr std::size_t kClipFlagsAt = 3;
constexpr std::size_t kClipEventsAt = 4;
constexpr std::size_t kClipRootAt = 6;
constexpr std::size_t kClipTravelAt = 8;
constexpr std::size_t kClipRotationsAt = 10;
constexpr std::uint8_t kNoClip = 0xFF;
constexpr std::uint8_t kKnownFlags = kSeqHasInitialScales | kSeqHasScaleTracks;
constexpr std::size_t kMaxClips = 0xFF;
constexpr std::size_t kMaxMotionOffset = 0xFFFF;

// The file header: the map length, a zero byte, the bones and the bank's size
// rounded up to a word.
constexpr std::size_t kFileHeaderBytes = 8;
constexpr std::size_t kFilePadAt = 1;
constexpr std::size_t kFileBonesAt = 2;
constexpr std::size_t kFileSizeAt = 4;
constexpr std::size_t kSizeAlignment = 4;
constexpr std::size_t kMaxMapLength = 0xFF;

// Initial keys: three big-endian halfwords. The game doubles a vector's
// halfwords, so their top two bits must agree.
constexpr std::size_t kVectorBytes = 6;
constexpr std::size_t kRawScaleBytes = 3;
constexpr std::size_t kPackedScaleBytes = 8;
constexpr std::size_t kPackedScaleKeysAt = 2;
constexpr std::uint8_t kPackedScaleTag = 0xFE;
constexpr std::uint8_t kPackedScaleVersion = 1;
constexpr std::uint16_t kKeySignBit = 0x8000;
constexpr std::uint16_t kKeyTopBit = 0x4000;
constexpr int kPackedKeyMin = -0x4000;
constexpr int kPackedKeyMax = 0x3FFF;
constexpr int kRawScaleMax = 0xFF;

// Commands (SequencePlayback.cpp DecodeCommand). A first byte with any of
// the byte-form axis bits holds a 5-bit run; otherwise it holds a 2-bit run
// and byte axes, and a word follows whose low bits name the word axes.
constexpr std::uint8_t kEnd = 0;
constexpr std::uint8_t kByteFormAxes = 0xE0;
constexpr std::array<std::uint8_t, kSeqAxes> kByteFormAxis{0x80, 0x40, 0x20};
constexpr std::uint8_t kByteFormRun = 0x1F;
constexpr std::array<std::uint8_t, kSeqAxes> kPackedByteAxis{0x10, 0x08, 0x04};
constexpr std::uint8_t kPackedRun = 3;
constexpr std::array<std::uint16_t, kSeqAxes> kWordAxis{4, 2, 1};
constexpr int kWordValueShift = 3;
constexpr std::uint16_t kFirstWordValueMask = 0x1FFF;
constexpr int kFirstWordMin = -0x1000;
constexpr int kFirstWordMax = 0xFFF;
constexpr int kByteMin = -0x80;
constexpr int kByteMax = 0x7F;
constexpr std::size_t kExtraRunMax = 0xFF;
constexpr std::size_t kByteFormMaxFrames = kByteFormRun + 1 + kExtraRunMax;
constexpr std::size_t kPackedMaxFrames = kPackedRun + 1 + kExtraRunMax;

// Event cursors: a frame byte, or 0xFF and the frames past 255.
constexpr std::uint8_t kLongFrame = 0xFF;
constexpr std::size_t kMaxEventFrame = kLongFrame + kExtraRunMax;

constexpr int kBitsPerByte = 8;

template <typename T>
T ReadLe(Bytes data, std::size_t offset) {
  T value{};
  std::memcpy(&value, data.subspan(offset, sizeof value).data(), sizeof value);
  return value;
}

std::uint8_t ByteAt(Bytes data, std::size_t offset) {
  return std::to_integer<std::uint8_t>(data[offset]);
}

std::uint16_t BigHalfAt(Bytes data, std::size_t offset) {
  return static_cast<std::uint16_t>((ByteAt(data, offset) << kBitsPerByte) |
                                    ByteAt(data, offset + 1));
}

std::unexpected<std::string> Fail(std::size_t offset,
                                  std::string_view message) {
  return std::unexpected(std::format("byte 0x{:X}: {}", offset, message));
}

std::size_t ClipHeaderBytes(std::size_t bones) {
  return kClipFixedBytes + kClipBytesPerBone * bones;
}

std::size_t RoundUp(std::size_t size) {
  return (size + kSizeAlignment - 1) / kSizeAlignment * kSizeAlignment;
}

class Writer {
 public:
  void PutByte(std::uint8_t value) { bytes_.push_back(std::byte{value}); }
  void PutLe16(std::uint16_t value) {
    PutByte(static_cast<std::uint8_t>(value));
    PutByte(static_cast<std::uint8_t>(value >> kBitsPerByte));
  }
  void PutLe32(std::uint32_t value) {
    PutLe16(static_cast<std::uint16_t>(value));
    PutLe16(static_cast<std::uint16_t>(value >> (kBitsPerByte * 2)));
  }
  void PutBe16(std::uint16_t value) {
    PutByte(static_cast<std::uint8_t>(value >> kBitsPerByte));
    PutByte(static_cast<std::uint8_t>(value));
  }
  void Append(Bytes bytes) {
    bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
  }
  [[nodiscard]] std::size_t Size() const { return bytes_.size(); }
  [[nodiscard]] Bytes View() const { return bytes_; }
  std::vector<std::byte> Take() { return std::move(bytes_); }

 private:
  std::vector<std::byte> bytes_;
};

// ---- Commands ----

// Reads forward from `at`, refusing bytes at or past `limit`.
class Cursor {
 public:
  Cursor(Bytes bytes, std::size_t at, std::size_t limit)
      : bytes_(bytes), at_(at), limit_(limit) {}

  [[nodiscard]] std::size_t At() const { return at_; }

  bool Byte(std::uint8_t& out) {
    if (at_ >= limit_) {
      return false;
    }
    out = ByteAt(bytes_, at_++);
    return true;
  }

  bool BigHalf(std::uint16_t& out) {
    if (at_ >= limit_ || limit_ - at_ < sizeof(std::uint16_t)) {
      return false;
    }
    out = BigHalfAt(bytes_, at_);
    at_ += sizeof(std::uint16_t);
    return true;
  }

 private:
  Bytes bytes_;
  std::size_t at_;
  std::size_t limit_;
};

struct ReadCommandResult {
  SeqCommand command;
  std::size_t end = 0;
};

// The word axes after the packed word: the first takes the word's top 13
// bits, each later one a whole word, and each clears its byte axis.
bool ReadWordAxes(Cursor& in, std::uint16_t packed, SeqCommand& command,
                  std::array<bool, kSeqAxes>& byte_axes) {
  auto value = static_cast<std::int16_t>(static_cast<std::int16_t>(packed) >>
                                         kWordValueShift);
  bool first = true;
  for (std::size_t axis = 0; axis < kSeqAxes; ++axis) {
    if ((packed & kWordAxis[axis]) == 0) {
      continue;
    }
    if (!first) {
      std::uint16_t next = 0;
      if (!in.BigHalf(next)) {
        return false;
      }
      value = static_cast<std::int16_t>(next);
    }
    command.axes[axis] = value;
    byte_axes[axis] = false;
    first = false;
  }
  return true;
}

// The command at `at`, which is not the terminator, read as DecodeCommand
// (SequencePlayback.cpp) reads it. Nothing when it runs past `limit`.
std::optional<ReadCommandResult> ReadCommand(Bytes motion, std::size_t at,
                                             std::size_t limit) {
  Cursor in(motion, at, limit);
  std::uint8_t first = 0;
  in.Byte(first);
  const bool byte_form = (first & kByteFormAxes) != 0;
  const std::uint8_t run_mask = byte_form ? kByteFormRun : kPackedRun;
  const auto run = static_cast<std::uint8_t>(first & run_mask);
  ReadCommandResult result;
  std::size_t frames = std::size_t{run} + 1;
  if (run == run_mask) {
    std::uint8_t extra = 0;
    if (!in.Byte(extra)) {
      return std::nullopt;
    }
    frames += extra;
  }
  result.command.frames = static_cast<std::uint16_t>(frames);
  std::array<bool, kSeqAxes> byte_axes{};
  const auto& bits = byte_form ? kByteFormAxis : kPackedByteAxis;
  for (std::size_t axis = 0; axis < kSeqAxes; ++axis) {
    byte_axes[axis] = (first & bits[axis]) != 0;
  }
  if (!byte_form) {
    std::uint16_t packed = 0;
    if (!in.BigHalf(packed) ||
        !ReadWordAxes(in, packed, result.command, byte_axes)) {
      return std::nullopt;
    }
  }
  for (std::size_t axis = 0; axis < kSeqAxes; ++axis) {
    std::uint8_t value = 0;
    if (byte_axes[axis]) {
      if (!in.Byte(value)) {
        return std::nullopt;
      }
      result.command.axes[axis] =
          static_cast<std::int16_t>(static_cast<std::int8_t>(value));
    }
  }
  result.end = in.At();
  return result;
}

void PutRun(Writer& out, std::uint8_t lead, std::size_t frames,
            std::uint8_t run_mask) {
  if (frames <= run_mask) {
    out.PutByte(static_cast<std::uint8_t>(lead | (frames - 1)));
    return;
  }
  out.PutByte(static_cast<std::uint8_t>(lead | run_mask));
  out.PutByte(static_cast<std::uint8_t>(frames - run_mask - 1));
}

std::string AxisName(std::size_t axis) {
  constexpr std::array<std::string_view, kSeqAxes> kNames{"x", "y", "z"};
  return std::string(kNames[axis]);
}

Result<std::vector<std::byte>> ByteForm(const SeqCommand& command) {
  if (command.frames == 0 || command.frames > kByteFormMaxFrames) {
    return std::unexpected(
        std::format("{} frames do not fit a command of byte deltas (1 to {})",
                    command.frames, kByteFormMaxFrames));
  }
  std::uint8_t lead = 0;
  for (std::size_t axis = 0; axis < kSeqAxes; ++axis) {
    if (command.axes[axis]) {
      lead |= kByteFormAxis[axis];
    }
  }
  Writer out;
  PutRun(out, lead, command.frames, kByteFormRun);
  for (const auto& value : command.axes) {
    if (value) {
      out.PutByte(static_cast<std::uint8_t>(*value));
    }
  }
  return out.Take();
}

// The axis the packed word's top 13 bits carry: the first word axis, or,
// when that needs more bits, the earliest axis before it, made a word too.
Result<std::optional<std::size_t>> FirstWordAxis(
    const SeqCommand& command, std::array<bool, kSeqAxes>& words) {
  const auto found = std::ranges::find(words, true);
  if (found == words.end()) {
    return std::nullopt;
  }
  const auto first = static_cast<std::size_t>(found - words.begin());
  const int value = *command.axes[first];
  if (value >= kFirstWordMin && value <= kFirstWordMax) {
    return first;
  }
  for (std::size_t axis = 0; axis < first; ++axis) {
    if (command.axes[axis]) {
      words[axis] = true;
      return axis;
    }
  }
  return std::unexpected(
      std::format("the {} delta {} needs more than 13 bits, and no earlier "
                  "axis can take the packed word",
                  AxisName(first), value));
}

Result<std::vector<std::byte>> PackedForm(const SeqCommand& command,
                                          std::array<bool, kSeqAxes> words) {
  if (command.frames == 0 || command.frames > kPackedMaxFrames) {
    return std::unexpected(
        std::format("{} frames do not fit a command of word deltas (1 to {})",
                    command.frames, kPackedMaxFrames));
  }
  const auto first = FirstWordAxis(command, words);
  if (!first) {
    return std::unexpected(first.error());
  }
  // Every present axis sets its byte bit, as the game's files do; a word
  // axis clears that bit before the bytes are read.
  std::uint8_t lead = 0;
  std::uint16_t packed = 0;
  for (std::size_t axis = 0; axis < kSeqAxes; ++axis) {
    if (command.axes[axis]) {
      lead |= kPackedByteAxis[axis];
    }
    if (words[axis]) {
      packed |= kWordAxis[axis];
    }
  }
  if (lead == 0 && command.frames == 1) {
    // Its first byte would be the terminator.
    return std::unexpected(
        std::string("a 1-frame command must change an axis"));
  }
  if (*first) {
    const auto value = static_cast<std::uint16_t>(*command.axes[**first]);
    packed |= static_cast<std::uint16_t>((value & kFirstWordValueMask)
                                         << kWordValueShift);
  }
  Writer out;
  PutRun(out, lead, command.frames, kPackedRun);
  out.PutBe16(packed);
  for (std::size_t axis = 0; axis < kSeqAxes; ++axis) {
    if (words[axis] && axis != **first) {
      out.PutBe16(static_cast<std::uint16_t>(*command.axes[axis]));
    }
  }
  for (std::size_t axis = 0; axis < kSeqAxes; ++axis) {
    if (!words[axis] && command.axes[axis]) {
      out.PutByte(static_cast<std::uint8_t>(*command.axes[axis]));
    }
  }
  return out.Take();
}

// The one form the encoder writes a command in: byte deltas when every
// present axis fits a signed byte, else the packed form with word deltas for
// the axes that do not.
Result<std::vector<std::byte>> EncodeCommand(const SeqCommand& command) {
  std::array<bool, kSeqAxes> words{};
  bool any_axis = false;
  for (std::size_t axis = 0; axis < kSeqAxes; ++axis) {
    if (const auto& value = command.axes[axis]) {
      any_axis = true;
      words[axis] = *value < kByteMin || *value > kByteMax;
    }
  }
  if (any_axis && std::ranges::none_of(words, std::identity{})) {
    return ByteForm(command);
  }
  return PackedForm(command, words);
}

// ---- Events ----

Result<std::size_t> EventStride(std::uint8_t op, Lengths lengths) {
  if (op >= lengths.size()) {
    return std::unexpected(std::format(
        "event {} is past the {}-entry stride table", op, lengths.size()));
  }
  std::size_t stride = lengths[op];
  if (op == kSeqLongIgnoredEvent) {
    stride += kSeqLongIgnoredExtraBytes;
  }
  if (stride == 0) {
    return std::unexpected(std::format("event {} has a zero stride", op));
  }
  return stride;
}

// ---- What each header word is read as ----

enum class Read : std::uint8_t { kNone, kOwnVector, kOwnScale, kCommands };

Read KeyRead(bool own) { return own ? Read::kOwnVector : Read::kCommands; }

// DecodeScales reads initial scales from an own clip's scale tracks when its
// flag 1 is set; ApplyPoseTracks plays the playing clip's scale tracks from
// their cursors when its flag 2 is.
Read ScaleRead(bool own, std::uint8_t flags) {
  if (own && (flags & kSeqHasInitialScales) != 0) {
    return Read::kOwnScale;
  }
  if ((flags & kSeqHasScaleTracks) != 0) {
    return Read::kCommands;
  }
  return Read::kNone;
}

std::string_view ReadName(Read read) {
  switch (read) {
    case Read::kOwnVector:
      return "a key and its commands";
    case Read::kOwnScale:
      return "a scale key and its commands";
    case Read::kCommands:
      return "commands alone";
    case Read::kNone:
      break;
  }
  return "nothing";
}

// ---- Playback reach, shared by both directions ----

// Where a clip's playback starts reading commands for `ref`.
std::optional<SeqEntry> CommandStart(const SeqRef& ref) {
  if (const auto* own = std::get_if<SeqOwn>(&ref)) {
    return SeqEntry{.element = own->element, .command = 0};
  }
  if (const auto* entry = std::get_if<SeqEntry>(&ref)) {
    return *entry;
  }
  return std::nullopt;
}

// Playback reads commands until it has covered `frames` frames or met a
// terminator; it must never run past a track's last command.
Status CheckReach(const SeqBank& bank, const SeqRef& ref, std::size_t frames) {
  const auto start = CommandStart(ref);
  if (!start) {
    return {};
  }
  const auto& track = std::get<SeqTrack>(bank.motion[start->element]);
  std::size_t reached = 0;
  for (std::size_t command = start->command; reached < frames; ++command) {
    if (command >= track.commands.size()) {
      if (track.terminated) {
        return {};
      }
      return std::unexpected(std::format(
          "motion element {} has no terminator and its commands end at frame "
          "{} of {}",
          start->element, reached, frames));
    }
    reached += track.commands[command].frames;
  }
  return {};
}

Status CheckClipReach(const SeqBank& bank, std::size_t index) {
  const SeqClip& clip = bank.clips[index];
  if (clip.length < 2) {
    return {};
  }
  const std::size_t frames = std::size_t{clip.length} - 1;
  auto check = [&](const SeqRef& ref, const std::string& what) -> Status {
    if (auto reach = CheckReach(bank, ref, frames); !reach) {
      return std::unexpected(
          std::format("clip {}'s {}: {}", index, what, reach.error()));
    }
    return {};
  };
  if (auto root = check(clip.root, "root"); !root) {
    return root;
  }
  if (clip.travel) {
    if (auto travel = check(SeqOwn{.element = *clip.travel}, "travel");
        !travel) {
      return travel;
    }
  }
  for (std::size_t bone = 0; bone < clip.rotations.size(); ++bone) {
    if (auto rotation =
            check(clip.rotations[bone], std::format("bone {} rotation", bone));
        !rotation) {
      return rotation;
    }
  }
  if (clip.scales && (clip.flags & kSeqHasScaleTracks) != 0) {
    for (std::size_t bone = 0; bone < clip.scales->size(); ++bone) {
      if (auto scale =
              check((*clip.scales)[bone], std::format("bone {} scale", bone));
          !scale) {
        return scale;
      }
    }
  }
  return {};
}

Status CheckPlayback(const SeqBank& bank) {
  for (std::size_t index = 0; index < bank.clips.size(); ++index) {
    if (auto reach = CheckClipReach(bank, index); !reach) {
      return reach;
    }
  }
  return {};
}

// ---- Decoding ----

// What a hard start (an offset read from its start) holds.
enum class Kind : std::uint8_t { kVector, kScale, kEvents };

std::string_view KindName(Kind kind) {
  switch (kind) {
    case Kind::kVector:
      return "a pose key";
    case Kind::kScale:
      return "a scale key";
    case Kind::kEvents:
      return "an event script";
  }
  return "?";
}

// A decoded element and where it lies in the motion area.
struct Piece {
  std::size_t end = 0;
  SeqElement element;
  // A track's command offsets, then its terminator's.
  std::vector<std::size_t> boundaries;
};

// Where a commands-alone word resolved: a piece's command, or a zero byte.
struct Target {
  std::optional<std::size_t> piece;
  std::size_t command = 0;
};

struct RawClip {
  std::uint16_t length = 0;
  std::uint8_t base = kNoClip;
  std::uint8_t flags = 0;
  std::uint16_t events = 0;
  std::uint16_t root = 0;
  std::uint16_t travel = 0;
  std::vector<std::uint16_t> rotations;
  std::vector<std::uint16_t> scales;
};

class Decoder {
 public:
  Decoder(Bytes bank, std::size_t bones, Lengths lengths,
          std::size_t report_base)
      : bank_(bank),
        bones_(bones),
        lengths_(lengths),
        report_base_(report_base) {}

  Result<SeqBank> Run();

 private:
  [[nodiscard]] std::unexpected<std::string> FailAt(
      std::size_t offset, std::string_view message) const {
    return Fail(report_base_ + offset, message);
  }
  [[nodiscard]] std::unexpected<std::string> FailMotion(
      std::size_t offset, std::string_view message) const {
    return FailAt(motion_at_ + offset, message);
  }
  [[nodiscard]] std::uint8_t MotionByte(std::size_t offset) const {
    return ByteAt(motion_, offset);
  }

  Status ReadLayout();
  Status ReadMap(std::size_t map_at, std::size_t clips);
  Status ReadClip(std::size_t index);
  Status CheckBases() const;
  Status Collect(const RawClip& clip);
  Status AddHard(std::size_t offset, Kind kind);
  Status AddRead(std::size_t offset, Read read);
  Status DecodeHardStarts();
  Result<Piece> DecodeHard(std::size_t start, Kind kind,
                           std::size_t limit) const;
  Result<Piece> DecodeScale(std::size_t start, std::size_t limit) const;
  Result<Keys> ReadKeys(std::size_t at, bool doubled) const;
  Result<Piece> DecodeTrack(SeqTrack track, std::size_t at,
                            std::size_t limit) const;
  Result<Piece> DecodeEvents(std::size_t start, std::size_t limit) const;
  Status ResolveCursors();
  Result<Target> Resolve(std::size_t offset);
  void FillGaps();
  [[nodiscard]] SeqBank Assemble() const;
  [[nodiscard]] SeqRef RefOf(
      std::size_t offset, Read read,
      const std::map<std::size_t, std::size_t>& index) const;

  Bytes bank_;
  std::size_t bones_;
  Lengths lengths_;
  std::size_t report_base_;
  std::size_t motion_at_ = 0;
  Bytes motion_;
  std::vector<std::optional<std::uint8_t>> map_;
  std::vector<RawClip> clips_;
  std::map<std::size_t, Kind> hard_;
  std::set<std::size_t> cursors_;
  std::map<std::size_t, Piece> pieces_;
  std::map<std::size_t, Target> targets_;
};

Status Decoder::ReadLayout() {
  if (bones_ > kSeqMaxBones) {
    return std::unexpected(std::format(
        "{} bones is more than the {} a pose holds", bones_, kSeqMaxBones));
  }
  if (bank_.size() < kBankHeaderBytes) {
    return FailAt(0, "the bank is shorter than its 8-byte header");
  }
  const std::size_t motion = ReadLe<std::uint32_t>(bank_, kMotionField);
  const std::size_t map = ReadLe<std::uint32_t>(bank_, kMapField);
  if (map < kBankHeaderBytes || map > motion || motion > bank_.size()) {
    return FailAt(kMotionField,
                  std::format("the map (0x{:X}) and motion (0x{:X}) offsets "
                              "do not lie in order inside the {}-byte bank",
                              map, motion, bank_.size()));
  }
  const std::size_t header_bytes = ClipHeaderBytes(bones_);
  if ((map - kBankHeaderBytes) % header_bytes != 0) {
    return FailAt(kMapField,
                  std::format("the clip headers before the map are not whole "
                              "{}-byte headers",
                              header_bytes));
  }
  const std::size_t clips = (map - kBankHeaderBytes) / header_bytes;
  if (clips > kMaxClips) {
    return FailAt(kMapField,
                  std::format("{} clips is more than {}", clips, kMaxClips));
  }
  motion_at_ = motion;
  motion_ = bank_.subspan(motion);
  if (auto read = ReadMap(map, clips); !read) {
    return read;
  }
  clips_.resize(clips);
  for (std::size_t index = 0; index < clips; ++index) {
    if (auto clip = ReadClip(index); !clip) {
      return clip;
    }
  }
  return CheckBases();
}

Status Decoder::ReadMap(std::size_t map_at, std::size_t clips) {
  for (std::size_t at = map_at; at < motion_at_; ++at) {
    const std::uint8_t clip = ByteAt(bank_, at);
    if (clip == kNoClip) {
      map_.emplace_back();
    } else if (clip >= clips) {
      return FailAt(at,
                    std::format("the map names clip {} of {}", clip, clips));
    } else {
      map_.emplace_back(clip);
    }
  }
  return {};
}

Status Decoder::ReadClip(std::size_t index) {
  const std::size_t header = kBankHeaderBytes + index * ClipHeaderBytes(bones_);
  RawClip& clip = clips_[index];
  clip.length = ReadLe<std::uint16_t>(bank_, header);
  clip.base = ByteAt(bank_, header + kClipBaseAt);
  clip.flags = ByteAt(bank_, header + kClipFlagsAt);
  if ((clip.flags & ~kKnownFlags) != 0) {
    return FailAt(header + kClipFlagsAt,
                  std::format("clip {}'s flags 0x{:X} hold bits the game "
                              "does not read",
                              index, clip.flags));
  }
  clip.events = ReadLe<std::uint16_t>(bank_, header + kClipEventsAt);
  clip.root = ReadLe<std::uint16_t>(bank_, header + kClipRootAt);
  clip.travel = ReadLe<std::uint16_t>(bank_, header + kClipTravelAt);
  const std::size_t rotations = header + kClipRotationsAt;
  const std::size_t scales = rotations + bones_ * sizeof(std::uint16_t);
  const Read scale = ScaleRead(clip.base == kNoClip, clip.flags);
  for (std::size_t bone = 0; bone < bones_; ++bone) {
    const std::size_t scale_at = scales + bone * sizeof(std::uint16_t);
    clip.rotations.push_back(
        ReadLe<std::uint16_t>(bank_, rotations + bone * sizeof(std::uint16_t)));
    clip.scales.push_back(ReadLe<std::uint16_t>(bank_, scale_at));
    if (scale == Read::kNone && clip.scales.back() != 0) {
      return FailAt(scale_at,
                    std::format("clip {}'s flags read no scales, but bone "
                                "{}'s scale word is not zero",
                                index, bone));
    }
  }
  return {};
}

Status Decoder::CheckBases() const {
  for (std::size_t index = 0; index < clips_.size(); ++index) {
    const std::uint8_t base = clips_[index].base;
    if (base == kNoClip) {
      continue;
    }
    if (base >= clips_.size() || clips_[base].base != kNoClip) {
      return FailAt(
          kBankHeaderBytes + index * ClipHeaderBytes(bones_) + kClipBaseAt,
          std::format("clip {}'s base {} is not an own clip", index, base));
    }
  }
  return {};
}

Status Decoder::AddHard(std::size_t offset, Kind kind) {
  if (offset >= motion_.size()) {
    return FailMotion(
        offset, std::format("{} starts past the motion area", KindName(kind)));
  }
  const auto [found, added] = hard_.emplace(offset, kind);
  if (!added && found->second != kind) {
    return FailMotion(
        offset, std::format("read as both {} and {}", KindName(found->second),
                            KindName(kind)));
  }
  return {};
}

Status Decoder::AddRead(std::size_t offset, Read read) {
  switch (read) {
    case Read::kOwnVector:
      return AddHard(offset, Kind::kVector);
    case Read::kOwnScale:
      return AddHard(offset, Kind::kScale);
    case Read::kCommands:
      cursors_.insert(offset);
      return {};
    case Read::kNone:
      break;
  }
  return {};
}

Status Decoder::Collect(const RawClip& clip) {
  const bool own = clip.base == kNoClip;
  if (clip.events != 0) {
    if (auto events = AddHard(clip.events, Kind::kEvents); !events) {
      return events;
    }
  }
  if (clip.travel != 0) {
    if (auto travel = AddHard(clip.travel, Kind::kVector); !travel) {
      return travel;
    }
  }
  if (auto root = AddRead(clip.root, KeyRead(own)); !root) {
    return root;
  }
  for (const std::uint16_t word : clip.rotations) {
    if (auto rotation = AddRead(word, KeyRead(own)); !rotation) {
      return rotation;
    }
  }
  const Read scale = ScaleRead(own, clip.flags);
  for (const std::uint16_t word : clip.scales) {
    if (auto read = AddRead(word, scale); !read) {
      return read;
    }
  }
  return {};
}

Result<Keys> Decoder::ReadKeys(std::size_t at, bool doubled) const {
  Keys keys{};
  for (std::size_t axis = 0; axis < kSeqAxes; ++axis) {
    const std::size_t field = at + axis * sizeof(std::uint16_t);
    const std::uint16_t word = BigHalfAt(motion_, field);
    if (((word & kKeySignBit) != 0) != ((word & kKeyTopBit) != 0)) {
      return FailMotion(field,
                        "a key's top two bits differ, which the game's "
                        "doubling would lose");
    }
    const auto half = static_cast<std::int16_t>(word);
    keys[axis] = doubled ? static_cast<std::int16_t>(half * 2) : half;
  }
  return keys;
}

Result<Piece> Decoder::DecodeTrack(SeqTrack track, std::size_t at,
                                   std::size_t limit) const {
  Piece piece;
  for (;;) {
    if (at == limit) {
      track.terminated = false;
      break;
    }
    piece.boundaries.push_back(at);
    if (MotionByte(at) == kEnd) {
      track.terminated = true;
      ++at;
      break;
    }
    const auto read = ReadCommand(motion_, at, limit);
    if (!read) {
      return FailMotion(at, "a command runs into the next element");
    }
    const auto canonical = EncodeCommand(read->command);
    if (!canonical ||
        !std::ranges::equal(*canonical, motion_.subspan(at, read->end - at))) {
      return FailMotion(at, "a command is not in the form the encoder writes");
    }
    track.commands.push_back(read->command);
    at = read->end;
  }
  piece.end = at;
  piece.element = std::move(track);
  return piece;
}

Result<Piece> Decoder::DecodeScale(std::size_t start, std::size_t limit) const {
  SeqTrack track;
  const bool packed = limit - start >= kPackedScaleKeysAt &&
                      MotionByte(start) == kPackedScaleTag &&
                      MotionByte(start + 1) == kPackedScaleVersion;
  const std::size_t bytes = packed ? kPackedScaleBytes : kRawScaleBytes;
  if (limit - start < bytes) {
    return FailMotion(start, "a scale key runs into the next element");
  }
  if (packed) {
    auto keys = ReadKeys(start + kPackedScaleKeysAt, false);
    if (!keys) {
      return std::unexpected(keys.error());
    }
    track.start = SeqStart::kPackedScale;
    track.initial = *keys;
  } else {
    track.start = SeqStart::kRawScale;
    for (std::size_t axis = 0; axis < kSeqAxes; ++axis) {
      track.initial[axis] = MotionByte(start + axis);
    }
  }
  return DecodeTrack(std::move(track), start + bytes, limit);
}

Result<Piece> Decoder::DecodeEvents(std::size_t start,
                                    std::size_t limit) const {
  SeqEvents events;
  std::size_t at = start;
  for (;;) {
    if (at >= limit) {
      return FailMotion(at, "an event script runs into the next element");
    }
    std::size_t frame = MotionByte(at++);
    if (frame == kLongFrame) {
      if (at >= limit) {
        return FailMotion(at, "an event script runs into the next element");
      }
      frame += MotionByte(at++);
    }
    if (at >= limit) {
      return FailMotion(at, "an event script runs into the next element");
    }
    SeqEvent event{.frame = static_cast<std::uint16_t>(frame),
                   .op = MotionByte(at),
                   .arguments = {}};
    if (!SeqEventHandled(event.op)) {
      events.events.push_back(std::move(event));
      return Piece{
          .end = at + 1, .element = std::move(events), .boundaries = {}};
    }
    const auto stride = EventStride(event.op, lengths_);
    if (!stride) {
      return FailMotion(at, stride.error());
    }
    if (limit - at < *stride) {
      return FailMotion(at, "an event runs into the next element");
    }
    for (std::size_t arg = 1; arg < *stride; ++arg) {
      event.arguments.push_back(MotionByte(at + arg));
    }
    events.events.push_back(std::move(event));
    at += *stride;
  }
}

Result<Piece> Decoder::DecodeHard(std::size_t start, Kind kind,
                                  std::size_t limit) const {
  switch (kind) {
    case Kind::kVector: {
      if (limit - start < kVectorBytes) {
        return FailMotion(start, "a pose key runs into the next element");
      }
      auto keys = ReadKeys(start, true);
      if (!keys) {
        return std::unexpected(keys.error());
      }
      SeqTrack track{.start = SeqStart::kVector,
                     .initial = *keys,
                     .commands = {},
                     .terminated = true};
      return DecodeTrack(std::move(track), start + kVectorBytes, limit);
    }
    case Kind::kScale:
      return DecodeScale(start, limit);
    case Kind::kEvents:
      return DecodeEvents(start, limit);
  }
  return FailMotion(start, "an unknown element kind");
}

Status Decoder::DecodeHardStarts() {
  for (auto it = hard_.begin(); it != hard_.end(); ++it) {
    const auto next = std::next(it);
    const std::size_t limit =
        next == hard_.end() ? motion_.size() : next->first;
    auto piece = DecodeHard(it->first, it->second, limit);
    if (!piece) {
      return std::unexpected(piece.error());
    }
    pieces_.emplace(it->first, std::move(*piece));
  }
  return {};
}

Result<Target> Decoder::Resolve(std::size_t offset) {
  if (offset >= motion_.size()) {
    return FailMotion(offset, "a track's commands start past the motion area");
  }
  auto after = pieces_.upper_bound(offset);
  if (after != pieces_.begin()) {
    const auto& [start, piece] = *std::prev(after);
    if (offset < piece.end) {
      const auto boundary = std::ranges::find(piece.boundaries, offset);
      if (boundary != piece.boundaries.end()) {
        return Target{.piece = start,
                      .command = static_cast<std::size_t>(
                          boundary - piece.boundaries.begin())};
      }
      if (MotionByte(offset) == kEnd) {
        return Target{};
      }
      return FailMotion(offset,
                        "a track's commands start inside another element");
    }
  }
  const std::size_t limit =
      after == pieces_.end() ? motion_.size() : after->first;
  auto piece = DecodeTrack(SeqTrack{}, offset, limit);
  if (!piece) {
    return std::unexpected(piece.error());
  }
  pieces_.emplace(offset, std::move(*piece));
  return Target{.piece = offset, .command = 0};
}

Status Decoder::ResolveCursors() {
  for (const std::size_t offset : cursors_) {
    auto target = Resolve(offset);
    if (!target) {
      return std::unexpected(target.error());
    }
    targets_.emplace(offset, *target);
  }
  return {};
}

void Decoder::FillGaps() {
  std::vector<std::pair<std::size_t, std::size_t>> gaps;
  std::size_t covered = 0;
  for (const auto& [start, piece] : pieces_) {
    if (start > covered) {
      gaps.emplace_back(covered, start);
    }
    covered = piece.end;
  }
  if (covered < motion_.size()) {
    gaps.emplace_back(covered, motion_.size());
  }
  for (const auto& [start, end] : gaps) {
    const auto bytes = motion_.subspan(start, end - start);
    pieces_.emplace(
        start,
        Piece{.end = end,
              .element = SeqUnread{.bytes = {bytes.begin(), bytes.end()}},
              .boundaries = {}});
  }
}

SeqRef Decoder::RefOf(std::size_t offset, Read read,
                      const std::map<std::size_t, std::size_t>& index) const {
  if (read != Read::kCommands) {
    return SeqOwn{.element = index.at(offset)};
  }
  const Target& target = targets_.at(offset);
  if (!target.piece) {
    return SeqEmpty{.hint = static_cast<std::uint16_t>(offset)};
  }
  return SeqEntry{.element = index.at(*target.piece),
                  .command = target.command};
}

SeqBank Decoder::Assemble() const {
  SeqBank bank;
  std::map<std::size_t, std::size_t> index;
  for (const auto& [start, piece] : pieces_) {
    index.emplace(start, bank.motion.size());
    bank.motion.push_back(piece.element);
  }
  bank.map = map_;
  for (const RawClip& raw : clips_) {
    const bool own = raw.base == kNoClip;
    SeqClip clip;
    clip.length = raw.length;
    if (!own) {
      clip.base = raw.base;
    }
    clip.flags = raw.flags;
    if (raw.events != 0) {
      clip.events = index.at(raw.events);
    }
    clip.root = RefOf(raw.root, KeyRead(own), index);
    if (raw.travel != 0) {
      clip.travel = index.at(raw.travel);
    }
    for (const std::uint16_t word : raw.rotations) {
      clip.rotations.push_back(RefOf(word, KeyRead(own), index));
    }
    const Read scale = ScaleRead(own, raw.flags);
    if (scale != Read::kNone) {
      clip.scales.emplace();
      for (const std::uint16_t word : raw.scales) {
        clip.scales->push_back(RefOf(word, scale, index));
      }
    }
    bank.clips.push_back(std::move(clip));
  }
  return bank;
}

Result<SeqBank> Decoder::Run() {
  if (auto layout = ReadLayout(); !layout) {
    return std::unexpected(layout.error());
  }
  for (const RawClip& clip : clips_) {
    if (auto collect = Collect(clip); !collect) {
      return std::unexpected(collect.error());
    }
  }
  if (auto hard = DecodeHardStarts(); !hard) {
    return std::unexpected(hard.error());
  }
  if (auto cursors = ResolveCursors(); !cursors) {
    return std::unexpected(cursors.error());
  }
  FillGaps();
  SeqBank bank = Assemble();
  if (auto reach = CheckPlayback(bank); !reach) {
    return std::unexpected(reach.error());
  }
  return bank;
}

// ---- Encoding ----

// Where an element landed: its start, and a track's command offsets, then
// its terminator's.
struct Placement {
  std::size_t start = 0;
  std::vector<std::size_t> boundaries;
};

class Encoder {
 public:
  Encoder(const SeqBank& bank, std::size_t bones, Lengths lengths)
      : bank_(bank), bones_(bones), lengths_(lengths) {}

  Result<std::vector<std::byte>> Run();

 private:
  Status CheckClip(std::size_t index) const;
  Status CheckElementRefs(const SeqClip& clip) const;
  Status CheckRef(const SeqRef& ref, Read read) const;
  [[nodiscard]] const SeqTrack* TrackAt(std::size_t element) const;
  Status Place(std::size_t element);
  Status PlaceTrack(const SeqTrack& track, Placement& placement);
  Status PlaceEvents(const SeqEvents& events);
  Result<std::uint16_t> OffsetOf(std::size_t start) const;
  Result<std::uint16_t> OffsetOf(const SeqRef& ref);
  Status PutClip(Writer& out, const SeqClip& clip);

  const SeqBank& bank_;
  std::size_t bones_;
  Lengths lengths_;
  Writer motion_;
  std::vector<Placement> placed_;
  std::optional<std::size_t> zero_;
};

const SeqTrack* Encoder::TrackAt(std::size_t element) const {
  if (element >= bank_.motion.size()) {
    return nullptr;
  }
  return std::get_if<SeqTrack>(&bank_.motion[element]);
}

Status Encoder::CheckRef(const SeqRef& ref, Read read) const {
  if (read == Read::kCommands) {
    if (std::holds_alternative<SeqEmpty>(ref)) {
      return {};
    }
    const auto* entry = std::get_if<SeqEntry>(&ref);
    const SeqTrack* track = entry ? TrackAt(entry->element) : nullptr;
    if (track == nullptr) {
      return std::unexpected(std::format(
          "reads {}, so it names a track's command or an empty track",
          ReadName(read)));
    }
    if (entry->command > track->commands.size() ||
        (entry->command == track->commands.size() && !track->terminated)) {
      return std::unexpected(
          std::format("command {} is past motion element {}'s commands",
                      entry->command, entry->element));
    }
    return {};
  }
  const auto* own = std::get_if<SeqOwn>(&ref);
  const SeqTrack* track = own ? TrackAt(own->element) : nullptr;
  const bool fits =
      track != nullptr &&
      (read == Read::kOwnVector ? track->start == SeqStart::kVector
                                : track->start == SeqStart::kRawScale ||
                                      track->start == SeqStart::kPackedScale);
  if (!fits) {
    return std::unexpected(std::format(
        "reads {}, so it names a track that starts with one", ReadName(read)));
  }
  return {};
}

Status Encoder::CheckElementRefs(const SeqClip& clip) const {
  if (clip.events &&
      (*clip.events >= bank_.motion.size() ||
       !std::holds_alternative<SeqEvents>(bank_.motion[*clip.events]))) {
    return std::unexpected(std::format(
        "its events name motion element {}, which is not an event script",
        *clip.events));
  }
  if (clip.travel) {
    if (auto travel =
            CheckRef(SeqOwn{.element = *clip.travel}, Read::kOwnVector);
        !travel) {
      return std::unexpected("its travel " + travel.error());
    }
  }
  return {};
}

Status Encoder::CheckClip(std::size_t index) const {
  const SeqClip& clip = bank_.clips[index];
  if ((clip.flags & ~kKnownFlags) != 0) {
    return std::unexpected(std::format(
        "flags 0x{:X} hold bits the game does not read", clip.flags));
  }
  if (clip.base && (*clip.base >= bank_.clips.size() ||
                    bank_.clips[*clip.base].base.has_value())) {
    return std::unexpected(
        std::format("base {} is not an own clip", *clip.base));
  }
  if (auto refs = CheckElementRefs(clip); !refs) {
    return refs;
  }
  const bool own = !clip.base;
  if (auto root = CheckRef(clip.root, KeyRead(own)); !root) {
    return std::unexpected("its root " + root.error());
  }
  if (clip.rotations.size() != bones_) {
    return std::unexpected(std::format("it has {} rotations for {} bones",
                                       clip.rotations.size(), bones_));
  }
  for (std::size_t bone = 0; bone < bones_; ++bone) {
    if (auto rotation = CheckRef(clip.rotations[bone], KeyRead(own));
        !rotation) {
      return std::unexpected(
          std::format("bone {}'s rotation {}", bone, rotation.error()));
    }
  }
  const Read scale = ScaleRead(own, clip.flags);
  if ((scale == Read::kNone) != !clip.scales) {
    return std::unexpected(
        std::string(clip.scales ? "it has scales, but its flags read none"
                                : "its flags read scales, but it has none"));
  }
  if (!clip.scales) {
    return {};
  }
  if (clip.scales->size() != bones_) {
    return std::unexpected(std::format("it has {} scales for {} bones",
                                       clip.scales->size(), bones_));
  }
  for (std::size_t bone = 0; bone < bones_; ++bone) {
    if (auto read = CheckRef((*clip.scales)[bone], scale); !read) {
      return std::unexpected(
          std::format("bone {}'s scale {}", bone, read.error()));
    }
  }
  return {};
}

Status PutKeys(Writer& out, const SeqTrack& track) {
  for (std::size_t axis = 0; axis < kSeqAxes; ++axis) {
    const int value = track.initial[axis];
    switch (track.start) {
      case SeqStart::kVector:
        if (value % 2 != 0) {
          return std::unexpected(
              std::format("the {} key {} is odd; the game's keys are even",
                          AxisName(axis), value));
        }
        out.PutBe16(static_cast<std::uint16_t>(value / 2));
        break;
      case SeqStart::kPackedScale:
        if (value < kPackedKeyMin || value > kPackedKeyMax) {
          return std::unexpected(
              std::format("the {} scale key {} is outside {} to {}",
                          AxisName(axis), value, kPackedKeyMin, kPackedKeyMax));
        }
        out.PutBe16(static_cast<std::uint16_t>(value));
        break;
      case SeqStart::kRawScale:
        if (value < 0 || value > kRawScaleMax) {
          return std::unexpected(
              std::format("the {} scale byte {} is outside 0 to {}",
                          AxisName(axis), value, kRawScaleMax));
        }
        out.PutByte(static_cast<std::uint8_t>(value));
        break;
      case SeqStart::kNone:
        if (value != 0) {
          return std::unexpected(
              std::string("a track without a start key keeps no initial "
                          "values"));
        }
        break;
    }
  }
  return {};
}

Status Encoder::PlaceTrack(const SeqTrack& track, Placement& placement) {
  if (track.start == SeqStart::kRawScale &&
      track.initial[0] == kPackedScaleTag &&
      track.initial[1] == kPackedScaleVersion) {
    return std::unexpected(
        std::string("raw scale bytes FE 01 would read as a packed scale"));
  }
  if (track.start == SeqStart::kPackedScale) {
    motion_.PutByte(kPackedScaleTag);
    motion_.PutByte(kPackedScaleVersion);
  }
  if (auto keys = PutKeys(motion_, track); !keys) {
    return keys;
  }
  for (std::size_t index = 0; index < track.commands.size(); ++index) {
    auto bytes = EncodeCommand(track.commands[index]);
    if (!bytes) {
      return std::unexpected(
          std::format("command {}: {}", index, bytes.error()));
    }
    placement.boundaries.push_back(motion_.Size());
    motion_.Append(*bytes);
  }
  if (track.terminated) {
    placement.boundaries.push_back(motion_.Size());
    motion_.PutByte(kEnd);
  }
  return {};
}

Status Encoder::PlaceEvents(const SeqEvents& events) {
  if (events.events.empty()) {
    return std::unexpected(std::string("an event script needs its end"));
  }
  for (std::size_t index = 0; index < events.events.size(); ++index) {
    const SeqEvent& event = events.events[index];
    const bool last = index + 1 == events.events.size();
    if (event.frame > kMaxEventFrame) {
      return std::unexpected(std::format("event {}'s frame {} is past {}",
                                         index, event.frame, kMaxEventFrame));
    }
    if (last && SeqEventHandled(event.op)) {
      return std::unexpected(std::format(
          "the last event's op {} does not end the script", event.op));
    }
    if (!last && !SeqEventHandled(event.op)) {
      return std::unexpected(
          std::format("event {}'s op {} ends the script before its last event",
                      index, event.op));
    }
    std::size_t stride = 1;
    if (!last) {
      const auto found = EventStride(event.op, lengths_);
      if (!found) {
        return std::unexpected(found.error());
      }
      stride = *found;
    }
    if (event.arguments.size() + 1 != stride) {
      return std::unexpected(
          std::format("event {} (op {}) has {} argument bytes, not {}", index,
                      event.op, event.arguments.size(), stride - 1));
    }
    if (event.frame >= kLongFrame) {
      motion_.PutByte(kLongFrame);
      motion_.PutByte(static_cast<std::uint8_t>(event.frame - kLongFrame));
    } else {
      motion_.PutByte(static_cast<std::uint8_t>(event.frame));
    }
    motion_.PutByte(event.op);
    for (const std::uint8_t argument : event.arguments) {
      motion_.PutByte(argument);
    }
  }
  return {};
}

Status Encoder::Place(std::size_t element) {
  Placement placement{.start = motion_.Size(), .boundaries = {}};
  Status placed;
  if (const auto* track = std::get_if<SeqTrack>(&bank_.motion[element])) {
    placed = PlaceTrack(*track, placement);
  } else if (const auto* events =
                 std::get_if<SeqEvents>(&bank_.motion[element])) {
    placed = PlaceEvents(*events);
  } else if (const auto& unread = std::get<SeqUnread>(bank_.motion[element]);
             unread.bytes.empty()) {
    placed = std::unexpected(std::string("an unread element holds no bytes"));
  } else {
    motion_.Append(unread.bytes);
  }
  if (!placed) {
    return std::unexpected(
        std::format("motion element {}: {}", element, placed.error()));
  }
  placed_.push_back(std::move(placement));
  return {};
}

Result<std::uint16_t> Encoder::OffsetOf(std::size_t start) const {
  if (start > kMaxMotionOffset) {
    return std::unexpected(std::format(
        "motion offset 0x{:X} is past the 0xFFFF a header word holds", start));
  }
  return static_cast<std::uint16_t>(start);
}

Result<std::uint16_t> Encoder::OffsetOf(const SeqRef& ref) {
  if (const auto* own = std::get_if<SeqOwn>(&ref)) {
    return OffsetOf(placed_[own->element].start);
  }
  if (const auto* entry = std::get_if<SeqEntry>(&ref)) {
    return OffsetOf(placed_[entry->element].boundaries[entry->command]);
  }
  const std::uint16_t hint = std::get<SeqEmpty>(ref).hint;
  const Bytes motion = motion_.View();
  if (hint < motion.size() && ByteAt(motion, hint) == kEnd) {
    return hint;
  }
  if (!zero_) {
    const auto found = std::ranges::find(motion, std::byte{kEnd});
    zero_ = static_cast<std::size_t>(found - motion.begin());
    if (found == motion.end()) {
      motion_.PutByte(kEnd);
    }
  }
  return OffsetOf(*zero_);
}

Status Encoder::PutClip(Writer& out, const SeqClip& clip) {
  out.PutLe16(clip.length);
  out.PutByte(clip.base.value_or(kNoClip));
  out.PutByte(clip.flags);
  std::vector<Result<std::uint16_t>> words;
  words.push_back(clip.events ? OffsetOf(placed_[*clip.events].start)
                              : Result<std::uint16_t>{0});
  words.push_back(OffsetOf(clip.root));
  words.push_back(clip.travel ? OffsetOf(placed_[*clip.travel].start)
                              : Result<std::uint16_t>{0});
  for (const SeqRef& rotation : clip.rotations) {
    words.push_back(OffsetOf(rotation));
  }
  for (std::size_t bone = 0; bone < bones_; ++bone) {
    words.push_back(clip.scales ? OffsetOf((*clip.scales)[bone])
                                : Result<std::uint16_t>{0});
  }
  if ((clip.events && words[0] == 0) || (clip.travel && words[2] == 0)) {
    return std::unexpected(std::string(
        "its events or travel lie at motion offset 0, which means none"));
  }
  for (const auto& word : words) {
    if (!word) {
      return std::unexpected(word.error());
    }
    out.PutLe16(*word);
  }
  return {};
}

Result<std::vector<std::byte>> Encoder::Run() {
  if (bones_ > kSeqMaxBones) {
    return std::unexpected(std::format(
        "{} bones is more than the {} a pose holds", bones_, kSeqMaxBones));
  }
  if (bank_.clips.size() > kMaxClips) {
    return std::unexpected(
        std::format("{} clips is more than {}", bank_.clips.size(), kMaxClips));
  }
  for (std::size_t index = 0; index < bank_.clips.size(); ++index) {
    if (auto clip = CheckClip(index); !clip) {
      return std::unexpected(std::format("clip {}: {}", index, clip.error()));
    }
  }
  for (const auto& clip : bank_.map) {
    if (clip && *clip >= bank_.clips.size()) {
      return std::unexpected(std::format("the map names clip {} of {}", *clip,
                                         bank_.clips.size()));
    }
  }
  if (auto reach = CheckPlayback(bank_); !reach) {
    return std::unexpected(reach.error());
  }
  for (std::size_t element = 0; element < bank_.motion.size(); ++element) {
    if (auto placed = Place(element); !placed) {
      return std::unexpected(placed.error());
    }
  }
  const std::size_t map_at =
      kBankHeaderBytes + bank_.clips.size() * ClipHeaderBytes(bones_);
  Writer out;
  out.PutLe32(static_cast<std::uint32_t>(map_at + bank_.map.size()));
  out.PutLe32(static_cast<std::uint32_t>(map_at));
  for (std::size_t index = 0; index < bank_.clips.size(); ++index) {
    if (auto clip = PutClip(out, bank_.clips[index]); !clip) {
      return std::unexpected(std::format("clip {}: {}", index, clip.error()));
    }
  }
  for (const auto& clip : bank_.map) {
    out.PutByte(clip.value_or(kNoClip));
  }
  out.Append(motion_.View());
  return out.Take();
}

Result<SeqBank> DecodeBank(Bytes bank, std::size_t bones, Lengths lengths,
                           std::size_t report_base, bool rebuild) {
  auto decoded = Decoder(bank, bones, lengths, report_base).Run();
  if (!decoded || !rebuild) {
    return decoded;
  }
  const auto rebuilt = Encoder(*decoded, bones, lengths).Run();
  if (!rebuilt) {
    return std::unexpected("the bank does not rebuild: " + rebuilt.error());
  }
  if (!std::ranges::equal(*rebuilt, bank)) {
    const auto mismatch = std::ranges::mismatch(*rebuilt, bank);
    return Fail(
        report_base + static_cast<std::size_t>(mismatch.in2 - bank.begin()),
        "the bank does not rebuild byte for byte");
  }
  return decoded;
}
}  // namespace

std::expected<SeqBank, std::string> DecodeSeqBank(
    std::span<const std::byte> bank, std::size_t bones,
    std::span<const std::uint8_t> event_lengths) {
  return DecodeBank(bank, bones, event_lengths, 0, true);
}

std::expected<std::vector<std::byte>, std::string> EncodeSeqBank(
    const SeqBank& bank, std::size_t bones,
    std::span<const std::uint8_t> event_lengths) {
  auto bytes = Encoder(bank, bones, event_lengths).Run();
  if (!bytes) {
    return bytes;
  }
  if (auto decoded = DecodeBank(*bytes, bones, event_lengths, 0, false);
      !decoded) {
    return std::unexpected("the encoded bank does not decode: " +
                           decoded.error());
  }
  return bytes;
}

std::expected<SeqFile, std::string> DecodeSeqFile(
    std::span<const std::byte> data,
    std::span<const std::uint8_t> event_lengths) {
  if (data.size() < kFileHeaderBytes) {
    return Fail(0, "the file is shorter than its 8-byte header");
  }
  if (ByteAt(data, kFilePadAt) != 0) {
    return Fail(kFilePadAt, "the byte after the map length is not zero");
  }
  SeqFile file;
  file.bones = ReadLe<std::uint16_t>(data, kFileBonesAt);
  const std::size_t size = ReadLe<std::uint32_t>(data, kFileSizeAt);
  const Bytes bank = data.subspan(kFileHeaderBytes);
  if (size != RoundUp(bank.size())) {
    return Fail(kFileSizeAt,
                std::format("the size field says {} bytes, but the {}-byte "
                            "bank rounds up to {}",
                            size, bank.size(), RoundUp(bank.size())));
  }
  auto decoded =
      DecodeBank(bank, file.bones, event_lengths, kFileHeaderBytes, true);
  if (!decoded) {
    return std::unexpected(decoded.error());
  }
  if (decoded->map.size() != ByteAt(data, 0)) {
    return Fail(0, std::format("the map length byte says {}, but the map "
                               "holds {}",
                               ByteAt(data, 0), decoded->map.size()));
  }
  file.bank = std::move(*decoded);
  return file;
}

std::expected<std::vector<std::byte>, std::string> EncodeSeqFile(
    const SeqFile& file, std::span<const std::uint8_t> event_lengths) {
  if (file.bank.map.size() > kMaxMapLength) {
    return std::unexpected(std::format("a map of {} entries is more than {}",
                                       file.bank.map.size(), kMaxMapLength));
  }
  auto bank = EncodeSeqBank(file.bank, file.bones, event_lengths);
  if (!bank) {
    return bank;
  }
  Writer out;
  out.PutByte(static_cast<std::uint8_t>(file.bank.map.size()));
  out.PutByte(0);
  out.PutLe16(file.bones);
  out.PutLe32(static_cast<std::uint32_t>(RoundUp(bank->size())));
  out.Append(*bank);
  return out.Take();
}

}  // namespace VS::Content
