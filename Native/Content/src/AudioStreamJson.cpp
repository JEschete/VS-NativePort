// The vs.audio-stream document (AudioStreamJson.hpp).
#include "Content/AudioStreamJson.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace VS::Content {
namespace {

template <class T>
using Result = std::expected<T, std::string>;

constexpr std::int64_t kWordLimit = std::numeric_limits<std::uint32_t>::max();
constexpr std::size_t kIdDigits = 16;
constexpr std::uint32_t kBlockFrames{PsxAdpcmDecoder::kBlockFrames};
// A stereo loop starts a whole chunk of each channel.
constexpr auto kStereoLoopFrames = static_cast<std::uint32_t>(
    kAudioStreamChunkBytes / PsxAdpcmDecoder::kBlockBytes * kBlockFrames);
constexpr std::string_view kVersionMember = "version";
constexpr std::array<std::string_view, 10> kStreamMembers{
    "schema",      "schema_version", "id",     "channels",   "frames",
    "sample_rate", "spu_pitch",      "repeat", "loop_frame", "start_page",
};

bool IsStreamId(std::string_view id) {
  return id.size() == kIdDigits && std::ranges::all_of(id, [](char digit) {
           return (digit >= '0' && digit <= '9') ||
                  (digit >= 'a' && digit <= 'f');
         });
}

// A whole-number member from `min` to `max` and where its value goes.
struct WordMember {
  std::string_view key;
  std::int64_t min = 0;
  std::int64_t max = 0;
  std::uint32_t* value = nullptr;
};

// The members' values, before checking that a header holds them together.
Result<AudioStreamInfo> MembersOf(const JsonMembers& members) {
  AudioStreamInfo info;
  auto id = members.String("id");
  if (!id) return std::unexpected(id.error());
  if (!IsStreamId(*id)) {
    return std::unexpected(std::format(
        "{} must be 16 lowercase hexadecimal digits", members.Where("id")));
  }
  info.id = std::move(*id);
  auto repeat = members.Boolean("repeat");
  if (!repeat) return std::unexpected(repeat.error());
  info.repeat = *repeat;
  const std::array<WordMember, 6> words{{
      {"channels", 1, 2, &info.channels},
      {"frames", 1, kWordLimit, &info.frames},
      {"sample_rate", 0, kWordLimit, &info.sample_rate},
      {"spu_pitch", 1, kAudioStreamMaximumPitch, &info.source_pitch},
      {"loop_frame", 0, kWordLimit, &info.loop_frame},
      {"start_page", 0, kWordLimit, &info.start_page},
  }};
  for (const WordMember& word : words) {
    auto value = members.Integer(word.key, word.min, word.max);
    if (!value) return std::unexpected(value.error());
    *word.value = static_cast<std::uint32_t>(*value);
  }
  return info;
}

// Nothing when a stream header holds `info`'s values together.
Result<void> CheckHeld(const JsonMembers& members, std::string_view where,
                       const AudioStreamInfo& info) {
  if (info.frames % kBlockFrames != 0) {
    return std::unexpected(
        std::format("{} must be a whole number of {}-frame ADPCM blocks",
                    members.Where("frames"), kBlockFrames));
  }
  const std::uint32_t alignment =
      info.channels == 2 ? kStereoLoopFrames : kBlockFrames;
  if (info.repeat
          ? info.loop_frame >= info.frames || info.loop_frame % alignment != 0
          : info.loop_frame != 0) {
    return std::unexpected(
        std::format("{} must be 0 for a stream that does not repeat, and "
                    "otherwise before frames and a multiple of {}",
                    members.Where("loop_frame"), alignment));
  }
  auto read = ReadAudioStreamHeader(AudioStreamHeaderOf(info));
  if (!read) {
    return std::unexpected(std::format("{}: {}", where, read.error()));
  }
  read->id = info.id;
  if (read->sample_rate != info.sample_rate) {
    return std::unexpected(std::format(
        "{} must be {}, spu_pitch * 44100 / 4096 rounded to the nearest hertz",
        members.Where("sample_rate"), read->sample_rate));
  }
  if (*read != info) {
    return std::unexpected(
        std::format("{} does not read back from its header", where));
  }
  return {};
}

}  // namespace

JsonDocument AudioStreamJson(const AudioStreamInfo& info) {
  JsonDocument json = JsonDocument::object();
  json["schema"] = std::string(kAudioStreamSchema);
  json["schema_version"] = kAudioStreamSchemaVersion;
  json["id"] = info.id;
  json["channels"] = info.channels;
  json["frames"] = info.frames;
  json["sample_rate"] = info.sample_rate;
  json["spu_pitch"] = info.source_pitch;
  json["repeat"] = info.repeat;
  json["loop_frame"] = info.loop_frame;
  json["start_page"] = info.start_page;
  return json;
}

std::expected<AudioStreamInfo, std::string> AudioStreamInfoOf(
    const JsonDocument& json, std::string_view where) {
  if (!json.is_object()) {
    return std::unexpected(std::format("{} must be an object", where));
  }
  const JsonMembers members(json, std::string(where));
  if (members.Find(kVersionMember) != nullptr &&
      members.Find("schema") == nullptr) {
    return std::unexpected(
        std::format("{} is a version 1 audio stream; run VS.Extract authoring "
                    "<datadir> to name its header",
                    where));
  }
  if (auto only = members.Only(kStreamMembers); !only) {
    return std::unexpected(only.error());
  }
  if (auto checked = CheckJsonSchema(members, where, kAudioStreamSchema,
                                     kAudioStreamSchemaVersion);
      !checked) {
    return std::unexpected(checked.error());
  }
  auto info = MembersOf(members);
  if (!info) return info;
  if (auto held = CheckHeld(members, where, *info); !held) {
    return std::unexpected(held.error());
  }
  return info;
}

}  // namespace VS::Content
