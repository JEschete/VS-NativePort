#pragma once

#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Core/Types.hpp"

namespace VS::Content {

// The extracted game data: what VS.Extract wrote from the user's disc
// (Native\data by default, never committed). Files keep their disc paths under
// disc\; files.tsv lists each one's first sector, size and form ("raw" = the
// disc file as is; otherwise the converted file's extension, e.g. "mkv" for
// MOV/TITLE.STR.mkv; "stream" = an interleaved file not converted), so code
// that still addresses the disc by sector finds it.
class DataStore {
 public:
  static constexpr u32 kDataSectorBytes = 2048;
  struct Entry {
    std::string path;  // disc path, e.g. "TITLE/TITLE.PRG"
    u32 lba = 0;
    u32 size = 0;
    std::string form;  // "raw", or the converted form

    u32 SectorCount() const;
    bool ContainsSector(u32 sector) const;
  };

  static constexpr const char* ManifestName = "files.tsv";

  // Opens the process-wide store (used by the C API in Content/DataAccess.h).
  static std::expected<void, std::string> Open(
      const std::filesystem::path& root);
  static const DataStore* Get();

  const std::filesystem::path& Root() const { return m_root_; }
  const std::vector<Entry>& Entries() const { return m_entries_; }
  const Entry* Find(std::string_view path) const;
  const Entry* FindBySector(
      u32 lba) const;  // the file whose sectors include `lba`
  std::filesystem::path FileOf(const Entry& e) const;

  bool Read(const Entry& e, u64 offset, std::span<u8> out) const;
  std::expected<std::vector<u8>, std::string> ReadAll(const Entry& e) const;

 private:
  std::filesystem::path m_root_;
  std::vector<Entry> m_entries_;  // sorted by lba
};

}  // namespace VS::Content
