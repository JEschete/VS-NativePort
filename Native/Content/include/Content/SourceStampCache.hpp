#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace VS::Content {

struct SourceFileStamp {
  std::uintmax_t size = 0;
  std::int64_t modified = 0;
  bool operator==(const SourceFileStamp&) const = default;
};

[[nodiscard]] std::expected<SourceFileStamp, std::string> ReadSourceFileStamp(
    const std::filesystem::path& path);

// A disposable optimization for original extracted sources only. Native CSVs
// and schemas must use content fingerprints, never these metadata stamps.
class SourceStampCache {
 public:
  explicit SourceStampCache(const std::filesystem::path& root);
  [[nodiscard]] std::expected<void, std::string> Load();
  [[nodiscard]] bool Use(std::string_view source, SourceFileStamp stamp,
                         std::string_view expected_hash);
  void Remember(std::string_view source, SourceFileStamp stamp,
                std::string_view verified_hash);
  // Call only after the complete conversion-verification pass succeeds.
  [[nodiscard]] std::expected<void, std::string> Publish();
  [[nodiscard]] const std::filesystem::path& Path() const { return path_; }

 private:
  struct Record {
    SourceFileStamp stamp;
    std::string sha256;
    bool operator==(const Record&) const = default;
  };
  std::filesystem::path path_;
  std::map<std::string, Record, std::less<>> previous_;
  std::map<std::string, Record, std::less<>> verified_;
};

}  // namespace VS::Content
