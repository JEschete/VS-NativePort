#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>

#include "Content/JsonText.hpp"
#include "Content/SeqFile.hpp"

// The named form of animation events (ASSET_PIPELINE.md §6.2.2, #520). An
// event script (SeqFile.hpp SeqEvents) is written as one object per event:
// its frame, its op by name and the op's arguments as named fields, read as
// the game's handlers read them (Game/src/Actors/AnimationEvents.cpp), e.g.
//   {"frame": 4, "op": "play-blended-animation", "animation": 5,
//    "blend_frames": 4}
// An op without a name is written as its number. Arguments that do not fit
// the op's fields, and the bytes the game skips (ops 40 and 41), are written
// as "bytes": [...]. Both forms rebuild the same bytes.
namespace VS::Content {

// How a field is stored in the event's argument bytes.
enum class SeqEventFieldKind : std::uint8_t {
  kByte,        // one byte, 0 to 255
  kSignedByte,  // one byte, -128 to 127
  kWord,        // a big-endian 16-bit word, 0 to 65535
  // A big-endian 16-bit word: its low 9 bits are the "vertex" member (0 to
  // 511) and its top 7 bits the field, a signed delta (-64 to 63).
  kVertexMove,
};

struct SeqEventField {
  std::string_view name;
  SeqEventFieldKind kind = SeqEventFieldKind::kByte;
};

inline constexpr std::size_t kSeqEventMaxFields = 4;
// The member holding a kVertexMove field's vertex index.
inline constexpr std::string_view kSeqEventVertexMember = "vertex";
// The member holding arguments written as bytes.
inline constexpr std::string_view kSeqEventBytesMember = "bytes";

struct SeqEventForm {
  std::uint8_t op = 0;
  std::string_view name;
  std::array<SeqEventField, kSeqEventMaxFields> fields{};
  std::size_t field_count = 0;
  // The game skips these arguments, so they are always written as bytes.
  bool skipped = false;

  [[nodiscard]] constexpr std::span<const SeqEventField> Fields() const {
    return std::span(fields).first(field_count);
  }
};

// Every named op in op order: op 0, "end", which ends every script in the
// game's data, and each op the game runs (SeqEventHandled).
[[nodiscard]] std::span<const SeqEventForm> SeqEventForms() noexcept;
[[nodiscard]] const SeqEventForm* FindSeqEventForm(std::uint8_t op) noexcept;
[[nodiscard]] const SeqEventForm* FindSeqEventForm(
    std::string_view name) noexcept;
// The argument bytes `form`'s fields take.
[[nodiscard]] std::size_t SeqEventFieldBytes(const SeqEventForm& form) noexcept;

// `event` in its named form.
[[nodiscard]] JsonDocument SeqEventJson(const SeqEvent& event);
// The event `json` holds; errors name the member at fault under `where`.
[[nodiscard]] std::expected<SeqEvent, std::string> SeqEventOf(
    const JsonDocument& json, const std::string& where);

}  // namespace VS::Content
