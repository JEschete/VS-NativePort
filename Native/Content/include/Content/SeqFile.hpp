#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

// The OBJ\*.SEQ animation bank and the bank every SHP embeds, decoded strictly
// for the asset pipeline (ASSET_PIPELINE.md §6.2, #205). A file is a count
// (the map's length), a zero byte, the bone count and the bank's size rounded
// up to a word, then the bank. A bank is the motion and map offsets, one clip
// header per clip, the map from animation index to clip, and the motion area
// the headers point into with halfword offsets.
//
// The motion area is decoded as the game reads it (AnimationPose.cpp,
// SequencePlayback.cpp, AnimationEvents.cpp): a pool of elements, each a track
// (an optional initial key and keyframe commands), an event script, or bytes
// nothing reads. Clips name elements instead of offsets, so a track can grow
// or shrink. Every command must be in the one form the encoder writes; the
// decoder rejects anything else with a message naming the offset, and a bank
// it accepts rebuilds byte for byte.
namespace VS::Content {

// The bones a bank can animate: the game's pose holds 41 besides the root
// (SequencePlayback.hpp kMaxPoseBones).
inline constexpr std::size_t kSeqMaxBones = 41;
inline constexpr std::size_t kSeqAxes = 3;
// Clip flags. An own clip with kSeqHasInitialScales starts each bone's scale
// from its scale track; kSeqHasScaleTracks plays the scale tracks' commands.
inline constexpr std::uint8_t kSeqHasInitialScales = 1;
inline constexpr std::uint8_t kSeqHasScaleTracks = 2;
// The event stride table the game reads (BATTLE.PRG D_800E9298), and where it
// sits in the file.
inline constexpr std::size_t kSeqEventLengthsAt = 0x80A98;
inline constexpr std::size_t kSeqEventLengthCount = 68;
// Event 41 skips this many bytes past its stride.
inline constexpr std::uint8_t kSeqLongIgnoredEvent = 41;
inline constexpr std::size_t kSeqLongIgnoredExtraBytes = 5;

struct SeqEventRange {
  std::uint8_t first = 0;
  std::uint8_t last = 0;
};
// The event opcodes the game runs (AnimationEvents.cpp EventOp); any other
// opcode ends the script.
[[nodiscard]] constexpr bool SeqEventHandled(std::uint8_t op) noexcept {
  constexpr std::array<SeqEventRange, 5> kHandled{
      {{1, 2}, {4, 15}, {19, 41}, {44, 44}, {51, 64}}};
  for (const SeqEventRange& range : kHandled) {
    if (op >= range.first && op <= range.last) {
      return true;
    }
  }
  return false;
}

// What a track starts with: nothing (only commands), a pose vector (root,
// rotation or travel key), three raw scale bytes, or a packed scale vector.
enum class SeqStart : std::uint8_t { kNone, kVector, kRawScale, kPackedScale };

// Adds `axes` (x, y, z; an absent axis keeps its previous delta) to the delta
// each of the next `frames` frames applies.
struct SeqCommand {
  std::uint16_t frames = 1;
  std::array<std::optional<std::int16_t>, kSeqAxes> axes{};

  bool operator==(const SeqCommand&) const = default;
};

struct SeqTrack {
  SeqStart start = SeqStart::kNone;
  // A vector holds the game's key values (rotation units of 8192 to a turn,
  // always even); a raw scale holds bytes; a packed scale holds the keys,
  // -16384 to 16383, where 64 is the unit scale.
  std::array<std::int16_t, kSeqAxes> initial{};
  std::vector<SeqCommand> commands;
  // Whether a zero byte follows the commands. A track without one runs into
  // the next element, which playback must never reach.
  bool terminated = true;

  bool operator==(const SeqTrack&) const = default;
};

// An event the script runs once its clip reaches `frame`. The last event of a
// script is the opcode that ends it, with no arguments.
struct SeqEvent {
  std::uint16_t frame = 0;
  std::uint8_t op = 0;
  std::vector<std::uint8_t> arguments;

  bool operator==(const SeqEvent&) const = default;
};

struct SeqEvents {
  std::vector<SeqEvent> events;

  bool operator==(const SeqEvents&) const = default;
};

// Bytes no clip reads.
struct SeqUnread {
  std::vector<std::byte> bytes;

  bool operator==(const SeqUnread&) const = default;
};

using SeqElement = std::variant<SeqTrack, SeqEvents, SeqUnread>;

// A track read from its start: its initial key, then its commands.
struct SeqOwn {
  std::size_t element = 0;

  bool operator==(const SeqOwn&) const = default;
};

// A track read from its commands alone: command `command` of track `element`,
// or its terminator when `command` is the command count.
struct SeqEntry {
  std::size_t element = 0;
  std::size_t command = 0;

  bool operator==(const SeqEntry&) const = default;
};

// Commands that end at once: any zero byte. `hint` is the motion offset the
// bank pointed at, which the encoder keeps while that byte is still zero.
struct SeqEmpty {
  std::uint16_t hint = 0;

  bool operator==(const SeqEmpty&) const = default;
};

using SeqRef = std::variant<SeqOwn, SeqEntry, SeqEmpty>;

// One clip. An own clip (no base) reads each key from its own tracks; a clip
// with a base takes the initial keys from that own clip's tracks and reads
// only commands from its own. Travel is always read from the playing clip.
struct SeqClip {
  std::uint16_t length = 1;
  std::optional<std::uint8_t> base;
  std::uint8_t flags = 0;
  std::optional<std::size_t> events;  // a SeqEvents element
  SeqRef root;
  std::optional<std::size_t> travel;  // a vector track
  std::vector<SeqRef> rotations;      // one per bone
  // One per bone when the clip reads them: an own clip with
  // kSeqHasInitialScales reads SeqOwn tracks, any clip with only
  // kSeqHasScaleTracks reads commands alone.
  std::optional<std::vector<SeqRef>> scales;

  bool operator==(const SeqClip&) const = default;
};

struct SeqBank {
  std::vector<SeqClip> clips;
  // Animation index to clip.
  std::vector<std::optional<std::uint8_t>> map;
  std::vector<SeqElement> motion;

  bool operator==(const SeqBank&) const = default;
};

struct SeqFile {
  std::uint16_t bones = 0;
  SeqBank bank;

  bool operator==(const SeqFile&) const = default;
};

// Decodes a bank for `bones` bones. `event_lengths` is the game's event stride
// table (kSeqEventLengthsAt in BATTLE.PRG).
[[nodiscard]] std::expected<SeqBank, std::string> DecodeSeqBank(
    std::span<const std::byte> bank, std::size_t bones,
    std::span<const std::uint8_t> event_lengths);
// The bank `bank` is. It decodes to an equivalent bank: the same clips, map,
// tracks and events, laid out in the same order. A bank DecodeSeqBank returned
// rebuilds exactly. Errors name what the bank cannot hold.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string> EncodeSeqBank(
    const SeqBank& bank, std::size_t bones,
    std::span<const std::uint8_t> event_lengths);

[[nodiscard]] std::expected<SeqFile, std::string> DecodeSeqFile(
    std::span<const std::byte> data,
    std::span<const std::uint8_t> event_lengths);
[[nodiscard]] std::expected<std::vector<std::byte>, std::string> EncodeSeqFile(
    const SeqFile& file, std::span<const std::uint8_t> event_lengths);

}  // namespace VS::Content
