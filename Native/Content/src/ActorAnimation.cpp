// The actor-animation form of SEQ banks (ActorAnimation.hpp).
#include "Content/ActorAnimation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace VS::Content {
namespace {
using Json = JsonDocument;
using Check = std::expected<void, std::string>;
template <typename T>
using Result = std::expected<T, std::string>;

// Each command is its frames, then a delta or null per axis.
constexpr std::size_t kCommandFields = 1 + kSeqAxes;
constexpr std::int64_t kByteMax = std::numeric_limits<std::uint8_t>::max();
constexpr std::int64_t kHalfwordMax = std::numeric_limits<std::uint16_t>::max();
constexpr std::int64_t kInt16Min = std::numeric_limits<std::int16_t>::min();
constexpr std::int64_t kInt16Max = std::numeric_limits<std::int16_t>::max();
// A packed scale key is a halved 16-bit value.
constexpr std::int64_t kPackedScaleMin = kInt16Min / 2;
constexpr std::int64_t kPackedScaleMax = kInt16Max / 2;
// 0xFF in a clip's base or the map means none, so a clip number stops below.
constexpr std::int64_t kClipNumberMax = kByteMax - 1;
// An event is its frame and op, then its arguments.
constexpr std::size_t kEventHeader = 2;
constexpr std::string_view kHexDigits = "0123456789abcdef";
constexpr unsigned kNibbleBits = 4;
constexpr unsigned kNibbleMask = 0x0F;
constexpr std::size_t kHexPerByte = 2;

struct Range {
  std::int64_t min = 0;
  std::int64_t max = 0;
};

// The member naming each initial key and the range of its values, by
// SeqStart.
struct StartForm {
  std::string_view key;
  Range range;
};
constexpr std::array<StartForm, 4> kStartForms{{
    {"", {}},
    {"vector", {kInt16Min, kInt16Max}},
    {"raw_scale", {0, kByteMax}},
    {"packed_scale", {kPackedScaleMin, kPackedScaleMax}},
}};

std::string At(std::string_view where, std::size_t index) {
  return std::format("{}[{}]", where, index);
}

Result<std::int64_t> NumberIn(const Json& value, Range range,
                              const std::string& where) {
  const std::optional<std::int64_t> number = JsonWholeNumber(value);
  if (!number || *number < range.min || *number > range.max) {
    return std::unexpected(
        std::format("{} must be a whole number from {} to {}", where, range.min,
                    range.max));
  }
  return *number;
}

Result<const Json*> ArrayMember(const JsonMembers& members,
                                std::string_view key) {
  auto value = members.Get(key);
  if (!value) return std::unexpected(value.error());
  if (!(*value)->is_array()) {
    return std::unexpected(
        std::format("{} must be an array", members.Where(key)));
  }
  return *value;
}

template <typename T>
Json OptionalJson(const std::optional<T>& value) {
  return value ? Json(*value) : Json(nullptr);
}

// --- Writing.

Json CommandsJson(const std::vector<SeqCommand>& commands) {
  Json flat = Json::array();
  for (const SeqCommand& command : commands) {
    flat.push_back(command.frames);
    for (const std::optional<std::int16_t>& axis : command.axes) {
      flat.push_back(OptionalJson(axis));
    }
  }
  return flat;
}

Json TrackJson(const SeqTrack& track) {
  Json json = Json::object();
  if (track.start != SeqStart::kNone) {
    json[std::string(
        kStartForms.at(static_cast<std::size_t>(track.start)).key)] =
        track.initial;
  }
  json["commands"] = CommandsJson(track.commands);
  if (!track.terminated) json["terminated"] = false;
  return json;
}

Json EventsJson(const SeqEvents& script) {
  Json events = Json::array();
  for (const SeqEvent& event : script.events) {
    Json entry = Json::array({event.frame, event.op});
    for (const std::uint8_t argument : event.arguments) {
      entry.push_back(argument);
    }
    events.push_back(std::move(entry));
  }
  Json json = Json::object();
  json["events"] = std::move(events);
  return json;
}

Json UnreadJson(const SeqUnread& unread) {
  std::string hex;
  hex.reserve(unread.bytes.size() * kHexPerByte);
  for (const std::byte value : unread.bytes) {
    const auto bits = std::to_integer<unsigned>(value);
    hex += kHexDigits[bits >> kNibbleBits];
    hex += kHexDigits[bits & kNibbleMask];
  }
  Json json = Json::object();
  json["unread"] = std::move(hex);
  return json;
}

Json ElementJson(const SeqElement& element) {
  if (const auto* track = std::get_if<SeqTrack>(&element)) {
    return TrackJson(*track);
  }
  if (const auto* events = std::get_if<SeqEvents>(&element)) {
    return EventsJson(*events);
  }
  return UnreadJson(std::get<SeqUnread>(element));
}

Json RefJson(const SeqRef& ref) {
  if (const auto* own = std::get_if<SeqOwn>(&ref)) return Json(own->element);
  Json json = Json::object();
  if (const auto* entry = std::get_if<SeqEntry>(&ref)) {
    json["track"] = entry->element;
    json["command"] = entry->command;
  } else {
    json["empty"] = std::get<SeqEmpty>(ref).hint;
  }
  return json;
}

Json RefsJson(const std::vector<SeqRef>& refs) {
  Json json = Json::array();
  for (const SeqRef& ref : refs) json.push_back(RefJson(ref));
  return json;
}

Json ClipJson(const SeqClip& clip) {
  Json json = Json::object();
  json["length"] = clip.length;
  json["base"] = OptionalJson(clip.base);
  json["flags"] = clip.flags;
  json["events"] = OptionalJson(clip.events);
  json["root"] = RefJson(clip.root);
  json["travel"] = OptionalJson(clip.travel);
  json["rotations"] = RefsJson(clip.rotations);
  json["scales"] = clip.scales ? RefsJson(*clip.scales) : Json(nullptr);
  return json;
}

// --- Reading.

Result<std::vector<SeqCommand>> CommandsOf(const JsonMembers& members) {
  auto value = ArrayMember(members, "commands");
  if (!value) return std::unexpected(value.error());
  const Json& flat = **value;
  const std::string where = members.Where("commands");
  if (flat.size() % kCommandFields != 0) {
    return std::unexpected(
        std::format("{} must hold frames, x, y, z for each command", where));
  }
  std::vector<SeqCommand> commands(flat.size() / kCommandFields);
  for (std::size_t index = 0; index < commands.size(); ++index) {
    const std::size_t at = index * kCommandFields;
    auto frames = NumberIn(flat[at], {1, kHalfwordMax}, At(where, at));
    if (!frames) return std::unexpected(frames.error());
    commands[index].frames = static_cast<std::uint16_t>(*frames);
    for (std::size_t axis = 0; axis < kSeqAxes; ++axis) {
      const Json& delta = flat[at + 1 + axis];
      if (delta.is_null()) continue;
      auto number =
          NumberIn(delta, {kInt16Min, kInt16Max}, At(where, at + 1 + axis));
      if (!number) return std::unexpected(number.error());
      commands[index].axes.at(axis) = static_cast<std::int16_t>(*number);
    }
  }
  return commands;
}

Check ReadInitial(const JsonMembers& members, const std::string& where_track,
                  SeqTrack& track) {
  std::size_t starts = 0;
  for (std::size_t form = 1; form < kStartForms.size(); ++form) {
    if (members.Find(kStartForms.at(form).key) != nullptr) {
      track.start = static_cast<SeqStart>(form);
      ++starts;
    }
  }
  if (starts > 1) {
    return std::unexpected(
        std::format("{} has more than one initial key", where_track));
  }
  if (track.start == SeqStart::kNone) return {};
  const StartForm& form = kStartForms.at(static_cast<std::size_t>(track.start));
  const Json& initial = *members.Find(form.key);
  const std::string where = members.Where(form.key);
  if (!initial.is_array() || initial.size() != kSeqAxes) {
    return std::unexpected(
        std::format("{} must be an array of {} numbers", where, kSeqAxes));
  }
  for (std::size_t axis = 0; axis < kSeqAxes; ++axis) {
    auto number = NumberIn(initial[axis], form.range, At(where, axis));
    if (!number) return std::unexpected(number.error());
    track.initial.at(axis) = static_cast<std::int16_t>(*number);
  }
  return {};
}

Result<SeqTrack> TrackOf(const JsonMembers& members, const std::string& where) {
  if (auto only = members.Only(
          {"vector", "raw_scale", "packed_scale", "commands", "terminated"});
      !only) {
    return std::unexpected(only.error());
  }
  SeqTrack track;
  if (auto initial = ReadInitial(members, where, track); !initial) {
    return std::unexpected(initial.error());
  }
  auto commands = CommandsOf(members);
  if (!commands) return std::unexpected(commands.error());
  track.commands = std::move(*commands);
  if (members.Find("terminated") != nullptr) {
    auto terminated = members.Boolean("terminated");
    if (!terminated) return std::unexpected(terminated.error());
    track.terminated = *terminated;
  }
  return track;
}

Result<SeqEvent> EventOf(const Json& json, const std::string& where) {
  if (!json.is_array() || json.size() < kEventHeader) {
    return std::unexpected(std::format(
        "{} must be an array of a frame, an op and its arguments", where));
  }
  SeqEvent event;
  auto frame = NumberIn(json[0], {0, kHalfwordMax}, At(where, 0));
  if (!frame) return std::unexpected(frame.error());
  event.frame = static_cast<std::uint16_t>(*frame);
  auto op = NumberIn(json[1], {0, kByteMax}, At(where, 1));
  if (!op) return std::unexpected(op.error());
  event.op = static_cast<std::uint8_t>(*op);
  for (std::size_t index = kEventHeader; index < json.size(); ++index) {
    auto argument = NumberIn(json[index], {0, kByteMax}, At(where, index));
    if (!argument) return std::unexpected(argument.error());
    event.arguments.push_back(static_cast<std::uint8_t>(*argument));
  }
  return event;
}

Result<SeqEvents> EventsOf(const JsonMembers& members) {
  if (auto only = members.Only({"events"}); !only) {
    return std::unexpected(only.error());
  }
  auto list = ArrayMember(members, "events");
  if (!list) return std::unexpected(list.error());
  SeqEvents script;
  const std::string where = members.Where("events");
  for (std::size_t index = 0; index < (*list)->size(); ++index) {
    auto event = EventOf((**list)[index], At(where, index));
    if (!event) return std::unexpected(event.error());
    script.events.push_back(std::move(*event));
  }
  return script;
}

std::optional<unsigned> HexDigit(char digit) {
  const std::size_t at = kHexDigits.find(digit);
  if (at == std::string_view::npos) return std::nullopt;
  return static_cast<unsigned>(at);
}

Result<SeqUnread> UnreadOf(const JsonMembers& members) {
  if (auto only = members.Only({"unread"}); !only) {
    return std::unexpected(only.error());
  }
  auto hex = members.String("unread");
  if (!hex) return std::unexpected(hex.error());
  const auto fail = [&] {
    return std::unexpected(std::format(
        "{} must be bytes in lowercase hexadecimal", members.Where("unread")));
  };
  if (hex->empty() || hex->size() % kHexPerByte != 0) return fail();
  SeqUnread unread;
  for (std::size_t at = 0; at < hex->size(); at += kHexPerByte) {
    const std::optional<unsigned> high = HexDigit((*hex)[at]);
    const std::optional<unsigned> low = HexDigit((*hex)[at + 1]);
    if (!high || !low) return fail();
    unread.bytes.push_back(
        static_cast<std::byte>((*high << kNibbleBits) | *low));
  }
  return unread;
}

Result<SeqElement> ElementOf(const Json& json, const std::string& where) {
  if (!json.is_object()) {
    return std::unexpected(std::format("{} must be an object", where));
  }
  const JsonMembers members(json, where);
  if (json.contains("events")) return EventsOf(members);
  if (json.contains("unread")) return UnreadOf(members);
  return TrackOf(members, where);
}

Result<std::size_t> IndexOf(const Json& json, const std::string& where) {
  auto number = NumberIn(json, {0, kHalfwordMax}, where);
  if (!number) return std::unexpected(number.error());
  return static_cast<std::size_t>(*number);
}

Result<SeqRef> RefOf(const Json& json, const std::string& where) {
  if (!json.is_object()) {
    auto element = IndexOf(json, where);
    if (!element) return std::unexpected(element.error());
    return SeqOwn{*element};
  }
  const JsonMembers members(json, where);
  if (json.contains("empty")) {
    if (auto only = members.Only({"empty"}); !only) {
      return std::unexpected(only.error());
    }
    auto hint = members.Integer("empty", 0, kHalfwordMax);
    if (!hint) return std::unexpected(hint.error());
    return SeqEmpty{static_cast<std::uint16_t>(*hint)};
  }
  if (auto only = members.Only({"track", "command"}); !only) {
    return std::unexpected(only.error());
  }
  auto element = members.Integer("track", 0, kHalfwordMax);
  if (!element) return std::unexpected(element.error());
  auto command = members.Integer("command", 0, kHalfwordMax);
  if (!command) return std::unexpected(command.error());
  return SeqEntry{static_cast<std::size_t>(*element),
                  static_cast<std::size_t>(*command)};
}

Result<std::vector<SeqRef>> RefsOf(const Json& json, const std::string& where) {
  if (!json.is_array()) {
    return std::unexpected(std::format("{} must be an array", where));
  }
  std::vector<SeqRef> refs;
  refs.reserve(json.size());
  for (std::size_t index = 0; index < json.size(); ++index) {
    auto ref = RefOf(json[index], At(where, index));
    if (!ref) return std::unexpected(ref.error());
    refs.push_back(*ref);
  }
  return refs;
}

// A member that is null or a number from `range`.
Result<std::optional<std::int64_t>> NullOr(const JsonMembers& members,
                                           std::string_view key, Range range) {
  auto value = members.Get(key);
  if (!value) return std::unexpected(value.error());
  if ((*value)->is_null()) return std::optional<std::int64_t>{};
  auto number = NumberIn(**value, range, members.Where(key));
  if (!number) {
    return std::unexpected(std::format("{} or null", number.error()));
  }
  return std::optional<std::int64_t>(*number);
}

std::optional<std::size_t> AsIndex(const std::optional<std::int64_t>& value) {
  if (!value) return std::nullopt;
  return static_cast<std::size_t>(*value);
}

Check ReadClipNumbers(const JsonMembers& members, SeqClip& clip) {
  auto length = members.Integer("length", 0, kHalfwordMax);
  if (!length) return std::unexpected(length.error());
  clip.length = static_cast<std::uint16_t>(*length);
  auto base = NullOr(members, "base", {0, kClipNumberMax});
  if (!base) return std::unexpected(base.error());
  if (*base) clip.base = static_cast<std::uint8_t>(**base);
  auto flags = members.Integer("flags", 0, kByteMax);
  if (!flags) return std::unexpected(flags.error());
  clip.flags = static_cast<std::uint8_t>(*flags);
  auto events = NullOr(members, "events", {0, kHalfwordMax});
  if (!events) return std::unexpected(events.error());
  clip.events = AsIndex(*events);
  auto travel = NullOr(members, "travel", {0, kHalfwordMax});
  if (!travel) return std::unexpected(travel.error());
  clip.travel = AsIndex(*travel);
  return {};
}

Result<SeqClip> ClipOf(const Json& json, const std::string& where) {
  const JsonMembers members(json, where);
  if (auto only = members.Only({"length", "base", "flags", "events", "root",
                                "travel", "rotations", "scales"});
      !only) {
    return std::unexpected(only.error());
  }
  SeqClip clip;
  if (auto numbers = ReadClipNumbers(members, clip); !numbers) {
    return std::unexpected(numbers.error());
  }
  auto root = members.Get("root").and_then(
      [&](const Json* value) { return RefOf(*value, members.Where("root")); });
  if (!root) return std::unexpected(root.error());
  clip.root = *root;
  auto rotations = members.Get("rotations").and_then([&](const Json* value) {
    return RefsOf(*value, members.Where("rotations"));
  });
  if (!rotations) return std::unexpected(rotations.error());
  clip.rotations = std::move(*rotations);
  auto scales = members.Get("scales");
  if (!scales) return std::unexpected(scales.error());
  if (!(*scales)->is_null()) {
    auto refs = RefsOf(**scales, members.Where("scales"));
    if (!refs) {
      return std::unexpected(std::format("{} or null", refs.error()));
    }
    clip.scales = std::move(*refs);
  }
  return clip;
}

Result<std::vector<std::optional<std::uint8_t>>> MapOf(
    const JsonMembers& members) {
  auto list = ArrayMember(members, "map");
  if (!list) return std::unexpected(list.error());
  std::vector<std::optional<std::uint8_t>> map;
  const std::string where = members.Where("map");
  for (std::size_t index = 0; index < (*list)->size(); ++index) {
    const Json& entry = (**list)[index];
    if (entry.is_null()) {
      map.emplace_back();
      continue;
    }
    auto clip = NumberIn(entry, {0, kClipNumberMax}, At(where, index));
    if (!clip) return std::unexpected(std::format("{} or null", clip.error()));
    map.emplace_back(static_cast<std::uint8_t>(*clip));
  }
  return map;
}

Check ReadClips(const JsonMembers& members, SeqBank& bank) {
  auto clips = ArrayMember(members, "clips");
  if (!clips) return std::unexpected(clips.error());
  const std::string where = members.Where("clips");
  for (std::size_t index = 0; index < (*clips)->size(); ++index) {
    auto clip = ClipOf((**clips)[index], At(where, index));
    if (!clip) return std::unexpected(clip.error());
    bank.clips.push_back(std::move(*clip));
  }
  return {};
}

Check ReadMotion(const JsonMembers& members, SeqBank& bank) {
  auto motion = ArrayMember(members, "motion");
  if (!motion) return std::unexpected(motion.error());
  const std::string where = members.Where("motion");
  bank.motion.reserve((*motion)->size());
  for (std::size_t index = 0; index < (*motion)->size(); ++index) {
    auto element = ElementOf((**motion)[index], At(where, index));
    if (!element) return std::unexpected(element.error());
    bank.motion.push_back(std::move(*element));
  }
  return {};
}

}  // namespace

JsonDocument SeqBankJson(const SeqBank& bank) {
  Json clips = Json::array();
  for (const SeqClip& clip : bank.clips) clips.push_back(ClipJson(clip));
  Json map = Json::array();
  for (const std::optional<std::uint8_t>& clip : bank.map) {
    map.push_back(OptionalJson(clip));
  }
  Json motion = Json::array();
  for (const SeqElement& element : bank.motion) {
    motion.push_back(ElementJson(element));
  }
  Json json = Json::object();
  json["clips"] = std::move(clips);
  json["map"] = std::move(map);
  json["motion"] = std::move(motion);
  return json;
}

std::expected<SeqBank, std::string> SeqBankOf(const JsonDocument& json,
                                              std::string_view where) {
  const JsonMembers members(json, std::string(where));
  if (auto only = members.Only({"clips", "map", "motion"}); !only) {
    return std::unexpected(only.error());
  }
  SeqBank bank;
  if (auto clips = ReadClips(members, bank); !clips) {
    return std::unexpected(clips.error());
  }
  auto map = MapOf(members);
  if (!map) return std::unexpected(map.error());
  bank.map = std::move(*map);
  if (auto motion = ReadMotion(members, bank); !motion) {
    return std::unexpected(motion.error());
  }
  return bank;
}

JsonDocument ActorAnimationJson(const ActorAnimationDocument& document) {
  Json json = Json::object();
  json["schema"] = kActorAnimationSchema;
  json["schema_version"] = kActorAnimationSchemaVersion;
  json["source"] = document.source;
  json["source_sha256"] = document.source_sha256;
  json["bones"] = document.file.bones;
  json["bank"] = SeqBankJson(document.file.bank);
  return json;
}

std::expected<ActorAnimationDocument, std::string> ActorAnimationOf(
    const JsonDocument& json, std::string_view where) {
  const JsonMembers members(json, std::string(where));
  if (auto only = members.Only({"schema", "schema_version", "source",
                                "source_sha256", "bones", "bank"});
      !only) {
    return std::unexpected(only.error());
  }
  auto source = JsonSourceOf(members, where, kActorAnimationSchema,
                             kActorAnimationSchemaVersion);
  if (!source) return std::unexpected(source.error());
  ActorAnimationDocument document;
  document.source = std::move(source->source);
  document.source_sha256 = std::move(source->source_sha256);
  auto bones =
      members.Integer("bones", 1, static_cast<std::int64_t>(kSeqMaxBones));
  if (!bones) return std::unexpected(bones.error());
  document.file.bones = static_cast<std::uint16_t>(*bones);
  auto bank = members.Get("bank").and_then([&](const Json* value) {
    return SeqBankOf(*value, members.Where("bank"));
  });
  if (!bank) return std::unexpected(bank.error());
  document.file.bank = std::move(*bank);
  return document;
}

}  // namespace VS::Content
