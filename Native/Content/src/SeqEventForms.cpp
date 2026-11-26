// The named form of animation events (SeqEventForms.hpp).
#include "Content/SeqEventForms.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace VS::Content {
namespace {
using Json = JsonDocument;
template <typename T>
using Result = std::expected<T, std::string>;

constexpr std::int64_t kByteMax = std::numeric_limits<std::uint8_t>::max();
constexpr std::int64_t kWordMax = std::numeric_limits<std::uint16_t>::max();
constexpr std::int64_t kSignedByteMax = std::numeric_limits<std::int8_t>::max();
constexpr std::int64_t kSignedByteMin = -kSignedByteMax - 1;
// A vertex move packs a 9-bit vertex index under a 7-bit signed delta
// (AnimationEvents.cpp UnpackVertexMove).
constexpr unsigned kVertexIndexBits = 9;
constexpr std::uint16_t kVertexIndexMask = 0x1FF;
constexpr std::uint16_t kVertexDeltaMask = 0x7F;
constexpr std::int64_t kVertexDeltaMin = -64;
constexpr std::int64_t kVertexDeltaMax = 63;
constexpr unsigned kByteMask = 0xFF;
// frame, op and bytes, which every event may hold.
constexpr std::size_t kFixedMembers = 3;
// Besides them, the fields and a vertex move's vertex index.
constexpr std::size_t kMaxMembers = kFixedMembers + 1 + kSeqEventMaxFields;

constexpr SeqEventField Byte(std::string_view name) {
  return {.name = name, .kind = SeqEventFieldKind::kByte};
}
constexpr SeqEventField SignedByte(std::string_view name) {
  return {.name = name, .kind = SeqEventFieldKind::kSignedByte};
}
constexpr SeqEventField Word(std::string_view name) {
  return {.name = name, .kind = SeqEventFieldKind::kWord};
}
constexpr SeqEventField VertexMove(std::string_view name) {
  return {.name = name, .kind = SeqEventFieldKind::kVertexMove};
}

template <typename... Fields>
constexpr SeqEventForm Form(std::uint8_t op, std::string_view name,
                            Fields... fields) {
  static_assert(sizeof...(Fields) <= kSeqEventMaxFields);
  return {.op = op,
          .name = name,
          .fields = {fields...},
          .field_count = sizeof...(Fields),
          .skipped = false};
}

constexpr SeqEventForm Skipped(std::uint8_t op, std::string_view name) {
  return {
      .op = op, .name = name, .fields = {}, .field_count = 0, .skipped = true};
}

// Names and fields follow the handlers in Game/src/Actors/AnimationEvents.cpp
// (func_800AD714's event table); op 0 ends every script in the data.
constexpr std::array kForms{
    Form(0, "end"),
    Form(1, "initialize-pose"),
    Form(2, "impact"),
    Form(4, "play-sound-pair", Byte("sound")),
    Form(5, "play-actor-sound", Byte("sound"), Byte("volume")),
    Form(6, "stop-actor-sound", Byte("sound")),
    Form(7, "play-effect-sound", Byte("sound")),
    Form(8, "play-effect-sound-at-volume", Byte("sound"), Byte("volume")),
    Form(9, "stop-effect-sound", Byte("sound")),
    Form(10, "play-model-sound", Byte("sound")),
    Form(11, "play-surface-contact-sound"),
    Form(12, "start-effect-track", Byte("track")),
    Form(13, "restart-unless-advanced"),
    Form(14, "start-weapon-trail", Byte("bone")),
    Form(15, "fade-weapon-trails", Byte("frames")),
    Form(19, "play-animation-13", Byte("blend_frames")),
    Form(20, "play-animation-1", Byte("blend_frames")),
    Form(21, "play-animation", Byte("animation")),
    Form(22, "play-blended-animation", Byte("animation"), Byte("blend_frames")),
    Form(23, "previous-animation"),
    Form(24, "next-animation"),
    Form(25, "weapon-pose-on"),
    Form(26, "show-weapons", Byte("frames")),
    Form(27, "hide-weapons", Byte("frames")),
    Form(28, "gimmick-interaction", Byte("command")),
    Form(29, "palette-fade"),
    Form(30, "cell-sheet-0", Byte("overlay"), Byte("source_x"),
         Byte("source_y"), Byte("columns")),
    Form(31, "cell-sheet-1", Byte("overlay"), Byte("source_x"),
         Byte("source_y"), Byte("columns")),
    Form(32, "play-timeline-1"),
    Form(33, "stop-timeline-1"),
    Form(34, "play-timeline-0"),
    Form(35, "stop-timeline-0"),
    Form(36, "move-vertex-x", VertexMove("delta")),
    Form(37, "move-vertex-y", VertexMove("delta")),
    Form(38, "move-vertex-z", VertexMove("delta")),
    Form(39, "move-vertex", VertexMove("x"), SignedByte("y"), SignedByte("z")),
    Skipped(40, "ignored"),
    Skipped(41, "ignored-long"),
    Form(44, "texture-overlay", Byte("overlay"), Byte("source_x"),
         Byte("source_y")),
    Form(51, "shadow-expand", Word("rate"), Byte("frames")),
    Form(52, "shadow-shrink", Word("rate"), Byte("frames")),
    Form(53, "shadow-radius", Word("expand_rate"), Word("shrink_rate"),
         Byte("frames")),
    Form(54, "shadow-level", Word("level"), Byte("frames")),
    Form(55, "wait", Byte("frames")),
    Form(56, "unlock-bone", Byte("bone")),
    Form(57, "lock-bone", Byte("bone")),
    Form(58, "finish-action"),
    Form(59, "reset-movement"),
    Form(60, "shadow-point", Byte("point")),
    Form(61, "normal-speed"),
    Form(62, "fast-speed"),
    Form(63, "source-effect"),
    Form(64, "weapon-pose-off"),
};

constexpr std::size_t kNoForm = kForms.size();
constexpr std::size_t kOpCount = std::size_t{1} << CHAR_BIT;

constexpr std::array<std::size_t, kOpCount> BuildIndex() {
  std::array<std::size_t, kOpCount> index{};
  index.fill(kNoForm);
  for (std::size_t at = 0; at < kForms.size(); ++at) {
    index.at(kForms.at(at).op) = at;
  }
  return index;
}
constexpr std::array<std::size_t, kOpCount> kIndex = BuildIndex();

// The table names op 0 and exactly the ops the game runs, once each.
constexpr bool CoversHandledOps() {
  std::size_t named = 0;
  for (std::size_t op = 0; op < kOpCount; ++op) {
    const bool expected =
        op == 0 || SeqEventHandled(static_cast<std::uint8_t>(op));
    if ((kIndex.at(op) != kNoForm) != expected) return false;
    if (expected) ++named;
  }
  return named == kForms.size();
}
static_assert(CoversHandledOps());

constexpr std::size_t KindBytes(SeqEventFieldKind kind) {
  switch (kind) {
    case SeqEventFieldKind::kByte:
    case SeqEventFieldKind::kSignedByte:
      return 1;
    case SeqEventFieldKind::kWord:
    case SeqEventFieldKind::kVertexMove:
      return 2;
  }
  return 0;
}

std::uint16_t WordAt(const std::vector<std::uint8_t>& bytes, std::size_t at) {
  return static_cast<std::uint16_t>((bytes.at(at) << CHAR_BIT) |
                                    bytes.at(at + 1));
}

void PushWord(std::vector<std::uint8_t>& bytes, std::uint16_t word) {
  bytes.push_back(static_cast<std::uint8_t>(word >> CHAR_BIT));
  bytes.push_back(static_cast<std::uint8_t>(word & kByteMask));
}

void WriteFields(Json& json, const SeqEventForm& form,
                 const std::vector<std::uint8_t>& arguments) {
  std::size_t at = 0;
  for (const SeqEventField& field : form.Fields()) {
    const std::string name(field.name);
    switch (field.kind) {
      case SeqEventFieldKind::kByte:
        json[name] = arguments.at(at);
        break;
      case SeqEventFieldKind::kSignedByte:
        json[name] = static_cast<std::int8_t>(arguments.at(at));
        break;
      case SeqEventFieldKind::kWord:
        json[name] = WordAt(arguments, at);
        break;
      case SeqEventFieldKind::kVertexMove: {
        const std::uint16_t word = WordAt(arguments, at);
        json[std::string(kSeqEventVertexMember)] = word & kVertexIndexMask;
        json[name] = static_cast<std::int16_t>(word) >> kVertexIndexBits;
        break;
      }
    }
    at += KindBytes(field.kind);
  }
}

Result<std::int64_t> NumberIn(const Json& value, std::int64_t min,
                              std::int64_t max, const std::string& where) {
  const std::optional<std::int64_t> number = JsonWholeNumber(value);
  if (!number || *number < min || *number > max) {
    return std::unexpected(std::format(
        "{} must be a whole number from {} to {}", where, min, max));
  }
  return *number;
}

Result<void> ReadField(const JsonMembers& members, const SeqEventField& field,
                       std::vector<std::uint8_t>& arguments) {
  switch (field.kind) {
    case SeqEventFieldKind::kByte: {
      auto value = members.Integer(field.name, 0, kByteMax);
      if (!value) return std::unexpected(value.error());
      arguments.push_back(static_cast<std::uint8_t>(*value));
      return {};
    }
    case SeqEventFieldKind::kSignedByte: {
      auto value = members.Integer(field.name, kSignedByteMin, kSignedByteMax);
      if (!value) return std::unexpected(value.error());
      arguments.push_back(
          static_cast<std::uint8_t>(static_cast<std::int8_t>(*value)));
      return {};
    }
    case SeqEventFieldKind::kWord: {
      auto value = members.Integer(field.name, 0, kWordMax);
      if (!value) return std::unexpected(value.error());
      PushWord(arguments, static_cast<std::uint16_t>(*value));
      return {};
    }
    case SeqEventFieldKind::kVertexMove: {
      auto vertex = members.Integer(kSeqEventVertexMember, 0, kVertexIndexMask);
      if (!vertex) return std::unexpected(vertex.error());
      auto delta =
          members.Integer(field.name, kVertexDeltaMin, kVertexDeltaMax);
      if (!delta) return std::unexpected(delta.error());
      const auto bits = static_cast<std::uint16_t>(*delta) & kVertexDeltaMask;
      PushWord(arguments,
               static_cast<std::uint16_t>((bits << kVertexIndexBits) |
                                          static_cast<std::uint16_t>(*vertex)));
      return {};
    }
  }
  return {};
}

Result<const SeqEventForm*> OpOf(const JsonMembers& members, std::uint8_t& op) {
  auto value = members.Get("op");
  if (!value) return std::unexpected(value.error());
  const std::string where = members.Where("op");
  if ((*value)->is_string()) {
    const auto& name = (*value)->get_ref<const std::string&>();
    const SeqEventForm* form = FindSeqEventForm(name);
    if (form == nullptr) {
      return std::unexpected(
          std::format("{} names no event op: '{}'", where, name));
    }
    op = form->op;
    return form;
  }
  auto number = NumberIn(**value, 0, kByteMax, where);
  if (!number) {
    return std::unexpected(
        std::format("{} must be an op name or a whole number from 0 to {}",
                    where, kByteMax));
  }
  op = static_cast<std::uint8_t>(*number);
  if (const SeqEventForm* form = FindSeqEventForm(op); form != nullptr) {
    return std::unexpected(
        std::format("{} must be written as its name, '{}'", where, form->name));
  }
  return nullptr;
}

Result<std::vector<std::uint8_t>> BytesOf(const JsonMembers& members) {
  auto list = members.Get(kSeqEventBytesMember);
  if (!list) return std::unexpected(list.error());
  const std::string where = members.Where(kSeqEventBytesMember);
  if (!(*list)->is_array()) {
    return std::unexpected(std::format("{} must be an array", where));
  }
  std::vector<std::uint8_t> bytes;
  for (std::size_t index = 0; index < (*list)->size(); ++index) {
    auto value = NumberIn((**list)[index], 0, kByteMax,
                          std::format("{}[{}]", where, index));
    if (!value) return std::unexpected(value.error());
    bytes.push_back(static_cast<std::uint8_t>(*value));
  }
  return bytes;
}

}  // namespace

std::span<const SeqEventForm> SeqEventForms() noexcept { return kForms; }

const SeqEventForm* FindSeqEventForm(std::uint8_t op) noexcept {
  const std::size_t at = kIndex.at(op);
  return at == kNoForm ? nullptr : &kForms.at(at);
}

const SeqEventForm* FindSeqEventForm(std::string_view name) noexcept {
  const auto found = std::ranges::find(kForms, name, &SeqEventForm::name);
  return found == kForms.end() ? nullptr : &*found;
}

std::size_t SeqEventFieldBytes(const SeqEventForm& form) noexcept {
  std::size_t bytes = 0;
  for (const SeqEventField& field : form.Fields()) {
    bytes += KindBytes(field.kind);
  }
  return bytes;
}

Json SeqEventJson(const SeqEvent& event) {
  Json json = Json::object();
  json["frame"] = event.frame;
  const SeqEventForm* form = FindSeqEventForm(event.op);
  if (form == nullptr) {
    json["op"] = event.op;
  } else {
    json["op"] = std::string(form->name);
  }
  if (form != nullptr && !form->skipped &&
      event.arguments.size() == SeqEventFieldBytes(*form)) {
    WriteFields(json, *form, event.arguments);
  } else if (!event.arguments.empty() ||
             (form != nullptr && form->field_count > 0)) {
    json[std::string(kSeqEventBytesMember)] = event.arguments;
  }
  return json;
}

Result<SeqEvent> SeqEventOf(const Json& json, const std::string& where) {
  if (!json.is_object()) {
    return std::unexpected(
        std::format("{} must be an object with a frame and an op", where));
  }
  const JsonMembers members(json, where);
  SeqEvent event;
  auto frame = members.Integer("frame", 0, kWordMax);
  if (!frame) return std::unexpected(frame.error());
  event.frame = static_cast<std::uint16_t>(*frame);
  auto form = OpOf(members, event.op);
  if (!form) return std::unexpected(form.error());
  std::array<std::string_view, kMaxMembers> keys{"frame", "op",
                                                 kSeqEventBytesMember};
  std::size_t key_count = kFixedMembers;
  const std::span<const SeqEventField> fields =
      *form != nullptr ? (*form)->Fields() : std::span<const SeqEventField>{};
  for (const SeqEventField& field : fields) {
    if (field.kind == SeqEventFieldKind::kVertexMove) {
      keys.at(key_count++) = kSeqEventVertexMember;
    }
    keys.at(key_count++) = field.name;
  }
  if (auto only = members.Only(std::span(keys).first(key_count)); !only) {
    return std::unexpected(only.error());
  }
  if (members.Find(kSeqEventBytesMember) != nullptr) {
    for (std::size_t at = kFixedMembers; at < key_count; ++at) {
      if (members.Find(keys.at(at)) != nullptr) {
        return std::unexpected(std::format(
            "{} has both bytes and the named field '{}'", where, keys.at(at)));
      }
    }
    auto bytes = BytesOf(members);
    if (!bytes) return std::unexpected(bytes.error());
    event.arguments = std::move(*bytes);
    return event;
  }
  for (const SeqEventField& field : fields) {
    if (auto read = ReadField(members, field, event.arguments); !read) {
      return std::unexpected(read.error());
    }
  }
  return event;
}

}  // namespace VS::Content
