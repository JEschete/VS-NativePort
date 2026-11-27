// The vs.audio-stream upgrade (AudioStreamAuthoring.hpp).
#include "Extract/AudioStreamAuthoring.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "Content/AudioStream.hpp"
#include "Content/AudioStreamJson.hpp"
#include "Content/JsonText.hpp"
#include "Extract/AnimationAuthoring.hpp"
#include "Platform/Paths.hpp"

namespace VS::Extract {
namespace {
namespace fs = std::filesystem;
using Json = Content::JsonDocument;
template <class T>
using Result = std::expected<T, std::string>;
using Header = std::array<std::uint8_t, Content::kAudioStreamHeaderBytes>;

constexpr std::string_view kVersion = "version";
constexpr std::string_view kSourceHeader = "source_header";
constexpr std::string_view kSourcePitch = "source_pitch";
constexpr std::int64_t kByteMax = 0xFF;
// The version 1 members the named form keeps under the same name.
constexpr std::array<std::string_view, 5> kKeptMembers{
    "channels", "frames", "sample_rate", "repeat", "loop_frame",
};

// The 64 bytes version 1 listed as source_header.
Result<Header> HeaderOf(const Content::JsonMembers& members) {
  auto list = members.Get(kSourceHeader);
  if (!list) return std::unexpected(list.error());
  const Json& bytes = **list;
  if (!bytes.is_array() || bytes.size() != Content::kAudioStreamHeaderBytes) {
    return std::unexpected(std::format("{} must list the {} header bytes",
                                       members.Where(kSourceHeader),
                                       Content::kAudioStreamHeaderBytes));
  }
  Header header{};
  for (std::size_t index = 0; index < header.size(); ++index) {
    const auto value = Content::JsonWholeNumber(bytes[index]);
    if (!value || *value < 0 || *value > kByteMax) {
      return std::unexpected(std::format("{}[{}] must be a byte",
                                         members.Where(kSourceHeader), index));
    }
    header[index] = static_cast<std::uint8_t>(*value);
  }
  return header;
}

// Nothing when each value version 1 named is the one `named` holds.
Result<void> CheckNamedValues(const Content::JsonMembers& members,
                              const Json& named) {
  for (const std::string_view key : kKeptMembers) {
    auto value = members.Get(key);
    if (!value) return std::unexpected(value.error());
    if (**value != named.at(std::string(key))) {
      return std::unexpected(std::format("{} is not the value its {} holds",
                                         members.Where(key), kSourceHeader));
    }
  }
  auto pitch = members.Get(kSourcePitch);
  if (!pitch) return std::unexpected(pitch.error());
  if (**pitch != named.at("spu_pitch")) {
    return std::unexpected(std::format("{} is not the value its {} holds",
                                       members.Where(kSourcePitch),
                                       kSourceHeader));
  }
  return {};
}

// The stream a version 1 stream.json in directory `id` describes, read from
// its source_header as the player reads it.
Result<Content::AudioStreamInfo> NumberedStreamOf(const Json& json,
                                                  const std::string& where,
                                                  std::string_view id) {
  const Content::JsonMembers members(json, where);
  if (auto only =
          members.Only({kVersion, "id", "channels", "frames", "sample_rate",
                        kSourcePitch, "repeat", "loop_frame", kSourceHeader});
      !only) {
    return std::unexpected(only.error());
  }
  const auto* version = members.Find(kVersion);
  if (version == nullptr ||
      Content::JsonWholeNumber(*version) != kNumberedStreamVersion) {
    return std::unexpected(
        std::format("{} must be {} for a stream without a schema",
                    members.Where(kVersion), kNumberedStreamVersion));
  }
  auto header = HeaderOf(members);
  if (!header) return std::unexpected(header.error());
  auto info = Content::ReadAudioStreamHeader(*header);
  if (!info) {
    return std::unexpected(
        std::format("{}: {}", members.Where(kSourceHeader), info.error()));
  }
  auto named_id = members.String("id");
  if (!named_id) return std::unexpected(named_id.error());
  if (*named_id != id) {
    return std::unexpected(std::format("{} must be its directory name {}",
                                       members.Where("id"), id));
  }
  info->id = std::move(*named_id);
  if (auto named = CheckNamedValues(members, Content::AudioStreamJson(*info));
      !named) {
    return std::unexpected(named.error());
  }
  if (Content::AudioStreamHeaderOf(*info) != *header) {
    return std::unexpected(
        std::format("{} does not rebuild from the values it holds", where));
  }
  return info;
}

// Moves the stream.json `json` in directory `id` holds to schema
// vs.audio-stream when version 1 wrote it; whether it did. A current file is
// checked and left as it is.
Result<bool> NameStream(Json& json, const std::string& where,
                        std::string_view id) {
  if (!json.is_object()) {
    return std::unexpected(std::format("{} must be an object", where));
  }
  if (json.contains("schema")) {
    auto current = Content::AudioStreamInfoOf(json, where);
    if (!current) return std::unexpected(current.error());
    if (current->id != id) {
      return std::unexpected(
          std::format("{}.id must be its directory name {}", where, id));
    }
    return false;
  }
  auto info = NumberedStreamOf(json, where, id);
  if (!info) return std::unexpected(info.error());
  Json named = LabeledJson(json, Content::AudioStreamJson(*info));
  auto read = Content::AudioStreamInfoOf(named, where);
  if (!read) return std::unexpected(read.error());
  if (*read != *info) {
    return std::unexpected(
        std::format("{} does not read back from its named form", where));
  }
  json = std::move(named);
  return true;
}

// The stream directory names under `root`, in order.
Result<std::vector<std::string>> StreamIds(const fs::path& root) {
  const fs::path directory = root / "audio" / "streams" / "akao";
  std::vector<std::string> ids;
  std::error_code error;
  if (!fs::is_directory(directory, error)) {
    if (error == std::errc::no_such_file_or_directory) return ids;
    if (error) {
      return std::unexpected(Platform::PathText(directory) + ": " +
                             error.message());
    }
    return ids;
  }
  fs::directory_iterator entry(directory, error);
  while (!error && entry != fs::directory_iterator()) {
    if (entry->is_directory(error) && !error) {
      ids.push_back(Platform::PathText(entry->path().filename()));
    }
    if (!error) entry.increment(error);
  }
  if (error) {
    return std::unexpected(Platform::PathText(directory) + ": " +
                           error.message());
  }
  std::ranges::sort(ids);
  return ids;
}

}  // namespace

std::expected<StreamAuthoringReport, std::string> UpgradeStreamAuthoring(
    const fs::path& root) {
  auto ids = StreamIds(root);
  if (!ids) return std::unexpected(ids.error());
  StreamAuthoringReport report;
  for (const std::string& id : *ids) {
    const std::string file = Content::AudioStreamFile(id);
    auto written = EditJsonOutput(root, file, [&](Json& json) {
      ++report.streams;
      return NameStream(json, file, id);
    });
    if (!written) return std::unexpected(written.error());
    if (*written) ++report.written;
  }
  return report;
}

}  // namespace VS::Extract
