#include "Content/SourceStampCache.hpp"

#include <cstddef>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>

#include "Content/ConversionManifest.hpp"
#include "Content/JsonText.hpp"
#include "Platform/AtomicFile.hpp"
#include "Platform/Paths.hpp"

namespace VS::Content {
namespace {
constexpr std::string_view kSchema = "vs.source-stamps";
constexpr std::uintmax_t kMaximumCacheBytes = std::uintmax_t{8} * 1024 * 1024;
using FileClockPeriod = std::filesystem::file_time_type::duration::period;

std::expected<std::optional<JsonDocument>, std::string> ReadCacheDocument(
    const std::filesystem::path& path) {
  std::error_code error;
  const bool exists = std::filesystem::exists(path, error);
  if (error) {
    return std::unexpected(Platform::PathText(path) + ": " + error.message());
  }
  if (!exists) return std::nullopt;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size > kMaximumCacheBytes) {
    return std::unexpected(
        "cannot read source cache " + Platform::PathText(path) +
        (error ? ": " + error.message() : ": exceeds size limit"));
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return std::unexpected("cannot open " + Platform::PathText(path));
  }
  std::string text(static_cast<std::size_t>(size), '\0');
  input.read(text.data(), static_cast<std::streamsize>(text.size()));
  if (!input || input.gcount() != static_cast<std::streamsize>(text.size())) {
    return std::unexpected("cannot read " + Platform::PathText(path));
  }
  auto json =
      ParseJsonText(std::as_bytes(std::span(text)), Platform::PathText(path));
  if (!json) return std::unexpected(json.error());
  return std::move(*json);
}
}  // namespace

std::expected<SourceFileStamp, std::string> ReadSourceFileStamp(
    const std::filesystem::path& path) {
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error) {
    return std::unexpected(Platform::PathText(path) + ": " + error.message());
  }
  const auto modified = std::filesystem::last_write_time(path, error);
  if (error) {
    return std::unexpected(Platform::PathText(path) + ": " + error.message());
  }
  return SourceFileStamp{.size = size,
                         .modified = modified.time_since_epoch().count()};
}

SourceStampCache::SourceStampCache(const std::filesystem::path& root)
    : path_(root / ".cache" / "source-verification.json") {}

std::expected<void, std::string> SourceStampCache::Load() {
  previous_.clear();
  verified_.clear();
  auto json = ReadCacheDocument(path_);
  if (!json) return std::unexpected(json.error());
  if (!*json) return {};
  const auto name = Platform::PathText(path_);
  const JsonMembers header(**json, name);
  if (auto only = header.Only({"schema", "schema_version", "tick_numerator",
                               "tick_denominator", "files"});
      !only) {
    return std::unexpected(only.error());
  }
  auto schema = header.String("schema");
  auto version = header.Integer("schema_version", 1, 1);
  auto numerator = header.Integer("tick_numerator", FileClockPeriod::num,
                                  FileClockPeriod::num);
  auto denominator = header.Integer("tick_denominator", FileClockPeriod::den,
                                    FileClockPeriod::den);
  auto files = header.Get("files");
  if (!schema) return std::unexpected(schema.error());
  if (!version) return std::unexpected(version.error());
  if (!numerator) return std::unexpected(numerator.error());
  if (!denominator) return std::unexpected(denominator.error());
  if (!files) return std::unexpected(files.error());
  if (*schema != kSchema || !(**files).is_array()) {
    return std::unexpected(name + ": incompatible source cache");
  }
  std::map<std::string, Record, std::less<>> records;
  for (const auto& value : **files) {
    const JsonMembers row(value, name + ".files");
    if (auto only = row.Only({"source", "size", "modified", "sha256"}); !only) {
      return std::unexpected(only.error());
    }
    auto source = row.String("source");
    auto bytes =
        row.Unsigned("size", std::numeric_limits<std::uint32_t>::max());
    auto time =
        row.Integer("modified", std::numeric_limits<std::int64_t>::min(),
                    std::numeric_limits<std::int64_t>::max());
    auto hash = row.String("sha256");
    if (!source) return std::unexpected(source.error());
    if (!bytes) return std::unexpected(bytes.error());
    if (!time) return std::unexpected(time.error());
    if (!hash) return std::unexpected(hash.error());
    if (!IsDataRelativePath(*source) || !IsLowercaseSha256(*hash)) {
      return std::unexpected(name + ": invalid source identity/hash");
    }
    if (!records.emplace(*source, Record{{*bytes, *time}, *hash}).second) {
      return std::unexpected(name + ": duplicate source " + *source);
    }
  }
  previous_ = std::move(records);
  return {};
}

bool SourceStampCache::Use(std::string_view source, SourceFileStamp stamp,
                           std::string_view expected_hash) {
  const auto found = previous_.find(source);
  if (found == previous_.end() || found->second.stamp != stamp ||
      found->second.sha256 != expected_hash) {
    return false;
  }
  verified_[std::string(source)] = found->second;
  return true;
}

void SourceStampCache::Remember(std::string_view source, SourceFileStamp stamp,
                                std::string_view verified_hash) {
  verified_[std::string(source)] = {stamp, std::string(verified_hash)};
}

std::expected<void, std::string> SourceStampCache::Publish() {
  if (verified_ == previous_) return {};
  JsonDocument files = JsonDocument::array();
  for (const auto& [source, record] : verified_) {
    files.push_back({{"source", source},
                     {"size", record.stamp.size},
                     {"modified", record.stamp.modified},
                     {"sha256", record.sha256}});
  }
  const JsonDocument json{{"schema", kSchema},
                          {"schema_version", 1},
                          {"tick_numerator", FileClockPeriod::num},
                          {"tick_denominator", FileClockPeriod::den},
                          {"files", files}};
  std::error_code error;
  std::filesystem::create_directories(path_.parent_path(), error);
  if (error) {
    return std::unexpected(Platform::PathText(path_) + ": " + error.message());
  }
  auto written =
      Platform::WriteFileAtomically(path_, FormatJsonText(json) + "\n");
  if (written) previous_ = verified_;
  return written;
}
}  // namespace VS::Content
